/*! \file

\brief Evaluation routines for the solid-poro element

This file contains the element-specific evaluation routines such as
Evaluate(...), EvaluateNeumann(...), etc.

\level 1
*/

#include "4C_mat_structporo.hpp"
#include "4C_solid_3D_ele_calc_lib.hpp"
#include "4C_solid_3D_ele_calc_lib_io.hpp"
#include "4C_solid_poro_3D_ele.hpp"

FOUR_C_NAMESPACE_OPEN


int DRT::ELEMENTS::SolidPoro::Evaluate(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, DRT::Element::LocationArray& la,
    CORE::LINALG::SerialDenseMatrix& elemat1, CORE::LINALG::SerialDenseMatrix& elemat2,
    CORE::LINALG::SerialDenseVector& elevec1, CORE::LINALG::SerialDenseVector& elevec2,
    CORE::LINALG::SerialDenseVector& elevec3)
{
  if (!material_post_setup_)
  {
    std::visit([&](auto& interface) { interface->MaterialPostSetup(*this, StructPoroMaterial()); },
        solid_calc_variant_);
    material_post_setup_ = true;
  }

  // get ptr to interface to time integration
  SetParamsInterfacePtr(params);

  const ELEMENTS::ActionType action = std::invoke(
      [&]()
      {
        if (IsParamsInterface())
          return ParamsInterface().GetActionType();
        else
          return String2ActionType(params.get<std::string>("action", "none"));
      });

  switch (action)
  {
    case DRT::ELEMENTS::struct_calc_nlnstiff:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->EvaluateNonlinearForceStiffnessMass(*this, this->StructPoroMaterial(),
                discretization, la[0].lm_, params, &elevec1, &elemat1, nullptr);
          },
          solid_calc_variant_);

      if (la.Size() > 2 and this->NumMaterial() > 1)
      {
        if (discretization.HasState(1, "porofluid"))
        {
          std::visit(
              [&](auto& interface)
              {
                interface->EvaluateNonlinearForceStiffness(*this, this->StructPoroMaterial(),
                    this->FluidPoroMultiMaterial(), this->GetEleKinematicType(), discretization, la,
                    params, &elevec1, &elemat1);
              },
              solidporo_calc_variant_);
        }
      }
      return 0;
    }
    case struct_calc_internalforce:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->EvaluateNonlinearForceStiffnessMass(*this, this->StructPoroMaterial(),
                discretization, la[0].lm_, params, &elevec1, nullptr, nullptr);
          },
          solid_calc_variant_);

      if (la.Size() > 2 and this->NumMaterial() > 1)
      {
        if (discretization.HasState(1, "porofluid"))
        {
          std::visit(
              [&](auto& interface)
              {
                interface->EvaluateNonlinearForceStiffness(*this, this->StructPoroMaterial(),
                    this->FluidPoroMultiMaterial(), this->GetEleKinematicType(), discretization, la,
                    params, &elevec1, nullptr);
              },
              solidporo_calc_variant_);
        }
      }
      return 0;
    }
    case struct_calc_nlnstiffmass:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->EvaluateNonlinearForceStiffnessMass(*this, this->StructPoroMaterial(),
                discretization, la[0].lm_, params, &elevec1, &elemat1, &elemat2);
          },
          solid_calc_variant_);

      // we skip this evaluation if the coupling is not setup yet, i.e.
      // if the secondary dofset or the secondary material was not set
      // this can happen during setup of the time integrator or restart
      // there might be a better way. For instance do not evaluate
      // before the setup of the multiphysics problem is completed.
      if (la.Size() > 2 and this->NumMaterial() > 1)
      {
        if (discretization.HasState(1, "porofluid"))
        {
          std::visit(
              [&](auto& interface)
              {
                interface->EvaluateNonlinearForceStiffness(*this, this->StructPoroMaterial(),
                    this->FluidPoroMultiMaterial(), this->GetEleKinematicType(), discretization, la,
                    params, &elevec1, &elemat1);
              },
              solidporo_calc_variant_);
        }
      }
      return 0;
    }
    case struct_calc_nlnstifflmass:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->EvaluateNonlinearForceStiffnessMass(*this, this->StructPoroMaterial(),
                discretization, la[0].lm_, params, &elevec1, &elemat1, &elemat2);
          },
          solid_calc_variant_);
      DRT::ELEMENTS::LumpMatrix(elemat2);
      return 0;
    }
    case struct_poro_calc_scatracoupling:
    {
      // no coupling -> return
      return 0;
    }
    case struct_poro_calc_fluidcoupling:
    {
      if (la.Size() > 2)
      {
        if (discretization.HasState(1, "porofluid"))
        {
          std::visit(
              [&](auto& interface)
              {
                interface->CouplingPoroelast(*this, this->StructPoroMaterial(),
                    this->FluidPoroMultiMaterial(), this->GetEleKinematicType(), discretization, la,
                    params, elemat1);
              },
              solidporo_calc_variant_);
        }
      }
      return 0;
    }
    case DRT::ELEMENTS::struct_calc_update_istep:
    {
      std::visit([&](auto& interface)
          { interface->Update(*this, SolidPoroMaterial(), discretization, la[0].lm_, params); },
          solid_calc_variant_);
      return 0;
    }
    case DRT::ELEMENTS::struct_calc_recover:
    {
      std::visit([&](auto& interface)
          { interface->Recover(*this, discretization, la[0].lm_, params); },
          solid_calc_variant_);
      return 0;
    }
    case struct_calc_stress:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->CalculateStress(*this, this->StructPoroMaterial(),
                StressIO{GetIOStressType(*this, params), GetStressData(*this, params)},
                StrainIO{GetIOStrainType(*this, params), GetStrainData(*this, params)},
                discretization, la[0].lm_, params);
          },
          solid_calc_variant_);

      if (la.Size() > 2)
      {
        if (discretization.HasState(1, "porofluid"))
        {
          std::visit([&](auto& interface)
              { interface->CouplingStress(*this, discretization, la[0].lm_, params); },
              solidporo_calc_variant_);
        }
      }
      return 0;
    }
    case struct_init_gauss_point_data_output:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->InitializeGaussPointDataOutput(
                *this, SolidPoroMaterial(), *ParamsInterface().GaussPointDataOutputManagerPtr());
          },
          solid_calc_variant_);
      return 0;
    }
    case struct_gauss_point_data_output:
    {
      std::visit(
          [&](auto& interface)
          {
            interface->EvaluateGaussPointDataOutput(
                *this, SolidPoroMaterial(), *ParamsInterface().GaussPointDataOutputManagerPtr());
          },
          solid_calc_variant_);
      return 0;
    }
    case DRT::ELEMENTS::struct_calc_predict:
    {
      // do nothing for now
      return 0;
    }
    default:
      FOUR_C_THROW("The element action %s is not yet implemented for the new solid elements",
          ActionType2String(action).c_str());
      return 0;
  }
}

int DRT::ELEMENTS::SolidPoro::EvaluateNeumann(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, CORE::Conditions::Condition& condition,
    std::vector<int>& lm, CORE::LINALG::SerialDenseVector& elevec1,
    CORE::LINALG::SerialDenseMatrix* elemat1)
{
  FOUR_C_THROW("not implemented");
  return 1;
}
FOUR_C_NAMESPACE_CLOSE