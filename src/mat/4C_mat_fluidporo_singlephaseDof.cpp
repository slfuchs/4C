/*----------------------------------------------------------------------*/
/*! \file
 \brief a material defining the degree of freedom of a single phase of
        a multiphase porous fluid

   \level 3

 *----------------------------------------------------------------------*/

#include "4C_mat_fluidporo_singlephaseDof.hpp"

#include "4C_global_data.hpp"
#include "4C_mat_fluidporo_singlephaselaw.hpp"
#include "4C_mat_par_bundle.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *  constructor (public)                               vuong 08/16      |
 *----------------------------------------------------------------------*/
MAT::PAR::FluidPoroPhaseDof::FluidPoroPhaseDof(Teuchos::RCP<CORE::MAT::PAR::Material> matdata)
    : Parameter(matdata)
{
}

/*----------------------------------------------------------------------*
 *  factory method for phase dof                       vuong 08/16      |
 *----------------------------------------------------------------------*/
MAT::PAR::FluidPoroPhaseDof* MAT::PAR::FluidPoroPhaseDof::CreatePhaseDof(int phasedofId)
{
  // retrieve problem instance to read from
  const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();

  // for the sake of safety
  if (GLOBAL::Problem::Instance(probinst)->Materials() == Teuchos::null)
    FOUR_C_THROW("List of materials cannot be accessed in the global problem instance.");
  // yet another safety check
  if (GLOBAL::Problem::Instance(probinst)->Materials()->Num() == 0)
    FOUR_C_THROW("List of materials in the global problem instance is empty.");

  // retrieve validated input line of material ID in question
  auto* curmat = GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(phasedofId);

  // phase law
  MAT::PAR::FluidPoroPhaseDof* phasedof = nullptr;

  switch (curmat->Type())
  {
    case CORE::Materials::m_fluidporo_phasedof_diffpressure:
    {
      phasedof = static_cast<MAT::PAR::FluidPoroPhaseDofDiffPressure*>(curmat);
      break;
    }
    case CORE::Materials::m_fluidporo_phasedof_pressure:
    {
      phasedof = static_cast<MAT::PAR::FluidPoroPhaseDofPressure*>(curmat);
      break;
    }
    case CORE::Materials::m_fluidporo_phasedof_saturation:
    {
      phasedof = static_cast<MAT::PAR::FluidPoroPhaseDofSaturation*>(curmat);
      break;
    }
    default:
      FOUR_C_THROW("invalid pressure-saturation law for material %d", curmat->Type());
      break;
  }

  return phasedof;
}

/************************************************************************/
/************************************************************************/

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
MAT::PAR::FluidPoroPhaseDofDiffPressure::FluidPoroPhaseDofDiffPressure(
    Teuchos::RCP<CORE::MAT::PAR::Material> matdata)
    : FluidPoroPhaseDof(matdata),
      diffpresCoeffs_(matdata->Get<std::vector<int>>("PRESCOEFF")),
      phaselawId_(matdata->Get<int>("PHASELAWID"))
{
  phaselaw_ = MAT::PAR::FluidPoroPhaseLaw::CreatePhaseLaw(phaselawId_);
}

/*----------------------------------------------------------------------*
 *  Initialize                                               vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::PAR::FluidPoroPhaseDofDiffPressure::Initialize()
{
  phaselaw_->Initialize();
  return;
}

/*----------------------------------------------------------------------*
 *  return phase law type                                  vuong 08/16 |
 *----------------------------------------------------------------------*/
CORE::Materials::MaterialType MAT::PAR::FluidPoroPhaseDofDiffPressure::PoroPhaseLawType() const
{
  return phaselaw_->Type();
}

/*----------------------------------------------------------------------*
 *  fill the dof matrix with the phase dofs                 vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::PAR::FluidPoroPhaseDofDiffPressure::FillDoFMatrix(
    CORE::LINALG::SerialDenseMatrix& dofmat, int numphase) const
{
  // safety check
  if ((int)diffpresCoeffs_.size() != dofmat.numCols())
    FOUR_C_THROW(
        "Number of phases given by the poro singlephase material %i "
        "does not match number of DOFs (%i phases and %i DOFs)!",
        phaselaw_->Id(), diffpresCoeffs_.size(), dofmat.numCols());

  // fill pressure coefficients into matrix
  for (size_t i = 0; i < diffpresCoeffs_.size(); i++)
  {
    const int val = diffpresCoeffs_[i];
    if (val != 0) dofmat(numphase, i) = val;
  }
}

/*----------------------------------------------------------------------*
 *  Evaluate generalized pressure of a phase                vuong 08/16 |
 *----------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofDiffPressure::EvaluateGenPressure(
    int phasenum, const std::vector<double>& state) const
{
  // return the corresponding dof value
  return state[phasenum];
}


/*----------------------------------------------------------------------*
 *   Evaluate saturation of the phase                       vuong 08/16 |
 *----------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofDiffPressure::EvaluateSaturation(
    int phasenum, const std::vector<double>& state, const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->EvaluateSaturation(pressure);
}


/*--------------------------------------------------------------------------*
 *  Evaluate derivative of saturation w.r.t. pressure           vuong 08/16 |
 *---------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofDiffPressure::evaluate_deriv_of_saturation_wrt_pressure(
    int phasenum, int doftoderive, const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->evaluate_deriv_of_saturation_wrt_pressure(doftoderive, pressure);
}

/*--------------------------------------------------------------------------*
 *  Evaluate 2nd derivative of saturation w.r.t. pressure  kremheller 05/17 |
 *---------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofDiffPressure::evaluate_second_deriv_of_saturation_wrt_pressure(
    int phasenum, int firstdoftoderive, int seconddoftoderive,
    const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->evaluate_second_deriv_of_saturation_wrt_pressure(
      firstdoftoderive, seconddoftoderive, pressure);
}

/*----------------------------------------------------------------------------------------*
 * Evaluate derivative of degree of freedom with respect to pressure          vuong 08/16 |
 *----------------------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofDiffPressure::evaluate_deriv_of_dof_wrt_pressure(
    int phasenum, int doftoderive, const std::vector<double>& state) const
{
  // derivative is the corresponding coefficient
  return diffpresCoeffs_[doftoderive];
}

/************************************************************************/
/************************************************************************/

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
MAT::PAR::FluidPoroPhaseDofPressure::FluidPoroPhaseDofPressure(
    Teuchos::RCP<CORE::MAT::PAR::Material> matdata)
    : FluidPoroPhaseDof(matdata), phaselawId_(matdata->Get<int>("PHASELAWID"))
{
  phaselaw_ = MAT::PAR::FluidPoroPhaseLaw::CreatePhaseLaw(phaselawId_);
  return;
}

/*----------------------------------------------------------------------*
 *  Initialize                                               vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::PAR::FluidPoroPhaseDofPressure::Initialize()
{
  phaselaw_->Initialize();
  return;
}

/*----------------------------------------------------------------------*
 *  return phase law type                                  vuong 08/16 |
 *----------------------------------------------------------------------*/
CORE::Materials::MaterialType MAT::PAR::FluidPoroPhaseDofPressure::PoroPhaseLawType() const
{
  return phaselaw_->Type();
}

/*----------------------------------------------------------------------*
 *  fill the dof matrix with the phase dofs                 vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::PAR::FluidPoroPhaseDofPressure::FillDoFMatrix(
    CORE::LINALG::SerialDenseMatrix& dofmat, int numphase) const
{
  // just mark the corresponding entry in the matrix
  dofmat(numphase, numphase) = 1.0;
}

/*----------------------------------------------------------------------*
 *  Evaluate generalized pressure of a phase                vuong 08/16 |
 *----------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofPressure::EvaluateGenPressure(
    int phasenum, const std::vector<double>& state) const
{
  // return the corresponding dof value
  return state[phasenum];
}


/*----------------------------------------------------------------------*
 *   Evaluate saturation of the phase                       vuong 08/16 |
 *----------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofPressure::EvaluateSaturation(
    int phasenum, const std::vector<double>& state, const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->EvaluateSaturation(pressure);
}


/*--------------------------------------------------------------------------*
 *  Evaluate derivative of saturation w.r.t. pressure           vuong 08/16 |
 *---------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofPressure::evaluate_deriv_of_saturation_wrt_pressure(
    int phasenum, int doftoderive, const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->evaluate_deriv_of_saturation_wrt_pressure(doftoderive, pressure);
}

/*--------------------------------------------------------------------------*
 *  Evaluate 2nd derivative of saturation w.r.t. pressure  kremheller 05/17 |
 *---------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofPressure::evaluate_second_deriv_of_saturation_wrt_pressure(
    int phasenum, int firstdoftoderive, int seconddoftoderive,
    const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->evaluate_second_deriv_of_saturation_wrt_pressure(
      firstdoftoderive, seconddoftoderive, pressure);
}

/*----------------------------------------------------------------------------------------*
 * Evaluate derivative of degree of freedom with respect to pressure          vuong 08/16 |
 *----------------------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofPressure::evaluate_deriv_of_dof_wrt_pressure(
    int phasenum, int doftoderive, const std::vector<double>& state) const
{
  double presurederiv = 0.0;

  // respective derivative of w.r.t. is either 0 or 1
  if (phasenum == doftoderive) presurederiv = 1.0;

  return presurederiv;
}

/************************************************************************/
/************************************************************************/

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
MAT::PAR::FluidPoroPhaseDofSaturation::FluidPoroPhaseDofSaturation(
    Teuchos::RCP<CORE::MAT::PAR::Material> matdata)
    : FluidPoroPhaseDof(matdata), phaselawId_(matdata->Get<int>("PHASELAWID"))
{
  phaselaw_ = MAT::PAR::FluidPoroPhaseLaw::CreatePhaseLaw(phaselawId_);
  return;
}

/*----------------------------------------------------------------------*
 *  Initialize                                               vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::PAR::FluidPoroPhaseDofSaturation::Initialize()
{
  phaselaw_->Initialize();
  return;
}

/*----------------------------------------------------------------------*
 *  return phase law type                                  vuong 08/16 |
 *----------------------------------------------------------------------*/
CORE::Materials::MaterialType MAT::PAR::FluidPoroPhaseDofSaturation::PoroPhaseLawType() const
{
  return phaselaw_->Type();
}

/*----------------------------------------------------------------------*
 *  fill the dof matrix with the phase dofs                 vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::PAR::FluidPoroPhaseDofSaturation::FillDoFMatrix(
    CORE::LINALG::SerialDenseMatrix& dofmat, int numphase) const
{
  // get pressure coefficients of phase law
  const std::vector<int>* presIDs = phaselaw_->PresIds();

  // safety check
  if ((int)presIDs->size() != dofmat.numCols())
    FOUR_C_THROW(
        "Number of phases given by the poro phase law material %i "
        "does not match number of DOFs (%i phases and %i DOFs)!",
        phaselaw_->Id(), presIDs->size(), dofmat.numCols());

  // fill pressure coefficients of phase law into matrix
  for (size_t i = 0; i < presIDs->size(); i++)
  {
    const int val = (*presIDs)[i];
    if (val != 0) dofmat(numphase, i) = val;
  }
}

/*----------------------------------------------------------------------*
 *  Evaluate generalized pressure of a phase                vuong 08/16 |
 *----------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofSaturation::EvaluateGenPressure(
    int phasenum, const std::vector<double>& state) const
{
  // evaluate the phase law for the generalized (i.e. some differential pressure)
  // the phase law depends on
  return phaselaw_->EvaluateGenPressure(state[phasenum]);
}


/*----------------------------------------------------------------------*
 *   Evaluate saturation of the phase                       vuong 08/16 |
 *----------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofSaturation::EvaluateSaturation(
    int phasenum, const std::vector<double>& state, const std::vector<double>& pressure) const
{
  // get the corresponding dof value
  return state[phasenum];
}


/*--------------------------------------------------------------------------*
 *  Evaluate derivative of saturation w.r.t. pressure           vuong 08/16 |
 *---------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofSaturation::evaluate_deriv_of_saturation_wrt_pressure(
    int phasenum, int doftoderive, const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->evaluate_deriv_of_saturation_wrt_pressure(doftoderive, pressure);
}

/*--------------------------------------------------------------------------*
 *  Evaluate 2nd derivative of saturation w.r.t. pressure  kremheller 05/17 |
 *---------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofSaturation::evaluate_second_deriv_of_saturation_wrt_pressure(
    int phasenum, int firstdoftoderive, int seconddoftoderive,
    const std::vector<double>& pressure) const
{
  // call the phase law
  return phaselaw_->evaluate_second_deriv_of_saturation_wrt_pressure(
      firstdoftoderive, seconddoftoderive, pressure);
}

/*----------------------------------------------------------------------------------------*
 * Evaluate derivative of degree of freedom with respect to pressure          vuong 08/16 |
 *----------------------------------------------------------------------------------------*/
double MAT::PAR::FluidPoroPhaseDofSaturation::evaluate_deriv_of_dof_wrt_pressure(
    int phasenum, int doftoderive, const std::vector<double>& pressure) const
{
  // call the phase law for the derivative
  return phaselaw_->evaluate_deriv_of_saturation_wrt_pressure(doftoderive, pressure);
}

/************************************************************************/
/************************************************************************/

FOUR_C_NAMESPACE_CLOSE
