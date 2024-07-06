/*----------------------------------------------------------------------*/
/*! \file

\brief Solid-scatra elements evaluate

\level 2


*----------------------------------------------------------------------*/

#include "4C_fem_general_element_center.hpp"
#include "4C_fem_general_extract_values.hpp"
#include "4C_mat_so3_material.hpp"
#include "4C_so3_element_service.hpp"
#include "4C_so3_scatra.hpp"
#include "4C_structure_new_enum_lists.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <class So3Ele, Core::FE::CellType distype>
void Discret::ELEMENTS::So3Scatra<So3Ele, distype>::pre_evaluate(Teuchos::ParameterList& params,
    Core::FE::Discretization& discretization, Core::Elements::Element::LocationArray& la)
{
  if (la.size() > 1)
  {
    // ask for the number of dofs of second dofset (scatra)
    const int numscal = discretization.num_dof(1, nodes()[0]);

    if (la[1].size() != numnod_ * numscal)
      FOUR_C_THROW(
          "So3_Scatra: pre_evaluate: Location vector length for concentrations does not match!");

    if (discretization.has_state(1, "scalarfield"))  // if concentrations were set
    {
      if (not(distype == Core::FE::CellType::hex8 or distype == Core::FE::CellType::hex27 or
              distype == Core::FE::CellType::tet4 or distype == Core::FE::CellType::tet10))
      {
        FOUR_C_THROW(
            "The Solidscatra elements are only tested for the Hex8, Hex27, Tet4, and Tet10 case. "
            "The following should work, but keep your eyes open (especially with the order of the "
            "Gauss points)");
      }

      /* =========================================================================*/
      // start concentration business
      /* =========================================================================*/
      auto gpconc = Teuchos::rcp(
          new std::vector<std::vector<double>>(numgpt_, std::vector<double>(numscal, 0.0)));

      // check if you can get the scalar state
      Teuchos::RCP<const Epetra_Vector> concnp = discretization.get_state(1, "scalarfield");

      if (concnp == Teuchos::null)
        FOUR_C_THROW("calc_struct_nlnstiff: Cannot get state vector 'scalarfield' ");

      // extract local values of the global vectors
      auto myconc = std::vector<double>(la[1].lm_.size(), 0.0);

      Core::FE::ExtractMyValues(*concnp, myconc, la[1].lm_);

      // element vector for k-th scalar
      std::vector<Core::LinAlg::Matrix<numnod_, 1>> econc(numscal);
      for (int k = 0; k < numscal; ++k)
        for (int i = 0; i < numnod_; ++i) (econc.at(k))(i, 0) = myconc.at(numscal * i + k);

      /* =========================================================================*/
      /* ================================================= Loop over Gauss Points */
      /* =========================================================================*/
      // volume of current element in reference configuration
      double volume_ref = 0.0;
      // mass in current element in reference configuration
      std::vector<double> mass_ref(numscal, 0.0);

      for (int igp = 0; igp < numgpt_; ++igp)
      {
        // detJrefpar_wgp = det(dX/dr) * w_gp to calculate volume in reference configuration
        const double detJrefpar_wgp = det_j_[igp] * intpoints_.qwgt[igp];

        volume_ref += detJrefpar_wgp;

        // concentrations at current gauss point
        std::vector<double> conc_gp_k(numscal, 0.0);

        // shape functions evaluated at current gauss point
        Core::LinAlg::Matrix<numnod_, 1> shapefunct_gp(true);
        Core::FE::shape_function<distype>(xsi_[igp], shapefunct_gp);

        for (int k = 0; k < numscal; ++k)
        {
          // identical shapefunctions for displacements and temperatures
          conc_gp_k.at(k) = shapefunct_gp.dot(econc.at(k));

          mass_ref.at(k) += conc_gp_k.at(k) * detJrefpar_wgp;
        }

        gpconc->at(igp) = conc_gp_k;
      }

      params.set<Teuchos::RCP<std::vector<std::vector<double>>>>("gp_conc", gpconc);

      // compute average concentrations. Now mass_ref is the element averaged concentration
      for (int k = 0; k < numscal; ++k) mass_ref.at(k) /= volume_ref;

      auto avgconc = Teuchos::rcp(new std::vector<std::vector<double>>(numgpt_, mass_ref));

      params.set<Teuchos::RCP<std::vector<std::vector<double>>>>("avg_conc", avgconc);

    }  // if (discretization.HasState(1,"scalarfield"))

    // if temperatures were set
    if (discretization.num_dof_sets() == 3)
    {
      if (discretization.has_state(2, "tempfield"))
      {
        if (not(distype == Core::FE::CellType::hex8 or distype == Core::FE::CellType::hex27 or
                distype == Core::FE::CellType::tet4 or distype == Core::FE::CellType::tet10))
        {
          FOUR_C_THROW(
              "The Solidscatra elements are only tested for the Hex8, Hex27, Tet4, and Tet10 case. "
              "The following should work, but keep your eyes open (especially with the order of "
              "the Gauss points");
        }

        /* =========================================================================*/
        // start temperature business
        /* =========================================================================*/
        auto gptemp = Teuchos::rcp(new std::vector<double>(std::vector<double>(numgpt_, 0.0)));

        Teuchos::RCP<const Epetra_Vector> tempnp = discretization.get_state(2, "tempfield");

        if (tempnp == Teuchos::null)
          FOUR_C_THROW("calc_struct_nlnstiff: Cannot get state vector 'tempfield' ");

        // extract local values of the global vectors
        auto mytemp = std::vector<double>(la[2].lm_.size(), 0.0);

        Core::FE::ExtractMyValues(*tempnp, mytemp, la[2].lm_);

        // element vector for k-th scalar
        Core::LinAlg::Matrix<numnod_, 1> etemp;

        for (int i = 0; i < numnod_; ++i) etemp(i, 0) = mytemp.at(i);

        /* =========================================================================*/
        /* ================================================= Loop over Gauss Points */
        /* =========================================================================*/

        for (int igp = 0; igp < numgpt_; ++igp)
        {
          // shape functions evaluated at current gauss point
          Core::LinAlg::Matrix<numnod_, 1> shapefunct_gp(true);
          Core::FE::shape_function<distype>(xsi_[igp], shapefunct_gp);

          // temperature at Gauss point withidentical shapefunctions for displacements and
          // temperatures
          gptemp->at(igp) = shapefunct_gp.dot(etemp);
        }

        params.set<Teuchos::RCP<std::vector<double>>>("gp_temp", gptemp);
      }
    }

    // If you need a pointer to the scatra material, use these lines:
    // we assume that the second material of the structure is the scatra element material
    // Teuchos::RCP<Core::Mat::Material> scatramat = So3Ele::Material(1);
    // params.set< Teuchos::RCP<Core::Mat::Material> >("scatramat",scatramat);
  }

  // TODO: (thon) actually we do not want this here, since it has nothing to do with scatra specific
  // stuff. But for now we let it be...
  const Core::LinAlg::Matrix<3, 1> center(Core::FE::element_center_refe_coords(*this).data());
  params.set("elecenter_coords_ref", center);
}

