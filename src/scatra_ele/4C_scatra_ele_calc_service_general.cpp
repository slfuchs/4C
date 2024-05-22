/*----------------------------------------------------------------------*/
/*! \file

\brief Internal implementation of ScaTra element

\level 1


*/
/*----------------------------------------------------------------------*/

#include "4C_coupling_volmortar_shape.hpp"
#include "4C_discretization_fem_general_extract_values.hpp"
#include "4C_discretization_fem_general_utils_boundary_integration.hpp"
#include "4C_fluid_rotsym_periodicbc.hpp"
#include "4C_global_data.hpp"
#include "4C_mat_scatra_multiscale.hpp"
#include "4C_nurbs_discret_nurbs_utils.hpp"
#include "4C_scatra_ele_action.hpp"
#include "4C_scatra_ele_calc.hpp"
#include "4C_scatra_ele_parameter_std.hpp"
#include "4C_scatra_ele_parameter_timint.hpp"
#include "4C_scatra_ele_parameter_turbulence.hpp"
#include "4C_utils_function.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 | evaluate action                                           fang 02/15 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
int DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::EvaluateAction(DRT::Element* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    const SCATRA::Action& action, DRT::Element::LocationArray& la,
    CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
    CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
    CORE::LINALG::SerialDenseVector& elevec1_epetra,
    CORE::LINALG::SerialDenseVector& elevec2_epetra,
    CORE::LINALG::SerialDenseVector& elevec3_epetra)
{
  //(for now) only first dof set considered
  const std::vector<int>& lm = la[0].lm_;
  // determine and evaluate action
  switch (action)
  {
    // calculate global mass matrix
    case SCATRA::Action::calc_mass_matrix:
    {
      // integration points and weights
      const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
          SCATRA::DisTypeToOptGaussRule<distype>::rule);

      // loop over integration points
      for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
      {
        // evaluate values of shape functions and domain integration factor at current integration
        // point
        const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

        // loop over dofs
        for (int k = 0; k < numdofpernode_; ++k) CalcMatMass(elemat1_epetra, k, fac, 1.);
      }  // loop over integration points

      break;
    }

    // calculate time derivative for time value t_0
    case SCATRA::Action::calc_initial_time_deriv:
    {
      // calculate matrix and rhs
      CalcInitialTimeDerivative(ele, elemat1_epetra, elevec1_epetra, params, discretization, la);
      break;
    }

    case SCATRA::Action::integrate_shape_functions:
    {
      // calculate integral of shape functions
      const auto dofids = params.get<Teuchos::RCP<CORE::LINALG::IntSerialDenseVector>>("dofids");
      IntegrateShapeFunctions(ele, elevec1_epetra, *dofids);

      break;
    }

    case SCATRA::Action::calc_flux_domain:
    {
      // get number of dofset associated with velocity related dofs
      const int ndsvel = scatrapara_->NdsVel();

      // get velocity values at nodes
      const Teuchos::RCP<const Epetra_Vector> convel =
          discretization.GetState(ndsvel, "convective velocity field");
      const Teuchos::RCP<const Epetra_Vector> vel =
          discretization.GetState(ndsvel, "velocity field");

      // safety check
      if (convel == Teuchos::null or vel == Teuchos::null) FOUR_C_THROW("Cannot get state vector");

      // determine number of velocity related dofs per node
      const int numveldofpernode = la[ndsvel].lm_.size() / nen_;

      // construct location vector for velocity related dofs
      std::vector<int> lmvel(nsd_ * nen_, -1);
      for (unsigned inode = 0; inode < nen_; ++inode)
        for (unsigned idim = 0; idim < nsd_; ++idim)
          lmvel[inode * nsd_ + idim] = la[ndsvel].lm_[inode * numveldofpernode + idim];

      // extract local values of (convective) velocity field from global state vector
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nsd_, nen_>>(*convel, econvelnp_, lmvel);
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nsd_, nen_>>(*vel, evelnp_, lmvel);

      // rotate the vector field in the case of rotationally symmetric boundary conditions
      rotsymmpbc_->RotateMyValuesIfNecessary(econvelnp_);
      rotsymmpbc_->RotateMyValuesIfNecessary(evelnp_);

      // need current values of transported scalar
      // -> extract local values from global vectors
      Teuchos::RCP<const Epetra_Vector> phinp = discretization.GetState("phinp");
      if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, ephinp_, lm);

      // access control parameter for flux calculation
      INPAR::SCATRA::FluxType fluxtype = scatrapara_->CalcFluxDomain();
      Teuchos::RCP<std::vector<int>> writefluxids = scatrapara_->WriteFluxIds();

      // we always get an 3D flux vector for each node
      CORE::LINALG::Matrix<3, nen_> eflux(true);

      // do a loop for systems of transported scalars
      for (int& writefluxid : *writefluxids)
      {
        int k = writefluxid - 1;
        // calculate flux vectors for actual scalar
        eflux.Clear();
        CalculateFlux(eflux, ele, fluxtype, k);
        // assembly
        for (unsigned inode = 0; inode < nen_; inode++)
        {
          const int fvi = inode * numdofpernode_ + k;
          elevec1_epetra[fvi] += eflux(0, inode);
          elevec2_epetra[fvi] += eflux(1, inode);
          elevec3_epetra[fvi] += eflux(2, inode);
        }
      }  // loop over numscal

      break;
    }

    case SCATRA::Action::calc_total_and_mean_scalars:
    {
      // get flag for inverting
      const bool inverting = params.get<bool>("inverting");

      const bool calc_grad_phi = params.get<bool>("calc_grad_phi");
      // need current scalar vector
      // -> extract local values from the global vectors
      auto phinp = discretization.GetState("phinp");
      if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, ephinp_, lm);

      // calculate scalars and domain integral
      CalculateScalars(ele, elevec1_epetra, inverting, calc_grad_phi);

      break;
    }

    case SCATRA::Action::calc_mean_scalar_time_derivatives:
    {
      CalculateScalarTimeDerivatives(discretization, lm, elevec1_epetra);
      break;
    }

    // calculate filtered fields for calculation of turbulent Prandtl number
    // required for dynamic Smagorinsky model in scatra
    case SCATRA::Action::calc_scatra_box_filter:
    {
      if (nsd_ == 3)
        CalcBoxFilter(ele, params, discretization, la);
      else
        FOUR_C_THROW("action 'calc_scatra_box_filter' is 3D specific action");

      break;
    }

    // calculate turbulent prandtl number of dynamic Smagorinsky model
    case SCATRA::Action::calc_turbulent_prandtl_number:
    {
      if (nsd_ == 3)
      {
        // get required quantities, set in dynamic Smagorinsky class
        Teuchos::RCP<Epetra_MultiVector> col_filtered_vel =
            params.get<Teuchos::RCP<Epetra_MultiVector>>("col_filtered_vel");
        Teuchos::RCP<Epetra_MultiVector> col_filtered_dens_vel =
            params.get<Teuchos::RCP<Epetra_MultiVector>>("col_filtered_dens_vel");
        Teuchos::RCP<Epetra_MultiVector> col_filtered_dens_vel_temp =
            params.get<Teuchos::RCP<Epetra_MultiVector>>("col_filtered_dens_vel_temp");
        Teuchos::RCP<Epetra_MultiVector> col_filtered_dens_rateofstrain_temp =
            params.get<Teuchos::RCP<Epetra_MultiVector>>("col_filtered_dens_rateofstrain_temp");
        Teuchos::RCP<Epetra_Vector> col_filtered_temp =
            params.get<Teuchos::RCP<Epetra_Vector>>("col_filtered_temp");
        Teuchos::RCP<Epetra_Vector> col_filtered_dens =
            params.get<Teuchos::RCP<Epetra_Vector>>("col_filtered_dens");
        Teuchos::RCP<Epetra_Vector> col_filtered_dens_temp =
            params.get<Teuchos::RCP<Epetra_Vector>>("col_filtered_dens_temp");

        // initialize variables to calculate
        double LkMk = 0.0;
        double MkMk = 0.0;
        double xcenter = 0.0;
        double ycenter = 0.0;
        double zcenter = 0.0;

        // calculate LkMk and MkMk
        switch (distype)
        {
          case CORE::FE::CellType::hex8:
          {
            scatra_calc_smag_const_LkMk_and_MkMk(col_filtered_vel, col_filtered_dens_vel,
                col_filtered_dens_vel_temp, col_filtered_dens_rateofstrain_temp, col_filtered_temp,
                col_filtered_dens, col_filtered_dens_temp, LkMk, MkMk, xcenter, ycenter, zcenter,
                ele);
            break;
          }
          default:
          {
            FOUR_C_THROW("Unknown element type for box filter application\n");
          }
        }

        // set Prt without averaging (only clipping)
        // calculate inverse of turbulent Prandtl number times (C_s*Delta)^2
        double inv_Prt;
        if (abs(MkMk) < 1E-16)
        {
          // std::cout << "warning: abs(MkMk) < 1E-16 -> set inverse of turbulent Prandtl number to
          // zero!"  << std::endl;
          inv_Prt = 0.0;
        }
        else
          inv_Prt = LkMk / MkMk;
        if (inv_Prt < 0.0) inv_Prt = 0.0;

        // set all values in parameter list
        params.set<double>("LkMk", LkMk);
        params.set<double>("MkMk", MkMk);
        params.set<double>("xcenter", xcenter);
        params.set<double>("ycenter", ycenter);
        params.set<double>("zcenter", zcenter);
        params.set<double>("ele_Prt", inv_Prt);
      }
      else
        FOUR_C_THROW("action 'calc_turbulent_prandtl_number' is a 3D specific action");

      break;
    }

    case SCATRA::Action::calc_vreman_scatra:
    {
      if (nsd_ == 3)
      {
        Teuchos::RCP<Epetra_MultiVector> col_filtered_phi =
            params.get<Teuchos::RCP<Epetra_MultiVector>>("col_filtered_phi");
        Teuchos::RCP<Epetra_Vector> col_filtered_phi2 =
            params.get<Teuchos::RCP<Epetra_Vector>>("col_filtered_phi2");
        Teuchos::RCP<Epetra_Vector> col_filtered_phiexpression =
            params.get<Teuchos::RCP<Epetra_Vector>>("col_filtered_phiexpression");
        Teuchos::RCP<Epetra_MultiVector> col_filtered_alphaijsc =
            params.get<Teuchos::RCP<Epetra_MultiVector>>("col_filtered_alphaijsc");

        // initialize variables to calculate
        double dt_numerator = 0.0;
        double dt_denominator = 0.0;

        // calculate LkMk and MkMk
        switch (distype)
        {
          case CORE::FE::CellType::hex8:
          {
            scatra_calc_vreman_dt(col_filtered_phi, col_filtered_phi2, col_filtered_phiexpression,
                col_filtered_alphaijsc, dt_numerator, dt_denominator, ele);
            break;
          }
          default:
          {
            FOUR_C_THROW("Unknown element type for vreman scatra application\n");
          }
          break;
        }

        elevec1_epetra(0) = dt_numerator;
        elevec1_epetra(1) = dt_denominator;
      }
      else
        FOUR_C_THROW("action 'calc_vreman_scatra' is a 3D specific action");


      break;
    }

    // calculate domain integral, i.e., surface area or volume of domain element
    case SCATRA::Action::calc_domain_integral:
    {
      CalcDomainIntegral(ele, elevec1_epetra);

      break;
    }

    // calculate normalized subgrid-diffusivity matrix
    case SCATRA::Action::calc_subgrid_diffusivity_matrix:
    {
      // calculate mass matrix and rhs
      CalcSubgrDiffMatrix(ele, elemat1_epetra);

      break;
    }

    // calculate mean Cai of multifractal subgrid-scale modeling approach
    case SCATRA::Action::calc_mean_Cai:
    {
      // get number of dofset associated with velocity related dofs
      const int ndsvel = scatrapara_->NdsVel();

      // get convective (velocity - mesh displacement) velocity at nodes
      Teuchos::RCP<const Epetra_Vector> convel =
          discretization.GetState(ndsvel, "convective velocity field");
      if (convel == Teuchos::null) FOUR_C_THROW("Cannot get state vector convective velocity");

      // determine number of velocity related dofs per node
      const int numveldofpernode = la[ndsvel].lm_.size() / nen_;

      // construct location vector for velocity related dofs
      std::vector<int> lmvel(nsd_ * nen_, -1);
      for (unsigned inode = 0; inode < nen_; ++inode)
        for (unsigned idim = 0; idim < nsd_; ++idim)
          lmvel[inode * nsd_ + idim] = la[ndsvel].lm_[inode * numveldofpernode + idim];

      // extract local values of convective velocity field from global state vector
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nsd_, nen_>>(*convel, econvelnp_, lmvel);

      // rotate the vector field in the case of rotationally symmetric boundary conditions
      rotsymmpbc_->RotateMyValuesIfNecessary(econvelnp_);

      // get phi for material parameters
      Teuchos::RCP<const Epetra_Vector> phinp = discretization.GetState("phinp");
      if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, ephinp_, lm);

      if (turbparams_->TurbModel() != INPAR::FLUID::multifractal_subgrid_scales)
        FOUR_C_THROW("Multifractal_Subgrid_Scales expected");

      double Cai = 0.0;
      double vol = 0.0;
      // calculate Cai and volume, do not include elements of potential inflow section
      if (turbparams_->AdaptCsgsPhi() and turbparams_->Nwl() and (not SCATRA::InflowElement(ele)))
      {
        // use one-point Gauss rule to do calculations at the element center
        CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToStabGaussRule<distype>::rule);
        vol = EvalShapeFuncAndDerivsAtIntPoint(intpoints, 0);

        // adopt integration points and weights for gauss point evaluation of B
        if (turbparams_->BD_Gp())
        {
          const CORE::FE::IntPointsAndWeights<nsd_ele_> gauss_intpoints(
              SCATRA::DisTypeToOptGaussRule<distype>::rule);
          intpoints = gauss_intpoints;
        }

        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
        {
          const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

          // density at t_(n)
          std::vector<double> densn(numscal_, 1.0);
          // density at t_(n+1) or t_(n+alpha_F)
          std::vector<double> densnp(numscal_, 1.0);
          // density at t_(n+alpha_M)
          std::vector<double> densam(numscal_, 1.0);

          // diffusivity / diffusivities (in case of systems) or (thermal conductivity/specific
          // heat) in case of loma

          diffmanager_ = Teuchos::rcp(new ScaTraEleDiffManager(numscal_));

          // fluid viscosity
          double visc(0.0);

          // set internal variables
          SetInternalVariablesForMatAndRHS();

          // get material
          GetMaterialParams(ele, densn, densnp, densam, visc);

          // get velocity at integration point
          CORE::LINALG::Matrix<nsd_, 1> convelint(true);
          convelint.Multiply(econvelnp_, funct_);

          // calculate characteristic element length
          double hk = CalcRefLength(vol, convelint);

          // estimate norm of strain rate
          double strainnorm = GetStrainRate(econvelnp_);
          strainnorm /= sqrt(2.0);

          // get Re from strain rate
          double Re_ele_str = strainnorm * hk * hk * densnp[0] / visc;
          if (Re_ele_str < 0.0) FOUR_C_THROW("Something went wrong!");
          // ensure positive values
          if (Re_ele_str < 1.0) Re_ele_str = 1.0;

          // calculate corrected Cai
          //           -3/16
          //  =(1 - (Re)   )
          //
          Cai += (1.0 - pow(Re_ele_str, -3.0 / 16.0)) * fac;
        }
      }

      // hand down the Cai and volume contribution to the time integration algorithm
      params.set<double>("Cai_int", Cai);
      params.set<double>("ele_vol", vol);

      break;
    }

    // calculate dissipation introduced by stabilization and turbulence models
    case SCATRA::Action::calc_dissipation:
    {
      CalcDissipation(params, ele, discretization, la);
      break;
    }

    case SCATRA::Action::calc_mass_center_smoothingfunct:
    {
      double interface_thickness = params.get<double>("INTERFACE_THICKNESS_TPF");

      if (numscal_ > 1)
      {
        std::cout << "#############################################################################"
                     "##############################"
                  << std::endl;
        std::cout << "#                                                 WARNING:                   "
                     "                             #"
                  << std::endl;
        std::cout << "# More scalars than the levelset are transported. Mass center calculations "
                     "have NOT been tested for this. #"
                  << std::endl;
        std::cout << "#                                                                            "
                     "                             # "
                  << std::endl;
        std::cout << "#############################################################################"
                     "##############################"
                  << std::endl;
      }
      // NOTE: add integral values only for elements which are NOT ghosted!
      if (ele->Owner() == discretization.Comm().MyPID())
      {
        // need current scalar vector
        // -> extract local values from the global vectors
        Teuchos::RCP<const Epetra_Vector> phinp = discretization.GetState("phinp");
        if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
        CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, ephinp_, lm);

        // calculate momentum vector and volume for element.
        CalculateMomentumAndVolume(ele, elevec1_epetra, interface_thickness);
      }
      break;
    }

    case SCATRA::Action::calc_error:
    {
      // check if length suffices
      if (elevec1_epetra.length() < 1) FOUR_C_THROW("Result vector too short");

      // need current solution
      Teuchos::RCP<const Epetra_Vector> phinp = discretization.GetState("phinp");
      if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, ephinp_, lm);

      CalErrorComparedToAnalytSolution(ele, params, elevec1_epetra);

      break;
    }

    case SCATRA::Action::calc_immersed_element_source:
    {
      int scalartoprovidwithsource = 0;
      double segregationconst = params.get<double>("segregation_constant");

      // assembly
      for (unsigned inode = 0; inode < nen_; inode++)
      {
        const int fvi = inode * numdofpernode_ + scalartoprovidwithsource;
        elevec1_epetra[fvi] += segregationconst;
      }

      break;
    }

    case SCATRA::Action::micro_scale_initialize:
    {
      if (ele->Material()->MaterialType() == CORE::Materials::m_scatra_multiscale)
      {
        const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToOptGaussRule<distype>::rule);

        // loop over all Gauss points
        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
        {
          // initialize micro scale in multi-scale simulations
          Teuchos::rcp_static_cast<MAT::ScatraMultiScale>(ele->Material())
              ->Initialize(ele->Id(), iquad, scatrapara_->IsAle());
        }
      }

      break;
    }

    case SCATRA::Action::micro_scale_prepare_time_step:
    case SCATRA::Action::micro_scale_solve:
    {
      if (ele->Material()->MaterialType() == CORE::Materials::m_scatra_multiscale)
      {
        // extract state variables at element nodes
        CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(
            *discretization.GetState("phinp"), ephinp_, lm);

        const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToOptGaussRule<distype>::rule);

        // loop over all Gauss points
        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
        {
          // evaluate shape functions at Gauss point
          EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

          // evaluate state variables at Gauss point
          SetInternalVariablesForMatAndRHS();

          if (action == SCATRA::Action::micro_scale_prepare_time_step)
          {
            // prepare time step on micro scale
            Teuchos::rcp_static_cast<MAT::ScatraMultiScale>(ele->Material())
                ->PrepareTimeStep(iquad, std::vector<double>(1, scatravarmanager_->Phinp(0)));
          }
          else
          {
            const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
                SCATRA::DisTypeToOptGaussRule<distype>::rule);

            const double detF = EvalDetFAtIntPoint(ele, intpoints, iquad);

            // solve micro scale
            std::vector<double> dummy(1, 0.);
            Teuchos::rcp_static_cast<MAT::ScatraMultiScale>(ele->Material())
                ->Evaluate(iquad, std::vector<double>(1, scatravarmanager_->Phinp(0)), dummy[0],
                    dummy, detF);
          }
        }
      }

      break;
    }

    case SCATRA::Action::micro_scale_update:
    {
      if (ele->Material()->MaterialType() == CORE::Materials::m_scatra_multiscale)
      {
        const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToOptGaussRule<distype>::rule);

        // loop over all Gauss points
        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
          // update multi-scale scalar transport material
          Teuchos::rcp_static_cast<MAT::ScatraMultiScale>(ele->Material())->Update(iquad);
      }

      break;
    }

    case SCATRA::Action::micro_scale_output:
    {
      if (ele->Material()->MaterialType() == CORE::Materials::m_scatra_multiscale)
      {
        const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToOptGaussRule<distype>::rule);

        // loop over all Gauss points
        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
          // create output on micro scale
          Teuchos::rcp_static_cast<MAT::ScatraMultiScale>(ele->Material())->Output(iquad);
      }

      break;
    }

    case SCATRA::Action::micro_scale_read_restart:
    {
      if (ele->Material()->MaterialType() == CORE::Materials::m_scatra_multiscale)
      {
        const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToOptGaussRule<distype>::rule);

        // loop over all Gauss points
        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
          // read restart on micro scale
          Teuchos::rcp_dynamic_cast<MAT::ScatraMultiScale>(ele->Material())->ReadRestart(iquad);
      }

      break;
    }

    case SCATRA::Action::micro_scale_set_time:
    {
      if (ele->Material()->MaterialType() == CORE::Materials::m_scatra_multiscale)
      {
        const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
            SCATRA::DisTypeToOptGaussRule<distype>::rule);

        // loop over all Gauss points
        for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
        {
          Teuchos::rcp_dynamic_cast<MAT::ScatraMultiScale>(ele->Material())
              ->SetTimeStepping(iquad, params.get<double>("dt"), params.get<double>("time"),
                  params.get<int>("step"));
        }
      }

      break;
    }

    case SCATRA::Action::calc_heteroreac_mat_and_rhs:
    {
      //--------------------------------------------------------------------------------
      // extract element based or nodal values
      //--------------------------------------------------------------------------------
      ExtractElementAndNodeValues(ele, params, discretization, la);

      for (int idof = 0; idof < numdofpernode_; idof++)
      {
        // no bodyforce
        bodyforce_[idof].Clear();
      }

      CalcHeteroReacMatAndRHS(ele, elemat1_epetra, elevec1_epetra);

      break;
    }

    case SCATRA::Action::transform_real_to_reference_point:
    {
      // init quantities
      CORE::LINALG::Matrix<nsd_, 1> x_real;
      for (unsigned int d = 0; d < nsd_; ++d) x_real(d, 0) = params.get<double*>("point")[d];
      xsi_(0, 0) = 0.0;
      for (unsigned int d = 1; d < nsd_; ++d) xsi_(d, 0) = 0.0;
      int count = 0;
      CORE::LINALG::Matrix<nsd_, 1> diff;

      // do the Newton loop
      bool inside = true;
      do
      {
        count++;
        EvalShapeFuncAndDerivsInParameterSpace();
        CORE::LINALG::Matrix<nsd_, 1> x_eval;
        for (unsigned int d = 0; d < nsd_; ++d)
        {
          for (unsigned int n = 0; n < nen_; ++n) x_eval(d, 0) += funct_(n, 0) * xyze_(d, n);
          x_eval(d, 0) -= x_real(d, 0);
        }
        diff.MultiplyTN(xij_, x_eval);

        for (unsigned int d = 0; d < nsd_; ++d)
        {
          xsi_(d, 0) -= diff(d, 0);
          if (xsi_(d, 0) > 10.0 || xsi_(d, 0) < -10.0) inside = false;
        }
      } while (count < 20 && diff.Norm1() > 1.0e-10 && inside);

      inside = true;
      for (unsigned int d = 0; d < nsd_; ++d)
        if (xsi_(d, 0) > 1.0 || xsi_(d, 0) < -1.0) inside = false;

      double pointarr[nsd_];
      if (!inside)
      {
        for (unsigned int d = 0; d < nsd_; ++d) pointarr[d] = -123.0;
      }
      else
      {
        for (unsigned int d = 0; d < nsd_; ++d) pointarr[d] = xsi_(d, 0);
      }
      params.set<double*>("point", pointarr);
      params.set<bool>("inside", inside);

      break;
    }

    case SCATRA::Action::evaluate_field_in_point:
    {
      for (unsigned int d = 0; d < nsd_; ++d) xsi_(d, 0) = params.get<double*>("point")[d];

      EvalShapeFuncAndDerivsInParameterSpace();

      Teuchos::RCP<const Epetra_Vector> phinp = discretization.GetState("phinp");
      if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, ephinp_, lm);

      if (params.get<int>("numscal") > numscal_)
        FOUR_C_THROW(
            "you requested the pointvalue of the %d-th scalar but there is only %d scalars",
            params.get<int>("numscal"), numscal_);

      const double value = funct_.Dot(ephinp_[params.get<int>("numscal")]);

      params.set<double>("value", value);

      break;
    }

    default:
    {
      FOUR_C_THROW("Not acting on this action. Forgot implementation?");
      break;
    }
  }  // switch(action)

  return 0;
}


