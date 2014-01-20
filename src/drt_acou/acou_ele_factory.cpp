/*--------------------------------------------------------------------------*/
/*!
\file fluid_ele_factory.cpp

\brief Factory of acoustic elements

<pre>
Maintainer: Svenja Schoeder
            schoeder@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089-289-15271
</pre>
*/
/*--------------------------------------------------------------------------*/

#include "acou_ele_factory.H"
#include "acou_ele_interface.H"
#include "acou_ele_calc.H"

/*--------------------------------------------------------------------------*
 |                                                  (public) schoeder 07/13 |
 *--------------------------------------------------------------------------*/
DRT::ELEMENTS::AcouEleInterface* DRT::ELEMENTS::AcouFactory::ProvideImpl(DRT::Element::DiscretizationType distype)
{
  switch(distype)
  {
    case DRT::Element::hex8:
    {
      return DefineProblemType<DRT::Element::hex8>();
    }
    case DRT::Element::hex20:
    {
      return DefineProblemType<DRT::Element::hex20>();
    }
    case DRT::Element::hex27:
    {
      return DefineProblemType<DRT::Element::hex27>();
    }
    case DRT::Element::tet4:
    {
      return DefineProblemType<DRT::Element::tet4>();
    }
    case DRT::Element::tet10:
    {
      return DefineProblemType<DRT::Element::tet10>();
    }
    case DRT::Element::wedge6:
    {
      return DefineProblemType<DRT::Element::wedge6>();
    }
    /* wedge15 cannot be used since no mesh generator exists
    case DRT::Element::wedge15:
    {
      return DefineProblemType<DRT::Element::wedge15>();
    }
    */
    case DRT::Element::pyramid5:
    {
      return DefineProblemType<DRT::Element::pyramid5>();
    }
    case DRT::Element::quad4:
    {
      return DefineProblemType<DRT::Element::quad4>();
    }
    case DRT::Element::quad8:
    {
      return DefineProblemType<DRT::Element::quad8>();
    }
    case DRT::Element::quad9:
    {
      return DefineProblemType<DRT::Element::quad9>();
    }
    case DRT::Element::tri3:
    {
      return DefineProblemType<DRT::Element::tri3>();
    }
    case DRT::Element::tri6:
    {
      return DefineProblemType<DRT::Element::tri6>();
    }
    // Nurbs support
    case DRT::Element::nurbs9:
    {
      return DefineProblemType<DRT::Element::nurbs9>();
    }
    case DRT::Element::nurbs27:
    {
      return DefineProblemType<DRT::Element::nurbs27>();
    }
    // no 1D elements
    default:
      dserror("Element shape %s not activated. Just do it.",DRT::DistypeToString(distype).c_str());
      break;
  }
  return NULL;
}

/*--------------------------------------------------------------------------*
 |                                                  (public) schoeder 07/13 |
 *--------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::AcouEleInterface* DRT::ELEMENTS::AcouFactory::DefineProblemType()
{
  return DRT::ELEMENTS::AcouEleCalc<distype>::Instance();
}

