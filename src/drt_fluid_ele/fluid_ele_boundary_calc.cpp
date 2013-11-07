/*!----------------------------------------------------------------------
\file fluid_ele_boundary_calc.cpp
\brief

evaluate boundary conditions not requiring parent-element evaluations

<pre>
Maintainers: Ursula Rasthofer & Volker Gravemeier
             {rasthofer,vgravem}@lnm.mw.tum.de
             http://www.lnm.mw.tum.de
             089 - 289-15236/-245
</pre>
*----------------------------------------------------------------------*/


#include "fluid_ele.H"
#include "fluid_ele_boundary_calc.H"
#include "fluid_ele_utils.H"

#include "../drt_inpar/inpar_fluid.H"
#include "../drt_inpar/inpar_fpsi.H"
#include "../drt_inpar/inpar_material.H"

#include "../drt_fem_general/drt_utils_boundary_integration.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_fem_general/drt_utils_nurbs_shapefunctions.H"

#include "../drt_nurbs_discret/drt_nurbs_utils.H"

#include "../drt_geometry/position_array.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/standardtypes_cpp.H"
#include "../drt_lib/drt_utils.H"

#include "../drt_mat/arrhenius_pv.H"
#include "../drt_mat/carreauyasuda.H"
#include "../drt_mat/ferech_pv.H"
#include "../drt_mat/fluidporo.H"
#include "../drt_mat/structporo.H"
#include "../drt_mat/herschelbulkley.H"
#include "../drt_mat/mixfrac.H"
#include "../drt_mat/modpowerlaw.H"
#include "../drt_mat/newtonianfluid.H"
#include "../drt_mat/permeablefluid.H"
#include "../drt_mat/sutherland.H"
#include "../drt_mat/yoghurt.H"

#include "../drt_poroelast/poroelast_utils.H"

#include "../drt_so3/so_poro_interface.H"

std::map<int,std::map<int,DRT::ELEMENTS::FluidBoundaryImplInterface*> * > DRT::ELEMENTS::FluidBoundaryImplInterface::instances_;
/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::FluidBoundaryImplInterface* DRT::ELEMENTS::FluidBoundaryImplInterface::Impl(DRT::ELEMENTS::FluidBoundary* ele)
{
  int num=0;
  DRT::ELEMENTS::Fluid* pele = ele->ParentElement();
  if(DRT::Problem::Instance()->ProblemType() == prb_fpsi and pele->Material()->MaterialType()==INPAR::MAT::m_fluidporo)
    num = (int)INPAR::FPSI::porofluid;
  else
    num = 0;

  switch (ele->Shape())
  {
  case DRT::Element::quad4:
  {
    return FluidBoundaryImpl<DRT::Element::quad4>::Instance(true,num);
  }
  case DRT::Element::quad8:
  {
    return FluidBoundaryImpl<DRT::Element::quad8>::Instance(true,num);
  }
  case DRT::Element::quad9:
  {
    return FluidBoundaryImpl<DRT::Element::quad9>::Instance(true,num);
  }
  case DRT::Element::tri3:
  {
    return FluidBoundaryImpl<DRT::Element::tri3>::Instance(true,num);
  }
  case DRT::Element::tri6:
  {
    return FluidBoundaryImpl<DRT::Element::tri6>::Instance(true,num);
  }
  case DRT::Element::line2:
  {
    return FluidBoundaryImpl<DRT::Element::line2>::Instance(true,num);
  }
  case DRT::Element::line3:
  {
    return FluidBoundaryImpl<DRT::Element::line3>::Instance(true,num);
  }
  case DRT::Element::nurbs2:    // 1D nurbs boundary element
  {
    return FluidBoundaryImpl<DRT::Element::nurbs2>::Instance(true,num);
  }
  case DRT::Element::nurbs3:    // 1D nurbs boundary element
  {
    return FluidBoundaryImpl<DRT::Element::nurbs3>::Instance(true,num);
  }
  case DRT::Element::nurbs4:    // 2D nurbs boundary element
  {
    return FluidBoundaryImpl<DRT::Element::nurbs4>::Instance(true,num);
  }
  case DRT::Element::nurbs9:    // 2D nurbs boundary element
  {
    return FluidBoundaryImpl<DRT::Element::nurbs9>::Instance(true,num);
  }
  default:
    dserror("Element shape %d (%d nodes) not activated. Just do it.", ele->Shape(), ele->NumNode());
    break;
  }
  return NULL;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::FluidBoundaryImplInterface* DRT::ELEMENTS::FluidBoundaryImpl<distype>::Instance(bool create, int num)
{
  if ( create )
  {
    if(static_cast<int>(my::instances_.count(num))==0)
    {
        my::instances_.insert(std::pair<int,std::map<int,DRT::ELEMENTS::FluidBoundaryImplInterface* > * >(num, new std::map<int,DRT::ELEMENTS::FluidBoundaryImplInterface*>));
        my::instances_.at(num)->insert(std::pair<int,DRT::ELEMENTS::FluidBoundaryImpl<distype>* >((int)distype,new FluidBoundaryImpl<distype>(num)));
    }
    else if ( my::instances_.count(num) > 0 and my::instances_.at(num)->count((int)distype) == 0 )
    {
      my::instances_.at(num)->insert(std::pair<int,DRT::ELEMENTS::FluidBoundaryImplInterface*>((int)distype, new FluidBoundaryImpl<distype>(num)));
    }

    return my::instances_.at(num)->at((int)distype);
  }
  else
  {
    if ( my::instances_.at(num)->size())
    {
      delete my::instances_.at(num)->at((int)distype);
      my::instances_.at(num)->erase((int)distype);

      if ( !my::instances_.at(num)->size())
        my::instances_.erase(num);
    }

    return NULL;
  }

  return NULL;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
  Instance( false, num_ );

}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::FluidBoundaryImpl<distype>::FluidBoundaryImpl(int num)
  : DRT::ELEMENTS::FluidBoundaryImplInterface(),
    xyze_(true),
    funct_(true),
    deriv_(true),
    unitnormal_(true),
    velint_(true),
    drs_(0.0),
    fac_(0.0),
    visc_(0.0),
    densaf_(1.0),
    num_(num)
{
  // pointer to class FluidImplParameter (access to the general parameter)
  fldpara_ = DRT::ELEMENTS::FluidEleParameter::Instance(num);

  return;
}



/*----------------------------------------------------------------------*
 |  Integrate a Surface Neumann boundary condition (public)  gammi 04/07|
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::FluidBoundaryImpl<distype>::EvaluateNeumann(
                              DRT::ELEMENTS::FluidBoundary* ele,
                              Teuchos::ParameterList&        params,
                              DRT::Discretization&           discretization,
                              DRT::Condition&                condition,
                              std::vector<int>&              lm,
                              Epetra_SerialDenseVector&      elevec1_epetra,
                              Epetra_SerialDenseMatrix*      elemat1_epetra)
{
  // find out whether we will use a time curve
  bool usetime = true;
  const double time = fldpara_->Time();
  if (time<0.0) usetime = false;

  // get time-curve factor/ n = - grad phi / |grad phi|
  const std::vector<int>* curve  = condition.Get<std::vector<int> >("curve");
  int curvenum = -1;
  if (curve) curvenum = (*curve)[0];
  double curvefac = 1.0;
  if (curvenum>=0 && usetime)
    curvefac = DRT::Problem::Instance()->Curve(curvenum).f(time);

  // get values, switches and spatial functions from the condition
  // (assumed to be constant on element boundary)
  const std::vector<int>*    onoff = condition.Get<std::vector<int> >   ("onoff");
  const std::vector<double>* val   = condition.Get<std::vector<double> >("val"  );
  const std::vector<int>*    func  = condition.Get<std::vector<int> >   ("funct");

  // get time factor for Neumann term
  const double timefac = fldpara_->TimeFacRhs();

  // get Gaussrule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get local node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // get scalar vector
  Teuchos::RCP<const Epetra_Vector> scaaf = discretization.GetState("scaaf");
  if (scaaf==Teuchos::null) dserror("Cannot get state vector 'scaaf'");

  // extract local values from global vector
  std::vector<double> myscaaf(lm.size());
  DRT::UTILS::ExtractMyValues(*scaaf,myscaaf,lm);

  LINALG::Matrix<bdrynen_,1> escaaf(true);

  // insert scalar into element array
  // the scalar is stored to the pressure dof
  for (int inode=0;inode<bdrynen_;++inode)
  {
    escaaf(inode) = myscaaf[(nsd_)+(inode*numdofpernode_)];
  }

  // get thermodynamic pressure at n+1/n+alpha_F
  const double thermpressaf = params.get<double>("thermodynamic pressure",0.0);

  // add potential ALE displacements
  if (ele->ParentElement()->IsAle())
  {
    Teuchos::RCP<const Epetra_Vector>  dispnp;
    std::vector<double>                mydispnp;
    dispnp = discretization.GetState("dispnp");
    if (dispnp != Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }

    for (int inode=0;inode<bdrynen_;++inode)
    {
      for(int idim=0;idim<(nsd_);++idim)
      {
        xyze_(idim,inode) += mydispnp[numdofpernode_*inode+idim];
      }
    }
  }

  // --------------------------------------------------
  // Now do the nurbs specific stuff
  // --------------------------------------------------

  // In the case of nurbs the normal vector is multiplied with normalfac
  double normalfac = 0.0;
  std::vector<Epetra_SerialDenseVector> mypknots(nsd_);
  std::vector<Epetra_SerialDenseVector> myknots (bdrynsd_);
  Epetra_SerialDenseVector weights(bdrynen_);

  // for isogeometric elements --- get knotvectors for parent
  // element and surface element, get weights
  if(IsNurbs<distype>::isnurbs)
  {
     bool zero_size = GetKnotVectorAndWeightsForNurbs(ele, discretization, mypknots, myknots, weights, normalfac);
     if(zero_size)
     {
       return 0;
     }
  }
  /*----------------------------------------------------------------------*
  |               start loop over integration points                     |
  *----------------------------------------------------------------------*/
  for (int gpid=0; gpid<intpoints.IP().nquad; ++gpid)
  {
    // evaluate shape functions and their derivatives,
    // compute unit normal vector and infinitesimal area element drs
    // (evaluation of nurbs-specific stuff not activated here)
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,&myknots,&weights);

    // get the required material information
    Teuchos::RCP<MAT::Material> material = ele->ParentElement()->Material();

    // get density
    // (evaluation always at integration point, in contrast to parent element)
    GetDensity(material,escaaf,thermpressaf);

    //    cout<<"Dens: "<<densaf_<<endl;
    const double fac_curve_time_dens = fac_*curvefac*timefac*densfac_;

    // factor given by spatial function
    double functfac = 1.0;

    // global coordinates of gausspoint
    LINALG::Matrix<(nsd_),1>  coordgp(0.0);

    // determine coordinates of current Gauss point
    coordgp.Multiply(xyze_,funct_);

    // we need a 3D position vector for function evaluation!
    double coordgp3D[3];
    coordgp3D[0]=0.0;
    coordgp3D[1]=0.0;
    coordgp3D[2]=0.0;
    for (int i=0;i<nsd_;i++)
      coordgp3D[i]=coordgp(i);

    int functnum = -1;
    const double* coordgpref = &coordgp3D[0]; // needed for function evaluation

    for(int idim=0; idim<(nsd_); ++idim)
    {
      if((*onoff)[idim])  // Is this dof activated
      {
        if (func) functnum = (*func)[idim];
        {
          if (functnum>0)
          {
            // evaluate function at current gauss point
            functfac = DRT::Problem::Instance()->Funct(functnum-1).Evaluate(idim,coordgpref,time,NULL);
          }
          else functfac = 1.0;
        }
        const double valfac = (*val)[idim]*fac_curve_time_dens*functfac;

        for(int inode=0; inode < bdrynen_; ++inode )
        {
          elevec1_epetra[inode*numdofpernode_+idim] += funct_(inode)*valfac;
        }
      }  // if (*onoff)
    }
  }

  return 0;
}

/*----------------------------------------------------------------------*
 | apply outflow boundary condition which is necessary for the          |
 | conservative element formulation (since the convective term was      |
 | partially integrated)                                                |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ConservativeOutflowConsistency(
    DRT::ELEMENTS::FluidBoundary*  ele,
    Teuchos::ParameterList&         params,
    DRT::Discretization&            discretization,
    std::vector<int>&               lm,
    Epetra_SerialDenseMatrix&       elemat1_epetra,
    Epetra_SerialDenseVector&       elevec1_epetra)
{
  if(fldpara_->TimeAlgo()== INPAR::FLUID::timeint_afgenalpha or
       fldpara_->TimeAlgo()== INPAR::FLUID::timeint_npgenalpha or
       fldpara_->TimeAlgo()== INPAR::FLUID::timeint_one_step_theta)
       dserror("The boundary condition ConservativeOutflowConsistency is not supported by ost/afgenalpha/npgenalpha!!\n"
               "the convective term is not partially integrated!");

  // ------------------------------------
  //     GET TIME INTEGRATION DATA
  // ------------------------------------
  // we use two timefacs for matrix and right hand side to be able to
  // use the method for both time integrations
  const double timefac_mat = params.get<double>("timefac_mat");
  const double timefac_rhs = params.get<double>("timefac_rhs");

  // get status of Ale
  const bool isale = ele->ParentElement()->IsAle();

  // ------------------------------------
  //     GET GENERAL ELEMENT DATA
  // ------------------------------------
  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get global node coordinates
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // ------------------------------------
  // get statevectors from discretisation
  // ------------------------------------

  // extract local displacements from the global vectors
  Teuchos::RCP<const Epetra_Vector>  dispnp;
  std::vector<double>                mydispnp;

  if (isale)
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }

    for (int inode=0;inode<bdrynen_;++inode)
    {
      for(int idim=0;idim<(nsd_);++idim)
      {
        xyze_(idim,inode) += mydispnp[numdofpernode_*inode+idim];
      }
    }
  }

  // extract local velocities from the global vectors
  LINALG::Matrix<nsd_, bdrynen_>   evel(true);

  Teuchos::RCP<const Epetra_Vector> vel = discretization.GetState("u and p (trial)");
  if (vel==Teuchos::null) dserror("Cannot get state vector 'u and p (trial)'");

  // extract local values from the global vectors
  std::vector<double> myvel(lm.size());
  DRT::UTILS::ExtractMyValues(*vel,myvel,lm);

  for (int inode=0;inode<bdrynen_;++inode)
  {
    for (int idim=0; idim<nsd_; ++idim)
    {
      evel(idim,inode) = myvel[numdofpernode_*inode+idim];
    }
  }

  // --------------------------------------------------
  // Now do the nurbs specific stuff
  // --------------------------------------------------

  // In the case of nurbs the normal vector is miultiplied with normalfac
  double normalfac = 0.0;
  std::vector<Epetra_SerialDenseVector> mypknots(nsd_);
  std::vector<Epetra_SerialDenseVector> myknots (bdrynsd_);
  Epetra_SerialDenseVector weights(bdrynen_);

  // for isogeometric elements --- get knotvectors for parent
  // element and surface element, get weights

  if(IsNurbs<distype>::isnurbs)
  {
     bool zero_size = GetKnotVectorAndWeightsForNurbs(ele, discretization, mypknots, myknots, weights, normalfac);
     if(zero_size)
     {
       return;
     }
  }

  /*----------------------------------------------------------------------*
   |               start loop over integration points                     |
   *----------------------------------------------------------------------*/
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurbs specific stuff
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,&myknots,&weights);

    // Multiply the normal vector with the integration factor
    unitnormal_.Scale(fac_);

    // in the case of nurbs the normal vector must be scaled with a special factor
    if (IsNurbs<distype>::isnurbs)
      unitnormal_.Scale(normalfac);

    // get velocity at integration point
    velint_.Multiply(evel,funct_);

    // compute normal flux
    const double u_o_n = velint_.Dot(unitnormal_);

    // rescaled flux (according to time integration)
    const double timefac_mat_u_o_n = timefac_mat*u_o_n;

   // dyadic product of element's normal vector and velocity
   LINALG::Matrix<nsd_,nsd_>  n_x_u(true);

   // dyadic product of u and n
   n_x_u.MultiplyNT(timefac_mat,velint_,unitnormal_);

    /*
              /                \
             |                  |
           + |  Du o n , u o v  |
             |                  |
              \                /
    */

    // fill all velocity elements of the matrix
    for (int ui=0; ui<bdrynen_; ++ui) // loop columns
    {
      //Epetra_SerialDenseMatrix  temp(nsd_,nsd_) = n_x_u (copy);
      LINALG::Matrix<nsd_,nsd_>   temp(n_x_u, false);

      // temp(nsd_,nsd) = n_x_u(nsd_,nsd_)*funct_(ui)
      temp.Scale(funct_(ui));

      for (int idimcol=0; idimcol < (nsd_); ++idimcol) // loop over dimensions for the columns
      {
        const int fui   = numdofpernode_*ui+idimcol;

        for (int vi=0; vi<bdrynen_; ++vi)  // loop rows
        {
          // temp(nsd_,nsd) *= funct_(vi)
          temp.Scale(funct_(vi));

          for (int idimrow = 0; idimrow < nsd_; ++idimrow) // loop over dimensions for the rows
          {
            const int fvi = numdofpernode_*vi+idimrow;
            elemat1_epetra(fvi  ,fui  ) += temp(fvi, fui);
          }  // end loop over dimensions for the rows
        } // end loop over rows (vi)
      } // end oop over dimensions for the columns
    } // end loop over columns (ui)

    /*
              /                \
             |                  |
           + |  u o n , Du o v  |
             |                  |
              \                /
    */

   // fill only diagonal velocity elements of the matrix
   for (int idim=0; idim < (nsd_); ++idim) // loop dimensions
   {
     for (int ui=0; ui<bdrynen_; ++ui) // loop columns
     {
       const int fui   = numdofpernode_*ui+idim;
       const double timefac_mat_u_o_n_funct_ui = timefac_mat_u_o_n*funct_(ui);

       for (int vi=0; vi<bdrynen_; ++vi)  // loop rows
       {
         const int fvi = numdofpernode_*vi + idim;
         const double timefac_mat_u_o_n_funct_ui_funct_vi
                   =
                   timefac_mat_u_o_n_funct_ui*funct_(vi);

         elemat1_epetra(fvi  ,fui  ) += timefac_mat_u_o_n_funct_ui_funct_vi;
       }  // loop rows
     }  // loop columns
   }  //loop over dimensions

  // rhs
  {
    // 3 temp vector
    LINALG::Matrix<nsd_,1>    temp(velint_, false);

    // temp(nsd, nsd_) *= timefac_rhs * u_o_n
    temp.Scale(timefac_rhs*u_o_n);

    for (int vi=0; vi<bdrynen_; ++vi) // loop rows  (test functions)
    {
      for (int idim = 0; idim<(nsd_); ++idim) // loop over dimensions
      {
        int fvi=numdofpernode_*vi + idim;

    /*


                /               \
               |                 |
             + |  u o n , u o v  |
               |                 |
                \               /
    */

        elevec1_epetra(fvi) -= temp(fvi)*funct_(vi);
      } // end loop over dimensions
    } // ui
  } // end rhs
  } // end gaussloop

  return;
}// DRT::ELEMENTS::FluidSurface::SurfaceConservativeOutflowConsistency