/*----------------------------------------------------------------------*
 | evaluate service routine                                  fang 02/15 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
int DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::EvaluateService(DRT::Element* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    DRT::Element::LocationArray& la, CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
    CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
    CORE::LINALG::SerialDenseVector& elevec1_epetra,
    CORE::LINALG::SerialDenseVector& elevec2_epetra,
    CORE::LINALG::SerialDenseVector& elevec3_epetra)
{
  // setup
  if (SetupCalc(ele, discretization) == -1) return 0;

  // check for the action parameter
  const auto action = Teuchos::getIntegralValue<SCATRA::Action>(params, "action");

  if (scatrapara_->IsAle() and action != SCATRA::Action::micro_scale_read_restart)
  {
    // get number of dofset associated with displacement related dofs
    const int ndsdisp = scatrapara_->NdsDisp();

    Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState(ndsdisp, "dispnp");
    if (dispnp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'dispnp'");

    // determine number of displacement related dofs per node
    const int numdispdofpernode = la[ndsdisp].lm_.size() / nen_;

    // construct location vector for displacement related dofs
    std::vector<int> lmdisp(nsd_ * nen_, -1);
    for (unsigned inode = 0; inode < nen_; ++inode)
      for (unsigned idim = 0; idim < nsd_; ++idim)
        lmdisp[inode * nsd_ + idim] = la[ndsdisp].lm_[inode * numdispdofpernode + idim];

    // extract local values of displacement field from global state vector
    CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nsd_, nen_>>(*dispnp, edispnp_, lmdisp);

    // add nodal displacements to point coordinates
    UpdateNodeCoordinates();
  }
  else
    edispnp_.Clear();

  // evaluate action
  EvaluateAction(ele, params, discretization, action, la, elemat1_epetra, elemat2_epetra,
      elevec1_epetra, elevec2_epetra, elevec3_epetra);

  return 0;
}

/*---------------------------------------------------------------------*
 | calculate filtered fields for turbulent Prandtl number   fang 02/15 |
 *---------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalcBoxFilter(DRT::Element* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    DRT::Element::LocationArray& la)
{
  // extract scalar values from global vector
  Teuchos::RCP<const Epetra_Vector> scalar = discretization.GetState("scalar");
  if (scalar == Teuchos::null) FOUR_C_THROW("Cannot get scalar!");
  CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*scalar, ephinp_, la[0].lm_);

  // get number of dofset associated with velocity related dofs
  const int ndsvel = scatrapara_->NdsVel();

  // get convective (velocity - mesh displacement) velocity at nodes
  Teuchos::RCP<const Epetra_Vector> convel =
      discretization.GetState(ndsvel, "convective velocity field");
  if (convel == Teuchos::null) FOUR_C_THROW("Cannot get state vector convective velocity");

  // determine number of velocity related dofs per node
  const int numveldofpernode = la[ndsvel].lm_.size() / nen_;

  // construct location vector for velocity related dofs
  std::vector<int> lmvel(nsd_ * nen_, -1);
  for (unsigned inode = 0; inode < nen_; ++inode)
    for (unsigned idim = 0; idim < nsd_; ++idim)
      lmvel[inode * nsd_ + idim] = la[ndsvel].lm_[inode * numveldofpernode + idim];

  // extract local values of convective velocity field from global state vector
  CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nsd_, nen_>>(*convel, evelnp_, lmvel);

  // rotate the vector field in the case of rotationally symmetric boundary conditions
  rotsymmpbc_->RotateMyValuesIfNecessary(evelnp_);

  // initialize the contribution of this element to the patch volume to zero
  double volume_contribution = 0.0;
  // initialize the contributions of this element to the filtered scalar quantities
  double dens_hat = 0.0;
  double temp_hat = 0.0;
  double dens_temp_hat = 0.0;
  double phi2_hat = 0.0;
  double phiexpression_hat = 0.0;
  // get pointers for vector quantities
  Teuchos::RCP<std::vector<double>> vel_hat =
      params.get<Teuchos::RCP<std::vector<double>>>("vel_hat");
  Teuchos::RCP<std::vector<double>> densvel_hat =
      params.get<Teuchos::RCP<std::vector<double>>>("densvel_hat");
  Teuchos::RCP<std::vector<double>> densveltemp_hat =
      params.get<Teuchos::RCP<std::vector<double>>>("densveltemp_hat");
  Teuchos::RCP<std::vector<double>> densstraintemp_hat =
      params.get<Teuchos::RCP<std::vector<double>>>("densstraintemp_hat");
  Teuchos::RCP<std::vector<double>> phi_hat =
      params.get<Teuchos::RCP<std::vector<double>>>("phi_hat");
  Teuchos::RCP<std::vector<std::vector<double>>> alphaijsc_hat =
      params.get<Teuchos::RCP<std::vector<std::vector<double>>>>("alphaijsc_hat");
  // integrate the convolution with the box filter function for this element
  // the results are assembled onto the *_hat arrays
  switch (distype)
  {
    case CORE::FE::CellType::hex8:
    {
      scatra_apply_box_filter(dens_hat, temp_hat, dens_temp_hat, phi2_hat, phiexpression_hat,
          vel_hat, densvel_hat, densveltemp_hat, densstraintemp_hat, phi_hat, alphaijsc_hat,
          volume_contribution, ele, params);

      break;
    }
    default:
    {
      FOUR_C_THROW("Unknown element type for box filter application\n");
      break;
    }
  }

  // hand down the volume contribution to the time integration algorithm
  params.set<double>("volume_contribution", volume_contribution);
  // as well as the filtered scalar quantities
  params.set<double>("dens_hat", dens_hat);
  params.set<double>("temp_hat", temp_hat);
  params.set<double>("dens_temp_hat", dens_temp_hat);
  params.set<double>("phi2_hat", phi2_hat);
  params.set<double>("phiexpression_hat", phiexpression_hat);
}  // DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::CalcBoxFilter


/*-----------------------------------------------------------------------------*
 | calculate mass matrix + rhs for initial time derivative calc.     gjb 03/12 |
 *-----------------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalcInitialTimeDerivative(
    DRT::Element* ele,                      //!< current element
    CORE::LINALG::SerialDenseMatrix& emat,  //!< element matrix
    CORE::LINALG::SerialDenseVector& erhs,  //!< element residual
    Teuchos::ParameterList& params,         //!< parameter list
    DRT::Discretization& discretization,    //!< discretization
    DRT::Element::LocationArray& la         //!< location array
)
{
  // extract relevant quantities from discretization and parameter list
  ExtractElementAndNodeValues(ele, params, discretization, la);

  //----------------------------------------------------------------------
  // calculation of element volume both for tau at ele. cent. and int. pt.
  //----------------------------------------------------------------------
  // use one-point Gauss rule to do calculations at the element center
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints_tau(
      SCATRA::DisTypeToStabGaussRule<distype>::rule);

  // volume of the element (2D: element surface area; 1D: element length)
  // (Integration of f(x) = 1 gives exactly the volume/surface/length of element)
  const double vol = EvalShapeFuncAndDerivsAtIntPoint(intpoints_tau, 0);

  //------------------------------------------------------------------------------------
  // get material parameters and stabilization parameters (evaluation at element center)
  //------------------------------------------------------------------------------------
  // density at t_(n)
  std::vector<double> densn(numscal_, 1.0);
  // density at t_(n+1) or t_(n+alpha_F)
  std::vector<double> densnp(numscal_, 1.0);
  // density at t_(n+alpha_M)
  std::vector<double> densam(numscal_, 1.0);

  // fluid viscosity
  double visc(0.0);

  // the stabilisation parameters (one per transported scalar)
  std::vector<double> tau(numscal_, 0.0);

  if (not scatrapara_->MatGP() or not scatrapara_->TauGP())
  {
    SetInternalVariablesForMatAndRHS();

    GetMaterialParams(ele, densn, densnp, densam, visc);

    if (not scatrapara_->TauGP())
    {
      for (int k = 0; k < numscal_; ++k)  // loop of each transported scalar
      {
        // calculation of stabilization parameter at element center
        CalcTau(tau[k], diffmanager_->GetIsotropicDiff(k),
            reamanager_->GetStabilizationCoeff(k, scatravarmanager_->Phinp(k)), densnp[k],
            scatravarmanager_->ConVel(k), vol);
      }
    }
  }

  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  /*----------------------------------------------------------------------*/
  // element integration loop
  /*----------------------------------------------------------------------*/
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    SetInternalVariablesForMatAndRHS();

    //----------------------------------------------------------------------
    // get material parameters (evaluation at integration point)
    //----------------------------------------------------------------------
    if (scatrapara_->MatGP()) GetMaterialParams(ele, densn, densnp, densam, visc, iquad);

    //------------ get values of variables at integration point
    for (int k = 0; k < numscal_; ++k)  // deal with a system of transported scalars
    {
      // get phi at integration point for all scalars
      const double& phiint = scatravarmanager_->Phinp(k);

      // convective part in convective form: rho*u_x*N,x+ rho*u_y*N,y
      CORE::LINALG::Matrix<nen_, 1> conv = scatravarmanager_->Conv(k);

      // velocity divergence required for conservative form
      double vdiv(0.0);
      if (scatrapara_->IsConservative()) GetDivergence(vdiv, evelnp_);

      // diffusive part used in stabilization terms
      CORE::LINALG::Matrix<nen_, 1> diff(true);
      // diffusive term using current scalar value for higher-order elements
      if (use2ndderiv_)
      {
        // diffusive part:  diffus * ( N,xx  +  N,yy +  N,zz )
        GetLaplacianStrongForm(diff);
        diff.Scale(diffmanager_->GetIsotropicDiff(k));
      }

      // calculation of stabilization parameter at integration point
      if (scatrapara_->TauGP())
      {
        CalcTau(tau[k], diffmanager_->GetIsotropicDiff(k),
            reamanager_->GetStabilizationCoeff(k, scatravarmanager_->Phinp(k)), densnp[k],
            scatravarmanager_->ConVel(k), vol);
      }

      const double fac_tau = fac * tau[k];

      //----------------------------------------------------------------
      // element matrix: transient term
      //----------------------------------------------------------------
      // transient term
      CalcMatMass(emat, k, fac, densam[k]);

      //----------------------------------------------------------------
      // element matrix: stabilization of transient term
      //----------------------------------------------------------------
      // the stabilization term is deactivated in CalcInitialTimeDerivative() on time integrator
      // level
      if (scatrapara_->StabType() != INPAR::SCATRA::stabtype_no_stabilization)
      {
        // subgrid-scale velocity (dummy)
        CORE::LINALG::Matrix<nen_, 1> sgconv(true);
        CalcMatMassStab(emat, k, fac_tau, densam[k], densnp[k], sgconv, diff);

        // remove convective stabilization of inertia term
        for (unsigned vi = 0; vi < nen_; ++vi)
        {
          const int fvi = vi * numdofpernode_ + k;
          erhs(fvi) += fac_tau * densnp[k] * conv(vi) * densnp[k] * phiint;
        }
      }

      // we solve: (w,dc/dt) = rhs
      // whereas the rhs is based on the standard element evaluation routine
      // including contributions resulting from the time discretization.
      // The contribution from the time discretization has to be removed before solving the system:
      CorrectRHSFromCalcRHSLinMass(erhs, k, fac, densnp[k], phiint);
    }  // loop over each scalar k
  }    // integration loop

  // scale element matrix appropriately to be consistent with scaling of global residual vector
  // computed by AssembleMatAndRHS() routine (see CalcInitialTimeDerivative() routine on time
  // integrator level)
  emat.scale(scatraparatimint_->TimeFacRhs());
}  // ScaTraEleCalc::CalcInitialTimeDerivative()


