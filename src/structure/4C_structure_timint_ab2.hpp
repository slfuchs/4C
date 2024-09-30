/*----------------------------------------------------------------------*/
/*! \file
\brief Structural time integration with Adams-Bashforth 2nd order (explicit)
\level 1

*/

/*----------------------------------------------------------------------*/
#ifndef FOUR_C_STRUCTURE_TIMINT_AB2_HPP
#define FOUR_C_STRUCTURE_TIMINT_AB2_HPP

/*----------------------------------------------------------------------*/
/* headers */
#include "4C_config.hpp"

#include "4C_structure_timint_expl.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/* belongs to structural dynamics namespace */
namespace Solid
{
  /*====================================================================*/
  /*!
   * \brief Adams-Bashforth2: 2nd order accurate,
   *                          explicit time integrator,
   *                          linear 2-step method
   * \author bborn
   * \date 06/08
   */
  class TimIntAB2 : public TimIntExpl
  {
    //! A friendly relation to adaptive partner
    friend class TimAdaAB2;

   public:
    //! @name Life
    //@{

    //! Constructor
    TimIntAB2(const Teuchos::ParameterList& timeparams,  //!< time parameters
        const Teuchos::ParameterList& ioparams,          //!< ioflags
        const Teuchos::ParameterList& sdynparams,        //!< input parameters
        const Teuchos::ParameterList& xparams,           //!< extra flags
        // const Teuchos::ParameterList& ab2params,  //!< AB2 flags
        Teuchos::RCP<Core::FE::Discretization> actdis,       //!< current discretisation
        Teuchos::RCP<Core::LinAlg::Solver> solver,           //!< the solver
        Teuchos::RCP<Core::LinAlg::Solver> contactsolver,    //!< the solver for contact meshtying
        Teuchos::RCP<Core::IO::DiscretizationWriter> output  //!< the output
    );

    //! Copy constructor
    TimIntAB2(const TimIntAB2& old) : TimIntExpl(old) { ; }

    /*! \brief Initialize this object

    Hand in all objects/parameters/etc. from outside.
    Construct and manipulate internal objects.

    \note Try to only perform actions in init(), which are still valid
          after parallel redistribution of discretizations.
          If you have to perform an action depending on the parallel
          distribution, make sure you adapt the affected objects after
          parallel redistribution.
          Example: cloning a discretization from another discretization is
          OK in init(...). However, after redistribution of the source
          discretization do not forget to also redistribute the cloned
          discretization.
          All objects relying on the parallel distribution are supposed to
          the constructed in \ref setup().

    \warning none
    \return bool
    \date 08/16
    \author rauch  */
    void init(const Teuchos::ParameterList& timeparams, const Teuchos::ParameterList& sdynparams,
        const Teuchos::ParameterList& xparams, Teuchos::RCP<Core::FE::Discretization> actdis,
        Teuchos::RCP<Core::LinAlg::Solver> solver) override;

    /*! \brief Setup all class internal objects and members

     setup() is not supposed to have any input arguments !

     Must only be called after init().

     Construct all objects depending on the parallel distribution and
     relying on valid maps like, e.g. the state vectors, system matrices, etc.

     Call all setup() routines on previously initialized internal objects and members.

    \note Must only be called after parallel (re-)distribution of discretizations is finished !
          Otherwise, e.g. vectors may have wrong maps.

    \warning none
    \return void
    \date 08/16
    \author rauch  */
    void setup() override;

    //@}

    //! @name Actions
    //@{

    //! Resize \p TimIntMStep<T> multi-step quantities
    void resize_m_step() override;

    //! Do time integration of single step
    int integrate_step() override;

    //! Update configuration after time step
    //!
    //! Thus the 'last' converged is lost and a reset of the time step
    //! becomes impossible. We are ready and keen awaiting the next time step.
    void update_step_state() override;

    //! Update Element
    void update_step_element() override;

    //@}

    //! @name Attribute access functions
    //@{

    //! Return time integrator name
    enum Inpar::Solid::DynamicType method_name() const override { return Inpar::Solid::dyna_ab2; }

    //! Provide number of steps, e.g. a single-step method returns 1,
    //! a m-multistep method returns m
    int method_steps() const override { return 2; }

    //! Give local order of accuracy of displacement part
    int method_order_of_accuracy_dis() const override { return 2; }

    //! Give local order of accuracy of velocity part
    int method_order_of_accuracy_vel() const override { return 2; }

    //! Return linear error coefficient of displacements
    double method_lin_err_coeff_dis() const override
    {
      const double dt = (*dt_)[0];
      const double dto = (*dt_)[-1];
      return (2. * dt + 3. * dto) / (12. * dt);
    }

    //! Return linear error coefficient of velocities
    double method_lin_err_coeff_vel() const override { return method_lin_err_coeff_dis(); }

    //@}

    //! @name System vectors
    //@{

    //! Return external force \f$F_{ext,n}\f$
    Teuchos::RCP<Core::LinAlg::Vector> fext() override { return fextn_; }

    //! Return external force \f$F_{ext,n+1}\f$
    Teuchos::RCP<Core::LinAlg::Vector> fext_new() override
    {
      FOUR_C_THROW("FextNew() not available in AB2");
      return Teuchos::null;
    }

    //! Read and set restart for forces
    void read_restart_force() override;

    //! Write internal and external forces for restart
    void write_restart_force(Teuchos::RCP<Core::IO::DiscretizationWriter> output) override;

    //@}


   protected:
    //! @name Global forces at \f$t_{n+1}\f$
    //@{
    Teuchos::RCP<Core::LinAlg::Vector> fextn_;   //!< external force
                                                 //!< \f$F_{int;n+1}\f$
    Teuchos::RCP<Core::LinAlg::Vector> fintn_;   //!< internal force
                                                 //!< \f$F_{int;n+1}\f$
    Teuchos::RCP<Core::LinAlg::Vector> fviscn_;  //!< Rayleigh viscous forces
                                                 //!< \f$C \cdot V_{n+1}\f$
    Teuchos::RCP<Core::LinAlg::Vector> fcmtn_;   //!< contact or meshtying forces
                                                 //!< \f$F_{cmt;n+1}\f$
    Teuchos::RCP<Core::LinAlg::Vector> frimpn_;  //!< time derivative of
                                                 //!< linear momentum
                                                 //!< (temporal rate of impulse)
                                                 //!< \f$\dot{P}_{n+1} = M \cdot \dot{V}_{n+1}\f$
    //@}

  };  // class TimIntAB2

}  // namespace Solid

/*----------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif
