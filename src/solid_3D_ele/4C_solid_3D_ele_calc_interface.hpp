/*! \file

\brief Interface of solid elements

\level 1
*/

#ifndef FOUR_C_SOLID_3D_ELE_CALC_INTERFACE_HPP
#define FOUR_C_SOLID_3D_ELE_CALC_INTERFACE_HPP


#include "4C_config.hpp"

#include "4C_inpar_structure.hpp"

FOUR_C_NAMESPACE_OPEN


namespace DRT::ELEMENTS
{
  struct StressIO
  {
    INPAR::STR::StressType type;
    std::vector<char>& mutable_data;
  };

  struct StrainIO
  {
    INPAR::STR::StrainType type;
    std::vector<char>& mutable_data;
  };

}  // namespace DRT::ELEMENTS
FOUR_C_NAMESPACE_CLOSE

#endif
