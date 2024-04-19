/*----------------------------------------------------------------------*/
/*! \file
\brief Implementation

\level 0

*----------------------------------------------------------------------*/

#include "baci_linalg_sparsematrixbase.hpp"

#include "baci_linalg_utils_sparse_algebra_math.hpp"
#include "baci_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::SetUseTranspose(bool UseTranspose)
{
  return sysmat_->SetUseTranspose(UseTranspose);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::Apply(const Epetra_MultiVector& X, Epetra_MultiVector& Y) const
{
  return sysmat_->Apply(X, Y);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::ApplyInverse(
    const Epetra_MultiVector& X, Epetra_MultiVector& Y) const
{
  return sysmat_->ApplyInverse(X, Y);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool CORE::LINALG::SparseMatrixBase::UseTranspose() const { return sysmat_->UseTranspose(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool CORE::LINALG::SparseMatrixBase::HasNormInf() const { return sysmat_->HasNormInf(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Epetra_Comm& CORE::LINALG::SparseMatrixBase::Comm() const { return sysmat_->Comm(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Epetra_Map& CORE::LINALG::SparseMatrixBase::OperatorDomainMap() const
{
  return sysmat_->OperatorDomainMap();
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Epetra_Map& CORE::LINALG::SparseMatrixBase::OperatorRangeMap() const
{
  return sysmat_->OperatorRangeMap();
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::MaxNumEntries() const { return sysmat_->MaxNumEntries(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double CORE::LINALG::SparseMatrixBase::NormInf() const { return sysmat_->NormInf(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double CORE::LINALG::SparseMatrixBase::NormOne() const { return sysmat_->NormOne(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double CORE::LINALG::SparseMatrixBase::NormFrobenius() const { return sysmat_->NormFrobenius(); }


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::Multiply(
    bool TransA, const Epetra_Vector& x, Epetra_Vector& y) const
{
  return sysmat_->Multiply(TransA, x, y);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::Multiply(
    bool TransA, const Epetra_MultiVector& X, Epetra_MultiVector& Y) const
{
  return sysmat_->Multiply(TransA, X, Y);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::LeftScale(const Epetra_Vector& x)
{
  return sysmat_->LeftScale(x);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::RightScale(const Epetra_Vector& x)
{
  return sysmat_->RightScale(x);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::PutScalar(double ScalarConstant)
{
  return sysmat_->PutScalar(ScalarConstant);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::Scale(double ScalarConstant)
{
  return sysmat_->Scale(ScalarConstant);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::ReplaceDiagonalValues(const Epetra_Vector& Diagonal)
{
  return sysmat_->ReplaceDiagonalValues(Diagonal);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::ReplaceRowMap(const Epetra_BlockMap& newmap)
{
  const int err = sysmat_->ReplaceRowMap(newmap);
  if (err) return err;

  // see method description
  const_cast<Epetra_BlockMap&>(sysmat_->Map()) = newmap;

  return 0;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
int CORE::LINALG::SparseMatrixBase::ExtractDiagonalCopy(Epetra_Vector& Diagonal) const
{
  return sysmat_->ExtractDiagonalCopy(Diagonal);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CORE::LINALG::SparseMatrixBase::Add(const CORE::LINALG::SparseOperator& A,
    const bool transposeA, const double scalarA, const double scalarB)
{
  A.AddOther(*this, transposeA, scalarA, scalarB);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CORE::LINALG::SparseMatrixBase::AddOther(CORE::LINALG::SparseMatrixBase& B,
    const bool transposeA, const double scalarA, const double scalarB) const
{
  // B.Add(*this, transposeA, scalarA, scalarB);
  CORE::LINALG::Add(*sysmat_, transposeA, scalarA, B, scalarB);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CORE::LINALG::SparseMatrixBase::AddOther(CORE::LINALG::BlockSparseMatrixBase& B,
    const bool transposeA, const double scalarA, const double scalarB) const
{
  FOUR_C_THROW("BlockSparseMatrix and SparseMatrix cannot be added");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool CORE::LINALG::SparseMatrixBase::IsDbcApplied(
    const Epetra_Map& dbcmap, bool diagonalblock, const CORE::LINALG::SparseMatrix* trafo) const
{
  if (not Filled()) FOUR_C_THROW("The matrix must be filled!");

  const int numdbcrows = dbcmap.NumMyElements();
  const int* dbcrows = dbcmap.MyGlobalElements();

  std::vector<int> gIndices(sysmat_->MaxNumEntries(), 0);
  std::vector<int> gtIndices((trafo ? trafo->MaxNumEntries() : 0), 0);

  bool isdbc = true;

  for (int i = 0; i < numdbcrows; ++i)
  {
    const int row = dbcrows[i];
    const int sys_rlid = sysmat_->RowMap().LID(row);

    // this can happen for blocks of a BlockSparseMatrix
    if (sys_rlid == -1) continue;

    int NumEntries = 0;
    double* Values = nullptr;
    int* Indices = nullptr;
    sysmat_->ExtractMyRowView(sys_rlid, NumEntries, Values, Indices);

    std::fill(gIndices.begin(), gIndices.end(), 0.0);
    for (int c = 0; c < NumEntries; ++c) gIndices[c] = sysmat_->ColMap().GID(Indices[c]);

    // handle a diagonal block
    if (diagonalblock)
    {
      if (NumEntries == 0) FOUR_C_THROW("Row %d is empty and part of a diagonal block!", row);

      if (trafo)
      {
        if (not trafo->Filled()) FOUR_C_THROW("The trafo matrix must be filled!");

        int tNumEntries = 0;
        double* tValues = nullptr;
        int* tIndices = nullptr;

        const int trafo_rlid = trafo->RowMap().LID(row);
        trafo->EpetraMatrix()->ExtractMyRowView(trafo_rlid, tNumEntries, tValues, tIndices);

        // get the global indices corresponding to the extracted local indices
        std::fill(gtIndices.begin(), gtIndices.end(), 0.0);
        for (int c = 0; c < tNumEntries; ++c) gtIndices[c] = trafo->ColMap().GID(tIndices[c]);

        for (int j = 0; j < tNumEntries; ++j)
        {
          int k = -1;
          while (++k < NumEntries)
            if (Indices[k] == tIndices[j]) break;

          if (k == NumEntries)
            FOUR_C_THROW("Couldn't find column index %d in row %d.", tIndices[j], row);

          if (std::abs(Values[k] - tValues[j]) > std::numeric_limits<double>::epsilon())
          {
            isdbc = false;
            break;
          }
        }
      }
      // handle standard diagonal blocks
      // --> 1.0 on the diagonal
      // --> 0.0 on all off-diagonals
      else
      {
        for (int j = 0; j < NumEntries; ++j)
        {
          if (gIndices[j] != row and Values[j] > std::numeric_limits<double>::epsilon())
          {
            isdbc = false;
            break;
          }
          else if (gIndices[j] == row)
            if (std::abs(1.0 - Values[j]) > std::numeric_limits<double>::epsilon())
            {
              isdbc = false;
              break;
            }
        }
      }
    }
    // we expect only zeros on the off-diagonal blocks
    else
    {
      for (int j = 0; j < NumEntries; ++j)
      {
        if (Values[j] > std::numeric_limits<double>::epsilon())
        {
          isdbc = false;
          break;
        }
      }
    }

    // stop as soon as the initial status changed once
    if (not isdbc) break;
  }

  int lisdbc = static_cast<int>(isdbc);
  int gisdbc = 0;
  Comm().MinAll(&lisdbc, &gisdbc, 1);

  return (gisdbc == 1);
}

FOUR_C_NAMESPACE_CLOSE
