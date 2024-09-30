/*-----------------------------------------------------------*/
/*! \file

\brief Model evaluator for structure part of partitioned fsi


\level 3

*/
/*-----------------------------------------------------------*/


#ifndef FOUR_C_FSI_STR_MODEL_EVALUATOR_PARTITIONED_HPP
#define FOUR_C_FSI_STR_MODEL_EVALUATOR_PARTITIONED_HPP

#include "4C_config.hpp"

#include "4C_structure_new_model_evaluator_generic.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Adapter
{
  class Structure;
}  // namespace Adapter

namespace Solid
{
  namespace ModelEvaluator
  {
    class PartitionedFSI : public Generic
    {
     public:
      //! constructor
      PartitionedFSI();


      //! get pointer to force vector at time level n+1 (full structural map).
      //! interface part is inserted in Adapter::FSIStructureWrapper.
      const Teuchos::RCP<Core::LinAlg::Vector>& get_interface_force_np_ptr()
      {
        return interface_force_np_ptr_;
      };

      //! setup class variables [derived]
      void setup() override;

      //! @name Functions which are derived from the base generic class
      //! @{

      //! [derived]
      Inpar::Solid::ModelType type() const override
      {
        return Inpar::Solid::model_partitioned_coupling;
      }

      //! reset class variables (without jacobian) [derived]
      void reset(const Core::LinAlg::Vector& x) override { return; };

      //! [derived]
      bool evaluate_force() override { return true; };

      //! [derived]
      bool evaluate_stiff() override { return true; };

      //! [derived] not needed in partitioned scheme
      bool evaluate_force_stiff() override { return true; };

      //! derived
      void pre_evaluate() override { return; };

      //! derived
      void post_evaluate() override { return; };

      //! derived
      bool assemble_force(Core::LinAlg::Vector& f, const double& timefac_np) const override;

      //! Assemble the jacobian at \f$t_{n+1}\f$ not needed in partitioned scheme
      bool assemble_jacobian(
          Core::LinAlg::SparseOperator& jac, const double& timefac_np) const override
      {
        return true;
      };

      //! [derived]
      void write_restart(
          Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const override
      {
        return;
      };

      //! [derived]
      void read_restart(Core::IO::DiscretizationReader& ioreader) override { return; };

      //! [derived]
      void predict(const Inpar::Solid::PredEnum& pred_type) override { return; };

      //! derived
      void run_pre_compute_x(const Core::LinAlg::Vector& xold, Core::LinAlg::Vector& dir_mutable,
          const NOX::Nln::Group& curr_grp) override
      {
        return;
      };

      //! recover condensed Lagrange multipliers
      void run_post_compute_x(const Core::LinAlg::Vector& xold, const Core::LinAlg::Vector& dir,
          const Core::LinAlg::Vector& xnew) override
      {
        return;
      };

      //! derived
      void run_post_iterate(const ::NOX::Solver::Generic& solver) override { return; };

      //! [derived]
      void update_step_state(const double& timefac_n) override;

      //! [derived]
      void update_step_element() override { return; };

      //! [derived]
      void determine_stress_strain() override { return; };

      //! [derived]
      void determine_energy() override { return; };

      //! [derived]
      void determine_optional_quantity() override { return; };

      //! [derived]
      void output_step_state(Core::IO::DiscretizationWriter& iowriter) const override { return; };

      //! derived
      void reset_step_state() override { return; };

      //! [derived]
      void post_output() override { return; };

      //! [derived]
      Teuchos::RCP<const Epetra_Map> get_block_dof_row_map_ptr() const override;

      //! [derived]
      Teuchos::RCP<const Core::LinAlg::Vector> get_current_solution_ptr() const override;

      //! [derived]
      Teuchos::RCP<const Core::LinAlg::Vector> get_last_time_step_solution_ptr() const override;

      //! @}

      /*! \brief linear structure solve with just a interface load
       *
       * The very special solve done in steepest descent relaxation
       * calculation (and matrix free Newton Krylov).
       *
       * \note Can only be called after a valid structural solve. */
      Teuchos::RCP<const Core::LinAlg::Vector> solve_relaxation_linear(
          Teuchos::RCP<Adapter::Structure> structure);

      virtual void setup_multi_map_extractor();

      //! set flag true if in order to request a relaxation solve
      void set_is_relaxation_solve(bool trueorfalse) { is_relaxationsolve_ = trueorfalse; };

      //! Returns the global input/output data container
      const Solid::TimeInt::BaseDataIO& get_in_output() const;

     private:
      //! fsi interface force at \f$t_{n+1}\f$
      Teuchos::RCP<Core::LinAlg::Vector> interface_force_np_ptr_;

      //! true if relaxation solve is requested
      bool is_relaxationsolve_;

    };  // class PartitionedFSI

  }  // namespace ModelEvaluator
}  // namespace Solid


FOUR_C_NAMESPACE_CLOSE

#endif
