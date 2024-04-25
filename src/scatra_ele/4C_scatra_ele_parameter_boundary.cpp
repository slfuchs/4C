/*---------------------------------------------------------------------*/
/*! \file
\brief singleton class holding all interface parameters required for boundary element evaluation

\level 2

*/
/*---------------------------------------------------------------------*/

#include "4C_scatra_ele_parameter_boundary.hpp"

#include "4C_utils_exceptions.hpp"
#include "4C_utils_singleton_owner.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::ScaTraEleParameterBoundary* DRT::ELEMENTS::ScaTraEleParameterBoundary::Instance(
    const std::string& disname)
{
  static auto singleton_map = CORE::UTILS::MakeSingletonMap<std::string>(
      [](const std::string& disname) {
        return std::unique_ptr<ScaTraEleParameterBoundary>(new ScaTraEleParameterBoundary(disname));
      });

  return singleton_map[disname].Instance(CORE::UTILS::SingletonAction::create, disname);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::ScaTraEleParameterBoundary::ScaTraEleParameterBoundary(const std::string& disname)
    : alphaa_(0.0),
      alphac_(0.0),
      conditiontype_(DRT::Condition::ConditionType::none),
      convtolimplicit_bv_(-1.0),
      density_(-1.0),
      molar_heat_capacity_(-1.0),
      is_pseudo_contact_(false),
      itemaxmimplicit_bv_(-1),
      kineticmodel_(-1),
      kr_(-1.0),
      molarmass_(-1.0),
      numelectrons_(0),
      numscal_(-1),
      onoff_(nullptr),
      peltier_(0.0),
      permeabilities_(nullptr),
      regularizationparameter_(-1.0),
      regularizationtype_(INPAR::S2I::RegularizationType::regularization_undefined),
      resistance_(0.0),
      resistivity_(0.0),
      capacitance_(0.0),
      stoichiometries_(nullptr),
      thermoperm_(-1.0)
{
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetParameters(Teuchos::ParameterList& parameters)
{
  kineticmodel_ = parameters.get<int>("kinetic model", std::numeric_limits<int>::infinity());
  conditiontype_ = parameters.get<DRT::Condition::ConditionType>(
      "condition type", DRT::Condition::ConditionType::none);

  // set parameters to internal members depending on condition type
  switch (conditiontype_)
  {
    case DRT::Condition::ConditionType::S2IKinetics:
    {
      // set parameters to internal members depending on kinetic model
      switch (kineticmodel_)
      {
        case INPAR::S2I::kinetics_constperm:
        case INPAR::S2I::kinetics_linearperm:
        {
          SetIsPseudoContact(parameters);
          SetNumScal(parameters);
          SetPermeabilities(parameters);
          break;
        }

        case INPAR::S2I::kinetics_constantinterfaceresistance:
        {
          SetIsPseudoContact(parameters);
          SetResistance(parameters);
          SetNumElectrons(parameters);
          SetOnOff(parameters);
          break;
        }

        case INPAR::S2I::kinetics_nointerfaceflux:
        {
          // do nothing
          break;
        }

        case INPAR::S2I::kinetics_butlervolmer:
        case INPAR::S2I::kinetics_butlervolmerlinearized:
        case INPAR::S2I::kinetics_butlervolmerreduced:
        case INPAR::S2I::kinetics_butlervolmerreducedcapacitance:
        case INPAR::S2I::kinetics_butlervolmerreducedlinearized:
        case INPAR::S2I::kinetics_butlervolmerpeltier:
        case INPAR::S2I::kinetics_butlervolmerresistance:
        case INPAR::S2I::kinetics_butlervolmerreducedthermoresistance:
        case INPAR::S2I::kinetics_butlervolmerreducedresistance:
        {
          SetAlpha(parameters);
          SetChargeTransferConstant(parameters);
          SetIsPseudoContact(parameters);
          SetNumElectrons(parameters);
          SetNumScal(parameters);
          SetStoichiometries(parameters);
          if (kineticmodel_ == INPAR::S2I::kinetics_butlervolmerreducedcapacitance)
          {
            SetCapacitance(parameters);
          }
          if (kineticmodel_ == INPAR::S2I::kinetics_butlervolmerpeltier)
            SetPeltier(parameters);
          else if (kineticmodel_ == INPAR::S2I::kinetics_butlervolmerresistance or
                   kineticmodel_ == INPAR::S2I::kinetics_butlervolmerreducedresistance)
          {
            SetConvTolIterNum(parameters);
            SetResistance(parameters);
          }
          if (kineticmodel_ == INPAR::S2I::kinetics_butlervolmerreducedthermoresistance)
          {
            SetEnergySubstanceRatio(parameters);
            SetThermoPerm(parameters);
          }
          break;
        }

        default:
        {
          FOUR_C_THROW("Not implemented for this kinetic model: %i", kineticmodel_);
          break;
        }
      }

      // regularization is not relevant for scatra-scatra interface coupling without growth
      regularizationtype_ = INPAR::S2I::RegularizationType::regularization_none;

      break;
    }

    case DRT::Condition::ConditionType::S2IKineticsGrowth:
    {
      // set parameters to internal members depending on kinetic model
      switch (kineticmodel_)
      {
        case INPAR::S2I::growth_kinetics_butlervolmer:
        {
          SetAlpha(parameters);
          SetChargeTransferConstant(parameters);
          SetDensityMolarMass(parameters);
          SetNumElectrons(parameters);
          SetNumScal(parameters);
          SetRegularization(parameters);
          SetResistivity(parameters);
          SetStoichiometries(parameters);
          break;
        }

        default:
        {
          FOUR_C_THROW("Not implemented for this kinetic model: %i", kineticmodel_);
          break;
        }
      }
      break;
    }

    default:
    {
      FOUR_C_THROW("Not implemented for this condition type: %i", conditiontype_);
      break;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetAlpha(Teuchos::ParameterList& parameters)
{
  alphaa_ = parameters.get<double>("alpha_a", std::numeric_limits<double>::infinity());
  alphac_ = parameters.get<double>("alpha_c", std::numeric_limits<double>::infinity());
  if (alphaa_ <= 0.0) FOUR_C_THROW("Alpha a must be positive!");
  if (alphac_ <= 0.0) FOUR_C_THROW("Alpha c must be positive!");
  if (alphaa_ + alphac_ != 1.0) FOUR_C_THROW("Sum of Alpha a and Alpha c must be equal to one!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetChargeTransferConstant(
    Teuchos::ParameterList& parameters)
{
  kr_ = parameters.get<double>("k_r", -1.0);
  if (kr_ <= 0.0) FOUR_C_THROW("Charge transfer constant k_r is negative!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetConvTolIterNum(
    Teuchos::ParameterList& parameters)
{
  convtolimplicit_bv_ =
      parameters.get<double>("CONVTOL_IMPLBUTLERVOLMER", std::numeric_limits<double>::infinity());
  if (convtolimplicit_bv_ <= 0.0) FOUR_C_THROW("Tolerance of convergence must be positive!");
  itemaxmimplicit_bv_ =
      parameters.get<int>("ITEMAX_IMPLBUTLERVOLMER", std::numeric_limits<int>::infinity());
  if (itemaxmimplicit_bv_ <= 0) FOUR_C_THROW("Maximum number of iterations must be positive!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetDensityMolarMass(
    Teuchos::ParameterList& parameters)
{
  density_ = parameters.get<double>("density", std::numeric_limits<double>::infinity());
  if (density_ <= 0.0) FOUR_C_THROW("Density must be positive");

  molarmass_ = parameters.get<double>("molar mass", std::numeric_limits<double>::infinity());
  if (molarmass_ <= 0.0) FOUR_C_THROW("Molar mass must be positive");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetEnergySubstanceRatio(
    Teuchos::ParameterList& parameters)
{
  molar_heat_capacity_ =
      parameters.get<double>("molar_heat_capacity", std::numeric_limits<double>::infinity());
  if (molar_heat_capacity_ < 0.0) FOUR_C_THROW("Ratio of energy- and mass-flux must be positive!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetIsPseudoContact(
    Teuchos::ParameterList& parameters)
{
  is_pseudo_contact_ =
      (parameters.get<int>("is_pseudo_contact", std::numeric_limits<int>::infinity()) == 1);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetNumElectrons(Teuchos::ParameterList& parameters)
{
  numelectrons_ = parameters.get<int>("numelectrons", std::numeric_limits<int>::infinity());
  if (numelectrons_ != 1)
    FOUR_C_THROW(
        "Invalid number of electrons in charge transfer at electrode-electrolyte interface!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetNumScal(Teuchos::ParameterList& parameters)
{
  numscal_ = parameters.get<int>("numscal", std::numeric_limits<int>::infinity());
  if (numscal_ <= 0) FOUR_C_THROW("Scalar must be positive");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetPeltier(Teuchos::ParameterList& parameters)
{
  peltier_ = parameters.get<double>("peltier", std::numeric_limits<double>::infinity());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetPermeabilities(
    Teuchos::ParameterList& parameters)
{
  permeabilities_ = parameters.get<const std::vector<double>*>("permeabilities");
  for (auto permeability : *permeabilities_)
    if (permeability < 0.0) FOUR_C_THROW("Permeability must be positive");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetRegularization(
    Teuchos::ParameterList& parameters)
{
  regularizationparameter_ = parameters.get<double>("regpar", -1.0);
  if (regularizationparameter_ < 0.0)
    FOUR_C_THROW("Regularization parameter for lithium stripping must not be negative!");
  regularizationtype_ = static_cast<INPAR::S2I::RegularizationType>(
      parameters.get<int>("regtype", std::numeric_limits<int>::infinity()));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetResistance(Teuchos::ParameterList& parameters)
{
  resistance_ = parameters.get<double>("resistance", std::numeric_limits<double>::infinity());
  if (resistance_ <= 0.0) FOUR_C_THROW("Resistance must be positive");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetResistivity(Teuchos::ParameterList& parameters)
{
  resistivity_ = 1.0 / (parameters.get<double>("conductivity", -1.0));
  if (resistivity_ <= 0.0) FOUR_C_THROW("Conductivity must be positive");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetCapacitance(Teuchos::ParameterList& parameters)
{
  capacitance_ = parameters.get<double>("capacitance", -1.0);
  if (capacitance_ <= 0.0) FOUR_C_THROW("Capacitance must be positive");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetStoichiometries(
    Teuchos::ParameterList& parameters)
{
  stoichiometries_ = parameters.get<const std::vector<int>*>("stoichiometries");

  if (stoichiometries_ == nullptr)
  {
    FOUR_C_THROW(
        "Cannot get vector of stoichiometric coefficients for scatra-scatra interface coupling!");
  }
  else
  {
    if (stoichiometries_->size() != 1)
      FOUR_C_THROW("Number of stoichiometric coefficients does not match number of scalars!");
    if ((*stoichiometries_)[0] != -1) FOUR_C_THROW("Invalid stoichiometric coefficient!");
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetThermoPerm(Teuchos::ParameterList& parameters)
{
  thermoperm_ = parameters.get<double>("thermoperm", std::numeric_limits<double>::infinity());
  if (thermoperm_ <= 0.0) FOUR_C_THROW("Thermo permeability must be posititve!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::ScaTraEleParameterBoundary::SetOnOff(Teuchos::ParameterList& parameters)
{
  onoff_ = parameters.get<const std::vector<int>*>("onoff");
  if (onoff_ == nullptr) FOUR_C_THROW("Cannot get vector 'onoff' from parameter list");
  if (onoff_->size() != 2) FOUR_C_THROW("Only two dofs are supported");
}

FOUR_C_NAMESPACE_CLOSE
