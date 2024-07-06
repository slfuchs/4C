/*----------------------------------------------------------------------*/
/*! \file
\brief
This file contains the hyperelastic toolbox. It allows summing up several summands
of several types (isotropic or anisotropic, splitted or not) to build a hyperelastic
strain energy function.

The input line should read
MAT 0   MAT_ElastHyper   NUMMAT 2 MATIDS 1 2 DENS 0

\level 1


*/
/*----------------------------------------------------------------------*/

#include "4C_mat_elasthyper.hpp"

#include "4C_global_data.hpp"
#include "4C_linalg_fixedsizematrix_voigt_notation.hpp"
#include "4C_mat_elasthyper_service.hpp"
#include "4C_mat_par_bundle.hpp"
#include "4C_mat_service.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::PAR::ElastHyper::ElastHyper(const Core::Mat::PAR::Parameter::Data& matdata)
    : Parameter(matdata),
      nummat_(matdata.parameters.get<int>("NUMMAT")),
      matids_(matdata.parameters.get<std::vector<int>>("MATIDS")),
      density_(matdata.parameters.get<double>("DENS")),
      polyconvex_(matdata.parameters.get<int>("POLYCONVEX"))

{
  // check if sizes fit
  if (nummat_ != (int)matids_.size())
    FOUR_C_THROW("number of materials %d does not fit to size of material vector %d", nummat_,
        matids_.size());

  // output, that polyconvexity is checked
  if (polyconvex_ != 0) std::cout << "Polyconvexity of your simulation is checked." << std::endl;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Core::Mat::Material> Mat::PAR::ElastHyper::create_material()
{
  return Teuchos::rcp(new Mat::ElastHyper(this));
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::ElastHyperType Mat::ElastHyperType::instance_;


Core::Communication::ParObject* Mat::ElastHyperType::create(const std::vector<char>& data)
{
  auto* elhy = new Mat::ElastHyper();
  elhy->unpack(data);

  return elhy;
}


/*----------------------------------------------------------------------*
 |  initialise static arrays                                 bborn 08/09|
 *----------------------------------------------------------------------*/



/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::ElastHyper::ElastHyper() : summandProperties_(), params_(nullptr), potsum_(0), anisotropy_() {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::ElastHyper::ElastHyper(Mat::PAR::ElastHyper* params)
    : summandProperties_(), params_(params), potsum_(0), anisotropy_()
{
  // make sure the referenced materials in material list have quick access parameters
  std::vector<int>::const_iterator m;
  for (m = params_->matids_.begin(); m != params_->matids_.end(); ++m)
  {
    const int matid = *m;
    Teuchos::RCP<Mat::Elastic::Summand> sum = Mat::Elastic::Summand::factory(matid);
    if (sum == Teuchos::null) FOUR_C_THROW("Failed to allocate");
    potsum_.push_back(sum);
    sum->register_anisotropy_extensions(anisotropy_);
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = unique_par_object_id();
  add_to_pack(data, type);
  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->id();  // in case we are in post-process mode
  add_to_pack(data, matid);
  summandProperties_.pack(data);

  anisotropy_.pack_anisotropy(data);

  if (params_ != nullptr)  // summands are not accessible in postprocessing mode
  {
    // loop map of associated potential summands
    for (const auto& p : potsum_)
    {
      p->pack_summand(data);
    }
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::unpack(const std::vector<char>& data)
{
  // make sure we have a pristine material
  params_ = nullptr;
  potsum_.clear();

  std::vector<char>::size_type position = 0;

  Core::Communication::ExtractAndAssertId(position, data, unique_par_object_id());

  // matid and recover params_
  int matid;
  extract_from_pack(position, data, matid);
  if (Global::Problem::instance()->materials() != Teuchos::null)
  {
    if (Global::Problem::instance()->materials()->num() != 0)
    {
      const int probinst = Global::Problem::instance()->materials()->get_read_from_problem();
      Core::Mat::PAR::Parameter* mat =
          Global::Problem::instance(probinst)->materials()->parameter_by_id(matid);
      if (mat->type() == material_type())
        params_ = dynamic_cast<Mat::PAR::ElastHyper*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->type(),
            material_type());
    }
  }

  summandProperties_.unpack(position, data);

  // Pack anisotropy
  anisotropy_.unpack_anisotropy(data, position);

  if (params_ != nullptr)  // summands are not accessible in postprocessing mode
  {
    // make sure the referenced materials in material list have quick access parameters
    std::vector<int>::const_iterator m;
    for (m = params_->matids_.begin(); m != params_->matids_.end(); ++m)
    {
      const int summand_matid = *m;
      Teuchos::RCP<Mat::Elastic::Summand> sum = Mat::Elastic::Summand::factory(summand_matid);
      if (sum == Teuchos::null) FOUR_C_THROW("Failed to allocate");
      potsum_.push_back(sum);
    }

    // loop map of associated potential summands
    for (auto& p : potsum_)
    {
      p->unpack_summand(data, position);
      p->register_anisotropy_extensions(anisotropy_);
    }

    // in the postprocessing mode, we do not unpack everything we have packed
    // -> position check cannot be done in this case
    if (position != data.size())
    {
      FOUR_C_THROW("Mismatch in size of data %d <-> %d", data.size(), position);
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
int Mat::ElastHyper::mat_id(const unsigned index) const
{
  if ((int)index >= params_->nummat_)
  {
    FOUR_C_THROW("Index too large");
  }

  return params_->matids_.at(index);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double Mat::ElastHyper::shear_mod() const
{
  // principal coefficients
  bool haveshearmod = false;
  double shearmod = 0.0;
  {
    // loop map of associated potential summands
    for (const auto& p : potsum_)
    {
      p->add_shear_mod(haveshearmod, shearmod);
    }
  }
  if (!haveshearmod)
  {
    FOUR_C_THROW("Cannot provide shear modulus equivalent");
  }
  return shearmod;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double Mat::ElastHyper::get_young()
{
  double young;
  double shear;
  double bulk;
  young = shear = bulk = 0.;
  for (auto& p : potsum_) p->add_youngs_mod(young, shear, bulk);

  if (bulk != 0. || shear != 0.) young += 9. * bulk * shear / (3. * bulk + shear);

  return young;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::setup_aaa(Teuchos::ParameterList& params, const int eleGID)
{
  // loop map of associated potential summands
  for (auto& p : potsum_)
  {
    p->setup_aaa(params, eleGID);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::setup(int numgp, Input::LineDefinition* linedef)
{
  // Read anisotropy
  anisotropy_.set_number_of_gauss_points(numgp);
  anisotropy_.read_anisotropy_from_element(linedef);

  // Setup summands
  for (auto& p : potsum_)
  {
    p->setup(numgp, linedef);
  }
  summandProperties_.clear();
  ElastHyperProperties(potsum_, summandProperties_);

  if (summandProperties_.viscoGeneral)
  {
    FOUR_C_THROW(
        "Never use viscoelastic-materials in Elasthyper-Toolbox. Use Viscoelasthyper-Toolbox "
        "instead.");
  }
}

void Mat::ElastHyper::post_setup(Teuchos::ParameterList& params, const int eleGID)
{
  anisotropy_.read_anisotropy_from_parameter_list(params);

  // Forward post_setup call to all summands
  for (auto& p : potsum_)
  {
    p->post_setup(params);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::update()
{
  // loop map of associated potential summands
  for (auto& p : potsum_)
  {
    p->update();
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::get_fiber_vecs(std::vector<Core::LinAlg::Matrix<3, 1>>& fibervecs)
{
  if (summandProperties_.anisoprinc || summandProperties_.anisomod)
  {
    for (auto& p : potsum_)
    {
      p->get_fiber_vecs(fibervecs);
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::evaluate_fiber_vecs(const double newgamma,
    const Core::LinAlg::Matrix<3, 3>& locsys, const Core::LinAlg::Matrix<3, 3>& defgrd)
{
  if (summandProperties_.anisoprinc || summandProperties_.anisomod)
  {
    for (auto& p : potsum_)
    {
      p->set_fiber_vecs(newgamma, locsys, defgrd);
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::strain_energy(
    const Core::LinAlg::Matrix<6, 1>& glstrain, double& psi, const int gp, const int eleGID)
{
  static Core::LinAlg::Matrix<6, 1> C_strain(true);
  C_strain.clear();
  static Core::LinAlg::Matrix<3, 1> prinv(true);
  prinv.clear();
  static Core::LinAlg::Matrix<3, 1> modinv(true);
  modinv.clear();

  EvaluateRightCauchyGreenStrainLikeVoigt(glstrain, C_strain);
  Core::LinAlg::Voigt::Strains::invariants_principal(prinv, C_strain);
  invariants_modified(modinv, prinv);

  // loop map of associated potential summands
  for (const auto& p : potsum_)
  {
    p->add_strain_energy(psi, prinv, modinv, glstrain, gp, eleGID);
  }
}



/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::evaluate(const Core::LinAlg::Matrix<3, 3>* defgrd,
    const Core::LinAlg::Matrix<6, 1>* glstrain, Teuchos::ParameterList& params,
    Core::LinAlg::Matrix<6, 1>* stress, Core::LinAlg::Matrix<6, 6>* cmat, const int gp,
    const int eleGID)
{
  bool checkpolyconvexity = (params_ != nullptr and params_->polyconvex_ != 0);

  ElastHyperEvaluate(*defgrd, *glstrain, params, *stress, *cmat, gp, eleGID, potsum_,
      summandProperties_, checkpolyconvexity);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::evaluate_cauchy_derivs(const Core::LinAlg::Matrix<3, 1>& prinv, const int gp,
    int eleGID, Core::LinAlg::Matrix<3, 1>& dPI, Core::LinAlg::Matrix<6, 1>& ddPII,
    Core::LinAlg::Matrix<10, 1>& dddPIII, const double* temp)
{
  for (auto& i : potsum_)
  {
    if (summandProperties_.isoprinc)
    {
      i->add_derivatives_principal(dPI, ddPII, prinv, gp, eleGID);
      i->add_third_derivatives_principal_iso(dddPIII, prinv, gp, eleGID);
    }
    if (summandProperties_.isomod || summandProperties_.anisomod || summandProperties_.anisoprinc)
      FOUR_C_THROW("not implemented for this form of strain energy function");
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::evaluate_cauchy_n_dir_and_derivatives(
    const Core::LinAlg::Matrix<3, 3>& defgrd, const Core::LinAlg::Matrix<3, 1>& n,
    const Core::LinAlg::Matrix<3, 1>& dir, double& cauchy_n_dir,
    Core::LinAlg::Matrix<3, 1>* d_cauchyndir_dn, Core::LinAlg::Matrix<3, 1>* d_cauchyndir_ddir,
    Core::LinAlg::Matrix<9, 1>* d_cauchyndir_dF, Core::LinAlg::Matrix<9, 9>* d2_cauchyndir_dF2,
    Core::LinAlg::Matrix<9, 3>* d2_cauchyndir_dF_dn,
    Core::LinAlg::Matrix<9, 3>* d2_cauchyndir_dF_ddir, int gp, int eleGID,
    const double* concentration, const double* temp, double* d_cauchyndir_dT,
    Core::LinAlg::Matrix<9, 1>* d2_cauchyndir_dF_dT)
{
  cauchy_n_dir = 0.0;

  static Core::LinAlg::Matrix<3, 3> b(true);
  b.multiply_nt(1.0, defgrd, defgrd, 0.0);
  static Core::LinAlg::Matrix<3, 1> bdn(true);
  bdn.multiply(1.0, b, n, 0.0);
  static Core::LinAlg::Matrix<3, 1> bddir(true);
  bddir.multiply(1.0, b, dir, 0.0);
  const double bdnddir = bdn.dot(dir);

  static Core::LinAlg::Matrix<3, 3> ib(true);
  ib.invert(b);
  static Core::LinAlg::Matrix<3, 1> ibdn(true);
  ibdn.multiply(1.0, ib, n, 0.0);
  static Core::LinAlg::Matrix<3, 1> ibddir(true);
  ibddir.multiply(1.0, ib, dir, 0.0);
  const double ibdnddir = ibdn.dot(dir);
  const double nddir = n.dot(dir);

  static Core::LinAlg::Matrix<6, 1> bV_strain(true);
  Core::LinAlg::Voigt::Strains::matrix_to_vector(b, bV_strain);
  static Core::LinAlg::Matrix<3, 1> prinv(true);
  Core::LinAlg::Voigt::Strains::invariants_principal(prinv, bV_strain);

  static Core::LinAlg::Matrix<3, 1> dPI(true);
  static Core::LinAlg::Matrix<6, 1> ddPII(true);
  static Core::LinAlg::Matrix<10, 1> dddPIII(true);
  dPI.clear();
  ddPII.clear();
  dddPIII.clear();
  evaluate_cauchy_derivs(prinv, gp, eleGID, dPI, ddPII, dddPIII, temp);

  const double prefac = 2.0 / std::sqrt(prinv(2));

  cauchy_n_dir = prefac * (prinv(1) * dPI(1) * nddir + prinv(2) * dPI(2) * nddir +
                              dPI(0) * bdnddir - prinv(2) * dPI(1) * ibdnddir);

  if (d_cauchyndir_dn != nullptr)
  {
    d_cauchyndir_dn->update(prinv(1) * dPI(1) + prinv(2) * dPI(2), dir, 0.0);
    d_cauchyndir_dn->update(dPI(0), bddir, 1.0);
    d_cauchyndir_dn->update(-prinv(2) * dPI(1), ibddir, 1.0);
    d_cauchyndir_dn->scale(prefac);
  }

  if (d_cauchyndir_ddir != nullptr)
  {
    d_cauchyndir_ddir->update(prinv(1) * dPI(1) + prinv(2) * dPI(2), n, 0.0);
    d_cauchyndir_ddir->update(dPI(0), bdn, 1.0);
    d_cauchyndir_ddir->update(-prinv(2) * dPI(1), ibdn, 1.0);
    d_cauchyndir_ddir->scale(prefac);
  }

  // calculate stuff that is needed for evaluations of derivatives w.r.t. F
  static Core::LinAlg::Matrix<9, 1> FV(true);
  Core::LinAlg::Voigt::matrix_3x3_to_9x1(defgrd, FV);
  static Core::LinAlg::Matrix<3, 3> iF(true);
  iF.invert(defgrd);
  static Core::LinAlg::Matrix<3, 3> iFT(true);
  iFT.update_t(iF);
  static Core::LinAlg::Matrix<9, 1> iFTV(true);
  Core::LinAlg::Voigt::matrix_3x3_to_9x1(iFT, iFTV);

  // calculation of dI_i/dF (derivatives of invariants of b w.r.t. deformation gradient)
  static Core::LinAlg::Matrix<3, 3> bdF(true);
  bdF.multiply(1.0, b, defgrd, 0.0);
  static Core::LinAlg::Matrix<9, 1> bdFV(true);
  Core::LinAlg::Voigt::matrix_3x3_to_9x1(bdF, bdFV);
  static Core::LinAlg::Matrix<3, 3> ibdF(true);
  ibdF.multiply(1.0, ib, defgrd, 0.0);
  static Core::LinAlg::Matrix<9, 1> ibdFV(true);
  Core::LinAlg::Voigt::matrix_3x3_to_9x1(ibdF, ibdFV);
  static Core::LinAlg::Matrix<9, 1> d_I1_dF(true);
  d_I1_dF.update(2.0, FV, 0.0);
  static Core::LinAlg::Matrix<9, 1> d_I2_dF(true);
  d_I2_dF.update(prinv(0), FV, 0.0);
  d_I2_dF.update(-1.0, bdFV, 1.0);
  d_I2_dF.scale(2.0);
  static Core::LinAlg::Matrix<9, 1> d_I3_dF(true);
  d_I3_dF.update(2.0 * prinv(2), ibdFV, 0.0);

  // calculate d(b \cdot n \cdot t)/dF
  static Core::LinAlg::Matrix<3, 1> tempvec3x1(true);
  static Core::LinAlg::Matrix<1, 3> tempvec1x3(true);
  tempvec1x3.multiply_tn(1.0, dir, defgrd, 0.0);
  static Core::LinAlg::Matrix<3, 3> d_bdnddir_dF(true);
  d_bdnddir_dF.multiply_nn(1.0, n, tempvec1x3, 0.0);
  tempvec1x3.multiply_tn(1.0, n, defgrd, 0.0);
  d_bdnddir_dF.multiply_nn(1.0, dir, tempvec1x3, 1.0);
  static Core::LinAlg::Matrix<9, 1> d_bdnddir_dFV(true);
  Core::LinAlg::Voigt::matrix_3x3_to_9x1(d_bdnddir_dF, d_bdnddir_dFV);

  // calculate d(b^{-1} \cdot n \cdot t)/dF
  static Core::LinAlg::Matrix<1, 3> dirdibdF(true);
  static Core::LinAlg::Matrix<1, 3> ndibdF(true);
  dirdibdF.multiply_tn(1.0, dir, ibdF, 0.0);
  static Core::LinAlg::Matrix<3, 3> d_ibdnddir_dF(true);
  d_ibdnddir_dF.multiply_nn(1.0, ibdn, dirdibdF, 0.0);
  ndibdF.multiply_tn(1.0, n, ibdF, 0.0);
  d_ibdnddir_dF.multiply_nn(1.0, ibddir, ndibdF, 1.0);
  d_ibdnddir_dF.scale(-1.0);
  static Core::LinAlg::Matrix<9, 1> d_ibdnddir_dFV(true);
  Core::LinAlg::Voigt::matrix_3x3_to_9x1(d_ibdnddir_dF, d_ibdnddir_dFV);

  if (temp != nullptr)
    evaluate_cauchy_temp_deriv(prinv, nddir, bdnddir, ibdnddir, temp, d_cauchyndir_dT, iFTV,
        d_bdnddir_dFV, d_ibdnddir_dFV, d_I1_dF, d_I2_dF, d_I3_dF, d2_cauchyndir_dF_dT);

  if (d_cauchyndir_dF != nullptr)
  {
    // next 3 updates add partial derivative of (\sigma * n * v) w.r.t. F for constant invariants
    // 1. part is term arising from d(J^{-1})/dF
    d_cauchyndir_dF->update(-prefac * (prinv(1) * dPI(1) * nddir + prinv(2) * dPI(2) * nddir +
                                          dPI(0) * bdnddir - prinv(2) * dPI(1) * ibdnddir),
        iFTV, 0.0);  // DsntDF is cleared here
    // 2. part is term arising from d(b * n * v)/dF
    d_cauchyndir_dF->update(prefac * dPI(0), d_bdnddir_dFV, 1.0);
    // 3. part is term arising from d(b_el^{-1} * n * v)/dF
    d_cauchyndir_dF->update(-prefac * prinv(2) * dPI(1), d_ibdnddir_dFV, 1.0);
    // add d(sigma * n * v)/dI1 \otimes dI1/dF
    d_cauchyndir_dF->update(prefac * (prinv(1) * ddPII(5) * nddir + prinv(2) * ddPII(4) * nddir +
                                         ddPII(0) * bdnddir - prinv(2) * ddPII(5) * ibdnddir),
        d_I1_dF, 1.0);
    // add d(sigma * n * v)/dI2 \otimes dI2/dF
    d_cauchyndir_dF->update(
        prefac * (dPI(1) * nddir + prinv(1) * ddPII(1) * nddir + prinv(2) * ddPII(3) * nddir +
                     ddPII(5) * bdnddir - prinv(2) * ddPII(1) * ibdnddir),
        d_I2_dF, 1.0);
    // add d(sigma * n * v)/dI3 \otimes dI3/dF
    d_cauchyndir_dF->update(
        prefac * (prinv(1) * ddPII(3) * nddir + dPI(2) * nddir + prinv(2) * ddPII(2) * nddir +
                     ddPII(4) * bdnddir - dPI(1) * ibdnddir - prinv(2) * ddPII(3) * ibdnddir),
        d_I3_dF, 1.0);
  }

  if (d2_cauchyndir_dF_dn != nullptr)
  {
    // next three blocks add d/dn(d(\sigma * n * v)/dF) for constant invariants
    // first part is term arising from d/dn(dJ^{-1}/dF)
    tempvec3x1.update(prinv(1) * dPI(1) + prinv(2) * dPI(2), dir, 0.0);
    tempvec3x1.update(dPI(0), bddir, 1.0);
    tempvec3x1.update(-prinv(2) * dPI(1), ibddir, 1.0);
    d2_cauchyndir_dF_dn->multiply_nt(-prefac, iFTV, tempvec3x1, 0.0);

    // second part is term arising from d/dn(d(b * n * v)/dF
    const double fac = prefac * dPI(0);
    tempvec1x3.multiply_tn(1.0, dir, defgrd, 0.0);
    for (int k = 0; k < 3; ++k)
    {
      for (int l = 0; l < 3; ++l)
      {
        for (int z = 0; z < 3; ++z)
          (*d2_cauchyndir_dF_dn)(
              Core::LinAlg::Voigt::IndexMappings::non_symmetric_tensor_to_voigt9_index(k, l), z) +=
              fac * (dir(k, 0) * defgrd(z, l) + static_cast<double>(k == z) * tempvec1x3(0, l));
      }
    }

    // third part is term arising from d/dn(d(b^{-1} * n * t)/dF
    const double fac2 = prefac * prinv(2) * dPI(1);
    for (int k = 0; k < 3; ++k)
    {
      for (int l = 0; l < 3; ++l)
      {
        for (int z = 0; z < 3; ++z)
          (*d2_cauchyndir_dF_dn)(
              Core::LinAlg::Voigt::IndexMappings::non_symmetric_tensor_to_voigt9_index(k, l), z) +=
              fac2 * (ibddir(k, 0) * ibdF(z, l) + ib(z, k) * dirdibdF(0, l));
      }
    }

    // add parts originating from d/dn(d(sigma * n * t)/dI1 \otimes dI1/dF)
    tempvec3x1.update(prinv(1) * ddPII(5) + prinv(2) * ddPII(4), dir, 0.0);
    tempvec3x1.update(ddPII(0), bddir, 1.0);
    tempvec3x1.update(-prinv(2) * ddPII(5), ibddir, 1.0);
    d2_cauchyndir_dF_dn->multiply_nt(prefac, d_I1_dF, tempvec3x1, 1.0);

    // add parts originating from d/dn(d(sigma * n * t)/dI2 \otimes dI2/dF)
    tempvec3x1.update(dPI(1) + prinv(1) * ddPII(1) + prinv(2) * ddPII(3), dir, 0.0);
    tempvec3x1.update(ddPII(5), bddir, 1.0);
    tempvec3x1.update(-prinv(2) * ddPII(1), ibddir, 1.0);
    d2_cauchyndir_dF_dn->multiply_nt(prefac, d_I2_dF, tempvec3x1, 1.0);

    // add parts originating from d/dn(d(sigma * n * t)/dI3 \otimes dI3/dF)
    tempvec3x1.update(prinv(1) * ddPII(3) + dPI(2) + prinv(2) * ddPII(2), dir, 0.0);
    tempvec3x1.update(ddPII(4), bddir, 1.0);
    tempvec3x1.update(-dPI(1) - prinv(2) * ddPII(3), ibddir, 1.0);
    d2_cauchyndir_dF_dn->multiply_nt(prefac, d_I3_dF, tempvec3x1, 1.0);
  }

  if (d2_cauchyndir_dF_ddir != nullptr)
  {
    // next three blocks add d/dt(d(\sigma * n * v)/dF) for constant invariants
    // first part is term arising from d/dt(dJ^{-1}/dF)
    tempvec3x1.update(prinv(1) * dPI(1) + prinv(2) * dPI(2), n, 0.0);
    tempvec3x1.update(dPI(0), bdn, 1.0);
    tempvec3x1.update(-prinv(2) * dPI(1), ibdn, 1.0);
    d2_cauchyndir_dF_ddir->multiply_nt(-prefac, iFTV, tempvec3x1, 0.0);

    // second part is term arising from d/dt(d(b * n * v)/dF
    const double fac = prefac * dPI(0);
    tempvec1x3.multiply_tn(1.0, n, defgrd, 0.0);
    for (int k = 0; k < 3; ++k)
    {
      for (int l = 0; l < 3; ++l)
      {
        for (int z = 0; z < 3; ++z)
          (*d2_cauchyndir_dF_ddir)(
              Core::LinAlg::Voigt::IndexMappings::non_symmetric_tensor_to_voigt9_index(k, l), z) +=
              fac * (n(k, 0) * defgrd(z, l) + static_cast<double>(k == z) * tempvec1x3(0, l));
      }
    }

    // third part is term arising from d/dn(d(b^{-1} * n * v)/dF
    const double fac2 = prefac * prinv(2) * dPI(1);
    for (int k = 0; k < 3; ++k)
    {
      for (int l = 0; l < 3; ++l)
      {
        for (int z = 0; z < 3; ++z)
          (*d2_cauchyndir_dF_ddir)(
              Core::LinAlg::Voigt::IndexMappings::non_symmetric_tensor_to_voigt9_index(k, l), z) +=
              fac2 * (ibdn(k, 0) * ibdF(z, l) + ib(z, k) * ndibdF(0, l));
      }
    }

    // add parts originating from d/dt(d(sigma * n * v)/dI1 \otimes dI1/dF)
    tempvec3x1.update(prinv(1) * ddPII(5) + prinv(2) * ddPII(4), n, 0.0);
    tempvec3x1.update(ddPII(0), bdn, 1.0);
    tempvec3x1.update(-prinv(2) * ddPII(5), ibdn, 1.0);
    d2_cauchyndir_dF_ddir->multiply_nt(prefac, d_I1_dF, tempvec3x1, 1.0);

    // add parts originating from d/dt(d(sigma * n * v)/dI2 \otimes dI2/dF)
    tempvec3x1.update(dPI(1) + prinv(1) * ddPII(1) + prinv(2) * ddPII(3), n, 0.0);
    tempvec3x1.update(ddPII(5), bdn, 1.0);
    tempvec3x1.update(-prinv(2) * ddPII(1), ibdn, 1.0);
    d2_cauchyndir_dF_ddir->multiply_nt(prefac, d_I2_dF, tempvec3x1, 1.0);

    // add parts originating from d/dt(d(sigma * n * v)/dI3 \otimes dI3/dF)
    tempvec3x1.update(prinv(1) * ddPII(3) + dPI(2) + prinv(2) * ddPII(2), n, 0.0);
    tempvec3x1.update(ddPII(4), bdn, 1.0);
    tempvec3x1.update(-dPI(1) - prinv(2) * ddPII(3), ibdn, 1.0);
    d2_cauchyndir_dF_ddir->multiply_nt(prefac, d_I3_dF, tempvec3x1, 1.0);
  }

  if (d2_cauchyndir_dF2 != nullptr)
  {
    // define and fill all tensors that can not be calculated using multiply operations first
    static Core::LinAlg::Matrix<9, 9> d_iFT_dF(true);
    static Core::LinAlg::Matrix<9, 9> d2_bdnddir_dF2(true);
    static Core::LinAlg::Matrix<9, 9> d2_ibdnddir_dF2(true);
    static Core::LinAlg::Matrix<9, 9> d2_I1_dF2(true);
    static Core::LinAlg::Matrix<9, 9> d2_I2_dF2(true);
    static Core::LinAlg::Matrix<9, 9> d2_I3_dF2(true);
    d_iFT_dF.clear();
    d2_bdnddir_dF2.clear();
    d2_ibdnddir_dF2.clear();
    d2_I1_dF2.clear();
    d2_I2_dF2.clear();
    d2_I3_dF2.clear();

    static Core::LinAlg::Matrix<3, 3> C(true);
    C.multiply_tn(1.0, defgrd, defgrd, 0.0);

    using map = Core::LinAlg::Voigt::IndexMappings;

    for (int k = 0; k < 3; ++k)
    {
      for (int l = 0; l < 3; ++l)
      {
        for (int m = 0; m < 3; ++m)
        {
          for (int a = 0; a < 3; ++a)
          {
            d_iFT_dF(map::non_symmetric_tensor_to_voigt9_index(k, l),
                map::non_symmetric_tensor_to_voigt9_index(m, a)) = -iF(l, m) * iF(a, k);
            d2_bdnddir_dF2(map::non_symmetric_tensor_to_voigt9_index(k, l),
                map::non_symmetric_tensor_to_voigt9_index(m, a)) =
                (dir(k, 0) * n(m, 0) + dir(m, 0) * n(k, 0)) * static_cast<double>(l == a);
            d2_ibdnddir_dF2(map::non_symmetric_tensor_to_voigt9_index(k, l),
                map::non_symmetric_tensor_to_voigt9_index(m, a)) =
                ibdF(k, a) * (ibddir(m, 0) * ndibdF(0, l) + ibdn(m, 0) * dirdibdF(0, l)) +
                ib(m, k) * (dirdibdF(0, a) * ndibdF(0, l) + dirdibdF(0, l) * ndibdF(0, a)) +
                ibdF(m, l) * (ibddir(k, 0) * ndibdF(0, a) + dirdibdF(0, a) * ibdn(k, 0));
            d2_I1_dF2(map::non_symmetric_tensor_to_voigt9_index(k, l),
                map::non_symmetric_tensor_to_voigt9_index(m, a)) =
                2.0 * static_cast<double>(k == m) * static_cast<double>(l == a);
            d2_I2_dF2(map::non_symmetric_tensor_to_voigt9_index(k, l),
                map::non_symmetric_tensor_to_voigt9_index(m, a)) =
                2.0 *
                (prinv(0) * static_cast<double>(k == m) * static_cast<double>(l == a) +
                    2.0 * defgrd(m, a) * defgrd(k, l) - static_cast<double>(k == m) * C(a, l) -
                    defgrd(k, a) * defgrd(m, l) - b(k, m) * static_cast<double>(l == a));
            d2_I3_dF2(map::non_symmetric_tensor_to_voigt9_index(k, l),
                map::non_symmetric_tensor_to_voigt9_index(m, a)) =
                2.0 * prinv(2) * (2.0 * ibdF(m, a) * ibdF(k, l) - ibdF(m, l) * ibdF(k, a));
          }
        }
      }
    }

    // terms below add contributions originating from d(1st term of DsntDF)/dF
    d2_cauchyndir_dF2->multiply_nt(prefac * (prinv(1) * dPI(1) * nddir + prinv(2) * dPI(2) * nddir +
                                                dPI(0) * bdnddir - prinv(2) * dPI(1) * ibdnddir),
        iFTV, iFTV, 0.0);  // D2sntDF2 is cleared here
    d2_cauchyndir_dF2->update(-prefac * (prinv(1) * dPI(1) * nddir + prinv(2) * dPI(2) * nddir +
                                            dPI(0) * bdnddir - prinv(2) * dPI(1) * ibdnddir),
        d_iFT_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(-prefac * dPI(0), iFTV, d_bdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * prinv(2) * dPI(1), iFTV, d_ibdnddir_dFV, 1.0);

    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (prinv(1) * ddPII(5) * nddir + prinv(2) * ddPII(4) * nddir + ddPII(0) * bdnddir -
                      prinv(2) * ddPII(5) * ibdnddir),
        iFTV, d_I1_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (dPI(1) * nddir + prinv(1) * ddPII(1) * nddir + prinv(2) * ddPII(3) * nddir +
                      ddPII(5) * bdnddir - prinv(2) * ddPII(1) * ibdnddir),
        iFTV, d_I2_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (prinv(1) * ddPII(3) * nddir + dPI(2) * nddir + prinv(2) * ddPII(2) * nddir +
                      ddPII(4) * bdnddir - dPI(1) * ibdnddir - prinv(2) * ddPII(3) * ibdnddir),
        iFTV, d_I3_dF, 1.0);

    // terms below add contributions originating from d(2nd term of DsntDF)/dF
    d2_cauchyndir_dF2->multiply_nt(-prefac * dPI(0), d_bdnddir_dFV, iFTV, 1.0);
    d2_cauchyndir_dF2->update(prefac * dPI(0), d2_bdnddir_dF2, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * ddPII(0), d_bdnddir_dFV, d_I1_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * ddPII(5), d_bdnddir_dFV, d_I2_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * ddPII(4), d_bdnddir_dFV, d_I3_dF, 1.0);

    // terms below add contributions originating from d(3rd term of DsntDF)/dF
    d2_cauchyndir_dF2->multiply_nt(prefac * prinv(2) * dPI(1), d_ibdnddir_dFV, iFTV, 1.0);
    d2_cauchyndir_dF2->update(-prefac * prinv(2) * dPI(1), d2_ibdnddir_dF2, 1.0);
    d2_cauchyndir_dF2->multiply_nt(-prefac * prinv(2) * ddPII(5), d_ibdnddir_dFV, d_I1_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(-prefac * prinv(2) * ddPII(1), d_ibdnddir_dFV, d_I2_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (dPI(1) + prinv(2) * ddPII(3)), d_ibdnddir_dFV, d_I3_dF, 1.0);

    // terms below add contributions originating from d(4th term of DsntDF)/dF
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (prinv(1) * ddPII(5) * nddir + prinv(2) * ddPII(4) * nddir + ddPII(0) * bdnddir -
                      prinv(2) * ddPII(5) * ibdnddir),
        d_I1_dF, iFTV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * ddPII(0), d_I1_dF, d_bdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(-prefac * prinv(2) * ddPII(5), d_I1_dF, d_ibdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->update(prefac * (prinv(1) * ddPII(5) * nddir + prinv(2) * ddPII(4) * nddir +
                                           ddPII(0) * bdnddir - prinv(2) * ddPII(5) * ibdnddir),
        d2_I1_dF2, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (prinv(1) * dddPIII(5) * nddir + prinv(2) * dddPIII(6) * nddir +
                     dddPIII(0) * bdnddir - prinv(2) * dddPIII(5) * ibdnddir),
        d_I1_dF, d_I1_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (ddPII(5) * nddir + prinv(1) * dddPIII(3) * nddir + prinv(2) * dddPIII(9) * nddir +
                     dddPIII(5) * bdnddir - prinv(2) * dddPIII(3) * ibdnddir),
        d_I1_dF, d_I2_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (prinv(1) * dddPIII(9) * nddir + ddPII(4) * nddir + prinv(2) * dddPIII(4) * nddir +
                     dddPIII(6) * bdnddir - ddPII(5) * ibdnddir - prinv(2) * dddPIII(9) * ibdnddir),
        d_I1_dF, d_I3_dF, 1.0);

    // terms below add contributions originating from d(5th term of DsntDF)/dF
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (dPI(1) * nddir + prinv(1) * ddPII(1) * nddir + prinv(2) * ddPII(3) * nddir +
                      ddPII(5) * bdnddir - prinv(2) * ddPII(1) * ibdnddir),
        d_I2_dF, iFTV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * ddPII(5), d_I2_dF, d_bdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(-prefac * prinv(2) * ddPII(1), d_I2_dF, d_ibdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->update(
        prefac * (dPI(1) * nddir + prinv(1) * ddPII(1) * nddir + prinv(2) * ddPII(3) * nddir +
                     ddPII(5) * bdnddir - prinv(2) * ddPII(1) * ibdnddir),
        d2_I2_dF2, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (ddPII(5) * nddir + prinv(1) * dddPIII(3) * nddir + prinv(2) * dddPIII(9) * nddir +
                     dddPIII(5) * bdnddir - prinv(2) * dddPIII(3) * ibdnddir),
        d_I2_dF, d_I1_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (2.0 * ddPII(1) * nddir + prinv(1) * dddPIII(1) * nddir +
                     prinv(2) * dddPIII(7) * nddir + dddPIII(3) * bdnddir -
                     prinv(2) * dddPIII(1) * ibdnddir),
        d_I2_dF, d_I2_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (2.0 * ddPII(3) * nddir + prinv(1) * dddPIII(7) * nddir +
                     prinv(2) * dddPIII(8) * nddir + dddPIII(9) * bdnddir - ddPII(1) * ibdnddir -
                     prinv(2) * dddPIII(7) * ibdnddir),
        d_I2_dF, d_I3_dF, 1.0);

    // terms below add contributions originating from d(6th term of DsntDF)/dF
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (prinv(1) * ddPII(3) * nddir + dPI(2) * nddir + prinv(2) * ddPII(2) * nddir +
                      ddPII(4) * bdnddir - dPI(1) * ibdnddir - prinv(2) * ddPII(3) * ibdnddir),
        d_I3_dF, iFTV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(prefac * ddPII(4), d_I3_dF, d_bdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        -prefac * (dPI(1) + prinv(2) * ddPII(3)), d_I3_dF, d_ibdnddir_dFV, 1.0);
    d2_cauchyndir_dF2->update(
        prefac * (prinv(1) * ddPII(3) * nddir + dPI(2) * nddir + prinv(2) * ddPII(2) * nddir +
                     ddPII(4) * bdnddir - dPI(1) * ibdnddir - prinv(2) * ddPII(3) * ibdnddir),
        d2_I3_dF2, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (prinv(1) * dddPIII(9) * nddir + ddPII(4) * nddir + prinv(2) * dddPIII(4) * nddir +
                     dddPIII(6) * bdnddir - ddPII(5) * ibdnddir - prinv(2) * dddPIII(9) * ibdnddir),
        d_I3_dF, d_I1_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (2.0 * ddPII(3) * nddir + prinv(1) * dddPIII(7) * nddir +
                     prinv(2) * dddPIII(8) * nddir + dddPIII(9) * bdnddir - ddPII(1) * ibdnddir -
                     prinv(2) * dddPIII(7) * ibdnddir),
        d_I3_dF, d_I2_dF, 1.0);
    d2_cauchyndir_dF2->multiply_nt(
        prefac * (prinv(1) * dddPIII(8) * nddir + 2.0 * ddPII(2) * nddir +
                     prinv(2) * dddPIII(2) * nddir + dddPIII(4) * bdnddir -
                     2.0 * ddPII(3) * ibdnddir - prinv(2) * dddPIII(8) * ibdnddir),
        d_I3_dF, d_I3_dF, 1.0);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::ElastHyper::vis_names(std::map<std::string, int>& names)
{
  if (anisotropic_principal() or anisotropic_modified())
  {
    std::vector<Core::LinAlg::Matrix<3, 1>> fibervecs;
    get_fiber_vecs(fibervecs);
    int vissize = fibervecs.size();
    std::string fiber;
    for (int i = 0; i < vissize; i++)
    {
      std::ostringstream s;
      s << "Fiber" << i + 1;
      fiber = s.str();
      names[fiber] = 3;  // 3-dim vector
    }
  }
  // do visualization for isotropic materials as well
  // loop map of associated potential summands
  for (auto& p : potsum_)
  {
    p->vis_names(names);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool Mat::ElastHyper::vis_data(
    const std::string& name, std::vector<double>& data, int numgp, int eleID)
{
  //
  int return_val = 0;
  if (anisotropic_principal() or anisotropic_modified())
  {
    std::vector<Core::LinAlg::Matrix<3, 1>> fibervecs;
    get_fiber_vecs(fibervecs);
    int vissize = fibervecs.size();
    for (int i = 0; i < vissize; i++)
    {
      std::ostringstream s;
      s << "Fiber" << i + 1;
      std::string fiber;
      fiber = s.str();
      if (name == fiber)
      {
        if ((int)data.size() != 3) FOUR_C_THROW("size mismatch");
        data[0] = fibervecs.at(i)(0);
        data[1] = fibervecs.at(i)(1);
        data[2] = fibervecs.at(i)(2);
      }
    }
    // return true;
    return_val = 1;
  }
  // loop map of associated potential summands
  for (auto& p : potsum_)
  {
    return_val += static_cast<int>(p->vis_data(name, data, numgp, eleID));
  }
  return (bool)return_val;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Mat::Elastic::Summand> Mat::ElastHyper::get_pot_summand_ptr(
    const Core::Materials::MaterialType& materialtype) const
{
  for (const auto& p : potsum_)
  {
    if (p->material_type() == materialtype) return p;
  }
  return Teuchos::null;
}

FOUR_C_NAMESPACE_CLOSE
