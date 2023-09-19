/*----------------------------------------------------------------------*/
/*! \file

\brief Adapter Layer for Structures with Algebraic Constraints

\level 2


*/

/*----------------------------------------------------------------------*/
/* headers */
#include "baci_adapter_str_constr_merged.H"

#include "baci_constraint_manager.H"
#include "baci_inpar_validparameters.H"
#include "baci_lib_condition_utils.H"
#include "baci_lib_globalproblem.H"
#include "baci_linalg_utils_sparse_algebra_create.H"
#include "baci_linalg_utils_sparse_algebra_manipulation.H"
#include "baci_structure_aux.H"
#include "baci_structure_timint_create.H"

#include <Teuchos_StandardParameterEntryValidators.hpp>
#include <Teuchos_TimeMonitor.hpp>

/*======================================================================*/
/* constructor */
ADAPTER::StructureConstrMerged::StructureConstrMerged(Teuchos::RCP<Structure> stru)
    : FSIStructureWrapper(stru), issetup_(false)
{
  // do nothing
}


/*----------------------------------------------------------------------*/
/* */
void ADAPTER::StructureConstrMerged::Setup()
{
  // call setup on time integrator
  StructureWrapper::Setup();

  // make sure
  if (structure_ == Teuchos::null) dserror("Failed to create the underlying structural adapter");

  // build merged dof row map
  dofrowmap_ = CORE::LINALG::MergeMap(
      *(structure_->DofRowMap()), *(structure_->GetConstraintManager()->GetConstraintMap()), false);

  // set up interface between merged and single maps
  conmerger_ = Teuchos::rcp(new CORE::LINALG::MapExtractor);
  conmerger_->Setup(
      *dofrowmap_, structure_->DofRowMap(), structure_->GetConstraintManager()->GetConstraintMap());

  // setup fsi-Interface
  interface_ = Teuchos::rcp(new STR::AUX::MapExtractor);
  interface_->Setup(*Discretization(), *dofrowmap_);

  issetup_ = true;
}


/*----------------------------------------------------------------------*/
/* */
Teuchos::RCP<const Epetra_Vector> ADAPTER::StructureConstrMerged::InitialGuess()
{
  if (not issetup_) dserror("Call Setup() first!");

  // get initial guesses from structure and constraintmanager
  Teuchos::RCP<const Epetra_Vector> strucGuess = structure_->InitialGuess();
  Teuchos::RCP<const Epetra_Vector> lagrGuess = Teuchos::rcp(
      new Epetra_Vector(*(structure_->GetConstraintManager()->GetConstraintMap()), true));

  // merge stuff together
  Teuchos::RCP<Epetra_Vector> mergedGuess = Teuchos::rcp(new Epetra_Vector(*dofrowmap_, true));
  conmerger_->AddCondVector(strucGuess, mergedGuess);
  conmerger_->AddOtherVector(lagrGuess, mergedGuess);

  return mergedGuess;
}

/*----------------------------------------------------------------------*/
/* right-hand side alias the dynamic force residual */
Teuchos::RCP<const Epetra_Vector> ADAPTER::StructureConstrMerged::RHS()
{
  // get rhs-vectors from structure and constraintmanager
  Teuchos::RCP<const Epetra_Vector> struRHS = structure_->RHS();
  Teuchos::RCP<const Epetra_Vector> lagrRHS = structure_->GetConstraintManager()->GetError();

  // merge stuff together
  Teuchos::RCP<Epetra_Vector> mergedRHS = Teuchos::rcp(new Epetra_Vector(*dofrowmap_, true));
  conmerger_->AddCondVector(struRHS, mergedRHS);
  conmerger_->AddOtherVector(-1.0, lagrRHS, mergedRHS);

  return mergedRHS;
}


/*----------------------------------------------------------------------*/
/* get current displacements D_{n+1} */
Teuchos::RCP<const Epetra_Vector> ADAPTER::StructureConstrMerged::Dispnp() const
{
  // get current state from structure and constraintmanager
  Teuchos::RCP<const Epetra_Vector> strudis = structure_->Dispnp();
  Teuchos::RCP<const Epetra_Vector> lagrmult =
      structure_->GetConstraintManager()->GetLagrMultVector();

  // merge stuff together
  Teuchos::RCP<Epetra_Vector> mergedstat = Teuchos::rcp(new Epetra_Vector(*dofrowmap_, true));
  conmerger_->AddCondVector(strudis, mergedstat);
  conmerger_->AddOtherVector(lagrmult, mergedstat);

  return mergedstat;
}


/*----------------------------------------------------------------------*/
/* get last converged displacements D_{n} */
Teuchos::RCP<const Epetra_Vector> ADAPTER::StructureConstrMerged::Dispn() const
{
  // get last converged state from structure and constraintmanager
  Teuchos::RCP<const Epetra_Vector> strudis = structure_->Dispn();
  Teuchos::RCP<const Epetra_Vector> lagrmult =
      structure_->GetConstraintManager()->GetLagrMultVectorOld();

  // merge stuff together
  Teuchos::RCP<Epetra_Vector> mergedstat = Teuchos::rcp(new Epetra_Vector(*dofrowmap_, true));
  conmerger_->AddCondVector(strudis, mergedstat);
  conmerger_->AddOtherVector(lagrmult, mergedstat);

  return mergedstat;
}

