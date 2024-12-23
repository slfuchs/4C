// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_poromultiphase_scatra_function_parameters.hpp"

#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  /*!
   * @brief check for correct naming and order of input parameters
   *
   * @tparam NumberOfParameters: Number of parameters
   * @param param_map: array including all valid parameter names in the correct order
   * @param funct_params: parameters read from input, which are checked
   * @param function_name: name of the function whose parameters are checked
   */

  template <int number_of_parameters>
  void check_naming_and_order_of_parameters(
      const std::array<std::string, number_of_parameters>& param_map,
      const std::vector<std::pair<std::string, double>>& funct_params, std::string function_name)
  {
    if (funct_params.size() != number_of_parameters)
    {
      std::string list_of_parameters{};
      for (std::size_t i = 0; i < number_of_parameters; ++i)
      {
        list_of_parameters += param_map[i] + ", ";
      }

      auto const message = "Wrong size of funct_params for " + function_name +
                           ", it should have exactly\n" + std::to_string(number_of_parameters) +
                           " funct_params (in this order): " + list_of_parameters;

      FOUR_C_THROW(message.c_str());
    }

    for (std::size_t i = 0; i < funct_params.size(); ++i)
    {
      auto const message = "Parameter number " + std::to_string(i + 1) + " for " + function_name +
                           " has to be " + param_map[i];
      if (funct_params[i].first != param_map[i]) FOUR_C_THROW(message.c_str());
    }
  }
}  // namespace

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::TumorGrowthLawHeavisideParameters::TumorGrowthLawHeavisideParameters(
    const std::vector<std::pair<std::string, double>>& funct_params)
    : gamma_T_growth(funct_params[0].second),
      w_nl_crit(funct_params[1].second),
      w_nl_env(funct_params[2].second),
      lambda(funct_params[3].second),
      p_t_crit(funct_params[4].second)
{
  constexpr int NUMBER_OF_PARAMS = 5;
  const std::string function_name = "TUMOR_GROWTH_LAW_HEAVISIDE";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{
      "gamma_T_growth", "w_nl_crit", "w_nl_env", "lambda", "p_t_crit"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::NecrosisLawHeavisideParameters::NecrosisLawHeavisideParameters(
    const std::vector<std::pair<std::string, double>>& funct_params)
    : gamma_t_necr(funct_params[0].second),
      w_nl_crit(funct_params[1].second),
      w_nl_env(funct_params[2].second),
      delta_a_t(funct_params[3].second),
      p_t_crit(funct_params[4].second)
{
  constexpr int NUMBER_OF_PARAMS = 5;
  const std::string function_name = "NECROSIS_LAW_HEAVISIDE";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{
      "gamma_t_necr", "w_nl_crit", "w_nl_env", "delta_a_t", "p_t_crit"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::OxygenConsumptionLawHeavisideParameters::
    OxygenConsumptionLawHeavisideParameters(
        const std::vector<std::pair<std::string, double>>& funct_params)
    : gamma_nl_growth(funct_params[0].second),
      gamma_0_nl(funct_params[1].second),
      w_nl_crit(funct_params[2].second),
      w_nl_env(funct_params[3].second),
      p_t_crit(funct_params[4].second)
{
  constexpr int NUMBER_OF_PARAMS = 5;
  const std::string function_name = "OXYGEN_CONSUMPTION_LAW_HEAVISIDE";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{
      "gamma_nl_growth", "gamma_0_nl", "w_nl_crit", "w_nl_env", "p_t_crit"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::TumorGrowthLawHeavisideNecroOxyParameters::
    TumorGrowthLawHeavisideNecroOxyParameters(
        const std::vector<std::pair<std::string, double>>& funct_params)
    : gamma_T_growth(funct_params[0].second),
      w_nl_crit(funct_params[1].second),
      w_nl_env(funct_params[2].second),
      lambda(funct_params[3].second),
      p_t_crit(funct_params[4].second)
{
  constexpr int NUMBER_OF_PARAMS = 5;
  const std::string function_name = "TUMOR_GROWTH_LAW_HEAVISIDE_OXY";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{
      "gamma_T_growth", "w_nl_crit", "w_nl_env", "lambda", "p_t_crit"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::OxygenTransvascularExchangeLawContParameters::
    OxygenTransvascularExchangeLawContParameters(
        const std::vector<std::pair<std::string, double>>& funct_params)
    : n(funct_params[0].second),
      Pb50(funct_params[1].second),
      CaO2_max(funct_params[2].second),
      alpha_bl_eff(funct_params[3].second),
      gammarhoSV(funct_params[4].second),
      rho_oxy(funct_params[5].second),
      rho_if(funct_params[6].second),
      rho_bl(funct_params[7].second),
      alpha_IF(funct_params[8].second)
{
  constexpr int NUMBER_OF_PARAMS = 9;
  const std::string function_name = "OXYGEN_TRANSVASCULAR_EXCHANGE_LAW_CONT";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{"n", "Pb50", "CaO2_max",
      "alpha_bl_eff", "gammarhoSV", "rho_oxy", "rho_IF", "rho_bl", " alpha_IF"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::OxygenTransvascularExchangeLawDiscParameters::
    OxygenTransvascularExchangeLawDiscParameters(
        const std::vector<std::pair<std::string, double>>& funct_params)
    : n(funct_params[0].second),
      Pb50(funct_params[1].second),
      CaO2_max(funct_params[2].second),
      alpha_bl_eff(funct_params[3].second),
      gammarho(funct_params[4].second),
      rho_oxy(funct_params[5].second),
      rho_if(funct_params[6].second),
      rho_bl(funct_params[7].second),
      S2_max(funct_params[8].second),
      alpha_IF(funct_params[9].second)
{
  constexpr int NUMBER_OF_PARAMS = 10;
  const std::string function_name = "OXYGEN_TRANSVASCULAR_EXCHANGE_LAW_DISC";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{"n", "Pb50", "CaO2_max",
      "alpha_bl_eff", "gamma*rho", "rho_oxy", "rho_IF", "rho_bl", "S2_max", "alpha_IF"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::LungOxygenExchangeLawParameters::LungOxygenExchangeLawParameters(
    const std::vector<std::pair<std::string, double>>& funct_params)
    : rho_oxy(funct_params[0].second),
      DiffAdVTLC(funct_params[1].second),
      alpha_oxy(funct_params[2].second),
      rho_air(funct_params[3].second),
      rho_bl(funct_params[4].second),
      n(funct_params[5].second),
      P_oB50(funct_params[6].second),
      NC_Hb(funct_params[7].second),
      P_atmospheric(funct_params[8].second),
      volfrac_blood_ref(funct_params[9].second)
{
  constexpr int NUMBER_OF_PARAMS = 10;
  const std::string function_name = "LUNG_OXYGEN_EXCHANGE_LAW";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{"rho_oxy", "DiffAdVTLC",
      "alpha_oxy", "rho_air", "rho_bl", "n", "P_oB50", "NC_Hb", "P_atmospheric",
      "volfrac_blood_ref"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
PoroMultiPhaseScaTra::LungCarbonDioxideExchangeLawParameters::
    LungCarbonDioxideExchangeLawParameters(
        const std::vector<std::pair<std::string, double>>& funct_params)
    : rho_CO2(funct_params[0].second),
      DiffsolAdVTLC(funct_params[1].second),
      pH(funct_params[2].second),
      rho_air(funct_params[3].second),
      rho_bl(funct_params[4].second),
      rho_oxy(funct_params[5].second),
      n(funct_params[6].second),
      P_oB50(funct_params[7].second),
      C_Hb(funct_params[8].second),
      NC_Hb(funct_params[9].second),
      alpha_oxy(funct_params[10].second),
      P_atmospheric(funct_params[11].second),
      ScalingFormmHg(funct_params[12].second),
      volfrac_blood_ref(funct_params[13].second)
{
  constexpr int NUMBER_OF_PARAMS = 14;
  const std::string function_name = "LUNG_CARBONDIOXIDE_EXCHANGE_LAW";

  static const std::array<std::string, NUMBER_OF_PARAMS> param_map{"rho_CO2", "DiffsolAdVTLC", "pH",
      "rho_air", "rho_bl", "rho_oxy", "n", "P_oB50", "C_Hb", "NC_Hb", "alpha_oxy", "P_atmospheric",
      "ScalingFormmHg", "volfrac_blood_ref"};

  // Check parameters
  check_naming_and_order_of_parameters<NUMBER_OF_PARAMS>(param_map, funct_params, function_name);
}

FOUR_C_NAMESPACE_CLOSE
