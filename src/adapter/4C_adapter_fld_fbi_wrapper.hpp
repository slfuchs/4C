/*----------------------------------------------------------------------*/
/*! \file

\brief Fluid field wrapper for fluid beam interactions

\level 2

*/
/*----------------------------------------------------------------------*/
#ifndef FOUR_C_ADAPTER_FLD_FBI_WRAPPER_HPP
#define FOUR_C_ADAPTER_FLD_FBI_WRAPPER_HPP

#include "4C_config.hpp"

#include "4C_adapter_fld_fluid_fsi.hpp"
#include "4C_fluid_meshtying.hpp"

#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

namespace CORE::LINALG
{
  class Solver;
  class SparseOperator;
}  // namespace CORE::LINALG

namespace IO
{
  class DiscretizationWriter;
}


namespace ADAPTER
{
  /*! \brief Fluid field adapter for fluid beam interaction
   *
   *
   *  Can only be used in conjunction with #FLD::FluidImplicitTimeInt
   */
  class FluidFBI : public FluidFSI
  {
   public:
    /// Constructor
    FluidFBI(Teuchos::RCP<Fluid> fluid, Teuchos::RCP<DRT::Discretization> dis,
        Teuchos::RCP<CORE::LINALG::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
        Teuchos::RCP<IO::DiscretizationWriter> output, bool isale, bool dirichletcond);

    /** \brief Pass in additional contributions from coupling terms for the system matrix
     *
     * To enforce weak dirichlet conditions as they arise from meshtying for example, such
     * contributions can be handed to the fluid, which will store the pointer on the coupling
     * contributions to assemble them into the system matrix in each Newton iteration.
     *
     * \param[in] matrix (size fluid_dof x fluid_dof) matrix containing weak dirichlet entries that
     * need to be assembled into the overall fluid system matrix
     */
    virtual void SetCouplingContributions(Teuchos::RCP<const CORE::LINALG::SparseOperator> matrix);

    /**
     * \brief Resets the external forces acting on the fluid to zero
     */
    virtual void ResetExternalForces();

    virtual Teuchos::RCP<const FLD::Meshtying> GetMeshtying();
  };
}  // namespace ADAPTER

FOUR_C_NAMESPACE_CLOSE

#endif
