/*----------------------------------------------------------------------*/
/*! \file

\brief Unit tests for the fixed size matrix.

\level 0
*/


#include <gtest/gtest.h>

#include "4C_linalg_fixedsizematrix.hpp"
#include "4C_utils_fad.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  auto GetTestValuesAssignmentOperators()
  {
    static const unsigned int n_dim = 2;
    using fad_type = Sacado::Fad::DFad<double>;

    Core::LinAlg::Matrix<n_dim, 1, double> X;
    X(0) = 3.0;
    X(1) = 4.1;

    Core::LinAlg::Matrix<n_dim, 1, fad_type> u;
    u(0) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(n_dim, 0, 0.4);
    u(1) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(n_dim, 1, 0.3);

    Core::LinAlg::Matrix<n_dim, 1, fad_type> x_ref;
    x_ref(0) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(n_dim, 0, 3.4);
    x_ref(1) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(n_dim, 1, 4.4);

    return std::tuple{X, u, x_ref};
  };

  template <typename T, typename V>
  void CheckTestResults(const T& x, const V& x_ref)
  {
    const double eps = 1e-12;
    const unsigned int n_dim = x.num_rows();

    // Check the values of the result as well as the first derivatives
    for (unsigned int i = 0; i < n_dim; i++)
    {
      EXPECT_NEAR(Core::FADUtils::CastToDouble(x(i)), Core::FADUtils::CastToDouble(x_ref(i)), eps);
      for (unsigned int j = 0; j < (unsigned int)x(i).length(); j++)
      {
        EXPECT_NEAR(Core::FADUtils::CastToDouble(x(i).dx(j)),
            Core::FADUtils::CastToDouble(x_ref(i).dx(j)), eps);
      }
    }
  }

  TEST(FixedSizeMatrixTest, AssignmentOperatorPlusEqualDifferentTypes)
  {
    const auto [X, u, x_ref] = GetTestValuesAssignmentOperators();
    using result_type = typename std::decay<decltype(x_ref)>::type;
    result_type x;

    x = u;
    x += X;

    CheckTestResults(x, x_ref);
  }

  TEST(FixedSizeMatrixTest, AssignmentOperatorMinusEqualDifferentTypes)
  {
    const auto [X, u, x_ref] = GetTestValuesAssignmentOperators();
    using result_type = typename std::decay<decltype(x_ref)>::type;
    result_type x;

    x = u;
    x.scale(-1.0);
    x -= X;
    x.scale(-1.0);

    CheckTestResults(x, x_ref);
  }

  TEST(FixedSizeMatrixTest, UpdateDifferentTypes)
  {
    const auto [X, u, x_ref] = GetTestValuesAssignmentOperators();
    using result_type = typename std::decay<decltype(x_ref)>::type;
    result_type x;

    x.update(X);
    x += u;

    CheckTestResults(x, x_ref);
  }

  TEST(FixedSizeMatrixTest, MultiplyDifferentTypes)
  {
    using fad_type = Sacado::Fad::DFad<double>;

    Core::LinAlg::Matrix<2, 4> shape_function_matrix(true);
    shape_function_matrix(0, 0) = 0.75;
    shape_function_matrix(1, 1) = 0.75;
    shape_function_matrix(0, 2) = 0.25;
    shape_function_matrix(1, 3) = 0.25;

    Core::LinAlg::Matrix<4, 1, fad_type> nodal_dof;
    nodal_dof(0) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(4, 0, 0.4);
    nodal_dof(1) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(4, 1, 1.4);
    nodal_dof(2) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(4, 2, 2.4);
    nodal_dof(3) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(4, 3, 3.4);

    Core::LinAlg::Matrix<2, 1, fad_type> u;
    u.multiply(shape_function_matrix, nodal_dof);

    Core::LinAlg::Matrix<2, 1, fad_type> u_ref;
    u_ref(0) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(4, 0, 0.9);
    u_ref(1) = Core::FADUtils::HigherOrderFadValue<fad_type>::apply(4, 1, 1.9);
    u_ref(0).fastAccessDx(0) = 0.75;
    u_ref(0).fastAccessDx(2) = 0.25;
    u_ref(1).fastAccessDx(1) = 0.75;
    u_ref(1).fastAccessDx(3) = 0.25;

    CheckTestResults(u, u_ref);
  }
}  // namespace

FOUR_C_NAMESPACE_CLOSE
