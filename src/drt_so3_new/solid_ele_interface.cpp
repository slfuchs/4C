/*----------------------------------------------------------------------*/
/*! \file

\brief Interface of solid elements

\level 1
*/
/*----------------------------------------------------------------------*/

#include "solid_ele_interface.H"
#include "../drt_structure_new/str_elements_paramsinterface.H"

void DRT::ELEMENTS::SolidEleInterface::ParamsInterfaceToList(
    STR::ELEMENTS::ParamsInterface& pi, Teuchos::ParameterList& pl)
{
  pl.set<double>("delta time", pi.GetDeltaTime());
}
