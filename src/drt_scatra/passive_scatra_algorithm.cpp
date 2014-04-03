/*----------------------------------------------------------------------*/
/*!
\file passive_scatra_algorithm.cpp

\brief Transport of passive scalars in Navier-Stokes velocity field

<pre>
Maintainer: Andreas Ehrl
            ehrl@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15252
</pre>
*/
/*----------------------------------------------------------------------*/

#include "passive_scatra_algorithm.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
SCATRA::PassiveScaTraAlgorithm::PassiveScaTraAlgorithm(const Epetra_Comm& comm, const Teuchos::ParameterList& prbdyn, const std::string disname, const Teuchos::ParameterList& solverparams)
  :  ScaTraFluidCouplingAlgorithm(comm,prbdyn,false,disname,solverparams),
   samstart_(prbdyn.sublist("TURBULENCE MODEL").get<int>("SAMPLING_START")),
   samstop_(prbdyn.sublist("TURBULENCE MODEL").get<int>("SAMPLING_STOP"))
{
 // no stuff to add here at the moment
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
SCATRA::PassiveScaTraAlgorithm::~PassiveScaTraAlgorithm()
{
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::TimeLoop()
{
  // write out inital state
  // Output();

  // time loop (no-subcycling at the moment)
  while (NotFinished())
  {
    // prepare next time step
    PrepareTimeStep();

    // solve nonlinear Navier-Stokes system
    DoFluidStep();

    // solve transport (convection-diffusion) equations for passive scalar
    DoTransportStep();

    // update all field solvers
    Update();

    // write output to screen and files
    Output();

  } // time loop

return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::PrepareTimeStep()
{
  IncrementTimeAndStep();

  FluidField().PrepareTimeStep();

  // prepare time step
  /* remark: initial velocity field has been transfered to scalar transport field in constructor of
   * ScaTraFluidCouplingAlgorithm (initialvelset_ == true). Time integration schemes, such as
   * the one-step-theta scheme, are thus initialized correctly.
   */
  ScaTraField()->PrepareTimeStep();

  if (Comm().MyPID()==0)
  {
    std::cout<<"\n******************\n   TIME STEP     \n******************\n";
    std::cout<<"\nStep:   " << Step() << " / " << NStep() << "\n";
    std::cout<<"\n******************\n   FLUID SOLVER  \n******************\n";
  }

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::DoFluidStep()
{
  // solve nonlinear Navier-Stokes system

  // currently only required for forced homogeneous isotropic turbulence with
  // passive scalar transport; does nothing otherwise
  FluidField().CalcIntermediateSolution();

  FluidField().Solve();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::DoTransportStep()
{
  if (Comm().MyPID()==0)
  {
    std::cout<<"\n******************\n TRANSPORT SOLVER \n******************\n";
  }

  // transfer velocities to scalar transport field solver
  // NOTE: so far, the convective velocity is chosen to equal the fluid velocity
  //       since it is not yet clear how the grid velocity should be interpolated
  //       properly -> hence, PassiveScaTraAlgorithm does not support moving
  //       meshes yet
  switch(FluidField().TimIntScheme())
  {
  case INPAR::FLUID::timeint_npgenalpha:
  case INPAR::FLUID::timeint_afgenalpha:
  {
    ScaTraField()->SetVelocityField(
        FluidField().Velaf(),
        FluidField().Accam(),
        FluidField().Velaf(),
        FluidField().FsVel(),
        Teuchos::null,
        FluidField().Discretization());;
  }
  break;
  case INPAR::FLUID::timeint_one_step_theta:
  case INPAR::FLUID::timeint_bdf2:
  case INPAR::FLUID::timeint_stationary:
  {
    ScaTraField()->SetVelocityField(
      FluidField().Velnp(),
        FluidField().Hist(),
        FluidField().Velnp(),
        FluidField().FsVel(),
        Teuchos::null,
        FluidField().Discretization()
    );
  }
  break;
  default:
    dserror("Time integration scheme not supported");
    break;
  }

  // solve the linear convection-diffusion equation(s)
  ScaTraField()->Solve();
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::Update(const int num)
{
  FluidField().Update();
  ScaTraField()->Update(num);
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::Output()
{
  // Note: The order is important here! In here control file entries are
  // written. And these entries define the order in which the filters handle
  // the Discretizations, which in turn defines the dof number ordering of the
  // Discretizations.

  if ((Step()>=samstart_) and (Step()<=samstop_))
  {
    // if statistics for one-way coupled problems is performed, provide
    // the field for the first scalar!
    FluidField().SetScalarFields(
        ScaTraField()->Phinp(),
        0.0,
        ScaTraField()->TrueResidual(),
        ScaTraField()->Discretization(),
        0 // do statistics for FIRST dof at every node!!
    );
  }

  FluidField().StatisticsAndOutput();
  ScaTraField()->Output();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SCATRA::PassiveScaTraAlgorithm::ReadInflowRestart(int restart)
{
  // in case a inflow generation in the inflow section has been performed,
  // there are not any scatra results available and the initial field is used
  FluidField().ReadRestart(restart);
  // as ReadRestart is only called for the FluidField
  // time and step have not been set in the superior class and the ScaTraField
  SetTimeStep(FluidField().Time(),FluidField().Step());
  ScaTraField()->SetTimeStep(FluidField().Time(),FluidField().Step());
  return;
}