/*----------------------------------------------------------------------*
 |  evaluate the element (public)                                       |
 *----------------------------------------------------------------------*/
template <class So3Ele, Core::FE::CellType distype>
int Discret::ELEMENTS::So3Scatra<So3Ele, distype>::evaluate(Teuchos::ParameterList& params,
    Core::FE::Discretization& discretization, Core::Elements::Element::LocationArray& la,
    Core::LinAlg::SerialDenseMatrix& elemat1_epetra,
    Core::LinAlg::SerialDenseMatrix& elemat2_epetra,
    Core::LinAlg::SerialDenseVector& elevec1_epetra,
    Core::LinAlg::SerialDenseVector& elevec2_epetra,
    Core::LinAlg::SerialDenseVector& elevec3_epetra)
{
  // start with ActionType "none"
  typename So3Scatra::ActionType act = So3Scatra::none;

  // get the required action
  std::string action = params.get<std::string>("action", "none");

  // get the required action and safety check
  if (action == "none")
    FOUR_C_THROW("No action supplied");
  else if (action == "calc_struct_stiffscalar")
    act = So3Scatra::calc_struct_stiffscalar;

  // at the moment all cases need the pre_evaluate routine, since we always need the concentration
  // value at the gp
  pre_evaluate(params, discretization, la);

  // what action shall be performed
  switch (act)
  {
    // coupling terms K_dS of stiffness matrix K^{SSI} for monolithic SSI
    case So3Scatra::calc_struct_stiffscalar:
    {
      Teuchos::RCP<const Epetra_Vector> disp = discretization.get_state(0, "displacement");
      if (disp == Teuchos::null) FOUR_C_THROW("Cannot get state vectors 'displacement'");

      // get my displacement vector
      std::vector<double> mydisp((la[0].lm_).size());
      Core::FE::ExtractMyValues(*disp, mydisp, la[0].lm_);

      // calculate the stiffness matrix
      nln_kd_s_ssi(la, mydisp, elemat1_epetra, params);

      break;
    }

    default:
    {
      // call the base class routine
      So3Ele::evaluate(params, discretization, la[0].lm_, elemat1_epetra, elemat2_epetra,
          elevec1_epetra, elevec2_epetra, elevec3_epetra);
      break;
    }  // default
  }    // switch(act)

  return 0;
}  // Evaluate


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <class So3Ele, Core::FE::CellType distype>
void Discret::ELEMENTS::So3Scatra<So3Ele, distype>::get_cauchy_n_dir_and_derivatives_at_xi(
    const Core::LinAlg::Matrix<3, 1>& xi, const std::vector<double>& disp_nodal_values,
    const std::vector<double>& scalar_nodal_values, const Core::LinAlg::Matrix<3, 1>& n,
    const Core::LinAlg::Matrix<3, 1>& dir, double& cauchy_n_dir,
    Core::LinAlg::SerialDenseMatrix* d_cauchyndir_dd,
    Core::LinAlg::SerialDenseMatrix* d_cauchyndir_ds, Core::LinAlg::Matrix<3, 1>* d_cauchyndir_dn,
    Core::LinAlg::Matrix<3, 1>* d_cauchyndir_ddir, Core::LinAlg::Matrix<3, 1>* d_cauchyndir_dxi)
{
  auto scalar_values_at_xi =
      Discret::ELEMENTS::ProjectNodalQuantityToXi<distype>(xi, scalar_nodal_values);
  double d_cauchyndir_ds_gp(0.0);
  // call base class
  So3Ele::get_cauchy_n_dir_and_derivatives_at_xi(xi, disp_nodal_values, n, dir, cauchy_n_dir,
      d_cauchyndir_dd, nullptr, nullptr, nullptr, nullptr, d_cauchyndir_dn, d_cauchyndir_ddir,
      d_cauchyndir_dxi, nullptr, nullptr, nullptr, scalar_values_at_xi.data(), &d_cauchyndir_ds_gp);

  if (d_cauchyndir_ds != nullptr)
  {
    d_cauchyndir_ds->shape(numnod_, 1);
    // get the shape functions
    Core::LinAlg::Matrix<numnod_, 1> shapefunct(true);
    Core::FE::shape_function<distype>(xi, shapefunct);
    // calculate DsntDs
    Core::LinAlg::Matrix<numnod_, 1>(d_cauchyndir_ds->values(), true)
        .update(d_cauchyndir_ds_gp, shapefunct, 1.0);
  }
}

