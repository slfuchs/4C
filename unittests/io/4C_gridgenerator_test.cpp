/*---------------------------------------------------------------------------*/
/*! \file

\brief Unittests for grid generator functionality

\level 1
*/
/*----------------------------------------------------------------------*/
#include <gtest/gtest.h>

#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_io_gridgenerator.hpp"
#include "4C_io_pstream.hpp"
#include "4C_mat_material_factory.hpp"
#include "4C_mat_par_bundle.hpp"
#include "4C_material_parameter_base.hpp"

#include <Epetra_SerialComm.h>

namespace
{
  using namespace FourC;

  void CreateMaterialInGlobalProblem()
  {
    Core::IO::InputParameterContainer mat_stvenant;
    mat_stvenant.add("YOUNG", 1.0);
    mat_stvenant.add("NUE", 0.1);
    mat_stvenant.add("DENS", 2.0);

    Global::Problem::instance()->materials()->insert(
        1, Mat::make_parameter(1, Core::Materials::MaterialType::m_stvenant, mat_stvenant));
  }

  class GridGeneratorTest : public ::testing::Test
  {
   public:
    GridGeneratorTest()
    {
      inputData_.bottom_corner_point_ = std::array<double, 3>{-1.0, -2.0, -3.0};
      inputData_.top_corner_point_ = std::array<double, 3>{2.5, 3.5, 4.5};
      inputData_.interval_ = std::array<int, 3>{5, 10, 15};
      inputData_.node_gid_of_first_new_node_ = 17;
    };

   protected:
    void SetUp() override
    {
      CreateMaterialInGlobalProblem();
      comm_ = Teuchos::rcp(new Epetra_SerialComm);
      Core::IO::cout.setup(false, false, false, Core::IO::standard, comm_, 0, 0, "dummyFilePrefix");
      testdis_ = Teuchos::rcp(new Core::FE::Discretization("dummy", comm_, 3));
    }

    void TearDown() override { Core::IO::cout.close(); }

   public:
    Core::IO::GridGenerator::RectangularCuboidInputs inputData_{};
    Teuchos::RCP<Core::FE::Discretization> testdis_;
    Teuchos::RCP<Epetra_Comm> comm_;
  };

  TEST_F(GridGeneratorTest, TestGridGeneratorWithHex8Elements)
  {
    inputData_.elementtype_ = "SOLID";
    inputData_.distype_ = "HEX8";
    inputData_.elearguments_ = "MAT 1 KINEM nonlinear";

    Core::IO::GridGenerator::CreateRectangularCuboidDiscretization(*testdis_, inputData_, true);

    testdis_->fill_complete(false, false, false);

    Core::Nodes::Node* lastNode = testdis_->l_row_node(testdis_->num_my_row_nodes() - 1);
    const auto nodePosition = lastNode->x();

    EXPECT_NEAR(nodePosition[0], 2.5, 1e-14);
    EXPECT_NEAR(nodePosition[1], 3.5, 1e-14);
    EXPECT_NEAR(nodePosition[2], 4.5, 1e-14);
    EXPECT_EQ(testdis_->num_my_row_nodes(), 1056);
    EXPECT_EQ(testdis_->num_my_row_elements(), 750);
    EXPECT_EQ(lastNode->id(), 7177);
  }

  TEST_F(GridGeneratorTest, TestGridGeneratorWithRotatedHex8Elements)
  {
    inputData_.elementtype_ = "SOLID";
    inputData_.distype_ = "HEX8";
    inputData_.elearguments_ = "MAT 1 KINEM nonlinear";
    inputData_.rotation_angle_ = std::array<double, 3>{30.0, 10.0, 7.0};

    Core::IO::GridGenerator::CreateRectangularCuboidDiscretization(*testdis_, inputData_, true);

    testdis_->fill_complete(false, false, false);

    Core::Nodes::Node* lastNode = testdis_->l_row_node(testdis_->num_my_row_nodes() - 1);
    const auto nodePosition = lastNode->x();

    EXPECT_NEAR(nodePosition[0], 2.6565639116964181, 1e-14);
    EXPECT_NEAR(nodePosition[1], 4.8044393443812901, 1e-14);
    EXPECT_NEAR(nodePosition[2], 2.8980306453470042, 1e-14);
    EXPECT_EQ(testdis_->num_my_row_nodes(), 1056);
    EXPECT_EQ(testdis_->num_my_row_elements(), 750);
    EXPECT_EQ(lastNode->id(), 7177);
  }

  TEST_F(GridGeneratorTest, TestGridGeneratorWithHex27Elements)
  {
    inputData_.elementtype_ = "SOLID";
    inputData_.distype_ = "HEX27";
    inputData_.elearguments_ = "MAT 1 KINEM nonlinear";

    Core::IO::GridGenerator::CreateRectangularCuboidDiscretization(*testdis_, inputData_, true);

    testdis_->fill_complete(false, false, false);

    Core::Nodes::Node* lastNode = testdis_->l_row_node(testdis_->num_my_row_nodes() - 1);
    const auto nodePosition = lastNode->x();

    EXPECT_NEAR(nodePosition[0], 2.5, 1e-14);
    EXPECT_NEAR(nodePosition[1], 3.5, 1e-14);
    EXPECT_NEAR(nodePosition[2], 4.5, 1e-14);
    EXPECT_EQ(testdis_->num_my_row_nodes(), 7161);
    EXPECT_EQ(testdis_->num_my_row_elements(), 750);
    EXPECT_EQ(lastNode->id(), 7177);
  }

  TEST_F(GridGeneratorTest, TestGridGeneratorWithWedge6Elements)
  {
    inputData_.elementtype_ = "SOLID";
    inputData_.distype_ = "WEDGE6";
    inputData_.elearguments_ = "MAT 1 KINEM nonlinear";
    inputData_.autopartition_ = true;

    Core::IO::GridGenerator::CreateRectangularCuboidDiscretization(*testdis_, inputData_, true);

    testdis_->fill_complete(false, false, false);

    Core::Nodes::Node* lastNode = testdis_->l_row_node(testdis_->num_my_row_nodes() - 1);
    const auto nodePosition = lastNode->x();

    EXPECT_NEAR(nodePosition[0], 2.5, 1e-14);
    EXPECT_NEAR(nodePosition[1], 3.5, 1e-14);
    EXPECT_NEAR(nodePosition[2], 4.5, 1e-14);
    EXPECT_EQ(testdis_->num_my_row_nodes(), 1056);
    EXPECT_EQ(testdis_->num_my_row_elements(), 1500);
    EXPECT_EQ(lastNode->id(), 7177);
  }

}  // namespace
