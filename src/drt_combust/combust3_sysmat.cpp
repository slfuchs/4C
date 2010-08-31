/*----------------------------------------------------------------------*/
/*!
\file combust3_sysmat.cpp

\brief call system matrix formulation
       premixed combustion problem / two-phase flow problems

<pre>
Maintainer: Florian Henke
            henke@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15265
</pre>
*/
/*----------------------------------------------------------------------*/

#ifdef D_FLUID3
#ifdef CCADISCRET

#include <Teuchos_TimeMonitor.hpp>

#include "combust3_sysmat.H"
#include "combust3_sysmat_premixed_nitsche.H"
#include "combust3_sysmat_premixed_stress.H"
#include "combust3_sysmat_twophaseflow.H"
#include "combust3_local_assembler.H"
#include "combust3_utils.H"
#include "combust3_interpolation.H"
#include "combust_defines.H"
#include "../drt_lib/drt_element.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_fluid/time_integration_element.H"
#include "../drt_f3/xfluid3_utils.H"
#include "../drt_f3/fluid3_stabilization.H"
#include "../drt_fem_general/drt_utils_gder2.H"
#include "../drt_fem_general/drt_utils_shapefunctions_service.H"
#include "../drt_xfem/enrichment.H"
#include "../drt_xfem/enrichment_utils.H"
#include "../drt_xfem/xfem_element_utils.H"
#include "../drt_geometry/integrationcell_coordtrafo.H"
#include "../drt_mat/matlist.H"
#include "../drt_mat/newtonianfluid.H"


using namespace XFEM::PHYSICS;


//! fill a number of (local) element arrays with unknown values from the (global) unknown vector given by the discretization
template <DRT::Element::DiscretizationType DISTYPE,
          XFEM::AssemblyType ASSTYPE,
          class M1, class V1, class M2, class V2, class V3>