/*----------------------------------------------------------------------*
 | compute additional term at Neumann inflow boundary          vg 01/11 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::NeumannInflow(
    DRT::ELEMENTS::FluidBoundary*  ele,
    Teuchos::ParameterList&        params,
    DRT::Discretization&           discretization,
    std::vector<int>&              lm,
    Epetra_SerialDenseMatrix&      elemat1,
    Epetra_SerialDenseVector&      elevec1)
{
  //----------------------------------------------------------------------
  // get control parameters for time integration
  //----------------------------------------------------------------------
  // get timefactor for left-hand side
  // One-step-Theta:    timefac = theta*dt
  // BDF2:              timefac = 2/3 * dt
  // af-genalpha: timefac = (alpha_F/alpha_M) * gamma * dt
  // np-genalpha: timefac = (alpha_F/alpha_M) * gamma * dt
  // genalpha:    timefac =  alpha_F * gamma * dt
  const double timefac = fldpara_->TimeFac();

  // get timefactor for right-hand side
  // One-step-Theta:            timefacrhs = theta*dt
  // BDF2:                      timefacrhs = 2/3 * dt
  // af-genalpha:               timefacrhs = (1/alpha_M) * gamma * dt
  // np-genalpha:               timefacrhs = (1/alpha_M) * gamma * dt
  // genalpha:                  timefacrhs = 1.0
  double timefacrhs = fldpara_->TimeFacRhs();

  // check ALE status
  const bool isale = ele->ParentElement()->IsAle();

  // set flag for type of linearization to default value (fixed-point-like)
  bool is_newton = fldpara_->IsNewton();

  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get global node coordinates for nsd_-dimensional domain
  // (nsd_: number of spatial dimensions of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // add potential ALE displacements
  Teuchos::RCP<const Epetra_Vector>  dispnp;
  std::vector<double>                mydispnp;
  if (isale)
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp != Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }

    for (int inode=0;inode<bdrynen_;++inode)
    {
      for(int idim=0;idim<(nsd_);++idim)
      {
        xyze_(idim,inode) += mydispnp[numdofpernode_*inode+idim];
      }
    }
  }

  // get velocity and scalar vector at time n+alpha_F/n+1
  Teuchos::RCP<const Epetra_Vector> velaf = discretization.GetState("velaf");
  Teuchos::RCP<const Epetra_Vector> scaaf = discretization.GetState("scaaf");
  if (velaf==Teuchos::null or scaaf==Teuchos::null)
    dserror("Cannot get state vector 'velaf' and/or 'scaaf'");

  // extract local values from global vector
  std::vector<double> myvelaf(lm.size());
  std::vector<double> myscaaf(lm.size());
  DRT::UTILS::ExtractMyValues(*velaf,myvelaf,lm);
  DRT::UTILS::ExtractMyValues(*scaaf,myscaaf,lm);

  // create Epetra objects for scalar array and velocities
  LINALG::Matrix<nsd_,bdrynen_> evelaf(true);
  LINALG::Matrix<bdrynen_,1>    escaaf(true);

  // insert velocity and scalar into element array
  for (int inode=0;inode<bdrynen_;++inode)
  {
    for (int idim=0; idim<(nsd_);++idim)
    {
      evelaf(idim,inode) = myvelaf[idim+(inode*numdofpernode_)];
    }
    escaaf(inode) = myscaaf[(nsd_)+(inode*numdofpernode_)];
  }

  // get thermodynamic pressure at n+1/n+alpha_F
  const double thermpressaf = params.get<double>("thermpress at n+alpha_F/n+1");

  // --------------------------------------------------
  // nurbs-specific stuff
  // --------------------------------------------------
  // normal vector multiplied by normalfac for nurbs
  double normalfac = 0.0;
  std::vector<Epetra_SerialDenseVector> mypknots(nsd_);
  std::vector<Epetra_SerialDenseVector> myknots (bdrynsd_);
  Epetra_SerialDenseVector weights(bdrynen_);

  // get knotvectors for parent element and surface element as well as weights
  // for isogeometric elements
  if(IsNurbs<distype>::isnurbs)
  {
     bool zero_size = GetKnotVectorAndWeightsForNurbs(ele, discretization, mypknots, myknots, weights, normalfac);
     if(zero_size)
     {
       return;
     }
  }

  /*----------------------------------------------------------------------*
   |               start loop over integration points                     |
   *----------------------------------------------------------------------*/
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // evaluate shape functions and their derivatives,
    // compute unit normal vector and infinitesimal area element drs
    // (evaluation of nurbs-specific stuff not activated here)
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,&myknots,&weights);

    // normal vector scaled by special factor in case of nurbs
    if (IsNurbs<distype>::isnurbs) unitnormal_.Scale(normalfac);

    // compute velocity vector and normal velocity at integration point
    // (values at n+alpha_F for generalized-alpha scheme, n+1 otherwise)
    double normvel = 0.0;
    velint_.Multiply(evelaf,funct_);
    normvel = velint_.Dot(unitnormal_);

    // check normal velocity -> further computation only required for
    // negative normal velocity, that is, inflow at this Neumann boundary
    if (normvel<-0.0001)
    {
      // get the required material information
      Teuchos::RCP<MAT::Material> material = ele->ParentElement()->Material();

      // get density
      // (evaluation always at integration point, in contrast to parent element)
      GetDensity(material,escaaf,thermpressaf);

      // extended integration factors for left- and right-hand side, respectively
      const double lhsfac = densaf_*normvel*timefac*fac_;
      const double rhsfac = densaf_*normvel*timefacrhs*fac_;

      // compute matrix contribution (fill diagonal elements)
      /*
              /                        \
             |                          |
           - |  v , rho * Du ( u o n )  |
             |                          |
              \                        /
      */
      for (int idim = 0; idim < nsd_; ++idim) // loop over dimensions
      {
        for (int vi=0; vi<bdrynen_; ++vi) // loop over rows
        {
          const double vlhs = lhsfac*funct_(vi);

          const int fvi = numdofpernode_*vi+idim;

          for (int ui=0; ui<bdrynen_; ++ui) // loop over columns
          {
            const int fui = numdofpernode_*ui+idim;

            elemat1(fvi,fui) -= vlhs*funct_(ui);
          } // end loop over columns
        }  // end loop over rows
      }  // end loop over dimensions

      // compute additional matrix contribution for Newton linearization
      if (is_newton)
      {
        // integration factor
        const double lhsnewtonfac = densaf_*timefac*fac_;

        // dyadic product of unit normal vector and velocity vector
        LINALG::Matrix<nsd_,nsd_>  n_x_u(true);
        n_x_u.MultiplyNT(velint_,unitnormal_);

        /*
                /                        \
               |                          |
             - |  v , rho * u ( Du o n )  |
               |                          |
                \                        /

               rho * v_i * u_i * Du_j * n_j

        */
        for (int vi=0; vi<bdrynen_; ++vi) // loop rows
        {
          const double dens_dt_v = lhsnewtonfac*funct_(vi);

          for (int idimrow=0; idimrow < nsd_; ++idimrow) // loop row dim.
          {
            const int fvi = numdofpernode_*vi+idimrow;

            for (int ui=0; ui<bdrynen_; ++ui) // loop columns
            {
              const double dens_dt_v_Du = dens_dt_v * funct_(ui);

              for (int idimcol = 0; idimcol < nsd_; ++idimcol) // loop column dim.
              {
                const int fui = numdofpernode_*ui+idimcol;

                elemat1(fvi,fui) -= dens_dt_v_Du*n_x_u(idimrow,idimcol);
              } // end loop row dimensions
            } // end loop rows
          } // end loop column dimensions
        } // end loop columns
      } // end of Newton loop

      // compute rhs contribution
      LINALG::Matrix<nsd_,1> vrhs(velint_, false);
      vrhs.Scale(rhsfac);

      for (int vi=0; vi<bdrynen_; ++vi) // loop over rows
      {
        for (int idim = 0; idim < nsd_; ++idim)  // loop over dimensions
        {
          const int fvi = numdofpernode_*vi+idim;

          elevec1(fvi) += funct_(vi)*vrhs(idim);
        } // end loop over dimensions
      }  // end loop over rows
    }
  }

  return;
}// DRT::ELEMENTS::FluidSurface::NeumannInflow


/*----------------------------------------------------------------------*
 |  Integrate shapefunctions over surface (private)            gjb 07/07|
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::IntegrateShapeFunction(
                  DRT::ELEMENTS::FluidBoundary* ele,
                  Teuchos::ParameterList& params,
                  DRT::Discretization&       discretization,
                  std::vector<int>&          lm,
                  Epetra_SerialDenseVector&  elevec1,
                  const std::vector<double>& edispnp)
{
  // get status of Ale
  const bool isale = ele->ParentElement()->IsAle();

  // get Gaussrule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_, LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  if (isale)
  {
    dsassert(edispnp.size()!=0,"paranoid");

    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0;idim<(nsd_);++idim)
      {
        xyze_(idim,inode) += edispnp[numdofpernode_*inode+idim];
      }
    }
  }

  /*----------------------------------------------------------------------*
  |               start loop over integration points                     |
  *----------------------------------------------------------------------*/
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points is not activated here
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    for (int inode=0;inode<bdrynen_;++inode)
    {
      for(int idim=0;idim<(nsd_);idim++)
      {
        elevec1(inode*numdofpernode_+idim)+= funct_(inode) * fac_;
      }
    }

  } /* end of loop over integration points gpid */


return;
} // DRT::ELEMENTS::FluidSurface::IntegrateShapeFunction


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ElementNodeNormal(
                                                     DRT::ELEMENTS::FluidBoundary*   ele,
                                                     Teuchos::ParameterList&          params,
                                                     DRT::Discretization&             discretization,
                                                     std::vector<int>&                lm,
                                                     Epetra_SerialDenseVector&        elevec1,
                                                     const std::vector<double>&       edispnp)
{
  const bool isale = ele->ParentElement()->IsAle();

  //get gaussrule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  if (isale)
  {
    dsassert(edispnp.size()!=0,"paranoid");

    for (int inode=0;inode<bdrynen_; ++inode)
    {
      for (int idim=0;idim<(nsd_); ++idim)
      {
        xyze_(idim,inode) += edispnp[numdofpernode_*inode+idim];
      }
    }
  }

  /*----------------------------------------------------------------------*
   |               start loop over integration points                     |
   *----------------------------------------------------------------------*/

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    for (int inode=0; inode<bdrynen_; ++inode)
    {
      for(int idim=0; idim<nsd_; ++idim)
      {
        elevec1(inode*numdofpernode_+idim) += unitnormal_(idim) * funct_(inode) * fac_;
      }
      // pressure dof is set to zero
      elevec1(inode*numdofpernode_+(nsd_)) = 0.0;
    }
  } /* end of loop over integration points gpid */

  return;

} // DRT::ELEMENTS::FluidSurface::ElementNodeNormal


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ElementMeanCurvature(
                                                        DRT::ELEMENTS::FluidBoundary*    ele,
                                                        Teuchos::ParameterList&           params,
                                                        DRT::Discretization&              discretization,
                                                        std::vector<int>&                 lm,
                                                        Epetra_SerialDenseVector&         elevec1,
                                                        const std::vector<double>&        edispnp,
                                                        std::vector<double>&              enormals)
{
  // get status of Ale
  const bool isale = ele->ParentElement()->IsAle();

  // get Gauss rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // node normals &
  LINALG::Matrix<nsd_,bdrynen_> norm_elem(true);
  LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(true);

  // coordinates of current node in reference coordinates
  LINALG::Matrix<bdrynsd_,1> xsi_node(true);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  if (isale)
  {
    dsassert(edispnp.size()!=0,"paranoid");

    for (int inode=0; inode<bdrynen_; ++inode)
    {
      for (int idim=0;idim<nsd_; ++idim)
      {
        xyze_(idim,inode) += edispnp[numdofpernode_*inode+idim];
      }
    }
  }

  // set normal vectors to length = 1.0
  // normal vector is coming from outside
  for (int inode=0; inode<bdrynen_; ++inode)
  {
    //double length = 0.0;
    for (int idim=0; idim < nsd_; ++idim)
    {
      norm_elem(idim,inode) = enormals[numdofpernode_*inode+idim];
    }
  }
  // compute normalized normal vector
  norm_elem.Scale(1/norm_elem.Norm2());

  // get local node coordinates of the element
  // function gives back a matrix with the local node coordinates of the element (nsd_,bdrynen_)
  // the function gives back an Epetra_SerialDenseMatrix!!!
  Epetra_SerialDenseMatrix xsi_ele = DRT::UTILS::getEleNodeNumbering_nodes_paramspace(distype);

  // ============================== loop over nodes ==========================
  for (int inode=0;inode<bdrynen_; ++inode)
  {
    // the local node coordinates matrix is split to a vector containing the local coordinates of the actual node
    for (int idim = 0; idim < bdrynsd_; idim++)
    {
      xsi_node(idim) = xsi_ele(idim,inode);
    }

    // get shape derivatives at this node
    // shape_function_2D_deriv1(deriv_, e0, e1, distype);
    DRT::UTILS::shape_function<distype>(xsi_node,funct_);

    // the metric tensor and its determinant
    //Epetra_SerialDenseMatrix      metrictensor(nsd_,nsd_);
    LINALG::Matrix<bdrynsd_,bdrynsd_> metrictensor(true);

    // Addionally, compute metric tensor
    DRT::UTILS::ComputeMetricTensorForBoundaryEle<distype>(xyze_,deriv_,metrictensor,drs_);

    dxyzdrs.MultiplyNT(deriv_,xyze_);

    // calculate mean curvature H at node.
    double H = 0.0;
    LINALG::Matrix<bdrynsd_,nsd_> dn123drs(0.0);

    dn123drs.MultiplyNT(deriv_,norm_elem);

    //Acc. to Bronstein ..."mittlere Kruemmung":
    // calculation of the mean curvature for a surface element
    if (bdrynsd_==2)
    {
      double L = 0.0, twoM = 0.0, N = 0.0;
      for (int i=0;i<3;i++)
      {
        L += (-1.0) * dxyzdrs(0,i) * dn123drs(0,i);
        twoM += (-1.0) * dxyzdrs(0,i) * dn123drs(1,i) - dxyzdrs(1,i) * dn123drs(0,i);
        N += (-1.0) * dxyzdrs(1,i) * dn123drs(1,i);
      }
      //mean curvature: H = 0.5*(k_1+k_2)
      H = 0.5 *
          (metrictensor(0,0)*N - twoM*metrictensor(0,1) + metrictensor(1,1)*L)
          / (drs_*drs_);
    }
    else
     dserror("Calcualtion of the mean curvature is only implemented for a 2D surface element");


    // get the number of elements adjacent to this node. Find out how many
    // will contribute to the interpolated mean curvature value.
    int contr_elements = 0;
    DRT::Node* thisNode = (ele->Nodes())[inode];
#ifdef DEBUG
    if (thisNode == NULL) dserror("No node!\n");
#endif
    int NumElement = thisNode->NumElement();
    DRT::Element** ElementsPtr = thisNode->Elements();

    // loop over adjacent Fluid elements
    for (int ele=0;ele<NumElement;ele++)
    {
      DRT::Element* Element = ElementsPtr[ele];

      // get surfaces
      std::vector< RCP< DRT::Element > > surfaces = Element->Surfaces();

      // loop over surfaces: how many free surfaces with this node on it?
      for (unsigned int surf=0; surf<surfaces.size(); ++surf)
      {
        Teuchos::RCP< DRT::Element > surface = surfaces[surf];
        DRT::Node** NodesPtr = surface->Nodes();
        int numfsnodes = 0;
        bool hasthisnode = false;

        for (int surfnode = 0; surfnode < surface->NumNode(); ++surfnode)
        {
          DRT::Node* checkNode = NodesPtr[surfnode];
          // check whether a free surface condition is active on this node
          if (checkNode->GetCondition("FREESURFCoupling") != NULL)
          {
            numfsnodes++;
          }
          if (checkNode->Id() == thisNode->Id())
          {
            hasthisnode = true;
          }
        }

        if (numfsnodes == surface->NumNode() and hasthisnode)
        {
          // this is a free surface adjacent to this node.
          contr_elements++;
        }

      }

    }
#ifdef DEBUG
    if (!contr_elements) dserror("No contributing elements found!\n");
#endif

    for(int idim=0; idim<nsd_; ++idim)
    {
      elevec1[inode*numdofpernode_+idim] = H / contr_elements;
    }
    elevec1[inode*numdofpernode_+(numdofpernode_-1)] = 0.0;
  } // END: loop over nodes

} // DRT::ELEMENTS::FluidSurface::ElementMeanCurvature



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ElementSurfaceTension(
                                                         DRT::ELEMENTS::FluidBoundary*   ele,
                                                         Teuchos::ParameterList&          params,
                                                         DRT::Discretization&             discretization,
                                                         std::vector<int>&                lm,
                                                         Epetra_SerialDenseVector&        elevec1,
                                                         const std::vector<double>&       edispnp,
                                                         std::vector<double>&             enormals,
                                                         std::vector<double>&             ecurvature)
                                                         // Attention: mynormals and mycurvature are not used in the function
{
  // get status of Ale
  const bool isale = ele->ParentElement()->IsAle();

  // get timefactor for left-hand side
  // One-step-Theta:    timefac = theta*dt
  // BDF2:              timefac = 2/3 * dt
  // af-genalpha: timefac = (alpha_F/alpha_M) * gamma * dt
  // np-genalpha: timefac = (alpha_F/alpha_M) * gamma * dt
  // genalpha:    timefac =  alpha_F * gamma * dt
  const double timefac = fldpara_->TimeFac();

  // isotropic and isothermal surface tension coefficient
  double SFgamma = 0.0;
  // get material data
  Teuchos::RCP<MAT::Material> mat = ele->ParentElement()->Material();
  if (mat==Teuchos::null)
    dserror("no mat from parent!");
  else if (mat->MaterialType()==INPAR::MAT::m_fluid)
  {
    const MAT::NewtonianFluid* actmat = static_cast<const MAT::NewtonianFluid*>(mat.get());
    SFgamma = actmat->Gamma();
  }
  else
    dserror("Newtonian fluid material expected but got type %d", mat->MaterialType());

  // get Gauss rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  if (isale)
  {
    dsassert(edispnp.size()!=0,"paranoid");

    for (int inode=0; inode<bdrynen_; ++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode) += edispnp[numdofpernode_*inode+idim];
      }
    }
  }

  /*----------------------------------------------------------------------*
   |               start loop over integration points                     |
   *----------------------------------------------------------------------*/

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    // fac multiplied by the timefac
    const double fac_timefac = fac_ * timefac;

    // Compute dxyzdrs
    LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(true);
    dxyzdrs.MultiplyNT(deriv_,xyze_);

    if (bdrynsd_==2)
    {
      double abs_dxyzdr = 0.0;
      double abs_dxyzds = 0.0;
      double pointproduct_rs = 0.0;

      for (int dim=0;dim<3;dim++)
      {
        abs_dxyzdr += dxyzdrs(0,dim) * dxyzdrs(0,dim);
        abs_dxyzds += dxyzdrs(1,dim) * dxyzdrs(1,dim);
        pointproduct_rs += dxyzdrs(0,dim) * dxyzdrs(1,dim);
      }
      abs_dxyzdr = sqrt(abs_dxyzdr);
      abs_dxyzds = sqrt(abs_dxyzds);

      for (int node=0;node<bdrynen_;++node)
      {
        for (int dim=0;dim<3;dim++)
        {
          // Right hand side Integral (SFgamma * -Surface_Gradient, weighting
          // function) on Gamma_FS
          // See Saksono eq. (26)
          // discretized as surface gradient * ( Shapefunction-Matrix
          // transformed )

          // This uses a surface_gradient extracted from gauss general
          // formula for 2H...
          // this gives convincing results with TET elements, but HEX
          // elements seem more difficult -> due to edge problems?
          // too many nonlinear iterations
          elevec1[node*numdofpernode_+dim] += SFgamma *
                                     (-1.0) / (
                                       drs_ * drs_ //= abs_dxyzdr * abs_dxyzdr * abs_dxyzds * abs_dxyzds - pointproduct_rs * pointproduct_rs
                                       )
                                     *
                                     (
                                       abs_dxyzds * abs_dxyzds * deriv_(0,node) * dxyzdrs(0,dim)
                                       - pointproduct_rs * deriv_(0,node) * dxyzdrs(1,dim)
                                       - pointproduct_rs * deriv_(1,node) * dxyzdrs(0,dim)
                                       + abs_dxyzdr * abs_dxyzdr * deriv_(1,node) * dxyzdrs(1,dim)
                                       )
                                     * fac_timefac;

        }
        elevec1[node*numdofpernode_+3] = 0.0;
      }
    } // end if (nsd_==2)
    else if (bdrynsd_==1)
    {
      for (int inode=0;inode<bdrynen_;++inode)
      {
         for(int idim=0;idim<2;idim++)
         {
            // Right hand side Integral (SFgamma * -Surface_Gradient, weighting
            // function) on Gamma_FS
            // See Saksono eq. (26)
            // discretized as surface gradient * ( Shapefunction-Matrix
            // transformed )
            // 2D: See Slikkerveer ep. (17)
            elevec1[inode*numdofpernode_+idim]+= SFgamma / drs_ / drs_ *
                                      (-1.0) * deriv_(0, inode) * dxyzdrs(0,idim)
                                      * fac_timefac;
         }
      }
    } // end if else (nsd_=1)
    else
      dserror("There are no 3D boundary elements implemented");
  } /* end of loop over integration points gpid */
} // DRT::ELEMENTS::FluidSurface::ElementSurfaceTension

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::AreaCalculation(
  DRT::ELEMENTS::FluidBoundary*  ele,
  Teuchos::ParameterList&        params,
  DRT::Discretization&           discretization,
  std::vector<int>&              lm)
{
  //------------------------------------------------------------------
  // get and set density and viscosity (still required for following routines:
  // FluidImpedanceBc/FluidVolumetricSurfaceFlowBc/Fluid_couplingBc::Area)
  //------------------------------------------------------------------
  Teuchos::RCP<MAT::Material> mat = ele->ParentElement()->Material();
  if(mat->MaterialType()== INPAR::MAT::m_fluid)
  {
    const MAT::NewtonianFluid* actmat = static_cast<const MAT::NewtonianFluid*>(mat.get());
    densaf_ = actmat->Density();
    visc_   = actmat->Viscosity();
  }
  else if(mat->MaterialType()== INPAR::MAT::m_permeable_fluid)
  {
    const MAT::PermeableFluid* actmat = static_cast<const MAT::PermeableFluid*>(mat.get());
    densaf_ = actmat->Density();
    visc_   = actmat->SetViscosity();
  }
  params.set<double>("density",   densaf_);
  params.set<double>("viscosity", visc_);
  //------------------------------------------------------------------
  // end of get and set density and viscosity
  //------------------------------------------------------------------

  //------------------------------------------------------------------
  // start of actual area calculation
  //------------------------------------------------------------------
  // get node coordinates (nsd_: dimension of boundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

#ifdef D_ALE_BFLOW
  // add potential ALE displacements
  Teuchos::RCP<const Epetra_Vector>  dispnp;
  std::vector<double>                mydispnp;
  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }

    dsassert(mydispnp.size()!=0,"paranoid");
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }
#endif // D_ALE_BFLOW

  // get initial value for area
  double area = params.get<double>("area");

  // get Gauss rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // loop over integration points
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    // add to area integral
    area += fac_;
  }

  // set final value for area
  params.set<double>("area",area);
  //------------------------------------------------------------------
  // end of actual area calculation
  //------------------------------------------------------------------

}//DRT::ELEMENTS::FluidSurface::AreaCalculation


