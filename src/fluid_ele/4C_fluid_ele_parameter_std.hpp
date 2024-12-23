// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_FLUID_ELE_PARAMETER_STD_HPP
#define FOUR_C_FLUID_ELE_PARAMETER_STD_HPP

#include "4C_config.hpp"

#include "4C_fluid_ele_parameter.hpp"
#include "4C_utils_singleton_owner.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Discret
{
  namespace Elements
  {
    class FluidEleParameterStd : public FluidEleParameter
    {
     public:
      /// Singleton access method
      static FluidEleParameterStd* instance(
          Core::Utils::SingletonAction action = Core::Utils::SingletonAction::create);

     private:
     protected:
      /// protected Constructor since we are a Singleton.
      FluidEleParameterStd();
    };

  }  // namespace Elements
}  // namespace Discret

FOUR_C_NAMESPACE_CLOSE

#endif