/*----------------------------------------------------------------------*
 | evaluate only the mechanical-scatra stiffness term     schmidt 10/17 |
 | for monolithic SSI, contribution to k_dS (private)                   |
 *----------------------------------------------------------------------*/
template <class So3Ele, Core::FE::CellType distype>
void Discret::ELEMENTS::So3Scatra<So3Ele, distype>::nln_kd_s_ssi(
    Core::Elements::Element::LocationArray& la,
    std::vector<double>& disp,                         // current displacement
    Core::LinAlg::SerialDenseMatrix& stiffmatrix_kdS,  // (numdim_*numnod_ ; numnod_)
    Teuchos::ParameterList& params)
{
  // calculate current and material coordinates of element
  Core::LinAlg::Matrix<numnod_, numdim_> xrefe(true);  // X, material coord. of element
  Core::LinAlg::Matrix<numnod_, numdim_> xcurr(true);  // x, current  coord. of element
  for (int i = 0; i < numnod_; ++i)
  {
    const auto& x = nodes()[i]->x();
    xrefe(i, 0) = x[0];
    xrefe(i, 1) = x[1];
    xrefe(i, 2) = x[2];

    xcurr(i, 0) = xrefe(i, 0) + disp[i * numdofpernode_ + 0];
    xcurr(i, 1) = xrefe(i, 1) + disp[i * numdofpernode_ + 1];
    xcurr(i, 2) = xrefe(i, 2) + disp[i * numdofpernode_ + 2];
  }

  // shape functions and their first derivatives
  Core::LinAlg::Matrix<numnod_, 1> shapefunct(true);
  Core::LinAlg::Matrix<numdim_, numnod_> deriv(true);
  // compute derivatives N_XYZ at gp w.r.t. material coordinates
  Core::LinAlg::Matrix<numdim_, numnod_> N_XYZ(true);
  // compute deformation gradient w.r.t. to material configuration
  Core::LinAlg::Matrix<numdim_, numdim_> defgrad(true);

  // evaluation of linearization w.r.t. certain primary variable
  const int differentiationtype =
      params.get<int>("differentiationtype", static_cast<int>(Solid::DifferentiationType::none));
  if (differentiationtype == static_cast<int>(Solid::DifferentiationType::none))
    FOUR_C_THROW("Cannot get differentation type");

  // get numscatradofspernode from parameter list in case of elch linearizations
  int numscatradofspernode(-1);
  if (differentiationtype == static_cast<int>(Solid::DifferentiationType::elch))
  {
    numscatradofspernode = params.get<int>("numscatradofspernode", -1);
    if (numscatradofspernode == -1)
      FOUR_C_THROW("Could not read 'numscatradofspernode' from parameter list!");
  }

  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  for (int gp = 0; gp < numgpt_; ++gp)
  {
    // get shape functions and their derivatives
    Core::FE::shape_function<distype>(xsi_[gp], shapefunct);
    Core::FE::shape_function_deriv1<distype>(xsi_[gp], deriv);

    // compute derivatives N_XYZ at gp w.r.t. material coordinates
    // by N_XYZ = J^-1 . N_rst
    N_XYZ.multiply(inv_j_[gp], deriv);

    // (material) deformation gradient
    // F = d xcurr / d xrefe = xcurr^T . N_XYZ^T
    defgrad.multiply_tt(xcurr, N_XYZ);

    // right Cauchy-Green tensor = F^T . F
    Core::LinAlg::Matrix<3, 3> cauchygreen;
    cauchygreen.multiply_tn(defgrad, defgrad);

    // calculate vector of right Cauchy-Green tensor
    Core::LinAlg::Matrix<numstr_, 1> cauchygreenvec;
    cauchygreenvec(0) = cauchygreen(0, 0);
    cauchygreenvec(1) = cauchygreen(1, 1);
    cauchygreenvec(2) = cauchygreen(2, 2);
    cauchygreenvec(3) = 2 * cauchygreen(0, 1);
    cauchygreenvec(4) = 2 * cauchygreen(1, 2);
    cauchygreenvec(5) = 2 * cauchygreen(2, 0);

    // Green Lagrange strain
    Core::LinAlg::Matrix<numstr_, 1> glstrain;
    // Green-Lagrange strain matrix E = 0.5 * (Cauchygreen - Identity)
    glstrain(0) = 0.5 * (cauchygreen(0, 0) - 1.0);
    glstrain(1) = 0.5 * (cauchygreen(1, 1) - 1.0);
    glstrain(2) = 0.5 * (cauchygreen(2, 2) - 1.0);
    glstrain(3) = cauchygreen(0, 1);
    glstrain(4) = cauchygreen(1, 2);
    glstrain(5) = cauchygreen(2, 0);

    // calculate nonlinear B-operator
    Core::LinAlg::Matrix<numstr_, numdofperelement_> bop(true);
    calculate_bop(&bop, &defgrad, &N_XYZ);

    /*==== call material law ======================================================*/
    // init derivative of second Piola-Kirchhoff stresses w.r.t. concentrations dSdc
    Core::LinAlg::Matrix<numstr_, 1> dSdc(true);

    // get dSdc, hand in nullptr as 'cmat' to evaluate the off-diagonal block
    Teuchos::RCP<Mat::So3Material> so3mat = Teuchos::rcp_static_cast<Mat::So3Material>(material());
    so3mat->evaluate(&defgrad, &glstrain, params, &dSdc, nullptr, gp, id());

    /*==== end of call material law ===============================================*/

    // k_dS = B^T . dS/dc * detJ * N * w(gp)
    const double detJ_w = det_j_[gp] * intpoints_.qwgt[gp];
    Core::LinAlg::Matrix<numdofperelement_, 1> BdSdc(true);
    BdSdc.multiply_tn(detJ_w, bop, dSdc);

    // loop over rows
    for (int rowi = 0; rowi < numdofperelement_; ++rowi)
    {
      const double BdSdc_rowi = BdSdc(rowi, 0);
      // loop over columns
      for (int coli = 0; coli < numnod_; ++coli)
      {
        // stiffness matrix w.r.t. elch dofs
        if (differentiationtype == static_cast<int>(Solid::DifferentiationType::elch))
          stiffmatrix_kdS(rowi, coli * numscatradofspernode) += BdSdc_rowi * shapefunct(coli, 0);
        else if (differentiationtype == static_cast<int>(Solid::DifferentiationType::temp))
          stiffmatrix_kdS(rowi, coli) += BdSdc_rowi * shapefunct(coli, 0);
        else
          FOUR_C_THROW("Unknown differentation type");
      }
    }
  }  // gauss point loop
}  // nln_kd_s_ssi


