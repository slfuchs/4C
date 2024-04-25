/*----------------------------------------------------------------------*/
/*! \file
\brief Definition of a time integrator for prestressing

\level 3

*/
/*----------------------------------------------------------------------*/

#include "4C_structure_new_impl_prestress.hpp"

#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_structure_new_model_evaluator.hpp"
#include "4C_structure_new_timint_basedataglobalstate.hpp"
#include "4C_structure_new_timint_basedatasdyn.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  inline bool IsMaterialIterative()
  {
    return Teuchos::getIntegralValue<INPAR::STR::PreStress>(
               GLOBAL::Problem::Instance()->StructuralDynamicParams(), "PRESTRESS") ==
           INPAR::STR::PreStress::material_iterative;
  }

  inline bool IsMaterialIterativeActive(const double currentTime)
  {
    INPAR::STR::PreStress pstype = Teuchos::getIntegralValue<INPAR::STR::PreStress>(
        GLOBAL::Problem::Instance()->StructuralDynamicParams(), "PRESTRESS");
    double pstime =
        GLOBAL::Problem::Instance()->StructuralDynamicParams().get<double>("PRESTRESSTIME");
    return pstype == INPAR::STR::PreStress::material_iterative && currentTime <= pstime + 1.0e-15;
  }

  static inline bool IsMulfActive(const double currentTime)
  {
    INPAR::STR::PreStress pstype = Teuchos::getIntegralValue<INPAR::STR::PreStress>(
        GLOBAL::Problem::Instance()->StructuralDynamicParams(), "PRESTRESS");
    double pstime =
        GLOBAL::Problem::Instance()->StructuralDynamicParams().get<double>("PRESTRESSTIME");
    return pstype == INPAR::STR::PreStress::mulf && currentTime <= pstime + 1.0e-15;
  }
}  // namespace

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::IMPLICIT::PreStress::PreStress() : absolute_displacement_norm_(1e9)
{
  // empty constructor
}


void STR::IMPLICIT::PreStress::WriteRestart(
    IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  CheckInitSetup();

  const auto zeros = Teuchos::rcp(new Epetra_Vector(*GlobalState().DofRowMapView(), true));

  // write zero dynamic forces (for dynamic restart after  static prestressing)
  iowriter.WriteVector("finert", zeros);
  iowriter.WriteVector("fvisco", zeros);

  ModelEval().WriteRestart(iowriter, forced_writerestart);
}

void STR::IMPLICIT::PreStress::UpdateStepState()
{
  CheckInitSetup();

  // Compute norm of the displacements
  GlobalState().GetDisNp()->NormInf(&absolute_displacement_norm_);

  if (!IsMaterialIterativePrestressConverged())
  {
    // Only update prestress if the material iterative prestress is not converged
    // update model specific variables
    ModelEval().UpdateStepState(0.0);
  }
}

void STR::IMPLICIT::PreStress::UpdateStepElement()
{
  CheckInitSetup();

  if (!IsMaterialIterativePrestressConverged())
  {
    // Only update prestress if the material iterative prestress is not converged
    ModelEval().UpdateStepElement();
  }
}

void STR::IMPLICIT::PreStress::PostUpdate()
{
  // Check for prestressing
  if (IsMulfActive(GlobalState().GetTimeN()))

  {
    if (GlobalState().GetMyRank() == 0) IO::cout << "====== Resetting Displacements" << IO::endl;
    // This is a MULF step, hence we do not update the displacements at the end of the
    // timestep. This is achieved by resetting the displacements, velocities and
    // accelerations.
    GlobalState().GetDisN()->PutScalar(0.0);
    GlobalState().GetVelN()->PutScalar(0.0);
    GlobalState().GetAccN()->PutScalar(0.0);
  }
  else if (IsMaterialIterativeActive(GlobalState().GetTimeN()))
  {
    // Print prestress status update
    if (GlobalState().GetMyRank() == 0)
    {
      IO::cout << "====== Iterative Prestress Status" << IO::endl;
      IO::cout << "abs-dis-inf-norm:                    " << absolute_displacement_norm_
               << IO::endl;
    }
  }
}

bool STR::IMPLICIT::PreStress::IsMaterialIterativePrestressConverged() const
{
  return IsMaterialIterative() &&
         GlobalState().GetStepN() >= SDyn().GetPreStressMinimumNumberOfLoadSteps() &&
         absolute_displacement_norm_ < SDyn().GetPreStressDisplacementTolerance();
}

bool STR::IMPLICIT::PreStress::EarlyStopping() const
{
  CheckInitSetup();

  if (IsMaterialIterativePrestressConverged())
  {
    if (GlobalState().GetMyRank() == 0)
    {
      IO::cout << "Prestress is converged. Stopping simulation." << IO::endl;
      IO::cout << "abs-dis-inf-norm:                    " << absolute_displacement_norm_
               << IO::endl;
    }
    return true;
  }

  return false;
}

void STR::IMPLICIT::PreStress::PostTimeLoop()
{
  if (IsMaterialIterative())
  {
    if (absolute_displacement_norm_ > SDyn().GetPreStressDisplacementTolerance())
    {
      FOUR_C_THROW(
          "Prestress algorithm did not converged within the given timesteps. "
          "abs-dis-inf-norm is "
          "%f",
          absolute_displacement_norm_);
    }
  }
}

FOUR_C_NAMESPACE_CLOSE
