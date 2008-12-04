/*----------------------------------------------------------------------*/
/*!
\file condif2_impl.cpp

\brief Internal implementation of Condif2 element

<pre>
Maintainer: Volker Gravemeier
            vgravem@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15245
</pre>
*/
/*----------------------------------------------------------------------*/

#ifdef D_FLUID2
#ifdef CCADISCRET

#include "condif2_impl.H"
#include "condif2_utils.H"
#include "../drt_mat/convecdiffus.H"
#include "../drt_mat/matlist.H"
#include "../drt_lib/drt_timecurve.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_lib/drt_globalproblem.H"
#include <Epetra_SerialDenseSolver.h>


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Condif2Impl* DRT::ELEMENTS::Condif2Impl::Impl(DRT::ELEMENTS::Condif2* c2)
{
  // we assume here, that numdofpernode is equal for every node within
  // the discretization and does not change during the computations
  const int numdofpernode = c2->NumDofPerNode(*(c2->Nodes()[0]));
  int numscal = numdofpernode;
  if (DRT::Problem::Instance()->ProblemType() == "elch")
    numscal -= 1;

  switch (c2->NumNode())
  {
  case 4:
  {
    static Condif2Impl* f4;
    if (f4==NULL)
      f4 = new Condif2Impl(4,numdofpernode,numscal);
    return f4;
  }
  case 8:
  {
    static Condif2Impl* f8;
    if (f8==NULL)
      f8 = new Condif2Impl(8,numdofpernode,numscal);
    return f8;
  }
  case 9:
  {
    static Condif2Impl* f9;
    if (f9==NULL)
      f9 = new Condif2Impl(9,numdofpernode,numscal);
    return f9;
  }
  case 3:
  {
    static Condif2Impl* f3;
    if (f3==NULL)
      f3 = new Condif2Impl(3,numdofpernode,numscal);
    return f3;
  }
  case 6:
  {
    static Condif2Impl* f6;
    if (f6==NULL)
      f6 = new Condif2Impl(6,numdofpernode,numscal);
    return f6;
  }
  default:
    dserror("node number %d not supported", c2->NumNode());
  }
  return NULL;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Condif2Impl::Condif2Impl(int iel, int numdofpernode, int numscal)
  : iel_(iel),
    numdofpernode_(numdofpernode),
    numscal_(numscal),
    xyze_(2,iel_),
    bodyforce_(iel_*numdofpernode_),
    diffus_(numscal_),
    valence_(numscal_),
    shcacp_(0),
    funct_(iel_),
    densfunct_(iel_),
    deriv_(2,iel_),
    deriv2_(3,iel_),
    xjm_(2,2),
    xij_(2,2),
    derxy_(2,iel_),
    derxy2_(3,iel_),
    rhs_(numdofpernode_),
    hist_(numdofpernode_),
    velint_(2),
    tau_(numscal_),
    kart_(numscal_),
    xder2_(3,2),
    fac_(0),
    conv_(iel_),
    diff_(iel_),
    gradphi_(2),
    lapphi_(2)
{
  return;
}


/*----------------------------------------------------------------------*
 |  calculate system matrix and rhs (public)                    vg 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::Sysmat(
    const DRT::ELEMENTS::Condif2*   ele, ///< the element those matrix is calculated
    const vector<double>&           ephinp, ///< scalar field at n+1
    const vector<double>&           ehist, ///< rhs from beginning of time step
    const vector<double>&           edensnp, ///< density field at n+1
    Epetra_SerialDenseMatrix*       sys_mat,///< element matrix to calculate
    Epetra_SerialDenseVector*       residual, ///< element rhs to calculate
    Epetra_SerialDenseVector&       subgrdiff, ///< subgrid-diff.-scaling vector
    const struct _MATERIAL*         material, ///< material pointer
    const double                    time, ///< current simulation time
    const double                    dt, ///< current time-step length
    const double                    timefac, ///< time discretization factor
    const double                    alphaF, ///< factor for gen.-alpha time int.
    const Epetra_SerialDenseVector& evelnp, ///< nodal velocities at n+1
    const bool                      temperature, ///< temperature flag
    const bool                      conservative, ///< flag for conservative form
    const enum Condif2::TauType     whichtau, ///< flag for stabilization parameter definition
    string                          fssgd, ///< subgrid-diff. flag
    const bool                      is_stationary, ///< stationary flag
    const bool                      is_genalpha ///< generalized-alpha flag
)
{
  const DRT::Element::DiscretizationType distype = ele->Shape();

  // get node coordinates
  for (int i=0;i<iel_;i++)
  {
    xyze_(0,i)=ele->Nodes()[i]->X()[0];
    xyze_(1,i)=ele->Nodes()[i]->X()[1];
  }

  // dead load in element nodes
  BodyForce(ele,time);

  // get diffusivity / diffusivities
  if (material->mattyp == m_matlist)
  {
    for (int k = 0;k<numscal_;++k)
    {
      const int matid = material->m.matlist->matids[k];
      const _MATERIAL& singlemat =  DRT::Problem::Instance()->Material(matid-1);

      if (singlemat.mattyp == m_ion)
      {
        valence_[k]= singlemat.m.ion->valence;
        diffus_[k]= singlemat.m.ion->diffusivity;
     /*   cout<<"MatId: "<<material->m.matlist->matids[k]
        <<" valence["<<k<<"] = "<<valence_[k]
        <<" diffusivity["<<k<<"] = "<<diffus_[k]<<endl;*/
      }
      else if (singlemat.mattyp == m_condif)
        diffus_[k]= singlemat.m.condif->diffusivity;
      else
        dserror("material type is not allowed");
#if 0
      cout<<"MatId: "<<material->m.matlist->matids[k]<<"diffusivity["<<k<<"] = "<<diffus[k]<<endl;
#endif
    }
    // set specific heat capacity at constant pressure to 1.0
    shcacp_ = 1.0;
  }
  else if (material->mattyp == m_condif)
  {
    dsassert(numdofpernode_==1,"more than 1 dof per node for condif material");

    // in case of a temperature equation, we get thermal conductivity instead of
    // diffusivity and have to divide by the specific heat capacity at constant
    // pressure; otherwise, it is the "usual" diffusivity
    if (temperature)
    {
      shcacp_ = material->m.condif->shc;
      diffus_[0] = material->m.condif->diffusivity/shcacp_;
    }
    else
    {
      // set specific heat capacity at constant pressure to 1.0, get diffusivity
      shcacp_ = 1.0;
      diffus_[0] = material->m.condif->diffusivity;
    }
  }
  else
    dserror("Material type is not supported");

  /*----------------------------------------------------------------------*/
  // calculation of stabilization parameter(s) tau
  /*----------------------------------------------------------------------*/
  CalTau(ele,subgrdiff,evelnp,edensnp,distype,dt,timefac,whichtau,fssgd,is_stationary,false);

  /*----------------------------------------------------------------------*/
  // integration loop for one condif2 element
  /*----------------------------------------------------------------------*/

  // flag for higher order elements
  const bool higher_order_ele = SCATRA::is2DHigherOrderElement(distype);

  // gaussian points
  const DRT::UTILS::IntegrationPoints2D intpoints(SCATRA::get2DOptimalGaussrule(distype));

  // integration loop
  for (int iquad=0; iquad<intpoints.nquad; ++iquad)
  {
    EvalShapeFuncAndDerivsAtIntPoint(intpoints,iquad,distype,higher_order_ele,ele);

    // density-weighted shape functions
    for (int j=0; j<iel_; j++)
    {
      densfunct_[j] = funct_[j]*edensnp[j];
    }

    // get (density-weighted) velocity at element center
    for (int i=0;i<2;i++)
    {
      velint_[i]=0.0;
      for (int j=0;j<iel_;j++)
      {
        velint_[i] += funct_[j]*evelnp[i+(2*j)];
      }
    }

    /*------------ get values of variables at integration point */
    for (int k = 0;k<numdofpernode_;++k)     // loop of each transported sclar
    {
      // get history data at integration point (weighted by density)
      hist_[k] = 0;
      for (int j=0;j<iel_;j++)
      {
        hist_[k] += densfunct_[j]*ehist[j*numdofpernode_+k];
      }

      // get bodyforce in gausspoint (divided by shcacp for temperature eq.)
      rhs_[k] = 0;
      for (int inode=0;inode<iel_;inode++)
      {
        rhs_[k]+= (1.0/shcacp_)*bodyforce_[inode*numdofpernode_+k]*funct_[inode];
      }
    }

    /*-------------- perform integration for entire matrix and rhs ---*/
    for (int k=0;k<numscal_;++k) // deal with a system of transported scalars
    {
      if (not is_stationary)
        CalMat(*sys_mat,*residual,ephinp,higher_order_ele,conservative,is_genalpha,timefac,alphaF,k);
      else
        CalMatStationary(*sys_mat,*residual,higher_order_ele,conservative,k);
    } // loop over each scalar

  } // integration loop

  if (numdofpernode_-numscal_== 1) // ELCH
  {
    // testing: set lower-right block to identity matrix:
    for (int vi=0; vi<iel_; ++vi)
    {
        //fac_funct_vi_densfunct_ui = fac_*funct_[vi]*densfunct_[ui];
        (*sys_mat)(vi*numdofpernode_+numscal_, vi*numdofpernode_+numscal_) += 1.0;
    }
  }
  //cout<<*sys_mat<<endl;
  return;
}