/*----------------------------------------------------------------------*
 |                                                       ismail 04/2010 |
 |                                                           vg 06/2013 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::PressureBoundaryIntegral(
  DRT::ELEMENTS::FluidBoundary*    ele,
  Teuchos::ParameterList&          params,
  DRT::Discretization&             discretization,
  std::vector<int>&                lm)
{
  // extract pressure values from global velocity/pressure vector
  Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");
  if (velnp == Teuchos::null) dserror("Cannot get state vector 'velnp'");

  std::vector<double> myvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);

  LINALG::Matrix<1,bdrynen_> eprenp(true);
  for (int inode=0;inode<bdrynen_;inode++)
  {
    eprenp(inode) = myvelnp[nsd_+inode*numdofpernode_];
  }

  // get node coordinates (nsd_: dimension of boundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

#ifdef D_ALE_BFLOW
  // add potential ALE displacements
  Teuchos::RCP<const Epetra_Vector>  dispnp;
  std::vector<double>                mydispnp;
  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"paranoid");
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }
#endif // D_ALE_BFLOW

  // get initial value for pressure boundary integral
  double press_int = params.get<double>("pressure boundary integral");

  // get Gauss rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // loop over integration points
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    // add to pressure boundary integral
    for (int inode=0;inode<bdrynen_;++inode)
    {
      press_int += funct_(inode) * eprenp(inode) *fac_;
    }
  }

  // set final value for pressure boundary integral
  params.set<double>("pressure boundary integral",press_int);

}//DRT::ELEMENTS::FluidSurface::PressureBoundaryIntegral


/*----------------------------------------------------------------------*
 |                                                        ismail 10/2010|
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::CenterOfMassCalculation(
  DRT::ELEMENTS::FluidBoundary*    ele,
  Teuchos::ParameterList&           params,
  DRT::Discretization&              discretization,
  std::vector<int>&                 lm)
{

  //------------------------------------------------------------------
  // This calculates the integrated the pressure from the
  // the actual pressure values
  //------------------------------------------------------------------
#if 1
  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  //GEO::fillInitialPositionArray<distype,nsd_,Epetra_SerialDenseMatrix>(ele,xyze_);
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

#ifdef D_ALE_BFLOW
  // Add the deformation of the ALE mesh to the nodes coordinates
  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;

  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"paranoid");
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }
#endif // D_ALE_BFLOW

  // first evaluate the area of the surface element
  params.set<double>("area",0.0);
  this->AreaCalculation(ele, params, discretization,lm);

  // get the surface element area
  const double elem_area = params.get<double>("area");

  LINALG::Matrix<(nsd_),1>  xyzGe(true);

  for (int i = 0; i< nsd_;i++)
  {
    //const IntegrationPoints2D  intpoints(gaussrule);
    for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
    {
      // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
      // Computation of the unit normal vector at the Gauss points
      // Computation of nurb specific stuff is not activated here
      EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

      // global coordinates of gausspoint
      LINALG::Matrix<(nsd_),1>  coordgp(true);

      // determine coordinates of current Gauss point
      coordgp.Multiply(xyze_,funct_);

      //Compute elment center of gravity
      xyzGe(i) += intpoints.IP().qwgt[gpid]*coordgp(i)*drs_;

    }  // end Gauss loop
    xyzGe(i) /= elem_area;
  }

  // Get the center of mass of the already calculate surface elements
  Teuchos::RCP<std::vector<double> > xyzG  = params.get<RCP<std::vector<double> > >("center of mass");

  Teuchos::RCP<std::vector<double> > normal  = params.get<RCP<std::vector<double> > >("normal");

  // Get the area of the of the already calculate surface elements
  double area = params.get<double>("total area");

  for (int i = 0; i<nsd_;i++)
  {
    (*xyzG)  [i] = ((*xyzG)[i]*area   + xyzGe(i)     *elem_area)/(area+elem_area);
    (*normal)[i] = ((*normal)[i]*area + unitnormal_(i)*elem_area)/(area+elem_area);
  }

  // set new center of mass
  params.set("total area", area+elem_area);

#endif
}//DRT::ELEMENTS::FluidSurface::CenterOfMassCalculation



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ComputeFlowRate(
                                                                DRT::ELEMENTS::FluidBoundary*    ele,
                                                                Teuchos::ParameterList&           params,
                                                                DRT::Discretization&              discretization,
                                                                std::vector<int>&                 lm,
                                                                Epetra_SerialDenseVector&         elevec1)
{
  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // extract local values from the global vectors
  Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");

  if (velnp==Teuchos::null)
    dserror("Cannot get state vector 'velnp'");

  std::vector<double> myvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);

  // allocate velocity vector
  LINALG::Matrix<nsd_,bdrynen_> evelnp(true);

  // split velocity and pressure, insert into element arrays
  for (int inode=0;inode<bdrynen_;inode++)
  {
    for (int idim=0; idim< nsd_; idim++)
    {
      evelnp(idim,inode) = myvelnp[idim+(inode*numdofpernode_)];
    }
  }

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  //GEO::fillInitialPositionArray<distype,nsd_,Epetra_SerialDenseMatrix>(ele,xyze_);
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

#ifdef D_ALE_BFLOW
  // Add the deformation of the ALE mesh to the nodes coordinates
  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;

  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"paranoid");
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }
#endif // D_ALE_BFLOW


  //const IntegrationPoints2D  intpoints(gaussrule);
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    //compute flowrate at gauss point
    velint_.Multiply(evelnp,funct_);

    // flowrate = uint o normal
    const double flowrate = velint_.Dot(unitnormal_);

    // store flowrate at first dof of each node
    // use negative value so that inflow is positiv
    for (int inode=0;inode<bdrynen_;++inode)
    {
      // see "A better consistency for low order stabilized finite element methods"
      // Jansen, Collis, Whiting, Shakib
      //
      // Here the principle is used to bring the flow rate to the outside world!!
      //
      // funct_ *  velint * n * fac
      //   |      |________________|
      //   |              |
      //   |         flow rate * fac  -> integral over Gamma
      //   |
      // flow rate is distributed to the single nodes of the element
      // = flow rate per node
      //
      // adding up all nodes (ghost elements are handled by the assembling strategy)
      // -> total flow rate at the desired boundary
      //
      // it can be interpreted as a rhs term
      //
      //  ( v , u o n)
      //               Gamma
      //
      elevec1[inode*numdofpernode_] += funct_(inode)* fac_ * flowrate;

      // alternative way:
      //
      //  velint * n * fac
      // |________________|
      //         |
      //    flow rate * fac  -> integral over Gamma
      //     = flow rate per element
      //
      //  adding up all elements (be aware of ghost elements!!)
      //  -> total flow rate at the desired boundary
      //     (is identical to the total flow rate computed above)
    }
  }
}//DRT::ELEMENTS::FluidSurface::ComputeFlowRate


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::FlowRateDeriv(
                                                 DRT::ELEMENTS::FluidBoundary*   ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                lm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseMatrix&        elemat2,
                                                 Epetra_SerialDenseVector&        elevec1,
                                                 Epetra_SerialDenseVector&        elevec2,
                                                 Epetra_SerialDenseVector&        elevec3)
{
  // This function is only implemented for 3D
  if(bdrynsd_!=2)
    dserror("FlowRateDeriv is only implemented for 3D!");

  // get status of Ale
  const bool isale = ele->ParentElement()->IsAle();

  Teuchos::RCP<const Epetra_Vector> dispnp;
  std::vector<double> edispnp;

  if (isale)
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp==Teuchos::null) dserror("Cannot get state vectors 'dispnp'");
    edispnp.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*dispnp,edispnp,lm);
  }

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // order of accuracy of grid velocity determination
  const Teuchos::ParameterList& fdyn = DRT::Problem::Instance()->FluidDynamicParams();
  const int gridvel = DRT::INPUT::IntegralValue<INPAR::FLUID::Gridvel>(fdyn, "GRIDVEL");

  // normal vector
  LINALG::Matrix<nsd_,1> normal(true);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  if (isale)
  {
    dsassert(edispnp.size()!=0,"paranoid");

    for (int inode=0; inode<bdrynen_; ++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode) += edispnp[numdofpernode_*inode+idim];
      }
    }
  }

  // get nodal velocities and pressures
  Teuchos::RCP<const Epetra_Vector> convelnp = discretization.GetState("convectivevel");

  if (convelnp==Teuchos::null)
    dserror("Cannot get state vector 'convectivevel'");

  // extract local values from the global vectors
  std::vector<double> myconvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*convelnp,myconvelnp,lm);

  // allocate velocities vector
  LINALG::Matrix<nsd_,bdrynen_> evelnp(true);

  for (int inode=0; inode<bdrynen_; ++inode)
  {
    for (int idim=0;idim<nsd_; ++idim)
    {
      evelnp(idim,inode) = myconvelnp[(numdofpernode_*inode)+idim];
    }
  }


  /*----------------------------------------------------------------------*
    |               start loop over integration points                     |
    *----------------------------------------------------------------------*/
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points is not activated here
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);
    // The integration factor is not multiplied with drs
    // since it is the same as the scaling factor for the unit normal
    // Therefore it cancels out!!
    const double fac = intpoints.IP().qwgt[gpid];

    // dxyzdrs vector -> normal which is not normalized
    LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(0.0);
    dxyzdrs.MultiplyNT(deriv_,xyze_);
    normal(0,0) = dxyzdrs(0,1) * dxyzdrs(1,2) - dxyzdrs(0,2) * dxyzdrs(1,1);
    normal(1,0) = dxyzdrs(0,2) * dxyzdrs(1,0) - dxyzdrs(0,0) * dxyzdrs(1,2);
    normal(2,0) = dxyzdrs(0,0) * dxyzdrs(1,1) - dxyzdrs(0,1) * dxyzdrs(1,0);

    //-------------------------------------------------------------------
    //  Q
    LINALG::Matrix<3,1> u(true);
    for (int dim=0;dim<3;++dim)
      for (int node=0;node<bdrynen_;++node)
        u(dim) += funct_(node) * evelnp(dim,node);

    for(int dim=0;dim<3;++dim)
      elevec3[0] += u(dim) * normal(dim,0) * fac;

    if (params.get<bool>("flowrateonly", false)==false)
    {
      //-------------------------------------------------------------------
      // dQ/du
      for (int node=0;node<bdrynen_;++node)
      {
        for (int dim=0;dim<3;++dim)
          elevec1[node*numdofpernode_+dim] += funct_(node) * normal(dim,0) * fac;
        elevec1[node*numdofpernode_+3] = 0.0;
      }

      //-------------------------------------------------------------------
      // dQ/dd

      // determine derivatives of surface normals wrt mesh displacements
      LINALG::Matrix<3,bdrynen_*3> normalderiv(true);

      for (int node=0;node<bdrynen_;++node)
      {
        normalderiv(0,3*node)   = 0.;
        normalderiv(0,3*node+1) = deriv_(0,node)*dxyzdrs(1,2)-deriv_(1,node)*dxyzdrs(0,2);
        normalderiv(0,3*node+2) = deriv_(1,node)*dxyzdrs(0,1)-deriv_(0,node)*dxyzdrs(1,1);

        normalderiv(1,3*node)   = deriv_(1,node)*dxyzdrs(0,2)-deriv_(0,node)*dxyzdrs(1,2);
        normalderiv(1,3*node+1) = 0.;
        normalderiv(1,3*node+2) = deriv_(0,node)*dxyzdrs(1,0)-deriv_(1,node)*dxyzdrs(0,0);

        normalderiv(2,3*node)   = deriv_(0,node)*dxyzdrs(1,1)-deriv_(1,node)*dxyzdrs(0,1);
        normalderiv(2,3*node+1) = deriv_(1,node)*dxyzdrs(0,0)-deriv_(0,node)*dxyzdrs(1,0);
        normalderiv(2,3*node+2) = 0.;
      }

      for (int node=0;node<bdrynen_;++node)
      {
        for (int dim=0;dim<3;++dim)
          for (int iterdim=0;iterdim<3;++iterdim)
            elevec2[node*numdofpernode_+dim] += u(iterdim) * normalderiv(iterdim,3*node+dim) * fac;
        elevec2[node*numdofpernode_+3] = 0.0;
      }

      // consideration of grid velocity
      if (isale)
      {
        // get time step size
        const double dt = params.get<double>("dt", -1.0);
        if (dt < 0.) dserror("invalid time step size");

        if (gridvel == INPAR::FLUID::BE)  // BE time discretization
        {
          for (int node=0;node<bdrynen_;++node)
          {
            for (int dim=0;dim<3;++dim)
              elevec2[node*numdofpernode_+dim] -= 1.0/dt * funct_(node) * normal(dim,0) * fac;
          }
        }
        else
          dserror("flowrate calculation: higher order of accuracy of grid velocity not implemented");
      }

      //-------------------------------------------------------------------
      // (d^2 Q)/(du dd)

      for (int unode=0;unode<bdrynen_;++unode)
      {
        for (int udim=0;udim<numdofpernode_;++udim)
        {
          for (int nnode=0;nnode<bdrynen_;++nnode)
          {
            for (int ndim=0;ndim<numdofpernode_;++ndim)
            {
              if (udim == 3 or ndim == 3)
                elemat1(unode*numdofpernode_+udim,nnode*numdofpernode_+ndim) = 0.0;
              else
                elemat1(unode*numdofpernode_+udim,nnode*numdofpernode_+ndim) = funct_(unode) * normalderiv(udim,3*nnode+ndim) * fac;
            }
          }
        }
      }

      //-------------------------------------------------------------------
      // (d^2 Q)/(dd)^2

      // determine second derivatives of surface normals wrt mesh displacements
      std::vector<LINALG::Matrix<bdrynen_*3,bdrynen_*3> > normalderiv2(3);

      for (int node1=0;node1<bdrynen_;++node1)
      {
        for (int node2=0;node2<bdrynen_;++node2)
        {
          double temp = deriv_(0,node1)*deriv_(1,node2)-deriv_(1,node1)*deriv_(0,node2);

          normalderiv2[0](node1*3+1,node2*3+2) = temp;
          normalderiv2[0](node1*3+2,node2*3+1) = - temp;

          normalderiv2[1](node1*3  ,node2*3+2) = - temp;
          normalderiv2[1](node1*3+2,node2*3  ) = temp;

          normalderiv2[2](node1*3  ,node2*3+1) = temp;
          normalderiv2[2](node1*3+1,node2*3  ) = - temp;
        }
      }

      for (int node1=0;node1<bdrynen_;++node1)
      {
        for (int dim1=0;dim1<numdofpernode_;++dim1)
        {
          for (int node2=0;node2<bdrynen_;++node2)
          {
            for (int dim2=0;dim2<numdofpernode_;++dim2)
            {
              if (dim1 == 3 or dim2 == 3)
                elemat2(node1*numdofpernode_+dim1,node2*numdofpernode_+dim2) = 0.0;
              else
              {
                for (int iterdim=0;iterdim<3;++iterdim)
                  elemat2(node1*numdofpernode_+dim1,node2*numdofpernode_+dim2) +=
                    u(iterdim) * normalderiv2[iterdim](node1*3+dim1,node2*3+dim2) * fac;
              }
            }
          }
        }
      }

      // consideration of grid velocity
      if (isale)
      {
        // get time step size
        const double dt = params.get<double>("dt", -1.0);
        if (dt < 0.) dserror("invalid time step size");

        if (gridvel == INPAR::FLUID::BE)
        {
          for (int node1=0;node1<bdrynen_;++node1)
          {
            for (int dim1=0;dim1<3;++dim1)
            {
              for (int node2=0;node2<bdrynen_;++node2)
              {
                for (int dim2=0;dim2<3;++dim2)
                {
                  elemat2(node1*numdofpernode_+dim1,node2*numdofpernode_+dim2) -= (1.0/dt * funct_(node1) * normalderiv(dim1, 3*node2+dim2)
                                                                                   + 1.0/dt * funct_(node2) * normalderiv(dim2, 3*node1+dim1))
                                                                                  * fac;
                }
              }
            }
          }
        }
        else
          dserror("flowrate calculation: higher order of accuracy of grid velocity not implemented");
      }

      //-------------------------------------------------------------------
    }
  }
}//DRT::ELEMENTS::FluidSurface::FlowRateDeriv


 /*----------------------------------------------------------------------*
  |  Impedance related parameters on boundary elements          AC 03/08  |
  *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ImpedanceIntegration(
                  DRT::ELEMENTS::FluidBoundary*    ele,
                  Teuchos::ParameterList&           params,
                  DRT::Discretization&              discretization,
                  std::vector<int>&                 lm,
                  Epetra_SerialDenseVector&         elevec1)
{
  //  const double thsl = params.get("thsl",0.0);
  const double thsl = fldpara_->TimeFacRhs();

  double pressure = params.get<double>("ConvolutedPressure");

  // get Gaussrule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

#ifdef D_ALE_BFLOW
  // Add the deformation of the ALE mesh to the nodes coordinates
  // displacements
  Teuchos::RCP<const Epetra_Vector>  dispnp;
  std::vector<double>                mydispnp;

  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"paranoid");
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }
#endif // D_ALE_BFLOW

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    const double fac_thsl_pres_inve = fac_ * thsl * pressure;

    for (int inode=0;inode<bdrynen_;++inode)
      for(int idim=0;idim<nsd_;++idim)
        // inward pointing normal of unit length
        elevec1[inode*numdofpernode_+idim] += funct_(inode) * fac_thsl_pres_inve * (-unitnormal_(idim));
  }
  //  cout<<"Pressure: "<<pressure<<endl;
  //  cout<<"thsl: "<<thsl<<endl;
  //  cout<<"density: "<<1.0/invdensity<<endl;
  //  exit(1);

  return;
} //DRT::ELEMENTS::FluidSurface::ImpedanceIntegration


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::EvalShapeFuncAtBouIntPoint(
    const DRT::UTILS::IntPointsAndWeights<bdrynsd_>&  intpoints,
    const int                                         gpid,
    const std::vector<Epetra_SerialDenseVector>*      myknots,
    const Epetra_SerialDenseVector*                   weights
)
{
  // local coordinates of the current integration point
  const double* gpcoord = (intpoints.IP().qxg)[gpid];
  for (int idim=0;idim<bdrynsd_;++idim)
  {
    xsi_(idim) = gpcoord[idim];
  }

  // get shape functions and derivatives in the plane of the element
  if(not IsNurbs<distype>::isnurbs)
  {
    // shape functions and their first derivatives of boundary element
    DRT::UTILS::shape_function<distype>(xsi_,funct_);
    DRT::UTILS::shape_function_deriv1<distype>(xsi_,deriv_);
  }
  // only for NURBS!!!
  else
  {
    if (bdrynsd_==2)
    {
      // this is just a temporary work-around
      Epetra_SerialDenseVector gp(2);
      gp(0)=xsi_(0);
      gp(1)=xsi_(1);

      DRT::NURBS::UTILS::nurbs_get_2D_funct_deriv
        (funct_  ,
         deriv_  ,
         gp     ,
         (*myknots),
         (*weights),
         distype);
    }
    else if(bdrynsd_==1)
    {
      //const double gp = xsi_(0);
      dserror("1d FluidBoundary nurbs elements not yet implemented");
      //DRT::NURBS::UTILS::nurbs_get_1D_funct_deriv(funct_,deriv_,gp,myknots,weights,distype);
    }
    else dserror("Discretisation type %s not yet implemented",DRT::DistypeToString(distype).c_str());
  }

  // compute measure tensor for surface element, infinitesimal area element drs
  // and (outward-pointing) unit normal vector
  LINALG::Matrix<bdrynsd_,bdrynsd_> metrictensor(true);
  DRT::UTILS::ComputeMetricTensorForBoundaryEle<distype>(xyze_,deriv_,metrictensor,drs_,&unitnormal_);

  // compute integration factor
  fac_ = intpoints.IP().qwgt[gpid]*drs_;

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
bool DRT::ELEMENTS::FluidBoundaryImpl<distype>::GetKnotVectorAndWeightsForNurbs(
    DRT::ELEMENTS::FluidBoundary*              ele,
    DRT::Discretization&                        discretization,
    std::vector<Epetra_SerialDenseVector>&      mypknots,
    std::vector<Epetra_SerialDenseVector>&      myknots,
    Epetra_SerialDenseVector&                   weights,
    double&                                     normalfac)
{
  // TODO: Check function 1D / 2D for Nurbs
  // ehrl
  if (bdrynsd_ == 1)
    dserror("1D line element -> It is not check if it is working.");

  // get pointer to parent element
  DRT::ELEMENTS::Fluid* parent_ele = ele->ParentElement();

  // local surface id
  const int surfaceid = ele->SurfaceNumber();

  // --------------------------------------------------
  // get knotvector

  DRT::NURBS::NurbsDiscretization* nurbsdis
    =
    dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(discretization));

  Teuchos::RCP<DRT::NURBS::Knotvector> knots=(*nurbsdis).GetKnotVector();

  bool zero_size = knots->GetBoundaryEleAndParentKnots(mypknots     ,
                                                     myknots      ,
                                                     normalfac    ,
                                                     parent_ele->Id(),
                                                     surfaceid    );

  // --------------------------------------------------
  // get node weights for nurbs elements
  for (int inode=0; inode<bdrynen_; ++inode)
  {
    DRT::NURBS::ControlPoint* cp
      =
      dynamic_cast<DRT::NURBS::ControlPoint* > (ele->Nodes()[inode]);

    weights(inode) = cp->W();
  }
  return zero_size;
}


/*----------------------------------------------------------------------*
 |  get density                                                vg 06/13 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::GetDensity(
  Teuchos::RCP<const MAT::Material>    material,
  const LINALG::Matrix<bdrynen_,1>&    escaaf,
  const double                         thermpressaf
)
{
// initially set density and density factor for Neumann boundary conditions to 1.0
// (the latter only changed for low-Mach-number flow/combustion problems)
densaf_  = 1.0;
densfac_ = 1.0;

if (material->MaterialType() == INPAR::MAT::m_fluid)
{
  const MAT::NewtonianFluid* actmat = static_cast<const MAT::NewtonianFluid*>(material.get());

  // varying density
  if (fldpara_->PhysicalType() == INPAR::FLUID::varying_density)
    densaf_ = funct_.Dot(escaaf);
  // Boussinesq approximation: Calculation of delta rho
  else if (fldpara_->PhysicalType() == INPAR::FLUID::boussinesq)
    dserror("Boussinesq approximation not yet supported for boundary terms!");
  else
    densaf_ = actmat->Density();
}
else if (material->MaterialType() == INPAR::MAT::m_carreauyasuda)
{
  const MAT::CarreauYasuda* actmat = static_cast<const MAT::CarreauYasuda*>(material.get());

  densaf_ = actmat->Density();
}
else if (material->MaterialType() == INPAR::MAT::m_modpowerlaw)
{
  const MAT::ModPowerLaw* actmat = static_cast<const MAT::ModPowerLaw*>(material.get());

  densaf_ = actmat->Density();
}
else if (material->MaterialType() == INPAR::MAT::m_herschelbulkley)
{
  const MAT::HerschelBulkley* actmat = static_cast<const MAT::HerschelBulkley*>(material.get());

  densaf_ = actmat->Density();
}
else if (material->MaterialType() == INPAR::MAT::m_yoghurt)
{
  const MAT::Yoghurt* actmat = static_cast<const MAT::Yoghurt*>(material.get());

  // get constant density
  densaf_ = actmat->Density();
}
else if (material->MaterialType() == INPAR::MAT::m_mixfrac)
{
  const MAT::MixFrac* actmat = static_cast<const MAT::MixFrac*>(material.get());

  // compute mixture fraction at n+alpha_F or n+1
  const double mixfracaf = funct_.Dot(escaaf);

  // compute density at n+alpha_F or n+1 based on mixture fraction
  densaf_ = actmat->ComputeDensity(mixfracaf);

  // set density factor for Neumann boundary conditions to density for present material
  densfac_ = densaf_;
}
else if (material->MaterialType() == INPAR::MAT::m_sutherland)
{
  const MAT::Sutherland* actmat = static_cast<const MAT::Sutherland*>(material.get());

  // compute temperature at n+alpha_F or n+1
  const double tempaf = funct_.Dot(escaaf);

  // compute density at n+alpha_F or n+1 based on temperature
  // and thermodynamic pressure
  densaf_ = actmat->ComputeDensity(tempaf,thermpressaf);

  // set density factor for Neumann boundary conditions to density for present material
  densfac_ = densaf_;
}
else if (material->MaterialType() == INPAR::MAT::m_arrhenius_pv)
{
  const MAT::ArrheniusPV* actmat = static_cast<const MAT::ArrheniusPV*>(material.get());

  // get progress variable at n+alpha_F or n+1
  const double provaraf = funct_.Dot(escaaf);

  // compute density at n+alpha_F or n+1 based on progress variable
  densaf_ = actmat->ComputeDensity(provaraf);

  // set density factor for Neumann boundary conditions to density for present material
  densfac_ = densaf_;
}
else if (material->MaterialType() == INPAR::MAT::m_ferech_pv)
{
  const MAT::FerEchPV* actmat = static_cast<const MAT::FerEchPV*>(material.get());

  // get progress variable at n+alpha_F or n+1
  const double provaraf = funct_.Dot(escaaf);

  // compute density at n+alpha_F or n+1 based on progress variable
  densaf_ = actmat->ComputeDensity(provaraf);

  // set density factor for Neumann boundary conditions to density for present material
  densfac_ = densaf_;
}
else if (material->MaterialType() == INPAR::MAT::m_permeable_fluid)
{
  const MAT::PermeableFluid* actmat = static_cast<const MAT::PermeableFluid*>(material.get());

  densaf_ = actmat->Density();
}
else if (material->MaterialType() == INPAR::MAT::m_fluidporo)
{
  const MAT::FluidPoro* actmat = static_cast<const MAT::FluidPoro*>(material.get());

  densaf_ = actmat->Density();
}
else dserror("Material type is not supported for density evaluation for boundary element!");

// check whether there is zero or negative density
if (densaf_ < EPS15) dserror("zero or negative density!");

return;
} // FluidBoundaryImpl::GetDensity

/*----------------------------------------------------------------------*
 |  Evaluating the velocity component of the traction      ismail 05/11 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::CalcTractionVelocityComponent(
  DRT::ELEMENTS::FluidBoundary*    ele,
  Teuchos::ParameterList&           params,
  DRT::Discretization&              discretization,
  std::vector<int>&                 lm,
  Epetra_SerialDenseVector&         elevec1)
{

  // extract local values from the global vectors
  Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");

  if (velnp==Teuchos::null)
    dserror("Cannot get state vector 'velnp'");

  std::vector<double> myvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);

  // allocate velocity vector
  LINALG::Matrix<nsd_,bdrynen_> evelnp(true);

  // split velocity and pressure, insert into element arrays
  for (int inode=0;inode<bdrynen_;inode++)
  {
    for (int idim=0; idim< nsd_; idim++)
    {
      evelnp(idim,inode) = myvelnp[idim+(inode*numdofpernode_)];
    }
  }


  Teuchos::RCP<Epetra_Vector> cond_velocities = params.get<RCP<Epetra_Vector> > ("condition velocities");
  Teuchos::RCP<Epetra_Map>    cond_dofrowmap  = params.get<RCP<Epetra_Map> > ("condition dofrowmap");

  double density=0.0; // inverse density of my parent element

  // get material of volume element this surface belongs to
  Teuchos::RCP<MAT::Material> mat = ele->ParentElement()->Material();

  if( mat->MaterialType() != INPAR::MAT::m_carreauyasuda
   && mat->MaterialType() != INPAR::MAT::m_modpowerlaw
   && mat->MaterialType() != INPAR::MAT::m_herschelbulkley
   && mat->MaterialType() != INPAR::MAT::m_fluid
   && mat->MaterialType() != INPAR::MAT::m_permeable_fluid)
          dserror("Material law is not a fluid");

  if(mat->MaterialType()== INPAR::MAT::m_fluid)
  {
    const MAT::NewtonianFluid* actmat = static_cast<const MAT::NewtonianFluid*>(mat.get());
    density = actmat->Density();
  }
  else if(mat->MaterialType()== INPAR::MAT::m_carreauyasuda)
  {
    const MAT::CarreauYasuda* actmat = static_cast<const MAT::CarreauYasuda*>(mat.get());
    density = actmat->Density();
  }
  else if(mat->MaterialType()== INPAR::MAT::m_modpowerlaw)
  {
    const MAT::ModPowerLaw* actmat = static_cast<const MAT::ModPowerLaw*>(mat.get());
    density = actmat->Density();
  }
  else if(mat->MaterialType()== INPAR::MAT::m_herschelbulkley)
  {
    const MAT::HerschelBulkley* actmat = static_cast<const MAT::HerschelBulkley*>(mat.get());
    density = actmat->Density();
  }
  else if(mat->MaterialType()== INPAR::MAT::m_permeable_fluid)
  {
    const MAT::PermeableFluid* actmat = static_cast<const MAT::PermeableFluid*>(mat.get());
    density = actmat->Density();
  }
  else
    dserror("Fluid material expected but got type %d", mat->MaterialType());

  //-------------------------------------------------------------------
  // get the tractions velocity component
  //-------------------------------------------------------------------

  // get Gaussrule
  //  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToGaussRuleForExactSol<distype>::rule);
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

#ifdef D_ALE_BFLOW
  // Add the deformation of the ALE mesh to the nodes coordinates
  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;

  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"paranoid");
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }
#endif // D_ALE_BFLOW
  const double timefac = fldpara_->TimeFacRhs();

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    // Get the velocity value at the corresponding Gauss point.
    std::vector<double> vel_gps(nsd_,0.0);
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for(int idim=0;idim<nsd_;++idim)
      {
        vel_gps[idim] += myvelnp[inode*numdofpernode_+idim]*funct_(inode);
      }
    }

    // Evaluate the normal velocity at the corresponding Gauss point
    double n_vel = 0.0;
    for(int idim = 0 ;idim<nsd_;++idim)
    {
      n_vel += vel_gps[idim]*(unitnormal_(idim));
    }
    // loop over all node and add the corresponding effect of the Neumann-Inflow condition
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for(int idim=0;idim<nsd_;++idim)
      {
        // evaluate the value of the Un.U at the corresponding Gauss point
        const double  uV = n_vel*vel_gps[idim] * density;
        const double fac_thsl_pres_inve = fac_ * timefac  * uV;

        // remove the Neumann-inflow contribution only if the normal velocity is an inflow velocity
        // i.e n_vel < 0
        if (n_vel<0.0)
        {
          elevec1[inode*numdofpernode_+idim] -= fac_thsl_pres_inve*funct_(inode);
        }
      }
      //      double radius = sqrt(pow(xyze_(0,inode),2.0)+pow(xyze_(1,inode),2.0));
      //      cout<<"n_vel("<<n_vel<<") vel: "<<n_vel<<" rad: "<<radius<<endl;
    }
  }
  return;
}

template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::ComputeNeumannUvIntegral(
  DRT::ELEMENTS::FluidBoundary*    ele,
  Teuchos::ParameterList&           params,
  DRT::Discretization&              discretization,
  std::vector<int>&                 lm,
  Epetra_SerialDenseVector&         elevec1)
{
}
//DRT::ELEMENTS::FluidSurface::ComputeNeumannUvIntegral

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::NoPenetration(
                                                 DRT::ELEMENTS::FluidBoundary*   ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                lm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseMatrix&        elemat2,
                                                 Epetra_SerialDenseVector&        elevec1,
                                                 Epetra_SerialDenseVector&        elevec2)
{
  // This function is only implemented for 3D
  if(bdrynsd_!=2 and bdrynsd_!=1)
    dserror("NoPenetration is only implemented for 3D and 2D!");

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;

  dispnp = discretization.GetState("dispnp");
  if (dispnp!=Teuchos::null)
  {
    mydispnp.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
  }
  dsassert(mydispnp.size()!=0,"no displacement values for boundary element");

  // Add the deformation of the ALE mesh to the nodes coordinates
  for (int inode=0;inode<bdrynen_;++inode)
    for (int idim=0; idim<nsd_; ++idim)
      xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];

  Teuchos::RCP<const Epetra_Vector>      condVector;
  std::vector<double>                mycondVector;

  condVector = discretization.GetState("condVector");
  if(condVector==Teuchos::null)
    dserror("could not get state 'condVector'");
  else
  {
    mycondVector.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*condVector,mycondVector,lm);
  }
  dsassert(mycondVector.size()!=0,"no condition IDs values for boundary element");

  //calculate normal
  Epetra_SerialDenseVector        normal;
  normal.Size(lm.size());

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    for (int inode=0; inode<bdrynen_; ++inode)
    {
      for(int idim=0; idim<nsd_; ++idim)
        normal(inode*numdofpernode_+idim) += unitnormal_(idim) * funct_(inode) * fac_;
      // pressure dof is set to zero
      normal(inode*numdofpernode_+(nsd_)) = 0.0;
    }
  } /* end of loop over integration points gpid */

  LINALG::Matrix<numdofpernode_,1> nodenormal(true);

  //check which matrix is to be filled
  POROELAST::coupltype coupling = params.get<POROELAST::coupltype>("coupling",POROELAST::undefined);

  if (coupling == POROELAST::fluidfluid)
  {
    //fill element matrix
    for (int inode=0;inode<bdrynen_;inode++)
    {
      for(int i=0;i<numdofpernode_;i++)
        nodenormal(i)=normal(inode*numdofpernode_+i);
      double norm = nodenormal.Norm2();
      nodenormal.Scale(1/norm);

      for (int idof=0;idof<numdofpernode_;idof++)
      {
        if(mycondVector[inode*numdofpernode_+idof]!=0.0)
        {
          for (int idof2=0;idof2<numdofpernode_;idof2++)
              elemat1(inode*numdofpernode_+idof,inode*numdofpernode_+idof2) += nodenormal(idof2);
        }
      }
    }
  }
  else if (coupling == POROELAST::fluidstructure)
  {
    // extract local values from the global vectors
    Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");
    Teuchos::RCP<const Epetra_Vector> gridvel = discretization.GetState("gridv");

    if (velnp==Teuchos::null)
      dserror("Cannot get state vector 'velnp'");
    if (gridvel==Teuchos::null)
      dserror("Cannot get state vector 'gridv'");

    std::vector<double> myvelnp(lm.size());
    DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);
    std::vector<double> mygridvel(lm.size());
    DRT::UTILS::ExtractMyValues(*gridvel,mygridvel,lm);

    // allocate velocity vectors
    LINALG::Matrix<nsd_,bdrynen_> evelnp(true);
    LINALG::Matrix<nsd_,bdrynen_> egridvel(true);

    // split velocity and pressure, insert into element arrays
    for (int inode=0;inode<bdrynen_;inode++)
      for (int idim=0; idim< nsd_; idim++)
      {
        evelnp(idim,inode) = myvelnp[idim+(inode*numdofpernode_)];
        egridvel(idim,inode) = mygridvel[idim+(inode*numdofpernode_)];
      }

    //  derivatives of surface normals wrt mesh displacements
    LINALG::Matrix<nsd_,bdrynen_*nsd_> normalderiv(true);

    for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
    {
      // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
      // Computation of the unit normal vector at the Gauss points is not activated here
      // Computation of nurb specific stuff is not activated here
      EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

      // dxyzdrs vector -> normal which is not normalized
      LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(0.0);
      dxyzdrs.MultiplyNT(deriv_,xyze_);

      // The integration factor is not multiplied with drs
      // since it is the same as the scaling factor for the unit normal derivatives
      // Therefore it cancels out!!
      const double fac = intpoints.IP().qwgt[gpid];

      if(nsd_==3)
        for (int node=0;node<bdrynen_;++node)
        {
          normalderiv(0,nsd_*node)   += 0.;
          normalderiv(0,nsd_*node+1) += (deriv_(0,node)*dxyzdrs(1,2)-deriv_(1,node)*dxyzdrs(0,2)) * funct_(node) * fac;
          normalderiv(0,nsd_*node+2) += (deriv_(1,node)*dxyzdrs(0,1)-deriv_(0,node)*dxyzdrs(1,1)) * funct_(node) * fac;

          normalderiv(1,nsd_*node)   += (deriv_(1,node)*dxyzdrs(0,2)-deriv_(0,node)*dxyzdrs(1,2)) * funct_(node) * fac;
          normalderiv(1,nsd_*node+1) += 0.;
          normalderiv(1,nsd_*node+2) += (deriv_(0,node)*dxyzdrs(1,0)-deriv_(1,node)*dxyzdrs(0,0)) * funct_(node) * fac;

          normalderiv(2,nsd_*node)   += (deriv_(0,node)*dxyzdrs(1,1)-deriv_(1,node)*dxyzdrs(0,1)) * funct_(node) * fac;
          normalderiv(2,nsd_*node+1) += (deriv_(1,node)*dxyzdrs(0,0)-deriv_(0,node)*dxyzdrs(1,0)) * funct_(node) * fac;
          normalderiv(2,nsd_*node+2) += 0.;
        }
      else if(nsd_==2)
        for (int node=0;node<bdrynen_;++node)
        {
          normalderiv(0,nsd_*node)   += 0.;
          normalderiv(0,nsd_*node+1) += deriv_(0,node) * funct_(node) * fac;

          normalderiv(1,nsd_*node)   += -deriv_(0,node) * funct_(node) * fac;
          normalderiv(1,nsd_*node+1) += 0.;
        }
    }//loop over gp

    //allocate auxiliary variable (= normalderiv^T * velocity)
    LINALG::Matrix<1,nsd_*bdrynen_> temp(true);
    //allocate convective velocity at node
    LINALG::Matrix<1,nsd_> convvel(true);

    //elemat1.Shape(bdrynen_*numdofpernode_,bdrynen_*nsd_);
    //fill element matrix
    for (int inode=0;inode<bdrynen_;inode++)
    {
      for(int i=0;i<numdofpernode_;i++)
        nodenormal(i)=normal(inode*numdofpernode_+i);

      double norm = nodenormal.Norm2();
      nodenormal.Scale(1/norm);

      for (int idof=0;idof<nsd_;idof++)
        convvel(idof)=evelnp(idof,inode) - egridvel(idof,inode);
      temp.Multiply(convvel,normalderiv);
      for (int idof=0;idof<numdofpernode_;idof++)
      {
        //if(abs(nodenormal(idof)) > 0.5)
        if(mycondVector[inode*numdofpernode_+idof]!=0.0)
        {
          for (int idof2=0;idof2<nsd_;idof2++)
          {
            elemat1(inode*numdofpernode_+idof,inode*nsd_+idof2) += temp(0,inode*nsd_+idof2);
            elemat2(inode*numdofpernode_+idof,inode*nsd_+idof2) += - nodenormal(idof2);
          }
          double normalconvvel = 0.0;
          for(int dim=0;dim<nsd_;dim++)
            normalconvvel += convvel(dim)*nodenormal(dim);
          elevec1(inode*numdofpernode_+idof) += -normalconvvel;
          break;
        }
      }
    }
  }//coupling == "fluid structure"
  else
    dserror("unknown coupling type for no penetration boundary condition");

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::NoPenetrationIDs(
                                                 DRT::ELEMENTS::FluidBoundary*   ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 Epetra_SerialDenseVector&        elevec1,
                                                 std::vector<int>&                lm)
{
  // This function is only implemented for 3D
  if(bdrynsd_!=2 and bdrynsd_!=1)
    dserror("NoPenetration is only implemented for 3D and 2D!");

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  //GEO::fillInitialPositionArray<distype,nsd_,Epetra_SerialDenseMatrix>(ele,xyze_);
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;

  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp!=Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"no displacement values for boundary element");

    // Add the deformation of the ALE mesh to the nodes coordinates
    for (int inode=0;inode<bdrynen_;++inode)
      for (int idim=0; idim<nsd_; ++idim)
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
  }
  else
    dserror("fluid poro element not an ALE element!");

  //calculate normal
  Epetra_SerialDenseVector        normal;
  normal.Size(lm.size());

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    for (int inode=0; inode<bdrynen_; ++inode)
    {
      for(int idim=0; idim<nsd_; ++idim)
        normal(inode*numdofpernode_+idim) += unitnormal_(idim) * funct_(inode) * fac_;
      // pressure dof is set to zero
      normal(inode*numdofpernode_+(nsd_)) = 0.0;
    }
  } /* end of loop over integration points gpid */

  LINALG::Matrix<numdofpernode_,1> nodenormal(true);

  //fill element matrix
  for (int inode=0;inode<bdrynen_;inode++)
  {
    for(int i=0;i<numdofpernode_;i++)
      nodenormal(i)=normal(inode*numdofpernode_+i);
    double norm = nodenormal.Norm2();
    nodenormal.Scale(1/norm);

    bool isset=false;
    for (int idof=0;idof<numdofpernode_;idof++)
    {
      if(isset==false and abs(nodenormal(idof)) > 0.5)
      {
        elevec1(inode*numdofpernode_+idof) = 1.0;
        isset=true;
      }
      else //no condition set on dof
        elevec1(inode*numdofpernode_+idof) = 0.0;
    }
  }
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::PoroBoundary(
                                                 DRT::ELEMENTS::FluidBoundary*    ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                plm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseVector&        elevec1)
{
  switch (distype)
  {
  // 2D:
  case DRT::Element::line2:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::quad4)
    {
      PoroBoundary<DRT::Element::quad4>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror("expected combination line2/quad4 for line/parent pair");
    }
    break;
  }
  case DRT::Element::line3:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::quad9)
    {
      PoroBoundary<DRT::Element::quad9>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror("expected combination line3/quad9 for line/parent pair");
    }
    break;
  }
  // 3D:
  case DRT::Element::quad4:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::hex8)
    {
      PoroBoundary<DRT::Element::hex8>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror("expected combination quad4/hex8 for surface/parent pair");
    }
    break;
  }
  case DRT::Element::tri3:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::tet4)
    {
      PoroBoundary<DRT::Element::tet4>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror("expected combination tri3/tet4 for surface/parent pair");
    }
    break;
  }
  case DRT::Element::tri6:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::tet10)
    {
      PoroBoundary<DRT::Element::tet10>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror("expected combination tri6/tet10 for surface/parent pair");
    }
    break;
  }
  case DRT::Element::quad9:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::hex27)
    {
      PoroBoundary<DRT::Element::hex27>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror("expected combination hex27/hex27 for surface/parent pair");
    }
    break;
  }
  default:
  {
    dserror("surface/parent element pair not yet implemented. Just do it.\n");
    break;
  }

  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
