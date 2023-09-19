/*----------------------------------------------------------------------*/
/*! \file

\brief Managing of space- and/or time-dependent functions

\level 0

*/
/*----------------------------------------------------------------------*/

#include "baci_fluid_functions.H"
#include "baci_fluid_xfluid_functions.H"
#include "baci_fluid_xfluid_functions_combust.H"
#include "baci_io.H"
#include "baci_lib_function.H"
#include "baci_lib_function_library.H"
#include "baci_lib_function_of_time.H"
#include "baci_lib_globalproblem.H"
#include "baci_lib_linedefinition.H"
#include "baci_poromultiphase_scatra_function.H"
#include "baci_structure_new_functions.H"
#include "baci_utils_exceptions.H"

#include <stdexcept>


namespace
{
  using LineDefinitionVector = std::vector<DRT::INPUT::LineDefinition>;

  using TypeErasedFunctionCreator = std::function<std::any(const LineDefinitionVector&)>;

  template <typename T>
  using FunctionCreator = Teuchos::RCP<T> (*)(const LineDefinitionVector&);

  /**
   * Utility function that takes a function object returning a Teuchos::RCP<T> and erases its return
   * type via std::any. In addition, if the returned object would be Teuchos::null, discard it and
   * return an empty std::any instead.
   */
  template <typename T>
  TypeErasedFunctionCreator WrapFunction(FunctionCreator<T> fun)
  {
    return [fun](const LineDefinitionVector& linedefs) -> std::any
    {
      Teuchos::RCP<T> created = fun(linedefs);
      if (created == Teuchos::null)
        return {};
      else
        return created;
    };
  }


  template <int dim>
  std::any CreateBuiltinFunction(const std::vector<DRT::INPUT::LineDefinition>& function_line_defs)
  {
    // List all known TryCreate functions in a vector, so they can be called with a unified
    // syntax below. Also, erase their exact return type, since we can only store std::any.
    std::vector<TypeErasedFunctionCreator> try_create_function_vector{
        WrapFunction(DRT::UTILS::TryCreateSymbolicFunctionOfAnything<dim>),
        WrapFunction(DRT::UTILS::TryCreateSymbolicFunctionOfSpaceTime<dim>),
        WrapFunction(DRT::UTILS::TryCreateFunctionOfTime)};

    for (const auto& try_create_function : try_create_function_vector)
    {
      auto maybe_function = try_create_function(function_line_defs);
      if (maybe_function.has_value()) return maybe_function;
    }

    dserror("Internal error: could not create a function that I should be able to create.");
  }

  // add one level of indirection to dispatch on the dimension later when the global problem is
  // available.
  auto CreateBuiltinFunctionDispatch(
      const std::vector<DRT::INPUT::LineDefinition>& function_line_defs)
  {
    switch (DRT::Problem::Instance()->NDim())
    {
      case 1:
        return CreateBuiltinFunction<1>(function_line_defs);
      case 2:
        return CreateBuiltinFunction<2>(function_line_defs);
      case 3:
        return CreateBuiltinFunction<3>(function_line_defs);
      default:
        dserror("Unsupported dimension %d.", DRT::Problem::Instance()->NDim());
    }
  }
}  // namespace


void PrintFunctionDatHeader()
{
  DRT::UTILS::FunctionManager functionmanager;
  DRT::INPUT::Lines lines = functionmanager.ValidFunctionLines();

  lines.Print(std::cout);
}



