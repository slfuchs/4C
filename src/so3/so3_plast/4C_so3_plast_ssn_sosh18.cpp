/*----------------------------------------------------------------------*/
/*! \file
\brief element
\level 2
*/
/*----------------------------------------------------------------------*/

#include "4C_so3_plast_ssn_sosh18.hpp"

#include "4C_global_data.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_linalg_serialdensevector.hpp"
#include "4C_mat_plasticelasthyper.hpp"
#include "4C_so3_hex18.hpp"
#include "4C_so3_plast_ssn_eletypes.hpp"
#include "4C_so3_sh18.hpp"
#include "4C_so3_utils.hpp"
#include "4C_structure_new_elements_paramsinterface.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 | build an instance of plast type                         seitz 11/14 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoSh18PlastType DRT::ELEMENTS::SoSh18PlastType::instance_;

DRT::ELEMENTS::SoSh18PlastType& DRT::ELEMENTS::SoSh18PlastType::Instance() { return instance_; }

/*----------------------------------------------------------------------*
| create the new element type (public)                     seitz 11/14 |
| is called in ElementRegisterType                                     |
*----------------------------------------------------------------------*/
CORE::COMM::ParObject* DRT::ELEMENTS::SoSh18PlastType::Create(const std::vector<char>& data)
{
  auto* object = new DRT::ELEMENTS::SoSh18Plast(-1, -1);
  object->Unpack(data);
  return object;
}

/*----------------------------------------------------------------------*
| create the new element type (public)                     seitz 11/14 |
| is called from ParObjectFactory                                      |
*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::SoSh18PlastType::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == GetElementTypeString())
  {
    Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::SoSh18Plast(id, owner));
    return ele;
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
| create the new element type (public)                     seitz 11/14 |
| virtual method of ElementType                                        |
*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::SoSh18PlastType::Create(const int id, const int owner)
{
  Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::SoSh18Plast(id, owner));
  return ele;
}


/*----------------------------------------------------------------------*
| initialise the element (public)                          seitz 11/14 |
*----------------------------------------------------------------------*/
int DRT::ELEMENTS::SoSh18PlastType::Initialize(DRT::Discretization& dis)
{
  return SoSh18Type::Initialize(dis);
}

/*----------------------------------------------------------------------*
 | setup the element definition (public)                    seitz 11/14 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoSh18PlastType::SetupElementDefinition(
    std::map<std::string, std::map<std::string, INPUT::LineDefinition>>& definitions)
{
  std::map<std::string, std::map<std::string, INPUT::LineDefinition>> definitions_sh18;
  SoSh18Type::SetupElementDefinition(definitions_sh18);

  std::map<std::string, INPUT::LineDefinition>& defs_sh18 = definitions_sh18["SOLIDSH18"];

  std::map<std::string, INPUT::LineDefinition>& defs = definitions[GetElementTypeString()];

  defs["HEX18"] = defs_sh18["HEX18"];
}

/*----------------------------------------------------------------------*
 | ctor (public)                                            seitz 11/14 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoSh18Plast::SoSh18Plast(int id, int owner)
    : SoBase(id, owner),
      DRT::ELEMENTS::So3Plast<CORE::FE::CellType::hex18>(id, owner),
      DRT::ELEMENTS::SoHex18(id, owner),
      DRT::ELEMENTS::SoSh18(id, owner)
{
  Teuchos::RCP<const Teuchos::ParameterList> params =
      GLOBAL::Problem::Instance()->getParameterList();
  if (params != Teuchos::null)
  {
    DRT::ELEMENTS::UTILS::ThrowErrorFDMaterialTangent(
        GLOBAL::Problem::Instance()->StructuralDynamicParams(), GetElementTypeString());
  }

  return;
}

/*----------------------------------------------------------------------*
 | copy-ctor (public)                                       seitz 11/14 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoSh18Plast::SoSh18Plast(const DRT::ELEMENTS::SoSh18Plast& old)
    : SoBase(old),
      DRT::ELEMENTS::So3Plast<CORE::FE::CellType::hex18>(old),
      DRT::ELEMENTS::SoHex18(old),
      DRT::ELEMENTS::SoSh18(old)
{
  return;
}

/*----------------------------------------------------------------------*
 | deep copy this instance of Solid3 and return pointer to              |
 | it (public)                                              seitz 11/14 |
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::SoSh18Plast::Clone() const
{
  auto* newelement = new DRT::ELEMENTS::SoSh18Plast(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 | pack data (public)                                       seitz 11/14 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoSh18Plast::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // add base class So3Plast Element
  DRT::ELEMENTS::So3Plast<CORE::FE::CellType::hex18>::Pack(data);

  // add base class So3_sh18
  DRT::ELEMENTS::SoSh18::Pack(data);

  return;
}

/*----------------------------------------------------------------------*
 | unpack data (public)                                     seitz 11/14 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoSh18Plast::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class So_hex8 Element
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  DRT::ELEMENTS::So3Plast<CORE::FE::CellType::hex18>::Unpack(basedata);
  ExtractfromPack(position, data, basedata);
  DRT::ELEMENTS::SoSh18::Unpack(basedata);

  SyncEAS();

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", (int)data.size(), position);
  return;
}

void DRT::ELEMENTS::SoSh18Plast::Print(std::ostream& os) const
{
  os << "So_sh18Plast ";
  Element::Print(os);
  std::cout << std::endl;
  return;
}

/*----------------------------------------------------------------------*
 | read this element, get the material (public)             seitz 11/14 |
 *----------------------------------------------------------------------*/