template <DRT::Element::DiscretizationType pdistype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::PoroBoundary(
                                                 DRT::ELEMENTS::FluidBoundary*   ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                plm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseVector&        elevec1)
{
  // This function is only implemented for 3D and 2D
  if(bdrynsd_!=2 and bdrynsd_!=1)
    dserror("PoroBoundary is only implemented for 3D and 2D!");

  POROELAST::coupltype coupling = params.get<POROELAST::coupltype>("coupling",POROELAST::undefined);
  if(coupling == POROELAST::undefined) dserror("no coupling defined for poro-boundary condition");
  const bool offdiag( coupling == POROELAST::fluidstructure);

  // get timescale parameter from parameter list (depends on time integration scheme)
  double timescale = params.get<double>("timescale",-1.0);
  if(timescale == -1.0 and offdiag)
    dserror("no timescale parameter in parameter list");

  //reset timescale in stationary case
  if(fldpara_->IsStationary())
    timescale=0.0;

  // get element location vector and ownerships
  std::vector<int> lm;
  std::vector<int> lmowner;
  std::vector<int> lmstride;
  ele->DRT::Element::LocationVector(discretization,lm,lmowner,lmstride);

  /// number of parentnodes
  static const int nenparent    = DRT::UTILS::DisTypeToNumNodePerEle<pdistype>::numNodePerElement;

  // get the parent element
  DRT::ELEMENTS::Fluid* pele = ele->ParentElement();

  const int peleid = pele->Id();
  //access structure discretization
  Teuchos::RCP<DRT::Discretization> structdis = Teuchos::null;
  structdis = DRT::Problem::Instance()->GetDis("structure");
  //get corresponding structure element (it has the same global ID as the scatra element)
  DRT::Element* structele = structdis->gElement(peleid);
  if (structele == NULL)
    dserror("Structure element %i not on local processor", peleid);

  DRT::ELEMENTS::So_Poro_Interface* so_interface = dynamic_cast<DRT::ELEMENTS::So_Poro_Interface*>(structele);
  if(so_interface == NULL)
    dserror("cast to so_interface failed!");

  //ask if the structure element has a porosity dof
  const bool porositydof = so_interface->HasExtraDof();

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;
  std::vector<double>                parentdispnp;

  dispnp = discretization.GetState("dispnp");
  if (dispnp!=Teuchos::null)
  {
    mydispnp.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    DRT::UTILS::ExtractMyValues(*dispnp,parentdispnp,plm);
  }
  dsassert(mydispnp.size()!=0,"no displacement values for boundary element");
  dsassert(parentdispnp.size()!=0,"no displacement values for parent element");

  // Add the deformation of the ALE mesh to the nodes coordinates
  for (int inode=0;inode<bdrynen_;++inode)
    for (int idim=0; idim<nsd_; ++idim)
      xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];

  // update element geometry of parent element
  LINALG::Matrix<nsd_,nenparent>  xrefe; // material coord. of parent element
  LINALG::Matrix<nsd_,nenparent> xcurr; // current  coord. of parent element
  {
    DRT::Node** nodes = pele->Nodes();
    for (int i=0; i<nenparent; ++i)
    {
      for (int j=0; j<nsd_; ++j)
      {
        const double* x = nodes[i]->X();
        xrefe(j,i) = x[j];
        xcurr(j,i) = xrefe(j,i) + parentdispnp[i*numdofpernode_+j];
      }
    }
  }

  // extract local values from the global vectors
  Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");
  Teuchos::RCP<const Epetra_Vector> gridvel = discretization.GetState("gridv");
  Teuchos::RCP<const Epetra_Vector> scaaf = discretization.GetState("scaaf");

  if (velnp==Teuchos::null)
    dserror("Cannot get state vector 'velnp'");
  if (gridvel==Teuchos::null)
    dserror("Cannot get state vector 'gridv'");

  std::vector<double> myvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);
  std::vector<double> mygridvel(lm.size());
  DRT::UTILS::ExtractMyValues(*gridvel,mygridvel,lm);
  std::vector<double> myscaaf(lm.size());
  DRT::UTILS::ExtractMyValues(*scaaf,myscaaf,lm);

  // allocate velocity vectors
  LINALG::Matrix<nsd_,bdrynen_> evelnp(true);
  LINALG::Matrix<bdrynen_,1> epressnp(true);
  LINALG::Matrix<nsd_,bdrynen_> edispnp(true);
  LINALG::Matrix<nsd_,bdrynen_> egridvel(true);
  LINALG::Matrix<bdrynen_,1> escaaf(true);
  LINALG::Matrix<bdrynen_,1> eporosity(true);

  // split velocity and pressure, insert into element arrays
  for (int inode=0;inode<bdrynen_;inode++)
  {
    for (int idim=0; idim< nsd_; idim++)
    {
      evelnp(idim,inode)   = myvelnp[idim+(inode*numdofpernode_)];
      edispnp(idim,inode)  = mydispnp[idim+(inode*numdofpernode_)];
      egridvel(idim,inode) = mygridvel[idim+(inode*numdofpernode_)];
    }
    epressnp(inode) = myvelnp[nsd_+(inode*numdofpernode_)];
    escaaf(inode) = myscaaf[nsd_+(inode*numdofpernode_)];
  }

  if(porositydof)
  {
    for (int inode=0;inode<bdrynen_;inode++)
      eporosity(inode) = mydispnp[nsd_+(inode*numdofpernode_)];
  }

  // get coordinates of gauss points w.r.t. local parent coordinate system
  Epetra_SerialDenseMatrix pqxg(intpoints.IP().nquad,nsd_);
  LINALG::Matrix<nsd_,nsd_>  derivtrafo(true);

  DRT::UTILS::BoundaryGPToParentGP<nsd_>( pqxg     ,
                                          derivtrafo,
                                          intpoints,
                                          pdistype ,
                                          distype  ,
                                          ele->SurfaceNumber());


  //structure velocity at gausspoint
  LINALG::Matrix<nsd_,1> gridvelint;

  //coordinates of gauss points of parent element
  LINALG::Matrix<nsd_ , 1>    pxsi(true);

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // get shape functions and derivatives in the plane of the element
    LINALG::Matrix<nenparent,1> pfunct(true);
    LINALG::Matrix<nsd_,nenparent> pderiv;
    LINALG::Matrix<nsd_,nenparent> pderiv_loc;

    // coordinates of the current integration point
    for (int idim=0;idim<nsd_ ;idim++)
      pxsi(idim) = pqxg(gpid,idim);

    DRT::UTILS::shape_function       <pdistype>(pxsi,pfunct);
    DRT::UTILS::shape_function_deriv1<pdistype>(pxsi,pderiv_loc);

    pderiv.Multiply(derivtrafo,pderiv_loc);

    // get Jacobian matrix and determinant w.r.t. spatial configuration
    // transposed jacobian "dx/ds"
    LINALG::Matrix<nsd_,nsd_>  xjm;
    LINALG::Matrix<nsd_,nsd_> Jmat;
    xjm.MultiplyNT(pderiv_loc,xcurr);
    Jmat.MultiplyNT(pderiv_loc,xrefe);
    // jacobian determinant "det(dx/ds)"
    const double det = xjm.Determinant();
    // jacobian determinant "det(dX/ds)"
    const double detJ = Jmat.Determinant();
    // jacobian determinant "det(dx/dX) = det(dx/ds)/det(dX/ds)"
    const double J = det/detJ;

    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    const double timefacpre = fldpara_->TimeFacPre() ;
    const double timefacfacpre = fldpara_->TimeFacPre() * fac_;
    const double rhsfac        = fldpara_->TimeFacRhs() * fac_;

    velint_.Multiply(evelnp,funct_);
    gridvelint.Multiply(egridvel,funct_);
    double press = epressnp.Dot(funct_);

    double scalar = escaaf.Dot(funct_);

    double dphi_dp=0.0;
    double dphi_dJ=0.0;
    double porosity_gp=0.0;

    params.set<double>("scalar",scalar);

    if(porositydof)
    {
      porosity_gp = eporosity.Dot(funct_);
    }
    else
    {
      so_interface->ComputeSurfPorosity(params,
                                     press,
                                     J,
                                     ele->SurfaceNumber(),
                                     gpid,
                                     porosity_gp,
                                     &dphi_dp,
                                     &dphi_dJ,
                                     NULL,                  //dphi_dJdp not needed
                                     NULL,                  //dphi_dJJ not needed
                                     NULL,                   //dphi_dpp not needed
                                     true
                                     );
    }

    // The integration factor is not multiplied with drs
    // since it is the same as the scaling factor for the unit normal derivatives
    // Therefore it cancels out!!
    const double fac = intpoints.IP().qwgt[gpid];

    //  derivatives of surface normals wrt mesh displacements
    LINALG::Matrix<nsd_,nenparent*nsd_> normalderiv(true);

    // dxyzdrs vector -> normal which is not normalized
    LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(0.0);
    dxyzdrs.MultiplyNT(deriv_,xyze_);

    if(nsd_==3)
      for (int node=0;node<nenparent;++node)
      {
        normalderiv(0,nsd_*node)   += 0.;
        normalderiv(0,nsd_*node+1) += (pderiv(0,node)*dxyzdrs(1,2)-pderiv(1,node)*dxyzdrs(0,2)) ;
        normalderiv(0,nsd_*node+2) += (pderiv(1,node)*dxyzdrs(0,1)-pderiv(0,node)*dxyzdrs(1,1)) ;

        normalderiv(1,nsd_*node)   += (pderiv(1,node)*dxyzdrs(0,2)-pderiv(0,node)*dxyzdrs(1,2)) ;
        normalderiv(1,nsd_*node+1) += 0.;
        normalderiv(1,nsd_*node+2) += (pderiv(0,node)*dxyzdrs(1,0)-pderiv(1,node)*dxyzdrs(0,0)) ;

        normalderiv(2,nsd_*node)   += (pderiv(0,node)*dxyzdrs(1,1)-pderiv(1,node)*dxyzdrs(0,1)) ;
        normalderiv(2,nsd_*node+1) += (pderiv(1,node)*dxyzdrs(0,0)-pderiv(0,node)*dxyzdrs(1,0)) ;
        normalderiv(2,nsd_*node+2) += 0.;
      }
    else //if(nsd_==2)
      for (int node=0;node<nenparent;++node)
      {
        normalderiv(0,nsd_*node)   += 0.;
        normalderiv(0,nsd_*node+1) += pderiv(0,node) ;

        normalderiv(1,nsd_*node)   += -pderiv(0,node) ;
        normalderiv(1,nsd_*node+1) += 0.;
      }

    //------------------------------------------------dJ/dus = dJ/dF : dF/dus = J * F^-T . N_X = J * N_x
    LINALG::Matrix<1,nsd_*nenparent> dJ_dus;
    // global derivatives of shape functions w.r.t x,y,z
    LINALG::Matrix<nsd_,nenparent> derxy;
    // inverse of transposed jacobian "ds/dx"
    LINALG::Matrix<nsd_,nsd_> xji;

    xji.Invert(xjm);
    derxy.Multiply(xji,pderiv_loc);

    for (int i=0; i<nenparent; i++)
      for (int j=0; j<nsd_; j++)
        dJ_dus(j+i*nsd_)=J*derxy(j,i);

    double normal_convel = 0.0;
    LINALG::Matrix<1,nsd_> convel;

    for (int idof=0;idof<nsd_;idof++)
    {
      normal_convel += unitnormal_(idof) *velint_(idof)  ;
      convel(idof)   = velint_(idof) ;
    }

    if(not fldpara_->IsStationary())
      for (int idof=0;idof<nsd_;idof++)
      {
        normal_convel += unitnormal_(idof) *( - gridvelint(idof) ) ;
        convel(idof)  -= gridvelint(idof);
      }

    LINALG::Matrix<1,nenparent*nsd_> tmp;
    tmp.Multiply(convel,normalderiv);

    //fill element matrix
    {
      if(not offdiag)
      {
        for (int inode=0;inode<nenparent;inode++)
          elevec1(inode*numdofpernode_+nsd_) -=  rhsfac * pfunct(inode) * porosity_gp * normal_convel;

        for (int inode=0;inode<nenparent;inode++)
          for (int nnod=0;nnod<nenparent;nnod++)
          {
            for (int idof2=0;idof2<nsd_;idof2++)
                elemat1(inode*numdofpernode_+nsd_,nnod*numdofpernode_+idof2) +=
                    timefacfacpre * pfunct(inode) * porosity_gp * unitnormal_(idof2) * pfunct(nnod)
                  ;
            elemat1(inode*numdofpernode_+nsd_,nnod*numdofpernode_+nsd_) +=
                + timefacfacpre * pfunct(inode) * dphi_dp* normal_convel * pfunct(nnod);
          }
      }

      else if(not porositydof)
      {
        for (int inode=0;inode<nenparent;inode++)
          for (int nnod=0;nnod<nenparent;nnod++)
            for (int idof2=0;idof2<nsd_;idof2++)
              elemat1(inode*numdofpernode_+nsd_,nnod*nsd_+idof2) +=
                      + tmp(0,nnod*nsd_+idof2) * porosity_gp * pfunct(inode) * timefacpre * fac
                      - pfunct(inode) * porosity_gp * unitnormal_(idof2) * timescale * pfunct(nnod) * timefacfacpre
                      + pfunct(inode) * dphi_dJ * dJ_dus(nnod*nsd_+idof2) * normal_convel * timefacfacpre
                      ;
      }

      else // offdiagonal and porositydof
        for (int inode=0;inode<nenparent;inode++)
          for (int nnod=0;nnod<nenparent;nnod++)
          {
            for (int idof2=0;idof2<nsd_;idof2++)
              elemat1(inode*numdofpernode_+nsd_,nnod*(nsd_+1)+idof2) +=
                      + tmp(0,nnod*nsd_+idof2) * porosity_gp* pfunct(inode) * timefacpre * fac
                      - pfunct(inode) * porosity_gp * unitnormal_(idof2) * timescale * pfunct(nnod) * timefacfacpre
                      + pfunct(inode) * dphi_dJ * dJ_dus(nnod*nsd_+idof2) * normal_convel * timefacfacpre
                      ;
            elemat1(inode*numdofpernode_+nsd_,nnod*(nsd_+1)+nsd_) +=
                      pfunct(inode) * pfunct(nnod) * normal_convel * timefacfacpre;
          }
    }
  } /* end of loop over integration points gpid */
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::PressureCoupling(
                                                 DRT::ELEMENTS::FluidBoundary*    ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                lm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseVector&        elevec1)
{
  // This function is only implemented for 3D
  if(bdrynsd_!=2 and bdrynsd_!=1)
    dserror("PressureCoupling is only implemented for 3D!");

  POROELAST::coupltype coupling = params.get<POROELAST::coupltype>("coupling",POROELAST::undefined);
  if(coupling == POROELAST::undefined) dserror("no coupling defined for poro-boundary condition");
  const bool offdiag( coupling == POROELAST::fluidstructure);

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;

  if (ele->ParentElement()->IsAle())
  {
    dispnp = discretization.GetState("dispnp");
    if (dispnp != Teuchos::null)
    {
      mydispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    }
    dsassert(mydispnp.size()!=0,"no displacement values for boundary element");

    // Add the deformation of the ALE mesh to the nodes coordinates
    for (int inode=0;inode<bdrynen_;++inode)
    {
      for (int idim=0; idim<nsd_; ++idim)
      {
        xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];
      }
    }
  }

  // extract local values from the global vectors
  Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");

  if (velnp == Teuchos::null)
    dserror("Cannot get state vector 'velnp'");

  std::vector<double> myvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);

  // allocate velocity vectors
  LINALG::Matrix<bdrynen_,1> epressnp(true);

  // split velocity and pressure, insert into element arrays
  for (int inode=0;inode<bdrynen_;inode++)
  {
     epressnp(inode)   = myvelnp[nsd_+(inode*numdofpernode_)];
  }

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    const double timefac       = fldpara_->TimeFac() ;
    const double timefacfac    = fldpara_->TimeFac() * fac_;
    const double rhsfac        = fldpara_->TimeFacRhs() * fac_;

    // get pressure at integration point
    double press = funct_.Dot(epressnp);

    // dxyzdrs vector -> normal which is not normalized
    LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(0.0);
    dxyzdrs.MultiplyNT(deriv_,xyze_);

    //  derivatives of surface normals wrt mesh displacements
    LINALG::Matrix<3,bdrynen_*3> normalderiv(true);

    // The integration factor is not multiplied with drs
    // since it is the same as the scaling factor for the unit normal derivatives
    // Therefore it cancels out!!
    const double fac = intpoints.IP().qwgt[gpid];

    if(nsd_==3)
      for (int node=0;node<bdrynen_;++node)
      {
        normalderiv(0,3*node)   += 0.;
        normalderiv(0,3*node+1) += (deriv_(0,node)*dxyzdrs(1,2)-deriv_(1,node)*dxyzdrs(0,2));
        normalderiv(0,3*node+2) += (deriv_(1,node)*dxyzdrs(0,1)-deriv_(0,node)*dxyzdrs(1,1));

        normalderiv(1,3*node)   += (deriv_(1,node)*dxyzdrs(0,2)-deriv_(0,node)*dxyzdrs(1,2));
        normalderiv(1,3*node+1) += 0.;
        normalderiv(1,3*node+2) += (deriv_(0,node)*dxyzdrs(1,0)-deriv_(1,node)*dxyzdrs(0,0));

        normalderiv(2,3*node)   += (deriv_(0,node)*dxyzdrs(1,1)-deriv_(1,node)*dxyzdrs(0,1));
        normalderiv(2,3*node+1) += (deriv_(1,node)*dxyzdrs(0,0)-deriv_(0,node)*dxyzdrs(1,0));
        normalderiv(2,3*node+2) += 0.;
      }
    else if(nsd_==2)
      for (int node=0;node<bdrynen_;++node)
      {
        normalderiv(0,nsd_*node)   += 0.;
        normalderiv(0,nsd_*node+1) += deriv_(0,node) * funct_(node) ;

        normalderiv(1,nsd_*node)   += -deriv_(0,node) * funct_(node) ;
        normalderiv(1,nsd_*node+1) += 0.;
      }

    //fill element matrix
    for (int inode=0;inode<bdrynen_;inode++)
    {
      for (int idof=0;idof<nsd_;idof++)
      {
        if(not offdiag)
          elevec1(inode*numdofpernode_+idof) -=  funct_(inode) * unitnormal_(idof) * press * rhsfac;
        for (int nnod=0;nnod<bdrynen_;nnod++)
        {
          if(not offdiag)
            elemat1(inode*numdofpernode_+idof,nnod*numdofpernode_+nsd_) +=
                 funct_(inode) * unitnormal_(idof) * funct_(nnod) * timefacfac
            ;
          else
            for (int idof2=0;idof2<nsd_;idof2++)
            {
              elemat1(inode*numdofpernode_+idof,nnod*nsd_+idof2) +=
                   normalderiv(idof,nnod*nsd_+idof2) * press * funct_(inode) * timefac * fac;
            }
        }
      }
    }
  } /* end of loop over integration points gpid */

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::PoroFlowRate(
                                                 DRT::ELEMENTS::FluidBoundary*    ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                plm,
                                                 Epetra_SerialDenseVector&        elevec1)
{
  switch (distype)
  {
  // 2D:
  case DRT::Element::line2:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::quad4)
    {
      PoroFlowRate<DRT::Element::quad4>(
          ele,
          params,
          discretization,
          plm,
          elevec1);
    }
    else
    {
      dserror("expected combination line2/quad4 for line/parent pair");
    }
    break;
  }
  case DRT::Element::line3:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::quad9)
    {
      PoroFlowRate<DRT::Element::quad9>(
          ele,
          params,
          discretization,
          plm,
          elevec1);
    }
    else
    {
      dserror("expected combination line3/quad9 for line/parent pair");
    }
    break;
  }
  // 3D:
  case DRT::Element::quad4:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::hex8)
    {
      PoroFlowRate<DRT::Element::hex8>(
          ele,
          params,
          discretization,
          plm,
          elevec1);
    }
    else
    {
      dserror("expected combination quad4/hex8 for surface/parent pair");
    }
    break;
  }
  case DRT::Element::tri3:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::tet4)
    {
      PoroFlowRate<DRT::Element::tet4>(
          ele,
          params,
          discretization,
          plm,
          elevec1);
    }
    else
    {
      dserror("expected combination tri3/tet4 for surface/parent pair");
    }
    break;
  }
  case DRT::Element::tri6:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::tet10)
    {
      PoroFlowRate<DRT::Element::tet10>(
          ele,
          params,
          discretization,
          plm,
          elevec1);
    }
    else
    {
      dserror("expected combination tri6/tet10 for surface/parent pair");
    }
    break;
  }
  case DRT::Element::quad9:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::hex27)
    {
      PoroFlowRate<DRT::Element::hex27>(
          ele,
          params,
          discretization,
          plm,
          elevec1);
    }
    else
    {
      dserror("expected combination hex27/hex27 for surface/parent pair");
    }
    break;
  }
  default:
  {
    dserror("surface/parent element pair not yet implemented. Just do it.\n");
    break;
  }

  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