/*----------------------------------------------------------------------*
 |  get the body force  (private)                              gjb 06/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::BodyForce(const DRT::ELEMENTS::Condif2* ele, const double time)
{
  vector<DRT::Condition*> myneumcond;

  // check whether all nodes have a unique surface Neumann condition
  DRT::UTILS::FindElementConditions(ele, "SurfaceNeumann", myneumcond);

  if (myneumcond.size()>1)
    dserror("more than one SurfaceNeumann cond on one node");

  if (myneumcond.size()==1)
  {
    // find out whether we will use a time curve
    const vector<int>* curve  = myneumcond[0]->Get<vector<int> >("curve");
    int curvenum = -1;

    if (curve) curvenum = (*curve)[0];

    // initialisation
    double curvefac    = 0.0;

    if (curvenum >= 0) // yes, we have a timecurve
    {
      // time factor for the intermediate step
      if(time >= 0.0)
      {
        curvefac = DRT::UTILS::TimeCurveManager::Instance().Curve(curvenum).f(time);
      }
      else
      {
        // A negative time value indicates an error.
        dserror("Negative time value in body force calculation: time = %f",time);
      }
    }
    else // we do not have a timecurve --- timefactors are constant equal 1
    {
      curvefac = 1.0;
    }

    // get values and switches from the condition
    const vector<int>*    onoff = myneumcond[0]->Get<vector<int> >   ("onoff");
    const vector<double>* val   = myneumcond[0]->Get<vector<double> >("val"  );

    // set this condition to the bodyforce array
    for (int jnode=0; jnode<iel_; jnode++)
    {
      for(int idof=0;idof<numdofpernode_;idof++)
      {
        bodyforce_(jnode*numdofpernode_+idof) = (*onoff)[idof]*(*val)[idof]*curvefac;
      }
    }
  }
  else
  {
    for (int jnode=0; jnode<iel_; jnode++)
    {
      for(int idof=0;idof<numdofpernode_;idof++)
      {
        // we have no dead load
        bodyforce_(jnode*numdofpernode_+idof) = 0.0;
      }
    }
  }

  return;

} //Condif2Impl::BodyForce


/*----------------------------------------------------------------------*
 |  calculate stabilization parameter  (private)              gjb 06/08 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::CalTau(
    const DRT::ELEMENTS::Condif2*&          ele,
    Epetra_SerialDenseVector&               subgrdiff,
    const Epetra_SerialDenseVector&         evel,
    const vector<double>&                   edens,
    const DRT::Element::DiscretizationType& distype,
    const double                            dt,
    const double&                           timefac,
    const enum Condif2::TauType             whichtau,
    string                                  fssgd,
    const bool&                             is_stationary,
    const bool                              initial
  )
{
  // get element-type constant for tau
  double mk=0.0;
  switch (distype)
  {
    case DRT::Element::tri3:
    case DRT::Element::quad4:
      mk = 0.333333333333333333333;
      break;
    case DRT::Element::tri6:
    case DRT::Element::quad8:
    case DRT::Element::quad9:
      mk = 0.083333333333333333333;
      break;
    default:
      dserror("type unknown!\n");
  }

  // use one-point Gauss rule to calculate tau at element center
  DRT::UTILS::GaussRule2D integrationrule_stabili = DRT::UTILS::intrule2D_undefined;
  switch(distype)
  {
    case DRT::Element::quad4:
    case DRT::Element::quad8:
    case DRT::Element::quad9:
      integrationrule_stabili = DRT::UTILS::intrule_quad_1point;
      break;
    case DRT::Element::tri3:
    case DRT::Element::tri6:
      integrationrule_stabili = DRT::UTILS::intrule_tri_1point;
      break;
    default:
      dserror("invalid discretization type");
  }

  // gaussian points
  const DRT::UTILS::IntegrationPoints2D  intpoints_tau(integrationrule_stabili);

  // prepare the standard FE stuff for this single integration point
  // we do not need second derivatives for the calculation of tau
  // EvalShapeFuncAndDerivsAtIntPoint(intpoints_tau,0,distype,false,ele);

  // shape functions and derivs at element center
  const double e1 = intpoints_tau.qxg[0][0];
  const double e2 = intpoints_tau.qxg[0][1];

  // shape functions and their derivatives
  DRT::UTILS::shape_function_2D(funct_,e1,e2,distype);

  // get (density-weighted) velocity at element center
  for (int i=0;i<2;i++)
  {
    velint_[i]=0.0;
    for (int j=0;j<iel_;j++)
    {
      velint_[i] += funct_[j]*evel[i+(2*j)];
    }
  }

  // stabilization parameter definition according to Bazilevs et al. (2007)
  if(whichtau == Condif2::bazilevs)
  {
    // ------------------------------compute inverse of transposed jacobian
    // ---------------------------------------get shapefunction derivatives
    DRT::UTILS::shape_function_2D_deriv1(deriv_,e1,e2,distype);

    /*----------------------------------------- compute Jacobian matrix */
    double dum;
    /*-------------------------------- determine jacobian at point r,s ---*/
    for (int i=0; i<2; i++)
    {
      for (int j=0; j<2; j++)
      {
        dum=0.0;
        for (int l=0; l<iel_; l++)
        {
          dum += deriv_(i,l)*xyze_(j,l);
        }
        xjm_(i,j)=dum;
      } // end of loop j
    } // end of loop i

    // The determinant is computed using Sarrus's rule:
    const double det = xjm_(0,0)*xjm_(1,1)-xjm_(0,1)*xjm_(1,0);

    if (det < 0.0)
      dserror("GLOBAL ELEMENT NO.%i\nNEGATIVE JACOBIAN DETERMINANT: %f", ele->Id(), det);
    if (abs(det) < 1E-16)
      dserror("GLOBAL ELEMENT NO.%i\nZERO JACOBIAN DETERMINANT: %f", ele->Id(), det);

    // ---------------------------------------inverse of transposed jacobian
    xij_(0,0) =  xjm_(1,1)/det;
    xij_(1,0) = -xjm_(1,0)/det;
    xij_(0,1) = -xjm_(0,1)/det;
    xij_(1,1) =  xjm_(0,0)/det;

    /*
                                                                1.0
               +-                                          -+ - ---
               |                                            |   2.0
               | 4.0    n+1       n+1             2         |
        tau  = | --- + u     * G u     + C * kappa  * G : G |
               |   2           -          I           -   - |
               | dt            -                      -   - |
               +-                                          -+

    */

    /*            +-           -+   +-           -+   +-           -+
                  |             |   |             |   |             |
                  |  dr    dr   |   |  ds    ds   |   |  dt    dt   |
            G   = |  --- * ---  | + |  --- * ---  | + |  --- * ---  |
             ij   |  dx    dx   |   |  dx    dx   |   |  dx    dx   |
                  |    i     j  |   |    i     j  |   |    i     j  |
                  +-           -+   +-           -+   +-           -+
    */
    /*            +----
                   \
          G : G =   +   G   * G
          -   -    /     ij    ij
          -   -   +----
                   i,j
    */
    /*                      +----
           n+1       n+1     \     n+1          n+1
          u     * G u     =   +   u    * G   * u
                  -          /     i     -ij    j
                  -         +----        -
                             i,j
    */
    double G;
    double normG = 0;
    double Gnormu = 0;
    for (int nn=0;nn<2;++nn)
    {
      for (int rr=0;rr<2;++rr)
      {
        G = xij_(nn,0)*xij_(rr,0) + xij_(nn,1)*xij_(rr,1) + xij_(nn,2)*xij_(rr,2);
        normG+=G*G;
        Gnormu+=velint_[nn]*G*velint_[rr];
      }
    }

    // definition of constant
    // (Akkerman et al. (2008) used 36.0 for quadratics, but Stefan
    //  brought 144.0 from Austin...)
    const double CI = 12.0/mk;

    // stabilization parameter for instationary case
    if (is_stationary == false)
    {
      double dens = 0.0;
      // get density at element center
      for (int j=0; j<iel_; j++)
      {
        dens += funct_[j]*edens[j];
      }

      for (int k = 0;k<numscal_;++k)
      {
        tau_[k] = 1.0/(sqrt((4.0*dens*dens)/(dt*dt)+Gnormu+CI*diffus_[k]*diffus_[k]*normG));
      }
    }
    // stabilization parameter for stationary case
    else
    {
      for (int k = 0;k<numscal_;++k)
      {
        tau_[k] = 1.0/(sqrt(Gnormu+CI*diffus_[k]*diffus_[k]*normG));
      }
    }

    // compute artificial diffusivity kappa_art_[k] if required
    if (fssgd == "artificial_all" and (not initial))
    {
      // get Euclidean norm of (weighted) velocity at element center
      const double vel_norm = sqrt(DSQR(velint_[0]) + DSQR(velint_[1]));

      for (int k = 0;k<numdofpernode_;++k)
      {
        kart_[k] = DSQR(vel_norm)/(sqrt(Gnormu+CI*diffus_[k]*diffus_[k]*normG));

        for (int vi=0; vi<iel_; ++vi)
        {
          subgrdiff(vi) = kart_[k]/ele->Nodes()[vi]->NumElement();
        }
      } // for k
    } // for artificial diffusivity
  }
  // stabilization parameter definition according to Franca and Valentin (2000)
  else if (whichtau == Condif2::franca_valentin)
  {
    // get characteristic element length: square root of element area
    double area=0;
    double a,b,c;

    switch (distype)
    {
      case DRT::Element::tri3:
      case DRT::Element::tri6:
      {
        a = (xyze_(0,0)-xyze_(0,1))*(xyze_(0,0)-xyze_(0,1))
            +(xyze_(1,0)-xyze_(1,1))*(xyze_(1,0)-xyze_(1,1)); /* line 0-1 squared */
        b = (xyze_(0,1)-xyze_(0,2))*(xyze_(0,1)-xyze_(0,2))
            +(xyze_(1,1)-xyze_(1,2))*(xyze_(1,1)-xyze_(1,2)); /* line 1-2 squared */
        c = (xyze_(0,2)-xyze_(0,0))*(xyze_(0,2)-xyze_(0,0))
            +(xyze_(1,2)-xyze_(1,0))*(xyze_(1,2)-xyze_(1,0)); /* diag 2-0 squared */
        area = 0.25 * sqrt(2.0*a*b + 2.0*b*c + 2.0*c*a - a*a - b*b - c*c);
        break;
      }
      case DRT::Element::quad4:
      case DRT::Element::quad8:
      case DRT::Element::quad9:
      {
        a = (xyze_(0,0)-xyze_(0,1))*(xyze_(0,0)-xyze_(0,1))
            +(xyze_(1,0)-xyze_(1,1))*(xyze_(1,0)-xyze_(1,1)); /* line 0-1 squared */
        b = (xyze_(0,1)-xyze_(0,2))*(xyze_(0,1)-xyze_(0,2))
            +(xyze_(1,1)-xyze_(1,2))*(xyze_(1,1)-xyze_(1,2)); /* line 1-2 squared */
        c = (xyze_(0,2)-xyze_(0,0))*(xyze_(0,2)-xyze_(0,0))
            +(xyze_(1,2)-xyze_(1,0))*(xyze_(1,2)-xyze_(1,0)); /* diag 2-0 squared */
        area = 0.25 * sqrt(2.0*a*b + 2.0*b*c + 2.0*c*a - a*a - b*b - c*c);
        a = (xyze_(0,2)-xyze_(0,3))*(xyze_(0,2)-xyze_(0,3))
            +(xyze_(1,2)-xyze_(1,3))*(xyze_(1,2)-xyze_(1,3)); /* line 2-3 squared */
        b = (xyze_(0,3)-xyze_(0,0))*(xyze_(0,3)-xyze_(0,0))
            +(xyze_(1,3)-xyze_(1,0))*(xyze_(1,3)-xyze_(1,0)); /* line 3-0 squared */
        area += 0.25 * sqrt(2.0*a*b + 2.0*b*c + 2.0*c*a - a*a - b*b - c*c);
        break;
      }
      default: dserror("type unknown!\n");
    }

    const double hk = sqrt(area);

    // get Euclidean norm of (weighted) velocity at element center
    const double vel_norm = sqrt(DSQR(velint_[0]) + DSQR(velint_[1]));

    // some necessary parameter definitions
    double epe1, epe2, xi1, xi2;

    // stabilization parameter for instationary case
    if (is_stationary == false)
    {
      for (int k = 0;k<numscal_;++k)
      {
        // check whether there is zero diffusivity
        if (diffus_[k] == 0.0)
          dserror("diffusivity is zero: Preventing division by zero at evaluation of stabilization parameter");

        /* parameter relating diffusive : reactive forces */
        epe1 = 2.0 * timefac * diffus_[k] / (mk * DSQR(hk));
        /* parameter relating convective : diffusive forces */
        epe2 = mk * vel_norm * hk / diffus_[k];
        xi1 = DMAX(epe1,1.0);
        xi2 = DMAX(epe2,1.0);

        tau_[k] = DSQR(hk)/((DSQR(hk)*xi1)/timefac + (2.0*diffus_[k]/mk)*xi2);
      }
    }
    // stabilization parameter for stationary case
    else
    {
      for (int k = 0;k<numscal_;++k)
      {
        // check whether there is zero diffusivity
        if (diffus_[k] == 0.0)
          dserror("diffusivity is zero: Preventing division by zero at evaluation of stabilization parameter");

        /* parameter relating convective : diffusive forces */
        epe2 = mk * vel_norm * hk / diffus_[k];
        xi2 = DMAX(epe2,1.0);

        tau_[k] = (DSQR(hk)*mk)/(2.0*diffus_[k]*xi2);
      }
    }

    // compute artificial diffusivity kappa_art_[k] if required
    if (fssgd == "artificial_all" and (not initial))
    {
      for (int k = 0;k<numdofpernode_;++k)
      {
        /* parameter relating convective : diffusive forces */
        epe2 = mk * vel_norm * hk / diffus_[k];
        xi2 = DMAX(epe2,1.0);

        kart_[k] = (DSQR(hk)*mk*DSQR(vel_norm))/(2.0*diffus_[k]*xi2);

        for (int vi=0; vi<iel_; ++vi)
        {
          subgrdiff(vi) = kart_[k]/ele->Nodes()[vi]->NumElement();
        }
      } // for k
    } // for artificial diffusivity
  }
  else dserror("unknown definition of tau\n");

  return;
} //Condif2Impl::Caltau


