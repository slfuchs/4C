/*!----------------------------------------------------------------------
\file fs3i_partitioned.cpp
\brief General algorithmic routines for partitioned solution approaches
       to fluid-structure-scalar-scalar interaction (FS3I), that is,
       algorithmic routines not specifically related to partitioned
       solution approaches to one -or two-way-coupled problem
       configurations, respectively

<pre>
Maintainers: Lena Yoshihara & Volker Gravemeier
             {yoshihara,vgravem}@lnm.mw.tum.de
             089/289-15303,-15245
</pre>

*----------------------------------------------------------------------*/


#include <Teuchos_TimeMonitor.hpp>

#include "../drt_lib/drt_utils_createdis.H"
#include "../drt_io/io_control.H"
#include "../drt_fsi/fsi_monolithic.H"
#include "../drt_fsi/fsi_monolithicfluidsplit.H"
#include "../drt_fsi/fsi_monolithicstructuresplit.H"
#include "../drt_fsi/fsi_utils.H"
#include "../drt_fsi/fsi_matrixtransform.H"
#include "../drt_lib/drt_condition_selector.H"
#include "../drt_lib/drt_condition_utils.H"
#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_solver.H"


#include "../drt_lib/drt_globalproblem.H"
#include "../drt_inpar/drt_validparameters.H"
#include "../drt_lib/drt_colors.H"

#include "../drt_adapter/adapter_coupling.H"
#include "../drt_adapter/ad_str_fsiwrapper.H"

#include "../drt_scatra/scatra_algorithm.H"
#include "../drt_scatra/scatra_utils_clonestrategy.H"

#include "../drt_ale/ale_utils_clonestrategy.H"
#include "../drt_adapter/ad_ale_fsi.H"

#include "../drt_lib/drt_condition_utils.H"

#include "fs3i_partitioned.H"


