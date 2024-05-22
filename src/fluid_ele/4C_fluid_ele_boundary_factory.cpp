/*----------------------------------------------------------------------*/
/*! \file

\brief factory class into templated evaluators for fluid boundary integration

\level 1


*/
/*----------------------------------------------------------------------*/

#include "4C_fluid_ele_boundary_factory.hpp"

#include "4C_fluid_ele_boundary_calc_poro.hpp"
#include "4C_fluid_ele_boundary_calc_std.hpp"
#include "4C_fluid_ele_boundary_interface.hpp"
#include "4C_fluid_ele_calc.hpp"

FOUR_C_NAMESPACE_OPEN

/*--------------------------------------------------------------------------*
 |                                                 (public) rasthofer 11/13 |
 *--------------------------------------------------------------------------*/
DRT::ELEMENTS::FluidBoundaryInterface* DRT::ELEMENTS::FluidBoundaryFactory::ProvideImpl(
    CORE::FE::CellType distype, std::string problem)
{
  switch (distype)
  {
    case CORE::FE::CellType::quad4:
    {
      return DefineProblemType<CORE::FE::CellType::quad4>(problem);
    }
    case CORE::FE::CellType::quad8:
    {
      return DefineProblemType<CORE::FE::CellType::quad8>(problem);
    }
    case CORE::FE::CellType::quad9:
    {
      return DefineProblemType<CORE::FE::CellType::quad9>(problem);
    }
    case CORE::FE::CellType::tri3:
    {
      return DefineProblemType<CORE::FE::CellType::tri3>(problem);
    }
    case CORE::FE::CellType::tri6:
    {
      return DefineProblemType<CORE::FE::CellType::tri6>(problem);
    }
    case CORE::FE::CellType::line2:
    {
      return DefineProblemType<CORE::FE::CellType::line2>(problem);
    }
    case CORE::FE::CellType::line3:
    {
      return DefineProblemType<CORE::FE::CellType::line3>(problem);
    }
    case CORE::FE::CellType::nurbs2:
    {
      return DefineProblemType<CORE::FE::CellType::nurbs2>(problem);
    }
    case CORE::FE::CellType::nurbs3:
    {
      return DefineProblemType<CORE::FE::CellType::nurbs3>(problem);
    }
    case CORE::FE::CellType::nurbs4:
    {
      return DefineProblemType<CORE::FE::CellType::nurbs4>(problem);
    }
    case CORE::FE::CellType::nurbs9:
    {
      return DefineProblemType<CORE::FE::CellType::nurbs9>(problem);
    }
    default:
      FOUR_C_THROW("Element shape %s not activated. Just do it.",
          CORE::FE::CellTypeToString(distype).c_str());
      break;
  }
  return nullptr;
}

/*--------------------------------------------------------------------------*
 |                                                 (public) rasthofer 11/13 |
 *--------------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
DRT::ELEMENTS::FluidBoundaryInterface* DRT::ELEMENTS::FluidBoundaryFactory::DefineProblemType(
    std::string problem)
{
  if (problem == "std")
    return DRT::ELEMENTS::FluidEleBoundaryCalcStd<distype>::Instance();
  else if (problem == "poro")
    return DRT::ELEMENTS::FluidEleBoundaryCalcPoro<distype>::Instance();
  else if (problem == "poro_p1")
    return DRT::ELEMENTS::FluidEleBoundaryCalcPoroP1<distype>::Instance();
  else
    FOUR_C_THROW("Defined problem type does not exist!!");

  return nullptr;
}

FOUR_C_NAMESPACE_CLOSE