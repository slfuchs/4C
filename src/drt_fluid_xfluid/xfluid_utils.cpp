/*!-----------------------------------------------------------------------------------------------*
\file xfluid_utils.cpp

\brief Basic functions used for xfluid applications

<pre>
Maintainer:  Magnus Winter
             winter@lnm.mw.tum.de
             http://www.lnm.mw.tum.de
             089 - 289-15236
</pre>
*------------------------------------------------------------------------------------------------*/

#include "../drt_lib/drt_element.H"

//Materials supported in XFEM currently
#include "../drt_mat/material.H"
#include "../drt_mat/matlist.H"
#include "../drt_mat/newtonianfluid.H"

#include "xfluid_utils.H"

// -------------------------------------------------------------------
// set master and slave parameters (winter 01/2015)
// -------------------------------------------------------------------
void XFLUID::UTILS::GetVolumeCellMaterial(DRT::Element* actele,
                                 Teuchos::RCP<MAT::Material> & mat,
                                 GEO::CUT::Point::PointPosition position)
{
  Teuchos::RCP<MAT::Material> material = actele->Material();

  if (material->MaterialType() == INPAR::MAT::m_matlist)
  {
    // get material list for this element
    const MAT::MatList* matlist = static_cast<const MAT::MatList*>(material.get());
    int numofmaterials = matlist->NumMat();

    //Error messages
    if(numofmaterials>2)
    {
      dserror("More than two materials is currently not supported.");
    }

    // set default id in list of materials
    if(position==GEO::CUT::Point::outside)
    {
      int matid = -1;
      matid     = matlist->MatID(0);
      mat       = matlist->MaterialById(matid);
    }
    else if(position==GEO::CUT::Point::inside)
    {
      int matid = -1;
      matid     = matlist->MatID(1);
      mat       = matlist->MaterialById(matid);
    }
    else
    {
      dserror("Volume cell is either undecided or on surface. That can't be good....");
    }
  }
  else
  {
    mat = material;
  }

  //std::cout << "mat->MaterialType(): " << mat->MaterialType() << std::endl;
  return;
}

/*----------------------------------------------------------------------*
 | Checks if Materials in parent and neighbor element are identical     |
 |                                                         winter 01/15 |
 *----------------------------------------------------------------------*/
void XFLUID::UTILS::SafetyCheckMaterials(Teuchos::RCP<MAT::Material> &          pmat,
                                Teuchos::RCP<MAT::Material> &          nmat)
{

  //------------------------------ see whether materials in patch are equal

  if(pmat->MaterialType() != nmat->MaterialType())
    dserror(" not the same material for master and slave parent element");

  if(pmat->MaterialType() == INPAR::MAT::m_matlist)
    dserror("A matlist has been found in edge based stabilization! If you are running XTPF, check calls as this should NOT happen!!!");

  if( pmat->MaterialType() != INPAR::MAT::m_carreauyasuda
   && pmat->MaterialType() != INPAR::MAT::m_modpowerlaw
   && pmat->MaterialType() != INPAR::MAT::m_herschelbulkley
   && pmat->MaterialType() != INPAR::MAT::m_fluid)
    dserror("Material law for parent element is not a fluid");

  if(pmat->MaterialType() == INPAR::MAT::m_fluid)
  {
    {

      const MAT::NewtonianFluid* actmat_p = static_cast<const MAT::NewtonianFluid*>(pmat.get());
      const double pvisc=actmat_p->Viscosity();
      const double pdens = actmat_p->Density();

      const MAT::NewtonianFluid* actmat_m = static_cast<const MAT::NewtonianFluid*>(nmat.get());
      const double nvisc = actmat_m->Viscosity();
      const double ndens = actmat_m->Density();

      if(std::abs(nvisc - pvisc) > 1e-14)
      {
        std::cout << "Parent element viscosity: " << pvisc << " ,neighbor element viscosity: " << nvisc << std::endl;
        dserror("parent and neighbor element do not have the same viscosity!");
      }
      if(std::abs(ndens - pdens) > 1e-14)
      {
        std::cout << "Parent element density: " << pdens << " ,neighbor element density: " << ndens << std::endl;
        dserror("parent and neighbor element do not have the same density!");
      }
    }
  }
  else
  {
    dserror("up to now I expect a FLUID (m_fluid) material for edge stabilization\n");
  }

  return;
}