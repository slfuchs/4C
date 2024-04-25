/*--------------------------------------------------------------------------*/
/*! \file

\brief Solve FSI problem with sliding grids using a monolithic scheme
with condensed fluid interface velocities


\level 2
*/
/*--------------------------------------------------------------------------*/
#ifndef FOUR_C_FSI_SLIDINGMONOLITHIC_FLUIDSPLIT_HPP
#define FOUR_C_FSI_SLIDINGMONOLITHIC_FLUIDSPLIT_HPP


#include "4C_config.hpp"

#include "4C_adapter_ale_fsi_msht.hpp"
#include "4C_adapter_fld_fluid_fsi_msht.hpp"
#include "4C_coupling_adapter.hpp"
#include "4C_coupling_adapter_mortar.hpp"
#include "4C_fsi_monolithic.hpp"
#include "4C_inpar_fsi.hpp"

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace ADAPTER
{
  class Coupling;
  class CouplingMortar;
}  // namespace ADAPTER

namespace CORE::LINALG
{
  class BlockSparseMatrixBase;
  class MatrixColTransform;
}  // namespace CORE::LINALG

namespace FSI
{
  class OverlappingBlockMatrix;

  namespace UTILS
  {
    class SlideAleUtils;
  }  // namespace UTILS
}  // namespace FSI


namespace FSI
{
  /// monolithic FSI algorithm with overlapping non-matching interface equations
  /*!

    In the sense of mortar coupling, fluid split means that
    the fluid field is chosen as slave field.
    Hence, the fluid velocity interface degrees of freedom are condensed
    from the system along with the condensation of the Lagrange multiplier
    field, that is used to enforce the coupling conditions.

    The fluid interface velocities are computed based on the structural
    interface displacements. The conversion is done by
    ADAPTER::FluidFSI::DisplacementToVelocity().

    \sa SlidingMonolithicFluidSplit
    \author wirtz
    \date 01/16
     */
  class SlidingMonolithicFluidSplit : public BlockMonolithic
  {
    friend class FSI::FSIResultTest;

   public:
    explicit SlidingMonolithicFluidSplit(
        const Epetra_Comm& comm, const Teuchos::ParameterList& timeparams);

    /*! do the setup for the monolithic system


    1.) setup coupling
    2.) create combined map
    3.) create block system matrix


    */
    void SetupSystem() override;

    //! @name Apply current field state to system

    /// setup composed system matrix from field solvers
    void SetupSystemMatrix(CORE::LINALG::BlockSparseMatrixBase& mat) override;

    //@}

    /// the composed system matrix
    Teuchos::RCP<CORE::LINALG::BlockSparseMatrixBase> SystemMatrix() const override;

    //! @name Methods for infnorm-scaling of the system

    /// apply infnorm scaling to linear block system
    void ScaleSystem(CORE::LINALG::BlockSparseMatrixBase& mat,  ///< Jacobian matrix
        Epetra_Vector& b                                        ///< right hand side
        ) override;

    /// undo infnorm scaling from scaled solution
    void UnscaleSolution(CORE::LINALG::BlockSparseMatrixBase& mat,  ///< Jacobian matrix
        Epetra_Vector& x,                                           ///< solution vector
        Epetra_Vector& b                                            ///< right hand side
        ) override;

    //@}

    /// read restart
    void ReadRestart(int step) override;

    /// start a new time step
    void PrepareTimeStep() override;

    /*! \brief Recover Lagrange multiplier \f$\lambda_\Gamma\f$
     *
     *  Recover Lagrange multiplier \f$\lambda_\Gamma\f$ at the interface at the
     *  end of each time step (i.e. condensed forces onto the structure) needed
     *  for rhs in next time step in order to guarantee temporal consistent
     *  exchange of coupling traction
     */
    void RecoverLagrangeMultiplier() override;

    /*! \brief Compute spurious interface energy increment due to temporal discretization
     *
     *  Due to the temporal discretization, spurious energy
     *  \f$\Delta E_\Gamma^{n\rightarrow n+1}\f$ might be produced at the
     *  interface. It can be computed as
     *  \f[
     *  \Delta E_\Gamma^{n\rightarrow n+1}
     *  = \left((a-b)\lambda^n +
     * (b-a)\lambda^{n+1}\right)\left(d_\Gamma^{S,n+1}-d_\Gamma^{S,n}\right) \f] with the time
     * interpolation factors a and b.
     *
     *  \author mayr.mt \date 05/2014
     */
    void CalculateInterfaceEnergyIncrement() override;

