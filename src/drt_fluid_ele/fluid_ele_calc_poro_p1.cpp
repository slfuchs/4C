/*----------------------------------------------------------------------*/
/*!
 \file fluid_ele_calc_poro_p1.cpp

 \brief

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *----------------------------------------------------------------------*/

#include "fluid_ele_calc_poro_p1.H"

#include "fluid_ele.H"
#include "fluid_ele_parameter.H"
#include "fluid_ele_utils.H"

#include "../drt_mat/fluidporo.H"

#include "../drt_so3/so_poro_interface.H"

#include "../drt_fluid/fluid_rotsym_periodicbc.H"

#include "../drt_geometry/position_array.H"


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::FluidEleCalcPoroP1<distype>* DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::Instance( bool create, int num )
{
  if ( create )
  {
    if(static_cast<int>(MY::instances_.count(num))==0)
    {
      MY::instances_.insert(std::pair<int,std::map<int,MY* >* >(num, new std::map<int,MY* >));
      MY::instances_.at(num)->insert(std::pair<int,MY* >((int)distype,new FluidEleCalcPoroP1<distype>(num)));
    }
    else if ( MY::instances_.count(num) > 0 and MY::instances_.at(num)->count((int)distype) == 0 )
    {
      MY::instances_.at(num)->insert(std::pair<int,MY* >((int)distype, new FluidEleCalcPoroP1<distype>(num)));
    }

    return static_cast<DRT::ELEMENTS::FluidEleCalcPoroP1<distype>* >(MY::instances_.at(num)->at((int)distype));
  }
  else
  {
    if ( MY::instances_.at(num)->size())
    {
      delete MY::instances_.at(num)->at((int)distype);
      MY::instances_.at(num)->erase((int)distype);

      if ( !(MY::instances_.at(num)->size()) )
        MY::instances_.erase(num);
    }

    return NULL;
  }

  return NULL;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
    Instance( false, numporop1_ );
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::FluidEleCalcPoroP1(int num)
  : DRT::ELEMENTS::FluidEleCalcPoro<distype>::FluidEleCalcPoro(num),
    numporop1_(num)
{

}

/*----------------------------------------------------------------------*
 * evaluation of coupling terms for porous flow (2)
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::Evaluate(
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
  my::GetStructMaterial();

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
  if (FluidEleCalc<distype>::fldpara_->IsGenalphaNP())
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

  if (not FluidEleCalc<distype>::fldpara_->IsGenalpha())
    eaccam.Clear();

  // ---------------------------------------------------------------------
  // get additional state vectors for ALE case: grid displacement and vel.
  // ---------------------------------------------------------------------
  LINALG::Matrix<my::nsd_, my::nen_> edispnp(true);
  LINALG::Matrix<my::nsd_, my::nen_> egridv(true);
  LINALG::Matrix<my::nsd_, my::nen_> egridvn(true);
  LINALG::Matrix<my::nsd_, my::nen_> edispn(true);

  LINALG::Matrix<my::nen_, 1> eporositynp(true);
  LINALG::Matrix<my::nen_, 1> eporositydot(true);
  LINALG::Matrix<my::nen_, 1> eporositydotn(true);

  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &edispnp,
      &eporositynp, "dispnp");
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &egridv,
      &eporositydot, "gridv");
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, NULL,
      &eporositydotn, "gridvn");
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &edispn,
      NULL, "dispn");

  // get node coordinates and number of elements per node
  GEO::fillInitialPositionArray<distype, my::nsd_, LINALG::Matrix<my::nsd_, my::nen_> >(
      ele, my::xyze_);

  my::PreEvaluate(params,ele,discretization);

  // call inner evaluate (does not know about DRT element or discretization object)
  int result = my::Evaluate(
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
                  &eporositynp,
                  &eporositydot,
                  &eporositydotn,
                  mat,
                  ele->IsAle(),
                  intpoints);

  return result;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::ComputePorosity(
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
  if(myporosity == NULL)
    dserror("no porosity values given!!");
  else
    porosity = shapfct.Dot(*myporosity);

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::ComputePorosityGradient(
                        const double&                                      dphidp,
                        const double&                                      dphidJ,
                        const LINALG::Matrix<my::nsd_,1>&                  gradJ,
                        const LINALG::Matrix<my::nen_,1>*                  eporositynp,
                        LINALG::Matrix<my::nsd_,1>&                        grad_porosity)
{
  if(eporositynp == NULL)
    dserror("no porosity values given for calculation of porosity gradient!!");

  //if( (my::fldpara_->PoroContiPartInt() == false) or my::visceff_)
  {
    //--------------------- current porosity gradient
    grad_porosity.Multiply(my::derxy_,*eporositynp);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::EvaluatePressureEquation(
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
  my::EvaluatePressureEquationNonTransient(params,
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

  // now the porosity time derivative (different for standard poro and poro_p1 elements)
  if (my::fldpara_->IsStationary() == false)
  {
    // inertia terms on the right hand side for instationary fluids

    if(eporositydot)
    {
      double porositydot =  my::funct_.Dot(*eporositydot);
      //double porositydot =  my::funct_.Dot(*eporositydotn);

      for (int vi=0; vi<my::nen_; ++vi)
      {//TODO : check genalpha case
        preforce(vi)-=  rhsfac * porositydot * my::funct_(vi) ;
      }

      //no need for adding RHS form previous time step, as it is already included in 'porositydot'
      //(for the one-step-theta case at least)
      //ComputeContiTimeRHS(params,*eporositydotn,preforce,rhsfac,1.0);

      //just update internal variables, no contribution to rhs
      const double porositydotn = my::funct_.Dot(*eporositydotn);

      my::histcon_ = my::fldpara_->OmTheta() * my::fldpara_->Dt() * porositydotn;

      //rhs from last time step
      my::rhscon_ = 1.0/my::fldpara_->Dt()/my::fldpara_->Theta() * my::histcon_;

      //transient part of continuity equation residual
      my::conres_old_ += porositydot - my::rhscon_;
    }
    else
      dserror("no porosity time derivative given for poro_p1 element!");
  }

  return;
}

/*----------------------------------------------------------------------*
 * evaluation of coupling terms for porous flow (2)
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::EvaluateOD(
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
  my::GetStructMaterial();

  // rotationally symmetric periodic bc's: do setup for current element
  // (only required to be set up for routines "ExtractValuesFromGlobalVector")
  my::rotsymmpbc_->Setup(ele);

  // construct views
  LINALG::Matrix<(my::nsd_ + 1) * my::nen_, (my::nsd_ + 1)* my::nen_> elemat1(elemat1_epetra, true);
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
  if (my::fldpara_->IsGenalphaNP())
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

  LINALG::Matrix<my::nen_, 1> eporositynp(true);

  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &edispnp,
      &eporositynp, "dispnp");
  my::ExtractValuesFromGlobalVector(discretization, lm, *my::rotsymmpbc_, &egridv,
      NULL, "gridv");

  //ExtractValuesFromGlobalVector(discretization,lm, *rotsymmpbc_, NULL, &initporosity_, "initporosity");

  // get node coordinates and number of elements per node
  GEO::fillInitialPositionArray<distype, my::nsd_, LINALG::Matrix<my::nsd_, my::nen_> >(
      ele, my::xyze_);

  my::PreEvaluate(params,ele,discretization);

  // call inner evaluate (does not know about DRT element or discretization object)
  int result = EvaluateOD(params,
      ebofoaf,
      elemat1,
      elevec1,
      evelaf,
      epreaf,
      evelnp,
      eprenp,
      emhist,
      echist,
      epressnp_timederiv,
      edispnp,
      egridv,
      escaaf,
      &eporositynp,
      mat,
      ele->IsAle(),
      intpoints);

  return result;
}

/*----------------------------------------------------------------------*
 * evaluation of coupling terms for porous flow (3)
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::EvaluateOD(
    Teuchos::ParameterList&                                           params,
    const LINALG::Matrix<my::nsd_,my::nen_> &                         ebofoaf,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_, (my::nsd_ + 1) * my::nen_> &  elemat1,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1> &                    elevec1,
    const LINALG::Matrix<my::nsd_,my::nen_> &                         evelaf,
    const LINALG::Matrix<my::nen_, 1> &                               epreaf,
    const LINALG::Matrix<my::nsd_, my::nen_> &                        evelnp,
    const LINALG::Matrix<my::nen_, 1> &                               eprenp,
    const LINALG::Matrix<my::nsd_,my::nen_> &                         emhist,
    const LINALG::Matrix<my::nen_,1>&                                 echist,
    const LINALG::Matrix<my::nen_, 1> &                               epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_> &                        edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_> &                        egridv,
    const LINALG::Matrix<my::nen_,1>&                                 escaaf,
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
  //if (isale and my::fldpara_->IsStationary())
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
        emhist,
        echist,
        epressnp_timederiv,
        edispnp,
        egridv,
        escaaf,
        eporositynp,
        elemat1,
        elevec1,
        mat,
        isale,
        intpoints);

    return 0;
  }

/*----------------------------------------------------------------------*
 |  calculate coupling matrix flow                          vuong 06/11 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::SysmatOD(
    Teuchos::ParameterList&                                         params,
    const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
    const LINALG::Matrix<my::nen_, 1>&                              epreaf,
    const LINALG::Matrix<my::nen_, 1>&                              eprenp,
    const LINALG::Matrix<my::nsd_,my::nen_> &                       emhist,
    const LINALG::Matrix<my::nen_,1>&                               echist,
    const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
    const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
    const LINALG::Matrix<my::nen_,1>&                               escaaf,
    const LINALG::Matrix<my::nen_,1>*                               eporositynp,
    LINALG::Matrix<(my::nsd_ + 1) * my::nen_,(my::nsd_ + 1) * my::nen_>&  ecoupl,
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

  LINALG::Matrix<my::nen_ * my::nsd_, my::nen_> ecouplp1_u(true); // coupling matrix for momentum equation
  LINALG::Matrix<my::nen_, my::nen_> ecouplp1_p(true); // coupling matrix for continuity equation

  //material coordinates xyze0
  my::xyze0_ = my::xyze_;

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
  GaussPointLoopP1OD(  params,
                       ebofoaf,
                       evelaf,
                       evelnp,
                       epreaf,
                       eprenp,
                       emhist,
                       echist,
                       epressnp_timederiv,
                       edispnp,
                       egridv,
                       escaaf,
                       eporositynp,
                       eforce,
                       ecoupl_u,
                       ecoupl_p,
                       ecouplp1_u,
                       ecouplp1_p,
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
    const int nsd_ui = my::nsd_ *ui;
    const int nsdp1_ui = (my::nsd_ + 1)*ui;

    for (int jdim=0; jdim < my::nsd_;++jdim)
    {
      const int nsd_ui_jdim = nsd_ui+jdim;
      const int nsdp1_ui_jdim = nsdp1_ui+jdim;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const int numdof_vi = my::numdofpernode_*vi;
        const int nsd_vi = my::nsd_*vi;

        for (int idim=0; idim <my::nsd_; ++idim)
        {
          ecoupl(numdof_vi+idim, nsdp1_ui_jdim) += ecoupl_u(nsd_vi+idim, nsd_ui_jdim);
        }
      }
    }
  }

  // add fluid pressure-structure displacement part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int nsd_ui = my::nsd_ *ui;
    const int nsdp1_ui = (my::nsd_ + 1)*ui;

    for (int jdim=0; jdim < my::nsd_;++jdim)
    {
      const int nsd_ui_jdim = nsd_ui+jdim;
      const int nsdp1_ui_jdim = nsdp1_ui+jdim;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        ecoupl(my::numdofpernode_*vi+my::nsd_, nsdp1_ui_jdim) += ecoupl_p(vi, nsd_ui_jdim);
      }
    }
  }

  // add fluid velocity-structure porosity part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int nsdp1_ui = (my::nsd_ + 1)*ui;

    for (int idim=0; idim < my::nsd_;++idim)
    {
      const int nsdp1_ui_nsd = nsdp1_ui+my::nsd_;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const int numdof_vi = my::numdofpernode_*vi;
        const int nsd_vi = my::nsd_*vi;

        ecoupl(numdof_vi+idim, nsdp1_ui_nsd) += ecouplp1_u(nsd_vi+idim, ui);
      }
    }
  }

  // add fluid pressure-structure porosity part to matrix
  for (int ui=0; ui<my::nen_; ++ui)
  {
    const int nsdp1_ui_nsd = (my::nsd_ + 1)*ui+my::nsd_;

    for (int vi=0; vi<my::nen_; ++vi)
      ecoupl(my::numdofpernode_*vi+my::nsd_, nsdp1_ui_nsd) += ecouplp1_p(vi, ui);
  }

  return;
}    //SysmatOD

/*----------------------------------------------------------------------*
 |  calculate coupling matrix flow                          vuong 06/11 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::GaussPointLoopP1OD(
                        Teuchos::ParameterList&                                         params,
                        const LINALG::Matrix<my::nsd_,my::nen_>&                        ebofoaf,
                        const LINALG::Matrix<my::nsd_, my::nen_>&                       evelaf,
                        const LINALG::Matrix<my::nsd_, my::nen_>&                       evelnp,
                        const LINALG::Matrix<my::nen_, 1>&                              epreaf,
                        const LINALG::Matrix<my::nen_, 1>&                              eprenp,
                        const LINALG::Matrix<my::nsd_,my::nen_> &                       emhist,
                        const LINALG::Matrix<my::nen_,1>&                               echist,
                        const LINALG::Matrix<my::nen_, 1> &                             epressnp_timederiv,
                        const LINALG::Matrix<my::nsd_, my::nen_>&                       edispnp,
                        const LINALG::Matrix<my::nsd_, my::nen_>&                       egridv,
                        const LINALG::Matrix<my::nen_,1>&                               escaaf,
                        const LINALG::Matrix<my::nen_,1>*                               eporositynp,
                        LINALG::Matrix<(my::nsd_ + 1) * my::nen_, 1>&                   eforce,
                        LINALG::Matrix<my::nen_ * my::nsd_, my::nen_ * my::nsd_>&       ecoupl_u,
                        LINALG::Matrix<my::nen_, my::nen_ * my::nsd_>&                  ecoupl_p,
                        LINALG::Matrix<my::nen_ * my::nsd_, my::nen_>&                  ecouplp1_u,
                        LINALG::Matrix<my::nen_, my::nen_>&                             ecouplp1_p,
                        Teuchos::RCP<const MAT::Material>                               material,
                        const DRT::UTILS::GaussIntegration &                            intpoints)
{
  // definition of velocity-based momentum residual vectors
  LINALG::Matrix< my::nsd_, my::nen_ * my::nsd_>  lin_resM_Dus(true);

  // set element area or volume
  const double vol = my::fac_;

  for ( DRT::UTILS::GaussIntegration::const_iterator iquad=intpoints.begin(); iquad!=intpoints.end(); ++iquad )
  {
    lin_resM_Dus.Clear();

    // evaluate shape functions and derivatives at integration point
    my::EvalShapeFuncAndDerivsAtIntPoint(iquad);

    // evaluate shape function derivatives w.r.t. to material coordinates at integration point
    const double det0 = my::SetupMaterialDerivatives();
    // determinant of deformationgradient det F = det ( d x / d X ) = det (dx/ds) * ( det(dX/ds) )^-1
    my::J_ = my::det_/det0;

    my::EvaluateVariablesAtGaussPointOD(
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

    //************************************************auxilary variables for computing the porosity

    double dphi_dp=0.0;
    double dphi_dJ=0.0;
    double dphi_dJdp=0.0;
    double dphi_dJJ=0.0;
    my::porosity_=0.0;

    // compute scalar at n+alpha_F or n+1
    const double scalaraf = my::funct_.Dot(escaaf);
    params.set<double>("scalar",scalaraf);
    ComputePorosity(  params,
                      my::press_,
                      my::J_,
                      *(iquad),
                      my::funct_,
                      eporositynp,
                      my::porosity_,
                      &dphi_dp,
                      &dphi_dJ,
                      &dphi_dJdp,
                      &dphi_dJJ,
                      NULL, //dphi_dpp not needed
                      false);

    double refporositydot = my::so_interface_->RefPorosityTimeDeriv();

    //---------------------------  dJ/dx = dJ/dF : dF/dx = JF^-T : dF/dx at gausspoint
    LINALG::Matrix<my::nsd_,1> gradJ(true);
    // spatial porosity gradient
    LINALG::Matrix<my::nsd_,1>             grad_porosity(true);
    //--------------------- linearization of porosity w.r.t. structure displacements
    LINALG::Matrix<1,my::nsd_*my::nen_> dphi_dus(true);

    //------------------------------------------------dJ/dus = dJ/dF : dF/dus = J * F^-T . N_X = J * N_x
    LINALG::Matrix<1,my::nsd_*my::nen_> dJ_dus(true);
    //------------------ d( grad(\phi) ) / du_s = d\phi/(dJ du_s) * dJ/dx+ d\phi/dJ * dJ/(dx*du_s) + d\phi/(dp*du_s) * dp/dx
    LINALG::Matrix<my::nsd_,my::nen_*my::nsd_> dgradphi_dus(true);

    // -------------------------(material) deformation gradient F = d my::xyze_ / d XYZE = my::xyze_ * N_XYZ_^T
    LINALG::Matrix<my::nsd_,my::nsd_> defgrd(false);
    defgrd.MultiplyNT(my::xyze_,my::N_XYZ_);

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

      my::ComputeFDerivative( edispnp,
                          defgrd_inv,
                          F_x,
                          F_X);

      //compute gradients if needed
      my::ComputeGradients(
                       dphi_dp,
                       dphi_dJ,
                       defgrd_IT_vec,
                       F_x,
                       eporositynp,
                       gradJ);

      my::ComputeLinearizationOD(
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
    my::GetMaterialParamters(material);

    my::ComputeSpatialReactionTerms(material,defgrd_inv);

    //compute linearization of spatial reaction tensor w.r.t. structural displacements
    {
      Teuchos::RCP<const MAT::FluidPoro> actmat = Teuchos::rcp_static_cast<const MAT::FluidPoro>(material);
      if(actmat->VaryingPermeablity())
        dserror("varying material permeablity not yet supported!");

      my::reatensorlinODvel_.Clear();
      my::reatensorlinODgridvel_.Clear();
      for (int n =0; n<my::nen_; ++n)
        for (int d =0; d<my::nsd_; ++d)
        {
          const int gid = my::nsd_ * n +d;
          for (int i=0; i<my::nsd_; ++i)
          {
            my::reatensorlinODvel_(i, gid)     += dJ_dus(gid)/my::J_ * my::reavel_(i);
            my::reatensorlinODgridvel_(i, gid) += dJ_dus(gid)/my::J_ * my::reagridvel_(i);
            my::reatensorlinODvel_(i, gid)     += dphi_dus(gid)/my::porosity_ * my::reavel_(i);
            my::reatensorlinODgridvel_(i, gid) += dphi_dus(gid)/my::porosity_ * my::reagridvel_(i);
            for (int j=0; j<my::nsd_; ++j)
            {
              for (int k=0; k<my::nsd_; ++k)
                for(int l=0; l<my::nsd_; ++l)
                {
                  my::reatensorlinODvel_(i, gid) += my::J_ * my::porosity_ *
                                                my::velint_(j) *
                                                   ( - defgrd_inv(k,d) * my::derxy_(i,n) * my::matreatensor_(k,l) * defgrd_inv(l,j)
                                                     - defgrd_inv(k,i) *
                                                     my::matreatensor_(k,l) * defgrd_inv(l,d) * my::derxy_(j,n)
                                                    );
                  my::reatensorlinODgridvel_(i, gid) += my::J_ * my::porosity_ *
                                                    my::gridvelint_(j) *
                                                       ( - defgrd_inv(k,d) * my::derxy_(i,n) * my::matreatensor_(k,l) * defgrd_inv(l,j)
                                                         - defgrd_inv(k,i) *
                                                         my::matreatensor_(k,l) * defgrd_inv(l,d) * my::derxy_(j,n)
                                                        );
                }
            }
          }
        }
    }

    // get stabilization parameters at integration point
    my::ComputeStabilizationParameters(vol);

    // compute old RHS of momentum equation and subgrid scale velocity
    my::ComputeOldRHSAndSubgridScaleVelocity();

    // compute old RHS of continuity equation
    my::ComputeOldRHSConti();

    //----------------------------------------------------------------------
    // set time-integration factors for left- and right-hand side
    //----------------------------------------------------------------------
    const double timefacfac = my::fldpara_->TimeFac() * my::fac_;
    const double timefacfacpre = my::fldpara_->TimeFacPre() * my::fac_;

    //***********************************************************************************************
    // 1) coupling terms in momentum balance

    my::FillMatrixMomentumOD(
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

    my::FillMatrixContiOD(  timefacfacpre,
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

    /*  reaction */
    /*
      /                           \
     |                             |
  -  |    sigma * v_f D(phi), v    |
     |                             |
      \                           /
     */
    for (int ui=0; ui<my::nen_; ++ui)
    {
      for (int vi=0; vi<my::nen_; ++vi)
      {
        const int fvi = my::nsd_*vi;
        const double tmp = my::funct_(vi)/my::porosity_;
        for (int idim = 0; idim <my::nsd_; ++idim)
        {
          ecouplp1_u(fvi+idim,ui) += timefacfac * tmp * my::reavel_(idim) * my::funct_(ui);
        } // end for (idim)
      } //vi
    } // ui

    //transient terms
    /*  reaction  and time derivative*/
    /*
      /                           \     /                           \
     |                             |    |                             |
  -  |    sigma * v_s D(phi), v    | +  |    D(phi), v                |
     |                             |    |                             |
      \                           /     \                           /
     */
    if (not my::fldpara_->IsStationary())
    {
      for (int ui=0; ui<my::nen_; ++ui)
      {
        for (int vi=0; vi<my::nen_; ++vi)
        {
          const int fvi = my::nsd_*vi;
          const double tmp = my::funct_(vi)/my::porosity_;
          for (int idim = 0; idim <my::nsd_; ++idim)
          {
            ecouplp1_u(fvi+idim,ui) += timefacfac * tmp * (-my::reagridvel_(idim)) * my::funct_(ui);
          } // end for (idim)
        } //vi
      } // ui

      for (int ui=0; ui<my::nen_; ++ui)
        for (int vi=0; vi<my::nen_; ++vi)
          ecouplp1_p(vi,ui) +=   my::fac_ * my::funct_(vi) * my::funct_(ui);
    }

    LINALG::Matrix<my::nen_,1>    derxy_convel(true);

    for (int i =0; i< my::nen_; i++)
      for (int j =0; j< my::nsd_; j++)
        derxy_convel(i) += my::derxy_(j,i) * my::velint_(j);

    if (not my::fldpara_->IsStationary())
    {
      for (int i =0; i< my::nen_; i++)
        for (int j =0; j< my::nsd_; j++)
          derxy_convel(i) += my::derxy_(j,i) * (-my::gridvelint_(j));
    }

    if( my::fldpara_->PoroContiPartInt() == false )
    {
      /*
        /                           \     /                             \
       |                             |    |                              |
       |    \nabla v_f D(phi), v     | +  |  (v_f-v_s) \nabla  D(phi), v |
       |                             |    |                              |
        \                           /     \                             /
       */
      for (int ui=0; ui<my::nen_; ++ui)
      {
        for (int vi=0; vi<my::nen_; ++vi)
        {
          ecouplp1_p(vi,ui) +=
                               + timefacfacpre * my::vdiv_ * my::funct_(vi) * my::funct_(ui)
                               + timefacfacpre * my::funct_(vi) * derxy_convel(ui)
                                 ;
        }
      }
    }
    else //my::fldpara_->PoroContiPartInt() == true
    {
      /*
          /                             \
          |                              |
       -  |  (v_f-v_s) \nabla  D(phi), v |
          |                              |
          \                             /
       */
      for (int ui=0; ui<my::nen_; ++ui)
      {
        for (int vi=0; vi<my::nen_; ++vi)
        {
          ecouplp1_p(vi,ui) += -1.0 * timefacfacpre * derxy_convel(vi) * my::funct_(ui)
                                 ;
        }
      }
      /*
          /                             \
          |                              |
          |  \nabla v_s D(phi), v        |
          |                              |
          \                             /
       */
      if (not my::fldpara_->IsStationary())
      {
        for (int ui=0; ui<my::nen_; ++ui)
        {
          for (int vi=0; vi<my::nen_; ++vi)
          {
            ecouplp1_p(vi,ui) += timefacfacpre * my::funct_(vi) * my::gridvdiv_ * my::funct_(ui)
                                   ;
          }
        }
      }
    }

  }//loop over gausspoints
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::PSPG(
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
  my::PSPG( estif_q_u,
            ppmat,
            preforce,
            lin_resM_Du,
            lin_resM_Dp,
            dphi_dp,
            fac3,
            timefacfac,
            timefacfacpre,
            rhsfac);

  double scal_grad_q=0.0;

  if(my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
  {
    scal_grad_q=0.0;//my::tau_(1);
  }
  else
  {
    scal_grad_q=my::fldpara_->AlphaF()*fac3;
  }

  for(int jdim=0;jdim<my::nsd_;++jdim)
  {
    for (int ui=0; ui<my::nen_; ++ui)
    {
      const int fui_p_jdim   = my::nsd_*ui + jdim;

      for (int vi=0; vi<my::nen_; ++vi)
      {
        const double temp_vi_idim=my::derxy_(jdim,vi)*scal_grad_q;

        estif_q_u(vi,fui_p_jdim) += timefacfacpre*my::conres_old_*my::funct_(ui)*temp_vi_idim;
      } // vi
    } // ui
  } //jdim

  const double temp = rhsfac*scal_grad_q*my::conres_old_;
  for (int idim = 0; idim <my::nsd_; ++idim)
  {
    for (int vi=0; vi<my::nen_; ++vi)
    {
      // pressure stabilisation
      preforce(vi) -= temp*my::derxy_(idim, vi)*my::velint_(idim);
    }
  } // end for(idim)
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalcPoroP1<distype>::ReacStab(
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
           lin_resM_Dp,
           dphi_dp,
           timefacfac,
           timefacfacpre,
           rhsfac,
           fac3);

  //todo: check stabilization contributions due to poro_p1 approach
//  double reac_tau;
//  if (my::fldpara_->Tds()==INPAR::FLUID::subscales_quasistatic)
//    reac_tau = 0.0;//my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::tau_(1);
//  else
//  {
//    dserror("Is this factor correct? Check for bugs!");
//    reac_tau = my::fldpara_->ViscReaStabFac()*my::reacoeff_*my::fldpara_->AlphaF()*fac3;
//  }
//
//  for (int vi=0; vi<my::nen_; ++vi)
//  {
//    const double v = reac_tau*my::funct_(vi)*my::conres_old_;
//
//    for(int idim=0;idim<my::nsd_;++idim)
//    {
//      const int fvi_p_idim = my::nsd_*vi+idim;
//
//      for (int ui=0; ui<my::nen_; ++ui)
//      {
//        const int fui_p_idim   = my::nsd_*ui + idim;
//
//        estif_u(fvi_p_idim,fui_p_idim) += v*my::funct_(idim);
//      } // ui
//    } //idim
//  } // vi
//
//  const double reac_fac = reac_tau*rhsfac;
//  const double v = reac_fac*my::conres_old_;
//  for (int idim =0;idim<my::nsd_;++idim)
//  {
//    for (int vi=0; vi<my::nen_; ++vi)
//    {
//        velforce(idim,vi) -= v*my::funct_(vi)*my::velint_(idim);
//    }
//  } // end for(idim)
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::hex8>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::hex20>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::hex27>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::tet4>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::tet10>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::wedge6>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::pyramid5>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::quad4>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::quad8>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::quad9>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::tri3>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::tri6>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::nurbs9>;
template class DRT::ELEMENTS::FluidEleCalcPoroP1<DRT::Element::nurbs27>;
