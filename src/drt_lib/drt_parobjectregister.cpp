/*----------------------------------------------------------------------------*/
/*!
\file drt_parobjectregister.cpp
\brief Implementation of registration of parallel objects
\level 1
\maintainer Martin Kronbichler
*/
/*----------------------------------------------------------------------------*/

#include <sstream>
#include <string>
#include <iostream>

#include "drt_parobjectregister.H"

#include "../drt_nurbs_discret/drt_control_point.H"
#include "../drt_beam3/beam3.H"
#include "../drt_beam3/beam3r.H"
#include "../drt_beam3/beam3eb.H"
#include "../drt_beam3/beam3k.H"
#include "../drt_discsh3/discsh3.H"
#include "../drt_rigidsphere/rigidsphere.H"
#include "../drt_truss3/truss3.H"
#include "../drt_torsion3/torsion3.H"
#include "../drt_s8/shell8.H"
#include "../drt_s8/shell8_scatra.H"
#include "../drt_membrane/membrane_eletypes.H"
#include "../drt_lubrication_ele/lubrication_ele.H"
#include "../drt_scatra_ele/scatra_ele.H"
#include "../drt_porofluidmultiphase_ele/porofluidmultiphase_ele.H"
#include "../drt_particle/particle_node.H"
#include "../drt_fluid_ele/fluid_ele.H"
#include "../drt_fluid_ele/fluid_ele_poro.H"
#include "../drt_fluid_ele/fluid_ele_immersed.H"
#include "../drt_fluid_ele/fluid_ele_poro_immersed.H"
#include "../drt_fluid_ele/fluid_ele_xwall.H"
#include "../drt_fluid_ele/fluid_ele_hdg.H"
#include "../drt_ale2/ale2.H"
#include "../drt_ale2/ale2_nurbs.H"
#include "../drt_ale3/ale3.H"
#include "../drt_ale3/ale3_nurbs.H"
#include "../drt_bele3/bele3.H"
#include "../drt_bele3/vele3.H"
#include "../drt_bele3/bele2.H"
#include "../drt_constraint/constraint_element2.H"
#include "../drt_constraint/constraint_element3.H"
#include "../drt_w1/wall1.H"
#include "../drt_w1/wall1_nurbs.H"
#include "../drt_w1/wall1_poro_eletypes.H"
#include "../drt_w1/wall1_poro_p1_eletypes.H"
#include "../drt_w1/wall1_scatra.H"
#include "../drt_w1/wall1_poro_scatra_eletypes.H"
#include "../drt_w1/wall1_poro_p1_scatra_eletypes.H"
#include "../drt_so3/so_hex8.H"
#include "../drt_so3/so_hex18.H"
#include "../drt_so3/so_sh18.H"
#include "../drt_so3/so_hex20.H"
#include "../drt_so3/so_hex27.H"
#include "../drt_so3/so_nurbs27.H"
#include "../drt_so3/so_sh8.H"
#include "../drt_so3/so_sh8p8.H"
#include "../drt_so3/so_tet4.H"
#include "../drt_so3/so_tet4av.H"
//#include "../drt_so3/so_ptet.H"
#include "../drt_so3/so_nstet.H"
#include "../drt_so3/so_nstet5.H"
#include "../drt_so3/so_tet10.H"
#include "../drt_so3/so_weg6.H"
#include "../drt_so3/so_pyramid5.H"
#include "../drt_so3/so_shw6.H"
#include "../drt_so3/so_hex8p1j1.H"
#include "../drt_so3/so_hex8fbar.H"
#include "../drt_so3/so_pyramid5fbar.H"
#include "../drt_so3/so3_poro_eletypes.H"
#include "../drt_so3/so3_poro_p1_eletypes.H"
#include "../drt_so3/so3_scatra_eletypes.H"
#include "../drt_so3/so3_poro_scatra_eletypes.H"
#include "../drt_so3/so3_poro_p1_scatra_eletypes.H"
#include "../drt_so3/so3_thermo_eletypes.H"
#include "../drt_so3/so3_plast/so3_ssn_plast_eletypes.H"
#include "../drt_so3/so3_plast/so3_ssn_plast_sosh18.H"
#include "../drt_so3/so3_plast/so3_ssn_plast_sosh8.H"
#include "../drt_thermo/thermo_element.H"
#include "../drt_mat/newtonianfluid.H"
#include "../drt_mat/stvenantkirchhoff.H"
#include "../drt_mat/thermostvenantkirchhoff.H"
#include "../drt_mat/thermoplasticlinelast.H"
#include "../drt_mat/micromaterial.H"
#include "../drt_mat/neohooke.H"
#include "../drt_mat/aaaneohooke.H"
#include "../drt_mat/aaaneohooke_stopro.H"
#include "../drt_mat/aaaraghavanvorp_damage.H"
#include "../drt_mat/aaa_mixedeffects.H"
#include "../drt_mat/aaagasser.H"
#include "../drt_mat/visconeohooke.H"
#include "../drt_mat/viscoanisotropic.H"
#include "../drt_mat/scalardepinterp.H"
#include "../drt_mat/scatra_mat.H"
#include "../drt_mat/scatra_mat_poro_ecm.H"
#include "../drt_mat/scatra_mat_multiporo.H"
#include "../drt_mat/myocard.H"
#include "../drt_mat/ion.H"
#include "../drt_mat/mixfrac.H"
#include "../drt_mat/sutherland.H"
#include "../drt_mat/cavitationfluid.H"
#include "../drt_mat/arrhenius_spec.H"
#include "../drt_mat/arrhenius_temp.H"
#include "../drt_mat/arrhenius_pv.H"
#include "../drt_mat/ferech_pv.H"
#include "../drt_mat/carreauyasuda.H"
#include "../drt_mat/modpowerlaw.H"
#include "../drt_mat/herschelbulkley.H"
#include "../drt_mat/yoghurt.H"
#include "../drt_mat/matlist.H"
#include "../drt_mat/matlist_reactions.H"
#include "../drt_mat/matlist_chemotaxis.H"
#include "../drt_mat/matlist_chemoreac.H"
#include "../drt_mat/elchmat.H"
#include "../drt_mat/elasthyper.H"
#include "../drt_mat/plasticelasthyper.H"
#include "../drt_mat/plastic_VarConstUpdate.H"
#include "../drt_mat/viscoelasthyper.H"
#include "../drt_mat/cnst_1d_art.H"
#include "../drt_mat/fourieriso.H"
#include "../drt_mat/membrane_elasthyper.H"
#include "../drt_mat/growthremodel_elasthyper.H"
#include "../drt_mat/constraintmixture.H"
#include "../drt_mat/constraintmixture_history.H"
#include "../drt_mat/plasticlinelast.H"
#include "../drt_mat/robinson.H"
#include "../drt_mat/damage.H"
#include "../drt_mat/spring.H"
#include "../drt_mat/beam_elasthyper.H"
#include "../drt_mat/optimization_density.H"
#include "../drt_mat/structporo.H"
#include "../drt_mat/structporo_reaction.H"
#include "../drt_mat/structporo_reaction_ecm.H"
#include "../drt_mat/fluidporo.H"
#include "../drt_mat/fluidporo_singlephase.H"
#include "../drt_mat/fluidporo_multiphase.H"
#include "../drt_mat/fluidporo_multiphase_reactions.H"
#include "../drt_mat/fluidporo_multiphase_singlereaction.H"
#include "../drt_mat/fluidporo_relpermeability_law.H"
#include "../drt_mat/fluidporo_viscosity_law.H"
#include "../drt_mat/acoustic.H"
#include "../drt_mat/acoustic_sol.H"
#include "../drt_mortar/mortar_node.H"
#include "../drt_mortar/mortar_element.H"
#include "../drt_contact/contact_node.H"
#include "../drt_contact/friction_node.H"
#include "../drt_contact/contact_element.H"
#include "../drt_art_net/artery.H"
#include "../drt_red_airways/red_airway.H"
#include "../drt_opti/topopt_optimizer_ele.H"
#include "../drt_acou/acou_ele.H"
#include "../drt_acou/acou_sol_ele.H"
#include "../drt_beaminteraction/beam_link_beam3r_lin2_pinjointed.H"
#include "../drt_beaminteraction/beam_link_beam3r_lin2_rigidjointed.H"
#include "../drt_beaminteraction/beam_link_truss.H"
#include "../drt_mat/activefiber.H"
#include "../drt_mat/biochemo_mechano_cell_activefiber.H"
#include "../drt_mat/biochemo_mechano_cell_passivefiber.H"
#include "../drt_immersed_problem/immersed_node.H"
#include "../drt_mat/maxwell_0d_acinus.H"
#include "../drt_mat/maxwell_0d_acinus_NeoHookean.H"
#include "../drt_mat/maxwell_0d_acinus_Exponential.H"
#include "../drt_mat/maxwell_0d_acinus_DoubleExponential.H"
#include "../drt_mat/maxwell_0d_acinus_Ogden.H"
#include "../drt_inv_analysis/particle_data.H"
#include "../drt_beaminteraction/crosslinker_node.H"
#include "../drt_binstrategy/drt_meshfree_multibin.H"
#include "../drt_mat/growth.H"

