/*!
\file geometry_pair_line_to_volume.cpp

\brief Class for interaction of lines and volumes.

\level 1
\maintainer Ivo Steinbrecher
*/


#include "geometry_pair_line_to_volume.H"
#include "geometry_pair_element_types.H"
#include "geometry_pair_utility_classes.H"
#include "geometry_pair_constants.H"

#include "../drt_lib/drt_dserror.H"
#include "../drt_beam3/beam3.H"


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
template <typename scalar_type_get_pos>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::GetElement1Position(
    const scalar_type& eta, const LINALG::TMatrix<scalar_type_get_pos, line::n_dof_, 1>& q,
    LINALG::TMatrix<scalar_type_get_pos, 3, 1>& r) const
{
  // Matrix for shape function values.
  LINALG::TMatrix<scalar_type, 1, line::n_nodes_ * line::n_val_> N(true);

  // Get discretization type.
  const DRT::Element::DiscretizationType distype = Element1()->Shape();

  if (line::n_val_ == 1)
  {
    dserror("One nodal value for line elements not yet implemented!");
    DRT::UTILS::shape_function_1D(N, eta, distype);
  }
  else if (line::n_val_ == 2)
  {
    double length = (dynamic_cast<const DRT::ELEMENTS::Beam3Base*>(Element1()))->RefLength();
    const DRT::Element::DiscretizationType distype1herm = DRT::Element::line2;

    // Get values of shape functions.
    DRT::UTILS::shape_function_hermite_1D(N, eta, length, distype1herm);

    // Calculate the position.
    r.Clear();
    for (unsigned int dim = 0; dim < 3; dim++)
    {
      for (unsigned int node = 0; node < line::n_nodes_; node++)
      {
        for (unsigned int val = 0; val < line::n_val_; val++)
        {
          r(dim) += q(3 * line::n_val_ * node + 3 * val + dim) * N(line::n_val_ * node + val);
        }
      }
    }
  }
  else
    dserror(
        "Only line elements with one (nodal positions) or two "
        "(nodal positions + nodal tangents) values are valid!");
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line,
    volume>::GetElement1PositionDerivative(const scalar_type& eta,
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q,
    LINALG::TMatrix<scalar_type, 3, 1>& dr) const
{
  // Matrix for shape function values.
  LINALG::TMatrix<scalar_type, 1, line::n_dof_> dN(true);

  // Get discretization type.
  const DRT::Element::DiscretizationType distype = Element1()->Shape();

  if (line::n_val_ == 1)
  {
    dserror("One nodal value for line elements not yet implemented!");
    DRT::UTILS::shape_function_1D_deriv1(dN, eta, distype);
  }
  else if (line::n_val_ == 2)
  {
    double length = (dynamic_cast<const DRT::ELEMENTS::Beam3Base*>(Element1()))->RefLength();
    const DRT::Element::DiscretizationType distype1herm = DRT::Element::line2;

    // Get values of shape functions.
    DRT::UTILS::shape_function_hermite_1D_deriv1(dN, eta, length, distype1herm);

    // Calculate the position.
    dr.Clear();
    for (unsigned int dim = 0; dim < 3; dim++)
    {
      for (unsigned int node = 0; node < line::n_nodes_; node++)
      {
        for (unsigned int val = 0; val < line::n_val_; val++)
        {
          dr(dim) += q(3 * line::n_val_ * node + 3 * val + dim) * dN(line::n_val_ * node + val);
        }
      }
    }
  }
  else
    dserror(
        "Only line elements with one (nodal positions) or two "
        "(nodal positions + nodal tangents) values are valid!");
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
template <typename scalar_type_get_pos>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::GetElement2Position(
    const LINALG::TMatrix<scalar_type, 3, 1>& xi,
    const LINALG::TMatrix<scalar_type_get_pos, volume::n_dof_, 1>& q,
    LINALG::TMatrix<scalar_type_get_pos, 3, 1>& r) const
{
  // Matrix for shape function values.
  LINALG::TMatrix<scalar_type, 1, volume::n_nodes_ * volume::n_val_> N(true);

  // Check what type of volume was given.
  if (volume::n_val_ != 1) dserror("Only volume elements with one nodal values are implemented!");

  // Clear shape function matrix.
  N.Clear();

  // Get the shape functions.
  DRT::UTILS::shape_function_3D(N, xi(0), xi(1), xi(2), Element2()->Shape());

  // Calculate the position.
  r.Clear();
  for (unsigned int dim = 0; dim < 3; dim++)
  {
    for (unsigned int node = 0; node < volume::n_nodes_; node++)
    {
      r(dim) += q(3 * node + dim) * N(node);
    }
  }
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line,
    volume>::GetElement2PositionDerivative(const LINALG::TMatrix<scalar_type, 3, 1>& xi,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q,
    LINALG::TMatrix<scalar_type, 3, 3>& dr) const
{
  // Matrix for shape function values.
  LINALG::TMatrix<scalar_type, 3, volume::n_nodes_ * volume::n_val_> dN(true);

  // Check what type of volume was given.
  if (volume::n_val_ != 1) dserror("Only volume elements with one nodal values are implemented!");

  // Clear shape function matrix.
  dN.Clear();

  // Get the shape functions.
  DRT::UTILS::shape_function_3D_deriv1(dN, xi(0), xi(1), xi(2), Element2()->Shape());

  // Calculate the position.
  dr.Clear();
  for (unsigned int dim = 0; dim < 3; dim++)
  {
    for (unsigned int direction = 0; direction < 3; direction++)
    {
      for (unsigned int node = 0; node < volume::n_nodes_; node++)
      {
        dr(dim, direction) += q(3 * node + dim) * dN(direction, node);
      }
    }
  }
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::ProjectPointOnLineToVolume(
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume, const scalar_type& eta,
    LINALG::TMatrix<scalar_type, 3, 1>& xi, ProjectionResult& projection_result) const
{
  // Initialize data structures
  // Point on line.
  LINALG::TMatrix<scalar_type, 3, 1> r_line;

  // Point on volume.
  LINALG::TMatrix<scalar_type, 3, 1> r_volume;

  // Jacobian.
  LINALG::TMatrix<scalar_type, 3, 3> J;
  LINALG::TMatrix<scalar_type, 3, 3> J_inverse;

  // Increment of xi.
  LINALG::TMatrix<scalar_type, 3, 1> delta_xi;

  // Residuum.
  LINALG::TMatrix<scalar_type, 3, 1> residuum;

  // Reset the projection result flag.
  projection_result = ProjectionResult::projection_not_found;

  // Local Newton iteration.
  {
    // Get the position on the beam that the solid should match.
    GetElement1Position(eta, q_line, r_line);

    unsigned int counter = 0;
    while (counter < CONSTANTS::local_newton_iter_max)
    {
      // Get the point coordinates on the volume.
      GetElement2Position(xi, q_volume, r_volume);

      // Evaluate the residuum $r_{volume} - r_{line} = R_{pos}$
      residuum = r_volume;
      residuum -= r_line;

      // Check if tolerance is fulfilled.
      if (residuum.Norm2() < CONSTANTS::local_newton_res_tol)
      {
        // We only check xi, as eta is given by the user and is assumed to be correct.
        if (ValidParameterElement2(xi))
          projection_result = ProjectionResult::projection_found_valid;
        else
          projection_result = ProjectionResult::projection_found_not_valid;
        break;
      }

      // Check if residuum is in a sensible range where we still expect to find a solution.
      if (residuum.Norm2() > CONSTANTS::local_newton_res_max) break;

      // Get the jacobian.
      GetElement2PositionDerivative(xi, q_volume, J);

      // Check the determinant of the jacobian.
      if (J.Determinant() < CONSTANTS::local_newton_det_tol) break;

      // Solve the linearized system.
      J_inverse.Invert(J);
      delta_xi.Multiply(J_inverse, residuum);
      xi -= delta_xi;

      // Advance Newton iteration counter.
      counter++;
    }
  }
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::ProjectPointsOnLineToVolume(
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume,
    std::vector<ProjectionPointLineToVolume<scalar_type>>& projection_points,
    unsigned int& n_projections_valid, unsigned int& n_projections) const
{
  // Initialize counters.
  n_projections_valid = 0;
  n_projections = 0;

  // Loop over points and check if they project to this volume.
  for (auto& point : projection_points)
  {
    // Project the point.
    ProjectPointOnLineToVolume(
        q_line, q_volume, point.GetEta(), point.GetXiMutable(), point.GetProjectionResultMutable());

    // Update the counters.
    if (point.GetProjectionResult() == ProjectionResult::projection_found_valid)
    {
      n_projections_valid++;
      n_projections++;
    }
    if (point.GetProjectionResult() == ProjectionResult::projection_found_not_valid)
      n_projections++;
  }
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::ProjectPointsOnLineToVolume(
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume,
    std::vector<ProjectionPointLineToVolume<scalar_type>>& projection_points,
    unsigned int& n_projections_valid) const
{
  // Initialize dummy variable.
  unsigned int n_projections_dummy;

  // Project the points.
  ProjectPointsOnLineToVolume(
      q_line, q_volume, projection_points, n_projections_valid, n_projections_dummy);
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::
    ProjectGaussPointsOnSegmentToVolume(const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
        const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume,
        const DRT::UTILS::IntegrationPoints1D& gauss_points,
        LineSegment<scalar_type>& segment) const
{
  // Set up the vector with the projection points.
  std::vector<ProjectionPointLineToVolume<scalar_type>>& projection_points =
      segment.GetProjectionPointsMutable();
  projection_points.clear();
  projection_points.reserve(gauss_points.nquad);
  LINALG::TMatrix<scalar_type, 3, 1> xi_start;
  this->ValidParameterElement2(xi_start);
  for (unsigned int i = 0; i < (unsigned int)gauss_points.nquad; i++)
  {
    scalar_type eta = segment.GetEtaA() +
                      (segment.GetEtaB() - segment.GetEtaA()) * 0.5 * (gauss_points.qxg[i][0] + 1.);
    projection_points.push_back(
        ProjectionPointLineToVolume<scalar_type>(eta, xi_start, gauss_points.qwgt[i]));
  }

  // Project the Gauss points to the volume.
  unsigned int n_valid_projections;
  ProjectPointsOnLineToVolume(q_line, q_volume, projection_points, n_valid_projections);

  // Check if all points could be projected.
  if (n_valid_projections != (unsigned int)gauss_points.nquad)
    dserror(
        "All Gauss points need to have a valid projection. The number of Gauss points is %d, but "
        "the number of valid projections is %d!",
        gauss_points.nquad, n_valid_projections);
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::IntersectLineWithSurface(
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume,
    const unsigned int& fixed_parameter, const double& fixed_value, scalar_type& eta,
    LINALG::TMatrix<scalar_type, 3, 1>& xi, ProjectionResult& projection_result) const
{
  // Check the input parameters.
  {
    if (GetVolumeType() == DiscretizationTypeVolume::hexaeder && fixed_parameter > 2)
      dserror(
          "Fixed_parameter in IntersectLineWithVolume has to be smaller than 3 with a hexaeder "
          "element.");
    else if (fixed_parameter > 3)
      dserror("fixed_parameter in IntersectLineWithVolume can be 3 at maximum.");
  }

  // Initialize data structures
  // Point on line.
  LINALG::TMatrix<scalar_type, 3, 1> r_line;
  LINALG::TMatrix<scalar_type, 3, 1> dr_line;

  // Point on volume.
  LINALG::TMatrix<scalar_type, 3, 1> r_volume;
  LINALG::TMatrix<scalar_type, 3, 3> dr_volume;

  // Residuum.
  LINALG::TMatrix<scalar_type, 4, 1> residuum;
  LINALG::TMatrix<scalar_type, 4, 1> delta_x;

  // Jacobian.
  LINALG::TMatrix<scalar_type, 4, 4> J;
  LINALG::TMatrix<scalar_type, 4, 4> J_inverse;

  // Solver.
  LINALG::FixedSizeSerialDenseSolver<4, 4> matrix_solver;

  // Reset the projection result flag.
  projection_result = ProjectionResult::projection_not_found;

  {
    // Local Newton iteration.
    unsigned int counter = 0;
    while (counter < CONSTANTS::local_newton_iter_max)
    {
      // Get the point coordinates on the line and volume.
      GetElement1Position(eta, q_line, r_line);
      GetElement2Position(xi, q_volume, r_volume);

      // Evaluate the residuum $r_{volume} - r_{line} = R_{pos}$ and $xi(i) - value = R_{surf}$
      J.PutScalar(0.);
      residuum.PutScalar(0.);
      for (unsigned int i = 0; i < 3; i++)
      {
        residuum(i) = r_volume(i) - r_line(i);
      }
      if (fixed_parameter < 3)
      {
        residuum(3) = xi(fixed_parameter) - fixed_value;
        J(3, fixed_parameter) = 1.;
      }
      else
      {
        for (unsigned int i = 0; i < 3; i++)
        {
          residuum(3) += xi(i);
          J(3, i) = 1.;
        }
        residuum(3) -= fixed_value;
      }

      // Check if tolerance is fulfilled.
      if (residuum.Norm2() < CONSTANTS::local_newton_res_tol)
      {
        // Check if the parameter coordinates are valid.
        if (ValidParameterElement1(eta) && ValidParameterElement2(xi))
          projection_result = ProjectionResult::projection_found_valid;
        else
          projection_result = ProjectionResult::projection_found_not_valid;
        break;
      }

      // Check if residuum is in a sensible range where we still expect to find a solution.
      if (residuum.Norm2() > CONSTANTS::local_newton_res_max) break;

      // Get the positional derivatives.
      GetElement1PositionDerivative(eta, q_line, dr_line);
      GetElement2PositionDerivative(xi, q_volume, dr_volume);

      // Fill up the jacobian.
      for (unsigned int i = 0; i < 3; i++)
      {
        for (unsigned int j = 0; j < 3; j++)
        {
          J(i, j) = dr_volume(i, j);
        }
        J(i, 3) = -dr_line(i);
      }

      // Solve the linearized system.
      if (abs(J.Determinant()) < CONSTANTS::local_newton_det_tol) break;
      matrix_solver.SetMatrix(J);
      matrix_solver.SetVectors(delta_x, residuum);
      int err = matrix_solver.Solve();
      if (err != 0) break;

      // Set the new parameter coordinates.
      eta -= delta_x(3);
      for (unsigned int i = 0; i < 3; i++) xi(i) -= delta_x(i);

      // Advance Newton iteration counter.
      counter++;
    }
  }
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::IntersectLineWithVolume(
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume,
    std::vector<ProjectionPointLineToVolume<scalar_type>>& intersection_points,
    const scalar_type& eta_start, const LINALG::TMatrix<scalar_type, 3, 1>& xi_start) const
{
  // Get number of faces for this volume and create a vector with the indices of the faces, so all
  // surfaces of the volume can be checked for an intersection with the line.
  unsigned int n_faces;
  std::vector<unsigned int> face_fixed_parameters;
  std::vector<double> face_fixed_values;
  if (GetVolumeType() == DiscretizationTypeVolume::hexaeder)
  {
    n_faces = 6;
    face_fixed_parameters = {0, 0, 1, 1, 2, 2};
    face_fixed_values = {-1., 1., -1., 1., -1., 1.};
  }
  else
  {
    n_faces = 4;
    face_fixed_parameters = {0, 1, 2, 3};
    face_fixed_values = {0., 0., 0., 1.};
  }

  // Clear the input vector.
  intersection_points.clear();
  intersection_points.reserve(n_faces);

  // Create variables.
  scalar_type eta;
  LINALG::TMatrix<scalar_type, 3, 1> xi;
  ProjectionResult intersection_found;

  // Try to intersect the beam with each face.
  for (unsigned int i = 0; i < n_faces; i++)
  {
    // Set starting values.
    xi = xi_start;
    eta = eta_start;

    // Intersect the line with the surface.
    IntersectLineWithSurface(q_line, q_volume, face_fixed_parameters[i], face_fixed_values[i], eta,
        xi, intersection_found);

    // If a valid intersection is found, add it to the output vector.
    if (intersection_found == ProjectionResult::projection_found_valid)
    {
      intersection_points.push_back(ProjectionPointLineToVolume<scalar_type>(eta, xi));
    }
  }
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::IntersectLineWithVolume(
    const LINALG::TMatrix<scalar_type, line::n_dof_, 1>& q_line,
    const LINALG::TMatrix<scalar_type, volume::n_dof_, 1>& q_volume,
    std::vector<ProjectionPointLineToVolume<scalar_type>>& intersection_points) const
{
  // Set default values for the parameter coordinates.
  scalar_type eta_start;
  LINALG::TMatrix<scalar_type, 3, 1> xi_start;
  SetStartValuesElement1(eta_start);
  SetStartValuesElement2(xi_start);

  // Call the intersect function.
  IntersectLineWithVolume(q_line, q_volume, intersection_points, eta_start, xi_start);
};

/**
 *
 */
template <typename scalar_type, typename line, typename volume>
GEOMETRYPAIR::DiscretizationTypeVolume
GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::GetVolumeType() const
{
  if (volume::n_nodes_ == 8 || volume::n_nodes_ == 20 || volume::n_nodes_ == 27)
    return GEOMETRYPAIR::DiscretizationTypeVolume::hexaeder;
  else if (volume::n_nodes_ == 4 || volume::n_nodes_ == 10)
    return GEOMETRYPAIR::DiscretizationTypeVolume::tetraeder;
  else
    dserror("Unknown volume type in GetVolumeType()!");
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
bool GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::ValidParameterElement1(
    const scalar_type& eta) const
{
  double xi_limit = 1.0 + CONSTANTS::projection_xi_eta_tol;
  if (fabs(eta) < xi_limit) return true;

  // Default value.
  return false;
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
bool GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::ValidParameterElement2(
    const LINALG::TMatrix<scalar_type, 3, 1>& xi) const
{
  double xi_limit = 1.0 + CONSTANTS::projection_xi_eta_tol;
  if (GetVolumeType() == DiscretizationTypeVolume::hexaeder)
  {
    if (fabs(xi(0)) < xi_limit && fabs(xi(1)) < xi_limit && fabs(xi(2)) < xi_limit) return true;
  }
  else
  {
    if (xi(0) > -CONSTANTS::projection_xi_eta_tol && xi(1) > -CONSTANTS::projection_xi_eta_tol &&
        xi(2) > -CONSTANTS::projection_xi_eta_tol &&
        xi(0) + xi(1) + xi(2) < 1.0 + CONSTANTS::projection_xi_eta_tol)
      return true;
  }

  // Default value.
  return false;
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::SetStartValuesElement1(
    scalar_type& eta) const
{
  eta = 0.;
}


/**
 *
 */
template <typename scalar_type, typename line, typename volume>
void GEOMETRYPAIR::GeometryPairLineToVolume<scalar_type, line, volume>::SetStartValuesElement2(
    LINALG::TMatrix<scalar_type, 3, 1>& xi) const
{
  if (GetVolumeType() == GEOMETRYPAIR::DiscretizationTypeVolume::hexaeder)
    xi.PutScalar(0.0);
  else
    xi.PutScalar(0.25);
}


/**
 * Explicit template initialization of template class.
 */
template class GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8>;
template class GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20>;
template class GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27>;
template class GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4>;
template class GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10>;


/**
 * We need to explicitly initialize the position functions for AD types. For example in case of beam
 * to solid meshtying the geometry interactions are done with the constant reference configuration
 * and therefore doubles, but in the Evaluate function the position needs to be evaluated with AD
 * types to get the difference in the current configuration.
 */
typedef Sacado::ELRFad::SLFad<double,
    GEOMETRYPAIR::t_hermite::n_dof_ + GEOMETRYPAIR::t_hex8::n_dof_>
    t_ad_hermite_hex8;
typedef Sacado::ELRFad::SLFad<double,
    GEOMETRYPAIR::t_hermite::n_dof_ + GEOMETRYPAIR::t_hex20::n_dof_>
    t_ad_hermite_hex20;
typedef Sacado::ELRFad::SLFad<double,
    GEOMETRYPAIR::t_hermite::n_dof_ + GEOMETRYPAIR::t_hex27::n_dof_>
    t_ad_hermite_hex27;
typedef Sacado::ELRFad::SLFad<double,
    GEOMETRYPAIR::t_hermite::n_dof_ + GEOMETRYPAIR::t_tet4::n_dof_>
    t_ad_hermite_tet4;
typedef Sacado::ELRFad::SLFad<double,
    GEOMETRYPAIR::t_hermite::n_dof_ + GEOMETRYPAIR::t_tet10::n_dof_>
    t_ad_hermite_tet10;

template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8>::GetElement1Position(const double&,
    const LINALG::TMatrix<t_ad_hermite_hex8, GEOMETRYPAIR::t_hermite::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_hex8, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20>::GetElement1Position(const double&,
    const LINALG::TMatrix<t_ad_hermite_hex20, GEOMETRYPAIR::t_hermite::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_hex20, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27>::GetElement1Position(const double&,
    const LINALG::TMatrix<t_ad_hermite_hex27, GEOMETRYPAIR::t_hermite::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_hex27, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4>::GetElement1Position(const double&,
    const LINALG::TMatrix<t_ad_hermite_tet4, GEOMETRYPAIR::t_hermite::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_tet4, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10>::GetElement1Position(const double&,
    const LINALG::TMatrix<t_ad_hermite_tet10, GEOMETRYPAIR::t_hermite::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_tet10, 3, 1>&) const;

template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8>::GetElement2Position(const LINALG::TMatrix<double, 3, 1>&,
    const LINALG::TMatrix<t_ad_hermite_hex8, GEOMETRYPAIR::t_hex8::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_hex8, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20>::GetElement2Position(const LINALG::TMatrix<double, 3, 1>&,
    const LINALG::TMatrix<t_ad_hermite_hex20, GEOMETRYPAIR::t_hex20::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_hex20, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27>::GetElement2Position(const LINALG::TMatrix<double, 3, 1>&,
    const LINALG::TMatrix<t_ad_hermite_hex27, GEOMETRYPAIR::t_hex27::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_hex27, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4>::GetElement2Position(const LINALG::TMatrix<double, 3, 1>&,
    const LINALG::TMatrix<t_ad_hermite_tet4, GEOMETRYPAIR::t_tet4::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_tet4, 3, 1>&) const;
template void GEOMETRYPAIR::GeometryPairLineToVolume<double, GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10>::GetElement2Position(const LINALG::TMatrix<double, 3, 1>&,
    const LINALG::TMatrix<t_ad_hermite_tet10, GEOMETRYPAIR::t_tet10::n_dof_, 1>&,
    LINALG::TMatrix<t_ad_hermite_tet10, 3, 1>&) const;
