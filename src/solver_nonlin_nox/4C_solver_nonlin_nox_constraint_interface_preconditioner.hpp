/*---------------------------------------------------------------------*/
/*! \file

\brief Preconditioner interface for constrained problems.



\level 3

*/
/*---------------------------------------------------------------------*/

#ifndef FOUR_C_SOLVER_NONLIN_NOX_CONSTRAINT_INTERFACE_PRECONDITIONER_HPP
#define FOUR_C_SOLVER_NONLIN_NOX_CONSTRAINT_INTERFACE_PRECONDITIONER_HPP

#include "4C_config.hpp"

#include "4C_solver_nonlin_nox_enum_lists.hpp"
#include "4C_solver_nonlin_nox_forward_decl.hpp"

#include <NOX_Epetra_Interface_Preconditioner.H>
#include <Teuchos_RCP.hpp>

#include <vector>

FOUR_C_NAMESPACE_OPEN

// forward declaration
namespace Core::LinAlg
{
  class Solver;
}

namespace NOX
{
  namespace Nln
  {
    namespace CONSTRAINT
    {
      namespace Interface
      {
        class Preconditioner;
      }  // namespace Interface
      // typedef
      typedef std::map<NOX::Nln::SolutionType, Teuchos::RCP<Interface::Preconditioner>>
          PrecInterfaceMap;

      namespace Interface
      {
        class Preconditioner : public ::NOX::Epetra::Interface::Preconditioner
        {
         public:
          /*! \brief Is the (CURRENT) system to solve a saddle-point system?
           *
           *  This check is supposed to return TRUE, only if the CURRENT system
           *  of equations is a saddle-point system. So in the case of inequality
           *  constraints, there is the possibility, that all constraints are
           *  inactive. In such a case the CURRENT system has no saddle-point shape
           *  and the function should return FALSE.
           *  Nevertheless, this may change during one of the following iterations!
           *
           *  \author Michael Hiermeier
           *  \date 04/2016 */
          virtual bool is_saddle_point_system() const = 0;

          /*! \brief Is the (CURRENT) system to solve a condensed system?
           *
           *  This check is supposed to return TRUE, only if the CURRENT system
           *  of equations involves any condensed quantities. So in the case of
           *  inequality constraints, there is the possibility, that all constraints
           *  are inactive. In such a case the CURRENT system needs no condensation
           *  and the function should return FALSE.
           *  Nevertheless, this may change during one of the following iterations!
           *
           *  \author Michael Hiermeier
           *  \date 04/2016 */
          virtual bool is_condensed_system() const = 0;

          //! Get necessary maps for the preconditioner.
          virtual void fill_maps_for_preconditioner(
              std::vector<Teuchos::RCP<Epetra_Map>>& maps) const = 0;

          //! Get the corresponding linear solver (optional)
          virtual Core::LinAlg::Solver* GetLinearSolver() const { return nullptr; };
        };
      }  // namespace Interface
    }    // namespace CONSTRAINT
  }      // namespace Nln
}  // namespace NOX

FOUR_C_NAMESPACE_CLOSE

#endif