template <DRT::Element::DiscretizationType pdistype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::PoroFlowRate(
                                                 DRT::ELEMENTS::FluidBoundary*   ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                plm,
                                                 Epetra_SerialDenseVector&        elevec1)
{
  // This function is only implemented for 3D and 2D
  if(bdrynsd_!=2 and bdrynsd_!=1)
    dserror("PoroBoundary is only implemented for 3D and 2D!");

  // get element location vector and ownerships
  std::vector<int> lm;
  std::vector<int> lmowner;
  std::vector<int> lmstride;
  ele->DRT::Element::LocationVector(discretization,lm,lmowner,lmstride);

  /// number of parentnodes
  static const int nenparent    = DRT::UTILS::DisTypeToNumNodePerEle<pdistype>::numNodePerElement;

  // get the parent element
  DRT::ELEMENTS::Fluid* pele = ele->ParentElement();

  const int peleid = pele->Id();
  //access structure discretization
  Teuchos::RCP<DRT::Discretization> structdis = Teuchos::null;
  structdis = DRT::Problem::Instance()->GetDis("structure");
  //get corresponding structure element (it has the same global ID as the scatra element)
  DRT::Element* structele = structdis->gElement(peleid);
  if (structele == NULL)
    dserror("Structure element %i not on local processor", peleid);

  DRT::ELEMENTS::So_Poro_Interface* so_interface = dynamic_cast<DRT::ELEMENTS::So_Poro_Interface*>(structele);
  if(so_interface == NULL)
    dserror("cast to so_interface failed!");

  //ask if the structure element has a porosity dof
  const bool porositydof = so_interface->HasExtraDof();

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);

  // displacements
  Teuchos::RCP<const Epetra_Vector>      dispnp;
  std::vector<double>                mydispnp;
  std::vector<double>                parentdispnp;

  dispnp = discretization.GetState("dispnp");
  if (dispnp!=Teuchos::null)
  {
    mydispnp.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*dispnp,mydispnp,lm);
    DRT::UTILS::ExtractMyValues(*dispnp,parentdispnp,plm);
  }
  dsassert(mydispnp.size()!=0,"no displacement values for boundary element");
  dsassert(parentdispnp.size()!=0,"no displacement values for parent element");

  // Add the deformation of the ALE mesh to the nodes coordinates
  for (int inode=0;inode<bdrynen_;++inode)
    for (int idim=0; idim<nsd_; ++idim)
      xyze_(idim,inode)+=mydispnp[numdofpernode_*inode+idim];

  // update element geometry of parent element
  LINALG::Matrix<nsd_,nenparent>  xrefe; // material coord. of parent element
  LINALG::Matrix<nsd_,nenparent> xcurr; // current  coord. of parent element
  {
    DRT::Node** nodes = pele->Nodes();
    for (int i=0; i<nenparent; ++i)
    {
      for (int j=0; j<nsd_; ++j)
      {
        const double* x = nodes[i]->X();
        xrefe(j,i) = x[j];
        xcurr(j,i) = xrefe(j,i) + parentdispnp[i*numdofpernode_+j];
      }
    }
  }

  // extract local values from the global vectors
  Teuchos::RCP<const Epetra_Vector> velnp = discretization.GetState("velnp");
  Teuchos::RCP<const Epetra_Vector> gridvel = discretization.GetState("gridv");

  if (velnp==Teuchos::null)
    dserror("Cannot get state vector 'velnp'");
  if (gridvel==Teuchos::null)
    dserror("Cannot get state vector 'gridv'");

  std::vector<double> myvelnp(lm.size());
  DRT::UTILS::ExtractMyValues(*velnp,myvelnp,lm);
  std::vector<double> mygridvel(lm.size());
  DRT::UTILS::ExtractMyValues(*gridvel,mygridvel,lm);

  // allocate velocity vectors
  LINALG::Matrix<nsd_,bdrynen_> evelnp(true);
  LINALG::Matrix<bdrynen_,1> epressnp(true);
  LINALG::Matrix<nsd_,bdrynen_> edispnp(true);
  LINALG::Matrix<nsd_,bdrynen_> egridvel(true);
  LINALG::Matrix<bdrynen_,1> escaaf(true);
  LINALG::Matrix<bdrynen_,1> eporosity(true);

  // split velocity and pressure, insert into element arrays
  for (int inode=0;inode<bdrynen_;inode++)
  {
    for (int idim=0; idim< nsd_; idim++)
    {
      evelnp(idim,inode)   = myvelnp[idim+(inode*numdofpernode_)];
      edispnp(idim,inode)  = mydispnp[idim+(inode*numdofpernode_)];
      egridvel(idim,inode) = mygridvel[idim+(inode*numdofpernode_)];
    }
    epressnp(inode) = myvelnp[nsd_+(inode*numdofpernode_)];
  }

  if(porositydof)
  {
    for (int inode=0;inode<bdrynen_;inode++)
      eporosity(inode) = mydispnp[nsd_+(inode*numdofpernode_)];
  }

  // get coordinates of gauss points w.r.t. local parent coordinate system
  Epetra_SerialDenseMatrix pqxg(intpoints.IP().nquad,nsd_);
  LINALG::Matrix<nsd_,nsd_>  derivtrafo(true);

  DRT::UTILS::BoundaryGPToParentGP<nsd_>( pqxg     ,
                                          derivtrafo,
                                          intpoints,
                                          pdistype ,
                                          distype  ,
                                          ele->SurfaceNumber());


  //structure velocity at gausspoint
  LINALG::Matrix<nsd_,1> gridvelint;

  //coordinates of gauss points of parent element
  LINALG::Matrix<nsd_ , 1>    pxsi(true);

  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // get shape functions and derivatives in the plane of the element
    LINALG::Matrix<nenparent,1> pfunct(true);
    LINALG::Matrix<nsd_,nenparent> pderiv;
    LINALG::Matrix<nsd_,nenparent> pderiv_loc;

    // coordinates of the current integration point
    for (int idim=0;idim<nsd_ ;idim++)
      pxsi(idim) = pqxg(gpid,idim);

    DRT::UTILS::shape_function       <pdistype>(pxsi,pfunct);
    DRT::UTILS::shape_function_deriv1<pdistype>(pxsi,pderiv_loc);

    pderiv.Multiply(derivtrafo,pderiv_loc);

    // get Jacobian matrix and determinant w.r.t. spatial configuration
    // transposed jacobian "dx/ds"
    LINALG::Matrix<nsd_,nsd_>  xjm;
    LINALG::Matrix<nsd_,nsd_> Jmat;
    xjm.MultiplyNT(pderiv_loc,xcurr);
    Jmat.MultiplyNT(pderiv_loc,xrefe);
    // jacobian determinant "det(dx/ds)"
    const double det = xjm.Determinant();
    // jacobian determinant "det(dX/ds)"
    const double detJ = Jmat.Determinant();
    // jacobian determinant "det(dx/dX) = det(dx/ds)/det(dX/ds)"
    const double J = det/detJ;

    // Computation of the integration factor & shape function at the Gauss point & derivative of the shape function at the Gauss point
    // Computation of the unit normal vector at the Gauss points
    // Computation of nurb specific stuff is not activated here
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    velint_.Multiply(evelnp,funct_);
    gridvelint.Multiply(egridvel,funct_);
    double press = epressnp.Dot(funct_);

    //double scalar = escaaf.Dot(funct_);

    double dphi_dp=0.0;
    double dphi_dJ=0.0;
    double porosity_gp=0.0;

   // params.set<double>("scalar",scalar);

    if(porositydof)
    {
      dserror("not implemented");
      //porosity_gp = eporosity.Dot(funct_);
    }
    else
    {
      so_interface->ComputeSurfPorosity(params,
                                     press,
                                     J,
                                     ele->SurfaceNumber(),
                                     gpid,
                                     porosity_gp,
                                     &dphi_dp,
                                     &dphi_dJ,
                                     NULL,                  //dphi_dJdp not needed
                                     NULL,                  //dphi_dJJ not needed
                                     NULL,                   //dphi_dpp not needed
                                     true
                                     );
    }

    // flowrate = uint o normal
    const double flowrate = ( velint_.Dot(unitnormal_)//- gridvelint.Dot(unitnormal_)
        ) * porosity_gp;

    // store flowrate at first dof of each node
    // use negative value so that inflow is positiv
    for (int inode=0;inode<bdrynen_;++inode)
    {
      // see "A better consistency for low order stabilized finite element methods"
      // Jansen, Collis, Whiting, Shakib
      //
      // Here the principle is used to bring the flow rate to the outside world!!
      //
      // funct_ *  velint * n * fac
      //   |      |________________|
      //   |              |
      //   |         flow rate * fac  -> integral over Gamma
      //   |
      // flow rate is distributed to the single nodes of the element
      // = flow rate per node
      //
      // adding up all nodes (ghost elements are handled by the assembling strategy)
      // -> total flow rate at the desired boundary
      //
      // it can be interpreted as a rhs term
      //
      //  ( v , u o n)
      //               Gamma
      //
      elevec1[inode*numdofpernode_] += funct_(inode)* fac_ * flowrate;

      // alternative way:
      //
      //  velint * n * fac
      // |________________|
      //         |
      //    flow rate * fac  -> integral over Gamma
      //     = flow rate per element
      //
      //  adding up all elements (be aware of ghost elements!!)
      //  -> total flow rate at the desired boundary
      //     (is identical to the total flow rate computed above)
    }
  }
}//DRT::ELEMENTS::FluidSurface::ComputeFlowRate


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::FPSICoupling(
                                                 DRT::ELEMENTS::FluidBoundary*    ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                plm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseVector&        elevec1)
{
  switch (distype)
  {
  // 2D:
  case DRT::Element::line2:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::quad4)
    {
      FPSICoupling<DRT::Element::quad4>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror(" expected combination line2/quad4 for surface/parent pair ");
    }
    break;
  }
  // 3D:
  case DRT::Element::quad4:
  {
    if(ele->ParentElement()->Shape()==DRT::Element::hex8)
    {
      FPSICoupling<DRT::Element::hex8>(
          ele,
          params,
          discretization,
          plm,
          elemat1,
          elevec1);
    }
    else
    {
      dserror(" expected combination quad4/hex8 for surface/parent pair ");
    }
    break;
  }
  default:
  {
    dserror("surface/parent element pair not yet implemented. Just do it.\n");
    break;
  }

  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