    /*! \brief Additional safety check of kinematic constraint during a single time step:
     *
     *  Constraint equation:
     *
     *  \f$D \mathbf{d}_{\Gamma}^{n+1} - D \mathbf{d}_{\Gamma}^{n} - \tau * M * \Delta
     * \mathbf{u}_{\Gamma}^{n+1} - \Delta t M * \mathbf{u}_{\Gamma}^{n} \doteq \mathbf{0}\f$
     *
     *  with interface time integration factor
     *  \f$\tau = \begin{cases}\frac{\Delta t}{2} & \text {if }2^{nd}\text{ order}\\ \Delta t& \text
     * {if }1^{st}\text{ order}\end{cases}\f$
     *
     *  Do this check only for safety reasons. Basically, the constraint is
     *  satisfied due to solving the condensed nonlinear system of equations.
     *  We expect really small violation norms.
     *
     *  \author mayr.mt \date 10/2012
     */
    virtual void CheckKinematicConstraint();

    /*! \brief Additional safety check of dynamic equilibrium during a single time step:
     *
     *  Dynamic equilibrium at the interface:
     *
     *  \f$M^{T} \mathbf{\lambda} - D^{T} \mathbf{\lambda} = \mathbf{0}\f$
     *
     *  Do this check only for safety reasons. Basically, the constraint is
     *  satisfied due to solving the condensed nonlinear system of equations.
     *  We expect really small violation norms.
     *
     *  \author mayr.mt \date 10/2012
     */
    virtual void CheckDynamicEquilibrium();

    //! @name Time Adaptivity
    //@{

    /*! \brief Select \f$\Delta t_{min}\f$ of all proposed time step sizes
     *         based on error estimation
     *
     *  Depending on the chosen method (fluid or structure split), only 3 of the
     *  6 available norms are useful. Each of these three norms delivers a new
     *  time step size. Select the minimum of these three as the new time step
     *  size.
     *
     *  \author mayr.mt \date 08/2013
     */
    double SelectDtErrorBased() const override;

    /*! \brief Check whether time step is accepted or not
     *
     *  In case that the local truncation error is small enough, the time step
     *  is accepted.
     *
     *  \author mayr.mt \date 08/2013
     */
    bool SetAccepted() const override;

    //@}

    /*! \brief Find future / desired owner for each node at the interface
     *
     *  The relation is saved in the map \c nodeOwner as node -- owner.
     *
     *  In \c inverseNodeOwner the same information is contained in the form
     *  owner -- nodes.
     *
     *  The maps are built for interface nodes of the domain \c domain, where
     *  domain = {fluid, structure}.
     */
    void CreateNodeOwnerRelationship(std::map<int, int>* nodeOwner,
        std::map<int, std::list<int>>* inverseNodeOwner, std::map<int, DRT::Node*>* fluidnodesPtr,
        std::map<int, DRT::Node*>* structuregnodesPtr,
        Teuchos::RCP<DRT::Discretization> structuredis, Teuchos::RCP<DRT::Discretization> fluiddis,
        const INPAR::FSI::Redistribute domain) override;

    Teuchos::RCP<ADAPTER::FluidFSIMsht> FsiFluidField()
    {
      return Teuchos::rcp_static_cast<ADAPTER::FluidFSIMsht>(FluidField());
    }

    Teuchos::RCP<ADAPTER::AleFsiMshtWrapper> FsiAleField()
    {
      return Teuchos::rcp_static_cast<ADAPTER::AleFsiMshtWrapper>(AleField());
    }

   protected:
    /// create the composed system matrix
    void CreateSystemMatrix();

    void Update() override;

    void Output() override;

    /// Write Lagrange multiplier
    void OutputLambda() override;

    /// setup solver for global block system
    Teuchos::RCP<::NOX::Epetra::LinearSystem> CreateLinearSystem(Teuchos::ParameterList& nlParams,
        ::NOX::Epetra::Vector& noxSoln, Teuchos::RCP<::NOX::Utils> utils) override;

