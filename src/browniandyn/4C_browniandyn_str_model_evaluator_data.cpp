/*---------------------------------------------------------------------*/
/*! \file

\brief Concrete implementation of the brownian dynamic parameter interface


\date Jun 22, 2016

\level 3

*/
/*---------------------------------------------------------------------*/


#include "4C_global_data.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_timint_base.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Solid::MODELEVALUATOR::BrownianDynData::BrownianDynData()
    : isinit_(false),
      issetup_(false),
      str_data_ptr_(Teuchos::null),
      viscosity_(0.0),
      kt_(0.0),
      maxrandforce_(0.0),
      timeintconstrandnumb_(0.0),
      beam_damping_coeff_specified_via_(Inpar::BROWNIANDYN::vague),
      beams_damping_coefficient_prefactors_perunitlength_(0),
      randomforces_(Teuchos::null)
{
  // empty constructor
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::MODELEVALUATOR::BrownianDynData::init(
    const Teuchos::RCP<const Solid::MODELEVALUATOR::Data>& str_data_ptr)
{
  issetup_ = false;

  str_data_ptr_ = str_data_ptr;

  const Teuchos::ParameterList& browndyn_params_list =
      Global::Problem::instance()->brownian_dynamics_params();

  // viscosity
  viscosity_ = browndyn_params_list.get<double>("VISCOSITY");
  // thermal energy
  kt_ = browndyn_params_list.get<double>("KT");
  // maximum random force (specified as multiple of standard deviation around mean value)
  maxrandforce_ = browndyn_params_list.get<double>("MAXRANDFORCE");
  // time interval with constant random forces
  timeintconstrandnumb_ = browndyn_params_list.get<double>("TIMESTEP");

  // the way how damping coefficient values for beams are specified
  beam_damping_coeff_specified_via_ =
      Core::UTILS::IntegralValue<Inpar::BROWNIANDYN::BeamDampingCoefficientSpecificationType>(
          browndyn_params_list, "BEAMS_DAMPING_COEFF_SPECIFIED_VIA");

  // if input file is chosen, get the required values and check them for sanity
  if (beam_damping_coeff_specified_via_ == Inpar::BROWNIANDYN::input_file)
  {
    std::istringstream input_file_linecontent(Teuchos::getNumericStringParameter(
        browndyn_params_list, "BEAMS_DAMPING_COEFF_PER_UNITLENGTH"));

    std::string word;
    char* input;
    while (input_file_linecontent >> word)
      beams_damping_coefficient_prefactors_perunitlength_.push_back(
          std::strtod(word.c_str(), &input));

    if (not beams_damping_coefficient_prefactors_perunitlength_.empty())
    {
      if (beams_damping_coefficient_prefactors_perunitlength_.size() != 3)
      {
        std::cout << "\ngiven beam damping coefficient values: ";
        for (unsigned int i = 0; i < beams_damping_coefficient_prefactors_perunitlength_.size();
             ++i)
          std::cout << beams_damping_coefficient_prefactors_perunitlength_[i] << " ";


        FOUR_C_THROW(
            "Expected 3 values for beam damping coefficients if specified via input file "
            "but got %d! Check your input file!",
            beams_damping_coefficient_prefactors_perunitlength_.size());
      }

      if (beams_damping_coefficient_prefactors_perunitlength_[0] < 0.0 or
          beams_damping_coefficient_prefactors_perunitlength_[0] < 0.0 or
          beams_damping_coefficient_prefactors_perunitlength_[0] < 0.0)
      {
        FOUR_C_THROW("The damping coefficients for beams must not be negative!");
      }
    }
  }
  // safety check for valid input parameter
  else if (beam_damping_coeff_specified_via_ == Inpar::BROWNIANDYN::vague)
  {
    FOUR_C_THROW("The way how beam damping coefficients are specified is not properly set!");
  }

  // set flag
  isinit_ = true;

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::MODELEVALUATOR::BrownianDynData::setup()
{
  check_init();

  // set flag
  issetup_ = true;

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::MODELEVALUATOR::BrownianDynData::resize_random_force_m_vector(
    Teuchos::RCP<Core::FE::Discretization> discret_ptr, int maxrandnumelement)
{
  check_init_setup();

  // resize in case of new crosslinkers that were set and are now part of the discretization
  randomforces_ = Teuchos::rcp(
      new Epetra_MultiVector(*(discret_ptr->element_col_map()), maxrandnumelement, true));

  return;
}

FOUR_C_NAMESPACE_CLOSE