/*----------------------------------------------------------------------*
 | evaluate shape functions and derivatives at int. point     gjb 08/08 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::EvalShapeFuncAndDerivsAtIntPoint(
    const DRT::UTILS::IntegrationPoints2D&  intpoints,        ///< integration points
    const int&                              iquad,            ///< id of current Gauss point
    const DRT::Element::DiscretizationType& distype,          ///< distinguish between DiscretizationType
    const bool&                             higher_order_ele, ///< are second derivatives needed?
    const DRT::ELEMENTS::Condif2*           ele               ///< the element
)
{
  // coordinates of the current integration point
  const double e1 = intpoints.qxg[iquad][0];
  const double e2 = intpoints.qxg[iquad][1];

  // shape functions and their first derivatives
  DRT::UTILS::shape_function_2D(funct_,e1,e2,distype);
  DRT::UTILS::shape_function_2D_deriv1(deriv_,e1,e2,distype);

  /*----------------------------------------- compute Jacobian matrix */
  // get Jacobian matrix and determinant
  // actually compute its transpose....
  /*
    +-            -+ T      +-            -+
    | dx   dx   dx |        | dx   dy   dz |
    | --   --   -- |        | --   --   -- |
    | dr   ds   dt |        | dr   dr   dr |
    |              |        |              |
    | dy   dy   dy |        | dx   dy   dz |
    | --   --   -- |   =    | --   --   -- |
    | dr   ds   dt |        | ds   ds   ds |
    |              |        |              |
    | dz   dz   dz |        | dx   dy   dz |
    | --   --   -- |        | --   --   -- |
    | dr   ds   dt |        | dt   dt   dt |
    +-            -+        +-            -+
   */
  double dum;
  /*-------------------------------- determine jacobian at point r,s ---*/
  for (int i=0; i<2; i++)
  {
    for (int j=0; j<2; j++)
    {
      dum=0.0;
      for (int l=0; l<iel_; l++)
      {
        dum += deriv_(i,l)*xyze_(j,l);
      }
      xjm_(i,j)=dum;
    } // end of loop j
  } // end of loop i

  // The determinant is computed using Sarrus's rule:
  const double det = xjm_(0,0)*xjm_(1,1)-xjm_(0,1)*xjm_(1,0);

  if (det < 0.0)
    dserror("GLOBAL ELEMENT NO.%i\nNEGATIVE JACOBIAN DETERMINANT: %f", ele->Id(), det);
  if (abs(det) < 1E-16)
    dserror("GLOBAL ELEMENT NO.%i\nZERO JACOBIAN DETERMINANT: %f", ele->Id(), det);

  fac_ = intpoints.qwgt[iquad]*det; // Gauss weight * det(J)

  /*------------------------------------------------------------------*/
  /*                                         compute global derivates */
  /*------------------------------------------------------------------*/
  // ---------------------------------------inverse of transposed jacobian
  xij_(0,0) =  xjm_(1,1)/det;
  xij_(1,0) = -xjm_(1,0)/det;
  xij_(0,1) = -xjm_(0,1)/det;
  xij_(1,1) =  xjm_(0,0)/det;

  /*------------------------------------------------- initialization */
  for(int k=0;k<iel_;k++)
  {
    derxy_(0,k)=0.0;
    derxy_(1,k)=0.0;
  }

  // ---------------------------------------- calculate global derivatives
  for (int k=0;k<iel_;k++)
  {
    derxy_(0,k) +=  xij_(0,0) * deriv_(0,k) + xij_(0,1) * deriv_(1,k) ;
    derxy_(1,k) +=  xij_(1,0) * deriv_(0,k) + xij_(1,1) * deriv_(1,k) ;
  }

  // ------------------------------------ compute second global derivatives
  if (higher_order_ele) CalSecondDeriv(e1,e2,distype);

  // say goodbye
  return;
}


