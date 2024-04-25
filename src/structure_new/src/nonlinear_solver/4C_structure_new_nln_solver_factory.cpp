/*-----------------------------------------------------------*/
/*! \file

\brief Factory for nonlinear solvers in structural dynamics


\level 3

*/
/*-----------------------------------------------------------*/

#include "4C_structure_new_nln_solver_factory.hpp"

#include "4C_structure_new_nln_solver_fullnewton.hpp"
#include "4C_structure_new_nln_solver_nox.hpp"
#include "4C_structure_new_nln_solver_ptc.hpp"
#include "4C_structure_new_nln_solver_singlestep.hpp"
#include "4C_structure_new_nln_solver_uzawa.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::NLN::SOLVER::Factory::Factory()
{
  // empty
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::NLN::SOLVER::Generic> STR::NLN::SOLVER::Factory::BuildNlnSolver(
    const enum INPAR::STR::NonlinSolTech& nlnSolType) const
{
  Teuchos::RCP<STR::NLN::SOLVER::Generic> nlnSolver = Teuchos::null;

  switch (nlnSolType)
  {
    case INPAR::STR::soltech_newtonfull:
      nlnSolver = Teuchos::rcp(new STR::NLN::SOLVER::FullNewton());
      break;
    case INPAR::STR::soltech_nox_nln:
      nlnSolver = Teuchos::rcp(new STR::NLN::SOLVER::Nox());
      break;
    case INPAR::STR::soltech_ptc:
      nlnSolver = Teuchos::rcp(new STR::NLN::SOLVER::PseudoTransient());
      break;
    case INPAR::STR::soltech_singlestep:
      nlnSolver = Teuchos::rcp(new STR::NLN::SOLVER::SingleStep());
      break;
    case INPAR::STR::soltech_newtonuzawanonlin:
    case INPAR::STR::soltech_newtonuzawalin:
      //      nlnSolver = Teuchos::rcp(new STR::NLN::SOLVER::Uzawa());
      //      break;
    default:
      FOUR_C_THROW("Solution technique \"%s\" is not implemented.",
          INPAR::STR::NonlinSolTechString(nlnSolType).c_str());
      break;
  }

  return nlnSolver;
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<STR::NLN::SOLVER::Generic> STR::NLN::SOLVER::BuildNlnSolver(
    const enum INPAR::STR::NonlinSolTech& nlnSolType)
{
  Factory factory;
  return factory.BuildNlnSolver(nlnSolType);
}

FOUR_C_NAMESPACE_CLOSE
