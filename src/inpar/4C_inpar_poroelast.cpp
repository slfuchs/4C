/*----------------------------------------------------------------------*/
/*! \file
\brief Input parameters for poro elasticity

\level 2


*/

/*----------------------------------------------------------------------*/



#include "4C_inpar_poroelast.hpp"

#include "4C_inpar_fluid.hpp"
#include "4C_linalg_equilibrate.hpp"
#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN


void INPAR::POROELAST::SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list)
{
  using namespace INPUT;
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  Teuchos::ParameterList& poroelastdyn =
      list->sublist("POROELASTICITY DYNAMIC", false, "Poroelasticity");

  // Coupling strategy for (monolithic) porous media solvers
  setStringToIntegralParameter<int>("COUPALGO", "poro_monolithic",
      "Coupling strategies for poroelasticity solvers",
      tuple<std::string>("poro_partitioned", "poro_monolithic", "poro_monolithicstructuresplit",
          "poro_monolithicfluidsplit", "poro_monolithicnopenetrationsplit",
          "poro_monolithicmeshtying"),
      tuple<int>(Partitioned, Monolithic, Monolithic_structuresplit, Monolithic_fluidsplit,
          Monolithic_nopenetrationsplit, Monolithic_meshtying),
      &poroelastdyn);

  // physical type of poro fluid flow (incompressible, varying density, loma, Boussinesq
  // approximation)
  setStringToIntegralParameter<int>("PHYSICAL_TYPE", "Poro", "Physical Type of Porofluid",
      tuple<std::string>("Poro", "Poro_P1"), tuple<int>(INPAR::FLUID::poro, INPAR::FLUID::poro_p1),
      &poroelastdyn);

  // physical type of poro fluid flow (incompressible, varying density, loma, Boussinesq
  // approximation)
  setStringToIntegralParameter<int>("TRANSIENT_TERMS", "all",
      "which equation includes transient terms",
      tuple<std::string>("none", "momentum", "continuity", "all"),
      tuple<int>(transient_none, transient_momentum_only, transient_continuity_only, transient_all),
      &poroelastdyn);

  // Output type
  CORE::UTILS::IntParameter(
      "RESTARTEVRY", 1, "write restart possibility every RESTARTEVRY steps", &poroelastdyn);
  // Time loop control
  CORE::UTILS::IntParameter("NUMSTEP", 200, "maximum number of Timesteps", &poroelastdyn);
  CORE::UTILS::DoubleParameter("MAXTIME", 1000.0, "total simulation time", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TIMESTEP", 0.05, "time step size dt", &poroelastdyn);
  CORE::UTILS::IntParameter(
      "ITEMAX", 10, "maximum number of iterations over fields", &poroelastdyn);
  CORE::UTILS::IntParameter("ITEMIN", 1, "minimal number of iterations over fields", &poroelastdyn);
  CORE::UTILS::IntParameter("RESULTSEVRY", 1, "increment for writing solution", &poroelastdyn);

  // Iterationparameters
  CORE::UTILS::DoubleParameter("TOLRES_GLOBAL", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLINC_GLOBAL", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLRES_DISP", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLINC_DISP", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLRES_PORO", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLINC_PORO", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter(
      "TOLRES_VEL", 1e-8, "tolerance in the residual norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLINC_VEL", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLRES_PRES", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLINC_PRES", 1e-8,
      "tolerance in the increment norm for the Newton iteration", &poroelastdyn);
  CORE::UTILS::DoubleParameter("TOLRES_NCOUP", 1e-8,
      "tolerance in the residual norm for the Newton iteration", &poroelastdyn);

  setStringToIntegralParameter<int>("NORM_INC", "AbsSingleFields",
      "type of norm for primary variables convergence check",
      tuple<std::string>("AbsGlobal", "AbsSingleFields"),
      tuple<int>(convnorm_abs_global, convnorm_abs_singlefields), &poroelastdyn);

  setStringToIntegralParameter<int>("NORM_RESF", "AbsSingleFields",
      "type of norm for residual convergence check",
      tuple<std::string>("AbsGlobal", "AbsSingleFields"),
      tuple<int>(convnorm_abs_global, convnorm_abs_singlefields), &poroelastdyn);

  setStringToIntegralParameter<int>("NORMCOMBI_RESFINC", "And",
      "binary operator to combine primary variables and residual force values",
      tuple<std::string>("And", "Or"), tuple<int>(bop_and, bop_or), &poroelastdyn);

  setStringToIntegralParameter<int>("VECTORNORM_RESF", "L2",
      "type of norm to be applied to residuals",
      tuple<std::string>("L1", "L1_Scaled", "L2", "Rms", "Inf"),
      tuple<int>(norm_l1, norm_l1_scaled, norm_l2, norm_rms, norm_inf), &poroelastdyn);

  setStringToIntegralParameter<int>("VECTORNORM_INC", "L2",
      "type of norm to be applied to residuals",
      tuple<std::string>("L1", "L1_Scaled", "L2", "Rms", "Inf"),
      tuple<int>(norm_l1, norm_l1_scaled, norm_l2, norm_rms, norm_inf), &poroelastdyn);

  CORE::UTILS::BoolParameter(
      "SECONDORDER", "Yes", "Second order coupling at the interface.", &poroelastdyn);

  CORE::UTILS::BoolParameter("CONTIPARTINT", "No",
      "Partial integration of porosity gradient in continuity equation", &poroelastdyn);

  CORE::UTILS::BoolParameter("CONTACTNOPEN", "No",
      "No-Penetration Condition on active contact surface in case of poro contact problem!",
      &poroelastdyn);

  CORE::UTILS::BoolParameter("MATCHINGGRID", "Yes", "is matching grid", &poroelastdyn);

  CORE::UTILS::BoolParameter("CONVECTIVE_TERM", "No", "convective term ", &poroelastdyn);

  // number of linear solver used for poroelasticity
  CORE::UTILS::IntParameter("LINEAR_SOLVER", -1,
      "number of linear solver used for poroelasticity problems", &poroelastdyn);

  // flag for equilibration of global system of equations
  setStringToIntegralParameter<CORE::LINALG::EquilibrationMethod>("EQUILIBRATION", "none",
      "flag for equilibration of global system of equations",
      tuple<std::string>("none", "rows_full", "rows_maindiag", "columns_full", "columns_maindiag",
          "rowsandcolumns_full", "rowsandcolumns_maindiag"),
      tuple<CORE::LINALG::EquilibrationMethod>(CORE::LINALG::EquilibrationMethod::none,
          CORE::LINALG::EquilibrationMethod::rows_full,
          CORE::LINALG::EquilibrationMethod::rows_maindiag,
          CORE::LINALG::EquilibrationMethod::columns_full,
          CORE::LINALG::EquilibrationMethod::columns_maindiag,
          CORE::LINALG::EquilibrationMethod::rowsandcolumns_full,
          CORE::LINALG::EquilibrationMethod::rowsandcolumns_maindiag),
      &poroelastdyn);
}

FOUR_C_NAMESPACE_CLOSE
