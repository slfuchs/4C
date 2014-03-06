/*----------------------------------------------------------------------*/
/*!
\file fluid_ele_calc_poro.cpp

\brief Internal implementation of poro Fluid element

<pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15251
</pre>
*/
/*----------------------------------------------------------------------*/

#include "fluid_ele.H"
#include "fluid_ele_utils.H"
#include "fluid_ele_action.H"
#include "fluid_ele_calc_poro.H"
#include "fluid_ele_parameter_poro.H"

#include "../drt_fluid/fluid_rotsym_periodicbc.H"

#include "../drt_fem_general/drt_utils_gder2.H"
#include "../drt_fem_general/drt_utils_nurbs_shapefunctions.H"

#include "../drt_geometry/position_array.H"

#include "../drt_mat/fluidporo.H"

#include "../drt_so3/so_poro_interface.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/standardtypes_cpp.H"

//#include "Sacado.hpp"
#include "../linalg/linalg_utils.H"

#include "../drt_inpar/inpar_fpsi.H"

#include "../drt_nurbs_discret/drt_nurbs_utils.H"


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::FluidEleCalcPoro<distype> * DRT::ELEMENTS::FluidEleCalcPoro<distype>::Instance( bool create )
{
  static FluidEleCalcPoro<distype> * instance;
  if ( create )
  {
    if ( instance==NULL )
    {
      instance = new FluidEleCalcPoro<distype>();
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
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
    Instance( false );
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::FluidEleCalcPoro<distype>::FluidEleCalcPoro()
  : DRT::ELEMENTS::FluidEleCalc<distype>::FluidEleCalc(),
    N_XYZ_(true),
    N_XYZ2_(true),
    N_XYZ2full_(true),
    xyze0_(true),
    histcon_(true),
    porosity_(0.0),
    grad_porosity_(true),
    gridvelint_(true),
    convel_(true),
    gridvdiv_(0.0),
    J_(0.0),
    press_(0.0),
    pressdot_(0.0),
    matreatensor_(true),
    reatensor_(true),
    reatensorlinODvel_(true),
    reatensorlinODgridvel_(true),
    reavel_(true),
    reagridvel_(true),
    reaconvel_(true),
    dtaudphi_(true),
    so_interface_(NULL)
{
  my::fldpara_ = DRT::ELEMENTS::FluidEleParameterPoro::Instance();
}

/*----------------------------------------------------------------------*
 * Action type: Evaluate
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::PreEvaluate(
                                                  Teuchos::ParameterList&      params,
                                                  DRT::ELEMENTS::Fluid*        ele,
                                                  DRT::Discretization&         discretization)
{
 //do nothing
  return;
}

/*----------------------------------------------------------------------*
 * Evaluate supporting methods of the element
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluateService(
    DRT::ELEMENTS::Fluid*     ele,
    Teuchos::ParameterList&   params,
    Teuchos::RCP<MAT::Material> & mat,
    DRT::Discretization&      discretization,
    std::vector<int>&         lm,
    Epetra_SerialDenseMatrix& elemat1,
    Epetra_SerialDenseMatrix& elemat2,
    Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseVector& elevec2,
    Epetra_SerialDenseVector& elevec3)
{
  // get the action required
  const FLD::Action act = DRT::INPUT::get<FLD::Action>(params,"action");

  switch(act)
  {
  case FLD::calc_volume:
  {
    return ComputeVolume(params,ele, discretization, lm, elevec1);
    break;
  }
  case FLD::calc_fluid_error:
  {
    return ComputeError(ele, params, mat, discretization, lm, elevec1);
    break;
  }
  default:
    dserror("unknown action for EvaluateService() in poro fluid element");
    break;
  }
  return -1;
}

/*----------------------------------------------------------------------*
 * Action type: Evaluate
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::Evaluate(DRT::ELEMENTS::Fluid*    ele,
                                                 DRT::Discretization &          discretization,
                                                 const std::vector<int> &       lm,
                                                 Teuchos::ParameterList&        params,
                                                 Teuchos::RCP<MAT::Material> &  mat,
                                                 Epetra_SerialDenseMatrix&      elemat1_epetra,
                                                 Epetra_SerialDenseMatrix&      elemat2_epetra,
                                                 Epetra_SerialDenseVector&      elevec1_epetra,
                                                 Epetra_SerialDenseVector&      elevec2_epetra,
                                                 Epetra_SerialDenseVector&      elevec3_epetra,
                                                 bool                           offdiag)
{
  Teuchos::RCP<const MAT::FluidPoro> actmat = Teuchos::rcp_static_cast<const MAT::FluidPoro>(mat);
  const_permeability_ = (actmat->PermeabilityFunction() == MAT::PAR::const_);

  if (not offdiag) //evaluate diagonal block (pure fluid block)
    return Evaluate(  ele,
                      discretization,
                      lm,
                      params,
                      mat,
                      elemat1_epetra,
                      elemat2_epetra,
                      elevec1_epetra,
                      elevec2_epetra,
                      elevec3_epetra,
                      my::intpoints_);
  else // evaluate off diagonal block (coupling block)
    return EvaluateOD(  ele,
                        discretization,
                        lm,
                        params,
                        mat,
                        elemat1_epetra,
                        elemat2_epetra,
                        elevec1_epetra,
                        elevec2_epetra,
                        elevec3_epetra,
                        my::intpoints_);
}


/*----------------------------------------------------------------------*
 * evaluation of coupling terms for porous flow (2)
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::Evaluate(
    DRT::ELEMENTS::Fluid*                       ele,
    DRT::Discretization &                       discretization,
    const std::vector<int> &                    lm,
    Teuchos::ParameterList&                     params,
    Teuchos::RCP<MAT::Material> &               mat,
    Epetra_SerialDenseMatrix&                   elemat1_epetra,
    Epetra_SerialDenseMatrix&                   elemat2_epetra,
    Epetra_SerialDenseVector&                   elevec1_epetra,
    Epetra_SerialDenseVector&                   elevec2_epetra,
    Epetra_SerialDenseVector&                   elevec3_epetra,
    const DRT::UTILS::GaussIntegration &        intpoints)
{
  // set element id
  my::eid_ = ele->Id();
  //get structure material
  GetStructMaterial();

  // rotationally symmetric periodic bc's: do setup for current element
  // (only required to be set up for routines "ExtractValuesFromGlobalVector")
  my::rotsymmpbc_->Setup(ele);

  // construct views
  LINALG::Matrix<(my::nsd_+1)*my::nen_,(my::nsd_+1)*my::nen_> elemat1(elemat1_epetra,true);
  //LINALG::Matrix<(my::nsd_+1)*my::nen_,(my::nsd_+1)*my::nen_> elemat2(elemat2_epetra,true);
  LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1> elevec1(elevec1_epetra, true);
  // elevec2 and elevec3 are currently not in use

  // ---------------------------------------------------------------------
  // call routine for calculation of body force in element nodes,
  // with pressure gradient prescribed as body force included for turbulent
  // channel flow and with scatra body force included for variable-density flow
  // (evaluation at time n+alpha_F for generalized-alpha scheme,
  //  and at time n+1 otherwise)
  // ---------------------------------------------------------------------
  LINALG::Matrix<my::nsd_,my::nen_> ebofoaf(true);
  LINALG::Matrix<my::nsd_,my::nen_> eprescpgaf(true);
  LINALG::Matrix<my::nen_,1>    escabofoaf(true);
  this->BodyForce(ele,ebofoaf,eprescpgaf,escabofoaf);

  // ---------------------------------------------------------------------
  // get all general state vectors: velocity/pressure, acceleration
  // and history
  // velocity/pressure values are at time n+alpha_F/n+alpha_M for
  // generalized-alpha scheme and at time n+1/n for all other schemes
  // acceleration values are at time n+alpha_M for
  // generalized-alpha scheme and at time n+1 for all other schemes
  // ---------------------------------------------------------------------
  // fill the local element vector/matrix with the global values
  // af_genalpha: velocity/pressure at time n+alpha_F
  // np_genalpha: velocity at time n+alpha_F, pressure at time n+1
  // ost:         velocity/pressure at time n+1
  LINALG::Matrix<my::nsd_, my::nen_> evelaf(true);
  LINALG::Matrix<my::nen_, 1> epreaf(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &evelaf,
      &epreaf, "velaf");

  // np_genalpha: additional vector for velocity at time n+1
  LINALG::Matrix<my::nsd_, my::nen_> evelnp(true);
  LINALG::Matrix<my::nen_, 1> eprenp(true);
  if (my::fldparatimint_->IsGenalphaNP())
    my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &evelnp,
        &eprenp, "velnp");

  LINALG::Matrix<my::nsd_, my::nen_> emhist(true);
  LINALG::Matrix<my::nen_, 1> echist(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &emhist,
      &echist, "hist");

  LINALG::Matrix<my::nsd_, my::nen_> eaccam(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &eaccam,
      NULL, "accam");

  LINALG::Matrix<my::nen_, 1> epren(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, NULL,
      &epren, "veln");

  LINALG::Matrix<my::nen_, 1> epressnp_timederiv(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, NULL,
      &epressnp_timederiv, "accnp");

  LINALG::Matrix<my::nen_,1> escaaf(true);
  my::ExtractValuesFromGlobalVector(discretization,lm, *my::rotsymmpbc_, NULL, &escaaf,"scaaf");

  if (not my::fldparatimint_->IsGenalpha())
    eaccam.Clear();

  // ---------------------------------------------------------------------
  // get additional state vectors for ALE case: grid displacement and vel.
  // ---------------------------------------------------------------------
  LINALG::Matrix<my::nsd_, my::nen_> edispnp(true);
  LINALG::Matrix<my::nsd_, my::nen_> egridv(true);
  LINALG::Matrix<my::nsd_, my::nen_> egridvn(true);
  LINALG::Matrix<my::nsd_, my::nen_> edispn(true);

  LINALG::Matrix<my::nen_, 1> eporositynp(true);

  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &edispnp,
      NULL, "dispnp");
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &egridv,
      NULL, "gridv");
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &edispn,
      NULL, "dispn");

  // get node coordinates and number of elements per node
  GEO::fillInitialPositionArray<distype, my::nsd_, LINALG::Matrix<my::nsd_, my::nen_> >(
      ele, my::xyze_);

  //----------------------------------------------------------------
  // Now do the nurbs specific stuff (for isogeometric elements)
  //----------------------------------------------------------------
  if(my::isNurbs_)
  {
    // access knots and weights for this element
    bool zero_size = DRT::NURBS::GetMyNurbsKnotsAndWeights(discretization,ele,my::myknots_,my::weights_);

    // if we have a zero sized element due to a interpolated point -> exit here
    if(zero_size)
    return(0);
  } // Nurbs specific stuff

  PreEvaluate(params,ele,discretization);

  // call inner evaluate (does not know about DRT element or discretization object)
  int result = Evaluate(
                  params,
                  ebofoaf,
                  elemat1,
                  elevec1,
                  evelaf,
                  epreaf,
                  evelnp,
                  eprenp,
                  epren,
                  emhist,
                  echist,
                  epressnp_timederiv,
                  eaccam,
                  edispnp,
                  edispn,
                  egridv,
                  escaaf,
                  NULL,
                  NULL,
                  NULL,
                  mat,
                  ele->IsAle(),
                  intpoints);

  return result;
}


/*----------------------------------------------------------------------*
 * evaluation of coupling terms for porous flow (2)
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluateOD(
    DRT::ELEMENTS::Fluid*                 ele,
    DRT::Discretization &                 discretization,
    const std::vector<int> &              lm,
    Teuchos::ParameterList&               params,
    Teuchos::RCP<MAT::Material> &         mat,
    Epetra_SerialDenseMatrix&             elemat1_epetra,
    Epetra_SerialDenseMatrix&             elemat2_epetra,
    Epetra_SerialDenseVector&             elevec1_epetra,
    Epetra_SerialDenseVector&             elevec2_epetra,
    Epetra_SerialDenseVector&             elevec3_epetra,
    const DRT::UTILS::GaussIntegration &  intpoints)
{
  // set element id
  my::eid_ = ele->Id();

  //get structure material
  GetStructMaterial();

  // rotationally symmetric periodic bc's: do setup for current element
  // (only required to be set up for routines "ExtractValuesFromGlobalVector")
  my::rotsymmpbc_->Setup(ele);

  // construct views
  LINALG::Matrix<(my::nsd_ + 1) * my::nen_, my::nsd_ * my::nen_> elemat1(elemat1_epetra, true);
  //  LINALG::Matrix<(my::nsd_+1)*my::nen_,(my::nsd_+1)*my::nen_> elemat2(elemat2_epetra,true);
  LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1> elevec1(elevec1_epetra, true);
  // elevec2 and elevec3 are currently not in use

  // ---------------------------------------------------------------------
  // call routine for calculation of body force in element nodes,
  // with pressure gradient prescribed as body force included for turbulent
  // channel flow and with scatra body force included for variable-density flow
  // (evaluation at time n+alpha_F for generalized-alpha scheme,
  //  and at time n+1 otherwise)
  // ---------------------------------------------------------------------
  LINALG::Matrix<my::nsd_,my::nen_> ebofoaf(true);
  LINALG::Matrix<my::nsd_,my::nen_> eprescpgaf(true);
  LINALG::Matrix<my::nen_,1>    escabofoaf(true);
  this->BodyForce(ele,ebofoaf,eprescpgaf,escabofoaf);

  // ---------------------------------------------------------------------
  // get all general state vectors: velocity/pressure, acceleration
  // and history
  // velocity/pressure values are at time n+alpha_F/n+alpha_M for
  // generalized-alpha scheme and at time n+1/n for all other schemes
  // acceleration values are at time n+alpha_M for
  // generalized-alpha scheme and at time n+1 for all other schemes
  // ---------------------------------------------------------------------
  // fill the local element vector/matrix with the global values
  // af_genalpha: velocity/pressure at time n+alpha_F
  // np_genalpha: velocity at time n+alpha_F, pressure at time n+1
  // ost:         velocity/pressure at time n+1
  LINALG::Matrix<my::nsd_, my::nen_> evelaf(true);
  LINALG::Matrix<my::nen_, 1> epreaf(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &evelaf,
      &epreaf, "velaf");

  // np_genalpha: additional vector for velocity at time n+1
  LINALG::Matrix<my::nsd_, my::nen_> evelnp(true);
  LINALG::Matrix<my::nen_, 1> eprenp(true);
  if (my::fldparatimint_->IsGenalphaNP())
    this->ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &evelnp,
        &eprenp, "velnp");

  LINALG::Matrix<my::nen_, 1> epressnp_timederiv(true);
  this->ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, NULL,
      &epressnp_timederiv, "accnp");

  LINALG::Matrix<my::nen_,1> escaaf(true);
  my::ExtractValuesFromGlobalVector(discretization,lm, *my::rotsymmpbc_, NULL, &escaaf,"scaaf");

  LINALG::Matrix<my::nsd_, my::nen_> emhist(true);
  LINALG::Matrix<my::nen_, 1> echist(true);
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &emhist,
      &echist, "hist");

  // ---------------------------------------------------------------------
  // get additional state vectors for ALE case: grid displacement and vel.
  // ---------------------------------------------------------------------
  LINALG::Matrix<my::nsd_, my::nen_> edispnp(true);
  LINALG::Matrix<my::nsd_, my::nen_> egridv(true);

  this->ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &edispnp,
      NULL, "dispnp");
  this->ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &egridv,
      NULL, "gridv");

  //ExtractValuesFromGlobalVector(discretization,lm, *rotsymmpbc_, NULL, &initporosity_, "initporosity");

  // get node coordinates and number of elements per node
  GEO::fillInitialPositionArray<distype, my::nsd_, LINALG::Matrix<my::nsd_, my::nen_> >(
      ele, my::xyze_);

  PreEvaluate(params,ele,discretization);

  // call inner evaluate (does not know about DRT element or discretization object)
  return EvaluateOD(params,
      ebofoaf,
      elemat1,
      elevec1,
      evelaf,
      epreaf,
      evelnp,
      eprenp,
      epressnp_timederiv,
      edispnp,
      egridv,
      escaaf,
      emhist,
      echist,
      NULL,
      mat,
      ele->IsAle(),
      intpoints);

}


/*----------------------------------------------------------------------*
 * evaluation of system matrix and residual for porous flow (3)
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::Evaluate(
  Teuchos::ParameterList&                                         params,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       ebofoaf,
  LINALG::Matrix<(my::nsd_+1)*my::nen_,(my::nsd_+1)*my::nen_> &   elemat1,
  LINALG::Matrix<(my::nsd_+1)*my::nen_,1> &                       elevec1,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       evelaf,
  const LINALG::Matrix<my::nen_,1>    &                           epreaf,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       evelnp,
  const LINALG::Matrix<my::nen_,1>    &                           eprenp,
  const LINALG::Matrix<my::nen_,1>    &                           epren,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       emhist,
  const LINALG::Matrix<my::nen_,1>&                               echist,
  const LINALG::Matrix<my::nen_,1>    &                           epressnp_timederiv,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       eaccam,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       edispnp,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       edispn,
  const LINALG::Matrix<my::nsd_,my::nen_> &                       egridv,
  const LINALG::Matrix<my::nen_,1>&                               escaaf,
  const LINALG::Matrix<my::nen_,1>*                               eporositynp,
  const LINALG::Matrix<my::nen_,1>*                               eporositydot,
  const LINALG::Matrix<my::nen_,1>*                               eporositydotn,
  Teuchos::RCP<MAT::Material>                                     mat,
  bool                                                            isale,
  const DRT::UTILS::GaussIntegration &                            intpoints)
{
  // flag for higher order elements
  my::is_higher_order_ele_ = IsHigherOrder<distype>::ishigherorder;
  // overrule higher_order_ele if input-parameter is set
  // this might be interesting for fast (but slightly
  // less accurate) computations
  if (my::fldpara_->IsInconsistent() == true) my::is_higher_order_ele_ = false;

  // stationary formulation does not support ALE formulation
 // if (isale and my::fldparatimint_->IsStationary())
 //   dserror("No ALE support within stationary fluid solver.");

  // ---------------------------------------------------------------------
  // call routine for calculating element matrix and right hand side
  // ---------------------------------------------------------------------
  Sysmat(params,
       ebofoaf,
       evelaf,
       evelnp,
       epreaf,
       eprenp,
       epren,
       eaccam,
       emhist,
       echist,
       epressnp_timederiv,
       edispnp,
       edispn,
       egridv,
       escaaf,
       eporositynp,
       eporositydot,
       eporositydotn,
       elemat1,
       elevec1,
       mat,
       isale,
       intpoints);

  return 0;
}




/*----------------------------------------------------------------------*
 * evaluation of coupling terms for porous flow (3)
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluateOD(
    Teuchos::ParameterList&                                           params,
    const LINALG::Matrix<my::nsd_,my::nen_> &                         ebofoaf,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_, my::nsd_ * my::nen_> &  elemat1,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1> &                    elevec1,
    const LINALG::Matrix<my::nsd_,my::nen_> &                         evelaf,
    const LINALG::Matrix<my::nen_, 1> &                               epreaf,
    const LINALG::Matrix<my::nsd_, my::nen_> &                        evelnp,
    const LINALG::Matrix<my::nen_, 1> &                               eprenp,
    const LINALG::Matrix<my::nen_, 1> &                               epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_> &                        edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_> &                        egridv,
    const LINALG::Matrix<my::nen_,1>&                                 escaaf,
    const LINALG::Matrix<my::nsd_,my::nen_>&                          emhist,
    const LINALG::Matrix<my::nen_,1>&                                 echist,
    const LINALG::Matrix<my::nen_,1>*                                 eporositynp,
    Teuchos::RCP<MAT::Material>                                       mat,
    bool                                                              isale,
    const DRT::UTILS::GaussIntegration &                              intpoints)
{
  // flag for higher order elements
  my::is_higher_order_ele_ = IsHigherOrder<distype>::ishigherorder;
  // overrule higher_order_ele if input-parameter is set
  // this might be interesting for fast (but slightly
  // less accurate) computations
  if (my::fldpara_->IsInconsistent() == true)
    my::is_higher_order_ele_ = false;

  // stationary formulation does not support ALE formulation
  //if (isale and my::fldparatimint_->IsStationary())
  //  dserror("No ALE support within stationary fluid solver.");

    // ---------------------------------------------------------------------
    // call routine for calculating element matrix and right hand side
    // ---------------------------------------------------------------------
    SysmatOD(
        params,
        ebofoaf,
        evelaf,
        evelnp,
        epreaf,
        eprenp,
        epressnp_timederiv,
        edispnp,
        egridv,
        escaaf,
        emhist,
        echist,
        eporositynp,
        elemat1,
        elevec1,
        mat,
        isale,
        intpoints);

    return 0;
}


/*----------------------------------------------------------------------*
 |  calculate element matrix and rhs for porous flow         vuong 06/11 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::Sysmat(
  Teuchos::ParameterList&                                       params,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      ebofoaf,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      evelaf,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      evelnp,
  const LINALG::Matrix<my::nen_,1>&                             epreaf,
  const LINALG::Matrix<my::nen_,1>&                             eprenp,
  const LINALG::Matrix<my::nen_,1>&                             epren,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      eaccam,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      emhist,
  const LINALG::Matrix<my::nen_,1>&                             echist,
  const LINALG::Matrix<my::nen_,1>    &                         epressnp_timederiv,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      edispnp,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      edispn,
  const LINALG::Matrix<my::nsd_,my::nen_>&                      egridv,
  const LINALG::Matrix<my::nen_,1>&                             escaaf,
  const LINALG::Matrix<my::nen_,1>*                             eporositynp,
  const LINALG::Matrix<my::nen_,1>*                             eporositydot,
  const LINALG::Matrix<my::nen_,1>*                             eporositydotn,
  LINALG::Matrix<(my::nsd_+1)*my::nen_,(my::nsd_+1)*my::nen_>&  estif,
  LINALG::Matrix<(my::nsd_+1)*my::nen_,1>&                      eforce,
  Teuchos::RCP<const MAT::Material>                             material,
  bool                                                          isale,
  const DRT::UTILS::GaussIntegration &                          intpoints
  )
{
  //------------------------------------------------------------------------
  //  preliminary definitions and evaluations
  //------------------------------------------------------------------------
  // definition of matrices
  LINALG::Matrix<my::nen_*my::nsd_,my::nen_*my::nsd_>  estif_u(true);
  LINALG::Matrix<my::nen_*my::nsd_,my::nen_>           estif_p_v(true);
  LINALG::Matrix<my::nen_, my::nen_*my::nsd_>          estif_q_u(true);
  LINALG::Matrix<my::nen_,my::nen_>                    ppmat(true);

  // definition of vectors
  LINALG::Matrix<my::nen_,1>         preforce(true);
  LINALG::Matrix<my::nsd_,my::nen_>  velforce(true);

  //material coordinates xyze0
  xyze0_= my::xyze_;

  // add displacement when fluid nodes move in the ALE case
  //if (isale)
  my::xyze_ += edispnp;

  //------------------------------------------------------------------------
  // potential evaluation of material parameters, subgrid viscosity
  // and/or stabilization parameters at element center
  //------------------------------------------------------------------------
  // evaluate shape functions and derivatives at element center
  my::EvalShapeFuncAndDerivsAtEleCenter();

  //------------------------------------------------------------------------
  //  start loop over integration points
  //------------------------------------------------------------------------
  GaussPointLoop(  params,
                     ebofoaf,
                     evelaf,
                     evelnp,
                     epreaf,
                     eprenp,
                     epressnp_timederiv,
                     edispnp,
                     egridv,
                     escaaf,
                     emhist,
                     echist,
                     eporositynp,
                     eporositydot,
                     eporositydotn,
                     estif_u,
                     estif_p_v,
                     estif_q_u,
                     ppmat,
                     preforce,
                     velforce,
                     material,
                     intpoints);

  //------------------------------------------------------------------------
  //  end loop over integration points
  //------------------------------------------------------------------------

  //------------------------------------------------------------------------
  //  add contributions to element matrix and right-hand-side vector
  //------------------------------------------------------------------------
  // add pressure part to right-hand-side vector
  for (int vi=0; vi<my::nen_; ++vi)
  {
    eforce(my::numdofpernode_*vi+my::nsd_)+=preforce(vi);
  }

  // add velocity part to right-hand-side vector
  for (int vi=0; vi<my::nen_; ++vi)
  {
    for (int idim=0; idim<my::nsd_; ++idim)
    {
      eforce(my::numdofpernode_*vi+idim)+=velforce(idim,vi);
    }
  }

  // add pressure-pressure part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int fuipp = my::numdofpernode_*ui+my::nsd_;

    for (int vi=0; vi<my::nen_; ++vi)
    {
      const int numdof_vi_p_nsd = my::numdofpernode_*vi+my::nsd_;

      estif(numdof_vi_p_nsd,fuipp)+=ppmat(vi,ui);
    }
  }

  // add velocity-velocity part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int numdof_ui = my::numdofpernode_*ui;
    const int nsd_ui = my::nsd_*ui;

    for (int jdim=0; jdim < my::nsd_;++jdim)
    {
      const int numdof_ui_jdim = numdof_ui+jdim;
      const int nsd_ui_jdim = nsd_ui+jdim;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const int numdof_vi = my::numdofpernode_*vi;
        const int nsd_vi = my::nsd_*vi;

        for (int idim=0; idim <my::nsd_; ++idim)
        {
          estif(numdof_vi+idim, numdof_ui_jdim) += estif_u(nsd_vi+idim, nsd_ui_jdim);
        }
      }
    }
  }

  // add velocity-pressure part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int numdof_ui_nsd = my::numdofpernode_*ui + my::nsd_;

    for (int vi=0; vi<my::nen_; ++vi)
    {
      const int nsd_vi = my::nsd_*vi;
      const int numdof_vi = my::numdofpernode_*vi;

      for (int idim=0; idim <my::nsd_; ++idim)
      {
        estif(numdof_vi+idim, numdof_ui_nsd) += estif_p_v(nsd_vi+idim, ui);
      }
    }
  }

  // add pressure-velocity part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int numdof_ui = my::numdofpernode_*ui;
    const int nsd_ui = my::nsd_*ui;

    for (int jdim=0; jdim < my::nsd_;++jdim)
    {
      const int numdof_ui_jdim = numdof_ui+jdim;
      const int nsd_ui_jdim = nsd_ui+jdim;

      for (int vi=0; vi<my::nen_; ++vi)
        estif(my::numdofpernode_*vi+my::nsd_, numdof_ui_jdim) += estif_q_u(vi, nsd_ui_jdim);
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 |  calculate coupling matrix flow                          vuong 06/11 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::SysmatOD(
    Teuchos::ParameterList&                                         params,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
    const LINALG::Matrix<my::nen_, 1>&                              epreaf,
    const LINALG::Matrix<my::nen_, 1>&                              eprenp,
    const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
    const LINALG::Matrix<my::nen_,1>&                               escaaf,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        emhist,
    const LINALG::Matrix<my::nen_,1>&                               echist,
    const LINALG::Matrix<my::nen_,1>*                               eporositynp,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_,my::nsd_ * my::nen_>&  ecoupl,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1>&                   eforce,
    Teuchos::RCP<const MAT::Material>                               material,
    bool                                                            isale,
    const DRT::UTILS::GaussIntegration &                            intpoints)
{
  //------------------------------------------------------------------------
  //  preliminary definitions and evaluations
  //------------------------------------------------------------------------
  // definition of matrices
  LINALG::Matrix<my::nen_ * my::nsd_, my::nen_ * my::nsd_> ecoupl_u(true); // coupling matrix for momentum equation
  LINALG::Matrix<my::nen_, my::nen_ * my::nsd_> ecoupl_p(true); // coupling matrix for continuity equation
  //LINALG::Matrix<(my::nsd_ + 1) * my::nen_, my::nen_ * my::nsd_> emesh(true); // linearisation of mesh motion

  //material coordinates xyze0
  xyze0_ = my::xyze_;

  // add displacement when fluid nodes move in the ALE case (in poroelasticity this is always the case)
  my::xyze_ += edispnp;

  //------------------------------------------------------------------------
  // potential evaluation of material parameters, subgrid viscosity
  // and/or stabilization parameters at element center
  //------------------------------------------------------------------------
  // evaluate shape functions and derivatives at element center
  my::EvalShapeFuncAndDerivsAtEleCenter();

  //------------------------------------------------------------------------
  //  start loop over integration points
  //------------------------------------------------------------------------
  GaussPointLoopOD(  params,
                     ebofoaf,
                     evelaf,
                     evelnp,
                     epreaf,
                     eprenp,
                     epressnp_timederiv,
                     edispnp,
                     egridv,
                     escaaf,
                     emhist,
                     echist,
                     eporositynp,
                     eforce,
                     ecoupl_u,
                     ecoupl_p,
                     material,
                     intpoints);
  //------------------------------------------------------------------------
  //  end loop over integration points
  //------------------------------------------------------------------------

  //------------------------------------------------------------------------
  //  add contributions to element matrix
  //------------------------------------------------------------------------

  // add fluid velocity-structure displacement part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int nsd_ui = my::nsd_*ui;

    for (int jdim=0; jdim < my::nsd_;++jdim)
    {
      const int nsd_ui_jdim = nsd_ui+jdim;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const int numdof_vi = my::numdofpernode_*vi;
        const int nsd_vi = my::nsd_*vi;

        for (int idim=0; idim <my::nsd_; ++idim)
        {
          ecoupl(numdof_vi+idim, nsd_ui_jdim) += ecoupl_u(nsd_vi+idim, nsd_ui_jdim);
        }
      }
    }
  }

  // add fluid pressure-structure displacement part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int nsd_ui = my::nsd_*ui;

    for (int jdim=0; jdim < my::nsd_;++jdim)
    {
      const int nsd_ui_jdim = nsd_ui+jdim;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl(my::numdofpernode_*vi+my::nsd_, nsd_ui_jdim) += ecoupl_p(vi, nsd_ui_jdim);
      }
    }
  }

  return;
}    //SysmatOD

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluatePressureEquation(
    Teuchos::ParameterList&                       params,
    const double&                                 timefacfacpre,
    const double&                                 rhsfac,
    const double&                                 dphi_dp,
    const double&                                 dphi_dJ,
    const double&                                 dphi_dJdp,
    const double&                                 dphi_dpp,
    const LINALG::Matrix<my::nen_,1>*             eporositydot,
    const LINALG::Matrix<my::nen_,1>*             eporositydotn,
    const LINALG::Matrix<my::nen_,1>&             echist,
    const LINALG::Matrix<my::nsd_,my::nen_>&      dgradphi_dp,
    LINALG::Matrix<my::nen_, my::nen_*my::nsd_>&  estif_q_u,
    LINALG::Matrix<my::nen_,my::nen_>&            ppmat,
    LINALG::Matrix<my::nen_,1>&                   preforce
    )
{

  // first evaluate terms without porosity time derivative
  EvaluatePressureEquationNonTransient(params,
                                    timefacfacpre,
                                    rhsfac,
                                    dphi_dp,
                                    dphi_dJ,
                                    dphi_dJdp,
                                    dphi_dpp,
                                    dgradphi_dp,
                                    estif_q_u,
                                    ppmat,
                                    preforce);

  // now the porosity time derivative (different for standard poro and other poro elements)
  if (my::fldparatimint_->IsStationary() == false)
  {
    // inertia terms on the right hand side for instationary fluids
    for (int vi=0; vi<my::nen_; ++vi)
    {//TODO : check genalpha case
      preforce(vi) -=  my::fac_ * ( press_ * dphi_dp ) * my::funct_(vi) ;
      preforce(vi) -=  rhsfac  * my::funct_(vi) * dphi_dJ * J_ * gridvdiv_;
    }

    const double rhsfac_rhscon = rhsfac*dphi_dp*my::rhscon_;
    for (int vi=0; vi<my::nen_; ++vi)
    {
      /* additional rhs term of continuity equation */
      preforce(vi) += rhsfac_rhscon*my::funct_(vi) ;
    }

    //additional left hand side term as history values are multiplied by dphi_dp^(n+1)
    for (int vi=0; vi<my::nen_; ++vi)
    {
      for (int ui=0; ui<my::nen_; ++ui)
      {
        ppmat(vi,ui)-= timefacfacpre*my::funct_(vi)*my::rhscon_*dphi_dpp*my::funct_(ui);
      } // ui
    }  // vi

    // in case of reactive porous medium : additional rhs term
    double refporositydot = so_interface_->RefPorosityTimeDeriv();
    for (int vi=0; vi<my::nen_; ++vi)
    {
      preforce(vi)-= rhsfac * refporositydot * my::funct_(vi) ;
    }

  } //if (my::fldparatimint_->IsStationary() == false)
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluatePressureEquationNonTransient(
    Teuchos::ParameterList&                       params,
    const double&                                 timefacfacpre,
    const double&                                 rhsfac,
    const double&                                 dphi_dp,
    const double&                                 dphi_dJ,
    const double&                                 dphi_dJdp,
    const double&                                 dphi_dpp,
    const LINALG::Matrix<my::nsd_,my::nen_>&      dgradphi_dp,
    LINALG::Matrix<my::nen_, my::nen_*my::nsd_>&  estif_q_u,
    LINALG::Matrix<my::nen_,my::nen_>&            ppmat,
    LINALG::Matrix<my::nen_,1>&                   preforce
    )
{
  double vel_grad_porosity = 0.0;
  for (int idim = 0; idim <my::nsd_; ++idim)
    vel_grad_porosity += grad_porosity_(idim)*my::velint_(idim);

  double    grad_porosity_gridvelint=0.0;
  for (int j =0; j< my::nsd_; j++)
    grad_porosity_gridvelint += grad_porosity_(j) * gridvelint_(j);

  if( static_cast<DRT::ELEMENTS::FluidEleParameterPoro*>(my::fldpara_)->PoroContiPartInt() == false )
  {
    for (int vi=0; vi<my::nen_; ++vi)
    {
      const double v = timefacfacpre*my::funct_(vi);
      for (int ui=0; ui<my::nen_; ++ui)
      {
        const int fui = my::nsd_*ui;

        for (int idim = 0; idim <my::nsd_; ++idim)
        {
          /* continuity term */
          /*
               /                      \
              |                        |
              | phi * nabla o Du  , q  |
              |                        |
               \                      /
          */
            /* porosity gradient term */
            /*
                 /                   \
                |                     |
                | grad(phi)* Du  , q  |
                |                     |
                 \                   /
            */
          estif_q_u(vi,fui+idim) += v * ( porosity_ * my::derxy_(idim,ui)
                      +  grad_porosity_(idim) * my::funct_(ui)
                      );
        }
      }
    }  // end for(idim)

    //auxiliary variables
    LINALG::Matrix<1,my::nen_> dgradphi_dp_gridvel ;
    LINALG::Matrix<1,my::nen_>  dgradphi_dp_velint;
    dgradphi_dp_gridvel.MultiplyTN(gridvelint_,dgradphi_dp);
    dgradphi_dp_velint.MultiplyTN(my::velint_,dgradphi_dp);

    // pressure terms on left-hand side
    /* poroelasticity term */
    /*
         /                            \
        |                   n+1        |
        | d(grad(phi))/dp* u    Dp, q  |
        |                   (i)        |
         \                            /

         /                            \
        |                  n+1        |
     +  | d(phi)/dp * div u    Dp, q  |
        |                  (i)        |
         \                            /
    */

    for (int vi=0; vi<my::nen_; ++vi)
    {
      const double v=timefacfacpre*my::funct_(vi);

      for (int ui=0; ui<my::nen_; ++ui)
      {
        ppmat(vi,ui)+= v * (    dphi_dp*my::vdiv_*my::funct_(ui)
                             +  dgradphi_dp_velint(ui)
                           )
                      ;
      } // ui
    }  // vi

    //right-hand side
    const double rhsfac_vdiv = rhsfac * my::vdiv_;
    for (int vi=0; vi<my::nen_; ++vi)
    {
      // velocity term on right-hand side
      preforce(vi) -=   rhsfac_vdiv * porosity_ * my::funct_(vi)
                      + rhsfac * vel_grad_porosity * my::funct_(vi)
                  ;
    }

    //transient porosity terms
    /*
         /                             \      /                                             \
        |                   n+1         |    |                        /   n+1  \             |
      - | d(grad(phi))/dp* vs    Dp, q  |  + | d(phi)/(dJdp) * J *div| vs       |  * Dp , q  |
        |                   (i)         |    |                        \  (i)   /             |
         \                             /      \                                             /

         /                    \     /                                \
        |                      |   |                    n+1           |
      + | d(phi)/dp *  Dp , q  | + | d^2(phi)/(dp)^2 * p   *  Dp , q  |
        |                      |   |                    (i)           |
         \                    /     \                                /

    */

    if (my::fldparatimint_->IsStationary() == false)
    {
      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = timefacfacpre*my::funct_(vi);
        const double w = my::fac_ * my::funct_(vi);
        for (int ui=0; ui<my::nen_; ++ui)
        {
            ppmat(vi,ui) += - v * dgradphi_dp_gridvel(ui)
                + v * ( dphi_dJdp * J_ * gridvdiv_ )* my::funct_(ui)
                + w * my::funct_(ui) *  dphi_dp
                + w * dphi_dpp * my::funct_(ui) * press_
                            ;
        }
      }  // end for(idim)

     //coupling term on right hand side
      for (int vi=0; vi<my::nen_; ++vi)
      {
        preforce(vi) -= rhsfac *my::funct_(vi) * (- grad_porosity_gridvelint )
                  ;
      }

    }  // end if (not stationary)
  }
  else //my::fldpara_->PoroContiPartInt() == true
  {
    for (int vi=0; vi<my::nen_; ++vi)
    {
      for (int ui=0; ui<my::nen_; ++ui)
      {
        const int fui = my::nsd_*ui;

        for (int idim = 0; idim <my::nsd_; ++idim)
        {
            /* porosity convective term */
            /*
                 /                   \
                |                     |
              - | phi * Du       , q  |
                |                     |
                 \                   /
            */
          estif_q_u(vi,fui+idim) += timefacfacpre * my::derxy_(idim,vi) *
                                    (
                                        -1.0 * porosity_ * my::funct_(ui)
                                    );
        }
      }
    }  // end for(idim)

    LINALG::Matrix<1,my::nen_> deriv_vel ;
    deriv_vel.MultiplyTN(my::velint_,my::derxy_);
    //stationary right-hand side
    for (int vi=0; vi<my::nen_; ++vi)
    {
      // velocity term on right-hand side
       preforce(vi) -= -1.0* rhsfac * porosity_ * deriv_vel(vi)
                  ;
    }

    // pressure terms on left-hand side
    /*
         /                                   \
        |                   n+1               |
        | -d(phi)/dp      * u    Dp, grad(q)  |
        |                   (i)               |
         \                                   /
    */

    for (int vi=0; vi<my::nen_; ++vi)
    {
      for (int ui=0; ui<my::nen_; ++ui)
      {
        ppmat(vi,ui)+= - timefacfacpre * dphi_dp * deriv_vel(vi) * my::funct_(ui);
      } // ui
    }  // vi

    if (my::fldparatimint_->IsStationary() == false)
    {
      //transient porosity terms
      /*
          /                                             \
         |                        /   n+1  \             |
         | d(phi)/(dJdp) * J *div| vs       |  * Dp , q  |
         |                        \  (i)   /             |
          \                                             /

           /                    \       /                                \
          |                      |   |                    n+1           |
        + | d(phi)/dp *  Dp , q  | + | d^2(phi)/(dp)^2 * p   *  Dp , q  |
          |                      |   |                    (i)           |
           \                    /       \                                /

           /                            \
          |                  n+1        |
       +  | d(phi)/dp * div vs   Dp, q  |
          |                  (i)        |
           \                            /

      */

      LINALG::Matrix<1,my::nen_> deriv_gridvel ;
      deriv_gridvel.MultiplyTN(gridvelint_,my::derxy_);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = timefacfacpre*my::funct_(vi);
        const double w = my::fac_ * my::funct_(vi);
        for (int ui=0; ui<my::nen_; ++ui)
        {
            ppmat(vi,ui) +=
                timefacfacpre * dphi_dp * deriv_gridvel(vi) * my::funct_(ui)
                + v * ( (dphi_dJdp * J_ + dphi_dp) * gridvdiv_ )* my::funct_(ui)
                + w * my::funct_(ui) *  dphi_dp
                + w * dphi_dpp * my::funct_(ui) * press_
                            ;
        }
      }  // end for(idim)

     //coupling term on right hand side
      for (int vi=0; vi<my::nen_; ++vi)
      {
        preforce(vi) -= rhsfac * porosity_ * deriv_gridvel(vi);
        preforce(vi) -= rhsfac  *my::funct_(vi) *  porosity_ * gridvdiv_;
      }

    }  // end if (not stationary)
  }//end if ContiPartInt

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::GaussPointLoop(
    Teuchos::ParameterList&                                         params,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
    const LINALG::Matrix<my::nen_, 1>&                              epreaf,
    const LINALG::Matrix<my::nen_, 1>&                              eprenp,
    const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
    const LINALG::Matrix<my::nen_,1>&                               escaaf,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        emhist,
    const LINALG::Matrix<my::nen_,1>&                               echist,
    const LINALG::Matrix<my::nen_,1>*                               eporositynp,
    const LINALG::Matrix<my::nen_,1>*                               eporositydot,
    const LINALG::Matrix<my::nen_,1>*                               eporositydotn,
    LINALG::Matrix<my::nen_*my::nsd_,my::nen_*my::nsd_>&            estif_u,
    LINALG::Matrix<my::nen_*my::nsd_,my::nen_>&                     estif_p_v,
    LINALG::Matrix<my::nen_, my::nen_*my::nsd_>&                    estif_q_u,
    LINALG::Matrix<my::nen_,my::nen_>&                              ppmat,
    LINALG::Matrix<my::nen_,1>&                                     preforce,
    LINALG::Matrix<my::nsd_,my::nen_>&                              velforce,
    Teuchos::RCP<const MAT::Material>                               material,
    const DRT::UTILS::GaussIntegration &                            intpoints
    )
{
  // definition of velocity-based momentum residual vectors
  LINALG::Matrix<my::nsd_*my::nsd_,my::nen_>  lin_resM_Du(true);
  LINALG::Matrix<my::nsd_,1>                  resM_Du(true);
  LINALG::Matrix<my::nsd_,my::nen_>  lin_resM_Dp(true);

  // set element area or volume
  const double vol = my::fac_;

  for ( DRT::UTILS::GaussIntegration::const_iterator iquad=intpoints.begin(); iquad!=intpoints.end(); ++iquad )
   {
     // evaluate shape functions and derivatives at integration point
     my::EvalShapeFuncAndDerivsAtIntPoint(iquad);

     const double det0 = SetupMaterialDerivatives();

     // determinant of deformationgradient det F = det ( d x / d X ) = det (dx/ds) * ( det(dX/ds) )^-1
     J_ = my::det_/det0;

     EvaluateVariablesAtGaussPoint(
         params,
         ebofoaf,
         evelaf,
         evelnp,
         epreaf,
         eprenp,
         epressnp_timederiv,
         edispnp,
         egridv,
         escaaf,
         emhist,
         echist,
         eporositynp,
         eporositydot,
         eporositydotn);

     //-----------------------------------auxilary variables for computing the porosity
     double dphi_dp=0.0;
     double dphi_dJ=0.0;
     double dphi_dJdp=0.0;
     double dphi_dpp=0.0;
     porosity_=0.0;

     // compute scalar at n+alpha_F or n+1
     const double scalaraf = my::funct_.Dot(escaaf);
     params.set<double>("scalar",scalaraf);

     ComputePorosity(  params,
                       press_,
                       J_,
                       *(iquad),
                       my::funct_,
                       eporositynp,
                       porosity_,
                       &dphi_dp,
                       &dphi_dJ,
                       &dphi_dJdp,
                       NULL, //dphi_dJJ not needed
                       &dphi_dpp,
                       false);

     if(porosity_ < 0.0 or porosity_ > 1.0)
        dserror("invalid porosity: %f",porosity_);

     //--linearization of porosity gradient w.r.t. pressure at gausspoint
     //d(grad(phi))/dp = dphi/(dJdp)* dJ/dx + d^2phi/(dp)^2 * dp/dx + dphi/dp* N,x
     LINALG::Matrix<my::nsd_,my::nen_>             dgradphi_dp(true);

     //porosity gradient (calculated only if needed)
     //LINALG::Matrix<my::nsd_,1>             grad_porosity(true);
     //--------------------------- dJ/dx
     LINALG::Matrix<my::nsd_,1> gradJ(true);

     // -------------------------(material) deformation gradient F = d xyze_ / d XYZE = xyze_ * N_XYZ_^T
     LINALG::Matrix<my::nsd_,my::nsd_>          defgrd(false);
     defgrd.MultiplyNT(my::xyze_,N_XYZ_);

     // inverse deformation gradient F^-1
     LINALG::Matrix<my::nsd_,my::nsd_>          defgrd_inv(false);
     defgrd_inv.Invert(defgrd);

     {
       //------------------------------------ build F^-1 as vector 9x1
       LINALG::Matrix<my::nsd_*my::nsd_,1> defgrd_inv_vec;
       for(int i=0; i<my::nsd_; i++)
         for(int j=0; j<my::nsd_; j++)
           defgrd_inv_vec(i*my::nsd_+j) = defgrd_inv(i,j);

       //------------------------------------ build F^-T as vector 9x1
       LINALG::Matrix<my::nsd_*my::nsd_,1> defgrd_IT_vec;
       for(int i=0; i<my::nsd_; i++)
         for(int j=0; j<my::nsd_; j++)
           defgrd_IT_vec(i*my::nsd_+j) = defgrd_inv(j,i);

       // dF/dx
       LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_> F_x(true);
       // dF/dX
       LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_> F_X(true);

       ComputeFDerivative( edispnp,
                           defgrd_inv,
                           F_x,
                           F_X);

       //compute gradients if needed
       ComputeGradients(
                        dphi_dp,
                        dphi_dJ,
                        defgrd_IT_vec,
                        F_x,
                        eporositynp,
                        gradJ);
     }

     ComputeLinearization(
                            dphi_dp,
                            dphi_dpp,
                            dphi_dJdp,
                            gradJ,
                            dgradphi_dp);

     //----------------------------------------------------------------------
     // potential evaluation of material parameters and/or stabilization
     // parameters at integration point
     //----------------------------------------------------------------------
     // get material parameters at integration point
     GetMaterialParamters(material);

     //get reaction tensor and linearisations of material reaction tensor
     ComputeSpatialReactionTerms(material,defgrd_inv);

     // get stabilization parameters at integration point
     ComputeStabilizationParameters(vol);

     // compute old RHS of momentum equation and subgrid scale velocity
     ComputeOldRHSAndSubgridScaleVelocity();

     // compute old RHS of continuity equation
     ComputeOldRHSConti();

     //----------------------------------------------------------------------
     // set time-integration factors for left- and right-hand side
     //----------------------------------------------------------------------
     const double timefacfac    = my::fldparatimint_->TimeFac()       * my::fac_;
     const double timefacfacpre = my::fldparatimint_->TimeFacPre()    * my::fac_;
     const double rhsfac        = my::fldparatimint_->TimeFacRhs()    * my::fac_;
     //const double rhsfacpre     = my::fldparatimint_->TimeFacRhsPre() * my::fac_;

     // set velocity-based momentum residual vectors to zero
     lin_resM_Du.Clear();
     resM_Du.Clear();
     lin_resM_Dp.Clear();

     // compute first version of velocity-based momentum residual containing
     // inertia and reaction term
     ComputeLinResMDu(timefacfac,lin_resM_Du);

     //----------------------------------------------------------------------
     // computation of standard Galerkin and stabilization contributions to
     // element matrix and right-hand-side vector
     //----------------------------------------------------------------------
     // 1) standard Galerkin inertia and reaction terms
     /* inertia (contribution to mass matrix) if not is_stationary */
     /*
             /              \
            |                |
            |    rho*Du , v  |
            |                |
             \              /
     */
     /*  reaction */
     /*
             /                \
            |                  |
            |    sigma*Du , v  |
            |                  |
             \                /
     */
     /* convection, convective ALE part  */
     /*
               /                             \
              |  /        n+1       \          |
              | |   rho*us   o nabla | Du , v  |
              |  \       (i)        /          |
               \                             /
     */

     for (int ui=0; ui<my::nen_; ++ui)
     {
       const int fui   = my::nsd_*ui;

       for (int vi=0; vi<my::nen_; ++vi)
       {
         const int fvi   = my::nsd_*vi;

         for (int idim = 0; idim <my::nsd_; ++idim)
           for (int jdim = 0; jdim <my::nsd_; ++jdim)
             estif_u(fvi+idim,fui+jdim) += my::funct_(vi)*lin_resM_Du(idim*my::nsd_+jdim,ui);
       } //vi
     } // ui

     // inertia terms on the right hand side for instationary fluids
     if (not my::fldparatimint_->IsStationary())
     {
       for (int idim = 0; idim <my::nsd_; ++idim)
       {
         if (my::fldparatimint_->IsGenalpha()) resM_Du(idim)+=rhsfac*my::densam_*my::accint_(idim);
         else                            resM_Du(idim)+=my::fac_*my::densaf_*my::velint_(idim);
       }

       //coupling part RHS
       // reacoeff * phi * v_s
       for (int vi=0; vi<my::nen_; ++vi)
       {
         for(int idim = 0; idim <my::nsd_; ++idim)
           velforce(idim,vi) -= -rhsfac* my::funct_(vi) * reagridvel_(idim) ;
       }
     }  // end if (not stationary)

     // convective ALE-part
     for (int idim = 0; idim <my::nsd_; ++idim)
     {
       resM_Du(idim)+=rhsfac*my::densaf_*my::conv_old_(idim);
     }  // end for(idim)

     // reactive part
     //double rhsfac_rea =rhsfac*my::reacoeff_;
     for (int idim = 0; idim <my::nsd_; ++idim)
     {
       resM_Du(idim) += rhsfac*reavel_(idim);
     }

     for (int vi=0; vi<my::nen_; ++vi)
     {
       for(int idim = 0; idim <my::nsd_; ++idim)
       {
         velforce(idim,vi)-=resM_Du(idim)*my::funct_(vi);
       }
     }


     /************************************************************************/
     /* Brinkman term: viscosity term */
     /*
                      /                        \
                     |       /  \         / \   |
               2 mu  |  eps | Du | , eps | v |  |
                     |       \  /         \ /   |
                      \                        /
     */

     if(my::visceff_)
     {
       LINALG::Matrix<my::nsd_,my::nsd_> viscstress(true);
       const double visceff_timefacfac = my::visceff_*timefacfac;
       const double porosity_inv=1.0/porosity_;

       for (int vi=0; vi<my::nen_; ++vi)
       {
         const int fvi   = my::nsd_*vi;
         const double temp2=visceff_timefacfac*my::funct_(vi)*porosity_inv;

         for (int jdim= 0; jdim<my::nsd_;++jdim)
         {
           const double temp=visceff_timefacfac*my::derxy_(jdim,vi);

           for (int ui=0; ui<my::nen_; ++ui)
           {
             const int fui   = my::nsd_*ui;

             for (int idim = 0; idim <my::nsd_; ++idim)
             {
               const int fvi_p_idim = fvi+idim;

               estif_u(fvi_p_idim,fui+jdim) +=   temp*my::derxy_(idim, ui)
                                               - temp2*(my::derxy_(idim, ui)*grad_porosity_(jdim));
               estif_u(fvi_p_idim,fui+idim) +=   temp*my::derxy_(jdim, ui)
                                               - temp2*(my::derxy_(jdim, ui)*grad_porosity_(jdim));
             } // end for (jdim)
           } // end for (idim)
         } // ui
       } //vi

       for (int jdim = 0; jdim < my::nsd_; ++jdim)
       {
         for (int idim = 0; idim < my::nsd_; ++idim)
         {
           viscstress(idim,jdim)=my::visceff_*(my::vderxy_(jdim,idim)+my::vderxy_(idim,jdim));
         }
       }

       LINALG::Matrix<my::nsd_,1> viscstress_gradphi(true);
       viscstress_gradphi.Multiply(viscstress,grad_porosity_);

       // computation of right-hand-side viscosity term
       for (int vi=0; vi<my::nen_; ++vi)
       {
         for (int idim = 0; idim < my::nsd_; ++idim)
         {
           for (int jdim = 0; jdim < my::nsd_; ++jdim)
           {
             /* viscosity term on right-hand side */
             velforce(idim,vi)-= rhsfac*
                                    (  viscstress(idim,jdim)*my::derxy_(jdim,vi)
                                     - porosity_inv * viscstress_gradphi(idim)*my::funct_(vi)
                                    );
           }
         }
       }

       LINALG::Matrix<my::nsd_,my::nen_> viscstress_dgradphidp(true);
       viscstress_dgradphidp.Multiply(viscstress,dgradphi_dp);
       for (int ui=0; ui<my::nen_; ++ui)
       {
         const double v = timefacfacpre*my::funct_(ui);
         for (int vi=0; vi<my::nen_; ++vi)
         {
           const int fvi = my::nsd_*vi;
           for (int idim = 0; idim <my::nsd_; ++idim)
           {
             estif_p_v(fvi + idim,ui) += v * porosity_inv * ( porosity_inv*viscstress_gradphi(idim) * dphi_dp* my::funct_(vi)
                                                             - viscstress_dgradphidp(idim,ui)
                                                            )
                                         ;
           }
         }
       }
     }

 /************************************************************************/
     // 3) standard Galerkin pressure term + poroelasticity terms
     /* pressure term */
     /*
          /                \
         |                  |
         |  Dp , nabla o v  |
         |                  |
          \                /
     */
     /* poroelasticity pressure term */
     /*
          /                           \      /                            \
         |         n+1                 |     |         n+1                 |
         |  sigma*u  * dphi/dp*Dp , v  |  -  |  sigma*vs  * dphi/dp*Dp , v |
         |         (i)                 |     |         (i)                 |
          \                           /       \                           /
     */
     ComputeLinResMDp(timefacfacpre,dphi_dp,lin_resM_Dp);

     for (int ui=0; ui<my::nen_; ++ui)
      {
        const double v = timefacfacpre*my::funct_(ui);
        for (int vi=0; vi<my::nen_; ++vi)
        {
          const int fvi = my::nsd_*vi;
          for (int idim = 0; idim <my::nsd_; ++idim)
          {
            estif_p_v(fvi + idim,ui)   +=   v * ( -1.0 * my::derxy_(idim, vi) );
          }
        }
      }

     for (int ui=0; ui<my::nen_; ++ui)
      {
        for (int vi=0; vi<my::nen_; ++vi)
        {
          const int fvi = my::nsd_*vi;
          for (int idim = 0; idim <my::nsd_; ++idim)
          {
            estif_p_v(fvi + idim,ui) +=  my::funct_(vi) * lin_resM_Dp(idim,ui)
                          ;
          }
        }
      }

      const double pressfac = press_*rhsfac;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        /* pressure term on right-hand side */
        for (int idim = 0; idim <my::nsd_; ++idim)
        {
          velforce(idim,vi)+= pressfac *  ( my::derxy_(idim, vi) );
        }
      }  //end for(idim)

 /************************************************************************/
     // 4) standard Galerkin continuity term + poroelasticity terms

      // this function is overwritten by the poro_p2 element (fluid_ele_calc_poro_p2),
      // as it evaluates a whole different pressure equation
      EvaluatePressureEquation( params,
                                timefacfacpre,
                                rhsfac,
                                dphi_dp,
                                dphi_dJ,
                                dphi_dJdp,
                                dphi_dpp,
                                eporositydot,
                                eporositydotn,
                                echist,
                                dgradphi_dp,
                                estif_q_u,
                                ppmat,
                                preforce);

 /***********************************************************************************************************/

     // 5) standard Galerkin bodyforce term on right-hand side
     this->BodyForceRhsTerm(velforce,rhsfac);

     // 6) PSPG term
     if (my::fldpara_->PSPG())
     {
       PSPG(estif_q_u,
            ppmat,
            preforce,
            lin_resM_Du,
            lin_resM_Dp,
            dphi_dp,
            0.0,
            timefacfac,
            timefacfacpre,
            rhsfac);
     }

     // 7) reactive stabilization term
     if (my::fldpara_->RStab() != INPAR::FLUID::reactive_stab_none)
     {
       ReacStab(estif_u,
                estif_p_v,
                velforce,
                lin_resM_Du,
                lin_resM_Dp,
                dphi_dp,
                timefacfac,
                timefacfacpre,
                rhsfac,
                0.0);
     }

     /************************************************************************/
     // 2) stabilization of continuity equation
     if (my::fldpara_->CStab())
     {
       dserror("continuity stabilization not implemented for poroelasticity");

       // In the case no continuity stabilization and no LOMA:
       // the factors 'conti_stab_and_vol_visc_fac' and 'conti_stab_and_vol_visc_rhs' are zero
       // therefore there is no contribution to the element stiffness matrix and
       // the viscous stress tensor is NOT altered!!
       //
       // ONLY
       // the rhs contribution of the viscous term is added!!

       double conti_stab_and_vol_visc_fac=0.0;
       double conti_stab_and_vol_visc_rhs=0.0;

       if (my::fldpara_->CStab())
       {
         conti_stab_and_vol_visc_fac+=timefacfacpre*my::tau_(2);
         conti_stab_and_vol_visc_rhs-=rhsfac*my::tau_(2)/porosity_*my::conres_old_;
       }

       /* continuity stabilisation on left hand side */
       /*
                   /                        \
                  |                          |
             tauC | nabla o Du  , nabla o v  |
                  |                          |
                   \                        /
       */
       /* viscosity term - subtraction for low-Mach-number flow */
       /*
                  /                             \             /                        \
                 |  1                      / \   |     2 mu  |                          |
          - 2 mu |  - (nabla o u) I , eps | v |  | = - ----- | nabla o Du  , nabla o v  |
                 |  3                      \ /   |       3   |                          |
                  \                             /             \                        /
       */
       for (int ui=0; ui<my::nen_; ++ui)
       {
         const int fui = my::nsd_*ui;

         for (int idim = 0; idim <my::nsd_; ++idim)
         {
           const int fui_p_idim = fui+idim;
           const double v0 = conti_stab_and_vol_visc_fac*my::derxy_(idim,ui);
           for (int vi=0; vi<my::nen_; ++vi)
           {
             const int fvi = my::nsd_*vi;

             for(int jdim=0;jdim<my::nsd_;++jdim)
             {
               estif_u(fvi+jdim,fui_p_idim) += v0*my::derxy_(jdim, vi) ;
             }
           }
         } // end for(idim)
       }

       // computation of right-hand-side viscosity term
       for (int vi=0; vi<my::nen_; ++vi)
       {
         for (int idim = 0; idim < my::nsd_; ++idim)
         {
           /* viscosity term on right-hand side */
           velforce(idim,vi)+= conti_stab_and_vol_visc_rhs*my::derxy_(idim,vi);
         }
       }

     }

   }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::GaussPointLoopOD(
    Teuchos::ParameterList&                                         params,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
    const LINALG::Matrix<my::nen_, 1>&                              epreaf,
    const LINALG::Matrix<my::nen_, 1>&                              eprenp,
    const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
    const LINALG::Matrix<my::nen_,1>&                               escaaf,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        emhist,
    const LINALG::Matrix<my::nen_,1>&                               echist,
    const LINALG::Matrix<my::nen_,1>*                               eporositynp,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1>&                   eforce,
    LINALG::Matrix<my::nen_ * my::nsd_, my::nen_ * my::nsd_>&       ecoupl_u,
    LINALG::Matrix<my::nen_, my::nen_ * my::nsd_>&                  ecoupl_p,
    Teuchos::RCP<const MAT::Material>                               material,
    const DRT::UTILS::GaussIntegration &                            intpoints
    )
{
  // definition of velocity-based momentum residual vectors
  LINALG::Matrix<my::nsd_, my::nen_ * my::nsd_>  lin_resM_Dus(true);

  // set element area or volume
  const double vol = my::fac_;

  for ( DRT::UTILS::GaussIntegration::const_iterator iquad=intpoints.begin(); iquad!=intpoints.end(); ++iquad )
  {
    //reset matrix
    lin_resM_Dus.Clear();

    // evaluate shape functions and derivatives at integration point
    my::EvalShapeFuncAndDerivsAtIntPoint(iquad);

    // evaluate shape function derivatives w.r.t. to material coordinates at integration point
    const double det0 = SetupMaterialDerivatives();

    // determinant of deformationgradient det F = det ( d x / d X ) = det (dx/ds) * ( det(dX/ds) )^-1
    J_ = my::det_/det0;

    EvaluateVariablesAtGaussPointOD(
          params,
          ebofoaf,
          evelaf,
          evelnp,
          epreaf,
          eprenp,
          epressnp_timederiv,
          edispnp,
          egridv,
          escaaf,
          emhist,
          echist,
          eporositynp);

    //************************************************auxilary variables for computing the porosity_

    double dphi_dp=0.0;
    double dphi_dJ=0.0;
    double dphi_dJdp=0.0;
    double dphi_dJJ=0.0;
    porosity_=0.0;

    // compute scalar at n+alpha_F or n+1
    const double scalaraf = my::funct_.Dot(escaaf);
    params.set<double>("scalar",scalaraf);
    ComputePorosity(  params,
                      press_,
                      J_,
                      *(iquad),
                      my::funct_,
                      eporositynp,
                      porosity_,
                      &dphi_dp,
                      &dphi_dJ,
                      &dphi_dJdp,
                      &dphi_dJJ,
                      NULL, //dphi_dpp not needed
                      false);

    double refporositydot = so_interface_->RefPorosityTimeDeriv();

    //---------------------------  dJ/dx = dJ/dF : dF/dx = JF^-T : dF/dx at gausspoint
    LINALG::Matrix<my::nsd_,1> gradJ(true);
    // spatial porosity gradient
    //LINALG::Matrix<my::nsd_,1>             grad_porosity(true);
    //--------------------- linearization of porosity w.r.t. structure displacements
    LINALG::Matrix<1,my::nsd_*my::nen_> dphi_dus(true);

    //------------------------------------------------dJ/dus = dJ/dF : dF/dus = J * F^-T . N_X = J * N_x
    LINALG::Matrix<1,my::nsd_*my::nen_> dJ_dus(true);
    //------------------ d( grad(\phi) ) / du_s = d\phi/(dJ du_s) * dJ/dx+ d\phi/dJ * dJ/(dx*du_s) + d\phi/(dp*du_s) * dp/dx
    LINALG::Matrix<my::nsd_,my::nen_*my::nsd_> dgradphi_dus(true);

    // -------------------------(material) deformation gradient F = d my::xyze_ / d XYZE = my::xyze_ * N_XYZ_^T
    LINALG::Matrix<my::nsd_,my::nsd_> defgrd(false);
    defgrd.MultiplyNT(my::xyze_,N_XYZ_);

    // inverse deformation gradient F^-1
    LINALG::Matrix<my::nsd_,my::nsd_> defgrd_inv(false);
    defgrd_inv.Invert(defgrd);

    {
      //------------------------------------ build F^-T as vector 9x1
      LINALG::Matrix<my::nsd_*my::nsd_,1> defgrd_IT_vec;
      for(int i=0; i<my::nsd_; i++)
        for(int j=0; j<my::nsd_; j++)
          defgrd_IT_vec(i*my::nsd_+j) = defgrd_inv(j,i);

      // dF/dx
      LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_> F_x(true);

      // dF/dX
      LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_> F_X(true);

      ComputeFDerivative( edispnp,
                          defgrd_inv,
                          F_x,
                          F_X);

      //compute gradients if needed
      ComputeGradients(
                       dphi_dp,
                       dphi_dJ,
                       defgrd_IT_vec,
                       F_x,
                       eporositynp,
                       gradJ);

      ComputeLinearizationOD(
                              dphi_dJ,
                              dphi_dJJ,
                              dphi_dJdp,
                              defgrd_inv,
                              defgrd_IT_vec,
                              F_x,
                              F_X,
                              gradJ,
                              dJ_dus,
                              dphi_dus,
                              dgradphi_dus);
    }

    //----------------------------------------------------------------------
    // potential evaluation of material parameters and/or stabilization
    // parameters at integration point
    //----------------------------------------------------------------------
    // get material parameters at integration point
    GetMaterialParamters(material);

    ComputeSpatialReactionTerms(material,defgrd_inv);

    //compute linearization of spatial reaction tensor w.r.t. structural displacements
    {
      Teuchos::RCP<const MAT::FluidPoro> actmat = Teuchos::rcp_static_cast<const MAT::FluidPoro>(material);
      if(actmat->VaryingPermeablity())
        dserror("varying material permeablity not yet supported!");

      const double porosity_inv = 1.0/porosity_;
      const double J_inv = 1.0/J_;

      reatensorlinODvel_.Clear();
      reatensorlinODgridvel_.Clear();
      for (int n =0; n<my::nen_; ++n)
        for (int d =0; d<my::nsd_; ++d)
        {
          const int gid = my::nsd_ * n +d;
          for (int i=0; i<my::nsd_; ++i)
          {
            reatensorlinODvel_(i, gid)     += dJ_dus(gid)*J_inv * reavel_(i);
            reatensorlinODgridvel_(i, gid) += dJ_dus(gid)*J_inv * reagridvel_(i);
            reatensorlinODvel_(i, gid)     += dphi_dus(gid)*porosity_inv * reavel_(i);
            reatensorlinODgridvel_(i, gid) += dphi_dus(gid)*porosity_inv * reagridvel_(i);

            for (int j=0; j<my::nsd_; ++j)
            {
              for (int k=0; k<my::nsd_; ++k)
                for(int l=0; l<my::nsd_; ++l)
                {
                  reatensorlinODvel_(i, gid) += J_ * porosity_ *
                                                my::velint_(j) *
                                                   ( - defgrd_inv(k,d) * my::derxy_(i,n) * matreatensor_(k,l) * defgrd_inv(l,j)
                                                     - defgrd_inv(k,i) * matreatensor_(k,l) * defgrd_inv(l,d) * my::derxy_(j,n)
                                                    );
                  reatensorlinODgridvel_(i, gid) += J_ * porosity_ *
                                                    gridvelint_(j) *
                                                       ( - defgrd_inv(k,d) * my::derxy_(i,n) * matreatensor_(k,l) * defgrd_inv(l,j)
                                                         - defgrd_inv(k,i) * matreatensor_(k,l) * defgrd_inv(l,d) * my::derxy_(j,n)
                                                        );
                }
            }
            if (!const_permeability_)//check if derivatives of reaction tensor are zero --> significant speed up
            {
              for (int j=0; j<my::nsd_; ++j)
              {
                for (int k=0; k<my::nsd_; ++k)
                  for(int l=0; l<my::nsd_; ++l)
                  {
                    reatensorlinODvel_(i, gid) += J_ * porosity_ *
                                                  my::velint_(j) *
                                                     ( defgrd_inv(k,i) * (matreatensorlinporosity_ (k,l) * dphi_dus(gid) + matreatensorlinJ_(k,l) * dJ_dus(gid)) * defgrd_inv(l,j)
                                                      );
                    reatensorlinODgridvel_(i, gid) += J_ * porosity_ *
                                                      gridvelint_(j) *
                                                         ( defgrd_inv(k,i) * (matreatensorlinporosity_ (k,l) * dphi_dus(gid) + matreatensorlinJ_(k,l) * dJ_dus(gid)) * defgrd_inv(l,j)
                                                          );
                  }
              }
            }
          }
        }
    }

    // get stabilization parameters at integration point
    ComputeStabilizationParameters(vol);

    // compute old RHS of momentum equation and subgrid scale velocity
    ComputeOldRHSAndSubgridScaleVelocity();

    // compute old RHS of continuity equation
    ComputeOldRHSConti();

    //----------------------------------------------------------------------
    // set time-integration factors for left- and right-hand side
    //----------------------------------------------------------------------

    const double timefacfac = my::fldparatimint_->TimeFac() * my::fac_;
    const double timefacfacpre = my::fldparatimint_->TimeFacPre() * my::fac_;

    //***********************************************************************************************
    // 1) coupling terms in momentum balance

    FillMatrixMomentumOD(
                          timefacfac,
                          evelaf,
                          egridv,
                          epreaf,
                          dgradphi_dus,
                          dphi_dp,
                          dphi_dJ,
                          dphi_dus,
                          refporositydot,
                          lin_resM_Dus,
                          ecoupl_u);

    //*************************************************************************************************************
    // 2) coupling terms in continuity equation

    FillMatrixContiOD(  timefacfacpre,
                        dphi_dp,
                        dphi_dJ,
                        dphi_dJJ,
                        dphi_dJdp,
                        refporositydot,
                        dgradphi_dus,
                        dphi_dus,
                        dJ_dus,
                        egridv,
                        lin_resM_Dus,
                        ecoupl_p);

  }//loop over gausspoints

  return;
}//GaussPointLoopOD

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::FillMatrixMomentumOD(
    const double&                                               timefacfac,
    const LINALG::Matrix<my::nsd_, my::nen_>&                   evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                   egridv,
    const LINALG::Matrix<my::nen_, 1>&                          epreaf,
    const LINALG::Matrix<my::nsd_,my::nen_*my::nsd_>&           dgradphi_dus,
    const double &                                              dphi_dp,
    const double &                                              dphi_dJ,
    const LINALG::Matrix<1,my::nsd_*my::nen_>&                  dphi_dus,
    const double &                                              refporositydot,
    LINALG::Matrix<my::nsd_, my::nen_ * my::nsd_>&              lin_resM_Dus,
    LINALG::Matrix<my::nen_ * my::nsd_, my::nen_ * my::nsd_>&   ecoupl_u
    )
{
  //stationary
  /*  reaction */
  /*
   /                                      \
   |                    n+1                |
   |    sigma * dphi/dus * u    * Dus , v  |
   |                        (i)            |
   \                                     /
   */
  /*  reactive ALE term */
  /*
   /                                  \
   |                  n+1             |
   |    - rho * grad u     * Dus , v  |
   |                  (i)             |
   \                                 /
   */

  const double fac_densaf=my::fac_*my::densaf_;
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int fui = my::nsd_*ui;

    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      for (int jdim = 0; jdim <my::nsd_; ++jdim)
      {
        lin_resM_Dus(idim,fui+jdim) += + timefacfac * reatensorlinODvel_(idim,fui+jdim)
                                           - fac_densaf * my::vderxy_(idim, jdim) * my::funct_(ui)
        ;
      }
    } // end for (idim)
  } // ui

  //transient terms
  /*  reaction */
  /*
    /                           \        /                                           \
   |                             |      |                            n+1              |
-  |    sigma * phi * D(v_s) , v |  -   |    sigma * d(phi)/d(us) * vs *  D(u_s) , v  |
   |                             |      |                             (i)             |
    \                           /        \                                           /
   */

  if (not my::fldparatimint_->IsStationary())
  {
    for (int ui=0; ui<my::nen_; ++ui)
    {
      const int fui = my::nsd_*ui;

      for (int idim = 0; idim <my::nsd_; ++idim)
      {
        for (int jdim =0; jdim<my::nsd_; ++jdim)
        {
          lin_resM_Dus(idim,fui+jdim) +=   my::fac_ * (-1.0) * reatensor_(idim,jdim) * my::funct_(ui)
                                             - timefacfac * reatensorlinODgridvel_(idim,fui+jdim)
          ;
        }
      } // end for (idim)
    } // ui
  }

  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int fui = my::nsd_*ui;

    for (int vi=0; vi<my::nen_; ++vi)
    {
      const int fvi = my::nsd_*vi;

      for (int idim = 0; idim <my::nsd_; ++idim)
      {
        for (int jdim = 0; jdim <my::nsd_; ++jdim)
        {
          ecoupl_u(fvi+idim,fui+jdim) += my::funct_(vi) * lin_resM_Dus(idim,fui+jdim);
        }
      } // end for (idim)
    } //vi
  } // ui

  //viscous terms (brinkman terms)
  if(my::visceff_)
  {
    LINALG::Matrix<my::nsd_,my::nsd_> viscstress(true);

    for (int jdim = 0; jdim < my::nsd_; ++jdim)
    {
      for (int idim = 0; idim < my::nsd_; ++idim)
      {
        viscstress(idim,jdim)=my::visceff_*(my::vderxy_(jdim,idim)+my::vderxy_(idim,jdim));
      }
    }

    LINALG::Matrix<my::nsd_,1> viscstress_gradphi(true);
    viscstress_gradphi.Multiply(viscstress,grad_porosity_);

    LINALG::Matrix<my::nsd_,my::nen_*my::nsd_> viscstress_dgradphidus(true);
    viscstress_dgradphidus.Multiply(viscstress,dgradphi_dus);

    const double porosity_inv = 1.0/porosity_;

    for (int ui=0; ui<my::nen_; ++ui)
    {
      const int fui = my::nsd_*ui;
      const double v = timefacfac*my::funct_(ui);
      for (int vi=0; vi<my::nen_; ++vi)
      {
        const int fvi = my::nsd_*vi;
        for (int idim = 0; idim <my::nsd_; ++idim)
        {
          for (int jdim =0; jdim<my::nsd_; ++jdim)
            ecoupl_u(fvi + idim,fui+jdim) += v * porosity_inv * (   porosity_inv*viscstress_gradphi(idim)*
                                                                    dphi_dus(fui+jdim)
                                                                  - viscstress_dgradphidus(idim,fui+jdim)
                                                                )
                                        ;
        }
      }
    }
  }

  //*************************************************************************************************************
  if(my::fldpara_->RStab() != INPAR::FLUID::reactive_stab_none)
  {
    double reac_tau;
    if (my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
      reac_tau = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::tau_(1);
    else
    {
      dserror("Is this factor correct? Check for bugs!");
      reac_tau=0.0;
      //reac_tau = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::fldpara_->AlphaF()*fac3;
    }

    if (my::is_higher_order_ele_ or my::fldpara_->IsNewton())
    {
      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = reac_tau*my::funct_(vi);

        for(int idim=0;idim<my::nsd_;++idim)
        {
          const int fvi_p_idim = my::nsd_*vi+idim;

          for(int jdim=0;jdim<my::nsd_;++jdim)
          {
            for (int ui=0; ui<my::nen_; ++ui)
            {
              const int fui_p_jdim   = my::nsd_*ui + jdim;

              ecoupl_u(fvi_p_idim,fui_p_jdim) += v*lin_resM_Dus(idim,fui_p_jdim);
            } // jdim
          } // vi
        } // ui
      } //idim
    } // end if (is_higher_order_ele_) or (newton_)
  }

  //*************************************************************************************************************
  // shape derivatives

  if (my::nsd_ == 3)
    LinMeshMotion_3D_OD(
        ecoupl_u,
        dphi_dp,
        dphi_dJ,
        refporositydot,
        my::fldparatimint_->TimeFac(),
        timefacfac);
  else if(my::nsd_ == 2)
    LinMeshMotion_2D_OD(
        ecoupl_u,
        dphi_dp,
        dphi_dJ,
        refporositydot,
        my::fldparatimint_->TimeFac(),
        timefacfac);
  else
    dserror("Linearization of the mesh motion is only available in 2D and 3D");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::FillMatrixContiOD(
    const double&                                               timefacfacpre,
    const double &                                              dphi_dp,
    const double &                                              dphi_dJ,
    const double&                                               dphi_dJJ,
    const double&                                               dphi_dJdp,
    const double &                                              refporositydot,
    const LINALG::Matrix<my::nsd_,my::nen_*my::nsd_>&           dgradphi_dus,
    const LINALG::Matrix<1,my::nsd_*my::nen_>&                  dphi_dus,
    const LINALG::Matrix<1,my::nsd_*my::nen_>&                  dJ_dus,
    const LINALG::Matrix<my::nsd_, my::nen_>&                   egridv,
    const LINALG::Matrix<my::nsd_, my::nen_ * my::nsd_>&        lin_resM_Dus,
    LINALG::Matrix<my::nen_, my::nen_ * my::nsd_>&              ecoupl_p
    )
{
  if (my::fldparatimint_->IsStationary() == false)
  {
    for (int vi=0; vi<my::nen_; ++vi)
    {
      const double w = timefacfacpre*my::funct_(vi);
      for (int ui=0; ui<my::nen_; ++ui)
      {
        const int fui = my::nsd_*ui;
        for(int idim = 0; idim <my::nsd_; ++idim)
        {
          ecoupl_p(vi,fui+idim)+=  w * dphi_dJdp * (-my::rhscon_) * dJ_dus(fui+idim)
          ;
        }
      }
    } // end for(idim)
  }

  if( static_cast<DRT::ELEMENTS::FluidEleParameterPoro*>(my::fldpara_)->PoroContiPartInt() == false )
  {
    //auxiliary variables
    LINALG::Matrix<1,my::nen_*my::nsd_> grad_porosity_us_velint;
    grad_porosity_us_velint.MultiplyTN(my::velint_,dgradphi_dus);

    // structure coupling terms on left-hand side
    /*  stationary */
    /*
      /                                 \      /                                    \
     |                 n+1              |     |                        n+1           |
     |   dphi/dus * div u    * Dus , v  |  +  |   d(grad(phi))/dus * u    * Dus , v  |
     |                  (i)             |     |                       (i)            |
      \                                /       \                                    /
     */
    for (int vi=0; vi<my::nen_; ++vi)
    {
      const double v=timefacfacpre*my::funct_(vi);

      for (int ui=0; ui<my::nen_; ++ui)
      {
        const int fui = my::nsd_*ui;
        for(int idim = 0; idim <my::nsd_; ++idim)
        {
           ecoupl_p(vi,fui+idim)+=   v * dphi_dus(fui+idim) * my::vdiv_
                                   + v * grad_porosity_us_velint(fui+idim)
          ;
        }
      } // ui
    } // vi

    //transient coupling terms
    if (my::fldparatimint_->IsStationary() == false)
    {
      LINALG::Matrix<1,my::nen_*my::nsd_> grad_porosity_us_gridvelint;
      grad_porosity_us_gridvelint.MultiplyTN(gridvelint_,dgradphi_dus);

      /*
        /                            \       /                                                      \
       |                              |     |                                    n+1                 |
       |   dphi/dJ * J * div Dus , v  |   + |   d^2(phi)/(dJ)^2 * dJ/dus  * J * div vs    * Dus , v  |
       |                              |     |                                        (i)             |
        \                            /      \                                                       /

       /                                            \        /                                        \
       |                           n+1               |      |                           n+1            |
    +  |   dphi/dJ * dJ/dus * div vs    * Dus, v     |    - |   d(grad(phi))/d(us) *  vs    * Dus , v  |
       |                           (i)               |      |                           (i)            |
       \                                            /        \                                        /

          /                       \
         |                         |
       - |    grad phi * Dus , v   |
         |                         |
          \                       /

          /                                          \
         |                             n+1            |
       + |    dphi/(dpdJ) * dJ/dus  * p    * Dus , v  |
         |                             (i)            |
         \                                           /
       */

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = my::fac_*my::funct_(vi);
        const double w = timefacfacpre*my::funct_(vi);
        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui = my::nsd_*ui;
          for(int idim = 0; idim <my::nsd_; ++idim)
          {
            ecoupl_p(vi,fui+idim)+=   v * ( dphi_dJ * J_ * my::derxy_(idim,ui) )
                                    + w * (
                                            + gridvdiv_  * ( dphi_dJJ * J_
                                                            + dphi_dJ ) * dJ_dus(fui+idim)
                                            - grad_porosity_us_gridvelint(fui+idim)
                                          )
                                    - v * grad_porosity_(idim) * my::funct_(ui)
                                    + v * dphi_dJdp * press_ * dJ_dus(fui+idim)
            ;
          }
        }
      } // end for(idim)

    } // end if (not stationary)
  }
  else //my::fldpara_->PoroContiPartInt() == true
  {
    LINALG::Matrix<1,my::nen_> deriv_vel ;
    deriv_vel.MultiplyTN(my::velint_,my::derxy_);

    // structure coupling terms on left-hand side
    /*  stationary */
    /*
      /                                    \
     |                 n+1                  |
     |   -dphi/dus *  u    * Dus , grad(v)  |
     |                  (i)                 |
      \                                    /
     */
    for (int vi=0; vi<my::nen_; ++vi)
    {
      for (int ui=0; ui<my::nen_; ++ui)
      {
        const int fui = my::nsd_*ui;
        for(int idim = 0; idim <my::nsd_; ++idim)
        {
          ecoupl_p(vi,fui+idim)+= timefacfacpre * (-1.0) * dphi_dus(fui+idim) * deriv_vel(vi)
          ;
        }
      } // ui
    } // vi

    //transient coupling terms
    if (my::fldparatimint_->IsStationary() == false)
    {

      /*
        /                                    \       /                                                      \
       |                                      |     |                                    n+1                 |
       |   (dphi/dJ * J + phi )* div Dus , v  |   + |   d^2(phi)/(dJ)^2 * dJ/dus  * J * div vs    * Dus , v  |
       |                                      |     |                                        (i)             |
        \                                    /      \                                                       /

       /                                            \
       |                           n+1               |
    +  |   dphi/dJ * dJ/dus * div vs    * Dus, v     |
       |                           (i)               |
       \                                            /

       /                                   \    /                        \
       |                    n+1            |    |                        |
    +  |   dphi/dus * div vs  * Dus, v     |  + |   phi *  Dus, grad(v)  |
       |                    (i)            |    |                        |
       \                                  /     \                       /

          /                                          \
         |                             n+1            |
       + |    dphi/(dpdJ) * dJ/dus  * p    * Dus , v  |
         |                             (i)            |
         \                                           /
       */

      LINALG::Matrix<1,my::nen_> deriv_gridvel ;
      deriv_gridvel.MultiplyTN(gridvelint_,my::derxy_);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double v = my::fac_*my::funct_(vi);
        const double w = timefacfacpre*my::funct_(vi);
        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui = my::nsd_*ui;
          for(int idim = 0; idim <my::nsd_; ++idim)
          {
            ecoupl_p(vi,fui+idim)+=   v * ( (dphi_dJ * J_ + porosity_)* my::derxy_(idim,ui) )
                                    + my::fac_ * my::derxy_(idim,vi) * ( porosity_ * my::funct_(ui) )
                                    + w * ( + gridvdiv_ * (
                                                            ( dphi_dJJ * J_ + dphi_dJ ) * dJ_dus(fui+idim)
                                                            + dphi_dus(fui+idim)
                                                          )
                                          )
                                    + timefacfacpre * deriv_gridvel(vi) * dphi_dus(fui+idim)
                                    + v * dphi_dJdp * press_ * dJ_dus(fui+idim)
            ;
          }
        }
      } // end for(idim)

    } // end if (not stationary)
  }// end if (partial integration)

  //*************************************************************************************************************
  // PSPG
  if(my::fldpara_->PSPG())
  {

    {
      const double v1 = timefacfacpre / porosity_ ;
      LINALG::Matrix<1, my::nen_ >   temp(true);
      temp.MultiplyTN(my::sgvelint_,my::derxy_);

      for(int jdim=0;jdim<my::nsd_;++jdim)
      {
        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui_p_jdim   = my::nsd_*ui + jdim;
          for (int vi=0; vi<my::nen_; ++vi)
          {
            ecoupl_p(vi,fui_p_jdim) += v1 * temp(vi) * dphi_dus(fui_p_jdim);
          } // vi
        } // ui
      } //jdim
    }

    double scal_grad_q=0.0;

    if(my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
    {
      scal_grad_q=my::tau_(1);
    }
    else
    {
      scal_grad_q= 0.0; //my::fldpara_->AlphaF()*fac3;
    }

    if (my::is_higher_order_ele_ || my::fldpara_->IsNewton())
    {
      LINALG::Matrix<my::nen_ , my::nen_ * my::nsd_>   temp(true);

      for (int vi=0; vi<my::nen_; ++vi)
        for (int ui=0; ui<my::nen_; ++ui)
          for(int idim=0;idim<my::nsd_;++idim)
            for(int jdim=0;jdim<my::nsd_;++jdim)
              temp(vi,ui*my::nsd_+jdim) += my::derxy_(idim,vi)*lin_resM_Dus(idim,ui*my::nsd_+jdim);

      for(int jdim=0;jdim<my::nsd_;++jdim)
      {
        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui_p_jdim   = my::nsd_*ui + jdim;
          for (int vi=0; vi<my::nen_; ++vi)
          {
            ecoupl_p(vi,fui_p_jdim) +=  scal_grad_q * temp(vi,fui_p_jdim);
          } // vi
        } // ui
      } //jdim
    } // end if (is_higher_order_ele_) or (newton_)
  }

  //*************************************************************************************************************
  // shape derivatives


  if (my::nsd_ == 3)
    LinMeshMotion_3D_Pres_OD (
        ecoupl_p,
        dphi_dp,
        dphi_dJ,
        refporositydot,
        timefacfacpre);
  else if(my::nsd_ == 2)
    LinMeshMotion_2D_Pres_OD (
        ecoupl_p,
        dphi_dp,
        dphi_dJ,
        refporositydot,
        timefacfacpre);
  else
    dserror("Linearization of the mesh motion is only available in 2D and 3D");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::LinMeshMotion_3D_OD(
    LINALG::Matrix<my::nsd_ * my::nen_, my::nsd_ * my::nen_>&         ecoupl_u,
    const double &                                                    dphi_dp,
    const double &                                                    dphi_dJ,
    const double &                                                    refporositydot,
    const double &                                                    timefac,
    const double &                                                    timefacfac)
{

  double addstab = 0.0;
  if(my::fldpara_->RStab() != INPAR::FLUID::reactive_stab_none)
  {
    if (my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
      addstab = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::tau_(1);
    else
    {
      dserror("Is this factor correct? Check for bugs!");
      //addstab = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::fldpara_->AlphaF()*fac3;
    }
  }
  //*************************** linearisation of mesh motion in momentum balance**********************************
  // mass

  if (my::fldparatimint_->IsStationary() == false)
  {
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = my::fac_ * my::densam_ * my::funct_(vi, 0) * (1.0 + addstab );
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_u(vi * 3    , ui * 3    ) += v * my::velint_(0) * my::derxy_(0, ui);
        ecoupl_u(vi * 3    , ui * 3 + 1) += v * my::velint_(0) * my::derxy_(1, ui);
        ecoupl_u(vi * 3    , ui * 3 + 2) += v * my::velint_(0) * my::derxy_(2, ui);

        ecoupl_u(vi * 3 + 1, ui * 3    ) += v * my::velint_(1) * my::derxy_(0, ui);
        ecoupl_u(vi * 3 + 1, ui * 3 + 1) += v * my::velint_(1) * my::derxy_(1, ui);
        ecoupl_u(vi * 3 + 1, ui * 3 + 2) += v * my::velint_(1) * my::derxy_(2, ui);

        ecoupl_u(vi * 3 + 2, ui * 3    ) += v * my::velint_(2) * my::derxy_(0, ui);
        ecoupl_u(vi * 3 + 2, ui * 3 + 1) += v * my::velint_(2) * my::derxy_(1, ui);
        ecoupl_u(vi * 3 + 2, ui * 3 + 2) += v * my::velint_(2) * my::derxy_(2, ui);
      }
    }
  }

  // rhs
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    double v = my::fac_ * my::funct_(vi, 0);
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_u(vi * 3    , ui * 3    ) += v * ( - my::rhsmom_(0) * my::fldparatimint_->Dt()
                                                     * my::fldparatimint_->Theta()) * my::derxy_(0, ui);
      ecoupl_u(vi * 3    , ui * 3 + 1) += v * ( - my::rhsmom_(0)
                                                     * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(1, ui);
      ecoupl_u(vi * 3    , ui * 3 + 2) += v * ( - my::rhsmom_(0)
                                                     * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(2, ui);

      ecoupl_u(vi * 3 + 1, ui * 3    ) += v * ( - my::rhsmom_(1)
                                           * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(0, ui);
      ecoupl_u(vi * 3 + 1, ui * 3 + 1) += v * ( - my::rhsmom_(1)
                                           * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(1, ui);
      ecoupl_u(vi * 3 + 1, ui * 3 + 2) += v * ( - my::rhsmom_(1)
                                           * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(2, ui);

      ecoupl_u(vi * 3 + 2, ui * 3    ) += v * ( - my::rhsmom_(2)
                                            * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(0, ui);
      ecoupl_u(vi * 3 + 2, ui * 3 + 1) += v * ( - my::rhsmom_(2)
                                            * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(1, ui);
      ecoupl_u(vi * 3 + 2, ui * 3 + 2) += v * ( - my::rhsmom_(2)
                                            * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(2, ui);
    }
  }

  //---------reaction term (darcy term)
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    //double v = timefacfac * my::funct_(vi, 0) * my::reacoeff_ * (1.0 + addstab );
    double v = timefacfac * my::funct_(vi, 0) * (1.0 + addstab );
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_u(vi * 3    , ui * 3    ) += v * reaconvel_(0) * my::derxy_(0, ui);
      ecoupl_u(vi * 3    , ui * 3 + 1) += v * reaconvel_(0) * my::derxy_(1, ui);
      ecoupl_u(vi * 3    , ui * 3 + 2) += v * reaconvel_(0) * my::derxy_(2, ui);

      ecoupl_u(vi * 3 + 1, ui * 3    ) += v * reaconvel_(1) * my::derxy_(0,ui);
      ecoupl_u(vi * 3 + 1, ui * 3 + 1) += v * reaconvel_(1) * my::derxy_(1, ui);
      ecoupl_u(vi * 3 + 1, ui * 3 + 2) += v * reaconvel_(1) * my::derxy_(2, ui);

      ecoupl_u(vi * 3 + 2, ui * 3    ) += v * reaconvel_(2) * my::derxy_(0,ui);
      ecoupl_u(vi * 3 + 2, ui * 3 + 1) += v * reaconvel_(2) * my::derxy_(1, ui);
      ecoupl_u(vi * 3 + 2, ui * 3 + 2) += v * reaconvel_(2) * my::derxy_(2, ui);
    }
  }

  //---------------convective term


#define derxjm_(r,c,d,i) derxjm_ ## r ## c ## d (i)

#define derxjm_001(ui) (my::deriv_(2, ui)*my::xjm_(1, 2) - my::deriv_(1, ui)*my::xjm_(2, 2))
#define derxjm_002(ui) (my::deriv_(1, ui)*my::xjm_(2, 1) - my::deriv_(2, ui)*my::xjm_(1, 1))

#define derxjm_100(ui) (my::deriv_(1, ui)*my::xjm_(2, 2) - my::deriv_(2, ui)*my::xjm_(1, 2))
#define derxjm_102(ui) (my::deriv_(2, ui)*my::xjm_(1, 0) - my::deriv_(1, ui)*my::xjm_(2, 0))

#define derxjm_200(ui) (my::deriv_(2, ui)*my::xjm_(1, 1) - my::deriv_(1, ui)*my::xjm_(2, 1))
#define derxjm_201(ui) (my::deriv_(1, ui)*my::xjm_(2, 0) - my::deriv_(2, ui)*my::xjm_(1, 0))

#define derxjm_011(ui) (my::deriv_(0, ui)*my::xjm_(2, 2) - my::deriv_(2, ui)*my::xjm_(0, 2))
#define derxjm_012(ui) (my::deriv_(2, ui)*my::xjm_(0, 1) - my::deriv_(0, ui)*my::xjm_(2, 1))

#define derxjm_110(ui) (my::deriv_(2, ui)*my::xjm_(0, 2) - my::deriv_(0, ui)*my::xjm_(2, 2))
#define derxjm_112(ui) (my::deriv_(0, ui)*my::xjm_(2, 0) - my::deriv_(2, ui)*my::xjm_(0, 0))

#define derxjm_210(ui) (my::deriv_(0, ui)*my::xjm_(2, 1) - my::deriv_(2, ui)*my::xjm_(0, 1))
#define derxjm_211(ui) (my::deriv_(2, ui)*my::xjm_(0, 0) - my::deriv_(0, ui)*my::xjm_(2, 0))

#define derxjm_021(ui) (my::deriv_(1, ui)*my::xjm_(0, 2) - my::deriv_(0, ui)*my::xjm_(1, 2))
#define derxjm_022(ui) (my::deriv_(0, ui)*my::xjm_(1, 1) - my::deriv_(1, ui)*my::xjm_(0, 1))

#define derxjm_120(ui) (my::deriv_(0, ui)*my::xjm_(1, 2) - my::deriv_(1, ui)*my::xjm_(0, 2))
#define derxjm_122(ui) (my::deriv_(1, ui)*my::xjm_(0, 0) - my::deriv_(0, ui)*my::xjm_(1, 0))

#define derxjm_220(ui) (my::deriv_(1, ui)*my::xjm_(0, 1) - my::deriv_(0, ui)*my::xjm_(1, 1))
#define derxjm_221(ui) (my::deriv_(0, ui)*my::xjm_(1, 0) - my::deriv_(1, ui)*my::xjm_(0, 0))

  const double timefacfac_det=timefacfac / my::det_;

  for (int ui = 0; ui < my::nen_; ++ui)
  {
    double v00 = +my::convvelint_(1)* (my::vderiv_(0, 0) * derxjm_(0,0,1,ui)
                      + my::vderiv_(0, 1)* derxjm_(0,1,1,ui) + my::vderiv_(0, 2) * derxjm_(0,2,1,ui))
                 + my::convvelint_(2) * (my::vderiv_(0, 0) * derxjm_(0,0,2,ui)
                      + my::vderiv_(0, 1)* derxjm_(0,1,2,ui) + my::vderiv_(0, 2) * derxjm_(0,2,2,ui));
    double v01 = +my::convvelint_(0) * (my::vderiv_(0, 0) * derxjm_(1,0,0,ui)
                      + my::vderiv_(0, 1) * derxjm_(1,1,0,ui) + my::vderiv_(0, 2) * derxjm_(1,2,0,ui))
                 + my::convvelint_(2) * (my::vderiv_(0, 0) * derxjm_(1,0,2,ui)
                      + my::vderiv_(0, 1) * derxjm_(1,1,2,ui) + my::vderiv_(0, 2) * derxjm_(1,2,2,ui));
    double v02 = +my::convvelint_(0) * (my::vderiv_(0, 0) * derxjm_(2,0,0,ui)
                      + my::vderiv_(0, 1) * derxjm_(2,1,0,ui) + my::vderiv_(0, 2) * derxjm_(2,2,0,ui))
                 + my::convvelint_(1) * (my::vderiv_(0, 0) * derxjm_(2,0,1,ui)
                      + my::vderiv_(0, 1) * derxjm_(2,1,1,ui) + my::vderiv_(0, 2) * derxjm_(2,2,1,ui));
    double v10 = +my::convvelint_(1) * (my::vderiv_(1, 0) * derxjm_(0,0,1,ui)
                      + my::vderiv_(1, 1) * derxjm_(0,1,1,ui) + my::vderiv_(1, 2) * derxjm_(0,2,1,ui))
                 + my::convvelint_(2) * (my::vderiv_(1, 0) * derxjm_(0,0,2,ui)
                      + my::vderiv_(1, 1) * derxjm_(0,1,2,ui) + my::vderiv_(1, 2) * derxjm_(0,2,2,ui));
    double v11 = +my::convvelint_(0) * (my::vderiv_(1, 0) * derxjm_(1,0,0,ui)
                      + my::vderiv_(1, 1) * derxjm_(1,1,0,ui) + my::vderiv_(1, 2) * derxjm_(1,2,0,ui))
                 + my::convvelint_(2) * (my::vderiv_(1, 0) * derxjm_(1,0,2,ui)
                      + my::vderiv_(1, 1) * derxjm_(1,1,2,ui) + my::vderiv_(1, 2) * derxjm_(1,2,2,ui));
    double v12 = +my::convvelint_(0) * (my::vderiv_(1, 0) * derxjm_(2,0,0,ui)
                      + my::vderiv_(1, 1) * derxjm_(2,1,0,ui) + my::vderiv_(1, 2) * derxjm_(2,2,0,ui))
                 + my::convvelint_(1) * (my::vderiv_(1, 0) * derxjm_(2,0,1,ui)
                      + my::vderiv_(1, 1) * derxjm_(2,1,1,ui) + my::vderiv_(1, 2) * derxjm_(2,2,1,ui));
    double v20 = +my::convvelint_(1) * (my::vderiv_(2, 0) * derxjm_(0,0,1,ui)
                      + my::vderiv_(2, 1) * derxjm_(0,1,1,ui) + my::vderiv_(2, 2) * derxjm_(0,2,1,ui))
                 + my::convvelint_(2) * (my::vderiv_(2, 0) * derxjm_(0,0,2,ui)
                      + my::vderiv_(2, 1) * derxjm_(0,1,2,ui) + my::vderiv_(2, 2) * derxjm_(0,2,2,ui));
    double v21 = +my::convvelint_(0) * (my::vderiv_(2, 0) * derxjm_(1,0,0,ui)
                      + my::vderiv_(2, 1) * derxjm_(1,1,0,ui) + my::vderiv_(2, 2) * derxjm_(1,2,0,ui))
                 + my::convvelint_(2) * (my::vderiv_(2, 0) * derxjm_(1,0,2,ui)
                      + my::vderiv_(2, 1) * derxjm_(1,1,2,ui) + my::vderiv_(2, 2) * derxjm_(1,2,2,ui));
    double v22 = +my::convvelint_(0) * (my::vderiv_(2, 0) * derxjm_(2,0,0,ui)
                      + my::vderiv_(2, 1) * derxjm_(2,1,0,ui) + my::vderiv_(2, 2) * derxjm_(2,2,0,ui))
                 + my::convvelint_(1) * (my::vderiv_(2, 0) * derxjm_(2,0,1,ui)
                      + my::vderiv_(2, 1) * derxjm_(2,1,1,ui) + my::vderiv_(2, 2) * derxjm_(2,2,1,ui));

    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = my::densaf_*timefacfac_det * my::funct_(vi) * (1.0 + addstab );

      ecoupl_u(vi * 3 + 0, ui * 3 + 0) += v * v00;
      ecoupl_u(vi * 3 + 0, ui * 3 + 1) += v * v01;
      ecoupl_u(vi * 3 + 0, ui * 3 + 2) += v * v02;

      ecoupl_u(vi * 3 + 1, ui * 3 + 0) += v * v10;
      ecoupl_u(vi * 3 + 1, ui * 3 + 1) += v * v11;
      ecoupl_u(vi * 3 + 1, ui * 3 + 2) += v * v12;

      ecoupl_u(vi * 3 + 2, ui * 3 + 0) += v * v20;
      ecoupl_u(vi * 3 + 2, ui * 3 + 1) += v * v21;
      ecoupl_u(vi * 3 + 2, ui * 3 + 2) += v * v22;
    }
  }

  // pressure;
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    double v = press_ * timefacfac_det;
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_u(vi * 3, ui * 3 + 1) += v * (   my::deriv_(0, vi) * derxjm_(0,0,1,ui)
                                            + my::deriv_(1, vi) * derxjm_(0,1,1,ui)
                                            + my::deriv_(2, vi) * derxjm_(0,2,1,ui));
      ecoupl_u(vi * 3, ui * 3 + 2) += v * (   my::deriv_(0, vi) * derxjm_(0,0,2,ui)
                                            + my::deriv_(1, vi) * derxjm_(0,1,2,ui)
                                            + my::deriv_(2, vi) * derxjm_(0,2,2,ui));

      ecoupl_u(vi * 3 + 1, ui * 3 + 0) += v * (   my::deriv_(0, vi) * derxjm_(1,0,0,ui)
                                                + my::deriv_(1, vi) * derxjm_(1,1,0,ui)
                                                + my::deriv_(2, vi) * derxjm_(1,2,0,ui));
      ecoupl_u(vi * 3 + 1, ui * 3 + 2) += v * (   my::deriv_(0, vi) * derxjm_(1,0,2,ui)
                                                + my::deriv_(1, vi) * derxjm_(1,1,2,ui)
                                                + my::deriv_(2, vi) * derxjm_(1,2,2,ui));

      ecoupl_u(vi * 3 + 2, ui * 3 + 0) += v * (   my::deriv_(0, vi) * derxjm_(2,0,0,ui)
                                                + my::deriv_(1, vi) * derxjm_(2,1,0,ui)
                                                + my::deriv_(2, vi) * derxjm_(2,2,0,ui));
      ecoupl_u(vi * 3 + 2, ui * 3 + 1) += v * (   my::deriv_(0, vi) * derxjm_(2,0,1,ui)
                                                + my::deriv_(1, vi) * derxjm_(2,1,1,ui)
                                                + my::deriv_(2, vi) * derxjm_(2,2,1,ui));
    }
  }

  // //---------viscous term (brinkman term)
#define xji_00 my::xji_(0,0)
#define xji_01 my::xji_(0,1)
#define xji_02 my::xji_(0,2)
#define xji_10 my::xji_(1,0)
#define xji_11 my::xji_(1,1)
#define xji_12 my::xji_(1,2)
#define xji_20 my::xji_(2,0)
#define xji_21 my::xji_(2,1)
#define xji_22 my::xji_(2,2)

#define xjm(i,j) my::xjm_(i,j)

  if(my::visceff_)
  {

    // part 1: derivative of 1/det

    double v = my::visceff_*timefac*my::fac_ * (1.0 + addstab );
    for (int ui=0; ui<my::nen_; ++ui)
    {
      double derinvJ0 = -v*(my::deriv_(0,ui)*xji_00 + my::deriv_(1,ui)*xji_01 + my::deriv_(2,ui)*xji_02);
      double derinvJ1 = -v*(my::deriv_(0,ui)*xji_10 + my::deriv_(1,ui)*xji_11 + my::deriv_(2,ui)*xji_12);
      double derinvJ2 = -v*(my::deriv_(0,ui)*xji_20 + my::deriv_(1,ui)*xji_21 + my::deriv_(2,ui)*xji_22);
      for (int vi=0; vi<my::nen_; ++vi)
      {
        double visres0 =     2.0*my::derxy_(0, vi)* my::vderxy_(0, 0)
                           +     my::derxy_(1, vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
                           +     my::derxy_(2, vi)*(my::vderxy_(0, 2) + my::vderxy_(2, 0)) ;
        double visres1 =         my::derxy_(0, vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
                           + 2.0*my::derxy_(1, vi)* my::vderxy_(1, 1)
                           +     my::derxy_(2, vi)*(my::vderxy_(1, 2) + my::vderxy_(2, 1)) ;
        double visres2 =         my::derxy_(0, vi)*(my::vderxy_(0, 2) + my::vderxy_(2, 0))
                           +     my::derxy_(1, vi)*(my::vderxy_(1, 2) + my::vderxy_(2, 1))
                           + 2.0*my::derxy_(2, vi)* my::vderxy_(2, 2) ;
        ecoupl_u(vi*3 + 0, ui*3 + 0) += derinvJ0*visres0;
        ecoupl_u(vi*3 + 1, ui*3 + 0) += derinvJ0*visres1;
        ecoupl_u(vi*3 + 2, ui*3 + 0) += derinvJ0*visres2;

        ecoupl_u(vi*3 + 0, ui*3 + 1) += derinvJ1*visres0;
        ecoupl_u(vi*3 + 1, ui*3 + 1) += derinvJ1*visres1;
        ecoupl_u(vi*3 + 2, ui*3 + 1) += derinvJ1*visres2;

        ecoupl_u(vi*3 + 0, ui*3 + 2) += derinvJ2*visres0;
        ecoupl_u(vi*3 + 1, ui*3 + 2) += derinvJ2*visres1;
        ecoupl_u(vi*3 + 2, ui*3 + 2) += derinvJ2*visres2;

        double visres0_poro =     2.0*refgrad_porosity_(0)*my::funct_(vi)* my::vderxy_(0, 0)
                                +     refgrad_porosity_(1)*my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
                                +     refgrad_porosity_(2)*my::funct_(vi)*(my::vderxy_(0, 2) + my::vderxy_(2, 0)) ;
        double visres1_poro =         refgrad_porosity_(0)*my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
                                + 2.0*refgrad_porosity_(1)*my::funct_(vi)* my::vderxy_(1, 1)
                                +     refgrad_porosity_(2)*my::funct_(vi)*(my::vderxy_(1, 2) + my::vderxy_(2, 1)) ;
        double visres2_poro =         refgrad_porosity_(0)*my::funct_(vi)*(my::vderxy_(0, 2) + my::vderxy_(2, 0))
                                +     refgrad_porosity_(1)*my::funct_(vi)*(my::vderxy_(1, 2) + my::vderxy_(2, 1))
                                + 2.0*refgrad_porosity_(2)*my::funct_(vi)* my::vderxy_(2, 2) ;

        ecoupl_u(vi*3 + 0, ui*3 + 0) += -1.0*derinvJ0/porosity_*visres0_poro;
        ecoupl_u(vi*3 + 1, ui*3 + 0) += -1.0*derinvJ0/porosity_*visres1_poro;
        ecoupl_u(vi*3 + 2, ui*3 + 0) += -1.0*derinvJ0/porosity_*visres2_poro;

        ecoupl_u(vi*3 + 0, ui*3 + 1) += -1.0*derinvJ1/porosity_*visres0_poro;
        ecoupl_u(vi*3 + 1, ui*3 + 1) += -1.0*derinvJ1/porosity_*visres1_poro;
        ecoupl_u(vi*3 + 2, ui*3 + 1) += -1.0*derinvJ1/porosity_*visres2_poro;

        ecoupl_u(vi*3 + 0, ui*3 + 2) += -1.0*derinvJ2/porosity_*visres0_poro;
        ecoupl_u(vi*3 + 1, ui*3 + 2) += -1.0*derinvJ2/porosity_*visres1_poro;
        ecoupl_u(vi*3 + 2, ui*3 + 2) += -1.0*derinvJ2/porosity_*visres2_poro;

//        double v0_poro =    // 2.0*my::funct_(vi)* my::vderxy_(0, 0)
//                           +     my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
//                                                                               * (   refgrad_porosity_(0) * derxjm_(0,0,1,ui)
//                                                                                   + refgrad_porosity_(1) * derxjm_(0,1,1,ui)
//                                                                                   + refgrad_porosity_(2) * derxjm_(0,2,1,ui)
//                                                                                 )
//                           +     my::funct_(vi)*(my::vderxy_(0, 2) + my::vderxy_(2, 0))
//                                                                               * (   refgrad_porosity_(0) * derxjm_(0,0,2,ui)
//                                                                                   + refgrad_porosity_(1) * derxjm_(0,1,2,ui)
//                                                                                   + refgrad_porosity_(2) * derxjm_(0,2,2,ui));
//        double v1_poro =         my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
//                                                                               * (   refgrad_porosity_(0) * derxjm_(1,0,0,ui)
//                                                                                   + refgrad_porosity_(1) * derxjm_(1,1,0,ui)
//                                                                                   + refgrad_porosity_(2) * derxjm_(1,2,0,ui))
//                          // + 2.0*refgrad_porosity_(1)*my::funct_(vi)* my::vderxy_(1, 1)
//                           +     my::funct_(vi)*(my::vderxy_(1, 2) + my::vderxy_(2, 1))
//                                                                               * (   refgrad_porosity_(0) * derxjm_(1,0,2,ui)
//                                                                                   + refgrad_porosity_(1) * derxjm_(1,1,2,ui)
//                                                                                   + refgrad_porosity_(2) * derxjm_(1,2,2,ui));
//        double v2_poro =         my::funct_(vi)*(my::vderxy_(0, 2) + my::vderxy_(2, 0))
//                                                                               * (   refgrad_porosity_(0) * derxjm_(2,0,0,ui)
//                                                                                   + refgrad_porosity_(1) * derxjm_(2,1,0,ui)
//                                                                                   + refgrad_porosity_(2) * derxjm_(2,2,0,ui))
//                           +     my::funct_(vi)*(my::vderxy_(1, 2) + my::vderxy_(2, 1))
//                                                                               * (   refgrad_porosity_(0) * derxjm_(2,0,1,ui)
//                                                                                   + refgrad_porosity_(1) * derxjm_(2,1,1,ui)
//                                                                                   + refgrad_porosity_(2) * derxjm_(2,2,1,ui));
//                          // + 2.0*refgrad_porosity_(2)*my::funct_(vi)* my::vderxy_(2, 2) ;
//
//        ecoupl_u(vi * 3 + 0, ui * 3 + 0) += -1.0*v/porosity_/my::det_ * v0_poro;
//        ecoupl_u(vi * 3 + 1, ui * 3 + 1) += -1.0*v/porosity_/my::det_ * v1_poro;
//        ecoupl_u(vi * 3 + 2, ui * 3 + 2) += -1.0*v/porosity_/my::det_ * v2_poro;
      }
    }

    // part 2: derivative of viscosity residual

    const double porosity_inv=1.0/porosity_;
     v = timefacfac_det*my::visceff_ * (1.0 + addstab );
    for (int ui=0; ui<my::nen_; ++ui)
    {
      double v0 = - my::vderiv_(0,0)*(xji_10*derxjm_100(ui) + xji_10*derxjm_100(ui) + xji_20*derxjm_200(ui) + xji_20*derxjm_200(ui))
                  - my::vderiv_(0,1)*(xji_11*derxjm_100(ui) + xji_10*derxjm_110(ui) + xji_21*derxjm_200(ui) + xji_20*derxjm_210(ui))
                  - my::vderiv_(0,2)*(xji_12*derxjm_100(ui) + xji_10*derxjm_120(ui) + xji_22*derxjm_200(ui) + xji_20*derxjm_220(ui))
                  - my::vderiv_(1,0)*(derxjm_100(ui)*xji_00)
                  - my::vderiv_(1,1)*(derxjm_100(ui)*xji_01)
                  - my::vderiv_(1,2)*(derxjm_100(ui)*xji_02)
                  - my::vderiv_(2,0)*(derxjm_200(ui)*xji_00)
                  - my::vderiv_(2,1)*(derxjm_200(ui)*xji_01)
                  - my::vderiv_(2,2)*(derxjm_200(ui)*xji_02);
      double v1 = - my::vderiv_(0,0)*(xji_10*derxjm_110(ui) + xji_11*derxjm_100(ui) + xji_20*derxjm_210(ui) + xji_21*derxjm_200(ui))
                  - my::vderiv_(0,1)*(xji_11*derxjm_110(ui) + xji_11*derxjm_110(ui) + xji_21*derxjm_210(ui) + xji_21*derxjm_210(ui))
                  - my::vderiv_(0,2)*(xji_12*derxjm_110(ui) + xji_11*derxjm_120(ui) + xji_22*derxjm_210(ui) + xji_21*derxjm_220(ui))
                  - my::vderiv_(1,0)*(derxjm_110(ui)*xji_00)
                  - my::vderiv_(1,1)*(derxjm_110(ui)*xji_01)
                  - my::vderiv_(1,2)*(derxjm_110(ui)*xji_02)
                  - my::vderiv_(2,0)*(derxjm_210(ui)*xji_00)
                  - my::vderiv_(2,1)*(derxjm_210(ui)*xji_01)
                  - my::vderiv_(2,2)*(derxjm_210(ui)*xji_02);
      double v2 = - my::vderiv_(0,0)*(xji_10*derxjm_120(ui) + xji_12*derxjm_100(ui) + xji_20*derxjm_220(ui) + xji_22*derxjm_200(ui))
                  - my::vderiv_(0,1)*(xji_11*derxjm_120(ui) + xji_12*derxjm_110(ui) + xji_21*derxjm_220(ui) + xji_22*derxjm_210(ui))
                  - my::vderiv_(0,2)*(xji_12*derxjm_120(ui) + xji_12*derxjm_120(ui) + xji_22*derxjm_220(ui) + xji_22*derxjm_220(ui))
                  - my::vderiv_(1,0)*(derxjm_120(ui)*xji_00)
                  - my::vderiv_(1,1)*(derxjm_120(ui)*xji_01)
                  - my::vderiv_(1,2)*(derxjm_120(ui)*xji_02)
                  - my::vderiv_(2,0)*(derxjm_220(ui)*xji_00)
                  - my::vderiv_(2,1)*(derxjm_220(ui)*xji_01)
                  - my::vderiv_(2,2)*(derxjm_220(ui)*xji_02);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 0, ui*3 + 0) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(2*derxjm_001(ui)*xji_00 + 2*derxjm_001(ui)*xji_00 + xji_20*derxjm_201(ui) + xji_20*derxjm_201(ui))
           - my::vderiv_(0,1)*(2*derxjm_011(ui)*xji_00 + 2*derxjm_001(ui)*xji_01 + xji_21*derxjm_201(ui) + xji_20*derxjm_211(ui))
           - my::vderiv_(0,2)*(2*derxjm_021(ui)*xji_00 + 2*derxjm_001(ui)*xji_02 + xji_22*derxjm_201(ui) + xji_20*derxjm_221(ui))
           - my::vderiv_(1,0)*(derxjm_001(ui)*xji_10)
           - my::vderiv_(1,1)*(derxjm_011(ui)*xji_10)
           - my::vderiv_(1,2)*(derxjm_021(ui)*xji_10)
           - my::vderiv_(2,0)*(derxjm_201(ui)*xji_00 + derxjm_001(ui)*xji_20)
           - my::vderiv_(2,1)*(derxjm_201(ui)*xji_01 + derxjm_011(ui)*xji_20)
           - my::vderiv_(2,2)*(derxjm_201(ui)*xji_02 + derxjm_021(ui)*xji_20);
      v1 = - my::vderiv_(0,0)*(2*derxjm_011(ui)*xji_00 + 2*derxjm_001(ui)*xji_01 + xji_21*derxjm_201(ui) + xji_20*derxjm_211(ui))
           - my::vderiv_(0,1)*(2*derxjm_011(ui)*xji_01 + 2*derxjm_011(ui)*xji_01 + xji_21*derxjm_211(ui) + xji_21*derxjm_211(ui))
           - my::vderiv_(0,2)*(2*derxjm_011(ui)*xji_02 + 2*derxjm_021(ui)*xji_01 + xji_21*derxjm_221(ui) + xji_22*derxjm_211(ui))
           - my::vderiv_(1,0)*(derxjm_001(ui)*xji_11)
           - my::vderiv_(1,1)*(derxjm_011(ui)*xji_11)
           - my::vderiv_(1,2)*(derxjm_021(ui)*xji_11)
           - my::vderiv_(2,0)*(derxjm_211(ui)*xji_00 + derxjm_001(ui)*xji_21)
           - my::vderiv_(2,1)*(derxjm_211(ui)*xji_01 + derxjm_011(ui)*xji_21)
           - my::vderiv_(2,2)*(derxjm_211(ui)*xji_02 + derxjm_021(ui)*xji_21);
      v2 = - my::vderiv_(0,0)*(2*derxjm_021(ui)*xji_00 + 2*derxjm_001(ui)*xji_02 + xji_22*derxjm_201(ui) + xji_20*derxjm_221(ui))
           - my::vderiv_(0,1)*(2*derxjm_011(ui)*xji_02 + 2*derxjm_021(ui)*xji_01 + xji_21*derxjm_221(ui) + xji_22*derxjm_211(ui))
           - my::vderiv_(0,2)*(2*derxjm_021(ui)*xji_02 + 2*derxjm_021(ui)*xji_02 + xji_22*derxjm_221(ui) + xji_22*derxjm_221(ui))
           - my::vderiv_(1,0)*(derxjm_001(ui)*xji_12)
           - my::vderiv_(1,1)*(derxjm_011(ui)*xji_12)
           - my::vderiv_(1,2)*(derxjm_021(ui)*xji_12)
           - my::vderiv_(2,0)*(derxjm_221(ui)*xji_00 + derxjm_001(ui)*xji_22)
           - my::vderiv_(2,1)*(derxjm_221(ui)*xji_01 + derxjm_011(ui)*xji_22)
           - my::vderiv_(2,2)*(derxjm_221(ui)*xji_02 + derxjm_021(ui)*xji_22);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 0, ui*3 + 1) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(2*derxjm_002(ui)*xji_00 + 2*derxjm_002(ui)*xji_00 + xji_10*derxjm_102(ui) + xji_10*derxjm_102(ui))
           - my::vderiv_(0,1)*(2*derxjm_012(ui)*xji_00 + 2*derxjm_002(ui)*xji_01 + xji_11*derxjm_102(ui) + xji_10*derxjm_112(ui))
           - my::vderiv_(0,2)*(2*derxjm_022(ui)*xji_00 + 2*derxjm_002(ui)*xji_02 + xji_12*derxjm_102(ui) + xji_10*derxjm_122(ui))
           - my::vderiv_(1,0)*(derxjm_002(ui)*xji_10 + derxjm_102(ui)*xji_00)
           - my::vderiv_(1,1)*(derxjm_012(ui)*xji_10 + derxjm_102(ui)*xji_01)
           - my::vderiv_(1,2)*(derxjm_022(ui)*xji_10 + derxjm_102(ui)*xji_02)
           - my::vderiv_(2,0)*(derxjm_002(ui)*xji_20)
           - my::vderiv_(2,1)*(derxjm_012(ui)*xji_20)
           - my::vderiv_(2,2)*(derxjm_022(ui)*xji_20);
      v1 = - my::vderiv_(0,0)*(2*derxjm_012(ui)*xji_00 + 2*derxjm_002(ui)*xji_01 + xji_11*derxjm_102(ui) + xji_10*derxjm_112(ui))
           - my::vderiv_(0,1)*(2*derxjm_012(ui)*xji_01 + 2*derxjm_012(ui)*xji_01 + xji_11*derxjm_112(ui) + xji_11*derxjm_112(ui))
           - my::vderiv_(0,2)*(2*derxjm_012(ui)*xji_02 + 2*derxjm_022(ui)*xji_01 + xji_11*derxjm_122(ui) + xji_12*derxjm_112(ui))
           - my::vderiv_(1,0)*(derxjm_002(ui)*xji_11 + derxjm_112(ui)*xji_00)
           - my::vderiv_(1,1)*(derxjm_012(ui)*xji_11 + derxjm_112(ui)*xji_01)
           - my::vderiv_(1,2)*(derxjm_022(ui)*xji_11 + derxjm_112(ui)*xji_02)
           - my::vderiv_(2,0)*(derxjm_002(ui)*xji_21)
           - my::vderiv_(2,1)*(derxjm_012(ui)*xji_21)
           - my::vderiv_(2,2)*(derxjm_022(ui)*xji_21);
      v2 = - my::vderiv_(0,0)*(2*derxjm_022(ui)*xji_00 + 2*derxjm_002(ui)*xji_02 + xji_12*derxjm_102(ui) + xji_10*derxjm_122(ui))
           - my::vderiv_(0,1)*(2*derxjm_012(ui)*xji_02 + 2*derxjm_022(ui)*xji_01 + xji_11*derxjm_122(ui) + xji_12*derxjm_112(ui))
           - my::vderiv_(0,2)*(2*derxjm_022(ui)*xji_02 + 2*derxjm_022(ui)*xji_02 + xji_12*derxjm_122(ui) + xji_12*derxjm_122(ui))
           - my::vderiv_(1,0)*(derxjm_002(ui)*xji_12 + derxjm_122(ui)*xji_00)
           - my::vderiv_(1,1)*(derxjm_012(ui)*xji_12 + derxjm_122(ui)*xji_01)
           - my::vderiv_(1,2)*(derxjm_022(ui)*xji_12 + derxjm_122(ui)*xji_02)
           - my::vderiv_(2,0)*(derxjm_002(ui)*xji_22)
           - my::vderiv_(2,1)*(derxjm_012(ui)*xji_22)
           - my::vderiv_(2,2)*(derxjm_022(ui)*xji_22);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 0, ui*3 + 2) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(derxjm_100(ui)*xji_00)
           - my::vderiv_(0,1)*(derxjm_110(ui)*xji_00)
           - my::vderiv_(0,2)*(derxjm_120(ui)*xji_00)
           - my::vderiv_(1,0)*(2*xji_10*derxjm_100(ui) + 2*xji_10*derxjm_100(ui) + xji_20*derxjm_200(ui) + xji_20*derxjm_200(ui))
           - my::vderiv_(1,1)*(2*xji_11*derxjm_100(ui) + 2*xji_10*derxjm_110(ui) + xji_21*derxjm_200(ui) + xji_20*derxjm_210(ui))
           - my::vderiv_(1,2)*(2*xji_12*derxjm_100(ui) + 2*xji_10*derxjm_120(ui) + xji_22*derxjm_200(ui) + xji_20*derxjm_220(ui))
           - my::vderiv_(2,0)*(derxjm_200(ui)*xji_10 + derxjm_100(ui)*xji_20)
           - my::vderiv_(2,1)*(derxjm_200(ui)*xji_11 + derxjm_110(ui)*xji_20)
           - my::vderiv_(2,2)*(derxjm_200(ui)*xji_12 + derxjm_120(ui)*xji_20);
      v1 = - my::vderiv_(0,0)*(derxjm_100(ui)*xji_01)
           - my::vderiv_(0,1)*(derxjm_110(ui)*xji_01)
           - my::vderiv_(0,2)*(derxjm_120(ui)*xji_01)
           - my::vderiv_(1,0)*(2*xji_10*derxjm_110(ui) + 2*xji_11*derxjm_100(ui) + xji_20*derxjm_210(ui) + xji_21*derxjm_200(ui))
           - my::vderiv_(1,1)*(2*xji_11*derxjm_110(ui) + 2*xji_11*derxjm_110(ui) + xji_21*derxjm_210(ui) + xji_21*derxjm_210(ui))
           - my::vderiv_(1,2)*(2*xji_12*derxjm_110(ui) + 2*xji_11*derxjm_120(ui) + xji_22*derxjm_210(ui) + xji_21*derxjm_220(ui))
           - my::vderiv_(2,0)*(derxjm_210(ui)*xji_10 + derxjm_100(ui)*xji_21)
           - my::vderiv_(2,1)*(derxjm_210(ui)*xji_11 + derxjm_110(ui)*xji_21)
           - my::vderiv_(2,2)*(derxjm_210(ui)*xji_12 + derxjm_120(ui)*xji_21);
      v2 = - my::vderiv_(0,0)*(derxjm_100(ui)*xji_02)
           - my::vderiv_(0,1)*(derxjm_110(ui)*xji_02)
           - my::vderiv_(0,2)*(derxjm_120(ui)*xji_02)
           - my::vderiv_(1,0)*(2*xji_10*derxjm_120(ui) + 2*xji_12*derxjm_100(ui) + xji_20*derxjm_220(ui) + xji_22*derxjm_200(ui))
           - my::vderiv_(1,1)*(2*xji_11*derxjm_120(ui) + 2*xji_12*derxjm_110(ui) + xji_21*derxjm_220(ui) + xji_22*derxjm_210(ui))
           - my::vderiv_(1,2)*(2*xji_12*derxjm_120(ui) + 2*xji_12*derxjm_120(ui) + xji_22*derxjm_220(ui) + xji_22*derxjm_220(ui))
           - my::vderiv_(2,0)*(derxjm_220(ui)*xji_10 + derxjm_100(ui)*xji_22)
           - my::vderiv_(2,1)*(derxjm_220(ui)*xji_11 + derxjm_110(ui)*xji_22)
           - my::vderiv_(2,2)*(derxjm_220(ui)*xji_12 + derxjm_120(ui)*xji_22);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 1, ui*3 + 0) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(derxjm_001(ui)*xji_10)
           - my::vderiv_(0,1)*(derxjm_001(ui)*xji_11)
           - my::vderiv_(0,2)*(derxjm_001(ui)*xji_12)
           - my::vderiv_(1,0)*(xji_00*derxjm_001(ui) + xji_00*derxjm_001(ui) + xji_20*derxjm_201(ui) + xji_20*derxjm_201(ui))
           - my::vderiv_(1,1)*(xji_01*derxjm_001(ui) + xji_00*derxjm_011(ui) + xji_21*derxjm_201(ui) + xji_20*derxjm_211(ui))
           - my::vderiv_(1,2)*(xji_02*derxjm_001(ui) + xji_00*derxjm_021(ui) + xji_22*derxjm_201(ui) + xji_20*derxjm_221(ui))
           - my::vderiv_(2,0)*(derxjm_201(ui)*xji_10)
           - my::vderiv_(2,1)*(derxjm_201(ui)*xji_11)
           - my::vderiv_(2,2)*(derxjm_201(ui)*xji_12);
      v1 = - my::vderiv_(0,0)*(derxjm_011(ui)*xji_10)
           - my::vderiv_(0,1)*(derxjm_011(ui)*xji_11)
           - my::vderiv_(0,2)*(derxjm_011(ui)*xji_12)
           - my::vderiv_(1,0)*(xji_00*derxjm_011(ui) + xji_01*derxjm_001(ui) + xji_20*derxjm_211(ui) + xji_21*derxjm_201(ui))
           - my::vderiv_(1,1)*(xji_01*derxjm_011(ui) + xji_01*derxjm_011(ui) + xji_21*derxjm_211(ui) + xji_21*derxjm_211(ui))
           - my::vderiv_(1,2)*(xji_02*derxjm_011(ui) + xji_01*derxjm_021(ui) + xji_22*derxjm_211(ui) + xji_21*derxjm_221(ui))
           - my::vderiv_(2,0)*(derxjm_211(ui)*xji_10)
           - my::vderiv_(2,1)*(derxjm_211(ui)*xji_11)
           - my::vderiv_(2,2)*(derxjm_211(ui)*xji_12);
      v2 = - my::vderiv_(0,0)*(derxjm_021(ui)*xji_10)
           - my::vderiv_(0,1)*(derxjm_021(ui)*xji_11)
           - my::vderiv_(0,2)*(derxjm_021(ui)*xji_12)
           - my::vderiv_(1,0)*(xji_00*derxjm_021(ui) + xji_02*derxjm_001(ui) + xji_20*derxjm_221(ui) + xji_22*derxjm_201(ui))
           - my::vderiv_(1,1)*(xji_01*derxjm_021(ui) + xji_02*derxjm_011(ui) + xji_21*derxjm_221(ui) + xji_22*derxjm_211(ui))
           - my::vderiv_(1,2)*(xji_02*derxjm_021(ui) + xji_02*derxjm_021(ui) + xji_22*derxjm_221(ui) + xji_22*derxjm_221(ui))
           - my::vderiv_(2,0)*(derxjm_221(ui)*xji_10)
           - my::vderiv_(2,1)*(derxjm_221(ui)*xji_11)
           - my::vderiv_(2,2)*(derxjm_221(ui)*xji_12);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 1, ui*3 + 1) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(derxjm_002(ui)*xji_10 + derxjm_102(ui)*xji_00)
           - my::vderiv_(0,1)*(derxjm_002(ui)*xji_11 + derxjm_112(ui)*xji_00)
           - my::vderiv_(0,2)*(derxjm_002(ui)*xji_12 + derxjm_122(ui)*xji_00)
           - my::vderiv_(1,0)*(xji_00*derxjm_002(ui) + xji_00*derxjm_002(ui) + 2*xji_10*derxjm_102(ui) + 2*xji_10*derxjm_102(ui))
           - my::vderiv_(1,1)*(xji_01*derxjm_002(ui) + xji_00*derxjm_012(ui) + 2*xji_11*derxjm_102(ui) + 2*xji_10*derxjm_112(ui))
           - my::vderiv_(1,2)*(xji_02*derxjm_002(ui) + xji_00*derxjm_022(ui) + 2*xji_12*derxjm_102(ui) + 2*xji_10*derxjm_122(ui))
           - my::vderiv_(2,0)*(derxjm_102(ui)*xji_20)
           - my::vderiv_(2,1)*(derxjm_112(ui)*xji_20)
           - my::vderiv_(2,2)*(derxjm_122(ui)*xji_20);
      v1 = - my::vderiv_(0,0)*(derxjm_012(ui)*xji_10 + derxjm_102(ui)*xji_01)
           - my::vderiv_(0,1)*(derxjm_012(ui)*xji_11 + derxjm_112(ui)*xji_01)
           - my::vderiv_(0,2)*(derxjm_012(ui)*xji_12 + derxjm_122(ui)*xji_01)
           - my::vderiv_(1,0)*(xji_00*derxjm_012(ui) + xji_01*derxjm_002(ui) + 2*xji_10*derxjm_112(ui) + 2*xji_11*derxjm_102(ui))
           - my::vderiv_(1,1)*(xji_01*derxjm_012(ui) + xji_01*derxjm_012(ui) + 2*xji_11*derxjm_112(ui) + 2*xji_11*derxjm_112(ui))
           - my::vderiv_(1,2)*(xji_02*derxjm_012(ui) + xji_01*derxjm_022(ui) + 2*xji_12*derxjm_112(ui) + 2*xji_11*derxjm_122(ui))
           - my::vderiv_(2,0)*(derxjm_102(ui)*xji_21)
           - my::vderiv_(2,1)*(derxjm_112(ui)*xji_21)
           - my::vderiv_(2,2)*(derxjm_122(ui)*xji_21);
      v2 = - my::vderiv_(0,0)*(derxjm_022(ui)*xji_10 + derxjm_102(ui)*xji_02)
           - my::vderiv_(0,1)*(derxjm_022(ui)*xji_11 + derxjm_112(ui)*xji_02)
           - my::vderiv_(0,2)*(derxjm_022(ui)*xji_12 + derxjm_122(ui)*xji_02)
           - my::vderiv_(1,0)*(xji_00*derxjm_022(ui) + xji_02*derxjm_002(ui) + 2*xji_10*derxjm_122(ui) + 2*xji_12*derxjm_102(ui))
           - my::vderiv_(1,1)*(xji_01*derxjm_022(ui) + xji_02*derxjm_012(ui) + 2*xji_11*derxjm_122(ui) + 2*xji_12*derxjm_112(ui))
           - my::vderiv_(1,2)*(xji_02*derxjm_022(ui) + xji_02*derxjm_022(ui) + 2*xji_12*derxjm_122(ui) + 2*xji_12*derxjm_122(ui))
           - my::vderiv_(2,0)*(derxjm_102(ui)*xji_22)
           - my::vderiv_(2,1)*(derxjm_112(ui)*xji_22)
           - my::vderiv_(2,2)*(derxjm_122(ui)*xji_22);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 1, ui*3 + 2) +=   v * (my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v * my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(derxjm_200(ui)*xji_00)
           - my::vderiv_(0,1)*(derxjm_210(ui)*xji_00)
           - my::vderiv_(0,2)*(derxjm_220(ui)*xji_00)
           - my::vderiv_(1,0)*(derxjm_200(ui)*xji_10 + derxjm_100(ui)*xji_20)
           - my::vderiv_(1,1)*(derxjm_210(ui)*xji_10 + derxjm_100(ui)*xji_21)
           - my::vderiv_(1,2)*(derxjm_220(ui)*xji_10 + derxjm_100(ui)*xji_22)
           - my::vderiv_(2,0)*(xji_10*derxjm_100(ui) + xji_10*derxjm_100(ui) + 2*xji_20*derxjm_200(ui) + 2*xji_20*derxjm_200(ui))
           - my::vderiv_(2,1)*(xji_11*derxjm_100(ui) + xji_10*derxjm_110(ui) + 2*xji_21*derxjm_200(ui) + 2*xji_20*derxjm_210(ui))
           - my::vderiv_(2,2)*(xji_12*derxjm_100(ui) + xji_10*derxjm_120(ui) + 2*xji_22*derxjm_200(ui) + 2*xji_20*derxjm_220(ui));
      v1 = - my::vderiv_(0,0)*(derxjm_200(ui)*xji_01)
           - my::vderiv_(0,1)*(derxjm_210(ui)*xji_01)
           - my::vderiv_(0,2)*(derxjm_220(ui)*xji_01)
           - my::vderiv_(1,0)*(derxjm_200(ui)*xji_11 + derxjm_110(ui)*xji_20)
           - my::vderiv_(1,1)*(derxjm_210(ui)*xji_11 + derxjm_110(ui)*xji_21)
           - my::vderiv_(1,2)*(derxjm_220(ui)*xji_11 + derxjm_110(ui)*xji_22)
           - my::vderiv_(2,0)*(xji_10*derxjm_110(ui) + xji_11*derxjm_100(ui) + 2*xji_20*derxjm_210(ui) + 2*xji_21*derxjm_200(ui))
           - my::vderiv_(2,1)*(xji_11*derxjm_110(ui) + xji_11*derxjm_110(ui) + 2*xji_21*derxjm_210(ui) + 2*xji_21*derxjm_210(ui))
           - my::vderiv_(2,2)*(xji_12*derxjm_110(ui) + xji_11*derxjm_120(ui) + 2*xji_22*derxjm_210(ui) + 2*xji_21*derxjm_220(ui));
      v2 = - my::vderiv_(0,0)*(derxjm_200(ui)*xji_02)
           - my::vderiv_(0,1)*(derxjm_210(ui)*xji_02)
           - my::vderiv_(0,2)*(derxjm_220(ui)*xji_02)
           - my::vderiv_(1,0)*(derxjm_200(ui)*xji_12 + derxjm_120(ui)*xji_20)
           - my::vderiv_(1,1)*(derxjm_210(ui)*xji_12 + derxjm_120(ui)*xji_21)
           - my::vderiv_(1,2)*(derxjm_220(ui)*xji_12 + derxjm_120(ui)*xji_22)
           - my::vderiv_(2,0)*(xji_10*derxjm_120(ui) + xji_12*derxjm_100(ui) + 2*xji_20*derxjm_220(ui) + 2*xji_22*derxjm_200(ui))
           - my::vderiv_(2,1)*(xji_11*derxjm_120(ui) + xji_12*derxjm_110(ui) + 2*xji_21*derxjm_220(ui) + 2*xji_22*derxjm_210(ui))
           - my::vderiv_(2,2)*(xji_12*derxjm_120(ui) + xji_12*derxjm_120(ui) + 2*xji_22*derxjm_220(ui) + 2*xji_22*derxjm_220(ui));

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 2, ui*3 + 0) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(derxjm_201(ui)*xji_00 + derxjm_001(ui)*xji_20)
           - my::vderiv_(0,1)*(derxjm_211(ui)*xji_00 + derxjm_001(ui)*xji_21)
           - my::vderiv_(0,2)*(derxjm_221(ui)*xji_00 + derxjm_001(ui)*xji_22)
           - my::vderiv_(1,0)*(derxjm_201(ui)*xji_10)
           - my::vderiv_(1,1)*(derxjm_211(ui)*xji_10)
           - my::vderiv_(1,2)*(derxjm_221(ui)*xji_10)
           - my::vderiv_(2,0)*(xji_00*derxjm_001(ui) + xji_00*derxjm_001(ui) + 2*xji_20*derxjm_201(ui) + 2*xji_20*derxjm_201(ui))
           - my::vderiv_(2,1)*(xji_01*derxjm_001(ui) + xji_00*derxjm_011(ui) + 2*xji_21*derxjm_201(ui) + 2*xji_20*derxjm_211(ui))
           - my::vderiv_(2,2)*(xji_02*derxjm_001(ui) + xji_00*derxjm_021(ui) + 2*xji_22*derxjm_201(ui) + 2*xji_20*derxjm_221(ui));
      v1 = - my::vderiv_(0,0)*(derxjm_201(ui)*xji_01 + derxjm_011(ui)*xji_20)
           - my::vderiv_(0,1)*(derxjm_211(ui)*xji_01 + derxjm_011(ui)*xji_21)
           - my::vderiv_(0,2)*(derxjm_221(ui)*xji_01 + derxjm_011(ui)*xji_22)
           - my::vderiv_(1,0)*(derxjm_201(ui)*xji_11)
           - my::vderiv_(1,1)*(derxjm_211(ui)*xji_11)
           - my::vderiv_(1,2)*(derxjm_221(ui)*xji_11)
           - my::vderiv_(2,0)*(xji_00*derxjm_011(ui) + xji_01*derxjm_001(ui) + 2*xji_20*derxjm_211(ui) + 2*xji_21*derxjm_201(ui))
           - my::vderiv_(2,1)*(xji_01*derxjm_011(ui) + xji_01*derxjm_011(ui) + 2*xji_21*derxjm_211(ui) + 2*xji_21*derxjm_211(ui))
           - my::vderiv_(2,2)*(xji_02*derxjm_011(ui) + xji_01*derxjm_021(ui) + 2*xji_22*derxjm_211(ui) + 2*xji_21*derxjm_221(ui));
      v2 = - my::vderiv_(0,0)*(derxjm_201(ui)*xji_02 + derxjm_021(ui)*xji_20)
           - my::vderiv_(0,1)*(derxjm_211(ui)*xji_02 + derxjm_021(ui)*xji_21)
           - my::vderiv_(0,2)*(derxjm_221(ui)*xji_02 + derxjm_021(ui)*xji_22)
           - my::vderiv_(1,0)*(derxjm_201(ui)*xji_12)
           - my::vderiv_(1,1)*(derxjm_211(ui)*xji_12)
           - my::vderiv_(1,2)*(derxjm_221(ui)*xji_12)
           - my::vderiv_(2,0)*(xji_00*derxjm_021(ui) + xji_02*derxjm_001(ui) + 2*xji_20*derxjm_221(ui) + 2*xji_22*derxjm_201(ui))
           - my::vderiv_(2,1)*(xji_01*derxjm_021(ui) + xji_02*derxjm_011(ui) + 2*xji_21*derxjm_221(ui) + 2*xji_22*derxjm_211(ui))
           - my::vderiv_(2,2)*(xji_02*derxjm_021(ui) + xji_02*derxjm_021(ui) + 2*xji_22*derxjm_221(ui) + 2*xji_22*derxjm_221(ui));

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 2, ui*3 + 1) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(derxjm_002(ui)*xji_20)
           - my::vderiv_(0,1)*(derxjm_002(ui)*xji_21)
           - my::vderiv_(0,2)*(derxjm_002(ui)*xji_22)
           - my::vderiv_(1,0)*(derxjm_102(ui)*xji_20)
           - my::vderiv_(1,1)*(derxjm_102(ui)*xji_21)
           - my::vderiv_(1,2)*(derxjm_102(ui)*xji_22)
           - my::vderiv_(2,0)*(xji_00*derxjm_002(ui) + xji_00*derxjm_002(ui) + xji_10*derxjm_102(ui) + xji_10*derxjm_102(ui))
           - my::vderiv_(2,1)*(xji_01*derxjm_002(ui) + xji_00*derxjm_012(ui) + xji_11*derxjm_102(ui) + xji_10*derxjm_112(ui))
           - my::vderiv_(2,2)*(xji_02*derxjm_002(ui) + xji_00*derxjm_022(ui) + xji_12*derxjm_102(ui) + xji_10*derxjm_122(ui));
      v1 = - my::vderiv_(0,0)*(derxjm_012(ui)*xji_20)
           - my::vderiv_(0,1)*(derxjm_012(ui)*xji_21)
           - my::vderiv_(0,2)*(derxjm_012(ui)*xji_22)
           - my::vderiv_(1,0)*(derxjm_112(ui)*xji_20)
           - my::vderiv_(1,1)*(derxjm_112(ui)*xji_21)
           - my::vderiv_(1,2)*(derxjm_112(ui)*xji_22)
           - my::vderiv_(2,0)*(xji_00*derxjm_012(ui) + xji_01*derxjm_002(ui) + xji_10*derxjm_112(ui) + xji_11*derxjm_102(ui))
           - my::vderiv_(2,1)*(xji_01*derxjm_012(ui) + xji_01*derxjm_012(ui) + xji_11*derxjm_112(ui) + xji_11*derxjm_112(ui))
           - my::vderiv_(2,2)*(xji_02*derxjm_012(ui) + xji_01*derxjm_022(ui) + xji_12*derxjm_112(ui) + xji_11*derxjm_122(ui));
      v2 = - my::vderiv_(0,0)*(derxjm_022(ui)*xji_20)
           - my::vderiv_(0,1)*(derxjm_022(ui)*xji_21)
           - my::vderiv_(0,2)*(derxjm_022(ui)*xji_22)
           - my::vderiv_(1,0)*(derxjm_122(ui)*xji_20)
           - my::vderiv_(1,1)*(derxjm_122(ui)*xji_21)
           - my::vderiv_(1,2)*(derxjm_122(ui)*xji_22)
           - my::vderiv_(2,0)*(xji_00*derxjm_022(ui) + xji_02*derxjm_002(ui) + xji_10*derxjm_122(ui) + xji_12*derxjm_102(ui))
           - my::vderiv_(2,1)*(xji_01*derxjm_022(ui) + xji_02*derxjm_012(ui) + xji_11*derxjm_122(ui) + xji_12*derxjm_112(ui))
           - my::vderiv_(2,2)*(xji_02*derxjm_022(ui) + xji_02*derxjm_022(ui) + xji_12*derxjm_122(ui) + xji_12*derxjm_122(ui));

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*3 + 2, ui*3 + 2) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 + my::deriv_(2,vi)*v2)
                                     - v*my::funct_(vi)*porosity_inv*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 + refgrad_porosity_(2)*v2);
      }
    }
  }//if(my::visceff_)

  //*************************** ReacStab**********************************
  if(my::fldpara_->RStab() != INPAR::FLUID::reactive_stab_none)
  {
    // pressure;
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v =  my::funct_(vi) * timefacfac_det * addstab;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_u(vi * 3 + 1, ui * 3) += v * (   refgradp_(0) * derxjm_(0,0,1,ui)
                                              + refgradp_(1) * derxjm_(0,1,1,ui)
                                              + refgradp_(2) * derxjm_(0,2,1,ui));
        ecoupl_u(vi * 3 + 2, ui * 3) += v * (   refgradp_(0) * derxjm_(0,0,2,ui)
                                              + refgradp_(1) * derxjm_(0,1,2,ui)
                                              + refgradp_(2) * derxjm_(0,2,2,ui));

        ecoupl_u(vi * 3 + 0, ui * 3 + 1) += v * (   refgradp_(0) * derxjm_(1,0,0,ui)
                                                  + refgradp_(1) * derxjm_(1,1,0,ui)
                                                  + refgradp_(2) * derxjm_(1,2,0,ui));
        ecoupl_u(vi * 3 + 2, ui * 3 + 1) += v * (   refgradp_(0) * derxjm_(1,0,2,ui)
                                                  + refgradp_(1) * derxjm_(1,1,2,ui)
                                                  + refgradp_(2) * derxjm_(1,2,2,ui));

        ecoupl_u(vi * 3 + 0, ui * 3 + 2) += v * (   refgradp_(0) * derxjm_(2,0,0,ui)
                                                  + refgradp_(1) * derxjm_(2,1,0,ui)
                                                  + refgradp_(2) * derxjm_(2,2,0,ui));
        ecoupl_u(vi * 3 + 1, ui * 3 + 2) += v * (   refgradp_(0) * derxjm_(2,0,1,ui)
                                                  + refgradp_(1) * derxjm_(2,1,1,ui)
                                                  + refgradp_(2) * derxjm_(2,2,1,ui));
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::LinMeshMotion_3D_Pres_OD(
    LINALG::Matrix< my::nen_, my::nsd_ * my::nen_>&                   ecoupl_p,
    const double &                                                    dphi_dp,
    const double &                                                    dphi_dJ,
    const double &                                                    refporositydot,
    const double &                                                    timefacfac)
{
  //*************************** linearisation of mesh motion in continuity equation**********************************

  const double timefacfac_det=timefacfac / my::det_;

  if( static_cast<DRT::ELEMENTS::FluidEleParameterPoro*>(my::fldpara_)->PoroContiPartInt() == false )
  {
    // (porosity_)*div u
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = timefacfac_det * my::funct_(vi, 0) * porosity_;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_p(vi, ui * 3    ) += v * (+ my::vderiv_(1, 0) * derxjm_(0,0,1,ui)
                                              + my::vderiv_(1, 1) * derxjm_(0,1,1,ui)
                                              + my::vderiv_(1, 2) * derxjm_(0,2,1,ui)
                                              + my::vderiv_(2, 0) * derxjm_(0,0,2,ui)
                                              + my::vderiv_(2, 1) * derxjm_(0,1,2,ui)
                                              + my::vderiv_(2, 2) * derxjm_(0,2,2,ui));

        ecoupl_p(vi, ui * 3 + 1) += v * (+ my::vderiv_(0, 0) * derxjm_(1,0,0,ui)
                                              + my::vderiv_(0, 1) * derxjm_(1,1,0,ui)
                                              + my::vderiv_(0, 2) * derxjm_(1,2,0,ui)
                                              + my::vderiv_(2, 0) * derxjm_(1,0,2,ui)
                                              + my::vderiv_(2, 1) * derxjm_(1,1,2,ui)
                                              + my::vderiv_(2, 2) * derxjm_(1,2,2,ui));

        ecoupl_p(vi, ui * 3 + 2) += v * (+ my::vderiv_(0, 0) * derxjm_(2,0,0,ui)
                                              + my::vderiv_(0, 1) * derxjm_(2,1,0,ui)
                                              + my::vderiv_(0, 2) * derxjm_(2,2,0,ui)
                                              + my::vderiv_(1, 0) * derxjm_(2,0,1,ui)
                                              + my::vderiv_(1, 1) * derxjm_(2,1,1,ui)
                                              + my::vderiv_(1, 2) * derxjm_(2,2,1,ui));
      }
    }

    if (my::fldparatimint_->IsStationary() == false)
    {
      // (dphi_dJ*J)*div vs
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v = timefacfac_det * my::funct_(vi, 0) * dphi_dJ * J_;
        for (int ui = 0; ui < my::nen_; ++ui)
        {
          ecoupl_p(vi, ui * 3 + 0) += v * (+ gridvelderiv_(1, 0) * derxjm_(0,0,1,ui)
                                           + gridvelderiv_(1, 1) * derxjm_(0,1,1,ui)
                                           + gridvelderiv_(1, 2) * derxjm_(0,2,1,ui)
                                           + gridvelderiv_(2, 0) * derxjm_(0,0,2,ui)
                                           + gridvelderiv_(2, 1) * derxjm_(0,1,2,ui)
                                           + gridvelderiv_(2, 2) * derxjm_(0,2,2,ui));

          ecoupl_p(vi, ui * 3 + 1) += v * (+ gridvelderiv_(0, 0) * derxjm_(1,0,0,ui)
                                           + gridvelderiv_(0, 1) * derxjm_(1,1,0,ui)
                                           + gridvelderiv_(0, 2) * derxjm_(1,2,0,ui)
                                           + gridvelderiv_(2, 0) * derxjm_(1,0,2,ui)
                                           + gridvelderiv_(2, 1) * derxjm_(1,1,2,ui)
                                           + gridvelderiv_(2, 2) * derxjm_(1,2,2,ui));

          ecoupl_p(vi, ui * 3 + 2) += v * (+ gridvelderiv_(0, 0) * derxjm_(2,0,0,ui)
                                           + gridvelderiv_(0, 1) * derxjm_(2,1,0,ui)
                                           + gridvelderiv_(0, 2) * derxjm_(2,2,0,ui)
                                           + gridvelderiv_(1, 0) * derxjm_(2,0,1,ui)
                                           + gridvelderiv_(1, 1) * derxjm_(2,1,1,ui)
                                           + gridvelderiv_(1, 2) * derxjm_(2,2,1,ui));
        }
      }
    }

    //-----------(u-vs)grad(phi)

    for (int ui = 0; ui < my::nen_; ++ui)
    {
      double v00 = + (my::velint_(1) - gridvelint_(1)) * (  refgrad_porosity_(0) * derxjm_(0,0,1,ui)
                                                          + refgrad_porosity_(1) * derxjm_(0,1,1,ui)
                                                          + refgrad_porosity_(2) * derxjm_(0,2,1,ui) )
                   + (my::velint_(2) - gridvelint_(2)) * (  refgrad_porosity_(0) * derxjm_(0,0,2,ui)
                                                          + refgrad_porosity_(1) * derxjm_(0,1,2,ui)
                                                          + refgrad_porosity_(2) * derxjm_(0,2,2,ui));
      double v01 = + (my::velint_(0) - gridvelint_(0)) * (  refgrad_porosity_(0) * derxjm_(1,0,0,ui)
                                                          + refgrad_porosity_(1) * derxjm_(1,1,0,ui)
                                                          + refgrad_porosity_(2) * derxjm_(1,2,0,ui))
                   + (my::velint_(2) - gridvelint_(2)) * (  refgrad_porosity_(0) * derxjm_(1,0,2,ui)
                                                          + refgrad_porosity_(1) * derxjm_(1,1,2,ui)
                                                          + refgrad_porosity_(2) * derxjm_(1,2,2,ui));
      double v02 = + (my::velint_(0) - gridvelint_(0)) * (  refgrad_porosity_(0) * derxjm_(2,0,0,ui)
                                                          + refgrad_porosity_(1) * derxjm_(2,1,0,ui)
                                                          + refgrad_porosity_(2) * derxjm_(2,2,0,ui))
                   + (my::velint_(1) - gridvelint_(1)) * (  refgrad_porosity_(0) * derxjm_(2,0,1,ui)
                                                          + refgrad_porosity_(1) * derxjm_(2,1,1,ui)
                                                          + refgrad_porosity_(2) * derxjm_(2,2,1,ui));

      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v = timefacfac_det * my::funct_(vi);

        ecoupl_p(vi, ui * 3 + 0) += v * v00;
        ecoupl_p(vi, ui * 3 + 1) += v * v01;
        ecoupl_p(vi, ui * 3 + 2) += v * v02;
      }
    }
  }
  else
  {
    if (my::fldparatimint_->IsStationary() == false)
    {
      // (dphi_dJ*J+phi)*div vs
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v = timefacfac_det * my::funct_(vi, 0) * (dphi_dJ * J_+porosity_);
        for (int ui = 0; ui < my::nen_; ++ui)
        {
          ecoupl_p(vi, ui * 3 + 0) += v * (+ gridvelderiv_(1, 0) * derxjm_(0,0,1,ui)
                                           + gridvelderiv_(1, 1) * derxjm_(0,1,1,ui)
                                           + gridvelderiv_(1, 2) * derxjm_(0,2,1,ui)
                                           + gridvelderiv_(2, 0) * derxjm_(0,0,2,ui)
                                           + gridvelderiv_(2, 1) * derxjm_(0,1,2,ui)
                                           + gridvelderiv_(2, 2) * derxjm_(0,2,2,ui));

          ecoupl_p(vi, ui * 3 + 1) += v * (+ gridvelderiv_(0, 0) * derxjm_(1,0,0,ui)
                                           + gridvelderiv_(0, 1) * derxjm_(1,1,0,ui)
                                           + gridvelderiv_(0, 2) * derxjm_(1,2,0,ui)
                                           + gridvelderiv_(2, 0) * derxjm_(1,0,2,ui)
                                           + gridvelderiv_(2, 1) * derxjm_(1,1,2,ui)
                                           + gridvelderiv_(2, 2) * derxjm_(1,2,2,ui));

          ecoupl_p(vi, ui * 3 + 2) += v * (+ gridvelderiv_(0, 0) * derxjm_(2,0,0,ui)
                                           + gridvelderiv_(0, 1) * derxjm_(2,1,0,ui)
                                           + gridvelderiv_(0, 2) * derxjm_(2,2,0,ui)
                                           + gridvelderiv_(1, 0) * derxjm_(2,0,1,ui)
                                           + gridvelderiv_(1, 1) * derxjm_(2,1,1,ui)
                                           + gridvelderiv_(1, 2) * derxjm_(2,2,1,ui));
        }
      }
    }

    //----------- phi * (u-vs)grad(vi)
    const double v = -1.0 * timefacfac_det * porosity_;

    for (int ui = 0; ui < my::nen_; ++ui)
    {
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v00 = + (my::velint_(1) - gridvelint_(1)) * (   my::deriv_(0,vi) * derxjm_(0,0,1,ui)
                                                             + my::deriv_(1,vi) * derxjm_(0,1,1,ui)
                                                             + my::deriv_(2,vi) * derxjm_(0,2,1,ui) )
                     + (my::velint_(2) - gridvelint_(2)) * (   my::deriv_(0,vi) * derxjm_(0,0,2,ui)
                                                             + my::deriv_(1,vi) * derxjm_(0,1,2,ui)
                                                             + my::deriv_(2,vi) * derxjm_(0,2,2,ui));
        double v01 = + (my::velint_(0) - gridvelint_(0)) * (   my::deriv_(0,vi) * derxjm_(1,0,0,ui)
                                                             + my::deriv_(1,vi) * derxjm_(1,1,0,ui)
                                                             + my::deriv_(2,vi) * derxjm_(1,2,0,ui))
                     + (my::velint_(2) - gridvelint_(2)) * (   my::deriv_(0,vi) * derxjm_(1,0,2,ui)
                                                             + my::deriv_(1,vi) * derxjm_(1,1,2,ui)
                                                             + my::deriv_(2,vi) * derxjm_(1,2,2,ui));
        double v02 = + (my::velint_(0) - gridvelint_(0)) * (   my::deriv_(0,vi) * derxjm_(2,0,0,ui)
                                                             + my::deriv_(1,vi) * derxjm_(2,1,0,ui)
                                                             + my::deriv_(2,vi) * derxjm_(2,2,0,ui))
                     + (my::velint_(1) - gridvelint_(1)) * (   my::deriv_(0,vi) * derxjm_(2,0,1,ui)
                                                             + my::deriv_(1,vi) * derxjm_(2,1,1,ui)
                                                             + my::deriv_(2,vi) * derxjm_(2,2,1,ui));

        ecoupl_p(vi, ui * 3 + 0) += v * v00;
        ecoupl_p(vi, ui * 3 + 1) += v * v01;
        ecoupl_p(vi, ui * 3 + 2) += v * v02;
      }
    }

  }//partial integration

  if (my::fldparatimint_->IsStationary() == false)
  {
    // dphi_dp*dp/dt + rhs
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = my::fac_ * my::funct_(vi, 0) *    dphi_dp * press_
                + timefacfac * my::funct_(vi, 0) * refporositydot ;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_p(vi, ui * 3)     += v * my::derxy_(0, ui);
        ecoupl_p(vi, ui * 3 + 1) += v * my::derxy_(1, ui);
        ecoupl_p(vi, ui * 3 + 2) += v * my::derxy_(2, ui);
      }
    }
  }

  //  rhs
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    double v = -1.0 * timefacfac * my::funct_(vi, 0) * ( dphi_dp * my::rhscon_);
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_p(vi, ui * 3)     += v * my::derxy_(0, ui);
      ecoupl_p(vi, ui * 3 + 1) += v * my::derxy_(1, ui);
      ecoupl_p(vi, ui * 3 + 2) += v * my::derxy_(2, ui);
    }
  }

  //-------------------
  if (my::fldpara_->PSPG())
  {
    // PSPG rhs
    {
      const double v = -1.0 * timefacfac_det;

      for (int ui = 0; ui < my::nen_; ++ui)
      {
        for (int vi = 0; vi < my::nen_; ++vi)
        {
          double v00 = + my::sgvelint_(1) * (   my::deriv_(0,vi) * derxjm_(0,0,1,ui)
                                              + my::deriv_(1,vi) * derxjm_(0,1,1,ui)
                                              + my::deriv_(2,vi) * derxjm_(0,2,1,ui) )
                       + my::sgvelint_(2) * (   my::deriv_(0,vi) * derxjm_(0,0,2,ui)
                                              + my::deriv_(1,vi) * derxjm_(0,1,2,ui)
                                              + my::deriv_(2,vi) * derxjm_(0,2,2,ui));
          double v01 = + my::sgvelint_(0) * (   my::deriv_(0,vi) * derxjm_(1,0,0,ui)
                                              + my::deriv_(1,vi) * derxjm_(1,1,0,ui)
                                              + my::deriv_(2,vi) * derxjm_(1,2,0,ui))
                       + my::sgvelint_(2) * (   my::deriv_(0,vi) * derxjm_(1,0,2,ui)
                                              + my::deriv_(1,vi) * derxjm_(1,1,2,ui)
                                              + my::deriv_(2,vi) * derxjm_(1,2,2,ui));
          double v02 = + my::sgvelint_(0) * (   my::deriv_(0,vi) * derxjm_(2,0,0,ui)
                                              + my::deriv_(1,vi) * derxjm_(2,1,0,ui)
                                              + my::deriv_(2,vi) * derxjm_(2,2,0,ui))
                       + my::sgvelint_(1) * (   my::deriv_(0,vi) * derxjm_(2,0,1,ui)
                                              + my::deriv_(1,vi) * derxjm_(2,1,1,ui)
                                              + my::deriv_(2,vi) * derxjm_(2,2,1,ui));

          ecoupl_p(vi, ui * 3 + 0) += v * v00;
          ecoupl_p(vi, ui * 3 + 1) += v * v01;
          ecoupl_p(vi, ui * 3 + 2) += v * v02;
        }
      }
    }

    double scal_grad_q=0.0;

    if(my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
    {
      scal_grad_q=my::tau_(1);
    }
    else
    {
      scal_grad_q=0.0;//my::fldpara_->AlphaF()*fac3;
    }

    //pressure
    {
      const double v = timefacfac_det * scal_grad_q;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        for (int vi = 0; vi < my::nen_; ++vi)
        {

          double v00 = + my::derxy_(1,vi) * (  refgradp_(0) * derxjm_(0,0,1,ui)
                                             + refgradp_(1) * derxjm_(0,1,1,ui)
                                             + refgradp_(2) * derxjm_(0,2,1,ui) )
                       + my::derxy_(2,vi) * (  refgradp_(0) * derxjm_(0,0,2,ui)
                                             + refgradp_(1) * derxjm_(0,1,2,ui)
                                             + refgradp_(2) * derxjm_(0,2,2,ui));
          double v01 = + my::derxy_(0,vi) * (  refgradp_(0) * derxjm_(1,0,0,ui)
                                             + refgradp_(1) * derxjm_(1,1,0,ui)
                                             + refgradp_(2) * derxjm_(1,2,0,ui))
                       + my::derxy_(2,vi) * (  refgradp_(0) * derxjm_(1,0,2,ui)
                                             + refgradp_(1) * derxjm_(1,1,2,ui)
                                             + refgradp_(2) * derxjm_(1,2,2,ui));
          double v02 = + my::derxy_(0,vi) * (  refgradp_(0) * derxjm_(2,0,0,ui)
                                             + refgradp_(1) * derxjm_(2,1,0,ui)
                                             + refgradp_(2) * derxjm_(2,2,0,ui))
                       + my::derxy_(1,vi) * (  refgradp_(0) * derxjm_(2,0,1,ui)
                                             + refgradp_(1) * derxjm_(2,1,1,ui)
                                             + refgradp_(2) * derxjm_(2,2,1,ui));

          ecoupl_p(vi, ui * 3 + 0) += v * v00;
          ecoupl_p(vi, ui * 3 + 1) += v * v01;
          ecoupl_p(vi, ui * 3 + 2) += v * v02;
        }
      }

      LINALG::Matrix<my::nen_,1> temp;
      temp.MultiplyTN(my::derxy_,my::gradp_);
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v3 = -1.0 * timefacfac * scal_grad_q * temp(vi);
        for (int ui = 0; ui < my::nen_; ++ui)
        {
          ecoupl_p(vi, ui * 3)     += v3 * my::derxy_(0, ui);
          ecoupl_p(vi, ui * 3 + 1) += v3 * my::derxy_(1, ui);
          ecoupl_p(vi, ui * 3 + 2) += v3 * my::derxy_(2, ui);
        }
      }

    }

    //convective term

    {
      const double v = my::densaf_*timefacfac_det * scal_grad_q;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        for (int vi = 0; vi < my::nen_; ++vi)
        {
          double v00 = + my::derxy_(1,vi) * my::convvelint_(1) * (
                                                  my::vderiv_(0, 0) * derxjm_(0,0,1,ui)
                                                + my::vderiv_(0, 1) * derxjm_(0,1,1,ui)
                                                + my::vderiv_(0, 2) * derxjm_(0,2,1,ui))
                       + my::derxy_(2,vi) * my::convvelint_(2) * (
                                                  my::vderiv_(0, 0) * derxjm_(0,0,2,ui)
                                                + my::vderiv_(0, 1) * derxjm_(0,1,2,ui)
                                                + my::vderiv_(0, 2) * derxjm_(0,2,2,ui));
          double v10 = + my::derxy_(1,vi) *  my::convvelint_(1) * (
                                                  my::vderiv_(1, 0) * derxjm_(0,0,1,ui)
                                                + my::vderiv_(1, 1) * derxjm_(0,1,1,ui)
                                                + my::vderiv_(1, 2) * derxjm_(0,2,1,ui))
                       + my::derxy_(2,vi) * my::convvelint_(2) * (
                                                  my::vderiv_(1, 0) * derxjm_(0,0,2,ui)
                                                + my::vderiv_(1, 1) * derxjm_(0,1,2,ui)
                                                + my::vderiv_(1, 2) * derxjm_(0,2,2,ui));
          double v20 = + my::derxy_(1,vi) * my::convvelint_(1) * (
                                                  my::vderiv_(2, 0) * derxjm_(0,0,1,ui)
                                                + my::vderiv_(2, 1) * derxjm_(0,1,1,ui)
                                                + my::vderiv_(2, 2) * derxjm_(0,2,1,ui))
                       + my::derxy_(2,vi) * my::convvelint_(2) * (
                                                  my::vderiv_(2, 0) * derxjm_(0,0,2,ui)
                                                + my::vderiv_(2, 1) * derxjm_(0,1,2,ui)
                                                + my::vderiv_(2, 2) * derxjm_(0,2,2,ui));
          double v01 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(0, 0) * derxjm_(1,0,0,ui)
                                                + my::vderiv_(0, 1) * derxjm_(1,1,0,ui)
                                                + my::vderiv_(0, 2) * derxjm_(1,2,0,ui))
                       + my::derxy_(2,vi) * my::convvelint_(2) * (
                                                  my::vderiv_(0, 0) * derxjm_(1,0,2,ui)
                                                + my::vderiv_(0, 1) * derxjm_(1,1,2,ui)
                                                + my::vderiv_(0, 2) * derxjm_(1,2,2,ui));
          double v11 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(1, 0) * derxjm_(1,0,0,ui)
                                                + my::vderiv_(1, 1) * derxjm_(1,1,0,ui)
                                                + my::vderiv_(1, 2) * derxjm_(1,2,0,ui))
                       + my::derxy_(2,vi) * my::convvelint_(2) * (
                                                  my::vderiv_(1, 0) * derxjm_(1,0,2,ui)
                                                + my::vderiv_(1, 1) * derxjm_(1,1,2,ui)
                                                + my::vderiv_(1, 2) * derxjm_(1,2,2,ui));
          double v21 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(2, 0) * derxjm_(1,0,0,ui)
                                                + my::vderiv_(2, 1) * derxjm_(1,1,0,ui)
                                                + my::vderiv_(2, 2) * derxjm_(1,2,0,ui))
                       + my::derxy_(2,vi) * my::convvelint_(2) * (
                                                  my::vderiv_(2, 0) * derxjm_(1,0,2,ui)
                                                + my::vderiv_(2, 1) * derxjm_(1,1,2,ui)
                                                + my::vderiv_(2, 2) * derxjm_(1,2,2,ui));
          double v02 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(0, 0) * derxjm_(2,0,0,ui)
                                                + my::vderiv_(0, 1) * derxjm_(2,1,0,ui)
                                                + my::vderiv_(0, 2) * derxjm_(2,2,0,ui))
                       + my::derxy_(1,vi) * my::convvelint_(1) * (
                                                  my::vderiv_(0, 0) * derxjm_(2,0,1,ui)
                                                + my::vderiv_(0, 1) * derxjm_(2,1,1,ui)
                                                + my::vderiv_(0, 2) * derxjm_(2,2,1,ui));
          double v12 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(1, 0) * derxjm_(2,0,0,ui)
                                                + my::vderiv_(1, 1) * derxjm_(2,1,0,ui)
                                                + my::vderiv_(1, 2) * derxjm_(2,2,0,ui))
                       + my::derxy_(1,vi) * my::convvelint_(1) * (
                                                  my::vderiv_(1, 0) * derxjm_(2,0,1,ui)
                                                + my::vderiv_(1, 1) * derxjm_(2,1,1,ui)
                                                + my::vderiv_(1, 2) * derxjm_(2,2,1,ui));
          double v22 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(2, 0) * derxjm_(2,0,0,ui)
                                                + my::vderiv_(2, 1) * derxjm_(2,1,0,ui)
                                                + my::vderiv_(2, 2) * derxjm_(2,2,0,ui))
                       + my::derxy_(1,vi) * my::convvelint_(1) * (
                                                  my::vderiv_(2, 0) * derxjm_(2,0,1,ui)
                                                + my::vderiv_(2, 1) * derxjm_(2,1,1,ui)
                                                + my::vderiv_(2, 2) * derxjm_(2,2,1,ui));

          ecoupl_p(vi, ui * 3 + 0) += v * (v00 + v10 + v20);
          ecoupl_p(vi, ui * 3 + 1) += v * (v01 + v11 + v21);
          ecoupl_p(vi, ui * 3 + 2) += v * (v02 + v12 + v22);
        }
      }
    }

  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::LinMeshMotion_2D_OD(
    LINALG::Matrix<my::nsd_ * my::nen_, my::nsd_ * my::nen_>&         ecoupl_u,
    const double &                                                    dphi_dp,
    const double &                                                    dphi_dJ,
    const double &                                                    refporositydot,
    const double &                                                    timefac,
    const double &                                                    timefacfac)
{

  double addstab = 0.0;
  if(my::fldpara_->RStab() != INPAR::FLUID::reactive_stab_none)
  {
    if (my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
      addstab = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::tau_(1);
    else
    {
      dserror("Is this factor correct? Check for bugs!");
      //addstab = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::fldpara_->AlphaF()*fac3;
    }
  }

  //*************************** linearisation of mesh motion in momentum balance**********************************
  // mass
  if (my::fldparatimint_->IsStationary() == false)
  {
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = my::fac_ * my::densam_ * my::funct_(vi, 0) * (1.0 + addstab );
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_u(vi * 2    , ui * 2    ) += v * my::velint_(0) * my::derxy_(0, ui);
        ecoupl_u(vi * 2    , ui * 2 + 1) += v * my::velint_(0) * my::derxy_(1, ui);

        ecoupl_u(vi * 2 + 1, ui * 2    ) += v * my::velint_(1) * my::derxy_(0, ui);
        ecoupl_u(vi * 2 + 1, ui * 2 + 1) += v * my::velint_(1) * my::derxy_(1, ui);
      }
    }
  }

  //rhs
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    double v = my::fac_ * my::funct_(vi, 0);
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_u(vi * 2    , ui * 2    ) += v * (- my::rhsmom_(0) * my::fldparatimint_->Dt()
                                                     * my::fldparatimint_->Theta()) * my::derxy_(0, ui);
      ecoupl_u(vi * 2    , ui * 2 + 1) += v * (- my::rhsmom_(0)
                                                     * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(1, ui);

      ecoupl_u(vi * 2 + 1, ui * 2    ) += v * (- my::rhsmom_(1)
                                           * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(0, ui);
      ecoupl_u(vi * 2 + 1, ui * 2 + 1) += v * (- my::rhsmom_(1)
                                           * my::fldparatimint_->Dt() * my::fldparatimint_->Theta()) * my::derxy_(1, ui);
    }
  }

  //---------reaction term (darcy term)
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    //double v = timefacfac * my::funct_(vi, 0) * my::reacoeff_ * (1.0 + addstab );
    double v = timefacfac * my::funct_(vi, 0) * (1.0 + addstab );
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_u(vi * 2    , ui * 2    ) += v * reaconvel_(0) * my::derxy_(0, ui);
      ecoupl_u(vi * 2    , ui * 2 + 1) += v * reaconvel_(0) * my::derxy_(1, ui);

      ecoupl_u(vi * 2 + 1, ui * 2    ) += v * reaconvel_(1) * my::derxy_(0,ui);
      ecoupl_u(vi * 2 + 1, ui * 2 + 1) += v * reaconvel_(1) * my::derxy_(1, ui);
    }
  }

  //---------------convective term

  for (int vi=0; vi<my::nen_; ++vi)
  {
    const int tvi  = 2*vi;
    const int tvip = tvi+1;
    const double v = my::densaf_*timefacfac/my::det_*my::funct_(vi) * (1.0 + addstab );
    for (int ui=0; ui<my::nen_; ++ui)
    {
      const int tui  = 2*ui;
      const int tuip = tui+1;

      ecoupl_u(tvi , tui ) += v*(
      + my::convvelint_(1)*(-my::vderiv_(0, 0)*my::deriv_(1,ui) + my::vderiv_(0, 1)*my::deriv_(0,ui))
      );

      ecoupl_u(tvi , tuip) += v*(
      + my::convvelint_(0)*( my::vderiv_(0, 0)*my::deriv_(1,ui) - my::vderiv_(0, 1)*my::deriv_(0,ui))
      );

      ecoupl_u(tvip, tui ) += v*(
      + my::convvelint_(1)*(-my::vderiv_(1, 0)*my::deriv_(1,ui) + my::vderiv_(1, 1)*my::deriv_(0,ui))
      );

      ecoupl_u(tvip, tuip) += v*(
      + my::convvelint_(0)*( my::vderiv_(1, 0)*my::deriv_(1,ui) - my::vderiv_(1, 1)*my::deriv_(0,ui))
      );
    }
  }

  // pressure
  for (int vi=0; vi<my::nen_; ++vi)
  {
    const int tvi  = 2*vi;
    const int tvip = tvi+1;
    const double v = press_*timefacfac/my::det_;
    for (int ui=0; ui<my::nen_; ++ui)
    {
      const int tui = 2*ui;
      ecoupl_u(tvi,  tui + 1) -= v*( my::deriv_(0, vi)*my::deriv_(1, ui) - my::deriv_(0, ui)*my::deriv_(1, vi)) ;
      ecoupl_u(tvip, tui    ) -= v*(-my::deriv_(0, vi)*my::deriv_(1, ui) + my::deriv_(0, ui)*my::deriv_(1, vi)) ;
    }
  }

  // //---------viscous term (brinkman term)

  if(my::visceff_)
  {

    // part 1: derivative of det

    double v = my::visceff_*timefac*my::fac_ * (1.0 + addstab );
    for (int ui=0; ui<my::nen_; ++ui)
    {
      double derinvJ0 = -v*(my::deriv_(0,ui)*my::xji_(0,0) + my::deriv_(1,ui)*my::xji_(0,1) );
      double derinvJ1 = -v*(my::deriv_(0,ui)*my::xji_(1,0) + my::deriv_(1,ui)*my::xji_(1,1) );
      for (int vi=0; vi<my::nen_; ++vi)
      {
        double visres0 =   2.0*my::derxy_(0, vi)* my::vderxy_(0, 0)
                           +     my::derxy_(1, vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0)) ;
        double visres1 =         my::derxy_(0, vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
                           + 2.0*my::derxy_(1, vi)* my::vderxy_(1, 1) ;

        ecoupl_u(vi*2    , ui*2    ) += derinvJ0*visres0;
        ecoupl_u(vi*2 + 1, ui*2    ) += derinvJ0*visres1;

        ecoupl_u(vi*2    , ui*2 + 1) += derinvJ1*visres0;
        ecoupl_u(vi*2 + 1, ui*2 + 1) += derinvJ1*visres1;

        double visres0_poro =     2.0*refgrad_porosity_(0)*my::funct_(vi)* my::vderxy_(0, 0)
                                +     refgrad_porosity_(1)*my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0));
        double visres1_poro =         refgrad_porosity_(0)*my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
                                + 2.0*refgrad_porosity_(1)*my::funct_(vi)* my::vderxy_(1, 1) ;

        ecoupl_u(vi*2 + 0, ui*2 + 0) += -1.0 * derinvJ0/porosity_*visres0_poro;
        ecoupl_u(vi*2 + 1, ui*2 + 0) += -1.0 * derinvJ0/porosity_*visres1_poro;

        ecoupl_u(vi*2 + 0, ui*2 + 1) += -1.0 * derinvJ1/porosity_*visres0_poro;
        ecoupl_u(vi*2 + 1, ui*2 + 1) += -1.0 * derinvJ1/porosity_*visres1_poro;

//        double v0_poro =   +     my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
//                                                                               * (   refgrad_porosity_(0) * my::deriv_(1,ui)
//                                                                                   + refgrad_porosity_(1) * my::deriv_(0,ui)
//                                                                                 );
//        double v1_poro =         my::funct_(vi)*(my::vderxy_(0, 1) + my::vderxy_(1, 0))
//                                                                               * (   refgrad_porosity_(0) * my::deriv_(1,ui)
//                                                                                   + refgrad_porosity_(1) * my::deriv_(0,ui)
//                                                                                 );
//
//        ecoupl_u(vi * 2 + 2, ui * 2 + 0) += -1.0 * v/porosity_/my::det_ * v0_poro;
//        ecoupl_u(vi * 2 + 2, ui * 2 + 1) += -1.0 * v/porosity_/my::det_ * v1_poro;
      }
    }

    // part 2: derivative of viscosity residual

     v = timefacfac*my::visceff_/my::det_ * (1.0 + addstab );
    for (int ui=0; ui<my::nen_; ++ui)
    {
      double v0 = - my::vderiv_(0,0)*(my::xji_(1,0)*my::deriv_(1,ui) + my::xji_(1,0)*my::deriv_(1,ui) )
                  - my::vderiv_(0,1)*(my::xji_(1,1)*my::deriv_(1,ui) + my::xji_(1,0)*my::deriv_(0,ui) )
                  - my::vderiv_(1,0)*(my::deriv_(1,ui)*my::xji_(0,0))
                  - my::vderiv_(1,1)*(my::deriv_(1,ui)*my::xji_(0,1));
      double v1 = - my::vderiv_(0,0)*(my::xji_(1,0)*my::deriv_(0,ui) + my::xji_(1,1)*my::deriv_(1,ui) )
                  - my::vderiv_(0,1)*(my::xji_(1,1)*my::deriv_(0,ui) + my::xji_(1,1)*my::deriv_(0,ui) )
                  - my::vderiv_(1,0)*(my::deriv_(0,ui)*my::xji_(0,0))
                  - my::vderiv_(1,1)*(my::deriv_(0,ui)*my::xji_(0,1));

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*2 + 0, ui*2 + 0) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 )
                                     - v*my::funct_(vi)/porosity_*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 );
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(2*my::deriv_(1,ui)*my::xji_(0,0) + 2*my::deriv_(1,ui)*my::xji_(0,0) )
           - my::vderiv_(0,1)*(2*my::deriv_(0,ui)*my::xji_(0,0) + 2*my::deriv_(1,ui)*my::xji_(0,1) )
           - my::vderiv_(1,0)*(my::deriv_(1,ui)*my::xji_(1,0))
           - my::vderiv_(1,1)*(my::deriv_(0,ui)*my::xji_(1,0));
      v1 = - my::vderiv_(0,0)*(2*my::deriv_(0,ui)*my::xji_(0,0) + 2*my::deriv_(1,ui)*my::xji_(0,1) )
           - my::vderiv_(0,1)*(2*my::deriv_(0,ui)*my::xji_(0,1) + 2*my::deriv_(0,ui)*my::xji_(0,1) )
           - my::vderiv_(1,0)*(my::deriv_(1,ui)*my::xji_(1,1))
           - my::vderiv_(1,1)*(my::deriv_(0,ui)*my::xji_(1,1));

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*2 + 0, ui*2 + 1) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 )
                                     - v*my::funct_(vi)/porosity_*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 );
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(my::deriv_(1,ui)*my::xji_(0,0))
           - my::vderiv_(0,1)*(my::deriv_(0,ui)*my::xji_(0,0))
           - my::vderiv_(1,0)*(2*my::xji_(1,0)*my::deriv_(1,ui) + 2*my::xji_(1,0)*my::deriv_(1,ui) )
           - my::vderiv_(1,1)*(2*my::xji_(1,1)*my::deriv_(1,ui) + 2*my::xji_(1,0)*my::deriv_(0,ui) );
      v1 = - my::vderiv_(0,0)*(my::deriv_(1,ui)*xji_01)
           - my::vderiv_(0,1)*(my::deriv_(0,ui)*xji_01)
           - my::vderiv_(1,0)*(2*my::xji_(1,0)*my::deriv_(0,ui) + 2*my::xji_(1,1)*my::deriv_(1,ui) )
           - my::vderiv_(1,1)*(2*my::xji_(1,1)*my::deriv_(0,ui) + 2*my::xji_(1,1)*my::deriv_(0,ui) );

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*2 + 1, ui*2 + 0) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 )
                                     - v*my::funct_(vi)/porosity_*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 );
      }

      ////////////////////////////////////////////////////////////////

      v0 = - my::vderiv_(0,0)*(my::deriv_(1,ui)*my::xji_(1,0))
           - my::vderiv_(0,1)*(my::deriv_(1,ui)*my::xji_(1,1))
           - my::vderiv_(1,0)*(my::xji_(0,0)*my::deriv_(1,ui) + my::xji_(0,0)*my::deriv_(1,ui) )
           - my::vderiv_(1,1)*(my::xji_(0,1)*my::deriv_(1,ui) + my::xji_(0,0)*my::deriv_(0,ui) );
      v1 = - my::vderiv_(0,0)*(my::deriv_(0,ui)*my::xji_(1,0))
           - my::vderiv_(0,1)*(my::deriv_(0,ui)*my::xji_(1,1))
           - my::vderiv_(1,0)*(my::xji_(0,0)*my::deriv_(0,ui) + my::xji_(0,1)*my::deriv_(1,ui) )
           - my::vderiv_(1,1)*(my::xji_(0,1)*my::deriv_(0,ui) + my::xji_(0,1)*my::deriv_(0,ui) );

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl_u(vi*2 + 1, ui*2 + 1) +=   v*(my::deriv_(0,vi)*v0 + my::deriv_(1,vi)*v1 )
                                     - v*my::funct_(vi)/porosity_*(refgrad_porosity_(0)*v0 + refgrad_porosity_(1)*v1 );
      }

    }
  }//if(my::visceff_)

  //*************************** ReacStab**********************************
  if(my::fldpara_->RStab() != INPAR::FLUID::reactive_stab_none)
  {
    // pressure;
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v =  my::funct_(vi) * timefacfac / my::det_ * addstab;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_u(vi * 2 + 1, ui * 2) += v * ( - refgradp_(0) * my::deriv_(1, ui)
                                              + refgradp_(1) * my::deriv_(0, ui));

        ecoupl_u(vi * 2 + 0, ui * 2 + 1) += v * (   refgradp_(0) * my::deriv_(1, ui)
                                                  - refgradp_(1) * my::deriv_(0, ui));
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::LinMeshMotion_2D_Pres_OD(
    LINALG::Matrix< my::nen_, my::nsd_ * my::nen_>&                   ecoupl_p,
    const double &                                                    dphi_dp,
    const double &                                                    dphi_dJ,
    const double &                                                    refporositydot,
    const double &                                                    timefacfac  )
{

  if (my::fldparatimint_->IsStationary() == false)
  {
    // dphi_dp*dp/dt
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = my::fac_ * my::funct_(vi, 0) * (   dphi_dp * press_ )
                 + timefacfac * my::funct_(vi, 0) * refporositydot ;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_p(vi, ui * 2    ) += v * my::derxy_(0, ui);
        ecoupl_p(vi, ui * 2 + 1) += v * my::derxy_(1, ui);
      }
    }
  }

  // rhs
  for (int vi = 0; vi < my::nen_; ++vi)
  {
    double v = -1.0 * timefacfac * my::funct_(vi, 0) *  dphi_dp*my::rhscon_;
    for (int ui = 0; ui < my::nen_; ++ui)
    {
      ecoupl_p(vi, ui * 2    ) += v * my::derxy_(0, ui);
      ecoupl_p(vi, ui * 2 + 1) += v * my::derxy_(1, ui);
    }
  }

  const double timefacfac_det = timefacfac / my::det_;
  if( static_cast<DRT::ELEMENTS::FluidEleParameterPoro*>(my::fldpara_)->PoroContiPartInt() == false )
  {

    // (porosity)*div u
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = timefacfac_det * my::funct_(vi, 0) * porosity_;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_p(vi, ui * 2    ) += v * (- my::vderiv_(1, 0) * my::deriv_(1,ui)
                                         + my::vderiv_(1, 1) * my::deriv_(0,ui));

        ecoupl_p(vi, ui * 2 + 1) += v * (+ my::vderiv_(0, 0) * my::deriv_(1,ui)
                                         - my::vderiv_(0, 1) * my::deriv_(0,ui));
      }
    }


    // (dphi_dJ*J_)*div vs
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = timefacfac_det * my::funct_(vi, 0) * dphi_dJ * J_;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_p(vi, ui * 2    ) += v * (- gridvelderiv_(1, 0) * my::deriv_(1,ui)
                                         + gridvelderiv_(1, 1) * my::deriv_(0,ui));

        ecoupl_p(vi, ui * 2 + 1) += v * (+ gridvelderiv_(0, 0) * my::deriv_(1,ui)
                                         - gridvelderiv_(0, 1) * my::deriv_(0,ui));
      }
    }

    //-----------(u-vs)grad(phi)

    for (int ui = 0; ui < my::nen_; ++ui)
    {
      double v00 = + (my::velint_(1) - gridvelint_(1)) * ( - refgrad_porosity_(0) * my::deriv_(1,ui)
                                                           + refgrad_porosity_(1) * my::deriv_(0,ui) );
      double v01 = + (my::velint_(0) - gridvelint_(0)) * ( + refgrad_porosity_(0) * my::deriv_(1,ui)
                                                           - refgrad_porosity_(1) * my::deriv_(0,ui) );

      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v = timefacfac_det * my::funct_(vi);

        ecoupl_p(vi, ui * 2    ) += v * v00;
        ecoupl_p(vi, ui * 2 + 1) += v * v01;
      }
    }

  }
  else
  {
    // (dphi_dJ*J+phi)*div vs
    for (int vi = 0; vi < my::nen_; ++vi)
    {
      double v = timefacfac_det * my::funct_(vi, 0) * (dphi_dJ * J_+porosity_);
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        ecoupl_p(vi, ui * 2    ) += v * (  - gridvelderiv_(1, 0) * my::deriv_(1,ui)
                                           + gridvelderiv_(1, 1) * my::deriv_(0,ui));

        ecoupl_p(vi, ui * 2 + 1) += v * (  + gridvelderiv_(0, 0) * my::deriv_(1,ui)
                                           - gridvelderiv_(0, 1) * my::deriv_(0,ui));
      }
    }

    //----------- phi * (u-vs)grad(vi)

    double v00 = -1.0 * timefacfac_det * porosity_ * (my::velint_(1) - gridvelint_(1));
    double v01 = -1.0 * timefacfac_det * porosity_ * (my::velint_(0) - gridvelint_(0));

    for (int ui = 0; ui < my::nen_; ++ui)
    {
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        ecoupl_p(vi, ui * 2    ) += v00 * ( - my::deriv_(0,vi) * my::deriv_(1,ui)
                                            + my::deriv_(1,vi) * my::deriv_(0,ui) );
        ecoupl_p(vi, ui * 2 + 1) += v01 * ( + my::deriv_(0,vi) * my::deriv_(1,ui)
                                            - my::deriv_(1,vi) * my::deriv_(0,ui) );
      }
    }

  }//partial integration
  //-------------------
  if (my::fldpara_->PSPG())
  {
    double v00 = -1.0 * timefacfac_det * my::sgvelint_(1);
    double v01 = -1.0 * timefacfac_det * my::sgvelint_(0);

    for (int ui = 0; ui < my::nen_; ++ui)
    {
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        ecoupl_p(vi, ui * 2    ) += v00 * ( - my::deriv_(0,vi) * my::deriv_(1,ui)
                                            + my::deriv_(1,vi) * my::deriv_(0,ui) );
        ecoupl_p(vi, ui * 2 + 1) += v01 * ( + my::deriv_(0,vi) * my::deriv_(1,ui)
                                            - my::deriv_(1,vi) * my::deriv_(0,ui) );
      }
    }

    double scal_grad_q=0.0;

    if(my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
    {
      scal_grad_q=my::tau_(1);
    }
    else
    {
      scal_grad_q=0.0;//my::fldpara_->AlphaF()*fac3;
    }

    //pressure
    {
      const double v = timefacfac_det * scal_grad_q;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        for (int vi = 0; vi < my::nen_; ++vi)
        {

          double v00 = + my::derxy_(1,vi) * (- refgradp_(0) * my::deriv_(1,ui)
                                             + refgradp_(1) * my::deriv_(0,ui));
          double v01 = + my::derxy_(0,vi) * (  refgradp_(0) * my::deriv_(1,ui)
                                             - refgradp_(1) * my::deriv_(0,ui));

          ecoupl_p(vi, ui * 2 + 0) += v * v00;
          ecoupl_p(vi, ui * 2 + 1) += v * v01;
        }
      }

      LINALG::Matrix<my::nen_,1> temp;
      temp.MultiplyTN(my::derxy_,my::gradp_);
      for (int vi = 0; vi < my::nen_; ++vi)
      {
        double v3 = -1.0 * timefacfac * scal_grad_q * temp(vi);
        for (int ui = 0; ui < my::nen_; ++ui)
        {
          ecoupl_p(vi, ui * 2)     += v3 * my::derxy_(0, ui);
          ecoupl_p(vi, ui * 2 + 1) += v3 * my::derxy_(1, ui);
        }
      }

    }

    //convective term

    {
      const double v = my::densaf_*timefacfac_det * scal_grad_q;
      for (int ui = 0; ui < my::nen_; ++ui)
      {
        for (int vi = 0; vi < my::nen_; ++vi)
        {
          double v00 = + my::derxy_(1,vi) * my::convvelint_(1) * (
                                                - my::vderiv_(0, 0) * my::deriv_(1,ui)
                                                + my::vderiv_(0, 1) * my::deriv_(0,ui));
          double v10 = + my::derxy_(1,vi) *  my::convvelint_(1) * (
                                                  my::vderiv_(1, 0) * my::deriv_(1,ui)
                                                - my::vderiv_(1, 1) * my::deriv_(0,ui));
          double v01 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                - my::vderiv_(0, 0) * my::deriv_(1,ui)
                                                + my::vderiv_(0, 1) * my::deriv_(0,ui));
          double v11 = + my::derxy_(0,vi) * my::convvelint_(0) * (
                                                  my::vderiv_(1, 0) * my::deriv_(1,ui)
                                                - my::vderiv_(1, 1) * my::deriv_(0,ui));

          ecoupl_p(vi, ui * 2 + 0) += v * (v00 + v10 );
          ecoupl_p(vi, ui * 2 + 1) += v * (v01 + v11 );
        }
      }
    }

  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::PSPG(
    LINALG::Matrix<my::nen_, my::nen_*my::nsd_> &         estif_q_u,
    LINALG::Matrix<my::nen_,my::nen_> &                   ppmat,
    LINALG::Matrix<my::nen_,1> &                          preforce,
    const LINALG::Matrix<my::nsd_*my::nsd_,my::nen_> &    lin_resM_Du,
    const LINALG::Matrix<my::nsd_,my::nen_> &             lin_resM_Dp,
    const double &                                        dphi_dp,
    const double &                                        fac3,
    const double &                                        timefacfac,
    const double &                                        timefacfacpre,
    const double &                                        rhsfac)
{
  // conservative, stabilization terms are neglected (Hughes)

  /* pressure stabilisation:                                            */
  /*
              /                 \
             |  ~n+af            |
           - |  u     , nabla q  |
             |                   |
              \                 /
  */

    double scal_grad_q=0.0;

    if(my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
    {
      scal_grad_q=my::tau_(1);
    }
    else
    {
      scal_grad_q=my::fldparatimint_->AlphaF()*fac3;
    }

    /* pressure stabilisation: inertia if not stationary*/
    /*
              /                  \
             |                    |
             |  rho*Du , nabla q  |
             |                    |
              \                  /
    */
    /* pressure stabilisation: convection, convective part */
    /*
              /                                   \
             |  /       n+1       \                |
             | |   rho*u   o nabla | Du , nabla q  |
             |  \      (i)        /                |
              \                                   /
    */
    /* pressure stabilisation: convection, reactive part if Newton */
    /*
              /                                   \
             |  /                \   n+1           |
             | |   rho*Du o nabla | u     , grad q |
             |  \                /   (i)           |
              \                                   /
    */
    /* pressure stabilisation: reaction if included */
    /*
              /                     \
             |                      |
             |  sigma*Du , nabla q  |
             |                      |
              \                    /
    */
    /* pressure stabilisation: viscosity (-L_visc_u) */
    /*
              /                              \
             |               /  \             |
         mu  |  nabla o eps | Du | , nabla q  |
             |               \  /             |
              \                              /
    */

    if (my::is_higher_order_ele_ || my::fldpara_->IsNewton())
    {
      for(int jdim=0;jdim<my::nsd_;++jdim)
      {
        for (int ui=0; ui<my::nen_; ++ui)
        {
          const int fui_p_jdim   = my::nsd_*ui + jdim;

          for(int idim=0;idim<my::nsd_;++idim)
          {
            const int nsd_idim=my::nsd_*idim;

            for (int vi=0; vi<my::nen_; ++vi)
            {
              const double temp_vi_idim=my::derxy_(idim,vi)*scal_grad_q;

              estif_q_u(vi,fui_p_jdim) += lin_resM_Du(nsd_idim+jdim,ui)*temp_vi_idim;
            } // jdim
          } // vi
        } // ui
      } //idim
    } // end if (is_higher_order_ele_) or (newton_)
    else
    {
      for (int vi=0; vi<my::nen_; ++vi)
      {
        for(int idim=0;idim<my::nsd_;++idim)
        {
          const int nsd_idim=my::nsd_*idim;

          const double temp_vi_idim=my::derxy_(idim, vi)*scal_grad_q;

          for (int ui=0; ui<my::nen_; ++ui)
          {
            const int fui_p_idim   = my::nsd_*ui + idim;

            estif_q_u(vi,fui_p_idim) += lin_resM_Du(nsd_idim+idim,ui)*temp_vi_idim;
          } // vi
        } // ui
      } //idim
    } // end if not (is_higher_order_ele_) nor (newton_)


    for (int ui=0; ui<my::nen_; ++ui)
    {
      for (int vi=0; vi<my::nen_; ++vi)
      {
        /* pressure stabilisation: pressure( L_pres_p) */
        /*
             /                    \
            |                      |
            |  nabla Dp , nabla q  |
            |                      |
             \                    /
        */
        double sum = 0.;
        double sum2 = 0.;
        for (int idim = 0; idim < my::nsd_; ++idim)
        {
          sum += my::derxy_(idim,ui) * my::derxy_(idim,vi);
          sum2 += lin_resM_Dp(idim,ui) * my::derxy_(idim,vi);
        }

        ppmat(vi,ui) += timefacfacpre*scal_grad_q*sum;
        ppmat(vi,ui) += scal_grad_q * sum2;
      } // vi
    }  // ui

    {
      const double v1 = -timefacfacpre * dtaudphi_(1)/scal_grad_q * dphi_dp;
      for (int ui=0; ui<my::nen_; ++ui)
      {
        for (int idim = 0; idim <my::nsd_; ++idim)
        {
          const double v= v1 * my::sgvelint_(idim) * my::funct_(ui);

          for (int vi=0; vi<my::nen_; ++vi)
          {
            ppmat(vi,ui) += v * my::derxy_(idim, vi);
          } // vi
        } // end for(idim)
      }  // ui
    }

    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      const double temp = rhsfac*my::sgvelint_(idim);

      for (int vi=0; vi<my::nen_; ++vi)
      {
        // pressure stabilisation
        preforce(vi) -= -1.0 * temp*my::derxy_(idim, vi);
      }
    } // end for(idim)

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeFDerivative(
                              const LINALG::Matrix<my::nsd_, my::nen_>&    edispnp,
                              const LINALG::Matrix<my::nsd_,my::nsd_>&     defgrd_inv,
                              LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_>&  F_x,
                              LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_>&  F_X)
{
  F_X.Clear();

  for(int i=0; i<my::nsd_; i++)
    for(int j=0; j<my::nsd_; j++)
      for(int k=0; k<my::nsd_; k++)
        for(int n=0; n<my::nen_; n++)
          F_X(i*my::nsd_+j, k) += N_XYZ2full_(j*my::nsd_+k,n)*edispnp(i,n);

  F_x.Multiply(F_X,defgrd_inv);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeGradients(
                      const double&                                      dphidp,
                      const double&                                      dphidJ,
                      const LINALG::Matrix<my::nsd_*my::nsd_,1>&         defgrd_IT_vec,
                      const LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_>&  F_x,
                      const LINALG::Matrix<my::nen_,1>*                  eporositynp,
                      LINALG::Matrix<my::nsd_,1>&                        gradJ)
{
  //---------------------------  dJ/dx = dJ/dF : dF/dx = JF^-T : dF/dx
  gradJ.MultiplyTN(J_, F_x,defgrd_IT_vec );

  ComputePorosityGradient(dphidp,dphidJ,gradJ,eporositynp);

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputePorosityGradient(
                        const double&                                      dphidp,
                        const double&                                      dphidJ,
                        const LINALG::Matrix<my::nsd_,1>&                  gradJ,
                        const LINALG::Matrix<my::nen_,1>*                  eporositynp)
{
  //if( (my::fldpara_->PoroContiPartInt() == false) or my::visceff_)
  {
    //--------------------- current porosity gradient
    for (int idim=0; idim<my::nsd_; ++idim)
      grad_porosity_(idim) = dphidp*my::gradp_(idim)+dphidJ*gradJ(idim);

    refgrad_porosity_.Multiply(my::xjm_,grad_porosity_);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeLinearization(
                                  const double&                                      dphi_dp,
                                  const double&                                      dphi_dpp,
                                  const double&                                      dphi_dJdp,
                                  const LINALG::Matrix<my::nsd_,1>&                  gradJ,
                                  LINALG::Matrix<my::nsd_,my::nen_>&                 dgradphi_dp)
{
  if( (static_cast<DRT::ELEMENTS::FluidEleParameterPoro*>(my::fldpara_)->PoroContiPartInt() == false) or my::visceff_)
  {
    //--linearization of porosity gradient w.r.t. pressure at gausspoint
    //d(grad(phi))/dp = dphi/(dJdp)* dJ/dx + d^2phi/(dp)^2 * dp/dx + dphi/dp* N,x
    dgradphi_dp.MultiplyNT(dphi_dJdp,gradJ,my::funct_ );
    dgradphi_dp.MultiplyNT(dphi_dpp, my::gradp_,my::funct_,1.0);
    dgradphi_dp.Update(dphi_dp, my::derxy_,1.0);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeLinearizationOD(
                            const double&                                      dphi_dJ,
                            const double&                                      dphi_dJJ,
                            const double&                                      dphi_dJp,
                            const LINALG::Matrix<my::nsd_,my::nsd_>&           defgrd_inv,
                            const LINALG::Matrix<my::nsd_*my::nsd_,1>&         defgrd_IT_vec,
                            const LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_>&  F_x,
                            const LINALG::Matrix<my::nsd_*my::nsd_,my::nsd_>&  F_X,
                            const LINALG::Matrix<my::nsd_,1>&                  gradJ,
                            LINALG::Matrix<1,my::nsd_*my::nen_>&               dJ_dus,
                            LINALG::Matrix<1,my::nsd_*my::nen_>&               dphi_dus,
                            LINALG::Matrix<my::nsd_,my::nen_*my::nsd_>&        dgradphi_dus)
{
  //------------------------------------------------dJ/dus = dJ/dF : dF/dus = J * F^-T . N_X = J * N_x
  for (int i=0; i<my::nen_; i++)
    for (int j=0; j<my::nsd_; j++)
      dJ_dus(j+i*my::nsd_)=J_*my::derxy_(j,i);

  //--------------------- linearization of porosity w.r.t. structure displacements
  dphi_dus.Update( dphi_dJ , dJ_dus );

  if( (static_cast<DRT::ELEMENTS::FluidEleParameterPoro*>(my::fldpara_)->PoroContiPartInt() == false) or my::visceff_)
  {
    //---------------------d(gradJ)/dus =  dJ/dus * F^-T . : dF/dx + J * dF^-T/dus : dF/dx + J * F^-T : N_X_x

    //dF^-T/dus : dF/dx = - (F^-1. dN/dx . u_s)^T  : dF/dx
    LINALG::Matrix<my::nsd_,my::nsd_*my::nen_> dFinvdus_dFdx(true);
    for (int i=0; i<my::nsd_; i++)
      for (int n =0; n<my::nen_; n++)
        for(int j=0; j<my::nsd_; j++)
        {
          const int gid = my::nsd_ * n +j;
          for (int k=0; k<my::nsd_; k++)
            for(int p=0; p<my::nsd_; p++)
              dFinvdus_dFdx(p, gid) += -defgrd_inv(i,j) * my::derxy_(k,n) * F_x(k*my::nsd_+i,p);
        }

    //F^-T : d(dF/dx)/dus =  F^-T : (N,XX * F^ -1 + dF/dX * F^-1 * N,x)
    LINALG::Matrix<my::nsd_,my::nsd_*my::nen_>        FinvT_dFx_dus(true);

    for (int n =0; n<my::nen_; n++)
      for(int j=0; j<my::nsd_; j++)
      {
        const int gid = my::nsd_ * n +j;
        for(int i=0; i<my::nsd_; i++)
          for(int k=0; k<my::nsd_; k++)
            for(int p=0; p<my::nsd_; p++)
            {
              FinvT_dFx_dus(p, gid) +=   defgrd_inv(i,j) * N_XYZ2full_(i*my::nsd_+k,n) * defgrd_inv(k,p) ;
              for(int l=0; l<my::nsd_; l++)
                FinvT_dFx_dus(p, gid) += - defgrd_inv(i,l) * F_X(i*my::nsd_+l,k) * defgrd_inv(k,j) * my::derxy_(p,n) ;
            }
      }

    //----d(gradJ)/dus =  dJ/dus * F^-T . : dF/dx + J * dF^-T/dus : dF/dx + J * F^-T : N_X_x
    LINALG::Matrix<1,my::nsd_> temp;
    temp.MultiplyTN( defgrd_IT_vec, F_x);

    //----d(gradJ)/dus =  dJ/dus * F^-T . : dF/dx + J * dF^-T/dus : dF/dx + J * F^-T : N_X_x
    LINALG::Matrix<my::nsd_,my::nen_*my::nsd_> dgradJ_dus;

    dgradJ_dus.MultiplyTN(temp,dJ_dus);

    dgradJ_dus.Update(J_,dFinvdus_dFdx,1.0);

    dgradJ_dus.Update(J_,FinvT_dFx_dus,1.0);

    //------------------ d( grad(\phi) ) / du_s = d\phi/(dJ du_s) * dJ/dx+ d\phi/dJ * dJ/(dx*du_s) + d\phi/(dp*du_s) * dp/dx
    dgradphi_dus.Multiply(dphi_dJJ, gradJ ,dJ_dus);
    dgradphi_dus.Update(dphi_dJ, dgradJ_dus, 1.0);
    dgradphi_dus.Multiply(dphi_dJp, my::gradp_, dJ_dus, 1.0);
  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputePorosity(
                                          Teuchos::ParameterList& params,
                                          const double& press,
                                          const double& J,
                                          const int& gp,
                                          const LINALG::Matrix<my::nen_,1>&       shapfct,
                                          const LINALG::Matrix<my::nen_,1>*           myporosity,
                                          double& porosity,
                                          double* dphi_dp,
                                          double* dphi_dJ,
                                          double* dphi_dJdp,
                                          double* dphi_dJJ,
                                          double* dphi_dpp,
                                          bool save)
{
  so_interface_->ComputePorosity( params,
                              press,
                              J,
                              gp,
                              porosity,
                              dphi_dp,
                              dphi_dJ,
                              dphi_dJdp,
                              dphi_dJJ,
                              dphi_dpp,
                              save);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
double DRT::ELEMENTS::FluidEleCalcPoro<distype>::SetupMaterialDerivatives()
{
  //------------------------get determinant of Jacobian dX / ds
  // transposed jacobian "dX/ds"
  LINALG::Matrix<my::nsd_,my::nsd_> xjm0;
  xjm0.MultiplyNT(my::deriv_,xyze0_);

  // inverse of transposed jacobian "ds/dX"
  LINALG::Matrix<my::nsd_,my::nsd_> xji0(true);
  double det0= xji0.Invert(xjm0);

  // ----------------------compute derivatives N_XYZ_ at gp w.r.t. material coordinates
  N_XYZ_.Multiply(xji0,my::deriv_);

  if(my::is_higher_order_ele_)
  {
    // get the second derivatives of standard element at current GP w.r.t. XYZ
    DRT::UTILS::gder2<distype>(xjm0,N_XYZ_,my::deriv2_,xyze0_,N_XYZ2_);

    if(my::nsd_==3)
    {
      for (int n =0; n<my::nen_; n++)
      {
        N_XYZ2full_(0,n) = N_XYZ2_(0,n);
        N_XYZ2full_(1,n) = N_XYZ2_(3,n);
        N_XYZ2full_(2,n) = N_XYZ2_(4,n);

        N_XYZ2full_(3,n) = N_XYZ2_(3,n);
        N_XYZ2full_(4,n) = N_XYZ2_(1,n);
        N_XYZ2full_(5,n) = N_XYZ2_(5,n);

        N_XYZ2full_(6,n) = N_XYZ2_(4,n);
        N_XYZ2full_(7,n) = N_XYZ2_(5,n);
        N_XYZ2full_(8,n) = N_XYZ2_(2,n);
      }
    }
    else
    {
      for (int n =0; n<my::nen_; n++)
      {
        N_XYZ2full_(0,n) = N_XYZ2_(0,n);
        N_XYZ2full_(1,n) = N_XYZ2_(2,n);

        N_XYZ2full_(2,n) = N_XYZ2_(2,n);
        N_XYZ2full_(3,n) = N_XYZ2_(1,n);
      }
    }
  }
  else
  {
    N_XYZ2_.Clear();
    N_XYZ2full_.Clear();
  }

  return det0;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::GetStructMaterial()
{
  //access structure discretization
  Teuchos::RCP<DRT::Discretization> structdis = Teuchos::null;
  structdis = DRT::Problem::Instance()->GetDis("structure");
  //get corresponding structure element (it has the same global ID as the fluid element)
  DRT::Element* structele = structdis->gElement(my::eid_);
  if(structele == NULL)
    dserror("Fluid element %i not on local processor", my::eid_);

  so_interface_ = dynamic_cast<DRT::ELEMENTS::So_Poro_Interface*>(structele);
  if(so_interface_ == NULL)
    dserror("cast to so_interface failed!");

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ReacStab(
    LINALG::Matrix<my::nen_*my::nsd_,my::nen_*my::nsd_> &     estif_u,
    LINALG::Matrix<my::nen_*my::nsd_,my::nen_> &              estif_p_v,
    LINALG::Matrix<my::nsd_,my::nen_> &                       velforce,
    LINALG::Matrix<my::nsd_*my::nsd_,my::nen_> &              lin_resM_Du,
    const LINALG::Matrix<my::nsd_,my::nen_> &                 lin_resM_Dp,
    const double &                                            dphi_dp,
    const double &                                            timefacfac,
    const double &                                            timefacfacpre,
    const double &                                            rhsfac,
    const double &                                            fac3)
{
  my::ReacStab(estif_u,
           estif_p_v,
           velforce,
           lin_resM_Du,
           timefacfac,
           timefacfacpre,
           rhsfac,
           fac3);

  double reac_tau;
  if (my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
    reac_tau = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::tau_(1);
  else
  {
    dserror("Is this factor correct? Check for bugs!");
    reac_tau = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::fldparatimint_->AlphaF()*fac3;
  }


  for (int vi=0; vi<my::nen_; ++vi)
  {
    const double v = reac_tau*my::funct_(vi);

    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      const int fvi = my::nsd_*vi + idim;

      for (int ui=0; ui<my::nen_; ++ui)
      {
        estif_p_v(fvi,ui) += v*lin_resM_Dp(idim,ui);
      }
    }
  }  // end for(idim)

  {//linearization of stabilization parameter w.r.t. fluid pressure
    const double v = my::fldpara_->ViscReaStabFac()* dphi_dp * ( my::reacoeff_*dtaudphi_(1)/my::tau_(1) + my::reacoeff_/porosity_ );
    for (int vi=0; vi<my::nen_; ++vi)
    {
      const double w = -1.0 * v * my::funct_(vi) ;

      for (int idim = 0; idim <my::nsd_; ++idim)
      {
        const int fvi = my::nsd_*vi + idim;

        for (int ui=0; ui<my::nen_; ++ui)
        {
          estif_p_v(fvi,ui) += w*my::sgvelint_(idim)*my::funct_(ui);
        }
      }
    }  // end for(idim)
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::GetMaterialParamters(Teuchos::RCP<const MAT::Material>   material)
{
  if (my::fldpara_->MatGp())
  {
    Teuchos::RCP<const MAT::FluidPoro> actmat = Teuchos::rcp_static_cast<const MAT::FluidPoro>(material);
    if(actmat->MaterialType() != INPAR::MAT::m_fluidporo)
      dserror("invalid fluid material for poroelasticity");

    // set density at n+alpha_F/n+1 and n+alpha_M/n+1
    my::densaf_ = actmat->Density();
    my::densam_ = my::densaf_;
    my::densn_  = my::densaf_;

    // calculate reaction coefficient
    my::reacoeff_ = actmat->ComputeReactionCoeff()*porosity_;

    my::visceff_       = actmat->EffectiveViscosity();
  }
  else dserror("Fluid material parameters have to be evaluated at gauss point for porous flow!");
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeSpatialReactionTerms(
    Teuchos::RCP<const MAT::Material>        material,
    const LINALG::Matrix<my::nsd_,my::nsd_>& invdefgrd)
{
  Teuchos::RCP<const MAT::FluidPoro> actmat = Teuchos::rcp_static_cast<const MAT::FluidPoro>(material);

  //material reaction tensor = inverse material permeability
  actmat->ComputeReactionTensor(matreatensor_,J_,porosity_);

  //spatial reaction tensor = J * F^-T * material reaction tensor * F^-1
  LINALG::Matrix<my::nsd_,my::nsd_> temp(true);
  temp.Multiply(J_*porosity_,matreatensor_,invdefgrd);
  //temp.Multiply(matreatensor_,invdefgrd);
  reatensor_.MultiplyTN(invdefgrd,temp);
//  reatensor_.Update(porosity_,matreatensor_);

  reavel_.Multiply(reatensor_,my::velint_);
  reagridvel_.Multiply(reatensor_,gridvelint_);
  reaconvel_.Multiply(reatensor_,convel_);

  //linearisations of material reaction tensor
  actmat->ComputeLinMatReactionTensor(matreatensorlinporosity_,matreatensorlinJ_,J_,porosity_);

  LINALG::Matrix<my::nsd_,my::nsd_> lin_p_tmp_1;
  LINALG::Matrix<my::nsd_,my::nsd_> lin_p_tmp_2;

  lin_p_tmp_1.MultiplyTN(J_,invdefgrd,matreatensorlinporosity_);
  lin_p_tmp_2.Multiply(lin_p_tmp_1,invdefgrd);

  lin_p_vel_.Multiply(lin_p_tmp_2,my::velint_);
  lin_p_vel_grid_.Multiply(lin_p_tmp_2,gridvelint_);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeOldRHSAndSubgridScaleVelocity()
{
  //----------------------------------------------------------------------
  // computation of various residuals and residual-based values such as
  // the subgrid-scale velocity
  //----------------------------------------------------------------------
  // compute rhs for momentum equation and momentum residual
  // -> different for generalized-alpha and other time-integration schemes
  //GetResidualMomentumEq(eaccam,my::fldparatimint_->TimeFac());
  if (my::fldparatimint_->IsGenalpha())
  {
    // rhs of momentum equation: density*bodyforce at n+alpha_F
    my::rhsmom_.Update(my::densaf_,my::bodyforce_,0.0);

    // get acceleration at time n+alpha_M at integration point
    //my::accint_.Multiply(eaccam,my::funct_);

    // evaluate momentum residual once for all stabilization right hand sides
    for (int rr=0;rr<my::nsd_;++rr)
    {
      my::momres_old_(rr) = my::densam_*my::accint_(rr)+my::densaf_*my::conv_old_(rr)+my::gradp_(rr)
                       -2*my::visceff_*my::visc_old_(rr)+reaconvel_(rr)-my::densaf_*my::bodyforce_(rr);
    }
  }
  else
  {
    if (not my::fldparatimint_->IsStationary())
    {
      // rhs of instationary momentum equation:
      // density*theta*bodyforce at n+1 + density*(histmom/dt)
      //                                      f = rho * g
        //my::rhsmom_.Update((my::densn_/my::fldparatimint_->Dt()),my::histmom_,my::densaf_*my::fldparatimint_->Theta(),my::bodyforce_);
      my::rhsmom_.Update((my::densn_/my::fldparatimint_->Dt()/my::fldparatimint_->Theta()),my::histmom_,my::densaf_,my::bodyforce_);

      // compute instationary momentum residual:
      // momres_old = u_(n+1)/dt + theta ( ... ) - my::histmom_/dt - theta*my::bodyforce_
      for (int rr=0;rr<my::nsd_;++rr)
      {
        /*my::momres_old_(rr) = my::densaf_*my::velint_(rr)/my::fldparatimint_->Dt()
                           +my::fldparatimint_->Theta()*(my::densaf_*conv_old_(rr)+my::gradp_(rr)
                           -2*my::visceff_*visc_old_(rr)+my::reacoeff_*my::velint_(rr))-my::rhsmom_(rr);*/
        my::momres_old_(rr) = ((my::densaf_*my::velint_(rr)/my::fldparatimint_->Dt()
                         +my::fldparatimint_->Theta()*(my::densaf_*my::conv_old_(rr)+my::gradp_(rr)
                         -2*my::visceff_*my::visc_old_(rr)+reaconvel_(rr)))/my::fldparatimint_->Theta())-my::rhsmom_(rr);
     }
    }
    else
    {
      // rhs of stationary momentum equation: density*bodyforce
      //                                       f = rho * g
      my::rhsmom_.Update(my::densaf_,my::bodyforce_,0.0);

      // compute stationary momentum residual:
      for (int rr=0;rr<my::nsd_;++rr)
      {
        my::momres_old_(rr) = my::densaf_*my::conv_old_(rr)
                             +my::gradp_(rr)
                             -2*my::visceff_*my::visc_old_(rr)
                             +reaconvel_(rr)-my::rhsmom_(rr)
        ;
      }
    }
  }
  //-------------------------------------------------------
  // compute subgrid-scale velocity
  my::sgvelint_.Update(-my::tau_(1),my::momres_old_,0.0);
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeStabilizationParameters(const double& vol)
{

  // calculate stabilization parameters at integration point
  if (my::fldpara_->TauGp())
  {
    // check stabilization parameter definition for porous flow
    if (not (my::fldpara_->WhichTau() == INPAR::FLUID::tau_franca_madureira_valentin_badia_codina or
             my::fldpara_->WhichTau() == INPAR::FLUID::tau_franca_madureira_valentin_badia_codina_wo_dt or
             my::fldpara_->WhichTau() == INPAR::FLUID::tau_not_defined))
      dserror("incorrect definition of stabilization parameter for porous flow");

    /*
    This stabilization parameter is only intended to be used for
    (viscous-)reactive problems such as Darcy(-Stokes/Brinkman) problems.

    literature:
    1) L.P. Franca, A.L. Madureira, F. Valentin, Towards multiscale
       functions: enriching finite element spaces with local but not
       bubble-like functions, Comput. Methods Appl. Mech. Engrg. 194
       (2005) 3006-3021.
    2) S. Badia, R. Codina, Stabilized continuous and discontinuous
       Galerkin techniques for Darcy flow, Comput. Methods Appl.
       Mech. Engrg. 199 (2010) 1654-1667.

    */

    // get element-type constant for tau
    const double mk = DRT::ELEMENTS::MK<distype>();

    // total reaction coefficient sigma_tot: sum of "artificial" reaction
    // due to time factor and reaction coefficient
    double sigma_tot = my::reacoeff_;

    if (not my::fldparatimint_->IsStationary())
    {
      sigma_tot += 1.0/my::fldparatimint_->TimeFac();
    }

    // calculate characteristic element length
    double h_u  = 0.0;
    double h_p     = 0.0;
    my::CalcCharEleLength(vol,0.0,h_u,h_p);

    // various parameter computations for case with dt:
     // relating viscous to reactive part
     const double re11 = 2.0 * my::visceff_ / (mk * my::densaf_ * sigma_tot * DSQR(h_p));

     // respective "switching" parameter
     const double xi11 = std::max(re11,1.0);

    // constants c_u and c_p as suggested in Badia and Codina (2010), method A
    const double c_u = 4.0;
    const double c_p = 4.0;

    // tau_Mu not required for porous flow
    my::tau_(0) = 0.0;
    my::tau_(1) = DSQR(h_p)/(c_u*DSQR(h_p)*my::densaf_*sigma_tot*xi11+(2.0*my::visceff_/mk));
    my::tau_(2) = c_p*DSQR(h_p)*my::reacoeff_/porosity_;

    dtaudphi_(0)= 0.0;
    dtaudphi_(1)= -1.0 * my::tau_(1)*my::tau_(1)*c_u*my::densaf_*my::reacoeff_/porosity_ ;
    dtaudphi_(2)= 0.0;

  }
  else dserror("Fluid stabilization parameters have to be evaluated at gauss point for porous flow!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeOldRHSConti()
{
  double vel_grad_porosity = 0.0;
  for (int idim = 0; idim <my::nsd_; ++idim)
    vel_grad_porosity += grad_porosity_(idim)*my::velint_(idim);

  double    grad_porosity_gridvelint=0.0;
  for (int j =0; j< my::nsd_; j++)
    grad_porosity_gridvelint += grad_porosity_(j) * gridvelint_(j);

  if (my::fldparatimint_->IsStationary() == false)
  {

    // rhs of continuity equation
    my::rhscon_ = 1.0/my::fldparatimint_->Dt()/my::fldparatimint_->Theta() * histcon_;

    my::conres_old_ = my::fldparatimint_->Theta()*(my::vdiv_* porosity_ + vel_grad_porosity-grad_porosity_gridvelint)
                      + press_/my::fldparatimint_->Dt()/my::fldparatimint_->Theta() - my::rhscon_;
  }
  else
  {
    //no time derivatives -> no history
    my::rhscon_ = 0.0;

    my::conres_old_ = my::vdiv_* porosity_ + vel_grad_porosity;
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeLinResMDu(
    const double & timefacfac,
    LINALG::Matrix<my::nsd_*my::nsd_,my::nen_>&  lin_resM_Du)
{
  int idim_nsd_p_idim[my::nsd_];
  for (int idim = 0; idim <my::nsd_; ++idim)
  {
    idim_nsd_p_idim[idim]=idim*my::nsd_+idim;
  }

  if (my::fldparatimint_->IsStationary() == false)
  {
    const double fac_densam=my::fac_*my::densam_;

    for (int ui=0; ui<my::nen_; ++ui)
    {
      const double v=fac_densam*my::funct_(ui);

      for (int idim = 0; idim <my::nsd_; ++idim)
      {
        //TODO : check genalpha case
        lin_resM_Du(idim_nsd_p_idim[idim],ui)+=v;
      }
    }
  }

 //reactive part
  //const double fac_reac=timefacfac*my::reacoeff_;
  for (int ui=0; ui<my::nen_; ++ui)
  {
    //const double v=fac_reac*my::funct_(ui);
    const double v=timefacfac*my::funct_(ui);

    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      for (int jdim = 0; jdim <my::nsd_; ++jdim)
        lin_resM_Du(idim*my::nsd_+jdim,ui)+=v*reatensor_(idim,jdim);
    }
  }

  //convective ALE-part
  const double timefacfac_densaf=timefacfac*my::densaf_;

  for (int ui=0; ui<my::nen_; ++ui)
  {
    const double v=timefacfac_densaf*my::conv_c_(ui);

    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      lin_resM_Du(idim_nsd_p_idim[idim],ui)+=v;
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeLinResMDp(
    const double & timefacfacpre,
    const double & dphi_dp,
    LINALG::Matrix<my::nsd_,my::nen_>&  lin_resM_Dp)
{
  /* poroelasticity pressure term */
  /*
       /                           \      /                            \
      |         n+1                 |     |         n+1                 |
      |  sigma*u  * dphi/dp*Dp , v  |  -  |  sigma*vs  * dphi/dp*Dp , v |
      |         (i)                 |     |         (i)                 |
       \                           /       \                           /
  */

  for (int ui=0; ui<my::nen_; ++ui)
  {
     //const double w = my::funct_(ui)*timefacfacpre*my::reacoeff_/porosity_*dphi_dp;
    const double w = my::funct_(ui)*timefacfacpre*dphi_dp/porosity_;
       for (int idim = 0; idim <my::nsd_; ++idim)
       {
         lin_resM_Dp(idim,ui) +=   w * reavel_(idim);
       }
   }
  if (!const_permeability_) //check if derivatives of reaction tensor are zero --> significant speed up
  {
    for (int ui=0; ui<my::nen_; ++ui)
    {
      const double w1 = my::funct_(ui)*timefacfacpre*dphi_dp*porosity_;
      for (int idim = 0; idim <my::nsd_; ++idim)
      {
        lin_resM_Dp(idim,ui) +=   w1 * lin_p_vel_(idim);
      }
    }
  }

  if (not my::fldparatimint_->IsStationary())
  {
    for (int ui=0; ui<my::nen_; ++ui)
    {
       const double w = my::funct_(ui)*timefacfacpre/porosity_*dphi_dp;
       for (int idim = 0; idim <my::nsd_; ++idim)
         lin_resM_Dp(idim,ui) +=  w * (- reagridvel_(idim) );
    }
    if (!const_permeability_) //check if derivatives of reaction tensor are zero --> significant speed up
    {
      for (int ui=0; ui<my::nen_; ++ui)
      {
        const double w1 = my::funct_(ui)*timefacfacpre*dphi_dp*porosity_;
        for (int idim = 0; idim <my::nsd_; ++idim)
          lin_resM_Dp(idim,ui) += - w1 * lin_p_vel_grid_(idim);
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluateVariablesAtGaussPoint(
    Teuchos::ParameterList&                                         params,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
    const LINALG::Matrix<my::nen_, 1>&                              epreaf,
    const LINALG::Matrix<my::nen_, 1>&                              eprenp,
    const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
    const LINALG::Matrix<my::nen_,1>&                               escaaf,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        emhist,
    const LINALG::Matrix<my::nen_,1>&                               echist,
    const LINALG::Matrix<my::nen_,1>*                               eporositynp,
    const LINALG::Matrix<my::nen_,1>*                               eporositydot,
    const LINALG::Matrix<my::nen_,1>*                               eporositydotn
    )
{
  //----------------------------------------------------------------------
  //  evaluation of various values at integration point:
  //  1) velocity (including derivatives and grid velocity)
  //  2) pressure (including derivatives)
  //  3) body-force vector
  //  4) "history" vector for momentum equation
  //----------------------------------------------------------------------
  // get velocity at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  my::velint_.Multiply(evelaf,my::funct_);

  // get velocity derivatives at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  my::vderxy_.MultiplyNT(evelaf,my::derxy_);

  // get velocity at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  gridvelint_.Multiply(egridv,my::funct_);

  // get convective velocity at integration point
  // (ALE case handled implicitly here using the (potential
  //  mesh-movement-dependent) convective velocity, avoiding
  //  various ALE terms used to be calculated before)
  //convmy::velint_.Update(my::velint_);
  //my::convvelint_.Multiply(-1.0, egridv, my::funct_, 0.0);
  my::convvelint_.Update(-1.0,gridvelint_,0.0);

  convel_.Update(-1.0,gridvelint_,1.0,my::velint_);

  // get pressure at integration point
  // (value at n+alpha_F for generalized-alpha scheme,
  //  value at n+alpha_F for generalized-alpha-NP schemen, n+1 otherwise)
  //double press(true);
  if(my::fldparatimint_->IsGenalphaNP())
    press_ = my::funct_.Dot(eprenp);
  else
    press_ = my::funct_.Dot(epreaf);

  // get pressure time derivative at integration point
  // (value at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  pressdot_ = my::funct_.Dot(epressnp_timederiv);

  // get pressure time derivative at integration point
  // (value at n )
  //double pressn_dot = my::funct_.Dot(epressn_timederiv);

  // get pressure gradient at integration point
  // (value at n+alpha_F for generalized-alpha scheme,
  //  value at n+alpha_F for generalized-alpha-NP schemen, n+1 otherwise)
  if(my::fldparatimint_->IsGenalphaNP())
    my::gradp_.Multiply(my::derxy_,eprenp);
  else
    my::gradp_.Multiply(my::derxy_,epreaf);

  // fluid pressure at gradient w.r.t to reference coordinates at gauss point
  refgradp_.Multiply(my::deriv_,epreaf);

  // get bodyforce at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  my::bodyforce_.Multiply(ebofoaf,my::funct_);

  // get momentum history data at integration point
  // (only required for one-step-theta and BDF2 time-integration schemes)
  my::histmom_.Multiply(emhist,my::funct_);

  // "history" of continuity equation, i.e. p^n + \Delta t * (1-theta) * \dot{p}^n
  histcon_=my::funct_.Dot(echist);

  // get structure velocity derivatives at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  LINALG::Matrix<my::nsd_,my::nsd_>              gridvelderxy;
  gridvelderxy.MultiplyNT(egridv,my::derxy_);

  // structure velocity derivatives w.r.t. reference coordinates at integration point
  gridvelderiv_.MultiplyNT(egridv,my::deriv_);

  //----------------------------------------------------------------------
  //  evaluation of various partial operators at integration point
  //  1) convective term from previous iteration (mandatorily set to zero)
  //  2) viscous term from previous iteration and viscous operator
  //  3) divergence of velocity from previous iteration
  //----------------------------------------------------------------------
  // set convective term from previous iteration to zero (required for
  // using routine for evaluation of momentum rhs/residual as given)
  //conv_old_.Clear();

  //set old convective term to ALE-Term only
  my::conv_old_.Multiply(my::vderxy_,my::convvelint_);
  my::conv_c_.MultiplyTN(my::derxy_,my::convvelint_);

  // set viscous term from previous iteration to zero (required for
  // using routine for evaluation of momentum rhs/residual as given)
  my::visc_old_.Clear();

  // compute divergence of velocity from previous iteration
  my::vdiv_ = 0.0;

  gridvdiv_ = 0.0;

  if (not my::fldparatimint_->IsGenalphaNP())
  {
    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      my::vdiv_ += my::vderxy_(idim, idim);
      gridvdiv_ += gridvelderxy(idim,idim);
    }
  }
  else
  {
    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      //get vdiv at time n+1 for np_genalpha,
      LINALG::Matrix<my::nsd_,my::nsd_> vderxy;
      vderxy.MultiplyNT(evelnp,my::derxy_);
      my::vdiv_ += vderxy(idim, idim);

      gridvdiv_ += gridvelderxy(idim,idim);
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoro<distype>::EvaluateVariablesAtGaussPointOD(
    Teuchos::ParameterList&                                         params,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
    const LINALG::Matrix<my::nen_, 1>&                              epreaf,
    const LINALG::Matrix<my::nen_, 1>&                              eprenp,
    const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
    const LINALG::Matrix<my::nen_,1>&                               escaaf,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        emhist,
    const LINALG::Matrix<my::nen_,1>&                               echist,
    const LINALG::Matrix<my::nen_,1>*                               eporositynp)
{
  //----------------------------------------------------------------------
  //  evaluation of various values at integration point:
  //  1) velocity (including my::derivatives and grid velocity)
  //  2) pressure (including my::derivatives)
  //  3) body-force vector
  //  4) "history" vector for momentum equation
  //  5) and more
  //----------------------------------------------------------------------
  // get velocity at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  my::velint_.Multiply(evelaf,my::funct_);

  // get velocity my::derivatives at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  my::vderxy_.MultiplyNT(evelaf,my::derxy_);

  my::vderiv_.MultiplyNT(evelaf, my::deriv_);

  // get velocity at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  gridvelint_.Multiply(egridv,my::funct_);

  convel_.Update(-1.0,gridvelint_,1.0,my::velint_);

  // get convective velocity at integration point
  // (ALE case handled implicitly here using the (potential
  //  mesh-movement-dependent) convective velocity, avoiding
  //  various ALE terms used to be calculated before)
  // convvelint_.Update(my::velint_);
  my::convvelint_.Update(-1.0, gridvelint_,0.0);

  // get pressure at integration point
  // (value at n+alpha_F for generalized-alpha scheme,
  //  value at n+alpha_F for generalized-alpha-NP schemen, n+1 otherwise)
  //double press(true);
  if(my::fldparatimint_->IsGenalphaNP())
    press_ = my::funct_.Dot(eprenp);
  else
    press_ = my::funct_.Dot(epreaf);

  // fluid pressure at gradient w.r.t to reference coordinates at gauss point
  refgradp_.Multiply(my::deriv_,epreaf);

  // get pressure time my::derivative at integration point
  // (value at n+1 )
  pressdot_ = my::funct_.Dot(epressnp_timederiv);

  // get pressure gradient at integration point
  // (value at n+alpha_F for generalized-alpha scheme,
  //  value at n+alpha_F for generalized-alpha-NP schemen, n+1 otherwise)
  if(my::fldparatimint_->IsGenalphaNP())
    my::gradp_.Multiply(my::derxy_,eprenp);
  else
    my::gradp_.Multiply(my::derxy_,epreaf);

  // get displacement my::derivatives at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  LINALG::Matrix<my::nsd_,my::nsd_> gridvelderxy;
  gridvelderxy.MultiplyNT(egridv,my::derxy_);

  gridvelderiv_.MultiplyNT(egridv,my::deriv_);

  // get bodyforce at integration point
  // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
  my::bodyforce_.Multiply(ebofoaf,my::funct_);

  // get momentum history data at integration point
  // (only required for one-step-theta and BDF2 time-integration schemes)
  my::histmom_.Multiply(emhist,my::funct_);

  // "history" of continuity equation, i.e. p^n + \Delta t * (1-theta) * \dot{p}^n
  histcon_=my::funct_.Dot(echist);

  //----------------------------------------------------------------------
  //  evaluation of various partial operators at integration point
  //  1) convective term from previous iteration (mandatorily set to zero)
  //  2) viscous term from previous iteration and viscous operator
  //  3) divergence of velocity from previous iteration
  //----------------------------------------------------------------------
  // set convective term from previous iteration to zero (required for
  // using routine for evaluation of momentum rhs/residual as given)
  //  conv_old_.Clear();

  //set old convective term to ALE-Term only
  my::conv_old_.Multiply(my::vderxy_,my::convvelint_);
  my::conv_c_.MultiplyTN(my::derxy_,my::convvelint_);

  // set viscous term from previous iteration to zero (required for
  // using routine for evaluation of momentum rhs/residual as given)
  my::visc_old_.Clear();

  // compute divergence of velocity from previous iteration
  my::vdiv_ = 0.0;

  gridvdiv_ = 0.0;
  if (not my::fldparatimint_->IsGenalphaNP())
  {
    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      my::vdiv_ += my::vderxy_(idim, idim);

      gridvdiv_ += gridvelderxy(idim,idim);
    }
  }
  else
  {
    for (int idim = 0; idim <my::nsd_; ++idim)
    {
      //get vdiv at time n+1 for np_genalpha,
      LINALG::Matrix<my::nsd_,my::nsd_> vderxy;
      vderxy.MultiplyNT(evelnp,my::derxy_);
      my::vdiv_ += vderxy(idim, idim);

      gridvdiv_ += gridvelderxy(idim,idim);
    }
  }
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeVolume(
    Teuchos::ParameterList&              params,
    DRT::ELEMENTS::Fluid*           ele,
    DRT::Discretization&            discretization,
    std::vector<int>&               lm,
    Epetra_SerialDenseVector&       elevec1)
{
  // get node coordinates
  GEO::fillInitialPositionArray<distype,my::nsd_, LINALG::Matrix<my::nsd_,my::nen_> >(ele,my::xyze_);
  // set element id
  my::eid_ = ele->Id();

  LINALG::Matrix<my::nsd_,my::nen_> edispnp(true);
  my::ExtractValuesFromGlobalVector(discretization,lm, *my::rotsymmpbc_, &edispnp, NULL,"dispnp");

  // get new node positions of ALE mesh
  my::xyze_ += edispnp;

  // integration loop
  for ( DRT::UTILS::GaussIntegration::iterator iquad=my::intpoints_.begin(); iquad!=my::intpoints_.end(); ++iquad )
  {
    // evaluate shape functions and derivatives at integration point
    my::EvalShapeFuncAndDerivsAtIntPoint(iquad);

    //-----------------------------------auxilary variables for computing the porosity
    porosity_=0.0;

    // compute scalar at n+alpha_F or n+1
    //const double scalaraf = my::funct_.Dot(escaaf);
    //params.set<double>("scalar",scalaraf);

    ComputePorosity(  params,
                      press_,
                      J_,
                      *(iquad),
                      my::funct_,
                      NULL,
                      porosity_,
                      NULL,
                      NULL,
                      NULL,
                      NULL, //dphi_dJJ not needed
                      NULL,
                      false);

    for (int nodes = 0; nodes < my::nen_; nodes++) // loop over nodes
    {
      elevec1((my::numdofpernode_) * nodes) += my::funct_(nodes) * porosity_* my::fac_;
    }
  } // end of integration loop

  return 0;
}


/*----------------------------------------------------------------------*
 * Action type: Compute Error
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeError(
    DRT::ELEMENTS::Fluid*           ele,
    Teuchos::ParameterList&         params,
    Teuchos::RCP<MAT::Material>&    mat,
    DRT::Discretization&            discretization,
    std::vector<int>&               lm,
    Epetra_SerialDenseVector&       elevec1
    )
{
  // integrations points and weights
  // more GP than usual due to (possible) cos/exp fcts in analytical solutions
  // degree 5
  const DRT::UTILS::GaussIntegration intpoints(distype, 5);
  return ComputeError( ele, params, mat,
                       discretization, lm,
                       elevec1, intpoints);
}

/*----------------------------------------------------------------------*
 * Action type: Compute Error
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoro<distype>::ComputeError(
    DRT::ELEMENTS::Fluid*           ele,
    Teuchos::ParameterList&         params,
    Teuchos::RCP<MAT::Material>&    mat,
    DRT::Discretization&            discretization,
    std::vector<int>&               lm,
    Epetra_SerialDenseVector&       elevec1,
    const DRT::UTILS::GaussIntegration & intpoints
    )
{
  // analytical solution
  LINALG::Matrix<my::nsd_,1>  u(true);
  double p = 0.0;

  // error
  LINALG::Matrix<my::nsd_,1> deltavel(true);
  double         deltap=0.0;

  const int calcerr = DRT::INPUT::get<INPAR::FLUID::CalcError>(params,"calculate error");

  //----------------------------------------------------------------------------
  //   Extract velocity/pressure from global vectors
  //----------------------------------------------------------------------------

  // fill the local element vector/matrix with the global values
  // af_genalpha: velocity/pressure at time n+alpha_F
  // np_genalpha: velocity at time n+alpha_F, pressure at time n+1
  // ost:         velocity/pressure at time n+1
  LINALG::Matrix<my::nsd_,my::nen_> evelaf(true);
  LINALG::Matrix<my::nen_,1>    epreaf(true);
  my::ExtractValuesFromGlobalVector(discretization,lm, *my::rotsymmpbc_, &evelaf, &epreaf,"velaf");

  // np_genalpha: additional vector for velocity at time n+1
  LINALG::Matrix<my::nsd_,my::nen_> evelnp(true);
  LINALG::Matrix<my::nen_,1>    eprenp(true);
  if (my::fldparatimint_->IsGenalphaNP())
    my::ExtractValuesFromGlobalVector(discretization,lm, *my::rotsymmpbc_, &evelnp, &eprenp,"velnp");

  //----------------------------------------------------------------------------
  //                         ELEMENT GEOMETRY
  //----------------------------------------------------------------------------

  // get node coordinates
  GEO::fillInitialPositionArray<distype,my::nsd_, LINALG::Matrix<my::nsd_,my::nen_> >(ele,my::xyze_);
  // set element id
  my::eid_ = ele->Id();

  LINALG::Matrix<my::nsd_,my::nen_>       edispnp(true);
  my::ExtractValuesFromGlobalVector(discretization,lm, *my::rotsymmpbc_, &edispnp, NULL,"dispnp");

  // get new node positions for isale
  my::xyze_ += edispnp;

//------------------------------------------------------------------
//                       INTEGRATION LOOP
//------------------------------------------------------------------

  for ( DRT::UTILS::GaussIntegration::iterator iquad=intpoints.begin(); iquad!=intpoints.end(); ++iquad )
  {
    // evaluate shape functions and derivatives at integration point
    my::EvalShapeFuncAndDerivsAtIntPoint(iquad);

    // get velocity at integration point
    // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
    my::velint_.Multiply(evelaf,my::funct_);

    // get pressure at integration point
    // (value at n+alpha_F for generalized-alpha scheme,
    //  value at n+alpha_F for generalized-alpha-NP schemen, n+1 otherwise)
    double preint(true);
    if(my::fldparatimint_->IsGenalphaNP())
      preint= my::funct_.Dot(eprenp);
    else
      preint = my::funct_.Dot(epreaf);

    /* H1 -error norm
    // compute first derivative of the velocity
    LINALG::Matrix<my::nsd_,my::nsd_> dervelint;
    dervelint.MultiplyNT(evelaf,derxy_);
    */

    // get coordinates at integration point
    LINALG::Matrix<my::nsd_,1> xyzint(true);
    xyzint.Multiply(my::xyze_,my::funct_);

    //  the error is evaluated at the specific time of the used time integration scheme
    //  n+alpha_F for generalized-alpha scheme
    //  value at n+alpha_F for generalized-alpha-NP schemen, n+1 otherwise)
    const double t = my::fldparatimint_->Time();

    // Compute analytical solution
    switch(calcerr)
    {
    case INPAR::FLUID::byfunct1:
    {
      const int func_no = 1;


      // function evaluation requires a 3D position vector!!
      double position[3];

      if (my::nsd_ == 2)
      {

        position[0] = xyzint(0);
        position[1] = xyzint(1);
        position[2] = 0.0;
      }
      else if(my::nsd_ == 3)
      {
        position[0] = xyzint(0);
        position[1] = xyzint(1);
        position[2] = xyzint(2);
      }
      else dserror("invalid nsd %d", my::nsd_);

      if(my::nsd_ == 2)
      {
        const double u_exact_x = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(0,position,t,NULL);
        const double u_exact_y = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(1,position,t,NULL);
        const double p_exact   = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(2,position,t,NULL);

        u(0) = u_exact_x;
        u(1) = u_exact_y;
        p    = p_exact;
      }
      else if(my::nsd_==3)
      {
        const double u_exact_x = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(0,position,t,NULL);
        const double u_exact_y = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(1,position,t,NULL);
        const double u_exact_z = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(2,position,t,NULL);
        const double p_exact   = DRT::Problem::Instance()->Funct(func_no-1).Evaluate(3,position,t,NULL);

        u(0) = u_exact_x;
        u(1) = u_exact_y;
        u(2) = u_exact_z;
        p    = p_exact;
      }
      else dserror("invalid dimension");

    }
    break;
    default:
      dserror("analytical solution is not defined");
      break;
    }

    // compute difference between analytical solution and numerical solution
    deltap    = preint - p;
    deltavel.Update(1.0, my::velint_, -1.0, u);

    /* H1 -error norm
    // compute error for first velocity derivative
    for(int i=0;i<my::nsd_;++i)
      for(int j=0;j<my::nsd_;++j)
        deltadervel(i,j)= dervelint(i,j) - dervel(i,j);
    */

    // L2 error
    // 0: vel_mag
    // 1: p
    // 2: vel_mag,analytical
    // 3: p_analytic
    // (4: vel_x)
    // (5: vel_y)
    // (6: vel_z)
    for (int isd=0;isd<my::nsd_;isd++)
    {
      elevec1[0] += deltavel(isd)*deltavel(isd)*my::fac_;
      //integrate analytical velocity (computation of relative error)
      elevec1[2] += u(isd)*u(isd)*my::fac_;
      // velocity components
      //elevec1[isd+4] += deltavel(isd)*deltavel(isd)*fac_;
    }
    elevec1[1] += deltap*deltap*my::fac_;
    //integrate analytical pressure (computation of relative error)
    elevec1[3] += p*p*my::fac_;

    /*
    //H1-error norm: first derivative of the velocity
    elevec1[4] += deltadervel.Dot(deltadervel)*fac_;
    */
  }

  return 0;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
// Ursula is responsible for this comment!
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::hex8>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::hex20>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::hex27>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::tet4>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::tet10>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::wedge6>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::pyramid5>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::quad4>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::quad8>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::quad9>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::tri3>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::tri6>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::nurbs9>;
template class DRT::ELEMENTS::FluidEleCalcPoro<DRT::Element::nurbs27>;
