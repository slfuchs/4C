/*----------------------------------------------------------------------*/
/*! \file
\brief  fluid material for poroelasticity problems


\level 2
 *-----------------------------------------------------------------------*/

#include "4C_mat_fluidporo.hpp"

#include "4C_global_data.hpp"
#include "4C_mat_par_bundle.hpp"

#include <vector>

FOUR_C_NAMESPACE_OPEN

namespace MAT::FLUIDPORO
{
  /*! @brief Compute structure tensor from a given direction vector
   *
   * Templated for the number of spatial dimensions, compute the structure tensor corresponding to a
   * direction vector taking the dyadic product of the vector with itself.
   *
   * @param [in] direction_vector     Non-zero direction vector (not necessarily unit-length)
   *                                  (@p dim dimensional vector)
   * @param [out] structure_tensor    Normalized structure tensor of the direction vector
   *                                  (@p dim x @p dim dyadic)
   */
  template <unsigned int dim>
  void CreateStructureTensorFromVector(
      const std::vector<double>& direction_vector, CORE::LINALG::Matrix<dim, dim>& structure_tensor)
  {
    structure_tensor.Clear();

    // Factor to normalize the structure tensor
    const double square_length = std::inner_product(
        direction_vector.begin(), direction_vector.end(), direction_vector.begin(), 0.0);

    if (square_length > 1e-8)
    {
      for (unsigned int i = 0; i < dim; ++i)
      {
        for (unsigned int j = 0; j < dim; ++j)
        {
          structure_tensor(i, j) = direction_vector[i] * direction_vector[j];
        }
      }

      structure_tensor.Scale(1. / square_length);
    }
    else
      FOUR_C_THROW("Check for a zero direction vector");
  }

  /*!
   * @brief  Base class for the anisotropy strategy in the FluidPoro material.
   *
   * For materials with anisotropic permeability properties, the reaction tensor will represent
   * the type of anisotropy and therefore will be calculated in different ways. This base class
   * defines the methods that are affected by the anisotropy. These methods are responsible for
   * computations regarding the reaction tensor. Some methods are already implemented by the
   * base class. They can be overridden depending on the needs of the specific type of
   * anisotropy.
   */
  class PoroAnisotropyStrategyBase
  {
   public:
    /*!
     * @brief Simple constructor
     *
     * @param [in] params Material parameters
     */
    PoroAnisotropyStrategyBase(const MAT::PAR::FluidPoro* params) : params_(params){};

    //! Virtual default destructor
    virtual ~PoroAnisotropyStrategyBase() = default;

    //! Compute the reaction coefficient
    virtual double ComputeReactionCoeff() const = 0;

    /*!
     * @brief Compute the material reaction tensor for the fluid - 2D
     *
     * This is the base class method. Is is overridden by derived classes to compute the reaction
     * tensor depending on the respective anisotropy strategy. The reaction tensor is the inverse
     * of the permeability tensor multiplied by the dynamic viscosity of the fluid.
     *
     * @param [out] reaction_tensor    The 2D reaction tensor computed according to the
     *                                    anisotropy strategy
     * @param [in] J  Determinant of the deformation gradient in case the reaction tensor is
     *                dependent on it
     * @param [in] porosity   Porosity value in case the reaction tensor is dependent on it
     * @param [in] anisotropic_permeability_directions    Principal directions of anisotropy
     *                                                    required to construct the reaction
     *                                                    tensor
     * @param [in] anisotropic_permeability_coeffs  Scaling coefficients for the permeability in
     *                                              anisotropy directions
     */
    virtual void ComputeReactionTensor(CORE::LINALG::Matrix<2, 2>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        const std::vector<double>& anisotropic_permeability_coeffs) const = 0;