/*----------------------------------------------------------------------*
 |  calculate second global derivatives w.r.t. x,y at point r,s (private)
 |                                                             vg 05/07
 |
 | From the three equations
 |
 |              +-             -+
 |  d^2N     d  | dx dN   dy dN |
 |  ----   = -- | --*-- + --*-- |
 |  dr^2     dr | dr dx   dr dy |
 |              +-             -+
 |
 |              +-             -+
 |  d^2N     d  | dx dN   dy dN |
 |  ------ = -- | --*-- + --*-- |
 |  ds^2     ds | ds dx   ds dy |
 |              +-             -+
 |
 |              +-             -+
 |  d^2N     d  | dx dN   dy dN |
 | -----   = -- | --*-- + --*-- |
 | ds dr     ds | dr dx   dr dy |
 |              +-             -+
 |
 | the matrix system
 |
 | +-                                        -+   +-    -+
 | |   /dx\^2        /dy\^2         dy dx     |	  | d^2N |
 | |  | -- |        | ---|        2*--*--     |	  | ---- |
 | |   \dr/	     \dr/ 	    dr dr     |	  | dx^2 |
 | |					      |	  |      |
 | |   /dx\^2        /dy\^2         dy dx     |	  | d^2N |
 | |  | -- |        | -- |        2*--*--     |	* | ---- |
 | |   \ds/	     \ds/ 	    ds ds     |   | dy^2 | =
 | |  					      |	  |      |
 | |   dx dx         dy dy      dx dy   dy dx |	  | d^2N |
 | |   --*--         --*--      --*-- + --*-- |   | ---- |
 | |   dr ds	     dr ds	dr ds   dr ds |	  | dxdy |
 | +-					     -+	  +-    -+
 |
 |             +-    -+   +-                 -+
 | 	       | d^2N |	  | d^2x dN   d^2y dN |
 | 	       | ---- |	  | ----*-- + ----*-- |
 |	       | dr^2 |	  | dr^2 dx   dr^2 dy |
 |	       |      |	  |                   |
 |	       | d^2N |	  | d^2x dN   d^2y dN |
 |          =  | ---- | - | ----*-- + ----*-- |
 |	       | ds^2 |	  | ds^2 dx   ds^2 dy |
 |	       |      |	  |                   |
 |	       | d^2N |	  | d^2x dN   d^2y dN |
 |	       | ---- |	  | ----*-- + ----*-- |
 |	       | drds |	  | drds dx   drds dy |
 |	       +-    -+	  +-                 -+
 |
 |
 | is derived. This is solved for the unknown global derivatives.
 |
 |
 |             jacobian_bar * derxy2 = deriv2 - xder2 * derxy
 |                                              |           |
 |                                              +-----------+
 |                                              'chainrulerhs'
 |                                     |                    |
 |                                     +--------------------+
 |                                          'chainrulerhs'
 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::CalSecondDeriv(
    const double&                            e1,      ///< first coordinate of GP
    const double&                            e2,      ///< second coordinate of GP
    const DRT::Element::DiscretizationType&  distype  ///< distinguish between DiscretizationType
    )
{
  /*--- get the second derivatives of standard element at current GP */
  DRT::UTILS::shape_function_2D_deriv2(deriv2_,e1,e2,distype);


  /*----------- now we have to compute the second global derivatives */
  // initialize and zero out everything
  static Epetra_SerialDenseMatrix bm(3,3);

  /*------------------------------------------------- initialization */
  for(int k=0;k<iel_;k++)
  {
    derxy2_(0,k)=0.0;
    derxy2_(1,k)=0.0;
    derxy2_(2,k)=0.0;
  } /* end of loop over k */

  // calculate elements of jacobian_bar matrix
  bm(0,0) =                     xjm_(0,0)*xjm_(0,0);
  bm(0,1) =                     xjm_(0,1)*xjm_(0,1);
  bm(0,2) =                 2.0*xjm_(0,0)*xjm_(0,1);

  bm(1,0) =                     xjm_(1,0)*xjm_(1,0);
  bm(1,1) =                     xjm_(1,1)*xjm_(1,1);
  bm(1,2) =                 2.0*xjm_(1,1)*xjm_(1,0);

  bm(2,0) =                     xjm_(0,0)*xjm_(1,0);
  bm(2,1) =                     xjm_(0,1)*xjm_(1,1);
  bm(2,2) = xjm_(0,0)*xjm_(1,1)+xjm_(0,1)*xjm_(1,0);

  /*------------------ determine 2nd derivatives of coord.-functions */

  /*
  |                                             0 1
  |         0 1              0...iel-1         +-+-+
  |        +-+-+             +-+-+-+-+         | | | 0
  |        | | | 0           | | | | | 0       +-+-+
  |        +-+-+             +-+-+-+-+         | | | .
  |        | | | 1     =     | | | | | 1     * +-+-+ .
  |        +-+-+             +-+-+-+-+         | | | .
  |        | | | 2           | | | | | 2       +-+-+
  |        +-+-+             +-+-+-+-+         | | | iel-1
  |                                            +-+-+
  |
  |        xder2               deriv2          xyze^T
  |
  |
  |                                     +-           -+
  |                                     | d^2x   d^2y |
  |                                     | ----   ---- |
  |                                     | dr^2   dr^2 |
  |                                     |             |
  |                                     | d^2x   d^2y |
  |                 yields    xder2  =  | ----   ---- |
  |                                     | ds^2   ds^2 |
  |                                     |             |
  |                                     | d^2x   d^2y |
  |                                     | ----   ---- |
  |                                     | drds   drds |
  |                                     +-           -+
  |
  |
  */

  //xder2_ = blitz::sum(deriv2_(i,k)*xyze_(j,k),k);
  for (int i = 0; i < 3; ++i)
  {
      for (int j = 0; j < 2; ++j)
      {
          for (int k = 0; k < iel_; ++k)
          {
              xder2_(i,j) += deriv2_(i,k)*xyze_(j,k);
          }
      }
  }

  /*
  |        0...iel-1             0 1
  |        +-+-+-+-+            +-+-+               0...iel-1
  |        | | | | | 0          | | | 0             +-+-+-+-+
  |        +-+-+-+-+            +-+-+               | | | | | 0
  |        | | | | | 1     =    | | | 1     *       +-+-+-+-+   * (-1)
  |        +-+-+-+-+            +-+-+               | | | | | 1
  |        | | | | | 2          | | | 2             +-+-+-+-+
  |        +-+-+-+-+            +-+-+
  |
  |       chainrulerhs          xder2                 derxy
  */
  //derxy2_ = -blitz::sum(xder2_(i,k)*derxy_(k,j),k);
  //derxy2_ = deriv2 - blitz::sum(xder2(i,k)*derxy(k,j),k);
  for (int i = 0; i < 3; ++i)
  {
      for (int j = 0; j < iel_; ++j)
      {
          derxy2_(i,j) += deriv2_(i,j);
          for (int k = 0; k < 2; ++k)
          {
              derxy2_(i,j) -= xder2_(i,k)*derxy_(k,j);
          }
      }
  }

  /*
  |        0...iel-1             0...iel-1             0...iel-1
  |        +-+-+-+-+             +-+-+-+-+             +-+-+-+-+
  |        | | | | | 0           | | | | | 0           | | | | | 0
  |        +-+-+-+-+             +-+-+-+-+             +-+-+-+-+
  |        | | | | | 1     =     | | | | | 1     +     | | | | | 1
  |        +-+-+-+-+             +-+-+-+-+             +-+-+-+-+
  |        | | | | | 2           | | | | | 2           | | | | | 2
  |        +-+-+-+-+             +-+-+-+-+             +-+-+-+-+
  |
  |       chainrulerhs          chainrulerhs             deriv2
  */
  //derxy2_ += deriv2_;

  /*
  |
  |          0  1  2         i        i
  |        +--+--+--+       +-+      +-+
  |        |  |  |  | 0     | | 0    | | 0
  |        +--+--+--+       +-+	     +-+
  |        |  |  |  | 1  *  | | 1 =  | | 1  for i=0...iel-1
  |        +--+--+--+       +-+	     +-+
  |        |  |  |  | 2     | | 2    | | 2
  |        +--+--+--+       +-+	     +-+
  |                          |        |
  |                          |        |
  |                        derxy2[i]  |
  |                                   |
  |                              chainrulerhs[i]
  |
  |
  |
  |                   0...iel-1
  |                   +-+-+-+-+
  |                   | | | | | 0
  |                   +-+-+-+-+
  |        yields     | | | | | 1
  |                   +-+-+-+-+
  |                   | | | | | 2
  |                   +-+-+-+-+
  |
  |                    derxy2
  |
  */

  Epetra_SerialDenseSolver solver;
  solver.SetMatrix(bm);

  // No need for a separate rhs. We assemble the rhs to the solution
  // vector. The solver will destroy the rhs and return the solution.
  solver.SetVectors(derxy2_,derxy2_);
  solver.Solve();

  return;
} //Condif2Impl::CalSecondDeriv