/*----------------------------------------------------------------------*
 |  CorrectRHSFromCalcRHSLinMass                             ehrl 06/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CorrectRHSFromCalcRHSLinMass(
    CORE::LINALG::SerialDenseVector& erhs, const int k, const double fac, const double densnp,
    const double phinp)
{
  // fac->-fac to change sign of rhs
  if (scatraparatimint_->IsIncremental())
    CalcRHSLinMass(erhs, k, 0.0, -fac, 0.0, densnp);
  else
    FOUR_C_THROW("Must be incremental!");
}


/*----------------------------------------------------------------------*
 |  Integrate shape functions over domain (private)           gjb 07/09 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::IntegrateShapeFunctions(
    const DRT::Element* ele, CORE::LINALG::SerialDenseVector& elevec1,
    const CORE::LINALG::IntSerialDenseVector& dofids)
{
  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // safety check
  if (dofids.numRows() < numdofpernode_)
    FOUR_C_THROW("Dofids vector is too short. Received not enough flags");

  // loop over integration points
  // this order is not efficient since the integration of the shape functions is always the same for
  // all species
  for (int gpid = 0; gpid < intpoints.IP().nquad; gpid++)
  {
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, gpid);

    // compute integral of shape functions (only for dofid)
    for (int k = 0; k < numdofpernode_; k++)
    {
      if (dofids[k] >= 0)
      {
        for (unsigned node = 0; node < nen_; node++)
        {
          elevec1[node * numdofpernode_ + k] += funct_(node) * fac;
        }
      }
    }

  }  // loop over integration points
}  // ScaTraEleCalc::IntegrateShapeFunction

/*----------------------------------------------------------------------*
  |  calculate weighted mass flux (no reactive flux so far)     gjb 06/08|
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalculateFlux(
    CORE::LINALG::Matrix<3, nen_>& flux, const DRT::Element* ele,
    const INPAR::SCATRA::FluxType fluxtype, const int k)
{
  /*
  * Actually, we compute here a weighted (and integrated) form of the fluxes!
  * On time integration level, these contributions are then used to calculate
  * an L2-projected representation of fluxes.
  * Thus, this method here DOES NOT YET provide flux values that are ready to use!!
  /                                                         \
  |                /   \                               /   \  |
  | w, -D * nabla | phi | + u*phi - frt*z_k*c_k*nabla | pot | |
  |                \   /                               \   /  |
  \                      [optional]      [ELCH]               /
  */

  // density at t_(n)
  std::vector<double> densn(numscal_, 1.0);
  // density at t_(n+1) or t_(n+alpha_F)
  std::vector<double> densnp(numscal_, 1.0);
  // density at t_(n+alpha_M)
  std::vector<double> densam(numscal_, 1.0);

  // fluid viscosity
  double visc(0.0);

  // get material parameters (evaluation at element center)
  if (not scatrapara_->MatGP())
  {
    SetInternalVariablesForMatAndRHS();

    GetMaterialParams(ele, densn, densnp, densam, visc);
  }

  // integration rule
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // integration loop
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    // evaluate shape functions and derivatives at integration point
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    SetInternalVariablesForMatAndRHS();

    // get material parameters (evaluation at integration point)
    if (scatrapara_->MatGP()) GetMaterialParams(ele, densn, densnp, densam, visc);

    // get velocity at integration point
    CORE::LINALG::Matrix<nsd_, 1> velint(true);
    CORE::LINALG::Matrix<nsd_, 1> convelint(true);
    velint.Multiply(evelnp_, funct_);
    convelint.Multiply(econvelnp_, funct_);

    // get gradient of scalar at integration point
    CORE::LINALG::Matrix<nsd_, 1> gradphi(true);
    gradphi.Multiply(derxy_, ephinp_[k]);

    // allocate and initialize!
    CORE::LINALG::Matrix<nsd_, 1> q(true);

    // add different flux contributions as specified by user input
    switch (fluxtype)
    {
      case INPAR::SCATRA::flux_total:
        // convective flux contribution
        q.Update(densnp[k] * scatravarmanager_->Phinp(k), convelint);

        [[fallthrough]];
      case INPAR::SCATRA::flux_diffusive:
        // diffusive flux contribution
        q.Update(-(diffmanager_->GetIsotropicDiff(k)), gradphi, 1.0);

        break;
      default:
        FOUR_C_THROW("received illegal flag inside flux evaluation for whole domain");
        break;
    }
    // q at integration point

    // integrate and assemble everything into the "flux" vector
    for (unsigned vi = 0; vi < nen_; vi++)
    {
      for (unsigned idim = 0; idim < nsd_; idim++)
      {
        flux(idim, vi) += fac * funct_(vi) * q(idim);
      }  // idim
    }    // vi

  }  // integration loop

  // set zeros for unused space dimensions
  for (unsigned idim = nsd_; idim < 3; idim++)
  {
    for (unsigned vi = 0; vi < nen_; vi++)
    {
      flux(idim, vi) = 0.0;
    }
  }
}  // ScaTraCalc::CalculateFlux


