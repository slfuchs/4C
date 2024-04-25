/*----------------------------------------------------------------------*/
/*! \file
\brief scatra time integration for loma
\level 2
 *------------------------------------------------------------------------------------------------*/

#ifndef FOUR_C_SCATRA_TIMINT_LOMA_HPP
#define FOUR_C_SCATRA_TIMINT_LOMA_HPP

#include "4C_config.hpp"

#include "4C_scatra_timint_implicit.hpp"

#include <Epetra_MpiComm.h>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN


namespace SCATRA
{
  class ScaTraTimIntLoma : public virtual ScaTraTimIntImpl
  {
   public:
    /// Standard Constructor
    ScaTraTimIntLoma(Teuchos::RCP<DRT::Discretization> dis,
        Teuchos::RCP<CORE::LINALG::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
        Teuchos::RCP<Teuchos::ParameterList> sctratimintparams,
        Teuchos::RCP<Teuchos::ParameterList> extraparams,
        Teuchos::RCP<IO::DiscretizationWriter> output);

    /*========================================================================*/
    //! @name Preconditioning
    /*========================================================================*/

    void SetupSplitter() override;

    // -----------------------------------------------------------------
    // general methods
    // -----------------------------------------------------------------

    /// initialize algorithm
    void Init() override;

    /// initialize algorithm
    void Setup() override;

    //! set initial thermodynamic pressure
    void SetInitialThermPressure();

    //! predict thermodynamic pressure and time derivative
    virtual void PredictThermPressure() = 0;

    //! compute initial thermodyn. pressure time derivative
    void ComputeInitialThermPressureDeriv();

    //! compute initial total mass in domain
    void ComputeInitialMass();

    //! compute thermodynamic pressure and time derivative
    virtual void ComputeThermPressure() = 0;

    //! compute thermodyn. press. from mass cons. in domain
    void ComputeThermPressureFromMassCons();

    //! compute values of thermodynamic pressure at intermediate time steps
    //! (required for generalized-alpha)
    virtual void ComputeThermPressureIntermediateValues() = 0;

    //!  compute time derivative of thermodynamic pressure after solution
    virtual void ComputeThermPressureTimeDerivative() = 0;

    //! update thermodynamic pressure and time derivative
    virtual void UpdateThermPressure() = 0;

    //! return thermo. press. at time step n
    double ThermPressN() const { return thermpressn_; }

    //! return thermo. press. at time step n+1
    double ThermPressNp() const { return thermpressnp_; }

    //! return thermo. press. at time step n+alpha_F
    virtual double ThermPressAf() = 0;

    //! return thermo. press. at time step n+alpha_M
    virtual double ThermPressAm() = 0;

    //! return time der. of thermo. press. at time step n+1
    double ThermPressDtNp() const { return thermpressdtnp_; }

    //! return time derivative of thermo. press. at time step n+alpha_F
    virtual double ThermPressDtAf() = 0;

    //! return time derivative of thermo. press. at time step n+alpha_M
    virtual double ThermPressDtAm() = 0;

   protected:
    /*!
     * @brief add parameters depending on the problem, i.e., loma, level-set, ...
     *
     * @param params parameter list
     */
    void AddProblemSpecificParametersAndVectors(Teuchos::ParameterList& params) override;

    virtual void AddThermPressToParameterList(Teuchos::ParameterList& params  //!< parameter list
        ) = 0;

    //! the parameter list for loma problems
    Teuchos::RCP<Teuchos::ParameterList> lomaparams_;

    //! initial mass in domain
    double initialmass_;

    //! thermodynamic pressure at n
    double thermpressn_;
    //! thermodynamic pressure at n+1
    double thermpressnp_;

    //! time deriv. of thermodynamic pressure at n
    double thermpressdtn_;
    //! time deriv. of thermodynamic pressure at n+1
    double thermpressdtnp_;
  };
}  // namespace SCATRA

FOUR_C_NAMESPACE_CLOSE

#endif
