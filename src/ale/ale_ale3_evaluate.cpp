/*----------------------------------------------------------------------------*/
/*! \file

\brief Evaluate routines of ALE element for 3D case


\level 1
*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include "ale_ale3.H"
#include "lib_discret.H"
#include "nurbs_discret.H"
#include "discretization_fem_general_utils_fem_shapefunctions.H"
#include "discretization_fem_general_utils_nurbs_shapefunctions.H"
#include "lib_exporter.H"
#include "utils_exceptions.H"
#include "lib_utils.H"
#include "mat_stvenantkirchhoff.H"
#include "mat_elasthyper.H"
#include <Epetra_SerialDenseSolver.h>

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::Ale3_Impl_Interface* DRT::ELEMENTS::Ale3_Impl_Interface::Impl(
    DRT::ELEMENTS::Ale3* ele)
{
  switch (ele->Shape())
  {
    case DRT::Element::hex8:
    {
      return Ale3_Impl<DRT::Element::hex8>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::hex20:
    {
      return Ale3_Impl<DRT::Element::hex20>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::hex27:
    {
      return Ale3_Impl<DRT::Element::hex27>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::tet4:
    {
      return Ale3_Impl<DRT::Element::tet4>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::tet10:
    {
      return Ale3_Impl<DRT::Element::tet10>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::wedge6:
    {
      return Ale3_Impl<DRT::Element::wedge6>::Instance(CORE::UTILS::SingletonAction::create);
    }
      /*  case DRT::Element::wedge15:
        {
          return Ale3_Impl<DRT::Element::wedge15>::Instance(CORE::UTILS::SingletonAction::create);
        }*/
    case DRT::Element::pyramid5:
    {
      return Ale3_Impl<DRT::Element::pyramid5>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::nurbs8:
    {
      return Ale3_Impl<DRT::Element::nurbs8>::Instance(CORE::UTILS::SingletonAction::create);
    }
    case DRT::Element::nurbs27:
    {
      return Ale3_Impl<DRT::Element::nurbs27>::Instance(CORE::UTILS::SingletonAction::create);
    }
    default:
      dserror("shape %d (%d nodes) not supported", ele->Shape(), ele->NumNode());
      break;
  }
  return NULL;
}

template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::Ale3_Impl<distype>* DRT::ELEMENTS::Ale3_Impl<distype>::Instance(
    CORE::UTILS::SingletonAction action)
{
  static auto singleton_owner = CORE::UTILS::MakeSingletonOwner(
      []()
      {
        return std::unique_ptr<DRT::ELEMENTS::Ale3_Impl<distype>>(
            new DRT::ELEMENTS::Ale3_Impl<distype>());
      });

  return singleton_owner.Instance(action);
}