/*----------------------------------------------------------------------*
 |  evaluate instationary convection-diffusion matrix (private)gjb 06/08|
 *----------------------------------------------------------------------*/

/*
In this routine the Gauss point contributions to the elemental coefficient
matrix of a stabilized condif2 element are calculated for the instationary
case. The procedure is based on the Rothe method of first discretizing in
time. Hence the resulting terms include coefficients containing time
integration variables such as theta or delta t which are represented by
'timefac'.

The stabilization is based on the residuum:

R = rho * c_p * phi + timefac * rho * c_p * u * grad(phi)
                    - timefac * diffus * laplace(phi) - rhsint

The corresponding weighting operators are
L = timefac * rho * c_p * u * grad(w) +/- timefac * diffus * laplace(w)

'+': USFEM (default)
'-': GLS


The calculation proceeds as follows.
1) obtain single operators of R and L
2) build Galerkin terms from them
3) build stabilizing terms from them
4) build Galerkin and stabilizing terms of RHS

NOTE: Galerkin and stabilization matrices are calculated within one
      routine.


for further comments see comment lines within code.

</pre>
\param **estif      DOUBLE        (o)   ele stiffness matrix
\param  *eforce     DOUBLE        (o)   ele force vector
\return void
------------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::CalMat(
    Epetra_SerialDenseMatrix& estif,
    Epetra_SerialDenseVector& eforce,
    const vector<double>&     ephinp,
    const bool                higher_order_ele,
    const bool                conservative,
    const bool                is_genalpha,
    const double&             timefac,
    const double&             alphaF,
    const int&                dofindex
    )
{
// number of degrees of freedom per node
const int numdof = numdofpernode_;

// stabilization parameter and integration factors
const double taufac     = tau_[dofindex]*fac_;
const double timefacfac = timefac*fac_;
const double timetaufac = timefac*taufac;
const double fac_diffus = timefacfac*diffus_[dofindex];

// evaluate rhs at integration point
static double rhsint;
rhsint = hist_[dofindex] + rhs_[dofindex]*(timefac/alphaF);

// convective part in convective form: rho*u_x*N,x+ rho*u_y*N,y
for (int i=0; i<iel_; i++)
{
  conv_[i] = velint_[0] * derxy_(0,i) + velint_[1] * derxy_(1,i);
}

// diffusive part: diffus*(N,xx+ N,yy)
if (higher_order_ele)
{
  for (int i=0; i<iel_; i++)
  {
    diff_[i] = diffus_[dofindex] * (derxy2_(0,i) + derxy2_(1,i));
  }
}

//----------------------------------------------------------------
// element matrix: standard Galerkin terms
//----------------------------------------------------------------
// transient term
for (int vi=0; vi<iel_; ++vi)
{
  const double v = fac_*funct_[vi];
  const int fvi = vi*numdof+dofindex;

  for (int ui=0; ui<iel_; ++ui)
  {
    const int fui = ui*numdof+dofindex;

    estif(fvi,fui) += v*densfunct_[ui];
  }
}

// convective term
if (conservative)
{
  // convective term in conservative form
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = timefacfac*conv_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) -= v*funct_[ui];
    }
  }
}
else
{
  // convective term in convective form
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = timefacfac*funct_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) += v*conv_[ui];
    }
  }
}

// diffusive term
for (int vi=0; vi<iel_; ++vi)
{
  const int fvi = vi*numdof+dofindex;

  for (int ui=0; ui<iel_; ++ui)
  {
    const int fui = ui*numdof+dofindex;

    estif(fvi,fui) += fac_diffus*(derxy_(0, ui)*derxy_(0, vi)+derxy_(1, ui)*derxy_(1, vi));
  }
}

//----------------------------------------------------------------
// element matrix: stabilization terms
//----------------------------------------------------------------
// convective stabilization of transient term (in convective form)
for (int vi=0; vi<iel_; ++vi)
{
  const double v = taufac*conv_[vi];
  const int fvi = vi*numdof+dofindex;

  for (int ui=0; ui<iel_; ++ui)
  {
    const int fui = ui*numdof+dofindex;

    estif(fvi,fui) += v*densfunct_[ui];
  }
}

// convective stabilization of convective term (in convective form)
for (int vi=0; vi<iel_; ++vi)
{
  const double v = timetaufac*conv_[vi];
  const int fvi = vi*numdof+dofindex;

  for (int ui=0; ui<iel_; ++ui)
  {
    const int fui = ui*numdof+dofindex;

    estif(fvi,fui) += v*conv_[ui];
  }
}

// The following stabilization terms are only for higher-order elements.
if (higher_order_ele)
{
  // convective stabilization of diffusive term (in convective form)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = timetaufac*conv_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) -= v*diff_[ui];
    }
  }

  // diffusive stabilization of transient term
  // (USFEM assumed here, sign change necessary for GLS)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = taufac*diff_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) += v*densfunct_[ui];
    }
  }

  // diffusive stabilization of convective term (in convective form)
  // (USFEM assumed here, sign change necessary for GLS)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = timetaufac*diff_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) += v*conv_[ui];
    }
  }

  // diffusive stabilization of diffusive term
  // (USFEM assumed here, sign change necessary for GLS)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = timetaufac*diff_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) -= v*diff_[ui];
    }
  }
}

//----------------------------------------------------------------
// element right hand side: standard Galerkin bodyforce term
//----------------------------------------------------------------
double vrhs = fac_*rhsint;
for (int vi=0; vi<iel_; ++vi)
{
  const int fvi = vi*numdof+dofindex;

  eforce[fvi] += vrhs*funct_[vi];
}

//----------------------------------------------------------------
// element right hand side: stabilization terms
//----------------------------------------------------------------
// convective stabilization of bodyforce term
vrhs = taufac*rhsint;
for (int vi=0; vi<iel_; ++vi)
{
  const int fvi = vi*numdof+dofindex;

  eforce[fvi] += vrhs*conv_[vi];
}

// diffusive stabilization of bodyforce term (only for higher-order elements)
// (USFEM assumed here, sign change necessary for GLS)
if (higher_order_ele)
{
  for (int vi=0; vi<iel_; ++vi)
  {
    const int fvi = vi*numdof+dofindex;

    eforce[fvi] += vrhs*diff_[vi];
  }
}

//----------------------------------------------------------------
// part of element right hand side only required for
// generalized-alpha time integration: temporal terms
//----------------------------------------------------------------
if (is_genalpha)
{
  // integration factors for temporal rhs
  const double rhstimefacfac = timefacfac*(1.0-alphaF)/alphaF;
  const double rhstimetaufac = timetaufac*(1.0-alphaF)/alphaF;

  // gradient of scalar at time step n
  for (int i=0;i<2;i++)
  {
    gradphi_[i]=0.0;
    for (int j=0;j<iel_;j++)
    {
      gradphi_[i] += derxy_(i,j)*ephinp[j];
    }
  }

  // convective part in convective form at time step n
  double convn = velint_[0] * gradphi_[0] + velint_[1] * gradphi_[0];

  // convective temporal rhs term
  if (conservative)
  {
    // scalar at integration point at time step n
    double phi=0.0;
    for (int i=0;i<iel_;i++)
    {
      phi += funct_[i]*ephinp[i];
    }

    // convective temporal rhs term in conservative form
    vrhs = rhstimefacfac*phi;
    for (int vi=0; vi<iel_; ++vi)
    {
      const int fvi = vi*numdof+dofindex;

      eforce[fvi] += vrhs*conv_[vi];
    }
  }
  else
  {
    // convective temporal rhs term in convective form
    vrhs = rhstimefacfac*convn;
    for (int vi=0; vi<iel_; ++vi)
    {
      const int fvi = vi*numdof+dofindex;

      eforce[fvi] -= vrhs*funct_[vi];
    }
  }

  // diffusive temporal rhs term
  vrhs = rhstimefacfac*diffus_[dofindex];
  for (int vi=0; vi<iel_; ++vi)
  {
    const int fvi = vi*numdof+dofindex;

    eforce[fvi] -= vrhs*(derxy_(0, vi)*gradphi_[0]+derxy_(1, vi)*gradphi_[1]);
  }

  // convective stabilization of convective temporal rhs term (in convective form)
  vrhs = rhstimetaufac*convn;
  for (int vi=0; vi<iel_; ++vi)
  {
    const int fvi = vi*numdof+dofindex;

    eforce[fvi] -= vrhs*conv_[vi];
  }

  // The following terms are only for higher-order elements.
  double diffn = 0.0;
  if (higher_order_ele)
  {
    for (int i=0;i<2;i++)
    {
      lapphi_[i]=0.0;
      for (int j=0;j<iel_;j++)
      {
        // second gradient (Laplacian) of scalar at time step n
        lapphi_[i] += derxy2_(i,j)*ephinp[j];
      }
    }

    // diffusive part at time step n
    diffn = diffus_[dofindex] * (lapphi_[0] + lapphi_[1]);

    // diffusive stabilization of convective temporal rhs term (in convective form)
    vrhs = rhstimetaufac*convn;
    for (int vi=0; vi<iel_; ++vi)
    {
      const int fvi = vi*numdof+dofindex;

      eforce[fvi] -= vrhs*diff_[vi];
    }

    // convective stabilization of diffusive temporal rhs term
    vrhs = rhstimetaufac*diffn;
    for (int vi=0; vi<iel_; ++vi)
    {
      const int fvi = vi*numdof+dofindex;

      eforce[fvi] -= vrhs*conv_[vi];
    }

    // diffusive stabilization of diffusive temporal rhs term
    vrhs = rhstimetaufac*diffn;
    for (int vi=0; vi<iel_; ++vi)
    {
      const int fvi = vi*numdof+dofindex;

      eforce[fvi] -= vrhs*diff_[vi];
    }
  }
}

return;
} //Condif2Impl::Condif2CalMat


/*----------------------------------------------------------------------*
 |  evaluate stationary convection-diffusion matrix (private)  gjb 06/08|
 *----------------------------------------------------------------------*/