bool DRT::ELEMENTS::SoSh18Plast::ReadElement(
    const std::string& eletype, const std::string& distype, INPUT::LineDefinition* linedef)
{
  bool read =
      (DRT::ELEMENTS::So3Plast<CORE::FE::CellType::hex18>::ReadElement(eletype, distype, linedef) &&
          DRT::ELEMENTS::SoSh18::ReadElement(eletype, distype, linedef));

  // sync the EAS info
  SyncEAS();


  return read;
}

/*----------------------------------------------------------------------*
 | read this element, get the material (public)             seitz 11/14 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoSh18Plast::SyncEAS()
{
  if (eas_ == true)
  {
    eastype_ = soh18p_eassosh18;
    neas_ = num_eas;
    So3Plast<CORE::FE::CellType::hex18>::KaaInv_ = Teuchos::rcp(new CORE::LINALG::SerialDenseMatrix(
        Teuchos::View, SoSh18::KaaInv_.A(), num_eas, num_eas, num_eas));
    So3Plast<CORE::FE::CellType::hex18>::Kad_ = Teuchos::rcp(new CORE::LINALG::SerialDenseMatrix(
        Teuchos::View, SoSh18::Kad_.A(), num_eas, num_eas, numdofperelement_));
    So3Plast<CORE::FE::CellType::hex18>::feas_ = Teuchos::rcp(
        new CORE::LINALG::SerialDenseVector(Teuchos::View, SoSh18::feas_.A(), num_eas));
    So3Plast<CORE::FE::CellType::hex18>::alpha_eas_ = Teuchos::rcp(
        new CORE::LINALG::SerialDenseVector(Teuchos::View, SoSh18::alpha_eas_.A(), num_eas));
    So3Plast<CORE::FE::CellType::hex18>::alpha_eas_last_timestep_ =
        Teuchos::rcp(new CORE::LINALG::SerialDenseVector(
            Teuchos::View, SoSh18::alpha_eas_last_timestep_.A(), num_eas));
    So3Plast<CORE::FE::CellType::hex18>::alpha_eas_delta_over_last_timestep_ =
        Teuchos::rcp(new CORE::LINALG::SerialDenseVector(
            Teuchos::View, SoSh18::alpha_eas_delta_over_last_timestep_.A(), num_eas));
    So3Plast<CORE::FE::CellType::hex18>::alpha_eas_inc_ = Teuchos::rcp(
        new CORE::LINALG::SerialDenseVector(Teuchos::View, SoSh18::alpha_eas_inc_.A(), num_eas));
    Kba_ = Teuchos::rcp(new std::vector<CORE::LINALG::SerialDenseMatrix>(
        numgpt_, CORE::LINALG::SerialDenseMatrix(plspintype_, num_eas, true)));
  }
  else
  {
    eastype_ = soh8p_easnone;
    neas_ = 0;
    So3Plast<CORE::FE::CellType::hex18>::KaaInv_ = Teuchos::null;
    So3Plast<CORE::FE::CellType::hex18>::Kad_ = Teuchos::null;
    So3Plast<CORE::FE::CellType::hex18>::feas_ = Teuchos::null;
    So3Plast<CORE::FE::CellType::hex18>::alpha_eas_ = Teuchos::null;
    Kba_ = Teuchos::null;
  }
}



/*----------------------------------------------------------------------*
 |                                                          seitz 05/14 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoSh18Plast::nln_stiffmass(std::vector<double>& disp,  // current displacements
    std::vector<double>& vel,                                              // current velocities
    std::vector<double>& temp,                                             // current temperatures
    CORE::LINALG::Matrix<numdofperelement_, numdofperelement_>*
        stiffmatrix,  // element stiffness matrix
    CORE::LINALG::Matrix<numdofperelement_, numdofperelement_>* massmatrix,  // element mass matrix
    CORE::LINALG::Matrix<numdofperelement_, 1>* force,      // element internal force vector
    CORE::LINALG::Matrix<numgpt_post, numstr_>* elestress,  // stresses at GP
    CORE::LINALG::Matrix<numgpt_post, numstr_>* elestrain,  // strains at GP
    Teuchos::ParameterList& params,                         // algorithmic parameters e.g. time
    const INPAR::STR::StressType iostress,                  // stress output option
    const INPAR::STR::StrainType iostrain                   // strain output option
)
{
  InvalidEleData();

  // do the evaluation of tsi terms
  const bool eval_tsi = (!temp.empty());
  if (tsi_) FOUR_C_THROW("no TSI for sosh18Plast (yet)");
  const double gp_temp = -1.e12;

  // update element geometry
  CORE::LINALG::Matrix<nen_, nsd_> xrefe;  // reference coord. of element
  CORE::LINALG::Matrix<nen_, nsd_> xcurr;  // current  coord. of element

  DRT::Node** nodes = Nodes();
  for (int i = 0; i < nen_; ++i)
  {
    const auto& x = nodes[i]->X();
    xrefe(i, 0) = x[0];
    xrefe(i, 1) = x[1];
    xrefe(i, 2) = x[2];

    xcurr(i, 0) = xrefe(i, 0) + disp[i * numdofpernode_ + 0];
    xcurr(i, 1) = xrefe(i, 1) + disp[i * numdofpernode_ + 1];
    xcurr(i, 2) = xrefe(i, 2) + disp[i * numdofpernode_ + 2];
  }

  // get plastic hyperelastic material
  MAT::PlasticElastHyper* plmat = nullptr;
  if (Material()->MaterialType() == CORE::Materials::m_plelasthyper)
    plmat = dynamic_cast<MAT::PlasticElastHyper*>(Material().get());

  // get time integration data
  double theta = StrParamsInterface().GetTimIntFactorDisp();
  double dt = StrParamsInterface().GetDeltaTime();
  if (eval_tsi && (stiffmatrix != nullptr || force != nullptr))
    if (theta == 0 || dt == 0)
      FOUR_C_THROW("time integration parameters not provided in element for TSI problem");


  // EAS stuff
  std::vector<CORE::LINALG::Matrix<6, num_eas>> M_gp(num_eas);
  CORE::LINALG::Matrix<3, 1> G3_0_contra;
  CORE::LINALG::Matrix<6, num_eas> M;
  CORE::LINALG::SerialDenseMatrix M_ep(Teuchos::View, M.A(), 6, 6, num_eas);
  CORE::LINALG::SerialDenseMatrix Kda(numdofperelement_, num_eas);

  // prepare EAS***************************************
  if (eas_)
  {
    SoSh18::EasSetup(M_gp, G3_0_contra, xrefe);
    SoSh18::feas_.Clear();
    SoSh18::KaaInv_.Clear();
    SoSh18::Kad_.Clear();
  }
  // prepare EAS***************************************

  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  for (int gp = 0; gp < NUMGPT_SOH18; ++gp)
  {
    InvalidGpData();

    // in-plane shape functions and derivatives
    CORE::LINALG::Matrix<9, 1> shapefunct_q9;
    CORE::FE::shape_function<CORE::FE::CellType::quad9>(SoSh18::xsi_[gp], shapefunct_q9);
    CORE::LINALG::Matrix<2, 9> deriv_q9;
    CORE::FE::shape_function_deriv1<CORE::FE::CellType::quad9>(SoSh18::xsi_[gp], deriv_q9);

    /* get the inverse of the Jacobian matrix which looks like:
    **         [ x_,r  y_,r  z_,r ]
    **     J = [ x_,s  y_,s  z_,s ]
    **         [ x_,t  y_,t  z_,t ]
    */
    // compute the Jacobian shell-style (G^T)
    CORE::LINALG::Matrix<NUMDIM_SOH18, NUMDIM_SOH18> jac;
    for (int dim = 0; dim < 3; ++dim)
      for (int k = 0; k < 9; ++k)
      {
        jac(0, dim) +=
            .5 * deriv_q9(0, k) * (xrefe(k + 9, dim) + xrefe(k, dim)) +
            .5 * SoSh18::xsi_[gp](2) * deriv_q9(0, k) * (xrefe(k + 9, dim) - xrefe(k, dim));

        jac(1, dim) +=
            .5 * deriv_q9(1, k) * (xrefe(k + 9, dim) + xrefe(k, dim)) +
            .5 * SoSh18::xsi_[gp](2) * deriv_q9(1, k) * (xrefe(k + 9, dim) - xrefe(k, dim));

        jac(2, dim) += .5 * shapefunct_q9(k) * (xrefe(k + 9, dim) - xrefe(k, dim));
      }
    double detJ = jac.Determinant();

    // transformation from local (parameter) element space to global(material) space
    // with famous 'T'-matrix already used for EAS but now evaluated at each gp
    CORE::LINALG::Matrix<MAT::NUM_STRESS_3D, MAT::NUM_STRESS_3D> TinvT;
    EvaluateT(jac, TinvT);

    // **********************************************************************
    // set up B-Operator in local(parameter) element space including ANS
    // **********************************************************************
    CORE::LINALG::Matrix<MAT::NUM_STRESS_3D, NUMDOF_SOH18> bop_loc(true);
    CalculateBopLoc(xcurr, xrefe, shapefunct_q9, deriv_q9, gp, bop_loc);
    CORE::LINALG::Matrix<MAT::NUM_STRESS_3D, NUMDOF_SOH18> bop;
    bop.Multiply(TinvT, bop_loc);

    // **************************************************************************
    // shell-like calculation of strains
    // see Diss. Koschnik page 41
    // **************************************************************************
    CORE::LINALG::Matrix<MAT::NUM_STRESS_3D, 1> lstrain(true);
    CalculateLocStrain(xcurr, xrefe, shapefunct_q9, deriv_q9, gp, lstrain);
    CORE::LINALG::Matrix<MAT::NUM_STRESS_3D, 1> glstrain;
    glstrain.Multiply(TinvT, lstrain);
    // **************************************************************************
    // shell-like calculation of strains
    // **************************************************************************

    // EAS: enhance the strains ***********************************************
    if (eas_)
    {
      double t33 = 0.;
      for (int dim = 0; dim < 3; ++dim) t33 += jac(2, dim) * G3_0_contra(dim);

      M.Multiply(t33 * t33 / detJ, TinvT, M_gp[gp], 0.);
      glstrain.Multiply(1., M, SoSh18::alpha_eas_, 1.);
    }
    // end EAS: enhance the strains *******************************************

    // calculate the deformation gradient consistent to the modified strains
    // but only if the material needs a deformation gradient (e.g. plasticity)
    CORE::LINALG::Matrix<NUMDIM_SOH18, NUMDIM_SOH18> defgrd;
    if (Teuchos::rcp_static_cast<MAT::So3Material>(Material())->NeedsDefgrd() ||
        iostrain == INPAR::STR::strain_ea || iostress == INPAR::STR::stress_cauchy)
    {
      // compute the deformation gradient - shell-style
      // deformation gradient with derivatives w.r.t. local basis
      CORE::LINALG::Matrix<NUMDIM_SOH18, NUMDIM_SOH18> defgrd_loc(true);
      for (int k = 0; k < 9; ++k)
        for (int dim = 0; dim < NUMDIM_SOH18; ++dim)
        {
          defgrd_loc(dim, 0) += .5 * deriv_q9(0, k) *
                                ((xcurr(k + 9, dim) + xcurr(k, dim)) +
                                    SoSh18::xsi_[gp](2) * (xcurr(k + 9, dim) - xcurr(k, dim)));
          defgrd_loc(dim, 1) += .5 * deriv_q9(1, k) *
                                ((xcurr(k + 9, dim) + xcurr(k, dim)) +
                                    SoSh18::xsi_[gp](2) * (xcurr(k + 9, dim) - xcurr(k, dim)));
          defgrd_loc(dim, 2) += .5 * shapefunct_q9(k) * (xcurr(k + 9, dim) - xcurr(k, dim));
        }

      // displacement-based deformation gradient
      CORE::LINALG::Matrix<NUMDIM_SOH18, NUMDIM_SOH18> defgrd_disp;
      defgrd_disp.MultiplyNT(defgrd_loc, SoSh18::invJ_[gp]);
      if (eas_ || dsg_shear_ || dsg_membrane_ || dsg_ctl_)
        SoSh18::CalcConsistentDefgrd(defgrd_disp, glstrain, defgrd);
    }

    // plastic flow increment
    BuildDeltaLp(gp);

    // material call *********************************************
    CORE::LINALG::Matrix<numstr_, 1> pk2;
    CORE::LINALG::Matrix<numstr_, numstr_> cmat;
    if (plmat != nullptr)
      plmat->EvaluateElast(&defgrd, &DeltaLp(), &pk2, &cmat, gp, Id());
    else
    {
      SolidMaterial()->Evaluate(&defgrd, &glstrain, params, &pk2, &cmat, gp, Id());
    }
    // material call *********************************************

    // strain output **********************************************************
    if (elestrain)
    {
      // return gp strains if necessary
      switch (iostrain)
      {
        case INPAR::STR::strain_gl:
        {
          if (elestrain == nullptr) FOUR_C_THROW("strain data not available");
          for (int i = 0; i < 3; ++i)
          {
            (*elestrain)(gp, i) = glstrain(i);
          }
          for (int i = 3; i < 6; ++i)
          {
            (*elestrain)(gp, i) = 0.5 * glstrain(i);
          }
        }
        break;
        case INPAR::STR::strain_ea:
        {
          CORE::LINALG::Matrix<3, 3> bi;
          bi.MultiplyNT(defgrd, defgrd);
          bi.Invert();
          for (int i = 0; i < 3; i++) (*elestrain)(gp, i) = .5 * (1. - bi(i, i));
          (*elestrain)(gp, 3) = -bi(0, 1);
          (*elestrain)(gp, 4) = -bi(2, 1);
          (*elestrain)(gp, 5) = -bi(0, 2);
          break;
        }
        case INPAR::STR::strain_none:
          break;
        default:
          FOUR_C_THROW("requested strain option not available");
          break;
      }
    }
    // end of strain output ***************************************************

    // stress output **********************************************************
    if (elestress)
    {
      // return gp strains if necessary
      switch (iostress)
      {
        case INPAR::STR::stress_2pk:
        {
          if (elestress == nullptr) FOUR_C_THROW("stress data not available");
          for (int i = 0; i < MAT::NUM_STRESS_3D; ++i)
          {
            (*elestress)(gp, i) = pk2(i);
          }
        }
        break;
        case INPAR::STR::stress_cauchy:
        {
          if (elestress == nullptr) FOUR_C_THROW("stress data not available");
          CORE::LINALG::Matrix<3, 3> pkstress;
          pkstress(0, 0) = pk2(0);
          pkstress(0, 1) = pk2(3);
          pkstress(0, 2) = pk2(5);
          pkstress(1, 0) = pkstress(0, 1);
          pkstress(1, 1) = pk2(1);
          pkstress(1, 2) = pk2(4);
          pkstress(2, 0) = pkstress(0, 2);
          pkstress(2, 1) = pkstress(1, 2);
          pkstress(2, 2) = pk2(2);

          CORE::LINALG::Matrix<3, 3> cauchystress;
          CORE::LINALG::Matrix<3, 3> temp;
          temp.Multiply(1.0 / defgrd.Determinant(), defgrd, pkstress);
          cauchystress.MultiplyNT(temp, defgrd);

          (*elestress)(gp, 0) = cauchystress(0, 0);
          (*elestress)(gp, 1) = cauchystress(1, 1);
          (*elestress)(gp, 2) = cauchystress(2, 2);
          (*elestress)(gp, 3) = cauchystress(0, 1);
          (*elestress)(gp, 4) = cauchystress(1, 2);
          (*elestress)(gp, 5) = cauchystress(0, 2);
        }
        break;
        case INPAR::STR::stress_none:
          break;
        default:
          FOUR_C_THROW("requested stress option not available");
          break;
      }
    }
    // end of stress output ***************************************************

    double detJ_w = detJ * SoSh18::wgt_[gp];

    // update internal force vector
    if (force != nullptr) force->MultiplyTN(detJ_w, bop, pk2, 1.0);

    // update stiffness matrix
    if (stiffmatrix != nullptr)
    {
      // integrate `elastic' and `initial-displacement' stiffness matrix
      // keu = keu + (B^T . C . B) * detJ * w(gp)
      CORE::LINALG::Matrix<MAT::NUM_STRESS_3D, NUMDOF_SOH18> cb;
      cb.Multiply(cmat, bop);
      stiffmatrix->MultiplyTN(detJ_w, bop, cb, 1.0);  // standard hex8 evaluation
      // intergrate `geometric' stiffness matrix and add to keu *****************
      CalculateGeoStiff(shapefunct_q9, deriv_q9, TinvT, gp, detJ_w, pk2, stiffmatrix);

      // EAS technology: integrate matrices --------------------------------- EAS
      if (eas_)
      {
        CORE::LINALG::Matrix<6, num_eas> cM;
        cM.Multiply(cmat, M);
        SoSh18::KaaInv_.MultiplyTN(detJ_w, M, cM, 1.);
        SoSh18::Kad_.MultiplyTN(detJ_w, M, cb, 1.);
        SoSh18::feas_.MultiplyTN(detJ_w, M, pk2, 1.);
        CORE::LINALG::DENSEFUNCTIONS::multiplyTN<double, numdofperelement_, numstr_, num_eas>(
            1.0, Kda.values(), detJ_w, cb.A(), M.A());
      }
      // EAS technology: integrate matrices --------------------------------- EAS
    }

    if (massmatrix != nullptr)  // evaluate mass matrix +++++++++++++++++++++++++
    {
      // shape function and derivatives
      CORE::LINALG::Matrix<NUMNOD_SOH18, 1> shapefunct;
      CORE::FE::shape_function<CORE::FE::CellType::hex18>(SoSh18::xsi_[gp], shapefunct);

      double density = Material()->Density(gp);

      // integrate consistent mass matrix
      const double factor = detJ_w * density;
      double ifactor, massfactor;
      for (int inod = 0; inod < NUMNOD_SOH18; ++inod)
      {
        ifactor = shapefunct(inod) * factor;
        for (int jnod = 0; jnod < NUMNOD_SOH18; ++jnod)
        {
          massfactor = shapefunct(jnod) * ifactor;  // intermediate factor
          (*massmatrix)(NUMDIM_SOH18 * inod + 0, NUMDIM_SOH18 * jnod + 0) += massfactor;
          (*massmatrix)(NUMDIM_SOH18 * inod + 1, NUMDIM_SOH18 * jnod + 1) += massfactor;
          (*massmatrix)(NUMDIM_SOH18 * inod + 2, NUMDIM_SOH18 * jnod + 2) += massfactor;
        }
      }
    }  // end of mass matrix +++++++++++++++++++++++++++++++++++++++++++++++++++


    // plastic modifications
    if ((stiffmatrix != nullptr || force != nullptr) && plmat != nullptr)
    {
      if (HavePlasticSpin())
      {
        if (eas_)
          CondensePlasticity<plspin>(defgrd, DeltaLp(), bop, nullptr, nullptr, detJ_w, gp, gp_temp,
              params, force, stiffmatrix, &M_ep, &Kda);
        else
          CondensePlasticity<plspin>(defgrd, DeltaLp(), bop, nullptr, nullptr, detJ_w, gp, gp_temp,
              params, force, stiffmatrix);
      }
      else
      {
        if (eas_)
          CondensePlasticity<zerospin>(defgrd, DeltaLp(), bop, nullptr, nullptr, detJ_w, gp,
              gp_temp, params, force, stiffmatrix, &M_ep, &Kda);
        else
          CondensePlasticity<zerospin>(defgrd, DeltaLp(), bop, nullptr, nullptr, detJ_w, gp,
              gp_temp, params, force, stiffmatrix);
      }
    }  // plastic modifications
    /* =========================================================================*/
  } /* ==================================================== end of Loop over GP */
  /* =========================================================================*/

  if ((stiffmatrix || force) && eas_)
  {
    CORE::LINALG::FixedSizeSerialDenseSolver<num_eas, num_eas, 1> solve_for_KaaInv;
    solve_for_KaaInv.SetMatrix(SoSh18::KaaInv_);
    int err2 = solve_for_KaaInv.Factor();
    int err = solve_for_KaaInv.Invert();
    if ((err != 0) || (err2 != 0)) FOUR_C_THROW("Inversion of Kaa failed");

    CORE::LINALG::Matrix<NUMDOF_SOH18, num_eas> KdaKaa;
    CORE::LINALG::DENSEFUNCTIONS::multiply<double, numdofperelement_, num_eas, num_eas>(
        0., KdaKaa.A(), 1., Kda.values(), SoSh18::KaaInv_.A());
    if (stiffmatrix) stiffmatrix->Multiply(-1., KdaKaa, SoSh18::Kad_, 1.);
    if (force) force->Multiply(-1., KdaKaa, SoSh18::feas_, 1.);
  }

  return;
}

FOUR_C_NAMESPACE_CLOSE