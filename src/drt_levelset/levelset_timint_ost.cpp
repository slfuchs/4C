/*!----------------------------------------------------------------------
\file levelset_timint_ost.cpp

\brief one-step theta time integration scheme for level-set problems

<pre>
Maintainer: Ursula Rasthofer
            rasthofer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>

*----------------------------------------------------------------------*/

#include "levelset_timint_ost.H"

#include "../drt_scatra_ele/scatra_ele_action.H"
#include <Teuchos_StandardParameterEntryValidators.hpp>
#include <Teuchos_TimeMonitor.hpp>
#include "../drt_io/io.H"
#include "../drt_io/io_pstream.H"
//#include "../linalg/linalg_solver.H"

#include "../drt_particle/scatra_particle_coupling.H"


#include "../drt_inpar/drt_validparameters.H"


/*----------------------------------------------------------------------*
 |  Constructor (public)                                rasthofer 09/13 |
 *----------------------------------------------------------------------*/
SCATRA::LevelSetTimIntOneStepTheta::LevelSetTimIntOneStepTheta(
  Teuchos::RCP<DRT::Discretization>      actdis,
  Teuchos::RCP<LINALG::Solver>           solver,
  Teuchos::RCP<Teuchos::ParameterList>   params,
  Teuchos::RCP<Teuchos::ParameterList>   sctratimintparams,
  Teuchos::RCP<Teuchos::ParameterList>   extraparams,
  Teuchos::RCP<IO::DiscretizationWriter> output)
: ScaTraTimIntImpl(actdis,solver,sctratimintparams,extraparams,output),
  LevelSetAlgorithm(actdis,solver,params,sctratimintparams,extraparams,output),
  TimIntOneStepTheta(actdis,solver,sctratimintparams,extraparams,output),
  alphaF_(-1.0) //Member introduced to make OST calculations in level set in combination with gen-alpha in fluid.
{
  // DO NOT DEFINE ANY STATE VECTORS HERE (i.e., vectors based on row or column maps)
  // this is important since we have problems which require an extended ghosting
  // this has to be done before all state vectors are initialized
  return;
}


/*----------------------------------------------------------------------*
| Destructor dtor (public)                              rasthofer 09/13 |
*-----------------------------------------------------------------------*/
SCATRA::LevelSetTimIntOneStepTheta::~LevelSetTimIntOneStepTheta()
{
  return;
}


