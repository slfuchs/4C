#ifdef CCADISCRET

#include "fsi_mortarmonolithic_structuresplit.H"
#include "fsi_debugwriter.H"
#include "fsi_overlapprec_fsiamg.H"
#include "fsi_statustest.H"
#include "fsi_nox_linearsystem_bgs.H"
#include "fsi_monolithic_linearsystem.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_inpar/inpar_fsi.H"
#include "../linalg/linalg_solver.H"
#include "../drt_fluid/fluid_utils_mapextractor.H"

#include "../drt_io/io_control.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
FSI::MortarMonolithicStructureSplit::MortarMonolithicStructureSplit(Epetra_Comm& comm)
  : BlockMonolithic(comm),
    comm_(comm)
{

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::SetupSystem()
{
  const Teuchos::ParameterList& fsidyn   = DRT::Problem::Instance()->FSIDynamicParams();
  linearsolverstrategy_ = Teuchos::getIntegralValue<INPAR::FSI::LinearBlockSolver>(fsidyn,"LINEARBLOCKSOLVER");
  
  aleproj_ = Teuchos::getIntegralValue<INPAR::FSI::SlideALEProj>(fsidyn,"SLIDEALEPROJ");

#ifdef PARALLEL
  Epetra_MpiComm comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm comm;
#endif
  SetDefaultParameters(fsidyn,NOXParameterList());

  // we use non-matching meshes at the interface
  // mortar with: structure = slave, fluid = master

  // structure to fluid

  coupsfm_.Setup(*FluidField().Discretization(),
                 *StructureField().Discretization(),
                 *AleField().Discretization(),
                 comm_,true);

  // fluid to ale at the interface

  icoupfa_.SetupConditionCoupling(*FluidField().Discretization(),
                                   FluidField().Interface().FSICondMap(),
                                  *AleField().Discretization(),
                                   AleField().Interface().FSICondMap(),
                                   "FSICoupling");

  // we might have a free surface
  if (FluidField().Interface().FSCondRelevant())
  {
    fscoupfa_.SetupConditionCoupling(*FluidField().Discretization(),
                                      FluidField().Interface().FSCondMap(),
                                     *AleField().Discretization(),
                                      AleField().Interface().FSCondMap(),
                                      "FREESURFCoupling");
  }

  // the fluid-ale coupling always matches
  const Epetra_Map* fluidnodemap = FluidField().Discretization()->NodeRowMap();
  const Epetra_Map* alenodemap   = AleField().Discretization()->NodeRowMap();

  ADAPTER::Coupling& coupfa = FluidAleCoupling();

  coupfa.SetupCoupling(*FluidField().Discretization(),
                       *AleField().Discretization(),
                       *fluidnodemap,
                       *alenodemap);

  FluidField().SetMeshMap(coupfa.MasterDofMap());

  // create combined map

  std::vector<Teuchos::RCP<const Epetra_Map> > vecSpaces;
  vecSpaces.push_back(StructureField().Interface().OtherMap());
  vecSpaces.push_back(FluidField()    .DofRowMap());
  vecSpaces.push_back(AleField()      .Interface().OtherMap());

  if (vecSpaces[0]->NumGlobalElements()==0)
    dserror("No inner structural equations. Splitting not possible. Panic.");

  SetDofRowMaps(vecSpaces);

  // Use normal matrix for fluid equations but build (splitted) mesh movement
  // linearization (if requested in the input file)
  FluidField().UseBlockMatrix(false);

  // Use splitted structure matrix
  StructureField().UseBlockMatrix();

  // build ale system matrix in splitted system
  AleField().BuildSystemMatrix(false);

  aleresidual_ = Teuchos::rcp(new Epetra_Vector(*AleField().Interface().OtherMap()));

  // get the PCITER from inputfile
  vector<int> pciter;
  vector<double> pcomega;
  vector<int> spciter;
  vector<double> spcomega;
  vector<int> fpciter;
  vector<double> fpcomega;
  vector<int> apciter;
  vector<double> apcomega;
  {
    int    word1;
    double word2;
    {
      std::istringstream pciterstream(Teuchos::getNumericStringParameter(fsidyn,"PCITER"));
      std::istringstream pcomegastream(Teuchos::getNumericStringParameter(fsidyn,"PCOMEGA"));
      while (pciterstream >> word1)
        pciter.push_back(word1);
      while (pcomegastream >> word2)
        pcomega.push_back(word2);
    }
    {
      std::istringstream pciterstream(Teuchos::getNumericStringParameter(fsidyn,"STRUCTPCITER"));
      std::istringstream pcomegastream(Teuchos::getNumericStringParameter(fsidyn,"STRUCTPCOMEGA"));
      while (pciterstream >> word1)
        spciter.push_back(word1);
      while (pcomegastream >> word2)
        spcomega.push_back(word2);
    }
    {
      std::istringstream pciterstream(Teuchos::getNumericStringParameter(fsidyn,"FLUIDPCITER"));
      std::istringstream pcomegastream(Teuchos::getNumericStringParameter(fsidyn,"FLUIDPCOMEGA"));
      while (pciterstream >> word1)
        fpciter.push_back(word1);
      while (pcomegastream >> word2)
        fpcomega.push_back(word2);
    }
    {
      std::istringstream pciterstream(Teuchos::getNumericStringParameter(fsidyn,"ALEPCITER"));
      std::istringstream pcomegastream(Teuchos::getNumericStringParameter(fsidyn,"ALEPCOMEGA"));
      while (pciterstream >> word1)
        apciter.push_back(word1);
      while (pcomegastream >> word2)
        apcomega.push_back(word2);
    }
  }

  // enable debugging
  if (Teuchos::getIntegralValue<int>(fsidyn,"DEBUGOUTPUT") & 2)
  {
    pcdbg_ = Teuchos::rcp(new UTILS::MonolithicDebugWriter(*this));
  }

  // create block system matrix
  switch(linearsolverstrategy_)
  {
  case INPAR::FSI::FSIAMG:
    systemmatrix_ = Teuchos::rcp(new OverlappingBlockMatrixFSIAMG(
                                                          Extractor(),
                                                          StructureField(),
                                                          FluidField(),
                                                          AleField(),
                                                          true,
                                                          Teuchos::getIntegralValue<int>(fsidyn,"SYMMETRICPRECOND"),
                                                          pcomega,
                                                          pciter,
                                                          spcomega,
                                                          spciter,
                                                          fpcomega,
                                                          fpciter,
                                                          apcomega,
                                                          apciter,
                                                          DRT::Problem::Instance()->ErrorFile()->Handle()));
  break;
  case INPAR::FSI::PreconditionedKrylov:
    systemmatrix_ = Teuchos::rcp(new OverlappingBlockMatrix(
                                                          pcdbg_,
                                                          Extractor(),
                                                          StructureField(),
                                                          FluidField(),
                                                          AleField(),
                                                          true,
                                                          Teuchos::getIntegralValue<int>(fsidyn,"SYMMETRICPRECOND"),
                                                          pcomega[0],
                                                          pciter[0],
                                                          spcomega[0],
                                                          spciter[0],
                                                          fpcomega[0],
                                                          fpciter[0],
                                                          apcomega[0],
                                                          apciter[0],
                                                          DRT::Problem::Instance()->ErrorFile()->Handle()));
  break;
  default:
    dserror("Unsupported type of monolithic solver");
  break;
  }
  
  // set up sliding ale if necessary
  switch(aleproj_)
  {
  case INPAR::FSI::ALEprojection_none:
    break;
  case INPAR::FSI::ALEprojection_curr:
  case INPAR::FSI::ALEprojection_ref:
    
    // set up sliding ale utils
    slideale_ = rcp(new FSI::UTILS::SlideAleUtils(StructureField().Discretization(),
                                                  FluidField().Discretization(),
                                                  coupsfm_,
                                                  false));
    
    iprojdisp_ = Teuchos::rcp(new Epetra_Vector(*coupsfm_.MasterDofRowMap(),true));
    iprojdispinc_ = Teuchos::rcp(new Epetra_Vector(*coupsfm_.MasterDofRowMap(),true));
    
    break;
  default:
    dserror("Strange things happen with ALE projection");
    break;
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::SetupRHS(Epetra_Vector& f, bool firstcall)
{
  TEUCHOS_FUNC_TIME_MONITOR("FSI::MortarMonolithicStructureSplit::SetupRHS");
  SetupVector(f,
              StructureField().RHS(),
              FluidField().RHS(),
              AleField().RHS(),
              FluidField().ResidualScaling());

  // add additional ale residual
  Extractor().AddVector(*aleresidual_,2,f);

  if (firstcall)
  {
 
    Teuchos::RCP<LINALG::BlockSparseMatrixBase> a = AleField().BlockSystemMatrix();
    if (a==Teuchos::null)
      dserror("expect ale block matrix");

    LINALG::SparseMatrix& aig = a->Matrix(0,1);
  
    Teuchos::RCP<Epetra_Vector> rhs = Teuchos::rcp(new Epetra_Vector(aig.RowMap()));
    Teuchos::RCP<Epetra_Vector> fveln = FluidField().ExtractInterfaceVeln();
    
    if (aleproj_!= INPAR::FSI::ALEprojection_none)
    {
      
      aig.Apply(*icoupfa_.MasterToSlave(iprojdispinc_),*rhs);
      rhs->Scale(-1.);
    
      Extractor().AddVector(*rhs,2,f);    
    }
    // additional rhs term for ALE equations
    // -dt Aig u(n)
    //
    //    1/dt Delta d(n+1) = theta Delta u(n+1) + u(n)
    //
    // And we are concerned with the u(n) part here.
      
    Teuchos::RCP<Epetra_Vector> aveln = icoupfa_.MasterToSlave(fveln);
    
    aig.Apply(*aveln,*rhs);

    rhs->Scale(-1.*Dt());
      
    Extractor().AddVector(*rhs,2,f);

    // structure
    Teuchos::RCP<LINALG::BlockSparseMatrixBase> s = StructureField().BlockSystemMatrix();
    Teuchos::RCP<LINALG::SparseMatrix> mortar = coupsfm_.GetMortarTrafo();


    Teuchos::RCP<Epetra_Vector> tmprhs = Teuchos::rcp(new Epetra_Vector(mortar->RowMap()));
    rhs = Teuchos::rcp(new Epetra_Vector(s->Matrix(0,1).RowMap()));

    mortar->Apply(*fveln,*tmprhs);
    s->Matrix(0,1).Apply(*tmprhs,*rhs);
    rhs->Scale(-1.*Dt());
    
    Teuchos::RCP<const Epetra_Vector> zeros = Teuchos::rcp(new const Epetra_Vector(rhs->Map(),true));
    LINALG::ApplyDirichlettoSystem(rhs,zeros,*(StructureField().GetDBCMapExtractor()->CondMap()));
    

    Extractor().AddVector(*rhs,0,f);
    rhs = Teuchos::rcp(new Epetra_Vector(s->Matrix(1,1).RowMap()));
    s->Matrix(1,1).Apply(*tmprhs,*rhs);
    tmprhs = Teuchos::rcp(new Epetra_Vector(mortar->DomainMap()));

    mortar->SetUseTranspose(true);
    mortar->Apply(*rhs,*tmprhs);
    mortar->SetUseTranspose(false);

    rhs = FluidField().Interface().InsertFSICondVector(tmprhs);
    double scale     = FluidField().ResidualScaling();
    rhs->Scale(-1.*Dt()/scale);
    
    zeros = Teuchos::rcp(new const Epetra_Vector(rhs->Map(),true));
    LINALG::ApplyDirichlettoSystem(rhs,zeros,*(FluidField().GetDBCMapExtractor()->CondMap()));

    Extractor().AddVector(*rhs,1,f);
    // shape derivatives
    Teuchos::RCP<LINALG::BlockSparseMatrixBase> mmm = FluidField().ShapeDerivatives();
    if (mmm!=Teuchos::null)
    {
      LINALG::SparseMatrix& fmig = mmm->Matrix(0,1);
      LINALG::SparseMatrix& fmgg = mmm->Matrix(1,1);

      rhs = Teuchos::rcp(new Epetra_Vector(fmig.RowMap()));
      fmig.Apply(*fveln,*rhs);
      Teuchos::RCP<Epetra_Vector> veln = FluidField().Interface().InsertOtherVector(rhs);

      rhs = Teuchos::rcp(new Epetra_Vector(fmgg.RowMap()));
      fmgg.Apply(*fveln,*rhs);
      FluidField().Interface().InsertFSICondVector(rhs,veln);

      veln->Scale(-1.*Dt());

      Extractor().AddVector(*veln,1,f);
    }

    // if there is a free surface
    if (FluidField().Interface().FSCondRelevant())
    {
      // here we extract the free surface submatrices from position 2
      LINALG::SparseMatrix& aig = a->Matrix(0,2);

      // extract fluid free surface velocities.
      Teuchos::RCP<Epetra_Vector> fveln = FluidField().ExtractFreeSurfaceVeln();
      Teuchos::RCP<Epetra_Vector> aveln = icoupfa_.MasterToSlave(fveln);

      Teuchos::RCP<Epetra_Vector> rhs = Teuchos::rcp(new Epetra_Vector(aig.RowMap()));
      aig.Apply(*aveln,*rhs);

      rhs->Scale(-1.*Dt());

      Extractor().AddVector(*rhs,1,f);

      // shape derivatives
      Teuchos::RCP<LINALG::BlockSparseMatrixBase> mmm = FluidField().ShapeDerivatives();
      if (mmm!=Teuchos::null)
      {
        // here we extract the free surface submatrices from position 2
        LINALG::SparseMatrix& fmig = mmm->Matrix(0,2);
        LINALG::SparseMatrix& fmgg = mmm->Matrix(2,2);

        rhs = Teuchos::rcp(new Epetra_Vector(fmig.RowMap()));
        fmig.Apply(*fveln,*rhs);
        Teuchos::RCP<Epetra_Vector> veln = FluidField().Interface().InsertOtherVector(rhs);

        rhs = Teuchos::rcp(new Epetra_Vector(fmgg.RowMap()));
        fmgg.Apply(*fveln,*rhs);
        FluidField().Interface().InsertFSCondVector(rhs,veln);

        veln->Scale(-1.*Dt());

        Extractor().AddVector(*veln,0,f);
      }
    }
  }
  // NOX expects a different sign here.
  f.Scale(-1.);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::SetupSystemMatrix(LINALG::BlockSparseMatrixBase& mat)
{
  TEUCHOS_FUNC_TIME_MONITOR("FSI::MortarMonolithicStructureSplit::SetupSystemMatrix");

  // extract Jacobian matrices and put them into composite system
  // matrix W

  //const ADAPTER::Coupling& coupsf = StructureFluidCoupling();
  //const ADAPTER::Coupling& coupsa = StructureAleCoupling();

  Teuchos::RCP<LINALG::SparseMatrix> mortar = coupsfm_.GetMortarTrafo();

  Teuchos::RCP<LINALG::BlockSparseMatrixBase> s = StructureField().BlockSystemMatrix();
  if (s==Teuchos::null)
    dserror("expect structure block matrix");
  Teuchos::RCP<LINALG::SparseMatrix> f = FluidField().SystemMatrix();
  if (f==Teuchos::null)
    dserror("expect fluid matrix");
  Teuchos::RCP<LINALG::BlockSparseMatrixBase> a = AleField().BlockSystemMatrix();
  if (a==Teuchos::null)
    dserror("expect ale block matrix");

  LINALG::SparseMatrix& aii = a->Matrix(0,0);
  LINALG::SparseMatrix& aig = a->Matrix(0,1);

  /*----------------------------------------------------------------------*/

  double scale     = FluidField().ResidualScaling();
  double timescale = FluidField().TimeScaling();
  // build block matrix
  // The maps of the block matrix have to match the maps of the blocks we
  // insert here.

  // Uncomplete fluid matrix to be able to deal with slightly defective
  // interface meshes.
  f->UnComplete();

  mat.Assign(0,0,View,s->Matrix(0,0));

  RCP<LINALG::SparseMatrix> sig = MLMultiply(s->Matrix(0,1),false,*mortar,false,false,false,true);
  RCP<LINALG::SparseMatrix> lsig = rcp(new LINALG::SparseMatrix(sig->RowMap(),81,false));

  lsig->Add(*sig,false,1./timescale,0.0);
  lsig->Complete(f->DomainMap(),sig->RangeMap());

  lsig->ApplyDirichlet( *(StructureField().GetDBCMapExtractor()->CondMap()),false);
  
  mat.Assign(0,1,View,*lsig);

  RCP<LINALG::SparseMatrix> sgi = MLMultiply(*mortar,true,s->Matrix(1,0),false,false,false,true);
  RCP<LINALG::SparseMatrix> lsgi = rcp(new LINALG::SparseMatrix(f->RowMap(),81,false));

  lsgi->Add(*sgi,false,1./scale,0.0);
  lsgi->Complete(sgi->DomainMap(),f->RangeMap());

  lsgi->ApplyDirichlet( *(FluidField().GetDBCMapExtractor()->CondMap()),false);
  
  mat.Assign(1,0,View,*lsgi);

  RCP<LINALG::SparseMatrix> sgg = MLMultiply(s->Matrix(1,1),false,*mortar,false,false,false,true);
  sgg = MLMultiply(*mortar,true,*sgg,false,false,false,true);
  
  sgg->ApplyDirichlet( *(FluidField().GetDBCMapExtractor()->CondMap()),false);
  
  f->Add(*sgg,false,1./(scale*timescale),1.0);
  mat.Assign(1,1,View,*f);

  aigtransform_(a->FullRowMap(),
                a->FullColMap(),
                aig,
                1./timescale,
                ADAPTER::Coupling::SlaveConverter(icoupfa_),
                mat.Matrix(2,1));
  mat.Assign(2,2,View,aii);

  /*----------------------------------------------------------------------*/
  // add optional fluid linearization with respect to mesh motion block

  Teuchos::RCP<LINALG::BlockSparseMatrixBase> mmm = FluidField().ShapeDerivatives();
  if (mmm!=Teuchos::null)
  {
    LINALG::SparseMatrix& fmii = mmm->Matrix(0,0);
    LINALG::SparseMatrix& fmig = mmm->Matrix(0,1);
    LINALG::SparseMatrix& fmgi = mmm->Matrix(1,0);
    LINALG::SparseMatrix& fmgg = mmm->Matrix(1,1);

    mat.Matrix(1,1).Add(fmgg,false,1./timescale,1.0);
    mat.Matrix(1,1).Add(fmig,false,1./timescale,1.0);

    const ADAPTER::Coupling& coupfa = FluidAleCoupling();

    fmgitransform_(mmm->FullRowMap(),
                   mmm->FullColMap(),
                   fmgi,
                   1.,
                   ADAPTER::Coupling::MasterConverter(coupfa),
                   mat.Matrix(1,2),
                   false,
                   false);

    fmiitransform_(mmm->FullRowMap(),
                   mmm->FullColMap(),
                   fmii,
                   1.,
                   ADAPTER::Coupling::MasterConverter(coupfa),
                   mat.Matrix(1,2),
                   false,
                   true);
  }

  // if there is a free surface
  if (FluidField().Interface().FSCondRelevant())
  {
    // here we extract the free surface submatrices from position 2
    LINALG::SparseMatrix& aig = a->Matrix(0,2);

    fsaigtransform_(a->FullRowMap(),
                    a->FullColMap(),
                    aig,
                    1./timescale,
                    ADAPTER::Coupling::SlaveConverter(fscoupfa_),
                    mat.Matrix(2,1));

    if (mmm!=Teuchos::null)
    {
      // We assume there is some space between fsi interface and free
      // surface. Thus the matrices mmm->Matrix(1,2) and mmm->Matrix(2,1) are
      // zero.

      // here we extract the free surface submatrices from position 2
      LINALG::SparseMatrix& fmig = mmm->Matrix(0,2);
      LINALG::SparseMatrix& fmgi = mmm->Matrix(2,0);
      LINALG::SparseMatrix& fmgg = mmm->Matrix(2,2);

      mat.Matrix(1,1).Add(fmgg,false,1./timescale,1.0);
      mat.Matrix(1,1).Add(fmig,false,1./timescale,1.0);

      const ADAPTER::Coupling& coupfa = FluidAleCoupling();

      fsmgitransform_(mmm->FullRowMap(),
                      mmm->FullColMap(),
                      fmgi,
                      1.,
                      ADAPTER::Coupling::MasterConverter(coupfa),
                      mat.Matrix(1,2),
                      false,
                      false);
    }
  }

  // done. make sure all blocks are filled.
  mat.Complete();
}

void FSI::MortarMonolithicStructureSplit::Update()
{
  
  // update history variabels for sliding ale
  if (aleproj_!= INPAR::FSI::ALEprojection_none)
  {
  
    Teuchos::RCP<Epetra_Vector> idispn = StructureField().ExtractInterfaceDispn();
    Teuchos::RCP<Epetra_Vector> idisptotal = StructureField().ExtractInterfaceDispnp();
    Teuchos::RCP<Epetra_Vector> idispstep = StructureField().ExtractInterfaceDispnp();
    idispstep->Update(-1.0, *idispn, 1.0);
  
    slideale_->Remeshing(idisptotal,
                        idispstep,
                        StructureField().Discretization(),
                        FluidField().Discretization(),
                        iprojdisp_,
                        coupsfm_,
                        Comm(),
                        aleproj_);
  
    Teuchos::RCP<Epetra_Vector> idisp =AleField().Interface().ExtractFSICondVector(AleField().ExtractDisplacement());
    
    iprojdispinc_->Update(1.0,*iprojdisp_,-1.0,*idisp,0.0);

    slideale_->EvaluateMortar(idisptotal, iprojdisp_, coupsfm_);
  
  }
  
  StructureField().Update();
  FluidField().Update();
  AleField().Update();

}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::InitialGuess(Teuchos::RCP<Epetra_Vector> ig)
{
  TEUCHOS_FUNC_TIME_MONITOR("FSI::MortarMonolithicStructureSplit::InitialGuess");

  SetupVector(*ig,
              StructureField().InitialGuess(),
              FluidField().InitialGuess(),
              AleField().InitialGuess(),
              0.0);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::ScaleSystem(LINALG::BlockSparseMatrixBase& mat, Epetra_Vector& b)
{
  //should we scale the system?
  const Teuchos::ParameterList& fsidyn   = DRT::Problem::Instance()->FSIDynamicParams();
  const bool scaling_infnorm = (bool)getIntegralValue<int>(fsidyn,"INFNORMSCALING");

  if (scaling_infnorm)
  {
    // The matrices are modified here. Do we have to change them back later on?

    Teuchos::RCP<Epetra_CrsMatrix> A = mat.Matrix(0,0).EpetraMatrix();
    srowsum_ = rcp(new Epetra_Vector(A->RowMap(),false));
    scolsum_ = rcp(new Epetra_Vector(A->RowMap(),false));
    A->InvRowSums(*srowsum_);
    A->InvColSums(*scolsum_);
    if (A->LeftScale(*srowsum_) or
        A->RightScale(*scolsum_) or
        mat.Matrix(0,1).EpetraMatrix()->LeftScale(*srowsum_) or
        mat.Matrix(0,2).EpetraMatrix()->LeftScale(*srowsum_) or
        mat.Matrix(1,0).EpetraMatrix()->RightScale(*scolsum_) or
        mat.Matrix(2,0).EpetraMatrix()->RightScale(*scolsum_))
      dserror("structure scaling failed");

    A = mat.Matrix(2,2).EpetraMatrix();
    arowsum_ = rcp(new Epetra_Vector(A->RowMap(),false));
    acolsum_ = rcp(new Epetra_Vector(A->RowMap(),false));
    A->InvRowSums(*arowsum_);
    A->InvColSums(*acolsum_);
    if (A->LeftScale(*arowsum_) or
        A->RightScale(*acolsum_) or
        mat.Matrix(2,0).EpetraMatrix()->LeftScale(*arowsum_) or
        mat.Matrix(2,1).EpetraMatrix()->LeftScale(*arowsum_) or
        mat.Matrix(0,2).EpetraMatrix()->RightScale(*acolsum_) or
        mat.Matrix(1,2).EpetraMatrix()->RightScale(*acolsum_))
      dserror("ale scaling failed");

    Teuchos::RCP<Epetra_Vector> sx = Extractor().ExtractVector(b,0);
    Teuchos::RCP<Epetra_Vector> ax = Extractor().ExtractVector(b,2);

    if (sx->Multiply(1.0, *srowsum_, *sx, 0.0))
      dserror("structure scaling failed");
    if (ax->Multiply(1.0, *arowsum_, *ax, 0.0))
      dserror("ale scaling failed");

    Extractor().InsertVector(*sx,0,b);
    Extractor().InsertVector(*ax,2,b);
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::UnscaleSolution(LINALG::BlockSparseMatrixBase& mat, Epetra_Vector& x, Epetra_Vector& b)
{
  const Teuchos::ParameterList& fsidyn   = DRT::Problem::Instance()->FSIDynamicParams();
  const bool scaling_infnorm = (bool)getIntegralValue<int>(fsidyn,"INFNORMSCALING");

  if (scaling_infnorm)
  {
    Teuchos::RCP<Epetra_Vector> sy = Extractor().ExtractVector(x,0);
    Teuchos::RCP<Epetra_Vector> ay = Extractor().ExtractVector(x,2);

    if (sy->Multiply(1.0, *scolsum_, *sy, 0.0))
      dserror("structure scaling failed");
    if (ay->Multiply(1.0, *acolsum_, *ay, 0.0))
      dserror("ale scaling failed");

    Extractor().InsertVector(*sy,0,x);
    Extractor().InsertVector(*ay,2,x);

    Teuchos::RCP<Epetra_Vector> sx = Extractor().ExtractVector(b,0);
    Teuchos::RCP<Epetra_Vector> ax = Extractor().ExtractVector(b,2);

    if (sx->ReciprocalMultiply(1.0, *srowsum_, *sx, 0.0))
      dserror("structure scaling failed");
    if (ax->ReciprocalMultiply(1.0, *arowsum_, *ax, 0.0))
      dserror("ale scaling failed");

    Extractor().InsertVector(*sx,0,b);
    Extractor().InsertVector(*ax,2,b);

    Teuchos::RCP<Epetra_CrsMatrix> A = mat.Matrix(0,0).EpetraMatrix();
    srowsum_->Reciprocal(*srowsum_);
    scolsum_->Reciprocal(*scolsum_);
    if (A->LeftScale(*srowsum_) or
        A->RightScale(*scolsum_) or
        mat.Matrix(0,1).EpetraMatrix()->LeftScale(*srowsum_) or
        mat.Matrix(0,2).EpetraMatrix()->LeftScale(*srowsum_) or
        mat.Matrix(1,0).EpetraMatrix()->RightScale(*scolsum_) or
        mat.Matrix(2,0).EpetraMatrix()->RightScale(*scolsum_))
      dserror("structure scaling failed");

    A = mat.Matrix(2,2).EpetraMatrix();
    arowsum_->Reciprocal(*arowsum_);
    acolsum_->Reciprocal(*acolsum_);
    if (A->LeftScale(*arowsum_) or
        A->RightScale(*acolsum_) or
        mat.Matrix(2,0).EpetraMatrix()->LeftScale(*arowsum_) or
        mat.Matrix(2,1).EpetraMatrix()->LeftScale(*arowsum_) or
        mat.Matrix(0,2).EpetraMatrix()->RightScale(*acolsum_) or
        mat.Matrix(1,2).EpetraMatrix()->RightScale(*acolsum_))
      dserror("ale scaling failed");

  }

#if 1
  // very simple hack just to see the linear solution

  Epetra_Vector r(b.Map());
  mat.Apply(x,r);
  r.Update(1.,b,1.);

  Teuchos::RCP<Epetra_Vector> sr = Extractor().ExtractVector(r,0);
  Teuchos::RCP<Epetra_Vector> fr = Extractor().ExtractVector(r,1);
  Teuchos::RCP<Epetra_Vector> ar = Extractor().ExtractVector(r,2);

  // increment additional ale residual
  aleresidual_->Update(-1.,*ar,0.);

  ios_base::fmtflags flags = Utils()->out().flags();

  double n,ns,nf,na;
  r.Norm2(&n);
  sr->Norm2(&ns);
  fr->Norm2(&nf);
  ar->Norm2(&na);
  Utils()->out() << std::scientific
                 << "\nlinear solver quality:\n"
                 << "L_2-norms:\n"
                 << END_COLOR "   |r|=" YELLOW << n
                 << END_COLOR "   |rs|=" YELLOW << ns
                 << END_COLOR "   |rf|=" YELLOW << nf
                 << END_COLOR "   |ra|=" YELLOW << na
                 << END_COLOR "\n";
  r.NormInf(&n);
  sr->NormInf(&ns);
  fr->NormInf(&nf);
  ar->NormInf(&na);
  Utils()->out() << "L_inf-norms:\n"
                 << END_COLOR "   |r|=" YELLOW << n
                 << END_COLOR "   |rs|=" YELLOW << ns
                 << END_COLOR "   |rf|=" YELLOW << nf
                 << END_COLOR "   |ra|=" YELLOW << na
                 << END_COLOR "\n";

  Utils()->out().flags(flags);
#endif
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::SetupVector(Epetra_Vector &f,
                                                Teuchos::RCP<const Epetra_Vector> sv,
                                                Teuchos::RCP<const Epetra_Vector> fv,
                                                Teuchos::RCP<const Epetra_Vector> av,
                                                double fluidscale)
{
  // extract the inner and boundary dofs of all three fields

  Teuchos::RCP<Epetra_Vector> sov = StructureField().Interface().ExtractOtherVector(sv);
  Teuchos::RCP<Epetra_Vector> aov = AleField()      .Interface().ExtractOtherVector(av);

  if (fluidscale!=0)
  {
    // add structure interface values to fluid vector

    Teuchos::RCP<LINALG::SparseMatrix> mortar = coupsfm_.GetMortarTrafo();

    Teuchos::RCP<Epetra_Vector> fcv = FluidField().Interface().ExtractFSICondVector(fv);
    Teuchos::RCP<Epetra_Vector> scv = StructureField().Interface().ExtractFSICondVector(sv);

    mortar->SetUseTranspose(true);
    mortar->Apply(*scv,*fcv);
    mortar->SetUseTranspose(false);

    Teuchos::RCP<Epetra_Vector> modfv = FluidField().Interface().InsertFSICondVector(fcv);
    modfv->Update(1.0, *fv, 1./fluidscale);

    Teuchos::RCP<const Epetra_Vector> zeros = Teuchos::rcp(new const Epetra_Vector(modfv->Map(),true));
    LINALG::ApplyDirichlettoSystem(modfv,zeros,*(FluidField().GetDBCMapExtractor()->CondMap()));
    
    Extractor().InsertVector(*modfv,1,f);
  }
  else
  {
    Extractor().InsertVector(*fv,1,f);
  }

  Extractor().InsertVector(*sov,0,f);
  Extractor().InsertVector(*aov,2,f);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<NOX::Epetra::LinearSystem>
FSI::MortarMonolithicStructureSplit::CreateLinearSystem(ParameterList& nlParams,
                                                  NOX::Epetra::Vector& noxSoln,
                                                  Teuchos::RCP<NOX::Utils> utils)
{
  Teuchos::RCP<NOX::Epetra::LinearSystem> linSys;

  Teuchos::ParameterList& printParams = nlParams.sublist("Printing");
  Teuchos::ParameterList& dirParams = nlParams.sublist("Direction");
  Teuchos::ParameterList& newtonParams = dirParams.sublist("Newton");
  Teuchos::ParameterList* lsParams = NULL;

  // in case of nonlinCG the linear solver list is somewhere else
  if (dirParams.get("Method","User Defined")=="User Defined")
    lsParams = &(newtonParams.sublist("Linear Solver"));
  else if (dirParams.get("Method","User Defined")=="NonlinearCG")
    lsParams = &(dirParams.sublist("Nonlinear CG").sublist("Linear Solver"));
  else dserror("Unknown nonlinear method");

  NOX::Epetra::Interface::Jacobian* iJac = this;
  NOX::Epetra::Interface::Preconditioner* iPrec = this;
  const Teuchos::RCP< Epetra_Operator > J = systemmatrix_;
  const Teuchos::RCP< Epetra_Operator > M = systemmatrix_;

  switch (linearsolverstrategy_)
  {
  case INPAR::FSI::PreconditionedKrylov:
  case INPAR::FSI::FSIAMG:
#if 1
    linSys = Teuchos::rcp(new FSI::MonolithicLinearSystem::MonolithicLinearSystem(
#else
    linSys = Teuchos::rcp(new NOX::Epetra::LinearSystemAztecOO(
#endif
                                                               printParams,
                                                               *lsParams,
                                                               Teuchos::rcp(iJac,false),
                                                               J,
                                                               Teuchos::rcp(iPrec,false),
                                                               M,
                                                               noxSoln));
    break;
  case INPAR::FSI::BGSAitken:
  case INPAR::FSI::BGSVectorExtrapolation:
  case INPAR::FSI::BGSJacobianFreeNewtonKrylov:
    linSys = Teuchos::rcp(new NOX::FSI::LinearBGSSolver(printParams,
                                                        *lsParams,
                                                        Teuchos::rcp(iJac,false),
                                                        J,
                                                        noxSoln,
                                                        StructureField().LinearSolver(),
                                                        FluidField().LinearSolver(),
                                                        AleField().LinearSolver(),
                                                        linearsolverstrategy_));
    break;
  default:
    dserror("unsupported linear block solver strategy: %d", linearsolverstrategy_);
  }

  return linSys;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<NOX::StatusTest::Combo>
FSI::MortarMonolithicStructureSplit::CreateStatusTest(Teuchos::ParameterList& nlParams,
                                                Teuchos::RCP<NOX::Epetra::Group> grp)
{
  // Create the convergence tests
  Teuchos::RCP<NOX::StatusTest::Combo> combo       = Teuchos::rcp(new NOX::StatusTest::Combo(NOX::StatusTest::Combo::OR));
  Teuchos::RCP<NOX::StatusTest::Combo> converged   = Teuchos::rcp(new NOX::StatusTest::Combo(NOX::StatusTest::Combo::AND));

  Teuchos::RCP<NOX::StatusTest::MaxIters> maxiters = Teuchos::rcp(new NOX::StatusTest::MaxIters(nlParams.get("Max Iterations", 100)));
  Teuchos::RCP<NOX::StatusTest::FiniteValue> fv    = Teuchos::rcp(new NOX::StatusTest::FiniteValue);

  Teuchos::RCP<NOX::StatusTest::NormUpdate> update =
  Teuchos::rcp(new NOX::StatusTest::NormUpdate(nlParams.get("Norm Update", 1.0e-5)));
  combo->addStatusTest(update);

  combo->addStatusTest(fv);
  combo->addStatusTest(converged);
  combo->addStatusTest(maxiters);

  // require one solve
  converged->addStatusTest(Teuchos::rcp(new NOX::FSI::MinIters(1)));

  // setup tests for structural displacements

  Teuchos::RCP<NOX::StatusTest::Combo> structcombo =
    Teuchos::rcp(new NOX::StatusTest::Combo(NOX::StatusTest::Combo::OR));

  Teuchos::RCP<NOX::FSI::PartialNormF> structureDisp =
    Teuchos::rcp(new NOX::FSI::PartialNormF("displacement",
                                            Extractor(),0,
                                            nlParams.get("Norm abs disp", 1.0e-6),
                                            NOX::Abstract::Vector::TwoNorm,
                                            NOX::FSI::PartialNormF::Scaled));
  Teuchos::RCP<NOX::FSI::PartialNormUpdate> structureDispUpdate =
    Teuchos::rcp(new NOX::FSI::PartialNormUpdate("displacement update",
                                                 Extractor(),0,
                                                 nlParams.get("Norm abs disp", 1.0e-6),
                                                 NOX::FSI::PartialNormUpdate::Scaled));

  AddStatusTest(structureDisp);
  structcombo->addStatusTest(structureDisp);
  //structcombo->addStatusTest(structureDispUpdate);

  converged->addStatusTest(structcombo);

  // setup tests for interface

  std::vector<Teuchos::RCP<const Epetra_Map> > interface;
  interface.push_back(FluidField().Interface().FSICondMap());
  interface.push_back(Teuchos::null);
  LINALG::MultiMapExtractor interfaceextract(*DofRowMap(),interface);

  Teuchos::RCP<NOX::StatusTest::Combo> interfacecombo =
    Teuchos::rcp(new NOX::StatusTest::Combo(NOX::StatusTest::Combo::OR));

  Teuchos::RCP<NOX::FSI::PartialNormF> interfaceTest =
    Teuchos::rcp(new NOX::FSI::PartialNormF("interface",
                                            interfaceextract,0,
                                            nlParams.get("Norm abs vel", 1.0e-6),
                                            NOX::Abstract::Vector::TwoNorm,
                                            NOX::FSI::PartialNormF::Scaled));
  Teuchos::RCP<NOX::FSI::PartialNormUpdate> interfaceTestUpdate =
    Teuchos::rcp(new NOX::FSI::PartialNormUpdate("interface update",
                                                 interfaceextract,0,
                                                 nlParams.get("Norm abs vel", 1.0e-6),
                                                 NOX::FSI::PartialNormUpdate::Scaled));

  AddStatusTest(interfaceTest);
  interfacecombo->addStatusTest(interfaceTest);

  converged->addStatusTest(interfacecombo);

  // setup tests for fluid velocities

  std::vector<Teuchos::RCP<const Epetra_Map> > fluidvel;
  fluidvel.push_back(FluidField().InnerVelocityRowMap());
  fluidvel.push_back(Teuchos::null);
  LINALG::MultiMapExtractor fluidvelextract(*DofRowMap(),fluidvel);

  Teuchos::RCP<NOX::StatusTest::Combo> fluidvelcombo =
    Teuchos::rcp(new NOX::StatusTest::Combo(NOX::StatusTest::Combo::OR));

  Teuchos::RCP<NOX::FSI::PartialNormF> innerFluidVel =
    Teuchos::rcp(new NOX::FSI::PartialNormF("velocity",
                                            fluidvelextract,0,
                                            nlParams.get("Norm abs vel", 1.0e-6),
                                            NOX::Abstract::Vector::TwoNorm,
                                            NOX::FSI::PartialNormF::Scaled));
  Teuchos::RCP<NOX::FSI::PartialNormUpdate> innerFluidVelUpdate =
    Teuchos::rcp(new NOX::FSI::PartialNormUpdate("velocity update",
                                                 fluidvelextract,0,
                                                 nlParams.get("Norm abs vel", 1.0e-6),
                                                 NOX::FSI::PartialNormUpdate::Scaled));

  AddStatusTest(innerFluidVel);
  fluidvelcombo->addStatusTest(innerFluidVel);
  //fluidvelcombo->addStatusTest(innerFluidVelUpdate);

  converged->addStatusTest(fluidvelcombo);

  // setup tests for fluid pressure

  std::vector<Teuchos::RCP<const Epetra_Map> > fluidpress;
  fluidpress.push_back(FluidField().PressureRowMap());
  fluidpress.push_back(Teuchos::null);
  LINALG::MultiMapExtractor fluidpressextract(*DofRowMap(),fluidpress);

  Teuchos::RCP<NOX::StatusTest::Combo> fluidpresscombo =
    Teuchos::rcp(new NOX::StatusTest::Combo(NOX::StatusTest::Combo::OR));

  Teuchos::RCP<NOX::FSI::PartialNormF> fluidPress =
    Teuchos::rcp(new NOX::FSI::PartialNormF("pressure",
                                            fluidpressextract,0,
                                            nlParams.get("Norm abs pres", 1.0e-6),
                                            NOX::Abstract::Vector::TwoNorm,
                                            NOX::FSI::PartialNormF::Scaled));
  Teuchos::RCP<NOX::FSI::PartialNormUpdate> fluidPressUpdate =
    Teuchos::rcp(new NOX::FSI::PartialNormUpdate("pressure update",
                                                 fluidpressextract,0,
                                                 nlParams.get("Norm abs pres", 1.0e-6),
                                                 NOX::FSI::PartialNormUpdate::Scaled));

  AddStatusTest(fluidPress);
  fluidpresscombo->addStatusTest(fluidPress);
  //fluidpresscombo->addStatusTest(fluidPressUpdate);

  converged->addStatusTest(fluidpresscombo);

  return combo;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::MortarMonolithicStructureSplit::ExtractFieldVectors(Teuchos::RCP<const Epetra_Vector> x,
                                                        Teuchos::RCP<const Epetra_Vector>& sx,
                                                        Teuchos::RCP<const Epetra_Vector>& fx,
                                                        Teuchos::RCP<const Epetra_Vector>& ax)
{
  TEUCHOS_FUNC_TIME_MONITOR("FSI::MortarMonolithicStructureSplit::ExtractFieldVectors");
  fx = Extractor().ExtractVector(x,1);
  Teuchos::RCP<LINALG::SparseMatrix> mortar = coupsfm_.GetMortarTrafo();

  // process structure unknowns

  Teuchos::RCP<Epetra_Vector> fcx = FluidField().Interface().ExtractFSICondVector(fx);
  FluidField().VelocityToDisplacement(fcx);
  Teuchos::RCP<const Epetra_Vector> sox = Extractor().ExtractVector(x,0);
  Teuchos::RCP<Epetra_Vector> scx = LINALG::CreateVector(*StructureField().Interface().FSICondMap());
  mortar->Apply(*fcx,*scx);
  Teuchos::RCP<Epetra_Vector> s = StructureField().Interface().InsertOtherVector(sox);
  StructureField().Interface().InsertFSICondVector(scx, s);
  sx = s;
  // process ale unknowns

  Teuchos::RCP<const Epetra_Vector> aox = Extractor().ExtractVector(x,2);
  Teuchos::RCP<Epetra_Vector> acx = icoupfa_.MasterToSlave(fcx);
  
  if (aleproj_!= INPAR::FSI::ALEprojection_none)
    acx->Update(1.0,*icoupfa_.MasterToSlave(iprojdispinc_),1.0);
  
  Teuchos::RCP<Epetra_Vector> a = AleField().Interface().InsertOtherVector(aox);
  AleField().Interface().InsertFSICondVector(acx, a);

  // if there is a free surface
  if (FluidField().Interface().FSCondRelevant())
  {
    Teuchos::RCP<Epetra_Vector> fcx = FluidField().Interface().ExtractFSCondVector(fx);
    FluidField().FreeSurfVelocityToDisplacement(fcx);

    Teuchos::RCP<Epetra_Vector> acx = fscoupfa_.MasterToSlave(fcx);
    AleField().Interface().InsertFSCondVector(acx, a);
  }

  ax = a;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::MortarMonolithicStructureSplit::StructToFluid
(
  Teuchos::RCP<Epetra_Vector> iv
) const
{
  dserror("Do not try to tranform slave (struct) to master (fluid)!");
  return coupsfm_.SlaveToMaster(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::MortarMonolithicStructureSplit::FluidToStruct
(
  Teuchos::RCP<Epetra_Vector> iv
) const
{
  return coupsfm_.MasterToSlave(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::MortarMonolithicStructureSplit::StructToFluid
(
  Teuchos::RCP<const Epetra_Vector> iv
) const
{
  dserror("Do not try to tranform slave (struct) to master (fluid)!");
  return coupsfm_.SlaveToMaster(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::MortarMonolithicStructureSplit::FluidToStruct
(
  Teuchos::RCP<const Epetra_Vector> iv
) const
{
  return coupsfm_.MasterToSlave(iv);
}

#endif
