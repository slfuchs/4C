/*!----------------------------------------------------------------------
\file fluid_dyn_nln_drt.cpp
\brief Main control routine for all fluid (in)stationary solvers,

     including instationary solvers based on

     o one-step-theta time-integration scheme

     o two-step BDF2 time-integration scheme
       (with potential one-step-theta start algorithm)

     o generalized-alpha time-integration scheme

     and stationary solver.

<pre>
Maintainer: Peter Gamnitzer
            gamnitzer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15235
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <ctime>
#include <cstdlib>
#include <iostream>

#include <Teuchos_TimeMonitor.hpp>
#include <Teuchos_StandardParameterEntryValidators.hpp>

#ifdef PARALLEL
#include <mpi.h>
#endif

#include "fluid_dyn_nln_drt.H"
#include "fluidimplicitintegration.H"
#include "fluid_genalpha_integration.H"
#include "../drt_lib/drt_resulttest.H"
#include "fluidresulttest.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_validparameters.H"

/*----------------------------------------------------------------------*
  |                                                       m.gee 06/01    |
  | general problem data                                                 |
  | global variable GENPROB genprob is defined in global_control.c       |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;

/*!----------------------------------------------------------------------
\brief file pointers

<pre>                                                         m.gee 8/00
This structure struct _FILES allfiles is defined in input_control_global.c
and the type is in standardtypes.h
It holds all file pointers and some variables needed for the FRSYSTEM
</pre>
*----------------------------------------------------------------------*/
extern struct _FILES  allfiles;

/*----------------------------------------------------------------------*
 | global variable *solv, vector of length numfld of structures SOLVAR  |
 | defined in solver_control.c                                          |
 |                                                                      |
 |                                                       m.gee 11/00    |
 *----------------------------------------------------------------------*/
extern struct _SOLVAR  *solv;



/*----------------------------------------------------------------------*
 * Main control routine for fluid including various solvers:
 *
 *        o instationary one-step-theta
 *        o instationary BDF2
 *        o instationary generalized-alpha
 *        o stationary
 *
 *----------------------------------------------------------------------*/
