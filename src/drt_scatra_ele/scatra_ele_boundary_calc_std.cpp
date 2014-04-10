/*----------------------------------------------------------------------*/
/*!
\file scatra_ele_boundary_calc_std.cpp

\brief evaluation of scatra boundary terms at integration points

<pre>
Maintainer: Andreas Ehrl
            ehrl@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15252
</pre>
 */
/*----------------------------------------------------------------------*/

#include "scatra_ele_boundary_calc_std.H"
#include "scatra_ele_parameter_elch.H"
#include "scatra_ele_parameter_std.H"
#include "scatra_ele_action.H"
#include "scatra_ele.H"

#include "../drt_lib/drt_globalproblem.H" // for curves and functions
#include "../drt_fem_general/drt_utils_boundary_integration.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_fem_general/drt_utils_nurbs_shapefunctions.H"
#include "../drt_nurbs_discret/drt_nurbs_discret.H"
#include "../drt_nurbs_discret/drt_nurbs_utils.H"
#include "../drt_geometry/position_array.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<distype> * DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<distype>::Instance(const int numdofpernode, const int numscal, bool create )
{
  static ScaTraEleBoundaryCalcStd<distype> * instance;
  if ( create )
  {
    if ( instance==NULL )
    {
      instance = new ScaTraEleBoundaryCalcStd<distype>(numdofpernode,numscal);
    }
  }
  else
  {
    if ( instance!=NULL )
      delete instance;
    instance = NULL;
  }
  return instance;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<distype>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
    Instance( 0, 0, false );
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<distype>::ScaTraEleBoundaryCalcStd(const int numdofpernode, const int numscal)
  : DRT::ELEMENTS::ScaTraBoundaryImpl<distype>::ScaTraBoundaryImpl(numdofpernode,numscal)
{
  // pointer to class ScaTraEleParameter
  my::scatraparams_ = DRT::ELEMENTS::ScaTraEleParameterStd::Instance();
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<distype>::EvaluateAction(
    DRT::ELEMENTS::TransportBoundary* ele,
    Teuchos::ParameterList&           params,
    DRT::Discretization&              discretization,
    std::vector<int>&                 lm,
    Epetra_SerialDenseMatrix&         elemat1_epetra,
    Epetra_SerialDenseMatrix&         elemat2_epetra,
    Epetra_SerialDenseVector&         elevec1_epetra,
    Epetra_SerialDenseVector&         elevec2_epetra,
    Epetra_SerialDenseVector&         elevec3_epetra
)
{
  // check for the action parameter
  const SCATRA::BoundaryAction action = DRT::INPUT::get<SCATRA::BoundaryAction>(params,"action");

  DRT::ELEMENTS::ScaTraBoundaryImpl<distype>::SetupCalc(ele,params,discretization);

  DRT::ELEMENTS::ScaTraBoundaryImpl<distype>::EvaluateAction(ele,
                                                             params,
                                                             discretization,
                                                             action,
                                                             lm,
                                                             elemat1_epetra,
                                                             elemat2_epetra,
                                                             elevec1_epetra,
                                                             elevec2_epetra,
                                                             elevec3_epetra);


  return 0;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
// template classes
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::quad4>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::quad8>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::quad9>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::tri3>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::tri6>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::line2>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::line3>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::nurbs3>;
template class DRT::ELEMENTS::ScaTraEleBoundaryCalcStd<DRT::Element::nurbs9>;
