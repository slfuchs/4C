/*--------------------------------------------------------------------------*/
/*! \file

\brief evaluation of scatra elements for isothermal space charge layer formation

\level 2

*/
/*--------------------------------------------------------------------------*/
#include "baci_scatra_ele_calc_elch_scl.H"

#include "baci_lib_discret.H"
#include "baci_lib_utils.H"
#include "baci_mat_material.H"
#include "baci_scatra_ele_parameter_std.H"
#include "baci_scatra_ele_parameter_timint.H"
#include "baci_scatra_ele_utils_elch_scl.H"
#include "baci_utils_singleton_owner.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>*
DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::Instance(
    const int numdofpernode, const int numscal, const std::string& disname)
{
  static auto singleton_map = CORE::UTILS::MakeSingletonMap<std::string>(
      [](const int numdofpernode, const int numscal, const std::string& disname)
      {
        return std::unique_ptr<ScaTraEleCalcElchScl<distype, probdim>>(
            new ScaTraEleCalcElchScl<distype, probdim>(numdofpernode, numscal, disname));
      });

  return singleton_map[disname].Instance(
      CORE::UTILS::SingletonAction::create, numdofpernode, numscal, disname);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::ScaTraEleCalcElchScl(
    const int numdofpernode, const int numscal, const std::string& disname)
    : DRT::ELEMENTS::ScaTraEleCalcElchDiffCond<distype, probdim>::ScaTraEleCalcElchDiffCond(
          numdofpernode, numscal, disname),
      diffcondmat_(INPAR::ELCH::diffcondmat_undefined),
      diffcondparams_(DRT::ELEMENTS::ScaTraEleParameterElchDiffCond::Instance(disname))
{
  // replace diffusion manager for diffusion-conduciton formulation by diffusion manager for SCLs
  my::diffmanager_ = Teuchos::rcp(new ScaTraEleDiffManagerElchScl(my::numscal_));

  // replace internal variable manager for diffusion-conduction by internal variable manager for
  // SCL formulation
  my::scatravarmanager_ =
      Teuchos::rcp(new ScaTraEleInternalVariableManagerElchScl<my::nsd_, my::nen_>(
          my::numscal_, myelch::elchparams_, diffcondparams_));

  // replace utility class for diffusion-conduction formulation by utility class for SCLs
  myelch::utils_ =
      DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>::Instance(numdofpernode, numscal, disname);

  // safety checks for stabilization settings
  if (my::scatrapara_->StabType() != INPAR::SCATRA::stabtype_no_stabilization or
      my::scatrapara_->TauDef() != INPAR::SCATRA::tau_zero)
  {
    dserror(
        "No stabilization available for the diffusion-conduction formulation, since we had no "
        "problems so far.");
  }
  if (not my::scatrapara_->MatGP() or not my::scatrapara_->TauGP())
  {
    dserror(
        "Since most of the materials of the Diffusion-conduction formulation depend on the "
        "concentration, an evaluation of the material and the stabilization parameter at the "
        "element center is disabled.");
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
double DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcFreeCharge(
    const double concentration)
{
  return DiffManager()->GetValence(0) * myelch::elchparams_->Faraday() *
         (concentration - DiffManager()->GetBulkConc());
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
double DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcFreeChargeDerConc()
{
  return DiffManager()->GetValence(0) * myelch::elchparams_->Faraday();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcMatPotCoulomb(
    CORE::LINALG::SerialDenseMatrix& emat, const double fac, const double invf,
    const double scalefac, const CORE::LINALG::Matrix<my::nsd_, 1>& gradpot, const double epsilon)
{
  for (unsigned vi = 0; vi < my::nen_; ++vi)
  {
    for (unsigned ui = 0; ui < my::nen_; ++ui)
    {
      double laplawf(0.);
      my::GetLaplacianWeakForm(laplawf, ui, vi);

      // linearization of the ohmic term
      //
      // (grad w, -epsilon D(grad pot))
      emat(vi * my::numdofpernode_ + my::numscal_, ui * my::numdofpernode_ + my::numscal_) +=
          fac * invf * scalefac * epsilon * laplawf;
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcRhsPotCoulomb(
    CORE::LINALG::SerialDenseVector& erhs, const double fac, const double invf,
    const double cond_invperm, const CORE::LINALG::Matrix<my::nsd_, 1>& gradpot,
    const double epsilon)
{
  for (unsigned vi = 0; vi < my::nen_; ++vi)
  {
    double laplawfrhs_gradpot(0.);
    my::GetLaplacianWeakFormRHS(laplawfrhs_gradpot, gradpot, vi);

    erhs[vi * my::numdofpernode_ + my::numscal_] -=
        fac * invf * cond_invperm * epsilon * laplawfrhs_gradpot;
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcMatPotSrc(
    CORE::LINALG::SerialDenseMatrix& emat, const int k, const double timefacfac, const double invf,
    const double cond_invperm, const double z_k_F)
{
  for (unsigned vi = 0; vi < my::nen_; ++vi)
  {
    for (unsigned ui = 0; ui < my::nen_; ++ui)
    {
      emat(vi * my::numdofpernode_ + my::numscal_, ui * my::numdofpernode_ + k) +=
          -z_k_F * timefacfac * invf * cond_invperm * my::funct_(vi) * my::funct_(ui);
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcRhsPotSrc(
    CORE::LINALG::SerialDenseVector& erhs, const int k, const double fac, const double invf,
    const double cond_invperm, const double q_F)
{
  for (unsigned vi = 0; vi < my::nen_; ++vi)
  {
    erhs[vi * my::numdofpernode_ + my::numscal_] -=
        -fac * invf * cond_invperm * my::funct_(vi) * q_F;
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcRhsDiffCur(
    CORE::LINALG::SerialDenseVector& erhs, const double rhsfac, const std::vector<double>& invfval,
    const std::vector<CORE::LINALG::Matrix<my::nsd_, 1>>& gradphi)
{
  if (diffcondmat_ != INPAR::ELCH::diffcondmat_scl)
    dserror("Diffusion-Conduction material has to be SCL material");

  for (unsigned vi = 0; vi < my::nen_; ++vi)
  {
    for (unsigned idim = 0; idim < my::nsd_; ++idim)
    {
      for (int k = 0; k < my::numscal_; ++k)
      {
        erhs[vi * my::numdofpernode_ + (my::numscal_ + 1) + idim] -=
            rhsfac * DiffManager()->GetPhasePoroTort(0) * my::funct_(vi) *
            DiffManager()->GetIsotropicDiff(k) * gradphi[k](idim);
      }
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcMatDiffCur(
    CORE::LINALG::SerialDenseMatrix& emat, const double timefacfac,
    const std::vector<double>& invfval,
    const std::vector<CORE::LINALG::Matrix<my::nsd_, 1>>& gradphi)
{
  for (unsigned vi = 0; vi < my::nen_; ++vi)
  {
    for (unsigned ui = 0; ui < my::nen_; ++ui)
    {
      // diffusive term
      // (grad w, D grad c)
      for (unsigned idim = 0; idim < my::nsd_; ++idim)
      {
        for (int k = 0; k < my::numscal_; ++k)
        {
          //  - D nabla c
          emat(vi * my::numdofpernode_ + (my::numscal_ + 1) + idim, ui * my::numdofpernode_ + k) +=
              timefacfac * DiffManager()->GetPhasePoroTort(0) * my::funct_(vi) *
              DiffManager()->GetIsotropicDiff(k) * my::derxy_(idim, ui);

          // linearization wrt DiffCoeff
          emat(vi * my::numdofpernode_ + (my::numscal_ + 1) + idim, ui * my::numdofpernode_ + k) +=
              timefacfac * DiffManager()->GetPhasePoroTort(0) *
              DiffManager()->GetConcDerivIsoDiffCoef(k, k) * my::funct_(vi) *
              (gradphi[k])(idim)*my::funct_(ui);
        }
      }
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcMatAndRhs(
    CORE::LINALG::SerialDenseMatrix& emat, CORE::LINALG::SerialDenseVector& erhs, const int k,
    const double fac, const double timefacfac, const double rhsfac, const double taufac,
    const double timetaufac, const double rhstaufac, CORE::LINALG::Matrix<my::nen_, 1>& tauderpot,
    double& rhsint)
{
  //----------------------------------------------------------------
  // 1) element matrix: instationary terms
  //----------------------------------------------------------------
  if (not my::scatraparatimint_->IsStationary())
    my::CalcMatMass(emat, k, fac, DiffManager()->GetPhasePoro(0));
  //----------------------------------------------------------------
  // 2) element matrix: stationary terms of ion-transport equation
  //----------------------------------------------------------------
  // 2b)  element matrix: diffusion term

  // current is not a solution variable
  if (not diffcondparams_->CurSolVar())
  {
    // i)  constant diffusion coefficient
    my::CalcMatDiff(emat, k, timefacfac * DiffManager()->GetPhasePoroTort(0));

    // ii) concentration depending diffusion coefficient
    mydiffcond::CalcMatDiffCoeffLin(
        emat, k, timefacfac, VarManager()->GradPhi(k), DiffManager()->GetPhasePoroTort(0));


    // 2d) electrical conduction term (transport equation)
    //     i)  conduction term + ohmic overpotential
    //         (w_k, - t_k kappa nabla phi /(z_k F)) , transference number: const., unity
    mydiffcond::CalcMatCondOhm(
        emat, k, timefacfac, DiffManager()->InvFVal(k), VarManager()->GradPot());
  }
  // equation for current is solved independently: our case!!!
  else if (diffcondparams_->CurSolVar())
  // dc/dt + nabla N = 0
  {
    // current term (with current as a solution variable)
    mydiffcond::CalcMatCond(emat, k, timefacfac, DiffManager()->InvFVal(k), VarManager()->CurInt());
  }

  //---------------------------------------------------------------------
  // 3)   governing equation for the electric potential field and free current
  //---------------------------------------------------------------------
  // see function CalcMatAndRhsOutsideScalarLoop()

  //-----------------------------------------------------------------------
  // 4) element right hand side vector (neg. residual of nonlinear problem)
  //-----------------------------------------------------------------------
  if (my::scatraparatimint_->IsIncremental() and not my::scatraparatimint_->IsStationary())
  {
    my::CalcRHSLinMass(
        erhs, k, rhsfac, fac, DiffManager()->GetPhasePoro(0), DiffManager()->GetPhasePoro(0));
  }

  // adaption of rhs with respect to time integration: no sources
  // Evaluation at Gauss Points before spatial integration
  my::ComputeRhsInt(rhsint, mydiffcond ::DiffManager()->GetPhasePoro(0),
      DiffManager()->GetPhasePoro(0), VarManager()->Hist(k));

  // add RHS and history contribution
  // Integrate RHS (@n) over element volume ==> total impact of timestep n
  my::CalcRHSHistAndSource(erhs, k, fac, rhsint);

  if (not diffcondparams_->CurSolVar())  // not utilized in previous investigations
  {
    // diffusion term
    my::CalcRHSDiff(erhs, k, rhsfac * DiffManager()->GetPhasePoroTort(0));

    // electrical conduction term (transport equation)
    // equation for current is inserted in the mass transport equation
    mydiffcond::CalcRhsCondOhm(erhs, k, rhsfac, DiffManager()->InvFVal(k), VarManager()->GradPot());
  }
  // equation for current is solved independently: free current density!
  // nabla dot (i/z_k F)
  else if (diffcondparams_->CurSolVar())
  {
    // curint: current density at GP, InvFVal(k): 1/(z_k F)
    mydiffcond::CalcRhsCond(erhs, k, rhsfac, DiffManager()->InvFVal(k), VarManager()->CurInt());
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::CalcMatAndRhsOutsideScalarLoop(
    CORE::LINALG::SerialDenseMatrix& emat, CORE::LINALG::SerialDenseVector& erhs, const double fac,
    const double timefacfac, const double rhsfac)
{
  //----------------------------------------------------------------
  // 3)   governing equation for the electric potential field
  //----------------------------------------------------------------
  if (not diffcondparams_->CurSolVar())
  {
    // 3c) Laplace equation: nabla^2 Phi + sum (F z_k c_k) = 0

    // i) eps nabla^2 Phi = 0: Matrix
    CalcMatPotCoulomb(emat, timefacfac, VarManager()->InvF(),
        DiffManager()->GetCond() / DiffManager()->GetPermittivity(), VarManager()->GradPot(),
        DiffManager()->GetPermittivity());

    //  RHS
    CalcRhsPotCoulomb(erhs, rhsfac, VarManager()->InvF(),
        DiffManager()->GetCond() / DiffManager()->GetPermittivity(), VarManager()->GradPot(),
        DiffManager()->GetPermittivity());

    // ii) -sum (F z_k c_k) = 0 (use of defined function);

    // set this to zero (only laplace equation zero charge) ==> linear function
    for (int k = 0; k < my::numscal_; ++k)
    {
      CalcMatPotSrc(emat, k, timefacfac, VarManager()->InvF(),
          DiffManager()->GetCond() / DiffManager()->GetPermittivity(), CalcFreeChargeDerConc());

      CalcRhsPotSrc(erhs, k, rhsfac, VarManager()->InvF(),
          DiffManager()->GetCond() / DiffManager()->GetPermittivity(),
          CalcFreeCharge(VarManager()->Phinp(k)));
    }
  }

  // 3c) Laplace equation based on free charge
  // i_F/(z_k F) = N+, ion flux!
  // equation for current is solved independently
  else if (diffcondparams_->CurSolVar())
  {
    //-----------------------------------------------------------------------
    // 5) equation for the current incl. rhs-terms
    //-----------------------------------------------------------------------

    // matrix terms
    // (xsi_i,Di)
    mydiffcond::CalcMatCurEquCur(emat, timefacfac, VarManager()->InvF());

    // (xsi, -D(kappa phi))
    mydiffcond::CalcMatCurEquOhm(emat, timefacfac, VarManager()->InvF(), VarManager()->GradPot());

    // (xsi, -D(z_k F D (c) nabla c)
    CalcMatDiffCur(emat, timefacfac, DiffManager()->InvFVal(), VarManager()->GradPhi());

    // (xsi_i,Di): stays the same
    mydiffcond::CalcRhsCurEquCur(erhs, rhsfac, VarManager()->InvF(), VarManager()->CurInt());

    // (xsi, -D(kappa phi)): stays the same, but local version of conductivity
    mydiffcond::CalcRhsCurEquOhm(erhs, rhsfac, VarManager()->InvF(), VarManager()->GradPot());

    // (xsi, - D(z_k F D(c) nabla c)
    CalcRhsDiffCur(erhs, rhsfac, DiffManager()->InvFVal(), VarManager()->GradPhi());

    //------------------------------------------------------------------------------------------
    // 3)   governing equation for the electric potential field and current (incl. rhs-terms)
    //------------------------------------------------------------------------------------------

    // i) eps nabla^2 Phi = 0: Matrix
    CalcMatPotCoulomb(emat, timefacfac, VarManager()->InvF(),
        DiffManager()->GetCond() / DiffManager()->GetPermittivity(), VarManager()->GradPot(),
        DiffManager()->GetPermittivity());

    //  RHS
    CalcRhsPotCoulomb(erhs, rhsfac, VarManager()->InvF(),
        DiffManager()->GetCond() / DiffManager()->GetPermittivity(), VarManager()->GradPot(),
        DiffManager()->GetPermittivity());
    // ii) -sum (F z_k c_k) = 0

    // set this to zero (only laplace equation zero charge) ==> linear function
    for (int k = 0; k < my::numscal_; ++k)
    {
      CalcMatPotSrc(emat, k, timefacfac, VarManager()->InvF(),
          DiffManager()->GetCond() / DiffManager()->GetPermittivity(), CalcFreeChargeDerConc());

      CalcRhsPotSrc(erhs, k, rhsfac, VarManager()->InvF(),
          DiffManager()->GetCond() / DiffManager()->GetPermittivity(),
          CalcFreeCharge(VarManager()->Phinp(k)));
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype, int probdim>
void DRT::ELEMENTS::ScaTraEleCalcElchScl<distype, probdim>::GetMaterialParams(
    const DRT::Element* ele, std::vector<double>& densn, std::vector<double>& densnp,
    std::vector<double>& densam, double& visc, const int iquad)
{
  // extract material from element
  Teuchos::RCP<MAT::Material> material = ele->Material();

  // evaluate electrolyte material
  if (material->MaterialType() == INPAR::MAT::m_elchmat)
  {
    Utils()->MatElchMat(
        material, VarManager()->Phinp(), VarManager()->Temperature(), DiffManager(), diffcondmat_);
  }
  else
    dserror("Invalid material type!");
}

// template classes
// 1D elements
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::line2, 1>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::line2, 2>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::line2, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::line3, 1>;

// 2D elements
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::tri3, 2>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::tri3, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::tri6, 2>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::quad4, 2>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::quad4, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::quad9, 2>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::nurbs9, 2>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::hex8, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::hex27, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::tet4, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::tet10, 3>;
template class DRT::ELEMENTS::ScaTraEleCalcElchScl<DRT::Element::pyramid5, 3>;
