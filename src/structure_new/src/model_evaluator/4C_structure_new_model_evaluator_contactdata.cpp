// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_inpar_contact.hpp"
#include "4C_structure_new_integrator.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_nln_solver_nox.hpp"
#include "4C_structure_new_timint_implicit.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Solid::ModelEvaluator::ContactData::ContactData()
    : isinit_(false),
      issetup_(false),
      mortar_action_(Mortar::eval_none),
      var_type_(Inpar::CONTACT::var_unknown),
      coupling_scheme_(Inpar::CONTACT::CouplingScheme::unknown),
      str_data_ptr_(Teuchos::null)
{
  // empty constructor
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::ContactData::init(
    const Teuchos::RCP<const Solid::ModelEvaluator::Data>& str_data_ptr)
{
  issetup_ = false;
  str_data_ptr_ = str_data_ptr;
  isinit_ = true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::ContactData::setup()
{
  check_init();

  issetup_ = true;
}

FOUR_C_NAMESPACE_CLOSE
