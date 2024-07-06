/*----------------------------------------------------------------------*/
/*! \file

\brief Specialize structures to correctly initialize the element data containers for Hermite
elements

\level 1

*/


#ifndef FOUR_C_BEAMINTERACTION_GEOMETRY_PAIR_ACCESS_TRAITS_HPP
#define FOUR_C_BEAMINTERACTION_GEOMETRY_PAIR_ACCESS_TRAITS_HPP

#include "4C_config.hpp"

#include "4C_beam3_base.hpp"
#include "4C_geometry_pair_element.hpp"

FOUR_C_NAMESPACE_OPEN


namespace GEOMETRYPAIR
{
  template <>
  struct SetShapeFunctionData<t_hermite>
  {
    static void set(
        ShapeFunctionData<t_hermite>& shape_function_data, const Core::Elements::Element* element)
    {
      // Get the reference length of the beam element
      const auto* beam_element = dynamic_cast<const Discret::ELEMENTS::Beam3Base*>(element);
      if (beam_element == nullptr)
        FOUR_C_THROW(
            "The element pointer has to point to a valid beam element when evaluating the shape "
            "function data of a hermite beam, as we need to get RefLength()!");
      shape_function_data.ref_length_ = beam_element->ref_length();
    }
  };
}  // namespace GEOMETRYPAIR

FOUR_C_NAMESPACE_CLOSE

#endif
