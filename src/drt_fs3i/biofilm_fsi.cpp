/*!----------------------------------------------------------------------
\file biofilm_fsi.cpp

<pre>
Maintainer: Mirella Coroneo
            coroneo@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>
*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | headers                                                              |
 *----------------------------------------------------------------------*/
#include "biofilm_fsi.H"
#include "../drt_io/io_gmsh.H"
#include "../drt_fsi/fsi_monolithicfluidsplit.H"
#include "../drt_lib/drt_utils_createdis.H"
#include "../drt_fluid/fluid_utils_mapextractor.H"
#include "../drt_structure/stru_aux.H"
#include "../drt_ale/ale_utils_clonestrategy.H"
#include "../drt_ale/ale_utils_mapextractor.H"
#include "../drt_adapter/adapter_scatra_base_algorithm.H"
#include "../drt_adapter/adapter_coupling.H"

//#define SCATRABLOCKMATRIXMERGE


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
FS3I::BiofilmFSI::BiofilmFSI(const Epetra_Comm& comm)
  :PartFS3I_1WC(comm),
   comm_(comm)
{
  //---------------------------------------------------------------------
  // set up struct ale
  //---------------------------------------------------------------------

  // this algorithm needs an ale discretization also for the structure in order to be able to handle the growth
  DRT::Problem* problem = DRT::Problem::Instance();
  problem->GetDis("structale")->FillComplete();

  // create struct ale elements if not yet existing
  RCP<DRT::Discretization> structaledis = problem->GetDis("structale");
  if (structaledis->NumGlobalNodes()==0)
  {
    RCP<DRT::Discretization> structdis = problem->GetDis("structure");
    Teuchos::RCP<DRT::UTILS::DiscretizationCreator<ALE::UTILS::AleCloneStrategy> > alecreator =
        Teuchos::rcp(new DRT::UTILS::DiscretizationCreator<ALE::UTILS::AleCloneStrategy>() );
    alecreator->CreateMatchingDiscretization(structdis,structaledis,11);
  }

  // a new ale algorithm is needed for struct ale (disnum=1)
  const Teuchos::ParameterList& fsidyn = problem->FSIDynamicParams();
  Teuchos::RCP<ALE::AleBaseAlgorithm> ale = Teuchos::rcp(new ALE::AleBaseAlgorithm(fsidyn,1));
  ale_ = ale->AleFieldrcp();

  //---------------------------------------------------------------------
  // set up couplings
  //---------------------------------------------------------------------

  const string condname = "FSICoupling";
  const int  ndim = DRT::Problem::Instance()->NDim();

  // set up ale-fluid couplings
  icoupfa_ = Teuchos::rcp(new ADAPTER::Coupling());
  icoupfa_->SetupConditionCoupling(*(fsi_->FluidField().Discretization()),
                                   (fsi_->FluidField().Interface()->FSICondMap()),
                                   *(fsi_->AleField().Discretization()),
                                   (fsi_->AleField().Interface()->FSICondMap()),
                                   condname,
                                   ndim);
  // the fluid-ale coupling always matches
  const Epetra_Map* fluidnodemap = fsi_->FluidField().Discretization()->NodeRowMap();
  const Epetra_Map* fluidalenodemap = fsi_->AleField().Discretization()->NodeRowMap();
  coupfa_ = Teuchos::rcp(new ADAPTER::Coupling());
  coupfa_->SetupCoupling(*(fsi_->FluidField().Discretization()),
                         *(fsi_->AleField().Discretization()),
                         *fluidnodemap,
                         *fluidalenodemap,
                         ndim);

  // set up structale-structure couplings
  icoupsa_ = Teuchos::rcp(new ADAPTER::Coupling());
  icoupsa_->SetupConditionCoupling(*(fsi_->StructureField()->Discretization()),
                                   fsi_->StructureField()->Interface()->FSICondMap(),
                                   *structaledis,
                                   ale_->Interface()->FSICondMap(),
                                   condname,
                                   ndim);
  // the structure-ale coupling always matches
  const Epetra_Map* structurenodemap = fsi_->StructureField()->Discretization()->NodeRowMap();
  const Epetra_Map* structalenodemap = structaledis->NodeRowMap();
  coupsa_ = Teuchos::rcp(new ADAPTER::Coupling());
  coupsa_->SetupCoupling(*(fsi_->StructureField()->Discretization()),
                         *structaledis,
                         *structurenodemap,
                         *structalenodemap,
                         ndim);

  /// do we need this? What's for???
  fsi_->FluidField().SetMeshMap(coupfa_->MasterDofMap());

  //---------------------------------------------------------------------
  // getting and initializing problem-specific parameters
  //---------------------------------------------------------------------

  const Teuchos::ParameterList& biofilmcontrol = DRT::Problem::Instance()->BIOFILMControlParams();

  // make sure that initial time derivative of concentration is not calculated
  // automatically (i.e. field-wise)
  const Teuchos::ParameterList& scatradyn = DRT::Problem::Instance()->ScalarTransportDynamicParams();
  if (DRT::INPUT::IntegralValue<int>(scatradyn,"SKIPINITDER")==false)
    dserror("Initial time derivative of phi must not be calculated automatically -> set SKIPINITDER to false");

  //fsi parameters
  dt_fsi = fsidyn.get<double>("TIMESTEP");
  nstep_fsi = fsidyn.get<int>("NUMSTEP");
  maxtime_fsi = fsidyn.get<double>("MAXTIME");
  step_fsi = 0;
  time_fsi = 0.;

  //growth parameters
  dt_bio= biofilmcontrol.get<double>("BIOTIMESTEP");
  nstep_bio= biofilmcontrol.get<int>("BIONUMSTEP");
  fluxcoef_ = biofilmcontrol.get<double>("FLUXCOEF");
  normforcecoef_ = biofilmcontrol.get<double>("NORMFORCECOEF");
  tangforcecoef_ = biofilmcontrol.get<double>("TANGFORCECOEF");
  step_bio=0;
  time_bio = 0.;

  //total time
  time_ = 0.;

  idispn_= fsi_->FluidField().ExtractInterfaceVeln();
  idispnp_= fsi_->FluidField().ExtractInterfaceVeln();
  iveln_= fsi_->FluidField().ExtractInterfaceVeln();

  struidispn_= fsi_->StructureField()->ExtractInterfaceDispn();
  struidispnp_= fsi_->StructureField()->ExtractInterfaceDispn();
  struiveln_= fsi_->StructureField()->ExtractInterfaceDispn();

  idispn_->PutScalar(0.0);
  idispnp_->PutScalar(0.0);
  iveln_->PutScalar(0.0);

  struidispn_->PutScalar(0.0);
  struidispnp_->PutScalar(0.0);
  struiveln_->PutScalar(0.0);

  norminflux_ = Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Discretization()->NodeRowMap())));
  normtraction_= Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Discretization()->NodeRowMap())));
  tangtraction_= Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Discretization()->NodeRowMap())));
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::Timeloop()
{

#ifdef PARALLEL
  Epetra_MpiComm comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm comm;
#endif

  const Teuchos::ParameterList& biofilmcontrol = DRT::Problem::Instance()->BIOFILMControlParams();
  const int biofilmgrowth = DRT::INPUT::IntegralValue<int>(biofilmcontrol,"BIOFILMGROWTH");

  if (biofilmgrowth)
  {
    //outer loop for biofilm growth
    while (step_bio <= nstep_bio)
    {
      //fsi_->SetupSystem();

      if (step_bio == 1)
      {
        StructGmshOutput();
        FluidGmshOutput();
      }

      // inner loop for fsi and scatra
      InnerTimeloop();

      StructGmshOutput();
      FluidGmshOutput();

      if (Comm().MyPID()==0)
      {
        cout<<"\n***********************\n     GROWTH STEP \n***********************\n";
        printf(" growth step = %3d   \n",step_bio);
        printf(" Total time = %3f   \n",time_);
      }

      // compute interface displacement and velocity
      ComputeInterfaceVectors(idispnp_,iveln_,struidispnp_,struiveln_);

      // if we have values at the fluid interface we need to apply them
      if (idispnp_!=Teuchos::null)
	    {
	      fsi_->AleField().ApplyInterfaceDisplacements(FluidToAle(idispnp_));
	    }
      // do all the settings and solve the fluid on a deforming mesh
      FluidAleSolve(idispnp_);

      // if we have values at the structure interface we need to apply them
      if (struidispnp_!=Teuchos::null)
      {
        ale_->ApplyInterfaceDisplacements(StructToAle(struidispnp_));
      }
      // do all the settings and solve the structure on a deforming mesh
      StructAleSolve(struidispnp_);

      // update time
      step_bio++;
      time_bio+=dt_bio;
      time_ = time_bio + time_fsi;

      // reset step and state vectors
      fsi_->StructureField()->Reset();
      fsi_->FluidField().Reset(false, false, step_bio);
      fsi_->AleField().Reset();

      fsi_->AleField().BuildSystemMatrix(false);
    }
  }

  if (!biofilmgrowth)
  {
    InnerTimeloop();
    StructGmshOutput();
    FluidGmshOutput();
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::InnerTimeloop()
{
  // initialize time and step each time we enter the innerloop
  double t=0.;
  step_fsi=0;
  // initialize fluxes and tractions each time we enter the innerloop
  norminflux_->PutScalar(0.0);
  normtraction_->PutScalar(0.0);
  tangtraction_->PutScalar(0.0);

//  // output of initial state
//  ScatraOutput();

  fsi_->PrepareTimeloop();

  // select fsi boundaries
  // (in the future it would be better to introduce a special condition "growth - surface/line"
  // to separate fsi boundaries from growth ones, instead of considering all fsi boundaries as growth boundaries)
  vector<std::string> condnames(1);
  condnames[0] = "FSICoupling";

  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> struscatra = scatravec_[1];

  // Calculation of growth can be based both to values averaged during the innertimeloop
  // (in this case it takes in account also the initial transient state),
  // or only to the last values coming from the fsi-scatra simulation
  const Teuchos::ParameterList& biofilmcontrol = DRT::Problem::Instance()->BIOFILMControlParams();
  const int avgrowth = DRT::INPUT::IntegralValue<int>(biofilmcontrol,"AVGROWTH");
  // in case of averaged values we need temporary variables
  Teuchos::RCP<Epetra_Vector> normtempinflux_ = Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Discretization()->NodeRowMap())));
  Teuchos::RCP<Epetra_Vector> normtemptraction_= Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Discretization()->NodeRowMap())));
  Teuchos::RCP<Epetra_Vector> tangtemptraction_= Teuchos::rcp(new Epetra_Vector(*(fsi_->StructureField()->Discretization()->NodeRowMap())));
  normtempinflux_->PutScalar(0.0);
  normtemptraction_->PutScalar(0.0);
  tangtemptraction_->PutScalar(0.0);


  while (step_fsi < nstep_fsi and t+1e-10*dt_fsi < maxtime_fsi)
  {
    step_fsi++;
    t+=dt_fsi;

    DoFSIStep();
    SetFSISolution();
    DoScatraStep();

    // access structure discretization
    Teuchos::RCP<DRT::Discretization> strudis = fsi_->StructureField()->Discretization();

    // recovery of forces at the interface node based on lagrange multipliers values
    Teuchos::RCP<Epetra_Vector> lambda_ = fsi_->GetLambda();

    // calculation of the flux at the interface based on normal influx values
    Teuchos::RCP<Epetra_MultiVector> strufluxn = struscatra->ScaTraField().CalcFluxAtBoundary(condnames,false);

    // calculate interface normals in deformed configuration
    Teuchos::RCP<Epetra_Vector> nodalnormals = Teuchos::rcp(new Epetra_Vector(*(strudis->DofRowMap())));

    Teuchos::ParameterList eleparams;
    eleparams.set("action","calc_cur_nodal_normals");
    strudis->ClearState();
    strudis->SetState("displacement",fsi_->StructureField()->Dispnp());
    strudis->EvaluateCondition(eleparams,Teuchos::null,Teuchos::null,nodalnormals,Teuchos::null,Teuchos::null,condnames[0]);
    strudis->ClearState();

    // loop over all local interface nodes of structure discretization
    Teuchos::RCP<Epetra_Map> condnodemap = DRT::UTILS::ConditionNodeRowMap(*strudis, condnames[0]);
    for (int nodei=0; nodei < condnodemap->NumMyElements(); nodei++)
    {
      // Here we rely on the fact that the structure scatra discretization is a clone of the structure mesh

      // get the processor's local node with the same lnodeid
      int gnodeid = condnodemap->GID(nodei);
      DRT::Node* strulnode = strudis->gNode(gnodeid);
      // get the degrees of freedom associated with this node
      vector<int> strunodedofs = strudis->Dof(strulnode);
      // determine number of space dimensions
      const int numdim = ((int) strunodedofs.size());

      std::vector<int> doflids(numdim);
      double temp = 0.;
      std::vector<double> unitnormal(3);
      for (int i=0; i<numdim; ++i)
      {
        doflids[i] = strudis->DofRowMap()->LID(strunodedofs[i]);
        unitnormal[i] = (*nodalnormals)[doflids[i]];
        temp += unitnormal[i]*unitnormal[i];
      }
      double absval = sqrt(temp);
      int lnodeid = strudis->NodeRowMap()->LID(gnodeid);

      // compute average unit nodal normal
      std::vector<double> Values(numdim);
      for(int j=0; j<numdim; ++j)
      {
        unitnormal[j] /= absval;
      }
      double tempflux = 0.0;
      double tempnormtrac = 0.0;
      double temptangtrac = 0.0;

      // compute first tangential direction
      std::vector<double> unittangentone(3);
      unittangentone[0] = -unitnormal[1];
      unittangentone[1] = unitnormal[0];
      unittangentone[2] = 0.0;

      // compute second tangential direction
      std::vector<double> unittangenttwo(3);
      unittangenttwo[0] = unitnormal[2]/(unitnormal[0]+unitnormal[1]*unitnormal[1]);
      unittangenttwo[1] = unitnormal[1]*unitnormal[2]/(unitnormal[0]+unitnormal[1]*unitnormal[1]);
      unittangenttwo[2] = -unitnormal[0];

      for(int index=0;index<numdim;++index)
      {
        double fluxcomp = (*((*strufluxn)(index)))[lnodeid];
        tempflux += fluxcomp*unitnormal[index];

        // for the calculation of the growth and erosion both the tangential and the normal
        // components of the forces acting on the interface are important.
        // Since probably they will have a different effect on the biofilm growth,
        // they are calculated separately and a different coefficient can be used.
        double traccomp = (*((*lambda_)(0)))[numdim*nodei+index];
        tempnormtrac +=traccomp*unitnormal[index];
        temptangtrac +=abs (traccomp*unittangentone[index]) + abs (traccomp*unittangenttwo[index]);
      }

      if (avgrowth)
      {
        (*((*normtempinflux_)(0)))[lnodeid] += tempflux;
        (*((*normtemptraction_)(0)))[lnodeid] += tempnormtrac;
        (*((*tangtemptraction_)(0)))[lnodeid] += temptangtrac;
      }
      else
      {
        (*((*norminflux_)(0)))[lnodeid] = tempflux;
        (*((*normtraction_)(0)))[lnodeid] = tempnormtrac;
        (*((*tangtraction_)(0)))[lnodeid] = temptangtrac;
      }
    }
  }

  // here is the averaging of variables needed for biofilm growth, in case the average way was chosen
  if (avgrowth)
  {
    Teuchos::RCP<DRT::Discretization> strudis = fsi_->StructureField()->Discretization();

    // loop over all local interface nodes of structure discretization
    Teuchos::RCP<Epetra_Map> condnodemap = DRT::UTILS::ConditionNodeRowMap(*strudis, condnames[0]);
    for (int i=0; i < condnodemap->NumMyElements(); i++)
    {
      // get the processor's local node with the same lnodeid
      int gnodeid = condnodemap->GID(i);
      int lnodeid = strudis->NodeRowMap()->LID(gnodeid);

      (*((*norminflux_)(0)))[lnodeid] = (*((*normtempinflux_)(0)))[lnodeid] / step_fsi;
      (*((*normtraction_)(0)))[lnodeid] = (*((*normtemptraction_)(0)))[lnodeid] / step_fsi;
      (*((*tangtraction_)(0)))[lnodeid] = (*((*tangtemptraction_)(0)))[lnodeid] / step_fsi;
    }
  }

  time_fsi+=t;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::ComputeInterfaceVectors(RCP<Epetra_Vector> idispnp,
                                               RCP<Epetra_Vector> iveln,
                                               RCP<Epetra_Vector> struidispnp,
                                               RCP<Epetra_Vector> struiveln)
{
  // initialize structure interface displacement at time t^{n+1}
  // shouldn't that be zeroed?
  struidispnp->PutScalar(0.0);

  // select fsi boundaries
  // (in the future it would be better to introduce a special condition "growth - surface/line"
  // to separate fsi boundaries from growth ones, instead of considering all fsi boundaries as growth boundaries)
  std::string condname = "FSICoupling";

  // set action for elements: compute normal vectors at nodes (for reference configuration)
  RCP<DRT::Discretization> strudis = fsi_->StructureField()->Discretization();
  Teuchos::RCP<Epetra_Vector> nodalnormals = Teuchos::rcp(new Epetra_Vector(*(strudis->DofRowMap())));
  Teuchos::ParameterList eleparams;
  eleparams.set("action","calc_ref_nodal_normals");
  strudis->EvaluateCondition(eleparams,Teuchos::null,Teuchos::null,nodalnormals,Teuchos::null,Teuchos::null,condname);

  // select row map with nodes from condition
  Teuchos::RCP<Epetra_Map> condnodemap = DRT::UTILS::ConditionNodeRowMap(*strudis, condname);

  // loop all conditioned nodes
  for (int i=0; i<condnodemap->NumMyElements(); ++i)
  {
    int nodegid = condnodemap->GID(i);
    if (strudis->HaveGlobalNode(nodegid)==false) dserror("node not found on this proc");
    DRT::Node* actnode = strudis->gNode(nodegid);
    std::vector<int> globaldofs = strudis->Dof(actnode);
    const int numdim = (int)(globaldofs.size());

    // extract averaged nodal normal and compute its absolute value
    std::vector<double> unitnormal(numdim);
    double temp = 0.;
    for (int j=0; j<numdim; ++j)
    {
      unitnormal[j] = (*nodalnormals)[strudis->DofRowMap()->LID(globaldofs[j])];
      temp += unitnormal[j]*unitnormal[j];
    }
    double absval = sqrt(temp);
    int lnodeid = strudis->NodeRowMap()->LID(nodegid);
    double influx = (*norminflux_)[lnodeid];
    double normforces = (*normtraction_)[lnodeid];
    double tangforces = (*tangtraction_)[lnodeid];

    // compute average unit nodal normal and "interface velocity"
    std::vector<double> Values(numdim);

    for(int j=0; j<numdim; ++j)
    {
      //introduced a tolerance otherwise NAN will be produced in case of zero absval
      double TOL = 1e-6;
      if (absval > TOL)
      {
        unitnormal[j] /= absval;
        Values[j] = - fluxcoef_ * influx * unitnormal[j]
                    + normforcecoef_ * normforces * unitnormal[j]
                    + tangforcecoef_ * tangforces * unitnormal[j];
      }
    }

    int error = struiveln_->ReplaceGlobalValues(numdim,&Values[0],&globaldofs[0]);
    if (error > 0) dserror("Could not insert values into vector struiveln_: error %d",error);

//    //output and debug
//    if (i==5)
//    {
//      std::ofstream f;
//      if (step_bio < 1)
//      {
//        f.open("inf.txt");
//        f << "#| ID | Step | Time | Influx ";
//        f<<"\n";
//      }
//      else
//        f.open("inf.txt",ios::app);
//      f << i << " " << step_bio << " " << time_bio << " "<< influx << " " ;
//      f << "\n";
//      f.flush();
//      f.close();
//
//      int nodegid = condnodemap->GID(i);
//      DRT::Node* actnode = strudis->gNode(nodegid);
//      std::ofstream a;
//      if (step_bio < 1)
//      {
//        a.open("coord.txt");
//        a << "#| ID | Step | Time | x | y | z | ";
//        a<<"\n";
//      }
//      else
//        a.open("coord.txt",ios::app);
//      a << i << " " << step_bio << " " << time_bio << " ";
//      actnode->Print(a);
//      a << "\n";
//      a.flush();
//      a.close();
//    }

  }

  struidispnp->Update(dt_bio,*struiveln_,0.0);

  Teuchos::RCP<Epetra_Vector> fluididisp = fsi_->StructToFluid(struidispnp);
  idispnp->Update(1.0, *fluididisp, 0.0);

	return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::FluidAleSolve(Teuchos::RCP<Epetra_Vector> idisp)
{
  fsi_->AleField().SetupDBCMapEx(1);
  fsi_->AleField().BuildSystemMatrix();
  fsi_->AleField().Solve();

  //change nodes reference position of the fluid field
  Teuchos::RCP<Epetra_Vector> fluiddisp = AleToFluidField(fsi_->AleField().ExtractDispnp());
  RCP<DRT::Discretization> fluiddis = fsi_->FluidField().Discretization();
  ChangeConfig(fluiddis, fluiddisp);

  //change nodes reference position also for the fluid ale field
  Teuchos::RCP<Epetra_Vector> fluidaledisp = fsi_->AleField().ExtractDispnp();
  RCP<DRT::Discretization> fluidaledis = fsi_->AleField().Discretization();
  ChangeConfig(fluidaledis, fluidaledisp);

  //change nodes reference position also for scatra fluid field
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[0];
  RCP<DRT::Discretization> scatradis = scatra->ScaTraField().Discretization();
  ScatraChangeConfig(scatradis, fluiddis, fluiddisp);

  //set the total displacement due to growth for output reasons
  //fluid
  fsi_->FluidField().SetFldGrDisp(fluiddisp);
  //fluid scatra
  const Epetra_Map* noderowmap = scatradis->NodeRowMap();
  Teuchos::RCP<Epetra_MultiVector> scatraflddisp;
  scatraflddisp = Teuchos::rcp(new Epetra_MultiVector(*noderowmap,3,true));
  VecToScatravec(scatradis, fluiddisp, scatraflddisp);
  scatra->ScaTraField().SetScFldGrDisp(scatraflddisp);

  fsi_->AleField().SetupDBCMapEx(0);

  // computation of fluid solution
  //fluid_->Solve();

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::StructAleSolve(Teuchos::RCP<Epetra_Vector> idisp)
{
  ale_->SetupDBCMapEx(1);
  ale_->BuildSystemMatrix();
  ale_->Solve();

  //change nodes reference position of the structure field
  Teuchos::RCP<Epetra_Vector> structdisp = AleToStructField(ale_->ExtractDispnp());
  RCP<DRT::Discretization> structdis = fsi_->StructureField()->Discretization();
  ChangeConfig(structdis, structdisp);
  structdis->FillComplete(false, true, true);

  //change nodes reference position also for the struct ale field
  RCP<DRT::Discretization> structaledis = ale_->Discretization();
  ChangeConfig(structaledis, ale_->ExtractDispnp());

  //change nodes reference position also for scatra structure field
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> struscatra = scatravec_[1];
  RCP<DRT::Discretization> struscatradis = struscatra->ScaTraField().Discretization();
  ScatraChangeConfig(struscatradis, structdis, structdisp);

  //set the total displacement due to growth for output reasons
  //structure
  fsi_->StructureField()->SetStrGrDisp(structdisp);
  //structure scatra
  const Epetra_Map* noderowmap = struscatradis->NodeRowMap();
  Teuchos::RCP<Epetra_MultiVector> scatrastrudisp;
  scatrastrudisp = Teuchos::rcp(new Epetra_MultiVector(*noderowmap, 3, true));
  VecToScatravec(struscatradis, structdisp, scatrastrudisp);
  struscatra->ScaTraField().SetScStrGrDisp(scatrastrudisp);

  ale_->SetupDBCMapEx(0);

  // computation of structure solution
  //structure_->Solve();

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::BiofilmFSI::FluidToAle(Teuchos::RCP<Epetra_Vector> iv) const
{
  return icoupfa_->MasterToSlave(iv);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::BiofilmFSI::AleToFluidField(Teuchos::RCP<Epetra_Vector> iv) const
{
  return coupfa_->SlaveToMaster(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::BiofilmFSI::AleToStructField(Teuchos::RCP<Epetra_Vector> iv) const
{
  return coupsa_->SlaveToMaster(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::BiofilmFSI::AleToStructField(Teuchos::RCP<const Epetra_Vector> iv) const
{
  return coupsa_->SlaveToMaster(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::BiofilmFSI::StructToAle(Teuchos::RCP<Epetra_Vector> iv) const
{
  return icoupsa_->MasterToSlave(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::BiofilmFSI::StructToAle(Teuchos::RCP<const Epetra_Vector> iv) const
{
  return icoupsa_->MasterToSlave(iv);
}


// is it needed?
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
//void FS3I::BiofilmFSI::UpdateAndOutput()
//{
//  for (unsigned i=0; i<scatravec_.size(); ++i)
//  {
//    Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[i];
//    scatra->ScaTraField().Update();
//
//    // perform time shift of interface displacement
//    idispn_->Update(1.0, *idispnp_ , 0.0);
//    struidispn_->Update(1.0, *struidispnp_ , 0.0);
//
//    // in order to do not have artificial velocity
//    idispnp_=idispn_;
//    struidispnp_=struidispn_;
//
//    scatra->ScaTraField().Output();
//  }
//}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::ChangeConfig(RCP<DRT::Discretization> dis, Teuchos::RCP<Epetra_Vector> disp)
{
  const int numnode = (dis->NodeRowMap())->NumMyElements();
  const Epetra_Vector& gvector =*disp;

  // loop over all nodes
  for (int index = 0; index < numnode; ++index)
  {
    // get current node
    int gid = (dis->NodeRowMap())->GID(index);
    DRT::Node* mynode = dis->gNode(gid);

    vector<int> globaldofs = dis->Dof(mynode);
    std::vector<double> nvector(globaldofs.size());

    // determine number of space dimensions
    const int numdim = DRT::Problem::Instance()->NDim();

    for (int i=0; i<numdim; ++i)
    {
      const int lid = gvector.Map().LID(globaldofs[i]);

      if (lid<0)
      dserror("Proc %d: Cannot find gid=%d in Epetra_Vector",gvector.Comm().MyPID(),globaldofs[i]);
      nvector[i] += gvector[lid];
    }

    mynode->ChangePos(nvector);
  }

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::ScatraChangeConfig(RCP<DRT::Discretization> scatradis,
                                          RCP<DRT::Discretization> dis,
                                          Teuchos::RCP<Epetra_Vector> disp)
{
  const int numnode = (scatradis->NodeRowMap())->NumMyElements();
  const Epetra_Vector& gvector =*disp;

  // loop over all nodes
  for (int index = 0; index < numnode; ++index)
  {
    // get current node
    int gid = (scatradis->NodeRowMap())->GID(index);
    DRT::Node* mynode = scatradis->gNode(gid);

    // get local fluid/structure node with the same local node id
    DRT::Node* lnode = dis->lRowNode(index);

    // get degrees of freedom associated with this fluid/structure node
    vector<int> nodedofs = dis->Dof(0,lnode);

    std::vector<double> nvector(nodedofs.size());

    // determine number of space dimensions
    const int numdim = DRT::Problem::Instance()->NDim();

    for (int i=0; i<numdim; ++i)
    {
      const int lid = gvector.Map().LID(nodedofs[i]);

      if (lid<0)
      dserror("Proc %d: Cannot find gid=%d in Epetra_Vector",gvector.Comm().MyPID(),nodedofs[i]);
      nvector[i] += gvector[lid];
    }

    mynode->ChangePos(nvector);
  }

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::VecToScatravec(RCP<DRT::Discretization> scatradis,
                                      Teuchos::RCP<Epetra_Vector> vec,
                                      Teuchos::RCP<Epetra_MultiVector> scatravec)
{
  // define error variable
  int err(0);

  // loop over all local nodes of scatra discretization
  for (int lnodeid=0; lnodeid < scatradis->NumMyRowNodes(); lnodeid++)
  {
    // determine number of space dimensions
    const int numdim = DRT::Problem::Instance()->NDim();

    for (int index=0;index < numdim; ++index)
    {
      double vecval = (*vec)[index+numdim*lnodeid];

      // insert value into node-based vector
      err = scatravec->ReplaceMyValue(lnodeid,index,vecval);

      if (err != 0) dserror("Error while inserting value into vector scatravec!");
    }

    // for 1- and 2-D problems: set all unused vector components to zero
    for (int index=numdim; index < 3; ++index)
    {
      err = scatravec->ReplaceMyValue(lnodeid,index,0.0);
      if (err != 0) dserror("Error while inserting value into vector scatravec!");
    }
  }

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::StructGmshOutput()
{
  const Teuchos::RCP<DRT::Discretization> structdis = fsi_->StructureField()->Discretization();
  const Teuchos::RCP<DRT::Discretization> structaledis = ale_->Discretization();
  RCP<DRT::Discretization> struscatradis = scatravec_[1]->ScaTraField().Discretization();

  const std::string filename = IO::GMSH::GetNewFileNameAndDeleteOldFiles("struct", step_bio, 701, false, structdis->Comm().MyPID());
  std::ofstream gmshfilecontent(filename.c_str());

  Teuchos::RCP<Epetra_Vector> structdisp = fsi_->StructureField()->ExtractDispn();
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" " << "struct displacement \" {" << endl;
    // draw vector field 'struct displacement' for every element
    IO::GMSH::VectorFieldDofBasedToGmsh(structdis,structdisp,gmshfilecontent);
    gmshfilecontent << "};" << endl;
  }

  Teuchos::RCP<Epetra_Vector> structaledisp = ale_->ExtractDispnp();
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" " << "struct ale displacement \" {" << endl;
    // draw vector field 'struct ale displacement' for every element
    IO::GMSH::VectorFieldDofBasedToGmsh(structaledis,structaledisp,gmshfilecontent);
    gmshfilecontent << "};" << endl;
  }

  Teuchos::RCP<Epetra_Vector> structphi = scatravec_[1]->ScaTraField().Phinp();
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" " << "struct phi \" {" << endl;
    // draw vector field 'struct phi' for every element
    IO::GMSH::ScalarFieldToGmsh(struscatradis,structphi,gmshfilecontent);
    gmshfilecontent << "};" << endl;
  }

  gmshfilecontent.close();

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BiofilmFSI::FluidGmshOutput()
{
  const Teuchos::RCP<DRT::Discretization> fluiddis = fsi_->FluidField().Discretization();
  const Teuchos::RCP<DRT::Discretization> fluidaledis = fsi_->AleField().Discretization();
  RCP<DRT::Discretization> struscatradis = scatravec_[0]->ScaTraField().Discretization();

  const std::string filenamefluid = IO::GMSH::GetNewFileNameAndDeleteOldFiles("fluid", step_bio, 701, false, fluiddis->Comm().MyPID());
  std::ofstream gmshfilecontent(filenamefluid.c_str());

  Teuchos::RCP<const Epetra_Vector> fluidvel = fsi_->FluidField().Velnp();
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" " << "fluid velocity \" {" << endl;
    // draw vector field 'fluid velocity' for every element
    IO::GMSH::VectorFieldDofBasedToGmsh(fluiddis,fluidvel,gmshfilecontent);
    gmshfilecontent << "};" << endl;
  }

  Teuchos::RCP<Epetra_Vector> fluidaledisp = fsi_->AleField().ExtractDispnp();
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" " << "fluid ale displacement \" {" << endl;
    // draw vector field 'fluid ale displacement' for every element
    IO::GMSH::VectorFieldDofBasedToGmsh(fluidaledis,fluidaledisp,gmshfilecontent);
    gmshfilecontent << "};" << endl;
  }

  Teuchos::RCP<Epetra_Vector> fluidphi = scatravec_[0]->ScaTraField().Phinp();
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" " << "fluid phi \" {" << endl;
    // draw vector field 'fluid phi' for every element
    IO::GMSH::ScalarFieldToGmsh(struscatradis,fluidphi,gmshfilecontent);
    gmshfilecontent << "};" << endl;
  }

  gmshfilecontent.close();

  return;
}
