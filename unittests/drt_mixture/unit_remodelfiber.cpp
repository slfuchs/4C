/*----------------------------------------------------------------------*/
/*! \file
\brief Testcases for the remodel fiber
\level 3
*/
/*----------------------------------------------------------------------*/

#include <memory>
#include "gtest/gtest.h"

#include <Sacado.hpp>

#include "matpar_material.H"
#include "growth_evolution_linear_cauchy.H"
#include "remodelfiber.H"
#include "mixture_constituent_remodelfiber_material_exponential.H"
#include "remodelfiber-internal.H"

#include "linalg_fixedsizematrix.H"

#include "unittests_assertions.h"

namespace
{
  using FADdouble = Sacado::Fad::DFad<double>;

  class RemodelFiberTest : public ::testing::Test
  {
   protected:
    template <typename T>
    MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, T> GenerateFiber()
    {
      const auto container = Teuchos::rcp(new MAT::PAR::Material());
      container->Add("K1", 1.3);
      container->Add("K2", 1.3);
      container->Add("COMPRESSION", true);

      fiber_material_parameter_ =
          std::make_shared<MIXTURE::PAR::RemodelFiberMaterialExponential<FADdouble>>(container);

      const auto material =
          std::make_shared<const MIXTURE::RemodelFiberMaterialExponential<FADdouble>>(
              fiber_material_parameter_.get());
      const auto growth_evolution =
          std::make_shared<const MIXTURE::CauchyLinearGrowthEvolution<FADdouble>>(3.4);
      MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, T> fiber(
          material, growth_evolution, 12.0, 1.1);
      return fiber;
    }

    // parameters
    std::shared_ptr<MIXTURE::PAR::RemodelFiberMaterialExponential<FADdouble>>
        fiber_material_parameter_;
  };

  TEST_F(RemodelFiberTest, TestEvaluateDGrowthEvolutionEquationDtDGrowth)
  {
    MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, FADdouble> fiber =
        GenerateFiber<FADdouble>();

    const double lambda_f = 1.02;
    const double lambda_ext = 1.014;
    const FADdouble growth_scalar = FADdouble(2, 0, 1.12);
    const FADdouble lambda_r = FADdouble(2, 1, 1.05);

    const FADdouble y =
        fiber.EvaluateGrowthEvolutionEquationDt(lambda_f, lambda_r, lambda_ext, growth_scalar);
    const FADdouble dGrowthEvolutionEquationDtDGrowth =
        fiber.EvaluateDGrowthEvolutionEquationDtDGrowth(
            lambda_f, lambda_r, lambda_ext, growth_scalar);

    EXPECT_FLOAT_EQ(y.dx(0), dGrowthEvolutionEquationDtDGrowth.val());
  }

  TEST_F(RemodelFiberTest, TestEvaluateDGrowthEvolutionEquationDtDRemodel)
  {
    MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, FADdouble> fiber =
        GenerateFiber<FADdouble>();

    const double lambda_f = 1.02;
    const double lambda_ext = 1.014;
    const FADdouble growth_scalar = FADdouble(2, 0, 1.12);
    const FADdouble lambda_r = FADdouble(2, 1, 1.05);

    const FADdouble y =
        fiber.EvaluateGrowthEvolutionEquationDt(lambda_f, lambda_r, lambda_ext, growth_scalar);
    const FADdouble dGrowthEvolutionEquationDtDRemodel =
        fiber.EvaluateDGrowthEvolutionEquationDtDRemodel(
            lambda_f, lambda_r, lambda_ext, growth_scalar);

    EXPECT_FLOAT_EQ(y.dx(1), dGrowthEvolutionEquationDtDRemodel.val());
  }

  TEST_F(RemodelFiberTest, TestEvaluateDRemodelEvolutionEquationDtDGrowth)
  {
    MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, FADdouble> fiber =
        GenerateFiber<FADdouble>();

    const double lambda_f = 1.02;
    const double lambda_ext = 1.014;
    const FADdouble growth_scalar = FADdouble(2, 0, 1.12);
    const FADdouble lambda_r = FADdouble(2, 1, 1.05);

    const FADdouble y = fiber.EvaluateRemodelEvolutionEquationDt(lambda_f, lambda_r, lambda_ext);
    const FADdouble dRemodelEvolutionEquationDtDGrowth =
        fiber.EvaluateDRemodelEvolutionEquationDtDGrowth(lambda_f, lambda_r, lambda_ext);

    EXPECT_FLOAT_EQ(y.dx(0), dRemodelEvolutionEquationDtDGrowth.val());
  }

  TEST_F(RemodelFiberTest, TestEvaluateDRemodelEvolutionEquationDtDRemodel)
  {
    MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, FADdouble> fiber =
        GenerateFiber<FADdouble>();

    const double lambda_f = 1.02;
    const double lambda_ext = 1.014;
    const FADdouble growth_scalar = FADdouble(2, 0, 1.12);
    const FADdouble lambda_r = FADdouble(2, 1, 1.05);

    const FADdouble y = fiber.EvaluateRemodelEvolutionEquationDt(lambda_f, lambda_r, lambda_ext);
    const FADdouble dRemodelEvolutionEquationDtDRemodel =
        fiber.EvaluateDRemodelEvolutionEquationDtDRemodel(lambda_f, lambda_r, lambda_ext);

    EXPECT_FLOAT_EQ(y.dx(1), dRemodelEvolutionEquationDtDRemodel.val());
  }

  TEST_F(RemodelFiberTest, TestEvaluateDFiberCauchyStressDRemodel)
  {
    MIXTURE::IMPLEMENTATION::RemodelFiberImplementation<2, FADdouble> fiber =
        GenerateFiber<FADdouble>();

    const double lambda_f = 1.02;
    const double lambda_ext = 1.014;
    const FADdouble growth_scalar = FADdouble(2, 0, 1.12);
    const FADdouble lambda_r = FADdouble(2, 1, 1.05);

    const FADdouble y = fiber.EvaluateFiberCauchyStress(lambda_f, lambda_r, lambda_ext);
    const FADdouble dFiberCauchyStressDRemodel =
        fiber.EvaluateDFiberCauchyStressDRemodel(lambda_f, lambda_r, lambda_ext);

    EXPECT_FLOAT_EQ(y.dx(1), dFiberCauchyStressDRemodel.val());
  }
}  // namespace