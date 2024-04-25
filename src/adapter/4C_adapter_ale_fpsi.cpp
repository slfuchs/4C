/*----------------------------------------------------------------------------*/
/*! \file
 \brief FPSI wrapper for the ALE time integration

 \level 2

 */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* header inclusions */
#include "4C_adapter_ale_fpsi.hpp"

#include "4C_ale_utils_mapextractor.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
ADAPTER::AleFpsiWrapper::AleFpsiWrapper(Teuchos::RCP<Ale> ale) : AleWrapper(ale)
{
  // create the FSI interface
  interface_ = Teuchos::rcp(new ALE::UTILS::MapExtractor);
  interface_->Setup(*Discretization(), true);  // create overlapping maps for fpsi problem

  return;
}


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void ADAPTER::AleFpsiWrapper::ApplyInterfaceDisplacements(Teuchos::RCP<const Epetra_Vector> idisp)
{
  interface_->InsertFPSICondVector(idisp, WriteAccessDispnp());

  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void ADAPTER::AleFpsiWrapper::ApplyFSIInterfaceDisplacements(
    Teuchos::RCP<const Epetra_Vector> idisp)
{
  interface_->InsertFSICondVector(idisp, WriteAccessDispnp());

  return;
}


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<const ALE::UTILS::MapExtractor> ADAPTER::AleFpsiWrapper::Interface() const
{
  return interface_;
}

FOUR_C_NAMESPACE_CLOSE