/*----------------------------------------------------------------------*
 | calculate the nonlinear B-operator (private)           schmidt 10/17 |
 *----------------------------------------------------------------------*/
template <class So3Ele, Core::FE::CellType distype>
void Discret::ELEMENTS::So3Scatra<So3Ele, distype>::calculate_bop(
    Core::LinAlg::Matrix<numstr_, numdofperelement_>* bop,  //!< (o): nonlinear B-operator
    const Core::LinAlg::Matrix<numdim_, numdim_>* defgrad,  //!< (i): deformation gradient
    const Core::LinAlg::Matrix<numdim_, numnod_>* N_XYZ)
    const  //!< (i): (material) derivative of shape functions
{
  // calc bop matrix if provided
  if (bop != nullptr)
  {
    /* non-linear B-operator (may so be called, meaning of B-operator is not so
    **  sharp in the non-linear realm) *
    **   B = F^{i,T} . B_L *
    ** with linear B-operator B_L =  N_XYZ (6x24) = (3x8)
    **
    **   B    =   F^T  . N_XYZ
    ** (6x24)    (3x3)   (3x8)
    **
    **      [ ... | F_11*N_{,1}^k  F_21*N_{,1}^k  F_31*N_{,1}^k | ... ]
    **      [ ... | F_12*N_{,2}^k  F_22*N_{,2}^k  F_32*N_{,2}^k | ... ]
    **      [ ... | F_13*N_{,3}^k  F_23*N_{,3}^k  F_33*N_{,3}^k | ... ]
    ** B =  [ ~~~   ~~~~~~~~~~~~~  ~~~~~~~~~~~~~  ~~~~~~~~~~~~~   ~~~ ]
    **      [       F_11*N_{,2}^k+F_12*N_{,1}^k                       ]
    **      [ ... |          F_21*N_{,2}^k+F_22*N_{,1}^k        | ... ]
    **      [                       F_31*N_{,2}^k+F_32*N_{,1}^k       ]
    **      [                                                         ]
    **      [       F_12*N_{,3}^k+F_13*N_{,2}^k                       ]
    **      [ ... |          F_22*N_{,3}^k+F_23*N_{,2}^k        | ... ]
    **      [                       F_32*N_{,3}^k+F_33*N_{,2}^k       ]
    **      [                                                         ]
    **      [       F_13*N_{,1}^k+F_11*N_{,3}^k                       ]
    **      [ ... |          F_23*N_{,1}^k+F_21*N_{,3}^k        | ... ]
    **      [                       F_33*N_{,1}^k+F_31*N_{,3}^k       ]
    */
    for (int i = 0; i < numnod_; ++i)
    {
      (*bop)(0, numdofpernode_ * i + 0) = (*defgrad)(0, 0) * (*N_XYZ)(0, i);
      (*bop)(0, numdofpernode_ * i + 1) = (*defgrad)(1, 0) * (*N_XYZ)(0, i);
      (*bop)(0, numdofpernode_ * i + 2) = (*defgrad)(2, 0) * (*N_XYZ)(0, i);
      (*bop)(1, numdofpernode_ * i + 0) = (*defgrad)(0, 1) * (*N_XYZ)(1, i);
      (*bop)(1, numdofpernode_ * i + 1) = (*defgrad)(1, 1) * (*N_XYZ)(1, i);
      (*bop)(1, numdofpernode_ * i + 2) = (*defgrad)(2, 1) * (*N_XYZ)(1, i);
      (*bop)(2, numdofpernode_ * i + 0) = (*defgrad)(0, 2) * (*N_XYZ)(2, i);
      (*bop)(2, numdofpernode_ * i + 1) = (*defgrad)(1, 2) * (*N_XYZ)(2, i);
      (*bop)(2, numdofpernode_ * i + 2) = (*defgrad)(2, 2) * (*N_XYZ)(2, i);
      /* ~~~ */
      (*bop)(3, numdofpernode_ * i + 0) =
          (*defgrad)(0, 0) * (*N_XYZ)(1, i) + (*defgrad)(0, 1) * (*N_XYZ)(0, i);
      (*bop)(3, numdofpernode_ * i + 1) =
          (*defgrad)(1, 0) * (*N_XYZ)(1, i) + (*defgrad)(1, 1) * (*N_XYZ)(0, i);
      (*bop)(3, numdofpernode_ * i + 2) =
          (*defgrad)(2, 0) * (*N_XYZ)(1, i) + (*defgrad)(2, 1) * (*N_XYZ)(0, i);
      (*bop)(4, numdofpernode_ * i + 0) =
          (*defgrad)(0, 1) * (*N_XYZ)(2, i) + (*defgrad)(0, 2) * (*N_XYZ)(1, i);
      (*bop)(4, numdofpernode_ * i + 1) =
          (*defgrad)(1, 1) * (*N_XYZ)(2, i) + (*defgrad)(1, 2) * (*N_XYZ)(1, i);
      (*bop)(4, numdofpernode_ * i + 2) =
          (*defgrad)(2, 1) * (*N_XYZ)(2, i) + (*defgrad)(2, 2) * (*N_XYZ)(1, i);
      (*bop)(5, numdofpernode_ * i + 0) =
          (*defgrad)(0, 2) * (*N_XYZ)(0, i) + (*defgrad)(0, 0) * (*N_XYZ)(2, i);
      (*bop)(5, numdofpernode_ * i + 1) =
          (*defgrad)(1, 2) * (*N_XYZ)(0, i) + (*defgrad)(1, 0) * (*N_XYZ)(2, i);
      (*bop)(5, numdofpernode_ * i + 2) =
          (*defgrad)(2, 2) * (*N_XYZ)(0, i) + (*defgrad)(2, 0) * (*N_XYZ)(2, i);
    }
  }
}  // calculate_bop