void DRT::UTILS::AddValidBuiltinFunctions(DRT::UTILS::FunctionManager& function_manager)
{
  using namespace DRT::INPUT;

  std::vector<LineDefinition> possible_lines = {
      LineDefinition::Builder().AddNamedString("SYMBOLIC_FUNCTION_OF_SPACE_TIME").Build(),

      LineDefinition::Builder().AddNamedString("SYMBOLIC_FUNCTION_OF_TIME").Build(),

      LineDefinition::Builder()
          .AddNamedInt("COMPONENT")
          .AddNamedString("SYMBOLIC_FUNCTION_OF_SPACE_TIME")
          .Build(),

      LineDefinition::Builder()
          .AddNamedInt("VARIABLE")
          .AddNamedString("NAME")
          .AddNamedString("TYPE")
          .AddOptionalNamedInt("NUMPOINTS")
          .AddOptionalNamedString("BYNUM")
          .AddOptionalNamedDoubleVector("TIMERANGE", 2)
          .AddOptionalNamedDoubleVector("TIMES", LengthFromIntNamed("NUMPOINTS"))
          .AddOptionalNamedDoubleVector("VALUES", LengthFromIntNamed("NUMPOINTS"))
          .AddOptionalNamedStringVector("DESCRIPTION",
              // Special case where only NUMPOINTS-1 are taken
              [](const LineDefinition& already_read_line)
              {
                try
                {
                  int length;
                  already_read_line.ExtractInt("NUMPOINTS", length);
                  return length - 1;
                }
                catch (const std::runtime_error& e)
                {
                  // When NUMPOINTS is not set, then we still allow for a single DESCRIPTION entry
                  return 1;
                }
              })
          .AddOptionalNamedString("PERIODIC")
          .AddOptionalNamedDouble("T1")
          .AddOptionalNamedDouble("T2")
          .Build(),

      LineDefinition::Builder()
          .AddNamedString("VARFUNCTION")
          .AddOptionalNamedInt("NUMCONSTANTS")
          .AddOptionalNamedPairOfStringAndDoubleVector(
              "CONSTANTS", LengthFromIntNamed("NUMCONSTANTS"))
          .Build()};

  function_manager.AddFunctionDefinition(possible_lines, CreateBuiltinFunctionDispatch);
}

DRT::UTILS::FunctionManager::FunctionManager()
{
  // Legacy: attach all function directly in the constructor
  // TODO these calls need to move into their respective modules
  AddValidBuiltinFunctions(*this);
  STR::AddValidStructureFunctions(*this);
  FLD::AddValidFluidFunctions(*this);
  AddValidCombustFunctions(*this);
  AddValidXfluidFunctions(*this);
  AddValidLibraryFunctions(*this);
  POROMULTIPHASESCATRA::AddValidPoroFunctions(*this);
}


DRT::INPUT::Lines DRT::UTILS::FunctionManager::ValidFunctionLines()
{
  DRT::INPUT::Lines lines(
      "FUNCT", "Definition of functions for various cases, mainly boundary conditions");

  for (const auto& [possible_lines, _] : attached_function_data_)
  {
    for (const auto& single_line : possible_lines)
    {
      lines.Add(single_line);
    }
  }

  return lines;
}


void DRT::UTILS::FunctionManager::AddFunctionDefinition(
    std::vector<INPUT::LineDefinition> possible_lines, FunctionFactory function_factory)
{
  attached_function_data_.emplace_back(std::move(possible_lines), std::move(function_factory));
}


void DRT::UTILS::FunctionManager::ReadInput(DRT::INPUT::DatFileReader& reader)
{
  functions_.clear();

  // Read FUNCT sections starting from FUNCT1 until the first empty one is encountered.
  // This implies that the FUNCT sections must form a contiguous range in the input file.
  // Otherwise, the read fails later.
  for (int funct_suffix = 1;; ++funct_suffix)
  {
    const bool stop_parsing = std::invoke(
        [&]()
        {
          for (auto& [possible_lines, function_factory] : attached_function_data_)
          {
            auto [parsed_lines, unparsed_lines] = DRT::INPUT::ReadMatchingLines(
                reader, "FUNCT" + std::to_string(funct_suffix), possible_lines);

            // A convoluted way of saying that there are no lines in the section, thus, stop
            // parsing. This can only be refactored if the reading mechanism is overhauled in
            // general.
            if (parsed_lines.size() + unparsed_lines.size() == 0)
            {
              return true;
            }

            if (parsed_lines.size() > 0 && unparsed_lines.size() == 0)
            {
              functions_.emplace_back(function_factory(parsed_lines));
              return false;
            }
          }

          // If we end up here, the current sections function definition could not be parsed.
          {
            const auto section_line_defs = reader.Section("--FUNCT" + std::to_string(funct_suffix));
            std::stringstream ss;
            for (const auto& line : section_line_defs)
            {
              ss << '\n' << line;
            }

            dserror("Could not parse the following lines into a Function known to BACI:\n%s",
                ss.str().c_str());
          }
        });

    // Stop reading as soon as the first FUNCT section in the input file is empty
    if (stop_parsing) break;
  }
}