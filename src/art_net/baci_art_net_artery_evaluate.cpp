
/*! \file

 \brief evaluate routines for the artery element


\level 3

*----------------------------------------------------------------------*/

#include "baci_art_net_artery.hpp"
#include "baci_art_net_artery_ele_action.hpp"
#include "baci_art_net_artery_ele_factory.hpp"
#include "baci_discretization_fem_general_utils_fem_shapefunctions.hpp"
#include "baci_inpar_bio.hpp"
#include "baci_lib_discret.hpp"
#include "baci_linalg_utils_sparse_algebra_math.hpp"
#include "baci_mat_cnst_1d_art.hpp"
#include "baci_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN


/*---------------------------------------------------------------------*
 //evaluate the element (public)                            ismail 06/09
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::Artery::Evaluate(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, LocationArray& la,
    CORE::LINALG::SerialDenseMatrix& elemat1, CORE::LINALG::SerialDenseMatrix& elemat2,
    CORE::LINALG::SerialDenseVector& elevec1, CORE::LINALG::SerialDenseVector& elevec2,
    CORE::LINALG::SerialDenseVector& elevec3)
{
  // check for the action parameter
  const ARTERY::Action action = CORE::UTILS::GetAsEnum<ARTERY::Action>(params, "action");
  /*
  Here must add the steps for evaluating an element
  */
  Teuchos::RCP<MAT::Material> mat = Material();

  switch (action)
  {
    case ARTERY::calc_sys_matrix_rhs:
    {
      return DRT::ELEMENTS::ArtNetFactory::ProvideImpl(Shape(), impltype_, discretization.Name())
          ->Evaluate(
              this, params, discretization, la, elemat1, elemat2, elevec1, elevec2, elevec3, mat);
    }
    break;
    case ARTERY::calc_scatra_sys_matrix_rhs:
    {
      return DRT::ELEMENTS::ArtNetFactory::ProvideImpl(Shape(), impltype_, discretization.Name())
          ->ScatraEvaluate(this, params, discretization, la[0].lm_, elemat1, elemat2, elevec1,
              elevec2, elevec3, mat);
      break;
    }
    case ARTERY::get_initial_artery_state:
    case ARTERY::set_term_bc:
    case ARTERY::set_scatra_term_bc:
    case ARTERY::set_scatra_bc:
    case ARTERY::solve_riemann_problem:
    case ARTERY::calc_postpro_vals:
    case ARTERY::calc_scatra_from_scatra_fb:
    case ARTERY::evaluate_wf_wb:
    case ARTERY::evaluate_scatra_analytically:
    case ARTERY::calc_flow_pressurebased:
    {
      return DRT::ELEMENTS::ArtNetFactory::ProvideImpl(Shape(), impltype_, discretization.Name())
          ->EvaluateService(this, action, params, discretization, la, elemat1, elemat2, elevec1,
              elevec2, elevec3, mat);
    }
    break;
    default:
      FOUR_C_THROW("Unkown type of action %d for Artery", action);
  }  // end of switch(act)


  return 0;
}  // end of DRT::ELEMENTS::Artery::Evaluate


int DRT::ELEMENTS::Artery::EvaluateNeumann(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, DRT::Condition& condition, std::vector<int>& lm,
    CORE::LINALG::SerialDenseVector& elevec1, CORE::LINALG::SerialDenseMatrix* elemat1)
{
  return 0;
}

/*----------------------------------------------------------------------*
 |  do nothing (public)                                     ismail 01/09|
 |                                                                      |
 |  The function is just a dummy.                                       |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::Artery::EvaluateDirichlet(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, DRT::Condition& condition, std::vector<int>& lm,
    CORE::LINALG::SerialDenseVector& elevec1)
{
  return 0;
}


// get optimal gaussrule for discretization type
CORE::FE::GaussRule1D DRT::ELEMENTS::Artery::getOptimalGaussrule(const CORE::FE::CellType& distype)
{
  CORE::FE::GaussRule1D rule = CORE::FE::GaussRule1D::undefined;
  switch (distype)
  {
    case CORE::FE::CellType::line2:
      rule = CORE::FE::GaussRule1D::line_2point;
      break;
    case CORE::FE::CellType::line3:
      rule = CORE::FE::GaussRule1D::line_3point;
      break;
    default:
      FOUR_C_THROW("unknown number of nodes for gaussrule initialization");
  }
  return rule;
}


// check, whether higher order derivatives for shape functions (dxdx, dxdy, ...) are necessary
bool DRT::ELEMENTS::Artery::isHigherOrderElement(const CORE::FE::CellType distype) const
{
  bool hoel = true;
  switch (distype)
  {
    case CORE::FE::CellType::line3:
      hoel = true;
      break;
    case CORE::FE::CellType::line2:
      hoel = false;
      break;
    default:
      FOUR_C_THROW("distype unknown!");
  }
  return hoel;
}

FOUR_C_NAMESPACE_CLOSE
