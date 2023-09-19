/*----------------------------------------------------------------------*/
/*! \file

\brief General prestress strategy for mixture constituents

\level 3


*/
/*----------------------------------------------------------------------*/
#include "baci_mixture_prestress_strategy.H"

#include "baci_inpar_material.H"
#include "baci_lib_globalproblem.H"
#include "baci_mat_par_bundle.H"
#include "baci_mat_par_material.H"
#include "baci_mat_service.H"
#include "baci_mixture_prestress_strategy_constant.H"
#include "baci_mixture_prestress_strategy_isocyl.H"
#include "baci_mixture_prestress_strategy_iterative.H"
#include "baci_utils_exceptions.H"

// Prestress stragegy factory generates the prestress strategy for a specific material id
MIXTURE::PAR::PrestressStrategy* MIXTURE::PAR::PrestressStrategy::Factory(int matid)
{
  // for the sake of safety
  if (DRT::Problem::Instance()->Materials() == Teuchos::null)
  {
    dserror("List of materials cannot be accessed in the global problem instance.");
  }

  // yet another safety check
  if (DRT::Problem::Instance()->Materials()->Num() == 0)
  {
    dserror("List of materials in the global problem instance is empty.");
  }

  // retrieve problem instance to read from
  const int probinst = DRT::Problem::Instance()->Materials()->GetReadFromProblem();

  // retrieve validated input line of material ID in question
  Teuchos::RCP<MAT::PAR::Material> curmat =
      DRT::Problem::Instance(probinst)->Materials()->ById(matid);

  switch (curmat->Type())
  {
    case INPAR::MAT::mix_prestress_strategy_cylinder:
    {
      return MAT::CreateMaterialParameterInstance<MIXTURE::PAR::IsotropicCylinderPrestressStrategy>(
          curmat);
    }
    case INPAR::MAT::mix_prestress_strategy_iterative:
    {
      return MAT::CreateMaterialParameterInstance<MIXTURE::PAR::IterativePrestressStrategy>(curmat);
    }
    case INPAR::MAT::mix_prestress_strategy_constant:
    {
      return MAT::CreateMaterialParameterInstance<MIXTURE::PAR::ConstantPrestressStrategy>(curmat);
    }
    default:
      dserror(
          "The referenced material with id %d is not registered as a prestress strategy!", matid);
  }

  return nullptr;
}