std::string DRT::ParObjectList()
{
  std::stringstream s;

  s << DRT::ContainerType::Instance().Name() << " "
    << DRT::ConditionObjectType::Instance().Name() << " "
    << DRT::NodeType::Instance().Name() << " "
    << DRT::NURBS::ControlPointType::Instance().Name() << " "
    << PARTICLE::ParticleNodeType::Instance().Name() << " "
    << IMMERSED::ImmersedNodeType::Instance().Name() << " "
    << CROSSLINKING::CrosslinkerNodeType::Instance().Name() << " "
    << DRT::MESHFREE::MeshfreeMultiBinType::Instance().Name() << " "
    << DRT::ELEMENTS::Beam3Type::Instance().Name() << " "
    << DRT::ELEMENTS::Beam3rType::Instance().Name() << " "
    << DRT::ELEMENTS::Beam3ebType::Instance().Name() << " "
    << DRT::ELEMENTS::Beam3kType::Instance().Name() << " "
    << DRT::ELEMENTS::DiscSh3Type::Instance().Name() << " "
    << DRT::ELEMENTS::RigidsphereType::Instance().Name() << " "
    << DRT::ELEMENTS::Truss3Type::Instance().Name() << " "
    << DRT::ELEMENTS::Torsion3Type::Instance().Name() << " "
    << DRT::ELEMENTS::Shell8Type::Instance().Name() << " "
    << DRT::ELEMENTS::Shell8ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::Membrane_tri3Type::Instance().Name() << " "
    << DRT::ELEMENTS::Membrane_tri6Type::Instance().Name() << " "
    << DRT::ELEMENTS::Membrane_quad4Type::Instance().Name() << " "
    << DRT::ELEMENTS::Membrane_quad9Type::Instance().Name() << " "
    << DRT::ELEMENTS::Wall1Type::Instance().Name() << " "
    << DRT::ELEMENTS::WallTri3PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::WallTri3PoroP1Type::Instance().Name() << " "
    << DRT::ELEMENTS::WallQuad4PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::WallQuad4PoroP1Type::Instance().Name() << " "
    << DRT::ELEMENTS::WallQuad9PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::WallQuad9PoroP1Type::Instance().Name() << " "
    << DRT::ELEMENTS::WallNurbs4PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::WallNurbs9PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::NURBS::Wall1NurbsType::Instance().Name() << " "
    << DRT::ELEMENTS::Wall1ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::WallQuad4PoroScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::WallQuad4PoroP1ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidXWallType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidXWallBoundaryType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidTypeImmersed::Instance().Name() << " "
    << DRT::ELEMENTS::FluidTypePoroImmersed::Instance().Name() << " "
    << DRT::ELEMENTS::FluidPoroEleType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidHDGType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidBoundaryType::Instance().Name() << " "
    << DRT::ELEMENTS::FluidPoroBoundaryType::Instance().Name() << " "
    << DRT::ELEMENTS::Ale3Type::Instance().Name() << " "
    << DRT::ELEMENTS::NURBS::Ale3_NurbsType::Instance().Name() << " "
    << DRT::ELEMENTS::Ale2Type::Instance().Name() << " "
    << DRT::ELEMENTS::NURBS::Ale2_NurbsType::Instance().Name() << " "
    << DRT::ELEMENTS::Bele2Type::Instance().Name() << " "
    << DRT::ELEMENTS::Bele3Type::Instance().Name() << " "
    << DRT::ELEMENTS::Vele3Type::Instance().Name() << " "
    << DRT::ELEMENTS::NStetType::Instance().Name() << " "
    << DRT::ELEMENTS::NStet5Type::Instance().Name() << " "
    << DRT::ELEMENTS::NURBS::So_nurbs27Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_nurbs27PoroType::Instance().Name() << " "
//    << DRT::ELEMENTS::PtetType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex18Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_sh18Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_sh18PlastType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_Hex8P1J1Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8fbarType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8fbarScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8fbarThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8PoroP1Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8PlastType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex8Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex20Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex27Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex27ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex27PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex27ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_nurbs27ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex20ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_hex27PlastType::Instance().Name() << " "
    << DRT::ELEMENTS::So_sh8Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_sh8PlastType::Instance().Name() << " "
    << DRT::ELEMENTS::So_sh8p8Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_shw6Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet10Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet10PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet10ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4PlastType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4PoroType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4PoroP1Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4PoroScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4PoroP1ScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet4avType::Instance().Name() << " "
    << DRT::ELEMENTS::So_tet10ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::So_weg6Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_pyramid5Type::Instance().Name() << " "
    << DRT::ELEMENTS::So_pyramid5fbarType::Instance().Name() << " "
    << DRT::ELEMENTS::ArteryType::Instance().Name() << " "
    << DRT::ELEMENTS::RedAirwayType::Instance().Name() << " "
    << DRT::ELEMENTS::RedAcinusType::Instance().Name() << " "
    << DRT::ELEMENTS::RedInterAcinarDepType::Instance().Name() << " "
    << DRT::ELEMENTS::RedAirBloodScatraType::Instance().Name() << " "
    << DRT::ELEMENTS::RedAirBloodScatraLine3Type::Instance().Name() << " "
    << DRT::ELEMENTS::ConstraintElement2Type::Instance().Name() << " "
    << DRT::ELEMENTS::ConstraintElement3Type::Instance().Name() << " "
    << DRT::ELEMENTS::LubricationType::Instance().Name() << " "
    << DRT::ELEMENTS::PoroFluidMultiPhaseType::Instance().Name() << " "
    << DRT::ELEMENTS::TransportType::Instance().Name() << " "
    << DRT::ELEMENTS::TopOptType::Instance().Name() << " "
    << DRT::ELEMENTS::ThermoType::Instance().Name() << " "
    << DRT::ELEMENTS::AcouType::Instance().Name() << " "
    << DRT::ELEMENTS::AcouSolType::Instance().Name() << " "
    << DRT::ELEMENTS::AcouBoundaryType::Instance().Name() << " "
    << DRT::ELEMENTS::AcouSolBoundaryType::Instance().Name() << " "
    << DRT::ELEMENTS::AcouIntFaceType::Instance().Name() << " "
    << DRT::ELEMENTS::AcouSolIntFaceType::Instance().Name() << " "
    << MAT::Cnst_1d_artType::Instance().Name() << " "
    << MAT::AAAgasserType::Instance().Name() << " "
    << MAT::AAAneohookeType::Instance().Name() << " "
    << MAT::AAAneohooke_stoproType::Instance().Name() << " "
    << MAT::AAAraghavanvorp_damageType::Instance().Name() << " "
    << MAT::AAA_mixedeffectsType::Instance().Name() << " "
    << MAT::ArrheniusPVType::Instance().Name() << " "
    << MAT::ArrheniusSpecType::Instance().Name() << " "
    << MAT::ArrheniusTempType::Instance().Name() << " "
    << MAT::CarreauYasudaType::Instance().Name() << " "
    << MAT::CavitationFluidType::Instance().Name() << " "
    << MAT::ConstraintMixtureType::Instance().Name() << " "
    << MAT::ConstraintMixtureHistoryType::Instance().Name() << " "
    << MAT::ElastHyperType::Instance().Name() << " "
    << MAT::PlasticElastHyperType::Instance().Name() << " "
    << MAT::PlasticElastHyperVCUType::Instance().Name() << " "
    << MAT::ViscoElastHyperType::Instance().Name() << " "
    << MAT::FerEchPVType::Instance().Name() << " "
    << MAT::FluidPoroType::Instance().Name() << " "
    << MAT::FluidPoroSinglePhaseType::Instance().Name() << " "
    << MAT::FluidPoroSingleVolFracType::Instance().Name() << " "
    << MAT::FluidPoroVolFracPressureType::Instance().Name() << " "
    << MAT::FluidPoroSingleReactionType::Instance().Name() << " "
    << MAT::FluidPoroMultiPhaseType::Instance().Name() << " "
    << MAT::FluidPoroMultiPhaseReactionsType::Instance().Name() << " "
    << MAT::FourierIsoType::Instance().Name() << " "
    << MAT::GrowthVolumetricType::Instance().Name() << " "
    << MAT::Membrane_ElastHyperType::Instance().Name() << " "
    << MAT::GrowthRemodel_ElastHyperType::Instance().Name() << " "
    << MAT::HerschelBulkleyType::Instance().Name() << " "
    << MAT::IonType::Instance().Name() << " "
    << MAT::MatListType::Instance().Name() << " "
    << MAT::MatListReactionsType::Instance().Name() << " "
    << MAT::MatListChemotaxisType::Instance().Name() << " "
    << MAT::MatListChemoReacType::Instance().Name() << " "
    << MAT::ElchMatType::Instance().Name() << " "
    << MAT::MicroMaterialType::Instance().Name() << " "
    << MAT::MixFracType::Instance().Name() << " "
    << MAT::ModPowerLawType::Instance().Name() << " "
    << MAT::MyocardType::Instance().Name() << " "
    << MAT::NeoHookeType::Instance().Name() << " "
    << MAT::NewtonianFluidType::Instance().Name() << " "
    << MAT::StructPoroType::Instance().Name() << " "
    << MAT::StructPoroReactionType::Instance().Name() << " "
    << MAT::StructPoroReactionECMType::Instance().Name() << " "
    << MAT::ScalarDepInterpType::Instance().Name() << " "
    << MAT::ScatraMatType::Instance().Name() << " "
    << MAT::ScatraMatPoroECMType::Instance().Name() << " "
    << MAT::ScatraMatMultiPoroFluidType::Instance().Name() << " "
    << MAT::ScatraMatMultiPoroVolFracType::Instance().Name() << " "
    << MAT::StVenantKirchhoffType::Instance().Name() << " "
    << MAT::SutherlandType::Instance().Name() << " "
    << MAT::ThermoStVenantKirchhoffType::Instance().Name() << " "
    << MAT::ThermoPlasticLinElastType::Instance().Name() << " "
    << MAT::ViscoAnisotropicType::Instance().Name() << " "
    << MAT::ViscoNeoHookeType::Instance().Name() << " "
    << MAT::YoghurtType::Instance().Name() << " "
    << MAT::SpringType::Instance().Name() << " "
    << MAT::BeamElastHyperMaterialType::Instance().Name() << " "
    << MAT::PlasticLinElastType::Instance().Name() << " "
    << MAT::RobinsonType::Instance().Name() << " "
    << MAT::DamageType::Instance().Name() << " "
    << MAT::TopOptDensType::Instance().Name() << " "
    << MAT::AcousticMatType::Instance().Name() << " "
    << MAT::AcousticSolMatType::Instance().Name() << " "
    << MAT::Maxwell_0d_acinusType::Instance().Name() << " "
    << MAT::Maxwell_0d_acinusNeoHookeanType::Instance().Name() << " "
    << MAT::Maxwell_0d_acinusExponentialType::Instance().Name() << " "
    << MAT::Maxwell_0d_acinusDoubleExponentialType::Instance().Name() << " "
    << MAT::Maxwell_0d_acinusOgdenType::Instance().Name() << " "
    << MORTAR::MortarNodeType::Instance().Name() << " "
    << MORTAR::MortarElementType::Instance().Name() << " "
    << CONTACT::CoNodeType::Instance().Name() << " "
    << CONTACT::FriNodeType::Instance().Name() << " "
    << CONTACT::CoElementType::Instance().Name() << " "
    << MAT::ActiveFiberType::Instance().Name() << " "
    << MAT::BioChemoMechanoCellActiveFiberType::Instance().Name() << " "
    << MAT::BioChemoMechanoCellPassiveFiberType::Instance().Name() << " "
    << INVANA::ParticleDataType::Instance().Name() << " "
    << BEAMINTERACTION::BeamLinkBeam3rLin2RigidJointedType::Instance().Name() << " "
    << BEAMINTERACTION::BeamLinkBeam3rLin2PinJointedType::Instance().Name() << " "
    << BEAMINTERACTION::BeamLinkTrussType::Instance().Name() << " "
    ;

  std::cout << s.str() << std::endl;
  return s.str();
}


void PrintParObjectList()
{
  std::cout << "defined parobject types: " << DRT::ParObjectList() << "\n";
}