/*----------------------------------------------------------------------*/
/* get last converged velocities V_{n} with zeroed Lagrange multiplier */
Teuchos::RCP<const Epetra_Vector> ADAPTER::StructureConstrMerged::Veln() const
{
  // get last converged state from structure and constraintmanager
  Teuchos::RCP<const Epetra_Vector> strudis = structure_->Veln();
  Teuchos::RCP<const Epetra_Vector> lagrmult = Teuchos::rcp(
      new Epetra_Vector(structure_->GetConstraintManager()->GetLagrMultVectorOld()->Map(), true));

  // merge stuff together
  Teuchos::RCP<Epetra_Vector> mergedstat = Teuchos::rcp(new Epetra_Vector(*dofrowmap_, true));
  conmerger_->AddCondVector(strudis, mergedstat);
  conmerger_->AddOtherVector(lagrmult, mergedstat);

  return mergedstat;
}

/*----------------------------------------------------------------------*/
/* get last converged accelerations A_{n} with zeroed Lagrange multiplier */
Teuchos::RCP<const Epetra_Vector> ADAPTER::StructureConstrMerged::Accn() const
{
  // get last converged state from structure and constraintmanager
  Teuchos::RCP<const Epetra_Vector> strudis = structure_->Accn();
  Teuchos::RCP<const Epetra_Vector> lagrmult = Teuchos::rcp(
      new Epetra_Vector(structure_->GetConstraintManager()->GetLagrMultVectorOld()->Map(), true));

  // merge stuff together
  Teuchos::RCP<Epetra_Vector> mergedstat = Teuchos::rcp(new Epetra_Vector(*dofrowmap_, true));
  conmerger_->AddCondVector(strudis, mergedstat);
  conmerger_->AddOtherVector(lagrmult, mergedstat);

  return mergedstat;
}

/*----------------------------------------------------------------------*/
/* non-overlapping DOF map */
Teuchos::RCP<const Epetra_Map> ADAPTER::StructureConstrMerged::DofRowMap() { return dofrowmap_; }


/*----------------------------------------------------------------------*/
/* stiffness, i.e. force residual R_{n+1} differentiated
 * by displacements D_{n+1} */
Teuchos::RCP<CORE::LINALG::SparseMatrix> ADAPTER::StructureConstrMerged::SystemMatrix()
{
  // create empty large matrix and get small ones from structure and constraints
  Teuchos::RCP<CORE::LINALG::SparseMatrix> mergedmatrix =
      Teuchos::rcp(new CORE::LINALG::SparseMatrix(*dofrowmap_, 81));
  Teuchos::RCP<CORE::LINALG::SparseMatrix> strustiff = structure_->SystemMatrix();
  strustiff->Complete();

  Teuchos::RCP<CORE::LINALG::SparseOperator> constiff =
      structure_->GetConstraintManager()->GetConstrMatrix();
  constiff->Complete();

  // Add matrices together
  mergedmatrix->Add(*strustiff, false, 1.0, 0.0);
  mergedmatrix->Add(*constiff, false, 1.0, 1.0);
  mergedmatrix->Add(*constiff, true, 1.0, 1.0);
  mergedmatrix->Complete(*dofrowmap_, *dofrowmap_);

  mergedmatrix->ApplyDirichlet(*(structure_->GetDBCMapExtractor()->CondMap()));

  return mergedmatrix;
}


/*----------------------------------------------------------------------*/
Teuchos::RCP<CORE::LINALG::BlockSparseMatrixBase>
ADAPTER::StructureConstrMerged::BlockSystemMatrix()
{
  dserror("constrained BlockSparseMatrix never to be implemented");
  return Teuchos::null;
}


/*----------------------------------------------------------------------*/
/* build linear system stiffness matrix and rhs/force residual
 *
 * Monolithic FSI accesses the linearised structure problem. */
void ADAPTER::StructureConstrMerged::Evaluate(Teuchos::RCP<const Epetra_Vector> dispstepinc)
{
  // 'initialize' structural displacement as null-pointer
  Teuchos::RCP<Epetra_Vector> dispstructstepinc = Teuchos::null;

  // Compute residual increments, update total increments and update lagrange multipliers
  if (dispstepinc != Teuchos::null)
  {
    // Extract increments for lagr multipliers and do update
    Teuchos::RCP<Epetra_Vector> lagrincr = conmerger_->ExtractOtherVector(dispstepinc);
    structure_->UpdateIterIncrConstr(lagrincr);
    dispstructstepinc = conmerger_->ExtractCondVector(dispstepinc);
  }
  // Hand down incremental displacements,
  // structure_ will compute the residual increments on its own
  structure_->Evaluate(dispstructstepinc);
}


/*----------------------------------------------------------------------*/
/* domain map */
const Epetra_Map& ADAPTER::StructureConstrMerged::DomainMap() const
{
  return *(CORE::LINALG::MergeMap(
      structure_->DomainMap(), *(structure_->GetConstraintManager()->GetConstraintMap()), false));
}

/*----------------------------------------------------------------------*/
// Apply interface forces
void ADAPTER::StructureConstrMerged::ApplyInterfaceForcesTemporaryDeprecated(
    Teuchos::RCP<Epetra_Vector> iforce)
{
  // create vector with displacement and constraint DOFs
  Teuchos::RCP<Epetra_Vector> fifc = CORE::LINALG::CreateVector(*DofRowMap(), true);

  // insert interface forces
  interface_->AddFSICondVector(iforce, fifc);

  // extract the force values from the displacement DOFs only
  Teuchos::RCP<Epetra_Vector> fifcdisp = CORE::LINALG::CreateVector(*conmerger_->CondMap(), true);
  conmerger_->ExtractCondVector(fifc, fifcdisp);

  // set interface forces within the structural time integrator
  SetForceInterface(fifcdisp);

  PreparePartitionStep();

  return;
}