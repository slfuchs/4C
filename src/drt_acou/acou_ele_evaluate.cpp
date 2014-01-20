/*!
\file acou_ele_evaluate.cpp
\brief

<pre>
Maintainer: Svenja Schoeder
            schoeder@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15271
</pre>
*/

#include "acou_ele_factory.H"

#include "acou_ele_interface.H"
#include "acou_ele.H"

/*---------------------------------------------------------------------*
*----------------------------------------------------------------------*/
void DRT::ELEMENTS::AcouType::PreEvaluate(DRT::Discretization&                  dis,
                                            Teuchos::ParameterList&               p,
                                            Teuchos::RCP<LINALG::SparseOperator>  systemmatrix1,
                                            Teuchos::RCP<LINALG::SparseOperator>  systemmatrix2,
                                            Teuchos::RCP<Epetra_Vector>           systemvector1,
                                            Teuchos::RCP<Epetra_Vector>           systemvector2,
                                            Teuchos::RCP<Epetra_Vector>           systemvector3)
{
  return;
}

/*---------------------------------------------------------------------*
*----------------------------------------------------------------------*/
int DRT::ELEMENTS::Acou::Evaluate(Teuchos::ParameterList&            params,
                                    DRT::Discretization&      discretization,
                                    std::vector<int>&         lm,
                                    Epetra_SerialDenseMatrix& elemat1,
                                    Epetra_SerialDenseMatrix& elemat2,
                                    Epetra_SerialDenseVector& elevec1,
                                    Epetra_SerialDenseVector& elevec2,
                                    Epetra_SerialDenseVector& elevec3)
{
  Teuchos::RCP<MAT::Material> mat = Material();
  return DRT::ELEMENTS::AcouFactory::ProvideImpl(Shape())->Evaluate(
                this,
                discretization,
                lm,
                params,
                mat,
                elemat1,
                elemat2,
                elevec1,
                elevec2,
                elevec3);
}

/*---------------------------------------------------------------------*
*----------------------------------------------------------------------*/
int DRT::ELEMENTS::Acou::EvaluateNeumann(Teuchos::ParameterList&    params,
                                           DRT::Discretization&      discretization,
                                           DRT::Condition&           condition,
                                           std::vector<int>&         lm,
                                           Epetra_SerialDenseVector& elevec1,
                                           Epetra_SerialDenseMatrix* elemat1)
{
  return 0;
}


