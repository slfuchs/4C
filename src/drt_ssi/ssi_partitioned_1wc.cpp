/*!------------------------------------------------------------------------------------------------*
 \file ssi_partitioned_1wc.cpp

 \brief one way coupled partitioned scalar structure interaction

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *------------------------------------------------------------------------------------------------*/

#include "ssi_partitioned_1wc.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../linalg/linalg_utils.H"

#include "../drt_adapter/ad_str_wrapper.H"
#include "../drt_adapter/adapter_scatra_base_algorithm.H"

SSI::SSI_Part1WC::SSI_Part1WC(const Epetra_Comm& comm,
    const Teuchos::ParameterList& globaltimeparams,
    const Teuchos::ParameterList& scatraparams,
    const Teuchos::ParameterList& structparams)
  : SSI_Part(comm, globaltimeparams,scatraparams,structparams)
{

}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Part1WC::DoStructStep()
{

  if (Comm().MyPID() == 0)
  {
    std::cout
        << "\n***********************\n STRUCTURE SOLVER \n***********************\n";
  }

  // solve the step problem
  structure_-> PrepareTimeStep();
  // Newton-Raphson iteration
  structure_-> Solve();
  // calculate stresses, strains, energies
  structure_-> PrepareOutput();
  // update all single field solvers
  structure_-> Update();
  // write output to files
  structure_-> Output();
  // write output to screen
  structure_->PrintStep();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Part1WC::DoScatraStep()
{

  if (Comm().MyPID() == 0)
  {
    std::cout
        << "\n***********************\n TRANSPORT SOLVER \n***********************\n";
  }

  // -------------------------------------------------------------------
  //                  solve nonlinear / linear equation
  // -------------------------------------------------------------------
  scatra_->ScaTraField()->PrepareTimeStep();

  // -------------------------------------------------------------------
  //                  solve nonlinear / linear equation
  // -------------------------------------------------------------------
  if(isscatrafromfile_){
    int diffsteps = structure_->Dt()/scatra_->ScaTraField()->Dt();
    if (scatra_->ScaTraField()->Step() % diffsteps ==0){
      scatra_->ScaTraField()->ReadRestart(scatra_->ScaTraField()->Step()); // read results from restart file
    }
  }
  else scatra_->ScaTraField()->Solve(); // really solve scatra problem


  // -------------------------------------------------------------------
  //                         update solution
  //        current solution becomes old solution of next timestep
  // -------------------------------------------------------------------
  scatra_->ScaTraField()->Update();

  // -------------------------------------------------------------------
  // evaluate error for problems with analytical solution
  // -------------------------------------------------------------------
  scatra_->ScaTraField()->EvaluateErrorComparedToAnalyticalSol();

  // -------------------------------------------------------------------
  //                         output of solution
  // -------------------------------------------------------------------
  scatra_->ScaTraField()->Output();
}

/*----------------------------------------------------------------------*/
//prepare time step
/*----------------------------------------------------------------------*/
void SSI::SSI_Part1WC::PrepareTimeStep()
{
  IncrementTimeAndStep();
  //PrintHeader();

  //PrepareTimeStep of single fields is called in DoStructStep and DoScatraStep
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
SSI::SSI_Part1WC_SolidToScatra::SSI_Part1WC_SolidToScatra(const Epetra_Comm& comm,
    const Teuchos::ParameterList& globaltimeparams,
    const Teuchos::ParameterList& scatraparams,
    const Teuchos::ParameterList& structparams)
  : SSI_Part1WC(comm, globaltimeparams, scatraparams, structparams)
{
  // build a proxy of the structure discretization for the scatra field
  Teuchos::RCP<DRT::DofSet> structdofset
    = structure_->Discretization()->GetDofSetProxy();

  // check if scatra field has 2 discretizations, so that coupling is possible
  if (scatra_->ScaTraField()->Discretization()->AddDofSet(structdofset)!=1)
    dserror("unexpected dof sets in scatra field");
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Part1WC_SolidToScatra::Timeloop()
{
  //InitialCalculations();

  if (structure_->Dt() > scatra_->ScaTraField()->Dt())
  {
    dserror("Timestepsize of scatra should be equal or bigger than solid timestep in solid to scatra interaction");
  }
  int diffsteps = scatra_->ScaTraField()->Dt()/structure_->Dt();
  while (NotFinished())
  {
    PrepareTimeStep();
    DoStructStep();  // It has its own time and timestep variables, and it increments them by itself.
    SetStructSolution();
    if (structure_->Step() % diffsteps == 0)
    {
      DoScatraStep();  // It has its own time and timestep variables, and it increments them by itself.
    }
  }

}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
SSI::SSI_Part1WC_ScatraToSolid::SSI_Part1WC_ScatraToSolid(const Epetra_Comm& comm,
    const Teuchos::ParameterList& globaltimeparams,
    const Teuchos::ParameterList& scatraparams,
    const Teuchos::ParameterList& structparams)
  : SSI_Part1WC(comm, globaltimeparams, scatraparams, structparams)
{
  // build a proxy of the scatra discretization for the structure field
  Teuchos::RCP<DRT::DofSet> scatradofset
    = scatra_->ScaTraField()->Discretization()->GetDofSetProxy();

  // check if structure field has 2 discretizations, so that coupling is possible
  if (structure_->Discretization()->AddDofSet(scatradofset)!=1)
    dserror("unexpected dof sets in structure field");

  // Flag for reading scatra result from restart file instead of computing it
  //DRT::Problem* problem = DRT::Problem::Instance();
  isscatrafromfile_ = DRT::INPUT::IntegralValue<bool>(DRT::Problem::Instance()->SSIControlParams(),"SCATRA_FROM_RESTART_FILE");

  /*
  if (isscatrafromfile_){
     std::string scatrafilename = Teuchos::getNumericStringParameter(problem->SSIControlParams()
                                                                              ,"SCATRA_FILENAME");
     Teuchos::RCP<IO::InputControl> inputscatra = Teuchos::rcp(new IO::InputControl(scatrafilename, comm));
     problem->SetInputControlFile(inputscatra);
  }
  */


}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Part1WC_ScatraToSolid::Timeloop()
{

  if (structure_->Dt() < scatra_->ScaTraField()->Dt())
  {
    dserror("Timestepsize of solid should be equal or bigger than scatra timestep in scatra to solid interaction");
  }
  int diffsteps = structure_->Dt()/scatra_->ScaTraField()->Dt();
  while (NotFinished())
  {
    PrepareTimeStep();
    DoScatraStep();  // It has its own time and timestep variables, and it increments them by itself.
    if (scatra_->ScaTraField()->Step()  % diffsteps ==0)
    {
      SetScatraSolution();
      DoStructStep();  // It has its own time and timestep variables, and it increments them by itself.
    }
  }

}
