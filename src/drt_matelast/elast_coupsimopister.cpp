/*----------------------------------------------------------------------*/
/*!
\file elast_coupsimopister.cpp
\brief


the input line should read
  MAT 1 ELAST_CoupSimoPister MUE 1000

<pre>
\maintainer Anna Birzle
            birzle@lnm.mw.tum.de
            089/289 15255
</pre>
*/

/*----------------------------------------------------------------------*/
/* macros */

/*----------------------------------------------------------------------*/
/* headers */

#include <limits>

#include "elast_coupsimopister.H"
#include "../drt_mat/matpar_material.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
MAT::ELASTIC::PAR::CoupSimoPister::CoupSimoPister(
  Teuchos::RCP<MAT::PAR::Material> matdata
  )
: Parameter(matdata),
  mue_(matdata->GetDouble("MUE"))
{
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
MAT::ELASTIC::CoupSimoPister::CoupSimoPister(MAT::ELASTIC::PAR::CoupSimoPister* params)
  : params_(params)
{
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::CoupSimoPister::AddStrainEnergy(
    double& psi,
    const LINALG::Matrix<3,1>& prinv,
    const LINALG::Matrix<3,1>& modinv,
    const int eleGID)
{
  // material Constant mu
  const double mue = params_->mue_;

  // strain energy: \Psi = 0.5*\mu(I_1-3) - \mu log(J)
  // add to overall strain energy
  psi += 0.5 * mue * (prinv(0) - 3.) - mue * log(std::pow(prinv(2),0.5));
}


/*----------------------------------------------------------------------
 *                                                       birzle 12/2014 */
/*----------------------------------------------------------------------*/
void MAT::ELASTIC::CoupSimoPister::AddDerivativesPrincipal(
    LINALG::Matrix<3,1>& dPI,
    LINALG::Matrix<6,1>& ddPII,
    const LINALG::Matrix<3,1>& prinv,
    const int eleGID
)
{
  const double mue     = params_->mue_;

  dPI(0) += 0.5*mue;
  dPI(2) -= 0.5*mue/prinv(2);

  ddPII(2) += 0.5*mue/(prinv(2)*prinv(2));

  return;
}


/*----------------------------------------------------------------------*/