//#define SCATRABLOCKMATRIXMERGE

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
FS3I::PartFS3I::PartFS3I(const Epetra_Comm& comm)
  : FS3I_Base(),
    comm_(comm)
{
  DRT::Problem* problem = DRT::Problem::Instance();
  const Teuchos::ParameterList& fs3icontrol = problem->FS3IControlParams();

  //---------------------------------------------------------------------
  // ensure correct order of three discretizations, with dof-numbering
  // such that structure dof < fluid dof < ale dofs
  // (ordering required at certain non-intuitive points)
  //---------------------------------------------------------------------
  problem->GetDis("structure")->FillComplete();
  problem->GetDis("fluid")->FillComplete();
  problem->GetDis("ale")->FillComplete();
  problem->GetDis("scatra1")->FillComplete();
  problem->GetDis("scatra2")->FillComplete();

  //---------------------------------------------------------------------
  // access discretizations for structure, fluid, ale as well as fluid-
  // and structure-based scalar transport and get material map for fluid
  // and scalar transport elements
  //---------------------------------------------------------------------
  Teuchos::RCP<DRT::Discretization> fluiddis = problem->GetDis("fluid");
  Teuchos::RCP<DRT::Discretization> structdis = problem->GetDis("structure");
  Teuchos::RCP<DRT::Discretization> fluidscatradis = problem->GetDis("scatra1");
  Teuchos::RCP<DRT::Discretization> structscatradis = problem->GetDis("scatra2");
  Teuchos::RCP<DRT::Discretization> aledis = problem->GetDis("ale");

  //---------------------------------------------------------------------
  // create ale discretization as a clone from fluid discretization
  //---------------------------------------------------------------------
  if (aledis->NumGlobalNodes()==0)
  {
    DRT::UTILS::CloneDiscretization<ALE::UTILS::AleCloneStrategy>(fluiddis,aledis);
    // setup material in every ALE element
    Teuchos::ParameterList params;
    params.set<std::string>("action", "setup_material");
    aledis->Evaluate(params);
  }
  else
    dserror("Providing an ALE mesh is not supported for problemtype FS3I.");

  //std::map<std::pair<std::string,std::string>,std::map<int,int> > clonefieldmatmap = problem->CloningMaterialMap();
  //if (clonefieldmatmap.size() < 2)
  //  dserror("At least two material lists required for partitioned FS3I!");

  //---------------------------------------------------------------------
  // create discretization for fluid-based scalar transport from and
  // according to fluid discretization
  //---------------------------------------------------------------------
  if (fluiddis->NumGlobalNodes()==0) dserror("Fluid discretization is empty!");

  // create fluid-based scalar transport elements if fluid-based scalar
  // transport discretization is empty
  if (fluidscatradis->NumGlobalNodes()==0)
  {
    DRT::UTILS::CloneDiscretization<SCATRA::ScatraFluidCloneStrategy>(fluiddis,fluidscatradis);
  }
  else
    dserror("Fluid AND ScaTra discretization present. This is not supported.");

  //---------------------------------------------------------------------
  // create discretization for structure-based scalar transport from and
  // according to structure discretization
  //---------------------------------------------------------------------
  if (structdis->NumGlobalNodes()==0) dserror("Structure discretization is empty!");

  // create structure-based scalar transport elements if structure-based
  // scalar transport discretization is empty
  if (structscatradis->NumGlobalNodes()==0)
  {
    DRT::UTILS::CloneDiscretization<SCATRA::ScatraFluidCloneStrategy>(structdis,structscatradis);
  }
  else
    dserror("Structure AND ScaTra discretization present. This is not supported.");

  //---------------------------------------------------------------------
  // get FSI coupling algorithm
  //---------------------------------------------------------------------
  const Teuchos::ParameterList& fsidyn   = problem->FSIDynamicParams();
  int coupling = Teuchos::getIntegralValue<int>(fsidyn,"COUPALGO");
  switch (coupling)
  {
    case fsi_iter_monolithicfluidsplit:
    {
      fsi_ = Teuchos::rcp(new FSI::MonolithicFluidSplit(comm,fs3icontrol));
      break;
    }
    case fsi_iter_monolithicstructuresplit:
    {
      fsi_ = Teuchos::rcp(new FSI::MonolithicStructureSplit(comm,fs3icontrol));
      break;
    }
    default:
    {
      dserror("Unknown coupling FSI algorithm");
      break;
    }
  }

  //---------------------------------------------------------------------
  // create instances for fluid- and structure-based scalar transport
  // solver and arrange them in combined vector
  //---------------------------------------------------------------------
  // get the solver number used for structural ScalarTransport solver
  const int linsolver1number = fs3icontrol.get<int>("LINEAR_SOLVER1");
  // get the solver number used for structural ScalarTransport solver
  const int linsolver2number = fs3icontrol.get<int>("LINEAR_SOLVER2");

  // check if the linear solver has a valid solver number
  if (linsolver1number == (-1))
    dserror("no linear solver defined for fluid ScalarTransport solver. Please set LINEAR_SOLVER2 in FS3I CONTROL to a valid number!");
  if (linsolver2number == (-1))
    dserror("no linear solver defined for structural ScalarTransport solver. Please set LINEAR_SOLVER2 in FS3I CONTROL to a valid number!");

  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> fluidscatra =
    Teuchos::rcp(new ADAPTER::ScaTraBaseAlgorithm(fs3icontrol,true,"scatra1",problem->SolverParams(linsolver1number)));
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> structscatra =
    Teuchos::rcp(new ADAPTER::ScaTraBaseAlgorithm(fs3icontrol,true,"scatra2",DRT::Problem::Instance()->SolverParams(linsolver2number)));

  scatravec_.push_back(fluidscatra);
  scatravec_.push_back(structscatra);

  //---------------------------------------------------------------------
  // check various input parameters
  //---------------------------------------------------------------------
  const Teuchos::ParameterList& structdyn = problem->StructuralDynamicParams();
  const Teuchos::ParameterList& fluiddyn  = problem->FluidDynamicParams();
  const Teuchos::ParameterList& scatradyn = problem->ScalarTransportDynamicParams();

  // check consistency of time-integration schemes in input file
  // (including parameter theta itself in case of one-step-theta scheme)
  // and rule out unsupported versions of generalized-alpha time-integration
  // scheme (as well as other inappropriate schemes) for fluid subproblem
  INPAR::SCATRA::TimeIntegrationScheme scatratimealgo = DRT::INPUT::IntegralValue<INPAR::SCATRA::TimeIntegrationScheme>(scatradyn,"TIMEINTEGR");
  INPAR::FLUID::TimeIntegrationScheme fluidtimealgo = DRT::INPUT::IntegralValue<INPAR::FLUID::TimeIntegrationScheme>(fluiddyn,"TIMEINTEGR");
  INPAR::STR::DynamicType structtimealgo = DRT::INPUT::IntegralValue<INPAR::STR::DynamicType>(structdyn,"DYNAMICTYP");

  if (fluidtimealgo  == INPAR::FLUID::timeint_one_step_theta)
  {
    if (scatratimealgo != INPAR::SCATRA::timeint_one_step_theta or
        structtimealgo != INPAR::STR::dyna_onesteptheta)
      dserror("Partitioned FS3I computations should feature consistent time-integration schemes for the subproblems; in this case, a one-step-theta scheme is intended to be used for the fluid subproblem, and different schemes are intended to be used for the structure and/or scalar transport subproblems!");

    if (scatradyn.get<double>("THETA") != fluiddyn.get<double>("THETA") or
        scatradyn.get<double>("THETA") != structdyn.sublist("ONESTEPTHETA").get<double>("THETA"))
    dserror("Parameter(s) theta for one-step-theta time-integration scheme defined in one or more of the individual fields do(es) not match for partitioned FS3I computation.");
  }
  else if (fluidtimealgo  == INPAR::FLUID::timeint_afgenalpha)
  {
    if (scatratimealgo != INPAR::SCATRA::timeint_gen_alpha or
        structtimealgo != INPAR::STR::dyna_genalpha)
      dserror("Partitioned FS3I computations should feature consistent time-integration schemes for the subproblems; in this case, a (alpha_f-based) generalized-alpha scheme is intended to be used for the fluid subproblem, and different schemes are intended to be used for the structure and/or scalar transport subproblems!");
  }
  else if (fluidtimealgo  == INPAR::FLUID::timeint_npgenalpha)
  {
      dserror("Partitioned FS3I computations do not support n+1-based generalized-alpha time-integration schemes for the fluid subproblem!");
  }
  else if (fluidtimealgo  == INPAR::FLUID::timeint_bdf2 or
           fluidtimealgo  == INPAR::FLUID::timeint_stationary)
  {
      dserror("Partitioned FS3I computations do not support stationary of BDF2 time-integration schemes for the fluid subproblem!");
  }

  // check that incremental formulation is used for scalar transport field,
  // according to structure and fluid field
  if (scatravec_[0]->ScaTraField()->IsIncremental() == false)
    dserror("Incremental formulation required for partitioned FS3I computations!");

  // ensure that initial time derivative of scalar is not calculated
  //if (DRT::INPUT::IntegralValue<int>(scatradyn,"SKIPINITDER")==false)
  //  dserror("Initial time derivative of phi must not be calculated automatically -> set SKIPINITDER to false");

  //---------------------------------------------------------------------
  // check existence of scatra coupling conditions for both
  // discretizations and definition of the permeability coefficient
  //---------------------------------------------------------------------
  std::vector<std::set<int> > condIDs;
  std::set<int> fluidIDs;
  std::set<int> structIDs;
  condIDs.push_back(fluidIDs);
  condIDs.push_back(structIDs);
  std::vector<std::map<int,double> > PermCoeffs;
  std::map<int,double> fluidcoeff;
  std::map<int,double> structcoeff;
  PermCoeffs.push_back(fluidcoeff);
  PermCoeffs.push_back(structcoeff);

  for (unsigned i=0; i<scatravec_.size(); ++i)
  {
    Teuchos::RCP<DRT::Discretization> dis = (scatravec_[i])->ScaTraField()->Discretization();
    std::vector<DRT::Condition*> coupcond;
    dis->GetCondition("ScaTraCoupling",coupcond);

    for (unsigned iter=0; iter<coupcond.size(); ++iter)
    {
      int myID = (coupcond[iter])->GetInt("coupling id");
      condIDs[i].insert(myID);

      if (!infperm_)
      {
        double myperm = (coupcond[iter])->GetDouble("permeability coefficient");
        PermCoeffs[i].insert(std::pair<int,double>(myID,myperm));
      }
    }
  }
  if (condIDs[0].size() != condIDs[1].size())
    dserror("ScaTra coupling conditions need to be defined on both discretizations");

  if (!infperm_)
  {
    std::map<int,double> fluid_PermCoeffs = PermCoeffs[0];
    std::map<int,double> struct_PermCoeffs = PermCoeffs[1];

    for (std::map<int,double>::iterator fit=fluid_PermCoeffs.begin(); fit!=fluid_PermCoeffs.end(); ++fit)
    {
      int ID = (*fit).first;
      double fluid_permcoef = (*fit).second;

      std::map<int,double>::iterator sit = struct_PermCoeffs.find(ID);
      if ((*sit).second != fluid_permcoef)
        dserror("Permeability coefficient of ScaTra interface needs to be the same in both conditions");
    }
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::ReadRestart()
{
  // read restart information, set vectors and variables
  // (Note that dofmaps might have changed in a redistribution call!)
  const int restart = DRT::Problem::Instance()->Restart();
  if (restart)
  {
    fsi_->ReadRestart(restart);

    for (unsigned i=0; i<scatravec_.size(); ++i)
    {
      Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> currscatra = scatravec_[i];
      currscatra->ScaTraField()->ReadRestart(restart);
    }

    time_ = fsi_->FluidField()->Time();
    step_ = fsi_->FluidField()->Step();
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::SetupSystem()
{
  // now do the coupling setup and create the combined dofmap
  fsi_->SetupSystem();

  /*----------------------------------------------------------------------*/
  /*                  General set up for scalar fields                    */
  /*----------------------------------------------------------------------*/

  // create map extractors needed for scatra condition coupling
  for (unsigned i=0; i<scatravec_.size(); ++i)
  {
    Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> currscatra = scatravec_[i];
    Teuchos::RCP<DRT::Discretization> currdis = currscatra->ScaTraField()->Discretization();
    const int numscal = currscatra->ScaTraField()->NumScal();
    Teuchos::RCP<LINALG::MultiMapExtractor> mapex = Teuchos::rcp(new LINALG::MultiMapExtractor());
    DRT::UTILS::MultiConditionSelector mcs;
    mcs.AddSelector(Teuchos::rcp(new DRT::UTILS::NDimConditionSelector(*currdis,"ScaTraCoupling",0,numscal)));
    mcs.SetupExtractor(*currdis,*currdis->DofRowMap(),*mapex);
    scatrafieldexvec_.push_back(mapex);
  }

  scatracoup_->SetupConditionCoupling(*(scatravec_[0]->ScaTraField()->Discretization()),
                                     scatrafieldexvec_[0]->Map(1),
                                     *(scatravec_[1]->ScaTraField()->Discretization()),
                                     scatrafieldexvec_[1]->Map(1),
                                     "ScaTraCoupling",
                                     scatravec_[0]->ScaTraField()->NumScal()); //we assume here that both discretisation have the same number of scalars

  // create map extractor for coupled scatra fields
  // the second field (currently structure) is always split
  std::vector<Teuchos::RCP<const Epetra_Map> > maps;

  // In the limiting case of an infinite permeability of the interface between
  // different scatra fields, the concentrations on both sides of the interface are
  // constrained to be equal. In this case, we keep the fluid scatra dofs at the
  // interface as unknowns in the overall system, whereas the structure scatra
  // dofs are condensed (cf. "structuresplit" in a monolithic FSI
  // system). Otherwise, both concentrations are kept in the overall system
  // and the equality of fluxes is considered explicitly.
  if (infperm_)
  {
    maps.push_back(scatrafieldexvec_[0]->FullMap());
    maps.push_back(scatrafieldexvec_[1]->Map(0));
  }
  else
  {
    maps.push_back(scatrafieldexvec_[0]->FullMap());
    maps.push_back(scatrafieldexvec_[1]->FullMap());
  }
  Teuchos::RCP<Epetra_Map> fullmap = LINALG::MultiMapExtractor::MergeMaps(maps);
  scatraglobalex_->Setup(*fullmap,maps);

  // create coupling vectors and matrices (only needed for finite surface permeabilities)
  if (!infperm_)
  {
    for (unsigned i=0; i<scatravec_.size(); ++i)
    {
      Teuchos::RCP<Epetra_Vector> scatracoupforce =
      Teuchos::rcp(new Epetra_Vector(*(scatraglobalex_->Map(i)),true));
      scatracoupforce_.push_back(scatracoupforce);

      Teuchos::RCP<LINALG::SparseMatrix> scatracoupmat =
        Teuchos::rcp(new LINALG::SparseMatrix(*(scatraglobalex_->Map(i)),27,false,true));
      scatracoupmat_.push_back(scatracoupmat);

      Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[i];
      const Epetra_Map* dofrowmap = scatra->ScaTraField()->Discretization()->DofRowMap();
      Teuchos::RCP<Epetra_Vector> zeros = LINALG::CreateVector(*dofrowmap,true);
      scatrazeros_.push_back(zeros);
    }
  }

  // create scatra block matrix
  scatrasystemmatrix_ =
    Teuchos::rcp(new LINALG::BlockSparseMatrix<LINALG::DefaultBlockMatrixStrategy>(*scatraglobalex_,
                                                                                   *scatraglobalex_,
                                                                                   27,
                                                                                   false,
                                                                                   true));

  // create scatra rhs vector
  scatrarhs_ = Teuchos::rcp(new Epetra_Vector(*scatraglobalex_->FullMap(),true));

  // create scatra increment vector
  scatraincrement_ = Teuchos::rcp(new Epetra_Vector(*scatraglobalex_->FullMap(),true));

  // check whether potential Dirichlet conditions at scatra interface are
  // defined for both discretizations
  CheckInterfaceDirichletBC();

  // scatra solver
  Teuchos::RCP<DRT::Discretization> firstscatradis = (scatravec_[0])->ScaTraField()->Discretization();
#ifdef SCATRABLOCKMATRIXMERGE
  Teuchos::RCP<Teuchos::ParameterList> scatrasolvparams = Teuchos::rcp(new Teuchos::ParameterList);
  scatrasolvparams->set("solver","umfpack");
  scatrasolver_ = Teuchos::rcp(new LINALG::Solver(scatrasolvparams,
                                         firstscatradis->Comm(),
                                         DRT::Problem::Instance()->ErrorFile()->Handle()));
#else
  const Teuchos::ParameterList& fs3icontrol = DRT::Problem::Instance()->FS3IControlParams();
  // get solver number used for fs3i
  const int linsolvernumber = fs3icontrol.get<int>("COUPLED_LINEAR_SOLVER");
  // check if LOMA solvers has a valid number
  if (linsolvernumber == (-1))
    dserror("no linear solver defined for FS3I problems. Please set COUPLED_LINEAR_SOLVER in FS3I CONTROL to a valid number!");

  const Teuchos::ParameterList& coupledscatrasolvparams =
    DRT::Problem::Instance()->SolverParams(linsolvernumber);
  const int solvertype =
    DRT::INPUT::IntegralValue<INPAR::SOLVER::SolverType>(coupledscatrasolvparams,"SOLVER");
  if (solvertype != INPAR::SOLVER::aztec_msr)
    dserror("aztec solver expected");
  const int azprectype =
    DRT::INPUT::IntegralValue<INPAR::SOLVER::AzPrecType>(coupledscatrasolvparams,"AZPREC");
  if (azprectype != INPAR::SOLVER::azprec_BGS2x2)
    dserror("Block Gauss-Seidel preconditioner expected");

  // use coupled scatra solver object
  scatrasolver_ = Teuchos::rcp(new LINALG::Solver(coupledscatrasolvparams,
                                         firstscatradis->Comm(),
                                         DRT::Problem::Instance()->ErrorFile()->Handle()));

  // get the solver number used for structural ScalarTransport solver
  const int linsolver1number = fs3icontrol.get<int>("LINEAR_SOLVER1");
  // get the solver number used for structural ScalarTransport solver
  const int linsolver2number = fs3icontrol.get<int>("LINEAR_SOLVER2");

  // check if the linear solver has a valid solver number
  if (linsolver1number == (-1))
    dserror("no linear solver defined for fluid ScalarTransport solver. Please set LINEAR_SOLVER2 in FS3I CONTROL to a valid number!");
  if (linsolver2number == (-1))
    dserror("no linear solver defined for structural ScalarTransport solver. Please set LINEAR_SOLVER2 in FS3I CONTROL to a valid number!");

  scatrasolver_->PutSolverParamsToSubParams("Inverse1",DRT::Problem::Instance()->SolverParams(linsolver1number));
  scatrasolver_->PutSolverParamsToSubParams("Inverse2",DRT::Problem::Instance()->SolverParams(linsolver2number));

  (scatravec_[0])->ScaTraField()->Discretization()->ComputeNullSpaceIfNecessary(scatrasolver_->Params().sublist("Inverse1"));
  (scatravec_[1])->ScaTraField()->Discretization()->ComputeNullSpaceIfNecessary(scatrasolver_->Params().sublist("Inverse2"));
#endif
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::TestResults(const Epetra_Comm& comm)
{
  DRT::Problem::Instance()->AddFieldTest(fsi_->FluidField()->CreateFieldTest());
  DRT::Problem::Instance()->AddFieldTest(fsi_->AleField()->CreateFieldTest());
  DRT::Problem::Instance()->AddFieldTest(fsi_->StructureField()->CreateFieldTest());

  for (unsigned i=0; i<scatravec_.size(); ++i)
  {
    Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[i];
    DRT::Problem::Instance()->AddFieldTest(scatra->CreateScaTraFieldTest());
  }
  DRT::Problem::Instance()->TestAll(comm);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::SetFSISolution()
{
  SetMeshDisp();
  SetVelocityFields();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::SetStructScatraSolution()
{
  fsi_->StructureField()->Discretization()->SetState(1,"temperature",(scatravec_[1])->ScaTraField()->Phinp());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::SetMeshDisp()
{
  // fluid field
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> fluidscatra = scatravec_[0];
  Teuchos::RCP<ADAPTER::Fluid> fluidadapter = fsi_->FluidField();
  fluidscatra->ScaTraField()->ApplyMeshMovement(fluidadapter->Dispnp(),
                                               fluidadapter->Discretization());

  // structure field
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> structscatra = scatravec_[1];
  const Teuchos::RCP<ADAPTER::Structure>& structadapter = fsi_->StructureField();
  structscatra->ScaTraField()->ApplyMeshMovement(structadapter->Dispnp(),
                                                structadapter->Discretization());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::SetVelocityFields()
{
  std::vector<Teuchos::RCP<const Epetra_Vector> > convel;
  std::vector<Teuchos::RCP<const Epetra_Vector> > vel;
  ExtractVel(convel, vel);

  std::vector<Teuchos::RCP<DRT::Discretization> > discret;

  discret.push_back(fsi_->FluidField()->Discretization());
  discret.push_back(fsi_->StructureField()->Discretization());

  for (unsigned i=0; i<scatravec_.size(); ++i)
  {
    Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[i];
    scatra->ScaTraField()->SetVelocityField(convel[i],
                                           Teuchos::null,
                                           vel[i],
                                           Teuchos::null,
                                           Teuchos::null,
                                           discret[i]);
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::PartFS3I::ExtractVel(std::vector<Teuchos::RCP<const Epetra_Vector> >& convel,
                      std::vector<Teuchos::RCP<const Epetra_Vector> >& vel)
{
  // extract fluid velocities

  convel.push_back(fsi_->FluidField()->ConvectiveVel());
  vel.push_back(fsi_->FluidField()->Velnp());

  // extract structure velocities and accelerations

  Teuchos::RCP<Epetra_Vector> velocity = Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Velnp())));
  vel.push_back(velocity);
  // structure ScaTra: velocity and grid velocity are identical!
  Teuchos::RCP<Epetra_Vector> zeros = Teuchos::rcp(new Epetra_Vector(velocity->Map(),true));
  convel.push_back(zeros);
}
