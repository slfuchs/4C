/*======================================================================*/
/*!
\file strutimada_ab2.cpp

\brief Adams-Bashforth2 time step indicator for time adaptivity

<pre>
Maintainer: Burkhard Bornemann
            bornemann@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15237
</pre>
*/

/*----------------------------------------------------------------------*/
/* definitions */
#ifdef CCADISCRET

/*----------------------------------------------------------------------*/
/* headers */
#include <iostream>

#include "strutimada_ab2.H"


/*----------------------------------------------------------------------*/
/* Slender constructor */
STR::StruTimAdaAB2::StruTimAdaAB2
(
  const Teuchos::ParameterList& ioparams,  //!< ioflags
  const Teuchos::ParameterList& sdynparams,  //!< TIS input parameters
  const Teuchos::ParameterList& xparams,  //!< extra flags
  const Teuchos::ParameterList& adaparams,  //!< adaptive input flags
  Teuchos::RCP<StruTimInt>& tis  //!< marching time integrator
)
: StruTimAda
  (
    sdynparams,
    adaparams,
    tis
  ),
  ab2_(Teuchos::null)
{
  // allocate Adams-Bashforth2 integrator
  ab2_ = Teuchos::rcp(new StruTimIntAB2(ioparams, sdynparams, xparams,
                                        tis->Discretization(), 
                                        tis->GetSolver(),
                                        tis->GetDiscretizationWriter()));

  // merge 
  ab2_->Merge(*tis);

  // resize multi-step quantities
  ab2_->ResizeMStep();
  
  // I am lost
  return;
}

/*----------------------------------------------------------------------*/
/* Provide local discretisation error */
void STR::StruTimAdaAB2::IntegrateStepAuxiliar()
{

  // get state vectors of marching integrator
  //const Teuchos::RCP<Epetra_Vector> dis = sti_->Disp();  // D_{n}^{A2}
  const Teuchos::RCP<Epetra_Vector> disn = sti_->Dispn();  // D_{n+1}^{A2}
  //const Teuchos::RCP<Epetra_Vector> vel = sti_->Vel();  // V_{n}^{A2}
  //const Teuchos::RCP<Epetra_Vector> acc = sti_->Acc();  // A_{n}^{A2}
  //const Teuchos::RCP<Epetra_Vector> accn = sti_->Accn();  // A_{n+1}^{A2}

  /*
  // build NM3* displacements D_{n+1}^{NM3*}
  // using the lower or equal than second order accurate new accelerations
  locerrdisn_->Update(1.0, *dis,
                      stepsize_, *vel,
                      0.0);
  locerrdisn_->Update(stepsize_*stepsize_/3.0, *acc,
                      stepsize_*stepsize_/6.0, *accn,
                      1.0);
  */
  
  // integrate
  ab2_->IntegrateStep();
  ab2_->ResetStep();
  // copy onto target
  locerrdisn_->Update(1.0, *(ab2_->disn_), 0.0);

  // provide local discretisation error vector
  // l_{n+1}^{A2} = D_{n+1}^{AB2} - D_{n+1}^{A1}
  //locerrdisn_->Update(-1.0, *disn, 1.0);

  // see you
  return;
}


/*----------------------------------------------------------------------*/
#endif  // #ifdef CCADISCRET
