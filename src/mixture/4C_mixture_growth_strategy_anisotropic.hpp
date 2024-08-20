/*----------------------------------------------------------------------*/
/*! \file
\brief Declaration of a growth strategy for anisotropic growth

\level 3
*/
/*----------------------------------------------------------------------*/
#ifndef FOUR_C_MIXTURE_GROWTH_STRATEGY_ANISOTROPIC_HPP
#define FOUR_C_MIXTURE_GROWTH_STRATEGY_ANISOTROPIC_HPP

#include "4C_config.hpp"

#include "4C_mat_anisotropy_extension_default.hpp"
#include "4C_mixture_growth_strategy.hpp"

FOUR_C_NAMESPACE_OPEN

namespace MIXTURE
{
  class AnisotropicGrowthStrategy;

  namespace PAR
  {
    class AnisotropicGrowthStrategy : public MIXTURE::PAR::MixtureGrowthStrategy
    {
     public:
      explicit AnisotropicGrowthStrategy(const Core::Mat::PAR::Parameter::Data& matdata);

      std::unique_ptr<MIXTURE::MixtureGrowthStrategy> create_growth_strategy() override;

      const int init_mode_;
      const int fiber_id_;

      /// structural tensor strategy
      Teuchos::RCP<Mat::Elastic::StructuralTensorStrategyBase> structural_tensor_strategy_;
    };
  }  // namespace PAR

  /*!
   * @brief Growth is modeled as an inelastic expansion of the whole cell in one predefined
   * direction
   *
   * The direction of growth can be specified with a fiber with the fiber id specified in FIBER_ID
   *
   */
  class AnisotropicGrowthStrategy : public MIXTURE::MixtureGrowthStrategy
  {
   public:
    explicit AnisotropicGrowthStrategy(MIXTURE::PAR::AnisotropicGrowthStrategy* params);

    void pack_mixture_growth_strategy(Core::Communication::PackBuffer& data) const override;

    void unpack_mixture_growth_strategy(Core::Communication::UnpackBuffer& buffer) override;

    void register_anisotropy_extensions(Mat::Anisotropy& anisotropy) override;

    [[nodiscard]] bool has_inelastic_growth_deformation_gradient() const override { return true; };

    void evaluate_inverse_growth_deformation_gradient(Core::LinAlg::Matrix<3, 3>& iFgM,
        const MIXTURE::MixtureRule& mixtureRule, double currentReferenceGrowthScalar,
        int gp) const override;

    void evaluate_growth_stress_cmat(const MIXTURE::MixtureRule& mixtureRule,
        double currentReferenceGrowthScalar,
        const Core::LinAlg::Matrix<1, 6>& dCurrentReferenceGrowthScalarDC,
        const Core::LinAlg::Matrix<3, 3>& F, const Core::LinAlg::Matrix<6, 1>& E_strain,
        Teuchos::ParameterList& params, Core::LinAlg::Matrix<6, 1>& S_stress,
        Core::LinAlg::Matrix<6, 6>& cmat, const int gp, const int eleGID) const override;

   private:
    ///! growth parameters as defined in the input file
    const PAR::AnisotropicGrowthStrategy* params_{};

    /// Anisotropy extension that manages fibers and structural tensors
    Mat::DefaultAnisotropyExtension<1> anisotropy_extension_;
  };
}  // namespace MIXTURE

FOUR_C_NAMESPACE_CLOSE

#endif