void fillElementUnknownsArrays(
    const XFEM::ElementDofManager& dofman,
    const DRT::ELEMENTS::Combust3::MyState& mystate,
    M1& evelnp,
    M1& eveln,
    M1& evelnm,
    M1& eaccn,
    V1& eprenp,
    V2& ephi,
    M2& etau,
    V3& ediscpres
)
{
  const size_t numnode = DRT::UTILS::DisTypeToNumNodePerEle<DISTYPE>::numNodePerElement;

  // number of parameters for each field (assumed to be equal for each velocity component and the pressure)
  //const int numparamvelx = getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Velx, numnode);
  const size_t numparamvelx = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Velx);
  const size_t numparamvely = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Vely);
  const size_t numparamvelz = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Velz);
  const size_t numparampres = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Pres);
  dsassert((numparamvelx == numparamvely) and (numparamvelx == numparamvelz) and (numparamvelx == numparampres), "assumption violation");
  const size_t shpVecSize       = COMBUST::SizeFac<ASSTYPE>::fac*numnode;
  if (numparamvelx > shpVecSize)
  {
    dserror("increase SizeFac for nodal unknowns");
  }

  const std::vector<int>& velxdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Velx>());
  const std::vector<int>& velydof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Vely>());
  const std::vector<int>& velzdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Velz>());
  const std::vector<int>& presdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Pres>());

  for (size_t iparam=0; iparam<numparamvelx; ++iparam)
  {
    evelnp(0,iparam) = mystate.velnp_[velxdof[iparam]];
    if (mystate.instationary_)
    {
      eveln( 0,iparam) = mystate.veln_[ velxdof[iparam]];
      evelnm(0,iparam) = mystate.velnm_[velxdof[iparam]];
      eaccn( 0,iparam) = mystate.accn_[ velxdof[iparam]];
    }
  }
  for (size_t iparam=0; iparam<numparamvely; ++iparam)
  {
    evelnp(1,iparam) = mystate.velnp_[velydof[iparam]];
    if (mystate.instationary_)
    {
      eveln( 1,iparam) = mystate.veln_[ velydof[iparam]];
      evelnm(1,iparam) = mystate.velnm_[velydof[iparam]];
      eaccn( 1,iparam) = mystate.accn_[ velydof[iparam]];
    }
  }
  for (size_t iparam=0; iparam<numparamvelz; ++iparam)
  {
    evelnp(2,iparam) = mystate.velnp_[velzdof[iparam]];
    if (mystate.instationary_)
    {
      eveln( 2,iparam) = mystate.veln_[ velzdof[iparam]];
      evelnm(2,iparam) = mystate.velnm_[velzdof[iparam]];
      eaccn( 2,iparam) = mystate.accn_[ velzdof[iparam]];
    }
  }
  for (size_t iparam=0; iparam<numparampres; ++iparam)
    eprenp(iparam) = mystate.velnp_[presdof[iparam]];

  const bool tauele_unknowns_present = (XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Tauxx, 0) > 0);
  if (tauele_unknowns_present)
  {
    // put one here to create arrays of size 1, since they are not needed anyway
    // in the xfem assembly, the numparam is determined by the dofmanager
    const size_t numparamtauxx = XFEM::NumParam<1,ASSTYPE>::get(dofman, XFEM::PHYSICS::Tauxx);
    const size_t numparamtauyy = XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Tauyy, 1);
    const size_t numparamtauzz = XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Tauzz, 1);
    const size_t numparamtauxy = XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Tauxy, 1);
    const size_t numparamtauxz = XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Tauxz, 1);
    const size_t numparamtauyz = XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Tauyz, 1);
    const DRT::Element::DiscretizationType stressdistype = COMBUST::StressInterpolation3D<DISTYPE>::distype;
    const size_t shpVecSizeStress = COMBUST::SizeFac<ASSTYPE>::fac*DRT::UTILS::DisTypeToNumNodePerEle<stressdistype>::numNodePerElement;
    if (numparamtauxx > shpVecSizeStress)
    {
      dserror("increase SizeFac for stress unknowns");
    }
    const std::vector<int>& tauxxdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Tauxx>());
    const std::vector<int>& tauyydof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Tauyy>());
    const std::vector<int>& tauzzdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Tauzz>());
    const std::vector<int>& tauxydof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Tauxy>());
    const std::vector<int>& tauxzdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Tauxz>());
    const std::vector<int>& tauyzdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Tauyz>());
    for (size_t iparam=0; iparam<numparamtauxx; ++iparam)   etau(0,iparam) = mystate.velnp_[tauxxdof[iparam]];
    for (size_t iparam=0; iparam<numparamtauyy; ++iparam)   etau(1,iparam) = mystate.velnp_[tauyydof[iparam]];
    for (size_t iparam=0; iparam<numparamtauzz; ++iparam)   etau(2,iparam) = mystate.velnp_[tauzzdof[iparam]];
    for (size_t iparam=0; iparam<numparamtauxy; ++iparam)   etau(3,iparam) = mystate.velnp_[tauxydof[iparam]];
    for (size_t iparam=0; iparam<numparamtauxz; ++iparam)   etau(4,iparam) = mystate.velnp_[tauxzdof[iparam]];
    for (size_t iparam=0; iparam<numparamtauyz; ++iparam)   etau(5,iparam) = mystate.velnp_[tauyzdof[iparam]];
  }
  const bool discpres_unknowns_present = (XFEM::getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::DiscPres, 0) > 0);
  if (discpres_unknowns_present)
  {
    const size_t numparamdiscpres = XFEM::NumParam<1,ASSTYPE>::get(dofman, XFEM::PHYSICS::DiscPres);
    const DRT::Element::DiscretizationType discpresdistype = COMBUST::DiscPressureInterpolation3D<DISTYPE>::distype;
    const size_t shpVecSizeDiscPres = COMBUST::SizeFac<ASSTYPE>::fac*DRT::UTILS::DisTypeToNumNodePerEle<discpresdistype>::numNodePerElement;
    if (numparamdiscpres > shpVecSizeDiscPres)
    {
      dserror("increase SizeFac for stress unknowns");
    }
    const vector<int>& discpresdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::DiscPres>());
    for (std::size_t iparam=0; iparam<numparamdiscpres; ++iparam)   ediscpres(iparam) = mystate.velnp_[discpresdof[iparam]];
  }

  // copy element phi vector from std::vector (mystate) to LINALG::Matrix (ephi)
  // TODO: this is inefficient, but it is nice to have only fixed size matrices afterwards!
  for (size_t iparam=0; iparam<numnode; ++iparam)
    ephi(iparam) = mystate.phinp_[iparam];
}


//! fill a number of (local) element arrays
template <DRT::Element::DiscretizationType DISTYPE,
          class M>
