/*----------------------------------------------------------------------*/
/*! \file

\brief Algorithm for the calculation of biofilm growth.
       It consists of:
       - an inner timeloop (resolving fsi and scatra (in both fluid and structure)
       at fluid-dynamic time-scale
       - an outer timeloop (resolving only the biofilm growth)
       at biological time-scale

\level 3


*----------------------------------------------------------------------*/

#ifndef FOUR_C_FS3I_BIOFILM_FSI_HPP
#define FOUR_C_FS3I_BIOFILM_FSI_HPP


#include "4C_config.hpp"

#include "4C_coupling_adapter.hpp"
#include "4C_fs3i_partitioned_1wc.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace Adapter
{
  class AleFsiWrapper;
  class StructureBio;
  class FSIStructureWrapper;
}  // namespace Adapter

namespace ALE
{
  class AleBaseAlgorithm;
}

namespace FS3I
{
  class BiofilmFSI : public PartFS3I1Wc
  {
   public:
    BiofilmFSI(const Epetra_Comm& comm);

    void init() override;

    void setup() override;

    void timeloop() override;

    void inner_timeloop();

    //! information transfer FSI -> ScaTra
    void set_fsi_solution();

    void compute_interface_vectors(Teuchos::RCP<Core::LinAlg::Vector> idispnp_,
        Teuchos::RCP<Core::LinAlg::Vector> iveln_, Teuchos::RCP<Core::LinAlg::Vector> struidispnp_,
        Teuchos::RCP<Core::LinAlg::Vector> struiveln_);

    Teuchos::RCP<Core::LinAlg::Vector> fluid_to_ale(Teuchos::RCP<Core::LinAlg::Vector> iv) const;

    Teuchos::RCP<Core::LinAlg::Vector> ale_to_fluid_field(
        Teuchos::RCP<Core::LinAlg::Vector> iv) const;

    /// field transform
    virtual Teuchos::RCP<Core::LinAlg::Vector> ale_to_struct_field(
        Teuchos::RCP<Core::LinAlg::Vector> iv) const;

    /// field transform
    virtual Teuchos::RCP<Core::LinAlg::Vector> ale_to_struct_field(
        Teuchos::RCP<const Core::LinAlg::Vector> iv) const;

    /// interface transform
    virtual Teuchos::RCP<Core::LinAlg::Vector> struct_to_ale(
        Teuchos::RCP<Core::LinAlg::Vector> iv) const;

    /// interface transform
    virtual Teuchos::RCP<Core::LinAlg::Vector> struct_to_ale(
        Teuchos::RCP<const Core::LinAlg::Vector> iv) const;

    /// solve fluid-ale
    virtual void fluid_ale_solve();

    /// solve structure-ale
    virtual void struct_ale_solve();

    void update_and_output();

    const Epetra_Comm& comm() { return comm_; }

    void vec_to_scatravec(Teuchos::RCP<Core::FE::Discretization> scatradis,
        Teuchos::RCP<Core::LinAlg::Vector> vec, Teuchos::RCP<Epetra_MultiVector> scatravec);

    void struct_gmsh_output();

    void fluid_gmsh_output();

   private:
    /// communication (mainly for screen output)
    const Epetra_Comm& comm_;

    /// coupling of fluid and ale (interface only)
    Teuchos::RCP<Coupling::Adapter::Coupling> icoupfa_;

    /// coupling of fluid and ale (whole field)
    Teuchos::RCP<Coupling::Adapter::Coupling> coupfa_;

    /// coupling of structure and ale (interface only)
    Teuchos::RCP<Coupling::Adapter::Coupling> icoupsa_;

    /// coupling of structure and ale (whole field)
    Teuchos::RCP<Coupling::Adapter::Coupling> coupsa_;

    // Teuchos::RCP< ::Adapter::FSIStructureWrapper> structure_;

    // Teuchos::RCP<ALE::AleBaseAlgorithm> ale_;
    Teuchos::RCP<Adapter::AleFsiWrapper> ale_;

    //    // total flux at the interface overall the InnerTimeloop
    //    Teuchos::RCP<Epetra_MultiVector> flux;
    //
    //    // total flux at the structure interface overall the InnerTimeloop
    //    Teuchos::RCP<Epetra_MultiVector> struflux;

    Teuchos::RCP<Core::LinAlg::Vector> norminflux_;

    Teuchos::RCP<Core::LinAlg::Vector> lambda_;
    Teuchos::RCP<Core::LinAlg::Vector> normtraction_;
    Teuchos::RCP<Core::LinAlg::Vector> tangtractionone_;
    Teuchos::RCP<Core::LinAlg::Vector> tangtractiontwo_;

    std::vector<double> nvector_;

    // coefficients used in the calculation of the displacement due to growth
    // fluxcoef_ multiply the scalar influx at the interface,
    // while normforcecoef_, tangoneforcecoef_ and tangtwoforcecoef_  multiply forces
    // in the normal and in the two tangential directions at the interface
    double fluxcoef_;
    double normforceposcoef_;
    double normforcenegcoef_;
    double tangoneforcecoef_;
    double tangtwoforcecoef_;

    //// growth time parameters

    // number of steps
    int nstep_bio_;

    // current step
    int step_bio_;

    // time step size
    double dt_bio_;

    // total time of the outer loop
    double time_bio_;


    //// scatra and fsi time parameters

    // number of steps
    int nstep_fsi_;

    // current step
    int step_fsi_;

    // time step size
    double dt_fsi_;

    // total time of the inner loop
    double time_fsi_;

    // maximum time
    double maxtime_fsi_;

    // total time
    double time_;

    /// fluid interface displacement at time t^{n}
    Teuchos::RCP<Core::LinAlg::Vector> idispn_;

    /// fluid interface displacement at time t^{n+1}
    Teuchos::RCP<Core::LinAlg::Vector> idispnp_;

    /// fluid velocity at interface (always zero!)
    Teuchos::RCP<Core::LinAlg::Vector> iveln_;

    /// structure interface displacement at time t^{n}
    Teuchos::RCP<Core::LinAlg::Vector> struidispn_;

    /// structure interface displacement at time t^{n+1}
    Teuchos::RCP<Core::LinAlg::Vector> struidispnp_;

    /// structure velocity at interface (always zero!)
    Teuchos::RCP<Core::LinAlg::Vector> struiveln_;

    /// total structure displacement due to growth
    Teuchos::RCP<Core::LinAlg::Vector> struct_growth_disp_;

    /// total fluid displacement due to growth
    Teuchos::RCP<Core::LinAlg::Vector> fluid_growth_disp_;

    /// total scatra structure displacement due to growth
    Teuchos::RCP<Epetra_MultiVector> scatra_struct_growth_disp_;

    /// total scatra fluid displacement due to growth
    Teuchos::RCP<Epetra_MultiVector> scatra_fluid_growth_disp_;
  };

}  // namespace FS3I

FOUR_C_NAMESPACE_CLOSE

#endif