    /*!
     * @brief Compute the material reaction tensor for the fluid - 3D
     *
     * This is the base class method. Is is overridden by derived classes to compute the reaction
     * tensor depending on the respective anisotropy strategy. The reaction tensor is the inverse
     * of the permeability tensor multiplied by the dynamic viscosity of the fluid.
     *
     * @param [out] reaction_tensor   The 3D reaction tensor computed according to the
     *                                anisotropy strategy (3x3 dyadic)
     * @param [in] J  Determinant of the deformation gradient
     * @param [in] porosity   Porosity value
     * @param [in] anisotropic_permeability_directions    Principal directions of anisotropy
     *                                                    required to construct the reaction
     *                                                    tensor
     * @param [in] anisotropic_permeability_coeffs  Scaling coefficients for the permeability in
     *                                              anisotropy directions
     */
    virtual void ComputeReactionTensor(CORE::LINALG::Matrix<3, 3>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        const std::vector<double>& anisotropic_permeability_coeffs) const = 0;

    /*! @brief Compute the derivatives of the material reaction tensor - 2D
     *
     * In case the material reaction tensor depends on the determinant of the deformation gradient
     * and/or the porosity (e.g. for the Kozeny-Carman equation), this method and its overriding
     * methods compute the derivative tensors.
     *
     * @param [out] linreac_dphi  Derivative of the material reaction tensor w.r.t. the porosity
     * (2x2 dyadic)
     * @param [out] linreac_dJ    Derivative of the material reaction tensor w.r.t. the
     *                            determinant of the deformation gradient (2x2 dyadic)
     * @param [in] J  Determinant of the deformation gradient
     * @param [in] porosity   Porosity value
     */
    virtual void ComputeLinMatReactionTensor(CORE::LINALG::Matrix<2, 2>& linreac_dphi,
        CORE::LINALG::Matrix<2, 2>& linreac_dJ, const double& J, const double& porosity) const
    {
      // Derivative is zero, clearing the tensors is enough
      // This is the standard case, so it is implemented in the base class
      linreac_dphi.Clear();
      linreac_dJ.Clear();
    };

    /*! @brief Compute the derivatives of the material reaction tensor - 3D
     *
     * In case the material reaction tensor depends on the determinant of the deformation gradient
     * and/or the porosity (e.g. for the Kozeny-Carman equation), this method and its overriding
     * methods compute the derivative tensors.
     *
     * @param [out] linreac_dphi  Derivative of the material reaction tensor w.r.t. the porosity
     *                            (3x3 dyadic)
     * @param [out] linreac_dJ    Derivative of the material reaction tensor w.r.t. the
     *                            determinant of the deformation gradient (3x3 dyadic)
     * @param [in] J  Determinant of the deformation gradient
     * @param [in] porosity   Porosity value
     */
    virtual void ComputeLinMatReactionTensor(CORE::LINALG::Matrix<3, 3>& linreac_dphi,
        CORE::LINALG::Matrix<3, 3>& linreac_dJ, const double& J, const double& porosity) const
    {
      // Derivative is zero, clearing the tensors is enough
      // This is the standard case, so it is implemented in the base class
      linreac_dphi.Clear();
      linreac_dJ.Clear();
    };

   protected:
    //! Material parameters
    const MAT::PAR::FluidPoro* params_;
  };

  /*!
   * @brief Reaction tensor (anisotropy) strategy for isotropy.
   *
   * In the case of isotropy, both 2D and 3D simulations are allowed. Furthermore, the isotropic
   * case implements two different strategies for the permeability-porosity dependency which are
   *  -# Constant material permeability, and
   *  -# Kozeny-Carman-Equation.
   */
  class PoroIsotropyStrategy : public PoroAnisotropyStrategyBase
  {
   public:
    //! Simple constructor
    PoroIsotropyStrategy(const MAT::PAR::FluidPoro* params) : PoroAnisotropyStrategyBase(params){};

    //! compute isotropy reaction coefficient
    double ComputeReactionCoeff() const override
    {
      const double viscosity = params_->viscosity_;
      const double permeability = params_->permeability_;

      if (viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      if (permeability <= 0.0) FOUR_C_THROW("zero or negative permeability");

      // trace of the reaction tensor divided by the dimension
      const double reacoeff = viscosity / permeability;

      return reacoeff;
    };

    //! compute isotropic reaction tensor - 2D
    void ComputeReactionTensor(CORE::LINALG::Matrix<2, 2>& reaction_tensor, const double& J,
        const double& porosity,
        [[maybe_unused]] const std::vector<std::vector<double>>&
            anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      ComputeReactionTensorForIsotropy<2>(reaction_tensor, J, porosity,
          anisotropic_permeability_directions, anisotropic_permeability_coeffs);
    };

    //! compute isotropic reaction tensor - 3D
    void ComputeReactionTensor(CORE::LINALG::Matrix<3, 3>& reaction_tensor, const double& J,
        const double& porosity,
        [[maybe_unused]] const std::vector<std::vector<double>>&
            anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      ComputeReactionTensorForIsotropy<3>(reaction_tensor, J, porosity,
          anisotropic_permeability_directions, anisotropic_permeability_coeffs);
    };

    /*!
     * @brief Compute reaction tensor for isotropy
     *
     * Templated for the number of spatial dimensions, compute the specific reaction tensor in the
     * case of isotropic permeability.
     *
     * @tparam dim  Number of spatial dimensions
     * @param [out] reaction_tensor Reaction tensor (@tp dim x @p dim dyadic)
     * @param [in] J  Determinant of the deformation gradient
     * @param [in] porosity   Porosity value
     * @param [in] anisotropic_permeability_directions  Unused - Principal directions of
     *                                                  anisotropy required to construct the
     *                                                  reaction tensor
     * @param [in] anisotropic_permeability_coeffs  Unused - Scaling coefficients for the
     *                                              permeability in anisotropy directions
     */
    template <unsigned int dim>
    void ComputeReactionTensorForIsotropy(CORE::LINALG::Matrix<dim, dim>& reaction_tensor,
        const double& J, const double& porosity,
        [[maybe_unused]] const std::vector<std::vector<double>>&
            anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const
    {
      double reacoeff = ComputeReactionCoeff();
      const double permeability_correction_factor = params_->permeability_correction_factor_;
      const auto permeability_function = params_->permeability_func_;

      reaction_tensor.Clear();

      if (permeability_function == MAT::PAR::kozeny_carman)
      {
        reacoeff *= (1 - porosity * porosity * J * J) /
                    (porosity * porosity * porosity * J * J * J) * permeability_correction_factor;
      }

      for (unsigned int i = 0; i < dim; i++) reaction_tensor(i, i) = reacoeff;
    };

    //! compute linearization of isotropic reaction tensor - 2D
    void ComputeLinMatReactionTensor(CORE::LINALG::Matrix<2, 2>& linreac_dphi,
        CORE::LINALG::Matrix<2, 2>& linreac_dJ, const double& J,
        const double& porosity) const override
    {
      ComputeLinMatReactionTensorForIsotropy<2>(linreac_dphi, linreac_dJ, J, porosity);
    };

    //! compute linearization of isotropic reaction tensor - 3D
    void ComputeLinMatReactionTensor(CORE::LINALG::Matrix<3, 3>& linreac_dphi,
        CORE::LINALG::Matrix<3, 3>& linreac_dJ, const double& J,
        const double& porosity) const override
    {
      ComputeLinMatReactionTensorForIsotropy<3>(linreac_dphi, linreac_dJ, J, porosity);
    };

    /*!
     * @brief Compute linearization reaction tensor for isotropy
     *
     * Templated for the number of spatial dimensions, compute the specific linearizations of the
     * reaction tensor w.r.t. the porosity and J in the case of isotropic permeability. A
     * distinction between constant permeability and the Kozeny-Carman permeability function is
     * made, which results in different derivatives.
     *
     * @tparam dim  Number of spatial dimensions
     * @param [out] linreac_dphi    Derivative of the material reaction tensor w.r.t. the porosity
     *                              (@p dim x @p dim dyadic)
     * @param [out] linreac_dJ  Derivative of the material reaction tensor w.r.t. the
     *                          determinant of the deformation gradient (@p dim x @p dim dyadic)
     * @param [in] J    Determinant of the deformation gradient
     * @param [in] porosity Porosity value
     */
    template <unsigned int dim>
    void ComputeLinMatReactionTensorForIsotropy(CORE::LINALG::Matrix<dim, dim>& linreac_dphi,
        CORE::LINALG::Matrix<dim, dim>& linreac_dJ, const double& J, const double& porosity) const
    {
      const double reacoeff = ComputeReactionCoeff();
      const double permeability_correction_factor = params_->permeability_correction_factor_;
      const auto permeability_function = params_->permeability_func_;

      linreac_dphi.Clear();
      linreac_dJ.Clear();

      if (permeability_function == MAT::PAR::constant)
      {
        // Permeability is not a function of porosity or J
        return;
      }
      else if (permeability_function == MAT::PAR::kozeny_carman)
      {
        // d(isotropic_mat_reactiontensor)/d(phi) = reacoeff * [(J * phi)^2 - 3] / ( J^3 * phi^4 )
        // d(isotropic_mat_reactiontensor)/d(J) = reacoeff * [(J * phi)^2 - 3] / ( J^4 * phi^3 )

        const double linreac_tmp = reacoeff * ((J * J * porosity * porosity) - 3.0) /
                                   (J * J * J * porosity * porosity * porosity) *
                                   permeability_correction_factor;
        linreac_dphi(0, 0) = linreac_tmp / porosity;
        linreac_dJ(0, 0) = linreac_tmp / J;

        for (unsigned int i = 1; i < dim; i++)
        {
          linreac_dphi(i, i) = linreac_dphi(0, 0);
          linreac_dJ(i, i) = linreac_dJ(0, 0);
        }
      }
    };
  };

  /*!
   * @brief Reaction tensor (anisotropy) strategy for constant material transverse isotropy.
   *
   * In the case of transverse isotropy, both 2D and 3D simulations can be done. For now,
   * only constant material permeabilities are allowed, i.e. there is no dependency between the
   * permeability and porosity in the material reaction tensor.
   */
  class PoroConstantMaterialTransverseIsotropyStrategy : public PoroAnisotropyStrategyBase
  {
   public:
    PoroConstantMaterialTransverseIsotropyStrategy(const MAT::PAR::FluidPoro* params)
        : PoroAnisotropyStrategyBase(params){};

    //! compute reaction coefficient for constant material transverse isotropy
    double ComputeReactionCoeff() const override
    {
      const double viscosity = params_->viscosity_;
      const double permeability = params_->permeability_;
      const double axial_permeability = params_->axial_permeability_;

      if (viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      if (permeability <= 0.0) FOUR_C_THROW("zero or negative permeability");
      if (axial_permeability <= 0.0) FOUR_C_THROW("zero or negative axial permeability");

      // trace of the 3D reaction tensor divided by 3 (even if the problem is in 2D)
      const double reacoeff = viscosity / 3. * (1. / axial_permeability + 2. / permeability);

      return reacoeff;
    };

    //! compute reaction tensor for constant material transverse isotropy - 2D
    void ComputeReactionTensor(CORE::LINALG::Matrix<2, 2>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      ComputeReactionTensorForConstantMaterialTransverseIsotropy<2>(reaction_tensor, J, porosity,
          anisotropic_permeability_directions, anisotropic_permeability_coeffs);
    };

    //! compute reaction tensor for constant material transverse isotropy - 3D
    void ComputeReactionTensor(CORE::LINALG::Matrix<3, 3>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      ComputeReactionTensorForConstantMaterialTransverseIsotropy<3>(reaction_tensor, J, porosity,
          anisotropic_permeability_directions, anisotropic_permeability_coeffs);
    };

    /*!
     * @brief Compute reaction tensor for constant material transverse isotropy
     *
     * Templated for the number of spatial dimensions, compute the specific reaction tensor in the
     * case of constant material transversely isotropic permeability.
     *
     * @tparam dim  Number of spatial dimensions
     * @param [out] reaction_tensor   Reaction tensor (@p dim x @p dim dyadic)
     * @param [in] J  Determinant of the deformation gradient
     * @param [in] porosity   Porosity value
     * @param [in] anisotropic_permeability_directions    Principal directions of anisotropy
     *                                                    required to construct the reaction
     *                                                    tensor
     * @param [in] anisotropic_permeability_coeffs  Unused - Scaling coefficients for the
     *                                              permeability in anisotropy directions
     */
    template <unsigned int dim>
    void ComputeReactionTensorForConstantMaterialTransverseIsotropy(
        CORE::LINALG::Matrix<dim, dim>& reaction_tensor, const double& J, const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const
    {
      const double dynamic_viscosity = params_->viscosity_;
      const double permeability = params_->permeability_;
      const double axial_permeability = params_->axial_permeability_;

      if (dynamic_viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      if (permeability <= 0.0) FOUR_C_THROW("zero or negative permeability");
      if (axial_permeability <= 0.0) FOUR_C_THROW("zero or negative axial permeability");
      if (anisotropic_permeability_directions.empty())
        FOUR_C_THROW("orthotropy directions not specified");

      reaction_tensor.Clear();

      CORE::LINALG::Matrix<dim, dim> permeability_tensor(true);
      CORE::LINALG::Matrix<dim, dim> structure_tensor(true);
      CreateStructureTensorFromVector<dim>(
          anisotropic_permeability_directions[0], structure_tensor);

      // Transverse component of the transversely isotropic permeability tensor
      for (unsigned int i = 0; i < dim; ++i) permeability_tensor(i, i) += permeability;

      // Axial component of the transversely isotropic permeability tensor
      permeability_tensor.Update(axial_permeability - permeability, structure_tensor, 1.0);

      reaction_tensor.Invert(permeability_tensor);
      reaction_tensor.Scale(dynamic_viscosity);
    }
  };

  /*!
   * @brief Reaction tensor (anisotropy) strategy for constant material orthotropy.
   *
   * Orthotropy can only be used in 3D simulations. For now, only constant material permeabilities
   * are allowed, i.e. there is no dependency between the permeability and porosity in the
   * material reaction tensor.
   */
  class PoroConstantMaterialOrthotropyStrategy : public PoroAnisotropyStrategyBase
  {
   public:
    //! Simple constructor
    PoroConstantMaterialOrthotropyStrategy(const MAT::PAR::FluidPoro* params)
        : PoroAnisotropyStrategyBase(params){};

    //! compute reaction coefficient for constant material orthotropy
    double ComputeReactionCoeff() const override
    {
      const double viscosity = params_->viscosity_;
      const std::vector<double> orthotropic_permeabilities = params_->orthotropic_permeabilities_;

      if (viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      for (int dim = 0; dim < 3; ++dim)
        if (orthotropic_permeabilities[dim] <= 0.0)
          FOUR_C_THROW("zero or negative permeability in direction " + std::to_string(dim + 1));

      // trace of the reaction tensor divided by 3
      const double reacoeff =
          viscosity / 3. *
          (1. / orthotropic_permeabilities[0] + 1. / orthotropic_permeabilities[1] +
              1. / orthotropic_permeabilities[2]);

      return reacoeff;
    };

    //! constant material orthotropy in 2D is not allowed
    void ComputeReactionTensor(CORE::LINALG::Matrix<2, 2>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      FOUR_C_THROW("Use transverse isotropy in 2D!");
    };

    //! compute reaction tensor for constant material orthotropy - 3D
    void ComputeReactionTensor(CORE::LINALG::Matrix<3, 3>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        [[maybe_unused]] const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      const double dynamic_viscosity = params_->viscosity_;
      const std::vector<double> orthotropic_permeabilities = params_->orthotropic_permeabilities_;

      if (dynamic_viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      for (int dim = 0; dim < 3; ++dim)
        if (orthotropic_permeabilities[dim] <= 0.0)
          FOUR_C_THROW("zero or negative permeability in direction " + std::to_string(dim + 1));
      if (anisotropic_permeability_directions.empty())
        FOUR_C_THROW("orthotropy directions not specified");

      reaction_tensor.Clear();

      CORE::LINALG::Matrix<3, 3> permeability_tensor(true);
      CORE::LINALG::Matrix<3, 3> structure_tensor(true);

      for (int dim = 0; dim < 3; ++dim)
      {
        CreateStructureTensorFromVector<3>(
            anisotropic_permeability_directions[dim], structure_tensor);
        permeability_tensor.Update(orthotropic_permeabilities[dim], structure_tensor, 1.0);
      }

      reaction_tensor.Invert(permeability_tensor);
      reaction_tensor.Scale(dynamic_viscosity);
    };

    //! constant material orthotropy in 2D is not allowed
    void ComputeLinMatReactionTensor(CORE::LINALG::Matrix<2, 2>& linreac_dphi,
        CORE::LINALG::Matrix<2, 2>& linreac_dJ, const double& J,
        const double& porosity) const override
    {
      // constant material orthotropy in 2D is not allowed
      FOUR_C_THROW("Use transverse isotropy in 2D!");
    };
  };

  /*!
   * @brief Reaction tensor (anisotropy) strategy for nodal material orthotropy.
   *
   * Orthotropy can only be used in 3D simulations. For now, only constant material permeabilities
   * are allowed, i.e. there is no dependency between the permeability and porosity in the
   * material reaction tensor.
   *
   * This orthotropy strategy does not obtain the different permeability values from the material
   * properties. Instead, it works with a basis permeability value from the material properties and
   * scales it in different directions with coefficients that are provided by the evaluation
   * methods. Obtaining the scaling coefficients from the evaluation methods allows to have varying
   * permeabilities inside the elements, where each element has prescribed nodal values for the
   * coefficients.
   */
  class PoroConstantMaterialNodalOrthotropyStrategy : public PoroAnisotropyStrategyBase
  {
   public:
    //! Simple constructor
    PoroConstantMaterialNodalOrthotropyStrategy(const MAT::PAR::FluidPoro* params)
        : PoroAnisotropyStrategyBase(params){};

    //! compute reaction coefficient for constant material nodal orthotropy
    double ComputeReactionCoeff() const override
    {
      const double viscosity = params_->viscosity_;
      const double permeability = params_->permeability_;

      if (viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      if (permeability <= 0.0) FOUR_C_THROW("zero or negative permeability");

      // reaction coefficient computed with the base permeability as a good approximation
      const double reacoeff = viscosity / permeability;

      return reacoeff;
    };

    //!  compute reaction tensor for constant material nodal orthotropy - 2D
    void ComputeReactionTensor(CORE::LINALG::Matrix<2, 2>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      ComputeReactionTensorForConstantMaterialNodalOrthotropy<2>(reaction_tensor, J, porosity,
          anisotropic_permeability_directions, anisotropic_permeability_coeffs);
    };

    //! compute reaction tensor for constant material nodal orthotropy - 3D
    void ComputeReactionTensor(CORE::LINALG::Matrix<3, 3>& reaction_tensor, const double& J,
        const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        const std::vector<double>& anisotropic_permeability_coeffs) const override
    {
      ComputeReactionTensorForConstantMaterialNodalOrthotropy<3>(reaction_tensor, J, porosity,
          anisotropic_permeability_directions, anisotropic_permeability_coeffs);
    };

    /*!
     * @brief Compute reaction tensor for constant material nodal orthotropy
     *
     * Templated for the number of spatial dimensions, compute the specific reaction tensor in the
     * case of constant material nodal orthotropic permeability.
     *
     * @tparam dim  Number of spatial dimensions
     * @param [out] reaction_tensor Reaction tensor (@p dim x @p dim dyadic)
     * @param [in] J  Determinant of the deformation gradient
     * @param [in] porosity   Porosity value
     * @param [in] anisotropic_permeability_directions    Principal directions of anisotropy
     *                                                    required to construct the reaction
     *                                                    tensor
     * @param [in] anisotropic_permeability_coeffs  Scaling coefficients for the permeability in
     *                                              anisotropy directions
     */
    template <unsigned int dim>
    void ComputeReactionTensorForConstantMaterialNodalOrthotropy(
        CORE::LINALG::Matrix<dim, dim>& reaction_tensor, const double& J, const double& porosity,
        const std::vector<std::vector<double>>& anisotropic_permeability_directions,
        const std::vector<double>& anisotropic_permeability_coeffs) const
    {
      const double dynamic_viscosity = params_->viscosity_;
      const double permeability = params_->permeability_;

      if (dynamic_viscosity <= 0.0) FOUR_C_THROW("zero or negative viscosity");
      if (permeability <= 0.0) FOUR_C_THROW("zero or negative permeability");
      if (anisotropic_permeability_directions.empty())
        FOUR_C_THROW("orthotropy directions not specified");
      if (anisotropic_permeability_coeffs.empty())
        FOUR_C_THROW("orthotropy coefficients not specified");

      reaction_tensor.Clear();

      CORE::LINALG::Matrix<dim, dim> permeability_tensor(true);
      CORE::LINALG::Matrix<dim, dim> structure_tensor(true);

      for (unsigned int i = 0; i < dim; ++i)
      {
        CreateStructureTensorFromVector<dim>(
            anisotropic_permeability_directions[i], structure_tensor);
        permeability_tensor.Update(
            permeability * anisotropic_permeability_coeffs[i], structure_tensor, 1.0);
      }

      reaction_tensor.Invert(permeability_tensor);
      reaction_tensor.Scale(dynamic_viscosity);
    }
  };

  Teuchos::RCP<MAT::FLUIDPORO::PoroAnisotropyStrategyBase> CreateAnisotropyStrategy(
      const MAT::PAR::FluidPoro* params)
  {
    switch (params->permeability_func_)
    {
      case MAT::PAR::constant:
      case MAT::PAR::kozeny_carman:
        return Teuchos::rcp(new MAT::FLUIDPORO::PoroIsotropyStrategy(params));
      case MAT::PAR::const_material_transverse:
        return Teuchos::rcp(
            new MAT::FLUIDPORO::PoroConstantMaterialTransverseIsotropyStrategy(params));
      case MAT::PAR::const_material_orthotropic:
        return Teuchos::rcp(new MAT::FLUIDPORO::PoroConstantMaterialOrthotropyStrategy(params));
      case MAT::PAR::const_material_nodal_orthotropic:
        return Teuchos::rcp(
            new MAT::FLUIDPORO::PoroConstantMaterialNodalOrthotropyStrategy(params));
      default:
        return Teuchos::null;
    }
  }

}  // namespace MAT::FLUIDPORO

MAT::PAR::FluidPoro::FluidPoro(Teuchos::RCP<MAT::PAR::Material> matdata)
    : Parameter(matdata),
      viscosity_(*matdata->Get<double>("DYNVISCOSITY")),
      density_(*matdata->Get<double>("DENSITY")),
      permeability_(*matdata->Get<double>("PERMEABILITY")),
      axial_permeability_(*matdata->Get<double>("AXIALPERMEABILITY")),
      type_(undefined),
      varying_permeability_(false),
      permeability_func_(MAT::PAR::pf_undefined),
      permeability_correction_factor_(1.0),
      initial_porosity_(1.0)
{
  const auto* typestring = matdata->Get<std::string>("TYPE");

  if (*typestring == "Darcy")
    type_ = darcy;
  else if (*typestring == "Darcy-Brinkman")
    type_ = darcy_brinkman;

  const auto* pfuncstring = matdata->Get<std::string>("PERMEABILITYFUNCTION");

  if (*pfuncstring == "Const")
    permeability_func_ = MAT::PAR::constant;
  else if (*pfuncstring == "Kozeny_Carman")
    permeability_func_ = MAT::PAR::kozeny_carman;
  else if (*pfuncstring == "Const_Material_Transverse")
    permeability_func_ = MAT::PAR::const_material_transverse;
  else if (*pfuncstring == "Const_Material_Orthotropy")
    permeability_func_ = MAT::PAR::const_material_orthotropic;
  else if (*pfuncstring == "Const_Material_Nodal_Orthotropy")
    permeability_func_ = MAT::PAR::const_material_nodal_orthotropic;
  else
    FOUR_C_THROW("Unknown permeability function: %s", pfuncstring->c_str());

  orthotropic_permeabilities_.resize(3, 0.0);
  if (permeability_func_ == MAT::PAR::const_material_orthotropic)
  {
    for (int dim = 0; dim < 3; ++dim)
      orthotropic_permeabilities_[dim] =
          *matdata->Get<double>("ORTHOPERMEABILITY" + std::to_string(dim + 1));
  }
}

Teuchos::RCP<MAT::Material> MAT::PAR::FluidPoro::CreateMaterial()
{
  return Teuchos::rcp(new MAT::FluidPoro(this));
}

void MAT::PAR::FluidPoro::SetInitialPorosity(double initial_porosity)
{
  initial_porosity_ = initial_porosity;

  if (permeability_func_ == MAT::PAR::kozeny_carman)
  {
    // c = (phi0^3 / (1 - phi0^2))
    permeability_correction_factor_ = initial_porosity_ * initial_porosity_ * initial_porosity_ /
                                      (1 - initial_porosity_ * initial_porosity_);
  }
  else
  {
    permeability_correction_factor_ = 1.0;
  }
}

MAT::FluidPoroType MAT::FluidPoroType::instance_;

CORE::COMM::ParObject* MAT::FluidPoroType::Create(const std::vector<char>& data)
{
  auto* fluid_poro = new MAT::FluidPoro();
  fluid_poro->Unpack(data);
  return fluid_poro;
}

MAT::FluidPoro::FluidPoro() : params_(nullptr), anisotropy_strategy_(Teuchos::null) {}

MAT::FluidPoro::FluidPoro(MAT::PAR::FluidPoro* params) : params_(params)
{
  anisotropy_strategy_ = MAT::FLUIDPORO::CreateAnisotropyStrategy(params);
}

void MAT::FluidPoro::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data, matid);
}

void MAT::FluidPoro::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid
  int matid;
  ExtractfromPack(position, data, matid);
  params_ = nullptr;
  if (GLOBAL::Problem::Instance()->Materials() != Teuchos::null)
  {
    if (GLOBAL::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::FluidPoro*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }
  }

  // Only execute if not in post-process mode
  if (params_ != nullptr) anisotropy_strategy_ = MAT::FLUIDPORO::CreateAnisotropyStrategy(params_);

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", data.size(), position);
}

double MAT::FluidPoro::ComputeReactionCoeff() const
{
  return anisotropy_strategy_->ComputeReactionCoeff();
}

void MAT::FluidPoro::ComputeReactionTensor(CORE::LINALG::Matrix<2, 2>& reaction_tensor,
    const double& J, const double& porosity,
    const std::vector<std::vector<double>>& anisotropic_permeability_directions,
    const std::vector<double>& anisotropic_permeability_coeffs) const
{
  anisotropy_strategy_->ComputeReactionTensor(reaction_tensor, J, porosity,
      anisotropic_permeability_directions, anisotropic_permeability_coeffs);
}

void MAT::FluidPoro::ComputeReactionTensor(CORE::LINALG::Matrix<3, 3>& reaction_tensor,
    const double& J, const double& porosity,
    const std::vector<std::vector<double>>& anisotropic_permeability_directions,
    const std::vector<double>& anisotropic_permeability_coeffs) const
{
  anisotropy_strategy_->ComputeReactionTensor(reaction_tensor, J, porosity,
      anisotropic_permeability_directions, anisotropic_permeability_coeffs);
}

void MAT::FluidPoro::ComputeLinMatReactionTensor(CORE::LINALG::Matrix<2, 2>& linreac_dphi,
    CORE::LINALG::Matrix<2, 2>& linreac_dJ, const double& J, const double& porosity) const
{
  anisotropy_strategy_->ComputeLinMatReactionTensor(linreac_dphi, linreac_dJ, J, porosity);
}

void MAT::FluidPoro::ComputeLinMatReactionTensor(CORE::LINALG::Matrix<3, 3>& linreac_dphi,
    CORE::LINALG::Matrix<3, 3>& linreac_dJ, const double& J, const double& porosity) const
{
  anisotropy_strategy_->ComputeLinMatReactionTensor(linreac_dphi, linreac_dJ, J, porosity);
}

double MAT::FluidPoro::EffectiveViscosity() const
{
  // set zero viscosity and only modify it for Darcy-Stokes problems
  double viscosity = -1.0;
  if (Type() == PAR::darcy)
    viscosity = 0.0;
  else if (Type() == PAR::darcy_brinkman)
    viscosity = Viscosity();
  else
    FOUR_C_THROW("Unknown flow type for porous flow");

  return viscosity;
}

FOUR_C_NAMESPACE_CLOSE
