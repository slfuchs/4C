/*----------------------------------------------------------------------*/
/*! \file
 \brief base algorithm for scalar transport within multiphase porous medium

   \level 3

 *----------------------------------------------------------------------*/

#include "4C_poromultiphase_scatra_base.hpp"

#include "4C_adapter_art_net.hpp"
#include "4C_adapter_porofluidmultiphase_wrapper.hpp"
#include "4C_adapter_poromultiphase.hpp"
#include "4C_adapter_scatra_base_algorithm.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_mat_fluidporo_multiphase.hpp"
#include "4C_mat_scatra_multiporo.hpp"
#include "4C_poromultiphase_scatra_utils.hpp"
#include "4C_poromultiphase_utils.hpp"
#include "4C_scatra_ele.hpp"
#include "4C_scatra_timint_implicit.hpp"
#include "4C_scatra_timint_meshtying_strategy_artery.hpp"
#include "4C_scatra_timint_poromulti.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::PoroMultiPhaseScaTraBase(
    const Epetra_Comm& comm, const Teuchos::ParameterList& globaltimeparams)
    : AlgorithmBase(comm, globaltimeparams),
      poromulti_(Teuchos::null),
      scatra_(Teuchos::null),
      fluxreconmethod_(Inpar::POROFLUIDMULTIPHASE::gradreco_none),
      ndsporofluid_scatra_(-1),
      timertimestep_("PoroMultiPhaseScaTraBase", true),
      dttimestep_(0.0),
      divcontype_(Core::UTILS::IntegralValue<Inpar::PoroMultiPhaseScaTra::DivContAct>(
          globaltimeparams, "DIVERCONT")),
      artery_coupl_(Core::UTILS::IntegralValue<int>(globaltimeparams, "ARTERY_COUPLING"))
{
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::init(
    const Teuchos::ParameterList& globaltimeparams, const Teuchos::ParameterList& algoparams,
    const Teuchos::ParameterList& poroparams, const Teuchos::ParameterList& structparams,
    const Teuchos::ParameterList& fluidparams, const Teuchos::ParameterList& scatraparams,
    const std::string& struct_disname, const std::string& fluid_disname,
    const std::string& scatra_disname, bool isale, int nds_disp, int nds_vel, int nds_solidpressure,
    int ndsporofluid_scatra, const std::map<int, std::set<int>>* nearbyelepairs)
{
  // save the dofset number of the scatra on the fluid dis
  ndsporofluid_scatra_ = ndsporofluid_scatra;

  // access the global problem
  Global::Problem* problem = Global::Problem::instance();

  // Create the two uncoupled subproblems.

  // -------------------------------------------------------------------
  // algorithm construction depending on
  // coupling scheme
  // -------------------------------------------------------------------
  // first of all check for possible couplings
  Inpar::POROMULTIPHASE::SolutionSchemeOverFields solschemeporo =
      Core::UTILS::IntegralValue<Inpar::POROMULTIPHASE::SolutionSchemeOverFields>(
          poroparams, "COUPALGO");
  Inpar::PoroMultiPhaseScaTra::SolutionSchemeOverFields solschemescatraporo =
      Core::UTILS::IntegralValue<Inpar::PoroMultiPhaseScaTra::SolutionSchemeOverFields>(
          algoparams, "COUPALGO");

  // partitioned -- monolithic not possible --> error
  if (solschemeporo !=
          Inpar::POROMULTIPHASE::SolutionSchemeOverFields::solscheme_twoway_monolithic &&
      solschemescatraporo ==
          Inpar::PoroMultiPhaseScaTra::SolutionSchemeOverFields::solscheme_twoway_monolithic)
    FOUR_C_THROW(
        "Your requested coupling is not available: possible couplings are:\n"
        "(STRUCTURE <--> FLUID) <--> SCATRA: partitioned -- partitioned_nested\n"
        "                                    monolithic  -- partitioned_nested\n"
        "                                    monolithic  -- monolithic\n"
        "YOUR CHOICE                       : partitioned -- monolithic");

  // monolithic -- partitioned sequential not possible
  if (solschemeporo ==
          Inpar::POROMULTIPHASE::SolutionSchemeOverFields::solscheme_twoway_monolithic &&
      solschemescatraporo == Inpar::PoroMultiPhaseScaTra::SolutionSchemeOverFields::
                                 solscheme_twoway_partitioned_sequential)
    FOUR_C_THROW(
        "Your requested coupling is not available: possible couplings are:\n"
        "(STRUCTURE <--> FLUID) <--> SCATRA: partitioned -- partitioned_nested\n"
        "                                    monolithic  -- partitioned_nested\n"
        "                                    monolithic  -- monolithic\n"
        "YOUR CHOICE                       : monolithic  -- partitioned_sequential");

  fluxreconmethod_ =
      Core::UTILS::IntegralValue<Inpar::POROFLUIDMULTIPHASE::FluxReconstructionMethod>(
          fluidparams, "FLUX_PROJ_METHOD");

  if (solschemescatraporo ==
          Inpar::PoroMultiPhaseScaTra::SolutionSchemeOverFields::solscheme_twoway_monolithic &&
      fluxreconmethod_ == Inpar::POROFLUIDMULTIPHASE::FluxReconstructionMethod::gradreco_l2)
  {
    FOUR_C_THROW(
        "Monolithic porofluidmultiphase-scatra coupling does not work with L2-projection!\n"
        "Set FLUX_PROJ_METHOD to none if you want to use monolithic coupling or use partitioned "
        "approach instead.");
  }

  poromulti_ = POROMULTIPHASE::UTILS::CreatePoroMultiPhaseAlgorithm(
      solschemeporo, globaltimeparams, get_comm());

  // initialize
  poromulti_->init(globaltimeparams, poroparams, structparams, fluidparams, struct_disname,
      fluid_disname, isale, nds_disp, nds_vel, nds_solidpressure, ndsporofluid_scatra,
      nearbyelepairs);

  // get the solver number used for ScalarTransport solver
  const int linsolvernumber = scatraparams.get<int>("LINEAR_SOLVER");

  // scatra problem
  scatra_ = Teuchos::rcp(new Adapter::ScaTraBaseAlgorithm(globaltimeparams, scatraparams,
      problem->solver_params(linsolvernumber), scatra_disname, true));

  // initialize the base algo.
  // scatra time integrator is constructed and initialized inside.
  scatra_->init();
  scatra_->sca_tra_field()->set_number_of_dof_set_displacement(1);
  scatra_->sca_tra_field()->set_number_of_dof_set_velocity(1);
  scatra_->sca_tra_field()->set_number_of_dof_set_pressure(2);

  // do we perform coupling with 1D artery
  if (artery_coupl_)
  {
    // get mesh tying strategy
    scatramsht_ = Teuchos::rcp_dynamic_cast<ScaTra::MeshtyingStrategyArtery>(
        scatra_->sca_tra_field()->strategy());
    if (scatramsht_ == Teuchos::null) FOUR_C_THROW("cast to Meshtying strategy failed!");

    scatramsht_->set_artery_time_integrator(poro_field()->fluid_field()->art_net_tim_int());
    scatramsht_->set_nearby_ele_pairs(nearbyelepairs);
  }

  // only now we must call setup() on the scatra time integrator.
  // all objects relying on the parallel distribution are
  // created and pointers are set.
  // calls setup() on the scatra time integrator inside.
  scatra_->sca_tra_field()->setup();

  // do we perform coupling with 1D artery
  if (artery_coupl_)
  {
    // this check can only be performed after calling setup
    scatramsht_->check_initial_fields();
  }

  std::vector<int> mydirichdofs(0);
  add_dirichmaps_volfrac_spec_ = Teuchos::rcp(new Epetra_Map(
      -1, 0, mydirichdofs.data(), 0, scatra_algo()->sca_tra_field()->discretization()->get_comm()));

  // done.
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::read_restart(int restart)
{
  if (restart)
  {
    // read restart data for structure field (will set time and step internally)
    poromulti_->read_restart(restart);

    // read restart data for scatra field (will set time and step internally)
    scatra_->sca_tra_field()->read_restart(restart);
    if (artery_coupl_) scatramsht_->art_scatra_field()->read_restart(restart);

    // reset time and step for the global algorithm
    set_time_step(scatra_->sca_tra_field()->time(), restart);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::timeloop()
{
  prepare_time_loop();

  while (not_finished())
  {
    prepare_time_step();

    // reset timer
    timertimestep_.reset();
    // *********** time measurement ***********
    double dtcpu = timertimestep_.wallTime();
    // *********** time measurement ***********
    time_step();
    // *********** time measurement ***********
    double mydttimestep = timertimestep_.wallTime() - dtcpu;
    get_comm().MaxAll(&mydttimestep, &dttimestep_, 1);
    // *********** time measurement ***********

    update_and_output();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::prepare_time_step(bool printheader)
{
  // the global control routine has its own time_ and step_ variables, as well as the single fields
  // keep them in sync!
  increment_time_and_step();

  if (printheader) print_header();

  set_poro_solution();
  scatra_->sca_tra_field()->prepare_time_step();
  if (artery_coupl_) scatramsht_->art_scatra_field()->prepare_time_step();
  // set structure-based scalar transport values
  set_scatra_solution();

  poromulti_->prepare_time_step();
  set_poro_solution();
  apply_additional_dbc_for_vol_frac_species();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::prepare_time_loop()
{
  // set structure-based scalar transport values
  set_scatra_solution();
  poromulti_->prepare_time_loop();
  // initial output for scatra field
  set_poro_solution();
  if (scatra_->sca_tra_field()->has_external_force())
    scatra_->sca_tra_field()->set_external_force();
  scatra_->sca_tra_field()->check_and_write_output_and_restart();
  if (artery_coupl_) scatramsht_->art_scatra_field()->check_and_write_output_and_restart();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::update_and_output()
{
  // set scatra on fluid (necessary for possible domain integrals)
  set_scatra_solution();
  poromulti_->update_and_output();

  // scatra field
  scatra_->sca_tra_field()->update();
  scatra_->sca_tra_field()->evaluate_error_compared_to_analytical_sol();
  scatra_->sca_tra_field()->check_and_write_output_and_restart();
  // artery scatra field
  if (artery_coupl_)
  {
    scatramsht_->art_scatra_field()->update();
    scatramsht_->art_scatra_field()->evaluate_error_compared_to_analytical_sol();
    scatramsht_->art_scatra_field()->check_and_write_output_and_restart();
  }
  if (get_comm().MyPID() == 0)
  {
    std::cout << "Finished POROMULTIPHASESCATRA STEP " << std::setw(5) << std::setprecision(4)
              << std::scientific << step() << "/" << std::setw(5) << std::setprecision(4)
              << std::scientific << n_step() << ": dtstep = " << dttimestep_ << std::endl;
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::create_field_test()
{
  Global::Problem* problem = Global::Problem::instance();

  poromulti_->create_field_test();
  problem->add_field_test(scatra_->create_sca_tra_field_test());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::set_poro_solution()
{
  // safety check
  Teuchos::RCP<ScaTra::ScaTraTimIntPoroMulti> poroscatra =
      Teuchos::rcp_dynamic_cast<ScaTra::ScaTraTimIntPoroMulti>(scatra_->sca_tra_field());
  if (poroscatra == Teuchos::null) FOUR_C_THROW("cast to ScaTraTimIntPoroMulti failed!");

  // set displacements
  poroscatra->apply_mesh_movement(poromulti_->struct_dispnp());

  // set the fluid solution
  poroscatra->set_solution_field_of_multi_fluid(
      poromulti_->relaxed_fluid_phinp(), poromulti_->fluid_field()->phin());

  // additionally, set nodal flux if L2-projection is desired
  if (fluxreconmethod_ == Inpar::POROFLUIDMULTIPHASE::FluxReconstructionMethod::gradreco_l2)
    poroscatra->set_l2_flux_of_multi_fluid(poromulti_->fluid_flux());

  if (artery_coupl_)
  {
    scatramsht_->set_artery_pressure();
    scatramsht_->apply_mesh_movement();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::apply_additional_dbc_for_vol_frac_species()
{
  // remove the old one
  scatra_algo()->sca_tra_field()->remove_dirich_cond(add_dirichmaps_volfrac_spec_);

  std::vector<int> mydirichdofs(0);

  // get map and validdof-vector
  const Epetra_Map* elecolmap = scatra_algo()->sca_tra_field()->discretization()->element_col_map();
  Teuchos::RCP<const Epetra_Vector> valid_volfracspec_dofs =
      poro_field()->fluid_field()->valid_vol_frac_spec_dofs();

  // we identify the volume fraction species dofs which do not have a physical meaning and set a
  // DBC on them
  for (int iele = 0; iele < elecolmap->NumMyElements(); ++iele)
  {
    // dynamic_cast necessary because virtual inheritance needs runtime information
    Discret::ELEMENTS::Transport* myele = dynamic_cast<Discret::ELEMENTS::Transport*>(
        scatra_algo()->sca_tra_field()->discretization()->g_element(elecolmap->GID(iele)));

    const Core::Mat::Material& material2 = *(myele->material(2));

    // check the material
    if (material2.material_type() != Core::Materials::m_fluidporo_multiphase and
        material2.material_type() != Core::Materials::m_fluidporo_multiphase_reactions)
      FOUR_C_THROW("only poro multiphase and poro multiphase reactions material valid");

    // cast fluid material
    const Mat::FluidPoroMultiPhase& multiphasemat =
        static_cast<const Mat::FluidPoroMultiPhase&>(material2);

    const int numfluidphases = multiphasemat.num_fluid_phases();
    const int numvolfrac = multiphasemat.num_vol_frac();
    const int numfluidmat = multiphasemat.num_mat();

    // this is only necessary if we have volume fractions present
    // TODO: this works only if we have the same number of phases in every element
    if (numfluidmat == numfluidphases) return;

    const Core::Mat::Material& material = *(myele->material());

    // cast scatra material
    const Mat::MatList& scatramat = static_cast<const Mat::MatList&>(material);

    if (not(scatramat.material_type() == Core::Materials::m_matlist or
            scatramat.material_type() == Core::Materials::m_matlist_reactions))
      FOUR_C_THROW("wrong type of ScaTra-Material");

    const int numscatramat = scatramat.num_mat();

    Core::Nodes::Node** nodes = myele->nodes();

    for (int inode = 0; inode < (myele->num_node()); inode++)
    {
      if (nodes[inode]->owner() ==
          scatra_algo()->sca_tra_field()->discretization()->get_comm().MyPID())
      {
        std::vector<int> scatradofs =
            scatra_algo()->sca_tra_field()->discretization()->dof(0, nodes[inode]);
        std::vector<int> fluiddofs =
            poro_field()->fluid_field()->discretization()->dof(0, nodes[inode]);

        for (int idof = 0; idof < numscatramat; ++idof)
        {
          int matid = scatramat.mat_id(idof);
          Teuchos::RCP<Core::Mat::Material> singlemat = scatramat.material_by_id(matid);
          if (singlemat->material_type() == Core::Materials::m_scatra_multiporo_fluid ||
              singlemat->material_type() == Core::Materials::m_scatra_multiporo_solid ||
              singlemat->material_type() == Core::Materials::m_scatra_multiporo_temperature)
          {
            // do nothing
          }
          else if (singlemat->material_type() == Core::Materials::m_scatra_multiporo_volfrac)
          {
            const Teuchos::RCP<const Mat::ScatraMatMultiPoroVolFrac>& scatravolfracmat =
                Teuchos::rcp_dynamic_cast<const Mat::ScatraMatMultiPoroVolFrac>(singlemat);

            const int scalartophaseid = scatravolfracmat->phase_id();
            // if not already in original dirich map     &&   if it is not a valid volume fraction
            // species dof identified with < 1
            if (scatra_algo()->sca_tra_field()->dirich_maps()->cond_map()->LID(scatradofs[idof]) ==
                    -1 &&
                (int)(*valid_volfracspec_dofs)
                        [poro_field()->fluid_field()->discretization()->dof_row_map()->LID(
                            fluiddofs[scalartophaseid + numvolfrac])] < 1)
            {
              mydirichdofs.push_back(scatradofs[idof]);
              scatra_algo()->sca_tra_field()->phinp()->ReplaceGlobalValue(scatradofs[idof], 0, 0.0);
            }
          }
          else
            FOUR_C_THROW("only MAT_scatra_multiporo_(fluid,volfrac,solid,temperature) valid here");
        }
      }
    }
  }

  // build map
  int nummydirichvals = mydirichdofs.size();
  add_dirichmaps_volfrac_spec_ = Teuchos::rcp(new Epetra_Map(-1, nummydirichvals,
      mydirichdofs.data(), 0, scatra_algo()->sca_tra_field()->discretization()->get_comm()));

  // add the condition
  scatra_algo()->sca_tra_field()->add_dirich_cond(add_dirichmaps_volfrac_spec_);

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::set_scatra_solution()
{
  poromulti_->set_scatra_solution(ndsporofluid_scatra_, scatra_->sca_tra_field()->phinp());
  if (artery_coupl_)
    poromulti_->fluid_field()->art_net_tim_int()->discretization()->set_state(
        2, "one_d_artery_phinp", scatramsht_->art_scatra_field()->phinp());
}

/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::scatra_dof_row_map()
    const
{
  return scatra_->sca_tra_field()->dof_row_map();
}

/*------------------------------------------------------------------------*
 *------------------------------------------------------------------------*/
void PoroMultiPhaseScaTra::PoroMultiPhaseScaTraBase::handle_divergence() const
{
  switch (divcontype_)
  {
    case Inpar::PoroMultiPhaseScaTra::divcont_continue:
    {
      // warn if itemax is reached without convergence, but proceed to
      // next timestep...
      if (get_comm().MyPID() == 0)
      {
        std::cout << "+---------------------------------------------------------------+"
                  << std::endl;
        std::cout << "|            >>>>>> continuing to next time step!               |"
                  << std::endl;
        std::cout << "+---------------------------------------------------------------+"
                  << std::endl
                  << std::endl;
      }
      break;
    }
    case Inpar::PoroMultiPhaseScaTra::divcont_stop:
    {
      FOUR_C_THROW("POROMULTIPHASESCATRA nonlinear solver not converged in ITEMAX steps!");
      break;
    }
    default:
      FOUR_C_THROW("unknown divercont action!");
      break;
  }
}

FOUR_C_NAMESPACE_CLOSE
