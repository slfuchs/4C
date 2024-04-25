/*---------------------------------------------------------------------*/
/*! \file

\brief Evaluation and assembly of all meshtying terms


\level 3

*/
/*---------------------------------------------------------------------*/

#ifndef FOUR_C_STRUCTURE_NEW_MODEL_EVALUATOR_MESHTYING_HPP
#define FOUR_C_STRUCTURE_NEW_MODEL_EVALUATOR_MESHTYING_HPP

#include "4C_config.hpp"

#include "4C_structure_new_model_evaluator_generic.hpp"
#include "4C_structure_new_timint_basedataglobalstate.hpp"

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace CONTACT
{
  class Manager;
  class MtAbstractStrategy;
}  // namespace CONTACT

namespace MORTAR
{
  class StrategyBase;
}  // namespace MORTAR

namespace STR
{
  namespace MODELEVALUATOR
  {
    class MeshtyingData;

    /*! \brief Model evaluator for meshtying problems
     *
     */
    class Meshtying : public Generic
    {
     public:
      //! constructor
      Meshtying();


      /*! \brief Initialize class variables [derived]
       *
       * @param eval_data_ptr
       * @param gstate_ptr
       * @param gio_ptr
       * @param int_ptr
       * @param timint_ptr
       * @param dof_offset
       */
      void Init(const Teuchos::RCP<STR::MODELEVALUATOR::Data>& eval_data_ptr,
          const Teuchos::RCP<STR::TIMINT::BaseDataGlobalState>& gstate_ptr,
          const Teuchos::RCP<STR::TIMINT::BaseDataIO>& gio_ptr,
          const Teuchos::RCP<STR::Integrator>& int_ptr,
          const Teuchos::RCP<const STR::TIMINT::Base>& timint_ptr, const int& dof_offset) override;

      //! setup class variables [derived]
      void Setup() override;

      //! @name Functions which are derived from the base generic class
      //!@{

      //! [derived]
      INPAR::STR::ModelType Type() const override { return INPAR::STR::model_meshtying; }

      //! [derived]
      void RemoveCondensedContributionsFromRhs(Epetra_Vector& rhs) override;

      //! [derived]
      bool AssembleForce(Epetra_Vector& f, const double& timefac_np) const override;

      //! Assemble the jacobian at \f$t_{n+1}\f$
      bool AssembleJacobian(
          CORE::LINALG::SparseOperator& jac, const double& timefac_np) const override;

      //! [derived]
      void WriteRestart(
          IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const override;

      //! [derived]
      void ReadRestart(IO::DiscretizationReader& ioreader) override;

      //! [derived]
      void Predict(const INPAR::STR::PredEnum& pred_type) override{};

      //! [derived]
      void RunPostComputeX(
          const Epetra_Vector& xold, const Epetra_Vector& dir, const Epetra_Vector& xnew) override;

      //! [derived]
      void RunPreComputeX(const Epetra_Vector& xold, Epetra_Vector& dir_mutable,
          const NOX::NLN::Group& curr_grp) override{};

      //! [derived]
      void RunPostIterate(const ::NOX::Solver::Generic& solver) override{};

      //! [derived]
      void RunPostApplyJacobianInverse(const Epetra_Vector& rhs, Epetra_Vector& result,
          const Epetra_Vector& xold, const NOX::NLN::Group& grp) override;

      //! [derived]
      void RunPreApplyJacobianInverse(const Epetra_Vector& rhs, Epetra_Vector& result,
          const Epetra_Vector& xold, const NOX::NLN::Group& grp) override;

      //! [derived]
      void UpdateStepState(const double& timefac_n) override{};

      //! [derived]
      void UpdateStepElement() override{};

      //! [derived]
      void DetermineStressStrain() override{};

      //! [derived]
      void DetermineEnergy() override{};

      //! [derived]
      void DetermineOptionalQuantity() override{};

      //! [derived]
      void OutputStepState(IO::DiscretizationWriter& iowriter) const override{};

      //! [derived]
      void ResetStepState() override{};

      //! [derived]
      Teuchos::RCP<const Epetra_Map> GetBlockDofRowMapPtr() const override;

      //! [derived]
      Teuchos::RCP<const Epetra_Vector> GetCurrentSolutionPtr() const override;

      //! [derived]
      Teuchos::RCP<const Epetra_Vector> GetLastTimeStepSolutionPtr() const override;

      //! [derived]
      void PostOutput() override{};

      /*! \brief Reset model specific variables (without jacobian) [derived]
       *
       * Nothing to do in case of meshtying.
       *
       * \param[in] x Current full state vector
       */
      void Reset(const Epetra_Vector& x) override{};

      //! \brief Perform actions just before the Evaluate() call [derived]
      void PreEvaluate() override{};

      //! \brief Perform actions right after the Evaluate() call [derived]
      void PostEvaluate() override{};

      //! @}

      //! @name Call-back routines
      //!@{

      Teuchos::RCP<const CORE::LINALG::SparseMatrix> GetJacobianBlock(
          const STR::MatBlockType bt) const;

      /** \brief Assemble the structural right-hand side vector
       *
       *  \param[in] without_these_models  Exclude all models defined in this vector
       *                                   during the assembly
       *  \param[in] apply_dbc             Apply Dirichlet boundary conditions
       *
       *  \author hiermeier \date 08/17 */
      Teuchos::RCP<Epetra_Vector> AssembleForceOfModels(
          const std::vector<INPAR::STR::ModelType>* without_these_models = nullptr,
          const bool apply_dbc = false) const;

      virtual Teuchos::RCP<CORE::LINALG::SparseOperator> GetAuxDisplJacobian() const
      {
        return Teuchos::null;
      };

      void EvaluateWeightedGapGradientError();

      //! [derived]
      bool EvaluateForce() override;

      //! [derived]
      bool EvaluateStiff() override;

      //! [derived]
      bool EvaluateForceStiff() override;

      /*!
      \brief Apply results of mesh initialization to the underlying problem discretization

      \note This is only necessary in case of a mortar method.

      \warning This routine modifies the reference coordinates of slave nodes at the meshtying
      interface.

      @param[in] Xslavemod Vector with modified nodal positions
      */
      void ApplyMeshInitialization(Teuchos::RCP<const Epetra_Vector> Xslavemod);

      //!@}

      //! @name Accessors
      //!@{

      //! Returns a pointer to the underlying meshtying strategy object
      const Teuchos::RCP<CONTACT::MtAbstractStrategy>& StrategyPtr();

      //! Returns the underlying meshtying strategy object
      CONTACT::MtAbstractStrategy& Strategy();
      const CONTACT::MtAbstractStrategy& Strategy() const;

      //!@}

     protected:
     private:
      /// Set the correct time integration parameters within the meshtying strategy
      void SetTimeIntegrationInfo(CONTACT::MtAbstractStrategy& strategy) const;

      //! meshtying strategy
      Teuchos::RCP<CONTACT::MtAbstractStrategy> strategy_ptr_;

      //! Mesh relocation for conservation of angular momentum
      Teuchos::RCP<Epetra_Vector> mesh_relocation_;
    };  // namespace MODELEVALUATOR

  }  // namespace MODELEVALUATOR
}  // namespace STR

FOUR_C_NAMESPACE_CLOSE

#endif
