/*----------------------------------------------------------------------------*/
/*! \file

\brief data container holding all beam to beam contact input parameters

\level 3

*/
/*----------------------------------------------------------------------------*/

#include "baci_beaminteraction_beam_to_beam_contact_params.hpp"

#include "baci_global_data.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
BEAMINTERACTION::BeamToBeamContactParams::BeamToBeamContactParams()
    : isinit_(false),
      issetup_(false),
      strategy_(INPAR::BEAMCONTACT::bstr_none),
      penalty_law_(INPAR::BEAMCONTACT::pl_lp),
      BTB_penalty_law_regularization_G0_(-1.0),
      BTB_penalty_law_regularization_F0_(-1.0),
      BTB_penalty_law_regularization_C0_(-1.0),
      gap_shift_(0.0),
      BTB_point_penalty_param_(-1.0),
      BTB_line_penalty_param_(-1.0),
      BTB_perp_shifting_angle1_(-1.0),
      BTB_perp_shifting_angle2_(-1.0),
      BTB_parallel_shifting_angle1_(-1.0),
      BTB_parallel_shifting_angle2_(-1.0),
      segangle_(-1.0),
      num_integration_intervals_(0),
      BTB_basicstiff_gap_(-1.0),
      BTB_endpoint_penalty_(false)
{
  // empty constructor
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::BeamToBeamContactParams::Init()
{
  issetup_ = false;

  // Teuchos parameter list for beam contact
  const Teuchos::ParameterList& beam_contact_params_list =
      GLOBAL::Problem::Instance()->BeamContactParams();

  /****************************************************************************/
  // get and check required parameters
  /****************************************************************************/
  strategy_ = CORE::UTILS::IntegralValue<INPAR::BEAMCONTACT::Strategy>(
      beam_contact_params_list, "BEAMS_STRATEGY");

  if (strategy_ != INPAR::BEAMCONTACT::bstr_penalty)
    FOUR_C_THROW(
        "currently only a penalty strategy is supported for beam contact"
        " if not using the 'old' beam contact manager!");

  /****************************************************************************/
  penalty_law_ = CORE::UTILS::IntegralValue<INPAR::BEAMCONTACT::PenaltyLaw>(
      beam_contact_params_list, "BEAMS_PENALTYLAW");

  /****************************************************************************/
  BTB_penalty_law_regularization_G0_ = beam_contact_params_list.get<double>("BEAMS_PENREGPARAM_G0");
  BTB_penalty_law_regularization_F0_ = beam_contact_params_list.get<double>("BEAMS_PENREGPARAM_F0");
  BTB_penalty_law_regularization_C0_ = beam_contact_params_list.get<double>("BEAMS_PENREGPARAM_C0");

  // Todo check and refine these safety checks
  if (penalty_law_ != INPAR::BEAMCONTACT::pl_lp and penalty_law_ != INPAR::BEAMCONTACT::pl_qp)
  {
    if (BTB_penalty_law_regularization_G0_ == -1.0 or BTB_penalty_law_regularization_F0_ == -1.0 or
        BTB_penalty_law_regularization_C0_ == -1.0)
      FOUR_C_THROW(
          "Regularized penalty law chosen, but not all regularization parameters are set!");
  }

  /****************************************************************************/
  // Todo check this parameter
  gap_shift_ = beam_contact_params_list.get<double>("BEAMS_GAPSHIFTPARAM");

  if (gap_shift_ != 0.0 and penalty_law_ != INPAR::BEAMCONTACT::pl_lpqp)
    FOUR_C_THROW("BEAMS_GAPSHIFTPARAM only possible for penalty law LinPosQuadPen!");

  /****************************************************************************/
  BTB_point_penalty_param_ = beam_contact_params_list.get<double>("BEAMS_BTBPENALTYPARAM");

  if (BTB_point_penalty_param_ < 0.0)
    FOUR_C_THROW("beam-to-beam point penalty parameter must not be negative!");


  // input parameters required for all-angle-beam contact formulation ...
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_SEGCON"))
  {
    /****************************************************************************/
    BTB_line_penalty_param_ = beam_contact_params_list.get<double>("BEAMS_BTBLINEPENALTYPARAM");

    if (BTB_line_penalty_param_ < 0.0)
      FOUR_C_THROW(
          "You chose all-angle-beam contact algorithm: thus, beam-to-beam line"
          " penalty parameter must not be negative!");

    /****************************************************************************/
    // Todo find more verbose and expressive naming
    // note: conversion from degrees (input parameter) to radians (class variable) done here!
    BTB_perp_shifting_angle1_ =
        beam_contact_params_list.get<double>("BEAMS_PERPSHIFTANGLE1") / 180.0 * M_PI;
    BTB_perp_shifting_angle2_ =
        beam_contact_params_list.get<double>("BEAMS_PERPSHIFTANGLE2") / 180.0 * M_PI;

    BTB_parallel_shifting_angle1_ =
        beam_contact_params_list.get<double>("BEAMS_PARSHIFTANGLE1") / 180.0 * M_PI;
    BTB_parallel_shifting_angle2_ =
        beam_contact_params_list.get<double>("BEAMS_PARSHIFTANGLE2") / 180.0 * M_PI;

    if (BTB_perp_shifting_angle1_ < 0.0 or BTB_perp_shifting_angle2_ < 0.0 or
        BTB_parallel_shifting_angle1_ < 0.0 or BTB_parallel_shifting_angle2_ < 0.0)
      FOUR_C_THROW(
          "You chose all-angle-beam contact algorithm: thus, shifting angles for"
          " beam-to-beam contact fade must be >= 0 degrees");

    if (BTB_perp_shifting_angle1_ > 0.5 * M_PI or BTB_perp_shifting_angle2_ > 0.5 * M_PI or
        BTB_parallel_shifting_angle1_ > 0.5 * M_PI or BTB_parallel_shifting_angle2_ > 0.5 * M_PI)
      FOUR_C_THROW(
          "You chose all-angle-beam contact algorithm: thus, Shifting angles for"
          " beam-to-beam contact fade must be <= 90 degrees");

    if (BTB_parallel_shifting_angle2_ <= BTB_perp_shifting_angle1_)
      FOUR_C_THROW("No angle overlap between large-angle and small-angle contact!");

    /****************************************************************************/
    // note: conversion from degrees (input parameter) to radians (class variable) done here!
    segangle_ = beam_contact_params_list.get<double>("BEAMS_SEGANGLE") / 180.0 * M_PI;

    if (segangle_ <= 0.0) FOUR_C_THROW("Segmentation angle must be greater than zero!");

    /****************************************************************************/
    num_integration_intervals_ = beam_contact_params_list.get<int>("BEAMS_NUMINTEGRATIONINTERVAL");

    if (num_integration_intervals_ <= 0)
      FOUR_C_THROW("Number of integration intervals must be greater than zero!");
  }

  /****************************************************************************/
  // Todo check need and usage of this parameter
  BTB_basicstiff_gap_ = beam_contact_params_list.get<double>("BEAMS_BASICSTIFFGAP");

  /****************************************************************************/
  BTB_endpoint_penalty_ =
      CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_ENDPOINTPENALTY");

  /****************************************************************************/
  // safety checks for currently unsupported parameter settings
  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_NEWGAP"))
    FOUR_C_THROW("BEAMS_NEWGAP currently not supported!");

  /****************************************************************************/
  // for the time being only allow all-angle-beam contact formulation ...
  if (not CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_SEGCON"))
    FOUR_C_THROW(
        "only all-angle-beam contact (BEAMS_SEGCON) formulation tested yet"
        " in new beam interaction framework!");

  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_DEBUG"))
    FOUR_C_THROW("get rid of this nasty BEAMS_DEBUG flag");

  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_INACTIVESTIFF"))
    FOUR_C_THROW("get rid of BEAMS_INACTIVESTIFF flag; no longer supported!");

  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_BTSOL") or
      beam_contact_params_list.get<double>("BEAMS_BTSPENALTYPARAM") != 0.0)
    FOUR_C_THROW("currently only beam-to-(BEAM/SPHERE) contact supported!");

  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_SMOOTHING") !=
      INPAR::BEAMCONTACT::bsm_none)
    FOUR_C_THROW("BEAMS_SMOOTHING currently not supported!");

  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_DAMPING") !=
          INPAR::BEAMCONTACT::bd_no or
      beam_contact_params_list.get<double>("BEAMS_DAMPINGPARAM") != -1000.0 or
      beam_contact_params_list.get<double>("BEAMS_DAMPREGPARAM1") != -1000.0 or
      beam_contact_params_list.get<double>("BEAMS_DAMPREGPARAM2") != -1000.0)
    FOUR_C_THROW("BEAMS_DAMPING currently not supported!");

  /****************************************************************************/
  if (beam_contact_params_list.get<double>("BEAMS_MAXDISISCALEFAC") != -1.0 or
      beam_contact_params_list.get<double>("BEAMS_MAXDELTADISSCALEFAC") != -1.0)
    FOUR_C_THROW("BEAMS_MAXDISISCALEFAC and BEAMS_MAXDELTADISSCALEFAC currently not supported!");

  /****************************************************************************/
  if (BTB_basicstiff_gap_ != -1.0) FOUR_C_THROW("BEAMS_BASICSTIFFGAP currently not supported!");

  /****************************************************************************/
  if (CORE::UTILS::IntegralValue<INPAR::BEAMCONTACT::OctreeType>(
          beam_contact_params_list, "BEAMS_OCTREE") != INPAR::BEAMCONTACT::boct_none or
      CORE::UTILS::IntegralValue<int>(beam_contact_params_list, "BEAMS_ADDITEXT") != true or
      beam_contact_params_list.get<int>("BEAMS_TREEDEPTH") != 6 or
      beam_contact_params_list.get<int>("BEAMS_BOXESINOCT") != 8)
    FOUR_C_THROW(
        "you seem to have set a search-related parameter in the beam contact section! "
        "this is not applicable in case of binning!");

  // Todo BEAMS_EXTVAL is missing here

  isinit_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::BeamToBeamContactParams::Setup()
{
  CheckInit();

  // empty for now

  issetup_ = true;
}

FOUR_C_NAMESPACE_CLOSE
