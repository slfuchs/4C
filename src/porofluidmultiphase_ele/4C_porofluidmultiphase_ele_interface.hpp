/*----------------------------------------------------------------------*/
/*! \file
 \brief generic interface for implementations of the porofluidmultiphase
        boundary element evaluation routines

   \level 3

 *----------------------------------------------------------------------*/

#ifndef FOUR_C_POROFLUIDMULTIPHASE_ELE_INTERFACE_HPP
#define FOUR_C_POROFLUIDMULTIPHASE_ELE_INTERFACE_HPP



#include "4C_config.hpp"

#include "4C_lib_element.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_linalg_serialdensevector.hpp"

#include <Teuchos_ParameterList.hpp>

#include <vector>

FOUR_C_NAMESPACE_OPEN

namespace DRT
{
  class Discretization;
  class Element;

  namespace ELEMENTS
  {
    /// Interface base class for PoroFluidMultiPhaseEleCalc
    /*!
      This class exists to provide a common interface for all template
      versions of PoroFluidMultiPhaseEleCalc.
     */
    class PoroFluidMultiPhaseEleInterface
    {
     public:
      /// Empty constructor
      PoroFluidMultiPhaseEleInterface() { return; };

      /// Empty destructor
      virtual ~PoroFluidMultiPhaseEleInterface() = default;

      /// Evaluate the element
      /*!
        This class does not provide a definition for this function; it
        must be defined in PoroFluidMultiPhaseEleCalc.
       */
      virtual int Evaluate(DRT::Element* ele, Teuchos::ParameterList& params,
          DRT::Discretization& discretization, DRT::Element::LocationArray& la,
          std::vector<CORE::LINALG::SerialDenseMatrix*>& elemat,
          std::vector<CORE::LINALG::SerialDenseVector*>& elevec) = 0;
    };
  }  // namespace ELEMENTS
}  // namespace DRT


FOUR_C_NAMESPACE_CLOSE

#endif