void fillElementGradPhi(
    const DRT::ELEMENTS::Combust3::MyState& mystate,
    M& egradphi)
{
  const size_t numnode = DRT::UTILS::DisTypeToNumNodePerEle<DISTYPE>::numNodePerElement;

  unsigned ipos;
  for (size_t iparam=0; iparam<numnode; ++iparam)
  {
    ipos = iparam*3;
    egradphi(0, iparam) = mystate.gradphinp_[ipos  ];
    egradphi(1, iparam) = mystate.gradphinp_[ipos+1];
    egradphi(2, iparam) = mystate.gradphinp_[ipos+2];
  }
}


/*------------------------------------------------------------------------------------------------*
 | get material parameters (constant within the domain integration cell)              henke 06/10 |
 *------------------------------------------------------------------------------------------------*/
void COMBUST::GetMaterialParams(
    Teuchos::RCP<const MAT::Material> material, // pointer to material (list)
    const bool indomplus, // boolean indicating side of the interface
    double&    dens,      // density
    double&    dynvisc    // dynamic viscosity
)
{
  //----------------------
  // get the material type
  //----------------------
#ifdef DEBUG
  // check if we really got a list of materials
  dsassert(material->MaterialType() == INPAR::MAT::m_matlist, "Material law is not of type m_matlist");
#endif
  // get material list for this element
  const MAT::MatList* matlist = static_cast<const MAT::MatList*>(material.get());
  // set default id in list of materials
  int matid = -1;
  // check on which side of the interface the cell is located
  if(indomplus) // cell belongs to burnt domain
  {
    matid = matlist->MatID(0); // burnt material (first material in material list)
  }
  else // cell belongs to unburnt domain
  {
    matid = matlist->MatID(1); // unburnt material (second material in material list)
  }
  // get material from list of materials
  Teuchos::RCP<const MAT::Material> matptr = matlist->MaterialById(matid);
  INPAR::MAT::MaterialType mattype = matptr->MaterialType();

  // choose from different materials
  switch(mattype)
  {
  //--------------------------------------------------------
  // Newtonian fluid for incompressible flow (standard case)
  //--------------------------------------------------------
  case INPAR::MAT::m_fluid:
  {
    const MAT::NewtonianFluid* mat = static_cast<const MAT::NewtonianFluid*>(matptr.get());
    // get the dynamic viscosity \nu
    dynvisc = mat->Viscosity();
    // get the density \rho^{n+1}
    dens = mat->Density();
    break;
  }
  //------------------------------------------------
  // different types of materials (to be added here)
  //------------------------------------------------
  default:
    dserror("material type not supported");
  }

  // security check
  if (dens < 0 or dynvisc < 0)
    dserror("material parameters could not be determined");
  return;
}


/*------------------------------------------------------------------------------------------------*
 | get material parameters for both domains                                           henke 08/10 |
 *------------------------------------------------------------------------------------------------*/
void COMBUST::GetMaterialParams(
    Teuchos::RCP<const MAT::Material> material, // pointer to material (list)
    double&    dens_plus,    // density in "plus domain"
    double&    dynvisc_plus, // dynamic viscosity in "plus domain"
    double&    dens_minus,   // density in "minus domain"
    double&    dynvisc_minus // dynamic viscosity in "minus domain"
)
{
  //----------------------
  // get the material type
  //----------------------
#ifdef DEBUG
  // check if we really got a list of materials
  dsassert(material->MaterialType() == INPAR::MAT::m_matlist, "Material law is not of type m_matlist");
#endif
  // get material list for this element
  const MAT::MatList* matlist = static_cast<const MAT::MatList*>(material.get());
  // set default id in list of materials
  int matid = -1;

  // get material for both sides of the interface ("plus" and "minus" domain)
  for (int matcount=0;matcount<2;matcount++)
  {
    // get ID of material
    // matcount==0: material in burnt domain   (first material in material list)
    // matcount==1: material in unburnt domain (second material in material list)
    matid = matlist->MatID(matcount);

    // get material from list of materials
    Teuchos::RCP<const MAT::Material> matptr = matlist->MaterialById(matid);
    INPAR::MAT::MaterialType mattype = matptr->MaterialType();

    // choose from different materials
    switch(mattype)
    {
    //--------------------------------------------------------
    // Newtonian fluid for incompressible flow (standard case)
    //--------------------------------------------------------
    case INPAR::MAT::m_fluid:
    {
      const MAT::NewtonianFluid* mat = static_cast<const MAT::NewtonianFluid*>(matptr.get());
      //--------------
      // plus material
      //--------------
      if (matcount==0)
      {
        // get the dynamic viscosity \nu
        dynvisc_plus = mat->Viscosity();
        // get the density \rho^{n+1}
        dens_plus = mat->Density();
      }
      //--------------
      // minus material
      //--------------
      if (matcount==1)
      {
        // get the dynamic viscosity \nu
        dynvisc_minus = mat->Viscosity();
        // get the density \rho^{n+1}
        dens_minus = mat->Density();
      }
      break;
    }
    //------------------------------------------------
    // different types of materials (to be added here)
    //------------------------------------------------
    default:
      dserror("material type not supported");
    }
  }

  // security check
  if ((dens_plus < 0  or dynvisc_plus < 0) and
      (dens_minus < 0 or dynvisc_minus < 0))
    dserror("material parameters could not be determined");

  return;
}


