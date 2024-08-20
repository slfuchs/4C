/*----------------------------------------------------------------------*/
/*! \file

\brief

\level 3

*/
/*----------------------------------------------------------------------*/
#include "4C_mat_anisotropy_utils.hpp"

#include "4C_comm_pack_buffer.hpp"
#include "4C_comm_parobject.hpp"
#include "4C_matelast_aniso_structuraltensor_strategy.hpp"

#include <iostream>

FOUR_C_NAMESPACE_OPEN

void Mat::read_anisotropy_fiber(const Core::IO::InputParameterContainer& container,
    std::string specifier, Core::LinAlg::Matrix<3, 1>& fiber_vector)
{
  auto fiber = container.get<std::vector<double>>(std::move(specifier));

  double f1norm = 0.;
  // normalization
  for (std::vector<double>::size_type i = 0; i < 3; ++i)
  {
    f1norm += fiber[i] * fiber[i];
  }
  f1norm = std::sqrt(f1norm);

  if (f1norm < 1e-9)
  {
    FOUR_C_THROW("The given fiber is not a vector but zero.");
  }

  // fill final fiber vector
  for (std::vector<double>::size_type i = 0; i < 3; ++i)
  {
    fiber_vector(i) = fiber[i] / f1norm;
  }
}

template <typename T, unsigned int numfib>
void Mat::compute_structural_tensors(
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, numfib>>& fibers,
    std::vector<std::array<T, numfib>>& structural_tensor,
    const Teuchos::RCP<Elastic::StructuralTensorStrategyBase>& strategy)
{
  // Need to compute the stuctural tensors
  if (Teuchos::is_null(strategy))
  {
    FOUR_C_THROW("Structural tensor strategy is null!");
  }

  structural_tensor.resize(fibers.size());
  for (std::vector<std::vector<Core::LinAlg::Matrix<3, 1>>>::size_type gp = 0; gp < fibers.size();
       ++gp)
  {
    for (std::vector<Core::LinAlg::Matrix<3, 1>>::size_type i = 0; i < numfib; ++i)
    {
      T A(false);
      strategy->setup_structural_tensor(fibers[gp].at(i), A);

      structural_tensor[gp].at(i).update(A);
    }
  }
}

template <typename T>
void Mat::pack_fiber_vector(
    Core::Communication::PackBuffer& buffer, const std::vector<std::vector<T>>& vct)
{
  Core::Communication::ParObject::add_to_pack(buffer, static_cast<int>(vct.size()));

  for (const auto& list : vct)
  {
    Core::Communication::ParObject::add_to_pack(buffer, list);
  }
}

template <typename T, unsigned int numfib>
void Mat::pack_fiber_array(
    Core::Communication::PackBuffer& buffer, const std::vector<std::array<T, numfib>>& vct)
{
  Core::Communication::ParObject::add_to_pack(buffer, static_cast<int>(vct.size()));

  for (const auto& list : vct)
  {
    for (const auto& fiber : list)
    {
      Core::Communication::ParObject::add_to_pack<T::num_rows(), T::num_cols()>(buffer, fiber);
    }
  }
}

template <typename T>
void Mat::unpack_fiber_vector(
    Core::Communication::UnpackBuffer& buffer, std::vector<std::vector<T>>& vct)
{
  vct.clear();
  int numgps = Core::Communication::ParObject::extract_int(buffer);
  for (int i = 0; i < numgps; ++i)
  {
    std::vector<T> mat(0);
    Core::Communication::ParObject::extract_from_pack(buffer, mat);
    vct.emplace_back(mat);
  }
}

template <typename T, unsigned int numfib>
void Mat::unpack_fiber_array(
    Core::Communication::UnpackBuffer& buffer, std::vector<std::array<T, numfib>>& vct)
{
  vct.clear();
  int numgps = Core::Communication::ParObject::extract_int(buffer);
  for (int i = 0; i < numgps; ++i)
  {
    std::array<T, numfib> mat;
    for (unsigned int j = 0; j < numfib; ++j)
    {
      Core::Communication::ParObject::extract_from_pack<T::num_rows(), T::num_cols()>(
          buffer, mat.at(j));
    }
    vct.emplace_back(mat);
  }
}

/*----------------------------------------------------------------------------*/
// explicit instantiation of template functions
template void Mat::compute_structural_tensors<Core::LinAlg::Matrix<6, 1>, 1u>(
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 1>>& fibers,
    std::vector<std::array<Core::LinAlg::Matrix<6, 1>, 1>>& structural_tensor,
    const Teuchos::RCP<Elastic::StructuralTensorStrategyBase>& strategy);
template void Mat::compute_structural_tensors<Core::LinAlg::Matrix<3, 3>, 1u>(
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 1>>& fibers,
    std::vector<std::array<Core::LinAlg::Matrix<3, 3>, 1>>& structural_tensor,
    const Teuchos::RCP<Elastic::StructuralTensorStrategyBase>& strategy);
