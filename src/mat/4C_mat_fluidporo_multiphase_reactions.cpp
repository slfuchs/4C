/*----------------------------------------------------------------------*/
/*! \file
 \brief a fluid material for porous multiphase flow with reactions (mass sources and sinks)

   \level 3

 *----------------------------------------------------------------------*/


#include "4C_mat_fluidporo_multiphase_reactions.hpp"

#include "4C_global_data.hpp"
#include "4C_mat_fluidporo_multiphase_singlereaction.hpp"
#include "4C_mat_par_bundle.hpp"

#include <vector>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 | rstandard constructor                                     vuong 08/16 |
 *----------------------------------------------------------------------*/
MAT::PAR::FluidPoroMultiPhaseReactions::FluidPoroMultiPhaseReactions(
    Teuchos::RCP<CORE::MAT::PAR::Material> matdata)
    : FluidPoroMultiPhase(matdata),
      numreac_((matdata->Get<int>("NUMREAC"))),
      reacids_((matdata->Get<std::vector<int>>("REACIDS")))
{
  // check if sizes fit
  if (numreac_ != (int)reacids_.size())
    FOUR_C_THROW("number of materials %d does not fit to size of material vector %d", nummat_,
        reacids_.size());

  if (numreac_ < 1)
    FOUR_C_THROW("if you don't have reactions, use MAT_matlist instead of MAT_matlist_reactions!");

  if (not local_)
  {
    // make sure the referenced materials in material list have quick access parameters
    std::vector<int>::const_iterator m;
    for (m = reacids_.begin(); m != reacids_.end(); ++m)
    {
      const int reacid = *m;
      Teuchos::RCP<CORE::MAT::Material> mat = MAT::Factory(reacid);

      // safety check and cast
      if (mat->MaterialType() != CORE::Materials::m_fluidporo_singlereaction)
        FOUR_C_THROW("only MAT_FluidPoroSingleReaction material valid");
      MAT::FluidPoroSingleReaction singlereacmat = static_cast<MAT::FluidPoroSingleReaction&>(*mat);
      if (singlereacmat.TotalNumDof() != this->nummat_)
        FOUR_C_THROW(
            "TOTALNUMDOF in MAT_FluidPoroSingleReaction does not correspond to NUMMAT in "
            "MAT_FluidPoroMultiPhaseReactions");

      MaterialMapWrite()->insert(std::pair<int, Teuchos::RCP<CORE::MAT::Material>>(reacid, mat));
    }
  }
}

Teuchos::RCP<CORE::MAT::Material> MAT::PAR::FluidPoroMultiPhaseReactions::CreateMaterial()
{
  return Teuchos::rcp(new MAT::FluidPoroMultiPhaseReactions(this));
}


MAT::FluidPoroMultiPhaseReactionsType MAT::FluidPoroMultiPhaseReactionsType::instance_;


CORE::COMM::ParObject* MAT::FluidPoroMultiPhaseReactionsType::Create(const std::vector<char>& data)
{
  MAT::FluidPoroMultiPhaseReactions* FluidPoroMultiPhaseReactions =
      new MAT::FluidPoroMultiPhaseReactions();
  FluidPoroMultiPhaseReactions->Unpack(data);
  return FluidPoroMultiPhaseReactions;
}


/*----------------------------------------------------------------------*
 | construct empty material object                           vuong 08/16 |
 *----------------------------------------------------------------------*/
MAT::FluidPoroMultiPhaseReactions::FluidPoroMultiPhaseReactions()
    : FluidPoroMultiPhase(), paramsreac_(nullptr)
{
}

/*----------------------------------------------------------------------*
 | construct the material object given material paramete     vuong 08/16 |
 *----------------------------------------------------------------------*/
MAT::FluidPoroMultiPhaseReactions::FluidPoroMultiPhaseReactions(
    MAT::PAR::FluidPoroMultiPhaseReactions* params)
    : FluidPoroMultiPhase(params), paramsreac_(params)
{
  // setup of material map
  if (paramsreac_->local_)
  {
    setup_mat_map();
  }
}

/*----------------------------------------------------------------------*
 | setup of material map                                     vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::FluidPoroMultiPhaseReactions::setup_mat_map()
{
  // We just have to add the reaction materials, since the rest is already done in
  // MAT::MatList::setup_mat_map() called from the MatList constructor

  // here's the recursive creation of materials
  std::vector<int>::const_iterator m;
  for (m = paramsreac_->ReacIds()->begin(); m != paramsreac_->ReacIds()->end(); ++m)
  {
    const int reacid = *m;
    Teuchos::RCP<CORE::MAT::Material> mat = MAT::Factory(reacid);
    if (mat == Teuchos::null) FOUR_C_THROW("Failed to allocate this material");
    MaterialMapWrite()->insert(std::pair<int, Teuchos::RCP<CORE::MAT::Material>>(reacid, mat));
  }
  return;
}

/*----------------------------------------------------------------------*
 | reset everything                                          vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::FluidPoroMultiPhaseReactions::Clear()
{
  paramsreac_ = nullptr;
  return;
}

/*----------------------------------------------------------------------*
 | Unpack data from a char vector into this class            vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::FluidPoroMultiPhaseReactions::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (paramsreac_ != nullptr) matid = paramsreac_->Id();  // in case we are in post-process mode

  AddtoPack(data, matid);

  // Pack base class material
  MAT::FluidPoroMultiPhase::Pack(data);
}

/*----------------------------------------------------------------------*
 | Unpack data from a char vector into this class            vuong 08/16 |
 *----------------------------------------------------------------------*/
void MAT::FluidPoroMultiPhaseReactions::Unpack(const std::vector<char>& data)
{
  // make sure we have a pristine material
  Clear();

  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid and recover paramsreac_
  int matid(-1);
  ExtractfromPack(position, data, matid);
  paramsreac_ = nullptr;
  if (GLOBAL::Problem::Instance()->Materials() != Teuchos::null)
    if (GLOBAL::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();
      CORE::MAT::PAR::Parameter* mat =
          GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
      {
        // Note: We need to do a dynamic_cast here since Chemotaxis, Reaction, and Chemo-reaction
        // are in a diamond inheritance structure
        paramsreac_ = dynamic_cast<MAT::PAR::FluidPoroMultiPhaseReactions*>(mat);
      }
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }

  // extract base class material
  std::vector<char> basedata(0);
  MAT::FluidPoroMultiPhase::ExtractfromPack(position, data, basedata);
  MAT::FluidPoroMultiPhase::Unpack(basedata);

  // in the postprocessing mode, we do not unpack everything we have packed
  // -> position check cannot be done in this case
  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", data.size(), position);
}

/*----------------------------------------------------------------------*
 | reaction ID by Index                                      vuong 08/16 |
 *----------------------------------------------------------------------*/
int MAT::FluidPoroMultiPhaseReactions::ReacID(const unsigned index) const
{
  if ((int)index < paramsreac_->numreac_)
    return paramsreac_->reacids_.at(index);
  else
  {
    FOUR_C_THROW("Index too large");
    return -1;
  }
}

FOUR_C_NAMESPACE_CLOSE