    /// setup of NOX convergence tests
    Teuchos::RCP<::NOX::StatusTest::Combo> CreateStatusTest(
        Teuchos::ParameterList& nlParams, Teuchos::RCP<::NOX::Epetra::Group> grp) override;

    /* \brief Extract the three field vectors from a given composed vector
     *
     *  The condensed ale degrees of freedom have to be recovered
     *  from the structure solution by a mortar mapping across the interface.
     *  The condensed fluid degrees of freedom have to be recovered
     *  from the ale solution using a suitable displacement-velocity
     *  conversion.
     *
     *  We are dealing with NOX here, so we get absolute values. x is the sum of
     *  all increments up to this point within this time step. Hence, the
     *  solution increments due to predictors have to be treated in a special
     *  way.
     *
     *  \sa  ADAPTER::FluidFSI::DisplacementToVelocity()
     */
    void ExtractFieldVectors(
        Teuchos::RCP<const Epetra_Vector> x,    ///< composed vector that contains all field vectors
        Teuchos::RCP<const Epetra_Vector>& sx,  ///< structural displacements
        Teuchos::RCP<const Epetra_Vector>& fx,  ///< fluid velocities and pressure
        Teuchos::RCP<const Epetra_Vector>& ax   ///< ale displacements
        ) override;

   private:
    /*! \brief Create the combined DOF row map for the FSI problem
     *
     *  Combine the DOF row maps of structure, fluid and ALE to an global FSI
     *  DOF row map.
     *
     *  \author mayr.mt \date 05/2014
     */
    void CreateCombinedDofRowMap() override;

    /*! \brief Setup the Dirichlet map extractor
     *
     *  Create a map extractor #dbcmaps_ for the Dirichlet degrees of freedom
     *  for the entire FSI problem. This is done just by combining the
     *  condition maps and other maps from structure, fluid and ALE to a
     *  FSI-global condition map and other map.
     *
     *  \author mayr.mt \date 05/2014
     */
    void SetupDBCMapExtractor() override;

    /// setup RHS contributions based on single field residuals
    void SetupRHSResidual(Epetra_Vector& f) override;

    /// setup RHS contributions based on the Lagrange multiplier field
    void SetupRHSLambda(Epetra_Vector& f) override;

    /// setup RHS contributions based on terms for first nonlinear iteration
    void SetupRHSFirstiter(Epetra_Vector& f) override;

    void CombineFieldVectors(Epetra_Vector& v, Teuchos::RCP<const Epetra_Vector> sv,
        Teuchos::RCP<const Epetra_Vector> fv, Teuchos::RCP<const Epetra_Vector> av,
        const bool slave_vectors_contain_interface_dofs) final;

    //! Create #lambda_ and #lambdaold_
    void SetLambda() override;

    //! Set #notsetup_ = true after redistribution
    void SetNotSetup() override
    {
      notsetup_ = true;
      return;
    }

    /*! block system matrix
     *  System matrix has a 4x4-block structure corresponding to the vector of unknowns
     *
     *  \f$\Delta x^T = [\Delta d_I^{S,n+1}~\Delta d_\Gamma^{S,n+1}~\Delta u_I^{F,n+1}~\Delta
     * d_I^{G,n+1}]\f$
     */
    Teuchos::RCP<CORE::LINALG::BlockSparseMatrixBase> systemmatrix_;

    /// communicator
    const Epetra_Comm& comm_;

    /// @name Matrix block transform objects
    /// Handle row and column map exchange for matrix blocks

    /// coupling of fluid and ale at the free surface
    Teuchos::RCP<CORE::ADAPTER::Coupling> fscoupfa_;

    /// coupling of structure and fluid at the interface
    Teuchos::RCP<CORE::ADAPTER::CouplingMortar> coupsfm_;

    Teuchos::RCP<CORE::LINALG::MatrixColTransform> aigtransform_;
    Teuchos::RCP<CORE::LINALG::MatrixColTransform> fmiitransform_;

    ///@}

    /// @name infnorm scaling

    Teuchos::RCP<Epetra_Vector> srowsum_;
    Teuchos::RCP<Epetra_Vector> scolsum_;
    Teuchos::RCP<Epetra_Vector> arowsum_;
    Teuchos::RCP<Epetra_Vector> acolsum_;