/*----------------------------------------------------------------------------------------*
 | calculate domain integral, i.e., surface area or volume of domain element   fang 07/15 |
 *----------------------------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalcDomainIntegral(
    const DRT::Element* ele,                 //!< the element we are dealing with
    CORE::LINALG::SerialDenseVector& scalar  //!< result vector for scalar integral to be computed
)
{
  // initialize variable for domain integral
  double domainintegral(0.);

  // get integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // loop over integration points
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    // evaluate values of shape functions and domain integration factor at current integration point
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    // add contribution from current integration point to domain integral
    for (unsigned vi = 0; vi < nen_; ++vi) domainintegral += funct_(vi) * fac;
  }  // loop over integration points

  // write result into result vector
  scalar(0) = domainintegral;
}  // DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::CalcDomainIntegral


/*----------------------------------------------------------------------*
|  calculate scalar(s) and domain integral                     vg 09/08|
*----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalculateScalars(const DRT::Element* ele,
    CORE::LINALG::SerialDenseVector& scalars, const bool inverting, const bool calc_grad_phi)
{
  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // integration loop
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    // evaluate everything on current GP
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    for (int k = 0; k < numdofpernode_; k++)
    {
      // evaluate 1.0/phi if needed
      CORE::LINALG::Matrix<nen_, 1> inv_ephinp(true);
      if (inverting)
      {
        for (unsigned i = 0; i < nen_; i++)
        {
          const double inv_value = 1.0 / ephinp_[k](i);
          if (std::abs(inv_value) < 1e-14) FOUR_C_THROW("Division by zero");
          inv_ephinp(i) = inv_value;
        }
      }

      // project phi or 1.0/phi to current GP and multiply with domain integration factor
      const double phi_gp = funct_.Dot(inverting ? inv_ephinp : ephinp_[k]);
      scalars[k] += phi_gp * fac;
    }
    scalars[numdofpernode_] += fac;

    if (calc_grad_phi)
    {
      DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::SetInternalVariablesForMatAndRHS();

      for (int k = 0; k < numscal_; k++)
      {
        const double gradphi_l2norm_gp = scatravarmanager_->GradPhi()[k].Norm2();
        scalars[numdofpernode_ + 1 + k] += gradphi_l2norm_gp * fac;
      }
    }
  }
}


/*----------------------------------------------------------------------*
 | calculate scalar time derivative(s) and domain integral   fang 03/18 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalculateScalarTimeDerivatives(
    const DRT::Discretization& discretization,  //!< discretization
    const std::vector<int>& lm,                 //!< location vector
    CORE::LINALG::SerialDenseVector& scalars  //!< result vector for scalar integrals to be computed
)
{
  // extract scalar time derivatives from global state vector
  const Teuchos::RCP<const Epetra_Vector> phidtnp = discretization.GetState("phidtnp");
  if (phidtnp == Teuchos::null) FOUR_C_THROW("Cannot get state vector \"phidtnp\"!");
  static std::vector<CORE::LINALG::Matrix<nen_, 1>> ephidtnp(numscal_);
  CORE::FE::ExtractMyValues(*phidtnp, ephidtnp, lm);

  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // loop over integration points
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    // evaluate values of shape functions and domain integration factor at current integration point
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    // calculate integrals of scalar time derivatives
    for (unsigned vi = 0; vi < nen_; ++vi)
    {
      const double fac_funct_vi = fac * funct_(vi);

      for (int k = 0; k < numscal_; ++k) scalars(k) += fac_funct_vi * ephidtnp[k](vi);
    }

    // calculate integral of domain
    scalars(numscal_) += fac;
  }  // loop over integration points
}  // DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::CalculateScalarTimeDerivatives


/*----------------------------------------------------------------------*
|  calculate momentum vector and minus domain integral          mw 06/14|
*----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalculateMomentumAndVolume(
    const DRT::Element* ele, CORE::LINALG::SerialDenseVector& momandvol,
    const double interface_thickness)
{
  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // integration loop
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    // coordinates of the current integration point.
    std::vector<double> gpcoord(nsd_);
    // levelset function at gaussian point.
    double ephi_gp = 0.0;
    // fac*funct at GP
    double fac_funct = 0.0;

    for (unsigned i = 0; i < nen_; i++)
    {
      // Levelset function (first scalar stored [0]) at gauss point
      ephi_gp += funct_(i) * ephinp_[0](i, 0);

      // Coordinate * shapefunction to get the coordinate value of the gausspoint.
      for (unsigned idim = 0; idim < nsd_; idim++)
      {
        gpcoord[idim] += funct_(i) * (ele->Nodes()[i]->X()[idim]);
      }

      // Summation of fac*funct_ for volume calculation.
      fac_funct += fac * funct_(i);
    }

    double heavyside_epsilon = 1.0;  // plus side

    // Smoothing function
    if (abs(ephi_gp) <= interface_thickness)
    {
      heavyside_epsilon = 0.5 * (1.0 + ephi_gp / interface_thickness +
                                    1.0 / M_PI * sin(M_PI * ephi_gp / interface_thickness));
    }
    else if (ephi_gp < interface_thickness)
    {
      heavyside_epsilon = 0.0;  // minus side
    }


    // add momentum vector and volume
    for (unsigned idim = 0; idim < nsd_; idim++)
    {
      momandvol(idim) += gpcoord[idim] * (1.0 - heavyside_epsilon) * fac_funct;
    }

    momandvol(nsd_) += fac_funct * (1.0 - heavyside_epsilon);

  }  // loop over integration points
}  // ScaTraEleCalc::CalculateMomentumAndVolume


/*----------------------------------------------------------------------*
 | calculate normalized subgrid-diffusivity matrix              vg 10/08|
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalcSubgrDiffMatrix(
    const DRT::Element* ele, CORE::LINALG::SerialDenseMatrix& emat)
{
  /*----------------------------------------------------------------------*/
  // integration loop for one element
  /*----------------------------------------------------------------------*/
  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // integration loop
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    for (int k = 0; k < numscal_; ++k)
    {
      // set diffusion coeff to 1.0
      diffmanager_->SetIsotropicDiff(1.0, k);

      // calculation of diffusive element matrix
      double timefacfac = scatraparatimint_->TimeFac() * fac;
      CalcMatDiff(emat, k, timefacfac);

      /*subtract SUPG term */
      // emat(fvi,fui) -= taufac*conv(vi)*conv(ui);
    }
  }  // integration loop
}  // ScaTraImpl::CalcSubgrDiffMatrix


