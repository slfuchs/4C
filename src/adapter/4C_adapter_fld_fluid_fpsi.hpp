// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_ADAPTER_FLD_FLUID_FPSI_HPP
#define FOUR_C_ADAPTER_FLD_FLUID_FPSI_HPP

#include "4C_config.hpp"

#include "4C_adapter_fld_fluid_fsi.hpp"

FOUR_C_NAMESPACE_OPEN

namespace FPSI
{
  namespace Utils
  {
    class MapExtractor;
  }
}  // namespace FPSI

namespace Adapter
{
  /*! \brief Fluid field adapter for fpsi
   *
   *
   *  Can only be used in conjunction with #FLD::FluidImplicitTimeInt
   */
  class FluidFPSI : public FluidFSI
  {
   public:
    /// Constructor
    FluidFPSI(Teuchos::RCP<Fluid> fluid, Teuchos::RCP<Core::FE::Discretization> dis,
        Teuchos::RCP<Core::LinAlg::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
        Teuchos::RCP<Core::IO::DiscretizationWriter> output, bool isale, bool dirichletcond);

    /// initialize algorithm
    void init() override;

    /// communication object at the interface without pressure dofs for FPSI problems
    Teuchos::RCP<FLD::Utils::MapExtractor> const& fpsi_interface() const override
    {
      return fpsiinterface_;
    }

    void setup_interface(const int nds_master = 0) override;

    void use_block_matrix(bool splitmatrix) override;
    virtual void use_block_matrix(
        bool splitmatrix, Teuchos::RCP<FPSI::Utils::MapExtractor> const& shapederivSplitter);

   protected:
    /// the interface map setup for fpsi interface
    Teuchos::RCP<FLD::Utils::MapExtractor> fpsiinterface_;

  };  // class FluidFPSI
}  // namespace Adapter

FOUR_C_NAMESPACE_CLOSE

#endif
