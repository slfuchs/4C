/*----------------------------------------------------------------------*/
/*! \file

\brief xfem based fluid result tests

\level 0

 */
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_FLUID_XFLUID_RESULTTEST_HPP
#define FOUR_C_FLUID_XFLUID_RESULTTEST_HPP


#include "4C_config.hpp"

#include "4C_linalg_vector.hpp"
#include "4C_utils_result_test.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace FLD
{
  // forward declarations
  class XFluid;
  class XFluidFluid;
  class XFluidFluid;

  /*!
    ResultTest class for XFluid
   */
  class XFluidResultTest : public Core::UTILS::ResultTest
  {
   public:
    //! ctor for standard XFEM problems
    XFluidResultTest(const FLD::XFluid& xfluid);

    //! ctor for XFF-problems
    XFluidResultTest(const FLD::XFluidFluid& xfluid);

    /// our version of nodal value tests
    /*!
      Possible position flags are "velx", "vely", "velz" and
      "pressure". With the obvious meaning.
     */
    void test_node(
        const Core::IO::InputParameterContainer& container, int& nerr, int& test_count) override;

   private:
    /// nodal value test (one can specify discretization and corresponding solution here!)
    void test_node(const Core::IO::InputParameterContainer& container, int& nerr, int& test_count,
        int node, const Core::FE::Discretization& discret,
        const Core::LinAlg::Vector<double>& velnp);

    /// XFEM discretization
    Teuchos::RCP<const Core::FE::Discretization> discret_;

    /// solution vector for XFEM discretization
    Teuchos::RCP<const Core::LinAlg::Vector<double>> velnp_;

    /// optional additional discretization for the same field (fluid-fluid coupling)
    Teuchos::RCP<const Core::FE::Discretization> coupl_discret_;

    /// solution vector for additional coupling discretization
    Teuchos::RCP<const Core::LinAlg::Vector<double>> coupl_velnp_;

    /// take care of node numbering off-by-one (will be removed soon)
    const bool node_from_zero_;
  };

}  // namespace FLD

FOUR_C_NAMESPACE_CLOSE

#endif
