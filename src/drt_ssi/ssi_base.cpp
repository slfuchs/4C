/*!------------------------------------------------------------------------------------------------*
 \file ssi_base.cpp

 \brief base class for all scalar structure algorithms

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *------------------------------------------------------------------------------------------------*/

#include "ssi_base.H"

#include "ssi_partitioned.H"
#include "ssi_utils.H"
#include "../drt_inpar/inpar_ssi.H"

#include "../drt_adapter/ad_str_wrapper.H"
#include "../drt_adapter/adapter_scatra_base_algorithm.H"
#include "../drt_adapter/adapter_coupling_mortar.H"

#include "../drt_lib/drt_globalproblem.H"
//for cloning
#include "../drt_lib/drt_utils_createdis.H"
#include "../drt_scatra/scatra_timint_implicit.H"
#include "../drt_scatra/scatra_utils_clonestrategy.H"
#include "../drt_scatra_ele/scatra_ele.H"

#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_mapextractor.H"

#include "../drt_particle/binning_strategy.H"

#include <Teuchos_TimeMonitor.hpp>
#include <Epetra_Time.h>


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
SSI::SSI_Base::SSI_Base(const Epetra_Comm& comm,
    const Teuchos::ParameterList& globaltimeparams,
    const Teuchos::ParameterList& scatraparams,
    const Teuchos::ParameterList& structparams):
    AlgorithmBase(comm, globaltimeparams),
    structure_(Teuchos::null),
    scatra_(Teuchos::null),
    zeros_(Teuchos::null),
    adaptermeshtying_(Teuchos::null),
    extractor_(Teuchos::null),
    matchinggrid_(true),
    boundarytransport_(false)
{
  DRT::Problem* problem = DRT::Problem::Instance();

  // get the solver number used for ScalarTransport solver
  const int linsolvernumber = scatraparams.get<int>("LINEAR_SOLVER");

  //2.- Setup discretizations.
  SetupDiscretizations(comm);

  //3.- Create the two uncoupled subproblems.
  // access the structural discretization
  Teuchos::RCP<DRT::Discretization> structdis = DRT::Problem::Instance()->GetDis("structure");

  // Set isale to false what should be the case in scatratosolid algorithm
  const INPAR::SSI::SolutionSchemeOverFields coupling
      = DRT::INPUT::IntegralValue<INPAR::SSI::SolutionSchemeOverFields>(problem->SSIControlParams(),"COUPALGO");

  bool isale = true;
  if(coupling == INPAR::SSI::ssi_OneWay_ScatraToSolid) isale = false;

  Teuchos::RCP<ADAPTER::StructureBaseAlgorithm> structure =
      Teuchos::rcp(new ADAPTER::StructureBaseAlgorithm(structparams, const_cast<Teuchos::ParameterList&>(structparams), structdis));
  structure_ = Teuchos::rcp_dynamic_cast<ADAPTER::Structure>(structure->StructureField());
  scatra_ = Teuchos::rcp(new ADAPTER::ScaTraBaseAlgorithm(scatraparams,scatraparams,problem->SolverParams(linsolvernumber),"scatra",isale));
  zeros_ = LINALG::CreateVector(*structure_->DofRowMap(), true);

}


/*----------------------------------------------------------------------*
 | read restart information for given time step (public)   vuong 01/12  |
 *----------------------------------------------------------------------*/
void SSI::SSI_Base::ReadRestart( int restart )
{

  if (restart)
  {
    scatra_->ScaTraField()->ReadRestart(restart);
    structure_->ReadRestart(restart);
    SetTimeStep(structure_->TimeOld(), restart);
  }

  return;
}