template <DRT::Element::DiscretizationType pdistype>
void DRT::ELEMENTS::FluidBoundaryImpl<distype>::FPSICoupling(
                                                 DRT::ELEMENTS::FluidBoundary*   ele,
                                                 Teuchos::ParameterList&          params,
                                                 DRT::Discretization&             discretization,
                                                 std::vector<int>&                plm,
                                                 Epetra_SerialDenseMatrix&        elemat1,
                                                 Epetra_SerialDenseVector&        elevec1)
{

 /*
   rauch 01/2013

          /                  \
         |                    |
  (1)    |  (u - vs) o n , q  |             normal continuity of flux in porofluid equation
         |                    |
          \                  /  Gamma_Interface

          /                                                                \
         |                                                                  |
  (2)    |  J (tau - pf o I + gamma rho_f u dyadic u) o F^-T o N , delta d  |    equality of interface traction vector in structural equation
         |                                                                  |
          \                                                                /  Gamma_Interface

          /                                                          \
         |   1                                                        |
  (3)    | ------ n o (-pf o I - gamma rho_f u dyadic u) o n , w o n  |          equality of normal interface traction in fluid equation
         | rho_f                                                      |
          \                                                          /  Gamma_Interface

          /                                                       \
         |  alphabj * mu_f                              I       I  |
  (4)    |  --------------- [u - (vs + phi(vf - vs))] o t , w o t  |             beavers-joseph condition in fluid equation
         |   rho_f sqrt(K)                                         |
          \                                                       /  Gamma_Interface


              nnod ->
             __ idof3 ->            __
     inod   |                         |
       idof2|                         |
        |   |                         |
      | V   |         elemat          |
      V     |                         |
            |                         |
            |                         |
            |__                     __|

  */


  // This function is only implemented for 3D
  if(bdrynsd_!=2 and bdrynsd_!=1)
  {
    dserror("Continuity boundary integral for FPSI coupling is only implemented for 3D and 2D!");
  }

  // number of parentnodes
   static const int nenparent = DRT::UTILS::DisTypeToNumNodePerEle<pdistype>::numNodePerElement;
   if(nenparent != 8)
   {
     dserror("nenparent not equal 8 for Hex8 element !!! ...");
   }

   // get the parent element
   DRT::ELEMENTS::Fluid* pele = ele->ParentElement();
   int currparenteleid = pele->Id();

  // get submatrix to fill
  const std::string block = params.get<std::string>("fillblock");

  // get map containing parent element facing current interface element
  const std::string tempstring("InterfaceFacingElementMap");
  Teuchos::RCP<std::map<int,int> > InterfaceFacingElementMap = params.get<Teuchos::RCP<std::map<int,int> > >(tempstring);
  std::map<int,int>::iterator it;

  // initialization of plenty of variables
  double fluiddensity               = 0.0;
  double fluiddynamicviscosity      = 0.0;
  double permeability               = 0.0;
  double beaversjosephcoefficient   = 0.0;
  double normoftangential1          = 0.0;
  double normoftangential2          = 0.0;
  double normoftangential1_n        = 0.0;
  double normoftangential2_n        = 0.0;
  double scalarintegraltransformfac = 0.0;
  double tangentialfac              = 0.0;

  LINALG::Matrix<nsd_,1> neumannoverinflow (true);

  std::vector<int> lm;
  std::vector<int> lmowner;
  std::vector<int> lmstride;

  std::vector<double>               my_displacements_np;
  std::vector<double>               my_displacements_n;
  std::vector<double>               my_parentdisp_np;
  std::vector<double>               my_parentdisp_n;
  std::vector<double>               porosity;

  LINALG::Matrix<nsd_,bdrynen_ > evelnp      (true);
  LINALG::Matrix<nsd_,bdrynen_ > eveln       (true);
  LINALG::Matrix<nsd_,nenparent> pevelnp     (true);
  LINALG::Matrix<nsd_,nenparent> peveln      (true); // at previous time step n
  LINALG::Matrix<nsd_,bdrynen_ > edispnp     (true);
  LINALG::Matrix<nsd_,bdrynen_ > egridvel    (true);
  LINALG::Matrix<nsd_,bdrynen_ > egridvel_n  (true);
  LINALG::Matrix<1   ,bdrynen_ > epressnp    (true);
  LINALG::Matrix<1   ,bdrynen_ > epressn     (true);
  LINALG::Matrix<nsd_,        1> gridvelint  (true);
  LINALG::Matrix<nsd_,        1> pxsi        (true);
  LINALG::Matrix<1   ,        1> pressint    (true);
  LINALG::Matrix<1   ,        1> pressint_n  (true); // at previous time step n
  LINALG::Matrix<nsd_,     nsd_> dudxi       (true);
  LINALG::Matrix<nsd_,     nsd_> dudxi_n     (true); // at previous time step n
  LINALG::Matrix<nsd_,     nsd_> dudxioJinv  (true);
  LINALG::Matrix<nsd_,     nsd_> dudxioJinv_n(true); // at previous time step n
  LINALG::Matrix<1   ,        1> tangentialvelocity1 (true);
  LINALG::Matrix<1   ,        1> tangentialvelocity2 (true);
  LINALG::Matrix<1   ,        1> tangentialgridvelocity1 (true);
  LINALG::Matrix<1   ,        1> tangentialgridvelocity2 (true);
  LINALG::Matrix<1   ,        1> normalvelocity (true);

  LINALG::Matrix<nsd_,nenparent>  xrefe;   // material coord. of parent element
  LINALG::Matrix<nsd_,nenparent>  xcurr;   // current  coord. of parent element
  LINALG::Matrix<nsd_,nenparent>  xcurr_n; // current  coord. of parent element at previous time step n

  Teuchos::RCP<const Epetra_Vector> displacements_np = discretization.GetState("dispnp");
  Teuchos::RCP<const Epetra_Vector> displacements_n  = discretization.GetState("dispn");
  Teuchos::RCP<const Epetra_Vector> fluidvelocity_np = discretization.GetState("velnp");
  Teuchos::RCP<const Epetra_Vector> fluidvelocity_n  = discretization.GetState("veln");
  Teuchos::RCP<const Epetra_Vector> gridvelocity     = discretization.GetState("gridv");

  if (fluidvelocity_np==Teuchos::null)
    dserror("Cannot get state vector 'fluidvelocity_np'");
  if (gridvelocity==Teuchos::null)
    dserror("Cannot get state vector 'gridvelocity'");
  if (displacements_np==Teuchos::null)
    dserror("Cannot get state vector 'displacements_np'");
  if (fluidvelocity_n ==Teuchos::null)
    dserror("Cannot get state vector 'fluidvelocity_n'");
  if (displacements_n ==Teuchos::null)
    dserror("Cannot get state vector 'displacements_n'");

  // get integration rule
  const DRT::UTILS::IntPointsAndWeights<bdrynsd_> intpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<distype>::rule);
  //(const DRT::UTILS::IntPointsAndWeights<nsd_> pintpoints(DRT::ELEMENTS::DisTypeToOptGaussRule<pdistype>::rule);

  // get node coordinates
  // (we have a nsd_ dimensional domain, since nsd_ determines the dimension of FluidBoundary element!)
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_);
  GEO::fillInitialPositionArray<distype,nsd_,LINALG::Matrix<nsd_,bdrynen_> >(ele,xyze_n_);

  // get element location vector and ownerships
  ele->DRT::Element::LocationVector(discretization,lm,lmowner,lmstride);

  // get material parameters and constants needed to calculate matrix terms
  const Teuchos::ParameterList& fpsidynparams =  DRT::Problem::Instance()->FPSIDynamicParams();

  Teuchos::RCP<MAT::Material>       fluidmaterial;
  Teuchos::RCP<MAT::Material>       generalmaterial;
  Teuchos::RCP<MAT::Material>       currentmaterial;
  Teuchos::RCP<MAT::FluidPoro>      porofluidmaterial;
  Teuchos::RCP<MAT::NewtonianFluid> newtonianfluidmaterial;

  currentmaterial = ele->ParentElement()->Material();
