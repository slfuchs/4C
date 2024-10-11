/*---------------------------------------------------------------------*/
/*! \file

\brief Evaluation and assembly of all mesh-tying terms


\level 3

*/
/*---------------------------------------------------------------------*/


#include "4C_structure_new_model_evaluator_meshtying.hpp"

#include "4C_contact_meshtying_abstract_strategy.hpp"
#include "4C_contact_meshtying_noxinterface.hpp"
#include "4C_contact_meshtying_strategy_factory.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_linalg_sparsematrix.hpp"
#include "4C_linalg_utils_densematrix_communication.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linalg_vector.hpp"
#include "4C_solver_nonlin_nox_group.hpp"
#include "4C_solver_nonlin_nox_group_prepostoperator.hpp"
#include "4C_solver_nonlin_nox_solver_linesearchbased.hpp"
#include "4C_structure_new_dbc.hpp"
#include "4C_structure_new_impl_generic.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_model_evaluator_manager.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Solid::ModelEvaluator::Meshtying::Meshtying() : strategy_ptr_(Teuchos::null)
{
  // empty
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::init(
    const Teuchos::RCP<Solid::ModelEvaluator::Data>& eval_data_ptr,
    const Teuchos::RCP<Solid::TimeInt::BaseDataGlobalState>& gstate_ptr,
    const Teuchos::RCP<Solid::TimeInt::BaseDataIO>& gio_ptr,
    const Teuchos::RCP<Solid::Integrator>& int_ptr,
    const Teuchos::RCP<const Solid::TimeInt::Base>& timint_ptr, const int& dof_offset)
{
  Solid::ModelEvaluator::Generic::init(
      eval_data_ptr, gstate_ptr, gio_ptr, int_ptr, timint_ptr, dof_offset);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::setup()
{
  check_init();

  // create the meshtying factory
  Mortar::STRATEGY::FactoryMT factory;
  factory.init(global_state_ptr()->get_discret());
  factory.setup(Global::Problem::instance()->n_dim());

  // check the problem dimension
  factory.check_dimension();

  // create some local variables (later to be stored in strategy)
  std::vector<Teuchos::RCP<Mortar::Interface>> interfaces;
  Teuchos::ParameterList cparams;

  // read and check contact input parameters
  factory.read_and_check_input(cparams);

  // check for fill_complete of discretization
  if (not discret().filled()) FOUR_C_THROW("discretization is not fill_complete.");

  // ---------------------------------------------------------------------
  // build the meshtying interfaces
  // ---------------------------------------------------------------------
  // FixMe Would be great, if we get rid of these poro parameters...
  bool poroslave = false;
  bool poromaster = false;
  factory.build_interfaces(cparams, interfaces, poroslave, poromaster);

  // ---------------------------------------------------------------------
  // build the solver strategy object
  // ---------------------------------------------------------------------
  strategy_ptr_ = factory.build_strategy(cparams, poroslave, poromaster, dof_offset(), interfaces);

  // build the search tree
  factory.build_search_tree(interfaces);

  // ---------------------------------------------------------------------
  // final touches to the meshtying strategy
  // ---------------------------------------------------------------------
  strategy_ptr_->store_dirichlet_status(integrator().get_dbc().get_dbc_map_extractor());
  strategy_ptr_->set_state(Mortar::state_new_displacement, integrator().get_dbc().get_zeros());
  strategy_ptr_->save_reference_state(integrator().get_dbc().get_zeros_ptr());
  strategy_ptr_->evaluate_reference_state();
  strategy_ptr_->inttime_init();
  set_time_integration_info(*strategy_ptr_);
  strategy_ptr_->redistribute_meshtying();
  strategy_ptr_->mortar_coupling(integrator().get_dbc().get_zeros_ptr());

  strategy_ptr_->nox_interface_ptr()->init(global_state_ptr());
  strategy_ptr_->nox_interface_ptr()->setup();

  if (!global_state().get_restart_step())
  {
    // perform mesh initialization if required by input parameter MESH_RELOCATION
    auto mesh_relocation_parameter = Teuchos::getIntegralValue<Inpar::Mortar::MeshRelocation>(
        Global::Problem::instance()->mortar_coupling_params(), "MESH_RELOCATION");

    if (mesh_relocation_parameter == Inpar::Mortar::relocation_initial)
    {
      Teuchos::RCP<const Core::LinAlg::Vector<double>> Xslavemod =
          dynamic_cast<Mortar::StrategyBase&>(*strategy_ptr_).mesh_initialization();
      Teuchos::RCP<const Core::LinAlg::Vector<double>> Xslavemod_noredist;
      if (Xslavemod != Teuchos::null)
      {
        mesh_relocation_ =
            Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*global_state().dof_row_map());
        const auto gdiscret = global_state().get_discret();
        if (strategy_ptr_->par_redist())
        {
          Teuchos::RCP<Core::LinAlg::Vector<double>> original_vec =
              Teuchos::make_rcp<Core::LinAlg::Vector<double>>(
                  *(strategy_ptr_->non_redist_slave_row_dofs()), true);

          Epetra_Export exporter(Xslavemod->Map(), *strategy_ptr_->non_redist_slave_row_dofs());

          int err = original_vec->Export(*Xslavemod, exporter, Insert);
          if (err) FOUR_C_THROW("Import failed with err=%d", err);

          Xslavemod_noredist = original_vec;
        }
        else
        {
          Xslavemod_noredist = Xslavemod;
        }

        for (const auto& node : gdiscret->my_row_node_range())
        {
          for (int d = 0; d < strategy_ptr_->n_dim(); ++d)
          {
            int dof = gdiscret->dof(node, d);
            if (strategy_ptr_->non_redist_slave_row_dofs()->LID(dof) != -1)
            {
              mesh_relocation_->operator[](mesh_relocation_->Map().LID(dof)) =
                  node->x()[d] - Xslavemod_noredist->operator[](Xslavemod_noredist->Map().LID(dof));
            }
          }
        }

        apply_mesh_initialization(Xslavemod_noredist);
      }
    }
    else if (mesh_relocation_parameter == Inpar::Mortar::relocation_timestep)
    {
      FOUR_C_THROW(
          "Meshtying with MESH_RELOCATION every_timestep not permitted. Change to MESH_RELOCATION "
          "initial or MESH_RELOCATION no.");
    }
  }

  issetup_ = true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Meshtying::assemble_force(
    Core::LinAlg::Vector<double>& f, const double& timefac_np) const
{
  Teuchos::RCP<const Core::LinAlg::Vector<double>> block_vec_ptr = Teuchos::null;
  if (Teuchos::getIntegralValue<Inpar::Mortar::AlgorithmType>(strategy().params(), "ALGORITHM") ==
          Inpar::Mortar::algorithm_gpts ||
      strategy().is_penalty())
  {
    block_vec_ptr = strategy().get_rhs_block_ptr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    FOUR_C_ASSERT(!block_vec_ptr.is_null(), "force not available");
    Core::LinAlg::assemble_my_vector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else if (strategy().is_condensed_system())
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = strategy().get_rhs_block_ptr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    Core::LinAlg::assemble_my_vector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else if (strategy().is_saddle_point_system())
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = strategy().get_rhs_block_ptr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    Core::LinAlg::assemble_my_vector(1.0, f, timefac_np, *block_vec_ptr);
  }

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Meshtying::assemble_jacobian(
    Core::LinAlg::SparseOperator& jac, const double& timefac_np) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> block_ptr = Teuchos::null;
  int err = 0;
  // ---------------------------------------------------------------------
  // Penalty / gpts / Nitsche system: no additional/condensed dofs
  // ---------------------------------------------------------------------
  if (Teuchos::getIntegralValue<Inpar::Mortar::AlgorithmType>(strategy().params(), "ALGORITHM") ==
          Inpar::Mortar::algorithm_gpts ||
      strategy().is_penalty())
  {
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_displ);
    if (strategy().is_penalty() && block_ptr.is_null()) return true;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd = global_state().extract_displ_block(jac);
    jac_dd->add(*block_ptr, false, timefac_np, 1.0);
  }
  // ---------------------------------------------------------------------
  // condensed system of equations
  // ---------------------------------------------------------------------
  else if (strategy().is_condensed_system())
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_displ);
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
      jac_dd_ptr->add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }
  }
  // ---------------------------------------------------------------------
  // saddle-point system of equations or no contact contributions
  // ---------------------------------------------------------------------
  else if (strategy().system_type() == Inpar::CONTACT::system_saddlepoint)
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_displ);
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
      jac_dd_ptr->add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kdz - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_lm);
    if (not block_ptr.is_null())
    {
      //      block_ptr->Scale(timefac_np);
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::displ_lm);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzd - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::lm_displ);
    if (not block_ptr.is_null())
    {
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::lm_displ);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzz - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::lm_lm);
    if (not block_ptr.is_null())
    {
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::lm_lm);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }
  }

  return (err == 0);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Teuchos::RCP<CONTACT::MtAbstractStrategy>& Solid::ModelEvaluator::Meshtying::strategy_ptr()
{
  check_init_setup();
  return strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CONTACT::MtAbstractStrategy& Solid::ModelEvaluator::Meshtying::strategy()
{
  check_init_setup();
  return *strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const CONTACT::MtAbstractStrategy& Solid::ModelEvaluator::Meshtying::strategy() const
{
  check_init_setup();
  return *strategy_ptr_;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> Solid::ModelEvaluator::Meshtying::get_block_dof_row_map_ptr() const
{
  check_init_setup();
  if (strategy().lm_dof_row_map_ptr() == Teuchos::null)
    return global_state().dof_row_map();
  else
  {
    auto systype =
        Teuchos::getIntegralValue<Inpar::CONTACT::SystemType>(strategy().params(), "SYSTEM");

    if (systype == Inpar::CONTACT::system_saddlepoint)
      return strategy().lm_dof_row_map_ptr();
    else
      return global_state().dof_row_map();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::Vector<double>>
Solid::ModelEvaluator::Meshtying::get_current_solution_ptr() const
{
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::Vector<double>>
Solid::ModelEvaluator::Meshtying::get_last_time_step_solution_ptr() const
{
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::run_pre_apply_jacobian_inverse(
    const Core::LinAlg::Vector<double>& rhs, Core::LinAlg::Vector<double>& result,
    const Core::LinAlg::Vector<double>& xold, const NOX::Nln::Group& grp)
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd = global_state().jacobian_displ_block();
  const_cast<CONTACT::MtAbstractStrategy&>(strategy())
      .run_pre_apply_jacobian_inverse(jac_dd, const_cast<Core::LinAlg::Vector<double>&>(rhs));
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::run_post_apply_jacobian_inverse(
    const Core::LinAlg::Vector<double>& rhs, Core::LinAlg::Vector<double>& result,
    const Core::LinAlg::Vector<double>& xold, const NOX::Nln::Group& grp)
{
  const_cast<CONTACT::MtAbstractStrategy&>(strategy()).run_post_apply_jacobian_inverse(result);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::SparseMatrix> Solid::ModelEvaluator::Meshtying::get_jacobian_block(
    const Solid::MatBlockType bt) const
{
  return global_state().get_jacobian_block(type(), bt);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Meshtying::evaluate_force()
{
  return strategy().evaluate_force(global_state().get_dis_np());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Meshtying::evaluate_force_stiff()
{
  return strategy().evaluate_force_stiff(global_state().get_dis_np());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Meshtying::evaluate_stiff()
{
  return strategy().evaluate_stiff(global_state().get_dis_np());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::apply_mesh_initialization(
    Teuchos::RCP<const Core::LinAlg::Vector<double>> Xslavemod)
{
  // check modified positions vector
  if (Xslavemod == Teuchos::null) return;

  // create fully overlapping slave node map
  Teuchos::RCP<const Epetra_Map> slavemap = strategy_ptr_->slave_row_nodes_ptr();
  Teuchos::RCP<Epetra_Map> allreduceslavemap = Core::LinAlg::allreduce_e_map(*slavemap);

  // export modified node positions to column map of problem discretization
  const Epetra_Map* dof_colmap = discret_ptr()->dof_col_map();
  const Epetra_Map* node_colmap = discret_ptr()->node_col_map();
  Teuchos::RCP<Core::LinAlg::Vector<double>> Xslavemodcol =
      Core::LinAlg::create_vector(*dof_colmap, false);
  Core::LinAlg::export_to(*Xslavemod, *Xslavemodcol);

  const int numnode = allreduceslavemap->NumMyElements();
  const int numdim = Global::Problem::instance()->n_dim();
  const Core::LinAlg::Vector<double>& gvector = *Xslavemodcol;

  // loop over all slave nodes (for all procs)
  for (int index = 0; index < numnode; ++index)
  {
    int gid = allreduceslavemap->GID(index);

    // only do someting for nodes in my column map
    int ilid = node_colmap->LID(gid);
    if (ilid < 0) continue;

    Core::Nodes::Node* mynode = discret_ptr()->g_node(gid);

    // get degrees of freedom associated with this fluid/structure node
    std::vector<int> nodedofs = discret_ptr()->dof(0, mynode);
    std::vector<double> nvector(3, 0.0);

    // create new position vector
    for (int i = 0; i < numdim; ++i)
    {
      const int lid = gvector.Map().LID(nodedofs[i]);

      if (lid < 0)
        FOUR_C_THROW("ERROR: Proc %d: Cannot find gid=%d in Core::LinAlg::Vector<double>",
            gvector.Comm().MyPID(), nodedofs[i]);

      nvector[i] += gvector[lid];
    }

    // set new reference position
    mynode->set_pos(nvector);
  }

  // re-initialize finite elements
  Core::Communication::ParObjectFactory::instance().initialize_elements(discret());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::run_post_compute_x(const Core::LinAlg::Vector<double>& xold,
    const Core::LinAlg::Vector<double>& dir, const Core::LinAlg::Vector<double>& xnew)
{
  check_init_setup();

  strategy().run_post_compute_x(xold, dir, xnew);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::remove_condensed_contributions_from_rhs(
    Core::LinAlg::Vector<double>& rhs)
{
  check_init_setup();

  strategy().remove_condensed_contributions_from_rhs(rhs);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  if (mesh_relocation_ != Teuchos::null)
    iowriter.write_vector("mesh_relocation", mesh_relocation_);
  else
  {
    Teuchos::RCP<Core::LinAlg::Vector<double>> tmp =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*discret().dof_row_map(), true);
    iowriter.write_vector("mesh_relocation", tmp);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::read_restart(Core::IO::DiscretizationReader& ioreader)
{
  mesh_relocation_ =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*discret().dof_row_map(), true);
  ioreader.read_vector(mesh_relocation_, "mesh_relocation");

  strategy_ptr_->set_state(Mortar::state_new_displacement, *mesh_relocation_);
  strategy_ptr_->mortar_coupling(mesh_relocation_);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Meshtying::set_time_integration_info(
    CONTACT::MtAbstractStrategy& strategy) const
{
  const Inpar::Solid::DynamicType dyntype = tim_int().get_data_sdyn().get_dynamic_type();
  const double time_fac = integrator().get_int_param();

  strategy.set_time_integration_info(time_fac, dyntype);
}

FOUR_C_NAMESPACE_CLOSE
