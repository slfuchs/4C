/*----------------------------------------------------------------------*/
/*! \file

\brief structural adapter for PASI problems

\level 3



*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | headers                                                              |
 *----------------------------------------------------------------------*/
#include "4C_adapter_str_pasiwrapper.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_structure_aux.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 | definitions                                                          |
 *----------------------------------------------------------------------*/
Adapter::PASIStructureWrapper::PASIStructureWrapper(Teuchos::RCP<Structure> structure)
    : StructureWrapper(structure)
{
  // set-up PASI interface
  interface_ = Teuchos::rcp(new Solid::MapExtractor);

  interface_->setup(*discretization(), *discretization()->dof_row_map());
}

void Adapter::PASIStructureWrapper::apply_interface_force(
    Teuchos::RCP<const Epetra_Vector> intfforce)
{
  pasi_model_evaluator()->get_interface_force_np_ptr()->PutScalar(0.0);

  if (intfforce != Teuchos::null)
    interface_->add_pasi_cond_vector(
        intfforce, pasi_model_evaluator()->get_interface_force_np_ptr());
}

FOUR_C_NAMESPACE_CLOSE
