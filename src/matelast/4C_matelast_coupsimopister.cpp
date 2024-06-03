/*----------------------------------------------------------------------*/
/*! \file
\brief Implementation of the Simo and Pister material model except the volumetric term
\level 1
*/
/*----------------------------------------------------------------------*/

#include "4C_matelast_coupsimopister.hpp"

#include "4C_material_parameter_base.hpp"

#include <limits>

FOUR_C_NAMESPACE_OPEN


MAT::ELASTIC::PAR::CoupSimoPister::CoupSimoPister(
    const Teuchos::RCP<CORE::MAT::PAR::Material>& matdata)
    : Parameter(matdata), mue_(matdata->Get<double>("MUE"))
{
}

MAT::ELASTIC::CoupSimoPister::CoupSimoPister(MAT::ELASTIC::PAR::CoupSimoPister* params)
    : params_(params)
{
}

void MAT::ELASTIC::CoupSimoPister::AddStrainEnergy(double& psi,
    const CORE::LINALG::Matrix<3, 1>& prinv, const CORE::LINALG::Matrix<3, 1>& modinv,
    const CORE::LINALG::Matrix<6, 1>& glstrain, const int gp, const int eleGID)
{
  // material Constant mu
  const double mue = params_->mue_;

  // strain energy: \Psi = 0.5*\mu(I_1-3) - \mu log(J)
  // add to overall strain energy
  psi += 0.5 * mue * (prinv(0) - 3.) - mue * log(std::pow(prinv(2), 0.5));
}

void MAT::ELASTIC::CoupSimoPister::add_derivatives_principal(CORE::LINALG::Matrix<3, 1>& dPI,
    CORE::LINALG::Matrix<6, 1>& ddPII, const CORE::LINALG::Matrix<3, 1>& prinv, const int gp,
    const int eleGID)
{
  const double mue = params_->mue_;

  dPI(0) += 0.5 * mue;
  dPI(2) -= 0.5 * mue / prinv(2);

  ddPII(2) += 0.5 * mue / (prinv(2) * prinv(2));
}
FOUR_C_NAMESPACE_CLOSE