/*----------------------------------------------------------------------*
 |  initialize time integration                         rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::Init()
{
  // call Init()-functions of base classes
  // note: this order is important
  TimIntOneStepTheta::Init();
  LevelSetAlgorithm::Init();

  return;
}


/*----------------------------------------------------------------------*
| Print information about current time step to screen   rasthofer 09/13 |
*-----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::PrintTimeStepInfo()
{
  if (myrank_==0)
  {
    if (not switchreinit_)
      TimIntOneStepTheta::PrintTimeStepInfo();
    else
    {
      if (reinitaction_ == INPAR::SCATRA::reinitaction_sussman)
        printf("PSEUDOTIMESTEP: %11.4E      %s          THETA = %11.4E   PSEUDOSTEP = %4d/%4d \n",
                  dtau_,MethodTitle().c_str(),thetareinit_,pseudostep_,pseudostepmax_);
      else if (reinitaction_ == INPAR::SCATRA::reinitaction_ellipticeq)
        printf("REINIT ELLIPTIC:\n");
    }
  }
  return;
}


/*----------------------------------------------------------------------*
 | Initialization procedure before the first time step  rasthofer 12/13 |
 -----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::PrepareFirstTimeStep()
{
  if (not switchreinit_)
    TimIntOneStepTheta::PrepareFirstTimeStep();
  else
  {
    // set required general parameters
    Teuchos::ParameterList eleparams;

    eleparams.set<int>("action",SCATRA::set_lsreinit_scatra_parameter);

    // set type of scalar transport problem
    eleparams.set<int>("scatratype",scatratype_);

    // reinitialization equation id given in convective form
    // ale is not intended here
    eleparams.set<int>("form of convective term",INPAR::SCATRA::convform_convective);
    eleparams.set("isale",false);

    // set flag for writing the flux vector fields
    eleparams.set<int>("writeflux",writeflux_);

    //! set vector containing ids of scalars for which flux vectors are calculated
    eleparams.set<Teuchos::RCP<std::vector<int> > >("writefluxids",writefluxids_);

    // parameters for stabilization
    eleparams.sublist("STABILIZATION") = params_->sublist("STABILIZATION");

    // set level-set reitialization specific parameters
    eleparams.sublist("REINITIALIZATION") = levelsetparams_->sublist("REINITIALIZATION");
    // turn off stabilization
    Teuchos::setStringToIntegralParameter<int>("STABTYPEREINIT",
          "no_stabilization",
          "type of stabilization (if any)",
          Teuchos::tuple<std::string>("no_stabilization"),
          Teuchos::tuple<std::string>("Do not use any stabilization"),
          Teuchos::tuple<int>(
              INPAR::SCATRA::stabtype_no_stabilization),
              &eleparams.sublist("REINITIALIZATION"));
    // turn off artificial diffusion
    Teuchos::setStringToIntegralParameter<int>("ARTDIFFREINIT",
            "no",
            "potential incorporation of all-scale subgrid diffusivity (a.k.a. discontinuity-capturing) term",
            Teuchos::tuple<std::string>("no"),
            Teuchos::tuple<std::string>("no artificial diffusion"),
            Teuchos::tuple<int>(INPAR::SCATRA::artdiff_none),
            &eleparams.sublist("REINITIALIZATION"));

    // parameters for finite difference check
    eleparams.set<int>("fdcheck",fdcheck_);
    eleparams.set<double>("fdcheckeps",fdcheckeps_);
    eleparams.set<double>("fdchecktol",fdchecktol_);

    // call standard loop over elements
    discret_->Evaluate(eleparams,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);

    // note: time-integration parameter list has not to be overwritten here, since we rely on incremental solve
    //       as already set in PrepareTimeLoopReinit()

    // compute time derivative of phi at pseudo-time tau=0
    CalcInitialPhidt();

    // eventually, undo changes in general parameter list
    SetReinitializationElementParameters();
  }

  return;
}


/*----------------------------------------------------------------------*
 | set part of the residual vector belonging to the old timestep        |
 |                                                      rasthofer 12/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::SetOldPartOfRighthandside()
{
  if (not switchreinit_)
    TimIntOneStepTheta::SetOldPartOfRighthandside();
  else
  // hist_ = phin_ + dt*(1-Theta)*phidtn_
   hist_->Update(1.0, *phin_, dtau_*(1.0-thetareinit_), *phidtn_, 0.0);

  return;
}


/*----------------------------------------------------------------------*
 | extended version for coupled level-set problems                      |
 | including reinitialization and particle correction   rasthofer 01/14 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::Update(const int num)
{
  // -----------------------------------------------------------------
  //                     reinitialize level-set
  // -----------------------------------------------------------------
  // will be done only if required
  Reinitialization();

  // -------------------------------------------------------------------
  //                     hybrid particle method
  // -------------------------------------------------------------------
  // correct zero level-set by particles if available
  ParticleCorrection();

  // -------------------------------------------------------------------
  //                         update solution
  //        current solution becomes old solution of next time step
  // -------------------------------------------------------------------
  UpdateState();

  return;
}


/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next time step      |
 |                                                      rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::UpdateState()
{
  if ((not switchreinit_) and particle_ == Teuchos::null)
  {
    // compute time derivative at time n+1
    ComputeTimeDerivative();

    // after the next command (time shift of solutions) do NOT call
    // ComputeTimeDerivative() anymore within the current time step!!!

    // solution of this step becomes most recent solution of the last step
    phin_ ->Update(1.0,*phinp_,0.0);

    // time deriv. of this step becomes most recent time derivative of
    // last step
    phidtn_->Update(1.0,*phidtnp_,0.0);
  }
  else
  {
    // solution of this step becomes most recent solution of the last step
    phin_ ->Update(1.0,*phinp_,0.0);

    // reinitialization is done, reset flag
    if (switchreinit_ == true)
      switchreinit_ = false;

    // compute time derivative at time n (and n+1)

    // we also have reset the time-integration parameter list for two reasons
    // 1: use of reinitialization equation overwrites time-integration parameter list (this is corrected afterwards)
    // 2: incremental solver has to be overwritten if used
    Teuchos::ParameterList eletimeparams;

    eletimeparams.set<int>("action",SCATRA::set_time_parameter);
    // set type of scalar transport problem (after preevaluate evaluate, which need scatratype is called)
    eletimeparams.set<int>("scatratype",scatratype_);

    eletimeparams.set<bool>("using generalized-alpha time integration",false);
    eletimeparams.set<bool>("using stationary formulation",false);
    eletimeparams.set<bool>("incremental solver",true); // this is important to have here

    eletimeparams.set<double>("time-step length",dta_);
    eletimeparams.set<double>("total time",time_);
    eletimeparams.set<double>("time factor",theta_*dta_);
    eletimeparams.set<double>("alpha_F",1.0);

    // call standard loop over elements
    discret_->Evaluate(eletimeparams,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);

    CalcInitialPhidt();
    // reset element time-integration parameters
    SetElementTimeParameter();
  }

  // update also particle field and related quantities
  if (particle_ != Teuchos::null)
  {
    // update convective velocity
    conveln_->Update(1.0,*convel_,0.0);

    particle_->Update();
  }

  return;
}


/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next timestep       |
 | used within reinitialization loop                    rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::UpdateReinit()
{
  //TODO: Fkt hier raus nehmen
  // compute time derivative at time n+1
  // time derivative of phi:
  // phidt(n+1) = (phi(n+1)-phi(n)) / (theta*dt) + (1-(1/theta))*phidt(n)
  const double fact1 = 1.0/(thetareinit_*dtau_);
  const double fact2 = 1.0 - (1.0/thetareinit_);
  phidtnp_->Update(fact2,*phidtn_,0.0);
  phidtnp_->Update(fact1,*phinp_,-fact1,*phin_,1.0);

  // solution of this step becomes most recent solution of the last step
  phin_ ->Update(1.0,*phinp_,0.0);

  // time deriv. of this step becomes most recent time derivative of
  // last step
  phidtn_->Update(1.0,*phidtnp_,0.0);

  return;
}


/*--------------------------------------------------------------------------------------------*
 | Redistribute the scatra discretization and vectors according to nodegraph  rasthofer 07/11 |
 |                                                                            DA wichmann     |
 *--------------------------------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::Redistribute(const Teuchos::RCP<Epetra_CrsGraph> nodegraph)
{
  // let the base class do the basic redistribution and transfer of the base class members
  LevelSetAlgorithm::Redistribute(nodegraph);

  // now do all the ost specfic steps
  const Epetra_Map* newdofrowmap = discret_->DofRowMap();
  Teuchos::RCP<Epetra_Vector> old;

  if (fsphinp_ != Teuchos::null)
  {
    old = fsphinp_;
    fsphinp_ = LINALG::CreateVector(*newdofrowmap,true);
    LINALG::Export(*old, *fsphinp_);
  }

  return;
}


/*----------------------------------------------------------------------*
 | setup problem after restart                          rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetTimIntOneStepTheta::ReadRestart(int start)
{
  // do basic restart
  TimIntOneStepTheta::ReadRestart(start);

  // read restart for particles
  if (particle_ != Teuchos::null)
  {
    if(myrank_ == 0)
      std::cout << "===== Particle restart! =====" <<std::endl;

    particle_->ReadRestart(start);
  }

  return;
}

/*----------------------------------------------------------------------*
 | Create Phiaf from OST values                            winter 09/14 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SCATRA::LevelSetTimIntOneStepTheta::PhiafOst(const double alphaf)
{
    const Epetra_Map* dofrowmap = discret_->DofRowMap();
    Teuchos::RCP< Epetra_Vector> phiaf = Teuchos::rcp(new Epetra_Vector(*dofrowmap,true));
    phiaf->Update((1.0-alphaf),*phin_,alphaf,*phinp_,0.0);
    return phiaf;
}

/*----------------------------------------------------------------------*
 | Create Phiam from OST values                            winter 09/14 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SCATRA::LevelSetTimIntOneStepTheta::PhiamOst(const double alpham)
{
    const Epetra_Map* dofrowmap = discret_->DofRowMap();
    Teuchos::RCP< Epetra_Vector> phiam = Teuchos::rcp(new Epetra_Vector(*dofrowmap,true));
    phiam->Update((1.0-alpham),*phin_,alpham,*phinp_,0.0);
    return phiam;
}

/*----------------------------------------------------------------------*
 | Create Phidtam from OST values                            winter 09/14 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SCATRA::LevelSetTimIntOneStepTheta::PhidtamOst(const double alpham)
{
    const Epetra_Map* dofrowmap = discret_->DofRowMap();
    Teuchos::RCP< Epetra_Vector> phidtam = Teuchos::rcp(new Epetra_Vector(*dofrowmap,true));
    phidtam->Update((1.0-alpham),*phidtn_,alpham,*phidtnp_,0.0);
    return phidtam;
}

