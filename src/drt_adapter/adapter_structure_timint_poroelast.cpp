/*!------------------------------------------------------------------------------------------------*
 \file adapter_structure_timint_poroelast.cpp

 \brief Structure field adapter for poroelasticity

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *------------------------------------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
#ifdef CCADISCRET

/*----------------------------------------------------------------------*/
#include "adapter_structure_timint_poroelast.H"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::StructureTimIntImplPoro::Evaluate(Teuchos::RCP<
    const Epetra_Vector> disiterinc)
{
  structure_->UpdateIterIncrementally(disiterinc);

  // builds tangent, residual and applies DBC
  structure_->PoroEvaluateForceStiffResidual();
  structure_->PrepareSystemForNewtonSolve();
}

/*----------------------------------------------------------------------*/
#endif  // #ifdef CCADISCRET
