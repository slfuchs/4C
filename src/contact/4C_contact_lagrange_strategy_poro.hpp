/*---------------------------------------------------------------------*/
/*! \file
\brief Contact Strategy handling the porous no penetraction condition on the active contact
interface

\level 3


*/
/*---------------------------------------------------------------------*/
#ifndef FOUR_C_CONTACT_LAGRANGE_STRATEGY_PORO_HPP
#define FOUR_C_CONTACT_LAGRANGE_STRATEGY_PORO_HPP

#include "4C_config.hpp"

#include "4C_contact_monocoupled_lagrange_strategy.hpp"
#include "4C_coupling_adapter.hpp"
#include "4C_coupling_adapter_converter.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

namespace CONTACT
{
  class LagrangeStrategyPoro : public MonoCoupledLagrangeStrategy
  {
   public:
    /*!
    \brief Standard Constructor

    */
    LagrangeStrategyPoro(const Teuchos::RCP<CONTACT::AbstractStratDataContainer>& data_ptr,
        const Epetra_Map* dof_row_map, const Epetra_Map* NodeRowMap, Teuchos::ParameterList params,
        std::vector<Teuchos::RCP<CONTACT::Interface>> interface, int dim,
        Teuchos::RCP<Epetra_Comm> comm, double alphaf, int maxdof, bool poroslave, bool poromaster);


    //! @name Access methods

    //@}

    //! @name Evaluation methods

    void do_read_restart(Core::IO::DiscretizationReader& reader,
        Teuchos::RCP<const Core::LinAlg::Vector> dis,
        Teuchos::RCP<CONTACT::ParamsInterface> cparams_ptr) override;

    /*! \brief Setup this strategy object (maps, vectors, etc.)

     All global maps and vectors are initialized by collecting
     the necessary information from all interfaces. In the case
     of a parallel redistribution, this method is called again
     to re-setup the above mentioned quantities. In this case
     we set the input parameter redistributed=TRUE. Moreover,
     when called for the first time (in the constructor) this
     method is given the input parameter init=TRUE to account
     for initialization of the active set. */
    void setup(bool redistributed, bool init) override;

    /*!
    /brief Activate the No Penetration Condition for the active contact surface!
     */
    void setup_no_penetration_condition();

    /*!
    \brief Initialize poro contact variables for next Newton step

    For a poro lagrangian strategy this includes the global normal / tangent matrices N and T,
    //Todo to be updated the global derivative matrices S and P and Tresca friction matrix L +
    vector r.
    */
    void poro_initialize(Coupling::Adapter::Coupling& coupfs,
        Teuchos::RCP<const Epetra_Map> fluiddofs, bool fullinit = true);

    /*!
    \brief as D and M Matrices are initialized here
    */
    void poro_mt_initialize();

    /*!
    \brief Prepare Matrices D and M, that are not Computed coming from the mortar adapter
    */
    void poro_mt_prepare_fluid_coupling();

    /*!
    \brief
    set some coupling matrices for the proro meshtying case mhataam-, dhat_ and invda_
    */
    void poro_mt_set_coupling_matrices();

    /*!
    \brief
    set old matrices dold_ mold_ and the old lagrange multiplier in case of poro meshtying
    */
    void poro_mt_update();

    /*!
    \brief Evaluate poro no penetration contact

     Evaluate poro coupling contact matrices for no penetration
     condition on contact surface
    */
    void evaluate_poro_no_pen_contact(
        Teuchos::RCP<Core::LinAlg::SparseMatrix>&
            k_fseff,  // global poro Coupling Matrix Fluid Structure K_FS
        Teuchos::RCP<Core::LinAlg::SparseMatrix>& Feff,  // global fluid Matrix in poro
        Teuchos::RCP<Core::LinAlg::Vector>& feff);       // global RHS of fluid in poro

    /*!
    \brief Evaluate poro no penetration contact

     Evaluate poro coupling contact matrices for no penetration
     condition on contact surface
    */
    void evaluate_poro_no_pen_contact(Teuchos::RCP<Core::LinAlg::SparseMatrix>& k_fseff,
        std::map<int, Teuchos::RCP<Core::LinAlg::SparseMatrix>*>& Feff,
        Teuchos::RCP<Core::LinAlg::Vector>& feff);

