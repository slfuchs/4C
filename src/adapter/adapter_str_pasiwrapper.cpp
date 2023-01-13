/*----------------------------------------------------------------------*/
/*! \file

\brief structural adapter for PASI problems

\level 3



*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | headers                                                              |
 *----------------------------------------------------------------------*/
#include "adapter_str_pasiwrapper.H"

#include "structure_aux.H"

#include "lib_discret.H"

/*----------------------------------------------------------------------*
 | definitions                                                          |
 *----------------------------------------------------------------------*/
ADAPTER::PASIStructureWrapper::PASIStructureWrapper(Teuchos::RCP<Structure> structure)
    : StructureWrapper(structure)
{
  // set-up PASI interface
  interface_ = Teuchos::rcp(new STR::AUX::MapExtractor);

  interface_->Setup(*Discretization(), *Discretization()->DofRowMap());
}

void ADAPTER::PASIStructureWrapper::ApplyInterfaceForce(Teuchos::RCP<const Epetra_Vector> intfforce)
{
  PASIModelEvaluator()->GetInterfaceForceNpPtr()->Scale(0.0);

  if (intfforce != Teuchos::null)
    interface_->AddPASICondVector(intfforce, PASIModelEvaluator()->GetInterfaceForceNpPtr());
}
