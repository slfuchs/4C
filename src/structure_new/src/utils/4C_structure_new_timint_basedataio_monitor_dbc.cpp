/*-----------------------------------------------------------------------------------------------*/
/*! \file

\brief input parameters related monitoring reaction forces for the structural (time) integration

\level 3

*/
/*-----------------------------------------------------------------------------------------------*/

#include "4C_structure_new_timint_basedataio_monitor_dbc.hpp"

#include "4C_utils_exceptions.hpp"
#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN


/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
STR::TIMINT::ParamsMonitorDBC::ParamsMonitorDBC()
    : isinit_(false),
      issetup_(false),
      output_interval_steps_(-1),
      of_precision_(-1),
      os_precision_(-1),
      file_type_("none"),
      write_header_(false)
{
  // empty constructor
}

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
void STR::TIMINT::ParamsMonitorDBC::Init(
    const Teuchos::ParameterList& IO_monitor_dbc_structure_paramslist)
{
  // We have to call Setup() after Init()
  issetup_ = false;

  // output intervall in steps
  output_interval_steps_ = IO_monitor_dbc_structure_paramslist.get<int>("INTERVAL_STEPS");

  // file precision
  of_precision_ = IO_monitor_dbc_structure_paramslist.get<int>("PRECISION_FILE");

  // screen precision
  os_precision_ = IO_monitor_dbc_structure_paramslist.get<int>("PRECISION_SCREEN");

  // file type
  file_type_ = IO_monitor_dbc_structure_paramslist.get<std::string>("FILE_TYPE");

  // write header in csv file
  write_header_ =
      CORE::UTILS::IntegralValue<int>(IO_monitor_dbc_structure_paramslist, "WRITE_HEADER");

  isinit_ = true;
}

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
void STR::TIMINT::ParamsMonitorDBC::Setup()
{
  FOUR_C_ASSERT(is_init(), "Init() has not been called, yet!");

  // Nothing to do here at the moment

  issetup_ = true;
}

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
void STR::TIMINT::ParamsMonitorDBC::check_init_setup() const
{
  FOUR_C_ASSERT(is_init() and is_setup(), "Call Init() and Setup() first!");
}

FOUR_C_NAMESPACE_CLOSE
