/*----------------------------------------------------------------------*/
/*! \file
\brief Input parameters for poro scatra

   \level 2

*/

/*----------------------------------------------------------------------*/



#include "4C_inpar_poroscatra.hpp"

#include "4C_inpar_poroelast.hpp"
#include "4C_inpar_scatra.hpp"
#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN



void INPAR::PORO_SCATRA::SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list)
{
  using namespace INPUT;
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  Teuchos::ParameterList& poroscatradyn = list->sublist(
      "POROSCATRA CONTROL", false, "Control paramters for scatra porous media coupling");

  // Output type
  CORE::UTILS::IntParameter(
      "RESTARTEVRY", 1, "write restart possibility every RESTARTEVRY steps", &poroscatradyn);
  // Time loop control
  CORE::UTILS::IntParameter("NUMSTEP", 200, "maximum number of Timesteps", &poroscatradyn);
  CORE::UTILS::DoubleParameter("MAXTIME", 1000.0, "total simulation time", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TIMESTEP", 0.05, "time step size dt", &poroscatradyn);
  CORE::UTILS::IntParameter("RESULTSEVRY", 1, "increment for writing solution", &poroscatradyn);
  CORE::UTILS::IntParameter(
      "ITEMAX", 10, "maximum number of iterations over fields", &poroscatradyn);
  CORE::UTILS::IntParameter(
      "ITEMIN", 1, "minimal number of iterations over fields", &poroscatradyn);

  // Iterationparameters
  CORE::UTILS::DoubleParameter("TOLRES_GLOBAL", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLINC_GLOBAL", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLRES_DISP", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLINC_DISP", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroscatradyn);
  //  CORE::UTILS::DoubleParameter("TOLRES_PORO",1e-8,"tolerance in the residual norm for the Newton
  //  iteration",&poroscatradyn); CORE::UTILS::DoubleParameter("TOLINC_PORO",1e-8,"tolerance in the
  //  increment norm for the Newton iteration",&poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLRES_VEL", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLINC_VEL", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLRES_PRES", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLINC_PRES", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLRES_SCALAR", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroscatradyn);
  CORE::UTILS::DoubleParameter("TOLINC_SCALAR", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroscatradyn);

  setStringToIntegralParameter<int>("NORM_INC", "AbsSingleFields",
      "type of norm for primary variables convergence check",
      tuple<std::string>("AbsGlobal", "AbsSingleFields"),
      tuple<int>(
          INPAR::POROELAST::convnorm_abs_global, INPAR::POROELAST::convnorm_abs_singlefields),
      &poroscatradyn);

  setStringToIntegralParameter<int>("NORM_RESF", "AbsSingleFields",
      "type of norm for residual convergence check",
      tuple<std::string>("AbsGlobal", "AbsSingleFields"),
      tuple<int>(
          INPAR::POROELAST::convnorm_abs_global, INPAR::POROELAST::convnorm_abs_singlefields),
      &poroscatradyn);

  setStringToIntegralParameter<int>("NORMCOMBI_RESFINC", "And",
      "binary operator to combine primary variables and residual force values",
      tuple<std::string>("And", "Or"),
      tuple<int>(INPAR::POROELAST::bop_and, INPAR::POROELAST::bop_or), &poroscatradyn);

  setStringToIntegralParameter<int>("VECTORNORM_RESF", "L2",
      "type of norm to be applied to residuals",
      tuple<std::string>("L1", "L1_Scaled", "L2", "Rms", "Inf"),
      tuple<int>(INPAR::POROELAST::norm_l1, INPAR::POROELAST::norm_l1_scaled,
          INPAR::POROELAST::norm_l2, INPAR::POROELAST::norm_rms, INPAR::POROELAST::norm_inf),
      &poroscatradyn);

  setStringToIntegralParameter<int>("VECTORNORM_INC", "L2",
      "type of norm to be applied to residuals",
      tuple<std::string>("L1", "L1_Scaled", "L2", "Rms", "Inf"),
      tuple<int>(INPAR::POROELAST::norm_l1, INPAR::POROELAST::norm_l1_scaled,
          INPAR::POROELAST::norm_l2, INPAR::POROELAST::norm_rms, INPAR::POROELAST::norm_inf),
      &poroscatradyn);

  // number of linear solver used for poroelasticity
  CORE::UTILS::IntParameter("LINEAR_SOLVER", -1,
      "number of linear solver used for monolithic poroscatra problems", &poroscatradyn);

  // Coupling strategy for poroscatra solvers
  setStringToIntegralParameter<int>("COUPALGO", "solid_to_scatra",
      "Coupling strategies for poroscatra solvers",
      tuple<std::string>("monolithic", "scatra_to_solid", "solid_to_scatra", "two_way"),
      tuple<int>(Monolithic, Part_ScatraToPoro, Part_PoroToScatra, Part_TwoWay), &poroscatradyn);

  CORE::UTILS::BoolParameter("MATCHINGGRID", "Yes", "is matching grid", &poroscatradyn);
}

FOUR_C_NAMESPACE_CLOSE