    /*!
    \brief Evaluate poro no penetration contact

     Evaluate all other matrixes here!
    */
    void evaluate_mat_poro_no_pen(Teuchos::RCP<Core::LinAlg::SparseMatrix>&
                                      k_fseff,  // global poro Coupling Matrix Fluid Structure K_FS
        Teuchos::RCP<Core::LinAlg::Vector>& feff);  // global RHS of fluid in poro

    /*!
    \brief Evaluate poro no penetration contact

     Evaluate all other matrixes here!
    */
    void evaluate_other_mat_poro_no_pen(
        Teuchos::RCP<Core::LinAlg::SparseMatrix>& Feff, int Column_Block_Id);

    /*!
    \brief Recovery method

    We only recover the Lagrange multipliers for poro no penetration condition here, which had been
    statically condensated during the setup of the global problem!*/

    void recover_poro_no_pen(
        Teuchos::RCP<Core::LinAlg::Vector> disi, Teuchos::RCP<Core::LinAlg::Vector> inc);

    void recover_poro_no_pen(Teuchos::RCP<Core::LinAlg::Vector> disi,
        std::map<int, Teuchos::RCP<Core::LinAlg::Vector>> inc);


    void update_poro_contact();

    /*!
    \brief Set current state
    ...Standard Implementation in Abstract Strategy:
    All interfaces are called to set the current deformation state
    (u, xspatial) in their nodes. Additionally, the new contact
    element areas are computed.

    ... + Overloaded Implementation in Poro Lagrange Strategy
    Set structure & fluid velocity and lagrangean multiplier to Contact nodes data container!!!

    \param statetype (in): enumerator defining which quantity to set (see mortar_interface.H for an
    overview) \param vec (in): current global state of the quantity defined by statetype
    */
    void set_state(
        const enum Mortar::StateType& statetype, const Core::LinAlg::Vector& vec) override;

    void set_parent_state(const enum Mortar::StateType& statetype, const Core::LinAlg::Vector& vec,
        const Core::FE::Discretization& dis) override;

    // Flag for Poro No Penetration Condition
    bool has_poro_no_penetration() const override { return no_penetration_; }

    // Return Lagrange Multiplier for No Penetration Condition!
    Teuchos::RCP<Core::LinAlg::Vector>& lambda_no_pen() { return lambda_; }
    Teuchos::RCP<const Core::LinAlg::Vector> lambda_no_pen() const { return lambda_; }

    // Return all active fluid slave dofs
    Teuchos::RCP<Epetra_Map>& fluid_active_n_dof_map() { return fgactiven_; };
    Teuchos::RCP<const Epetra_Map> fluid_active_n_dof_map() const { return fgactiven_; };

   protected:
    // don't want = operator and cctor
    LagrangeStrategyPoro operator=(const LagrangeStrategyPoro& old) = delete;
    LagrangeStrategyPoro(const LagrangeStrategyPoro& old) = delete;

    // flag activation poro contact no penetration condition
    // h.Willmann the name is misleading as the bool is also used for other cases to access some
    // methods
    bool no_penetration_;

    // time integration
    double nopenalpha_;  // 1-theta!!!

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        csfsn_;  // poro coupling stiffness block Csf_sn (needed for LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        csfsm_;  // poro coupling stiffness block Csf_sm (needed for LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        csfss_;  // poro coupling stiffness block Csf_ss (needed for LM)

    // For Recovery of no penetration lagrange multiplier!!!
    Teuchos::RCP<Core::LinAlg::Vector> ffs_;  // poro fluid RHS (needed for no pen LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        cfssn_;  // poro coupling stiffness block Cfs_sn (needed for no pen LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        cfssm_;  // poro coupling stiffness block Cfs_sm (needed for no pen LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        cfsss_;  // poro coupling stiffness block Cfs_ss (needed for no pen LM)

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fsn_;  // poro fluid stiffness block F_sn (needed for no pen LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fsm_;  // poro fluid stiffness block F_sm (needed for no pen LM)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fss_;  // poro fluid stiffness block F_ss (needed for no pen LM)

    std::map<int, Teuchos::RCP<Core::LinAlg::SparseOperator>>
        cfx_s_;  // offdiagonal coupling stiffness blocks on slave side!

    // Matrices transformed to the fluid dofs!!!
    Teuchos::RCP<Core::LinAlg::SparseMatrix> fdhat_;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> fmhataam_;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> finvda_;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> ftanginvD_;

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fdoldtransp_;  // global transposed Mortar matrix D (last end-point t_n)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fmoldtransp_;  // global transposed Mortar matrix M (last end-point t_n)