    //@}

    /// additional ale residual to avoid incremental ale errors
    Teuchos::RCP<Epetra_Vector> aleresidual_;

    /// preconditioned block Krylov or block Gauss-Seidel linear solver
    INPAR::FSI::LinearBlockSolver linearsolverstrategy_;

    /// ale movement relative to structure (none, slide_curr, slide_ref)
    INPAR::FSI::SlideALEProj aleproj_;
    bool notsetup_;  ///< indicates if Setup has not been called yet

    Teuchos::RCP<FSI::UTILS::SlideAleUtils> slideale_;  ///< Sliding Ale helper class

    Teuchos::RCP<Epetra_Vector> iprojdispinc_;  ///< displacement of fluid side of the interface
    Teuchos::RCP<Epetra_Vector> iprojdisp_;     ///< displacement of fluid side of the interface

    /// @name Recovery of Lagrange multiplier at the end of each time step

    //! Lagrange multiplier \f$\lambda_\Gamma^n\f$ at the interface (ie condensed forces onto the
    //! fluid) evaluated at old time step \f$t_n\f$ but needed for next time step \f$t_{n+1}\f$
    Teuchos::RCP<Epetra_Vector> lambda_;

    //! Lagrange multiplier of previous time step
    Teuchos::RCP<Epetra_Vector> lambdaold_;

    //! interface structure displacement increment \f$\Delta(\Delta d_{\Gamma,i+1}^{n+1})\f$ at
    //! current NOX iteration \f$i+1\f$
    Teuchos::RCP<Epetra_Vector> ddginc_;

    //! inner fluid velocity increment \f$\Delta(\Delta u_{I,i+1}^{n+1})\f$ at current NOX iteration
    //! \f$i+1\f$
    Teuchos::RCP<Epetra_Vector> duiinc_;

    //! interface displacement solution of the structure at previous NOX iteration
    Teuchos::RCP<const Epetra_Vector> disgprev_;

    //! inner velocity solution of fluid at previous NOX iteration
    Teuchos::RCP<const Epetra_Vector> veliprev_;

    //! interface velocity solution of the fluid at previous NOX iteration
    Teuchos::RCP<const Epetra_Vector> velgprev_;

    //! inner ALE displacement solution at previous NOX iteration
    Teuchos::RCP<const Epetra_Vector> aleiprev_;

    //! interface ALE displacement solution at previous NOX iteration
    Teuchos::RCP<const Epetra_Vector> alegprev_;

    //! inner ALE displacement increment \f$\Delta(\Delta d_{I,i+1}^{G,n+1})\f$ at current NOX
    //! iteration \f$i+1\f$
    Teuchos::RCP<Epetra_Vector> ddialeinc_;

    //! block \f$F_{\Gamma I,i+1}\f$ of fluid matrix at current NOX iteration \f$i+1\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fgicur_;

    //! block \f$F_{\Gamma I,i}\f$ of fluid matrix at previous NOX iteration \f$i\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fgiprev_;

    //! block \f$F_{\Gamma\Gamma,i+1}\f$ of fluid matrix at current NOX iteration \f$i+1\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fggcur_;

    //! block \f$F_{\Gamma\Gamma,i}\f$ of fluid matrix at previous NOX iteration \f$i\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fggprev_;

    //! block \f$F_{\Gamma I,i+1}^G\f$ of fluid shape derivatives matrix at current NOX iteration
    //! \f$i+1\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fmgicur_;

    //! block \f$F_{\Gamma I,i}^G\f$ of fluid shape derivatives matrix at previous NOX iteration
    //! \f$i\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fmgiprev_;

    //! block \f$F_{\Gamma\Gamma,i+1}^G\f$ of fluid shape derivatives matrix at current NOX
    //! iteration \f$i+1\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fmggcur_;

    //! block \f$F_{\Gamma\Gamma,i}^G\f$ of fluid shape derivatives matrix at previous NOX iteration
    //! \f$i\f$
    Teuchos::RCP<const CORE::LINALG::SparseMatrix> fmggprev_;

    //@}

    //! summation of amount of artificial interface energy due to temporal discretization
    double energysum_;
  };
}  // namespace FSI


FOUR_C_NAMESPACE_CLOSE

#endif
