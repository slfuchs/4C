/*----------------------------------------------------------------------*/
/*! \file
 \brief material for multiphase porous fluid

   \level 3

 *----------------------------------------------------------------------*/



#include "4C_mat_fluidporo_multiphase.hpp"

#include "4C_global_data.hpp"
#include "4C_mat_fluidporo_singlephase.hpp"
#include "4C_mat_par_bundle.hpp"
#include "4C_porofluidmultiphase_ele_calc_utils.hpp"

#include <Teuchos_SerialDenseSolver.hpp>

#include <vector>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 | constructor of paramter class                            vuong 08/16 |
 *----------------------------------------------------------------------*/
Mat::PAR::FluidPoroMultiPhase::FluidPoroMultiPhase(const Core::Mat::PAR::Parameter::Data& matdata)
    : MatList(matdata),
      permeability_(matdata.parameters.get<double>("PERMEABILITY")),
      numfluidphases_(matdata.parameters.get<int>("NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE")),
      numvolfrac_(-1),
      dof2pres_(Teuchos::null),
      constraintphaseID_(-1),
      isinit_(false)
{
}

/*----------------------------------------------------------------------*
 | create a poro multiphase material                        vuong 08/16 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::Mat::Material> Mat::PAR::FluidPoroMultiPhase::create_material()
{
  return Teuchos::rcp(new Mat::FluidPoroMultiPhase(this));
}

/*----------------------------------------------------------------------*
 | initialize                                               vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::PAR::FluidPoroMultiPhase::initialize()
{
  //  matrix holding the conversion from pressures and dofs
  dof2pres_ = Teuchos::rcp(new Core::LinAlg::SerialDenseMatrix(numfluidphases_, numfluidphases_));

  //  matrix holding the conversion from pressures and dofs
  // reset
  dof2pres_->putScalar(0.0);

  // get number of volume fractions
  numvolfrac_ = (int)(((int)matids_.size() - numfluidphases_) / 2);

  // safety check
  if ((int)matids_.size() != (int)(numvolfrac_ * 2 + numfluidphases_))
    FOUR_C_THROW(
        "You have chosen %i materials, %i fluidphases and %f volume fractions, check your input "
        "definition\n"
        "Your Input should always look like (for example: 4 fluid phases, 2 volume fractions):\n"
        "MAT 0 MAT_FluidPoroMultiPhase LOCAL No PERMEABILITY 1.0 NUMMAT 8 MATIDS    1 2 3 4 5 6 7 "
        "8 NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE 4 END\n"
        "with: 4 fluid phases in multiphase pore space: materials have to be "
        "MAT_FluidPoroSinglePhase \n"
        "      2 volume fractions: materials have to be MAT_FluidPoroSingleVolFrac \n"
        "      2 volume fraction pressures: materials have to be MAT_FluidPoroVolFracPressure ",
        (int)matids_.size(), numfluidphases_,
        (double)(((double)matids_.size() - (double)numfluidphases_) / 2.0));

  for (int iphase = 0; iphase < (int)matids_.size(); iphase++)
  {
    // get the single phase material by its ID
    const int matid = matids_[iphase];
    Teuchos::RCP<Core::Mat::Material> singlemat = material_by_id(matid);

    // fluidphases at [0...numfluidphases-1]
    if (iphase < numfluidphases_)
    {
      // safety check and cast
      if (singlemat->material_type() != Core::Materials::m_fluidporo_singlephase)
        FOUR_C_THROW(
            "You have chosen %i fluidphases, however your material number %i is no poro "
            "singlephase material\n"
            "Your Input should always look like (for example: 4 fluid phases, 2 volume "
            "fractions):\n"
            "MAT 0 MAT_FluidPoroMultiPhase LOCAL No PERMEABILITY 1.0 NUMMAT 8 MATIDS    1 2 3 4 5 "
            "6 7 8 NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE 4 END\n"
            "with: 4 fluid phases in multiphase pore space: materials have to be "
            "MAT_FluidPoroSinglePhase \n"
            "      2 volume fractions: materials have to be MAT_FluidPoroSingleVolFrac \n"
            "      2 volume fraction pressures: materials have to be MAT_FluidPoroVolFracPressure ",
            numfluidphases_, iphase + 1);

      const Mat::FluidPoroSinglePhase& singlephase =
          static_cast<const Mat::FluidPoroSinglePhase&>(*singlemat);

      if (singlephase.poro_phase_law_type() == Core::Materials::m_fluidporo_phaselaw_constraint)
      {
        if (constraintphaseID_ != -1)
          FOUR_C_THROW(
              "More than one constraint phase law defined. Are you sure this makes sense?");
        constraintphaseID_ = iphase;
      }

      // fill the coefficients into matrix
      singlephase.fill_do_f_matrix(*dof2pres_, iphase);
    }
    // volume fractions at [numfluidphases-1...numfluidphases-1+numvolfrac]
    else if (iphase < numfluidphases_ + numvolfrac_)
    {
      // safety check
      if (singlemat->material_type() != Core::Materials::m_fluidporo_singlevolfrac)
        FOUR_C_THROW(
            "You have chosen %i fluid phases and %i volume fractions, however your material number "
            "%i is no poro volume fraction material\n"
            "Your Input should always look like (for example: 4 fluid phases, 2 volume "
            "fractions):\n"
            "MAT 0 MAT_FluidPoroMultiPhase LOCAL No PERMEABILITY 1.0 NUMMAT 8 MATIDS    1 2 3 4 5 "
            "6 7 8 NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE 4 END\n"
            "with: 4 fluid phases in multiphase pore space: materials have to be "
            "MAT_FluidPoroSinglePhase \n"
            "      2 volume fractions: materials have to be MAT_FluidPoroSingleVolFrac \n"
            "      2 volume fraction pressures: materials have to be MAT_FluidPoroVolFracPressure ",
            numfluidphases_, (int)matids_.size() - numfluidphases_, iphase + 1);
    }
    // volume fraction pressures at [numfluidphases-1+numvolfrac...numfluidphases-1+2*numvolfrac]
    else if (iphase < numfluidphases_ + 2 * numvolfrac_)
    {
      // safety check
      if (singlemat->material_type() != Core::Materials::m_fluidporo_volfracpressure)
        FOUR_C_THROW(
            "You have chosen %i fluid phases and %i volume fractions, however your material number "
            "%i is no poro volume fraction pressure material\n"
            "Your Input should always look like (for example: 4 fluid phases, 2 volume "
            "fractions):\n"
            "MAT 0 MAT_FluidPoroMultiPhase LOCAL No PERMEABILITY 1.0 NUMMAT 8 MATIDS    1 2 3 4 5 "
            "6 7 8 NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE 4 END\n"
            "with: 4 fluid phases in multiphase pore space: materials have to be "
            "MAT_FluidPoroSinglePhase \n"
            "      2 volume fractions: materials have to be MAT_FluidPoroSingleVolFrac \n"
            "      2 volume fraction pressures: materials have to be MAT_FluidPoroVolFracPressure ",
            numfluidphases_, (int)matids_.size() - numfluidphases_, iphase + 1);
    }
    else
      FOUR_C_THROW("something went wrong here, why is iphase = %i", iphase);
  }

  // check
  if (constraintphaseID_ == -1 && numfluidphases_ > 0)
    FOUR_C_THROW(
        "No constraint phase law defined but NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE > 0. Are you "
        "sure this makes sense?");

  // invert dof2pres_ to get conversion from dofs to pressures for the fluid phases
  if (numfluidphases_ > 0)
  {
    using ordinalType = Core::LinAlg::SerialDenseMatrix::ordinalType;
    using scalarType = Core::LinAlg::SerialDenseMatrix::scalarType;
    Teuchos::SerialDenseSolver<ordinalType, scalarType> inverse;
    inverse.setMatrix(dof2pres_);
    int err = inverse.invert();
    if (err != 0)
      FOUR_C_THROW(
          "Inversion of matrix for DOF transform failed with errorcode %d. Is your system of DOFs "
          "linear independent?",
          err);
  }

  isinit_ = true;
  return;
}

/*----------------------------------------------------------------------*
 | global instance of parameter class                       vuong 08/16 |
 *----------------------------------------------------------------------*/
