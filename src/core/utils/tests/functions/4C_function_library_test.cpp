/*---------------------------------------------------------------------------*/
/*! \file

\brief Unittests for the function library

\level 3
*/
/*----------------------------------------------------------------------*/

#include <gtest/gtest.h>

#include "4C_utils_cubic_spline_interpolation.hpp"
#include "4C_utils_function.hpp"
#include "4C_utils_function_library.hpp"

#include <fstream>

FOUR_C_NAMESPACE_OPEN

namespace
{
  class CubicSplineFromCSVTest : public ::testing::Test
  {
   protected:
    CubicSplineFromCSVTest()
    {
      const std::string csv_template_file_name = "template.csv";
      setup_template_csv_file(csv_template_file_name);

      cubic_spline_from_csv_ =
          Teuchos::rcp(new Core::UTILS::CubicSplineFromCSV(csv_template_file_name));
    }

    void setup_template_csv_file(const std::string& csv_template_file_name) const
    {
      std::ofstream test_csv_file{csv_template_file_name};
      // include header line
      test_csv_file << "#x,y" << '\n';
      // include four test lines
      test_csv_file << "0.30,4.40" << '\n';
      test_csv_file << "0.35,4.30" << '\n';
      test_csv_file << "0.40,4.25" << '\n';
      test_csv_file << "0.45,4.10" << '\n';
    }

    Teuchos::RCP<Core::UTILS::FunctionOfScalar> cubic_spline_from_csv_;
  };

  TEST_F(CubicSplineFromCSVTest, TestEvaluate)
  {
    const std::vector<double> x_test = {0.33, 0.36, 0.4, 0.42};
    const std::vector<double> solutions = {4.33232, 4.29, 4.25, 4.20152};

    for (std::size_t i = 0; i < x_test.size(); ++i)
      EXPECT_NEAR(cubic_spline_from_csv_->evaluate(x_test[i]), solutions[i], 1.0e-12);
  }

  TEST_F(CubicSplineFromCSVTest, TestEvaluateDerivative)
  {
    const std::vector<double> x_test = {0.33, 0.36, 0.4, 0.42};
    const std::vector<double> solutions = {-1.968, -8.4e-1, -1.8, -2.952};

    for (std::size_t i = 0; i < x_test.size(); ++i)
      EXPECT_NEAR(cubic_spline_from_csv_->evaluate_derivative(x_test[i], 1), solutions[i], 1.0e-12);
  }
}  // namespace
FOUR_C_NAMESPACE_CLOSE