/*
In this routine the Gauss point contributions to the elemental coefficient
matrix of a stabilized condif2 element are calculated for the stationary
case.

The stabilization is based on the residuum:

R = rho * c_p * u * grad(phi) - diffus *  laplace(phi) - rhsint

The corresponding weighting operators are
L = rho * c_p * u * grad(w) +/- diffus *  laplace(w)

'+': USFEM (default)
'-': GLS


The calculation proceeds as follows.
1) obtain single operators of R and L
2) build Galerkin terms from them
3) build stabilizing terms from them
4) build Galerkin and stabilizing terms of RHS

NOTE: Galerkin and stabilization matrices are calculated within one
      routine.


for further comments see comment lines within code.

</pre>
\param **estif      DOUBLE        (o)   ele stiffness matrix
\param  *eforce     DOUBLE        (o)   ele force vector
\return void
------------------------------------------------------------------------*/

void DRT::ELEMENTS::Condif2Impl::CalMatStationary(
    Epetra_SerialDenseMatrix& estif,
    Epetra_SerialDenseVector& eforce,
    const bool                higher_order_ele,
    const bool                conservative,
    const int&                dofindex
    )
{
// number of degrees of freedom per node
const int numdof = numdofpernode_;

// stabilization parameter and integration factor
const double taufac     = tau_[dofindex]*fac_;
const double fac_diffus = fac_*diffus_[dofindex];

// evaluate rhs at integration point
static double rhsint;
rhsint = rhs_[dofindex];

// convective part in convective form: rho*u_x*N,x+ rho*u_y*N,y
for (int i=0; i<iel_; i++)
{
  conv_[i] = velint_[0] * derxy_(0,i) + velint_[1] * derxy_(1,i);
}

// diffusive part: diffus*(N,xx+ N,yy)
if (higher_order_ele)
{
  for (int i=0; i<iel_; i++)
  {
    diff_[i] = diffus_[dofindex] * (derxy2_(0,i) + derxy2_(1,i));
  }
}

//----------------------------------------------------------------
// element matrix: standard Galerkin terms
//----------------------------------------------------------------
// convective term
if (conservative)
{
  // convective term in conservative form
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = fac_*conv_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) -= v*funct_[ui];
    }
  }
}
else
{
  // convective term in convective form
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = fac_*funct_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) += v*conv_[ui];
    }
  }
}

