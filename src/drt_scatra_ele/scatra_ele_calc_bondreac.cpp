/*----------------------------------------------------------------------*/
/*!
 \file scatra_ele_calc_bondreac.cpp

 \brief main file containing routines for calculation of scatra element with reactive scalars and bond dynamics.

 \level 2

 <pre>
   \maintainer Andreas Rauch
               rauch@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289 - 15240
 </pre>
 *----------------------------------------------------------------------*/


#include "scatra_ele_calc_bondreac.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_element.H"

#include "../drt_mat/matlist_bondreacs.H"
#include "../drt_mat/matlist_reactions.H"
#include "../drt_mat/scatra_mat.H"
#include "../drt_mat/matlist.H"

#include "../drt_fem_general/drt_utils_boundary_integration.H"


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::ScaTraEleCalcBondReac(
    const int numdofpernode,
    const int numscal,
    const std::string& disname)
: DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::ScaTraEleCalc(
    numdofpernode,
    numscal,
    disname),
  DRT::ELEMENTS::ScaTraEleCalcAdvReac<distype,probdim>::ScaTraEleCalcAdvReac(
      numdofpernode,
      numscal,
      disname)
  {
    // give pointer to traction vector to ScatraEleBondReacCalc
    exchange_manager_ = DRT::ImmersedFieldExchangeManager::Instance();
    surface_traction_ = exchange_manager_->GetPointerSurfaceTraction();
  }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype,int probdim>
DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim> *
    DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::Instance(
        const int numdofpernode,
        const int numscal,
        const std::string& disname,
        const ScaTraEleCalcBondReac *delete_me )
    {
  static std::map<std::pair<std::string,int>,ScaTraEleCalcBondReac<distype,probdim>* > instances;

  std::pair<std::string,int> key(disname,numdofpernode);

  if(delete_me == NULL)
  {
    if(instances.find(key) == instances.end())
      instances[key] = new ScaTraEleCalcBondReac<distype,probdim>(numdofpernode,numscal,disname);
  }

  else
  {
    // since we keep several instances around in the general case, we need to
    // find which of the instances to delete with this call. This is done by
    // letting the object to be deleted hand over the 'this' pointer, which is
    // located in the map and deleted
    for( typename std::map<std::pair<std::string,int>,ScaTraEleCalcBondReac<distype,probdim>* >::iterator i=instances.begin(); i!=instances.end(); ++i )
      if ( i->second == delete_me )
      {
        delete i->second;
        instances.erase(i);
        return NULL;
      }
    dserror("Could not locate the desired instance. Internal error.");
  }

  return instances[key];
    }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
  Instance( 0, 0, "", this );
}


