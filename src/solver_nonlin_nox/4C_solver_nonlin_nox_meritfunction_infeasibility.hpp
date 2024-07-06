/*-----------------------------------------------------------*/
/*! \file

\brief Implementation of the infeasibility merit function for
       constrained problems. Especially useful for the filter method.



\level 3

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_SOLVER_NONLIN_NOX_MERITFUNCTION_INFEASIBILITY_HPP
#define FOUR_C_SOLVER_NONLIN_NOX_MERITFUNCTION_INFEASIBILITY_HPP

#include "4C_config.hpp"

#include "4C_solver_nonlin_nox_forward_decl.hpp"

#include <NOX_MeritFunction_Generic.H>

#include <map>

FOUR_C_NAMESPACE_OPEN

namespace NOX
{
  namespace Nln
  {
    namespace MeritFunction
    {
      enum MeritFctName : int;

      class Infeasibility : public ::NOX::MeritFunction::Generic
      {
        enum Type
        {
          type_vague,    //!< undefined type
          type_two_norm  //!< use a L2-norm of the infeasibility vector
        };

       public:
        /// constructor
        Infeasibility(const Teuchos::ParameterList& params, const ::NOX::Utils& u);

        //! Computes the merit function, \f$ f(x) \f$.
        double computef(const ::NOX::Abstract::Group& grp) const override;

        /*! Computes the gradient of the merit function, \f$ \nabla f \f$, and
         *  returns the result in the \c result vector. */
        void computeGradient(
            const ::NOX::Abstract::Group& group, ::NOX::Abstract::Vector& result) const override;

        /*! Computes the inner product of the given direction and the gradient
         *  associated with the merit function. Returns the steepest descent
         *  direction in the \c result vector. */
        double computeSlope(
            const ::NOX::Abstract::Vector& dir, const ::NOX::Abstract::Group& grp) const override;

        //! Compute the quadratic model,\f$ m(d) \f$, for the given merit function.
        double computeQuadraticModel(
            const ::NOX::Abstract::Vector& dir, const ::NOX::Abstract::Group& grp) const override;

        /*! Computes the vector in the steepest descent direction that minimizes
         *  the quadratic model. */
        void computeQuadraticMinimizer(
            const ::NOX::Abstract::Group& grp, ::NOX::Abstract::Vector& result) const override;

        //! Returns the name of the merit function.
        const std::string& name() const override;

        //! return the name of the merit function as enumerator
        enum MeritFctName type() const;

       private:
        /// \brief Get a list of currently supported infeasibility merit function types
        /** This list is a sub-list of the merit function enumerator list.
         *
         *  \author hiermeier \date 12/17 */
        std::map<std::string, MeritFctName> get_supported_type_list() const;

        /// Set the infeasibility merit function type
        void set_type(const std::string& type_name);

       private:
        enum MeritFctName infeasibility_type_;

        std::string merit_function_name_;
      };
    }  // namespace MeritFunction
  }    // namespace Nln
}  // namespace NOX

FOUR_C_NAMESPACE_CLOSE

#endif
