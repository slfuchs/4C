/*!----------------------------------------------------------------------
\file solver_amgnxn_preconditioner.cpp

<pre>
Maintainer: Francesc Verdugo 
            verdugo@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15262
Created on: Feb 27, 2014
</pre>
*----------------------------------------------------------------------*/

#ifdef HAVE_MueLu
#ifdef HAVE_Trilinos_Q1_2014

#include <Xpetra_MultiVectorFactory.hpp>
#include <MueLu_MLParameterListInterpreter_decl.hpp>
#include <MueLu_ParameterListInterpreter.hpp> 
#include "../drt_lib/drt_dserror.H"
#include "solver_amgnxn_preconditioner.H" 



/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

LINALG::SOLVER::BlockSparseMatrixAUX::BlockSparseMatrixAUX
( 
    std::vector< Teuchos::RCP<SparseMatrix> > blocks,
    int rows,
    int cols,
    Epetra_DataAccess access
) : 
    rows_(rows),
    cols_(cols),
    blocks_(rows*cols,Teuchos::null),
    domainmaps_(Teuchos::null),
    rangemaps_(Teuchos::null),
    domainimporters_(cols,Teuchos::null),
    rangeimporters_(rows,Teuchos::null)
{

  // Check if the number of given of blocks is consistent with rows and cols
  // and decide what to do
  int NumBlocks = blocks.size();
  bool flag_all_blocks_are_given = false;
  if(NumBlocks==rows*cols)
    flag_all_blocks_are_given = true;
  else if(NumBlocks==rows and NumBlocks==cols)
    flag_all_blocks_are_given = false;
  else
    dserror("The number of given blocks is not consistent with the given number of block rows and columns");

  //Build up the block matrix
  if(flag_all_blocks_are_given) // All the blocks are given
    Setup(blocks,access);
  else // Only the diagonal blocks are given
  {
    std::vector< Teuchos::RCP<SparseMatrix> > blocks_all(Rows()*Cols(),Teuchos::null);
    FillWithZeroOfDiagonalBlocks(blocks,blocks_all);
    Setup(blocks_all,access);
  }

}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::BlockSparseMatrixAUX::FillWithZeroOfDiagonalBlocks(
   const std::vector< Teuchos::RCP<SparseMatrix> >& blocks,
         std::vector< Teuchos::RCP<SparseMatrix> >& blocks_all)
{

  // We assume that blocks.size() == Rows() == Cols() and blocks_all.size == Rows()*Cols()

  // Insert diagonal blocks
  for(int i=0;i<Rows();i++)
    blocks_all[i*Cols()+i]=blocks[i];

  // Build up off diagonal blocks and insert them
  for(int i=0;i<Rows();i++)
    for(int j=0;j<Cols();j++)
      if(i!=j)
      {
        const Epetra_Map& range_map_i  = blocks[i]->RangeMap();
        const Epetra_Map& domain_map_j = blocks[j]->DomainMap();
        Teuchos::RCP<SparseMatrix> block_ij = Teuchos::rcp(new SparseMatrix(range_map_i,1));
        block_ij->Zero();
        block_ij->Complete(domain_map_j,range_map_i);
        blocks_all[i*Cols()+j]=block_ij;
      }

  return;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::BlockSparseMatrixAUX::Setup(
    std::vector< Teuchos::RCP<SparseMatrix> > blocks,
    Epetra_DataAccess access)
{

  // Check if the number of given of blocks is consistent with rows and cols
  int NumBlocks = blocks.size();
  if(NumBlocks!=Rows()*Cols())
    dserror("The number of given blocks is not consistent with the given number of block rows and columns");

  // Create the blocks
  blocks_.assign(NumBlocks,Teuchos::null);
  for(int i=0;i<NumBlocks;i++)
  {
    if(blocks[i]==Teuchos::null)
      dserror("The supplied blocks cannot be null pointers");
    blocks_[i] = Teuchos::rcp(new SparseMatrix(*blocks[i],access));
  }

  // check that all the rows have the same range map
  for(int row=0;row<Rows();row++)
    for(int col=1;col<Cols();col++) 
      if(!((Matrix(row,col).RangeMap()).SameAs(Matrix(row,0).RangeMap())))
        dserror("The range map must be the same for all blocks in the same row");

  // check that all the cols have the same domain map
  for(int col=0;col<Cols();col++) 
    for(int row=1;row<Rows();row++)
      if(!((Matrix(row,col).DomainMap()).SameAs(Matrix(0,col).DomainMap())))
        dserror("The domain map must be the same for all blocks in the same col");

  //  We assume that the maps have unique GIDs
  for(int row=0;row<Rows();row++) 
    if(!Matrix(row,0).RangeMap().UniqueGIDs())
      dserror("At least on map in the given blocks is not unique");
  for(int col=0;col<Cols();col++) 
    if(!Matrix(0,col).DomainMap().UniqueGIDs())
      dserror("At least on map in the given blocks is not unique");

  // Compute the shift value
  std::vector<int> domainShifts(Cols(),-1);
  int domainStartPoint = 0;
  for(int col=0;col<Cols();col++) 
  {
    int MaxGID   = Matrix(0,col).DomainMap().MaxAllGID();
    int MinGID   = Matrix(0,col).DomainMap().MinAllGID();
    int TotalGID = Matrix(0,col).DomainMap().NumGlobalElements(); 
    if(TotalGID!=MaxGID-MinGID+1)
      dserror("The given maps cannot have gaps");
    domainShifts[col]= domainStartPoint - MinGID;
    domainStartPoint += TotalGID;
  }
  std::vector<int> rangeShifts(Rows(),-1);
  int rangeStartPoint = 0;
  for(int row=0;row<Rows();row++) 
  {
    int MaxGID   = Matrix(row,0).RangeMap().MaxAllGID();
    int MinGID   = Matrix(row,0).RangeMap().MinAllGID();
    int TotalGID = Matrix(row,0).RangeMap().NumGlobalElements(); 
    if(TotalGID!=MaxGID-MinGID+1)
      dserror("The given maps cannot have gaps");
    rangeShifts[row]= rangeStartPoint - MinGID;
    rangeStartPoint += TotalGID;
  }

  // Compute shifted map
  std::vector< Teuchos::RCP<const Epetra_Map> > domainmapsshifted(Cols(),Teuchos::null);
  for(int col=0;col<Cols();col++) 
    domainmapsshifted[col] = ComputeShiftedMap(Matrix(0,col).DomainMap(),domainShifts[col]);
  std::vector< Teuchos::RCP<const Epetra_Map> > rangemapsshifted(Rows(),Teuchos::null);
  for(int row=0;row<Rows();row++) 
    rangemapsshifted[row] = ComputeShiftedMap(Matrix(row,0).RangeMap(),rangeShifts[row]);

  // Create the multimap extractors using the shifted maps
  Teuchos::RCP<Epetra_Map> fullmap_domain = MultiMapExtractor::MergeMaps(domainmapsshifted);
  domainmaps_ = Teuchos::rcp(new MultiMapExtractor(*fullmap_domain ,domainmapsshifted ));
  Teuchos::RCP<Epetra_Map> fullmap_range  = MultiMapExtractor::MergeMaps(rangemapsshifted );
  rangemaps_  = Teuchos::rcp(new MultiMapExtractor(*fullmap_range  ,rangemapsshifted  ));

  // Create the importers between shifted and unshifted maps
  for(int col=0;col<Cols();col++) 
    domainimporters_[col]=
      Teuchos::rcp(new Epetra_Import(Matrix(0,col).DomainMap(),*domainmapsshifted[col]));
  for(int row=0;row<Rows();row++) 
  {
    // We don't understand why, but we need to copy the source map here!
    rangeimporters_[row] = 
      Teuchos::rcp(new Epetra_Import(*rangemapsshifted[row],
            CopyMapByHand(Matrix(row,0).RangeMap())));
  }
  return;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

Teuchos::RCP<Epetra_Map> LINALG::SOLVER::BlockSparseMatrixAUX::ComputeShiftedMap
  (const Epetra_Map& Map,int shift)
{
  std::vector<int> NewGlobalElements(Map.NumMyElements(),0);
  int myGID = 0;
  for(int i=0;i<(int)NewGlobalElements.size();i++)
  {
    myGID = Map.GID(i);
    if(myGID<0)
      dserror("wrong global id");
    NewGlobalElements[i] = shift + myGID;
  }
  Teuchos::RCP<Epetra_Map> New_Map = 
    Teuchos::rcp(new Epetra_Map(-1,Map.NumMyElements(),&NewGlobalElements[0],0,Map.Comm()));
  return New_Map;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

Epetra_Map LINALG::SOLVER::BlockSparseMatrixAUX::CopyMapByHand(const Epetra_Map& MapIn)
{
  std::vector<int> MyGlobalElements(MapIn.NumMyElements(),0);
  for(int i=0;i<MapIn.NumMyElements();i++)
    MyGlobalElements[i] = MapIn.GID(i);
  Epetra_Map MapOut(-1,MyGlobalElements.size(),&MyGlobalElements[0],0,MapIn.Comm());
  return MapOut;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

int LINALG::SOLVER::BlockSparseMatrixAUX::ApplyBlock
  (int r,int c, const Epetra_MultiVector &X, Epetra_MultiVector &Y) const
{

  Epetra_MultiVector XU(DomainMapUnShifted(c),X.NumVectors());
  Epetra_MultiVector YU(RangeMapUnShifted(r) ,Y.NumVectors());

  XU.Import(X,*domainimporters_[c],Insert);
  Matrix(r,c).Apply(XU,YU);
  Y.Import(YU,*rangeimporters_[r],Insert);

  return 0;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

int LINALG::SOLVER::BlockSparseMatrixAUX::Apply 
 (const Epetra_MultiVector &X, Epetra_MultiVector &Y) const
{

  int rows = Rows();
  int cols = Cols();
  Y.PutScalar(0.0);


  for (int rblock=0; rblock<rows; ++rblock)
  {
    Teuchos::RCP<Epetra_MultiVector> rowresult = rangemaps_->Vector(rblock,Y.NumVectors());
    Teuchos::RCP<Epetra_MultiVector> rowy      = rangemaps_->Vector(rblock,Y.NumVectors());
    for (int cblock=0; cblock<cols; ++cblock)
    {
      Teuchos::RCP<Epetra_MultiVector> colx = domainmaps_->ExtractVector(X,cblock);
      int err = ApplyBlock(rblock,cblock,*colx,*rowy);
      if (err!=0)
        dserror("failed to apply vector to matrix: err=%d",err);
      rowresult->Update(1.0,*rowy,1.0);
    }
    rangemaps_->InsertVector(*rowresult,rblock,Y);
  }

  return 0;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

Teuchos::RCP<LINALG::BlockSparseMatrixBase> 
  LINALG::SOLVER::FactoryBlockSparseMatrix::CreateBlockSparseMatrix
(
  std::vector< Teuchos::RCP<SparseMatrix> > blocks,
  int rows,
  int cols,
  Epetra_DataAccess access,
  bool explicitdirichlet,
  bool savegraph
)
{
  // Check if the number of given of blocks is consistent with rows and cols
  int NumBlocks = blocks.size();
  bool flag_all_blocks_are_given = false;
  if(NumBlocks==rows*cols)
    flag_all_blocks_are_given = true;
  else if(NumBlocks==rows and NumBlocks==cols)
    flag_all_blocks_are_given = false;
  else
    dserror("The number of given blocks is not consistent with the given number of block rows and columns");

  // Determine the estimated number of non zero entries per row
  int npr = 0;
  for(int i=0; i<(int)blocks.size();i++)
  {
    if(blocks[i]==Teuchos::null)
      dserror("The given blocks cannot be null pointers");
    if(blocks[i]->MaxNumEntries()>npr)
      npr = blocks[i]->MaxNumEntries(); 
  }

  // Some checks 
  if(flag_all_blocks_are_given)
  {
    // check that all the rows have the same range map
    for(int row=0;row<rows;row++)
      for(int col=1;col<cols;col++) 
        if(!((blocks[row*cols+col]->RangeMap()).SameAs(blocks[row*cols+0]->RangeMap())))
          dserror("The range map must be the same for all blocks in the same row");
    // check that all the cols have the same domain map
    for(int col=0;col<cols;col++) 
      for(int row=1;row<rows;row++)
        if(!((blocks[row*cols+col]->DomainMap()).SameAs(blocks[0*cols+col]->DomainMap())))
          dserror("The domain map must be the same for all blocks in the same col");
  }

  // build the partial and full domain maps  
  std::vector< Teuchos::RCP<const Epetra_Map> > domain_maps(cols,Teuchos::null);  
  for(int i=0;i<cols;i++)
    if(flag_all_blocks_are_given)
      domain_maps[i]= Teuchos::rcp(new Epetra_Map(blocks[0*cols+i]->DomainMap())); 
  // we assume the rest of rows are consistent. 
    else
      domain_maps[i]= Teuchos::rcp(new Epetra_Map(blocks[i]->DomainMap()));
  Teuchos::RCP<Epetra_Map> fullmap_domain = MultiMapExtractor::MergeMaps(domain_maps);
  Teuchos::RCP<MultiMapExtractor> domainmaps  = 
    Teuchos::rcp(new MultiMapExtractor(*fullmap_domain ,domain_maps ));

  // build the partial and full range maps 
  std::vector< Teuchos::RCP<const Epetra_Map> > range_maps (rows,Teuchos::null);  
  for(int i=0;i<rows;i++)
    if(flag_all_blocks_are_given)
      range_maps[i] = Teuchos::rcp(new Epetra_Map(blocks[i*cols+0]->RangeMap()));
  // we assume the rest of cols are consistent. 
    else
      range_maps[i] = Teuchos::rcp(new Epetra_Map(blocks[i]->RangeMap()));
  Teuchos::RCP<Epetra_Map> fullmap_range  = MultiMapExtractor::MergeMaps(range_maps);
  Teuchos::RCP<MultiMapExtractor> rangemaps  = 
    Teuchos::rcp(new MultiMapExtractor(*fullmap_range ,range_maps ));

  // Create the concrete matrix
  Teuchos::RCP<LINALG::BlockSparseMatrixBase> the_matrix = 
    Teuchos::rcp( new LINALG::BlockSparseMatrix<LINALG::DefaultBlockMatrixStrategy>(
          *domainmaps,
          *rangemaps,
          npr,
          explicitdirichlet,
          savegraph));

  // Assign the blocks
  if(flag_all_blocks_are_given)
  {
    for(int i=0;i<rows;i++)
      for(int j=0;j<cols;j++)
        the_matrix->Assign(i,j,access,*(blocks[i*cols+j]));
  }
  else
  {
    for(int i=0;i<rows;i++)
      the_matrix->Assign(i,i,access,*(blocks[i]));
    // Do not forget to zero out the off-diagonal blocks!!!
    for(int i=0;i<rows;i++)
      for(int j=0;j<cols;j++)
        if(i!=j)
        {
          the_matrix->Matrix(i,j).Zero();
          the_matrix->Matrix(i,j).Scale(0.0);
        }
  }

  // Call complete 
  the_matrix->Complete();

  // Return
  return the_matrix;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

LINALG::SOLVER::Richardson_Vcycle_Operator::Richardson_Vcycle_Operator
(int NumLevels,int NumSweeps,double omega):
NumLevels_(NumLevels),
NumSweeps_(NumSweeps),
omega_(omega),
Avec_(NumLevels,Teuchos::null),
Pvec_(NumLevels-1,Teuchos::null),
Rvec_(NumLevels-1,Teuchos::null),
SvecPre_(NumLevels,Teuchos::null),
SvecPos_(NumLevels-1,Teuchos::null),
flag_set_up_A_(false),
flag_set_up_P_(false),
flag_set_up_R_(false),
flag_set_up_Pre_(false),
flag_set_up_Pos_(false)
{}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::SetOperators
  (std::vector< Teuchos::RCP<Epetra_Operator> > Avec)
{
  if((int)Avec.size()!=NumLevels_)
    dserror("Error in Setting Avec_: Size dismatch.");
  for(int i=0;i<NumLevels_;i++)
  {
    if(Avec[i]==Teuchos::null)
      dserror("Error in Setting Avec_: Null pointer.");
    Avec_[i]=Avec[i];
  }
  flag_set_up_A_ = true;
  return;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::SetProjectors
  (std::vector< Teuchos::RCP<Epetra_Operator> > Pvec)
{
  if((int)Pvec.size()!=NumLevels_-1)
    dserror("Error in Setting Pvec_: Size dismatch.");
  for(int i=0;i<NumLevels_-1;i++)
  {
    if(Pvec[i]==Teuchos::null)
      dserror("Error in Setting Pvec_: Null pointer.");
    Pvec_[i]=Pvec[i];
  }
  flag_set_up_P_ = true;
  return;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::SetRestrictors
  (std::vector< Teuchos::RCP<Epetra_Operator> > Rvec)
{
  if((int)Rvec.size()!=NumLevels_-1)
    dserror("Error in Setting Rvec_: Size dismatch.");
  for(int i=0;i<NumLevels_-1;i++)
  {
    if(Rvec[i]==Teuchos::null)
      dserror("Error in Setting Rvec_: Null pointer.");
    Rvec_[i]=Rvec[i];
  }
  flag_set_up_R_ = true;
  return;
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::SetPreSmoothers
  (std::vector< Teuchos::RCP<LINALG::SOLVER::SmootherWrapperBase> > SvecPre)
{
  if((int)SvecPre.size()!=NumLevels_)
    dserror("Error in Setting SvecPre: Size dismatch.");
  for(int i=0;i<NumLevels_;i++)
  {
    if(SvecPre[i]==Teuchos::null)
      dserror("Error in Setting SvecPre: Null pointer.");
    SvecPre_[i]=SvecPre[i];
  }
  flag_set_up_Pre_ = true;
  return;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::SetPosSmoothers
  (std::vector< Teuchos::RCP<LINALG::SOLVER::SmootherWrapperBase> > SvecPos)
{
  if((int)SvecPos.size()!=NumLevels_-1)
    dserror("Error in Setting SvecPos: Size dismatch.");
  for(int i=0;i<NumLevels_-1;i++)
  {
    if(SvecPos[i]==Teuchos::null)
      dserror("Error in Setting SvecPos: Null pointer.");
    SvecPos_[i]=SvecPos[i];
  }
  flag_set_up_Pos_ = true;
  return;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::Vcycle(const Epetra_MultiVector& X,
                                                Epetra_MultiVector& Y, 
                                                int level, 
                                                bool InitialGuessIsZero) const
{

  if(level!=NumLevels_-1) // Perform one iteration of the V-cycle
  {
    // Apply presmoother 
    SvecPre_[level]->Apply(X,Y,InitialGuessIsZero);

    // Compute residual TODO optimize if InitialGuessIsZero == true 
    Epetra_MultiVector DX(X.Map(),X.NumVectors());
    Avec_[level]->Apply(Y,DX);
    DX.Update(1.0,X,-1.0);

    //  Create coarser representation of the residual
    Epetra_MultiVector DXcoarse(Rvec_[level]->OperatorRangeMap(),X.NumVectors());
    Rvec_[level]->Apply(DX,DXcoarse);

    // Damp error with coarser levels
    Epetra_MultiVector DYcoarse(Pvec_[level]->OperatorDomainMap(),X.NumVectors(),true);
    DYcoarse.PutScalar(0.0);
    Vcycle(DXcoarse,DYcoarse,level+1,true);

    // Compute correction
    Epetra_MultiVector DY(Y.Map(),X.NumVectors());
    Pvec_[level]->Apply(DYcoarse,DY);
    Y.Update(1.0,DY,1.0);

    // Apply post smoother 
    SvecPos_[level]->Apply(X,Y,false);
  }
  else // Apply presmoother
    SvecPre_[level]->Apply(X,Y,InitialGuessIsZero);


  return;
} //  Richardson_Vcycle_Operator::Vcycle

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::Richardson_Vcycle(const Epetra_MultiVector& X,
                                                           Epetra_MultiVector& Y, 
                                                           int start_level) const
{
  // Create auxiliary vectors
  Epetra_MultiVector Ytmp(Y.Map(),X.NumVectors(),true); // true May be not necessary TODO
  Epetra_MultiVector DX(X.Map(),X.NumVectors(),false);
  Epetra_MultiVector DY(Y.Map(),X.NumVectors(),false);

  for(int i=0;i<NumSweeps_;i++)
  {

    double scal_aux = (i==0)? 0.0 : 1.0;

    // Compute residual
    if(i!=0)
      Avec_[0]->Apply(Ytmp,DX);
    DX.Update(1.0,X,-1.0*scal_aux);

    // Apply V-cycle as preconditioner
    DY.PutScalar(0.0);
    Vcycle(DX,DY,start_level,true);

    // Apply correction
    Ytmp.Update(omega_,DY,scal_aux);

  }
  Y=Ytmp;
  return;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::Richardson_Vcycle_Operator::Apply(const Epetra_MultiVector& X,
                                               Epetra_MultiVector& Y, 
                                               int start_level) const
{
  // Check if everithing is set up
  if(!flag_set_up_A_)
    dserror("Operators missing");
  if(!flag_set_up_P_)
    dserror("Projectors missing");
  if(!flag_set_up_R_)
    dserror("Restrictors missing");
  if(!flag_set_up_Pre_)
    dserror("Pre-smoothers missing");
  if(!flag_set_up_Pos_)
    dserror("Post-smoothers missing");

// std::cout << "Richardson_Vcycle_Operator::Apply" << std::endl;

  // Work!
  Richardson_Vcycle(X,Y,start_level);
  return;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

LINALG::SOLVER::BlockSmootherWrapperBGS::BlockSmootherWrapperBGS
(
    //Teuchos::RCP<BlockSparseMatrixBase> A,
    Teuchos::RCP<BlockSparseMatrixAUX> A,
    std::vector< Teuchos::RCP<LINALG::SOLVER::NonBlockSmootherWrapperBase> > S,
    int global_iter,
    double global_omega,
    bool flip_order
)
:
A_(A),
S_(S),
global_iter_(global_iter),
global_omega_(global_omega),
NumBlocks_(A->Rows())
{

  // Some cheks
  if(NumBlocks_!=A_->Cols())
    dserror("The input matrix should be block square");
  if(NumBlocks_!=(int)S_.size())
    dserror("The number of matrix blocks does not coincide with the number of given smoothers");

  // Setup flip order
  index_order_.resize(NumBlocks_);
  if (!flip_order)
    for(int i=0;i<NumBlocks_;i++)
      index_order_[i]=i; /// 0,1,...,n-1
  else
    for(int i=0;i<NumBlocks_;i++)
      index_order_[i]=NumBlocks_ -i -1; /// n-1,n-2,...,0

} // LINALG::SOLVER::BlockSmootherWrapperBGS::BlockSmootherWrapperBGS


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::BlockSmootherWrapperBGS::RichardsonBGS(const Epetra_MultiVector& X,
                                                    Epetra_MultiVector& Y) const
{

  //==================================================================
  //
  // This routine implements several sweeps of a Richardson iteration
  // using a BGS preconditioner.
  //
  // Conceptually, It does the following in each iteration:
  //
  //    DX^k = X - A*Y^k;
  //    DY^k =(omega*L+D)^{-1} * DX^k
  //    Y^{k+1} = Y^k + omega*DY^k
  //
  // Where A = L + D + U is a decomposition of A in terms of the
  // lower/upper triangual blocks and diagonal blocks
  //
  // The actual implementation is a little bit different because we
  // want to solve for D^{-1} instead of (omega*L+D)^{-1}.
  //
  //==================================================================

  // Auxiliary: References to domain and map extractors
  const MultiMapExtractor& range_ex  = A_->RangeExtractor();
  const MultiMapExtractor& domain_ex = A_->DomainExtractor();

  // Extract a copy of each block of the initial guess vector
  std::vector< Teuchos::RCP<Epetra_MultiVector> > Y_blocks(NumBlocks_);
  for(int i=0;i<NumBlocks_;i++)
    Y_blocks[i] = domain_ex.ExtractVector(Y,i); 

  int i_fliped = -1;
  int j_fliped = -1;
  // Run several sweeps
  for(int k=0;k<global_iter_;k++)
  {

    // Loop in blocks
    for(int i=0;i<NumBlocks_;i++)
    {
      // Apply reordering
      i_fliped = index_order_[i];

      // Extract a copy of the block inside the rhs vector
      Teuchos::RCP<Epetra_MultiVector> X_i = range_ex.ExtractVector(X,i_fliped);

      // Create auxiliary vectors
      Teuchos::RCP<Epetra_MultiVector> tmpX_i =
        Teuchos::rcp( new Epetra_MultiVector(X_i->Map(),X.NumVectors()) );
      Teuchos::RCP<Epetra_MultiVector> tmpY_i = 
        Teuchos::rcp( new Epetra_MultiVector(Y_blocks[i_fliped]->Map(),X.NumVectors()));

      // Compute "residual"
      for(int j=0;j<NumBlocks_;j++)
      {
        j_fliped = index_order_[j];
        //const LINALG::SparseMatrix& Op_ij = A_->Matrix(i_fliped,j_fliped);
        //Op_ij.Apply(*(Y_blocks[j_fliped]),*tmpX_i);
        A_->ApplyBlock(i_fliped,j_fliped,*(Y_blocks[j_fliped]),*tmpX_i);
        X_i->Update(-1.0,*tmpX_i,1.0);
      }

      // Solve Diagonal block
      tmpY_i->PutScalar(0.0);
      S_[i_fliped]->Apply(*X_i,*tmpY_i,true);

      // Update
      Y_blocks[i_fliped]->Update(global_omega_,*tmpY_i,1.0);

    }// Loop in blocks

  }// Run several sweeps

  // Insert vectors in the right place
  for(int i=0;i<NumBlocks_;i++)
    domain_ex.InsertVector(*(Y_blocks[i]),i,Y);

  return;
} // LINALG::SOLVER::BlockSmootherWrapperBGS::RichardsonBGS


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::BlockSmootherWrapperBGS::Apply(const Epetra_MultiVector& X,
                                            Epetra_MultiVector& Y, 
                                            bool InitialGuessIsZero) const
{

  // TODO improve performance using InitialGuessIsZero

  // Work!
  RichardsonBGS(X,Y);

  return;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/



LINALG::SOLVER::AMGnxn_Operator::AMGnxn_Operator
(
  std::vector< Teuchos::RCP<Hierarchy>  > H,
  Teuchos::RCP<BlockSparseMatrixBase>     A,
  int                                     NumLevelAMG,
  int                                     NumSweepsAMG,
  double                                  omegaAMG,
  std::vector<int>                        NumSweepsPreSmoo,
  std::vector<double>                     omegaPreSmoo,     
  std::vector<bool>                       flipPreSmoo,      
  std::vector<int>                        NumSweepsPosSmoo,  
  std::vector<double>                     omegaPosSmoo,     
  std::vector<bool>                       flipPosSmoo      
)
: 
  H_(H),
  A_(A), 
  NumBlocks_(A->Rows()),
  NumSweepsAMG_(NumSweepsAMG),
  omegaAMG_(omegaAMG),       
  NumSweepsPreSmoo_(NumSweepsPreSmoo),
  omegaPreSmoo_(omegaPreSmoo),   
  flipPreSmoo_(flipPreSmoo),    
  NumSweepsPosSmoo_(NumSweepsPosSmoo),
  omegaPosSmoo_(omegaPosSmoo),   
  flipPosSmoo_(flipPosSmoo),    
  is_setup_flag_(false),
  P_(Teuchos::null),
  NumLevelMax_(-1000000),
  NumLevelMin_( 1000000),
  NumLevelAMG_(0),
  ALocal_   (1,1,Teuchos::null),
  PLocal_   (1,1,Teuchos::null),
  RLocal_   (1,1,Teuchos::null),
  SPreLocal_(1,1,Teuchos::null),
  SPosLocal_(1,1,Teuchos::null)
{

  // Determine the maximum and minimum number of levels
  if((int)H_.size()!=NumBlocks_)
    dserror("The number of provided Hierarchies should coincide with the number of diagonal blocks!");
  int NumLevel_block = 0;
  for(int block=0;block<NumBlocks_;block++)
  {
    NumLevel_block = H_[block]->GetNumberOfLevels();
    if(NumLevelMax_<NumLevel_block)
      NumLevelMax_=NumLevel_block;
    if(NumLevelMin_>NumLevel_block)
      NumLevelMin_=NumLevel_block;
  }

  // This is the number of Monolithic AMG Levels
  NumLevelAMG_ = std::min(NumLevelMin_,NumLevelAMG); 

  // Check if the supplied block smoother options have the right sizes
  if((int)NumSweepsPreSmoo_.size()<NumLevelAMG_)
    dserror("The number of sweeps for the block pre-smother should be given for all the levels!");
  if((int)omegaPreSmoo_.size()<NumLevelAMG_)
    dserror("The damping omega for the block pre-smother should be given for all the levels!");
  if((int)flipPreSmoo_.size()<NumLevelAMG_)
    dserror("The flip option for the block pre-smother should be given for all the levels!");
  if((int)NumSweepsPosSmoo_.size()<NumLevelAMG_-1)
    dserror("The number of sweeps for the block post-smother should be given for all the levels!");
  if((int)omegaPosSmoo_.size()<NumLevelAMG_-1)
    dserror("The damping omega for the block post-smother should be given for all the levels!");
  if((int)flipPosSmoo_.size()<NumLevelAMG_-1)
    dserror("The flip option for the block post-smother should be given for all the levels!");

  // Print some parameters. 
#ifdef DEBUG
  if (A->Comm().MyPID() == 0)
  {
    std::cout << "=========================================================" << std::endl;
    std::cout << " AMGnxn Parameters" << std::endl;
    std::cout << " NumLevelAMG_     = " << NumLevelAMG_ << std::endl;
    std::cout << " NumSweepsAMG_    = " << NumSweepsAMG_ << std::endl;
    std::cout << " omegaAMG_        = " << omegaAMG_ << std::endl;
    std::cout << " NumSweepsPreSmoo_= ";
    for(int i=0;i<NumLevelAMG_;i++)
      std::cout << NumSweepsPreSmoo_[i] << " ";
    std::cout << std::endl;
    std::cout << " omegaPreSmoo_    = ";
    for(int i=0;i<NumLevelAMG_;i++)
      std::cout << omegaPreSmoo_[i] << " ";
    std::cout << std::endl;
    std::cout << " flipPreSmoo_     = ";
    for(int i=0;i<NumLevelAMG_;i++)
      std::cout << flipPreSmoo_[i] << " ";
    std::cout << std::endl;
    std::cout << " NumSweepsPosSmoo_= ";
    for(int i=0;i<NumLevelAMG_-1;i++)
      std::cout << NumSweepsPosSmoo_[i] << " ";
    std::cout << std::endl;
    std::cout << " omegaPosSmoo_    = ";
    for(int i=0;i<NumLevelAMG_-1;i++)
      std::cout << omegaPosSmoo_[i] << " ";
    std::cout << std::endl;
    std::cout << " flipPosSmoo_     = ";
    for(int i=0;i<NumLevelAMG_-1;i++)
      std::cout << flipPosSmoo_[i] << " ";
    std::cout << std::endl;
    std::cout << "=======================================================" << std::endl;
  }
#endif

  // Setup the operator 
  SetUp();
}


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/
void  LINALG::SOLVER::AMGnxn_Operator::SetUp()
{

  //==========================================
  // Extract hierarchy operators
  //==========================================

  // Fetch some properties of the system block matrix
  bool explicitdirichlet = A_->Matrix(0,0).ExplicitDirichlet();
  bool savegraph         = A_->Matrix(0,0).SaveGraph();

  // The rows of with matrices will be associated
  // with the fields (or blocks) and the columns with the levels.
  // Not all the values in these matrices will be filled because the number of levels
  // in each field might differ.
  ALocal_.assign   (NumBlocks_,NumLevelMax_,Teuchos::null);
  PLocal_.assign   (NumBlocks_,NumLevelMax_,Teuchos::null);
  RLocal_.assign   (NumBlocks_,NumLevelMax_,Teuchos::null);
  SPreLocal_.assign(NumBlocks_,NumLevelMax_,Teuchos::null);
  SPosLocal_.assign(NumBlocks_,NumLevelMax_,Teuchos::null);

  // Auxiliary pointers
  Teuchos::RCP<Level>        this_level = Teuchos::null;
  Teuchos::RCP<Matrix>              myA = Teuchos::null;
  Teuchos::RCP<Epetra_CrsMatrix> myAcrs = Teuchos::null;
  Teuchos::RCP<SparseMatrix>     myAspa = Teuchos::null;
  Teuchos::RCP<SmootherBase>        myS = Teuchos::null;
  Teuchos::RCP<LINALG::SOLVER::NonBlockSmootherWrapperMueLu> mySWrap = Teuchos::null;

  // Loop in blocks 
  for(int block=0;block<NumBlocks_;block++)
  {
    int NumLevel_block = H_[block]->GetNumberOfLevels();

    // Loop in levels
    for(int level=0;level<NumLevel_block;level++)
    {
      this_level=H_[block]->GetLevel(level);

      // Extract operator
      if (this_level->IsAvailable("A"))
      {
        myA    = this_level->Get< Teuchos::RCP<Matrix> >("A"); //Matrix
        myAcrs = MueLu::Utils<double,int,int,Node,LocalMatOps>::Op2NonConstEpetraCrs(myA); 
        myAspa = Teuchos::rcp(new SparseMatrix(*myAcrs,explicitdirichlet,savegraph)); // TODO Copy?
        ALocal_[block][level]=myAspa;
      }
      else
        dserror("Error in extracting A");

      // Extract PreSmoother
      if (this_level->IsAvailable("PreSmoother"))
      {
        myS     = this_level->Get< Teuchos::RCP<SmootherBase> >("PreSmoother");
        mySWrap = Teuchos::rcp(new LINALG::SOLVER::NonBlockSmootherWrapperMueLu(myS));
        SPreLocal_[block][level]=mySWrap;
      }
      else
        dserror("Error in extracting PreSmoother");

      if(level<NumLevel_block-1)
      {
        // Extract postsmoother
        if (this_level->IsAvailable("PostSmoother"))
        {
          myS     = this_level->Get< Teuchos::RCP<SmootherBase> >("PostSmoother");
          mySWrap = Teuchos::rcp(new LINALG::SOLVER::NonBlockSmootherWrapperMueLu(myS));
          SPosLocal_[block][level]=mySWrap;
        }
        else
          dserror("Error in extracting PostSmoother");
      }

      if(level!=0)
      {

        // Extract prolongator
        if (this_level->IsAvailable("P"))
        {
          myA    = this_level->Get< Teuchos::RCP<Matrix> >("P");
          myAcrs = MueLu::Utils<double,int,int,Node,LocalMatOps>::Op2NonConstEpetraCrs(myA);
          myAspa = Teuchos::rcp(new SparseMatrix(*myAcrs,explicitdirichlet,savegraph)); 
          PLocal_[block][level-1]=myAspa;
        }
        else
          dserror("Error in extracting P");

        // Extract restrictor
        if (this_level->IsAvailable("R"))
        {
          myA = this_level->Get< Teuchos::RCP<Matrix> >("R");
          myAcrs =MueLu::Utils<double,int,int,Node,LocalMatOps>::Op2NonConstEpetraCrs(myA);
          myAspa = Teuchos::rcp(new SparseMatrix(*myAcrs,explicitdirichlet,savegraph)); 
          RLocal_[block][level-1]=myAspa;
        }
        else
          dserror("Error in extracting R");
      }

    }// Loop in levels

  }// Loop in blocks 


  //==========================================
  // Build coarser Matrix, Projections, Restrictions 
  // and smoothers. May be the bottle neck is here
  //==========================================

  // The number of monolithic AMG levels is the minimum number of levels in all the fields
  std::vector< Teuchos::RCP<BlockSparseMatrixAUX> > AGlobal  (NumLevelAMG_  ,Teuchos::null);  
  std::vector< Teuchos::RCP<BlockSparseMatrixAUX> > PGlobal  (NumLevelAMG_-1,Teuchos::null); 
  std::vector< Teuchos::RCP<BlockSparseMatrixAUX> > RGlobal  (NumLevelAMG_-1,Teuchos::null); 

  // build projectors and restrictors
  // Loop in levels
  for(int level=0;level<NumLevelAMG_-1;level++)
  {

    //Allocate vector containing the block diagonal blocks
    std::vector< Teuchos::RCP<SparseMatrix> > Pblocks(NumBlocks_,Teuchos::null);
    std::vector< Teuchos::RCP<SparseMatrix> > Rblocks(NumBlocks_,Teuchos::null);

    // The transfer operators are already computed by MueLu. 
    // Recover them and put them at the right place
    for(int block=0;block<NumBlocks_;block++)
    {
      Pblocks[block]=PLocal_[block][level];
      Rblocks[block]=RLocal_[block][level];
    }

    // Build the sparse Matrices
    PGlobal[level] = 
      Teuchos::rcp(new BlockSparseMatrixAUX(Pblocks,NumBlocks_,NumBlocks_,Copy)); //TODO copy?
    RGlobal[level] =
      Teuchos::rcp(new BlockSparseMatrixAUX(Rblocks,NumBlocks_,NumBlocks_,Copy)); //TODO copy?

  } // Loop in Levels


  // Build matrix
  // Loop in levels
  for(int level=0;level<NumLevelAMG_;level++)
  {
    if(level==0) // if fine level
     // AGlobal[level]= A_; //TODO Hypothesis: A_ and H_ are consistent
    {

      // Allocate vector containing all the individual blocks and fill it
      std::vector< Teuchos::RCP<SparseMatrix> > Ablocks(NumBlocks_*NumBlocks_,Teuchos::null);
      for(int row=0;row<NumBlocks_;row++) 
        for(int col=0;col<NumBlocks_;col++)
        {
          Teuchos::RCP<SparseMatrix> Aij = 
            Teuchos::rcp(new SparseMatrix(A_->Matrix(row,col),View)); 
          Ablocks[row*NumBlocks_+col] = Aij;
        }
      AGlobal[level] =
        Teuchos::rcp(new BlockSparseMatrixAUX(Ablocks,NumBlocks_,NumBlocks_,Copy)); //TODO copy?
    }
    else // if coarse levels
    {
      // Allocate vector containing all the individual blocks
      std::vector< Teuchos::RCP<SparseMatrix> > Ablocks(NumBlocks_*NumBlocks_,Teuchos::null);

      // The diagonal blocks are already computed by MueLu, thus we just have to fetch them
      // We store them in the diagonal positions corresponding to a row major matrix storage
      for(int block=0;block<NumBlocks_;block++)
        Ablocks[block*NumBlocks_+block] = ALocal_[block][level];

      // Compute and insert off diagonal blocks
      for(int row=0;row<NumBlocks_;row++)
        for(int col=0;col<NumBlocks_;col++)
        {
          if(row!=col) // The diagonal blocks have been already assigned
          {
            // The RAP multiplication. This might be the more expensive part!
            const SparseMatrix& A_spa   = AGlobal[level-1]->Matrix(row,col); 
            const SparseMatrix& P_spa   = PGlobal[level-1]->Matrix(col,col); 
            const SparseMatrix& R_spa   = RGlobal[level-1]->Matrix(row,row); 
            Teuchos::RCP<SparseMatrix>  AP_spa  = Teuchos::null;
            AP_spa  = LINALG::Multiply(A_spa,false,P_spa,false,true);
            if(AP_spa==Teuchos::null)
              dserror("Error in AP");
            Teuchos::RCP<SparseMatrix>  RAP_spa = Teuchos::null;
            RAP_spa = LINALG::Multiply(R_spa,false,*AP_spa,false,true);
            if(RAP_spa==Teuchos::null)
              dserror("Error in RAP");
            Ablocks[row*NumBlocks_+col] = RAP_spa; 
          }
        }

      // At this point the vector containing the blocks is filled!
      // Build the block sparse matrix
      AGlobal[level] =
        Teuchos::rcp(new BlockSparseMatrixAUX(Ablocks,NumBlocks_,NumBlocks_,Copy)); //TODO copy?

    } // if coarse levels
  } // Loop in levels

  //==========================================
  // Build block level Smothers
  //==========================================

  // To be filled
  std::vector< Teuchos::RCP<LINALG::SOLVER::BlockSmootherWrapperBase> > 
    SPreGlobal(NumLevelAMG_  ,Teuchos::null);
  std::vector< Teuchos::RCP<LINALG::SOLVER::BlockSmootherWrapperBase> > 
    SPosGlobal(NumLevelAMG_-1,Teuchos::null);

  // Auxiliary
  std::vector< Teuchos::RCP<LINALG::SOLVER::NonBlockSmootherWrapperBase> > 
    Svec(NumBlocks_,Teuchos::null);
  Teuchos::RCP<LINALG::SOLVER::BlockSmootherWrapperBGS> S_bgs = Teuchos::null;
  Teuchos::RCP<LINALG::SOLVER::NonBlockSmootherWrapperBase> S_base = Teuchos::null;
  Teuchos::RCP<LINALG::SOLVER::NonBlockSmootherAUX> S_aux = Teuchos::null;

  // Loop in levels
  for(int level=0;level<NumLevelAMG_;level++)
  {
    if(level<NumLevelAMG_-1) // fine levels
    {
      for(int block=0;block<NumBlocks_;block++)
       //  Svec[block] = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>
       //    (SPreLocal_[block][level]);
      {
        S_base = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>
          (SPreLocal_[block][level]);
        S_aux = Teuchos::rcp(new NonBlockSmootherAUX
            (S_base,AGlobal[level]->DomainImporters(block),AGlobal[level]->RangeImporters(block)));
        Svec[block] = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>(S_aux);
      }
      S_bgs = Teuchos::rcp(new BlockSmootherWrapperBGS(
            AGlobal[level],
            Svec,
            NumSweepsPreSmoo_[level],
            omegaPreSmoo_[level],
            flipPreSmoo_[level]));   
      SPreGlobal[level] =
        Teuchos::rcp_dynamic_cast<LINALG::SOLVER::BlockSmootherWrapperBase>(S_bgs);


      for(int block=0;block<NumBlocks_;block++)
      //  Svec[block] = 
      //    Teuchos::rcp_dynamic_cast<NonBlockSmootherWrapperBase>(SPosLocal_[block][level]); 
      {
        S_base = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>
          (SPosLocal_[block][level]);
        S_aux = Teuchos::rcp(new NonBlockSmootherAUX
            (S_base,AGlobal[level]->DomainImporters(block),AGlobal[level]->RangeImporters(block)));
        Svec[block] = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>(S_aux);
      }
      S_bgs = Teuchos::rcp(new BlockSmootherWrapperBGS(
            AGlobal[level],
            Svec,
            NumSweepsPosSmoo_[level],
            omegaPosSmoo_[level],
            flipPosSmoo_[level]));   
      SPosGlobal[level] = 
        Teuchos::rcp_dynamic_cast<LINALG::SOLVER::BlockSmootherWrapperBase>(S_bgs);

    }// fine levels
    else // Coarsest level
    {

      // Loop in blocks
      for(int block=0;block<NumBlocks_;block++)
      {
        if(H_[block]->GetNumberOfLevels() > NumLevelAMG_) //if this block has more levels
        {
          // We create a AMG V cycle using the remainder levels
          Teuchos::RCP<Richardson_Vcycle_Operator> myV = 
            CreateRemainingHierarchy(level,H_[block]->GetNumberOfLevels(),block);

          // We use the created AMG V cycle as coarse level smoother for this block
          Teuchos::RCP<SmootherWrapperVcycle> S_vcycle =
            Teuchos::rcp(new LINALG::SOLVER::SmootherWrapperVcycle(myV));

          //  Svec[block] = 
          //    Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>(S_vcycle);
          S_base = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>(S_vcycle);
          S_aux = Teuchos::rcp(new NonBlockSmootherAUX
              (S_base,AGlobal[level]->DomainImporters(block),AGlobal[level]->RangeImporters(block)));
          Svec[block] = 
            Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>(S_aux);

        }//if this block has more levels
        else
         //  Svec[block] = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>
         //    (SPreLocal_[block][level]);
        {
          S_base = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>
            (SPreLocal_[block][level]);
          S_aux = Teuchos::rcp(new NonBlockSmootherAUX(S_base,
                AGlobal[level]->DomainImporters(block),AGlobal[level]->RangeImporters(block)));
          Svec[block] = Teuchos::rcp_dynamic_cast<LINALG::SOLVER::NonBlockSmootherWrapperBase>
            (S_aux);
        }
      } // Loop in blocks

      // Create the coarse level smoother
      S_bgs =  Teuchos::rcp(new LINALG::SOLVER::BlockSmootherWrapperBGS(
            AGlobal[level],
            Svec,
            NumSweepsPreSmoo_[level],
            omegaPreSmoo_[level],
            flipPreSmoo_[level]));   
      SPreGlobal[level] = 
        Teuchos::rcp_dynamic_cast<LINALG::SOLVER::BlockSmootherWrapperBase>(S_bgs);

    }// Coarsest level

  } // Loop in levels

  //==========================================
  // Build up the AMG preconditioner
  //==========================================

  P_ = Teuchos::rcp(new LINALG::SOLVER::Richardson_Vcycle_Operator
      (NumLevelAMG_,NumSweepsAMG_,omegaAMG_));

  std::vector< Teuchos::RCP<Epetra_Operator> > AGlobal_ep_op(NumLevelAMG_,Teuchos::null);
  for(int i=0;i<NumLevelAMG_;i++)
    AGlobal_ep_op[i]=Teuchos::rcp_dynamic_cast<Epetra_Operator>(AGlobal[i]);
  P_->SetOperators  (AGlobal_ep_op);

  std::vector< Teuchos::RCP<Epetra_Operator> > PGlobal_ep_op(NumLevelAMG_-1,Teuchos::null);
  for(int i=0;i<NumLevelAMG_-1;i++)
    PGlobal_ep_op[i]=Teuchos::rcp_dynamic_cast<Epetra_Operator>(PGlobal[i]);
  P_->SetProjectors (PGlobal_ep_op);

  std::vector< Teuchos::RCP<Epetra_Operator> > RGlobal_ep_op(NumLevelAMG_-1,Teuchos::null);
  for(int i=0;i<NumLevelAMG_-1;i++)
    RGlobal_ep_op[i]=Teuchos::rcp_dynamic_cast<Epetra_Operator>(RGlobal[i]);
  P_->SetRestrictors(RGlobal_ep_op);

  std::vector< Teuchos::RCP<LINALG::SOLVER::SmootherWrapperBase> > 
    SPreGlobal_base(NumLevelAMG_,Teuchos::null);
  for(int i=0;i<NumLevelAMG_;i++)
    SPreGlobal_base[i]=
      Teuchos::rcp_dynamic_cast<LINALG::SOLVER::SmootherWrapperBase>(SPreGlobal[i]);
  P_->SetPreSmoothers (SPreGlobal_base);

  std::vector< Teuchos::RCP<LINALG::SOLVER::SmootherWrapperBase> > 
    SPosGlobal_base(NumLevelAMG_-1,Teuchos::null);
  for(int i=0;i<NumLevelAMG_-1;i++)
    SPosGlobal_base[i]=
      Teuchos::rcp_dynamic_cast<LINALG::SOLVER::SmootherWrapperBase>(SPosGlobal[i]);
  P_->SetPosSmoothers(SPosGlobal_base);


  //==========================================
  // We have finished
  //==========================================
  is_setup_flag_ = true;
  return;
} //AMGnxn_Operator::SetUp


/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/
Teuchos::RCP<LINALG::SOLVER::Richardson_Vcycle_Operator> 
  LINALG::SOLVER::AMGnxn_Operator::CreateRemainingHierarchy
  (int start_level,int num_levels,int block)
{


  int NumLevels_amg_block = num_levels - start_level;
  int NumSweeps_amg_block = 1; // TODO this value could be defined be the user
  double omega_amg_block = 1.0;// TODO this value could be defined be the user

  Teuchos::RCP<Richardson_Vcycle_Operator> myV = 
    Teuchos::rcp(new Richardson_Vcycle_Operator
         (NumLevels_amg_block,
          NumSweeps_amg_block,
          omega_amg_block));

  // Fetch building AMG operators for this block TODO check if we fetch the right things
  std::vector< Teuchos::RCP<Epetra_Operator> > Avec(NumLevels_amg_block,Teuchos::null);
  for(int i=0;i<NumLevels_amg_block;i++)
    Avec[i]=Teuchos::rcp_dynamic_cast<Epetra_Operator>(ALocal_[block][start_level+i]);

  std::vector< Teuchos::RCP<Epetra_Operator> > Pvec(NumLevels_amg_block-1,Teuchos::null);
  for(int i=0;i<NumLevels_amg_block-1;i++)
    Pvec[i]=Teuchos::rcp_dynamic_cast<Epetra_Operator>(PLocal_[block][start_level+i]);

  std::vector< Teuchos::RCP<Epetra_Operator> > Rvec(NumLevels_amg_block-1,Teuchos::null);
  for(int i=0;i<NumLevels_amg_block-1;i++)
    Rvec[i]=Teuchos::rcp_dynamic_cast<Epetra_Operator>(RLocal_[block][start_level+i]);

  std::vector< Teuchos::RCP<LINALG::SOLVER::SmootherWrapperBase> > 
    SvecPre(NumLevels_amg_block,Teuchos::null);
  for(int i=0;i<NumLevels_amg_block;i++)
    SvecPre[i]=Teuchos::rcp_dynamic_cast<LINALG::SOLVER::SmootherWrapperBase>
      (SPreLocal_[block][start_level+i]);

  std::vector< Teuchos::RCP<LINALG::SOLVER::SmootherWrapperBase> > 
    SvecPos(NumLevels_amg_block-1,Teuchos::null);
  for(int i=0;i<NumLevels_amg_block-1;i++)
    SvecPos[i]=Teuchos::rcp_dynamic_cast<LINALG::SOLVER::SmootherWrapperBase>
      (SPosLocal_[block][start_level+i]);

  // Fill the AMG v cycle operator
  myV->SetOperators  (Avec); 
  myV->SetProjectors (Pvec);
  myV->SetRestrictors(Rvec);
  myV->SetPreSmoothers (SvecPre);
  myV->SetPosSmoothers (SvecPos);


  return myV;
}



/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/


int  LINALG::SOLVER::AMGnxn_Operator::ApplyInverse(const Epetra_MultiVector &X,
                                           Epetra_MultiVector &Y) const
{
  if(!is_setup_flag_)
    dserror("ApplyInverse cannot be called without a previous set up of the preconditioner");
  P_->Apply(X,Y);
  return 0;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

LINALG::SOLVER::AMGnxn_Preconditioner::AMGnxn_Preconditioner
(
  FILE * outfile,
  Teuchos::ParameterList & params
) :
  LINALG::SOLVER::PreconditionerType( outfile ),
  params_(params)
{}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

void LINALG::SOLVER::AMGnxn_Preconditioner::Setup
(
  bool create,
  Epetra_Operator * matrix,
  Epetra_MultiVector * x,
  Epetra_MultiVector * b 
)
{
  //=================================================================
  // Preliminaries 
  //=================================================================

  // Setup underlying linear system
  SetupLinearProblem( matrix, x, b );

  // Decide if the setup has to be done
  if(!create)
    return;

  // Free old matrix and preconditioner
  A_ = Teuchos::null;
  P_ = Teuchos::null;

  // Create own copy of the system matrix
  BlockSparseMatrixBase* A_bl = dynamic_cast<BlockSparseMatrixBase*>(matrix);
  if(A_bl==NULL)
    dserror("The AMGnxn preconditioner works only for BlockSparseMatrixBase or derived classes");
  A_ = A_bl->Clone(Copy);

  // Determine number of blocks
  int NumBlocks = A_->Rows();
  if(A_->Rows() != A_->Cols())
    dserror("The AMGnxn preconditioner works only for block square matrices");

  //=================================================================
  // Build up MueLu Hierarchies of each one of the blocks
  //=================================================================

  //To be filled
  std::vector< Teuchos::RCP<Hierarchy>  > H(NumBlocks,Teuchos::null);
  // Auxiliary
  std::string Inverse_str = "Inverse";
  std::string xmlFileName;
  Teuchos::RCP<Epetra_Operator> A_eop;
  // loop in blocks
  for(int block=0;block<NumBlocks;block++)
  {

    // Pick up the operator
    A_eop = A_->Matrix(block,block).EpetraOperator();

    // Get the right sublist  and build
    if(!params_.isSublist(Inverse_str + ConvertInt(block+1)))
      dserror("Not found inverse list for block %d", block);
    Teuchos::ParameterList& inverse_list = params_.sublist(Inverse_str + ConvertInt(block+1));
    if(inverse_list.isSublist("MueLu Parameters"))
    {
      Teuchos::ParameterList& mllist = inverse_list.sublist("MueLu Parameters");
      H[block]=BuildMueLuHierarchy(mllist,A_eop,block,NumBlocks);
    }
    else if(inverse_list.isSublist("ML Parameters"))
    {
      Teuchos::ParameterList& mllist = inverse_list.sublist("ML Parameters");
      H[block]=BuildMueLuHierarchy(mllist,A_eop,block,NumBlocks);
    }
    else
      dserror("Not found MueLu Parameters nor ML Parameters for block %d", block+1);

  } // loop in blocks

  //=================================================================
  // Pick-up the input parameters 
  //=================================================================

  if(!params_.isSublist("AMGnxn Parameters"))
    dserror("AMGnxn Parameters not found!");
  Teuchos::ParameterList& amglist = params_.sublist("AMGnxn Parameters");

  int  NumLevelAMG = amglist.get<int>("maxlevel",0); 
  if(NumLevelAMG<1) 
    dserror("Error in recovering maxlevel");

  std::vector<int> NumSweepsPreSmoo;
  {
    Teuchos::RCP< std::vector<int> > ptr_aux = 
      amglist.get< Teuchos::RCP< std::vector<int> > >("smotimes",Teuchos::null); 
    if(ptr_aux == Teuchos::null)
      dserror("Error in recovering smotimes");
    else
      NumSweepsPreSmoo = *ptr_aux;
  }

  std::vector<bool> flipPreSmoo;      
  {
    Teuchos::RCP< std::vector<bool> > ptr_aux = 
      amglist.get< Teuchos::RCP< std::vector<bool> > >("smoflip",Teuchos::null); 
    if(ptr_aux == Teuchos::null)
      dserror("Error in recovering smoflip");
    else
      flipPreSmoo = *ptr_aux;
  }

  std::vector<double> omegaPreSmoo;     
  {
    Teuchos::RCP< std::vector<double> > ptr_aux = 
      amglist.get< Teuchos::RCP< std::vector<double> > >("smodamp",Teuchos::null); 
    if(ptr_aux == Teuchos::null)
      dserror("Error in recovering smodamp");
    else
      omegaPreSmoo = *ptr_aux;
  }

  //TODO now this is hard-coded. Supply it by dat file if required
  int                   NumSweepsAMG = 1; 
  double                omegaAMG = 1.0;      
  std::vector<int>      NumSweepsPosSmoo=NumSweepsPreSmoo;  
  std::vector<double>   omegaPosSmoo=omegaPreSmoo;     
  std::vector<bool>     flipPosSmoo=flipPreSmoo; 

  //=================================================================
  // Build up the preconditioner operator
  //=================================================================

  P_ = Teuchos::rcp(new AMGnxn_Operator(
        H,
        A_,
        NumLevelAMG,
        NumSweepsAMG,
        omegaAMG,
        NumSweepsPreSmoo,
        omegaPreSmoo,     
        flipPreSmoo,      
        NumSweepsPosSmoo,  
        omegaPosSmoo,     
        flipPosSmoo      
        ));

  // Finished
  return;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

Epetra_Operator * LINALG::SOLVER::AMGnxn_Preconditioner::PrecOperator() const 
{
  return &*P_;
}

/*------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------*/

// This function is inspired in LINALG::SOLVER::MueLuPreconditioner::Setup
Teuchos::RCP<Hierarchy> LINALG::SOLVER::AMGnxn_Preconditioner::BuildMueLuHierarchy
(
 Teuchos::ParameterList& mllist,
 Teuchos::RCP<Epetra_Operator> A_eop,
 int block,
 int NumBlocks
)
{
  //Pick up the right info in this list
  std::string xmlFileName = mllist.get<std::string>("xml file","none");
  int numdf = mllist.get<int>("PDE equations",-1);
  int dimns = mllist.get<int>("null space: dimension",-1);
  Teuchos::RCP<std::vector<double> > nsdata = 
    mllist.get<Teuchos::RCP<std::vector<double> > >("nullspace",Teuchos::null);

  //Some cheks
  if(numdf<1 or dimns<1)
    dserror("Error: PDE equations or null space dimension wrong.");
  if(nsdata==Teuchos::null)
    dserror("Error: null space data is empty");

  //Prepare operator for MueLu
  Teuchos::RCP<Epetra_CrsMatrix> A_crs = Teuchos::rcp_dynamic_cast<Epetra_CrsMatrix>(A_eop);
  if(A_crs==Teuchos::null)
    dserror("Make sure that the input matrix is a Epetra_CrsMatrix (or derived)");
  Teuchos::RCP<CrsMatrix> mueluA = Teuchos::rcp(new Xpetra::EpetraCrsMatrix(A_crs));
  Teuchos::RCP<CrsMatrixWrap> mueluA_wrap = Teuchos::rcp(new CrsMatrixWrap(mueluA));
  Teuchos::RCP<Matrix> mueluOp = Teuchos::rcp_dynamic_cast<Matrix>(mueluA_wrap);
  mueluOp->SetFixedBlockSize(numdf);

  // Prepare null space vector for MueLu
  Teuchos::RCP<const Xpetra::Map<LO,GO,NO> > rowMap = mueluA->getRowMap();
  Teuchos::RCP<MultiVector> nspVector = 
    Xpetra::MultiVectorFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::Build(rowMap,dimns,true);
  for ( size_t i=0; i < Teuchos::as<size_t>(dimns); i++) {
    Teuchos::ArrayRCP<Scalar> nspVectori = nspVector->getDataNonConst(i);
    const size_t myLength = nspVector->getLocalLength();
    for(size_t j=0; j<myLength; j++) {
      nspVectori[j] = (*nsdata)[i*myLength+j];
    }
  }

  // Build up hierarchy
  Teuchos::RCP<Hierarchy> H = Teuchos::null;
  if(xmlFileName != "none")
  {
#ifdef DEBUG
    if (A_eop->Comm().MyPID() == 0)
      std::cout << "AMGnxn Preconditioner in block " << block << " < " << NumBlocks 
        << " : Using XML file " << xmlFileName << std::endl; 
#endif
    ParameterListInterpreter mueLuFactory(xmlFileName,*(mueluOp->getRowMap()->getComm()));
    H = mueLuFactory.CreateHierarchy();
    H->SetDefaultVerbLevel(MueLu::Extreme); // TODO sure?
    H->GetLevel(0)->Set("A", mueluOp);
    H->GetLevel(0)->Set("Nullspace", nspVector);
    H->GetLevel(0)->setlib(Xpetra::UseEpetra);
    H->setlib(Xpetra::UseEpetra);
    mueLuFactory.SetupHierarchy(*H);
  }
  else
  { // TODO. This branch is not working yet

    dserror("The ML parameter list input for AMGnxn is not working yet. Use .xml files");
    //#if DEBUG
    //    std::cout << "AMGnxn Preconditioner in block " << block << " < " << NumBlocks 
    //      << " : Using ML parameter list" << std::endl; 
    //#endif
    //    mllist.remove("aggregation: threshold",false); // no support for aggregation: threshold
    //    MLParameterListInterpreter mueLuFactory(mllist/*, vec*/); // TODO vec?? 
    //    H = mueLuFactory.CreateHierarchy();
    //    H->SetDefaultVerbLevel(MueLu::Extreme); // TODO sure?
    //    //std::cout << " mueluOp->getGlobalNumEntries() = " << mueluOp->getGlobalNumEntries() << std::endl;
    //    H->GetLevel(0)->Set("A", mueluOp);
    //    H->GetLevel(0)->Set("Nullspace", nspVector);
    //    H->GetLevel(0)->setlib(Xpetra::UseEpetra);
    //    H->setlib(Xpetra::UseEpetra);
    //    mueLuFactory.SetupHierarchy(*H);
  }

  return H; 
}


#endif // HAVE_MueLu
#endif // HAVE_Trilinos_Q1_2014

