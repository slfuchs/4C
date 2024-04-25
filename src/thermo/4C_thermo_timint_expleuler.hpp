/*----------------------------------------------------------------------*/
/*! \file
\brief Thermal time integration with forward Euler order (explicit)
\level 3
*/

/*----------------------------------------------------------------------*
 | definitions                                               dano 01/12 |
 *----------------------------------------------------------------------*/
#ifndef FOUR_C_THERMO_TIMINT_EXPLEULER_HPP
#define FOUR_C_THERMO_TIMINT_EXPLEULER_HPP


/*----------------------------------------------------------------------*
 | headers                                                   dano 01/12 |
 *----------------------------------------------------------------------*/
#include "4C_config.hpp"

#include "4C_thermo_timint_expl.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 | belongs to thermal dynamics namespace                    bborn 01/12 |
 *----------------------------------------------------------------------*/
namespace THR
{
  /*====================================================================*/
  /*!
   * \brief Forward Euler
   *        explicit time integrator
   * \author dano
   * \date 01/12
   */
  class TimIntExplEuler : public TimIntExpl
  {
   public:
    //! @name Life
    //@{

    //! Constructor
    TimIntExplEuler(const Teuchos::ParameterList& ioparams,  //!< ioflags
        const Teuchos::ParameterList& tdynparams,            //!< input parameters
        const Teuchos::ParameterList& xparams,               //!< extra flags
        Teuchos::RCP<DRT::Discretization> actdis,            //!< current discretisation
        Teuchos::RCP<CORE::LINALG::Solver> solver,           //!< the solver
        Teuchos::RCP<IO::DiscretizationWriter> output        //!< the output
    );

    //! Destructor
    // ....

    //! Empty constructor
    TimIntExplEuler() : TimIntExpl() { ; }

    //! Copy constructor
    TimIntExplEuler(const TimIntExplEuler& old) : TimIntExpl(old) { ; }

    //! Resize #TimIntMStep<T> multi-step quantities
    void ResizeMStep() override { FOUR_C_THROW("not a multistep method"); }

    //@}

    //! @name Actions
    //@{

    //! Do time integration of single step
    void IntegrateStep() override;

    //! Update configuration after time step
    //!
    //! Thus the 'last' converged is lost and a reset of the time step
    //! becomes impossible. We are ready and keen awating the next time step.
    void UpdateStepState() override;

    //! Update Element
    void UpdateStepElement() override;

    //@}

    //! @name Attribute access functions
    //@{

    //! Return time integrator name
    enum INPAR::THR::DynamicType MethodName() const override { return INPAR::THR::dyna_expleuler; }

    //! Provide number of steps, e.g. a single-step method returns 1,
    //! a m-multistep method returns m
    int MethodSteps() override { return 1; }

    //! Give local order of accuracy of temperature part
    int MethodOrderOfAccuracy() override { return 1; }

    //! Return linear error coefficient of temperatures
    double MethodLinErrCoeff() override
    {
      FOUR_C_THROW("no time adaptivity possible.");
      return 0.0;
    }

    //@}

    //! @name System vectors
    //@{

    //! Return external force \f$F_{ext,n}\f$
    Teuchos::RCP<Epetra_Vector> Fext() override { return fextn_; }

    //! Return external force \f$F_{ext,n+1}\f$
    Teuchos::RCP<Epetra_Vector> FextNew()
    {
      FOUR_C_THROW("FextNew() not available in ExplEuler");
      return Teuchos::null;
    }

    //! Read and set restart for forces
    void ReadRestartForce() override;

    //! Write internal and external forces for restart
    void WriteRestartForce(Teuchos::RCP<IO::DiscretizationWriter> output) override;

    //@}

   protected:
    //! @name Global forces at \f$t_{n+1}\f$
    //@{
    Teuchos::RCP<Epetra_Vector> fextn_;  //!< external force
                                         //!< \f$F_{int;n+1}\f$
    Teuchos::RCP<Epetra_Vector> fintn_;  //!< internal force
                                         //!< \f$F_{int;n+1}\f$
    //@}

  };  // class TimIntExplEuler

}  // namespace THR

/*----------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif
