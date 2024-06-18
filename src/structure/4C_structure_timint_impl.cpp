/*----------------------------------------------------------------------*/
/*! \file
\brief Implicit time integration for structural dynamics

\level 1

*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/* headers */

#include "4C_structure_timint_impl.hpp"

#include "4C_beamcontact_beam3contact_manager.hpp"
#include "4C_beaminteraction_beam_to_beam_contact_defines.hpp"
#include "4C_cardiovascular0d_manager.hpp"
#include "4C_constraint_manager.hpp"
#include "4C_constraint_solver.hpp"
#include "4C_constraint_springdashpot_manager.hpp"
#include "4C_contact_abstract_strategy.hpp"  // needed in CmtLinearSolve (for feeding the contact solver with latest information about the contact status)
#include "4C_contact_defines.hpp"
#include "4C_contact_manager.hpp"
#include "4C_contact_meshtying_abstract_strategy.hpp"  // needed in CmtLinearSolve (for feeding the contact solver with latest information about the contact status)
#include "4C_contact_meshtying_contact_bridge.hpp"
#include "4C_contact_meshtying_manager.hpp"
#include "4C_fem_condition_locsys.hpp"
#include "4C_fem_condition_utils.hpp"
#include "4C_fem_discretization_nullspace.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_beamcontact.hpp"
#include "4C_inpar_contact.hpp"
#include "4C_inpar_wear.hpp"
#include "4C_io_control.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_krylov_projector.hpp"
#include "4C_linalg_mapextractor.hpp"
#include "4C_linalg_multiply.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_print.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_mor_pod.hpp"
#include "4C_mortar_defines.hpp"
#include "4C_mortar_manager_base.hpp"
#include "4C_mortar_strategy_base.hpp"
#include "4C_so3_hex8.hpp"
#include "4C_so3_hex8p1j1.hpp"
#include "4C_so3_sh8p8.hpp"
#include "4C_so3_shw6.hpp"
#include "4C_structure_aux.hpp"
#include "4C_structure_timint.hpp"
#include "4C_structure_timint_noxgroup.hpp"

#include <sstream>
#ifdef FOUR_C_ENABLE_FE_TRAPPING
#include <cfenv>
#endif

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/* constructor */
STR::TimIntImpl::TimIntImpl(const Teuchos::ParameterList& timeparams,
    const Teuchos::ParameterList& ioparams, const Teuchos::ParameterList& sdynparams,
    const Teuchos::ParameterList& xparams, Teuchos::RCP<Core::FE::Discretization> actdis,
    Teuchos::RCP<Core::LinAlg::Solver> solver, Teuchos::RCP<Core::LinAlg::Solver> contactsolver,
    Teuchos::RCP<Core::IO::DiscretizationWriter> output)
    : TimInt(timeparams, ioparams, sdynparams, xparams, actdis, solver, contactsolver, output),
      pred_(Core::UTILS::IntegralValue<Inpar::STR::PredEnum>(sdynparams, "PREDICT")),
      itertype_(Core::UTILS::IntegralValue<Inpar::STR::NonlinSolTech>(sdynparams, "NLNSOL")),
      normtypedisi_(Core::UTILS::IntegralValue<Inpar::STR::ConvNorm>(sdynparams, "NORM_DISP")),
      normtypefres_(Core::UTILS::IntegralValue<Inpar::STR::ConvNorm>(sdynparams, "NORM_RESF")),
      normtypepres_(Core::UTILS::IntegralValue<Inpar::STR::ConvNorm>(sdynparams, "NORM_PRES")),
      normtypepfres_(Core::UTILS::IntegralValue<Inpar::STR::ConvNorm>(sdynparams, "NORM_INCO")),
      combdispre_(
          Core::UTILS::IntegralValue<Inpar::STR::BinaryOp>(sdynparams, "NORMCOMBI_DISPPRES")),
      combfrespfres_(
          Core::UTILS::IntegralValue<Inpar::STR::BinaryOp>(sdynparams, "NORMCOMBI_RESFINCO")),
      combdisifres_(
          Core::UTILS::IntegralValue<Inpar::STR::BinaryOp>(sdynparams, "NORMCOMBI_RESFDISP")),
      iternorm_(Core::UTILS::IntegralValue<Inpar::STR::VectorNorm>(sdynparams, "ITERNORM")),
      itermax_(sdynparams.get<int>("MAXITER")),
      itermin_(sdynparams.get<int>("MINITER")),
      toldisi_(sdynparams.get<double>("TOLDISP")),
      tolfres_(sdynparams.get<double>("TOLRES")),
      tolpfres_(sdynparams.get<double>("TOLINCO")),
      tolpres_(sdynparams.get<double>("TOLPRE")),
      uzawaparam_(sdynparams.get<double>("UZAWAPARAM")),
      uzawaitermax_(sdynparams.get<int>("UZAWAMAXITER")),
      tolcon_(sdynparams.get<double>("TOLCONSTR")),
      tolcardvasc0d_(Global::Problem::Instance()->cardiovascular0_d_structural_params().get<double>(
          "TOL_CARDVASC0D_RES")),
      tolcardvasc0ddofincr_(
          Global::Problem::Instance()->cardiovascular0_d_structural_params().get<double>(
              "TOL_CARDVASC0D_DOFINCR")),
      iter_(-1),
      normcharforce_(0.0),
      normchardis_(0.0),
      normfres_(0.0),
      normfresr_(0.0),
      normdisi_(0.0),
      normdisir_(0.0),
      normcon_(0.0),
      normcardvasc0d_(0.0),
      normcardvasc0ddofincr_(0.0),
      normpfres_(0.0),
      normpres_(0.0),
      normcontconstr_(0.0),  // < norm of contact constraints (saddlepoint formulation)
      normlagr_(0.0),        // < norm of lagrange multiplier increment (saddlepoint formulation)
      normw_(0.0),
      normwrhs_(0.0),
      normwm_(0.0),
      normwmrhs_(0.0),
      alpha_ls_(sdynparams.get<double>("ALPHA_LS")),
      sigma_ls_(sdynparams.get<double>("SIGMA_LS")),
      ls_maxiter_(sdynparams.get<int>("LSMAXITER")),
      cond_res_(0.0),
      disi_(Teuchos::null),
      fres_(Teuchos::null),
      freact_(Teuchos::null),
      updateprojection_(false),
      stcscale_(Core::UTILS::IntegralValue<Inpar::STR::StcScale>(sdynparams, "STC_SCALING")),
      stclayer_(sdynparams.get<int>("STC_LAYER")),
      ptcdt_(sdynparams.get<double>("PTCDT")),
      dti_(1.0 / ptcdt_)
{
  // Keep this constructor empty!
  // First do everything on the more basic objects like the discretizations, like e.g.
  // redistribution of elements. Only then call the setup to this class. This will call the setup to
  // all classes in the inheritance hierarchy. This way, this class may also override a method that
  // is called during setup() in a base class. general variable verifications:
  return;
}

/*----------------------------------------------------------------------------------------------*
 * Initialize this class                                                            rauch 09/16 |
 *----------------------------------------------------------------------------------------------*/
void STR::TimIntImpl::init(const Teuchos::ParameterList& timeparams,
    const Teuchos::ParameterList& sdynparams, const Teuchos::ParameterList& xparams,
    Teuchos::RCP<Core::FE::Discretization> actdis, Teuchos::RCP<Core::LinAlg::Solver> solver)
{
  // call init() in base class
  STR::TimInt::init(timeparams, sdynparams, xparams, actdis, solver);

  if (itermax_ < 0)
    FOUR_C_THROW("MAXITER has to be greater than or equal to zero. Fix your input file.");

  if (itermin_ < 0)
    FOUR_C_THROW("MINITER has to be greater than or equal to zero. Fix your input file.");

  if (toldisi_ <= 0) FOUR_C_THROW("TOLDISP has to be greater than zero. Fix your input file.");

  if (tolfres_ <= 0) FOUR_C_THROW("TOLRES has to be greater than zero. Fix your input file.");

  if (itermin_ > itermax_)
    FOUR_C_THROW("ITERMIN has to be smaller than or equal to ITERMAX. Fix your input file.");

  if (tolpfres_ <= 0) FOUR_C_THROW("TOLINCO has to be greater than zero. Fix your input file.");

  if (tolpres_ <= 0) FOUR_C_THROW("TOLPRE has to be greater than zero. Fix your input file.");

  if (uzawaparam_ <= 0)
    FOUR_C_THROW("UZAWAPARAM has to be greater than zero. Fix your input file.");

  if (uzawaitermax_ < 0)
    FOUR_C_THROW("UZAWAMAXITER has to be greater than or equal to zero. Fix your input file.");

  if (tolcon_ <= 0) FOUR_C_THROW("TOLCONSTR has to be greater than zero. Fix your input file.");

  if (tolcardvasc0d_ <= 0)
    FOUR_C_THROW("TOL_0D_RES has to be greater than zero. Fix your input file.");

  if (tolcardvasc0ddofincr_ <= 0)
    FOUR_C_THROW("TOL_0D_DOFINCR has to be greater than zero. Fix your input file.");

  if ((alpha_ls_ <= 0) or (alpha_ls_ >= 1))
    FOUR_C_THROW("Valid interval for ALPHA_LS is (0,1). Fix your input file.");

  if ((sigma_ls_ <= 0) or (sigma_ls_ >= 1))
    FOUR_C_THROW("Valid interval for SIGMA_LS is (0,1). Fix your input file.");

  if (ls_maxiter_ < 0)
    FOUR_C_THROW("LSMAXITER has to be greater than or equal to zero. Fix your input file.");

  if (ptcdt_ <= 0) FOUR_C_THROW("PTCDT has to be greater than zero. Fix your input file.");

  // setup NOX parameter lists
  if (itertype_ == Inpar::STR::soltech_noxnewtonlinesearch)
    NoxSetup();
  else if (itertype_ == Inpar::STR::soltech_noxgeneral)
    NoxSetup(xparams.sublist("NOX"));

  // done so far
  return;
}

/*----------------------------------------------------------------------------------------------*
 * Setup this class                                                                 rauch 09/16 |
 *----------------------------------------------------------------------------------------------*/
void STR::TimIntImpl::setup()
{
  // call setup() in base class
  STR::TimInt::setup();

  // verify: if system has constraints implemented with Lagrange multipliers,
  // then Uzawa-type solver is used
  if (conman_->HaveConstraintLagr())
  {
    if ((itertype_ != Inpar::STR::soltech_newtonuzawalin) and
        (itertype_ != Inpar::STR::soltech_newtonuzawanonlin))
      FOUR_C_THROW("Chosen solution technique %s does not work constrained.",
          Inpar::STR::NonlinSolTechString(itertype_).c_str());
  }
  else if (cardvasc0dman_->have_cardiovascular0_d())
  {
    if (itertype_ != Inpar::STR::soltech_newtonuzawalin)
      if (myrank_ == 0)
        FOUR_C_THROW("Chosen solution technique %s does not work with Cardiovascular0D bc.",
            Inpar::STR::NonlinSolTechString(itertype_).c_str());
  }
  else if ((itertype_ == Inpar::STR::soltech_newtonuzawalin) or
           (itertype_ == Inpar::STR::soltech_newtonuzawanonlin))
  {
    FOUR_C_THROW(
        "Chosen solution technique %s does only work constrained or with Cardiovascular0D bc.",
        Inpar::STR::NonlinSolTechString(itertype_).c_str());
  }

  // setup tolerances and binary operators for convergence check of contact/meshtying problems
  // in saddlepoint formulation
  tolcontconstr_ = tolfres_;
  tollagr_ = toldisi_;
  combfrescontconstr_ = Inpar::STR::bop_and;  // default values, avoid uninitialized variables
  combdisilagr_ = Inpar::STR::bop_and;
  normtypecontconstr_ = Inpar::STR::
      convnorm_abs;  // Core::UTILS::IntegralValue<Inpar::STR::ConvNorm>(sdynparams,"NORM_RESF"));
  normtypeplagrincr_ = Inpar::STR::
      convnorm_abs;  // Core::UTILS::IntegralValue<Inpar::STR::ConvNorm>(sdynparams,"NORM_DISP"));

  if (have_contact_meshtying())
  {
    // extract information from parameter lists
    tolcontconstr_ = cmtbridge_->GetStrategy().Params().get<double>("TOLCONTCONSTR");
    tollagr_ = cmtbridge_->GetStrategy().Params().get<double>("TOLLAGR");
    combfrescontconstr_ = Core::UTILS::IntegralValue<Inpar::STR::BinaryOp>(
        cmtbridge_->GetStrategy().Params(), "NORMCOMBI_RESFCONTCONSTR");
    combdisilagr_ = Core::UTILS::IntegralValue<Inpar::STR::BinaryOp>(
        cmtbridge_->GetStrategy().Params(), "NORMCOMBI_DISPLAGR");
  }


  // setup binary operators for convergence check of semi-smooth plasticity problems
  combfresplconstr_ = Inpar::STR::bop_and;
  combdisiLp_ = Inpar::STR::bop_and;
  combfresEasres_ = Inpar::STR::bop_and;
  combdisiEasIncr_ = Inpar::STR::bop_and;

  // -------------------------------------------------------------------
  // setup Krylov projection if necessary
  // -------------------------------------------------------------------
  //
  // sysmat might be singular, e.g. when solid is not fully supported
  // in this case, we need a basis vector for the nullspace/kernel

  // get condition "KrylovSpaceProjection" from discretization
  std::vector<Core::Conditions::Condition*> KSPcond;
  discret_->GetCondition("KrylovSpaceProjection", KSPcond);
  int numcond = KSPcond.size();
  int numsolid = 0;

  Core::Conditions::Condition* kspcond = nullptr;
  // check if for solid Krylov projection is required
  for (int icond = 0; icond < numcond; icond++)
  {
    const std::string& name = KSPcond[icond]->parameters().get<std::string>("discretization");
    if (name == "solid")
    {
      numsolid++;
      kspcond = KSPcond[icond];
    }
  }

  if (numsolid == 1)
  {
    setup_krylov_space_projection(kspcond);
    if (myrank_ == 0) std::cout << "\nSetup of KrylovSpaceProjection in solid field\n" << std::endl;
  }
  else if (numsolid == 0)
  {
    projector_ = Teuchos::null;
  }
  else
    FOUR_C_THROW("Received more than one KrylovSpaceCondition for solid field");

  // prepare line search
  if (itertype_ == Inpar::STR::soltech_newtonls) PrepareLineSearch();

  // create empty residual force vector
  fres_ = Core::LinAlg::CreateVector(*dof_row_map_view(), false);

  // create empty reaction force vector of full length
  freact_ = Core::LinAlg::CreateVector(*dof_row_map_view(), false);

  // iterative displacement increments IncD_{n+1}
  // also known as residual displacements
  disi_ = Core::LinAlg::CreateVector(*dof_row_map_view(), true);

  // prepare matrix for scaled thickness business of thin shell structures
  stcmat_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*dof_row_map_view(), 81, true, true));
  stccompl_ = false;

  return;
}

/*----------------------------------------------------------------------*/
/* integrate step */
int STR::TimIntImpl::IntegrateStep()
{
  int error = 0;
  Predict();
  error = Solve();
  return error;
}

void STR::TimIntImpl::output(const bool forced_writerestart)
{
  OutputStep(forced_writerestart);

  // write Gmsh output
  write_gmsh_struc_output_step();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::prepare_time_step()
{
  // safety checks
  check_is_init();
  check_is_setup();

  // update end time \f$t_{n+1}\f$ of this time step to cope with time step size adaptivity
  SetTimen((*time_)[0] + (*dt_)[0]);

  // prepare contact for new time step
  PrepareStepContact();

  // predict
  Predict();
}

/*----------------------------------------------------------------------*/
/* predict solution */
void STR::TimIntImpl::Predict()
{
  // safety checks
  check_is_init();
  check_is_setup();

  // things that need to be done before Predict
  PrePredict();

  // Update locals systems (which may be time dependent)
  if (locsysman_ != Teuchos::null)
    locsysman_->Update(timen_, {}, Global::Problem::Instance()->FunctionManager());

  // set iteration step to 0 (predictor)
  iter_ = 0;
  // choose predictor
  if ((pred_ == Inpar::STR::pred_constdis) or (pred_ == Inpar::STR::pred_constdispres))
  {
    predict_const_dis_consist_vel_acc();
    normdisi_ = 1.0e6;
    normpres_ = 1.0e6;
  }
  else if (pred_ == Inpar::STR::pred_constvel)
  {
    predict_const_vel_consist_acc();
    normdisi_ = 1.0e6;
    normpres_ = 1.0e6;
  }
  else if (pred_ == Inpar::STR::pred_constacc)
  {
    PredictConstAcc();
    normdisi_ = 1.0e6;
    normpres_ = 1.0e6;
  }
  else if ((pred_ == Inpar::STR::pred_constdisvelacc) or
           (pred_ == Inpar::STR::pred_constdisvelaccpres))
  {
    predict_const_dis_vel_acc();
    normdisi_ = 1.0e6;
    normpres_ = 1.0e6;
  }
  else if (pred_ == Inpar::STR::pred_tangdis)
  {
    predict_tang_dis_consist_vel_acc();
    // normdisi_ has been set
  }
  else
  {
    FOUR_C_THROW("Trouble in determining predictor %i", pred_);
  }

  // zerofy pressure DOFs and time-derivatives
  if (pressure_ != Teuchos::null)
  {
    if ((pred_ != Inpar::STR::pred_constdispres) and (pred_ != Inpar::STR::pred_constdisvelaccpres))
    {
      pressure_->InsertCondVector(pressure_->ExtractCondVector(zeros_), disn_);
    }
    pressure_->InsertCondVector(pressure_->ExtractCondVector(zeros_), veln_);
    pressure_->InsertCondVector(pressure_->ExtractCondVector(zeros_), accn_);
  }

  // apply Dirichlet BCs
  apply_dirichlet_bc(timen_, disn_, veln_, accn_, false);

  // create parameter list to hand in boolean flag indicating that this a predictor
  Teuchos::ParameterList params;
  params.set<bool>("predict", true);

  // residual of condensed variables (e.g. EAS) for NewtonLS
  if (fresn_str_ != Teuchos::null)
  {
    params.set<double>("cond_rhs_norm", 0.);
    params.set<int>("MyPID", myrank_);
  }

  // compute residual forces fres_ and stiffness stiff_
  // If we use a tangential predictor, the contact status could have been changed in contrast
  // to a constant predictor. Thus the contact status has to be reevaluated! (hiermeier 22.01.2014)
  if (pred_ == Inpar::STR::pred_tangdis) params.set<bool>("predict", false);

  // compute residual forces fres_ and stiffness stiff_
  evaluate_force_stiff_residual(params);

  // get residual of condensed variables (e.g. EAS) for NewtonLS
  if (fresn_str_ != Teuchos::null)
  {
    double loc = params.get<double>("cond_rhs_norm");
    discret_->Comm().SumAll(&loc, &cond_res_, 1);
  }

  // rotate to local coordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

  // extract reaction forces
  // reactions are negative to balance residual on DBC
  freact_->Update(-1.0, *fres_, 0.0);
  dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
  // rotate reaction forces back to global coordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

  // blank residual at DOFs on Dirichlet BC
  dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
  // rotate back to global coordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

  // split norms
  if (pressure_ != Teuchos::null)
  {
    Teuchos::RCP<Epetra_Vector> fres = pressure_->ExtractOtherVector(fres_);
    normfres_ = STR::calculate_vector_norm(iternorm_, fres);
    Teuchos::RCP<Epetra_Vector> fpres = pressure_->ExtractCondVector(fres_);
    normpfres_ = STR::calculate_vector_norm(iternorm_, fpres);
  }
  else
  {
    // build residual force norm
    normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
  }

  // determine characteristic norms
  // we set the minimum of calc_ref_norm_force() and #tolfres_, because
  // we want to prevent the case of a zero characteristic fnorm
  normcharforce_ = CalcRefNormForce();
  if (normcharforce_ == 0.0) normcharforce_ = tolfres_;
  normchardis_ = calc_ref_norm_displacement();
  if (normchardis_ == 0.0) normchardis_ = toldisi_;


  // output
  print_predictor();

  // enjoy your meal
  return;
}

/*----------------------------------------------------------------------*/
/* prepare partition step */
void STR::TimIntImpl::prepare_partition_step()
{
  // set iteration step to 0
  iter_ = 0;

  // apply Dirichlet BCs
  apply_dirichlet_bc(timen_, disn_, veln_, accn_, false);

  // create parameter list to hand in boolean flag indicating that this a predictor
  Teuchos::ParameterList params;
  params.set<bool>("predict", true);

  // compute residual forces fres_ and stiffness stiff_
  evaluate_force_stiff_residual(params);

  // rotate to local co-ordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

  // extract reaction forces
  // reactions are negative to balance residual on DBC
  freact_->Update(-1.0, *fres_, 0.0);
  dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
  // rotate reaction forces back to global co-ordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

  // blank residual at DOFs on Dirichlet BC
  dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
  // rotate back to global co-ordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

  // split norms
  if (pressure_ != Teuchos::null)
  {
    Teuchos::RCP<Epetra_Vector> fres = pressure_->ExtractOtherVector(fres_);
    normfres_ = STR::calculate_vector_norm(iternorm_, fres);
    Teuchos::RCP<Epetra_Vector> fpres = pressure_->ExtractCondVector(fres_);
    normpfres_ = STR::calculate_vector_norm(iternorm_, fpres);
  }
  else
  {
    // build residual force norm
    normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
  }

  // determine characteristic norms
  // we set the minumum of calc_ref_norm_force() and #tolfres_, because
  // we want to prevent the case of a zero characteristic fnorm
  normcharforce_ = CalcRefNormForce();
  if (normcharforce_ == 0.0) normcharforce_ = tolfres_;
  normchardis_ = calc_ref_norm_displacement();
  if (normchardis_ == 0.0) normchardis_ = toldisi_;


  // output
  print_predictor();

  // enjoy your meal
  return;
}


/*----------------------------------------------------------------------*/
/* Check for LS with condensed variables and do preparations */
void STR::TimIntImpl::PrepareLineSearch()
{
  // each proc searchs through his elements
  int haveCondensationLocal = 0;
  int haveCondensationGlobal = 0;

  // each proc searches through his elements
  for (int i = 0; i < discret_->NumMyRowElements(); i++)
  {
    Core::Elements::Element* actele = discret_->lRowElement(i);
    Discret::ELEMENTS::SoHex8* ele_hex8 = dynamic_cast<Discret::ELEMENTS::SoHex8*>(actele);
    if ((ele_hex8 != nullptr && ele_hex8->HaveEAS() == true) ||
        (actele->ElementType() == Discret::ELEMENTS::SoHex8P1J1Type::Instance()) ||
        (actele->ElementType() == Discret::ELEMENTS::SoShw6Type::Instance()))
      haveCondensationLocal = 1;
    if (actele->ElementType() == Discret::ELEMENTS::SoSh8p8Type::Instance())
      FOUR_C_THROW(
          "no line search for this element implemented.\n"
          "Feel free to implement similar to hex8 with EAS");
  }
  discret_->Comm().MaxAll(&haveCondensationLocal, &haveCondensationGlobal, 1);
  if (haveCondensationGlobal)
  {
    fresn_str_ = Core::LinAlg::CreateVector(*dof_row_map_view(), true);
    fintn_str_ = Core::LinAlg::CreateVector(*dof_row_map_view(), true);
  }
  return;
}
/*----------------------------------------------------------------------*/
/* predict solution as constant displacements, velocities
 * and accelerations */
void STR::TimIntImpl::predict_const_dis_vel_acc()
{
  // constant predictor
  disn_->Update(1.0, *(*dis_)(0), 0.0);
  veln_->Update(1.0, *(*vel_)(0), 0.0);
  accn_->Update(1.0, *(*acc_)(0), 0.0);
  disi_->PutScalar(0.0);

  // see you next time step
  return;
}

/*----------------------------------------------------------------------*/
void STR::TimIntImpl::predict_tang_dis_consist_vel_acc()
{
  // initialise
  disn_->Update(1.0, *(*dis_)(0), 0.0);
  veln_->Update(1.0, *(*vel_)(0), 0.0);
  accn_->Update(1.0, *(*acc_)(0), 0.0);
  disi_->PutScalar(0.0);

  // for displacement increments on Dirichlet boundary
  Teuchos::RCP<Epetra_Vector> dbcinc = Core::LinAlg::CreateVector(*dof_row_map_view(), true);

  // copy last converged displacements
  dbcinc->Update(1.0, *(*dis_)(0), 0.0);

  // get Dirichlet values at t_{n+1}
  apply_dirichlet_bc(timen_, dbcinc, Teuchos::null, Teuchos::null, false);

  // subtract the displacements of the last converged step
  // DBC-DOFs hold increments of current step
  // free-DOFs hold zeros
  dbcinc->Update(-1.0, *(*dis_)(0), 1.0);

  // create parameter list to hand in boolean flag indicating that this a predictor
  Teuchos::ParameterList params;
  params.set<bool>("predict", true);

  // compute residual forces fres_ and stiffness stiff_
  // at disn_, etc which are unchanged
  evaluate_force_stiff_residual(params);

  // add linear reaction forces to residual
  {
    // linear reactions
    Teuchos::RCP<Epetra_Vector> freact = Core::LinAlg::CreateVector(*dof_row_map_view(), true);
    stiff_->Multiply(false, *dbcinc, *freact);

    // add linear reaction forces due to prescribed Dirichlet BCs
    fres_->Update(1.0, *freact, 1.0);
  }

  // rotate to local co-ordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

  // extract reaction forces
  freact_->Update(-1.0, *fres_, 0.0);  // reactions are negative
  dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
  // rotate reaction forces back to global co-ordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

  // blank residual at DOFs on Dirichlet BC
  dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
  // rotate back to global co-ordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

  // make negative residual
  fres_->Scale(-1.0);

  // transform to local co-ordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);

  // apply Dirichlet BCs to system of equations
  disi_->PutScalar(0.0);
  stiff_->Complete();
  if (get_loc_sys_trafo() != Teuchos::null)
  {
    Core::LinAlg::apply_dirichlet_to_system(
        *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
        *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
  }
  else
  {
    Core::LinAlg::apply_dirichlet_to_system(
        *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
  }

  // solve for disi_
  // Solve K_Teffdyn . IncD = -R  ===>  IncD_{n+1}
  if (have_contact_meshtying())
    CmtLinearSolve();  // use contact/meshtying solver
  else
  {
    Core::LinAlg::SolverParams solver_params;
    solver_params.refactor = true;
    solver_params.reset = true;
    solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
  }

  // recover contact / meshtying Lagrange multipliers
  if (have_contact_meshtying()) cmtbridge_->Recover(disi_);

  // decide which norms have to be evaluated
  bool bPressure = pressure_ != Teuchos::null;
  bool bContactSP =
      (have_contact_meshtying() &&
          Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
              cmtbridge_->GetStrategy().Params(), "STRATEGY") == Inpar::CONTACT::solution_lagmult &&
          (Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
               cmtbridge_->GetStrategy().Params(), "SYSTEM") != Inpar::CONTACT::system_condensed ||
              Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
                  cmtbridge_->GetStrategy().Params(), "SYSTEM") !=
                  Inpar::CONTACT::system_condensed_lagmult));

  if (bPressure && bContactSP)
    FOUR_C_THROW(
        "We only support either contact/meshtying in saddlepoint formulation or structure with "
        "pressure DOFs");
  if (bPressure == false && bContactSP == false)
  {
    // build residual displacement norm
    normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
  }
  if (bPressure)
  {
    Teuchos::RCP<Epetra_Vector> pres = pressure_->ExtractCondVector(disi_);
    Teuchos::RCP<Epetra_Vector> disp = pressure_->ExtractOtherVector(disi_);
    normpres_ = STR::calculate_vector_norm(iternorm_, pres);
    normdisi_ = STR::calculate_vector_norm(iternorm_, disp);
  }
  if (bContactSP)
  {
    // extract subvectors
    Teuchos::RCP<Epetra_Vector> lagrincr = cmtbridge_->GetStrategy().LagrMultSolveIncr();

    // build residual displacement norm
    normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
    // build lagrange multiplier increment norm
    if (lagrincr != Teuchos::null)
      normlagr_ = STR::calculate_vector_norm(iternorm_, lagrincr);
    else
      normlagr_ = -1.0;
  }

  // set Dirichlet increments in displacement increments
  disi_->Update(1.0, *dbcinc, 1.0);

  // update end-point displacements etc
  update_iter_incrementally();
  // disn_->Update(1.0, *disi_, 1.0);

  // MARK:
  // velocities and accelerations unset on Dirichlet boundary

  // reset to zero
  disi_->PutScalar(0.0);

  // reset anything that needs to be reset at the element level
  {
    // create the parameters for the discretization
    Teuchos::ParameterList p;
    p.set("action", "calc_struct_reset_istep");
    // go to elements
    discret_->evaluate(
        p, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
    discret_->ClearState();
  }

  // shalom
  return;
}

/*--------------------------------------------------------------------------*
 | setup Krylov projector including first fill                    nis Feb13 |
 *--------------------------------------------------------------------------*/
void STR::TimIntImpl::setup_krylov_space_projection(Core::Conditions::Condition* kspcond)
{
  // get number of mode flags in dat-file
  const int nummodes = kspcond->parameters().get<int>("NUMMODES");

  // get rigid body mode flags - number and order as in ComputeNullspace
  // e.g. for a 3-D solid: [transx transy transz rotx roty rotz]
  const auto* modeflags = &kspcond->parameters().get<std::vector<int>>("ONOFF");

  // get actual active mode ids given in dat-file
  std::vector<int> activemodeids;
  for (int rr = 0; rr < nummodes; ++rr)
  {
    if (((*modeflags)[rr]) != 0)
    {
      activemodeids.push_back(rr);
    }
  }

  // get from dat-file definition how weights are to be computed
  const std::string* weighttype =
      &kspcond->parameters().get<std::string>("weight vector definition");

  // since we only use total Lagrange, no update necessary.
  updateprojection_ = false;

  // create the projector
  projector_ = Teuchos::rcp(
      new Core::LinAlg::KrylovProjector(activemodeids, weighttype, discret_->dof_row_map()));

  // update the projector
  update_krylov_space_projection();
}

/*--------------------------------------------------------------------------*
 | update projection vectors w_ and c_ for Krylov projection      nis Feb13 |
 *--------------------------------------------------------------------------*/
void STR::TimIntImpl::update_krylov_space_projection()
{
  const std::string* weighttype = projector_->WeightType();

  // only pointvalues are permissible for now - feel free to extend to integration!
  if (*weighttype == "integration") FOUR_C_THROW("option integration not implemented");

  // get Teuchos::RCP to kernel vector of projector
  // since we are in 'pointvalue' mode, weights are changed implicitly
  Teuchos::RCP<Epetra_MultiVector> c = projector_->GetNonConstKernel();
  c->PutScalar(0.0);

  // get number of modes and their ids
  std::vector<int> modeids = projector_->Modes();

  Teuchos::RCP<Epetra_Map> nullspaceMap = Teuchos::rcp(new Epetra_Map(*discret_->dof_row_map()));
  Teuchos::RCP<Epetra_MultiVector> nullspace =
      Core::FE::ComputeNullSpace(*discret_, 3, 6, nullspaceMap);
  if (nullspace == Teuchos::null) FOUR_C_THROW("nullspace not successfully computed");

  // sort vector of nullspace data into kernel vector c_
  for (size_t i = 0; i < Teuchos::as<size_t>(modeids.size()); i++)
  {
    Epetra_Vector* ci = (*c)(i);
    Epetra_Vector* ni = (*nullspace)(modeids[i]);
    const size_t myLength = ci->MyLength();
    for (size_t j = 0; j < myLength; j++)
    {
      (*ci)[j] = (*ni)[j];
    }
  }

  // fillcomplete the projector to compute (w^T c)^(-1)
  projector_->fill_complete();
}

/*----------------------------------------------------------------------*/
/* evaluate external forces and its linearization at t_{n+1} */
void STR::TimIntImpl::apply_force_stiff_external(const double time,  //!< evaluation time
    const Teuchos::RCP<Epetra_Vector> dis,                           //!< old displacement state
    const Teuchos::RCP<Epetra_Vector> disn,                          //!< new displacement state
    const Teuchos::RCP<Epetra_Vector> vel,                           //!< velocity state
    Teuchos::RCP<Epetra_Vector>& fext,                               //!< external force
    Teuchos::RCP<Core::LinAlg::SparseOperator>& fextlin  //!< linearization of external force
)
{
  Teuchos::ParameterList p;
  // other parameters needed by the elements
  p.set("total time", time);
  p.set<const Core::UTILS::FunctionManager*>(
      "function_manager", &Global::Problem::Instance()->FunctionManager());

  // set vector values needed by elements
  discret_->ClearState();
  discret_->set_state(0, "displacement", dis);

  if (damping_ == Inpar::STR::damp_material) discret_->set_state(0, "velocity", vel);
  // get load vector
  const Teuchos::ParameterList& sdyn = Global::Problem::Instance()->structural_dynamic_params();
  bool loadlin = (Core::UTILS::IntegralValue<int>(sdyn, "LOADLIN") == 1);

  if (!loadlin)
    discret_->evaluate_neumann(p, *fext);
  else
  {
    discret_->set_state(0, "displacement new", disn);
    discret_->evaluate_neumann(p, fext, fextlin);
  }

  // go away
  return;
}

/*----------------------------------------------------------------------*/
/* evaluate ordinary internal force, its stiffness at state */
void STR::TimIntImpl::apply_force_stiff_internal(const double time, const double dt,
    const Teuchos::RCP<Epetra_Vector> dis, const Teuchos::RCP<Epetra_Vector> disi,
    const Teuchos::RCP<Epetra_Vector> vel, Teuchos::RCP<Epetra_Vector> fint,
    Teuchos::RCP<Core::LinAlg::SparseOperator> stiff, Teuchos::ParameterList& params,
    Teuchos::RCP<Core::LinAlg::SparseOperator> damp)
{
  // *********** time measurement ***********
  double dtcpu = timer_->wallTime();
  // *********** time measurement ***********

  // action for elements
  const std::string action = "calc_struct_nlnstiff";
  params.set("action", action);
  // other parameters that might be needed by the elements
  params.set("total time", time);
  params.set("delta time", dt);
  params.set("damping", damping_);
  if (pressure_ != Teuchos::null) params.set("volume", 0.0);

  // set vector values needed by elements
  discret_->ClearState();
  discret_->set_state(0, "residual displacement", disi);
  discret_->set_state(0, "displacement", dis);
  if (damping_ == Inpar::STR::damp_material) discret_->set_state(0, "velocity", vel);
  // fintn_->PutScalar(0.0);  // initialise internal force vector

  // Set material displacement state for ale-wear formulation
  if ((dismatn_ != Teuchos::null)) discret_->set_state(0, "material_displacement", dismatn_);


  /* Additionally we hand in "fint_str_"
   * This is usually Teuchos::null unless we do line search in
   * combination with elements that perform a local condensation
   * e.g. hex8 with EAS or semi-smooth Newton plasticity.
   * In such cases, fint_str_ contains the right hand side
   * without the modifications due to the local condensation procedure.
   */
  if (fintn_str_ != Teuchos::null) fintn_str_->PutScalar(0.);
  discret_->evaluate(params, stiff, damp, fint, Teuchos::null, fintn_str_);
  discret_->ClearState();

  // *********** time measurement ***********
  dtele_ = timer_->wallTime() - dtcpu;
  // *********** time measurement ***********
}

/*----------------------------------------------------------------------*/
/* evaluate inertia force and its linearization */
void STR::TimIntImpl::apply_force_stiff_internal_and_inertial(const double time, const double dt,
    const double timintfac_dis, const double timintfac_vel, const Teuchos::RCP<Epetra_Vector> dis,
    const Teuchos::RCP<Epetra_Vector> disi, const Teuchos::RCP<Epetra_Vector> vel,
    const Teuchos::RCP<Epetra_Vector> acc, Teuchos::RCP<Epetra_Vector> fint,
    Teuchos::RCP<Epetra_Vector> finert, Teuchos::RCP<Core::LinAlg::SparseOperator> stiff,
    Teuchos::RCP<Core::LinAlg::SparseOperator> mass, Teuchos::ParameterList& params,
    const double beta, const double gamma, const double alphaf, const double alpham)
{
  // action for elements
  const std::string action = "calc_struct_nlnstiffmass";
  params.set("action", action);
  // other parameters that might be needed by the elements
  params.set("total time", time);
  params.set("delta time", dt);

  params.set("timintfac_dis", timintfac_dis);
  params.set("timintfac_vel", timintfac_vel);

  if (HaveNonlinearMass() == Inpar::STR::ml_rotations)
  {
    params.set("rot_beta", beta);
    params.set("rot_gamma", gamma);
    params.set("rot_alphaf", alphaf);
    params.set("rot_alpham", alpham);
  }

  discret_->ClearState();
  discret_->set_state(0, "residual displacement", disi);
  discret_->set_state(0, "displacement", dis);
  discret_->set_state(0, "velocity", vel);
  discret_->set_state(0, "acceleration", acc);

  // Set material displacement state for struct-ale
  if ((dismatn_ != Teuchos::null)) discret_->set_state(0, "material_displacement", dismatn_);

  /* Additionally we hand in "fint_str_"
   * This is usually Teuchos::null unless we do line search in
   * combination with elements that perform a local condensation
   * e.g. hex8 with EAS or semi-smooth Newton plasticity.
   * In such cases, fint_str_ contains the right hand side
   * without the modifications due to the local condensation procedure.
   */
  discret_->evaluate(params, stiff, mass, fint, finert, fintn_str_);
  discret_->ClearState();

  mass->Complete();

  return;
};

/*----------------------------------------------------------------------*/
/* evaluate forces due to constraints */
void STR::TimIntImpl::apply_force_stiff_constraint(const double time,
    const Teuchos::RCP<Epetra_Vector> dis, const Teuchos::RCP<Epetra_Vector> disn,
    Teuchos::RCP<Epetra_Vector>& fint, Teuchos::RCP<Core::LinAlg::SparseOperator>& stiff,
    Teuchos::ParameterList pcon)
{
  if (conman_->HaveConstraint())
  {
    conman_->evaluate_force_stiff(time, dis, disn, fint, stiff, pcon);
  }

  return;
}

/*----------------------------------------------------------------------*/
/* evaluate forces due to Cardiovascular0D bcs */
void STR::TimIntImpl::apply_force_stiff_cardiovascular0_d(const double time,
    const Teuchos::RCP<Epetra_Vector> disn, Teuchos::RCP<Epetra_Vector>& fint,
    Teuchos::RCP<Core::LinAlg::SparseOperator>& stiff, Teuchos::ParameterList pwindk)
{
  if (cardvasc0dman_->have_cardiovascular0_d())
  {
    cardvasc0dman_->evaluate_force_stiff(time, disn, fint, stiff, pwindk);
  }

  return;
}

/*----------------------------------------------------------------------*/
/* evaluate forces and stiffness due to spring dashpot BCs */
void STR::TimIntImpl::apply_force_stiff_spring_dashpot(
    Teuchos::RCP<Core::LinAlg::SparseOperator> stiff, Teuchos::RCP<Epetra_Vector> fint,
    Teuchos::RCP<Epetra_Vector> disn, Teuchos::RCP<Epetra_Vector> veln, bool predict,
    Teuchos::ParameterList psprdash)
{
  psprdash.set("total time", Time());
  if (springman_->HaveSpringDashpot())
  {
    auto stiff_sparse = Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(stiff);
    if (stiff_sparse == Teuchos::null)
      FOUR_C_THROW("Cannot cast stiffness matrix to sparse matrix!");
    springman_->stiffness_and_internal_forces(stiff_sparse, fint, disn, veln, psprdash);
  }

  return;
}

/*----------------------------------------------------------------------*/
/* evaluate forces and stiffness due to contact / meshtying */
void STR::TimIntImpl::apply_force_stiff_contact_meshtying(
    Teuchos::RCP<Core::LinAlg::SparseOperator>& stiff, Teuchos::RCP<Epetra_Vector>& fresm,
    Teuchos::RCP<Epetra_Vector>& dis, bool predict)
{
  if (have_contact_meshtying())
  {
    // *********** time measurement ***********
    double dtcpu = timer_->wallTime();
    // *********** time measurement ***********

    // contact / meshtying modifications need -fres
    fresm->Scale(-1.0);

    if (cmtbridge_->HaveContact())
    {
      if (cmtbridge_->ContactManager()->GetStrategy().has_poro_no_penetration())
      {
        // set structural velocity for poro normal no penetration
        Teuchos::RCP<Epetra_Vector> svel = Teuchos::rcp(new Epetra_Vector(*Velnp()));
        cmtbridge_->ContactManager()->GetStrategy().set_state(Mortar::state_svelocity, *svel);
      }
    }

    // make contact / meshtying modifications to lhs and rhs
    // (depending on whether this is a predictor step or not)
    if (cmtbridge_->HaveMeshtying())
      cmtbridge_->MtManager()->GetStrategy().ApplyForceStiffCmt(
          dis, stiff, fresm, stepn_, iter_, predict);
    if (cmtbridge_->HaveContact())
    {
      dynamic_cast<CONTACT::AbstractStrategy&>(cmtbridge_->ContactManager()->GetStrategy())
          .SetParentState("displacement", dis, discret_);
      cmtbridge_->ContactManager()->GetStrategy().ApplyForceStiffCmt(
          dis, stiff, fresm, stepn_, iter_, predict);
    }


    // scaling back
    fresm->Scale(-1.0);

    // *********** time measurement ***********
    dtcmt_ = timer_->wallTime() - dtcpu;
    // *********** time measurement ***********

    // visualization of current Newton step
#ifdef MORTARGMSH2
    bool gmsh =
        Core::UTILS::IntegralValue<int>(Global::Problem::Instance()->IOParams(), "OUTPUT_GMSH");
    if (gmsh) cmtbridge_->VisualizeGmsh(stepn_, iter_);
#endif
  }

  return;
}

/*----------------------------------------------------------------------*/
/* evaluate forces and stiffness due to beam contact */
void STR::TimIntImpl::apply_force_stiff_beam_contact(
    Teuchos::RCP<Core::LinAlg::SparseOperator>& stiff, Teuchos::RCP<Epetra_Vector>& fresm,
    Teuchos::RCP<Epetra_Vector>& dis, bool predict)
{
  if (HaveBeamContact())
  {
    // *********** time measurement ***********
    double dtcpu = timer_->wallTime();
    // *********** time measurement ***********

    // contact / meshtying modifications need -fres
    fresm->Scale(-1.0);

    // create empty parameter list
    Teuchos::ParameterList beamcontactparams;
    beamcontactparams.set("iter", iter_);
    beamcontactparams.set("dt", (*dt_)[0]);
    beamcontactparams.set("numstep", step_);

    // make contact / meshtying modifications to lhs and rhs
    // (set boolean flag 'newsti' to true, which activates
    // sclaing of contact stiffness with appropriate scaling
    // factor, e.g. (1.0-alphaf), internally)
    beamcman_->evaluate(*system_matrix(), *fresm, *dis, beamcontactparams, true, timen_);

    // scaling back
    fresm->Scale(-1.0);

    // *********** time measurement ***********
    dtcmt_ = timer_->wallTime() - dtcpu;
    // *********** time measurement ***********

    // visualization of current Newton step
#ifdef GMSHNEWTONSTEPS
    beamcman_->GmshOutput(*disn_, stepn_, iter_);
    beamcman_->ConsoleOutput();
#endif  // #ifdef GMSHNEWTONSTEPS
  }

  return;
}

/*----------------------------------------------------------------------*/
/* Check residual displacement and limit it if necessary*/
void STR::TimIntImpl::limit_stepsize_beam_contact(Teuchos::RCP<Epetra_Vector>& disi)
{
  if (HaveBeamContact())
  {
    double minimal_radius = beamcman_->GetMinEleRadius();
    double maxdisiscalefac =
        beamcman_->beam_contact_parameters().get<double>("BEAMS_MAXDISISCALEFAC");
    if (maxdisiscalefac > 0)
    {
      double disi_infnorm = 0.0;
      disi->NormInf(&disi_infnorm);

      while (disi_infnorm > maxdisiscalefac * minimal_radius)
      {
        if (myrank_ == 0)
          std::cout << "      Residual displacement scaled! (Minimal element radius: "
                    << minimal_radius << ")" << std::endl;

        disi->Scale(0.5);
        disi->NormInf(&disi_infnorm);
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*/
/* calculate characteristic/reference norms for displacements
 * originally by lw */
double STR::TimIntImpl::calc_ref_norm_displacement()
{
  // The reference norms are used to scale the calculated iterative
  // displacement norm and/or the residual force norm. For this
  // purpose we only need the right order of magnitude, so we don't
  // mind evaluating the corresponding norms at possibly different
  // points within the timestep (end point, generalized midpoint).

  double charnormdis = 0.0;
  if (pressure_ != Teuchos::null)
  {
    Teuchos::RCP<Epetra_Vector> disp = pressure_->ExtractOtherVector((*dis_)(0));
    charnormdis = STR::calculate_vector_norm(iternorm_, disp);
  }
  else
    charnormdis = STR::calculate_vector_norm(iternorm_, (*dis_)(0));

  // rise your hat
  return charnormdis;
}

/*----------------------------------------------------------------------*/
bool STR::TimIntImpl::Converged()
{
  // verify: #normcharforce_ has been delivered strictly larger than zero
  if (normcharforce_ <= 0.0)
  {
    FOUR_C_THROW("Characteristic force norm %g must be strictly larger than 0", normcharforce_);
  }
  // verify: #normchardis_ has been delivered strictly larger than zero
  if (normchardis_ <= 0.0)
  {
    FOUR_C_THROW(
        "Characteristic displacement norm %g must be strictly larger than 0", normchardis_);
  }


  // check for single norms
  bool convdis = false;
  bool convfres = false;

  // residual displacement
  switch (normtypedisi_)
  {
    case Inpar::STR::convnorm_abs:
      if (mor_->HaveMOR())
        convdis = normdisir_ < toldisi_;
      else
        convdis = normdisi_ < toldisi_;
      break;
    case Inpar::STR::convnorm_rel:
      convdis = normdisi_ < std::max(normchardis_ * toldisi_, 1e-15);
      break;
    case Inpar::STR::convnorm_mix:
      convdis = ((normdisi_ < toldisi_) or (normdisi_ < std::max(normchardis_ * toldisi_, 1e-15)));
      break;
    default:
      FOUR_C_THROW("Cannot check for convergence of residual displacements!");
      break;
  }

  // residual forces
  switch (normtypefres_)
  {
    case Inpar::STR::convnorm_abs:
      if (mor_->HaveMOR())
        convfres = normfresr_ < tolfres_;
      else
        convfres = normfres_ < tolfres_;
      break;
    case Inpar::STR::convnorm_rel:
      convfres = normfres_ < std::max(tolfres_ * normcharforce_, 1e-15);
      break;
    case Inpar::STR::convnorm_mix:
      convfres =
          ((normfres_ < tolfres_) or (normfres_ < std::max(tolfres_ * normcharforce_, 1e-15)));
      break;
    default:
      FOUR_C_THROW("Cannot check for convergence of residual forces!");
      break;
  }

  // check constraint
  bool cc = true;
  if (conman_->HaveConstraintLagr())
  {
    cc = normcon_ < tolcon_;
  }

  // check 0D cardiovascular model
  bool cv0d = true;
  bool cv0dincr = true;
  if (cardvasc0dman_->have_cardiovascular0_d())
  {
    cv0d = normcardvasc0d_ < tolcardvasc0d_;
    cv0dincr = normcardvasc0ddofincr_ < tolcardvasc0ddofincr_;
  }

  // check contact (active set)
  bool ccontact = true;
  if (have_contact_meshtying())
  {
    // check which case (application, strategy) we are in
    Inpar::CONTACT::SolvingStrategy stype =
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
            cmtbridge_->GetStrategy().Params(), "STRATEGY");
    bool semismooth =
        Core::UTILS::IntegralValue<int>(cmtbridge_->GetStrategy().Params(), "SEMI_SMOOTH_NEWTON");

    // only do this convergence check for semi-smooth Lagrange multiplier contact
    if (cmtbridge_->HaveContact() &&
        (stype == Inpar::CONTACT::solution_lagmult ||
            stype == Inpar::CONTACT::solution_augmented) &&
        semismooth)
      ccontact = cmtbridge_->GetStrategy().active_set_semi_smooth_converged();

    // add convergence check for saddlepoint formulations
    // use separate convergence checks for contact constraints and
    // LM increments
    if ((stype == Inpar::CONTACT::solution_lagmult || stype == Inpar::CONTACT::solution_augmented))
    {
      bool convDispLagrIncr = false;
      bool convDispWIncr = false;
      bool convDispWMIncr = false;

      switch (normtypeplagrincr_)
      {
        case Inpar::STR::convnorm_abs:
          convDispLagrIncr = normlagr_ < tollagr_;
          convDispWIncr = normw_ < 1e-12;    // WEAR
          convDispWMIncr = normwm_ < 1e-12;  // WEAR
          break;
        /*case Inpar::STR::convnorm_rel:
          convDispLagrIncr = normlagr_ < tollagr_;
          break;*/
        default:
          FOUR_C_THROW("Unknown norm type for Lagrange multiplier increment.");
          break;
      }

      // switch between "and" and "or"
      if (combdisilagr_ == Inpar::STR::bop_and)
        convdis = convdis and convDispLagrIncr and convDispWIncr and convDispWMIncr;
      else if (combdisilagr_ == Inpar::STR::bop_or)
        convdis = convdis or convDispLagrIncr;
      else
        FOUR_C_THROW("Something went terribly wrong with binary operator!");

      bool convResfContConstr = false;

      switch (normtypecontconstr_)
      {
        case Inpar::STR::convnorm_abs:
          convResfContConstr = normcontconstr_ < tolcontconstr_;
          break;
        /*case Inpar::STR::convnorm_rel:
          //convDispLagrIncr = normcontconstr_ < std::max(tolfres_*normcharforce_,1e-15);
          convResfContConstr = normcontconstr_ < tolcontconstr_;
          break;*/
        default:
          FOUR_C_THROW("You should not turn up here.");
          break;
      }

      // switch between "and" and "or"
      if (combfrescontconstr_ == Inpar::STR::bop_and)
        convfres = convfres and convResfContConstr;
      else if (combfrescontconstr_ == Inpar::STR::bop_or)
        convfres = convfres or convResfContConstr;
      else
        FOUR_C_THROW("Something went terribly wrong with binary operator!");
    }

  }  // end HaveMeshtyingContact()

  // pressure related stuff
  if (pressure_ != Teuchos::null)
  {
    bool convpre = false;
    bool convfpre = false;

    // pressure
    switch (normtypepres_)
    {
      case Inpar::STR::convnorm_abs:
        convpre = normpres_ < tolpres_;
        break;
      default:
        FOUR_C_THROW(
            "Cannot check for convergence of residual pressures! Only for absolute residuals "
            "implemeted so far!");
        break;
    }

    // incompressible residual
    switch (normtypepfres_)
    {
      case Inpar::STR::convnorm_abs:
        convfpre = normpfres_ < tolpfres_;
        break;
      default:
        FOUR_C_THROW("Cannot check for convergence of incompressible force residuals!");
        break;
    }


    // combine fields
    if (combdispre_ == Inpar::STR::bop_and)
      convdis = convdis and convpre;
    else if (combdispre_ == Inpar::STR::bop_or)
      convdis = convdis or convpre;
    else
      FOUR_C_THROW("Something went terribly wrong with binary operator!");

    if (combfrespfres_ == Inpar::STR::bop_and)
      convfres = convfres and convfpre;
    else if (combfrespfres_ == Inpar::STR::bop_or)
      convfres = convfres or convfpre;
    else
      FOUR_C_THROW("Something went terribly wrong with binary operator!");
  }

  // combine displacement-like and force-like residuals
  bool conv = false;
  if (combdisifres_ == Inpar::STR::bop_and)
    conv = convdis and convfres;
  else if (combdisifres_ == Inpar::STR::bop_or)
    conv = convdis or convfres;
  else
    FOUR_C_THROW("Something went terribly wrong with binary operator!");


  // return things
  return (conv and cc and cv0d and cv0dincr and ccontact);
}

/*----------------------------------------------------------------------*/
/* solve equilibrium */
Inpar::STR::ConvergenceStatus STR::TimIntImpl::Solve()
{
  // safety check
  check_is_init();
  check_is_setup();

  // things to be done before solving
  PreSolve();

  int nonlin_error = 0;
  // special nonlinear iterations for contact / meshtying
  if (have_contact_meshtying())
  {
    // check additionally if we have contact AND a Cardiovascular0D or constraint bc
    if (have_cardiovascular0_d())
      nonlin_error = cmt_windk_constr_nonlinear_solve();
    else
      nonlin_error = CmtNonlinearSolve();
  }

  // special nonlinear iterations for beam contact
  else if (HaveBeamContact())
  {
    // choose solution technique in accordance with user's will
    nonlin_error = beam_contact_nonlinear_solve();
  }

  // all other cases
  else
  {
    // choose solution technique in accordance with user's will
    switch (itertype_)
    {
      case Inpar::STR::soltech_newtonfull:
        nonlin_error = NewtonFull();
        break;
      case Inpar::STR::soltech_newtonls:
        nonlin_error = NewtonLS();
        break;
      case Inpar::STR::soltech_newtonuzawanonlin:
        nonlin_error = uzawa_non_linear_newton_full();
        break;
      case Inpar::STR::soltech_newtonuzawalin:
        nonlin_error = uzawa_linear_newton_full();
        break;
      case Inpar::STR::soltech_noxnewtonlinesearch:
      case Inpar::STR::soltech_noxgeneral:
        nonlin_error = NoxSolve();
        break;
      case Inpar::STR::soltech_ptc:
        nonlin_error = PTC();
        break;
      // catch problems
      default:
        FOUR_C_THROW("Solution technique \"%s\" is not implemented.",
            Inpar::STR::NonlinSolTechString(itertype_).c_str());
        break;
    }
  }

  // since it is possible that the nonlinear solution fails only on some procs
  // we need to communicate the error
  int lnonlin_error = nonlin_error;
  discretization()->Comm().MaxAll(&lnonlin_error, &nonlin_error, 1);

  Inpar::STR::ConvergenceStatus status = static_cast<Inpar::STR::ConvergenceStatus>(nonlin_error);

  // Only relevant, if the input parameter DIVERCONT is used and set to divcontype_ == adapt_step:
  // In this case, the time step size is halved as consequence of a non-converging nonlinear solver.
  // After a prescribed number of converged time steps, the time step is doubled again. The
  // following methods checks, if the time step size can be increased again.
  check_for_time_step_increase(status);
  check_for3_d0_dptc_reset(status);

  return status;
}
/*----------------------------------------------------------------------*/
/* solution with full Newton-Raphson iteration */
int STR::TimIntImpl::NewtonFull()
{
  // we do a Newton-Raphson iteration here.
  // the specific time integration has set the following
  // --> On #fres_ is the positive force residuum
  // --> On #stiff_ is the effective dynamic stiffness matrix

  // check whether we have a sanely filled stiffness matrix
  if (not stiff_->Filled())
  {
    FOUR_C_THROW("Effective stiffness matrix must be filled here");
  }

  if (outputeveryiter_)
  {
    int restart = Global::Problem::Instance()->Restart();
    if (stepn_ == (restart + 1)) outputcounter_ = 0;
    OutputEveryIter(true);
  }

  // initialise equilibrium loop
  iter_ = 1;
  normfres_ = CalcRefNormForce();
  // normdisi_ was already set in predictor; this is strictly >0
  timer_->reset();

  int element_error = 0;
  int linsolve_error = 0;
  // equilibrium iteration loop
  while (((not Converged() and (not linsolve_error) and (not element_error)) and
             (iter_ <= itermax_)) or
         (iter_ <= itermin_))
  {
    // make negative residual
    fres_->Scale(-1.0);

    // transform to local co-ordinate systems
    if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);

    // STC preconditioning
    STCPreconditioning();

    // apply Dirichlet BCs to system of equations
    disi_->PutScalar(0.0);  // Useful? depends on solver and more
    if (get_loc_sys_trafo() != Teuchos::null)
    {
      Core::LinAlg::apply_dirichlet_to_system(
          *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
          *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
    }
    else
    {
      Core::LinAlg::apply_dirichlet_to_system(
          *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
    }

    // *********** time measurement ***********
    double dtcpu = timer_->wallTime();
    // *********** time measurement ***********

    // solve for disi_
    // Solve K_Teffdyn . IncD = -R  ===>  IncD_{n+1}
    Core::LinAlg::SolverParams solver_params;
    if (solveradapttol_ and (iter_ > 1))
    {
      solver_params.nonlin_tolerance = tolfres_;
      solver_params.nonlin_residual = normfres_;
      solver_params.lin_tol_better = solveradaptolbetter_;
    }

    // linear solver call (contact / meshtying case or default)
    if (have_contact_meshtying())
      CmtLinearSolve();
    else
    {
      solver_params.refactor = true;
      solver_params.reset = iter_ == 1;
      solver_params.projector = projector_;
      linsolve_error = solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
      // check for problems in linear solver
      // however we only care about this if we have a fancy divcont action (meaning function will
      // return 0 )
      linsolve_error = LinSolveErrorCheck(linsolve_error);
    }
    solver_->ResetTolerance();

    // In beam contact applications it can be necessary to limit the Newton step size (scaled
    // residual displacements)
    limit_stepsize_beam_contact(disi_);

    // recover standard displacements
    RecoverSTCSolution();

    // recover contact / meshtying Lagrange multipliers
    if (have_contact_meshtying()) cmtbridge_->Recover(disi_);

    // *********** time measurement ***********
    dtsolve_ = timer_->wallTime() - dtcpu;
    // *********** time measurement ***********

    // update end-point displacements etc
    UpdateIter(iter_);

    if (outputeveryiter_) OutputEveryIter(true);

    // create empty parameter list
    Teuchos::ParameterList params;

    // set flag for element error in form of a negative Jacobian determinant
    // in parameter list in case of potential continuation
    if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
    {
      params.set<bool>("tolerate_errors", true);
      params.set<bool>("eval_error", false);
    }

    // compute residual forces #fres_ and stiffness #stiff_
    // whose components are globally oriented
    evaluate_force_stiff_residual(params);

    // check for element error in form of a negative Jacobian determinant
    // in case of potential continuation
    if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
      element_error = ElementErrorCheck(params.get<bool>("eval_error"));

    // blank residual at (locally oriented) Dirichlet DOFs
    // rotate to local co-ordinate systems
    if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

    // extract reaction forces
    // reactions are negative to balance residual on DBC
    freact_->Update(-1.0, *fres_, 0.0);
    dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
    // rotate reaction forces back to global co-ordinate system
    if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

    // blank residual at DOFs on Dirichlet BC
    dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
    // rotate back to global co-ordinate system
    if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

    // cancel in residual those forces that would excite rigid body modes and
    // that thus vanish in the Krylov space projection
    if (projector_ != Teuchos::null) projector_->ApplyPT(*fres_);

    // decide which norms have to be evaluated
    bool bPressure = pressure_ != Teuchos::null;
    bool bContactSP = (have_contact_meshtying() &&
                       ((Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
                             cmtbridge_->GetStrategy().Params(), "STRATEGY") ==
                                Inpar::CONTACT::solution_lagmult &&
                            (Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
                                 cmtbridge_->GetStrategy().Params(), "SYSTEM") !=
                                    Inpar::CONTACT::system_condensed ||
                                Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
                                    cmtbridge_->GetStrategy().Params(), "SYSTEM") !=
                                    Inpar::CONTACT::system_condensed_lagmult)) ||
                           (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
                                cmtbridge_->GetStrategy().Params(), "STRATEGY") ==
                               Inpar::CONTACT::solution_augmented)));

    if (bPressure && bContactSP)
      FOUR_C_THROW(
          "We only support either contact/meshtying in saddlepoint formulation or structure with "
          "pressure DOFs");
    if (bPressure == false && bContactSP == false)
    {
      // build residual force norm
      normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
      // build residual displacement norm
      normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
    }
    if (bPressure)
    {
      Teuchos::RCP<Epetra_Vector> pres = pressure_->ExtractCondVector(fres_);
      Teuchos::RCP<Epetra_Vector> disp = pressure_->ExtractOtherVector(fres_);
      normpfres_ = STR::calculate_vector_norm(iternorm_, pres);
      normfres_ = STR::calculate_vector_norm(iternorm_, disp);

      pres = pressure_->ExtractCondVector(disi_);
      disp = pressure_->ExtractOtherVector(disi_);
      normpres_ = STR::calculate_vector_norm(iternorm_, pres);
      normdisi_ = STR::calculate_vector_norm(iternorm_, disp);
    }
    if (bContactSP)
    {
      // extract subvectors (for mt and contact use only contact lm)
      Teuchos::RCP<Epetra_Vector> lagrincr = cmtbridge_->GetStrategy().LagrMultSolveIncr();
      Teuchos::RCP<Epetra_Vector> constrrhs = cmtbridge_->GetStrategy().ConstrRhs();

      // build residual force norm
      normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
      // build residual displacement norm
      normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
      // build residual constraint norm
      if (constrrhs != Teuchos::null)
        normcontconstr_ = STR::calculate_vector_norm(iternorm_, constrrhs);
      else
        normcontconstr_ = -1.0;

      // build lagrange multiplier increment norm
      if (lagrincr != Teuchos::null)
        normlagr_ = STR::calculate_vector_norm(iternorm_, lagrincr);
      else
        normlagr_ = -1.0;

      // for wear discretization
      Inpar::Wear::WearType wtype = Core::UTILS::IntegralValue<Inpar::Wear::WearType>(
          cmtbridge_->GetStrategy().Params(), "WEARTYPE");
      Inpar::Wear::WearSide wside = Core::UTILS::IntegralValue<Inpar::Wear::WearSide>(
          cmtbridge_->GetStrategy().Params(), "WEAR_SIDE");

      if (wtype == Inpar::Wear::wear_primvar)
      {
        Teuchos::RCP<Epetra_Vector> wincr = cmtbridge_->GetStrategy().WSolveIncr();
        Teuchos::RCP<Epetra_Vector> wearrhs = cmtbridge_->GetStrategy().WearRhs();

        if (wearrhs != Teuchos::null)
          normwrhs_ = STR::calculate_vector_norm(iternorm_, wearrhs);
        else
          normwrhs_ = -1.0;

        if (wincr != Teuchos::null)
          normw_ = STR::calculate_vector_norm(iternorm_, wincr);
        else
          normw_ = -1.0;

        if (wside == Inpar::Wear::wear_both)
        {
          Teuchos::RCP<Epetra_Vector> wmincr = cmtbridge_->GetStrategy().WMSolveIncr();
          Teuchos::RCP<Epetra_Vector> wearmrhs = cmtbridge_->GetStrategy().WearMRhs();

          if (wearmrhs != Teuchos::null)
            normwmrhs_ = STR::calculate_vector_norm(iternorm_, wearmrhs);
          else
            normwmrhs_ = -1.0;

          if (wmincr != Teuchos::null)
            normwm_ = STR::calculate_vector_norm(iternorm_, wmincr);
          else
            normwm_ = -1.0;
        }
        else
        {
          normwm_ = 0.0;
          normwmrhs_ = 0.0;
        }
      }
      else
      {
        normw_ = 0.0;
        normwrhs_ = 0.0;
        normwm_ = 0.0;
        normwmrhs_ = 0.0;
      }
    }

    // print stuff
    print_newton_iter();

    // increment equilibrium loop index
    iter_ += 1;
  }  // end equilibrium loop

  // correct iteration counter
  iter_ -= 1;

  // call monitor
  if (conman_->HaveMonitor())
  {
    conman_->compute_monitor_values(disn_);
  }

  // do nonlinear solver error check
  return newton_full_error_check(linsolve_error, element_error);
}

/*----------------------------------------------------------------------*/
/* error check for full Newton problems */
int STR::TimIntImpl::newton_full_error_check(int linerror, int eleerror)
{
  // if everything is fine print to screen and return
  if (Converged())
  {
    if (myrank_ == 0) print_newton_conv();
    return 0;
  }
  // now some error checks: do we have an element problem
  // only check if we continue in this case; other wise, we ignore the error
  if (eleerror and divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
  {
    return eleerror;
  }
  // do we have a problem in the linear solver
  // only check if we want to do something fancy other wise we ignore the error in the linear solver
  else if (linerror and (divcontype_ == Inpar::STR::divcont_halve_step or
                            divcontype_ == Inpar::STR::divcont_adapt_step or
                            divcontype_ == Inpar::STR::divcont_rand_adapt_step or
                            divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                            divcontype_ == Inpar::STR::divcont_repeat_step or
                            divcontype_ == Inpar::STR::divcont_repeat_simulation or
                            divcontype_ == Inpar::STR::divcont_adapt_penaltycontact))
  {
    return linerror;
  }
  else
  {
    if ((iter_ >= itermax_) and (divcontype_ == Inpar::STR::divcont_stop))
    {
      // write restart output of last converged step before stopping
      output(true);

      FOUR_C_THROW("Newton unconverged in %d iterations", iter_);
      return 1;
    }
    else if ((iter_ >= itermax_) and (divcontype_ == Inpar::STR::divcont_continue))
    {
      if (myrank_ == 0)
        Core::IO::cout << "Newton unconverged in " << iter_ << " iterations, continuing"
                       << Core::IO::endl;
      return 0;
    }
    else if ((iter_ >= itermax_) and
             (divcontype_ == Inpar::STR::divcont_halve_step or
                 divcontype_ == Inpar::STR::divcont_adapt_step or
                 divcontype_ == Inpar::STR::divcont_rand_adapt_step or
                 divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                 divcontype_ == Inpar::STR::divcont_repeat_step or
                 divcontype_ == Inpar::STR::divcont_repeat_simulation or
                 divcontype_ == Inpar::STR::divcont_adapt_penaltycontact))
    {
      if (myrank_ == 0)
        Core::IO::cout << "Newton unconverged in " << iter_ << " iterations " << Core::IO::endl;
      return 1;
    }
  }
  FOUR_C_THROW("Fatal error in NonLinSolveErrorCheck, case not implemented ");
  return 0;
}

/*----------------------------------------------------------------------*/
/* error check for linear solver problems */
int STR::TimIntImpl::LinSolveErrorCheck(int linerror)
{
  // we only care about problems in the linear solver if we have a fancy divcont action
  if (linerror and (divcontype_ == Inpar::STR::divcont_halve_step or
                       divcontype_ == Inpar::STR::divcont_adapt_step or
                       divcontype_ == Inpar::STR::divcont_rand_adapt_step or
                       divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                       divcontype_ == Inpar::STR::divcont_repeat_step or
                       divcontype_ == Inpar::STR::divcont_repeat_simulation or
                       divcontype_ == Inpar::STR::divcont_adapt_penaltycontact or
                       divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err))
  {
    if (myrank_ == 0) Core::IO::cout << "Linear solver is having trouble " << Core::IO::endl;
    return 2;
  }
  else
  {
    return 0;
  }
}

/*----------------------------------------------------------------------*/
/* error check for element problems in form of a negative Jacobian determinant */
int STR::TimIntImpl::ElementErrorCheck(bool evalerr)
{
  // merly care about element problems if there is a fancy divcont action
  // and element errors are considered
  if (evalerr and (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                      divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err))
  {
    if (myrank_ == 0)
      Core::IO::cout << "Element error in form of a negative Jacobian determinant "
                     << Core::IO::endl;
    return 3;
  }
  else
  {
    return 0;
  }
}

/*---------------------------------------------------------------------*/
/* solution with line search algorithm                  hiermeier 08/13*/
/*---------------------------------------------------------------------*/
int STR::TimIntImpl::NewtonLS()
{
  // The specific time integration has set the following
  // --> On #fres_ is the positive force residuum
  // --> On #stiff_ is the effective dynamic stiffness matrix

  int linsolve_error = 0;
  int fscontrol = 0;  // integer for a first step control (equal 1: deactivation) // fixme check if
                      // this is necessary for structural mechanics
  bool eval_error = false;  // an error occurred in the structure evaluation

  // check whether we have a sanely filled stiffness matrix
  if (not stiff_->Filled()) FOUR_C_THROW("Effective stiffness matrix must be filled here");

  if (outputeveryiter_)
  {
    int restart = Global::Problem::Instance()->Restart();
    if (stepn_ == (restart + 1)) outputcounter_ = 0;
    OutputEveryIter(true);
  }

  // initialize equilibrium loop (outer Full Newton loop)
  iter_ = 1;
  normfres_ = CalcRefNormForce();
  // normdisi_ was already set in predictor; this is strictly >0
  timer_->reset();

  // Merit function at current stage and for ls step
  std::vector<double> merit_fct(2);

  // Temporal copies of different vectors. Necessary for the sufficient decrease check.
  Teuchos::RCP<Epetra_Vector> tdisn = Teuchos::rcp(new Epetra_Vector(*disn_));
  Teuchos::RCP<Epetra_Vector> tveln = Teuchos::rcp(new Epetra_Vector(*veln_));
  Teuchos::RCP<Epetra_Vector> taccn = Teuchos::rcp(new Epetra_Vector(*accn_));

  // equilibrium iteration loop (outer full Newton loop)
  while (
      ((not Converged() and (not linsolve_error)) and (iter_ <= itermax_)) or (iter_ <= itermin_))
  {
    // initialize the Newton line search iteration counter
    int iter_ls = 0;
    double step_red = 1.0;

    /**************************************************************
    ***           Save successful iteration state               ***
    ***************************************************************/

    // It's necessary to save a temporal copy of the end-point displacements,
    // before any update is performed (because of the pseudo energy norm):
    tdisn->Update(1.0, *disn_, 0.0);  // copy of the displ vector
    tveln->Update(1.0, *veln_, 0.0);  // copy of the velocity vector
    taccn->Update(1.0, *accn_, 0.0);  // copy of the acceleration vector

    /**************************************************************
    ***                       Solver Call                       ***
    ***************************************************************/
    linsolve_error = LsSolveNewtonStep();

    // Evaluate merit function
    if (iter_ == 1)
      LsEvalMeritFct(merit_fct[0]);
    else
      merit_fct[0] = merit_fct[1];

    // Check if pred_constdis is used. If yes, the first step is not controlled.
    if (pred_ == Inpar::STR::pred_constdis or pred_ == Inpar::STR::pred_constdisvelacc)
      fscontrol = 1;
    else if ((pred_ == Inpar::STR::pred_tangdis || pred_ == Inpar::STR::pred_constacc ||
                 pred_ == Inpar::STR::pred_constvel) ||
             (iter_ > 1))
      fscontrol = 0;
    else
      FOUR_C_THROW(
          "The behavior of the chosen predictor is not yet tested in the line search framework.");

    /**************************************************************
    ***      Update right-hand side and stiffness matrix        ***
    ***************************************************************/
    Teuchos::ParameterList params;
    params.set<bool>("tolerate_errors", true);
    params.set<bool>("eval_error", false);
    if (fresn_str_ != Teuchos::null)
    {
      // attention: though it is called rhs_norm it actually contains sum x_i^2, i.e. the square of
      // the L2-norm
      params.set<double>("cond_rhs_norm", 0.);
      // need to know the processor id
      params.set<int>("MyPID", myrank_);
    }
    {
      int exceptcount = 0;
#ifdef FOUR_C_ENABLE_FE_TRAPPING
      fedisableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
#endif
      evaluate_force_stiff_residual(params);
#ifdef FOUR_C_ENABLE_FE_TRAPPING
      if (fetestexcept(FE_INVALID) || fetestexcept(FE_OVERFLOW) || fetestexcept(FE_DIVBYZERO) ||
          params.get<bool>("eval_error") == true)
        exceptcount = 1;
#endif
      int tmp = 0;
      discret_->Comm().SumAll(&exceptcount, &tmp, 1);
      if (tmp) eval_error = true;
#ifdef FOUR_C_ENABLE_FE_TRAPPING
      feclearexcept(FE_ALL_EXCEPT);
      feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
#endif
    }

    // get residual of condensed variables (e.g. EAS) for NewtonLS
    if (fresn_str_ != Teuchos::null)
    {
      double loc = params.get<double>("cond_rhs_norm");
      discret_->Comm().SumAll(&loc, &cond_res_, 1);
    }

    // blank residual at (locally oriented) Dirichlet DOFs
    // rotate to local co-ordinate systems
    if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

    // extract reaction forces
    // reactions are negative to balance residual on DBC
    freact_->Update(-1.0, *fres_, 0.0);
    dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
    // rotate reaction forces back to global co-ordinate system
    if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

    // blank residual at DOFs on Dirichlet BC
    dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
    // rotate back to global co-ordinate system
    if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

    // cancel in residual those forces that would excite rigid body modes and
    // that thus vanish in the Krylov space projection
    if (projector_ != Teuchos::null) projector_->ApplyPT(*fres_);

    /**************************************************************
    ***           merit function (current iteration)            ***
    ***************************************************************/
    int err = LsEvalMeritFct(merit_fct[1]);
    eval_error = (eval_error || err);

    if (outputeveryiter_) OutputEveryIter(true);

    /**************************************************************
    ***          1st inner LINE SEARCH loop                     ***
    ***************************************************************/

    while ((iter_ - fscontrol > 0) &&
           ((!LsConverged(merit_fct.data(), step_red) || eval_error) && (iter_ls < ls_maxiter_)))
    {
      /**************************************************************
      ***           Display line search information               ***
      ***************************************************************/
      if (iter_ls == 0) ls_print_line_search_iter(merit_fct.data(), iter_ls, step_red);

      // increase inner loop count
      ++iter_ls;

      /**************************************************************
      ***                   Step size control                     ***
      ***************************************************************/
      step_red *= alpha_ls_;
      // >>>> displacement, velocity, acceleration <<<<<<<<<<<<<<<
      // scale displ. increment
      disi_->Scale(alpha_ls_);
      // load old displ. vector
      disn_->Update(1.0, *tdisn, 0.0);
      // load old vel. vector
      veln_->Update(1.0, *tveln, 0.0);
      // load old acc. vector
      accn_->Update(1.0, *taccn, 0.0);

      // Update nodal displ., vel., acc., etc.
      UpdateIter(iter_);
      /**************************************************************
      ***   Update right-hand side (and part. stiffness matrix)   ***
      ***************************************************************/
      ls_update_structural_rh_sand_stiff(eval_error, merit_fct[1]);

      /**************************************************************
      ***           Display line search information               ***
      ***************************************************************/
      ls_print_line_search_iter(merit_fct.data(), iter_ls, step_red);

      if (!(eval_error) && (outputeveryiter_)) OutputEveryIter(true, true);
    }

    if (iter_ls != 0)
    {
      if ((myrank_ == 0) and printscreen_ and (StepOld() % printscreen_ == 0) and printiter_)
      {
        std::ostringstream oss;
        std::string dashline;
        dashline.assign(64, '-');
        oss << dashline;

        fprintf(stdout, "%s\n", oss.str().c_str());
        fflush(stdout);
      }
    }

    /**************************************************************
    ***      Print Newton Step information                      ***
    ***************************************************************/

    // build residual force norm
    normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
    // build residual displacement norm
    normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);

    print_newton_iter();

    // increment equilibrium loop index
    iter_ += 1;
  }  // end equilibrium loop

  // correct iteration counter
  iter_ -= 1;

  // call monitor
  if (conman_->HaveMonitor()) conman_->compute_monitor_values(disn_);

  // do nonlinear solver error check
  return newton_full_error_check(linsolve_error, 0);
}


/*----------------------------------------------------------------------*/
/*   Solver Call (line search)                          hiermeier 09/13 */
/*----------------------------------------------------------------------*/
int STR::TimIntImpl::LsSolveNewtonStep()
{
  int linsolve_error = 0;
  /**************************************************************
  ***           Prepare the solution procedure                ***
  ***************************************************************/
  // make negative residual
  fres_->Scale(-1.0);

  // transform to local co-ordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);

  // STC preconditioning
  STCPreconditioning();

  // apply Dirichlet BCs to system of equations
  disi_->PutScalar(0.0);  // Useful? depends on solver and more
  if (get_loc_sys_trafo() != Teuchos::null)
  {
    Core::LinAlg::apply_dirichlet_to_system(
        *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
        *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
  }
  else
  {
    Core::LinAlg::apply_dirichlet_to_system(
        *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
  }

  /**************************************************************
  ***                     Solver Call                         ***
  ***************************************************************/
  // *********** time measurement ***********
  double dtcpu = timer_->wallTime();
  // *********** time measurement ***********

  // solve for disi_
  // Solve K_Teffdyn . IncD = -R  ===>  IncD_{n+1}
  Core::LinAlg::SolverParams solver_params;
  if (solveradapttol_ and (iter_ > 1))
  {
    solver_params.nonlin_tolerance = tolfres_;
    solver_params.nonlin_residual = normfres_;
    solver_params.lin_tol_better = solveradaptolbetter_;
  }


  solver_params.refactor = iter_ == 1;
  solver_params.reset = true;
  solver_params.projector = projector_;
  linsolve_error = solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
  // check for problems in linear solver
  // however we only care about this if we have a fancy divcont action (meaning function will return
  // 0 )
  linsolve_error = LinSolveErrorCheck(linsolve_error);

  // In beam contact applications it can be necessary to limit the Newton step size (scaled residual
  // displacements)
  limit_stepsize_beam_contact(disi_);

  solver_->ResetTolerance();

  // recover standard displacements
  RecoverSTCSolution();

  // *********** time measurement ***********
  dtsolve_ = timer_->wallTime() - dtcpu;
  // *********** time measurement ***********

  // update end-point displacements etc
  UpdateIter(iter_);

  return (linsolve_error);
}

/*----------------------------------------------------------------------*/
/*   Update structural RHS and stiff (line search)      hiermeier 09/13 */
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::ls_update_structural_rh_sand_stiff(bool& isexcept, double& merit_fct)
{
  // --- Checking for floating point exceptions
#ifdef FOUR_C_ENABLE_FE_TRAPPING
  fedisableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
#endif

  // compute residual forces #fres_ and stiffness #stiff_
  // whose components are globally oriented
  int exceptcount = 0;
  Teuchos::ParameterList params;
  // elements may tolerate errors usually leading to dserrors
  // in such cases the elements force the line search to reduce
  // the step size by setting "eval_error" to true
  params.set<bool>("tolerate_errors", true);
  params.set<bool>("eval_error", false);
  // condensed degrees of freedom need to know the step reduction
  params.set<double>("alpha_ls", alpha_ls_);
  // line search needs to know the residuals of additional condensed dofs
  if (fresn_str_ != Teuchos::null)
  {
    params.set<double>("cond_rhs_norm", 0.);
    // need to know the processor id
    params.set<int>("MyPID", myrank_);
  }
  evaluate_force_stiff_residual(params);

  // get residual of condensed variables (e.g. EAS) for NewtonLS
  if (fresn_str_ != Teuchos::null)
  {
    double loc = params.get<double>("cond_rhs_norm");
    discret_->Comm().SumAll(&loc, &cond_res_, 1);
  }

#ifdef FOUR_C_ENABLE_FE_TRAPPING
  if (fetestexcept(FE_INVALID) || fetestexcept(FE_OVERFLOW) || fetestexcept(FE_DIVBYZERO) ||
      params.get<bool>("eval_error") == true)
    exceptcount = 1;
#endif

  // synchronize the exception flag isexcept on all processors
  int exceptsum = 0;
  discret_->Comm().SumAll(&exceptcount, &exceptsum, 1);
  if (exceptsum > 0)
    isexcept = true;
  else
    isexcept = false;

#ifdef FOUR_C_ENABLE_FE_TRAPPING
  feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
  feclearexcept(FE_ALL_EXCEPT);
#endif
  // blank residual at (locally oriented) Dirichlet DOFs
  // rotate to local co-ordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

  // extract reaction forces
  // reactions are negative to balance residual on DBC
  freact_->Update(-1.0, *fres_, 0.0);
  dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
  // rotate reaction forces back to global co-ordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

  // blank residual at DOFs on Dirichlet BC
  dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
  // rotate back to global co-ordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

  // cancel in residual those forces that would excite rigid body modes and
  // that thus vanish in the Krylov space projection
  if (projector_ != Teuchos::null) projector_->ApplyPT(*fres_);

  /**************************************************************
  ***          merit function (current iteration)             ***
  ***************************************************************/
  int err = LsEvalMeritFct(merit_fct);
  isexcept = (isexcept || err);

  return;
}


/*----------------------------------------------------------------------*/
/*   Evaluate the merit function (line search)          hiermeier 08/13 */
/*----------------------------------------------------------------------*/
int STR::TimIntImpl::LsEvalMeritFct(double& merit_fct)
{
#ifdef FOUR_C_ENABLE_FE_TRAPPING
  fedisableexcept(FE_OVERFLOW);
#endif
  int err = 0;
  // Calculate the quadratic norm of the right-hand side as merit function
  // Calculate the merit function value: (1/2) * <RHS,RHS>
  if (fresn_str_ == Teuchos::null)
  {
    err = fres_->Dot(*fres_, &merit_fct);
  }
  else
  {
    merit_fct = 0.;
    err = fresn_str_->Dot(*fresn_str_, &merit_fct);
    merit_fct += cond_res_;
  }
  merit_fct *= 0.5;

  int exceptcount = 0;
#ifdef FOUR_C_ENABLE_FE_TRAPPING
  if (fetestexcept(FE_OVERFLOW)) exceptcount = 1;
#endif
  int exceptsum = 0;
  discret_->Comm().SumAll(&exceptcount, &exceptsum, 1);
  if (exceptsum != 0) return err;
#ifdef FOUR_C_ENABLE_FE_TRAPPING
  feclearexcept(FE_ALL_EXCEPT);
  feenableexcept(FE_OVERFLOW);
#endif

  return 0;
}

/*----------------------------------------------------------------------*/
/*   Print information about the last line search step  hiermeier 09/13 */
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::ls_print_line_search_iter(double* mf_value, int iter_ls, double step_red)
{
  normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
  // print to standard out
  if ((myrank_ == 0) and printscreen_ and (StepOld() % printscreen_ == 0) and printiter_)
  {
    std::ostringstream oss;
    if (iter_ls == 0)
    {
      std::string dashline;
      dashline.assign(64, '-');
      oss << dashline << std::endl;
      oss << std::setw(6) << "ls_iter";
      oss << std::setw(16) << "step_scale";
      oss << std::setw(16) << "abs-dis-norm";
      oss << std::setw(16) << "merit-fct";
      oss << std::setw(10) << "te";
      oss << std::endl;
    }

    oss << std::setw(7) << iter_ls;
    oss << std::setw(16) << std::setprecision(5) << std::scientific << step_red;
    // build residual displacement norm
    oss << std::setw(16) << std::setprecision(5) << std::scientific << normdisi_;
    if (iter_ls == 0)
      oss << std::setw(16) << std::setprecision(5) << std::scientific << mf_value[0];
    else
      oss << std::setw(16) << std::setprecision(5) << std::scientific << mf_value[1];
    oss << std::setw(10) << std::setprecision(2) << std::scientific << dtele_;

    // finish oss
    oss << std::ends;

    fprintf(stdout, "%s\n", oss.str().c_str());
    fflush(stdout);
  }

  // see you
  return;
}

/*----------------------------------------------------------------------*/
/*   Inner convergence check (line search)              hiermeier 08/13 */
/*----------------------------------------------------------------------*/
bool STR::TimIntImpl::LsConverged(double* mf_value, double step_red)
{
  bool check_ls_mf = false;

  /**************************************************************
  ***           Check for sufficient descent                  ***
  ***************************************************************/
  // mf_value[1]: NEW merit function value
  //            --> f(x + alpha_ls * dx)
  // mf_value[0]: OLD merit function value (initial value at the beginning of the time step
  //              or function value of the last converged iteration step. Converged means that
  //              the last step fulfilled the LsConverged test.)
  //            --> f(x)
  // The check follows to
  //            f(x + alpha_ls * dx) - f(x) <= - 2 * sigma_ls * step_red_ * f(x).
  check_ls_mf = ((mf_value[1] - mf_value[0]) <= -2.0 * sigma_ls_ * step_red * mf_value[0]);


  return (check_ls_mf);
}


/*----------------------------------------------------------------------*/
/* do non-linear Uzawa iteration within a full NRI is called,
 * originally by tk */
int STR::TimIntImpl::uzawa_non_linear_newton_full()
{
  // now or never, break it
  FOUR_C_THROW(
      "Sorry dude, non-linear Uzawa with full Newton-Raphson"
      " iteration is available in source, but it has not been"
      " tested in silico and should not be used overcredulously."
      " Feel free to remove this FOUR_C_THROW but be careful and check"
      " if things run as expected.");

  // do Newton-Raphson iteration, which contains here effects of
  // constraint forces and stiffness
  // this call ends up with new displacements etc on \f$D_{n+1}\f$ etc
  int error = NewtonFull();
  if (error) return error;

  // compute constraint error ...
  conman_->compute_error(timen_, disn_);
  // ... and its norm
  normcon_ = conman_->GetErrorNorm();
  // talk to user
  if (myrank_ == 0)
  {
    std::cout << "Constraint error for Newton solution: " << normcon_ << std::endl;
  }

  // Uzawa iteration loop
  int uziter = 0;
  while ((normcon_ > tolcon_) and (uziter <= uzawaitermax_))
  {
    // Lagrange multiplier is increased by #uzawaparam_ times ConstrError
    conman_->UpdateLagrMult(uzawaparam_);

    // Keep new Lagrange multiplier fixed and solve for new displacements

    // REALLY NECESSARY, OR EVEN COUNTERPRODUCTIVE ???
    Predict();

    // do Newton-Raphson iteration, which contains here effects of
    // constraint forces and stiffness
    // this call ends up with new displacements etc on \f$D_{n+1}\f$ etc
    int error = NewtonFull();
    if (error) return error;


    // compute constraint error ...
    conman_->compute_error(timen_, disn_);
    // ... and its norm
    normcon_ = conman_->GetErrorNorm();
    // talk to user
    if (myrank_ == 0)
    {
      std::cout << "Constraint error for computed displacement: " << normcon_ << std::endl;
    }

    // increment loop counter
    uziter += 1;
  }

  // for output
  iter_ = uziter + 1;
  return 0;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::update_step_constraint()
{
  if (conman_->HaveConstraint()) conman_->update();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::update_step_cardiovascular0_d()
{
  if (cardvasc0dman_->have_cardiovascular0_d())
  {
    cardvasc0dman_->UpdateTimeStep();
    if (cardvasc0dman_->GetIsPeriodic())
    {
      SetTimeEnd(timen_);
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::update_step_spring_dashpot()
{
  if (springman_->HaveSpringDashpot()) springman_->update();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool STR::TimIntImpl::HaveConstraint() { return conman_->HaveConstraintLagr(); }

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool STR::TimIntImpl::have_cardiovascular0_d() { return cardvasc0dman_->have_cardiovascular0_d(); }

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool STR::TimIntImpl::HaveSpringDashpot() { return springman_->HaveSpringDashpot(); }

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::update_iter_incr_constr(
    Teuchos::RCP<Epetra_Vector> lagrincr  ///< Lagrange multiplier increment
)
{
  conman_->UpdateLagrMult(lagrincr);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void STR::TimIntImpl::update_iter_incr_cardiovascular0_d(
    Teuchos::RCP<Epetra_Vector> cv0ddofincr  ///< wk dof increment
)
{
  cardvasc0dman_->UpdateCv0DDof(cv0ddofincr);
}

/*----------------------------------------------------------------------*/
/* do linearised Uzawa iterations with full NRI
 * originally by tk 11/07 */
int STR::TimIntImpl::uzawa_linear_newton_full()
{
  int linsolve_error = 0;
  int element_error = 0;
  if (conman_->HaveConstraint())
  {
    // allocate additional vectors and matrices
    Teuchos::RCP<Epetra_Vector> conrhs = Teuchos::rcp(new Epetra_Vector(*(conman_->GetError())));

    Teuchos::RCP<Epetra_Vector> lagrincr =
        Teuchos::rcp(new Epetra_Vector(*(conman_->GetConstraintMap())));

    // check whether we have a sanely filled stiffness matrix
    if (not stiff_->Filled())
    {
      FOUR_C_THROW("Effective stiffness matrix must be filled here");
    }

    // initialise equilibrium loop
    iter_ = 1;
    normfres_ = CalcRefNormForce();
    // normdisi_ was already set in predictor; this is strictly >0
    normcon_ = conman_->GetErrorNorm();
    timer_->reset();

    // equilibrium iteration loop
    while (((not Converged() and (not linsolve_error) and (not element_error)) and
               (iter_ <= itermax_)) or
           (iter_ <= itermin_))
    {
      // make negative residual
      fres_->Scale(-1.0);

      //    // uncomplete stiffness matrix, so stuff can be inserted again
      //    stiff_->UnComplete();

      // transform to local co-ordinate systems
      if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);

      // apply Dirichlet BCs to system of equations
      disi_->PutScalar(0.0);  // Useful? depends on solver and more
      if (get_loc_sys_trafo() != Teuchos::null)
      {
        Core::LinAlg::apply_dirichlet_to_system(
            *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
            *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
      }
      else
      {
        Core::LinAlg::apply_dirichlet_to_system(
            *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
      }

      // prepare residual Lagrange multiplier
      lagrincr->PutScalar(0.0);

      // *********** time measurement ***********
      double dtcpu = timer_->wallTime();
      // *********** time measurement ***********

      // Use STC preconditioning on system matrix
      STCPreconditioning();

      // get constraint matrix with and without Dirichlet zeros
      Teuchos::RCP<Core::LinAlg::SparseMatrix> constr =
          (Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(conman_->GetConstrMatrix()));
      Teuchos::RCP<Core::LinAlg::SparseMatrix> constrT =
          Teuchos::rcp(new Core::LinAlg::SparseMatrix(*constr));

      constr->ApplyDirichlet(*(dbcmaps_->CondMap()), false);

      // Apply STC on constraint matrices of desired
      if (stcscale_ != Inpar::STR::stc_none)
      {
        // std::cout<<"scaling constraint matrices"<<std::endl;
        constrT = Core::LinAlg::MLMultiply(*stcmat_, true, *constrT, false, false, false, true);
        if (stcscale_ == Inpar::STR::stc_currsym)
        {
          constr = Core::LinAlg::MLMultiply(*stcmat_, true, *constr, false, false, false, true);
        }
      }
      // Call constraint solver to solve system with zeros on diagonal
      consolv_->Solve(system_matrix(), constr, constrT, disi_, lagrincr, fres_, conrhs);

      // recover unscaled solution
      RecoverSTCSolution();

      // *********** time measurement ***********
      dtsolve_ = timer_->wallTime() - dtcpu;
      // *********** time measurement ***********

      // transform back to global co-ordinate system
      if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(disi_);

      // update Lagrange multiplier
      conman_->UpdateLagrMult(lagrincr);
      // update end-point displacements etc
      UpdateIter(iter_);

      // create parameter list
      Teuchos::ParameterList params;

      // set flag for element error in form of a negative Jacobian determinant
      // in parameter list in case of potential continuation
      if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
      {
        params.set<bool>("tolerate_errors", true);
        params.set<bool>("eval_error", false);
      }

      // compute residual forces #fres_ and stiffness #stiff_
      // which contain forces and stiffness of constraints
      evaluate_force_stiff_residual(params);

      // check for element error in form of a negative Jacobian determinant
      // in case of potential continuation
      if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
        element_error = ElementErrorCheck(params.get<bool>("eval_error"));

      // compute residual and stiffness of constraint equations
      conrhs = Teuchos::rcp(new Epetra_Vector(*(conman_->GetError())));

      // blank residual at (locally oriented) Dirichlet DOFs
      // rotate to local co-ordinate systems
      if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

      // extract reaction forces
      // reactions are negative to balance residual on DBC
      freact_->Update(-1.0, *fres_, 0.0);
      dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
      // rotate reaction forces back to global co-ordinate system
      if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

      // blank residual at DOFs on Dirichlet BC
      dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
      // rotate back to global co-ordinate system
      if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

      // why was this here? part of else statement below!!! (mhv 01/2015)
      //      // build residual force norm
      //      normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
      //      // build residual displacement norm
      //      normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
      //      // build residual Lagrange multiplier norm
      //      normcon_ = conman_->GetErrorNorm();

      if (pressure_ != Teuchos::null)
      {
        Teuchos::RCP<Epetra_Vector> pres = pressure_->ExtractCondVector(fres_);
        Teuchos::RCP<Epetra_Vector> disp = pressure_->ExtractOtherVector(fres_);
        normpfres_ = STR::calculate_vector_norm(iternorm_, pres);
        normfres_ = STR::calculate_vector_norm(iternorm_, disp);

        pres = pressure_->ExtractCondVector(disi_);
        disp = pressure_->ExtractOtherVector(disi_);
        normpres_ = STR::calculate_vector_norm(iternorm_, pres);
        normdisi_ = STR::calculate_vector_norm(iternorm_, disp);
      }
      else
      {
        // build residual force norm
        normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
        // build residual displacement norm
        normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
        // build residual Lagrange multiplier norm
        normcon_ = conman_->GetErrorNorm();
      }

      // print stuff
      print_newton_iter();

      // increment equilibrium loop index
      iter_ += 1;
    }  // end equilibrium loop

    // correct iteration counter
    iter_ -= 1;
  }
  else if (cardvasc0dman_->have_cardiovascular0_d())
  {
    // check whether we have a sanely filled stiffness matrix
    if (not stiff_->Filled())
    {
      FOUR_C_THROW("Effective stiffness matrix must be filled here");
    }

    // initialise equilibrium loop
    iter_ = 1;
    normfres_ = CalcRefNormForce();
    // normdisi_ was already set in predictor; this is strictly >0
    normcardvasc0d_ = cardvasc0dman_->get_cardiovascular0_drhs_norm();
    normcardvasc0ddofincr_ = cardvasc0dman_->get_cardiovascular0_d_dof_incr_norm();
    timer_->reset();

    double nc;
    double ncstr;
    fres_->NormInf(&ncstr);
    double nc0d = 0.0;  // cardvasc0dman_->get_cardiovascular0_drhs_inf_norm();
    if (ncstr >= nc0d)
      nc = ncstr;
    else
      nc = nc0d;

    double dti = cardvasc0dman_->Get_k_ptc();

    const bool ptc_3D0D = Core::UTILS::IntegralValue<int>(
        Global::Problem::Instance()->cardiovascular0_d_structural_params(), "PTC_3D0D");

    // equilibrium iteration loop
    while (((not Converged() and (not linsolve_error) and (not element_error)) and
               (iter_ <= itermax_)) or
           (iter_ <= itermin_))
    {
      // make negative residual
      fres_->Scale(-1.0);

      // modify stiffness matrix with dti
      if (ptc_3D0D)
      {
        if (myrank_ == 0 and dti > 0.0) Core::IO::cout << "k_ptc = " << dti << Core::IO::endl;
      }

      // uncomplete stiffness matrix, so stuff can be inserted again
      // stiff_->UnComplete();

      // transform to local co-ordinate systems
      if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);

      // apply Dirichlet BCs to system of equations
      disi_->PutScalar(0.0);  // Useful? depends on solver and more
      if (get_loc_sys_trafo() != Teuchos::null)
      {
        Core::LinAlg::apply_dirichlet_to_system(
            *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
            *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
      }
      else
      {
        Core::LinAlg::apply_dirichlet_to_system(
            *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
      }

      // *********** time measurement ***********
      double dtcpu = timer_->wallTime();
      // *********** time measurement ***********

      // Use STC preconditioning on system matrix
      STCPreconditioning();

      // linear solver call (contact / meshtying case or default)
      if (have_contact_meshtying())
        linsolve_error = cmt_windk_constr_linear_solve(dti);
      else
      {
        // Call Cardiovascular0D solver to solve system
        linsolve_error = cardvasc0dman_->Solve(system_matrix(), disi_, fres_, dti);
      }

      // check for problems in linear solver
      // however we only care about this if we have a fancy divcont action  (meaning function will
      // return 0)
      linsolve_error = LinSolveErrorCheck(linsolve_error);

      // recover contact / meshtying Lagrange multipliers
      if (have_contact_meshtying()) cmtbridge_->Recover(disi_);

      // *********** time measurement ***********
      dtsolve_ = timer_->wallTime() - dtcpu;
      // *********** time measurement ***********

      // transform back to global co-ordinate system
      if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(disi_);

      // update end-point displacements, velocities, accelerations
      UpdateIter(iter_);

      // create parameter list
      Teuchos::ParameterList params;

      // set flag for element error in form of a negative Jacobian determinant
      // in parameter list in case of potential continuation
      if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
          divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err)
      {
        params.set<bool>("tolerate_errors", true);
        params.set<bool>("eval_error", false);
      }

      // compute residual forces #fres_ and stiffness #stiff_
      // which contain forces and stiffness of Cardiovascular0Ds
      evaluate_force_stiff_residual(params);

      // check for element error in form of a negative Jacobian determinant
      // in case of potential continuation
      if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
          divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err)
        element_error = ElementErrorCheck(params.get<bool>("eval_error"));

      // blank residual at (locally oriented) Dirichlet DOFs
      // rotate to local co-ordinate systems
      if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

      // extract reaction forces
      // reactions are negative to balance residual on DBC
      freact_->Update(-1.0, *fres_, 0.0);
      dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
      // rotate reaction forces back to global co-ordinate system
      if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

      // blank residual at DOFs on Dirichlet BC
      dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
      // rotate back to global co-ordinate system
      if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

      if (pressure_ != Teuchos::null)
      {
        Teuchos::RCP<Epetra_Vector> pres = pressure_->ExtractCondVector(fres_);
        Teuchos::RCP<Epetra_Vector> disp = pressure_->ExtractOtherVector(fres_);
        normpfres_ = STR::calculate_vector_norm(iternorm_, pres);
        normfres_ = STR::calculate_vector_norm(iternorm_, disp);

        pres = pressure_->ExtractCondVector(disi_);
        disp = pressure_->ExtractOtherVector(disi_);
        normpres_ = STR::calculate_vector_norm(iternorm_, pres);
        normdisi_ = STR::calculate_vector_norm(iternorm_, disp);
      }
      else
      {
        if (mor_->HaveMOR())
        {
          // build residual force norm with reduced force residual
          Teuchos::RCP<Epetra_Vector> fres_r = mor_->ReduceResidual(fres_);
          normfresr_ = STR::calculate_vector_norm(iternorm_, fres_r);

          // build residual displacement norm with reduced residual displacements
          Teuchos::RCP<Epetra_Vector> disi_r = mor_->ReduceResidual(disi_);
          normdisir_ = STR::calculate_vector_norm(iternorm_, disi_r);
        }

        // build residual force norm
        normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
        // build residual displacement norm
        normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
        // build residual 0D cardiovascular residual norm
        normcardvasc0d_ = cardvasc0dman_->get_cardiovascular0_drhs_norm();
        // build residual 0D cardiovascular residual dof increment norm
        normcardvasc0ddofincr_ = cardvasc0dman_->get_cardiovascular0_d_dof_incr_norm();
      }

      // print stuff
      print_newton_iter();

      // update ptc
      if (ptc_3D0D)
      {
        double np;
        double npstr;
        fres_->NormInf(&npstr);
        double np0d = 0.0;  // cardvasc0dman_->get_cardiovascular0_drhs_inf_norm();
        if (npstr >= np0d)
          np = npstr;
        else
          np = np0d;

        dti *= (np / nc);
        dti = std::max(dti, 0.0);

        nc = np;
      }

      // increment equilibrium loop index
      iter_ += 1;
    }  // end equilibrium loop

    // correct iteration counter
    iter_ -= 1;
  }

  // do nonlinear solver error check
  return uzawa_linear_newton_full_error_check(linsolve_error, element_error);
}

/*----------------------------------------------------------------------------*/
int STR::TimIntImpl::uzawa_linear_newton_full_error_check(int linerror, int eleerror)
{
  // if everything is fine print to screen and return
  if (Converged())
  {
    // compute and print monitor values
    if (conman_->HaveMonitor())
    {
      conman_->compute_monitor_values(disn_);
    }

    // print newton message on proc 0
    if (myrank_ == 0) conman_->PrintMonitorValues();

    // print Cardiovascular0D output
    if (cardvasc0dman_->have_cardiovascular0_d()) cardvasc0dman_->PrintPresFlux(false);

    return 0;
  }

  // now some error checks: do we have an element problem
  // only check if we continue in this case; other wise, we ignore the error
  if (eleerror and (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                       divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err))
  {
    return eleerror;
  }

  // now some error checks
  // do we have a problem in the linear solver
  // only check if we want to do something fancy other wise we ignore the error in the linear solver
  if (linerror and (divcontype_ == Inpar::STR::divcont_halve_step or
                       divcontype_ == Inpar::STR::divcont_adapt_step or
                       divcontype_ == Inpar::STR::divcont_rand_adapt_step or
                       divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                       divcontype_ == Inpar::STR::divcont_repeat_step or
                       divcontype_ == Inpar::STR::divcont_repeat_simulation or
                       divcontype_ == Inpar::STR::divcont_adapt_penaltycontact or
                       divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err))
  {
    return linerror;
  }
  else
  {
    if ((iter_ >= itermax_) and (divcontype_ == Inpar::STR::divcont_stop))
    {
      // write restart output of last converged step before stopping
      output(true);

      FOUR_C_THROW("Newton unconverged in %d iterations", iter_);
      return 1;
    }
    else if ((iter_ >= itermax_) and (divcontype_ == Inpar::STR::divcont_continue))
    {
      if (myrank_ == 0)
        Core::IO::cout << "Newton unconverged in " << iter_ << " iterations, continuing"
                       << Core::IO::endl;
      if (conman_->HaveMonitor()) conman_->compute_monitor_values(disn_);
      return 0;
    }
    else if ((iter_ >= itermax_) and
             (divcontype_ == Inpar::STR::divcont_halve_step or
                 divcontype_ == Inpar::STR::divcont_adapt_step or
                 divcontype_ == Inpar::STR::divcont_rand_adapt_step or
                 divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err or
                 divcontype_ == Inpar::STR::divcont_repeat_step or
                 divcontype_ == Inpar::STR::divcont_repeat_simulation or
                 divcontype_ == Inpar::STR::divcont_adapt_penaltycontact or
                 divcontype_ == Inpar::STR::divcont_adapt_3D0Dptc_ele_err))
    {
      if (myrank_ == 0)
        Core::IO::cout << "Newton unconverged in " << iter_ << " iterations " << Core::IO::endl;
      return 1;
    }
  }
  FOUR_C_THROW("Fatal error in uzawa_linear_newton_full_error_check, case not implemented ");
  return 0;
}

/*----------------------------------------------------------------------*/
/* solution with nonlinear iteration for contact / meshtying */
int STR::TimIntImpl::CmtNonlinearSolve()
{
  //********************************************************************
  // get some parameters
  //********************************************************************
  // strategy type
  Inpar::CONTACT::SolvingStrategy soltype =
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
          cmtbridge_->GetStrategy().Params(), "STRATEGY");

  // semi-smooth Newton type
  bool semismooth =
      Core::UTILS::IntegralValue<int>(cmtbridge_->GetStrategy().Params(), "SEMI_SMOOTH_NEWTON");

  // iteration type
  if (itertype_ != Inpar::STR::soltech_newtonfull)
    FOUR_C_THROW("Unknown type of equilibrium iteration");

  //********************************************************************
  // Solving Strategy using Lagrangian Multipliers
  //********************************************************************
  if (soltype == Inpar::CONTACT::solution_lagmult)
  {
    //********************************************************************
    // 1) SEMI-SMOOTH NEWTON FOR CONTACT
    // The search for the correct active set (=contact nonlinearity) and
    // the large deformation linearization (=geometrical nonlinearity) are
    // merged into one semi-smooth Newton method and solved within ONE
    // iteration loop (which is then basically a standard Newton).
    //********************************************************************
    if (cmtbridge_->HaveContact() && semismooth)
    {
      // nonlinear iteration
      int error = NewtonFull();
      if (error) return error;
    }

    //********************************************************************
    // 2) FIXED-POINT APPROACH FOR CONTACT
    // The search for the correct active set (=contact nonlinearity) is
    // represented by a fixed-point approach, whereas the large deformation
    // linearization (=geometrical nonlinearity) is treated by a standard
    // Newton scheme. This yields TWO nested iteration loops
    //********************************************************************
    else if (cmtbridge_->HaveContact() && !semismooth)
    {
      // active set strategy
      int activeiter = 0;
      while (cmtbridge_->GetStrategy().ActiveSetConverged() == false)
      {
        // increase active set iteration index
        ++activeiter;

        // predictor step (except for first active set step)
        if (activeiter > 1) Predict();

        // nonlinear iteration
        int error = NewtonFull();
        if (error) return error;

        // update of active set (fixed-point)
        cmtbridge_->GetStrategy().UpdateActiveSet();
      }
    }

    //********************************************************************
    // 3) STANDARD NEWTON APPROACH FOR MESHTYING
    // No search for the correct active set has to be resolved for mortar
    // meshtying and mortar coupling is linear in this case. Thus, only
    // the large deformation FE problem remains to be solved as nonlinearity
    // Here, a standard Newton scheme is applied and we have ONLY ONE loop.
    //********************************************************************
    else
    {
      // nonlinear iteration
      int error = NewtonFull();
      if (error) return error;
    }
  }
  //********************************************************************
  // 4) AUGMENTED SEMI-SMOOTH NEWTON FOR CONTACT
  // The search for the correct active set (=contact nonlinearity) and
  // the large deformation linearization (=geometrical nonlinearity) are
  // merged into one semi-smooth Newton method and solved within ONE
  // iteration loop (which is then basically a standard Newton).
  // TODO EXTENSION TO A SMOOTH NEWTON APPROACH ARE UNDER CONSIDERATION
  //********************************************************************
  else if (soltype == Inpar::CONTACT::solution_augmented)
  {
    if (cmtbridge_->HaveContact() && semismooth)
    {
      int error = 0;
      // nonlinear iteration
      if (itertype_ == Inpar::STR::soltech_newtonfull)
        error = NewtonFull();
      else if (itertype_ == Inpar::STR::soltech_newtonls)
        error = NewtonLS();

      if (error) return error;
    }
  }

  //********************************************************************
  // Solving Strategy using Regularization Techniques (Penalty Method)
  //********************************************************************
  else if (soltype == Inpar::CONTACT::solution_penalty ||
           soltype == Inpar::CONTACT::solution_multiscale)
  {
    // nonlinear iteration
    int error = NewtonFull();
    if (error) return error;

    // update constraint norm
    cmtbridge_->GetStrategy().update_constraint_norm();
  }

  //********************************************************************
  // Solving Strategy using Nitsche's method
  //********************************************************************
  else if (soltype == Inpar::CONTACT::solution_nitsche)
  {
    // nonlinear iteration
    return NewtonFull();
  }

  //********************************************************************
  // Solving Strategy using Augmented Lagrange Techniques with Uzawa
  //********************************************************************
  else if (soltype == Inpar::CONTACT::solution_uzawa)
  {
    // get tolerance and maximum Uzawa steps
    double eps = cmtbridge_->GetStrategy().Params().get<double>("UZAWACONSTRTOL");
    int maxuzawaiter = cmtbridge_->GetStrategy().Params().get<int>("UZAWAMAXSTEPS");

    // Augmented Lagrangian loop (Uzawa)
    int uzawaiter = 0;
    do
    {
      // increase iteration index
      ++uzawaiter;
      if (uzawaiter > maxuzawaiter)
        FOUR_C_THROW("Uzawa unconverged in %d iterations", maxuzawaiter);
      if (!myrank_) std::cout << "Starting Uzawa step No. " << uzawaiter << std::endl;

      // for second, third,... Uzawa step: out-of-balance force
      if (uzawaiter > 1)
      {
        fres_->Scale(-1.0);
        cmtbridge_->GetStrategy().InitializeUzawa(stiff_, fres_);
        fres_->Scale(-1.0);
      }

      // nonlinear iteration
      int error = NewtonFull();
      if (error) return error;

      // update constraint norm and penalty parameter
      cmtbridge_->GetStrategy().update_constraint_norm(uzawaiter);

      // store Lagrange multipliers for next Uzawa step
      cmtbridge_->GetStrategy().update_uzawa_augmented_lagrange();
      cmtbridge_->GetStrategy().store_nodal_quantities(Mortar::StrategyBase::lmuzawa);

    } while (cmtbridge_->GetStrategy().ConstraintNorm() >= eps);

    // reset penalty parameter
    cmtbridge_->GetStrategy().ResetPenalty();
  }

  return 0;
}

/*----------------------------------------------------------------------*/
/* linear solver call for contact / meshtying */
void STR::TimIntImpl::CmtLinearSolve()
{
  // adapt tolerance for contact solver
  // note: tolerance for fallback solver already adapted in NewtonFull
  Core::LinAlg::SolverParams solver_params;
  if (solveradapttol_ and (iter_ > 1))
  {
    solver_params.nonlin_tolerance = tolfres_;
    solver_params.nonlin_residual = normfres_;
    solver_params.lin_tol_better = solveradaptolbetter_;
  }

  Inpar::CONTACT::SolvingStrategy soltype =
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
          cmtbridge_->GetStrategy().Params(), "STRATEGY");
  Inpar::CONTACT::SystemType systype = Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
      cmtbridge_->GetStrategy().Params(), "SYSTEM");

  // update information about active slave dofs
  //**********************************************************************
  // feed solver/preconditioner with additional information about the contact/meshtying problem
  //**********************************************************************
  {
    // TODO: maps for merged meshtying and contact problem !!!

    Teuchos::RCP<Epetra_Map> masterDofMap;
    Teuchos::RCP<Epetra_Map> slaveDofMap;
    Teuchos::RCP<Epetra_Map> innerDofMap;
    Teuchos::RCP<Epetra_Map> activeDofMap;
    Teuchos::RCP<Mortar::StrategyBase> strat = Teuchos::rcpFromRef(cmtbridge_->GetStrategy());
    strat->collect_maps_for_preconditioner(masterDofMap, slaveDofMap, innerDofMap, activeDofMap);

    // feed Belos based solvers with contact information
    if (contactsolver_->Params().isSublist("Belos Parameters"))
    {
      Teuchos::ParameterList& mueluParams = contactsolver_->Params().sublist("Belos Parameters");
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact masterDofMap", masterDofMap);
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact slaveDofMap", slaveDofMap);
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact innerDofMap", innerDofMap);
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact activeDofMap", activeDofMap);
      Teuchos::RCP<CONTACT::AbstractStrategy> costrat =
          Teuchos::rcp_dynamic_cast<CONTACT::AbstractStrategy>(strat);
      if (costrat != Teuchos::null)
        mueluParams.set<std::string>("Core::ProblemType", "contact");
      else
        mueluParams.set<std::string>("Core::ProblemType", "meshtying");
      mueluParams.set<int>("time step", step_);
      mueluParams.set<int>("iter", iter_);
    }
  }  // end: feed solver with contact/meshtying information

  // analysis of eigenvalues and condition number
#ifdef CONTACTEIG
     // global counter
  static int globindex = 0;
  ++globindex;

  // print to file in matlab format
  std::ostringstream filename;
  const std::string filebase = "sparsematrix";
  filename << "o/matlab_output/" << filebase << "_" << globindex << ".mtl";
  Core::LinAlg::PrintMatrixInMatlabFormat(
      filename.str().c_str(), *(SystemMatrix()->EpetraMatrix()));

  // print sparsity pattern to file
  Core::LinAlg::PrintSparsityToPostscript(*(SystemMatrix()->EpetraMatrix()));
#endif  // #ifdef CONTACTEIG

  //**********************************************************************
  // Solving a saddle point system
  // (1) Standard / Dual Lagrange multipliers -> SaddlePoint
  // (2) Direct Augmented Lagrange strategy
  //**********************************************************************
  solver_params.refactor = true;
  solver_params.reset = iter_ == 1;
  if ((soltype == Inpar::CONTACT::solution_lagmult ||
          soltype == Inpar::CONTACT::solution_augmented) &&
      (systype != Inpar::CONTACT::system_condensed &&
          systype != Inpar::CONTACT::system_condensed_lagmult))
  {
    // check if contact contributions are present,
    // if not we make a standard solver call to speed things up
    if (!cmtbridge_->GetStrategy().IsInContact() && !cmtbridge_->GetStrategy().WasInContact() &&
        !cmtbridge_->GetStrategy().was_in_contact_last_time_step())
    {
      solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
    }
    else
    {
      // otherwise, solve the saddle point linear system

      Teuchos::RCP<Epetra_Operator> blockMat = Teuchos::null;
      Teuchos::RCP<Epetra_Vector> blocksol = Teuchos::null;
      Teuchos::RCP<Epetra_Vector> blockrhs = Teuchos::null;

      // build the saddle point system
      cmtbridge_->GetStrategy().build_saddle_point_system(
          stiff_, fres_, disi_, dbcmaps_, blockMat, blocksol, blockrhs);

      // solve the linear system
      contactsolver_->Solve(blockMat, blocksol, blockrhs, solver_params);

      // split vector and update internal displacement and Lagrange multipliers
      cmtbridge_->GetStrategy().update_displacements_and_l_mincrements(disi_, blocksol);
    }
  }

  //**********************************************************************
  // Solving a purely displacement based system
  // (1) Dual (not Standard) Lagrange multipliers -> Condensed
  // (2) Penalty and Uzawa Augmented Lagrange strategies
  //**********************************************************************
  else
  {
    if (cmtbridge_->HaveMeshtying())
    {
      // solve with contact solver
      contactsolver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
    }
    else if (cmtbridge_->HaveContact())
    {
      // check if contact contributions are present,
      // if not we make a standard solver call to speed things up
      if (!cmtbridge_->GetStrategy().IsInContact() && !cmtbridge_->GetStrategy().WasInContact() &&
          !cmtbridge_->GetStrategy().was_in_contact_last_time_step())
      {
        // standard solver call (fallback solver for pure structure problem)
        solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
        return;
      }

      // solve with contact solver
      contactsolver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
    }
  }

  // reset tolerance for contact solver
  contactsolver_->ResetTolerance();

  return;
}

/*----------------------------------------------------------------------*/
/* solution with nonlinear iteration for beam contact */
int STR::TimIntImpl::beam_contact_nonlinear_solve()
{
  //********************************************************************
  // get some parameters
  //********************************************************************
  // strategy type
  Inpar::BEAMCONTACT::Strategy strategy = Core::UTILS::IntegralValue<Inpar::BEAMCONTACT::Strategy>(
      beamcman_->beam_contact_parameters(), "BEAMS_STRATEGY");

  // unknown types of nonlinear iteration schemes
  if (itertype_ != Inpar::STR::soltech_newtonfull)
    FOUR_C_THROW("Unknown type of equilibrium iteration");

  //**********************************************************************
  // solving strategy using regularization with penalty method
  // (nonlinear solution approach: ordinary NEWTON)
  //**********************************************************************
  if (strategy == Inpar::BEAMCONTACT::bstr_penalty)
  {
    // nonlinear iteration (Newton)
    int error = NewtonFull();
    if (error) return error;

    // update constraint norm
    beamcman_->UpdateConstrNorm();
  }
  //**********************************************************************

  //**********************************************************************
  // misuse of beam contact module for GMSH output
  // (nonlinear solution approach: ordinary NEWTON)
  //**********************************************************************
  else if (strategy == Inpar::BEAMCONTACT::bstr_gmshonly)
  {
    // nonlinear iteration (Newton)
    int error = NewtonFull();
    if (error) return error;
  }
  //**********************************************************************

  //**********************************************************************
  // unknown solving strategy
  //**********************************************************************
  else
  {
    FOUR_C_THROW("ERROR: Chosen strategy not yet available for beam contact");
  }

  return 0;
}

/*----------------------------------------------------------------------*/
/* solution with pseudo transient continuation */
int STR::TimIntImpl::PTC()
{
  // we do a PTC iteration here.
  // the specific time integration has set the following
  // --> On #fres_ is the positive force residuum
  // --> On #stiff_ is the effective dynamic stiffness matrix

  // check whether we have a sanely filled stiffness matrix
  if (not stiff_->Filled())
  {
    FOUR_C_THROW("Effective stiffness matrix must be filled here");
  }

  if (outputeveryiter_)
  {
    int restart = Global::Problem::Instance()->Restart();
    if (stepn_ == (restart + 1)) outputcounter_ = 0;
    OutputEveryIter(true);
  }

  // initialise equilibrium loop
  iter_ = 1;
  normfres_ = CalcRefNormForce();
  // normdisi_ was already set in predictor; this is strictly >0
  timer_->reset();

  double ptcdt = ptcdt_;
  double nc;
  fres_->NormInf(&nc);
  double dti = 1 / ptcdt;

  int element_error = 0;
  int linsolve_error = 0;
  // equilibrium iteration loop
  while (((not Converged() and (not linsolve_error) and (not element_error)) and
             (iter_ <= itermax_)) or
         (iter_ <= itermin_))
  {
    // make negative residual
    fres_->Scale(-1.0);

    // transform to local co-ordinate systems
    if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);

    // modify stiffness matrix with dti
    {
      Teuchos::RCP<Epetra_Vector> tmp =
          Core::LinAlg::CreateVector(system_matrix()->RowMap(), false);
      tmp->PutScalar(dti);
      Teuchos::RCP<Epetra_Vector> diag =
          Core::LinAlg::CreateVector(system_matrix()->RowMap(), false);
      system_matrix()->ExtractDiagonalCopy(*diag);
      diag->Update(1.0, *tmp, 1.0);
      system_matrix()->replace_diagonal_values(*diag);
    }

    // apply Dirichlet BCs to system of equations
    disi_->PutScalar(0.0);  // Useful? depends on solver and more
    if (get_loc_sys_trafo() != Teuchos::null)
    {
      Core::LinAlg::apply_dirichlet_to_system(
          *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
          *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
    }
    else
    {
      Core::LinAlg::apply_dirichlet_to_system(
          *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
    }

    // *********** time measurement ***********
    double dtcpu = timer_->wallTime();
    // *********** time measurement ***********

    // STC preconditioning
    STCPreconditioning();


    // solve for disi_
    // Solve K_Teffdyn . IncD = -R  ===>  IncD_{n+1}
    Core::LinAlg::SolverParams solver_params;
    if (solveradapttol_ and (iter_ > 1))
    {
      solver_params.nonlin_tolerance = tolfres_;
      solver_params.nonlin_residual = normfres_;
      solver_params.lin_tol_better = solveradaptolbetter_;
    }
    // linear solver call (contact / meshtying case or default)
    if (have_contact_meshtying())
      CmtLinearSolve();
    else
    {
      solver_params.refactor = true;
      solver_params.reset = iter_ == 1;
      linsolve_error = solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);
      // check for problems in linear solver
      // however we only care about this if we have a fancy divcont action  (meaning function will
      // return 0 )
      linsolve_error = LinSolveErrorCheck(linsolve_error);
    }
    solver_->ResetTolerance();

    // recover standard displacements
    RecoverSTCSolution();

    // recover contact / meshtying Lagrange multipliers
    if (have_contact_meshtying()) cmtbridge_->Recover(disi_);

    // *********** time measurement ***********
    dtsolve_ = timer_->wallTime() - dtcpu;
    // *********** time measurement ***********

    // update end-point displacements etc
    UpdateIter(iter_);

    if (outputeveryiter_) OutputEveryIter(true);

    // create parameter list
    Teuchos::ParameterList params;

    // set flag for element error in form of a negative Jacobian determinant
    // in parameter list in case of potential continuation
    if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
    {
      params.set<bool>("tolerate_errors", true);
      params.set<bool>("eval_error", false);
    }

    // compute residual forces #fres_ and stiffness #stiff_
    // whose components are globally oriented
    evaluate_force_stiff_residual(params);

    // check for element error in form of a negative Jacobian determinant
    // in case of potential continuation
    if (divcontype_ == Inpar::STR::divcont_rand_adapt_step_ele_err)
      element_error = ElementErrorCheck(params.get<bool>("eval_error"));

    // blank residual at (locally oriented) Dirichlet DOFs
    // rotate to local co-ordinate systems
    if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

    // extract reaction forces
    // reactions are negative to balance residual on DBC
    freact_->Update(-1.0, *fres_, 0.0);
    dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
    // rotate reaction forces back to global co-ordinate system
    if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);

    // blank residual at DOFs on Dirichlet BC
    dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
    // rotate back to global co-ordinate system
    if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

    // decide which norms have to be evaluated
    bool bPressure = pressure_ != Teuchos::null;
    bool bContactSP = (have_contact_meshtying() &&
                       Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
                           cmtbridge_->GetStrategy().Params(), "STRATEGY") ==
                           Inpar::CONTACT::solution_lagmult &&
                       (Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
                            cmtbridge_->GetStrategy().Params(), "SYSTEM") !=
                               Inpar::CONTACT::system_condensed ||
                           Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
                               cmtbridge_->GetStrategy().Params(), "SYSTEM") !=
                               Inpar::CONTACT::system_condensed));

    if (bPressure && bContactSP)
      FOUR_C_THROW(
          "We only support either contact/meshtying in saddlepoint formulation or structure with "
          "pressure DOFs");
    if (bPressure == false && bContactSP == false)
    {
      // build residual force norm
      normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
      // build residual displacement norm
      normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
    }
    if (bPressure)
    {
      Teuchos::RCP<Epetra_Vector> pres = pressure_->ExtractCondVector(fres_);
      Teuchos::RCP<Epetra_Vector> disp = pressure_->ExtractOtherVector(fres_);
      normpfres_ = STR::calculate_vector_norm(iternorm_, pres);
      normfres_ = STR::calculate_vector_norm(iternorm_, disp);

      pres = pressure_->ExtractCondVector(disi_);
      disp = pressure_->ExtractOtherVector(disi_);
      normpres_ = STR::calculate_vector_norm(iternorm_, pres);
      normdisi_ = STR::calculate_vector_norm(iternorm_, disp);
    }
    if (bContactSP)
    {
      // extract subvectors
      Teuchos::RCP<Epetra_Vector> lagrincr = cmtbridge_->GetStrategy().LagrMultSolveIncr();
      Teuchos::RCP<Epetra_Vector> constrrhs = cmtbridge_->GetStrategy().ConstrRhs();

      // build residual force norm
      normfres_ = STR::calculate_vector_norm(iternorm_, fres_);
      // build residual displacement norm
      normdisi_ = STR::calculate_vector_norm(iternorm_, disi_);
      // build residual constraint norm
      if (constrrhs != Teuchos::null)
        normcontconstr_ = STR::calculate_vector_norm(iternorm_, constrrhs);
      else
        normcontconstr_ = -1.0;
      // build lagrange multiplier increment norm
      if (lagrincr != Teuchos::null)
        normlagr_ = STR::calculate_vector_norm(iternorm_, lagrincr);
      else
        normlagr_ = -1.0;
    }

    // print stuff
    dti_ = dti;
    print_newton_iter();

    // update ptc
    {
      double np;
      fres_->NormInf(&np);
      dti *= (np / nc);
      dti = std::max(dti, 0.0);
      nc = np;
    }
    // increment equilibrium loop index
    iter_ += 1;
  }  // end equilibrium loop

  // correct iteration counter
  iter_ -= 1;

  // call monitor
  if (conman_->HaveMonitor())
  {
    conman_->compute_monitor_values(disn_);
  }

  // do nonlinear solver error check
  return newton_full_error_check(linsolve_error, element_error);
}


/*----------------------------------------------------------------------*/
/* Update iteration */
void STR::TimIntImpl::UpdateIter(const int iter  //!< iteration counter
)
{
  // Doing update_iter_iteratively() is not sufficient in the first Newton step
  // since the predictor might lead to velocities and accelerations that are
  // not consistently computed from the displacements based on the time
  // integration scheme.
  // Hence, in the first nonlinear iteration, we do update_iter_incrementally()
  // to ensure consistent velocities and accelerations across all predictors.
  //
  // From the second nonlinear iteration on, both update routines lead to
  // exactly the same results.
  if (iter <= 1)
  {
    update_iter_incrementally();
  }
  else
  {
    update_iter_iteratively();
  }

  // morning is broken
  return;
}

/*----------------------------------------------------------------------*/
/* Update iteration incrementally with prescribed residual displacements */
void STR::TimIntImpl::update_iter_incrementally(
    const Teuchos::RCP<const Epetra_Vector> disi  //!< input residual displacements
)
{
  // select residual displacements
  if (disi != Teuchos::null)
    disi_->Update(1.0, *disi, 0.0);  // set the new solution we just got
  else
    disi_->PutScalar(0.0);

  // recover contact / meshtying Lagrange multipliers (monolithic FSI)
  // not in the case of TSI with contact
  if (Global::Problem::Instance()->GetProblemType() != Core::ProblemType::tsi)
    if (have_contact_meshtying() && disi != Teuchos::null) cmtbridge_->Recover(disi_);

  // Update using #disi_
  update_iter_incrementally();

  // leave this place
  return;
}

/*----------------------------------------------------------------------*/
/* print to screen
 * lw 12/07 */
void STR::TimIntImpl::print_predictor()
{
  // only master processor
  if ((myrank_ == 0) and printscreen_ and (StepOld() % printscreen_ == 0))
  {
    Core::IO::cout << "Structural predictor for field '" << discret_->Name() << "' "
                   << Inpar::STR::PredEnumString(pred_) << " yields ";

    // relative check of force residual
    if (normtypefres_ == Inpar::STR::convnorm_rel)
    {
      Core::IO::cout << "scaled res-norm " << normfres_ / normcharforce_ << Core::IO::endl;
    }
    // absolute check of force residual
    else if (normtypefres_ == Inpar::STR::convnorm_abs)
    {
      Core::IO::cout << "absolute res-norm " << normfres_ << Core::IO::endl;
    }
    // mixed absolute-relative check of force residual
    else if (normtypefres_ == Inpar::STR::convnorm_mix)
    {
      Core::IO::cout << "mixed res-norm " << std::min(normfres_, normfres_ / normcharforce_)
                     << Core::IO::endl;
    }
    // default
    else
    {
      FOUR_C_THROW("You should not turn up here.");
    }
  }

  // leave your hat on
  return;
}


/*----------------------------------------------------------------------*/
/* print Newton-Raphson iteration to screen and error file
 * originally by lw 12/07, tk 01/08 */
void STR::TimIntImpl::print_newton_iter()
{
  // print to standard out
  if ((myrank_ == 0) and printscreen_ and (StepOld() % printscreen_ == 0) and printiter_)
  {
    if (iter_ == 1) print_newton_iter_header(stdout);
    print_newton_iter_text(stdout);
  }
}

/*----------------------------------------------------------------------*/
void STR::TimIntImpl::print_newton_iter_header(FILE* ofile)
{
  // open outstd::stringstream
  std::ostringstream oss;

  // enter converged state etc
  oss << std::setw(6) << "numiter";

  // different style due relative or absolute error checking
  // displacement
  switch (normtypefres_)
  {
    case Inpar::STR::convnorm_rel:
      oss << std::setw(16) << "rel-res-norm";
      break;
    case Inpar::STR::convnorm_abs:
      oss << std::setw(16) << "abs-res-norm";
      if (mor_->HaveMOR()) oss << std::setw(16) << "abs-res-norm-r";
      break;
    case Inpar::STR::convnorm_mix:
      oss << std::setw(16) << "mix-res-norm";
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  if (pressure_ != Teuchos::null)
  {
    switch (normtypepfres_)
    {
      case Inpar::STR::convnorm_abs:
        oss << std::setw(16) << "abs-inco-norm";
        break;
      default:
        FOUR_C_THROW("You should not turn up here.");
        break;
    }
  }

  switch (normtypedisi_)
  {
    case Inpar::STR::convnorm_rel:
      oss << std::setw(16) << "rel-dis-norm";
      break;
    case Inpar::STR::convnorm_abs:
      oss << std::setw(16) << "abs-dis-norm";
      if (mor_->HaveMOR()) oss << std::setw(16) << "abs-dis-norm-r";
      break;
    case Inpar::STR::convnorm_mix:
      oss << std::setw(16) << "mix-dis-norm";
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  if (pressure_ != Teuchos::null)
  {
    switch (normtypepfres_)
    {
      case Inpar::STR::convnorm_abs:
        oss << std::setw(16) << "abs-pre-norm";
        break;
      default:
        FOUR_C_THROW("You should not turn up here.");
        break;
    }
  }

  // add norms of Lagrange multiplier parts (contact/meshtying in saddlepoint formulation only)
  if (have_contact_meshtying())
  {
    // strategy and system setup types
    Inpar::CONTACT::SolvingStrategy soltype =
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
            cmtbridge_->GetStrategy().Params(), "STRATEGY");
    Inpar::CONTACT::SystemType systype = Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
        cmtbridge_->GetStrategy().Params(), "SYSTEM");
    Inpar::Wear::WearType wtype = Core::UTILS::IntegralValue<Inpar::Wear::WearType>(
        cmtbridge_->GetStrategy().Params(), "WEARTYPE");
    Inpar::Wear::WearSide wside = Core::UTILS::IntegralValue<Inpar::Wear::WearSide>(
        cmtbridge_->GetStrategy().Params(), "WEAR_SIDE");

    if ((soltype == Inpar::CONTACT::solution_lagmult ||
            soltype == Inpar::CONTACT::solution_augmented) &&
        (systype != Inpar::CONTACT::system_condensed &&
            systype != Inpar::CONTACT::system_condensed_lagmult))
    {
      switch (normtypecontconstr_)
      {
        case Inpar::STR::convnorm_rel:
          oss << std::setw(20) << "rel-contconstr-norm";
          break;
        case Inpar::STR::convnorm_abs:
          oss << std::setw(20) << "abs-contconstr-norm";
          break;
        default:
          FOUR_C_THROW("You should not turn up here.");
          break;
      }

      switch (normtypeplagrincr_)
      {
        case Inpar::STR::convnorm_rel:
          oss << std::setw(20) << "rel-lagrincr-norm";
          break;
        case Inpar::STR::convnorm_abs:
        {
          oss << std::setw(20) << "abs-lagrincr-norm";
          if (wtype == Inpar::Wear::wear_primvar)
          {
            oss << std::setw(20) << "abs-wearincr-S-norm";
            oss << std::setw(20) << "abs-wearcon-S-norm";
            if (wside == Inpar::Wear::wear_both)
            {
              oss << std::setw(20) << "abs-wearincr-M-norm";
              oss << std::setw(20) << "abs-wearcon-M-norm";
            }
          }
          break;
        }
        default:
          FOUR_C_THROW("You should not turn up here.");
          break;
      }
    }
  }

  // add constraint norm
  if (conman_->HaveConstraintLagr())
  {
    oss << std::setw(16) << "abs-constr-norm";
  }

  // add Cardiovascular0D norm
  if (cardvasc0dman_->have_cardiovascular0_d())
  {
    oss << std::setw(16) << "abs-0Dres-norm";
    oss << std::setw(16) << "abs-0Dinc-norm";
  }

  if (itertype_ == Inpar::STR::soltech_ptc)
  {
    oss << std::setw(16) << "        PTC-dti";
  }

  // add solution time
  oss << std::setw(13) << "ts";
  oss << std::setw(10) << "te";
  if (have_contact_meshtying()) oss << std::setw(10) << "tc";

  // add contact set information
  if (have_contact_meshtying())
  {
    // only print something for contact, not for meshtying
    if (cmtbridge_->HaveContact())
    {
      oss << std::setw(11) << "#active";
      if (cmtbridge_->GetStrategy().Friction()) oss << std::setw(10) << "#slip";
    }
  }

  // finish oss
  oss << std::ends;

  // print to screen (could be done differently...)
  fprintf(ofile, "%s\n", oss.str().c_str());

  // print it, now
  fflush(ofile);

  // nice to have met you
  return;
}

/*----------------------------------------------------------------------*/
/* print Newton-Raphson iteration to screen
 * originally by lw 12/07, tk 01/08 */
void STR::TimIntImpl::print_newton_iter_text(FILE* ofile)
{
  // open outstd::stringstream
  std::ostringstream oss;

  // enter converged state etc
  oss << std::setw(7) << iter_;

  // different style due relative or absolute error checking
  // displacement
  switch (normtypefres_)
  {
    case Inpar::STR::convnorm_rel:
      oss << std::setw(16) << std::setprecision(5) << std::scientific << normfres_ / normcharforce_;
      break;
    case Inpar::STR::convnorm_abs:
      oss << std::setw(16) << std::setprecision(5) << std::scientific << normfres_;
      if (mor_->HaveMOR())
        oss << std::setw(16) << std::setprecision(5) << std::scientific << normfresr_;
      break;
    case Inpar::STR::convnorm_mix:
      oss << std::setw(16) << std::setprecision(5) << std::scientific
          << std::min(normfres_, normfres_ / normcharforce_);
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  if (pressure_ != Teuchos::null)
  {
    switch (normtypepfres_)
    {
      case Inpar::STR::convnorm_abs:
        oss << std::setw(16) << std::setprecision(5) << std::scientific << normpfres_;
        break;
      default:
        FOUR_C_THROW("You should not turn up here.");
        break;
    }
  }

  switch (normtypedisi_)
  {
    case Inpar::STR::convnorm_rel:
      oss << std::setw(16) << std::setprecision(5) << std::scientific << normdisi_ / normchardis_;
      break;
    case Inpar::STR::convnorm_abs:
      oss << std::setw(16) << std::setprecision(5) << std::scientific << normdisi_;
      if (mor_->HaveMOR())
        oss << std::setw(16) << std::setprecision(5) << std::scientific << normdisir_;
      break;
    case Inpar::STR::convnorm_mix:
      oss << std::setw(16) << std::setprecision(5) << std::scientific
          << std::min(normdisi_, normdisi_ / normchardis_);
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  if (pressure_ != Teuchos::null)
  {
    switch (normtypepfres_)
    {
      case Inpar::STR::convnorm_abs:
        oss << std::setw(16) << std::scientific << normpres_;
        break;
      default:
        FOUR_C_THROW("You should not turn up here.");
        break;
    }
  }

  // add norms of Lagrange multiplier parts (contact/meshtying in saddlepoint formulation only)
  if (have_contact_meshtying())
  {
    // strategy and system setup types
    Inpar::CONTACT::SolvingStrategy soltype =
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
            cmtbridge_->GetStrategy().Params(), "STRATEGY");
    Inpar::CONTACT::SystemType systype = Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
        cmtbridge_->GetStrategy().Params(), "SYSTEM");
    Inpar::Wear::WearType wtype = Core::UTILS::IntegralValue<Inpar::Wear::WearType>(
        cmtbridge_->GetStrategy().Params(), "WEARTYPE");
    Inpar::Wear::WearSide wside = Core::UTILS::IntegralValue<Inpar::Wear::WearSide>(
        cmtbridge_->GetStrategy().Params(), "WEAR_SIDE");

    if ((soltype == Inpar::CONTACT::solution_lagmult ||
            soltype == Inpar::CONTACT::solution_augmented) &&
        (systype != Inpar::CONTACT::system_condensed &&
            systype != Inpar::CONTACT::system_condensed_lagmult))
    {
      // we only support abs norms
      oss << std::setw(20) << std::setprecision(5) << std::scientific
          << normcontconstr_;  // RHS for contact constraints
      oss << std::setw(20) << std::setprecision(5) << std::scientific
          << normlagr_;  // norm Lagrange multipliers

      if (wtype == Inpar::Wear::wear_primvar)
      {
        oss << std::setw(20) << std::setprecision(5) << std::scientific << normw_;  // norm wear
        oss << std::setw(20) << std::setprecision(5) << std::scientific
            << normwrhs_;  // norm wear rhs
        if (wside == Inpar::Wear::wear_both)
        {
          oss << std::setw(20) << std::setprecision(5) << std::scientific << normwm_;  // norm wear
          oss << std::setw(20) << std::setprecision(5) << std::scientific
              << normwmrhs_;  // norm wear rhs
        }
      }
    }
  }

  // add constraint norm
  if (conman_->HaveConstraintLagr())
  {
    oss << std::setw(16) << std::setprecision(5) << std::scientific << normcon_;
  }

  // add Cardiovascular0D norm
  if (cardvasc0dman_->have_cardiovascular0_d())
  {
    oss << std::setw(16) << std::setprecision(5) << std::scientific << normcardvasc0d_;
    oss << std::setw(16) << std::setprecision(5) << std::scientific << normcardvasc0ddofincr_;
  }

  if (itertype_ == Inpar::STR::soltech_ptc)
  {
    oss << std::setw(16) << std::setprecision(5) << std::scientific << dti_;
  }

  // add solution time
  oss << std::setw(13) << std::setprecision(2) << std::scientific << dtsolve_;
  oss << std::setw(10) << std::setprecision(2) << std::scientific << dtele_;
  if (have_contact_meshtying())
    oss << std::setw(10) << std::setprecision(2) << std::scientific << dtcmt_;

  // add contact set information
  if (have_contact_meshtying())
  {
    // only print something for contact, not for meshtying
    Inpar::CONTACT::SolvingStrategy soltype =
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
            cmtbridge_->GetStrategy().Params(), "STRATEGY");
    bool semismooth =
        Core::UTILS::IntegralValue<int>(cmtbridge_->GetStrategy().Params(), "SEMI_SMOOTH_NEWTON");
    if (cmtbridge_->HaveContact())
    {
      if (soltype == Inpar::CONTACT::solution_augmented && semismooth)
      {
        bool ccontact = cmtbridge_->GetStrategy().active_set_semi_smooth_converged();
        // active set changed
        if (!ccontact)
          oss << std::setw(8) << cmtbridge_->GetStrategy().NumberOfActiveNodes() << "(c)";
        // active set didnot change
        else
          oss << std::setw(8) << cmtbridge_->GetStrategy().NumberOfActiveNodes() << "(-)";
      }
      else
        oss << std::setw(11) << cmtbridge_->GetStrategy().NumberOfActiveNodes();
      if (cmtbridge_->GetStrategy().Friction())
        oss << std::setw(10) << cmtbridge_->GetStrategy().NumberOfSlipNodes();
    }
  }

  // finish oss
  oss << std::ends;

  // print to screen (could be done differently...)
  fprintf(ofile, "%s\n", oss.str().c_str());

  // print it, now
  fflush(ofile);

  // nice to have met you
  return;
}

/*----------------------------------------------------------------------*/
/* Export active set and characteristic calculation times into text files */
void STR::TimIntImpl::export_contact_quantities()
{
  // add integration time contribution from every newton step
  inttime_global_ += cmtbridge_->GetStrategy().Inttime();

  double iteration = (double)iter_ + 1.0;
  double curinttime = (cmtbridge_->GetStrategy().Inttime()) / (iteration);

  std::cout << "*** averaged inttime per newton step =  " << curinttime << std::endl;
  std::cout << "*** total inttime per time step= " << curinttime * iteration << std::endl;

  // write number of active nodes for converged newton in textfile xx x.active
  FILE* MyFile = nullptr;
  std::ostringstream filename;
  const std::string filebase = Global::Problem::Instance()->OutputControlFile()->file_name();
  filename << filebase << ".active";
  MyFile = fopen(filename.str().c_str(), "at+");

  // store active set
  if (MyFile)
  {
    fprintf(MyFile, "%d\t", cmtbridge_->GetStrategy().NumberOfActiveNodes());
    fprintf(MyFile, "%d\n", cmtbridge_->GetStrategy().NumberOfSlipNodes());
    fclose(MyFile);
  }
  else
    FOUR_C_THROW("ERROR: File could not be opened.");


  // write required time
  FILE* MyFile2 = nullptr;
  std::ostringstream filename2;
  const std::string filebase2 = Global::Problem::Instance()->OutputControlFile()->file_name();
  filename2 << filebase2 << ".time";
  MyFile2 = fopen(filename2.str().c_str(), "at+");

  // store characteristic times
  if (MyFile2)
  {
    fprintf(MyFile2, "%g\t", dtsolve_);
    fprintf(MyFile2, "%g\t", dtele_);
    fprintf(MyFile2, "%g\t", dtcmt_);
    fprintf(MyFile2, "%g\t", curinttime);
    fprintf(MyFile2, "%g\n", curinttime * iteration);
    fclose(MyFile2);
  }
  else
    FOUR_C_THROW("ERROR: File could not be opened.");

  return;
}

/*----------------------------------------------------------------------*/
/* print statistics of converged NRI */
void STR::TimIntImpl::print_newton_conv()
{
#ifdef CONTACTEXPORT
  // output integration time for contact and more...
  if (have_contact_meshtying())
  {
    export_contact_quantities();
  }
#endif

  // print constraint manager's lore
  if (conman_->HaveMonitor())
  {
    conman_->PrintMonitorValues();
  }

  // somebody did the door
  return;
}

/*----------------------------------------------------------------------*/
/* print step summary */
void STR::TimIntImpl::print_step()
{
  // print out (only on master CPU)
  if ((myrank_ == 0) and printscreen_ and (StepOld() % printscreen_ == 0))
  {
    print_step_text(stdout);
  }
}

/*----------------------------------------------------------------------*/
/* print step summary */
void STR::TimIntImpl::print_step_text(FILE* ofile)
{
  // open outstd::stringstream
  std::ostringstream oss;

  // the text
  oss << "Finalised step " << std::setw(1) << step_;
  oss << " / " << std::setw(1) << stepmax_;
  oss << " | time " << std::setw(9) << std::setprecision(3) << std::scientific << (*time_)[0];
  oss << " | dt " << std::setw(9) << std::setprecision(3) << std::scientific << (*dt_)[0];
  oss << " | numiter " << std::setw(1) << iter_;
  oss << " | wct " << std::setw(8) << std::setprecision(2) << std::scientific
      << timer_->totalElapsedTime(true);
  oss << "\n--------------------------------------------------------------------------------\n";

  // print to ofile (could be done differently...)
  fprintf(ofile, "%s\n", oss.str().c_str());

  // print it, now
  fflush(ofile);

  // fall asleep
  return;
}

/*----------------------------------------------------------------------*/
/* Linear structure solve with just an interface load */
Teuchos::RCP<Epetra_Vector> STR::TimIntImpl::solve_relaxation_linear()
{
  // create parameter list
  Teuchos::ParameterList params;

  // Evaluate/define the residual force vector #fres_ for
  // relaxation solution with solve_relaxation_linear
  evaluate_force_stiff_residual_relax(params);

  // negative residual
  fres_->Scale(-1.0);

  // apply Dirichlet BCs to system of equations
  disi_->PutScalar(0.0);  // Useful? depends on solver and more
  Core::LinAlg::apply_dirichlet_to_system(*stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));

  // solve for #disi_
  Core::LinAlg::SolverParams solver_params;
  solver_params.refactor = true;
  solver_params.reset = true;
  solver_->Solve(stiff_->EpetraOperator(), disi_, fres_, solver_params);

  // go back
  return disi_;
}

/*----------------------------------------------------------------------*/
/* Prepare system for solving with Newton's method */
void STR::TimIntImpl::prepare_system_for_newton_solve(const bool preparejacobian)
{
  // rotate residual to local coordinate systems
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(fres_);

  // extract reaction forces
  // reactions are negative to balance residual on DBC
  freact_->Update(-1.0, *fres_, 0.0);
  dbcmaps_->InsertOtherVector(dbcmaps_->ExtractOtherVector(zeros_), freact_);
  // rotate reaction forces back to global coordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(freact_);
  // blank residual at DOFs on Dirichlet BCs
  dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), fres_);
  // rotate reaction forces back to global coordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateLocalToGlobal(fres_);

  // make the residual negative
  fres_->Scale(-1.0);

  // transform stiff_ and fres_ to local coordinate system
  if (locsysman_ != Teuchos::null) locsysman_->RotateGlobalToLocal(system_matrix(), fres_);
  // local matrix and rhs required for correctly applying Dirichlet boundary
  // conditions: rows with inclined Dirichlet boundary condition can be blanked
  // and a '1.0' is put at the diagonal term

  // blank iterative increment
  disi_->PutScalar(0.0);  // Useful? depends on solver and more

  // apply Dirichlet BCs to system of equations
  if (preparejacobian)
  {
    if (get_loc_sys_trafo() != Teuchos::null)
    {
      Core::LinAlg::apply_dirichlet_to_system(
          *Core::LinAlg::CastToSparseMatrixAndCheckSuccess(stiff_), *disi_, *fres_,
          *get_loc_sys_trafo(), *zeros_, *(dbcmaps_->CondMap()));
    }
    else
      Core::LinAlg::apply_dirichlet_to_system(
          *stiff_, *disi_, *fres_, *zeros_, *(dbcmaps_->CondMap()));
  }

  // final sip
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::TimIntImpl::use_block_matrix(
    Teuchos::RCP<const Core::LinAlg::MultiMapExtractor> domainmaps,
    Teuchos::RCP<const Core::LinAlg::MultiMapExtractor> rangemaps)
{
  // (re)allocate system matrix
  stiff_ =
      Teuchos::rcp(new Core::LinAlg::BlockSparseMatrix<Core::LinAlg::DefaultBlockMatrixStrategy>(
          *domainmaps, *rangemaps, 81, false, true));
  mass_ =
      Teuchos::rcp(new Core::LinAlg::BlockSparseMatrix<Core::LinAlg::DefaultBlockMatrixStrategy>(
          *domainmaps, *rangemaps, 81, false, true));
  if (damping_ != Inpar::STR::damp_none)
    damp_ =
        Teuchos::rcp(new Core::LinAlg::BlockSparseMatrix<Core::LinAlg::DefaultBlockMatrixStrategy>(
            *domainmaps, *rangemaps, 81, false, true));

  // recalculate mass and damping matrices

  Teuchos::RCP<Epetra_Vector> fint =
      Core::LinAlg::CreateVector(*dof_row_map_view(), true);  // internal force

  stiff_->Zero();
  mass_->Zero();

  {
    // create the parameters for the discretization
    Teuchos::ParameterList p;
    // action for elements
    p.set("action", "calc_struct_nlnstiffmass");
    // other parameters that might be needed by the elements
    p.set("total time", (*time_)[0]);
    p.set("delta time", (*dt_)[0]);

    Teuchos::RCP<Epetra_Vector> finert = Teuchos::null;
    if (HaveNonlinearMass())
    {
      finert = Core::LinAlg::CreateVector(*dof_row_map_view(), true);  // intertial force
      // Note: the following parameters are just dummies, since they are only needed to calculate
      // finert which we will not use anyway
      p.set("timintfac_dis", 0.0);  // dummy!
      p.set("timintfac_vel", 0.0);  // dummy!
    }

    if (pressure_ != Teuchos::null) p.set("volume", 0.0);
    // set vector values needed by elements
    discret_->ClearState();
    discret_->set_state("residual displacement", zeros_);
    discret_->set_state("displacement", (*dis_)(0));
    discret_->set_state(0, "velocity", (*vel_)(0));
    discret_->set_state(0, "acceleration", (*acc_)(0));
    if (damping_ == Inpar::STR::damp_material) discret_->set_state("velocity", (*vel_)(0));

    discret_->evaluate(p, stiff_, mass_, fint, finert, Teuchos::null);
    discret_->ClearState();
  }

  // finish mass matrix
  mass_->Complete();

  // close stiffness matrix
  stiff_->Complete();

  // build Rayleigh damping matrix if desired
  if (damping_ == Inpar::STR::damp_rayleigh)
  {
    damp_->Add(*stiff_, false, dampk_, 0.0);
    damp_->Add(*mass_, false, dampm_, 1.0);
    damp_->Complete();
  }

  // in case of C0 pressure field, we need to get rid of
  // pressure equations
  if (pressure_ != Teuchos::null)
  {
    mass_->ApplyDirichlet(*(pressure_->CondMap()));
  }

  // We need to reset the stiffness matrix because its graph (topology)
  // is not finished yet in case of constraints and posssibly other side
  // effects (basically managers).
  stiff_->Reset();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::TimIntImpl::STCPreconditioning()
{
  if (stcscale_ != Inpar::STR::stc_none)
  {
    if (!stccompl_)
    {
      ComputeSTCMatrix();
      stccompl_ = true;
    }

    stiff_ = MLMultiply(*(Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(stiff_)), *stcmat_,
        true, false, true);
    if (stcscale_ == Inpar::STR::stc_currsym)
    {
      stiff_ = MLMultiply(*stcmat_, true,
          *(Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(stiff_)), false, true, false,
          true);
      Teuchos::RCP<Epetra_Vector> fressdc = Core::LinAlg::CreateVector(*dof_row_map_view(), true);
      stcmat_->Multiply(true, *fres_, *fressdc);
      fres_->Update(1.0, *fressdc, 0.0);
    }
  }
}

/*----------------------------------------------------------------------------*/
void STR::TimIntImpl::ComputeSTCMatrix()
{
  stcmat_->Zero();
  // create the parameters for the discretization
  Teuchos::ParameterList p;
  // action for elements
  discret_->set_state("residual displacement", disi_);
  discret_->set_state("displacement", disn_);

  const std::string action = "calc_stc_matrix";
  p.set("action", action);
  p.set<int>("stc_scaling", stcscale_);
  p.set("stc_layer", 1);

  discret_->evaluate(p, stcmat_, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);

  stcmat_->Complete();

#ifdef FOUR_C_ENABLE_ASSERTIONS
  if (iter_ == 1 && step_ == 0)
  {
    std::string fname = Global::Problem::Instance()->OutputControlFile()->file_name_only_prefix();
    fname += ".stcmatrix1.mtl";
    if (myrank_ == 0) std::cout << "Printing stcmatrix1 to file" << std::endl;
    Core::LinAlg::PrintMatrixInMatlabFormat(
        fname, *((Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(stcmat_))->EpetraMatrix()));
  }
#endif

  for (int lay = 2; lay <= stclayer_; ++lay)
  {
    Teuchos::ParameterList pe;

    pe.set("action", action);
    pe.set<int>("stc_scaling", stcscale_);
    pe.set("stc_layer", lay);

    Teuchos::RCP<Core::LinAlg::SparseMatrix> tmpstcmat =
        Teuchos::rcp(new Core::LinAlg::SparseMatrix(*dof_row_map_view(), 81, true, true));
    tmpstcmat->Zero();

    discret_->evaluate(pe, tmpstcmat, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
    tmpstcmat->Complete();

#ifdef FOUR_C_ENABLE_ASSERTIONS
    if (iter_ == 1 && step_ == 0)
    {
      std::string fname = Global::Problem::Instance()->OutputControlFile()->file_name_only_prefix();
      fname += ".stcmatrix2.mtl";
      if (myrank_ == 0) std::cout << "Printing stcmatrix2 to file" << std::endl;
      Core::LinAlg::PrintMatrixInMatlabFormat(fname,
          *((Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(tmpstcmat))->EpetraMatrix()));
    }
#endif

    stcmat_ = MLMultiply(*tmpstcmat, *stcmat_, false, false, true);
  }

  discret_->ClearState();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::TimIntImpl::RecoverSTCSolution()
{
  if (stcscale_ != Inpar::STR::stc_none)
  {
    Teuchos::RCP<Epetra_Vector> disisdc = Core::LinAlg::CreateVector(*dof_row_map_view(), true);

    stcmat_->Multiply(false, *disi_, *disisdc);
    disi_->Update(1.0, *disisdc, 0.0);
  }

  return;
}


/*----------------------------------------------------------------------*/
/* solution with nonlinear iteration for contact / meshtying AND Cardiovascular0D bcs*/
int STR::TimIntImpl::cmt_windk_constr_nonlinear_solve()
{
  //********************************************************************
  // get some parameters
  //********************************************************************
  // strategy type
  Inpar::CONTACT::SolvingStrategy soltype =
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
          cmtbridge_->GetStrategy().Params(), "STRATEGY");

  // semi-smooth Newton type
  bool semismooth =
      Core::UTILS::IntegralValue<int>(cmtbridge_->GetStrategy().Params(), "SEMI_SMOOTH_NEWTON");

  // iteration type
  if (itertype_ != Inpar::STR::soltech_newtonuzawalin)
    FOUR_C_THROW(
        "Unknown type of equilibrium iteration! Choose newtonlinuzawa instead of fullnewton!");

  //********************************************************************
  // Solving Strategy using Lagrangian Multipliers
  //********************************************************************
  if (soltype == Inpar::CONTACT::solution_lagmult)
  {
    //********************************************************************
    // 1) SEMI-SMOOTH NEWTON FOR CONTACT
    // The search for the correct active set (=contact nonlinearity) and
    // the large deformation linearization (=geometrical nonlinearity) are
    // merged into one semi-smooth Newton method and solved within ONE
    // iteration loop (which is then basically a standard Newton).
    //********************************************************************
    if (cmtbridge_->HaveContact() && semismooth)
    {
      // nonlinear iteration
      int error = uzawa_linear_newton_full();
      if (error) return error;
    }

    //********************************************************************
    // 2) FIXED-POINT APPROACH FOR CONTACT
    // The search for the correct active set (=contact nonlinearity) is
    // represented by a fixed-point approach, whereas the large deformation
    // linearization (=geometrical nonlinearity) is treated by a standard
    // Newton scheme. This yields TWO nested iteration loops
    //********************************************************************
    else if (cmtbridge_->HaveContact() && !semismooth)
    {
      // active set strategy
      int activeiter = 0;
      while (cmtbridge_->GetStrategy().ActiveSetConverged() == false)
      {
        // increase active set iteration index
        ++activeiter;

        // predictor step (except for first active set step)
        if (activeiter > 1) Predict();

        // nonlinear iteration
        int error = uzawa_linear_newton_full();
        if (error) return error;

        // update of active set (fixed-point)
        cmtbridge_->GetStrategy().UpdateActiveSet();
      }
    }

    //********************************************************************
    // 3) STANDARD NEWTON APPROACH FOR MESHTYING
    // No search for the correct active set has to be resolved for mortar
    // meshtying and mortar coupling is linear in this case. Thus, only
    // the large deformation FE problem remains to be solved as nonlinearity
    // Here, a standard Newton scheme is applied and we have ONLY ONE loop.
    //********************************************************************
    else
    {
      // nonlinear iteration
      int error = uzawa_linear_newton_full();
      if (error) return error;
    }
  }

  //********************************************************************
  // Solving Strategy using Regularization Techniques (Penalty Method)
  //********************************************************************
  else if (soltype == Inpar::CONTACT::solution_penalty)
  {
    // nonlinear iteration
    int error = uzawa_linear_newton_full();
    if (error) return error;

    // update constraint norm
    cmtbridge_->GetStrategy().update_constraint_norm();
  }

  //********************************************************************
  // Solving Strategy using Augmented Lagrange Techniques with Uzawa
  //********************************************************************
  else if (soltype == Inpar::CONTACT::solution_uzawa)
  {
    // get tolerance and maximum Uzawa steps
    double eps = cmtbridge_->GetStrategy().Params().get<double>("UZAWACONSTRTOL");
    int maxuzawaiter = cmtbridge_->GetStrategy().Params().get<int>("UZAWAMAXSTEPS");

    // Augmented Lagrangian loop (Uzawa)
    int uzawaiter = 0;
    do
    {
      // increase iteration index
      ++uzawaiter;
      if (uzawaiter > maxuzawaiter)
        FOUR_C_THROW("Uzawa unconverged in %d iterations", maxuzawaiter);
      if (!myrank_) std::cout << "Starting Uzawa step No. " << uzawaiter << std::endl;

      // for second, third,... Uzawa step: out-of-balance force
      if (uzawaiter > 1)
      {
        fres_->Scale(-1.0);
        cmtbridge_->GetStrategy().InitializeUzawa(stiff_, fres_);
        fres_->Scale(-1.0);
      }

      // nonlinear iteration
      int error = uzawa_linear_newton_full();
      if (error) return error;

      // update constraint norm and penalty parameter
      cmtbridge_->GetStrategy().update_constraint_norm(uzawaiter);

      // store Lagrange multipliers for next Uzawa step
      cmtbridge_->GetStrategy().update_uzawa_augmented_lagrange();
      cmtbridge_->GetStrategy().store_nodal_quantities(Mortar::StrategyBase::lmuzawa);

    } while (cmtbridge_->GetStrategy().ConstraintNorm() >= eps);

    // reset penalty parameter
    cmtbridge_->GetStrategy().ResetPenalty();
  }

  return 0;
}



/*----------------------------------------------------------------------*/
/* linear solver call for contact / meshtying AND Cardiovascular0D bcs*/
int STR::TimIntImpl::cmt_windk_constr_linear_solve(const double k_ptc)
{
  // strategy and system setup types
  Inpar::CONTACT::SolvingStrategy soltype =
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(
          cmtbridge_->GetStrategy().Params(), "STRATEGY");
  Inpar::CONTACT::SystemType systype = Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
      cmtbridge_->GetStrategy().Params(), "SYSTEM");

  int linsolve_error = 0;

  // update information about active slave dofs
  //**********************************************************************
  // feed solver/preconditioner with additional information about the contact/meshtying problem
  //**********************************************************************
  {
    Teuchos::RCP<Epetra_Map> masterDofMap;
    Teuchos::RCP<Epetra_Map> slaveDofMap;
    Teuchos::RCP<Epetra_Map> innerDofMap;
    Teuchos::RCP<Epetra_Map> activeDofMap;
    Teuchos::RCP<Mortar::StrategyBase> strat = Teuchos::rcpFromRef(cmtbridge_->GetStrategy());
    strat->collect_maps_for_preconditioner(masterDofMap, slaveDofMap, innerDofMap, activeDofMap);

    // feed Belos based solvers with contact information
    // if (contactsolver_->Params().isSublist("Belos Parameters"))
    if (cardvasc0dman_->GetSolver()->Params().isSublist("Belos Parameters"))
    {
      // Teuchos::ParameterList& mueluParams = contactsolver_->Params().sublist("Belos Parameters");
      Teuchos::ParameterList& mueluParams =
          cardvasc0dman_->GetSolver()->Params().sublist("Belos Parameters");
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact masterDofMap", masterDofMap);
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact slaveDofMap", slaveDofMap);
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact innerDofMap", innerDofMap);
      mueluParams.set<Teuchos::RCP<Epetra_Map>>("contact activeDofMap", activeDofMap);
      Teuchos::RCP<CONTACT::AbstractStrategy> costrat =
          Teuchos::rcp_dynamic_cast<CONTACT::AbstractStrategy>(strat);
      if (costrat != Teuchos::null)
        mueluParams.set<std::string>("Core::ProblemType", "contact");
      else
        mueluParams.set<std::string>("Core::ProblemType", "meshtying");
      mueluParams.set<int>("time step", step_);
      mueluParams.set<int>("iter", iter_);
    }

  }  // end: feed solver with contact/meshtying information

  // analysis of eigenvalues and condition number
#ifdef CONTACTEIG
     // global counter
  static int globindex = 0;
  ++globindex;

  // print to file in matlab format
  std::ostringstream filename;
  const std::string filebase = "sparsematrix";
  filename << "o/matlab_output/" << filebase << "_" << globindex << ".mtl";
  Core::LinAlg::PrintMatrixInMatlabFormat(
      filename.str().c_str(), *(SystemMatrix()->EpetraMatrix()));

  // print sparsity pattern to file
  Core::LinAlg::PrintSparsityToPostscript(*(SystemMatrix()->EpetraMatrix()));
#endif  // #ifdef CONTACTEIG

  //**********************************************************************
  // Solving a saddle point system
  // -> does not work together with constraints / Cardiovascular0D bcs
  // (1) Standard / Dual Lagrange multipliers -> SaddlePointCoupled
  // (2) Standard / Dual Lagrange multipliers -> SaddlePointSimpler
  //**********************************************************************
  if (soltype == Inpar::CONTACT::solution_lagmult &&
      (systype != Inpar::CONTACT::system_condensed &&
          systype != Inpar::CONTACT::system_condensed_lagmult))
  {
    FOUR_C_THROW(
        "Constraints / Cardiovascular0D bcs together with saddle point contact system does not "
        "work (yet)!");
  }

  //**********************************************************************
  // Solving a purely displacement based system
  // (1) Dual (not Standard) Lagrange multipliers -> Condensed
  // (2) Penalty and Augmented Lagrange strategies
  //**********************************************************************
  else
  {
    // solve with Cardiovascular0D solver
    linsolve_error = cardvasc0dman_->Solve(system_matrix(), disi_, fres_, k_ptc);
  }

  return linsolve_error;
}

/*-----------------------------------------------------------------------------*
 * check, if according to divercont flag                             meier 01/15
 * time step size can be increased
 *-----------------------------------------------------------------------------*/
void STR::TimIntImpl::check_for_time_step_increase(Inpar::STR::ConvergenceStatus& status)
{
  const int maxnumfinestep = 4;

  if (divcontype_ != Inpar::STR::divcont_adapt_step)
    return;
  else if (status == Inpar::STR::conv_success and divconrefinementlevel_ != 0)
  {
    divconnumfinestep_++;

    if (divconnumfinestep_ == maxnumfinestep)
    {
      // increase the step size if the remaining number of steps is a even number
      if (((stepmax_ - stepn_) % 2) == 0 and stepmax_ != stepn_)
      {
        Core::IO::cout << "Nonlinear solver successful. Double timestep size!" << Core::IO::endl;

        divconrefinementlevel_--;
        divconnumfinestep_ = 0;

        stepmax_ = stepmax_ - (stepmax_ - stepn_) / 2;

        // double the time step size
        (*dt_)[0] = (*dt_)[0] * 2;
      }
      else  // otherwise we have to wait one more time step until the step size can be increased
      {
        divconnumfinestep_--;
      }
    }
    return;
  }
}

void STR::TimIntImpl::check_for3_d0_dptc_reset(Inpar::STR::ConvergenceStatus& status)
{
  const int maxnumfinestep = 1;

  if (divcontype_ != Inpar::STR::divcont_adapt_3D0Dptc_ele_err)
    return;
  else if (status == Inpar::STR::conv_success and divconrefinementlevel_ != 0 and
           cardvasc0dman_->Get_k_ptc() != 0.0)
  {
    divconnumfinestep_++;

    if (divconnumfinestep_ == maxnumfinestep)
    {
      if (myrank_ == 0)
      {
        Core::IO::cout << "Nonlinear solver successful. Reset 3D-0D PTC to normal Newton!"
                       << Core::IO::endl;
      }
      divconrefinementlevel_ = 0;
      divconnumfinestep_ = 0;

      // reset k_ptc
      cardvasc0dman_->Reset_k_ptc();
    }
    return;
  }
}

FOUR_C_NAMESPACE_CLOSE
