/*--------------------------------------------------------------------------*/
/*! \file
\brief Material model for the lubrication film

\level 3

*/
/*--------------------------------------------------------------------------*/


#include "4C_mat_lubrication_mat.hpp"

#include "4C_global_data.hpp"
#include "4C_mat_lubrication_law.hpp"
#include "4C_mat_par_bundle.hpp"

#include <vector>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::PAR::LubricationMat::LubricationMat(const Core::Mat::PAR::Parameter::Data& matdata)
    : Parameter(matdata),
      density_(matdata.parameters.get<double>("DENSITY")),
      lubricationlawID_(matdata.parameters.get<int>("LUBRICATIONLAWID")),
      lubricationlaw_(nullptr)
{
  // retrieve problem instance to read from
  const int probinst = Global::Problem::instance()->materials()->get_read_from_problem();

  // for the sake of safety
  if (Global::Problem::instance(probinst)->materials() == Teuchos::null)
    FOUR_C_THROW("List of materials cannot be accessed in the global problem instance.");
  // yet another safety check
  if (Global::Problem::instance(probinst)->materials()->num() == 0)
    FOUR_C_THROW("List of materials in the global problem instance is empty.");

  auto* curmat =
      Global::Problem::instance(probinst)->materials()->parameter_by_id(lubricationlawID_);

  switch (curmat->type())
  {
    case Core::Materials::m_lubrication_law_constant:
    {
      lubricationlaw_ = static_cast<Mat::PAR::LubricationLaw*>(curmat);
      break;
    }
    case Core::Materials::m_lubrication_law_barus:
    {
      lubricationlaw_ = static_cast<Mat::PAR::LubricationLaw*>(curmat);
      break;
    }
    case Core::Materials::m_lubrication_law_roeland:
    {
      lubricationlaw_ = static_cast<Mat::PAR::LubricationLaw*>(curmat);
      break;
    }
    default:
      FOUR_C_THROW("invalid material for lubrication law %d", curmat->type());
      break;
  }
}


Teuchos::RCP<Core::Mat::Material> Mat::PAR::LubricationMat::create_material()
{
  return Teuchos::rcp(new Mat::LubricationMat(this));
}

Mat::LubricationMatType Mat::LubricationMatType::instance_;

Core::Communication::ParObject* Mat::LubricationMatType::create(
    Core::Communication::UnpackBuffer& buffer)
{
  Mat::LubricationMat* lubrication_mat = new Mat::LubricationMat();
  lubrication_mat->unpack(buffer);
  return lubrication_mat;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::LubricationMat::LubricationMat() : params_(nullptr) {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::LubricationMat::LubricationMat(Mat::PAR::LubricationMat* params) : params_(params) {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::LubricationMat::pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = unique_par_object_id();
  add_to_pack(data, type);

  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->id();  // in case we are in post-process mode
  add_to_pack(data, matid);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::LubricationMat::unpack(Core::Communication::UnpackBuffer& buffer)
{
  Core::Communication::extract_and_assert_id(buffer, unique_par_object_id());

  // matid and recover params_
  int matid;
  extract_from_pack(buffer, matid);
  params_ = nullptr;
  if (Global::Problem::instance()->materials() != Teuchos::null)
    if (Global::Problem::instance()->materials()->num() != 0)
    {
      const int probinst = Global::Problem::instance()->materials()->get_read_from_problem();
      Core::Mat::PAR::Parameter* mat =
          Global::Problem::instance(probinst)->materials()->parameter_by_id(matid);
      if (mat->type() == material_type())
        params_ = static_cast<Mat::PAR::LubricationMat*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->type(),
            material_type());
    }

  FOUR_C_THROW_UNLESS(buffer.at_end(), "Buffer not fully consumed.");
}

/*----------------------------------------------------------------------*
                                                              wirtz 09/16|
*----------------------------------------------------------------------*/
double Mat::LubricationMat::compute_viscosity(const double press)
{
  double visc = -1.;
  params_->lubricationlaw_->compute_viscosity(press, visc);

  return visc;
}


/*----------------------------------------------------------------------*
                                                              wirtz 09/16|
*----------------------------------------------------------------------*/
double Mat::LubricationMat::compute_viscosity_deriv(const double press, const double visc)
{
  double visc_dp = -1;
  params_->lubricationlaw_->constitutive_derivatives(press, visc, visc_dp);

  return visc_dp;
}

FOUR_C_NAMESPACE_CLOSE
