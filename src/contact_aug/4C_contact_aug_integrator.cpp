/*---------------------------------------------------------------------*/
/*! \file
\brief A class to perform integrations of Mortar matrices on the overlap
       of two Mortar::Elements in 1D and 2D (derived version for
       augmented contact)

\level 2

*/
/*---------------------------------------------------------------------*/
#include "4C_contact_aug_integrator.hpp"

#include "4C_contact_aug_contact_integrator_utils.hpp"
#include "4C_contact_aug_element_utils.hpp"
#include "4C_contact_element.hpp"
#include "4C_contact_node.hpp"
#include "4C_contact_paramsinterface.hpp"
#include "4C_fem_general_utils_integration.hpp"
#include "4C_linalg_serialdensevector.hpp"
#include "4C_mortar_coupling3d_classes.hpp"

#include <Epetra_Map.h>
#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN

// define and initialize static member
CONTACT::INTEGRATOR::UniqueProjInfoPair CONTACT::Aug::IntegrationWrapper::projInfo_(0);

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
CONTACT::Aug::IntegrationWrapper::IntegrationWrapper(
    Teuchos::ParameterList& params, Core::FE::CellType eletype, const Epetra_Comm& comm)
    : CONTACT::Integrator::Integrator(params, eletype, comm), integrator_(nullptr)

