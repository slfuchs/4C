/*---------------------------------------------------------------------*/
/*!
\file contact_noxinterface.cpp

\brief Concrete mplementation of all the %NOX::NLN::CONSTRAINT::Interface::Required
       (pure) virtual routines.

\level 3

\maintainer Michael Hiermeier

\date May 2, 2016

*/
/*---------------------------------------------------------------------*/

#include "contact_noxinterface.H"
#include "contact_abstract_strategy.H"

#include "../solver_nonlin_nox/nox_nln_aux.H"

#include "../linalg/linalg_utils.H"

#include <NOX_Epetra_Vector.H>
#include <Epetra_Vector.h>

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
CONTACT::NoxInterface::NoxInterface()
    : isinit_(false),
      issetup_(false),
      strategy_ptr_(Teuchos::null),
      cycling_maps_(std::vector<Teuchos::RCP<Epetra_Map> >(0))
{
  // should stay empty
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::NoxInterface::Init(
    const Teuchos::RCP<CONTACT::CoAbstractStrategy>& strategy_ptr)
{
  issetup_ = false;

  strategy_ptr_ = strategy_ptr;

  // set flag at the end
  isinit_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::NoxInterface::Setup()
{
  CheckInit();

  // set flag at the end
  issetup_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double CONTACT::NoxInterface::GetConstraintRHSNorms(
    const NOX::NLN::StatusTest::QuantityType& chQ,
    const NOX::Abstract::Vector::NormType& type,
    const bool& isScaled) const
{
  if (chQ != NOX::NLN::StatusTest::quantity_contact_normal and
      chQ != NOX::NLN::StatusTest::quantity_contact_friction)
    return -1.0;

  Teuchos::RCP<const Epetra_Vector> constrRhs =
      Strategy().GetRhsBlockPtr(STR::block_constraint);
  // no contact contributions present
  if (constrRhs.is_null())
    return 0.0;

  // export the vector to the current redistributed map
  Teuchos::RCP<Epetra_Vector> constrRhs_red =
      Teuchos::rcp(new Epetra_Vector(Strategy().LMDoFRowMap(true)));
  LINALG::Export(*constrRhs,*constrRhs_red);
  // replace the map
  constrRhs_red->ReplaceMap(Strategy().SlDoFRowMap(true));

  double constrNorm = -1.0;
  Teuchos::RCP<const NOX::Epetra::Vector> constrRhs_nox = Teuchos::null;
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      // create vector with redistributed slave dof row map in normal direction
      Teuchos::RCP<Epetra_Vector> nConstrRhs =
          Teuchos::rcp(new Epetra_Vector(Strategy().SlNormalDoFRowMap(true)));
      LINALG::Export(*constrRhs_red,*nConstrRhs);

      constrRhs_nox = Teuchos::rcp(new NOX::Epetra::Vector(nConstrRhs,
          NOX::Epetra::Vector::CreateView));
      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      // create vector with redistributed slave dof row map in tangential directions
      Teuchos::RCP<Epetra_Vector> tConstrRhs =
          Teuchos::rcp(new Epetra_Vector(Strategy().SlTangentialDoFRowMap(true)));
      LINALG::Export(*constrRhs_red,*tConstrRhs);

      constrRhs_nox = Teuchos::rcp(new NOX::Epetra::Vector(tConstrRhs,
          NOX::Epetra::Vector::CreateView));
      break;
    }
    default:
    {
      dserror("Unsupported quantity type!");
      break;
    }
  }
  constrNorm = constrRhs_nox->norm(type);
  if (isScaled)
    constrNorm /= static_cast<double>(constrRhs_nox->length());

  return constrNorm;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double CONTACT::NoxInterface::GetLagrangeMultiplierUpdateRMS(
    const Epetra_Vector& xNew,
    const Epetra_Vector& xOld,
    const double& aTol,
    const double& rTol,
    const NOX::NLN::StatusTest::QuantityType& chQ,
    const bool& disable_implicit_weighting) const
{
  if (chQ != NOX::NLN::StatusTest::quantity_contact_normal and
      chQ != NOX::NLN::StatusTest::quantity_contact_friction)
    return -1.0;

  double rms = -1.0;
  Teuchos::RCP<Epetra_Vector> z_ptr     = Teuchos::null;
  Teuchos::RCP<Epetra_Vector> zincr_ptr = Teuchos::null;
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      // create vectors with redistributed slave dof row map in normal direction
      z_ptr = Teuchos::rcp(new Epetra_Vector(Strategy().SlNormalDoFRowMap(true)));
      zincr_ptr = Teuchos::rcp(new Epetra_Vector(Strategy().SlNormalDoFRowMap(true)));

      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      // create vectors with redistributed slave dof row map in tangential directions
      z_ptr = Teuchos::rcp(new Epetra_Vector(Strategy().SlTangentialDoFRowMap(true)));
      zincr_ptr = Teuchos::rcp(new Epetra_Vector(Strategy().SlTangentialDoFRowMap(true)));

      break;
    }
    default:
    {
      dserror("Unsupported quantity type!");
      break;
    }
  }
  LINALG::Export(*Strategy().GetLagrMultNp(true),*z_ptr);
  LINALG::Export(*Strategy().GetLagrMultSolveIncr(),*zincr_ptr);

  rms = NOX::NLN::AUX::RootMeanSquareNorm(aTol,rTol,
      Strategy().GetLagrMultNp(true),Strategy().GetLagrMultSolveIncr(),
      disable_implicit_weighting);

  return rms;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double CONTACT::NoxInterface::GetLagrangeMultiplierUpdateNorms(
    const Epetra_Vector& xNew,
    const Epetra_Vector& xOld,
    const NOX::NLN::StatusTest::QuantityType& chQ,
    const NOX::Abstract::Vector::NormType& type,
    const bool& isScaled) const
{
  if (chQ != NOX::NLN::StatusTest::quantity_contact_normal and
      chQ != NOX::NLN::StatusTest::quantity_contact_friction)
    return -1.0;

  double updatenorm = -1.0;
  Teuchos::RCP<Epetra_Vector> zincr_ptr = Teuchos::null;
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      // create vector with redistributed slave dof row map in normal direction
      zincr_ptr = Teuchos::rcp(new Epetra_Vector(Strategy().SlNormalDoFRowMap(true)));

      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      // create vector with redistributed slave dof row map in tangential directions
      zincr_ptr = Teuchos::rcp(new Epetra_Vector(Strategy().SlTangentialDoFRowMap(true)));

      break;
    }
    default:
    {
      dserror("Unsupported quantity type!");
      break;
    }
  }
  LINALG::Export(*Strategy().GetLagrMultSolveIncr(),*zincr_ptr);
  Teuchos::RCP<const NOX::Epetra::Vector> zincr_nox_ptr =
      Teuchos::rcp(new NOX::Epetra::Vector(zincr_ptr,NOX::Epetra::Vector::CreateView));

  updatenorm = zincr_nox_ptr->norm(type);
  // do scaling if desired
  if (isScaled)
    updatenorm /= static_cast<double>(zincr_nox_ptr->length());

  return updatenorm;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double CONTACT::NoxInterface::GetPreviousLagrangeMultiplierNorms(
    const Epetra_Vector& xOld,
    const NOX::NLN::StatusTest::QuantityType& chQ,
    const NOX::Abstract::Vector::NormType& type,
    const bool& isScaled) const
{
  if (chQ != NOX::NLN::StatusTest::quantity_contact_normal and
      chQ != NOX::NLN::StatusTest::quantity_contact_friction)
    return -1.0;

  double zoldnorm = -1.0;

  /* lagrange multiplier of the previous Newton step
   * (NOT equal to zOld_, which is stored in the Strategy object!!!) */
  Teuchos::RCP<Epetra_Vector> zold_ptr =
      Teuchos::rcp(new Epetra_Vector(*Strategy().GetLagrMultNp(true)));
  zold_ptr->Update(-1.0,*Strategy().GetLagrMultSolveIncr(),1.0);
  Teuchos::RCP<NOX::Epetra::Vector> zold_nox_ptr = Teuchos::null;
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      Teuchos::RCP<Epetra_Vector> znold_ptr =
          Teuchos::rcp(new Epetra_Vector(Strategy().SlNormalDoFRowMap(true)));
      LINALG::Export(*zold_ptr,*znold_ptr);

      zold_nox_ptr =
          Teuchos::rcp(new NOX::Epetra::Vector(znold_ptr,NOX::Epetra::Vector::CreateView));
      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      Teuchos::RCP<Epetra_Vector> ztold_ptr =
          Teuchos::rcp(new Epetra_Vector(Strategy().SlTangentialDoFRowMap(true)));
      LINALG::Export(*zold_ptr,*ztold_ptr);

      zold_nox_ptr =
          Teuchos::rcp(new NOX::Epetra::Vector(ztold_ptr,NOX::Epetra::Vector::CreateView));
      break;
    }
    default:
    {
      dserror("The given quantity type is unsupported!");
      break;
    }
  }

  zoldnorm = zold_nox_ptr->norm(type);
  // do scaling if desired
  if (isScaled)
    zoldnorm /= static_cast<double>(zold_nox_ptr->length());

  return zoldnorm;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
