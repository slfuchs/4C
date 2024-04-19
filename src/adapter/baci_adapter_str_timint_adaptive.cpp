/*----------------------------------------------------------------------*/
/*! \file

\brief Structure field adapter for time step size adaptivity

\level 2


*/

/*----------------------------------------------------------------------*/
/* headers */
#include "baci_adapter_str_timint_adaptive.hpp"

#include "baci_structure_timada.hpp"
#include "baci_structure_timint.hpp"
#include "baci_structure_timint_create.hpp"

FOUR_C_NAMESPACE_OPEN


/*======================================================================*/
/* constructor */
ADAPTER::StructureTimIntAda::StructureTimIntAda(
    Teuchos::RCP<STR::TimAda> sta, Teuchos::RCP<Structure> sti)
    : StructureWrapper(sti), sta_(sta)
{
  // make sure
  if (sta_ == Teuchos::null) FOUR_C_THROW("Failed to create structural integrator");
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
int ADAPTER::StructureTimIntAda::Integrate() { return sta_->Integrate(); }

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::StructureTimIntAda::PrepareOutput(bool force_prepare)
{
  sta_->PrepareOutputPeriod(force_prepare);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::StructureTimIntAda::Output() { sta_->OutputPeriod(); }


/*----------------------------------------------------------------------*/

FOUR_C_NAMESPACE_CLOSE
