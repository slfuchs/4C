/*-----------------------------------------------------------*/
/*! \file

\brief Auxiliary methods.



\level 3

*/
/*-----------------------------------------------------------*/

#include "4C_solver_nonlin_nox_aux.hpp"

#include "4C_inpar_boolifyparameters.hpp"
#include "4C_linalg_blocksparsematrix.hpp"
#include "4C_solver_nonlin_nox_linearsystem.hpp"
#include "4C_solver_nonlin_nox_statustest_activeset.hpp"
#include "4C_solver_nonlin_nox_statustest_combo.hpp"
#include "4C_solver_nonlin_nox_statustest_normf.hpp"
#include "4C_solver_nonlin_nox_statustest_normupdate.hpp"
#include "4C_solver_nonlin_nox_statustest_normwrms.hpp"

#include <Epetra_Vector.h>
#include <NOX_Abstract_ImplicitWeighting.H>
#include <NOX_Observer_Vector.hpp>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void NOX::NLN::AUX::SetPrintingParameters(Teuchos::ParameterList& p_nox, const Epetra_Comm& comm)
{
  // make all Yes/No integral values to Boolean
  INPUT::BoolifyValidInputParameters(p_nox);

  // adjust printing parameter list
  Teuchos::ParameterList& printParams = p_nox.sublist("Printing");
  printParams.set<int>("MyPID", comm.MyPID());
  printParams.set<int>("Output Precision", 5);
  printParams.set<int>("Output Processor", 0);
  int outputinformationlevel = ::NOX::Utils::Error;  // ::NOX::Utils::Error==0
  if (printParams.get<bool>("Error", true)) outputinformationlevel += ::NOX::Utils::Error;
  if (printParams.get<bool>("Warning", true)) outputinformationlevel += ::NOX::Utils::Warning;
  if (printParams.get<bool>("Outer Iteration", true))
    outputinformationlevel += ::NOX::Utils::OuterIteration;
  if (printParams.get<bool>("Inner Iteration", true))
    outputinformationlevel += ::NOX::Utils::InnerIteration;
  if (printParams.get<bool>("Parameters", false))
    outputinformationlevel += ::NOX::Utils::Parameters;
  if (printParams.get<bool>("Details", false)) outputinformationlevel += ::NOX::Utils::Details;
  if (printParams.get<bool>("Outer Iteration StatusTest", true))
    outputinformationlevel += ::NOX::Utils::OuterIterationStatusTest;
  if (printParams.get<bool>("Linear Solver Details", false))
    outputinformationlevel += ::NOX::Utils::LinearSolverDetails;
  if (printParams.get<bool>("Test Details", false))
    outputinformationlevel += ::NOX::Utils::TestDetails;
  if (printParams.get<bool>("Debug", false)) outputinformationlevel += ::NOX::Utils::Debug;
  printParams.set("Output Information", outputinformationlevel);

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
NOX::NLN::LinSystem::OperatorType NOX::NLN::AUX::GetOperatorType(
    const CORE::LINALG::SparseOperator& op)
{
  const Epetra_Operator* testOperator = nullptr;

  // Is it a LINALG_BlockSparseMatrix
  testOperator = dynamic_cast<
      const CORE::LINALG::BlockSparseMatrix<CORE::LINALG::DefaultBlockMatrixStrategy>*>(&op);
  if (testOperator != nullptr) return NOX::NLN::LinSystem::LinalgBlockSparseMatrix;

  // Is it a LINALG_SparseMatrix?
  testOperator = dynamic_cast<const CORE::LINALG::SparseMatrix*>(&op);
  if (testOperator != nullptr) return NOX::NLN::LinSystem::LinalgSparseMatrix;

  // Is it a LINALG_SparseMatrixBase?
  testOperator = dynamic_cast<const CORE::LINALG::SparseMatrixBase*>(&op);
  if (testOperator != nullptr) return NOX::NLN::LinSystem::LinalgSparseMatrixBase;

  // Otherwise it must be a LINALG_SparseOperator
  return NOX::NLN::LinSystem::LinalgSparseOperator;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
NOX::NLN::LinSystem::LinearSystemType NOX::NLN::AUX::GetLinearSystemType(
    const NOX::NLN::LinearSystem::SolverMap& linsolvers)
{
  const unsigned int num_ls = linsolvers.size();
  const std::map<enum NOX::NLN::SolutionType, Teuchos::RCP<CORE::LINALG::Solver>>::const_iterator
      ci_end = linsolvers.end();

  switch (num_ls)
  {
    case 1:
    {
      // --- Pure structural case (+ spring dashpot)
      if (linsolvers.find(NOX::NLN::sol_structure) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_structure;
      }
      else if (linsolvers.find(NOX::NLN::sol_scatra) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_scatra;
      }
      // --- ToDo has to be extended

      FOUR_C_THROW(
          "There is no capable linear system type for the given linear "
          "solver combination! ( 1 linear solver )");
      exit(EXIT_FAILURE);
    }
    case 2:
    {
      // --- Structure/Contact case (+ spring dashpot)
      if (linsolvers.find(NOX::NLN::sol_structure) != ci_end and
          linsolvers.find(NOX::NLN::sol_contact) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_structure_contact;
      }
      // --- Structure/CardioVascular0D case (+ spring dashpot)
      else if (linsolvers.find(NOX::NLN::sol_structure) != ci_end and
               linsolvers.find(NOX::NLN::sol_cardiovascular0d) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_structure_cardiovascular0d;
      }
      // --- Structure/Lagrange|Penalty Constaint case (+ spring dashpot)
      else if (linsolvers.find(NOX::NLN::sol_structure) != ci_end and
               linsolvers.find(NOX::NLN::sol_lag_pen_constraint) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_structure_lag_pen_constraint;
      }
      else if (linsolvers.find(NOX::NLN::sol_structure) != ci_end and
               linsolvers.find(NOX::NLN::sol_meshtying) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_structure_meshtying;
      }
      // --- ToDo has to be extended

      FOUR_C_THROW(
          "There is no capable linear system type for the given linear "
          "solver combination ( 2 linear solvers )!");
    }
    case 3:
    {
      // --- Structure/Contact case (+ spring dashpot)
      if (linsolvers.find(NOX::NLN::sol_structure) != ci_end and
          linsolvers.find(NOX::NLN::sol_contact) != ci_end and
          linsolvers.find(NOX::NLN::sol_meshtying) != ci_end)
      {
        return NOX::NLN::LinSystem::linear_system_structure_contact;
      }
      FOUR_C_THROW(
          "There is no capable linear system type for the given linear "
          "solver combination ( 3 linear solvers )!");
    }
    default:
    {
      FOUR_C_THROW(
          "There is no capable linear system type for the given linear "
          "solver combination!");
      exit(EXIT_FAILURE);
    }
  }

  return NOX::NLN::LinSystem::linear_system_undefined;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double NOX::NLN::AUX::RootMeanSquareNorm(const double& atol, const double& rtol,
    Teuchos::RCP<const Epetra_Vector> xnew, Teuchos::RCP<const Epetra_Vector> xincr,
    const bool& disable_implicit_weighting)
{
  double rval = 0.0;

  // calculate the old iterate (k-1)
  Teuchos::RCP<Epetra_Vector> v = Teuchos::rcp(new Epetra_Vector(*xnew));
  v->Update(-1.0, *xincr, 1.0);

  // new auxiliary vector
  Teuchos::RCP<Epetra_Vector> u = Teuchos::rcp(new Epetra_Vector(xnew->Map(), false));

  // create the weighting factor u = RTOL |x^(k-1)| + ATOL
  u->PutScalar(1.0);
  u->Update(rtol, *v, atol);

  // v = xincr/u (elementwise)
  v->ReciprocalMultiply(1.0, *u, *xincr, 0);

  // Turn off implicit scaling of norm if the vector supports it
  // ToDo Check if this makes any sense for pure Epetra_Vectors
  Teuchos::RCP<::NOX::Abstract::ImplicitWeighting> iw_v;
  iw_v = Teuchos::rcp_dynamic_cast<::NOX::Abstract::ImplicitWeighting>(v, false);
  bool saved_status = false;
  if (Teuchos::nonnull(iw_v) and disable_implicit_weighting)
  {
    saved_status = iw_v->getImplicitWeighting();
    iw_v->setImplicitWeighting(false);
  }

  // rval = sqrt (v * v / N)
  v->Norm2(&rval);
  rval /= std::sqrt(static_cast<double>(v->GlobalLength()));

  // Set the implicit scaling back to original value
  if (nonnull(iw_v) && disable_implicit_weighting) iw_v->setImplicitWeighting(saved_status);

  return rval;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double NOX::NLN::AUX::GetNormWRMSClassVariable(const ::NOX::StatusTest::Generic& test,
    const NOX::NLN::StatusTest::QuantityType& qType, const std::string& classVariableName)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  const NOX::NLN::StatusTest::Combo* comboTest =
      dynamic_cast<const NOX::NLN::StatusTest::Combo*>(&test);

  // if it is no combo test, we just have to check for the desired type
  if (comboTest == nullptr)
  {
    const NOX::NLN::StatusTest::NormWRMS* normWRMSTest =
        dynamic_cast<const NOX::NLN::StatusTest::NormWRMS*>(&test);

    // no normF StatusTest...
    if (normWRMSTest == nullptr) return -1.0;
    // yeah we found one...
    else
    {
      // look for the right quantity and get the desired class variable value
      if (classVariableName == "ATOL")
        return normWRMSTest->GetAbsoluteTolerance(qType);
      else if (classVariableName == "RTOL")
        return normWRMSTest->GetRelativeTolerance(qType);
    }
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();
    double ret = -1.0;
    for (std::size_t i = 0; i < tests.size(); ++i)
    {
      // recursive function call
      ret = GetNormWRMSClassVariable(*(tests[i]), qType, classVariableName);
      if (ret != -1.0) return ret;
    }
  }

  // default return
  return -1.0;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double NOX::NLN::AUX::GetNormFClassVariable(const ::NOX::StatusTest::Generic& test,
    const NOX::NLN::StatusTest::QuantityType& qType, const std::string& classVariableName)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  const NOX::NLN::StatusTest::Combo* comboTest =
      dynamic_cast<const NOX::NLN::StatusTest::Combo*>(&test);

  // if it is no combo test, we just have to check for the desired type
  if (comboTest == nullptr)
  {
    const NOX::NLN::StatusTest::NormF* normFTest =
        dynamic_cast<const NOX::NLN::StatusTest::NormF*>(&test);

    // no normF StatusTest...
    if (normFTest == nullptr) return -1.0;
    // yeah we found one...
    else
    {
      // look for the right quantity and get the desired class variable value
      if (classVariableName == "NormF")
        return normFTest->GetNormF(qType);
      else if (classVariableName == "TrueTolerance")
        return normFTest->GetTrueTolerance(qType);
      else if (classVariableName == "SpecifiedTolerance")
        return normFTest->GetSpecifiedTolerance(qType);
      else if (classVariableName == "InitialTolerance")
        return normFTest->GetInitialTolerance(qType);
    }
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();
    double ret = -1.0;
    for (std::size_t i = 0; i < tests.size(); ++i)
    {
      // recursive function call
      ret = GetNormFClassVariable(*(tests[i]), qType, classVariableName);
      if (ret != -1.0) return ret;
    }
  }

  // default return
  return -1.0;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <class T>
bool NOX::NLN::AUX::IsQuantity(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  const NOX::NLN::StatusTest::Combo* comboTest =
      dynamic_cast<const NOX::NLN::StatusTest::Combo*>(&test);

  // if it is no combo test, we just have to check for the desired type
  if (comboTest == nullptr)
  {
    const T* desiredTest = dynamic_cast<const T*>(&test);

    // not the desired status test...
    if (desiredTest == nullptr) return false;
    // yeah we found one...
    else
    {
      // check for the quantity
      return desiredTest->IsQuantity(qtype);
    }
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();
    for (std::size_t i = 0; i < tests.size(); ++i)
    {
      // recursive function call
      bool isfound = IsQuantity<T>(*(tests[i]), qtype);
      if (isfound) return isfound;
    }
  }

  // default return
  return false;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <class T>
int NOX::NLN::AUX::GetNormType(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  const NOX::NLN::StatusTest::Combo* comboTest =
      dynamic_cast<const NOX::NLN::StatusTest::Combo*>(&test);

  // if it is no combo test, we just have to check for the desired type
  if (comboTest == nullptr)
  {
    const T* desiredTest = dynamic_cast<const T*>(&test);

    // not the desired status test...
    if (desiredTest == nullptr) return -100;
    // yeah we found one...
    else
    {
      // get the norm type of the given quantity
      return desiredTest->GetNormType(qtype);
    }
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();
    for (std::size_t i = 0; i < tests.size(); ++i)
    {
      // recursive function call
      int ret = GetNormType<T>(*(tests[i]), qtype);
      if (ret != -100) return ret;
    }
  }

  // default return
  return -100;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <class T>
::NOX::StatusTest::Generic* NOX::NLN::AUX::GetOuterStatusTestWithQuantity(
    ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType qtype)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  NOX::NLN::StatusTest::Combo* comboTest = dynamic_cast<NOX::NLN::StatusTest::Combo*>(&test);

  // if it is no combo test, we just have to check for the desired type
  if (comboTest == nullptr)
  {
    T* desiredTest = dynamic_cast<T*>(&test);

    // not the desired status test...
    if (desiredTest == nullptr) return nullptr;
    // yeah we found one...
    else
    {
      // check for the quantity
      if (desiredTest->IsQuantity(qtype))
        return desiredTest;
      else
        return nullptr;
    }
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();
    for (auto& ctest : tests)
    {
      // recursive function call
      ::NOX::StatusTest::Generic* ptr = GetOuterStatusTestWithQuantity<T>(*ctest, qtype);
      if (ptr) return ptr;
    }
  }

  // default return
  return nullptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <class T>
::NOX::StatusTest::Generic* NOX::NLN::AUX::GetOuterStatusTest(::NOX::StatusTest::Generic& otest)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  const NOX::NLN::StatusTest::Combo* comboTest =
      dynamic_cast<const NOX::NLN::StatusTest::Combo*>(&otest);

  // if it is no combo test, we just have to check for the desired type
  if (not comboTest)
  {
    return dynamic_cast<T*>(&otest);
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();

    ::NOX::StatusTest::Generic* gdesired_test = nullptr;
    for (const auto& test : tests)
    {
      // recursive function call
      ::NOX::StatusTest::Generic* desired_test = GetOuterStatusTest<T>(*test);

      // the test is not of the specified type, go to the next one
      if (not desired_test) continue;

      // first found test
      if (not gdesired_test) gdesired_test = desired_test;
      // we've found already one test of the same type
      else
      {
        const enum ::NOX::StatusTest::StatusType gstatus = gdesired_test->getStatus();

        // If there are more tests of the same type, we return the
        // test which is possible unconverged (conservative choice, AND-combination).
        gdesired_test = (gstatus == ::NOX::StatusTest::Converged ? desired_test : gdesired_test);
      }
    }
    return gdesired_test;
  }

  // default return
  return nullptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template <class T>
int NOX::NLN::AUX::GetOuterStatus(const ::NOX::StatusTest::Generic& test)
{
  // try to cast the given test to a NOX_StatusTest_Combo
  const NOX::NLN::StatusTest::Combo* comboTest =
      dynamic_cast<const NOX::NLN::StatusTest::Combo*>(&test);

  // if it is no combo test, we just have to check for the desired type
  if (comboTest == nullptr)
  {
    const T* desiredTest = dynamic_cast<const T*>(&test);

    // not the desired status test...
    if (desiredTest == nullptr) return -100;
    // yeah we found one...
    else
    {
      // get the global status
      return desiredTest->getStatus();
    }
  }
  // if the nox_nln_statustest_combo Test cast was successful
  else
  {
    const std::vector<Teuchos::RCP<::NOX::StatusTest::Generic>>& tests = comboTest->GetTestVector();
    int gRet = -100;
    for (std::size_t i = 0; i < tests.size(); ++i)
    {
      // recursive function call
      int lRet = GetOuterStatus<T>(*(tests[i]));

      // the test is not of the specified type, go to the next one
      if (lRet == -100) continue;

      // first found test
      if (gRet == -100) gRet = lRet;
      // we've found already one test of the same type
      else
      {
        ::NOX::StatusTest::StatusType gstatus =
            static_cast<enum ::NOX::StatusTest::StatusType>(gRet);

        // If there are more tests of the same type, we return the
        // status of the possible unconverged test (conservative choice).
        gRet = (gstatus == ::NOX::StatusTest::Converged ? lRet : gRet);
      }
    }
    return gRet;
  }

  // default return
  return -100;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
enum NOX::NLN::SolutionType NOX::NLN::AUX::ConvertQuantityType2SolutionType(
    const enum NOX::NLN::StatusTest::QuantityType& qtype)
{
  enum NOX::NLN::SolutionType soltype = NOX::NLN::sol_unknown;
  switch (qtype)
  {
    case NOX::NLN::StatusTest::quantity_structure:
    case NOX::NLN::StatusTest::quantity_eas:
    case NOX::NLN::StatusTest::quantity_plasticity:
    case NOX::NLN::StatusTest::quantity_pressure:
      soltype = NOX::NLN::sol_structure;
      break;
    case NOX::NLN::StatusTest::quantity_lag_pen_constraint:
      soltype = NOX::NLN::sol_lag_pen_constraint;
      break;
    case NOX::NLN::StatusTest::quantity_contact_normal:
    case NOX::NLN::StatusTest::quantity_contact_friction:
      soltype = NOX::NLN::sol_contact;
      break;
    case NOX::NLN::StatusTest::quantity_meshtying:
      soltype = NOX::NLN::sol_meshtying;
      break;
    case NOX::NLN::StatusTest::quantity_cardiovascular0d:
      soltype = NOX::NLN::sol_cardiovascular0d;
      break;
    case NOX::NLN::StatusTest::quantity_unknown:
    default:
      FOUR_C_THROW("Unknown conversion for the quantity type \"%s\".",
          NOX::NLN::StatusTest::QuantityType2String(qtype).c_str());
  }
  // return the corresponding solution type
  return soltype;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
enum ::NOX::Abstract::Vector::NormType NOX::NLN::AUX::String2NormType(const std::string& name)
{
  enum ::NOX::Abstract::Vector::NormType norm_type = ::NOX::Abstract::Vector::TwoNorm;
  if (name == "Two Norm")
    norm_type = ::NOX::Abstract::Vector::TwoNorm;
  else if (name == "One Norm")
    norm_type = ::NOX::Abstract::Vector::OneNorm;
  else if (name == "Max Norm")
    norm_type = ::NOX::Abstract::Vector::MaxNorm;
  else
    FOUR_C_THROW("Unknown conversion from STL_STRING to NormType enum for %s.", name.c_str());

  return norm_type;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void NOX::NLN::AUX::AddToPrePostOpVector(
    Teuchos::ParameterList& p_nox_opt, const Teuchos::RCP<::NOX::Observer>& ppo_ptr)
{
  // if there is already a pre/post operator, we will convert the pre/post op
  // to a pre/post op vector and add the previous and new pre/post op
  if (p_nox_opt.isType<Teuchos::RCP<::NOX::Observer>>("User Defined Pre/Post Operator"))
  {
    Teuchos::RCP<::NOX::Observer> user_ppo =
        p_nox_opt.get<Teuchos::RCP<::NOX::Observer>>("User Defined Pre/Post Operator");

    Teuchos::RCP<::NOX::ObserverVector> user_ppo_vec =
        Teuchos::rcp_dynamic_cast<::NOX::ObserverVector>(user_ppo, false);

    if (user_ppo_vec.is_null())
    {
      user_ppo_vec = Teuchos::rcp(new ::NOX::ObserverVector());
      user_ppo_vec->pushBack(user_ppo);
    }

    user_ppo_vec->pushBack(ppo_ptr);

    p_nox_opt.set<Teuchos::RCP<::NOX::Observer>>("User Defined Pre/Post Operator", user_ppo_vec);
  }
  // if there is no pre/post operator, we will just add the new one
  else
    p_nox_opt.set<Teuchos::RCP<::NOX::Observer>>("User Defined Pre/Post Operator", ppo_ptr);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
std::string NOX::NLN::AUX::GetDirectionMethodListName(const Teuchos::ParameterList& p)
{
  if (not p.isSublist("Direction"))
    FOUR_C_THROW("There is no \"Direction\" sub-list in the parameter list!");
  const Teuchos::ParameterList& pdir = p.sublist("Direction");

  if (not pdir.isParameter("Method"))
    FOUR_C_THROW("There is no \"Method\" parameter in the Direction sub-list!");

  const std::string* dir_str = &pdir.get<std::string>("Method");
  if (*dir_str == "User Defined")
  {
    dir_str = &pdir.get<std::string>("User Defined Method");
  }
  if (*dir_str == "Newton" or *dir_str == "Modified Newton")
    return "Newton";
  else
  {
    FOUR_C_THROW("Currently unsupported direction method string: %s", dir_str->c_str());
    exit(EXIT_FAILURE);
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
template ::NOX::StatusTest::Generic*
NOX::NLN::AUX::GetOuterStatusTestWithQuantity<NOX::NLN::StatusTest::NormF>(
    ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType qtype);
template ::NOX::StatusTest::Generic*
NOX::NLN::AUX::GetOuterStatusTestWithQuantity<NOX::NLN::StatusTest::NormUpdate>(
    ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType qtype);
template ::NOX::StatusTest::Generic*
NOX::NLN::AUX::GetOuterStatusTestWithQuantity<NOX::NLN::StatusTest::NormWRMS>(
    ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType qtype);
template bool NOX::NLN::AUX::IsQuantity<NOX::NLN::StatusTest::NormF>(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype);
template bool NOX::NLN::AUX::IsQuantity<NOX::NLN::StatusTest::NormUpdate>(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype);
template bool NOX::NLN::AUX::IsQuantity<NOX::NLN::StatusTest::NormWRMS>(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype);
template int NOX::NLN::AUX::GetNormType<NOX::NLN::StatusTest::NormF>(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype);
template int NOX::NLN::AUX::GetNormType<NOX::NLN::StatusTest::NormUpdate>(
    const ::NOX::StatusTest::Generic& test, const NOX::NLN::StatusTest::QuantityType& qtype);
template ::NOX::StatusTest::Generic*
NOX::NLN::AUX::GetOuterStatusTest<NOX::NLN::StatusTest::ActiveSet>(
    ::NOX::StatusTest::Generic& otest);
template int NOX::NLN::AUX::GetOuterStatus<NOX::NLN::StatusTest::NormF>(
    const ::NOX::StatusTest::Generic& test);
template int NOX::NLN::AUX::GetOuterStatus<NOX::NLN::StatusTest::NormUpdate>(
    const ::NOX::StatusTest::Generic& test);
template int NOX::NLN::AUX::GetOuterStatus<NOX::NLN::StatusTest::NormWRMS>(
    const ::NOX::StatusTest::Generic& test);
template int NOX::NLN::AUX::GetOuterStatus<NOX::NLN::StatusTest::ActiveSet>(
    const ::NOX::StatusTest::Generic& test);

FOUR_C_NAMESPACE_CLOSE