/*!
  Calculate matrix and rhs for stationary problem formulation
  */
template <DRT::Element::DiscretizationType DISTYPE,
          XFEM::AssemblyType ASSTYPE>
void Sysmat(
    const DRT::ELEMENTS::Combust3*    ele,            ///< the element those matrix is calculated
    const Teuchos::RCP<COMBUST::InterfaceHandleCombust>  ih, ///< connection to the interface handler
    const XFEM::ElementDofManager&    dofman,         ///< dofmanager of the current element
    const DRT::ELEMENTS::Combust3::MyState&  mystate, ///< element state variables
    Epetra_SerialDenseMatrix&         estif,          ///< element matrix to calculate
    Epetra_SerialDenseVector&         eforce,         ///< element rhs to calculate
    Teuchos::RCP<const MAT::Material> material,       ///< fluid material
    const INPAR::FLUID::TimeIntegrationScheme timealgo,       ///< time discretization type
    const double                      dt,             ///< delta t (time step size)
    const double                      theta,          ///< factor for one step theta scheme
    const bool                        newton,         ///< full Newton or fixed-point-like
    const bool                        pstab,          ///< flag for stabilisation
    const bool                        supg,           ///< flag for stabilisation
    const bool                        cstab,          ///< flag for stabilisation
    const INPAR::FLUID::TauType       tautype,        ///< stabilization parameter definition
    const bool                        instationary,   ///< switch between stationary and instationary formulation
    const INPAR::COMBUST::CombustionType combusttype, ///< switch for type of combusiton problem
    const double                      flamespeed,     ///<
    const double                      nitschevel,     ///<
    const double                      nitschepres,    ///<
    const INPAR::COMBUST::SurfaceTensionApprox surftensapprox, ///<
    const double                      surftenscoeff,   ///<
    const bool                        connected_interface,
    const INPAR::COMBUST::VelocityJumpType veljumptype,
    const INPAR::COMBUST::NormalTensionJumpType normaltensionjumptype
)
{
  // initialize element stiffness matrix and force vector
  estif.Scale(0.0);
  eforce.Scale(0.0);

  const int NUMDOF = 4;

  LocalAssembler<DISTYPE, ASSTYPE, NUMDOF> assembler(dofman, estif, eforce);

  // split velocity and pressure (and stress)
  const int shpVecSize       = COMBUST::SizeFac<ASSTYPE>::fac*DRT::UTILS::DisTypeToNumNodePerEle<DISTYPE>::numNodePerElement;
  const size_t numnode = DRT::UTILS::DisTypeToNumNodePerEle<DISTYPE>::numNodePerElement;
  const DRT::Element::DiscretizationType stressdistype = COMBUST::StressInterpolation3D<DISTYPE>::distype;
  const DRT::Element::DiscretizationType discpresdistype = COMBUST::DiscPressureInterpolation3D<DISTYPE>::distype;
  const int shpVecSizeStress = COMBUST::SizeFac<ASSTYPE>::fac*DRT::UTILS::DisTypeToNumNodePerEle<stressdistype>::numNodePerElement;
  const int shpVecSizeDiscPres = COMBUST::SizeFac<ASSTYPE>::fac*DRT::UTILS::DisTypeToNumNodePerEle<discpresdistype>::numNodePerElement;
  LINALG::Matrix<shpVecSize,1> eprenp;
  LINALG::Matrix<3,shpVecSize> evelnp;
  LINALG::Matrix<3,shpVecSize> eveln;
  LINALG::Matrix<3,shpVecSize> evelnm;
  LINALG::Matrix<3,shpVecSize> eaccn;
  LINALG::Matrix<numnode,1> ephi;
  LINALG::Matrix<6,shpVecSizeStress> etau;
  LINALG::Matrix<shpVecSizeDiscPres,1> ediscpres;

  fillElementUnknownsArrays<DISTYPE,ASSTYPE>(
      dofman, mystate, evelnp, eveln, evelnm, eaccn, eprenp, ephi, etau, ediscpres);

  switch(combusttype)
  {
  case INPAR::COMBUST::combusttype_premixedcombustion:
  {
#ifdef COMBUST_NITSCHE
    COMBUST::SysmatDomainNitsche<DISTYPE,ASSTYPE,NUMDOF>(
        ele, ih, dofman, evelnp, eveln, evelnm, eaccn, eprenp, ephi,
        material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary, assembler);
#endif
#ifdef COMBUST_STRESS_BASED
    COMBUST::SysmatDomainStress<DISTYPE,ASSTYPE,NUMDOF>(
        ele, ih, dofman, evelnp, eveln, evelnm, eaccn, eprenp, ephi, etau, ediscpres,
        material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary, assembler);
#endif

#ifndef COMBUST_DECOUPLEDXFEM
#ifdef COMBUST_NITSCHE
    // boundary integrals are only added for intersected elements (fully enriched elements)
    if (ele->Intersected() == true)
    {
      COMBUST::SysmatBoundaryNitsche<DISTYPE,ASSTYPE,NUMDOF>(
          ele, ih, dofman, evelnp, eprenp, ephi, material, timealgo, dt, theta, assembler,
          flamespeed,nitschevel,nitschepres,surftensapprox,surftenscoeff);
    }
#endif
#ifdef COMBUST_STRESS_BASED
    // boundary integrals are only added for intersected elements (fully enriched elements)
    if (ele->Intersected() == true)
    {
      COMBUST::SysmatBoundaryStress<DISTYPE,ASSTYPE,NUMDOF>(
          ele, ih, dofman, evelnp, eprenp, ephi, etau, ediscpres, material, timealgo, dt, theta, assembler,
          flamespeed);
    }
#endif
#endif
  }
  break;
  case INPAR::COMBUST::combusttype_twophaseflow:
  {
    COMBUST::SysmatTwoPhaseFlow<DISTYPE,ASSTYPE,NUMDOF>(
        ele, ih, dofman, evelnp, eveln, evelnm, eaccn, eprenp, ephi, etau,
        material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary, assembler);
  }
  break;
  case INPAR::COMBUST::combusttype_twophaseflow_surf:
  {
//      double ele_meas_plus = 0.0;  // we need measure of element in plus domain and minus domain
//      double ele_meas_minus = 0.0; // for different averages <> and {}
//
//      TPF::SysmatTwoPhaseFlow_Surf_Domain<DISTYPE,ASSTYPE,NUMDOF>(
//        ele, ih, dofman, evelnp, eveln, evelnm, eaccn, eprenp, ephi, etau,
//        material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary, assembler, ele_meas_plus, ele_meas_minus);
//
//      // boundary integrals are added for intersected and touched elements (fully or partially enriched elements)
//      if (ele->Intersected() == true || ele->Touched_Plus() == true )
//      {
//        // get smoothed gradient of phi for surface tension applications
//        LINALG::Matrix<3,numnode> egradphi;
//        fillElementGradPhi<DISTYPE>(mystate, egradphi);
//
//        TPF::SysmatTwoPhaseFlow_Surf_Boundary<DISTYPE,ASSTYPE,NUMDOF>(
//            ele, ih, dofman, evelnp, eprenp, ephi, egradphi, etau, material, timealgo, dt, theta, assembler,
//            flamespeed,nitschevel,nitschepres, ele_meas_plus, ele_meas_minus,surftensapprox,surftenscoeff,connected_interface, veljumptype, normaltensionjumptype);
//      }
  }
  break;
  case INPAR::COMBUST::combusttype_twophaseflowjump:
  {
#ifdef TWOPHASEFLOW_NITSCHE
    // schott Jun 16, 2010
    double ele_meas_plus = 0.0;  // we need measure of element in plus domain and minus domain
    double ele_meas_minus = 0.0; // for different averages <> and {}

    COMBUST::SysmatDomainNitsche<DISTYPE,ASSTYPE,NUMDOF>(
        ele, ih, dofman, evelnp, eveln, evelnm, eaccn, eprenp, ephi,
        material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary, assembler,
        ele_meas_plus, ele_meas_minus);

    // boundary integrals are added for intersected and touched elements (fully or partially enriched elements)
    // TODO Das, oder das naechste
    if (ele->Intersected() == true || ele->Touched_Plus() == true )
    {
      // get smoothed gradient of phi for surface tension applications
      LINALG::Matrix<3,numnode> egradphi;
      fillElementGradPhi<DISTYPE>(mystate, egradphi);

      COMBUST::SysmatBoundaryNitsche<DISTYPE,ASSTYPE,NUMDOF>(
          ele, ih, dofman, evelnp, eprenp, ephi, egradphi, material, timealgo, dt, theta, assembler,
          flamespeed,nitschevel,nitschepres, ele_meas_plus, ele_meas_minus,surftensapprox,surftenscoeff,connected_interface, veljumptype, normaltensionjumptype);
    }
#endif

#ifdef TWOPHASEFLOW_NITSCHE
    // schott Jun 16, 2010

    COMBUST::SysmatDomainNitsche<DISTYPE,ASSTYPE,NUMDOF>(
        ele, ih, dofman, evelnp, eveln, evelnm, eaccn, eprenp, ephi,
        material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary, assembler,
        ele_meas_plus, ele_meas_minus);
    // boundary integrals are only added for intersected elements (fully enriched elements)
    // TODO Das, oder das obere
    if (ele->Intersected() == true)
    {
      // get smoothed gradient of phi for surface tension applications
      LINALG::Matrix<3,numnode> egradphi;
      fillElementGradPhi<DISTYPE>(mystate, egradphi);

      COMBUST::SysmatBoundaryNitsche<DISTYPE,ASSTYPE,NUMDOF>(
          ele, ih, dofman, evelnp, eprenp, ephi, egradphi, material, timealgo, dt, theta, assembler,
          flamespeed, nitschevel, nitschepres, ele_meas_plus, ele_meas_minus,
          surftensapprox, surftenscoeff, connected_interface, veljumptype, normaltensionjumptype);
    }
#endif
  }
  break;
  default:
    dserror("unknown type of combustion problem");
  }

  //----------------------------------
  // symmetry check for element matrix
  // TODO: remove symmetry check
  //----------------------------------
//  //cout << endl << "stiffness matrix of element: " << ele->Id() << " columns " << estif.N() << " rows " << estif.M() << endl << endl;
//  int counter = 0;
//  for (int row=0; row<estif.M(); ++row)
//  {
//    for (int col=0; col<estif.N(); ++col)
//    {
//      //    cout << estif(row,col);
//      double diff = estif(row,col)-estif(col,row);
//      if (!((diff>-1.0E-9) and (diff<+1.0E-9)))
//      {
//        //cout << counter << " difference of entry " << estif(row,col) << " is not 0.0, but " << diff << endl;
//        //cout << "stiffness matrix entry " << estif(row,col) << " transpose " << estif(col,row) << endl;
//      }
//      counter++;
//    }
//  }
//  //cout << "counter " << counter << endl;
//  //dserror("STOP after first element matrix");

}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void COMBUST::callSysmat(
    const XFEM::AssemblyType             assembly_type,
    const DRT::ELEMENTS::Combust3*       ele,
    const Teuchos::RCP<COMBUST::InterfaceHandleCombust>&  ih,
    const XFEM::ElementDofManager&       eleDofManager,
    const DRT::ELEMENTS::Combust3::MyState&  mystate,   ///< element state variables
    Epetra_SerialDenseMatrix&            estif,
    Epetra_SerialDenseVector&            eforce,
    Teuchos::RCP<const MAT::Material>    material,
    const INPAR::FLUID::TimeIntegrationScheme timealgo, ///< time discretization type
    const double                         dt,            ///< delta t (time step size)
    const double                         theta,         ///< factor for one step theta scheme
    const bool                           newton,
    const bool                           pstab,
    const bool                           supg,
    const bool                           cstab,
    const INPAR::FLUID::TauType          tautype,       ///< stabilization parameter definition
    const bool                           instationary,
    const INPAR::COMBUST::CombustionType combusttype,
    const double                         flamespeed,
    const double                         nitschevel,
    const double                         nitschepres,
    const INPAR::COMBUST::SurfaceTensionApprox  surftensapprox,
    const double                                surftenscoeff,
    const bool                                  connected_interface,
    const INPAR::COMBUST::VelocityJumpType      veljumptype,
    const INPAR::COMBUST::NormalTensionJumpType normaltensionjumptype)
{
  if (assembly_type == XFEM::standard_assembly)
  {
    switch (ele->Shape())
    {
    case DRT::Element::hex8:
      Sysmat<DRT::Element::hex8,XFEM::standard_assembly>(
          ele, ih, eleDofManager, mystate, estif, eforce,
          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
          combusttype,flamespeed,nitschevel,nitschepres,surftensapprox,surftenscoeff,
          connected_interface,veljumptype,normaltensionjumptype);
    break;
//    case DRT::Element::hex20:
//      Sysmat<DRT::Element::hex20,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
//    case DRT::Element::hex27:
//      Sysmat<DRT::Element::hex27,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
//    case DRT::Element::tet4:
//      Sysmat<DRT::Element::tet4,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
//    case DRT::Element::tet10:
//      Sysmat<DRT::Element::tet10,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
    default:
      dserror("standard_assembly Sysmat not templated yet");
    };
  }
  else
  {
    switch (ele->Shape())
    {
    case DRT::Element::hex8:
      Sysmat<DRT::Element::hex8,XFEM::xfem_assembly>(
          ele, ih, eleDofManager, mystate, estif, eforce,
          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff,
          connected_interface,veljumptype,normaltensionjumptype);
    break;
//    case DRT::Element::hex20:
//      Sysmat<DRT::Element::hex20,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
//    case DRT::Element::hex27:
//      Sysmat<DRT::Element::hex27,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
//    case DRT::Element::tet4:
//      Sysmat<DRT::Element::tet4,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
//    case DRT::Element::tet10:
//      Sysmat<DRT::Element::tet10,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres,surftensapprox,surftenscoeff);
//    break;
    default:
      dserror("xfem_assembly Sysmat not templated yet");
    };
  }
}