enum NOX::StatusTest::StatusType CONTACT::NoxInterface::GetActiveSetInfo(
    const NOX::NLN::StatusTest::QuantityType& chQ,
    int& activesetsize) const
{
  bool semismooth = DRT::INPUT::IntegralValue<int>(Strategy().Params(),
      "SEMI_SMOOTH_NEWTON");
  if (not semismooth)
    dserror("Currently we support only the semi-smooth Newton case!");
  // ---------------------------------------------------------------------------
  // get the number of active nodes for the given active set type
  // ---------------------------------------------------------------------------
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      activesetsize = Strategy().NumberOfActiveNodes();
      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      activesetsize = Strategy().NumberOfSlipNodes();
      break;
    }
    default:
    {
      dserror("The given quantity type is unsupported!");
      break;
    }
  }
  // ---------------------------------------------------------------------------
  // translate the active set semi-smooth Newton convergence flag
  // ---------------------------------------------------------------------------
  if (Strategy().ActiveSetSemiSmoothConverged())
    return NOX::StatusTest::Converged;
  else
    return NOX::StatusTest::Unconverged;
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> CONTACT::NoxInterface::GetCurrentActiveSetMap(
    const enum NOX::NLN::StatusTest::QuantityType& chQ) const
{
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      return Strategy().ActiveRowNodes();
      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      return Strategy().SlipRowNodes();
      break;
    }
    default:
    {
      dserror("The given active set type is unsupported!");
      break;
    }
  } // switch (active_set_type)

  return Teuchos::null;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> CONTACT::NoxInterface::GetOldActiveSetMap(
    const enum NOX::NLN::StatusTest::QuantityType& chQ) const
{
  switch (chQ)
  {
    case NOX::NLN::StatusTest::quantity_contact_normal:
    {
      return Strategy().GetOldActiveRowNodes();
      break;
    }
    case NOX::NLN::StatusTest::quantity_contact_friction:
    {
      return Strategy().GetOldSlipRowNodes();
      break;
    }
    default:
    {
      dserror("The given active set type is unsupported!");
      break;
    }
  } // switch (active_set_type)

  return Teuchos::null;
}