void dyn_fluid_drt()
{

  // -------------------------------------------------------------------
  // access the discretization
  // -------------------------------------------------------------------
  RefCountPtr<DRT::Discretization> actdis = null;
  actdis = DRT::Problem::Instance()->Dis(genprob.numff,0);

  // -------------------------------------------------------------------
  // set degrees of freedom in the discretization
  // -------------------------------------------------------------------
  if (!actdis->Filled()) actdis->FillComplete();

  // -------------------------------------------------------------------
  // context for output and restart
  // -------------------------------------------------------------------
  IO::DiscretizationWriter output(actdis);
  output.WriteMesh(0,0.0);

  // -------------------------------------------------------------------
  // set some pointers and variables
  // -------------------------------------------------------------------
  SOLVAR        *actsolv  = &solv[0];

  const Teuchos::ParameterList& probtype = DRT::Problem::Instance()->ProblemTypeParams();
  const Teuchos::ParameterList& probsize = DRT::Problem::Instance()->ProblemSizeParams();
  const Teuchos::ParameterList& ioflags  = DRT::Problem::Instance()->IOParams();
  const Teuchos::ParameterList& fdyn     = DRT::Problem::Instance()->FluidDynamicParams();

  if (actdis->Comm().MyPID()==0)
    DRT::INPUT::PrintDefaultParameters(std::cout, fdyn);

  // -------------------------------------------------------------------
  // create a solver
  // -------------------------------------------------------------------
  RCP<ParameterList> solveparams = rcp(new ParameterList());
  LINALG::Solver solver(solveparams,actdis->Comm(),allfiles.out_err);
  solver.TranslateSolverParameters(*solveparams,actsolv);
  actdis->ComputeNullSpaceIfNecessary(*solveparams);

  // -------------------------------------------------------------------
  // create a second solver for SIMPLER preconditioner if chosen from input
  // -------------------------------------------------------------------
  if (getIntegralValue<int>(fdyn,"SIMPLER"))
  {
    ParameterList& p = solveparams->sublist("SIMPLER");
    RCP<ParameterList> params = rcp(&p,false);
    LINALG::Solver s(params,actdis->Comm(),allfiles.out_err);
    s.TranslateSolverParameters(*params,&solv[genprob.numfld]);
  }

  // -------------------------------------------------------------------
  // set parameters in list required for all schemes
  // -------------------------------------------------------------------
  ParameterList fluidtimeparams;

  fluidtimeparams.set<int>("Simple Preconditioner",Teuchos::getIntegralValue<int>(fdyn,"SIMPLER"));

  // -------------------------------------- number of degrees of freedom
  // number of degrees of freedom
  fluidtimeparams.set<int>              ("number of velocity degrees of freedom" ,probsize.get<int>("DIM"));

  // ---------------------------- low-Mach-number or incompressible flow
  fluidtimeparams.set<string>("low-Mach-number solver"   ,fdyn.get<string>("LOWMACH"));

  // ------------------------------------------------ basic scheme, i.e.
  // --------------------- solving nonlinear or linearised flow equation
  fluidtimeparams.set<int>("type of nonlinear solve" ,
                     Teuchos::getIntegralValue<int>(fdyn,"DYNAMICTYP"));

  // -------------------------------------------------- time integration
  // the default time step size
  fluidtimeparams.set<double>           ("time step size"           ,fdyn.get<double>("TIMESTEP"));
  // maximum simulation time
  fluidtimeparams.set<double>           ("total time"               ,fdyn.get<double>("MAXTIME"));
  // maximum number of timesteps
  fluidtimeparams.set<int>              ("max number timesteps"     ,fdyn.get<int>("NUMSTEP"));

  // ---------------------------------------------- nonlinear iteration
  // set linearisation scheme
  fluidtimeparams.set<string>          ("Linearisation",fdyn.get<string>("NONLINITER"));
  // maximum number of nonlinear iteration steps
  fluidtimeparams.set<int>             ("max nonlin iter steps"     ,fdyn.get<int>("ITEMAX"));
  // stop nonlinear iteration when both incr-norms are below this bound
  fluidtimeparams.set<double>          ("tolerance for nonlin iter" ,fdyn.get<double>("CONVTOL"));
  // set convergence check
  fluidtimeparams.set<string>          ("CONVCHECK"  ,fdyn.get<string>("CONVCHECK"));
  // set adaptoive linear solver tolerance
  fluidtimeparams.set<bool>            ("ADAPTCONV",getIntegralValue<int>(fdyn,"ADAPTCONV")==1);
  fluidtimeparams.set<double>          ("ADAPTCONV_BETTER",fdyn.get<double>("ADAPTCONV_BETTER"));

  // ----------------------------------------------- restart and output
  // restart
  fluidtimeparams.set                  ("write restart every"       ,fdyn.get<int>("RESTARTEVRY"));
  // solution output
  fluidtimeparams.set                  ("write solution every"      ,fdyn.get<int>("UPRES"));
  // flag for writing stresses
  fluidtimeparams.set                  ("write stresses"            ,Teuchos::getIntegralValue<int>(ioflags,"FLUID_STRESS"));
  // ---------------------------------------------------- lift and drag
  fluidtimeparams.set<int>("liftdrag",Teuchos::getIntegralValue<int>(fdyn,"LIFTDRAG"));

  // -----------evaluate error for test flows with analytical solutions
  int init = Teuchos::getIntegralValue<int>(fdyn,"INITIALFIELD");
  fluidtimeparams.set                  ("eval err for analyt sol"   ,init);

  // ---------------------------- fine-scale subgrid viscosity approach
  fluidtimeparams.set<string>           ("fs subgrid viscosity"   ,fdyn.get<string>("FSSUGRVISC"));

  // -----------------------sublist containing stabilization parameters
  fluidtimeparams.sublist("STABILIZATION")=fdyn.sublist("STABILIZATION");

  // --------------------------sublist containing turbulence parameters
  {
    fluidtimeparams.sublist("TURBULENCE MODEL")=fdyn.sublist("TURBULENCE MODEL");

    fluidtimeparams.sublist("TURBULENCE MODEL").set<string>("statistics outfile",allfiles.outputfile_kenner);
  }

  // -------------------------------------------------------------------
  // additional parameters and algorithm call depending on respective
  // time-integration (or stationary) scheme
  // -------------------------------------------------------------------
  FLUID_TIMEINTTYPE iop = Teuchos::getIntegralValue<FLUID_TIMEINTTYPE>(fdyn,"TIMEINTEGR");
  if(iop == timeint_stationary or
     iop == timeint_one_step_theta or
     iop == timeint_bdf2
    )
  {
    // -----------------------------------------------------------------
    // set additional parameters in list for OST/BDF2/stationary scheme
    // -----------------------------------------------------------------
    // type of time-integration (or stationary) scheme
    fluidtimeparams.set<FLUID_TIMEINTTYPE>("time int algo",iop);
    // parameter theta for time-integration schemes
    fluidtimeparams.set<double>           ("theta"                    ,fdyn.get<double>("THETA"));
    // number of steps for potential start algorithm
    fluidtimeparams.set<int>              ("number of start steps"    ,fdyn.get<int>("NUMSTASTEPS"));
    // parameter theta for potential start algorithm
    fluidtimeparams.set<double>           ("start theta"              ,fdyn.get<double>("START_THETA"));

    //------------------------------------------------------------------
    // create all vectors and variables associated with the time
    // integration (call the constructor);
    // the only parameter from the list required here is the number of
    // velocity degrees of freedom
    //------------------------------------------------------------------
    FLD::FluidImplicitTimeInt fluidimplicit(actdis,
                                            solver,
                                            fluidtimeparams,
                                            output);

    // initial field from restart or calculated by given function
    if (probtype.get<int>("RESTART"))
    {
      // read the restart information, set vectors and variables
      fluidimplicit.ReadRestart(probtype.get<int>("RESTART"));
    }
    else
    {
      // set initial field by given function
      if(init>0)
      {
        int startfuncno = fdyn.get<int>("STARTFUNCNO");
        if (init!=2 and init!=3)
        {
          startfuncno=-1;
        }
        fluidimplicit.SetInitialFlowField(init,startfuncno);
      }
    }

    fluidtimeparams.set<FILE*>("err file",allfiles.out_err);

    // call time-integration (or stationary) scheme
    fluidimplicit.Integrate();

    // do result test if required
    DRT::ResultTestManager testmanager(actdis->Comm());
    testmanager.AddFieldTest(rcp(new FLD::FluidResultTest(fluidimplicit)));
    testmanager.TestAll();
  }
  else if (iop == timeint_gen_alpha)
  {
    // -------------------------------------------------------------------
    // set additional parameters in list for generalized-alpha scheme
    // -------------------------------------------------------------------
#if 1
    // parameter alpha_M for for generalized-alpha scheme
    fluidtimeparams.set<double>           ("alpha_M"                  ,fdyn.get<double>("ALPHA_M"));
    // parameter alpha_F for for generalized-alpha scheme
    fluidtimeparams.set<double>           ("alpha_F"                  ,fdyn.get<double>("ALPHA_F"));
#else
    // parameter alpha_M for for generalized-alpha scheme
    fluidtimeparams.set<double>           ("alpha_M"                  ,1.-fdyn.get<double>("ALPHA_M"));
    // parameter alpha_F for for generalized-alpha scheme
    fluidtimeparams.set<double>           ("alpha_F"                  ,1.-fdyn.get<double>("ALPHA_F"));
#endif

    fluidtimeparams.set<double>           ("gamma"                    ,fdyn.get<double>("GAMMA"));

    // create all vectors and variables associated with the time
    // integration (call the constructor);
    // the only parameter from the list required here is the number of
    // velocity degrees of freedom
    //------------------------------------------------------------------
    FLD::FluidGenAlphaIntegration genalphaint(actdis,
                                              solver,
                                              fluidtimeparams,
                                              output,
                                              false);


    // initial field from restart or calculated by given function
    if (probtype.get<int>("RESTART"))
    {
      // read the restart information, set vectors and variables
      genalphaint.ReadRestart(probtype.get<int>("RESTART"));
    }
    else
    {
      // set initial field by given function
      if(init>0)
      {
        int startfuncno = fdyn.get<int>("STARTFUNCNO");
        if (init!=2 and init!=3)
        {
          startfuncno=-1;
        }
        genalphaint.SetInitialFlowField(init,startfuncno);
      }
    }

    // call generalized-alpha time-integration scheme
    genalphaint.GenAlphaTimeloop();

    // do result test if required
    DRT::ResultTestManager testmanager(actdis->Comm());
    testmanager.AddFieldTest(rcp(new FLD::FluidResultTest(genalphaint)));
    testmanager.TestAll();

  }
  else
  {
    dserror("Unknown solver type for drt_fluid");
  }

  return;

} // end of dyn_fluid_drt()

#endif  // #ifdef CCADISCRET