/*!
  Calculate Nitsche errors for Nitsche problem formulation
                                                                   schott Jun 15, 2010
  */
template <DRT::Element::DiscretizationType DISTYPE,
          XFEM::AssemblyType ASSTYPE>
void NitscheErrors(
    ParameterList&                         eleparams,
    const INPAR::COMBUST::NitscheError&  NitscheErrorType,
    const DRT::ELEMENTS::Combust3*         ele,            ///< the element those matrix is calculated
    const Teuchos::RCP<COMBUST::InterfaceHandleCombust>  ih,   ///< connection to the interface handler
    const XFEM::ElementDofManager&                       dofman,         ///< dofmanager of the current element
    const DRT::ELEMENTS::Combust3::MyState&              mystate, ///< element state variables
    Teuchos::RCP<const MAT::Material>                    material       ///< fluid material
)
{
//  const int NUMDOF = 4;

  // split velocity and pressure (and stress)
  const int shpVecSize       = COMBUST::SizeFac<ASSTYPE>::fac*DRT::UTILS::DisTypeToNumNodePerEle<DISTYPE>::numNodePerElement;
  const size_t numnode = DRT::UTILS::DisTypeToNumNodePerEle<DISTYPE>::numNodePerElement;

  LINALG::Matrix<shpVecSize,1> eprenp;
  LINALG::Matrix<3,shpVecSize> evelnp;
  LINALG::Matrix<numnode,1> ephi;


  //==============================================================================================================
  // fill velocity and pressure Arrays

  //  fillElementUnknownsArrays<DISTYPE,ASSTYPE>(dofman, mystate, evelnp, eveln, evelnm, eaccn, eprenp,
  //      ephi, etau, ediscpres);

  // number of parameters for each field (assumed to be equal for each velocity component and the pressure)
  //const int numparamvelx = getNumParam<ASSTYPE>(dofman, XFEM::PHYSICS::Velx, numnode);
  const size_t numparamvelx = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Velx);
  const size_t numparamvely = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Vely);
  const size_t numparamvelz = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Velz);
  const size_t numparampres = XFEM::NumParam<numnode,ASSTYPE>::get(dofman, XFEM::PHYSICS::Pres);
  dsassert((numparamvelx == numparamvely) and (numparamvelx == numparamvelz) and (numparamvelx == numparampres), "assumption violation");

  if ((int)numparamvelx > shpVecSize)
  {
    dserror("increase SizeFac for nodal unknowns");
  }

  const std::vector<int>& velxdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Velx>());
  const std::vector<int>& velydof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Vely>());
  const std::vector<int>& velzdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Velz>());
  const std::vector<int>& presdof(dofman.LocalDofPosPerField<XFEM::PHYSICS::Pres>());

  for (size_t iparam=0; iparam<numparamvelx; ++iparam)
  {
    evelnp(0,iparam) = mystate.velnp_[velxdof[iparam]];
  }
  for (size_t iparam=0; iparam<numparamvely; ++iparam)
  {
    evelnp(1,iparam) = mystate.velnp_[velydof[iparam]];
  }
  for (size_t iparam=0; iparam<numparamvelz; ++iparam)
  {
    evelnp(2,iparam) = mystate.velnp_[velzdof[iparam]];
  }
  for (size_t iparam=0; iparam<numparampres; ++iparam)
    eprenp(iparam) = mystate.velnp_[presdof[iparam]];
  
  // copy element phi vector from std::vector (mystate) to LINALG::Matrix (ephi)
  // TODO: this is inefficient, but it is nice to have only fixed size matrices afterwards!
  for (size_t iparam=0; iparam<numnode; ++iparam)
    ephi(iparam) = mystate.phinp_[iparam];