/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
int DRT::ELEMENTS::Ale3::Evaluate(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, std::vector<int>& lm, Epetra_SerialDenseMatrix& elemat1,
    Epetra_SerialDenseMatrix& elemat2, Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseVector& elevec2, Epetra_SerialDenseVector& elevec3)
{
  DRT::ELEMENTS::Ale3::ActionType act = Ale3::none;

  // get the action required
  std::string action = params.get<std::string>("action", "none");
  if (action == "none")
    dserror("No action supplied");
  else if (action == "calc_ale_laplace_material")
    act = Ale3::calc_ale_laplace_material;
  else if (action == "calc_ale_laplace_spatial")
    act = Ale3::calc_ale_laplace_spatial;
  else if (action == "calc_ale_solid")
    act = Ale3::calc_ale_solid;
  else if (action == "calc_ale_solid_linear")
    act = Ale3::calc_ale_solid_linear;
  else if (action == "calc_ale_springs_material")
    act = Ale3::calc_ale_springs_material;
  else if (action == "calc_ale_springs_spatial")
    act = Ale3::calc_ale_springs_spatial;
  else if (action == "calc_ale_node_normal")
    act = Ale3::calc_ale_node_normal;
  else if (action == "setup_material")
    act = Ale3::setup_material;
  else if (action == "calc_jacobian_determinant")
    act = Ale3::calc_det_jac;
  else
    dserror("Unknown type of action for Ale3");

  // get the material
  Teuchos::RCP<MAT::Material> mat = Material();

  switch (act)
  {
    case calc_ale_laplace_material:
    {
      std::vector<double> my_dispnp;
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      my_dispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->static_ke_laplace(
          this, discretization, elemat1, elevec1, my_dispnp, mat, false);

      break;
    }
    case calc_ale_laplace_spatial:
    {
      std::vector<double> my_dispnp;
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      my_dispnp.resize(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->static_ke_laplace(
          this, discretization, elemat1, elevec1, my_dispnp, mat, true);

      break;
    }
    case calc_ale_solid:
    {
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      std::vector<double> my_dispnp(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->static_ke_nonlinear(
          this, discretization, lm, elemat1, elevec1, my_dispnp, params, true);

      break;
    }
    case calc_ale_solid_linear:
    {
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      std::vector<double> my_dispnp(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->static_ke_nonlinear(
          this, discretization, lm, elemat1, elevec1, my_dispnp, params, false);

      break;
    }
    case calc_ale_springs_material:
    {
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      std::vector<double> my_dispnp(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->static_ke_spring(this, elemat1, elevec1, my_dispnp, false);

      break;
    }
    case calc_ale_springs_spatial:
    {
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      std::vector<double> my_dispnp(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->static_ke_spring(this, elemat1, elevec1, my_dispnp, true);

      break;
    }
    case calc_ale_node_normal:
    {
      Teuchos::RCP<const Epetra_Vector> dispnp = discretization.GetState("dispnp");
      std::vector<double> my_dispnp(lm.size());
      DRT::UTILS::ExtractMyValues(*dispnp, my_dispnp, lm);

      Ale3_Impl_Interface::Impl(this)->ElementNodeNormal(this, elevec1, my_dispnp);

      break;
    }
    case setup_material:
    {
      // get material
      Teuchos::RCP<MAT::So3Material> so3mat =
          Teuchos::rcp_dynamic_cast<MAT::So3Material>(mat, true);

      if (so3mat->MaterialType() != INPAR::MAT::m_elasthyper and
          so3mat->MaterialType() !=
              INPAR::MAT::m_stvenant)  // ToDo (mayr): allow only materials without history
      {
        dserror(
            "Illegal material type for ALE. Only materials allowed that do "
            "not store Gauss point data and do not need additional data from the "
            "element line definition.");
      }

      if (so3mat->MaterialType() == INPAR::MAT::m_elasthyper)
      {
        so3mat = Teuchos::rcp_dynamic_cast<MAT::ElastHyper>(mat, true);
        so3mat->Setup(0, NULL);
      }
      break;  // no setup for St-Venant
    }
    case calc_det_jac:
    {
      dserror("Not implement for 3D, yet.");
      break;
    }
    default:
      dserror("Unknown type of action for Ale3");
      break;
  }
  return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
int DRT::ELEMENTS::Ale3::EvaluateNeumann(Teuchos::ParameterList& params,
    DRT::Discretization& discretization, DRT::Condition& condition, std::vector<int>& lm,
    Epetra_SerialDenseVector& elevec1, Epetra_SerialDenseMatrix* elemat1)
{
  return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ElementNodeNormal(
    Ale3* ele, Epetra_SerialDenseVector& elevec1, std::vector<double>& my_dispnp)
{
  if (distype == DRT::Element::nurbs8 or distype == DRT::Element::nurbs27)
  {
    dserror("not implemented!");
  }

  LINALG::Matrix<3, iel> xyze;

  // get node coordinates
  DRT::Node** nodes = ele->Nodes();
  for (int i = 0; i < iel; i++)
  {
    const double* x = nodes[i]->X();
    xyze(0, i) = x[0];
    xyze(1, i) = x[1];
    xyze(2, i) = x[2];
  }

  for (int i = 0; i < iel; i++)
  {
    xyze(0, i) += my_dispnp[3 * i + 0];
    xyze(1, i) += my_dispnp[3 * i + 1];
    xyze(2, i) += my_dispnp[3 * i + 2];
  }

  /*----------------------------------------- declaration of variables ---*/
  LINALG::Matrix<iel, 1> funct;
  LINALG::Matrix<3, iel> deriv;
  LINALG::Matrix<3, 3> xjm;
  LINALG::Matrix<3, 3> xji;

  // gaussian points
  const CORE::DRT::UTILS::GaussRule3D gaussrule = getOptimalGaussrule();
  const CORE::DRT::UTILS::IntegrationPoints3D intpoints(gaussrule);

  // integration loops
  for (int iquad = 0; iquad < intpoints.nquad; iquad++)
  {
    const double e1 = intpoints.qxg[iquad][0];
    const double e2 = intpoints.qxg[iquad][1];
    const double e3 = intpoints.qxg[iquad][2];

    // get values of shape functions and derivatives in the gausspoint
    CORE::DRT::UTILS::shape_function_3D(funct, e1, e2, e3, distype);
    CORE::DRT::UTILS::shape_function_3D_deriv1(deriv, e1, e2, e3, distype);

    // compute jacobian matrix
    // determine jacobian at point r,s,t
    xjm.MultiplyNT(deriv, xyze);

    // determinant and inverse of jacobian
    const double det = xji.Invert(xjm);

    // integrate shapefunction gradient over element
    const double fac = intpoints.qwgt[iquad] * det;

    for (int node = 0; node < iel; ++node)
    {
      for (int dim = 0; dim < 3; ++dim)
      {
        int row = 3 * node + dim;
        elevec1(row) += (deriv(0, node) * xji(dim, 0) + deriv(1, node) * xji(dim, 1) +
                            deriv(2, node) * xji(dim, 2)) *
                        fac;
      }
    }
  }
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_edge_geometry(int i, int j,
    const LINALG::Matrix<3, iel>& xyze, double& length, double& dx, double& dy, double& dz)
{
  /*---------------------------------------------- x-, y- and z-difference ---*/
  dx = xyze(0, j) - xyze(0, i);
  dy = xyze(1, j) - xyze(1, i);
  dz = xyze(2, j) - xyze(2, i);
  /*------------------------------- determine distance between i and j ---*/
  length = sqrt(dx * dx + dy * dy + dz * dz);
#ifdef DEBUG
  if (length < (1.0E-14)) dserror("edge or diagonal of element has zero length");
#endif
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_add_tria_stiffness(int node_p, int node_q, int node_r,
    int node_s, const LINALG::Matrix<3, 1>& sq, const double len_sq, const LINALG::Matrix<3, 1>& rp,
    const double len_rp, const LINALG::Matrix<3, 1>& qp, const LINALG::Matrix<3, 1>& local_x,
    LINALG::Matrix<3 * iel, 3 * iel>& sys_mat)
{
  // Positions for dynamic triangle (2D)
  // sequence: s,j,q
  LINALG::Matrix<2, 3> xyze_dyn_tria;

  // Some matrices that can be found in the paper are not assembled
  // here, because it can be done with less memory. I use only one
  // transformation matrix to get from the triangle plane to the 12x12
  // stiffness component of this triangle.

  // transformation matrix from the plane of the triangle to the
  // three-dimensional global frame.
  // This corresponds to (R(sjq) x r(sjq) x S^T)^T from Farhat et al.
  LINALG::Matrix<12, 3> trans_matrix;

  // rotational stiffness matrix for tetrahedron with given dynamic triangle
  LINALG::Matrix<12, 12> k_dyn_tet;

  // local x,y in the plane of the dynamic triangle
  // these are the 3d-vectors that span this plane
  LINALG::Matrix<3, 1> local_y;

  // the point p relativ to the point (0,0) in the triangle plane
  // transformed into 3d space
  LINALG::Matrix<3, 1> p;

  // local x-value of s xyze_dyn_tria(0,0) := (-1.0)*sq*local_x (local origin lies on plane pqr)
  // local y-value of s xyze_dyn_tria(1,0 := 0.0
  xyze_dyn_tria(0, 0) = -sq.Dot(local_x);
  xyze_dyn_tria(1, 0) = 0.0;

  // local_y = (sq + xyze_dyn_tria(0,0)*local_x)/|(sq + xyze_dyn_tria(0,0)*local_x)|
  // xyze_dyn_tria(1,2) = |sq + xyze_dyn_tria(0,0)*local_x|, xyze_dyn_tria(0,2) := 0.0
  local_y.Update(xyze_dyn_tria(0, 0), local_x, 1.0, sq);  // just an intermediate step
  xyze_dyn_tria(1, 2) = local_y.Norm2();
  xyze_dyn_tria(0, 2) = 0.0;

  if (xyze_dyn_tria(1, 2) != 0)  // == 0 will trigger the parallel check below
    local_y.Scale(1.0 / xyze_dyn_tria(1, 2));

  // check = (local_y x sq) * rp  If this is very small rp is parallel
  // to the plane spanned by local_y and sq.
  double check = (local_y(1) * sq(2) - local_y(2) * sq(1)) * rp(0) +
                 (local_y(2) * sq(0) - local_y(0) * sq(2)) * rp(1) +
                 (local_y(0) * sq(1) - local_y(1) * sq(0)) * rp(2);
  check /= sqrt((rp(0) * rp(0) + rp(1) * rp(1) + rp(2) * rp(2)) *
                (sq(0) * sq(0) + sq(1) * sq(1) + sq(2) * sq(2)));


  // if rp and local_y are parallel calculate stiffness of lineal spring s-q
  if (fabs(check) < 1e-2 or fabs(xyze_dyn_tria(1, 2)) < 1e-9)
  {
    const int ts = 3 * node_s;
    const int tsp = ts + 1;
    const int tspp = ts + 2;
    const int tq = 3 * node_q;
    const int tqp = tq + 1;
    const int tqpp = tq + 2;
    // we know the edge-information from above.
    const double factor = 1.0 / (len_sq * len_sq * len_sq);
    const double dxx_l3 = sq(0) * sq(0) * factor;
    const double dxy_l3 = sq(0) * sq(1) * factor;
    const double dxz_l3 = sq(0) * sq(2) * factor;
    const double dyy_l3 = sq(1) * sq(1) * factor;
    const double dyz_l3 = sq(1) * sq(2) * factor;
    const double dzz_l3 = sq(2) * sq(2) * factor;
    // put values in 'element stiffness'
    // rows for node_s
    sys_mat(ts, ts) += dxx_l3;
    sys_mat(ts, tsp) += dxy_l3;
    sys_mat(ts, tspp) += dxz_l3;

    sys_mat(ts, tq) -= dxx_l3;
    sys_mat(ts, tqp) -= dxy_l3;
    sys_mat(ts, tqpp) -= dxz_l3;

    sys_mat(tsp, ts) += dxy_l3;
    sys_mat(tsp, tsp) += dyy_l3;
    sys_mat(tsp, tspp) += dyz_l3;

    sys_mat(tsp, tq) -= dxy_l3;
    sys_mat(tsp, tqp) -= dyy_l3;
    sys_mat(tsp, tqpp) -= dyz_l3;

    sys_mat(tspp, ts) += dxz_l3;
    sys_mat(tspp, tsp) += dyz_l3;
    sys_mat(tspp, tspp) += dzz_l3;

    sys_mat(tspp, tq) -= dxz_l3;
    sys_mat(tspp, tqp) -= dyz_l3;
    sys_mat(tspp, tqpp) -= dzz_l3;

    // rows for node_q
    sys_mat(tq, ts) -= dxx_l3;
    sys_mat(tq, tsp) -= dxy_l3;
    sys_mat(tq, tspp) -= dxz_l3;

    sys_mat(tq, tq) += dxx_l3;
    sys_mat(tq, tqp) += dxy_l3;
    sys_mat(tq, tqpp) += dxz_l3;

    sys_mat(tqp, ts) -= dxy_l3;
    sys_mat(tqp, tsp) -= dyy_l3;
    sys_mat(tqp, tspp) -= dyz_l3;

    sys_mat(tqp, tq) += dxy_l3;
    sys_mat(tqp, tqp) += dyy_l3;
    sys_mat(tqp, tqpp) += dyz_l3;

    sys_mat(tqpp, ts) -= dxz_l3;
    sys_mat(tqpp, tsp) -= dyz_l3;
    sys_mat(tqpp, tspp) -= dzz_l3;

    sys_mat(tqpp, tq) += dxz_l3;
    sys_mat(tqpp, tqp) += dyz_l3;
    sys_mat(tqpp, tqpp) += dzz_l3;
  }
  else
  {
    // local x,y-values of j, using pO + Oj + jp = 0
    //(O is local origin on plane pqr)
    xyze_dyn_tria(0, 1) = 0.0;
    p.Update(xyze_dyn_tria(1, 2), local_y, 1, qp);

    ///// solve xyze_dyn_tria(1,1) * local_y = p + lambda * (-rp)
    double d;
    double lambda;
    const double limit = 1e-4 * len_rp;  // I think the limit can be even higher
    if (fabs(d = local_y(0) * rp(1) - local_y(1) * rp(0)) > limit)
    {
      const double fac = 1.0 / d;
      xyze_dyn_tria(1, 1) = (rp(1) * p(0) - rp(0) * p(1)) * fac;
      lambda = (local_y(0) * p(1) - local_y(1) * p(0)) * fac;
    }
    else if (fabs(d = local_y(0) * rp(2) - local_y(2) * rp(0)) > limit)
    {
      const double fac = 1.0 / d;
      xyze_dyn_tria(1, 1) = (rp(2) * p(0) - rp(0) * p(2)) * fac;
      lambda = (local_y(0) * p(2) - local_y(2) * p(0)) * fac;
    }
    else /* we know it has to work here, because the system is solvable  */
    {
      const double fac = 1.0 / (local_y(1) * rp(2) - local_y(2) * rp(1));
      xyze_dyn_tria(1, 1) = (rp(2) * p(1) - rp(1) * p(2)) * fac;
      lambda = (local_y(1) * p(2) - local_y(2) * p(1)) * fac;
    }

    if (lambda < 0.0)
      lambda = 0.0;
    else if (lambda > 1.0)
      lambda = 1.0;
    const double one_minus_lambda = 1 - lambda;

    ////// evaluate torsional stiffness of dynamic triangle
    const double& tmp = xyze_dyn_tria(0, 0);
    const double y_jk = -xyze_dyn_tria(1, 1) + xyze_dyn_tria(1, 2);
    // squares of side lengths
    const double l_ij_sq = xyze_dyn_tria(1, 1) * xyze_dyn_tria(1, 1) + tmp * tmp;
    const double l_jk_sq = y_jk * y_jk;
    const double l_ki_sq = xyze_dyn_tria(1, 2) * xyze_dyn_tria(1, 2) + tmp * tmp;
    // auxiliary values same as in Farhat et al.
    const double a_ij = -tmp / (l_ij_sq);
    const double a_jk = 0.0;  // 0.0 / (l_jk_sq)
    const double a_ki = tmp / (l_ki_sq);
    const double b_ij = xyze_dyn_tria(1, 1) / (l_ij_sq);
    const double b_jk = y_jk / (l_jk_sq);
    const double b_ki = -xyze_dyn_tria(1, 2) / (l_ki_sq);

    const double a_ij_0 = a_ij * local_y(0);
    const double a_ij_1 = a_ij * local_y(1);
    const double a_ij_2 = a_ij * local_y(2);
    const double a_jk_0 = a_jk * local_y(0);
    const double a_jk_1 = a_jk * local_y(1);
    const double a_jk_2 = a_jk * local_y(2);
    const double a_ki_0 = a_ki * local_y(0);
    const double a_ki_1 = a_ki * local_y(1);
    const double a_ki_2 = a_ki * local_y(2);
    const double b_ij_0 = b_ij * local_x(0);
    const double b_ij_1 = b_ij * local_x(1);
    const double b_ij_2 = b_ij * local_x(2);
    const double b_jk_0 = b_jk * local_x(0);
    const double b_jk_1 = b_jk * local_x(1);
    const double b_jk_2 = b_jk * local_x(2);
    const double b_ki_0 = b_ki * local_x(0);
    const double b_ki_1 = b_ki * local_x(1);
    const double b_ki_2 = b_ki * local_x(2);

    // area of the triangle
    const double area_double =
        0.5 * sqrt(2.0 * l_ij_sq * l_jk_sq + 2.0 * l_jk_sq * l_ki_sq + 2.0 * l_ki_sq * l_ij_sq -
                   l_ij_sq * l_ij_sq - l_jk_sq * l_jk_sq - l_ki_sq * l_ki_sq);
    const double area_double_sqare = area_double * area_double;


#ifdef DEBUG /*---------------------------------- check edge lengths ---*/
    if (l_ij_sq < (1.0E-7)) dserror("edge or diagonal of element has zero length");
    if (l_jk_sq < (1.0E-7)) dserror("edge or diagonal of element has zero length");
    if (l_ki_sq < (1.0E-7)) dserror("edge or diagonal of element has zero length");
#endif

    /*---------------------------------- determine torsional stiffnesses ---*/
    // instead of the whole matrix only the non-zero diagonal is stored
    const double C0 = l_ij_sq * l_ki_sq / area_double_sqare;
    const double C1 = l_ij_sq * l_jk_sq / area_double_sqare;
    const double C2 = l_ki_sq * l_jk_sq / area_double_sqare;

    /*--------------------------------------- fill transformation matrix ---*/
    // This corresponds to (R(sjq) x r(sjq) x S^T)^T from Farhat et al.
    trans_matrix(0, 0) = one_minus_lambda * (b_ij_0 - a_ij_0);
    trans_matrix(1, 0) = one_minus_lambda * (b_ij_1 - a_ij_1);
    trans_matrix(2, 0) = one_minus_lambda * (b_ij_2 - a_ij_2);
    trans_matrix(3, 0) = b_ki_0 - a_ki_0;
    trans_matrix(4, 0) = b_ki_1 - a_ki_1;
    trans_matrix(5, 0) = b_ki_2 - a_ki_2;
    trans_matrix(6, 0) = lambda * (b_ij_0 - a_ij_0);
    trans_matrix(7, 0) = lambda * (b_ij_1 - a_ij_1);
    trans_matrix(8, 0) = lambda * (b_ij_2 - a_ij_2);
    trans_matrix(9, 0) = a_ij_0 + a_ki_0 - b_ki_0 - b_ij_0;
    trans_matrix(10, 0) = a_ij_1 + a_ki_1 - b_ki_1 - b_ij_1;
    trans_matrix(11, 0) = a_ij_2 + a_ki_2 - b_ki_2 - b_ij_2;

    trans_matrix(0, 1) = one_minus_lambda * (a_jk_0 + a_ij_0 - b_ij_0 - b_jk_0);
    trans_matrix(1, 1) = one_minus_lambda * (a_jk_1 + a_ij_1 - b_ij_1 - b_jk_1);
    trans_matrix(2, 1) = one_minus_lambda * (a_jk_2 + a_ij_2 - b_ij_2 - b_jk_2);
    trans_matrix(3, 1) = b_jk_0 - a_jk_0;
    trans_matrix(4, 1) = b_jk_1 - a_jk_1;
    trans_matrix(5, 1) = b_jk_2 - a_jk_2;
    trans_matrix(6, 1) = lambda * (a_jk_0 + a_ij_0 - b_ij_0 - b_jk_0);
    trans_matrix(7, 1) = lambda * (a_jk_1 + a_ij_1 - b_ij_1 - b_jk_1);
    trans_matrix(8, 1) = lambda * (a_jk_2 + a_ij_2 - b_ij_2 - b_jk_2);
    trans_matrix(9, 1) = b_ij_0 - a_ij_0;
    trans_matrix(10, 1) = b_ij_1 - a_ij_1;
    trans_matrix(11, 1) = b_ij_2 - a_ij_2;

    trans_matrix(0, 2) = one_minus_lambda * (b_jk_0 - a_jk_0);
    trans_matrix(1, 2) = one_minus_lambda * (b_jk_1 - a_jk_1);
    trans_matrix(2, 2) = one_minus_lambda * (b_jk_2 - a_jk_2);
    trans_matrix(3, 2) = a_ki_0 + a_jk_0 - b_jk_0 - b_ki_0;
    trans_matrix(4, 2) = a_ki_1 + a_jk_1 - b_jk_1 - b_ki_1;
    trans_matrix(5, 2) = a_ki_2 + a_jk_2 - b_jk_2 - b_ki_2;
    trans_matrix(6, 2) = lambda * (b_jk_0 - a_jk_0);
    trans_matrix(7, 2) = lambda * (b_jk_1 - a_jk_1);
    trans_matrix(8, 2) = lambda * (b_jk_2 - a_jk_2);
    trans_matrix(9, 2) = b_ki_0 - a_ki_0;
    trans_matrix(10, 2) = b_ki_1 - a_ki_1;
    trans_matrix(11, 2) = b_ki_2 - a_ki_2;

    /*----------------------------------- perform matrix multiplications ---*/
    ////// Compute k_dyn_tet
    // trans_matrix x C x trans_matrix^(T)
    // This corresponds to S x r(sjq)^T x R(sjq)^T x C x R(sjq) x r(sjq) x S^T
    for (int i = 0; i < 12; ++i)
    {
      const double C0T0 = C0 * trans_matrix(i, 0);
      const double C1T1 = C1 * trans_matrix(i, 1);
      const double C2T2 = C2 * trans_matrix(i, 2);
      k_dyn_tet(i, i) =
          C0T0 * trans_matrix(i, 0) + C1T1 * trans_matrix(i, 1) + C2T2 * trans_matrix(i, 2);
      for (int j = 0; j < i; ++j)
      {
        k_dyn_tet(i, j) = k_dyn_tet(j, i) =
            C0T0 * trans_matrix(j, 0) + C1T1 * trans_matrix(j, 1) + C2T2 * trans_matrix(j, 2);
      }
    }

    // This makes writing the loop over sys_mat much easier
    std::vector<int> matrix_access(4);
    matrix_access[0] = 3 * node_p;
    matrix_access[1] = 3 * node_q;
    matrix_access[2] = 3 * node_r;
    matrix_access[3] = 3 * node_s;

    // Sort values in element's sys_mat
    for (int i = 0; i < 4; ++i)
    {
      const int ti = 3 * i;
      const int tip = ti + 1;
      const int tipp = ti + 2;
      const int sys_mat_i = matrix_access[i];
      for (int j = 0; j < 4; ++j)
      {
        const int tj = 3 * j;
        const int tjp = tj + 1;
        const int tjpp = tj + 2;
        const int sys_mat_j = matrix_access[j];

        sys_mat(sys_mat_i, sys_mat_j) += k_dyn_tet(ti, tj);
        sys_mat(sys_mat_i, sys_mat_j + 1) += k_dyn_tet(ti, tjp);
        sys_mat(sys_mat_i, sys_mat_j + 2) += k_dyn_tet(ti, tjpp);

        sys_mat(sys_mat_i + 1, sys_mat_j) += k_dyn_tet(tip, tj);
        sys_mat(sys_mat_i + 1, sys_mat_j + 1) += k_dyn_tet(tip, tjp);
        sys_mat(sys_mat_i + 1, sys_mat_j + 2) += k_dyn_tet(tip, tjpp);

        sys_mat(sys_mat_i + 2, sys_mat_j) += k_dyn_tet(tipp, tj);
        sys_mat(sys_mat_i + 2, sys_mat_j + 1) += k_dyn_tet(tipp, tjp);
        sys_mat(sys_mat_i + 2, sys_mat_j + 2) += k_dyn_tet(tipp, tjpp);
      }
    }
  }
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_add_tetra_stiffness(int tet_0, int tet_1, int tet_2,
    int tet_3, LINALG::Matrix<3 * iel, 3 * iel>& sys_mat, const LINALG::Matrix<3, iel>& xyze)
{
  // according to Farhat et al.
  // twelve-triangle configuration

  LINALG::Matrix<3, 1> e01, e02, e03, e10, e12, e13, e20, e21, e23, e30, e31, e32, local_x;
  double l01, l02, l03, l12, l13, l23;
  ale3_edge_geometry(tet_0, tet_1, xyze, l01, e01(0), e01(1), e01(2));
  ale3_edge_geometry(tet_0, tet_2, xyze, l02, e02(0), e02(1), e02(2));
  ale3_edge_geometry(tet_0, tet_3, xyze, l03, e03(0), e03(1), e03(2));
  ale3_edge_geometry(tet_1, tet_2, xyze, l12, e12(0), e12(1), e12(2));
  ale3_edge_geometry(tet_1, tet_3, xyze, l13, e13(0), e13(1), e13(2));
  ale3_edge_geometry(tet_2, tet_3, xyze, l23, e23(0), e23(1), e23(2));
  e10(0) = -e01(0);
  e10(1) = -e01(1);
  e10(2) = -e01(2);
  e20(0) = -e02(0);
  e20(1) = -e02(1);
  e20(2) = -e02(2);
  e21(0) = -e12(0);
  e21(1) = -e12(1);
  e21(2) = -e12(2);
  e30(0) = -e03(0);
  e30(1) = -e03(1);
  e30(2) = -e03(2);
  e31(0) = -e13(0);
  e31(1) = -e13(1);
  e31(2) = -e13(2);
  e32(0) = -e23(0);
  e32(1) = -e23(1);
  e32(2) = -e23(2);

  local_x(0) = e12(1) * e23(2) - e12(2) * e23(1);
  local_x(1) = e12(2) * e23(0) - e12(0) * e23(2);
  local_x(2) = e12(0) * e23(1) - e12(1) * e23(0);
  local_x.Scale(1.0 / local_x.Norm2());
  ale3_add_tria_stiffness(tet_1, tet_2, tet_3, tet_0, e02, l02, e31, l13, e21, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_1, tet_3, tet_2, tet_0, e03, l03, e21, l12, e31, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_2, tet_1, tet_3, tet_0, e01, l01, e32, l23, e12, local_x, sys_mat);

  local_x(0) = e23(1) * e03(2) - e23(2) * e03(1);
  local_x(1) = e23(2) * e03(0) - e23(0) * e03(2);
  local_x(2) = e23(0) * e03(1) - e23(1) * e03(0);
  local_x.Scale(1.0 / local_x.Norm2());
  ale3_add_tria_stiffness(tet_0, tet_2, tet_3, tet_1, e12, l12, e30, l03, e20, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_0, tet_3, tet_2, tet_1, e13, l13, e20, l02, e30, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_2, tet_0, tet_3, tet_1, e10, l01, e32, l23, e02, local_x, sys_mat);

  local_x(0) = e01(1) * e03(2) - e01(2) * e03(1);
  local_x(1) = e01(2) * e03(0) - e01(0) * e03(2);
  local_x(2) = e01(0) * e03(1) - e01(1) * e03(0);
  local_x.Scale(1.0 / local_x.Norm2());
  ale3_add_tria_stiffness(tet_0, tet_1, tet_3, tet_2, e21, l12, e30, l03, e10, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_1, tet_0, tet_3, tet_2, e20, l02, e31, l13, e01, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_0, tet_3, tet_1, tet_2, e23, l23, e10, l01, e30, local_x, sys_mat);

  local_x(0) = e01(1) * e12(2) - e01(2) * e12(1);
  local_x(1) = e01(2) * e12(0) - e01(0) * e12(2);
  local_x(2) = e01(0) * e12(1) - e01(1) * e12(0);
  local_x.Scale(1.0 / local_x.Norm2());
  ale3_add_tria_stiffness(tet_0, tet_1, tet_2, tet_3, e31, l13, e20, l02, e10, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_0, tet_2, tet_1, tet_3, e32, l23, e10, l01, e20, local_x, sys_mat);
  ale3_add_tria_stiffness(tet_1, tet_0, tet_2, tet_3, e30, l03, e21, l12, e01, local_x, sys_mat);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_tors_spring_tet4(
    LINALG::Matrix<3 * iel, 3 * iel>& sys_mat, const LINALG::Matrix<3, iel>& xyze)
{
  ale3_add_tetra_stiffness(0, 1, 2, 3, sys_mat, xyze);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_tors_spring_pyramid5(
    LINALG::Matrix<3 * iel, 3 * iel>& sys_mat, const LINALG::Matrix<3, iel>& xyze)
{
  ale3_add_tetra_stiffness(0, 1, 3, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(0, 1, 2, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 2, 3, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(0, 2, 3, 4, sys_mat, xyze);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_tors_spring_wedge6(
    LINALG::Matrix<3 * iel, 3 * iel>& sys_mat, const LINALG::Matrix<3, iel>& xyze)
{
  ale3_add_tetra_stiffness(2, 0, 1, 3, sys_mat, xyze);
  ale3_add_tetra_stiffness(2, 0, 1, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(2, 0, 1, 5, sys_mat, xyze);

  ale3_add_tetra_stiffness(3, 0, 1, 5, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 0, 2, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 1, 2, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 1, 2, 5, sys_mat, xyze);

  ale3_add_tetra_stiffness(4, 0, 1, 5, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 0, 2, 5, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 0, 3, 5, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 1, 3, 5, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 2, 3, 5, sys_mat, xyze);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_tors_spring_hex8(
    LINALG::Matrix<3 * iel, 3 * iel>& sys_mat, const LINALG::Matrix<3, iel>& xyze)
{
  // Use 8 tetrahedra to prevent node-face-penetration
  ale3_add_tetra_stiffness(0, 1, 3, 4, sys_mat, xyze);
  ale3_add_tetra_stiffness(0, 1, 2, 5, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 2, 3, 6, sys_mat, xyze);
  ale3_add_tetra_stiffness(0, 2, 3, 7, sys_mat, xyze);

  ale3_add_tetra_stiffness(0, 4, 5, 7, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 4, 5, 6, sys_mat, xyze);
  ale3_add_tetra_stiffness(2, 5, 6, 7, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 4, 6, 7, sys_mat, xyze);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline void DRT::ELEMENTS::Ale3_Impl<distype>::ale3_tors_spring_nurbs27(
    LINALG::Matrix<3 * iel, 3 * iel>& sys_mat, const LINALG::Matrix<3, iel>& xyze)
{
  //                          v
  //                         /
  //                        /
  //          X---------X---------X
  //         /|24      /|25      /|26
  //  w     / |       / |       / |
  //  ^    /  |      /  |      /  |
  //  |   X---------X---------X   |
  //  |  /|21 |    /|22 |    /|23 |
  //  | / |   X---/-|---X---/-|---X
  //   /  |  /|15/  |  /|16/  |  /|17                X---------X
  //  X---------X---------X   | / |                 /|7       /|6     0,1,3,4
  //  |18 |/  | |19 |/  | |20 |/  |                / |       / |      0,1,2,5
  //  |   X-----|---X-----|---X   |               /  |      /  |      1,2,3,6
  //  |  /|12 | |  /|13 | |  /|14 |              X---------X   |      0,2,3,7
  //  | / |   X-|-/-|---X-|-/-|---X              |4  |     |5  |
  //  |/  |  /6 |/  |  /7 |/  |  /8              |   X-----|---X      0,4,5,7
  //  X---------X---------X   | /                |  /3     |  /2      1,4,5,6
  //  |9  |/    |10 |/    |11 |/               | /       | /        2,5,6,7
  //  |   X-----|---X-----|---X                |/        |/         3,4,6,7
  //  |  /3     |  /4     |  /5             X---------X
  //  | /       | /       | /              0         1
  //  |/        |/        |/
  //  X---------X---------X ----->u
  //   0         1         2
  //

  // Use 8 tetrahedra to prevent node-face-penetration

  ale3_add_tetra_stiffness(0, 1, 3, 9, sys_mat, xyze);
  ale3_add_tetra_stiffness(0, 1, 4, 10, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 4, 3, 13, sys_mat, xyze);
  ale3_add_tetra_stiffness(0, 4, 3, 12, sys_mat, xyze);

  ale3_add_tetra_stiffness(0, 9, 10, 12, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 9, 10, 13, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 10, 13, 12, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 9, 13, 12, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(1, 2, 4, 10, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 2, 5, 11, sys_mat, xyze);
  ale3_add_tetra_stiffness(2, 5, 4, 14, sys_mat, xyze);
  ale3_add_tetra_stiffness(1, 5, 4, 13, sys_mat, xyze);

  ale3_add_tetra_stiffness(1, 10, 11, 13, sys_mat, xyze);
  ale3_add_tetra_stiffness(2, 10, 11, 14, sys_mat, xyze);
  ale3_add_tetra_stiffness(5, 11, 14, 13, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 10, 14, 13, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(4, 5, 7, 13, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 5, 8, 14, sys_mat, xyze);
  ale3_add_tetra_stiffness(5, 8, 7, 17, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 8, 7, 16, sys_mat, xyze);

  ale3_add_tetra_stiffness(4, 13, 14, 16, sys_mat, xyze);
  ale3_add_tetra_stiffness(5, 13, 14, 17, sys_mat, xyze);
  ale3_add_tetra_stiffness(8, 14, 17, 16, sys_mat, xyze);
  ale3_add_tetra_stiffness(7, 13, 17, 16, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(3, 4, 6, 12, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 4, 7, 13, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 7, 6, 16, sys_mat, xyze);
  ale3_add_tetra_stiffness(3, 7, 6, 15, sys_mat, xyze);

  ale3_add_tetra_stiffness(3, 12, 13, 15, sys_mat, xyze);
  ale3_add_tetra_stiffness(4, 12, 13, 16, sys_mat, xyze);
  ale3_add_tetra_stiffness(7, 13, 16, 15, sys_mat, xyze);
  ale3_add_tetra_stiffness(6, 12, 16, 15, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(9, 10, 12, 18, sys_mat, xyze);
  ale3_add_tetra_stiffness(9, 10, 13, 19, sys_mat, xyze);
  ale3_add_tetra_stiffness(10, 13, 12, 22, sys_mat, xyze);
  ale3_add_tetra_stiffness(9, 13, 12, 21, sys_mat, xyze);

  ale3_add_tetra_stiffness(9, 18, 19, 21, sys_mat, xyze);
  ale3_add_tetra_stiffness(10, 18, 19, 22, sys_mat, xyze);
  ale3_add_tetra_stiffness(13, 19, 22, 21, sys_mat, xyze);
  ale3_add_tetra_stiffness(12, 18, 22, 21, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(10, 11, 13, 19, sys_mat, xyze);
  ale3_add_tetra_stiffness(10, 11, 14, 20, sys_mat, xyze);
  ale3_add_tetra_stiffness(11, 14, 13, 23, sys_mat, xyze);
  ale3_add_tetra_stiffness(10, 14, 13, 22, sys_mat, xyze);

  ale3_add_tetra_stiffness(10, 19, 20, 22, sys_mat, xyze);
  ale3_add_tetra_stiffness(11, 19, 20, 23, sys_mat, xyze);
  ale3_add_tetra_stiffness(14, 20, 23, 22, sys_mat, xyze);
  ale3_add_tetra_stiffness(13, 19, 23, 22, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(13, 14, 16, 22, sys_mat, xyze);
  ale3_add_tetra_stiffness(13, 14, 17, 23, sys_mat, xyze);
  ale3_add_tetra_stiffness(14, 17, 16, 26, sys_mat, xyze);
  ale3_add_tetra_stiffness(13, 17, 16, 25, sys_mat, xyze);

  ale3_add_tetra_stiffness(13, 22, 23, 25, sys_mat, xyze);
  ale3_add_tetra_stiffness(14, 22, 23, 26, sys_mat, xyze);
  ale3_add_tetra_stiffness(17, 23, 26, 25, sys_mat, xyze);
  ale3_add_tetra_stiffness(16, 22, 26, 25, sys_mat, xyze);

  // -----------------------------------------------

  ale3_add_tetra_stiffness(12, 13, 15, 21, sys_mat, xyze);
  ale3_add_tetra_stiffness(12, 13, 16, 22, sys_mat, xyze);
  ale3_add_tetra_stiffness(13, 16, 15, 25, sys_mat, xyze);
  ale3_add_tetra_stiffness(12, 16, 15, 24, sys_mat, xyze);

  ale3_add_tetra_stiffness(12, 21, 22, 24, sys_mat, xyze);
  ale3_add_tetra_stiffness(13, 21, 22, 25, sys_mat, xyze);
  ale3_add_tetra_stiffness(16, 22, 25, 24, sys_mat, xyze);
  ale3_add_tetra_stiffness(15, 21, 25, 24, sys_mat, xyze);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Ale3_Impl<distype>::static_ke_spring(Ale3* ele,
    Epetra_SerialDenseMatrix& sys_mat_epetra, Epetra_SerialDenseVector& residual_epetra,
    const std::vector<double>& displacements, const bool spatialconfiguration)
{
  LINALG::Matrix<3 * iel, 3 * iel> sys_mat(sys_mat_epetra.A(), true);
  LINALG::Matrix<3 * iel, 1> residual(residual_epetra.A(), true);
  int node_i, node_j;  // end nodes of spring
  double length;       // length of edge
  double dx, dy, dz;   // deltas in each direction
  double factor;

  // get node coordinates
  LINALG::Matrix<3, iel> xyze;
  DRT::Node** nodes = ele->Nodes();
  for (int i = 0; i < iel; i++)
  {
    const double* x = nodes[i]->X();
    xyze(0, i) = x[0];
    xyze(1, i) = x[1];
    xyze(2, i) = x[2];
  }

  // update spatial configuration (if necessary)
  if (spatialconfiguration)
  {
    for (int i = 0; i < iel; i++)
    {
      xyze(0, i) += displacements[3 * i];
      xyze(1, i) += displacements[3 * i + 1];
      xyze(2, i) += displacements[3 * i + 2];
    }
  }

  // lineal springs from all corner nodes to all corner nodes
  // loop over all edges and diagonals of the element
  // We depend on the corner nodes to be the first nodes in xyze!
  for (node_i = 0; node_i < (numcnd - 1); ++node_i)
  {
    const int ti = 3 * node_i;
    const int tip = ti + 1;
    const int tipp = ti + 2;

    double ii = 0.0;
    double iip = 0.0;
    double iipp = 0.0;
    double ipi = 0.0;
    double ipip = 0.0;
    double ipipp = 0.0;
    double ippi = 0.0;
    double ippip = 0.0;
    double ippipp = 0.0;
    for (node_j = node_i + 1; node_j < numcnd; ++node_j)
    {
      const int tj = 3 * node_j;
      const int tjp = tj + 1;
      const int tjpp = tj + 2;
      ale3_edge_geometry(node_i, node_j, xyze, length, dx, dy, dz);
      factor = 1.0 / (length * length * length);
      const double dxx_l3 = dx * dx * factor;
      const double dxy_l3 = dx * dy * factor;
      const double dxz_l3 = dx * dz * factor;
      const double dyy_l3 = dy * dy * factor;
      const double dyz_l3 = dy * dz * factor;
      const double dzz_l3 = dz * dz * factor;

      // put values in 'element stiffness'
      // rows for node_i
      ii += dxx_l3;
      iip += dxy_l3;
      iipp += dxz_l3;

      sys_mat(ti, tj) -= dxx_l3;
      sys_mat(ti, tjp) -= dxy_l3;
      sys_mat(ti, tjpp) -= dxz_l3;

      ipi += dxy_l3;
      ipip += dyy_l3;
      ipipp += dyz_l3;

      sys_mat(tip, tj) -= dxy_l3;
      sys_mat(tip, tjp) -= dyy_l3;
      sys_mat(tip, tjpp) -= dyz_l3;

      ippi += dxz_l3;
      ippip += dyz_l3;
      ippipp += dzz_l3;

      sys_mat(tipp, tj) -= dxz_l3;
      sys_mat(tipp, tjp) -= dyz_l3;
      sys_mat(tipp, tjpp) -= dzz_l3;

      // rows for node_j
      sys_mat(tj, ti) -= dxx_l3;
      sys_mat(tj, tip) -= dxy_l3;
      sys_mat(tj, tipp) -= dxz_l3;

      sys_mat(tj, tj) += dxx_l3;
      sys_mat(tj, tjp) += dxy_l3;
      sys_mat(tj, tjpp) += dxz_l3;

      sys_mat(tjp, ti) -= dxy_l3;
      sys_mat(tjp, tip) -= dyy_l3;
      sys_mat(tjp, tipp) -= dyz_l3;

      sys_mat(tjp, tj) += dxy_l3;
      sys_mat(tjp, tjp) += dyy_l3;
      sys_mat(tjp, tjpp) += dyz_l3;

      sys_mat(tjpp, ti) -= dxz_l3;
      sys_mat(tjpp, tip) -= dyz_l3;
      sys_mat(tjpp, tipp) -= dzz_l3;

      sys_mat(tjpp, tj) += dxz_l3;
      sys_mat(tjpp, tjp) += dyz_l3;
      sys_mat(tjpp, tjpp) += dzz_l3;
    }
    sys_mat(ti, ti) += ii;
    sys_mat(ti, tip) += iip;
    sys_mat(ti, tipp) += iipp;
    sys_mat(tip, ti) += ipi;
    sys_mat(tip, tip) += ipip;
    sys_mat(tip, tipp) += ipipp;
    sys_mat(tipp, ti) += ippi;
    sys_mat(tipp, tip) += ippip;
    sys_mat(tipp, tipp) += ippipp;
  }

  // build in torsional springs
  // and put edge nodes on the middle of the respective edge
  switch (distype)
  {
    case DRT::Element::tet10:

      sys_mat(12, 0) = -0.5;
      sys_mat(12, 3) = -0.5;
      sys_mat(12, 12) = 1.0;
      sys_mat(13, 1) = -0.5;
      sys_mat(13, 4) = -0.5;
      sys_mat(13, 13) = 1.0;
      sys_mat(14, 2) = -0.5;
      sys_mat(14, 5) = -0.5;
      sys_mat(14, 14) = 1.0;

      sys_mat(15, 3) = -0.5;
      sys_mat(15, 6) = -0.5;
      sys_mat(15, 15) = 1.0;
      sys_mat(16, 4) = -0.5;
      sys_mat(16, 7) = -0.5;
      sys_mat(16, 16) = 1.0;
      sys_mat(17, 5) = -0.5;
      sys_mat(17, 8) = -0.5;
      sys_mat(17, 17) = 1.0;

      sys_mat(18, 6) = -0.5;
      sys_mat(18, 0) = -0.5;
      sys_mat(18, 18) = 1.0;
      sys_mat(19, 7) = -0.5;
      sys_mat(19, 1) = -0.5;
      sys_mat(19, 19) = 1.0;
      sys_mat(20, 8) = -0.5;
      sys_mat(20, 2) = -0.5;
      sys_mat(20, 20) = 1.0;

      sys_mat(21, 0) = -0.5;
      sys_mat(21, 9) = -0.5;
      sys_mat(21, 21) = 1.0;
      sys_mat(22, 1) = -0.5;
      sys_mat(22, 10) = -0.5;
      sys_mat(22, 22) = 1.0;
      sys_mat(23, 2) = -0.5;
      sys_mat(23, 11) = -0.5;
      sys_mat(23, 23) = 1.0;

      sys_mat(24, 3) = -0.5;
      sys_mat(24, 9) = -0.5;
      sys_mat(24, 24) = 1.0;
      sys_mat(25, 4) = -0.5;
      sys_mat(25, 10) = -0.5;
      sys_mat(25, 25) = 1.0;
      sys_mat(26, 5) = -0.5;
      sys_mat(26, 11) = -0.5;
      sys_mat(26, 26) = 1.0;

      sys_mat(27, 6) = -0.5;
      sys_mat(27, 9) = -0.5;
      sys_mat(27, 27) = 1.0;
      sys_mat(28, 7) = -0.5;
      sys_mat(28, 10) = -0.5;
      sys_mat(28, 28) = 1.0;
      sys_mat(29, 8) = -0.5;
      sys_mat(29, 11) = -0.5;
      sys_mat(29, 29) = 1.0;

      ale3_tors_spring_tet4(sys_mat, xyze);
      break;

    case DRT::Element::tet4:
      ale3_tors_spring_tet4(sys_mat, xyze);
      break;

    case DRT::Element::pyramid5:
      ale3_tors_spring_pyramid5(sys_mat, xyze);
      break;

    case DRT::Element::wedge15:

      for (int k = 0; k < 3; k++)
      {
        const int tk = 3 * k;
        const int t6k = tk + 3 * 6;
        const int a = 3 * ((k < 2) ? (k + 1) : 0);
        sys_mat(t6k, tk) = -0.5;
        sys_mat(t6k + 1, tk + 1) = -0.5;
        sys_mat(t6k + 2, tk + 2) = -0.5;
        sys_mat(t6k, a) = -0.5;
        sys_mat(t6k + 1, a + 1) = -0.5;
        sys_mat(t6k + 2, a + 2) = -0.5;
        sys_mat(t6k, t6k) = 1.0;
        sys_mat(t6k + 1, t6k + 1) = 1.0;
        sys_mat(t6k + 2, t6k + 2) = 1.0;
      }

      for (int k = 0; k < 3; k++)
      {
        const int tk = 3 * k;
        const int t3k = tk + 3 * 3;
        const int t9k = 3 * (9 + k);
        sys_mat(t9k, tk) = -0.5;
        sys_mat(t9k + 1, tk + 1) = -0.5;
        sys_mat(t9k + 2, tk + 2) = -0.5;
        sys_mat(t9k, t3k) = -0.5;
        sys_mat(t9k + 1, t3k + 1) = -0.5;
        sys_mat(t9k + 2, t3k + 2) = -0.5;
        sys_mat(t9k, t9k) = 1.0;
        sys_mat(t9k + 1, t9k + 1) = 1.0;
        sys_mat(t9k + 2, t9k + 2) = 1.0;
      }

      for (int k = 0; k < 3; k++)
      {
        const int t3k = 3 * (3 + k);
        const int t12k = 3 * (12 + k);
        const int a = 3 * (3 + ((k < 2) ? (k + 1) : 0));
        sys_mat(t12k, t3k) = -0.5;
        sys_mat(t12k + 1, t3k + 1) = -0.5;
        sys_mat(t12k + 2, t3k + 2) = -0.5;
        sys_mat(t12k, a) = -0.5;
        sys_mat(t12k + 1, a + 1) = -0.5;
        sys_mat(t12k + 2, a + 2) = -0.5;
        sys_mat(t12k, t12k) = 1.0;
        sys_mat(t12k + 1, t12k + 1) = 1.0;
        sys_mat(t12k + 2, t12k + 2) = 1.0;
      }

      ale3_tors_spring_wedge6(sys_mat, xyze);
      break;

    case DRT::Element::wedge6:
      ale3_tors_spring_wedge6(sys_mat, xyze);
      break;


    case DRT::Element::hex20:

      for (int k = 0; k < 4; k++)
      {
        const int tk = 3 * k;
        const int t8k = tk + 3 * 8;
        const int a = 3 * ((k < 3) ? (k + 1) : 0);
        sys_mat(t8k, tk) = -0.5;
        sys_mat(t8k + 1, tk + 1) = -0.5;
        sys_mat(t8k + 2, tk + 2) = -0.5;
        sys_mat(t8k, a) = -0.5;
        sys_mat(t8k + 1, a + 1) = -0.5;
        sys_mat(t8k + 2, a + 2) = -0.5;
        sys_mat(t8k, t8k) = 1.0;
        sys_mat(t8k + 1, t8k + 1) = 1.0;
        sys_mat(t8k + 2, t8k + 2) = 1.0;
      }

      for (int k = 0; k < 4; k++)
      {
        const int tk = 3 * k;
        const int t4k = tk + 3 * 4;
        const int t12k = 3 * (12 + k);
        sys_mat(t12k, tk) = -0.5;
        sys_mat(t12k + 1, tk + 1) = -0.5;
        sys_mat(t12k + 2, tk + 2) = -0.5;
        sys_mat(t12k, t4k) = -0.5;
        sys_mat(t12k + 1, t4k + 1) = -0.5;
        sys_mat(t12k + 2, t4k + 2) = -0.5;
        sys_mat(t12k, t12k) = 1.0;
        sys_mat(t12k + 1, t12k + 1) = 1.0;
        sys_mat(t12k + 2, t12k + 2) = 1.0;
      }

      for (int k = 0; k < 4; k++)
      {
        const int t4k = 3 * (4 + k);
        const int t16k = 3 * (16 + k);
        const int a = 3 * (4 + ((k < 3) ? (k + 1) : 0));
        sys_mat(t16k, t4k) = -0.5;
        sys_mat(t16k + 1, t4k + 1) = -0.5;
        sys_mat(t16k + 2, t4k + 2) = -0.5;
        sys_mat(t16k, a) = -0.5;
        sys_mat(t16k + 1, a + 1) = -0.5;
        sys_mat(t16k + 2, a + 2) = -0.5;
        sys_mat(t16k, t16k) = 1.0;
        sys_mat(t16k + 1, t16k + 1) = 1.0;
        sys_mat(t16k + 2, t16k + 2) = 1.0;
      }

      ale3_tors_spring_hex8(sys_mat, xyze);
      break;

    case DRT::Element::hex27:

      for (int k = 0; k < 4; k++)
      {
        const int tk = 3 * k;
        const int t8k = tk + 3 * 8;
        const int a = 3 * ((k < 3) ? (k + 1) : 0);
        sys_mat(t8k, tk) = -0.5;
        sys_mat(t8k + 1, tk + 1) = -0.5;
        sys_mat(t8k + 2, tk + 2) = -0.5;
        sys_mat(t8k, a) = -0.5;
        sys_mat(t8k + 1, a + 1) = -0.5;
        sys_mat(t8k + 2, a + 2) = -0.5;
        sys_mat(t8k, t8k) = 1.0;
        sys_mat(t8k + 1, t8k + 1) = 1.0;
        sys_mat(t8k + 2, t8k + 2) = 1.0;
      }

      for (int k = 0; k < 4; k++)
      {
        const int tk = 3 * k;
        const int t4k = tk + 3 * 4;
        const int t12k = tk + 3 * 12;
        sys_mat(t12k, tk) = -0.5;
        sys_mat(t12k + 1, tk + 1) = -0.5;
        sys_mat(t12k + 2, tk + 2) = -0.5;
        sys_mat(t12k, t4k) = -0.5;
        sys_mat(t12k + 1, t4k + 1) = -0.5;
        sys_mat(t12k + 2, t4k + 2) = -0.5;
        sys_mat(t12k, t12k) = 1.0;
        sys_mat(t12k + 1, t12k + 1) = 1.0;
        sys_mat(t12k + 2, t12k + 2) = 1.0;
      }

      for (int k = 0; k < 4; k++)
      {
        const int t4k = 3 * (4 + k);
        const int t16k = 3 * (16 + k);
        const int a = 3 * (4 + ((k < 3) ? (k + 1) : 0));
        sys_mat(t16k, t4k) = -0.5;
        sys_mat(t16k + 1, t4k + 1) = -0.5;
        sys_mat(t16k + 2, t4k + 2) = -0.5;
        sys_mat(t16k, a) = -0.5;
        sys_mat(t16k + 1, a + 1) = -0.5;
        sys_mat(t16k + 2, a + 2) = -0.5;
        sys_mat(t16k, t16k) = 1.0;
        sys_mat(t16k + 1, t16k + 1) = 1.0;
        sys_mat(t16k + 2, t16k + 2) = 1.0;
      }

      sys_mat(3 * 20, 3 * 8) = -0.5;
      sys_mat(3 * 20 + 1, 3 * 8 + 1) = -0.5;
      sys_mat(3 * 20 + 2, 3 * 8 + 2) = -0.5;
      sys_mat(3 * 20, 3 * 10) = -0.5;
      sys_mat(3 * 20 + 1, 3 * 10 + 1) = -0.5;
      sys_mat(3 * 20 + 2, 3 * 10 + 2) = -0.5;
      sys_mat(3 * 20, 3 * 20) = 1.0;
      sys_mat(3 * 20 + 1, 3 * 20 + 1) = 1.0;
      sys_mat(3 * 20 + 2, 3 * 20 + 2) = 1.0;

      for (int k = 0; k < 4; k++)
      {
        const int t8k = 3 * (8 + k);
        const int t16k = 3 * (16 + k);
        const int t21k = 3 * (21 + k);
        sys_mat(t21k, t8k) = -0.5;
        sys_mat(t21k + 1, t8k + 1) = -0.5;
        sys_mat(t21k + 2, t8k + 2) = -0.5;
        sys_mat(t21k, t16k) = -0.5;
        sys_mat(t21k + 1, t16k + 1) = -0.5;
        sys_mat(t21k + 2, t16k + 2) = -0.5;
        sys_mat(t21k, t21k) = 1.0;
        sys_mat(t21k + 1, t21k + 1) = 1.0;
        sys_mat(t21k + 2, t21k + 2) = 1.0;
      }

      sys_mat(3 * 25, 3 * 16) = -0.5;
      sys_mat(3 * 25 + 1, 3 * 16 + 1) = -0.5;
      sys_mat(3 * 25 + 2, 3 * 16 + 2) = -0.5;
      sys_mat(3 * 25, 3 * 18) = -0.5;
      sys_mat(3 * 25 + 1, 3 * 18 + 1) = -0.5;
      sys_mat(3 * 25 + 2, 3 * 18 + 2) = -0.5;
      sys_mat(3 * 25, 3 * 25) = 1.0;
      sys_mat(3 * 25 + 1, 3 * 25 + 1) = 1.0;
      sys_mat(3 * 25 + 2, 3 * 25 + 2) = 1.0;

      sys_mat(3 * 26, 3 * 21) = -0.5;
      sys_mat(3 * 26 + 1, 3 * 21 + 1) = -0.5;
      sys_mat(3 * 26 + 2, 3 * 21 + 2) = -0.5;
      sys_mat(3 * 26, 3 * 23) = -0.5;
      sys_mat(3 * 26 + 1, 3 * 23 + 1) = -0.5;
      sys_mat(3 * 26 + 2, 3 * 23 + 2) = -0.5;
      sys_mat(3 * 26, 3 * 26) = 1.0;
      sys_mat(3 * 26 + 1, 3 * 26 + 1) = 1.0;
      sys_mat(3 * 26 + 2, 3 * 26 + 2) = 1.0;

      ale3_tors_spring_hex8(sys_mat, xyze);
      break;

    case DRT::Element::hex8:
      ale3_tors_spring_hex8(sys_mat, xyze);
      break;

    default:
      dserror("unknown distype in ale spring dynamic");
      break;
  }

  // compute residual
  residual.Scale(0.0);
  for (int i = 0; i < 3 * iel; ++i)
    for (int j = 0; j < 3 * iel; ++j) residual(i, 0) += sys_mat(i, j) * displacements[j];

  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Ale3_Impl<distype>::static_ke_nonlinear(Ale3* ele, DRT::Discretization& dis,
    std::vector<int>& lm, Epetra_SerialDenseMatrix& sys_mat_epetra,
    Epetra_SerialDenseVector& residual_epetra, std::vector<double>& my_dispnp,
    Teuchos::ParameterList& params, const bool spatialconfiguration)
{
  const int numdof = NODDOF_ALE3 * iel;
  // A view to sys_mat_epetra
  LINALG::Matrix<numdof, numdof> sys_mat(sys_mat_epetra.A(), true);
  // update element geometry
  LINALG::Matrix<iel, NUMDIM_ALE3> xrefe;  // material coord. of element
  LINALG::Matrix<iel, NUMDIM_ALE3> xcurr;  // current  coord. of element
  for (int i = 0; i < iel; ++i)
  {
    xrefe(i, 0) = ele->Nodes()[i]->X()[0];
    xrefe(i, 1) = ele->Nodes()[i]->X()[1];
    xrefe(i, 2) = ele->Nodes()[i]->X()[2];

    xcurr(i, 0) = xrefe(i, 0);
    xcurr(i, 1) = xrefe(i, 1);
    xcurr(i, 2) = xrefe(i, 2);

    if (spatialconfiguration)
    {
      xcurr(i, 0) += my_dispnp[i * NODDOF_ALE3 + 0];
      xcurr(i, 1) += my_dispnp[i * NODDOF_ALE3 + 1];
      xcurr(i, 2) += my_dispnp[i * NODDOF_ALE3 + 2];
    }
  }
  // --------------------------------------------------
  // Now do the nurbs specific stuff
  std::vector<Epetra_SerialDenseVector> myknots;
  LINALG::Matrix<iel, 1> weights(iel);

  if (distype == DRT::Element::nurbs8 || distype == DRT::Element::nurbs27)
  {
    DRT::NURBS::NurbsDiscretization* nurbsdis =
        dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(dis));

    myknots.resize(3);
    bool zero_size = (*((*nurbsdis).GetKnotVector())).GetEleKnots(myknots, ele->Id());

    if (zero_size)
    {
      return;
    }

    for (int inode = 0; inode < iel; ++inode)
    {
      DRT::NURBS::ControlPoint* cp = dynamic_cast<DRT::NURBS::ControlPoint*>(ele->Nodes()[inode]);
      weights(inode) = cp->W();
    }
  }
  /*----------------------------------------- declaration of variables ---*/
  LINALG::Matrix<iel, 1> funct;
  LINALG::Matrix<NUMDIM_ALE3, iel> deriv;
  // LINALG::Matrix<3,  iel> deriv;
  LINALG::Matrix<3, 3> xjm;
  LINALG::Matrix<3, 3> xji;
  LINALG::Matrix<6, numdof> bop;
  LINALG::Matrix<6, 6> D(true);
  // gaussian points
  const CORE::DRT::UTILS::GaussRule3D gaussrule = getOptimalGaussrule();
  const CORE::DRT::UTILS::IntegrationPoints3D intpoints(gaussrule);

  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  for (int iquad = 0; iquad < intpoints.nquad; ++iquad)
  {
    const double e1 = intpoints.qxg[iquad][0];
    const double e2 = intpoints.qxg[iquad][1];
    const double e3 = intpoints.qxg[iquad][2];
    // get values of shape functions and derivatives in the gausspoint
    if (distype != DRT::Element::nurbs8 && distype != DRT::Element::nurbs27)
    {
      // shape functions and their derivatives for polynomials
      CORE::DRT::UTILS::shape_function_3D(funct, e1, e2, e3, distype);
      CORE::DRT::UTILS::shape_function_3D_deriv1(deriv, e1, e2, e3, distype);
    }
    else
    {
      // nurbs version
      Epetra_SerialDenseVector gp(3);
      gp(0) = e1;
      gp(1) = e2;
      gp(2) = e3;

      CORE::DRT::NURBS::UTILS::nurbs_get_3D_funct_deriv(
          funct, deriv, gp, myknots, weights, distype);
    }
    /* compute the Jacobian matrix which looks like:
    **         [ x_,r  y_,r  z_,r ]
    **     J = [ x_,s  y_,s  z_,s ]
    **         [ x_,t  y_,t  z_,t ]
    */
    LINALG::Matrix<NUMDIM_ALE3, NUMDIM_ALE3> jac, jacinv;
    jac.MultiplyNN(deriv, xrefe);
    const double detJ = jac.Invert();

    if (abs(detJ) < 1E-16)
      dserror("ZERO JACOBIAN DETERMINANT");
    else if (detJ < 0.0)
      dserror("NEGATIVE JACOBIAN DETERMINANT");

    /* compute derivatives N_XYZ at gp w.r.t. material coordinates
    ** by solving   Jac . N_XYZ = N_rst   for N_XYZ
    ** Inverse of Jacobian is therefore not explicitly computed
    */
    LINALG::Matrix<NUMDIM_ALE3, iel> N_XYZ;
    N_XYZ.MultiplyNN(jac, deriv);

    // (material) deformation gradient F = d xcurr / d xrefe = xcurr^T * N_XYZ^T
    LINALG::Matrix<NUMDIM_ALE3, NUMDIM_ALE3> defgrd;
    defgrd.MultiplyTT(xcurr, N_XYZ);

    // Right Cauchy-Green tensor = F^T * F
    LINALG::Matrix<NUMDIM_ALE3, NUMDIM_ALE3> cauchygreen;
    cauchygreen.MultiplyTN(defgrd, defgrd);
    // Green-Lagrange strains matrix E = 0.5 * (Cauchygreen - Identity)
    // GL strain vector glstrain={E11,E22,E33,2*E12,2*E23,2*E31}
    LINALG::Matrix<MAT::NUM_STRESS_3D, 1> glstrain;
    glstrain(0) = 0.5 * (cauchygreen(0, 0) - 1.0);
    glstrain(1) = 0.5 * (cauchygreen(1, 1) - 1.0);
    glstrain(2) = 0.5 * (cauchygreen(2, 2) - 1.0);
    glstrain(3) = cauchygreen(0, 1);
    glstrain(4) = cauchygreen(1, 2);
    glstrain(5) = cauchygreen(2, 0);

    /* non-linear B-operator (may so be called, meaning
    ** of B-operator is not so sharp in the non-linear realm) *
    ** B = F . Bl *
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
    LINALG::Matrix<MAT::NUM_STRESS_3D, numdof> bop;
    for (int i = 0; i < iel; ++i)
    {
      bop(0, NODDOF_ALE3 * i + 0) = defgrd(0, 0) * N_XYZ(0, i);
      bop(0, NODDOF_ALE3 * i + 1) = defgrd(1, 0) * N_XYZ(0, i);
      bop(0, NODDOF_ALE3 * i + 2) = defgrd(2, 0) * N_XYZ(0, i);
      bop(1, NODDOF_ALE3 * i + 0) = defgrd(0, 1) * N_XYZ(1, i);
      bop(1, NODDOF_ALE3 * i + 1) = defgrd(1, 1) * N_XYZ(1, i);
      bop(1, NODDOF_ALE3 * i + 2) = defgrd(2, 1) * N_XYZ(1, i);
      bop(2, NODDOF_ALE3 * i + 0) = defgrd(0, 2) * N_XYZ(2, i);
      bop(2, NODDOF_ALE3 * i + 1) = defgrd(1, 2) * N_XYZ(2, i);
      bop(2, NODDOF_ALE3 * i + 2) = defgrd(2, 2) * N_XYZ(2, i);
      /* ~~~ */
      bop(3, NODDOF_ALE3 * i + 0) = defgrd(0, 0) * N_XYZ(1, i) + defgrd(0, 1) * N_XYZ(0, i);
      bop(3, NODDOF_ALE3 * i + 1) = defgrd(1, 0) * N_XYZ(1, i) + defgrd(1, 1) * N_XYZ(0, i);
      bop(3, NODDOF_ALE3 * i + 2) = defgrd(2, 0) * N_XYZ(1, i) + defgrd(2, 1) * N_XYZ(0, i);
      bop(4, NODDOF_ALE3 * i + 0) = defgrd(0, 1) * N_XYZ(2, i) + defgrd(0, 2) * N_XYZ(1, i);
      bop(4, NODDOF_ALE3 * i + 1) = defgrd(1, 1) * N_XYZ(2, i) + defgrd(1, 2) * N_XYZ(1, i);
      bop(4, NODDOF_ALE3 * i + 2) = defgrd(2, 1) * N_XYZ(2, i) + defgrd(2, 2) * N_XYZ(1, i);
      bop(5, NODDOF_ALE3 * i + 0) = defgrd(0, 2) * N_XYZ(0, i) + defgrd(0, 0) * N_XYZ(2, i);
      bop(5, NODDOF_ALE3 * i + 1) = defgrd(1, 2) * N_XYZ(0, i) + defgrd(1, 0) * N_XYZ(2, i);
      bop(5, NODDOF_ALE3 * i + 2) = defgrd(2, 2) * N_XYZ(0, i) + defgrd(2, 0) * N_XYZ(2, i);
    }
    // call material law cccccccccccccccccccccccccccccccccccccccccccccccccccccc
    LINALG::Matrix<MAT::NUM_STRESS_3D, MAT::NUM_STRESS_3D> cmat_f(true);
    LINALG::Matrix<MAT::NUM_STRESS_3D, 1> stress_f(true);
    LINALG::Matrix<MAT::NUM_STRESS_3D, 1> glstrain_f(glstrain.A());
    // QUICK HACK until so_disp exclusively uses LINALG::Matrix!!!!!
    LINALG::Matrix<NUMDIM_ALE3, NUMDIM_ALE3> fixed_defgrd(defgrd);
    Teuchos::RCP<MAT::So3Material> so3mat =
        Teuchos::rcp_dynamic_cast<MAT::So3Material>(ele->Material());
    so3mat->Evaluate(&fixed_defgrd, &glstrain_f, params, &stress_f, &cmat_f, iquad, ele->Id());
    // end of call material law ccccccccccccccccccccccccccccccccccccccccccccccc

    // integrate internal force vector f = f + (B^T . sigma) * detJ * w(gp)
    LINALG::Matrix<numdof, 1> residual(residual_epetra, true);
    residual.MultiplyTN(detJ * intpoints.qwgt[iquad], bop, stress_f, 1.0);

    // integrate `elastic' and `initial-displacement' stiffness matrix
    // keu = keu + (B^T . C . B) * detJ * w(gp)
    LINALG::Matrix<MAT::NUM_STRESS_3D, numdof> cb;
    cb.MultiplyNN(cmat_f, bop);  // temporary C . B
    sys_mat.MultiplyTN(detJ * intpoints.qwgt[iquad], bop, cb, 1.0);

    // integrate `geometric' stiffness matrix and add to keu *****************
    LINALG::Matrix<MAT::NUM_STRESS_3D, 1> sfac(stress_f);  // auxiliary integrated stress
    sfac.Scale(detJ * intpoints.qwgt[iquad]);  // detJ*w(gp)*[S11,S22,S33,S12=S21,S23=S32,S13=S31]
    double SmB_L[NUMDIM_ALE3];                 // intermediate Sm.B_L
    // kgeo += (B_L^T . sigma . B_L) * detJ * w(gp)  with B_L = Ni,Xj see NiliFEM-Skript
    for (int inod = 0; inod < iel; ++inod)
    {
      SmB_L[0] = sfac(0) * N_XYZ(0, inod) + sfac(3) * N_XYZ(1, inod) + sfac(5) * N_XYZ(2, inod);
      SmB_L[1] = sfac(3) * N_XYZ(0, inod) + sfac(1) * N_XYZ(1, inod) + sfac(4) * N_XYZ(2, inod);
      SmB_L[2] = sfac(5) * N_XYZ(0, inod) + sfac(4) * N_XYZ(1, inod) + sfac(2) * N_XYZ(2, inod);
      for (int jnod = 0; jnod < iel; ++jnod)
      {
        double bopstrbop = 0.0;  // intermediate value
        for (int idim = 0; idim < NUMDIM_ALE3; ++idim) bopstrbop += N_XYZ(idim, jnod) * SmB_L[idim];
        (sys_mat)(NUMDIM_ALE3 * inod + 0, NUMDIM_ALE3 * jnod + 0) += bopstrbop;
        (sys_mat)(NUMDIM_ALE3 * inod + 1, NUMDIM_ALE3 * jnod + 1) += bopstrbop;
        (sys_mat)(NUMDIM_ALE3 * inod + 2, NUMDIM_ALE3 * jnod + 2) += bopstrbop;
      }
    }  // end of integrate `geometric' stiffness ******************************

    /* =========================================================================*/
  } /* ==================================================== end of Loop over GP */
  /* =========================================================================*/

  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Ale3_Impl<distype>::static_ke_laplace(Ale3* ele, DRT::Discretization& dis,
    Epetra_SerialDenseMatrix& sys_mat_epetra, Epetra_SerialDenseVector& residual,
    std::vector<double>& my_dispnp, Teuchos::RCP<MAT::Material> material,
    const bool spatialconfiguration)
{
  //  dserror("We don't know what is really done in the element evaluation"
  //      "of the Laplace smoothing strategy. Check this CAREFULLY before"
  //      "using it.");

  const int nd = 3 * iel;
  // A view to sys_mat_epetra
  LINALG::Matrix<nd, nd> sys_mat(sys_mat_epetra.A(), true);

  //  get material using class StVenantKirchhoff
  //  if (material->MaterialType()!=INPAR::MAT::m_stvenant)
  //    dserror("stvenant material expected but got type %d", material->MaterialType());
  //  MAT::StVenantKirchhoff* actmat = static_cast<MAT::StVenantKirchhoff*>(material.get());

  LINALG::Matrix<3, iel> xyze;

  // get node coordinates
  DRT::Node** nodes = ele->Nodes();
  for (int i = 0; i < iel; i++)
  {
    const double* x = nodes[i]->X();
    xyze(0, i) = x[0];
    xyze(1, i) = x[1];
    xyze(2, i) = x[2];
  }

  // update spatial configuration if necessary
  if (spatialconfiguration)
  {
    for (int i = 0; i < iel; i++)
    {
      xyze(0, i) += my_dispnp[3 * i + 0];
      xyze(1, i) += my_dispnp[3 * i + 1];
      xyze(2, i) += my_dispnp[3 * i + 2];
    }
  }

  // --------------------------------------------------
  // Now do the nurbs specific stuff
  std::vector<Epetra_SerialDenseVector> myknots(3);
  LINALG::Matrix<iel, 1> weights(iel);

  if (distype == DRT::Element::nurbs8 or distype == DRT::Element::nurbs27)
  {
    DRT::NURBS::NurbsDiscretization* nurbsdis =
        dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(dis));

    bool zero_size = (*((*nurbsdis).GetKnotVector())).GetEleKnots(myknots, ele->Id());

    if (zero_size) return;

    for (int inode = 0; inode < iel; ++inode)
    {
      DRT::NURBS::ControlPoint* cp = dynamic_cast<DRT::NURBS::ControlPoint*>(ele->Nodes()[inode]);

      weights(inode) = cp->W();
    }
  }

  /*----------------------------------------- declaration of variables ---*/
  LINALG::Matrix<iel, 1> funct(true);
  LINALG::Matrix<3, iel> deriv(true);
  LINALG::Matrix<3, 3> xjm(true);
  LINALG::Matrix<3, 3> xji(true);
  LINALG::Matrix<3, iel> deriv_xy(true);
  LINALG::Matrix<iel, iel> tempmat(true);
  LINALG::Matrix<3 * iel, 1> tempmat2(true);

  double vol = 0.;

  // gaussian points
  const CORE::DRT::UTILS::GaussRule3D gaussrule = getOptimalGaussrule();
  const CORE::DRT::UTILS::IntegrationPoints3D intpoints(gaussrule);

  // This whole method was copied from the ALE2 element and extended to 3D.
  // ToDo: proper computation and usage of min_detF. Is there any detailed literature
  //       on this approach?

  // integration loops
  for (int iquad = 0; iquad < intpoints.nquad; iquad++)
  {
    const double e1 = intpoints.qxg[iquad][0];
    const double e2 = intpoints.qxg[iquad][1];
    const double e3 = intpoints.qxg[iquad][2];


    // get values of shape functions and derivatives in the gausspoint
    if (distype != DRT::Element::nurbs8 && distype != DRT::Element::nurbs27)
    {
      // shape functions and their derivatives for polynomials
      CORE::DRT::UTILS::shape_function_3D(funct, e1, e2, e3, distype);
      CORE::DRT::UTILS::shape_function_3D_deriv1(deriv, e1, e2, e3, distype);
    }
    else
    {
      // nurbs version
      Epetra_SerialDenseVector gp(3);
      gp(0) = e1;
      gp(1) = e2;
      gp(2) = e3;

      CORE::DRT::NURBS::UTILS::nurbs_get_3D_funct_deriv(
          funct, deriv, gp, myknots, weights, distype);
    }

    // determine jacobian matrix at point r,s,t
    xjm.MultiplyNT(deriv, xyze);

    // determinant and inverse of jacobian
    const double det = xji.Invert(xjm);

    // calculate element volume
    const double fac = intpoints.qwgt[iquad] * det;
    vol += fac;

    // compute global derivatives
    deriv_xy.Multiply(xji, deriv);

    /*------------------------- diffusivity depends on displacement ---*/
    //   This is how it is done in the 2d implementation ALE2:
    //   const double k_diff = 1.0/min_detF/min_detF;

    //   This is how we do it here for the time being due to lack of detailed knowledge
    //   on the underlying concept of min_detF (see comments above). We simply
    //   use here the Jacobi determinant evaluated at the Gauss points instead of
    //   min_detF determined at corner node positions.
    const double k_diff = 1.0;  // 1.0/(det*det);
    //   Due to this heuristic, small elements are artificially made stiffer
    //   and large elements are made softer
    /*------------------------------- sort it into stiffness matrix ---*/

    tempmat.MultiplyTN(fac * k_diff, deriv_xy, deriv_xy, 1.0);

  }  // integration loop

  // insert finished temporary matrix
  for (int d = 0; d < 3; d++)
    for (int i = 0; i < iel; i++)
      for (int j = 0; j < iel; j++) sys_mat(i * 3 + d, j * 3 + d) += tempmat(i, j);

  // compute residual vector
  residual.Scale(0.0);
  for (int i = 0; i < nd; ++i)
    for (int j = 0; j < nd; ++j) residual[i] += sys_mat(i, j) * my_dispnp[j];

  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
inline CORE::DRT::UTILS::GaussRule3D DRT::ELEMENTS::Ale3_Impl<distype>::getOptimalGaussrule()
{
  switch (distype)
  {
    case DRT::Element::hex8:
    case DRT::Element::nurbs8:
      return CORE::DRT::UTILS::GaussRule3D::hex_8point;
    case DRT::Element::hex20:
    case DRT::Element::hex27:
    case DRT::Element::nurbs27:
      return CORE::DRT::UTILS::GaussRule3D::hex_27point;
    case DRT::Element::tet4:
      return CORE::DRT::UTILS::GaussRule3D::tet_4point;
    case DRT::Element::tet10:
      return CORE::DRT::UTILS::GaussRule3D::tet_5point;
    case DRT::Element::wedge6:
      return CORE::DRT::UTILS::GaussRule3D::wedge_6point;
    case DRT::Element::wedge15:
      return CORE::DRT::UTILS::GaussRule3D::wedge_9point;
    case DRT::Element::pyramid5:
      return CORE::DRT::UTILS::GaussRule3D::pyramid_8point;
    default:
      dserror("unknown number of nodes for gaussrule initialization");
      return CORE::DRT::UTILS::GaussRule3D::undefined;
  }
}