Mat::FluidPoroMultiPhaseType Mat::FluidPoroMultiPhaseType::instance_;

/*----------------------------------------------------------------------*
 | create material from data                                vuong 08/16 |
 *----------------------------------------------------------------------*/
Core::Communication::ParObject* Mat::FluidPoroMultiPhaseType::create(
    Core::Communication::UnpackBuffer& buffer)
{
  Mat::FluidPoroMultiPhase* FluidPoroMultiPhase = new Mat::FluidPoroMultiPhase();
  FluidPoroMultiPhase->unpack(buffer);
  return FluidPoroMultiPhase;
}


/*----------------------------------------------------------------------*
 | construct empty material object                          vuong 08/16 |
 *----------------------------------------------------------------------*/
Mat::FluidPoroMultiPhase::FluidPoroMultiPhase() : MatList(), paramsporo_(nullptr) {}

/*----------------------------------------------------------------------*
 | construct the material object given material parameter   vuong 08/16 |
 *----------------------------------------------------------------------*/
Mat::FluidPoroMultiPhase::FluidPoroMultiPhase(Mat::PAR::FluidPoroMultiPhase* params)
    : MatList(params), paramsporo_(params)
{
}

/*----------------------------------------------------------------------*
 | reset everything                                          vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::clear()
{
  paramsporo_ = nullptr;
  return;
}

/*----------------------------------------------------------------------*
 | initialize                                               vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::initialize()
{
  std::map<int, Teuchos::RCP<Core::Mat::Material>>* materials;

  if (parameter() != nullptr)  // params is null pointer in post-process mode
  {
    if (parameter()->local_)
      materials = material_map_write();
    else
      materials = parameter()->material_map_write();

    std::map<int, Teuchos::RCP<Core::Mat::Material>>::iterator it;
    for (it = materials->begin(); it != materials->end(); it++)
    {
      Teuchos::RCP<Mat::FluidPoroSinglePhaseBase> actphase =
          Teuchos::rcp_dynamic_cast<FluidPoroSinglePhaseBase>(it->second, true);
      actphase->initialize();
    }

    if (not paramsporo_->isinit_) paramsporo_->initialize();
  }
  return;
}

/*----------------------------------------------------------------------*
 | Unpack data from a char vector into this class            vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = unique_par_object_id();
  add_to_pack(data, type);

  // matid
  int matid = -1;
  if (paramsporo_ != nullptr) matid = paramsporo_->id();  // in case we are in post-process mode

  add_to_pack(data, matid);

  // Pack base class material
  Mat::MatList::pack(data);
}

/*----------------------------------------------------------------------*
 | Unpack data from a char vector into this class            vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::unpack(Core::Communication::UnpackBuffer& buffer)
{
  // make sure we have a pristine material
  clear();



  Core::Communication::extract_and_assert_id(buffer, unique_par_object_id());

  // matid and recover paramsporo_
  int matid(-1);
  extract_from_pack(buffer, matid);
  paramsporo_ = nullptr;
  if (Global::Problem::instance()->materials() != Teuchos::null)
    if (Global::Problem::instance()->materials()->num() != 0)
    {
      const int probinst = Global::Problem::instance()->materials()->get_read_from_problem();
      Core::Mat::PAR::Parameter* mat =
          Global::Problem::instance(probinst)->materials()->parameter_by_id(matid);
      if (mat->type() == material_type())
      {
        // Note: We need to do a dynamic_cast here since Chemotaxis, Reaction, and Chemo-reaction
        // are in a diamond inheritance structure
        paramsporo_ = dynamic_cast<Mat::PAR::FluidPoroMultiPhase*>(mat);
      }
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->type(),
            material_type());
    }

  // extract base class material
  std::vector<char> basedata(0);
  Mat::MatList::extract_from_pack(buffer, basedata);
  Core::Communication::UnpackBuffer basedata_buffer(basedata);
  Mat::MatList::unpack(basedata_buffer);

  // in the postprocessing mode, we do not unpack everything we have packed
  // -> position check cannot be done in this case
  FOUR_C_THROW_UNLESS(buffer.at_end(), "Buffer not fully consumed.");
}

/*----------------------------------------------------------------------*
 *  Evaluate generalized pressure of all phases            vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::evaluate_gen_pressure(
    std::vector<double>& genpressure, const std::vector<double>& phinp) const
{
  // evaluate the pressures
  for (int iphase = 0; iphase < num_fluid_phases(); iphase++)
  {
    // get the single phase material
    const Mat::FluidPoroSinglePhase& singlephasemat =
        POROFLUIDMULTIPHASE::ElementUtils::get_single_phase_mat_from_multi_material(*this, iphase);

    // evaluate generalized pressure (i.e. some kind of linear combination of the true pressures)
    genpressure[iphase] = singlephasemat.evaluate_gen_pressure(iphase, phinp);
  }
  return;
}

/*----------------------------------------------------------------------*
 *   Evaluate saturation of the phase                       vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::evaluate_saturation(std::vector<double>& saturation,
    const std::vector<double>& phinp, const std::vector<double>& pressure) const
{
  // get the number of the phase, which saturation is calculated by the saturation constraint
  const int constraintsaturationphase = paramsporo_->constraintphaseID_;

  // the constraint saturation is calculated from 1- sum(all other saturations)
  saturation[constraintsaturationphase] = 1.0;
  for (int iphase = 0; iphase < num_fluid_phases(); iphase++)
  {
    if (iphase != constraintsaturationphase)
    {
      // get the single phase material
      const Mat::FluidPoroSinglePhase& singlephasemat =
          POROFLUIDMULTIPHASE::ElementUtils::get_single_phase_mat_from_multi_material(
              *this, iphase);

      saturation[iphase] = singlephasemat.evaluate_saturation(iphase, phinp, pressure);
      // the saturation of the last phase is 1.0- (sum of all saturations)
      saturation[constraintsaturationphase] -= saturation[iphase];
    }
  }
  return;
}

/*----------------------------------------------------------------------*
 | transform generalized pressures to true pressures        vuong 08/16 |
 *----------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::transform_gen_pres_to_true_pres(
    const std::vector<double>& phinp, std::vector<double>& phi_transformed) const
{
  // get trafo matrix
  const Core::LinAlg::SerialDenseMatrix& dof2pres = *paramsporo_->dof2pres_;
  // simple matrix vector product
  phi_transformed.resize(phinp.size());
  for (int i = 0; i < num_fluid_phases(); i++)
    for (int j = 0; j < num_fluid_phases(); j++) phi_transformed[i] += dof2pres(i, j) * phinp[j];
  return;
}

/*----------------------------------------------------------------------------------------*
 * Evaluate derivative of degree of freedom with respect to pressure          vuong 08/16 |
 *----------------------------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::evaluate_deriv_of_dof_wrt_pressure(
    Core::LinAlg::SerialDenseMatrix& derivs, const std::vector<double>& state) const
{
  for (int iphase = 0; iphase < num_fluid_phases(); iphase++)
  {
    // get the single phase material by its ID
    const int matid = mat_id(iphase);
    Teuchos::RCP<Core::Mat::Material> singlemat = material_by_id(matid);
    const Mat::FluidPoroSinglePhase& singlephase =
        static_cast<const Mat::FluidPoroSinglePhase&>(*singlemat);

    for (int jphase = 0; jphase < num_fluid_phases(); jphase++)
    {
      derivs(iphase, jphase) =
          singlephase.evaluate_deriv_of_dof_wrt_pressure(iphase, jphase, state);
    }
  }
  return;
}

/*--------------------------------------------------------------------------*
 *  Evaluate derivative of saturation w.r.t. pressure           vuong 08/16 |
 *---------------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::evaluate_deriv_of_saturation_wrt_pressure(
    Core::LinAlg::SerialDenseMatrix& derivs, const std::vector<double>& pressure) const
{
  // get the number of the phase, which saturation is calculated by the saturation constraint
  const int constraintsaturationphase = paramsporo_->constraintphaseID_;

  for (int iphase = 0; iphase < num_fluid_phases(); iphase++)
  {
    // skip constraint saturation phase
    if (iphase == constraintsaturationphase) continue;

    // get the single phase material by its ID
    const int matid = mat_id(iphase);
    Teuchos::RCP<Core::Mat::Material> singlemat = material_by_id(matid);
    const Mat::FluidPoroSinglePhase& singlephase =
        static_cast<const Mat::FluidPoroSinglePhase&>(*singlemat);

    for (int jphase = 0; jphase < num_fluid_phases(); jphase++)
    {
      const double saturationderiv =
          singlephase.evaluate_deriv_of_saturation_wrt_pressure(iphase, jphase, pressure);
      derivs(iphase, jphase) = saturationderiv;
      // the saturation of the last phase is 1.0- (sum of all saturations)
      // -> the derivative of this saturation = -1.0 (sum of all saturation derivatives)
      derivs(constraintsaturationphase, jphase) += -1.0 * saturationderiv;
    }
  }
  return;
}

/*--------------------------------------------------------------------------*
 *  Evaluate 2nd derivative of saturation w.r.t. pressure  kremheller 05/17 |
 *---------------------------------------------------------------------------*/