//  if(ele->Id()==43 and discretization.Name() == "fluid")
//  {
//    std::cout<<"Called on Dis: "<<discretization.Name()<<" boundary ele id: "<<ele->Id()<<" opposing ele id: "<<InterfaceFacingElementMap->find(ele->Id())->second<<endl;
//    std::cout<<"interface owner: "<<ele->Owner()<<"  bulk element owned by dis?: "<<discretization.HaveGlobalElement(InterfaceFacingElementMap->find(ele->Id())->second)<<endl;
//  }
    if(discretization.Name() == "fluid")
  {
    Teuchos::RCP<DRT::Discretization> porofluiddis = DRT::Problem::Instance()-> GetDis("porofluid");
    it = InterfaceFacingElementMap->find(ele->Id());
    DRT::Element* porofluidelement = porofluiddis -> gElement(it -> second);

    generalmaterial        = porofluidelement -> Material();
    porofluidmaterial      = Teuchos::rcp_dynamic_cast<MAT::FluidPoro>(generalmaterial);
    newtonianfluidmaterial = Teuchos::rcp_dynamic_cast<MAT::NewtonianFluid>(currentmaterial);

    permeability          = porofluidmaterial      -> Permeability();
    fluiddensity          = newtonianfluidmaterial -> Density();
    fluiddynamicviscosity = newtonianfluidmaterial -> Viscosity();
  }
  else if (discretization.Name() == "porofluid")
  {
    Teuchos::RCP<DRT::Discretization> fluiddis     = DRT::Problem::Instance()-> GetDis("fluid");
    it = InterfaceFacingElementMap->find(ele->Id());
    DRT::Element* fluidelement = fluiddis -> gElement(it -> second);

    fluidmaterial            = fluidelement -> Material();
    newtonianfluidmaterial   = Teuchos::rcp_dynamic_cast<MAT::NewtonianFluid>(fluidmaterial);
    porofluidmaterial        = Teuchos::rcp_dynamic_cast<MAT::FluidPoro>(currentmaterial);

    permeability             = porofluidmaterial      -> Permeability();
    fluiddensity             = newtonianfluidmaterial -> Density();
    fluiddynamicviscosity    = newtonianfluidmaterial -> Viscosity();
  }

  beaversjosephcoefficient = fpsidynparams           . get<double>("ALPHABJ");

  // calculate factor for the tangential interface condition on the free fluid field
  tangentialfac = (beaversjosephcoefficient*fluiddynamicviscosity)/(fluiddensity*sqrt(permeability));

  const double timescale = params.get<double>("timescale",-1.0);
  if(timescale == -1.0)
    dserror("no timescale parameter in parameter list");

  if (displacements_np!=Teuchos::null)
  {
    my_displacements_np.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*displacements_np,my_displacements_np,lm);
    DRT::UTILS::ExtractMyValues(*displacements_np,my_parentdisp_np,  plm);
  }
  dsassert(my_displacements_np.size()!=0,"no displacement values for boundary element");
  dsassert(my_parentdisp_np.size()!=0,   "no displacement values for parent element");

  if (displacements_n !=Teuchos::null)
  {
    my_displacements_n.resize(lm.size());
    DRT::UTILS::ExtractMyValues(*displacements_n,my_displacements_n,lm);
    DRT::UTILS::ExtractMyValues(*displacements_n,my_parentdisp_n,   plm);
  }
  dsassert(my_displacements_n.size()!=0,"no displacement values for boundary element at time step n");
  dsassert(my_parentdisp_n.size()!=0,   "no displacement values for parent element at time step n");

  // Add the deformation of the ALE mesh to the nodes coordinates
  for (int inode=0;inode<bdrynen_;++inode)
  {
    for (int idim=0; idim<nsd_; ++idim)
    {
      xyze_(idim,inode)  +=my_displacements_np[numdofpernode_*inode+idim];
      xyze_n_(idim,inode)+=my_displacements_n [numdofpernode_*inode+idim];
    }
  }

  // update element geometry of parent element
  {
    DRT::Node** nodes = pele->Nodes();
    for (int inode=0;inode<nenparent;++inode)
    {
      for (int idof=0;idof<nsd_;++idof)
      {
        const double* x = nodes[inode]->X();
        xrefe(idof,inode)   = x[idof];
        xcurr(idof,inode)   = xrefe(idof,inode) + my_parentdisp_np[inode*numdofpernode_+idof];
        xcurr_n(idof,inode) = xrefe(idof,inode) + my_parentdisp_n [inode*numdofpernode_+idof];
      }
    }
  }

  // extract local values from the global vectors
  std::vector<double> my_fluidvelocity_np(lm.size());
  DRT::UTILS::ExtractMyValues(*fluidvelocity_np,my_fluidvelocity_np,lm);
  std::vector<double> my_fluidvelocity_n(lm.size());  // at previous time step n
  DRT::UTILS::ExtractMyValues(*fluidvelocity_n,my_fluidvelocity_n,lm);
  std::vector<double> my_gridvelocity(lm.size());
  DRT::UTILS::ExtractMyValues(*gridvelocity,my_gridvelocity,lm);
  std::vector<double> my_parentfluidvelocity_np(plm.size());
  DRT::UTILS::ExtractMyValues(*fluidvelocity_np,my_parentfluidvelocity_np,plm);
  std::vector<double> my_parentfluidvelocity_n (plm.size());  // at previous time step n
  DRT::UTILS::ExtractMyValues(*fluidvelocity_n ,my_parentfluidvelocity_n ,plm);

  // split velocity and pressure, insert into element arrays
  for (int inode=0;inode<bdrynen_;inode++)
  {
    for (int idim=0; idim< nsd_; idim++)
    {
      evelnp  (idim,inode) = my_fluidvelocity_np[idim+(inode*numdofpernode_)];
      eveln   (idim,inode) = my_fluidvelocity_n [idim+(inode*numdofpernode_)];
      edispnp (idim,inode) = my_displacements_np[idim+(inode*numdofpernode_)];
      egridvel(idim,inode) = my_gridvelocity    [idim+(inode*numdofpernode_)];
    }
    epressnp(inode) = my_fluidvelocity_np[nsd_+(numdofpernode_*inode)];
    epressn (inode) = my_fluidvelocity_n [nsd_+(numdofpernode_*inode)];
  }

  for (int inode=0;inode<nenparent;inode++)
  {
    for (int idim=0; idim< nsd_; idim++)
    {
      pevelnp  (idim,inode) = my_parentfluidvelocity_np[idim+(inode*numdofpernode_)];
      peveln   (idim,inode) = my_parentfluidvelocity_n [idim+(inode*numdofpernode_)];
    }
  }

  // get porosity values from parent element
    Teuchos::RCP<DRT::Discretization> structdis = Teuchos::null;

      //access structure discretization
      structdis = DRT::Problem::Instance()->GetDis("structure");
      //std::cout<<structdis<<endl;
      DRT::Element* structele = NULL;
      //get corresponding structure element (it has the same global ID as the porofluid element)
      if(discretization.Name()=="structure" or discretization.Name()=="porofluid")
      {
        structele = structdis->gElement(currparenteleid);
      }
      else if(discretization.Name()=="fluid")
      {
        it = InterfaceFacingElementMap->find(ele->Id());
        structele = structdis -> gElement(it -> second);
      }

      if (structele == NULL)
      {
        dserror("Structure element %i not on local processor", currparenteleid);
      }
      // get porous material
      const Teuchos::RCP<MAT::StructPoro>& structmat = Teuchos::rcp_dynamic_cast<MAT::StructPoro>(structele->Material());
      if(structmat->MaterialType() != INPAR::MAT::m_structporo)
      {
       dserror("invalid structure material for poroelasticity");
      }

  // get coordinates of gauss points w.r.t. local parent coordinate system
  Epetra_SerialDenseMatrix pqxg(intpoints.IP().nquad,nsd_);
  LINALG::Matrix<nsd_,nsd_>  derivtrafo(true);

  DRT::UTILS::BoundaryGPToParentGP<nsd_>( pqxg      ,
                                          derivtrafo,
                                          intpoints ,
                                          pdistype  ,
                                          distype   ,
                                          ele->SurfaceNumber());

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////     Loop over Gauss-Points    /////////////////////
  ////////////////////////////////////////////////////////////////////////////
  for (int gpid=0; gpid<intpoints.IP().nquad; gpid++)
  {
    // get shape functions and derivatives in the plane of the element
    LINALG::Matrix<nenparent,1>    pfunct    (true); // parent element shape function
    LINALG::Matrix<nsd_,nenparent> pderiv    (true); // derivatives of parent element shape functions in interface coordinate system
    LINALG::Matrix<nsd_,nenparent> pderiv_loc(true); // derivatives of parent element shape functions in parent element coordinate system

    // coordinates of the current integration point in parent coordinate system
    for (int idim=0;idim<nsd_ ;idim++)
    {
      pxsi(idim) = pqxg(gpid,idim);
    }

    // evalute parent element shape function at current integration point in parent coordinate system
    DRT::UTILS::shape_function       <pdistype>(pxsi,pfunct);
    // evaluate derivatives of parent element shape functions at current integration point in parent coordinate system
    DRT::UTILS::shape_function_deriv1<pdistype>(pxsi,pderiv_loc);
    // transformation from parent element coordinate system to interface element coordinate system
    pderiv.Multiply(derivtrafo,pderiv_loc);

//    std::cout<<"pderiv : "<<pderiv<<endl;
//    std::cout<<"pderiv_loc : "<<pderiv_loc<<endl;

    double dphi_dp=0.0;
    double dphi_dJ=0.0;
    double dphi_dJdp=0.0;
    double dphi_dJJ=0.0;
    double dphi_dpp=0.0;
    double porosityint=0.0;

    // get Jacobian matrix and determinant w.r.t. spatial configuration
    //
    // |J| = det(xjm) * det(Jmat^-1) = det(xjm) * 1/det(Jmat)
    //
    //    _                     _
    //   |  x_1,1  x_2,1  x_3,1  |           d x_i
    //   |  x_1,2  x_2,2  x_3,2  | = xjm  = --------
    //   |_ x_1,3  x_2,3  x_3,3 _|           d s_j
    //    _
    //   |  X_1,1  X_2,1  X_3,1  |           d X_i
    //   |  X_1,2  X_2,2  X_3,2  | = Jmat = --------
    //   |_ X_1,3  X_2,3  X_3,3 _|           d s_j
    //
    LINALG::Matrix<nsd_,nsd_>    xjm;
    LINALG::Matrix<nsd_,nsd_>    xjm_n; // at previous time step n
    LINALG::Matrix<nsd_,nsd_>   Jmat;
    xjm.MultiplyNT (pderiv_loc,xcurr);
    xjm_n.MultiplyNT (pderiv_loc,xcurr_n);
    Jmat.MultiplyNT(pderiv_loc,xrefe);
    double det  = xjm.Determinant();
    double detJ = Jmat.Determinant();
    const double J = det/detJ;

    // inverse of transposed jacobian "ds/dx" (xjm)
    LINALG::Matrix<nsd_,nsd_> xji;
    LINALG::Matrix<nsd_,nsd_> xji_n; // at previous time step n
    //    _                     _
    //   |  s_1,1  s_2,1  s_3,1  |           d s_i
    //   |  s_1,2  s_2,2  s_3,2  | = xji  = -------- ;  [xji] o [xjm] = I
    //   |_ s_1,3  s_2,3  s_3,3 _|           d x_j
    //    _
    xji.Invert(xjm);
    xji_n.Invert(xjm_n);

    // check unitiy of  [xji] o [xjm]
    LINALG::Matrix<nsd_,nsd_> eye;
    eye.Multiply(xji,xjm);
    if(abs(eye(0,0)-1.0) > 1e-11 or abs(eye(1,1)-1.0) > 1e-11 or abs(eye(2,2)-1.0) > 1e-11)
    {
      std::cout<<eye<<std::endl;
      dserror("matrix times its inverse is not equal identity ... that sucks !!!");
    }
    if(abs(eye(0,1)) > 1e-11 or abs(eye(0,2)) > 1e-11 or abs(eye(1,0)) > 1e-11 or abs(eye(1,2)) > 1e-11  or abs(eye(2,0)) > 1e-11  or abs(eye(2,1)) > 1e-11 )
    {
      std::cout<<eye<<std::endl;
      dserror("matrix times its inverse is not equal identity ... that sucks !!!");
    }

    // evaluate unitnormal_ , deriv_, ...
    EvalShapeFuncAtBouIntPoint(intpoints,gpid,NULL,NULL);

    // fac_ = intpoints.IP().qwgt[gpid]*drs_ done in EvalShapeFuncAtBouIntPoint()
    const double timefac       = fldpara_->TimeFac();
    const double timefacpre    = fldpara_->TimeFacPre();
    const double timefacfacpre = fldpara_->TimeFacPre()    * fac_;
    const double rhsfac        = fldpara_->TimeFacRhs()    * fac_;
    //const double rhsfacpre     = fldpara_->TimeFacRhsPre() * fac_;
    const double theta         = fldpara_->Theta();

    // The integration factor is not multiplied with drs
    // since it is the same as the scaling factor for the unit normal derivatives
    // Therefore it cancels out!!
    const double fac = intpoints.IP().qwgt[gpid];

    // calculate variables at gausspoint
    velint_     .Multiply(evelnp,    funct_);
    velint_n_   .Multiply(eveln ,    funct_);
    gridvelint  .Multiply(egridvel  ,funct_);
    pressint    .Multiply(epressnp,  funct_);
    pressint_n  .Multiply(epressn ,  funct_);

    //                                         _              _
    //                                        | u1,1 u1,2 u1,3 |
    // dudxi = u_i,alhpa = N_A,alpha u^A_i =  | u2,1 u2,2 u2,3 |
    //                                        |_u3,1 u3,2 u3,3_|
    //
    dudxi  .MultiplyNT(pevelnp,pderiv);    // corrected: switched pevelnp and pderiv
    dudxi_n.MultiplyNT(peveln,pderiv);

    //                                            l=_  1     2     3  _
    //         -1                               i=1| u1,x1 u1,x2 u1,x3 |
    // dudxi o J  = N_A,alpha u^A_i xi_alpha,l =  2| u2,x1 u2,x2 u2,x3 | = gradu
    //                                            3|_u3,x1 u3,x2 u3,x3_|
    //
    dudxioJinv.MultiplyNT(dudxi,xji);
    dudxioJinv_n.MultiplyNT(dudxi_n,xji_n); // at previus time step n

    LINALG::Matrix<1   ,     nsd_> graduon(true);
    LINALG::Matrix<1   ,     nsd_> graduon_n(true); // from previous time step
    //
    // l=  1     2     3
    // [  ...   ...   ...  ]
    //
    //
    for (int idof=0;idof<nsd_;idof++) // l Loop
    {
      for (int idof2=0;idof2<nsd_;idof2++)
      {
        graduon(0,idof)   += dudxioJinv(idof,idof2)*unitnormal_(idof2);
        graduon_n(0,idof) += dudxioJinv_n(idof,idof2)*unitnormal_n_(idof2);
      }
    }
    LINALG::Matrix<1   ,     nsd_> graduTon  (true);
    LINALG::Matrix<1   ,     nsd_> graduTon_n(true); // at previous time step n
    //
    // l=  1     2     3
    // [  ...   ...   ...  ]
    //
    //
    for (int idof=0;idof<nsd_;idof++) // l Loop
    {
      for (int idof2=0;idof2<nsd_;idof2++)
      {
        graduTon  (0,idof) += dudxioJinv  (idof2,idof)*unitnormal_(idof2);
        graduTon_n(0,idof) += dudxioJinv_n(idof2,idof)*unitnormal_n_(idof2);
      }
    }

    if(discretization.Name() == "porofluid" or discretization.Name() == "structure")
      structmat->ComputeSurfPorosity(params,pressint(0,0), J,ele->SurfaceNumber(),gpid,porosityint,&dphi_dp,&dphi_dJ,&dphi_dJdp,&dphi_dJJ,&dphi_dpp,false);
    else
      porosityint = 1.0;


    if(porosityint < 0.00001)
    { std::cout<<"Discretization: "<<discretization.Name()<<std::endl;
      std::cout<<"SurfaceNumber:  "<<ele->SurfaceNumber()<<std::endl;
      std::cout<<"Porosity:       "<<porosityint<<"  at gp: "<<gpid<<std::endl;
      std::cout<<"Pressure at gp: "<<pressint(0,0)<<std::endl;
      std::cout<<"Jacobian:       "<<J<<std::endl;
      dserror("unreasonably low porosity for poro problem");
    }
    // dxyzdrs vector -> normal which is not normalized built from cross product of columns
    // of Jacobian matrix d(x,y,z)/d(r,s)
    LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs(0.0);
    LINALG::Matrix<bdrynsd_,nsd_> dxyzdrs_n(0.0);
    dxyzdrs.MultiplyNT(deriv_,xyze_);
    dxyzdrs_n.MultiplyNT(deriv_,xyze_n_);

    // tangential surface vectors are columns of dxyzdrs
    LINALG::Matrix<nsd_,1> tangential1(0.0);
    LINALG::Matrix<nsd_,1> tangential2(0.0);
    LINALG::Matrix<nsd_,1> tangential1_n(0.0);
    LINALG::Matrix<nsd_,1> tangential2_n(0.0);

    for (int idof=0;idof<nsd_;idof++)
    {
      tangential1(idof,0) = dxyzdrs(0,idof);
      tangential2(idof,0) = dxyzdrs(1,idof);

      tangential1_n(idof,0) = dxyzdrs_n(0,idof);
      tangential2_n(idof,0) = dxyzdrs_n(1,idof);
    }

    normoftangential1 = tangential1.Norm2();
    normoftangential2 = tangential2.Norm2();
    normoftangential1_n = tangential1_n.Norm2();
    normoftangential2_n = tangential2_n.Norm2();

    // normalize tengential vectors
    tangential1.Scale(1/normoftangential1);
    tangential2.Scale(1/normoftangential2);

    tangential1_n.Scale(1/normoftangential1_n);
    tangential2_n.Scale(1/normoftangential2_n);

    //                                                             I
    // calculate tangential structure velocity (gridvelocity) vs o t
    //
    // [nsd_ x 1] o [nsd_ x 1]
    //
    LINALG::Matrix<1,1> tangentialvs1(true);
    LINALG::Matrix<1,1> tangentialvs2(true);
    tangentialvs1.MultiplyTN(gridvelint,tangential1);
    tangentialvs2.MultiplyTN(gridvelint,tangential2);

    //                                          I
    // calculate tangential fluid velocity vf o t
    //
    // [nsd_ x 1] o [nsd_ x 1]
    //
    LINALG::Matrix<1,1> tangentialvf1(true);
    LINALG::Matrix<1,1> tangentialvf2(true);
    tangentialvf1.MultiplyTN(velint_,tangential1);
    tangentialvf2.MultiplyTN(velint_,tangential2);
    //std::cout<<"Tangential Structure Velocity at integration point: \n"<<tangentialvs1<<endl;

    //  derivatives of surface tangentials with respect to mesh displacements
    //              I
    //            d t_i             I                               I   I
    //            -------- = 1/abs( t )* (N_L,(r,s) Kronecker^i_l - t_i t_l N_L,(r,s) )
    //            d d^L_l
    //
    //         _______________L=1_____________    ______________L=2_____________   ______ ...
    //     __ /l =  1         2         3     \  /l = 1          2        3     \ /       __
    //  i= |                                    |                                |          |
    //  t1 |  N_1,(r,s)-() -(...)      -(...)   |  N_2,(r,s)   ...       ...     |  ...     |
    //     |                                    |                                |          |
    //  t2 |  -(...)     N_1,(r,s)-()  -(...)   |    ...      N_2,(r,s)  ...     |  ...     |
    //     |                                    |                                |          |
    //  t3 |  -(...)     -(...)    N_1,(r,s)-() |    ...       ...     N_2,(r,s) |  ...     |
    //     |_                                                                              _|
    //
    LINALG::Matrix<nsd_,nenparent*nsd_> tangentialderiv1(true);
    LINALG::Matrix<nsd_,nenparent*nsd_> tangentialderiv2(true);

    for (int node=0;node<nenparent;++node)
    {
      // block diagonal entries
      for (int idof=0;idof<nsd_;++idof)
      {
          tangentialderiv1(idof,(node*nsd_)+idof) = pderiv(0,node)/normoftangential1;
          tangentialderiv2(idof,(node*nsd_)+idof) = pderiv(1,node)/normoftangential2;
      }

      // terms from linearization of norm
      for (int idof=0;idof<nsd_;++idof)
      {
        for (int idof2=0;idof2<nsd_;idof2++)
        {
          tangentialderiv1(idof,(node*nsd_)+idof2) -= (tangential1(idof,0)*tangential1(idof2,0)*pderiv(0,node))/(pow(normoftangential1,3.0));
          tangentialderiv2(idof,(node*nsd_)+idof2) -= (tangential1(idof,0)*tangential1(idof2,0)*pderiv(1,node))/(pow(normoftangential2,3.0));;
        }
      }
    }
    //          I        ___L=1___  __L=2___  ___ ...
    //        d t_j     /l=1 2 3  \/l=1 2 3 \/
    // vs_j --------- = [  x x x      x x x            ]
    //       d d^L_l
    //
    LINALG::Matrix<nenparent*nsd_,1> vsotangentialderiv1(true);
    LINALG::Matrix<nenparent*nsd_,1> vsotangentialderiv2(true);
    for (int inode=0;inode<nenparent;inode++)
    {
      for (int idof=0;idof<nsd_;idof++)
      {
        for (int idof2=0;idof2<nsd_;idof2++)
        {
          vsotangentialderiv1((inode*nsd_)+idof,0) += gridvelint(idof2,0)*tangentialderiv1(idof2,(inode*nsd_)+idof);
          vsotangentialderiv2((inode*nsd_)+idof,0) += gridvelint(idof2,0)*tangentialderiv2(idof2,(inode*nsd_)+idof);
        }
      }
    }
    LINALG::Matrix<nenparent*nsd_,1> vfotangentialderiv1(true);
    LINALG::Matrix<nenparent*nsd_,1> vfotangentialderiv2(true);
    for (int inode=0;inode<nenparent;inode++)
    {
      for (int idof=0;idof<nsd_;idof++)
      {
        for (int idof2=0;idof2<nsd_;idof2++)
        {
          vfotangentialderiv1((inode*nsd_)+idof,0) += velint_(idof2,0)*tangentialderiv1(idof2,(inode*nsd_)+idof);
          vfotangentialderiv2((inode*nsd_)+idof,0) += velint_(idof2,0)*tangentialderiv2(idof2,(inode*nsd_)+idof);
        }
      }
    }


    //  derivatives of surface normals with respect to mesh displacements:
    //                                 d n_i
    //                                --------
    //                                 d d^L_l
    //
    //  parent element shape functions are used because the matrix normalderiv
    //  must have the proper dimension to be compatible to the evaluation of
    //  the matrix terms. as built below the matrix normalderiv has more entries
    //  than needed to calculate the surface integrals since the derivatives of
    //  the parent element shape functions do not necessarily vanish at the boundary
    //  gauss points. later those additional entries are however multiplied by the
    //  weighting function in those gauss points which are only different from zero
    //  when they belong to an interface node. thus all terms not belonging to the
    //  interface and its corresponding basic functions become zero. this makes perfect
    //  sense for the normal and its linearization are well determined solely by the
    //  surface of the element.
    LINALG::Matrix<nsd_,nenparent*nsd_> normalderiv(true);

    if(nsd_ == 3)
      for (int node=0;node<nenparent;++node)
      {
        normalderiv(0,3*node)   += 0.;
        normalderiv(0,3*node+1) += (pderiv(0,node)*dxyzdrs(1,2)-pderiv(1,node)*dxyzdrs(0,2));
        normalderiv(0,3*node+2) += (pderiv(1,node)*dxyzdrs(0,1)-pderiv(0,node)*dxyzdrs(1,1));

        normalderiv(1,3*node)   += (pderiv(1,node)*dxyzdrs(0,2)-pderiv(0,node)*dxyzdrs(1,2));
        normalderiv(1,3*node+1) += 0.;
        normalderiv(1,3*node+2) += (pderiv(0,node)*dxyzdrs(1,0)-pderiv(1,node)*dxyzdrs(0,0));

        normalderiv(2,3*node)   += (pderiv(0,node)*dxyzdrs(1,1)-pderiv(1,node)*dxyzdrs(0,1));
        normalderiv(2,3*node+1) += (pderiv(1,node)*dxyzdrs(0,0)-pderiv(0,node)*dxyzdrs(1,0));
        normalderiv(2,3*node+2) += 0.;
      }
    else
      for (int node=0;node<nenparent;++node)
      {
        normalderiv(0,nsd_*node)   += 0.;
        normalderiv(0,nsd_*node+1) += deriv_(0,node) * funct_(node) * fac;

        normalderiv(1,nsd_*node)   += -deriv_(0,node) * funct_(node) * fac;
        normalderiv(1,nsd_*node+1) += 0.;
      }


      // dxyzdrs(0,:) x dxyzdrs(1,:) non unit normal
      //           _     _       _     _
      //          |       |     |       |
      //          | x_1,r |     | x_1,s |
      //          |       |     |       |
      //          | x_2,r |  X  | x_2,s |
      //          |       |     |       |
      //          | x_3,r |     | x_3,s |
      //          |_     _|     |_     _|
      //
      LINALG::Matrix<nsd_,1> normal(true);

      normal(0,0) = dxyzdrs(0,1)*dxyzdrs(1,2) - dxyzdrs(0,2)*dxyzdrs(1,1);
      normal(1,0) = dxyzdrs(0,2)*dxyzdrs(1,0) - dxyzdrs(0,0)*dxyzdrs(1,2);
      normal(2,0) = dxyzdrs(0,0)*dxyzdrs(1,1) - dxyzdrs(0,1)*dxyzdrs(1,0);
      // transformation factor for surface integrals without normal vector
      scalarintegraltransformfac = normal.Norm2(); // || x,r x x,s ||

      // linearization of || x,r x x,s || = ||n||
      //
      //                L=__                           1                                                      2        ...     nenparent __
      //  d ||n||    l=  |                                                                               |          |        |             |
      //  ------- :   1  |1/||n||*(n_2*(x_3,1 N_L,2 - x_3,2 N_L,1) + n_3*(x_2,2 N_L,1 - x_2,1 N_L,2))    |          |        |             |
      //  d d^L_l     2  |1/||n||*(n_1*(x_3,2 N_L,1 - x_3,1 N_L,2) + n_3*(x_1,1 N_L,2 - x_1,2 N_L,1))    |          |        |             |
      //              3  |1/||n||*(n_1*(x_2,1 N_L,2 - x_2,2 N_L,1) + n_2*(x_1,2 N_L,1 - x_1,1 N_L,2))    |          |        |             |
      //                 |_                                                                              |          |        |            _|
      //
      //
      LINALG::Matrix<nsd_,nenparent> linearizationofscalarintegraltransformfac(true);

      for (int node=0;node<nenparent;++node)
      {
        linearizationofscalarintegraltransformfac(0,node) =
            (
             normal(1,0)*(dxyzdrs(0,2)*pderiv(1,node) - dxyzdrs(1,2)*pderiv(0,node))+
             normal(2,0)*(dxyzdrs(1,1)*pderiv(0,node) - dxyzdrs(0,1)*pderiv(1,node))
            )/scalarintegraltransformfac;

        linearizationofscalarintegraltransformfac(1,node) =
            (
             normal(0,0)*(dxyzdrs(1,2)*pderiv(0,node) - dxyzdrs(0,2)*pderiv(1,node))+
             normal(2,0)*(dxyzdrs(0,0)*pderiv(1,node) - dxyzdrs(1,0)*pderiv(0,node))
            )/scalarintegraltransformfac;

        linearizationofscalarintegraltransformfac(2,node) =
            (
             normal(0,0)*(dxyzdrs(0,1)*pderiv(1,node) - dxyzdrs(1,1)*pderiv(0,node))+
             normal(1,0)*(dxyzdrs(1,0)*pderiv(0,node) - dxyzdrs(0,0)*pderiv(1,node))
            )/scalarintegraltransformfac;
      }


      //------------------------------------- d|J|/dd = d|J|/dF : dF/dd = |J| * F^-T . N_X = |J| * N_x
      //
      // linearization of jacobian determinant w.r.t. structural displacements
      LINALG::Matrix<1,nsd_*nenparent> dJ_dds;
      // global derivatives of shape functions w.r.t x,y,z (material configuration)
      LINALG::Matrix<nsd_,nenparent> derxy;

      //                                        _                          _
      //            d  N_A      d xi_alpha     |  N1,1 N2,1 N3,1 N4,1 ...   |
      //  derxy  = ----------  ----------- =   |  N1,2 N2,2 N3,2 N4,2 ...   |
      //            d xi_alpha  d   x_j        |_ N1,3 N2,3 N3,3 N4,3 ...  _|
      //
      derxy.Multiply(xji,pderiv);

      for (int i=0; i<nenparent; i++)
        for (int j=0; j<nsd_; j++)
          dJ_dds(j+i*nsd_)=J*derxy(j,i);

      //
      //
      //            d xi_beta
      //  N_L,beta  ---------- n^j = derxy o n
      //            d   x_j
      //
      LINALG::Matrix<1,nenparent> dNdxon(true);
      for (int inode=0;inode<nenparent;inode++)
      {
        for (int idof=0;idof<nsd_;idof++)
        {
          dNdxon(0,inode) += derxy(idof,inode)*unitnormal_(idof);
        }
      }


//      std::cout<<"pfunct : "<<pfunct<<endl;
//      std::cout<<"funct_ : "<<funct_<<endl;

      //LINALG::Matrix<1,nsd_*nenparent> gradNon;
      LINALG::Matrix<1,     nenparent> gradNon(true);
      LINALG::Matrix<1,nsd_*nenparent> gradN(true);
      //              d xi_alpha
      //  N_L,alpha  ------------ [g_L x g_j]
      //              d  x_j
      //
      //      ___L=1___  __L=2___  ___ ...
      //     /j=1 2 3  \/j=1 2 3 \/
      //    [  x x x      x x x            ]
      //
      //gradN.MultiplyTT(pderiv,xji);
      for (int inode=0;inode<nenparent;inode++) // L     Loop
      {
        for (int idof=0;idof<nsd_;idof++)       // j     Loop
        {
          for (int idof2=0;idof2<nsd_;idof2++)  // alpha Loop
          {
            //gradNon(0,(inode*nsd_)+idof) = pderiv(idof2,inode)*(xji(idof,idof2))*unitnormal_(idof);
            gradN(0,(inode*nsd_)+idof)   += pderiv(idof2,inode)*(xji(idof,idof2));
            //std::cout<<pderiv<<endl;
            //std::cout<<xjm<<endl;
            //std::cout<<xji<<endl;
            //dserror("");
          }
          gradNon(0,inode)+= gradN(0,inode*nsd_+idof)*unitnormal_(idof);
        }
      }


      // gradient of u once contracted with linearization of normal
      //
      //                                L= 1 ... nenparent
      //                         i=   _ l= 1 ... nsd_        _
      //               d  n_j      1 |     ...                |
      //   N_A,j u^A_i -------- =  2 |     ...                |
      //               d d^L_l     3 |_    ...               _|
      //
      LINALG::Matrix<nsd_,nsd_*nenparent> graduonormalderiv;
      graduonormalderiv.Multiply(dudxioJinv,normalderiv);

      // transposed gradient of u once contracted with linearization of normal
      //
      //                                L= 1 ... nenparent
      //                         i=   _ l= 1 ... nsd_        _
      //               d  n_j      1 |     ...                |
      //   N_A,i u^A_j -------- =  2 |     ...                |
      //               d d^L_l     3 |_    ...               _|
      //
      LINALG::Matrix<nsd_,nsd_*nenparent> graduTonormalderiv;
      graduTonormalderiv.MultiplyTN(dudxioJinv,normalderiv);

      // Isn't that cool?
      LINALG::Matrix<1,nenparent> survivor;
      for (int inode=0;inode<nenparent;inode++)
      {
        if (pfunct(inode) != 0)
        {
          survivor(0,inode) = 1.0;
        }
        else
        {
          survivor(0,inode) = 0.;
        }
      }

      if(abs(scalarintegraltransformfac - drs_) > 1e-11)
      {
        std::cout<<"drs_ = "<<drs_<<std::endl;
        std::cout<<"scalarintegraltransformfac = "<<scalarintegraltransformfac<<std::endl;
        dserror("scalarintegraltransformfac should be equal drs_ !");
      }

      normalvelocity.MultiplyTN(velint_ ,unitnormal_);

      ////////////////////////////////////////////////////////////////////////////
      //////////////////////////      Loop over Nodes       //////////////////////
      ////////////////////////////////////////////////////////////////////////////
      for (int inode=0;inode<nenparent;inode++)
      {
        double normal_u_minus_vs = 0.0;
        LINALG::Matrix<1,nsd_> u_minus_vs(true);

        for (int idof=0;idof<nsd_;idof++)
        {
          normal_u_minus_vs += unitnormal_(idof) * (velint_(idof) - gridvelint(idof));
          u_minus_vs(idof)   = velint_(idof) - gridvelint(idof);
        }

        LINALG::Matrix<1,nenparent*nsd_> u_minus_vs_normalderiv(true);
        u_minus_vs_normalderiv.Multiply(u_minus_vs,normalderiv);


        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////      Fill Element Matrix      /////////////////////
        ////////////////////////////////////////////////////////////////////////////
        for (int nnod=0;nnod<nenparent;nnod++)
        {
          for (int idof2=0;idof2<nsd_;idof2++)
          {
            if(block == "Porofluid_Freefluid")
            {
              /*
                    d(q,(u-vs) o n) / d(u)

                    evaluated on FluidField(): flip sign because unitnormal_ points in opposite direction
               */
                //double timefacfacpre = DRT::ELEMENTS::FluidEleParameter::Instance(INPAR::FPSI::porofluid)->TimeFacPre();
                elemat1(inode*numdofpernode_+nsd_,nnod*numdofpernode_+idof2) -=
                  (
                        (timefacfacpre) * pfunct(inode) * unitnormal_(idof2) * pfunct(nnod)
                  );
             }
            else if (block == "Porofluid_Structure")
            {
              /*
                      d(q,(u-vs) o n) / d(ds)

                      evaluated on FluidField(): unitnormal_ points in wrong direction -> flip sign
               */
              //double timefacpre = DRT::ELEMENTS::FluidEleParameter::Instance(INPAR::FPSI::porofluid)->TimeFacPre();
              elemat1(inode*numdofpernode_+nsd_,nnod*numdofpernode_+idof2) +=
                                  - u_minus_vs_normalderiv(0,nnod*nsd_+idof2) * pfunct(inode) * timefacpre *fac* survivor(nnod) // no drs_ needed, since it is contained in the linearization w.r.t. nonunitnormal (normalderiv) -> timefacpre*fac instead of timefafacpre = timefacpre * fac_ (fac_ = fac*drs_)
                                  + pfunct(inode) * unitnormal_(idof2) * timescale * pfunct(nnod) * (timefacfacpre);
            }

            else if (block == "Fluid_Porofluid")
            {
              /*
                        d(w o n, pf_pm) / d(pf_pm) (3)

                        evaluated on PoroField(): flip sign because unitnormal_ points in opposite direction
               */
              elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+nsd_) -=
                             ( // sign checked to be negative
                                 pfunct(inode) * pfunct(nnod) * unitnormal_(idof2)

                              )/fluiddensity*fac_*timefac;//scalarintegraltransformfac;

              /*                              _                      _
                              I  alpha mu_f  |                        |   I  /
                        d(w o t,------------ | u - (vs + phi(vf -vs)) | o t / d(pfpm)
                                  rho_f K    |_           |          _|    /
                                 \_________/              V
                                tangentialfac         porosityint

                                evaluated on PoroField(): no sign flipping because there's no multiplication by unitnormal_

               */
              elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+nsd_) -=
                            ( // sign checked to be negative
                                 tangential1(idof2,0)*(tangentialvf1(0,0)-tangentialvs1(0,0)) +   // d phi / dpfpm
                                 tangential2(idof2,0)*(tangentialvf2(0,0)-tangentialvs2(0,0))

                             )*pfunct(inode)*tangentialfac*dphi_dp*fac_*timefac;//scalarintegraltransformfac;

              for (int idof3=0;idof3<nsd_;idof3++)
              {
                /*                              _                      _
                                I  alpha mu_f  |                        |   I  /
                          d(w o t,------------ | u - (vs + phi(vf -vs)) | o t / d(vf)
                                    rho_f K    |_           |          _|    /
                                   \_________/              V
                                  tangentialfac         porosityint

                                  evaluated on PoroField(): no sign flipping because there's no multiplication by unitnormal_

                 */
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3) -=
                    ( // sign checked to be negative
                        tangential1(idof2,0)*tangential1(idof3,0) +
                        tangential2(idof2,0)*tangential2(idof3,0)

                    )*pfunct(inode)*pfunct(nnod)*porosityint*tangentialfac*fac_*timefac;

              }
            }

            else if (block == "Fluid_Structure")
            {
              if (discretization.Name() == "porofluid")
              {
                /*
                        d(w o n, pf_pm * drs_) / d(ds)

                        evaluated on PoroField(): flip sign because unitnormal_ points in opposite direction
                 */
                for (int idof3=0;idof3<nsd_;idof3++)
                {
                  elemat1((inode*numdofpernode_)+idof2,(nnod*nsd_)+idof3) -=
                      (
                          pfunct(inode) * normalderiv(idof2,(nnod*nsd_)+idof3)* drs_ +
                          pfunct(inode) * unitnormal_(idof2) * (linearizationofscalarintegraltransformfac(idof3,nnod))    // d ||n|| / d d^l_L
                      )*pressint(0,0)/fluiddensity * fac * timefac * survivor(nnod) ; // *fac_ since normalderiv is referring to the test function
                }// idof3

                /*                              _                      _
                              I  alpha mu_f  |                        |   I  /
                        d(w o t,------------ | u - (vs + phi(vf -vs)) | o t / d(ds)
                                  rho_f K    |_           |          _|    /
                                 \_________/              V
                                tangentialfac         porosityint

                                evaluated on PoroField():
                 */
                for (int idof3=0;idof3<nsd_;idof3++)
                {
                  elemat1((inode*numdofpernode_)+idof2,(nnod*nsd_)+idof3)  -=
                      ((
                          tangential1(idof2,0)*(tangentialvs1(0,0) + porosityint*(tangentialvf1(0,0) - tangentialvs1(0,0)))+      // d ||n||/d d^L_l
                          tangential2(idof2,0)*(tangentialvs2(0,0) + porosityint*(tangentialvf2(0,0) - tangentialvs2(0,0)))

                      )*(linearizationofscalarintegraltransformfac(idof3,nnod)/drs_)*survivor(nnod) // -> survivor(nnod) in order to filter the entries which do not belong to the interface
                      +(
                          tangentialderiv1(idof2,(nnod*nsd_)+idof3)*(porosityint*(tangentialvf1(0,0) - tangentialvs1(0,0)))+      // d t^i/d d^L_l
                          tangentialderiv2(idof2,(nnod*nsd_)+idof3)*(porosityint*(tangentialvf2(0,0) - tangentialvs2(0,0)))

                      )*porosityint*survivor(nnod)
                  +(
                      tangential1(idof2,0)*(vfotangentialderiv1((nnod*nsd_)+idof3) - vsotangentialderiv1((nnod*nsd_)+idof3)) + // d t^j/d d^L_l
                      tangential2(idof2,0)*(vfotangentialderiv2((nnod*nsd_)+idof3) - vsotangentialderiv2((nnod*nsd_)+idof3))

                  )*porosityint*survivor(nnod)
                  -(
                      tangential1(idof2,0)*tangential1(idof3,0) +                              // d vs / d d^L_l  (sign checked)
                      tangential2(idof2,0)*tangential2(idof3,0)

                  )*pfunct(nnod)*timescale*porosityint
                  +(
                      tangential1(idof2,0)*(tangentialvf1(0,0)-tangentialvs1(0,0)) +           // d phi / d d^L_l
                      tangential2(idof2,0)*(tangentialvf2(0,0)-tangentialvs2(0,0))

                  )*dphi_dJ*dJ_dds((nnod*nsd_)+idof3)
                  +(
                      tangential1(idof2,0)*tangential1(idof3,0) +                             // d vs / d d^L_l (front term without phi) (sign checked)
                      tangential2(idof2,0)*tangential2(idof3,0)

                  )*pfunct(nnod)*timescale
                  +(
                      tangentialderiv1(idof2,(nnod*nsd_)+idof3)*tangentialvs1(0,0) +           // d t^i/d d^L_l (front term without phi)
                      tangentialderiv2(idof2,(nnod*nsd_)+idof3)*tangentialvs2(0,0)

                  )*survivor(nnod)
                  +(
                      tangential1(idof2,0)*vsotangentialderiv1((nnod*nsd_)+idof3) +            // d t^j/d d^L_l (front term without phi)
                      tangential2(idof2,0)*vsotangentialderiv2((nnod*nsd_)+idof3)

                  )*survivor(nnod)

                      )*pfunct(inode)*tangentialfac*fac_*timefac;
                }// idof3
              }
              else if (discretization.Name() == "fluid")
              {
                for (int idof3=0;idof3<nsd_;idof3++)
                {
                  elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3) +=

                    ( (
                          tangential1(idof2,0)*tangentialvf1(0,0)+      // d ||n||/d d^L_l
                          tangential2(idof2,0)*tangentialvf2(0,0)

                      )*(linearizationofscalarintegraltransformfac(idof3,nnod)/drs_)*survivor(nnod) // -> survivor(nnod) in order to filter the entries which do not belong to the interface
                    + (
                          tangentialderiv1(idof2,(nnod*nsd_)+idof3)*tangentialvf1(0,0)+      // d t^i/d d^L_l
                          tangentialderiv2(idof2,(nnod*nsd_)+idof3)*tangentialvf2(0,0)

                      )*survivor(nnod)
                  +(
                      tangential1(idof2,0)*vfotangentialderiv1((nnod*nsd_)+idof3)+ // d t^j/d d^L_l
                      tangential2(idof2,0)*vfotangentialderiv2((nnod*nsd_)+idof3)

                   )*survivor(nnod)
                  )*fac_*timefac*pfunct(inode)*tangentialfac;
                }
              }
            }// block Fluid_Structure

            else if (block == "Fluid_Fluid")
            {
              /*
                        d(w o t,tangentialfac * u o t) / d(du)
               */
              for (int idof3=0;idof3<nsd_;idof3++)
              {
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  +=
                    (
                      tangential1(idof2)*tangential1(idof3) +
                      tangential2(idof2)*tangential2(idof3)
                     )*pfunct(nnod)*pfunct(inode)*tangentialfac*fac_*timefac;
              }
            }// block Fluid_Fluid

            else if (block == "NeumannIntegration" and elemat1 != Teuchos::null)
            {
              if (discretization.Name() == "fluid")
              {
              /*
                        d (d,[tau - pf o I + gamma rho_f u dyadic u] o [x,1 x x,2]) / d(du)
                               |
                               V
                       2*mu*0.5*(u_i,j+u_j,i)

                       evaluated on FluidField()
               */
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof2)  -=
                    (
                      // d (mu*(u_i,j+u_j,i)) / d u^L_l
                       pfunct(inode)*gradNon(0,nnod)        // d u_i,j / d u^L_l
                    )*fluiddynamicviscosity*fac_*timefac/fluiddensity;

                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+nsd_)  +=
                    (
                        // d (dd , pf o n) / d pf_B
                        // flip sign
                        pfunct(inode)*pfunct(nnod)*unitnormal_(idof2)
                    )*fac_*timefac/fluiddensity;

                for (int idof3=0;idof3<nsd_;idof3++)
                {
                  elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  -=
                      (
                          // d (2*mu*0.5*(u_i,j+u_j,i)) / d u^L_l
                          pfunct(inode)*gradN(0,(nnod*nsd_)+idof2)*unitnormal_(idof3)*fluiddynamicviscosity    // d u_j,i / d u^L_l
                      )*fac_*timefac/fluiddensity;
                }
              } // if dis=fluid
            } // block NeumannIntegration

            else if (block == "NeumannIntegration_Ale")
            {
              for (int idof3=0;idof3<nsd_;idof3++)
              {
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  -=
                                    (
                                        // d (dd, mu*u_i,j o n ) / d d^L_l
                                      + fluiddynamicviscosity*pfunct(inode)*dudxioJinv(idof2,idof3)*dNdxon(nnod)*fac_        // d ui,j / d d^L_l

                                        // d (dd, mu*u_j,i o n ) / d d^L_l
                                      + fluiddynamicviscosity*pfunct(inode)*graduon(0,idof3)*derxy(idof2,nnod)*fac_          // d uj,i / d d^L,l
                                    )*abs(survivor(0,nnod)-1.0)*theta/fluiddensity;      // <- only inner dofs survive
              }
            }// block == "NeumannIntegration_Ale"

            else if (block == "NeumannIntegration_Struct")
            {

              for (int idof3=0;idof3<nsd_;idof3++)
              {
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  -=
                    (
                          // d (dd , - pf o n) / d d^L_l
                      - pfunct(inode)*pressint(0,0)*normalderiv(idof2,(nnod*nsd_)+idof3)*fac                 // d n_j / d d^L_l

                          // d (dd, mu*u_i,j o n ) / d d^L_l
                      + fluiddynamicviscosity*pfunct(inode)*dudxioJinv(idof2,idof3)*dNdxon(nnod)*fac_        // d ui,j / d d^L_l
                      + fluiddynamicviscosity*pfunct(inode)*graduonormalderiv(idof2,(nnod*nsd_)+idof3)*fac   // d n / d d^L_l

                        // d (dd, mu*u_j,i o n ) / d d^L_l
                      + fluiddynamicviscosity*pfunct(inode)*graduon(0,idof3)*derxy(idof2,nnod)*fac_          // d uj,i / d d^L,l
                      + fluiddynamicviscosity*pfunct(inode)*graduTonormalderiv(idof2,(nnod*nsd_)+idof3)*fac  // d n_j / d^L_l
                    )*survivor(nnod)*theta/fluiddensity;      // <- only boundary dofs survive
              }

            }// block == "NeumannIntegration_Struct"

            else if(block == "Structure_Fluid" )
            {
              /*
                                      d (d,[tau - pf o I + gamma rho_f u dyadic u] o [x,1 x x,2]) / d(du)
                                             |
                                             V
                                     2*mu*0.5*(u_i,j+u_j,i)

                                     evaluated on FluidField()
               */
              elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof2)  +=
                  ((
                      // d (mu*(u_i,j+u_j,i)) / d u^L_l

                      pfunct(inode)*gradNon(0,nnod)       // d u_i,j / d u^L_l

                  )*fluiddynamicviscosity*fac_*theta);


              elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+nsd_)  -=
                  ((
                      // d (dd , pf o n) / d pf_B
                      // flip sign

                      pfunct(inode)*pfunct(nnod)*unitnormal_(idof2)

                   )*fac_*theta
                  );

              for(int idof3=0;idof3<nsd_;idof3++)
              {
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  +=
                    (
                        // d (2*mu*0.5*(u_i,j+u_j,i)) / d u^L_l

                        pfunct(inode)*gradN(0,(nnod*nsd_)+idof2)*unitnormal_(idof3)   // d u_j,i / d u^L_l
                    )*fac_*theta*fluiddynamicviscosity ;
              }
            } // block structure_fluid

            else if (block == "Structure_Structure")
            {
              for(int idof3=0;idof3<nsd_;idof3++)
              {
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  +=
                    (
                          // d (dd , - pf o n) / d d^L_l
                      - pfunct(inode)*pressint(0,0)*normalderiv(idof2,(nnod*nsd_)+idof3)*fac                 // d n_j / d d^L_l

                          // d (dd, mu*u_i,j o n ) / d d^L_l
                      + fluiddynamicviscosity*pfunct(inode)*dudxioJinv(idof2,idof3)*dNdxon(nnod)*fac_        // d ui,j / d d^L_l
                      + fluiddynamicviscosity*pfunct(inode)*graduonormalderiv(idof2,(nnod*nsd_)+idof3)*fac   // d n / d d^L_l

                        // d (dd, mu*u_j,i o n ) / d d^L_l
                      + fluiddynamicviscosity*pfunct(inode)*graduon(0,idof3)*derxy(idof2,nnod)*fac_          // d uj,i / d d^L,l
                      + fluiddynamicviscosity*pfunct(inode)*graduTonormalderiv(idof2,(nnod*nsd_)+idof3)*fac  // d n_j / d^L_l
                    )*survivor(nnod)*theta ;      // <- only boundary dofs survive
              }
            } //block structure_structure

            else if (block == "Structure_Ale")
            {
              for(int idof3=0;idof3<nsd_;idof3++)
              {
                elemat1((inode*numdofpernode_)+idof2,(nnod*numdofpernode_)+idof3)  +=
                                    (
                                        // d (dd, mu*u_i,j o n ) / d d^L_l
                                      + fluiddynamicviscosity*pfunct(inode)*dudxioJinv(idof2,idof3)*dNdxon(nnod)*fac_        // d ui,j / d d^L_l

                                        // d (dd, mu*u_j,i o n ) / d d^L_l
                                      + fluiddynamicviscosity*pfunct(inode)*graduon(0,idof3)*derxy(idof2,nnod)*fac_          // d uj,i / d d^L,l

                                    )*abs(survivor(0,nnod)-1.0)*theta;      // <- only inner dofs survive
              }
            }// block structure_ale

            else if(block == "defaultblock" && (block != "fluid" && block != "fluidfluid" && block != "structure" && block != "conti"))
            {
              dserror("no proper block specification available in parameterlist ...");
            } // blocks
          } // idof2
        } // nnod
      } // Loop over parent nodes (inode)

      tangentialvelocity1    .MultiplyTN(velint_   ,tangential1);
      tangentialvelocity2    .MultiplyTN(velint_   ,tangential2);
      tangentialgridvelocity1.MultiplyTN(gridvelint,tangential1);
      tangentialgridvelocity2.MultiplyTN(gridvelint,tangential2);


      ////////////////////////////////////////////////////////////////////////////
      //////////////////////////      Loop over Nodes       //////////////////////
      ////////////////////////////////////////////////////////////////////////////
      for(int inode = 0; inode < nenparent; inode++)
      {
        double normal_u_minus_vs = 0.0;
        LINALG::Matrix<1,nsd_> u_minus_vs (true);

        for(int idof=0;idof<nsd_;idof++)
        {
          normal_u_minus_vs += unitnormal_(idof) * (velint_(idof) - gridvelint(idof));
          u_minus_vs(idof)   = velint_(idof) - gridvelint(idof);
        }

        LINALG::Matrix<1,nenparent*nsd_> u_minus_vs_normalderiv (true);
        u_minus_vs_normalderiv.Multiply(u_minus_vs,normalderiv);

        ////////////////////////////////////////////////////////////////////////////
        ////////////////////////            Fill RHS           /////////////////////
        ////////////////////////////////////////////////////////////////////////////

        if(block == "conti")
        {
          /*
            Evaluated on FluidField() wears (+) in residual; multiplied by (-1) for RHS; switch sign because of opposite normal -> (+)
           */
          //double rhsfacpre = DRT::ELEMENTS::FluidEleParameter::Instance(INPAR::FPSI::porofluid)->TimeFacRhsPre();
          elevec1(inode*numdofpernode_+nsd_) += rhsfac * pfunct(inode) * normal_u_minus_vs;

        } // block conti

        else if(block == "structure")
        {
          /*
                    (2)  N * (tau - pf I) o n   << from last iteration at time n+1

                    evaluated on FluidField(); unitnormal_ opposite to strucutral unitnormal -> application of nanson's formula yields structural normal -> * (-1)
           */
          for(int idof2=0;idof2<nsd_;idof2++)
          {
            elevec1(inode*numdofpernode_+idof2) -=     (  theta *pfunct(inode)*(fluiddynamicviscosity*(graduon(idof2)+graduTon(idof2))     - pressint(0,0)   * unitnormal_(idof2))
                                                  +  (1.0-theta)*pfunct(inode)*(fluiddynamicviscosity*(graduon_n(idof2)+graduTon_n(idof2)) - pressint_n(0,0) * unitnormal_n_(idof2))
                                                       )*survivor(inode)*fac_;
          }
        } // block structure

        else if(block == "fluid")
        {
          /*
                  evaluated on PoroFluidField()

                  (3+4) - N*n * 1/rhof * (pf) + N*t*tangentialfac*[u- (vs + phi(vf-vs))]ot  << from last iteration at time n+1
           */
          for(int idof2=0;idof2<nsd_;idof2++)
          {
            elevec1(inode*numdofpernode_+idof2) +=(+(pfunct(inode)*unitnormal_(idof2)*pressint(0,0)/fluiddensity) // pressure part
                                                   +((pfunct(inode)*tangential1(idof2)*(tangentialgridvelocity1(0,0)+porosityint*(tangentialvelocity1(0,0)-tangentialgridvelocity1(0,0)))) // Beavers-Joseph
                                                   + (pfunct(inode)*tangential2(idof2)*(tangentialgridvelocity2(0,0)+porosityint*(tangentialvelocity2(0,0)-tangentialgridvelocity2(0,0))))
                                                  )*tangentialfac)*rhsfac*survivor(inode);
          }
        } // block fluid

        else if (block == "fluidfluid")
        {
          /*
                    (4)  N*t*tangentialfac*[u]ot  << from last iteration at time n+1
           */
          for (int idof2=0;idof2<nsd_;idof2++)
          {
            elevec1(inode*numdofpernode_+idof2)-= (  pfunct(inode)*tangential1(idof2)*tangentialvelocity1(0,0)
                                                   + pfunct(inode)*tangential2(idof2)*tangentialvelocity2(0,0)
                                                  )*tangentialfac*rhsfac*survivor(inode);
          }
        } // block fluidfluid

        else if (block == "NeumannIntegration")
        {
          if (discretization.Name() != "fluid")
          {
            dserror("Tried to call NeumannIntegration on a discretization other than 'fluid'. \n"
                    "You think that's funny, hu ?? Roundhouse-Kick !!!");
          }

          for (int idof2=0;idof2<nsd_;idof2++)
          {
            elevec1(inode*numdofpernode_+idof2)+= ((- pfunct(inode)*pressint(0,0)*unitnormal_(idof2)*rhsfac
                                                    + pfunct(inode)*fluiddynamicviscosity*(graduon(idof2)+graduTon(idof2))*rhsfac)/fluiddensity
                                                  )*survivor(inode);
          } // block NeumannIntegration

        } // NeumannIntegration
      } // Loop over interface nodes (inode)
  } // Loop over integration points
  return;
} // FPSI Coupling Terms
