/*----------------------------------------------------------------------*/
/*! \file

\brief Basis of all LOMA algorithms

\level 2


*/
/*----------------------------------------------------------------------*/


#include "4C_loma_algorithm.hpp"

#include "4C_fluid_ele_action.hpp"
#include "4C_fluid_timint_loma.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_solver.hpp"
#include "4C_io_control.hpp"
#include "4C_lib_assemblestrategy.hpp"
#include "4C_linalg_blocksparsematrix.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linear_solver_method.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_scatra_timint_loma.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
LOMA::Algorithm::Algorithm(const Epetra_Comm& comm, const Teuchos::ParameterList& prbdyn,
    const Teuchos::ParameterList& solverparams)
    : ScaTraFluidCouplingAlgorithm(comm, prbdyn, false, "scatra", solverparams),
      monolithic_(false),
      lomadbcmap_(Teuchos::null),
      lomaincrement_(Teuchos::null),
      lomarhs_(Teuchos::null),
      zeros_(Teuchos::null),
      lomasystemmatrix_(Teuchos::null),
      lomasolver_(Teuchos::null),
      dt_(0.0),
      maxtime_(0.0),
      stepmax_(0),
      itmax_(0),
      itmaxpre_(0),
      itmaxbs_(0),
      ittol_(1.0),
      samstart_(-1),
      turbinflow_(false),
      numinflowsteps_(-1),
      probdyn_(prbdyn)
{
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::Init()
{
  // call Init() in base class
  ADAPTER::ScaTraFluidCouplingAlgorithm::Init();

  // flag for monolithic solver
  monolithic_ = (CORE::UTILS::IntegralValue<int>(probdyn_, "MONOLITHIC"));

  // time-step length, maximum time and maximum number of steps
  dt_ = probdyn_.get<double>("TIMESTEP");
  maxtime_ = probdyn_.get<double>("MAXTIME");
  stepmax_ = probdyn_.get<int>("NUMSTEP");

  // (preliminary) maximum number of iterations and tolerance for outer iteration
  ittol_ = probdyn_.get<double>("CONVTOL");
  itmaxpre_ = probdyn_.get<int>("ITEMAX");
  // maximum number of iterations before sampling (turbulent flow only)
  itmaxbs_ = probdyn_.get<int>("ITEMAX_BEFORE_SAMPLING");

  // flag for constant thermodynamic pressure
  consthermpress_ = probdyn_.get<std::string>("CONSTHERMPRESS");

  // flag for special flow and start of sampling period from fluid parameter list
  const Teuchos::ParameterList& fluiddyn = GLOBAL::Problem::Instance()->FluidDynamicParams();
  special_flow_ = fluiddyn.sublist("TURBULENCE MODEL").get<std::string>("CANONICAL_FLOW");
  samstart_ = fluiddyn.sublist("TURBULENCE MODEL").get<int>("SAMPLING_START");

  // check scatra solver type, which should be incremental, for the time being
  if (not ScaTraField()->IsIncremental())
    FOUR_C_THROW("Incremental ScaTra formulation required for low-Mach-number flow");

  // flag for turbulent inflow
  turbinflow_ =
      CORE::UTILS::IntegralValue<int>(fluiddyn.sublist("TURBULENT INFLOW"), "TURBULENTINFLOW");
  // number of inflow steps
  numinflowsteps_ = fluiddyn.sublist("TURBULENT INFLOW").get<int>("NUMINFLOWSTEP");
  if (turbinflow_)
  {
    if (Comm().MyPID() == 0)
    {
      std::cout << "##############################################################" << '\n';
      std::cout << "#                     TURBULENT INFLOW                       #" << '\n';
      std::cout << "# Caution!                                                   #" << '\n';
      std::cout << "# Assumptions: - constant thermodynamic pressure in main     #" << '\n';
      std::cout << "#                problem domain                              #" << '\n';
      std::cout << "#              - inflow domain is closed system without in-/ #" << '\n';
      std::cout << "#                outflow and heating                         #" << '\n';
      std::cout << "#                -> constant thermodynamic pressure          #" << '\n';
      std::cout << "##############################################################" << '\n';
    }

    if (special_flow_ != "loma_backward_facing_step")
      FOUR_C_THROW("Turbulent inflow generation only for backward-facing step!");
    if (consthermpress_ != "Yes")
      FOUR_C_THROW("Constant thermodynamic pressure in main problem domain!");
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::Setup()
{
  // call Setup() in base class
  ADAPTER::ScaTraFluidCouplingAlgorithm::Setup();

  const Teuchos::ParameterList& fluiddyn = GLOBAL::Problem::Instance()->FluidDynamicParams();

  // preparatives for monolithic solver
  if (monolithic_)
  {
    // check whether turbulent inflow is included,
    // which is currently not possible for monolithic solver
    if (turbinflow_) FOUR_C_THROW("No turbulent inflow for monolithic low-Mach-number solver");

    // check whether (fluid) linearization scheme is a fixed-point-like scheme,
    // which is the only one enabled for monolithic solver, for the time being
    INPAR::FLUID::LinearisationAction linearization =
        CORE::UTILS::IntegralValue<INPAR::FLUID::LinearisationAction>(fluiddyn, "NONLINITER");
    if (linearization != INPAR::FLUID::fixed_point_like)
      FOUR_C_THROW(
          "Only a fixed-point-like iteration scheme is enabled for monolithic low-Mach-number "
          "solver, for the time being!");

    // generate proxy of scatra dof set to be used by fluid field
    Teuchos::RCP<CORE::Dofsets::DofSetInterface> scatradofset =
        ScaTraField()->Discretization()->GetDofSetProxy();

    // check number of dof sets in respective fields
    if (FluidField()->Discretization()->AddDofSet(scatradofset) != 1)
      FOUR_C_THROW("Incorrect number of dof sets in fluid field!");

    // create combined map for loma problem
    std::vector<Teuchos::RCP<const Epetra_Map>> dofrowmaps;

    // insert actual (zeroth) map of the discretization: first fluid, then scatra
    {
      dofrowmaps.push_back(FluidField()->DofRowMap(0));
      const Epetra_Map* dofrowmapscatra = (ScaTraField()->Discretization())->DofRowMap(0);
      dofrowmaps.push_back(Teuchos::rcp(dofrowmapscatra, false));
    }

    // check existence of elements
    if (dofrowmaps[0]->NumGlobalElements() == 0) FOUR_C_THROW("No fluid elements!");
    if (dofrowmaps[1]->NumGlobalElements() == 0) FOUR_C_THROW("No scatra elements!");

    Teuchos::RCP<Epetra_Map> fullmap = CORE::LINALG::MultiMapExtractor::MergeMaps(dofrowmaps);

    // full loma block dofrowmap
    lomablockdofrowmap_.Setup(*fullmap, dofrowmaps);

    // get solver number used for LOMA solver
    const int linsolvernumber = probdyn_.get<int>("LINEAR_SOLVER");
    // check if LOMA solvers has a valid number
    if (linsolvernumber == (-1))
      FOUR_C_THROW(
          "no linear solver defined for LOMA. Please set LINEAR_SOLVER in LOMA CONTROL to a valid "
          "number! This solver has to be an iterative solver with BGS2x2 block preconditioner.");

    // create loma solver
    // get solver parameter list of linear LOMA solver
    const Teuchos::ParameterList& lomasolverparams =
        GLOBAL::Problem::Instance()->SolverParams(linsolvernumber);

    const auto solvertype =
        Teuchos::getIntegralValue<CORE::LINEAR_SOLVER::SolverType>(lomasolverparams, "SOLVER");

    if (solvertype != CORE::LINEAR_SOLVER::SolverType::belos)
      FOUR_C_THROW(
          "SOLVER %i is not valid for LOMA. It has to be an iterative Solver (with BGS2x2 block "
          "preconditioner)",
          linsolvernumber);

    const auto azprectype = Teuchos::getIntegralValue<CORE::LINEAR_SOLVER::PreconditionerType>(
        lomasolverparams, "AZPREC");
    if (azprectype != CORE::LINEAR_SOLVER::PreconditionerType::block_gauss_seidel_2x2)
      FOUR_C_THROW(
          "SOLVER %i is not valid for LOMA. It has to be an iterative Solver with BGS2x2 block "
          "preconditioner",
          linsolvernumber);

    // use loma solver object
    lomasolver_ = Teuchos::rcp(
        new CORE::LINALG::Solver(lomasolverparams, FluidField()->Discretization()->Comm()));

    // todo extract ScalarTransportFluidSolver
    const int fluidsolver = fluiddyn.get<int>("LINEAR_SOLVER");
    if (fluidsolver == (-1))
      FOUR_C_THROW(
          "no linear solver defined for fluid LOMA (inflow) problem. Please set LINEAR_SOLVER in "
          "FLUID DYNAMIC to a valid number! This solver block is used for the primary variables "
          "(Inverse1 block) within BGS2x2 preconditioner.");

    lomasolver_->PutSolverParamsToSubParams(
        "Inverse1", GLOBAL::Problem::Instance()->SolverParams(fluidsolver));

    // get linear solver id from SCALAR TRANSPORT DYNAMIC
    const Teuchos::ParameterList& scatradyn =
        GLOBAL::Problem::Instance()->ScalarTransportDynamicParams();
    const int scalartransportsolvernumber = scatradyn.get<int>("LINEAR_SOLVER");
    if (scalartransportsolvernumber == (-1))
      FOUR_C_THROW(
          "no linear solver defined for LOMA problem. Please set LINEAR_SOLVER in SCALAR TRANSPORT "
          "DYNAMIC to a valid number! This solver block is used for the secondary variables "
          "(Inverse2 block) within BGS2x2 preconditioner.");

    lomasolver_->PutSolverParamsToSubParams(
        "Inverse2", GLOBAL::Problem::Instance()->SolverParams(scalartransportsolvernumber));

    FluidField()->Discretization()->ComputeNullSpaceIfNecessary(
        lomasolver_->Params().sublist("Inverse1"));
    ScaTraField()->Discretization()->ComputeNullSpaceIfNecessary(
        lomasolver_->Params().sublist("Inverse2"));

    // create loma block matrix
    lomasystemmatrix_ =
        Teuchos::rcp(new CORE::LINALG::BlockSparseMatrix<CORE::LINALG::DefaultBlockMatrixStrategy>(
            lomablockdofrowmap_, lomablockdofrowmap_, 135, false, true));

    // create loma rhs vector
    lomarhs_ = Teuchos::rcp(new Epetra_Vector(*lomablockdofrowmap_.FullMap(), true));

    // create loma increment vector
    lomaincrement_ = Teuchos::rcp(new Epetra_Vector(*lomablockdofrowmap_.FullMap(), true));

    // create vector of zeros for enforcing zero Dirichlet boundary conditions
    zeros_ = Teuchos::rcp(new Epetra_Vector(*lomablockdofrowmap_.FullMap(), true));

    // create combined Dirichlet boundary condition map
    const Teuchos::RCP<const Epetra_Map> fdbcmap = FluidField()->GetDBCMapExtractor()->CondMap();
    const Teuchos::RCP<const Epetra_Map> sdbcmap = ScaTraField()->DirichMaps()->CondMap();
    lomadbcmap_ = CORE::LINALG::MergeMap(fdbcmap, sdbcmap, false);
  }

  return;
}



/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::TimeLoop()
{
  CheckIsInit();
  CheckIsSetup();

  // do initial calculations
  // if and only if it is the first time step
  // do not do initial calculations after restarts
  if (Step() == 0 or (turbinflow_ and Step() == numinflowsteps_))
    InitialCalculations();
  else
    // set scalar field and thermodynamic pressure for evaluation of
    // Neumann boundary conditions in FLUID at beginning of first time step
    FluidField()->SetScalarFields(ScaTraField()->Phinp(),
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressNp(),
        Teuchos::null, ScaTraField()->Discretization());

  // time loop
  while (NotFinished())
  {
    IncrementTimeAndStep();

    // prepare time step
    PrepareTimeStep();

    // do outer iteration loop for particular type of algorithm
    if (monolithic_)
      MonoLoop();
    else
      OuterLoop();

    // update for next time step
    TimeUpdate();

    // write output to screen and files
    Output();

  }  // time loop

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::InitialCalculations()
{
  // set initial velocity field for evaluation of initial scalar time
  // derivative in SCATRA
  ScaTraField()->SetVelocityField(
      FluidField()->Velnp(), Teuchos::null, Teuchos::null, FluidField()->FsVel());

  // set initial value of thermodynamic pressure in SCATRA
  Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->SetInitialThermPressure();

  // energy conservation: compute initial time derivative of therm. pressure
  // mass conservation: compute initial mass (initial time deriv. assumed zero)
  if (consthermpress_ == "No_energy")
    Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())
        ->ComputeInitialThermPressureDeriv();
  else if (consthermpress_ == "No_mass")
    Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ComputeInitialMass();

  // set initial scalar field and thermodynamic pressure for evaluation of
  // Neumann boundary conditions in FLUID at beginning of first time step
  FluidField()->SetScalarFields(ScaTraField()->Phinp(),
      Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressNp(),
      Teuchos::null, ScaTraField()->Discretization());

  // write initial fields
  // Output();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::PrepareTimeStep()
{
  CheckIsInit();
  CheckIsSetup();

  // prepare scalar transport time step
  // (+ computation of initial scalar time derivative in first time step)
  ScaTraField()->PrepareTimeStep();

  // predict thermodynamic pressure and time derivative
  // (if not constant or based on mass conservation)
  if (consthermpress_ == "No_energy")
    Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->PredictThermPressure();

  // prepare fluid time step, among other things, predict velocity field
  FluidField()->PrepareTimeStep();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::OuterLoop()
{
  CheckIsInit();
  CheckIsSetup();

  int itnum = 0;
  bool stopnonliniter = false;

  if (Comm().MyPID() == 0)
  {
    std::cout << "\n****************************************\n          OUTER ITERATION "
                 "LOOP\n****************************************\n";

    printf("TIME: %11.4E/%11.4E  DT = %11.4E  %s  STEP = %4d/%4d\n", Time(), maxtime_, dt_,
        ScaTraField()->MethodTitle().c_str(), Step(), stepmax_);
  }

  //  // maximum number of iterations tolerance for outer iteration
  //  // currently default for turbulent channel flow: only one iteration before sampling
  //  if (special_flow_ == "loma_channel_flow_of_height_2" && Step() < samstart_ )
  //       itmax_ = 1;
  //  else itmax_ = itmaxpre_;

  // maximum number of iterations tolerance for outer iteration
  // reduced number of iterations for turbulent flow: only before sampling
  if (special_flow_ != "no" && Step() < samstart_)
  {
    itmax_ = itmaxbs_;
    if (Comm().MyPID() == 0 and (Step() == 1 or (turbinflow_ and Step() == (numinflowsteps_ + 1))))
    {
      std::cout << "\n+----------------------------------------------------------------------------"
                   "----------------+"
                << std::endl;
      std::cout << "Special turbulent variable-density flow: reduced number of iterations before "
                   "sampling: "
                << itmax_ << std::endl;
      std::cout << "+------------------------------------------------------------------------------"
                   "--------------+\n"
                << std::endl;
    }
  }
  else
  {
    itmax_ = itmaxpre_;
    if (Comm().MyPID() == 0 and special_flow_ != "no" and Step() == samstart_)
    {
      std::cout << "\n+----------------------------------------------------------------------------"
                   "----------------+"
                << std::endl;
      std::cout << "Special turbulent variable-density flow: maximum number of iterations allowed: "
                << itmax_ << std::endl;
      std::cout << "+------------------------------------------------------------------------------"
                   "--------------+\n"
                << std::endl;
    }
  }

  // evaluate fluid predictor step (currently not performed)
  // FluidField()->Predictor();

  // set fluid values required in scatra
  SetFluidValuesInScaTra();

  // initially solve scalar transport equation
  // (values for intermediate time steps were calculated at the end of PerpareTimeStep)
  if (Comm().MyPID() == 0)
    std::cout << "\n****************************************\n        SCALAR TRANSPORT "
                 "SOLVER\n****************************************\n";
  ScaTraField()->Solve();

  while (stopnonliniter == false)
  {
    itnum++;

    // in case of non-constant thermodynamic pressure: compute
    // (either based on energy conservation or based on mass conservation)
    if (consthermpress_ == "No_energy")
      Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ComputeThermPressure();
    else if (consthermpress_ == "No_mass")
      Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())
          ->ComputeThermPressureFromMassCons();

    // set scatra values required in fluid
    SetScaTraValuesInFluid();

    // solve low-Mach-number flow equations
    if (Comm().MyPID() == 0)
      std::cout << "\n****************************************\n              FLUID "
                   "SOLVER\n****************************************\n";
    FluidField()->Solve();

    // set fluid values required in scatra
    SetFluidValuesInScaTra();

    // solve scalar transport equation
    if (Comm().MyPID() == 0)
      std::cout << "\n****************************************\n        SCALAR TRANSPORT "
                   "SOLVER\n****************************************\n";
    ScaTraField()->Solve();

    // check convergence and stop iteration loop if convergence is achieved
    stopnonliniter = ConvergenceCheck(itnum);
  }

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::MonoLoop()
{
  int itnum = 0;
  bool stopnonliniter = false;

  if (Comm().MyPID() == 0)
  {
    std::cout << "\n****************************************\n       MONOLITHIC ITERATION "
                 "LOOP\n****************************************\n";

    printf("TIME: %11.4E/%11.4E  DT = %11.4E  %s  STEP = %4d/%4d\n", Time(), maxtime_, dt_,
        ScaTraField()->MethodTitle().c_str(), Step(), stepmax_);
  }

  // maximum number of iterations tolerance for monolithic iteration
  // currently default for turbulent channel flow: only one iteration before sampling
  if (special_flow_ == "loma_channel_flow_of_height_2" && Step() < samstart_)
    itmax_ = 1;
  else
    itmax_ = itmaxpre_;

  // evaluate fluid predictor step (currently not performed)
  // FluidField()->Predictor();

  while (stopnonliniter == false)
  {
    itnum++;

    // set fluid values required in scatra
    SetFluidValuesInScaTra();

    // in case of non-constant thermodynamic pressure: compute
    // (either based on energy conservation or based on mass conservation)
    if (consthermpress_ == "No_energy")
      Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ComputeThermPressure();
    else if (consthermpress_ == "No_mass")
      Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())
          ->ComputeThermPressureFromMassCons();

    // set scatra values required in fluid
    SetScaTraValuesInFluid();

    // preparatives for scalar transport and fluid solver
    ScaTraField()->PrepareLinearSolve();
    FluidField()->PrepareSolve();

    // set up matrix and right-hand-side for monolithic low-Mach-number system
    SetupMonoLomaMatrix();
    SetupMonoLomaRHS();

    // solve monolithic low-Mach-number system
    MonoLomaSystemSolve();

    // update for next iteration step
    IterUpdate();

    // check convergence and stop iteration loop if convergence is achieved
    stopnonliniter = ConvergenceCheck(itnum);
  }

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::SetFluidValuesInScaTra()
{
  // set respective field vectors for velocity/pressure, acceleration
  // and discretization based on time-integration scheme
  switch (FluidField()->TimIntScheme())
  {
    case INPAR::FLUID::timeint_afgenalpha:
    {
      ScaTraField()->SetVelocityField(
          FluidField()->Velaf(), FluidField()->Accam(), Teuchos::null, FluidField()->FsVel(), true);
    }
    break;
    case INPAR::FLUID::timeint_one_step_theta:
    case INPAR::FLUID::timeint_bdf2:
    {
      ScaTraField()->SetVelocityField(
          FluidField()->Velnp(), FluidField()->Hist(), Teuchos::null, FluidField()->FsVel(), true);
    }
    break;
    default:
      FOUR_C_THROW("Time integration scheme not supported");
      break;
  }
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::SetScaTraValuesInFluid()
{
  // set scalar and thermodynamic pressure values as well as time
  // derivatives and discretization based on time-integration scheme
  switch (FluidField()->TimIntScheme())
  {
    case INPAR::FLUID::timeint_afgenalpha:
    {
      if (FluidField()->PhysicalType() == INPAR::FLUID::tempdepwater)
        FluidField()->SetIterScalarFields(ScaTraField()->Phiaf(), ScaTraField()->Phiam(),
            ScaTraField()->Phidtam(), ScaTraField()->Discretization());
      else
        FluidField()->SetLomaIterScalarFields(ScaTraField()->Phiaf(), ScaTraField()->Phiam(),
            ScaTraField()->Phidtam(), ScaTraField()->FsPhi(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressAf(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressAm(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtAf(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtAm(),
            ScaTraField()->Discretization());
    }
    break;
    case INPAR::FLUID::timeint_one_step_theta:
    {
      if (FluidField()->PhysicalType() == INPAR::FLUID::tempdepwater)
        FluidField()->SetIterScalarFields(ScaTraField()->Phinp(), ScaTraField()->Phin(),
            ScaTraField()->Phidtnp(), ScaTraField()->Discretization());
      else
        FluidField()->SetLomaIterScalarFields(ScaTraField()->Phinp(), ScaTraField()->Phin(),
            ScaTraField()->Phidtnp(), ScaTraField()->FsPhi(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressNp(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressN(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtNp(),
            Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtNp(),
            ScaTraField()->Discretization());
    }
    break;
    default:
      FOUR_C_THROW("Time integration scheme not supported");
      break;
  }
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::SetupMonoLomaMatrix()
{
  // set loma block matrix to zero
  lomasystemmatrix_->Zero();

  //----------------------------------------------------------------------
  // 1st diagonal block (upper left): fluid weighting - fluid solution
  //----------------------------------------------------------------------
  // get matrix block
  Teuchos::RCP<CORE::LINALG::SparseMatrix> mat_ff = FluidField()->SystemMatrix();

  // uncomplete matrix block (appears to be required in certain cases)
  mat_ff->UnComplete();

  // assign matrix block
  lomasystemmatrix_->Assign(0, 0, CORE::LINALG::View, *mat_ff);

  //----------------------------------------------------------------------
  // 2nd diagonal block (lower right): scatra weighting - scatra solution
  //----------------------------------------------------------------------
  // get matrix block
  Teuchos::RCP<CORE::LINALG::SparseMatrix> mat_ss = ScaTraField()->SystemMatrix();

  // uncomplete matrix block (appears to be required in certain cases)
  mat_ss->UnComplete();

  // assign matrix block
  lomasystemmatrix_->Assign(1, 1, CORE::LINALG::View, *mat_ss);

  // complete loma block matrix
  lomasystemmatrix_->Complete();

  //----------------------------------------------------------------------
  // 1st off-diagonal block (upper right): fluid weighting - scatra solution
  //----------------------------------------------------------------------
  // create matrix block
  Teuchos::RCP<CORE::LINALG::SparseMatrix> mat_fs = Teuchos::null;
  mat_fs = Teuchos::rcp(new CORE::LINALG::SparseMatrix(
      *(FluidField()->Discretization()->DofRowMap(0)), 27, true, true));

  // evaluate loma off-diagonal matrix block in fluid
  EvaluateLomaODBlockMatFluid(mat_fs);

  // uncomplete matrix block (appears to be required in certain cases)
  mat_fs->UnComplete();

  // assign matrix block
  lomasystemmatrix_->Assign(0, 1, CORE::LINALG::View, *mat_fs);

  //----------------------------------------------------------------------
  // 2nd off-diagonal block (lower left): scatra weighting - fluid solution
  //----------------------------------------------------------------------
  // create matrix block
  Teuchos::RCP<CORE::LINALG::SparseMatrix> mat_sf = Teuchos::null;
  mat_sf = Teuchos::rcp(new CORE::LINALG::SparseMatrix(
      *(ScaTraField()->Discretization()->DofRowMap(0)), 108, true, true));

  // evaluate loma off-diagonal matrix block in scatra
  // (for present fixed-point-like iteration: no entries)
  // EvaluateLomaODBlockMatScaTra(mat_sf);

  // uncomplete matrix block (appears to be required in certain cases)
  mat_sf->UnComplete();

  // assign matrix block
  lomasystemmatrix_->Assign(1, 0, CORE::LINALG::View, *mat_sf);

  // complete loma block matrix
  lomasystemmatrix_->Complete();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::EvaluateLomaODBlockMatFluid(Teuchos::RCP<CORE::LINALG::SparseMatrix> mat_fs)
{
  // create parameters for fluid discretization
  Teuchos::ParameterList fparams;

  // set action type
  fparams.set<int>("action", FLD::calc_loma_mono_odblock);

  // set general vector values needed by elements
  FluidField()->Discretization()->ClearState();
  FluidField()->Discretization()->SetState(0, "hist", FluidField()->Hist());
  FluidField()->Discretization()->SetState(0, "accam", FluidField()->Accam());
  FluidField()->Discretization()->SetState(0, "scaaf", FluidField()->Scaaf());
  FluidField()->Discretization()->SetState(0, "scaam", FluidField()->Scaam());

  // set time-integration-scheme-specific element parameters and vector values
  if (FluidField()->TimIntScheme() == INPAR::FLUID::timeint_afgenalpha)
  {
    // set thermodynamic pressures
    fparams.set("thermpress at n+alpha_F/n+1",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressAf());
    fparams.set("thermpress at n+alpha_M/n",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressAm());
    fparams.set("thermpressderiv at n+alpha_F/n+1",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtAf());
    fparams.set("thermpressderiv at n+alpha_M/n+1",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtAm());

    // set velocity vector
    FluidField()->Discretization()->SetState(0, "velaf", FluidField()->Velaf());
  }
  else if (FluidField()->TimIntScheme() == INPAR::FLUID::timeint_one_step_theta)
  {
    // set thermodynamic pressures
    fparams.set("thermpress at n+alpha_F/n+1",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressNp());
    fparams.set("thermpress at n+alpha_M/n",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressN());
    fparams.set("thermpressderiv at n+alpha_F/n+1",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtNp());
    fparams.set("thermpressderiv at n+alpha_M/n+1",
        Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressDtNp());

    // set velocity vector
    FluidField()->Discretization()->SetState(0, "velaf", FluidField()->Velnp());
  }
  else
    FOUR_C_THROW("Time integration scheme not supported");

  // build specific assemble strategy for this off-diagonal matrix block,
  // which is assembled in fluid solver
  // fluid dof set = 0, scatra dof set = 1
  DRT::AssembleStrategy fluidstrategy(0,  // rows: fluid dof set
      1,                                  // columns: scatra dof set
      mat_fs, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);

  // evaluate off-diagonal matrix block entries for fluid element
  FluidField()->Discretization()->Evaluate(fparams, fluidstrategy);
  FluidField()->Discretization()->ClearState();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::SetupMonoLomaRHS()
{
  // define fluid and scatra residual vectors
  Teuchos::RCP<const Epetra_Vector> fluidres = FluidField()->RHS();
  Teuchos::RCP<const Epetra_Vector> scatrares = ScaTraField()->Residual();

  // insert fluid and scatra residual vectors into loma residual vector
  lomablockdofrowmap_.InsertVector(*fluidres, 0, *lomarhs_);
  lomablockdofrowmap_.InsertVector(*scatrares, 1, *lomarhs_);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::MonoLomaSystemSolve()
{
  CheckIsInit();
  CheckIsSetup();

  // set incremental solution vector to zero
  lomaincrement_->PutScalar(0.0);

  // apply Dirichlet boundary conditions to system
  CORE::LINALG::ApplyDirichletToSystem(
      *lomasystemmatrix_, *lomaincrement_, *lomarhs_, *zeros_, *lomadbcmap_);

  // solve monolithic low-Mach-number system
  CORE::LINALG::SolverParams solver_params;
  solver_params.refactor = true;
  solver_params.reset = true;
  lomasolver_->Solve(lomasystemmatrix_->EpetraOperator(), lomaincrement_, lomarhs_, solver_params);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::IterUpdate()
{
  // define incremental fluid and scatra solution vectors
  Teuchos::RCP<const Epetra_Vector> incfluid;
  Teuchos::RCP<const Epetra_Vector> incscatra;

  // extract incremental fluid and scatra solution vectors
  // from incremental low-Mach-number solution vector
  incfluid = lomablockdofrowmap_.ExtractVector(lomaincrement_, 0);
  incscatra = lomablockdofrowmap_.ExtractVector(lomaincrement_, 1);

  // add incremental fluid and scatra solution vectors to
  // respective solution vectors from last iteration step
  FluidField()->IterUpdate(incfluid);
  ScaTraField()->UpdateIter(incscatra);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool LOMA::Algorithm::ConvergenceCheck(int itnum)
{
  // define flags for fluid and scatra convergence check
  bool fluidstopnonliniter = false;
  bool scatrastopnonliniter = false;

  // fluid convergence check
  if (Comm().MyPID() == 0)
  {
    std::cout << "\n****************************************\n  CONVERGENCE CHECK FOR ITERATION "
                 "STEP\n****************************************\n";
    std::cout << "\n****************************************\n              FLUID "
                 "CHECK\n****************************************\n";
  }
  fluidstopnonliniter =
      FluidField()->ConvergenceCheck(itnum, itmax_, ittol_, ittol_, ittol_, ittol_);

  // scatra convergence check
  if (Comm().MyPID() == 0)
    std::cout << "\n****************************************\n         SCALAR TRANSPORT "
                 "CHECK\n****************************************\n";
  scatrastopnonliniter = Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())
                             ->ConvergenceCheck(itnum, itmax_, ittol_);

  if (fluidstopnonliniter == true and scatrastopnonliniter == true)
    return true;
  else
    return false;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::TimeUpdate()
{
  // update scalar
  ScaTraField()->Update();

  // in case of non-constant thermodynamic pressure: update
  if (consthermpress_ == "No_energy" or consthermpress_ == "No_mass")
    Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->UpdateThermPressure();

  // update fluid
  FluidField()->Update();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::Output()
{
  // set scalar and thermodynamic pressure at n+1 and SCATRA trueresidual
  // for statistical evaluation and evaluation of Neumann boundary
  // conditions at the beginning of the subsequent time step
  FluidField()->SetScalarFields(ScaTraField()->Phinp(),
      Teuchos::rcp_dynamic_cast<SCATRA::ScaTraTimIntLoma>(ScaTraField())->ThermPressNp(),
      ScaTraField()->TrueResidual(), ScaTraField()->Discretization());

  // Note: The order is important here! Herein, control file entries are
  // written, defining the order in which the filters handle the
  // discretizations, which in turn defines the dof number ordering of the
  // discretizations.
  FluidField()->StatisticsAndOutput();

  ScaTraField()->CheckAndWriteOutputAndRestart();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void LOMA::Algorithm::ReadInflowRestart(int restart)
{
  // in case a inflow generation in the inflow section has been performed,
  // there are not any scatra results available and the initial field is used
  // caution: if AVM3Preparation is called ,e.g., for multifractal subgrid-scale
  //          modeling the physical parameters (dens, visc, diff) are required
  //          to obtain non-zero values which otherwise cause troubles when dividing by them
  //          we have to set the temperature field here
  // set initial scalar field
  FluidField()->SetScalarFields(
      ScaTraField()->Phinp(), 0.0, Teuchos::null, ScaTraField()->Discretization());
  FluidField()->ReadRestart(restart);
  // as ReadRestart is only called for the FluidField
  // time and step have not been set in the superior class and the ScaTraField
  SetTimeStep(FluidField()->Time(), FluidField()->Step());
  ScaTraField()->SetTimeStep(FluidField()->Time(), FluidField()->Step());
  return;
}

FOUR_C_NAMESPACE_CLOSE