/*-----------------------------------------------------------*/
/*! \file

\brief factory for time integrator


\level 3

*/
/*-----------------------------------------------------------*/

#include "4C_structure_new_factory.hpp"

#include "4C_global_data.hpp"
#include "4C_structure_new_dbc.hpp"
#include "4C_structure_new_expl_ab2.hpp"
#include "4C_structure_new_expl_abx.hpp"
#include "4C_structure_new_expl_centrdiff.hpp"
#include "4C_structure_new_expl_forwardeuler.hpp"
#include "4C_structure_new_impl_gemm.hpp"
#include "4C_structure_new_impl_genalpha.hpp"
#include "4C_structure_new_impl_genalpha_liegroup.hpp"
#include "4C_structure_new_impl_ost.hpp"        // derived from ost
#include "4C_structure_new_impl_prestress.hpp"  // derived from statics
#include "4C_structure_new_impl_statics.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::Factory::Factory()
{
  // empty constructor
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::Integrator> STR::Factory::BuildIntegrator(
    const STR::TIMINT::BaseDataSDyn& datasdyn) const
{
  Teuchos::RCP<STR::Integrator> int_ptr = Teuchos::null;
  int_ptr = BuildImplicitIntegrator(datasdyn);
  if (int_ptr.is_null()) int_ptr = BuildExplicitIntegrator(datasdyn);
  FOUR_C_ASSERT(
      !int_ptr.is_null(), "We could not find a suitable dynamic integrator (Dynamic Type).");

  return int_ptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::Integrator> STR::Factory::BuildImplicitIntegrator(
    const STR::TIMINT::BaseDataSDyn& datasdyn) const
{
  Teuchos::RCP<STR::IMPLICIT::Generic> impl_int_ptr = Teuchos::null;

  const enum INPAR::STR::DynamicType& dyntype = datasdyn.GetDynamicType();
  const enum INPAR::STR::PreStress& prestresstype = datasdyn.GetPreStressType();

  // check if we have a problem that needs to be prestressed
  const bool is_prestress = prestresstype != INPAR::STR::PreStress::none;
  if (is_prestress)
  {
    impl_int_ptr = Teuchos::rcp(new STR::IMPLICIT::PreStress());
    return impl_int_ptr;
  }

  switch (dyntype)
  {
    // Static analysis
    case INPAR::STR::dyna_statics:
    {
      impl_int_ptr = Teuchos::rcp(new STR::IMPLICIT::Statics());
      break;
    }

    // Generalised-alpha time integration
    case INPAR::STR::dyna_genalpha:
    {
      impl_int_ptr = Teuchos::rcp(new STR::IMPLICIT::GenAlpha());
      break;
    }

    // Generalised-alpha time integration for Lie groups (e.g. SO3 group of rotation matrices)
    case INPAR::STR::dyna_genalpha_liegroup:
    {
      impl_int_ptr = Teuchos::rcp(new STR::IMPLICIT::GenAlphaLieGroup());
      break;
    }

    // One-step-theta (OST) time integration
    case INPAR::STR::dyna_onesteptheta:
    {
      impl_int_ptr = Teuchos::rcp(new STR::IMPLICIT::OneStepTheta());
      break;
    }

    // Generalised energy-momentum method (GEMM)
    case INPAR::STR::dyna_gemm:
    {
      impl_int_ptr = Teuchos::rcp(new STR::IMPLICIT::Gemm());
      break;
    }

    // Everything else
    default:
    {
      /* Do nothing and return Techos::null. */
      break;
    }
  }  // end of switch(dynType)

  return impl_int_ptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::Integrator> STR::Factory::BuildExplicitIntegrator(
    const STR::TIMINT::BaseDataSDyn& datasdyn) const
{
  Teuchos::RCP<STR::EXPLICIT::Generic> expl_int_ptr = Teuchos::null;

  switch (datasdyn.GetDynamicType())
  {
    // Forward Euler Scheme
    case INPAR::STR::dyna_expleuler:
    {
      expl_int_ptr = Teuchos::rcp(new STR::EXPLICIT::ForwardEuler());
      break;
    }

    // Central Difference Scheme
    case INPAR::STR::dyna_centrdiff:
    {
      expl_int_ptr = Teuchos::rcp(new STR::EXPLICIT::CentrDiff());
      break;
    }

    // Adams-Bashforth-2 Scheme
    case INPAR::STR::dyna_ab2:
    {
      expl_int_ptr = Teuchos::rcp(new STR::EXPLICIT::AdamsBashforth2());
      break;
    }

    // Adams-Bashforth-4 Scheme
    case INPAR::STR::dyna_ab4:
    {
      expl_int_ptr = Teuchos::rcp(new STR::EXPLICIT::AdamsBashforthX<4>());
      break;
    }

    // Everything else
    default:
    {
      /* Do nothing and return Techos::null. */
      break;
    }
  }  // end of switch(dynType)

  return expl_int_ptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::Integrator> STR::BuildIntegrator(const STR::TIMINT::BaseDataSDyn& datasdyn)
{
  STR::Factory factory;

  return factory.BuildIntegrator(datasdyn);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::Dbc> STR::Factory::BuildDbc(const STR::TIMINT::BaseDataSDyn& datasdyn) const
{
  // if you want your model specific dbc object, check here if your model type is
  // active ( datasdyn.GetModelTypes() )and build your own dbc object
  Teuchos::RCP<STR::Dbc> dbc = Teuchos::null;
  dbc = Teuchos::rcp(new STR::Dbc());

  return dbc;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::Dbc> STR::BuildDbc(const STR::TIMINT::BaseDataSDyn& datasdyn)
{
  STR::Factory factory;

  return factory.BuildDbc(datasdyn);
}

FOUR_C_NAMESPACE_CLOSE
