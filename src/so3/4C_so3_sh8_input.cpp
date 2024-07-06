/*----------------------------------------------------------------------*/
/*! \file
\brief solid shell8 element formulation
\level 1

*----------------------------------------------------------------------*/


#include "4C_io_linedefinition.hpp"
#include "4C_mat_so3_material.hpp"
#include "4C_so3_sh8.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool Discret::ELEMENTS::SoSh8::read_element(
    const std::string& eletype, const std::string& distype, Input::LineDefinition* linedef)
{
  // read number of material model
  int material_id = 0;
  linedef->extract_int("MAT", material_id);
  set_material(0, Mat::Factory(material_id));

  solid_material()->setup(NUMGPT_SOH8, linedef);

  // temporary variable for read-in
  std::string buffer;


  // read kinematic flag
  linedef->extract_string("KINEM", buffer);
  if (buffer == "linear")
  {
    // kintype_ = soh8_linear;
    FOUR_C_THROW("Only nonlinear kinematics for SO_SH8 implemented!");
  }
  else if (buffer == "nonlinear")
  {
    kintype_ = Inpar::Solid::KinemType::nonlinearTotLag;
  }
  else
    FOUR_C_THROW("Reading SO_HEX8p1j1 element failed KINEM unknown");

  // check if material kinematics is compatible to element kinematics
  solid_material()->valid_kinematics(kintype_);

  // read EAS technology flag
  linedef->extract_string("EAS", buffer);

  // full EAS technology
  if (buffer == "sosh8")
  {
    eastype_ = soh8_eassosh8;
    neas_ = 7;  // number of eas parameters for EAS_SOSH8
    soh8_easinit();
  }
  // no EAS technology
  else if (buffer == "none")
  {
    eastype_ = soh8_easnone;
    neas_ = 0;  // number of eas parameters for EAS_SOSH8
  }
  else
    FOUR_C_THROW("Reading of SO_SH8 EAS technology failed");

  // read ANS technology flag
  linedef->extract_string("ANS", buffer);
  if (buffer == "sosh8")
  {
    anstype_ = anssosh8;
  }
  // no ANS technology
  else if (buffer == "none")
  {
    anstype_ = ansnone;
  }
  else
    FOUR_C_THROW("Reading of SO_SH8 ANS technology failed");

  linedef->extract_string("THICKDIR", buffer);
  nodes_rearranged_ = false;

  // global X
  if (buffer == "xdir") thickdir_ = globx;
  // global Y
  else if (buffer == "ydir")
    thickdir_ = globy;
  // global Z
  else if (buffer == "zdir")
    thickdir_ = globz;
  // find automatically through Jacobian of Xrefe
  else if (buffer == "auto")
    thickdir_ = autoj;
  // local r
  else if (buffer == "rdir")
    thickdir_ = enfor;
  // local s
  else if (buffer == "sdir")
    thickdir_ = enfos;
  // local t
  else if (buffer == "tdir")
    thickdir_ = enfot;
  // no noderearrangement
  else if (buffer == "none")
  {
    thickdir_ = none;
    nodes_rearranged_ = true;
  }
  else
    FOUR_C_THROW("Reading of SO_SH8 thickness direction failed");

  return true;
}

FOUR_C_NAMESPACE_CLOSE