// diffusive term
for (int vi=0; vi<iel_; ++vi)
{
  const int fvi = vi*numdof+dofindex;

  for (int ui=0; ui<iel_; ++ui)
  {
    const int fui = ui*numdof+dofindex;

    estif(fvi,fui) += fac_diffus*(derxy_(0, ui)*derxy_(0, vi)+derxy_(1, ui)*derxy_(1, vi));
  }
}

//----------------------------------------------------------------
// element matrix: stabilization terms
//----------------------------------------------------------------
// convective stabilization of convective term (in convective form)
for (int vi=0; vi<iel_; ++vi)
{
  const double v = taufac*conv_[vi];
  const int fvi = vi*numdof+dofindex;

  for (int ui=0; ui<iel_; ++ui)
  {
    const int fui = ui*numdof+dofindex;

    estif(fvi,fui) += v*conv_[ui];
  }
}

// The following stabilization terms are only for higher-order elements.
if (higher_order_ele)
{
  // convective stabilization of diffusive term (in convective form)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = taufac*conv_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) -= v*diff_[ui];
    }
  }

  // diffusive stabilization of convective term (in convective form)
  // (USFEM assumed here, sign change necessary for GLS)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = taufac*diff_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) += v*conv_[ui];
    }
  }

  // diffusive stabilization of diffusive term
  // (USFEM assumed here, sign change necessary for GLS)
  for (int vi=0; vi<iel_; ++vi)
  {
    const double v = taufac*diff_[vi];
    const int fvi = vi*numdof+dofindex;

    for (int ui=0; ui<iel_; ++ui)
    {
      const int fui = ui*numdof+dofindex;

      estif(fvi,fui) -= v*diff_[ui];
    }
  }
}

//----------------------------------------------------------------
// element right hand side: standard Galerkin bodyforce term
//----------------------------------------------------------------
double vrhs = fac_*rhsint;
for (int vi=0; vi<iel_; ++vi)
{
  const int fvi = vi*numdof+dofindex;

  eforce[fvi] += vrhs*funct_[vi];
}

//----------------------------------------------------------------
// element right hand side: stabilization terms
//----------------------------------------------------------------
// convective stabilization of bodyforce term
vrhs = taufac*rhsint;
for (int vi=0; vi<iel_; ++vi)
{
  const int fvi = vi*numdof+dofindex;

  eforce[fvi] += vrhs*conv_[vi];
}

// diffusive stabilization of bodyforce term (only for higher-order elements)
// (USFEM assumed here, sign change necessary for GLS)
if (higher_order_ele)
{
  for (int vi=0; vi<iel_; ++vi)
  {
    const int fvi = vi*numdof+dofindex;

    eforce[fvi] += vrhs*diff_[vi];
  }
}

return;
} //Condif2Impl::CalMatStationary


/*----------------------------------------------------------------------*
 | calculate mass matrix + rhs for determ. initial time deriv. gjb 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::InitialTimeDerivative(
    const DRT::ELEMENTS::Condif2*   ele,
    const vector<double>&           ephi0,
    const vector<double>&           edens0,
    Epetra_SerialDenseMatrix&       massmat,
    Epetra_SerialDenseVector&       rhs,
    Epetra_SerialDenseVector&       subgrdiff,
    const struct _MATERIAL*         material,
    const double                    time,
    const double                    dt,
    const double                    timefac,
    const Epetra_SerialDenseVector& evel0,
    const bool                      temperature,
    const bool                      conservative,
    const enum Condif2::TauType     whichtau,
    string                          fssgd
    )
{
  const DRT::Element::DiscretizationType distype = ele->Shape();

  // get node coordinates
  for (int i=0;i<iel_;i++)
  {
    xyze_(0,i)=ele->Nodes()[i]->X()[0];
    xyze_(1,i)=ele->Nodes()[i]->X()[1];
  }

  // dead load in element nodes
  BodyForce(ele,time);

  // get diffusivity / diffusivities
  if (material->mattyp == m_matlist)
  {
    for (int k = 0;k<numscal_;++k)
    {
      const int matid = material->m.matlist->matids[k];
      const _MATERIAL& singlemat =  DRT::Problem::Instance()->Material(matid-1);

      if (singlemat.mattyp == m_ion)
      {
        valence_[k]= singlemat.m.ion->valence;
        diffus_[k]= singlemat.m.ion->diffusivity;
     /*   cout<<"MatId: "<<material->m.matlist->matids[k]
        <<" valence["<<k<<"] = "<<valence_[k]
        <<" diffusivity["<<k<<"] = "<<diffus_[k]<<endl;*/
      }
      else if (singlemat.mattyp == m_condif)
        diffus_[k]= singlemat.m.condif->diffusivity;
      else
        dserror("material type is not allowed");
#if 0
      cout<<"MatId: "<<material->m.matlist->matids[k]<<"diffusivity["<<k<<"] = "<<diffus[k]<<endl;