/*----------------------------------------------------------------------*
 | initialize element (private)                            schmidt 10/17|
 *----------------------------------------------------------------------*/
template <class So3Ele, Core::FE::CellType distype>
void Discret::ELEMENTS::So3Scatra<So3Ele, distype>::init_element()
{
  // resize gauss point coordinates, inverse of the jacobian and determinant of the jacobian
  xsi_.resize(numgpt_);
  inv_j_.resize(numgpt_);
  det_j_.resize(numgpt_);

  // calculate coordinates in reference (material) configuration
  Core::LinAlg::Matrix<numnod_, numdim_> xrefe;
  for (int i = 0; i < numnod_; ++i)
  {
    xrefe(i, 0) = nodes()[i]->x()[0];
    xrefe(i, 1) = nodes()[i]->x()[1];
    xrefe(i, 2) = nodes()[i]->x()[2];
  }

  // calculate gauss point coordinates, the inverse jacobian and the determinant of the jacobian
  for (int gp = 0; gp < numgpt_; ++gp)
  {
    // gauss point coordinates
    const double* gpcoord = intpoints_.point(gp);
    for (int idim = 0; idim < numdim_; idim++) xsi_[gp](idim) = gpcoord[idim];

    // get derivative of shape functions w.r.t. parameter coordinates, needed for calculation of the
    // inverse of the jacobian
    Core::LinAlg::Matrix<numdim_, numnod_> deriv;
    Core::FE::shape_function_deriv1<distype>(xsi_[gp], deriv);

    // get the inverse of the Jacobian matrix which looks like:
    /*
                 [ X_,r  Y_,r  Z_,r ]^-1
          J^-1 = [ X_,s  Y_,s  Z_,s ]
                 [ X_,t  Y_,t  Z_,t ]
     */

    inv_j_[gp].multiply(deriv, xrefe);
    // here Jacobian is inverted and det(J) is calculated
    det_j_[gp] = inv_j_[gp].invert();

    // make sure determinant of jacobian is positive
    if (det_j_[gp] <= 0.0) FOUR_C_THROW("Element Jacobian mapping %10.5e <= 0.0", det_j_[gp]);
  }
}


template class Discret::ELEMENTS::So3Scatra<Discret::ELEMENTS::SoHex8, Core::FE::CellType::hex8>;
template class Discret::ELEMENTS::So3Scatra<Discret::ELEMENTS::SoHex27, Core::FE::CellType::hex27>;
template class Discret::ELEMENTS::So3Scatra<Discret::ELEMENTS::SoHex8fbar,
    Core::FE::CellType::hex8>;
template class Discret::ELEMENTS::So3Scatra<Discret::ELEMENTS::SoTet4, Core::FE::CellType::tet4>;
template class Discret::ELEMENTS::So3Scatra<Discret::ELEMENTS::SoTet10, Core::FE::CellType::tet10>;
template class Discret::ELEMENTS::So3Scatra<Discret::ELEMENTS::SoWeg6, Core::FE::CellType::wedge6>;

FOUR_C_NAMESPACE_CLOSE