    Teuchos::RCP<Epetra_Map> fgsdofrowmap_;   // fluid slave dofs
    Teuchos::RCP<Epetra_Map> fgmdofrowmap_;   // fluid master dofs
    Teuchos::RCP<Epetra_Map> fgsmdofrowmap_;  // fluid slave + master dofs
    Teuchos::RCP<Epetra_Map> fgndofrowmap_;   // fluid other dofs
    Teuchos::RCP<Epetra_Map> fgactivedofs_;   // fluid active slave dofs
    Teuchos::RCP<Epetra_Map> falldofrowmap_;  // all fluid dofs
    Teuchos::RCP<Epetra_Map> fgactiven_;      // active normal fluid dofs
    Teuchos::RCP<Epetra_Map> fgactivet_;      // active tangential fluid dofs

    /// @name matrix transformation
    //! transform object for linearized ncoup matrix \f$linncoup\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> linncoupveltransform_;
    //! transform object for linearized ncoup matrix \f$linncoup\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> linncoupdisptransform_;
    //! transform object for tangential times Dinv matrix \f$T*D^-1\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowColTransform> tanginvtransform_;
    //! transform object for linearized tangentlambda matrix \f$lintanglambda\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> lintangentlambdatransform_;
    //! transform object for linearized Dlambda matrix \f$linDlambda\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> porolindmatrixtransform_;
    //! transform object for linearized Mlambda matrix \f$linMlambda\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> porolinmmatrixtransform_;  // h.Willmann
    //! transform object for mhataam = invda * mmatrixa
    Teuchos::RCP<Coupling::Adapter::MatrixRowColTransform> mhataamtransform_;
    //! transform object for dhat
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> dhattransform_;
    //! transform object for mold
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> doldtransform_;
    //! transform object for dold
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> moldtransform_;
    //! transform object for active part of inverse D matrix \f$invDa\f$
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> invDatransform_;


    Teuchos::RCP<Core::LinAlg::Vector>
        lambda_;  // current vector of Lagrange multipliers(for poro no pen.) at t_n+1
    Teuchos::RCP<Core::LinAlg::Vector>
        lambdaold_;  // old vector of Lagrange multipliers(for poro no pen.) at t_n

    //... add the relevant matrices !!!
    Teuchos::RCP<Core::LinAlg::Vector> NCoup_;  ///< normal coupling vector (for RHS)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        NCoup_lindisp_;  ///< linearisation of normal coupling w.r.t. displacements
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        NCoup_linvel_;  ///< linearisation of normal coupling w.r.t. fluid velocity

    Teuchos::RCP<Core::LinAlg::Vector>
        fNCoup_;  ///< normal coupling vector (for RHS) -- transformed to fluid dofs
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fNCoup_lindisp_;  ///< linearisation of normal coupling w.r.t. displacements -- transformed
                          ///< to fluid dofs
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fNCoup_linvel_;  ///< linearisation of normal coupling w.r.t. fluid velocity -- transformed
                         ///< to fluid dofs

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        Tangential_;  ///< matrix with tangential vectors inside

    Teuchos::RCP<Core::LinAlg::SparseMatrix> linTangentiallambda_;  ///< linearized tangential times
                                                                    ///< lambda

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        flinTangentiallambda_;  ///< linearized tangential times lambda -- transformed to fluid dofs

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        porolindmatrix_;  // global Matrix LinD containing slave fc derivatives (with lm from poro
                          // no penetration)
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        porolinmmatrix_;  // global Matrix LinM containing master fc derivatives (with lm from poro
                          // no penetration)

    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fporolindmatrix_;  // global Matrix LinD containing slave fc derivatives (with lm from poro
                           // no penetration) -- transformed to fluid dofs
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        fporolinmmatrix_;  // global Matrix LinM containing master fc derivatives (with lm from poro
                           // no penetration) -- transformed to fluid dofs

    bool poroslave_;   // true if interface slave side is purely poroelastic
    bool poromaster_;  // true if interface master sided is purely poroelastic
    // it must be assured that these two are previously set correctly and that there is no mixed
    // master or slave interface with both structural and poroelastic elements
  };  // class POROLagrangeStrategy
}  // namespace CONTACT


FOUR_C_NAMESPACE_CLOSE

#endif