/*----------------------------------------------------------------------------------------*
 | finite difference check on element level (for debugging only) (protected)   fang 10/14 |
 *----------------------------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::FDCheck(DRT::Element* ele,
    CORE::LINALG::SerialDenseMatrix& emat, CORE::LINALG::SerialDenseVector& erhs,
    CORE::LINALG::SerialDenseVector& subgrdiff)
{
  // screen output
  std::cout << "FINITE DIFFERENCE CHECK FOR ELEMENT " << ele->Id();

  // make a copy of state variables to undo perturbations later
  std::vector<CORE::LINALG::Matrix<nen_, 1>> ephinp_original(numscal_);
  for (int k = 0; k < numscal_; ++k)
    for (unsigned i = 0; i < nen_; ++i) ephinp_original[k](i, 0) = ephinp_[k](i, 0);

  // generalized-alpha time integration requires a copy of history variables as well
  std::vector<CORE::LINALG::Matrix<nen_, 1>> ehist_original(numscal_);
  if (scatraparatimint_->IsGenAlpha())
  {
    for (int k = 0; k < numscal_; ++k)
      for (unsigned i = 0; i < nen_; ++i) ehist_original[k](i, 0) = ehist_[k](i, 0);
  }

  // initialize element matrix and vectors for perturbed state
  CORE::LINALG::SerialDenseMatrix emat_dummy(emat);
  CORE::LINALG::SerialDenseVector erhs_perturbed(erhs);
  CORE::LINALG::SerialDenseVector subgrdiff_dummy(subgrdiff);

  // initialize counter for failed finite difference checks
  unsigned counter(0);

  // initialize tracking variable for maximum absolute and relative errors
  double maxabserr(0.);
  double maxrelerr(0.);

  // loop over columns of element matrix by first looping over nodes and then over dofs at each node
  for (unsigned inode = 0; inode < nen_; ++inode)
  {
    for (int idof = 0; idof < numdofpernode_; ++idof)
    {
      // number of current column of element matrix
      unsigned col = inode * numdofpernode_ + idof;

      // clear element matrix and vectors for perturbed state
      emat_dummy.putScalar(0.0);
      erhs_perturbed.putScalar(0.0);
      subgrdiff_dummy.putScalar(0.0);

      // fill state vectors with original state variables
      for (int k = 0; k < numscal_; ++k)
        for (unsigned i = 0; i < nen_; ++i) ephinp_[k](i, 0) = ephinp_original[k](i, 0);
      if (scatraparatimint_->IsGenAlpha())
        for (int k = 0; k < numscal_; ++k)
          for (unsigned i = 0; i < nen_; ++i) ehist_[k](i, 0) = ehist_original[k](i, 0);

      // impose perturbation
      if (scatraparatimint_->IsGenAlpha())
      {
        // perturbation of phi(n+alphaF), not of phi(n+1) => scale epsilon by factor alphaF
        ephinp_[idof](inode, 0) += scatraparatimint_->AlphaF() * scatrapara_->FDCheckEps();

        // perturbation of phi(n+alphaF) by alphaF*epsilon corresponds to perturbation of phidtam
        // (stored in ehist_) by alphaM*epsilon/(gamma*dt); note: alphaF/timefac = alphaM/(gamma*dt)
        ehist_[idof](inode, 0) +=
            scatraparatimint_->AlphaF() / scatraparatimint_->TimeFac() * scatrapara_->FDCheckEps();
      }
      else
        ephinp_[idof](inode, 0) += scatrapara_->FDCheckEps();

      // calculate element right-hand side vector for perturbed state
      Sysmat(ele, emat_dummy, erhs_perturbed, subgrdiff_dummy);

      // Now we compare the difference between the current entries in the element matrix
      // and their finite difference approximations according to
      // entries ?= (-erhs_perturbed + erhs_original) / epsilon

      // Note that the element right-hand side equals the negative element residual.
      // To account for errors due to numerical cancellation, we additionally consider
      // entries - erhs_original / epsilon ?= -erhs_perturbed / epsilon

      // Note that we still need to evaluate the first comparison as well. For small entries in the
      // element matrix, the second comparison might yield good agreement in spite of the entries
      // being wrong!
      for (unsigned row = 0; row < static_cast<unsigned>(numdofpernode_ * nen_); ++row)
      {
        // get current entry in original element matrix
        const double entry = emat(row, col);

        // finite difference suggestion (first divide by epsilon and then subtract for better
        // conditioning)
        const double fdval = -erhs_perturbed(row) / scatrapara_->FDCheckEps() +
                             erhs(row) / scatrapara_->FDCheckEps();

        // confirm accuracy of first comparison
        if (abs(fdval) > 1.e-17 and abs(fdval) < 1.e-15)
          FOUR_C_THROW("Finite difference check involves values too close to numerical zero!");

        // absolute and relative errors in first comparison
        const double abserr1 = entry - fdval;
        if (abs(abserr1) > abs(maxabserr)) maxabserr = abserr1;
        double relerr1(0.);
        if (abs(entry) > 1.e-17)
          relerr1 = abserr1 / abs(entry);
        else if (abs(fdval) > 1.e-17)
          relerr1 = abserr1 / abs(fdval);
        if (abs(relerr1) > abs(maxrelerr)) maxrelerr = relerr1;

        // evaluate first comparison
        if (abs(relerr1) > scatrapara_->FDCheckTol())
        {
          if (!counter) std::cout << " --> FAILED AS FOLLOWS:" << std::endl;
          std::cout << "emat[" << row << "," << col << "]:  " << entry << "   ";
          std::cout << "finite difference suggestion:  " << fdval << "   ";
          std::cout << "absolute error:  " << abserr1 << "   ";
          std::cout << "relative error:  " << relerr1 << std::endl;

          counter++;
        }

        // first comparison OK
        else
        {
          // left-hand side in second comparison
          const double left = entry - erhs(row) / scatrapara_->FDCheckEps();

          // right-hand side in second comparison
          const double right = -erhs_perturbed(row) / scatrapara_->FDCheckEps();

          // confirm accuracy of second comparison
          if (abs(right) > 1.e-17 and abs(right) < 1.e-15)
            FOUR_C_THROW("Finite difference check involves values too close to numerical zero!");

          // absolute and relative errors in second comparison
          const double abserr2 = left - right;
          if (abs(abserr2) > abs(maxabserr)) maxabserr = abserr2;
          double relerr2(0.);
          if (abs(left) > 1.e-17)
            relerr2 = abserr2 / abs(left);
          else if (abs(right) > 1.e-17)
            relerr2 = abserr2 / abs(right);
          if (abs(relerr2) > abs(maxrelerr)) maxrelerr = relerr2;

          // evaluate second comparison
          if (abs(relerr2) > scatrapara_->FDCheckTol())
          {
            if (!counter) std::cout << " --> FAILED AS FOLLOWS:" << std::endl;
            std::cout << "emat[" << row << "," << col << "]-erhs[" << row << "]/eps:  " << left
                      << "   ";
            std::cout << "-erhs_perturbed[" << row << "]/eps:  " << right << "   ";
            std::cout << "absolute error:  " << abserr2 << "   ";
            std::cout << "relative error:  " << relerr2 << std::endl;

            counter++;
          }
        }
      }
    }
  }

  // screen output in case finite difference check is passed
  if (!counter)
    std::cout << " --> PASSED WITH MAXIMUM ABSOLUTE ERROR " << maxabserr
              << " AND MAXIMUM RELATIVE ERROR " << maxrelerr << std::endl;

  // undo perturbations of state variables
  for (int k = 0; k < numscal_; ++k)
    for (unsigned i = 0; i < nen_; ++i) ephinp_[k](i, 0) = ephinp_original[k](i, 0);
  if (scatraparatimint_->IsGenAlpha())
    for (int k = 0; k < numscal_; ++k)
      for (unsigned i = 0; i < nen_; ++i) ehist_[k](i, 0) = ehist_original[k](i, 0);
}

/*---------------------------------------------------------------------*
  |  calculate error compared to analytical solution           gjb 10/08|
  *---------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalErrorComparedToAnalytSolution(
    const DRT::Element* ele, Teuchos::ParameterList& params,
    CORE::LINALG::SerialDenseVector& errors)
{
  if (Teuchos::getIntegralValue<SCATRA::Action>(params, "action") != SCATRA::Action::calc_error)
    FOUR_C_THROW("How did you get here?");

  // -------------- prepare common things first ! -----------------------
  // set constants for analytical solution
  const double t = scatraparatimint_->Time();

  // integration points and weights
  // more GP than usual due to (possible) cos/exp fcts in analytical solutions
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToGaussRuleForExactSol<distype>::rule);

  const auto errortype = CORE::UTILS::GetAsEnum<INPAR::SCATRA::CalcError>(params, "calcerrorflag");
  switch (errortype)
  {
    case INPAR::SCATRA::calcerror_byfunction:
    {
      const int errorfunctno = params.get<int>("error function number");

      // analytical solution
      double phi_exact(0.0);
      double deltaphi(0.0);
      //! spatial gradient of current scalar value
      CORE::LINALG::Matrix<nsd_, 1> gradphi(true);
      CORE::LINALG::Matrix<nsd_, 1> gradphi_exact(true);
      CORE::LINALG::Matrix<nsd_, 1> deltagradphi(true);

      // start loop over integration points
      for (int iquad = 0; iquad < intpoints.IP().nquad; iquad++)
      {
        const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

        // get coordinates at integration point
        // gp reference coordinates
        CORE::LINALG::Matrix<nsd_, 1> xyzint(true);
        xyzint.Multiply(xyze_, funct_);

        // function evaluation requires a 3D position vector!!
        double position[3] = {0.0, 0.0, 0.0};

        for (unsigned dim = 0; dim < nsd_; ++dim) position[dim] = xyzint(dim);

        for (int k = 0; k < numdofpernode_; ++k)
        {
          // scalar at integration point at time step n+1
          const double phinp = funct_.Dot(ephinp_[k]);
          // spatial gradient of current scalar value
          gradphi.Multiply(derxy_, ephinp_[k]);

          phi_exact = GLOBAL::Problem::Instance()
                          ->FunctionById<CORE::UTILS::FunctionOfSpaceTime>(errorfunctno - 1)
                          .Evaluate(position, t, k);

          std::vector<double> gradphi_exact_vec =
              GLOBAL::Problem::Instance()
                  ->FunctionById<CORE::UTILS::FunctionOfSpaceTime>(errorfunctno - 1)
                  .EvaluateSpatialDerivative(position, t, k);

          if (gradphi_exact_vec.size())
          {
            if (nsd_ == nsd_ele_)
              for (unsigned dim = 0; dim < nsd_; ++dim) gradphi_exact(dim) = gradphi_exact_vec[dim];
            else
            {
              // std::cout<<"Warning: Gradient of analytical solution cannot be evaluated correctly
              // for transport on curved surfaces!"<<std::endl;
              gradphi_exact.Clear();
            }
          }
          else
          {
            std::cout << "Warning: Gradient of analytical solution was not evaluated!" << std::endl;
            gradphi_exact.Clear();
          }

          // error at gauss point
          deltaphi = phinp - phi_exact;
          deltagradphi.Update(1.0, gradphi, -1.0, gradphi_exact);

          // 0: delta scalar for L2-error norm
          // 1: delta scalar for H1-error norm
          // 2: analytical scalar for L2 norm
          // 3: analytical scalar for H1 norm

          // the error for the L2 and H1 norms are evaluated at the Gauss point

          // integrate delta scalar for L2-error norm
          errors(k * 4 + 0) += deltaphi * deltaphi * fac;
          // integrate delta scalar for H1-error norm
          errors(k * 4 + 1) += deltaphi * deltaphi * fac;
          // integrate analytical scalar for L2 norm
          errors(k * 4 + 2) += phi_exact * phi_exact * fac;
          // integrate analytical scalar for H1 norm
          errors(k * 4 + 3) += phi_exact * phi_exact * fac;

          // integrate delta scalar derivative for H1-error norm
          errors(k * 4 + 1) += deltagradphi.Dot(deltagradphi) * fac;
          // integrate analytical scalar derivative for H1 norm
          errors(k * 4 + 3) += gradphi_exact.Dot(gradphi_exact) * fac;
        }
      }  // loop over integration points
    }
    break;

    case INPAR::SCATRA::calcerror_spherediffusion:
    {
      // analytical solution
      double phi_exact(0.0);
      double deltaphi(0.0);
      //! spatial gradient of current scalar value
      CORE::LINALG::Matrix<nsd_, 1> gradphi(true);
      CORE::LINALG::Matrix<nsd_, 1> gradphi_exact(true);
      CORE::LINALG::Matrix<nsd_, 1> deltagradphi(true);

      // start loop over integration points
      for (int iquad = 0; iquad < intpoints.IP().nquad; iquad++)
      {
        const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

        // get coordinates at integration point
        // gp reference coordinates
        CORE::LINALG::Matrix<nsd_, 1> xyzint(true);
        xyzint.Multiply(xyze_, funct_);

        for (int k = 0; k < numscal_; k++)
        {
          const double x = xyzint(0);
          const double y = xyzint(1);
          const double z = xyzint(2);

          // scalar at integration point at time step n+1
          const double phinp = funct_.Dot(ephinp_[k]);
          // spatial gradient of current scalar value
          gradphi.Multiply(derxy_, ephinp_[k]);

          phi_exact = exp(-6 * t) * x * y + 10;

          gradphi_exact(0) = (1.0 - 2.0 * x * x) * y * exp(-6.0 * t);
          gradphi_exact(1) = (1.0 - 2.0 * y * y) * x * exp(-6.0 * t);
          gradphi_exact(2) = -2.0 * x * y * z * exp(-6.0 * t);

          // error at gauss point
          deltaphi = phinp - phi_exact;
          deltagradphi.Update(1.0, gradphi, -1.0, gradphi_exact);

          // 0: delta scalar for L2-error norm
          // 1: delta scalar for H1-error norm
          // 2: analytical scalar for L2 norm
          // 3: analytical scalar for H1 norm

          // the error for the L2 and H1 norms are evaluated at the Gauss point

          // integrate delta scalar for L2-error norm
          errors(k * numscal_ + 0) += deltaphi * deltaphi * fac;
          // integrate delta scalar for H1-error norm
          errors(k * numscal_ + 1) += deltaphi * deltaphi * fac;
          // integrate analytical scalar for L2 norm
          errors(k * numscal_ + 2) += phi_exact * phi_exact * fac;
          // integrate analytical scalar for H1 norm
          errors(k * numscal_ + 3) += phi_exact * phi_exact * fac;

          // integrate delta scalar derivative for H1-error norm
          errors(k * numscal_ + 1) += deltagradphi.Dot(deltagradphi) * fac;
          // integrate analytical scalar derivative for H1 norm
          errors(k * numscal_ + 3) += gradphi_exact.Dot(gradphi_exact) * fac;
        }
      }  // loop over integration points
    }
    break;
    default:
      FOUR_C_THROW("Unknown analytical solution!");
      break;
  }  // switch(errortype)
}  // DRT::ELEMENTS::ScaTraEleCalc<distype,probdim>::CalErrorComparedToAnalytSolution


/*----------------------------------------------------------------------*
|  calculate system matrix and rhs (public)                  vuong 07/16|
*----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::CalcHeteroReacMatAndRHS(
    DRT::Element* ele,                      ///< the element whose matrix is calculated
    CORE::LINALG::SerialDenseMatrix& emat,  ///< element matrix to calculate
    CORE::LINALG::SerialDenseVector& erhs   ///< element rhs to calculate
)
{
  //----------------------------------------------------------------------
  // calculation of element volume both for tau at ele. cent. and int. pt.
  //----------------------------------------------------------------------
  const double vol = EvalShapeFuncAndDerivsAtEleCenter();

  //----------------------------------------------------------------------
  // get material and stabilization parameters (evaluation at element center)
  //----------------------------------------------------------------------
  // density at t_(n)
  std::vector<double> densn(numscal_, 1.0);
  // density at t_(n+1) or t_(n+alpha_F)
  std::vector<double> densnp(numscal_, 1.0);
  // density at t_(n+alpha_M)
  std::vector<double> densam(numscal_, 1.0);

  // fluid viscosity
  double visc(0.0);

  // the stabilization parameters (one per transported scalar)
  std::vector<double> tau(numscal_, 0.0);

  if (not scatrapara_->TauGP())
  {
    for (int k = 0; k < numscal_; ++k)  // loop of each transported scalar
    {
      // get velocity at element center
      CORE::LINALG::Matrix<nsd_, 1> convelint = scatravarmanager_->ConVel(k);
      // calculation of stabilization parameter at element center
      CalcTau(tau[k], diffmanager_->GetIsotropicDiff(k),
          reamanager_->GetStabilizationCoeff(k, scatravarmanager_->Phinp(k)), densnp[k], convelint,
          vol);
    }
  }

  // material parameter at the element center are also necessary
  // even if the stabilization parameter is evaluated at the element center
  if (not scatrapara_->MatGP())
  {
    // set gauss point variables needed for evaluation of mat and rhs
    SetInternalVariablesForMatAndRHS();

    GetMaterialParams(ele, densn, densnp, densam, visc);
  }

  //----------------------------------------------------------------------
  // integration loop for one element
  //----------------------------------------------------------------------
  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    const double fac = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

    // set gauss point variables needed for evaluation of mat and rhs
    SetInternalVariablesForMatAndRHS();

    //----------------------------------------------------------------------
    // get material parameters (evaluation at integration point)
    //----------------------------------------------------------------------
    if (scatrapara_->MatGP()) GetMaterialParams(ele, densn, densnp, densam, visc, iquad);

    // loop all scalars
    for (int k = 0; k < numscal_; ++k)  // deal with a system of transported scalars
    {
      // reactive part of the form: (reaction coefficient)*phi
      double rea_phi(0.0);
      rea_phi = densnp[k] * scatravarmanager_->Phinp(k) * reamanager_->GetReaCoeff(k);

      // compute rhs containing bodyforce (divided by specific heat capacity) and,
      // for temperature equation, the time derivative of thermodynamic pressure,
      // if not constant, and for temperature equation of a reactive
      // equation system, the reaction-rate term
      double rhsint(0.0);
      GetRhsInt(rhsint, densnp[k], k);

      double scatrares(0.0);
      // calculate strong residual
      CalcStrongResidual(k, scatrares, densam[k], densnp[k], rea_phi, rhsint, tau[k]);

      if (scatrapara_->TauGP())
      {
        // (re)compute stabilization parameter at integration point, since diffusion may have
        // changed
        CalcTau(tau[k], diffmanager_->GetIsotropicDiff(k),
            reamanager_->GetStabilizationCoeff(k, scatravarmanager_->Phinp(k)), densnp[k],
            scatravarmanager_->ConVel(k), vol);  // TODO:(Thon) do we really have to do this??
      }

      //----------------------------------------------------------------
      // standard Galerkin terms
      //----------------------------------------------------------------

      // stabilization parameter and integration factors
      const double taufac = tau[k] * fac;
      const double timefacfac = scatraparatimint_->TimeFac() * fac;
      const double timetaufac = scatraparatimint_->TimeFac() * taufac;

      //----------------------------------------------------------------
      // 3) element matrix: reactive term
      //----------------------------------------------------------------

      CORE::LINALG::Matrix<nen_, 1> sgconv(true);
      CORE::LINALG::Matrix<nen_, 1> diff(true);
      // diffusive term using current scalar value for higher-order elements
      if (use2ndderiv_)
      {
        // diffusive part:  diffus * ( N,xx  +  N,yy +  N,zz )
        GetLaplacianStrongForm(diff);
        diff.Scale(diffmanager_->GetIsotropicDiff(k));
      }

      // including stabilization
      if (reamanager_->Active())
      {
        CalcMatReact(emat, k, timefacfac, timetaufac, taufac, densnp[k], sgconv, diff);
      }

      //----------------------------------------------------------------
      // 5) element right hand side
      //----------------------------------------------------------------
      //----------------------------------------------------------------
      // computation of bodyforce (and potentially history) term,
      // residual, integration factors and standard Galerkin transient
      // term (if required) on right hand side depending on respective
      // (non-)incremental stationary or time-integration scheme
      //----------------------------------------------------------------
      double rhsfac = scatraparatimint_->TimeFacRhs() * fac;
      double rhstaufac = scatraparatimint_->TimeFacRhsTau() * taufac;

      ComputeRhsInt(rhsint, densam[k], densnp[k], 0.0);

      RecomputeScatraResForRhs(scatrares, k, diff, densn[k], densnp[k], rea_phi, rhsint);

      //----------------------------------------------------------------
      // standard Galerkin transient, old part of rhs and bodyforce term
      //----------------------------------------------------------------
      CalcRHSHistAndSource(erhs, k, fac, rhsint);

      //----------------------------------------------------------------
      // reactive terms (standard Galerkin and stabilization) on rhs
      //----------------------------------------------------------------

      if (reamanager_->Active())
        CalcRHSReact(erhs, k, rhsfac, rhstaufac, rea_phi, densnp[k], scatrares);

    }  // end loop all scalars
  }    // end loop Gauss points
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype, int probdim>
double DRT::ELEMENTS::ScaTraEleCalc<distype, probdim>::EvalDetFAtIntPoint(
    const DRT::Element* const& ele, const CORE::FE::IntPointsAndWeights<nsd_ele_>& intpoints,
    const int iquad)
{
  // get determinant of derivative of spatial coordinate w.r.t. parameter coordinates
  const double det_dxds = EvalShapeFuncAndDerivsAtIntPoint(intpoints, iquad);

  // get derivatives of element shape function w.r.t. parameter coordinates
  CORE::LINALG::Matrix<nsd_ele_, nen_> deriv_ele;
  CORE::FE::shape_function_deriv1<distype>(xsi_, deriv_ele);

  // reference coordinates of element nodes
  CORE::LINALG::Matrix<nsd_, nen_> XYZ;
  CORE::GEO::fillInitialPositionArray<distype, nsd_, CORE::LINALG::Matrix<nsd_, nen_>>(ele, XYZ);

  // reference coordinates of elemental nodes in space dimension of element
  CORE::LINALG::Matrix<nsd_ele_, nen_> XYZe;
  for (int i = 0; i < static_cast<int>(nsd_ele_); ++i)
    for (int j = 0; j < static_cast<int>(nen_); ++j) XYZe(i, j) = XYZ(i, j);

  // compute derivative of parameter coordinates w.r.t. reference coordinates
  CORE::LINALG::Matrix<nsd_ele_, nsd_ele_> dXds;
  dXds.MultiplyNT(deriv_ele, XYZe);

  return det_dxds / dXds.Determinant();
}

FOUR_C_NAMESPACE_CLOSE

// template classes

#include "4C_scatra_ele_calc_fwd.hpp"