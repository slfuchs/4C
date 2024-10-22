// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_fluid_utils_mapextractor.hpp"

#include "4C_fem_condition_selector.hpp"
#include "4C_fem_condition_utils.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::MapExtractor::setup(
    const Core::FE::Discretization& dis, bool withpressure, bool overlapping, const int nds_master)
{
  const int ndim = Global::Problem::instance()->n_dim();
  Core::Conditions::MultiConditionSelector mcs;
  mcs.set_overlapping(overlapping);  // defines if maps can overlap
  mcs.add_selector(Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(
      dis, "FSICoupling", 0, ndim + withpressure));
  mcs.add_selector(Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(
      dis, "FREESURFCoupling", 0, ndim + withpressure));
  mcs.add_selector(Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(
      dis, "StructAleCoupling", 0, ndim + withpressure));
  mcs.add_selector(Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(
      dis, "Mortar", 0, ndim + withpressure));
  mcs.add_selector(Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(
      dis, "ALEUPDATECoupling", 0, ndim + withpressure));
  mcs.setup_extractor(dis, *dis.dof_row_map(nds_master), *this);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::MapExtractor::setup(
    Teuchos::RCP<const Epetra_Map>& additionalothermap, const FLD::Utils::MapExtractor& extractor)
{
  // build the new othermap
  std::vector<Teuchos::RCP<const Epetra_Map>> othermaps;
  othermaps.push_back(additionalothermap);
  othermaps.push_back(extractor.other_map());

  if (Core::LinAlg::MultiMapExtractor::intersect_maps(othermaps)->NumGlobalElements() > 0)
    FOUR_C_THROW("Failed to add dofmap of foreign discretization to other_map. Detected overlap.");

  Teuchos::RCP<const Epetra_Map> mergedothermap =
      Core::LinAlg::MultiMapExtractor::merge_maps(othermaps);

  // the vector of maps for the new map extractor consists of othermap at position 0
  // followed by the maps of conditioned DOF
  std::vector<Teuchos::RCP<const Epetra_Map>> maps;
  // append the merged other map at first position
  maps.push_back(mergedothermap);

  // append the condition maps subsequently
  for (int i = 1; i < extractor.num_maps(); ++i) maps.push_back(extractor.Map(i));

  // merge
  Teuchos::RCP<const Epetra_Map> fullmap = Core::LinAlg::MultiMapExtractor::merge_maps(maps);

  Core::LinAlg::MultiMapExtractor::setup(*fullmap, maps);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<std::set<int>> FLD::Utils::MapExtractor::conditioned_element_map(
    const Core::FE::Discretization& dis) const
{
  Teuchos::RCP<std::set<int>> condelements =
      Core::Conditions::conditioned_element_map(dis, "FSICoupling");
  Teuchos::RCP<std::set<int>> condelements2 =
      Core::Conditions::conditioned_element_map(dis, "FREESURFCoupling");
  Teuchos::RCP<std::set<int>> condelements3 =
      Core::Conditions::conditioned_element_map(dis, "StructAleCoupling");
  Teuchos::RCP<std::set<int>> condelements4 =
      Core::Conditions::conditioned_element_map(dis, "Mortar");
  Teuchos::RCP<std::set<int>> condelements5 =
      Core::Conditions::conditioned_element_map(dis, "ALEUPDATECoupling");
  std::copy(condelements2->begin(), condelements2->end(),
      std::inserter(*condelements, condelements->begin()));
  std::copy(condelements3->begin(), condelements3->end(),
      std::inserter(*condelements, condelements->begin()));
  std::copy(condelements4->begin(), condelements4->end(),
      std::inserter(*condelements, condelements->begin()));
  std::copy(condelements5->begin(), condelements5->end(),
      std::inserter(*condelements, condelements->begin()));
  return condelements;
}

void FLD::Utils::VolumetricFlowMapExtractor::setup(const Core::FE::Discretization& dis)
{
  const int ndim = Global::Problem::instance()->n_dim();
  Core::Conditions::MultiConditionSelector mcs;
  mcs.set_overlapping(true);  // defines if maps can overlap
  mcs.add_selector(Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(
      dis, "VolumetricSurfaceFlowCond", 0, ndim));
  mcs.setup_extractor(dis, *dis.dof_row_map(), *this);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::KSPMapExtractor::setup(const Core::FE::Discretization& dis)
{
  Core::Conditions::MultiConditionSelector mcs;
  mcs.add_selector(
      Teuchos::make_rcp<Core::Conditions::ConditionSelector>(dis, "KrylovSpaceProjection"));
  mcs.setup_extractor(dis, *dis.dof_row_map(), *this);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<std::set<int>> FLD::Utils::KSPMapExtractor::conditioned_element_map(
    const Core::FE::Discretization& dis) const
{
  Teuchos::RCP<std::set<int>> condelements =
      Core::Conditions::conditioned_element_map(dis, "KrylovSpaceProjection");
  return condelements;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::VelPressExtractor::setup(const Core::FE::Discretization& dis)
{
  const int ndim = Global::Problem::instance()->n_dim();
  Core::LinAlg::create_map_extractor_from_discretization(dis, ndim, *this);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::FsiMapExtractor::setup(const Core::FE::Discretization& dis)
{
  const int ndim = Global::Problem::instance()->n_dim();
  Core::Conditions::MultiConditionSelector mcs;
  mcs.add_selector(
      Teuchos::make_rcp<Core::Conditions::NDimConditionSelector>(dis, "FSICoupling", 0, ndim));
  mcs.setup_extractor(dis, *dis.dof_row_map(), *this);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::FsiMapExtractor::setup(Teuchos::RCP<const Epetra_Map>& additionalothermap,
    const FLD::Utils::FsiMapExtractor& extractor)
{
  // build the new othermap
  std::vector<Teuchos::RCP<const Epetra_Map>> othermaps;
  othermaps.push_back(additionalothermap);
  othermaps.push_back(extractor.other_map());

  if (Core::LinAlg::MultiMapExtractor::intersect_maps(othermaps)->NumGlobalElements() > 0)
    FOUR_C_THROW("Failed to add dofmap of foreign discretization to other_map. Detected overlap.");

  Teuchos::RCP<const Epetra_Map> mergedothermap =
      Core::LinAlg::MultiMapExtractor::merge_maps(othermaps);

  // the vector of maps for the new map extractor consists of othermap at position 0
  // followed by the maps of conditioned DOF
  std::vector<Teuchos::RCP<const Epetra_Map>> maps;
  // append the merged other map at first position
  maps.push_back(mergedothermap);

  // append the condition maps subsequently
  for (int i = 1; i < extractor.num_maps(); ++i) maps.push_back(extractor.Map(i));

  // merge
  Teuchos::RCP<const Epetra_Map> fullmap = Core::LinAlg::MultiMapExtractor::merge_maps(maps);

  Core::LinAlg::MultiMapExtractor::setup(*fullmap, maps);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::Utils::XFluidFluidMapExtractor::setup(const Epetra_Map& fullmap,
    Teuchos::RCP<const Epetra_Map> fluidmap, Teuchos::RCP<const Epetra_Map> xfluidmap)
{
  std::vector<Teuchos::RCP<const Epetra_Map>> maps;
  maps.push_back(fluidmap);
  maps.push_back(xfluidmap);
  MultiMapExtractor::setup(fullmap, maps);
}

FOUR_C_NAMESPACE_CLOSE