template void Mat::compute_structural_tensors<Core::LinAlg::Matrix<6, 1>, 2u>(
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 2>>& fibers,
    std::vector<std::array<Core::LinAlg::Matrix<6, 1>, 2>>& structural_tensor,
    const Teuchos::RCP<Elastic::StructuralTensorStrategyBase>& strategy);
template void Mat::compute_structural_tensors<Core::LinAlg::Matrix<3, 3>, 2u>(
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 2>>& fibers,
    std::vector<std::array<Core::LinAlg::Matrix<3, 3>, 2>>& structural_tensor,
    const Teuchos::RCP<Elastic::StructuralTensorStrategyBase>& strategy);

template void Mat::pack_fiber_vector(Core::Communication::PackBuffer& buffer,
    const std::vector<std::vector<Core::LinAlg::Matrix<3, 3>>>& vct);
template void Mat::unpack_fiber_vector(Core::Communication::UnpackBuffer& buffer,
    std::vector<std::vector<Core::LinAlg::Matrix<3, 3>>>& vct);
template void Mat::pack_fiber_vector(Core::Communication::PackBuffer& buffer,
    const std::vector<std::vector<Core::LinAlg::Matrix<3, 1>>>& vct);
template void Mat::unpack_fiber_vector(Core::Communication::UnpackBuffer& buffer,
    std::vector<std::vector<Core::LinAlg::Matrix<3, 1>>>& vct);
template void Mat::pack_fiber_vector(Core::Communication::PackBuffer& buffer,
    const std::vector<std::vector<Core::LinAlg::Matrix<6, 1>>>& vct);
template void Mat::unpack_fiber_vector(Core::Communication::UnpackBuffer& buffer,
    std::vector<std::vector<Core::LinAlg::Matrix<6, 1>>>& vct);

template void Mat::pack_fiber_array<Core::LinAlg::Matrix<3, 1>, 1u>(
    Core::Communication::PackBuffer& buffer,
    const std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 1>>& vct);
template void Mat::pack_fiber_array<Core::LinAlg::Matrix<3, 3>, 1u>(
    Core::Communication::PackBuffer& buffer,
    const std::vector<std::array<Core::LinAlg::Matrix<3, 3>, 1>>& vct);
template void Mat::pack_fiber_array<Core::LinAlg::Matrix<6, 1>, 1u>(
    Core::Communication::PackBuffer& buffer,
    const std::vector<std::array<Core::LinAlg::Matrix<6, 1>, 1>>& vct);

template void Mat::unpack_fiber_array<Core::LinAlg::Matrix<3, 1>, 1>(
    Core::Communication::UnpackBuffer& buffer,
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 1>>& vct);
template void Mat::unpack_fiber_array<Core::LinAlg::Matrix<3, 3>, 1>(
    Core::Communication::UnpackBuffer& buffer,
    std::vector<std::array<Core::LinAlg::Matrix<3, 3>, 1>>& vct);
template void Mat::unpack_fiber_array<Core::LinAlg::Matrix<6, 1>, 1>(
    Core::Communication::UnpackBuffer& buffer,
    std::vector<std::array<Core::LinAlg::Matrix<6, 1>, 1>>& vct);

template void Mat::pack_fiber_array<Core::LinAlg::Matrix<3, 1>, 2u>(
    Core::Communication::PackBuffer& buffer,
    const std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 2>>& vct);
template void Mat::pack_fiber_array<Core::LinAlg::Matrix<3, 3>, 2u>(
    Core::Communication::PackBuffer& buffer,
    const std::vector<std::array<Core::LinAlg::Matrix<3, 3>, 2>>& vct);
template void Mat::pack_fiber_array<Core::LinAlg::Matrix<6, 1>, 2u>(
    Core::Communication::PackBuffer& buffer,
    const std::vector<std::array<Core::LinAlg::Matrix<6, 1>, 2>>& vct);

template void Mat::unpack_fiber_array<Core::LinAlg::Matrix<3, 1>, 2>(
    Core::Communication::UnpackBuffer& buffer,
    std::vector<std::array<Core::LinAlg::Matrix<3, 1>, 2>>& vct);
template void Mat::unpack_fiber_array<Core::LinAlg::Matrix<3, 3>, 2>(
    Core::Communication::UnpackBuffer& buffer,
    std::vector<std::array<Core::LinAlg::Matrix<3, 3>, 2>>& vct);
template void Mat::unpack_fiber_array<Core::LinAlg::Matrix<6, 1>, 2>(
    Core::Communication::UnpackBuffer& buffer,
    std::vector<std::array<Core::LinAlg::Matrix<6, 1>, 2>>& vct);

FOUR_C_NAMESPACE_CLOSE