/*----------------------------------------------------------------------*
 | read restart information for given time (public)        AN, JH 10/14 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Base::ReadRestartfromTime( double restarttime )
{
  if ( restarttime > 0.0 )
  {

    int restartstructure = SSI::Utils::CheckTimeStepping(structure_->Dt(), restarttime);
    int restartscatra    = SSI::Utils::CheckTimeStepping(scatra_->ScaTraField()->Dt(), restarttime);

    scatra_->ScaTraField()->ReadRestart(restartscatra);
    structure_->ReadRestart(restartstructure);
    SetTimeStep(structure_->TimeOld(), restartstructure);

  }

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::TestResults(const Epetra_Comm& comm)
{
  DRT::Problem* problem = DRT::Problem::Instance();

  problem->AddFieldTest(structure_->CreateFieldTest());
  problem->AddFieldTest(scatra_->CreateScaTraFieldTest());
  problem->TestAll(comm);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::SetupDiscretizations(const Epetra_Comm& comm)
{
  // Scheme   : the structure discretization is received from the input. Then, an ale-scatra disc. is cloned.

  DRT::Problem* problem = DRT::Problem::Instance();

  //1.-Initialization.
  Teuchos::RCP<DRT::Discretization> structdis = problem->GetDis("structure");
  Teuchos::RCP<DRT::Discretization> scatradis = problem->GetDis("scatra");
  if(!structdis->Filled())
    structdis->FillComplete();
  if(!scatradis->Filled())
    scatradis->FillComplete();

  if (scatradis->NumGlobalNodes()==0)
  {
    // fill scatra discretization by cloning structure discretization
    DRT::UTILS::CloneDiscretization<SCATRA::ScatraFluidCloneStrategy>(structdis,scatradis);

    // set implementation type
    for(int i=0; i<scatradis->NumMyColElements(); ++i)
    {
      DRT::ELEMENTS::Transport* element = dynamic_cast<DRT::ELEMENTS::Transport*>(scatradis->lColElement(i));
      if(element == NULL)
        dserror("Invalid element type!");
      else
        element->SetImplType(DRT::INPUT::IntegralValue<INPAR::SCATRA::ImplType>(problem->SSIControlParams(),"SCATRATYPE"));
    }
  }

  else
  {
    std::map<std::string,std::string> conditions_to_copy;
    SCATRA::ScatraFluidCloneStrategy clonestrategy;
    conditions_to_copy = clonestrategy.ConditionsToCopy();
    DRT::UTILS::DiscretizationCreatorBase creator;
    creator.CopyConditions(scatradis,scatradis,conditions_to_copy);

    // redistribute discr. with help of binning strategy
    if(scatradis->Comm().NumProc()>1)
    {
      scatradis->FillComplete();
      structdis->FillComplete();
      // create vector of discr.
      std::vector<Teuchos::RCP<DRT::Discretization> > dis;
      dis.push_back(structdis);
      dis.push_back(scatradis);

      std::vector<Teuchos::RCP<Epetra_Map> > stdelecolmap;
      std::vector<Teuchos::RCP<Epetra_Map> > stdnodecolmap;

      /// binning strategy is created and parallel redistribution is performed
      Teuchos::RCP<BINSTRATEGY::BinningStrategy> binningstrategy =
        Teuchos::rcp(new BINSTRATEGY::BinningStrategy(dis,stdelecolmap,stdnodecolmap));
    }
  }

  matchinggrid_ = DRT::INPUT::IntegralValue<bool>(problem->SSIControlParams(),"MATCHINGGRID");

}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::SetStructSolution( Teuchos::RCP<const Epetra_Vector> disp,
                                       Teuchos::RCP<const Epetra_Vector> vel )
{
  SetMeshDisp(disp);
  SetVelocityFields(vel);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::SetScatraSolution( Teuchos::RCP<const Epetra_Vector> phi )
{
  if(not boundarytransport_)
    structure_->Discretization()->SetState(1,"temperature",phi);
  else
    dserror("transfering scalar state to structure discretization not implemented for"
        "transport on structural boundary. Only SolidToScatra coupling available.");
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::SetVelocityFields( Teuchos::RCP<const Epetra_Vector> vel)
{
  if(not boundarytransport_)
  {
    scatra_->ScaTraField()->SetVelocityField(
        zeros_, //convective vel.
        Teuchos::null, //acceleration
        vel, //velocity
        Teuchos::null, //fsvel
        Teuchos::null, //dofset
        structure_->Discretization()); //discretization
  }
  else
  {
    scatra_->ScaTraField()->SetVelocityField(
        adaptermeshtying_->MasterToSlave(extractor_->ExtractCondVector(zeros_)), //convective vel.
        Teuchos::null, //acceleration
        adaptermeshtying_->MasterToSlave(extractor_->ExtractCondVector(vel)), //velocity
        Teuchos::null, //fsvel
        Teuchos::null, //dofset
        scatra_->ScaTraField()->Discretization(),
        false,
        1); //discretization
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::SetMeshDisp( Teuchos::RCP<const Epetra_Vector> disp )
{
  if(not boundarytransport_)
  {
    scatra_->ScaTraField()->ApplyMeshMovement(
        disp,
        structure_->Discretization());
  }
  else
  {
    scatra_->ScaTraField()->ApplyMeshMovement(
        adaptermeshtying_->MasterToSlave(extractor_->ExtractCondVector(disp)),
        scatra_->ScaTraField()->Discretization(),
        1);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void SSI::SSI_Base::SetupBoundaryScatra()
{
  DRT::Problem* problem = DRT::Problem::Instance();
  Teuchos::RCP<DRT::Discretization> structdis = problem->GetDis("structure");
  Teuchos::RCP<DRT::Discretization> scatradis = problem->GetDis("scatra");

  //check for ssi coupling condition
  std::vector<DRT::Condition*> ssicoupling;
  scatradis->GetCondition("SSICoupling",ssicoupling);
  if(ssicoupling.size())
    boundarytransport_=true;
  else
    boundarytransport_=false;

  if(boundarytransport_)
  {
    adaptermeshtying_ = Teuchos::rcp(new ADAPTER::CouplingMortar());

    std::vector<int> coupleddof(problem->NDim(), 1);
    // Setup of meshtying adapter
    adaptermeshtying_->Setup(structdis,
                            scatradis,
                            Teuchos::null,
                            coupleddof,
                            "SSICoupling",
                            structdis->Comm(),
                            false,
                            false,
                            0,
                            1
                            );

    extractor_= Teuchos::rcp(new LINALG::MapExtractor(*structdis->DofRowMap(0),adaptermeshtying_->MasterDofRowMap(),true));
  }
}