{
  // empty constructor body
  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Aug::IntegrationWrapper::integrate_deriv_cell3_d_aux_plane(Mortar::Element& sele,
    Mortar::Element& mele, Teuchos::RCP<Mortar::IntCell> cell, double* auxn,
    const Epetra_Comm& comm, const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr)
{
  if (cparams_ptr.is_null()) FOUR_C_THROW("The contact parameter interface pointer is undefined!");

  // explicitly defined shape function type needed
  if (shape_fcn() == Inpar::Mortar::shape_undefined)
    FOUR_C_THROW(
        "ERROR: integrate_deriv_cell3_d_aux_plane called without specific shape "
        "function defined!");

  // check for problem dimension
  FOUR_C_ASSERT(Dim() == 3, "ERROR: 3D integration method called for non-3D problem");

  // check input data
  if ((!sele.IsSlave()) || (mele.IsSlave()))
    FOUR_C_THROW(
        "ERROR: integrate_deriv_cell3_d_aux_plane called on a wrong type of "
        "Mortar::Element pair!");
  if (cell == Teuchos::null)
    FOUR_C_THROW("integrate_deriv_cell3_d_aux_plane called without integration cell");

  if (shape_fcn() == Inpar::Mortar::shape_dual ||
      shape_fcn() == Inpar::Mortar::shape_petrovgalerkin)
    FOUR_C_THROW(
        "ERROR: integrate_deriv_cell3_d_aux_plane supports no Dual shape functions for the "
        "augmented Lagrange solving strategy!");

  GlobalTimeMonitor* timer_ptr = cparams_ptr->GetTimer<GlobalTimeID>(0);

  timer_ptr->start(GlobalTimeID::integrate_deriv_cell3_d_aux_plane);
  integrator_ = IntegratorGeneric::Create(Dim(), sele.Shape(), mele.Shape(), *cparams_ptr, this);
  integrator_->integrate_deriv_cell3_d_aux_plane(sele, mele, *cell, auxn);
  timer_ptr->stop(GlobalTimeID::integrate_deriv_cell3_d_aux_plane);

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Aug::IntegrationWrapper::IntegrateDerivEle3D(Mortar::Element& sele,
    std::vector<Mortar::Element*> meles, bool* boundary_ele, bool* proj, const Epetra_Comm& comm,
    const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr)
{
  TEUCHOS_FUNC_TIME_MONITOR(CONTACT_FUNC_NAME);

  // explicitly defined shape function type needed
  if (shape_fcn() == Inpar::Mortar::shape_undefined)
    FOUR_C_THROW(
        "ERROR: integrate_deriv_cell3_d_aux_plane called without specific shape "
        "function defined!");

  // check for problem dimension
  FOUR_C_ASSERT(Dim() == 3, "ERROR: 3D integration method called for non-3D problem");

  // get slave element nodes themselves for normal evaluation
  Core::Nodes::Node** mynodes = sele.Nodes();
  if (!mynodes) FOUR_C_THROW("IntegrateDerivCell3D: Null pointer!");

  // check input data
  for (unsigned test = 0; test < meles.size(); ++test)
  {
    if ((!sele.IsSlave()) || (meles[test]->IsSlave()))
      FOUR_C_THROW(
          "ERROR: IntegrateDerivCell3D called on a wrong type of "
          "Mortar::Element pair!");
  }

  // contact with wear
  if (wearlaw_ != Inpar::Wear::wear_none) FOUR_C_THROW("Wear is not supported!");

  // Boundary Segmentation check -- HasProj()-check
  //  *boundary_ele = BoundarySegmCheck3D(sele,meles);
  *boundary_ele = false;

  GlobalTimeMonitor* timer_ptr = cparams_ptr->GetTimer<GlobalTimeID>(0);
  timer_ptr->start(GlobalTimeID::IntegrateDerivEle3D);

  *proj = INTEGRATOR::find_feasible_master_elements(sele, meles, boundary_ele, *this, projInfo_);

  for (auto& info_pair : projInfo_)
  {
    Mortar::Element& mele = *(info_pair.first);
    integrator_ = IntegratorGeneric::Create(Dim(), sele.Shape(), mele.Shape(), *cparams_ptr, this);
    integrator_->evaluate(sele, mele, *boundary_ele, info_pair.second);
  }

  timer_ptr->stop(GlobalTimeID::IntegrateDerivEle3D);

  Epetra_Vector* sele_times = cparams_ptr->Get<Epetra_Vector>(0);
  const int slid = sele_times->Map().LID(sele.Id());
  if (slid == -1)
    FOUR_C_THROW("Couldn't find the current slave element GID #%d on proc #%d.", sele.Id(),
        sele_times->Map().Comm().MyPID());
  (*sele_times)[slid] += timer_ptr->getLastTimeIncr();

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Aug::IntegrationWrapper::integrate_deriv_slave_element(Mortar::Element& sele,
    const Epetra_Comm& comm, const Teuchos::RCP<Mortar::ParamsInterface>& mparams_ptr)
{
  Teuchos::RCP<CONTACT::ParamsInterface> cparams_ptr =
      Teuchos::rcp_dynamic_cast<CONTACT::ParamsInterface>(mparams_ptr, true);

  integrate_deriv_slave_element(sele, comm, cparams_ptr);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Aug::IntegrationWrapper::integrate_deriv_slave_element(Mortar::Element& sele,
    const Epetra_Comm& comm, const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr)
{
  if (cparams_ptr.is_null()) FOUR_C_THROW("The contact parameter interface pointer is undefined!");

  integrator_ = IntegratorGeneric::Create(Dim(), sele.Shape(), sele.Shape(), *cparams_ptr, this);
  integrator_->integrate_deriv_slave_element(sele);

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Aug::IntegrationWrapper::integrate_deriv_segment2_d(Mortar::Element& sele,
    double& sxia, double& sxib, Mortar::Element& mele, double& mxia, double& mxib,
    const Epetra_Comm& comm, const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr)
{
  // *********************************************************************
  // Check integrator input for non-reasonable quantities
  // *********************************************************************
  if (cparams_ptr.is_null()) FOUR_C_THROW("The contact parameter interface pointer is undefined!");

  // explicitly defined shape function type needed
  if (shape_fcn() == Inpar::Mortar::shape_undefined)
    FOUR_C_THROW("integrate_deriv_segment2_d called without specific shape function defined!");

  // Petrov-Galerkin approach for LM not yet implemented for quadratic FE
  if (sele.Shape() == Core::FE::CellType::line3 ||
      shape_fcn() == Inpar::Mortar::shape_petrovgalerkin)
    FOUR_C_THROW("Petrov-Galerkin / quadratic FE interpolation not yet implemented.");

  // check for problem dimension
  FOUR_C_ASSERT(Dim() == 2, "ERROR: 2D integration method called for non-2D problem");

  // check input data
  if ((!sele.IsSlave()) || (mele.IsSlave()))
    FOUR_C_THROW("IntegrateAndDerivSegment called on a wrong type of Mortar::Element pair!");
  if ((sxia < -1.0) || (sxib > 1.0))
    FOUR_C_THROW("IntegrateAndDerivSegment called with infeasible slave limits!");
  if ((mxia < -1.0) || (mxib > 1.0))
    FOUR_C_THROW("IntegrateAndDerivSegment called with infeasible master limits!");

  GlobalTimeMonitor* timer_ptr = cparams_ptr->GetTimer<GlobalTimeID>(0);
  timer_ptr->start(GlobalTimeID::integrate_deriv_segment2_d);

  integrator_ = IntegratorGeneric::Create(Dim(), sele.Shape(), mele.Shape(), *cparams_ptr, this);
  integrator_->integrate_deriv_segment2_d(sele, sxia, sxib, mele, mxia, mxib);

  timer_ptr->stop(GlobalTimeID::integrate_deriv_segment2_d);

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::Aug::IntegrationWrapper::IntegrateDerivEle2D(Mortar::Element& sele,
    std::vector<Mortar::Element*> meles, bool* boundary_ele,
    const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr)
{
  TEUCHOS_FUNC_TIME_MONITOR(CONTACT_FUNC_NAME);

  // *********************************************************************
  // Check integrator input for non-reasonable quantities
  // *********************************************************************
  if (cparams_ptr.is_null()) FOUR_C_THROW("The contact parameter interface pointer is undefined!");

  // explicitly defined shape function type needed
  if (shape_fcn() == Inpar::Mortar::shape_undefined)
    FOUR_C_THROW("integrate_deriv_segment2_d called without specific shape function defined!");

  // check for problem dimension
  if (Dim() != 2) FOUR_C_THROW("2D integration method called for non-2D problem");

  // get slave element nodes themselves
  Core::Nodes::Node** mynodes = sele.Nodes();
  if (!mynodes) FOUR_C_THROW("IntegrateAndDerivSegment: Null pointer!");

  // check input data
  for (int i = 0; i < (int)meles.size(); ++i)
  {
    if ((!sele.IsSlave()) || (meles[i]->IsSlave()))
      FOUR_C_THROW("IntegrateAndDerivSegment called on a wrong type of Mortar::Element pair!");
  }

  // number of nodes (slave) and problem dimension
  const int nrow = sele.num_node();

  // decide whether boundary modification has to be considered or not
  // this is element-specific (is there a boundary node in this element?)
  for (int k = 0; k < nrow; ++k)
  {
    Mortar::Node* mymrtrnode = dynamic_cast<Mortar::Node*>(mynodes[k]);
    if (!mymrtrnode) FOUR_C_THROW("integrate_deriv_segment2_d: Null pointer!");
  }

  GlobalTimeMonitor* timer_ptr = cparams_ptr->GetTimer<GlobalTimeID>(0);
  timer_ptr->start(GlobalTimeID::IntegrateDerivEle2D);

  // Boundary Segmentation check -- HasProj()-check
  if (IntegrationType() == Inpar::Mortar::inttype_elements_BS)
    *boundary_ele = BoundarySegmCheck2D(sele, meles);

  if (*boundary_ele == false || IntegrationType() == Inpar::Mortar::inttype_elements)
  {
    INTEGRATOR::find_feasible_master_elements(sele, meles, *this, projInfo_);

    for (auto& info_pair : projInfo_)
    {
      Mortar::Element& mele = *(info_pair.first);
      integrator_ =
          IntegratorGeneric::Create(Dim(), sele.Shape(), mele.Shape(), *cparams_ptr, this);
      integrator_->evaluate(sele, mele, false, info_pair.second);
    }
  }  // boundary_ele check

  timer_ptr->stop(GlobalTimeID::IntegrateDerivEle2D);

  Epetra_Vector* sele_times = cparams_ptr->Get<Epetra_Vector>(0);
  const int slid = sele_times->Map().LID(sele.Id());
  if (slid == -1)
    FOUR_C_THROW("Couldn't find the current slave element GID #%d on proc #%d.", sele.Id(),
        sele_times->Map().Comm().MyPID());
  (*sele_times)[slid] += timer_ptr->getLastTimeIncr();

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::Create(int probdim,
    Core::FE::CellType slavetype, Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams,
    CONTACT::Integrator* wrapper)
{
  switch (probdim)
  {
    case 2:
      return create2_d(slavetype, mastertype, cparams, wrapper);
    case 3:
      return create3_d(slavetype, mastertype, cparams, wrapper);
    default:
      FOUR_C_THROW("Unsupported problem dimension %d", probdim);
      exit(EXIT_FAILURE);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::create2_d(
    Core::FE::CellType slavetype, Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams,
    CONTACT::Integrator* wrapper)
{
  switch (slavetype)
  {
    case Core::FE::CellType::line2:
      return create2_d<Core::FE::CellType::line2>(mastertype, cparams, wrapper);
    case Core::FE::CellType::nurbs2:
      return create2_d<Core::FE::CellType::nurbs2>(mastertype, cparams, wrapper);
    case Core::FE::CellType::nurbs3:
      return create2_d<Core::FE::CellType::nurbs3>(mastertype, cparams, wrapper);
    default:
      FOUR_C_THROW("Unsupported slave element type %d|\"%s\"", slavetype,
          Core::FE::CellTypeToString(slavetype).c_str());
      exit(EXIT_FAILURE);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <Core::FE::CellType slavetype>
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::create2_d(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper)
{
  switch (mastertype)
  {
    case Core::FE::CellType::line2:
      return create2_d<slavetype, Core::FE::CellType::line2>(cparams, wrapper);
    case Core::FE::CellType::nurbs2:
      return create2_d<slavetype, Core::FE::CellType::nurbs2>(cparams, wrapper);
    case Core::FE::CellType::nurbs3:
      return create2_d<slavetype, Core::FE::CellType::nurbs3>(cparams, wrapper);
    default:
      FOUR_C_THROW("Unsupported master element type %d|\"%s\"", mastertype,
          Core::FE::CellTypeToString(mastertype).c_str());
      exit(EXIT_FAILURE);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <Core::FE::CellType slavetype, Core::FE::CellType mastertype>
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::create2_d(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper)
{
  const enum Inpar::CONTACT::VariationalApproach var_type = cparams.get_variational_approach_type();

  switch (var_type)
  {
    case Inpar::CONTACT::var_incomplete:
    {
      typedef DebugIncompleteIntPolicy<2, slavetype, mastertype> incomplete_policy;
      return Integrator<2, slavetype, mastertype, incomplete_policy>::Instance(&cparams, wrapper);
    }
    case Inpar::CONTACT::var_complete:
    {
      typedef DebugCompleteIntPolicy<2, slavetype, mastertype> complete_policy;
      return Integrator<2, slavetype, mastertype, complete_policy>::Instance(&cparams, wrapper);
    }
    default:
    {
      FOUR_C_THROW("Unknown variational approach! (var_type= \"%s\" | %d)",
          Inpar::CONTACT::VariationalApproach2String(var_type).c_str(), var_type);
      exit(EXIT_FAILURE);
    }
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::create3_d(
    Core::FE::CellType slavetype, Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams,
    CONTACT::Integrator* wrapper)
{
  switch (slavetype)
  {
    case Core::FE::CellType::quad4:
      return create3_d<Core::FE::CellType::quad4>(mastertype, cparams, wrapper);
    case Core::FE::CellType::tri3:
      return create3_d<Core::FE::CellType::tri3>(mastertype, cparams, wrapper);
    case Core::FE::CellType::nurbs4:
      return create3_d<Core::FE::CellType::nurbs4>(mastertype, cparams, wrapper);
    case Core::FE::CellType::nurbs9:
      return create3_d<Core::FE::CellType::nurbs9>(mastertype, cparams, wrapper);
    default:
      FOUR_C_THROW("Unsupported slave element type %d|\"%s\"",
          Core::FE::CellTypeToString(mastertype).c_str());
      exit(EXIT_FAILURE);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <Core::FE::CellType slavetype>
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::create3_d(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper)
{
  switch (mastertype)
  {
    case Core::FE::CellType::quad4:
      return create3_d<slavetype, Core::FE::CellType::quad4>(cparams, wrapper);
    case Core::FE::CellType::tri3:
      return create3_d<slavetype, Core::FE::CellType::tri3>(cparams, wrapper);
    case Core::FE::CellType::nurbs4:
      return create3_d<slavetype, Core::FE::CellType::nurbs4>(cparams, wrapper);
    case Core::FE::CellType::nurbs9:
      return create3_d<slavetype, Core::FE::CellType::nurbs9>(cparams, wrapper);
    default:
      FOUR_C_THROW("Unsupported master element type %d|\"%s\"",
          Core::FE::CellTypeToString(mastertype).c_str());
      exit(EXIT_FAILURE);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <Core::FE::CellType slavetype, Core::FE::CellType mastertype>
CONTACT::Aug::IntegratorGeneric* CONTACT::Aug::IntegratorGeneric::create3_d(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper)
{
  const enum Inpar::CONTACT::VariationalApproach var_type = cparams.get_variational_approach_type();

  switch (var_type)
  {
    case Inpar::CONTACT::var_incomplete:
    {
      typedef DebugIncompleteIntPolicy<3, slavetype, mastertype> incomplete_policy;
      return Integrator<3, slavetype, mastertype, incomplete_policy>::Instance(&cparams, wrapper);
    }
    case Inpar::CONTACT::var_complete:
    {
      typedef DebugCompleteIntPolicy<3, slavetype, mastertype> complete_policy;
      return Integrator<3, slavetype, mastertype, complete_policy>::Instance(&cparams, wrapper);
    }
    default:
    {
      FOUR_C_THROW("Unknown variational approach! (var_type= \"%s\" | %d)",
          Inpar::CONTACT::VariationalApproach2String(var_type).c_str(), var_type);
      exit(EXIT_FAILURE);
    }
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>*
CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::Instance(
    CONTACT::ParamsInterface* cparams, CONTACT::Integrator* wrapper)
{
  static auto singleton_owner = Core::UTILS::MakeSingletonOwner(
      []()
      {
        return std::unique_ptr<Integrator<probdim, slavetype, mastertype, IntPolicy>>(
            new Integrator<probdim, slavetype, mastertype, IntPolicy>);
      });

  auto instance = singleton_owner.Instance(Core::UTILS::SingletonAction::create);
  instance->init(cparams, wrapper);
  instance->IntPolicy::timer_.setComm(&wrapper->Comm());

  return instance;
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::Integrator()
    : IntegratorGeneric(), IntPolicy()
{
  /* empty */
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::Integrator(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator& wrapper)
    : IntegratorGeneric(cparams, wrapper), IntPolicy()
{
  /* empty */
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype,
    IntPolicy>::integrate_deriv_segment2_d(Mortar::Element& sele, double& sxia, double& sxib,
    Mortar::Element& mele, double& mxia, double& mxib)
{
  FOUR_C_THROW(
      "Deprecated method! The segmented based integration is no longer "
      "supported!");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype,
    IntPolicy>::integrate_deriv_cell3_d_aux_plane(Mortar::Element& sele, Mortar::Element& mele,
    Mortar::IntCell& cell, double* auxn)
{
  FOUR_C_THROW(
      "Deprecated method! The segmented based integration is no longer "
      "supported!");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype,
    IntPolicy>::integrate_deriv_slave_element(Mortar::Element& sele)
{
  // set evaluator
  const enum Mortar::ActionType action = c_params().get_action_type();
  set_evaluator(action);

  for (int gp = 0; gp < this->wrapper().nGP(); ++gp)
  {
    const std::array<double, 2> eta = {
        this->wrapper().Coordinate(gp, 0), this->wrapper().Coordinate(gp, 1)};
    const double wgt = this->wrapper().Weight(gp);

    // get Gauss point in slave element coordinates
    const double sxi[2] = {eta[0], eta[1]};
    const Core::LinAlg::Matrix<2, 1> sxi_mat(sxi, true);

    // evaluate Lagrange multiplier shape functions (on slave element)
    sele.evaluate_shape_lag_mult(this->shape_fcn(), sxi, lmval_, lmderiv_, my::SLAVENUMNODE, true);

    // evaluate shape function and derivative values (on slave element)
    shape_function_and_deriv1<slavetype>(sele, sxi_mat, sval_, sderiv_);

    // integrate the slave jacobian
    const double jac = sele.Jacobian(sxi);

    // evaluate the convective slave base vectors
    Core::LinAlg::Matrix<3, 2> stau;
    sele.Metrics(sxi, &stau(0, 0), &stau(0, 1));

    // evaluate the slave Jacobian 1-st order derivative
    evaluator_->Deriv_Jacobian(sele, sxi, sderiv_, stau);

    // *** SLAVE NODES ****************************************************
    // compute the tributary area
    gp_aug_a(sele, lmval_, wgt, jac);

    // compute 1-st order derivative of the tributary area
    get_deriv1st_aug_a(sele, lmval_, wgt, jac, derivjac_);

    // compute 2-nd order derivative of the tributary area
    evaluator_->Get_Deriv2nd_AugA(sele, lmval_, wgt, deriv2ndjac_);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::evaluate(
    Mortar::Element& sele, Mortar::Element& mele, bool boundary_ele,
    const CONTACT::INTEGRATOR::UniqueProjInfo& projInfo)
{
  if (this->wrapper().IntegrationType() != Inpar::Mortar::inttype_elements)
    FOUR_C_THROW("How did you come here?");

  const enum Mortar::ActionType action = c_params().get_action_type();

  // set the evaluator: 1-st derivatives only, or 1-st AND 2-nd derivatives
  set_evaluator(action);

  // choose the integration scheme
  switch (action)
  {
    case Mortar::eval_static_constraint_rhs:
    {
      integrate_weighted_gap(sele, mele, boundary_ele, projInfo);
      break;
    }
    case Mortar::eval_force_stiff:
    case Mortar::eval_force:
    {
      integrate_deriv_ele(sele, mele, boundary_ele, projInfo);
      break;
    }
    case Mortar::eval_wgap_gradient_error:
    {
      integrate_weighted_gap_gradient_error(sele, mele, boundary_ele, projInfo);
      break;
    }
    default:
    {
      FOUR_C_THROW("Unconsidered ActionType = %d | \"%s\" ", action,
          Mortar::ActionType2String(action).c_str());
      exit(EXIT_FAILURE);
    }
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::set_evaluator(
    const enum Mortar::ActionType action)
{
  switch (action)
  {
    case Mortar::eval_static_constraint_rhs:
    {
      /* do nothing, since no derivatives have to be evaluated */
      break;
    }
    case Mortar::eval_force:
    case Mortar::eval_wgap_gradient_error:
    {
      if (evaluator_.is_null() or evaluator_->GetType() != Evaluator::Type::deriv1st_only)
        evaluator_ = Teuchos::rcp(new EvaluatorDeriv1stOnly(*this));

      //      static int count = 0;
      //      std::cout << "eval_force = " << ++count << std::endl;
      break;
    }
    case Mortar::eval_force_stiff:
    {
      if (evaluator_.is_null() or evaluator_->GetType() != Evaluator::Type::full)
        evaluator_ = Teuchos::rcp(new EvaluatorFull(*this));

      //      static int count = 0;
      //      std::cout << "eval_force_stiff = " << ++count << std::endl;
      break;
    }
    default:
    {
      FOUR_C_THROW("Unconsidered ActionType = %d | \"%s\" ", action,
          Mortar::ActionType2String(action).c_str());
      exit(EXIT_FAILURE);
    }
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::integrate_deriv_ele(
    Mortar::Element& sele, Mortar::Element& mele, bool boundary_ele,
    const CONTACT::INTEGRATOR::UniqueProjInfo& projInfo)
{
  // get slave and master nodal coords for Jacobian / GP evaluation
  sele.GetNodalCoords(scoord_);

  const int linsize = get_lin_size(sele);

  {
    // get the gausspoints of this slave / master element pair
    const unsigned num_gps = projInfo.gaussPoints_.size();

    //**********************************************************************
    // loop over all Gauss points for integration
    //**********************************************************************
    hard_reset(linsize);

    for (my::gp_id_ = 0; static_cast<unsigned>(my::gp_id_) < num_gps; ++my::gp_id_)
    {
      const int gp = projInfo.gaussPoints_[my::gp_id_];

      // coordinates and weight
      const std::array<double, 2> eta = {
          this->wrapper().Coordinate(gp, 0), this->wrapper().Coordinate(gp, 1)};
      const double wgt = this->wrapper().Weight(gp) * projInfo.scaling_[my::gp_id_];

      // get Gauss point in slave element coordinates
      const double sxi[2] = {eta[0], eta[1]};
      const Core::LinAlg::Matrix<2, 1> sxi_mat(sxi, true);

      // evaluate Lagrange multiplier shape functions (on slave element)
      sele.evaluate_shape_lag_mult(
          this->shape_fcn(), sxi, lmval_, lmderiv_, my::SLAVENUMNODE, true);

      // evaluate trace space shape functions (on both elements)
      shape_function_and_deriv1<slavetype>(sele, sxi_mat, sval_, sderiv_);

      // evaluate the convective slave base vectors
      Core::LinAlg::Matrix<3, 2> stau;
      sele.Metrics(sxi, &stau(0, 0), &stau(0, 1));

      // evaluate the two Jacobians (int. cell and slave element)
      const double jacslave = sele.Jacobian(sxi);

      // evaluate linearizations *******************************************
      // evaluate the slave Jacobian 1-st and 2-nd order derivatives
      //      Deriv_Jacobian( sele, sxi, sderiv_, stau, derivjac_, &deriv2ndjac_ );
      evaluator_->Deriv_Jacobian(sele, sxi, sderiv_, stau);

      const double uniqueProjalpha = projInfo.uniqueProjAlpha_[my::gp_id_];
      const Core::LinAlg::Matrix<2, 1>& uniqueMxi = projInfo.uniqueMxi_[my::gp_id_];

      mele.GetNodalCoords(mcoord_);

      // get mval
      shape_function_and_deriv1_and_deriv2<mastertype>(mele, uniqueMxi, mval_, mderiv_, mderiv2nd_);

      // evaluate the convective master base vectors
      Core::LinAlg::Matrix<3, 2> mtau;
      mele.Metrics(uniqueMxi.A(), &mtau(0, 0), &mtau(0, 1));

      // evaluate the GP master coordinate 1-st and 2-nd order derivatives
      evaluator_->Deriv_MXiGP(
          sele, mele, sxi, uniqueMxi.A(), uniqueProjalpha, sval_, mval_, mderiv_, mtau);

      //**********************************************************************
      // evaluate at GP and lin char. quantities
      //**********************************************************************
      // calculate the averaged normal + derivative at gp level
      gp_normal_deriv_normal(sele, sval_, gpn_, dn_non_unit_, ddn_non_unit_, dn_unit_, ddn_unit_);

      // integrate scaling factor kappa
      gp_kappa(sele, lmval_, wgt, jacslave);

      // integrate the inner integral relating to the first order derivative of
      // the discrete normal gap for later usage (for all found slave nodes)
      IntPolicy::Get_Deriv1st_GapN(
          sele, mele, sval_, mval_, gpn_, mtau, dmxigp_, deriv_gapn_sl_, deriv_gapn_ma_);

      // evaluate normal gap (split into slave and master contributions)
      double gapn_sl = 0.0;
      double gapn_ma = 0.0;
      gap_n(sele, mele, sval_, mval_, gpn_, gapn_sl, gapn_ma);

      // evaluate the weighted gap (slave / master)
      gp_w_gap(sele, lmval_, gapn_sl, gapn_ma, wgt, jacslave);

      // 1-st order derivative of the weighted gap (variation)
      IntPolicy::Get_Deriv1st_WGap(
          sele, lmval_, gapn_sl, gapn_ma, wgt, jacslave, derivjac_, deriv_gapn_sl_, deriv_gapn_ma_);

      // 1-st order derivative of the weighted gap (necessary for the
      // linearization of the constraint equations in case of the complete AND
      // incomplete variational approach)
      IntPolicy::get_deriv1st_w_gap_complete(linsize, sele, mele, sval_, mval_, lmval_, gpn_, mtau,
          dmxigp_, gapn_sl, gapn_ma, wgt, jacslave, derivjac_);

      IntPolicy::Get_Debug(sele, lmval_, gapn_sl, gapn_ma, wgt, jacslave, gpn_, uniqueMxi.A());

      IntPolicy::Get_Deriv1st_Debug(sele, lmval_, sval_, sderiv_, stau, derivjac_, dmxigp_,
          dn_unit_, deriv_gapn_sl_, gapn_sl, wgt, jacslave);

      switch (c_params().get_action_type())
      {
        case Mortar::eval_force_stiff:
        {
          get_deriv1st_kappa(sele, lmval_, wgt, derivjac_);

          get_deriv2nd_kappa(sele, lmval_, wgt, deriv2ndjac_);

          IntPolicy::Get_Deriv2nd_WGap(sele, mele, sval_, mval_, lmval_, mderiv_, mderiv2nd_, mtau,
              gpn_, wgt, gapn_sl, gapn_ma, jacslave, derivjac_, deriv2ndjac_, dmxigp_, ddmxigp_,
              dn_unit_, ddn_unit_, deriv_gapn_sl_, deriv_gapn_ma_);

          IntPolicy::Get_Deriv2nd_Debug(sele, lmval_, sval_, sderiv_, stau, derivjac_,
              deriv_gapn_sl_, deriv2ndjac_, ddmxigp_, dn_unit_, ddn_unit_, gapn_sl, wgt, jacslave);

          break;
        }
        default:
          // do nothing
          break;
      }

      weak_reset(linsize);
    }  // GP-loop

    IntPolicy::CompleteNodeData(sele);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::integrate_weighted_gap(
    Mortar::Element& sele, Mortar::Element& mele, bool boundary_ele,
    const CONTACT::INTEGRATOR::UniqueProjInfo& projInfo)
{
  // get slave and master nodal coords for Jacobian / GP evaluation
  sele.GetNodalCoords(scoord_);

  const int linsize = get_lin_size(sele);

  {
    // get the gausspoints of this slave / master element pair
    const unsigned num_gps = projInfo.gaussPoints_.size();

    //**********************************************************************
    // loop over all Gauss points for integration
    //**********************************************************************
    hard_reset(linsize);

    for (my::gp_id_ = 0; static_cast<unsigned>(my::gp_id_) < num_gps; ++my::gp_id_)
    {
      const int gp = projInfo.gaussPoints_[my::gp_id_];

      // coordinates and weight
      const std::array<double, 2> eta = {
          this->wrapper().Coordinate(gp, 0), this->wrapper().Coordinate(gp, 1)};
      const double wgt = this->wrapper().Weight(gp) * projInfo.scaling_[my::gp_id_];

      // get Gauss point in slave element coordinates
      const double sxi[2] = {eta[0], eta[1]};
      const Core::LinAlg::Matrix<2, 1> sxi_mat(sxi, true);

      // evaluate Lagrange multiplier shape functions (on slave element)
      sele.evaluate_shape_lag_mult(
          this->shape_fcn(), sxi, lmval_, lmderiv_, my::SLAVENUMNODE, true);

      // evaluate trace space shape functions (on both elements)
      shape_function_and_deriv1<slavetype>(sele, sxi_mat, sval_, sderiv_);

      // evaluate the two Jacobians (int. cell and slave element)
      const double jacslave = sele.Jacobian(sxi);

      const Core::LinAlg::Matrix<2, 1>& uniqueMxi = projInfo.uniqueMxi_[my::gp_id_];

      mele.GetNodalCoords(mcoord_);

      // get mval and mderiv1
      shape_function_and_deriv1<mastertype>(mele, uniqueMxi, mval_, mderiv_);

      // integrate scaling factor kappa
      gp_kappa(sele, lmval_, wgt, jacslave);

      // calculate the averaged unified GP normal
      gp_normal(sele, sval_, gpn_);

      // evaluate normal gap (split into slave and master contributions)
      double gapn_sl = 0.0;
      double gapn_ma = 0.0;
      gap_n(sele, mele, sval_, mval_, gpn_, gapn_sl, gapn_ma);

      // evaluate the weighted gap (slave / master)
      gp_w_gap(sele, lmval_, gapn_sl, gapn_ma, wgt, jacslave);

      weak_reset(linsize);
    }  // GP-loop
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype,
    IntPolicy>::integrate_weighted_gap_gradient_error(Mortar::Element& sele, Mortar::Element& mele,
    bool boundary_ele, const CONTACT::INTEGRATOR::UniqueProjInfo& projInfo)
{
  // access unordered maps
  std::unordered_map<int, Deriv1stMap>* grad_error_ma_ptr =
      this->c_params().template GetUnorderedMap<int, Deriv1stMap>(0);
  std::unordered_map<int, Deriv1stMap>* grad_error_jac_ptr =
      this->c_params().template GetUnorderedMap<int, Deriv1stMap>(1);

  // get slave and master nodal coords for Jacobian / GP evaluation
  sele.GetNodalCoords(scoord_);

  const int linsize = get_lin_size(sele);

  std::vector<unsigned> active_nlids;
  active_nlids.reserve(my::SLAVENUMNODE);
  extract_active_slave_node_li_ds(active_nlids, sele);

  {
    // get the gausspoints of this slave / master element pair
    const unsigned num_gps = projInfo.gaussPoints_.size();

    //**********************************************************************
    // loop over all Gauss points for integration
    //**********************************************************************
    hard_reset(linsize);

    for (my::gp_id_ = 0; static_cast<unsigned>(my::gp_id_) < num_gps; ++my::gp_id_)
    {
      const int gp = projInfo.gaussPoints_[my::gp_id_];

      // coordinates and weight
      const std::array<double, 2> eta = {
          this->wrapper().Coordinate(gp, 0), this->wrapper().Coordinate(gp, 1)};
      const double wgt = this->wrapper().Weight(gp) * projInfo.scaling_[my::gp_id_];

      // get Gauss point in slave element coordinates
      const double sxi[2] = {eta[0], eta[1]};
      const Core::LinAlg::Matrix<2, 1> sxi_mat(sxi, true);

      // evaluate Lagrange multiplier shape functions (on slave element)
      sele.evaluate_shape_lag_mult(
          this->shape_fcn(), sxi, lmval_, lmderiv_, my::SLAVENUMNODE, true);

      // evaluate trace space shape functions (on both elements)
      shape_function_and_deriv1<slavetype>(sele, sxi_mat, sval_, sderiv_);

      // evaluate the convective slave base vectors
      Core::LinAlg::Matrix<3, 2> stau;
      sele.Metrics(sxi, &stau(0, 0), &stau(0, 1));

      // evaluate the two Jacobians (int. cell and slave element)
      const double jacslave = sele.Jacobian(sxi);

      // evaluate linearizations *******************************************
      // evaluate the slave Jacobian 1-st and 2-nd order derivatives
      evaluator_->Deriv_Jacobian(sele, sxi, sderiv_, stau);

      const double uniqueProjalpha = projInfo.uniqueProjAlpha_[my::gp_id_];
      const Core::LinAlg::Matrix<2, 1>& uniqueMxi = projInfo.uniqueMxi_[my::gp_id_];

      mele.GetNodalCoords(mcoord_);

      // get mval and mderiv1
      shape_function_and_deriv1<mastertype>(mele, uniqueMxi, mval_, mderiv_);

      // evaluate the convective master base vectors
      Core::LinAlg::Matrix<3, 2> mtau;
      mele.Metrics(uniqueMxi.A(), &mtau(0, 0), &mtau(0, 1));

      // evaluate the GP master coordinate 1-st and 2-nd order derivatives
      evaluator_->Deriv_MXiGP(
          sele, mele, sxi, uniqueMxi.A(), uniqueProjalpha, sval_, mval_, mderiv_, mtau);

      //**********************************************************************
      // evaluate at GP and lin char. quantities
      //**********************************************************************
      // calculate the averaged normal + derivative at gp level
      gp_normal_deriv_normal(sele, sval_, gpn_, dn_non_unit_, ddn_non_unit_, dn_unit_, ddn_unit_);

      // integrate the inner integral relating to the first order derivative of
      // the discrete normal gap for later usage (for all found slave nodes)
      IntPolicy::Get_Deriv1st_GapN(
          sele, mele, sval_, mval_, gpn_, mtau, dmxigp_, deriv_gapn_sl_, deriv_gapn_ma_);

      // evaluate normal gap (split into slave and master contributions)
      double gapn_sl = 0.0;
      double gapn_ma = 0.0;
      gap_n(sele, mele, sval_, mval_, gpn_, gapn_sl, gapn_ma);

      IntPolicy::get_deriv1st_w_gap_n_error(sele, active_nlids, lmval_, gpn_, gapn_sl, gapn_ma, wgt,
          jacslave, derivjac_, mtau, dmxigp_, deriv_gapn_ma_, *grad_error_ma_ptr,
          *grad_error_jac_ptr);

      weak_reset(linsize);
    }  // GP-loop
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
int CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::get_lin_size(
    Mortar::Element& sele) const
{
  int linsize = 0;
  const Core::Nodes::Node* const* mynodes = sele.Nodes();
  for (unsigned i = 0; i < my::SLAVENUMNODE; ++i)
  {
    const Node& cnode = static_cast<const Node&>(*mynodes[i]);
    linsize += cnode.GetLinsize();
  }

  return linsize;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype,
    IntPolicy>::extract_active_slave_node_li_ds(std::vector<unsigned>& active_nlids,
    const Mortar::Element& sele) const
{
  const Epetra_Map* active_snode_row_map = this->c_params().template Get<Epetra_Map>(1);

  const int* nodeids = sele.NodeIds();

  for (unsigned i = 0; i < my::SLAVENUMNODE; ++i)
  {
    if (active_snode_row_map->LID(nodeids[i]) != -1)
    {
      active_nlids.push_back(i);
    }
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::hard_reset(
    const unsigned linsize)
{
  Core::Gen::reset(my::SLAVEDIM, 0, dsxigp_);

  Core::Gen::reset(my::MASTERDIM, linsize + my::MASTERNUMNODE * probdim, dmxigp_);
  Core::Gen::reset(linsize + my::MASTERNUMNODE * probdim, dalpha_);
  Core::Gen::reset(my::MASTERDIM, linsize + my::MASTERNUMNODE * probdim, ddmxigp_);

  std::fill(gpn_, gpn_ + 3, 0.0);
  Core::Gen::reset(probdim, linsize + probdim * my::MASTERNUMNODE, dn_non_unit_);
  Core::Gen::reset(probdim, linsize + probdim * my::MASTERNUMNODE, ddn_non_unit_);
  Core::Gen::reset(probdim, linsize + probdim * my::MASTERNUMNODE, dn_unit_);
  Core::Gen::reset(probdim, linsize + probdim * my::MASTERNUMNODE, ddn_unit_);

  Core::Gen::reset(probdim * my::SLAVENUMNODE, deriv_gapn_sl_);
  Core::Gen::reset(linsize + probdim * my::MASTERNUMNODE, deriv_gapn_ma_);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <unsigned probdim, Core::FE::CellType slavetype, Core::FE::CellType mastertype,
    class IntPolicy>
void CONTACT::Aug::Integrator<probdim, slavetype, mastertype, IntPolicy>::weak_reset(
    const unsigned linsize)
{
  Core::Gen::reset(my::SLAVEDIM, 0, dsxigp_);

  Core::Gen::weak_reset(dmxigp_);
  Core::Gen::weak_reset(dalpha_);
  Core::Gen::weak_reset(ddmxigp_);

  std::fill(gpn_, gpn_ + 3, 0.0);
  Core::Gen::weak_reset(dn_non_unit_);
  Core::Gen::weak_reset(ddn_non_unit_);
  Core::Gen::reset(probdim, linsize + probdim * my::MASTERNUMNODE, dn_unit_);
  Core::Gen::weak_reset(ddn_unit_);

  Core::Gen::reset(probdim * my::SLAVENUMNODE, deriv_gapn_sl_);
  Core::Gen::reset(linsize + probdim * my::MASTERNUMNODE, deriv_gapn_ma_);
}

/*----------------------------------------------------------------------------*/
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::line2>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::line2, Core::FE::CellType::line2>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::nurbs2>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::nurbs2, Core::FE::CellType::nurbs2>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::nurbs2, Core::FE::CellType::nurbs3>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::nurbs3>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::nurbs3, Core::FE::CellType::nurbs3>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create2_d<Core::FE::CellType::nurbs3, Core::FE::CellType::nurbs2>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::quad4>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::quad4, Core::FE::CellType::quad4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::quad4, Core::FE::CellType::tri3>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::quad4, Core::FE::CellType::nurbs4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::quad4, Core::FE::CellType::nurbs9>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::tri3>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::tri3, Core::FE::CellType::quad4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::tri3, Core::FE::CellType::tri3>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::tri3, Core::FE::CellType::nurbs4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::tri3, Core::FE::CellType::nurbs9>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs4>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs4, Core::FE::CellType::nurbs4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs4, Core::FE::CellType::quad4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs4, Core::FE::CellType::tri3>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs4, Core::FE::CellType::nurbs9>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs9>(
    Core::FE::CellType mastertype, CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs9, Core::FE::CellType::nurbs9>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs9, Core::FE::CellType::quad4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs9, Core::FE::CellType::tri3>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);
template CONTACT::Aug::IntegratorGeneric*
CONTACT::Aug::IntegratorGeneric::create3_d<Core::FE::CellType::nurbs9, Core::FE::CellType::nurbs4>(
    CONTACT::ParamsInterface& cparams, CONTACT::Integrator* wrapper);

FOUR_C_NAMESPACE_CLOSE

#include "4C_contact_aug_integrator.inst.hpp"