#endif
    }
    // set specific heat capacity at constant pressure to 1.0
    shcacp_ = 1.0;
  }
  else if (material->mattyp == m_condif)
  {
    dsassert(numdofpernode_==1,"more than 1 dof per node for condif material");

    // in case of a temperature equation, we get thermal conductivity instead of
    // diffusivity and have to divide by the specific heat capacity at constant
    // pressure; otherwise, it is the "usual" diffusivity
    if (temperature)
    {
      shcacp_ = material->m.condif->shc;
      diffus_[0] = material->m.condif->diffusivity/shcacp_;
    }
    else
    {
      shcacp_ = 1.0;
      diffus_[0] = material->m.condif->diffusivity;
    }
  }
  else
    dserror("Material type is not supported");

  /*----------------------------------------------------------------------*/
  // calculation of instationary(!) stabilization parameter(s)
  /*----------------------------------------------------------------------*/
  CalTau(ele,subgrdiff,evel0,edens0,distype,dt,timefac,whichtau,fssgd,false,true);

  /*----------------------------------------------------------------------*/
  // integration loop for one condif2 element
  /*----------------------------------------------------------------------*/

  // flag for higher order elements
  const bool higher_order_ele = SCATRA::is2DHigherOrderElement(distype);

  // gaussian points
  const DRT::UTILS::IntegrationPoints2D intpoints(SCATRA::get2DOptimalGaussrule(distype));

  // integration loop
  for (int iquad=0; iquad<intpoints.nquad; ++iquad)
  {
    EvalShapeFuncAndDerivsAtIntPoint(intpoints,iquad,distype,higher_order_ele,ele);

    // density-weighted shape functions
    for (int j=0; j<iel_; j++)
    {
      densfunct_[j] = funct_[j]*edens0[j];
    }

    // get (density-weighted) velocity at element center
    for (int i=0;i<2;i++)
    {
      velint_[i]=0.0;
      for (int j=0;j<iel_;j++)
      {
        velint_[i] += funct_[j]*evel0[i+(2*j)];
      }
    }

    vector<double> phi0(numdofpernode_);

    /*------------ get values of variables at integration point */
    for (int k = 0;k<numscal_;++k)     // loop of each transported sclar
    {
      // get bodyforce in gausspoint (divided by shcacp for temperature eq.)
      rhs_[k] = 0;
      for (int inode=0;inode<iel_;inode++)
      {
        rhs_[k]+= (1.0/shcacp_)*bodyforce_[inode*numscal_+k]*funct_[inode];
        // note: bodyforce calculation isn't filled with functionality yet.
        // -> this line has no effect since bodyforce is always zero.
      }
    }

    // convective part in convective form: rho*u_x*N,x+ rho*u_y*N,y
    for (int i=0; i<iel_; i++)
    {
      conv_[i] = velint_[0] * derxy_(0,i) + velint_[1] * derxy_(1,i);
    }

    /*-------------- perform integration for entire matrix and rhs ---*/
    for (int dofindex=0;dofindex<numscal_;++dofindex) // deal with a system of transported scalars
    {
      // number of degrees of freedom per node
      const int numdof = numdofpernode_;

      // stabilization parameter  and integration factor
      const double taufac     = tau_[dofindex]*fac_;
      const double fac_diffus = fac_*diffus_[dofindex];

      // evaluate rhs at integration point
      static double rhsint;
      rhsint = rhs_[dofindex];

      // diffusive part: diffus*(N,xx+ N,yy)
      if (higher_order_ele)
      {
        for (int i=0; i<iel_; i++)
        {
          diff_[i] = diffus_[dofindex] * (derxy2_(0,i) + derxy2_(1,i));
        }
      }

      //----------------------------------------------------------------
      // element matrix: standard Galerkin terms
      //----------------------------------------------------------------
      // transient term
      for (int vi=0; vi<iel_; ++vi)
      {
        const double v = fac_*funct_[vi];
        const int fvi = vi*numdof+dofindex;

        for (int ui=0; ui<iel_; ++ui)
        {
          const int fui = ui*numdof+dofindex;

          massmat(fvi,fui) += v*densfunct_[ui];
        }
      }

      // convective term
      if (conservative)
      {
        // convective term in conservative form
        for (int vi=0; vi<iel_; ++vi)
        {
          const double v = fac_*conv_[vi];
          const int fvi = vi*numdof+dofindex;

          for (int ui=0; ui<iel_; ++ui)
          {
            const int fui = ui*numdof+dofindex;

            rhs[fvi] += v*funct_[ui]*ephi0[fui];
          }
        }
      }
      else
      {
        // convective term in convective form
        for (int vi=0; vi<iel_; ++vi)
        {
          const double v = fac_*funct_[vi];
          const int fvi = vi*numdof+dofindex;

          for (int ui=0; ui<iel_; ++ui)
          {
            const int fui = ui*numdof+dofindex;

            rhs[fvi] -= v*conv_[ui]*ephi0[fui];
          }
        }
      }

      // diffusive term
      for (int vi=0; vi<iel_; ++vi)
      {
        const int fvi = vi*numdof+dofindex;

        for (int ui=0; ui<iel_; ++ui)
        {
          const int fui = ui*numdof+dofindex;

          rhs[fvi] -= fac_diffus*(derxy_(0, ui)*derxy_(0, vi)+derxy_(1, ui)*derxy_(1, vi))*ephi0[fui];
        }
      }

      //----------------------------------------------------------------
      // element matrix: stabilization terms
      //----------------------------------------------------------------
      // convective stabilization of transient term (in convective form)
      for (int vi=0; vi<iel_; ++vi)
      {
        const double v = taufac*conv_[vi];
        const int fvi = vi*numdof+dofindex;

        for (int ui=0; ui<iel_; ++ui)
        {
          const int fui = ui*numdof+dofindex;

          massmat(fvi,fui) += v*densfunct_[ui];
        }
      }

      // convective stabilization of convective term (in convective form)
      for (int vi=0; vi<iel_; ++vi)
      {
        const double v = taufac*conv_[vi];
        const int fvi = vi*numdof+dofindex;

        for (int ui=0; ui<iel_; ++ui)
        {
          const int fui = ui*numdof+dofindex;

          rhs[fvi] -= v*conv_[ui]*ephi0[fui];
        }
      }

      // The following stabilization terms are only for higher-order elements.
      if (higher_order_ele)
      {
        // convective stabilization of diffusive term (in convective form)
        for (int vi=0; vi<iel_; ++vi)
        {
          const double v = taufac*conv_[vi];
          const int fvi = vi*numdof+dofindex;

          for (int ui=0; ui<iel_; ++ui)
          {
            const int fui = ui*numdof+dofindex;

            rhs[fvi] += v*diff_[ui]*ephi0[fui];
          }
        }

        // diffusive stabilization of transient term
        // (USFEM assumed here, sign change necessary for GLS)
        for (int vi=0; vi<iel_; ++vi)
        {
          const double v = taufac*diff_[vi];
          const int fvi = vi*numdof+dofindex;

          for (int ui=0; ui<iel_; ++ui)
          {
            const int fui = ui*numdof+dofindex;

            massmat(fvi,fui) += v*densfunct_[ui];
          }
        }

        // diffusive stabilization of convective term (in convective form)
        // (USFEM assumed here, sign change necessary for GLS)
        for (int vi=0; vi<iel_; ++vi)
        {
          const double v = taufac*diff_[vi];
          const int fvi = vi*numdof+dofindex;

          for (int ui=0; ui<iel_; ++ui)
          {
            const int fui = ui*numdof+dofindex;

            rhs[fvi] -= v*conv_[ui]*ephi0[fui];
          }
        }

        // diffusive stabilization of diffusive term
        // (USFEM assumed here, sign change necessary for GLS)
        for (int vi=0; vi<iel_; ++vi)
        {
          const double v = taufac*diff_[vi];
          const int fvi = vi*numdof+dofindex;

          for (int ui=0; ui<iel_; ++ui)
          {
            const int fui = ui*numdof+dofindex;

            rhs[fvi] += v*diff_[ui]*ephi0[fui];
          }
        }
      }

      //----------------------------------------------------------------
      // element right hand side: standard Galerkin bodyforce term
      //----------------------------------------------------------------
      double vrhs = fac_*rhsint;
      for (int vi=0; vi<iel_; ++vi)
      {
        const int fvi = vi*numdof+dofindex;

        rhs[fvi] += vrhs*funct_[vi];
      }

      //----------------------------------------------------------------
      // element right hand side: stabilization terms
      //----------------------------------------------------------------
      // convective stabilization of bodyforce term
      vrhs = taufac*rhsint;
      for (int vi=0; vi<iel_; ++vi)
      {
        const int fvi = vi*numdof+dofindex;

        rhs[fvi] += vrhs*conv_[vi];
      }

      // diffusive stabilization of bodyforce term (only for higher-order elements)
      // (USFEM assumed here, sign change necessary for GLS)
      if (higher_order_ele)
      {
        for (int vi=0; vi<iel_; ++vi)
        {
          const int fvi = vi*numdof+dofindex;

          rhs[fvi] += vrhs*diff_[vi];
        }
      }
    } // loop over each scalar

    if (numdofpernode_-numscal_== 1) // ELCH
    {
      // testing: set lower-right block to identity matrix:
      for (int vi=0; vi<iel_; ++vi)
      {
          const int fvi = vi*numdofpernode_+numscal_;

          massmat(fvi,fvi) += 1.0;
      }
    }

  } // integration loop

  return;
} // Condif2Impl::InitialTimeDerivative


/*----------------------------------------------------------------------*
 | calculate normalized subgrid-diffusivity matrix              vg 10/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Condif2Impl::CalcSubgridDiffMatrix(
    const DRT::ELEMENTS::Condif2* ele,
    Epetra_SerialDenseMatrix&     sys_mat_sd,
    const double                  timefac,
    const bool                    is_stationary
    )
{
const DRT::Element::DiscretizationType distype = ele->Shape();

// get node coordinates
for (int i=0;i<iel_;i++)
{
  xyze_(0,i)=ele->Nodes()[i]->X()[0];
  xyze_(1,i)=ele->Nodes()[i]->X()[1];
}

/*----------------------------------------------------------------------*/
// integration loop for one condif2 element
/*----------------------------------------------------------------------*/
// gaussian points
const DRT::UTILS::IntegrationPoints2D intpoints(SCATRA::get2DOptimalGaussrule(distype));

// integration loop
for (int iquad=0; iquad<intpoints.nquad; ++iquad)
{
  EvalShapeFuncAndDerivsAtIntPoint(intpoints,iquad,distype,false,ele);

  for (int dofindex=0;dofindex<numscal_;++dofindex)
  {
    const int numdof = numdofpernode_;

    // parameter for artificial diffusivity (scaled to one here)
    double kartfac = fac_;
    if (not is_stationary) kartfac *= timefac;

    for (int vi=0; vi<iel_; ++vi)
    {
      const int fvi = vi*numdof+dofindex;

      for (int ui=0; ui<iel_; ++ui)
      {
        const int fui = ui*numdof+dofindex;

        sys_mat_sd(fvi,fui) += kartfac*(derxy_(0,vi)*derxy_(0,ui)+derxy_(1,vi)*derxy_(1,ui));

        /*subtract SUPG term */
        //sys_mat_sd(fvi,fui) -= taufac*conv[vi]*conv[ui] ;
      }
    }
  }
} // integration loop

return;
} // Condif2Impl::CalcSubgridDiffMatrix


#endif
#endif