//=================================================================================================================
//  double ele_meas_plus = 0.0;	// we need measure of element in plus domain and minus domain
//  double ele_meas_minus = 0.0;	// for different averages <> and {}
//
//	  COMBUST::BuildDomainNitscheErrors<DISTYPE,ASSTYPE,NUMDOF>(
//	      eleparams, NitscheErrorType, ele, ih, dofman, evelnp, eprenp, ephi, material, ele_meas_plus, ele_meas_minus);
//
//	  if (ele->Intersected() == true || ele->Touched_Plus() == true)
//	  {
//		  COMBUST::BuildBoundaryNitscheErrors<DISTYPE,ASSTYPE,NUMDOF>(
//	        eleparams, NitscheErrorType, ele, ih, dofman, evelnp, eprenp, ephi, material, ele_meas_plus, ele_meas_minus);
//	  }
}


/*----------------------------------------------------------------------*
 *----------------------------------------- schott Jun 15, 2010---------*/
void COMBUST::callNitscheErrors(
    ParameterList&                              eleparams,        ///< list of parameters
    const INPAR::COMBUST::NitscheError& NitscheErrorType, ///<
    const XFEM::AssemblyType                    assembly_type,    ///<
    const DRT::ELEMENTS::Combust3*              ele,              ///<
    const Teuchos::RCP<COMBUST::InterfaceHandleCombust>&  ih,     ///<
    const XFEM::ElementDofManager&              eleDofManager,    ///<
    const DRT::ELEMENTS::Combust3::MyState&     mystate,          ///< element state variables
    Teuchos::RCP<const MAT::Material>           material          ///<
)
{
  if (assembly_type == XFEM::standard_assembly)
  {
    switch (ele->Shape())
    {
    case DRT::Element::hex8:
      NitscheErrors<DRT::Element::hex8,XFEM::standard_assembly>(
          eleparams, NitscheErrorType, ele, ih, eleDofManager, mystate, material);
    break;
//    case DRT::Element::hex20:
//      Sysmat<DRT::Element::hex20,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
//    case DRT::Element::hex27:
//      Sysmat<DRT::Element::hex27,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
//    case DRT::Element::tet4:
//      Sysmat<DRT::Element::tet4,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
//    case DRT::Element::tet10:
//      Sysmat<DRT::Element::tet10,XFEM::standard_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
    default:
      dserror("standard_assembly Sysmat not templated yet");
    };
  }
  else
  {
    switch (ele->Shape())
    {
    case DRT::Element::hex8:
      NitscheErrors<DRT::Element::hex8,XFEM::xfem_assembly>(
          eleparams, NitscheErrorType, ele, ih, eleDofManager, mystate, material);
    break;
//    case DRT::Element::hex20:
//      Sysmat<DRT::Element::hex20,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
//    case DRT::Element::hex27:
//      Sysmat<DRT::Element::hex27,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
//    case DRT::Element::tet4:
//      Sysmat<DRT::Element::tet4,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
//    case DRT::Element::tet10:
//      Sysmat<DRT::Element::tet10,XFEM::xfem_assembly>(
//          ele, ih, eleDofManager, mystate, estif, eforce,
//          material, timealgo, dt, theta, newton, pstab, supg, cstab, tautype, instationary,
//          combusttype, flamespeed, nitschevel, nitschepres);
//    break;
    default:
      dserror("xfem_assembly Sysmat not templated yet");
    };
  }
}


#endif
#endif
