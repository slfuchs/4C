/*----------------------------------------------------------------------*/
/*!
 \file poro_scatra_part_1wc.cpp

 \brief

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *----------------------------------------------------------------------*/


#include "poro_scatra_part_1wc.H"

#include "../drt_adapter/adapter_scatra_base_algorithm.H"

#include "poro_base.H"

#include "../drt_adapter/ad_str_fsiwrapper.H"
#include "../drt_adapter/ad_fld_poro.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_discret.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PORO_SCATRA_Part_1WC::DoPoroStep()
{
  //1)  solve the step problem. Methods obtained from poroelast->TimeLoop(sdynparams); --> sdynparams
  //      CUIDADO, aqui vuelve a avanzar el paso de tiempo. Hay que corregir eso.
  //2)  Newton-Raphson iteration
  //3)  calculate stresses, strains, energies
  //4)  update all single field solvers
  //5)  write output to screen and files
  poro_-> DoTimeStep();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PORO_SCATRA_Part_1WC::DoScatraStep()
{
  const Epetra_Comm& comm =
      DRT::Problem::Instance()->GetDis("structure")->Comm();

  if (comm.MyPID() == 0)
  {
    cout
        << "\n***********************\n TRANSPORT SOLVER \n***********************\n";
  }
  // -------------------------------------------------------------------
  // prepare time step
  // -------------------------------------------------------------------
  scatra_->ScaTraField().PrepareTimeStep();

  // -------------------------------------------------------------------
  //                  solve nonlinear / linear equation
  // -------------------------------------------------------------------
  scatra_->ScaTraField().Solve();

  // -------------------------------------------------------------------
  //                         update solution
  //        current solution becomes old solution of next timestep
  // -------------------------------------------------------------------
  scatra_->ScaTraField().Update();

  // -------------------------------------------------------------------
  // evaluate error for problems with analytical solution
  // -------------------------------------------------------------------
  scatra_->ScaTraField().EvaluateErrorComparedToAnalyticalSol();

  // -------------------------------------------------------------------
  //                         output of solution
  // -------------------------------------------------------------------
  scatra_->ScaTraField().Output();
}

/*----------------------------------------------------------------------*/
//prepare time step
/*----------------------------------------------------------------------*/
void POROELAST::PORO_SCATRA_Part_1WC::PrepareTimeStep()
{
  IncrementTimeAndStep();
  //PrintHeader();

  //PrepareTimeStep of single fields is called in DoPoroStep and DoScatraStep
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
POROELAST::PORO_SCATRA_Part_1WC_PoroToScatra::PORO_SCATRA_Part_1WC_PoroToScatra(const Epetra_Comm& comm,
    const Teuchos::ParameterList& timeparams)
  : PORO_SCATRA_Part_1WC(comm, timeparams)
{
  // build a proxy of the structure discretization for the scatra field
  Teuchos::RCP<DRT::DofSet> structdofset
    = poro_->StructureField()->Discretization()->GetDofSetProxy();

  // check if scatra field has 2 discretizations, so that coupling is possible
  if (scatra_->ScaTraField().Discretization()->AddDofSet(structdofset)!=1)
    dserror("unexpected dof sets in scatra field");
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PORO_SCATRA_Part_1WC_PoroToScatra::Timeloop()
{
  //InitialCalculations();

  while (NotFinished())
  {
    PrepareTimeStep();

    DoPoroStep(); // It has its own time and timestep variables, and it increments them by itself.
    SetPoroSolution();
    DoScatraStep(); // It has its own time and timestep variables, and it increments them by itself.
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
POROELAST::PORO_SCATRA_Part_1WC_ScatraToPoro::PORO_SCATRA_Part_1WC_ScatraToPoro(const Epetra_Comm& comm,
    const Teuchos::ParameterList& timeparams)
  : PORO_SCATRA_Part_1WC(comm, timeparams)
{
  // build a proxy of the scatra discretization for the structure field
  Teuchos::RCP<DRT::DofSet> scatradofset
    = scatra_->ScaTraField().Discretization()->GetDofSetProxy();

  // check if structure field has 2 discretizations, so that coupling is possible
  if (poro_->StructureField()->Discretization()->AddDofSet(scatradofset)!=1)
    dserror("unexpected dof sets in structure field");
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PORO_SCATRA_Part_1WC_ScatraToPoro::Timeloop()
{
  //InitialCalculations();

  while (NotFinished())
  {
    PrepareTimeStep();

    DoScatraStep(); // It has its own time and timestep variables, and it increments them by itself.
    SetScatraSolution();
    DoPoroStep(); // It has its own time and timestep variables, and it increments them by itself.
  }
}
