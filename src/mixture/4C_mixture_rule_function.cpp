/*----------------------------------------------------------------------*/
/*! \file

\brief Mixture rule for homogenized constrained mixtures with mass fractions defined through
functions

\level 3


*/
/*----------------------------------------------------------------------*/
#include "4C_mixture_rule_function.hpp"

#include "4C_global_data.hpp"
#include "4C_linalg_fixedsizematrix.hpp"
#include "4C_material_parameter_base.hpp"
#include "4C_mixture_constituent.hpp"
#include "4C_utils_exceptions.hpp"

#include <Epetra_ConfigDefs.h>
#include <Teuchos_RCP.hpp>

#include <algorithm>
#include <iosfwd>
#include <string>


FOUR_C_NAMESPACE_OPEN

namespace
{
  std::vector<const Core::UTILS::FunctionOfSpaceTime*> CreateFunctionsFromFunctionIds(
      const std::vector<int>& funct_ids)
  {
    std::vector<const Core::UTILS::FunctionOfSpaceTime*> functions;
    // get function handles from function ids
    for (int id : funct_ids)
    {
      const auto* function =
          &Global::Problem::Instance()->FunctionById<Core::UTILS::FunctionOfSpaceTime>(id - 1);

      const std::string errorMessage =
          "pointer to mass fraction function with id " + std::to_string(id) + " is nullptr!";
      FOUR_C_ASSERT(function != nullptr, errorMessage.c_str());

      functions.emplace_back(function);
    }
    return functions;
  }
}  // namespace

MIXTURE::PAR::FunctionMixtureRule::FunctionMixtureRule(
    const Core::Mat::PAR::Parameter::Data& matdata)
    : MixtureRule(matdata),
      initial_reference_density_(matdata.parameters.Get<double>("DENS")),
      mass_fractions_funct_ids_(matdata.parameters.Get<std::vector<int>>("MASSFRACFUNCT")){};

std::unique_ptr<MIXTURE::MixtureRule> MIXTURE::PAR::FunctionMixtureRule::create_rule()
{
  return std::make_unique<MIXTURE::FunctionMixtureRule>(this);
}

MIXTURE::FunctionMixtureRule::FunctionMixtureRule(MIXTURE::PAR::FunctionMixtureRule* params)
    : MixtureRule(params), params_(params), mass_fractions_functions_()
{
  // cannot setup mass_fractions_functions_ here because at this state, functions are not yet read
  // from input
}

void MIXTURE::FunctionMixtureRule::setup(Teuchos::ParameterList& params, const int eleGID)
{
  MixtureRule::setup(params, eleGID);

  mass_fractions_functions_ = CreateFunctionsFromFunctionIds(params_->mass_fractions_funct_ids_);
}

void MIXTURE::FunctionMixtureRule::unpack_mixture_rule(
    std::vector<char>::size_type& position, const std::vector<char>& data)
{
  MIXTURE::MixtureRule::unpack_mixture_rule(position, data);

  mass_fractions_functions_ = CreateFunctionsFromFunctionIds(params_->mass_fractions_funct_ids_);
}

void MIXTURE::FunctionMixtureRule::evaluate(const Core::LinAlg::Matrix<3, 3>& F,
    const Core::LinAlg::Matrix<6, 1>& E_strain, Teuchos::ParameterList& params,
    Core::LinAlg::Matrix<6, 1>& S_stress, Core::LinAlg::Matrix<6, 6>& cmat, const int gp,
    const int eleGID)
{
  // define temporary matrices
  Core::LinAlg::Matrix<6, 1> cstress;
  Core::LinAlg::Matrix<6, 6> ccmat;

  // initialize sum of mass fractions for validity check
  double sum = 0.0;

  // Iterate over all constituents and add all stress/cmat contributions
  for (std::size_t i = 0; i < constituents().size(); ++i)
  {
    // mass fractions are defined by evaluating the specified function at the gauss point reference
    // coordinates (and the current time)

    // get gauss point reference coordinates and current time
    const auto& reference_coordinates = params.get<Core::LinAlg::Matrix<3, 1>>("gp_coords_ref");
    const double time = params.get<double>("total time");

    // evaluate the mass fraction function at the gauss point reference coordinates and current time
    const double massfrac =
        mass_fractions_functions_[i]->evaluate(reference_coordinates.A(), time, 0);
    sum += massfrac;
    double constituent_density = params_->initial_reference_density_ * massfrac;

    // add stress contribution to global stress
    MixtureConstituent& constituent = *constituents()[i];
    cstress.Clear();
    ccmat.Clear();
    constituent.evaluate(F, E_strain, params, cstress, ccmat, gp, eleGID);

    S_stress.Update(constituent_density, cstress, 1.0);
    cmat.Update(constituent_density, ccmat, 1.0);
  }

  // validity check whether mass fractions summed up to 1
  if (std::abs(1.0 - sum) > 1e-8)
    FOUR_C_THROW("Evaluated mass fractions don't sum up to 1, which is unphysical.");
}


FOUR_C_NAMESPACE_CLOSE
