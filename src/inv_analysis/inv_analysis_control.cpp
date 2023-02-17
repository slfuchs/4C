/*----------------------------------------------------------------------------*/
/*! \file
\brief Control object to handle solution of the inverse analysis

\level 3

*/

/*----------------------------------------------------------------------------*/
/* headers */
#include "inv_analysis_control.H"

// TEUCHOS
#include "Teuchos_ParameterList.hpp"

// INVANA
#include "inv_analysis_base.H"
#include "inv_analysis_factory.H"
#include "inv_analysis_resulttest.H"
#include "inv_analysis_writer.H"

// INTERNAL OPTIMIZER
#include "inv_analysis_optimizer_factory.H"
#include "inv_analysis_optimizer_base.H"

// BACI
#include "lib_dserror.H"
#include "inpar_statinvanalysis.H"
#include "lib_globalproblem.H"
#include "io.H"
#include "io_control.H"
#include "timestepping_mstep.H"


/*----------------------------------------------------------------------------*/
INVANA::InvanaControl::InvanaControl()
    : invprob_(Teuchos::null),
      invanaopt_(Teuchos::null),
      input_(Teuchos::null),
      x_(Teuchos::null),
      f_(Teuchos::null),
      val_(0.0)
{
  return;
}

/*----------------------------------------------------------------------------*/
void INVANA::InvanaControl::Init(const Teuchos::ParameterList& invp)
{
  // create an instance of an optimization problem
  INVANA::InvanaFactory invfac;
  invprob_ = invfac.Create(invp);

  // optimization algorithm
  INVANA::OptimizerFactory optimizerfac;
  invanaopt_ = optimizerfac.Create(invp);

  invanaopt_->Init(invprob_);
  invanaopt_->Setup();

  return;
}

int INVANA::InvanaControl::Solve(int restart)
{
  invanasolve(restart);

  return 0;
}

/*----------------------------------------------------------------------*/
void INVANA::InvanaControl::invanasolve(int restart)
{
  // solve
  if (restart) InvanaOpti()->ReadRestart(restart);
  InvanaOpti()->Integrate();

  // store solution
  f_ = Teuchos::rcp(new Epetra_MultiVector(InvanaOpti()->GetGradientView()));
  val_ = InvanaOpti()->GetObjFunctValView();
  return;
}

/*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::ResultTest> INVANA::InvanaControl::CreateFieldTest()
{
  return Teuchos::rcp(new INVANA::InvanaResultTest(*this));
}