/*----------------------------------------------------------------------*
 |  get the material constants  (private)                   rauch 12/16 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::GetMaterialParams(
    const DRT::Element*  ele,       //!< the element we are dealing with
    std::vector<double>& densn,     //!< density at t_(n)
    std::vector<double>& densnp,    //!< density at t_(n+1) or t_(n+alpha_F)
    std::vector<double>& densam,    //!< density at t_(n+alpha_M)
    double&              visc,      //!< fluid viscosity
    const int            iquad      //!< id of current gauss point
)
{
  // get surface traction  and porosity at gauss point
  const double porosity = GetPorosity(ele,iquad);
  const double traction = GetTraction(ele,iquad);


  // get the material
  Teuchos::RCP<MAT::Material> material = ele->Material();

  // We may have some reactive and some non-reactive elements in one discretisation.
  // But since the calculation classes are singleton, we have to reset all reactive stuff in case
  // of non-reactive elements:
  advreac::ReaManager()->Clear(my::numscal_);

  if (material->MaterialType() == INPAR::MAT::m_matlist)
  {
    const Teuchos::RCP<const MAT::MatList> actmat = Teuchos::rcp_dynamic_cast<const MAT::MatList>(material);
    if (actmat->NumMat() != my::numscal_) dserror("Not enough materials in MatList.");

    for (int k = 0;k<my::numscal_;++k)
    {
      int matid = actmat->MatID(k);
      Teuchos::RCP< MAT::Material> singlemat = actmat->MaterialById(matid);

      advreac::Materials(singlemat,k,densn[k],densnp[k],densam[k],visc,iquad);
    }
  }

  else if (material->MaterialType() == INPAR::MAT::m_matlist_reactions)
  {
    const Teuchos::RCP<MAT::MatListReactions> actmat = Teuchos::rcp_dynamic_cast<MAT::MatListReactions>(material);
    if (actmat->NumMat() != my::numscal_) dserror("Not enough materials in MatList.");

    for (int k = 0;k<my::numscal_;++k)
    {
      int matid = actmat->MatID(k);
      Teuchos::RCP< MAT::Material> singlemat = actmat->MaterialById(matid);

      //Note: order is important here!!
      advreac::Materials(singlemat,k,densn[k],densnp[k],densam[k],visc,iquad);

      advreac::SetAdvancedReactionTerms(k,actmat,advreac::GetGpCoord()); //every reaction calculation stuff happens in here!!
    }
  }

  else if (material->MaterialType() == INPAR::MAT::m_matlist_bondreacs)
  {
    const Teuchos::RCP<MAT::MatListBondReacs> actmat = Teuchos::rcp_dynamic_cast<MAT::MatListBondReacs>(material);
    if (actmat->NumMat() != my::numscal_) dserror("Not enough materials in MatList.");

    for (int k = 0;k<my::numscal_;++k)
    {
      int matid = actmat->MatID(k);
      Teuchos::RCP< MAT::Material> singlemat = actmat->MaterialById(matid);

      //Note: order is important here!!
      advreac::Materials(singlemat,k,densn[k],densnp[k],densam[k],visc,iquad);

      SetBondReactionTerms(k,actmat,traction,porosity,advreac::GetGpCoord()); //every reaction calculation stuff happens in here!!
    }
  }

  else
  {
    advreac::Materials(material,0,densn[0],densnp[0],densam[0],visc,iquad);
  }

  return;
} //ScaTraEleCalc::GetMaterialParams


/*-------------------------------------------------------------------------------*
 |  set reac. body force, reaction coefficient and derivatives       rauch 12/16 |
 *-------------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::SetBondReactionTerms(
    const int                                 k,            //!< index of current scalar
    const Teuchos::RCP<MAT::MatListBondReacs> matreaclist,  //!< index of current scalar
    const double                              traction,     //!< traction at current gp
    const double                              porosity,     //!< average receptor-ligand distance
    const double* gpcoord
)
{
  const Teuchos::RCP<ScaTraEleReaManagerAdvReac> remanager = advreac::ReaManager();

  //! scalar values at t_(n+1) or t_(n+alpha_F)
  const std::vector<double>& phinp = my::scatravarmanager_->Phinp();

  //! scalar values at t_(n)
  const std::vector<double>& phin = my::scatravarmanager_->Phin();

  remanager->AddToReaBodyForce( matreaclist->CalcReaBodyForceTerm(k,phinp,phin,traction,porosity,gpcoord) ,k );

  matreaclist->CalcReaBodyForceDerivMatrix(k,remanager->GetReaBodyForceDerivVector(k),phinp,phin,traction,porosity,gpcoord);
}


/*-------------------------------------------------------------------------------*
 |  evaluate single bond traction at gauss point                     rauch 12/16 |
 *-------------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
double DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::GetTraction(
    const DRT::Element*  ele,       //!< the element we are dealing with
    const int            iquad      //!< id of current gauss point
) const
{

  double traction=-1234.0;

  // get global problem
  DRT::Problem* problem = DRT::Problem::Instance();

  // get struct discretization
  Teuchos::RCP<DRT::Discretization> dis = problem->GetDis("cellscatra");

  // get element location vector
  DRT::Element::LocationArray la(dis->NumDofSets());
  ele->LocationVector(*dis,la,false);

  // get structure_lm from second dofset
  // the first dofset is the scatra surface and the second dofset the structure
  const std::vector<int>& struct_lm = la[1].lm_;

  // evaluate traction only for elements which are mapped on surface_traction_ vector.
  // this is done by checking whether all the nodal locations are mapped
  // todo replace with something else! or check whether compatible with multi processors!
  const size_t ldim = struct_lm.size();
  bool ele_is_condition=true;
  for (size_t i=0; i<ldim; ++i)
  {
    const int lid = (*surface_traction_).Map().LID(struct_lm[i]);
    if (lid<0)
      ele_is_condition=false;
  }

  //extract values if element is adhesion surface element
  if (ele_is_condition)
  {
    // extract values to helper variable mytraction
    std::vector<double> mytraction(struct_lm.size());
    DRT::UTILS::ExtractMyValues(*surface_traction_,mytraction,struct_lm);

    // number of nodes and numdofs per node
    const int numNode = ele->NumNode();
    const int struct_numdofpernode = struct_lm.size()/numNode;

    // integration points and weights for boundary (!) gp --> quad4
    const DRT::UTILS::IntPointsAndWeights<2> intpoints (DRT::ELEMENTS::DisTypeToOptGaussRule<DRT::Element::quad4>::rule);

    // coordinates of current integration point in face element coordinate system --> QUAD4
    LINALG::Matrix<2,1> xsi(true);
    xsi(0) = intpoints.IP().qxg[iquad][0];
    xsi(1) = intpoints.IP().qxg[iquad][1];

    // shapefunct and derivates of face element in face element coordinate system
    LINALG::Matrix<4, 1> shapefunct;
    DRT::UTILS::shape_function<DRT::Element::quad4>(xsi,shapefunct);

    // nodal drag vector
    LINALG::Matrix<4,1> drag_nd(true);

    // loop over all scatra element nodes to get surface traction at nodes (quad4)
    for (int node=0; node<numNode; node++)
    {
      drag_nd(node,0) = mytraction[node*struct_numdofpernode];
    }

    // drag at gauss point
    double drag_gp=0;

    // get drag at gp from nodal drag
    for (int node=0; node<numNode; node++)
      drag_gp += shapefunct(node) * drag_nd(node,0);

    // write final traction value
    traction = drag_gp;
  }
  else
  {
    traction=0.0;
  }

  return traction;

}


/*-------------------------------------------------------------------------------*
 |  evaluate single bond traction at gauss point                     rauch 12/16 |
 *-------------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
double DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::GetPorosity(
    const DRT::Element*  ele,       //!< the element we are dealing with
    const int            iquad      //!< id of current gauss point
) const
{
  // todo so far we have a hard coded porosity for experimental testing purposes
  double porosity = 0.8;

  return porosity;

}


/*----------------------------------------------------------------------*
 | extract element based or nodal values                    rauch 12/16 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype,int probdim>
void DRT::ELEMENTS::ScaTraEleCalcBondReac<distype,probdim>::ExtractElementAndNodeValues(
    DRT::Element*                 ele,
    Teuchos::ParameterList&       params,
    DRT::Discretization&          discretization,
    DRT::Element::LocationArray&  la
)
{
  // call abse class version
  DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::ExtractElementAndNodeValues(ele,params,discretization,la);

  // we additionally add phin to our variables
  if (params.get<int>("action") == SCATRA::calc_heteroreac_mat_and_rhs )
  {
    const std::vector<int>&    lm = la[0].lm_;

    // extract additional local values from global vector
    Teuchos::RCP<const Epetra_Vector> phin = discretization.GetState("phin");
    if (phin==Teuchos::null) dserror("Cannot get state vector 'phin'");
    DRT::UTILS::ExtractMyValues<LINALG::Matrix<my::nen_,1> >(*phin,my::ephin_,lm);
  }
}


// template classes

// 1D elements
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::line2,1>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::line2,2>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::line2,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::line3,1>;

// 2D elements
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::tri3,2>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::tri3,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::tri6,2>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::quad4,2>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::quad4,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::quad9,2>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::nurbs9,2>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::hex8,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::hex27,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::tet4,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::tet10,3>;
template class DRT::ELEMENTS::ScaTraEleCalcBondReac<DRT::Element::pyramid5,3>;
