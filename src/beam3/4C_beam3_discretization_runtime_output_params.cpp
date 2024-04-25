/*-----------------------------------------------------------------------------------------------*/
/*! \file

\brief input parameters related to output at runtime for beams

\level 3

*/
/*-----------------------------------------------------------------------------------------------*/

#include "4C_beam3_discretization_runtime_output_params.hpp"

#include "4C_utils_exceptions.hpp"
#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
DRT::ELEMENTS::BeamRuntimeOutputParams::BeamRuntimeOutputParams()
    : isinit_(false),
      issetup_(false),
      output_displacement_state_(false),
      use_absolute_positions_visualizationpoint_coordinates_(true),
      write_internal_energy_element_(false),
      write_kinetic_energy_element_(false),
      write_triads_visualizationpoints_(false),
      write_material_crosssection_strains_gausspoints_(false),
      write_material_crosssection_strains_continuous_(false),
      write_material_crosssection_stresses_gausspoints_(false),
      write_spatial_crosssection_stresses_gausspoints_(false),
      write_filament_condition_(false),
      write_orientation_parameter_(false),
      write_rve_crosssection_forces_(false),
      write_ref_length_(false),
      write_element_gid_(false),
      write_element_ghosting_(false),
      n_subsegments_(0)
{
  // empty constructor
}

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
void DRT::ELEMENTS::BeamRuntimeOutputParams::Init(
    const Teuchos::ParameterList& IO_vtk_structure_beams_paramslist)
{
  // We have to call Setup() after Init()
  issetup_ = false;

  // initialize the parameter values

  output_displacement_state_ =
      (bool)CORE::UTILS::IntegralValue<int>(IO_vtk_structure_beams_paramslist, "DISPLACEMENT");

  use_absolute_positions_visualizationpoint_coordinates_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "USE_ABSOLUTE_POSITIONS");

  write_internal_energy_element_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "INTERNAL_ENERGY_ELEMENT");

  write_kinetic_energy_element_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "KINETIC_ENERGY_ELEMENT");

  write_triads_visualizationpoints_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "TRIAD_VISUALIZATIONPOINT");

  write_material_crosssection_strains_gausspoints_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "STRAINS_GAUSSPOINT");

  write_material_crosssection_strains_continuous_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "STRAINS_CONTINUOUS");

  write_material_crosssection_stresses_gausspoints_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "MATERIAL_FORCES_GAUSSPOINT");

  write_material_crosssection_strains_continuous_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "MATERIAL_FORCES_CONTINUOUS");

  write_spatial_crosssection_stresses_gausspoints_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "SPATIAL_FORCES_GAUSSPOINT");

  write_orientation_parameter_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "ORIENTATION_PARAMETER");

  write_rve_crosssection_forces_ = (bool)CORE::UTILS::IntegralValue<int>(
      IO_vtk_structure_beams_paramslist, "RVE_CROSSSECTION_FORCES");

  write_ref_length_ =
      (bool)CORE::UTILS::IntegralValue<int>(IO_vtk_structure_beams_paramslist, "REF_LENGTH");

  write_element_gid_ =
      (bool)CORE::UTILS::IntegralValue<int>(IO_vtk_structure_beams_paramslist, "ELEMENT_GID");

  write_element_ghosting_ =
      (bool)CORE::UTILS::IntegralValue<int>(IO_vtk_structure_beams_paramslist, "ELEMENT_GHOSTING");

  n_subsegments_ = IO_vtk_structure_beams_paramslist.get<int>("NUMBER_SUBSEGMENTS");
  if (n_subsegments_ < 1)
    FOUR_C_THROW("The number of subsegments has to be at least 1. Got %d", n_subsegments_);

  isinit_ = true;
}

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
void DRT::ELEMENTS::BeamRuntimeOutputParams::Setup()
{
  if (not IsInit()) FOUR_C_THROW("Init() has not been called, yet!");

  // Nothing to do here at the moment

  issetup_ = true;
}

/*-----------------------------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------------------------*/
void DRT::ELEMENTS::BeamRuntimeOutputParams::CheckInitSetup() const
{
  if (not IsInit() or not IsSetup()) FOUR_C_THROW("Call Init() and Setup() first!");
}

FOUR_C_NAMESPACE_CLOSE