void Mat::FluidPoroMultiPhase::evaluate_second_deriv_of_saturation_wrt_pressure(
    std::vector<Core::LinAlg::SerialDenseMatrix>& derivs, const std::vector<double>& pressure) const
{
  // get the number of the phase, which saturation is calculated by the saturation constraint
  const int constraintsaturationphase = paramsporo_->constraintphaseID_;

  for (int iphase = 0; iphase < num_fluid_phases(); iphase++)
  {
    // skip constraint saturation phase
    if (iphase == constraintsaturationphase) continue;

    // get the single phase material by its ID
    const int matid = mat_id(iphase);
    Teuchos::RCP<Core::Mat::Material> singlemat = material_by_id(matid);
    const Mat::FluidPoroSinglePhase& singlephase =
        static_cast<const Mat::FluidPoroSinglePhase&>(*singlemat);

    for (int jphase = 0; jphase < num_fluid_phases(); jphase++)
    {
      for (int kphase = 0; kphase < num_fluid_phases(); kphase++)
      {
        const double saturationderivderiv =
            singlephase.evaluate_second_deriv_of_saturation_wrt_pressure(
                iphase, jphase, kphase, pressure);
        derivs[iphase](jphase, kphase) = saturationderivderiv;
        // the saturation of the last phase is 1.0- (sum of all saturations)
        // -> the derivative of this saturation = -1.0 (sum of all saturation derivatives)
        derivs[constraintsaturationphase](jphase, kphase) += -1.0 * saturationderivderiv;
      }
    }
  }
  return;
}

FOUR_C_NAMESPACE_CLOSE
