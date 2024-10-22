// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_structure_new_expl_ab2.hpp"

#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_structure_new_model_evaluator_manager.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_structure_new_timint_basedataglobalstate.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Solid::EXPLICIT::AdamsBashforth2::AdamsBashforth2()
    : fvisconp_ptr_(Teuchos::null),
      fviscon_ptr_(Teuchos::null),
      finertianp_ptr_(Teuchos::null),
      finertian_ptr_(Teuchos::null)
{
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::setup()
{
  check_init();

  // Call the setup() of the abstract base class first.
  Generic::setup();

  // ---------------------------------------------------------------------------
  // setup pointers to the force vectors of the global state data container
  // ---------------------------------------------------------------------------
  finertian_ptr_ = global_state().get_finertial_n();
  finertianp_ptr_ = global_state().get_finertial_np();

  fviscon_ptr_ = global_state().get_fvisco_n();
  fvisconp_ptr_ = global_state().get_fvisco_np();

  // ---------------------------------------------------------------------------
  // resizing of multi-step quantities
  // ---------------------------------------------------------------------------
  global_state().get_multi_time()->resize(-1, 0, true);
  global_state().get_delta_time()->resize(-1, 0, true);
  global_state().get_multi_dis()->resize(-1, 0, global_state().dof_row_map_view(), true);
  global_state().get_multi_vel()->resize(-1, 0, global_state().dof_row_map_view(), true);
  global_state().get_multi_acc()->resize(-1, 0, global_state().dof_row_map_view(), true);

  // here we initialized the dt of previous steps in the database, since a resize is performed
  const double dt = (*global_state().get_delta_time())[0];
  global_state().get_delta_time()->update_steps(dt);

  // -------------------------------------------------------------------
  // set initial displacement
  // -------------------------------------------------------------------
  set_initial_displacement(
      tim_int().get_data_sdyn().get_initial_disp(), tim_int().get_data_sdyn().start_func_no());

  // Has to be set before the post_setup() routine is called!
  issetup_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::post_setup()
{
  check_init_setup();
  equilibrate_initial_state();

  model_eval().post_setup();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::set_state(const Core::LinAlg::Vector<double>& x)
{
  check_init_setup();

  const double dt = (*global_state().get_delta_time())[0];
  const double dto = (*global_state().get_delta_time())[-1];
  const double dta = (2.0 * dt * dto + dt * dt) / (2.0 * dto);
  const double dtb = -(dt * dt) / (2.0 * dto);

  // ---------------------------------------------------------------------------
  // new end-point acceleration
  // ---------------------------------------------------------------------------
  Teuchos::RCP<Core::LinAlg::Vector<double>> accnp_ptr = global_state().extract_displ_entries(x);
  global_state().get_acc_np()->Scale(1.0, *accnp_ptr);

  // ---------------------------------------------------------------------------
  // new end-point velocities
  // ---------------------------------------------------------------------------
  global_state().get_vel_np()->Update(1.0, (*(global_state().get_multi_vel()))[0], 0.0);
  global_state().get_vel_np()->Update(dta, (*(global_state().get_multi_acc()))[0], dtb,
      (*(global_state().get_multi_acc()))[-1], 1.0);

  // ---------------------------------------------------------------------------
  // new end-point displacements
  // ---------------------------------------------------------------------------
  global_state().get_dis_np()->Update(1.0, (*(global_state().get_multi_dis()))[0], 0.0);
  global_state().get_dis_np()->Update(dta, (*(global_state().get_multi_vel()))[0], dtb,
      (*(global_state().get_multi_vel()))[-1], 1.0);

  // ---------------------------------------------------------------------------
  // update the elemental state
  // ---------------------------------------------------------------------------
  model_eval().update_residual();
  model_eval().run_recover();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::add_visco_mass_contributions(
    Core::LinAlg::Vector<double>& f) const
{
  // viscous damping forces at t_{n+1}
  Core::LinAlg::assemble_my_vector(1.0, f, 1.0, *fvisconp_ptr_);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::add_visco_mass_contributions(
    Core::LinAlg::SparseOperator& jac) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff_ptr = global_state().extract_displ_block(jac);
  // set mass matrix
  stiff_ptr->add(*global_state().get_mass_matrix(), false, 1.0, 0.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  check_init_setup();
  // write dynamic forces
  iowriter.write_vector("finert", finertian_ptr_);
  iowriter.write_vector("fvisco", fviscon_ptr_);

  model_eval().write_restart(iowriter, forced_writerestart);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::read_restart(Core::IO::DiscretizationReader& ioreader)
{
  check_init_setup();
  ioreader.read_vector(finertian_ptr_, "finert");
  ioreader.read_vector(fviscon_ptr_, "fvisco");

  model_eval().read_restart(ioreader);
  update_constant_state_contributions();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::EXPLICIT::AdamsBashforth2::update_step_state()
{
  check_init_setup();

  // ---------------------------------------------------------------------------
  // dynamic effects
  // ---------------------------------------------------------------------------
  // new at t_{n+1} -> t_n
  //    finertial_{n} := finertial_{n+1}
  finertian_ptr_->Scale(1.0, *finertianp_ptr_);
  // new at t_{n+1} -> t_n
  //    fviscous_{n} := fviscous_{n+1}
  fviscon_ptr_->Scale(1.0, *fvisconp_ptr_);

  // ---------------------------------------------------------------------------
  // update model specific variables
  // ---------------------------------------------------------------------------
  model_eval().update_step_state(0.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double Solid::EXPLICIT::AdamsBashforth2::method_lin_err_coeff_dis() const
{
  const double dt = (*global_state().get_delta_time())[0];
  const double dto = (*global_state().get_delta_time())[-1];
  return (2. * dt + 3. * dto) / (12. * dt);
}

FOUR_C_NAMESPACE_CLOSE
