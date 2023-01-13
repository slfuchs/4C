/*----------------------------------------------------------------------*/
/*! \file
\brief multiscale variant of 3D quadratic serendipity element
\level 2


*----------------------------------------------------------------------*/


#include "so3_hex20.H"
#include "mat_micromaterial.H"
#include "lib_globalproblem.H"
#include "comm_utils.H"
#include "lib_discret.H"



/*----------------------------------------------------------------------*
 |  homogenize material density (public)                                |
 *----------------------------------------------------------------------*/
// this routine is intended to determine a homogenized material
// density for multi-scale analyses by averaging over the initial volume

void DRT::ELEMENTS::So_hex20::soh20_homog(Teuchos::ParameterList& params)
{
  if (DRT::Problem::Instance(0)->GetCommunicators()->SubComm()->MyPID() == Owner())
  {
    double homogdens = 0.;
    const static std::vector<double> weights = soh20_weights();

    for (int gp = 0; gp < NUMGPT_SOH20; ++gp)
    {
      const double density = Material()->Density(gp);
      homogdens += detJ_[gp] * weights[gp] * density;
    }

    double homogdensity = params.get<double>("homogdens", 0.0);
    params.set("homogdens", homogdensity + homogdens);
  }

  return;
}


/*----------------------------------------------------------------------*
 |  Read restart on the microscale                                      |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex20::soh20_read_restart_multi()
{
  Teuchos::RCP<MAT::Material> mat = Material();

  if (mat->MaterialType() == INPAR::MAT::m_struct_multiscale)
  {
    auto* micro = dynamic_cast<MAT::MicroMaterial*>(mat.get());
    int eleID = Id();
    bool eleowner = false;
    if (DRT::Problem::Instance()->GetDis("structure")->Comm().MyPID() == Owner()) eleowner = true;

    for (int gp = 0; gp < NUMGPT_SOH20; ++gp) micro->ReadRestart(gp, eleID, eleowner);
  }

  return;
}
