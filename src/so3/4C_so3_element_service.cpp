/*----------------------------------------------------------------------*/
/*! \file
\brief Implementation of free service functions for elements

\level 3
*/
/*----------------------------------------------------------------------*/

#include "4C_so3_element_service.hpp"

#include "4C_discretization_fem_general_utils_fem_shapefunctions.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_so3_hex8.hpp"
#include "4C_so3_tet10.hpp"

FOUR_C_NAMESPACE_OPEN

void DRT::ELEMENTS::AssembleGaussPointValues(
    std::vector<Teuchos::RCP<Epetra_MultiVector>>& global_data,
    const CORE::LINALG::SerialDenseMatrix& gp_data, const DRT::Element& ele)
{
  for (int gp = 0; gp < gp_data.numRows(); ++gp)
  {
    const Epetra_BlockMap& elemap = global_data[gp]->Map();
    int lid = elemap.LID(ele.Id());
    if (lid != -1)
    {
      for (int i = 0; i < gp_data.numCols(); ++i)
      {
        (*((*global_data[gp])(i)))[lid] += gp_data(gp, i);
      }
    }
  }
}

void DRT::ELEMENTS::AssembleNodalElementCount(
    Epetra_IntVector& global_count, const DRT::Element& ele)
{
  for (int n = 0; n < ele.NumNode(); ++n)
  {
    const int lid = global_count.Map().LID(ele.NodeIds()[n]);

    if (lid != -1)
    {
      global_count[lid] += 1;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
std::vector<double> DRT::ELEMENTS::ProjectNodalQuantityToXi(
    const CORE::LINALG::Matrix<3, 1>& xi, const std::vector<double>& nodal_quantity)
{
  const int numNodesPerElement = CORE::FE::num_nodes<distype>;

  CORE::LINALG::Matrix<numNodesPerElement, 1> shapefunct(true);
  CORE::FE::shape_function<distype>(xi, shapefunct);

  const int num_dof_per_node = static_cast<int>(nodal_quantity.size()) / numNodesPerElement;
  std::vector<double> projected_quantities(num_dof_per_node, 0.0);

  for (int dof = 0; dof < num_dof_per_node; ++dof)
  {
    for (int i = 0; i < numNodesPerElement; ++i)
    {
      projected_quantities[dof] += nodal_quantity[i * num_dof_per_node + dof] * shapefunct(i);
    }
  }

  return projected_quantities;
}

// explicit template instantiations
template std::vector<double> DRT::ELEMENTS::ProjectNodalQuantityToXi<CORE::FE::CellType::hex8>(
    const CORE::LINALG::Matrix<3, 1>&, const std::vector<double>&);
template std::vector<double> DRT::ELEMENTS::ProjectNodalQuantityToXi<CORE::FE::CellType::hex27>(
    const CORE::LINALG::Matrix<3, 1>&, const std::vector<double>&);
template std::vector<double> DRT::ELEMENTS::ProjectNodalQuantityToXi<CORE::FE::CellType::tet4>(
    const CORE::LINALG::Matrix<3, 1>&, const std::vector<double>&);
template std::vector<double> DRT::ELEMENTS::ProjectNodalQuantityToXi<CORE::FE::CellType::tet10>(
    const CORE::LINALG::Matrix<3, 1>&, const std::vector<double>&);
template std::vector<double> DRT::ELEMENTS::ProjectNodalQuantityToXi<CORE::FE::CellType::wedge6>(
    const CORE::LINALG::Matrix<3, 1>&, const std::vector<double>&);

FOUR_C_NAMESPACE_CLOSE
