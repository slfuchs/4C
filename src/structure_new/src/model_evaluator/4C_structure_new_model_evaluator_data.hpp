/*-----------------------------------------------------------*/
/*! \file

\brief Concrete implementation of the structural and all related parameter interfaces.


\level 3

*/
/*-----------------------------------------------------------*/
#ifndef FOUR_C_STRUCTURE_NEW_MODEL_EVALUATOR_DATA_HPP
#define FOUR_C_STRUCTURE_NEW_MODEL_EVALUATOR_DATA_HPP

#include "4C_config.hpp"

#include "4C_contact_paramsinterface.hpp"  // base class of the ContactData class
#include "4C_inpar_browniandyn.hpp"        // enums
#include "4C_lib_discret.hpp"
#include "4C_structure_new_elements_paramsinterface.hpp"  // base class of the Data class
#include "4C_structure_new_enum_lists.hpp"
#include "4C_structure_new_gauss_point_data_output_manager.hpp"
#include "4C_structure_new_timint_basedatasdyn.hpp"  // base class of the Data class
#include "4C_utils_exceptions.hpp"
#include "4C_utils_pairedvector.hpp"

#include <Epetra_MultiVector.h>
#include <NOX_Abstract_Vector.H>

#include <unordered_map>

// forward declarations
class Epetra_Comm;

FOUR_C_NAMESPACE_OPEN

namespace CORE::GEO
{
  namespace MESHFREE
  {
    class BoundingBox;
  }
}  // namespace CORE::GEO

namespace STR
{
  namespace TIMINT
  {
    class BaseDataGlobalState;
    class MeshFreeData;
    class BaseDataIO;
    class Base;
  }  // namespace TIMINT
  namespace MODELEVALUATOR
  {
    class BeamData;
    class ContactData;
    class BrownianDynData;
    class GaussPointDataOutputManager;


    /*! \brief Discrete implementation of the STR::ELEMENTS::ParamsInterface
     *
     * This class represents an actual implementation of the STR::ELEMENTS::ParamsInterface class
     * and gives you all the functionality to interchange data between the elements and the
     * structural time integrators.
     *
     * To add a new and more specialized data container or method
     * that is not directly linked to the pure structure problem,
     * but is linked to related problem classes such as contact, beam interaction or
     * Brownian dynamics instead, please refer to these sub-containers.
     * If these sub-containers are not sufficient for your purposes,
     * other sub-containers can be added following the provided templates.
     * To do so, the following basic steps should be considered:
     * - Add a pointer pointing to your new container as a member to this class (parent class).
     * - Initialize and setup your container in the Setup routine of this parent class.
     * - Add an accessor such as Contact() or BrownianDyn().
     * - Create your own data container class and use a call-back to the parent container to avoid
     *   code redundancy.
     *
     * \author hiermeier \date 03/2016
     */
    class Data : public STR::ELEMENTS::ParamsInterface
    {
      typedef std::map<enum NOX::NLN::StatusTest::QuantityType,
          enum ::NOX::Abstract::Vector::NormType>
          quantity_norm_type_map;

     public:
      //! constructor
      Data();


      //! initialize the stuff coming from outside
      void Init(const Teuchos::RCP<const STR::TIMINT::Base>& timint_ptr);

      //! setup member variables
      void Setup();

      //! @name Derived STR::ELEMENTS::ParamsInterface accessors
      //!@{

      //! get the desired action type [derived]
      [[nodiscard]] inline enum DRT::ELEMENTS::ActionType GetActionType() const override
      {
        CheckInitSetup();
        return ele_action_;
      }

      //! get the total time for the element evaluation [derived]
      [[nodiscard]] inline double GetTotalTime() const override
      {
        CheckInitSetup();
        return total_time_;
      }

      //! get the current time step for the element evaluation [derived]
      [[nodiscard]] inline double GetDeltaTime() const override
      {
        CheckInitSetup();
        return delta_time_;
      }

      //! get the current step length [derived]
      [[nodiscard]] inline double GetStepLength() const override
      {
        CheckInitSetup();
        return step_length_;
      }

      //! get the is_default_step indicator [derived]
      [[nodiscard]] inline bool IsDefaultStep() const override
      {
        CheckInitSetup();
        return is_default_step_;
      }

      //! get the current damping type [derived]
      [[nodiscard]] enum INPAR::STR::DampKind GetDampingType() const override;

      //! get the tolerate errors indicator [derived]
      [[nodiscard]] inline bool IsTolerateErrors() const override
      {
        CheckInitSetup();
        return is_tolerate_errors_;
      }

      //! get the structural time integration factor for the displacement [derived]
      [[nodiscard]] inline double GetTimIntFactorDisp() const override
      {
        CheckInitSetup();
        return timintfactor_disp_;
      }

      //! get the structural time integration factor for the velocities [derived]
      [[nodiscard]] inline double GetTimIntFactorVel() const override
      {
        CheckInitSetup();
        return timintfactor_vel_;
      }

      //! get the predictor type of the structural time integration
      [[nodiscard]] enum INPAR::STR::PredEnum GetPredictorType() const override
      {
        CheckInitSetup();
        return predict_type_;
      }

      //! is the current state the predictor state?
      bool IsPredictorState() const;

      //! mutable access to the stress data vector
      Teuchos::RCP<std::vector<char>>& StressDataPtr() override;

      //! mutable access to the strain data vector
      Teuchos::RCP<std::vector<char>>& StrainDataPtr() override;

      //! mutable access to the plastic strain data vector
      Teuchos::RCP<std::vector<char>>& PlasticStrainDataPtr() override;

      //! mutable access to the stress data vector
      Teuchos::RCP<std::vector<char>>& CouplingStressDataPtr() override;

      //! mutable access to the optional quantity data vector
      Teuchos::RCP<std::vector<char>>& OptQuantityDataPtr() override;

      //! get the current stress type [derived]
      [[nodiscard]] enum INPAR::STR::StressType GetStressOutputType() const override;

      //! get the current strain type [derived]
      [[nodiscard]] enum INPAR::STR::StrainType GetStrainOutputType() const override;

      //! get the current plastic strain type [derived]
      [[nodiscard]] enum INPAR::STR::StrainType GetPlasticStrainOutputType() const override;

      //! get the current coupling stress type [derived]
      [[nodiscard]] enum INPAR::STR::StressType GetCouplingStressOutputType() const override;

      //! get the current strain type [derived]
      virtual enum INPAR::STR::OptQuantityType GetOptQuantityOutputType() const;

      //< get the manager of Gauss point data output
      Teuchos::RCP<GaussPointDataOutputManager>& GaussPointDataOutputManagerPtr() override;

      //! register energy type to be computed and written to file
      void InsertEnergyTypeToBeConsidered(enum STR::EnergyType type);

      //! read-only access to energy data
      std::map<enum STR::EnergyType, double> const& GetEnergyData() const;

      //! read-only access to energy data
      double GetEnergyData(enum STR::EnergyType type) const;

      //! read-only access to energy data
      double GetEnergyData(const std::string type) const;

      //! set value for a specific energy type
      void SetValueForEnergyType(double value, enum STR::EnergyType type);

      //! clear values for all energy types
      void ClearValuesForAllEnergyTypes();

      /*! \brief Add contribution to energy of specified type [derived]
       *
       * @param value Value to be added
       * @param type Type of energy to be added to
       */
      void AddContributionToEnergyType(
          const double value, const enum STR::EnergyType type) override;

      //! get Interface to brownian dyn data [derived]
      [[nodiscard]] inline Teuchos::RCP<BROWNIANDYN::ParamsInterface> GetBrownianDynParamInterface()
          const override
      {
        CheckInitSetup();
        return browniandyn_data_ptr_;
      }

      //! get special parameter interface for beam elements [derived]
      [[nodiscard]] inline Teuchos::RCP<STR::ELEMENTS::BeamParamsInterface>
      GetBeamParamsInterfacePtr() const override
      {
        FOUR_C_ASSERT(!beam_data_ptr_.is_null(), "pointer to beam data container not set!");
        return beam_data_ptr_;
      }

      /** \brief get reference to the set model evaluator
       *
       *  \note Currently only used in the contact data container and therefore
       *  not part of the ParamsInterface. Feel free to add. */
      const Generic& GetModelEvaluator() const
      {
        FOUR_C_ASSERT(model_ptr_, "No reference to the model evaluator available!");

        return *model_ptr_;
      }

      /// get the current non-linear solver correction type
      NOX::NLN::CorrectionType GetCorrectionType() const
      {
        CheckInitSetup();
        return corr_type_;
      }

      /// get number of system modifications in case of a mod. Newton direction method
      int GetNumberOfModifiedNewtonCorrections() const
      {
        CheckInitSetup();
        return num_corr_mod_newton_;
      }

      //!@name set routines which can be called inside of the element [derived]
      //! @{

      /*! \brief Set the element evaluation error flag inside the element
       *
       * @param[in] error_flag Error flag to be set
       */
      inline void SetEleEvalErrorFlag(const enum STR::ELEMENTS::EvalErrorFlag& error_flag) override
      {
        ele_eval_error_flag_ = error_flag;
      }

      /*! \brief Collects and calculates the update norm of the current processor
       *
       * These methods are used to calculate the norms for a status test
       * w.r.t. internally, elementwise stored quantities.
       *
       * This is supported by the EAS formulation of the HEX8 element.
       * This specific method is used for the NormUpdate status test that tests the increments
       * of the solution variables.
       *
       * @param[in] qtype Quantity type which is tested
       * @param[in] numentries Length/size of the value arrays
       * @param[in] my_update_values Local part of increment/direction vector (with default step
       *                             length)
       * @param[in] my_new_sol_values Local part of the already updated solution vector
       * @param[in] step_length Step length of a possible active globalization strategy
       * @param[in] owner Owner of the corresponding element (used to avoid summing up ghost
       *                  entries)
       *
       * \sa SumIntoMyPreviousSolNorm
       */
      void SumIntoMyUpdateNorm(const enum NOX::NLN::StatusTest::QuantityType& qtype,
          const int& numentries, const double* my_update_values, const double* my_new_sol_values,
          const double& step_length, const int& owner) override;

      /*! brief Collect and calculate solution norm of previous accepted Newton step on current proc
       *
       * @param[in] qtype Quantity type which is tested
       * @param[in] numentries Length/size of the value arrays
       * @param[in] my_old_sol_values Local part of the previous solution vector
       * @param[in] owner Owner of the corresponding element (used to avoid summing up ghost
       *                  entries)
       *
       * \sa SumIntoMyUpdateNorm
       */
      void SumIntoMyPreviousSolNorm(const enum NOX::NLN::StatusTest::QuantityType& qtype,
          const int& numentries, const double* my_old_sol_values, const int& owner) override;

      //!@}

      /*! \brief Returns the partial update norm of the given quantity on the current processor
       *
       * \todo Complete documentation of return parameters.
       *
       * @param[in] qtype Quantity type which is tested
       * @return
       */
      inline double GetMyUpdateNorm(const enum NOX::NLN::StatusTest::QuantityType& qtype) const
      {
        CheckInitSetup();
        std::map<enum NOX::NLN::StatusTest::QuantityType, double>::const_iterator c_it;
        c_it = my_update_norm_.find(qtype);
        // not on this proc
        if (c_it == my_update_norm_.end()) return 0.0;
        return c_it->second;
      }

      /*! \brief Return partial root-mean-squared norm of given quantity on current processor
       *
       * \todo Complete documentation of return parameters.
       *
       * @param[in] qtype Quantity type which is tested
       * @return
       */
      inline double GetMyRMSNorm(const enum NOX::NLN::StatusTest::QuantityType& qtype) const
      {
        CheckInitSetup();
        std::map<enum NOX::NLN::StatusTest::QuantityType, double>::const_iterator c_it;
        c_it = my_rms_norm_.find(qtype);
        // not on this proc
        if (c_it == my_rms_norm_.end()) return 0.0;
        return c_it->second;
      }

      /*! \brief Return partial solution norm of previous accepted Newton step of given quantity on
       *  current processor
       *
       * \todo Complete documentation of return parameters.
       *
       * @param[in] qtype Quantity type which is tested
       * @return
       */
      inline double GetMyPreviousSolNorm(const enum NOX::NLN::StatusTest::QuantityType& qtype) const
      {
        CheckInitSetup();
        std::map<enum NOX::NLN::StatusTest::QuantityType, double>::const_iterator c_it;
        c_it = my_prev_sol_norm_.find(qtype);
        // not on this proc
        if (c_it == my_prev_sol_norm_.end()) return 0.0;
        return c_it->second;
      }

      /*! brief Returns the update norm type
       *
       * \todo Complete documentation of return parameters.
       *
       * @param[in] qtype Quantity type which is tested
       * @return
       */
      inline enum ::NOX::Abstract::Vector::NormType GetUpdateNormType(
          const enum NOX::NLN::StatusTest::QuantityType& qtype) const
      {
        CheckInitSetup();
        // collect the norm types only once
        static bool iscollected = false;
        if (not iscollected)
        {
          CollectNormTypesOverAllProcs(normtype_update_);
          iscollected = true;
        }

        std::map<enum NOX::NLN::StatusTest::QuantityType,
            enum ::NOX::Abstract::Vector::NormType>::const_iterator c_it;
        c_it = normtype_update_.find(qtype);
        if (c_it == normtype_update_.end())
          FOUR_C_THROW("The corresponding norm type could not be found! (quantity: %s)",
              NOX::NLN::StatusTest::QuantityType2String(qtype).c_str());
        return c_it->second;
      }

      /*! \brief Returns the dof number
       *
       * \todo Complete documentation of return parameters.
       *
       * @param[in] qtype Quantity type which is tested
       * @return
       */
      inline int GetMyDofNumber(const enum NOX::NLN::StatusTest::QuantityType& qtype) const
      {
        CheckInitSetup();
        std::map<enum NOX::NLN::StatusTest::QuantityType, std::size_t>::const_iterator c_it;
        c_it = my_dof_number_.find(qtype);
        // not on this proc
        if (c_it == my_dof_number_.end()) return 0;
        return static_cast<int>(c_it->second);
      }

      /*! \brief Did an element evaluation error occur?
       *
       * @return Boolean flag to indicate occurrence error during element evaluation
       */
      bool IsEleEvalError() const;

      /*! brief Access the element evaluation error flag
       *
       * @return Flag describing errors during element evaluation
       */
      inline STR::ELEMENTS::EvalErrorFlag GetEleEvalErrorFlag() const override
      {
        return ele_eval_error_flag_;
      }

      /*! @name Set routines which are used to set the parameters of the data container
       *
       *  \warning These functions are not allowed to be called by the elements!
       */
      //!@{

      /*! \brief Set the action type
       *
       * @param[in] actiontype Action type
       */
      inline void SetActionType(const enum DRT::ELEMENTS::ActionType& actiontype)
      {
        ele_action_ = actiontype;
      }

      /*! \brief Set the tolerate errors flag
       *
       * @param[in] is_tolerate_errors Boolean flag to indicate error tolerance
       */
      inline void SetIsTolerateError(const bool& is_tolerate_errors)
      {
        is_tolerate_errors_ = is_tolerate_errors;
      }

      /*! \brief Set the current step length
       *
       * @param[in] step_length Value for current step length to be set
       */
      inline void SetStepLength(const double& step_length) { step_length_ = step_length; }

      //! set the default step flag
      inline void SetIsDefaultStep(const bool& is_default_step)
      {
        is_default_step_ = is_default_step;
      }

      /// set the number of system corrections in case of a mod. Newton direction method
      inline void SetNumberOfModifiedNewtonCorrections(const int num_corr)
      {
        num_corr_mod_newton_ = num_corr;
      }

      /// set the current system correction type of the non-linear solver
      inline void SetCorrectionType(const NOX::NLN::CorrectionType corr_type)
      {
        corr_type_ = corr_type;
      }

      //! set the total time for the evaluation call
      inline void SetTotalTime(const double& total_time) { total_time_ = total_time; }

      //! set the current time step for the evaluation call
      inline void SetDeltaTime(const double& dt) { delta_time_ = dt; }

      //! set the time integration factor for the displacements
      inline void SetTimIntFactorDisp(const double& timintfactor_disp)
      {
        timintfactor_disp_ = timintfactor_disp;
      }

      //! set the time integration factor for the velocities
      inline void SetTimIntFactorVel(const double& timintfactor_vel)
      {
        timintfactor_vel_ = timintfactor_vel;
      }

      //! set the predictor type of the structural time integration
      inline void SetPredictorType(const INPAR::STR::PredEnum& predictor_type)
      {
        predict_type_ = predictor_type;
      }

      //! set stress data vector
      inline void SetStressData(const Teuchos::RCP<std::vector<char>>& stressdata)
      {
        stressdata_ptr_ = stressdata;
      }

      /*!
       * \brief Set the pointer to the manager of gauss point data output
       *
       * \param data_manager Manager of gauss point data output
       */
      inline void SetGaussPointDataOutputManagerPtr(
          const Teuchos::RCP<GaussPointDataOutputManager> data_manager)
      {
        gauss_point_data_manager_ptr_ = data_manager;
      }

      /// Return constant manager of gauss point data output
      inline const Teuchos::RCP<GaussPointDataOutputManager>& GetGaussPointDataOutputManagerPtr()
          const
      {
        CheckInitSetup();
        return gauss_point_data_manager_ptr_;
      }

      //! get stress data vector
      inline const Teuchos::RCP<std::vector<char>>& GetStressData() const
      {
        return stressdata_ptr_;
      }

      //! get nodal postprocessed stress data vector
      inline const Teuchos::RCP<Epetra_MultiVector>& GetStressDataNodePostprocessed() const
      {
        return stressdata_postprocessed_nodal_ptr_;
      }

      //! get nodal postprocessed stress data vector
      inline Teuchos::RCP<Epetra_MultiVector>& GetStressDataNodePostprocessed()
      {
        return stressdata_postprocessed_nodal_ptr_;
      }

      //! get element postprocessed stress data vector
      inline const Teuchos::RCP<Epetra_MultiVector>& GetStressDataElementPostprocessed() const
      {
        return stressdata_postprocessed_element_ptr_;
      }

      //! get element postprocessed stress data vector
      inline Teuchos::RCP<Epetra_MultiVector>& GetStressDataElementPostprocessed()
      {
        return stressdata_postprocessed_element_ptr_;
      }

      //! set element volume data vector
      inline void SetElementVolumeData(const Teuchos::RCP<Epetra_Vector>& ele_volumes)
      {
        elevolumes_ptr_ = ele_volumes;
      }

      //! set stress data vector
      inline void SetCouplingStressData(const Teuchos::RCP<std::vector<char>>& couplstressdata)
      {
        couplstressdata_ptr_ = couplstressdata;
      }

      //! set strain data vector
      inline void SetStrainData(const Teuchos::RCP<std::vector<char>>& straindata)
      {
        straindata_ptr_ = straindata;
      }

      //! get strain data vector
      inline const Teuchos::RCP<std::vector<char>>& GetStrainData() const
      {
        return straindata_ptr_;
      }

      //! get nodal postprocessed strain data vector
      inline const Teuchos::RCP<Epetra_MultiVector>& GetStrainDataNodePostprocessed() const
      {
        return straindata_postprocessed_nodal_ptr_;
      }

      //! get nodal postprocessed strain data vector
      inline Teuchos::RCP<Epetra_MultiVector>& GetStrainDataNodePostprocessed()
      {
        return straindata_postprocessed_nodal_ptr_;
      }

      //! get element postprocessed strain data vector
      inline const Teuchos::RCP<Epetra_MultiVector>& GetStrainDataElementPostprocessed() const
      {
        return straindata_postprocessed_element_ptr_;
      }

      //! get element postprocessed strain data vector
      inline Teuchos::RCP<Epetra_MultiVector>& GetStrainDataElementPostprocessed()
      {
        return straindata_postprocessed_element_ptr_;
      }

      //! set plastic strain data vector
      inline void SetPlasticStrainData(const Teuchos::RCP<std::vector<char>>& plastic_straindata)
      {
        plastic_straindata_ptr_ = plastic_straindata;
      }

      //! set optional quantity data vector
      inline void SetOptQuantityData(const Teuchos::RCP<std::vector<char>>& optquantitydata)
      {
        optquantitydata_ptr_ = optquantitydata;
      }

      //! set model evaluator ptr
      inline void SetModelEvaluator(Generic* model_ptr) { model_ptr_ = model_ptr; }

      //! reset the partial update norm value of the current processor
      void ResetMyNorms(const bool& isdefaultstep);

      //! return element volume data vector (read-only)
      const Epetra_Vector& CurrentElementVolumeData() const;

      //! return the stress data (read-only)
      const std::vector<char>& StressData() const;

      //! return the strain data (read-only)
      const std::vector<char>& StrainData() const;

      //! return the plastic strain data (read-only)
      const std::vector<char>& PlasticStrainData() const;

      //! return the coupling stress data (read-only)
      const std::vector<char>& CouplingStressData() const;

      //! return the optional quantity data (read-only)
      const std::vector<char>& OptQuantityData() const;

      //!@}

      /*! @name Accessors to the remaining data containers
       *
       * \warning You are not allowed to call these functions on element level.
       */
      //!@{

      //! access the beam data container, if applicable
      inline BeamData& GetBeamData()
      {
        FOUR_C_ASSERT(!beam_data_ptr_.is_null(), "pointer to beam data container not set!");
        return *beam_data_ptr_;
      }
      inline const Teuchos::RCP<BeamData>& GetBeamDataPtr()
      {
        FOUR_C_ASSERT(!beam_data_ptr_.is_null(), "pointer to beam data container not set!");
        return beam_data_ptr_;
      }

      //! access the contact data container, if the contact model is active
      inline ContactData& Contact()
      {
        FOUR_C_ASSERT(!contact_data_ptr_.is_null(), "The contact model is not active!");
        return *contact_data_ptr_;
      }
      inline const Teuchos::RCP<ContactData>& ContactPtr() const
      {
        FOUR_C_ASSERT(!contact_data_ptr_.is_null(), "The contact model is not active!");
        return contact_data_ptr_;
      }

      //! access the brownian dynamic data container
      inline BrownianDynData& BrownianDyn()
      {
        FOUR_C_ASSERT(
            !browniandyn_data_ptr_.is_null(), "The brownian dynamic model is not active!");
        return *browniandyn_data_ptr_;
      }
      inline const Teuchos::RCP<BrownianDynData>& BrownianDynPtr()
      {
        FOUR_C_ASSERT(
            !browniandyn_data_ptr_.is_null(), "The brownian dynamic model is not active!");
        return browniandyn_data_ptr_;
      }
      //!@}

      /*! @name Accessors to some important member variables
       *  (necessary for possible other model evaluator containers, etc.) */
      //!@{

      //! Time integration strategy
      inline const STR::TIMINT::Base& TimInt() const
      {
        CheckInit();
        return *timint_ptr_;
      }

      //! Structural dynamic data
      inline const STR::TIMINT::BaseDataSDyn& SDyn() const
      {
        CheckInit();
        return *sdyn_ptr_;
      }

      //! input/ouput parameters
      inline const STR::TIMINT::BaseDataIO& InOutput() const
      {
        CheckInit();
        return *io_ptr_;
      }

      //! global state variables
      inline const STR::TIMINT::BaseDataGlobalState& GState() const
      {
        CheckInit();
        return *gstate_ptr_;
      }

      //! get the nonlinear iteration number
      int GetNlnIter() const;

      //! get the current step counter \f$(n+1)\f$
      int GetStepNp() const;

      //! get the predictor indicator
      bool IsPredictor() const;

      /*! Get the step number from which the current simulation has been
       *  restarted. Equal to 0 if no restart has been performed. */
      int GetRestartStep() const;

      //!@}

     protected:
      //! returns the isinit_ flag
      inline const bool& IsInit() const { return isinit_; };

      //! returns the issetup_ flag
      inline const bool& IsSetup() const { return issetup_; };

      //! Checks the init and setup status
      inline void CheckInitSetup() const
      {
        FOUR_C_ASSERT(IsInit() and IsSetup(), "Call Init() and Setup() first!");
      }

      //! Checks the init status
      inline void CheckInit() const { FOUR_C_ASSERT(IsInit(), "Init() has not been called, yet!"); }

     private:
      //! fill the normtype maps
      void FillNormTypeMaps();

      /*! \brief Get the norm type of the desired quantity.
       *
       *  If the norm type can be found, the function returns true,
       *  otherwise false. */
      bool GetUpdateNormType(const enum NOX::NLN::StatusTest::QuantityType& qtype,
          enum ::NOX::Abstract::Vector::NormType& normtype);

      /*! \brief Get the WRMS absolute and relative tolerances of the desired quantity.
       *
       *  If the tolerances can be found, the function returns true,
       *  otherwise false. */
      bool GetWRMSTolerances(
          const enum NOX::NLN::StatusTest::QuantityType& qtype, double& atol, double& rtol);

      /*! \brief Sum locally values into a norm of the desired type
       *
       * \todo Complete documentation of input parameters.
       *
       * @param[in] numentries
       * @param[in] my_values
       * @param[in] normtype
       * @param[in] step_length
       * @param[in/out] my_norm
       */
      void SumIntoMyNorm(const int& numentries, const double* my_values,
          const enum ::NOX::Abstract::Vector::NormType& normtype, const double& step_length,
          double& my_norm) const;

      /*! \brief Calculate a local relative mean square sum for the global WRMS status test
       *
       * \todo Complete documentation of input parameters.
       *
       * (1) \f$ v_i = x_{i}^{k-1} = x_{i}^{k} - sl* \Delta x_{i}^{k} \f$
       * (2) \f$ my_rms_norm = \sum_{i} [(x_i^{k}-x_{i}^{k-1}) / (RTOL * |x_{i}^{k-1}| + ATOL)]^{2}
       * \f$
       *
       * \param[in] atol Absolute tolerance
       * \param[in] rtol Relative tolerance
       * \param[in] step_length Step length of a possible active globalization strategy
       * \param[in] numentries Length/size of the value arrays
       * \param[in] my_update_values Local part of increment/direction vector (with default step
       *                             length)
       * \param[in] my_new_sol_values Local part of the already updated solution vector
       * \param[in/out] my_rms Root mean squared norm (to be summed into)
       */
      void SumIntoMyRelativeMeanSquare(const double& atol, const double& rtol,
          const double& step_length, const int& numentries, const double* my_update_values,
          const double* my_new_sol_values, double& my_rms) const;

      void CollectNormTypesOverAllProcs(const quantity_norm_type_map& normtypes) const;

     private:
      //! indicator if the Init() routine has been called, yet.
      bool isinit_;

      //! indicator if the Setup() routine has been called, yet.
      bool issetup_;

      /*! \brief Indicator for the norm type maps
       *
       * If true, the norm type maps have already been initialized successfully.
       */
      bool isntmaps_filled_;

      //! @name General element control parameters
      //!@{

      //! Current action type
      enum DRT::ELEMENTS::ActionType ele_action_;

      //! Current predictor type
      enum INPAR::STR::PredEnum predict_type_;

      //! element evaluation error flag
      enum STR::ELEMENTS::EvalErrorFlag ele_eval_error_flag_;

      //! tolerate errors flag
      bool is_tolerate_errors_;

      //! total time for the evaluation
      double total_time_;

      //! current time step for the evaluation
      double delta_time_;
      //!@}

      //! @name Control parameters for the handling of element internal variables (e.g. EAS)
      //!@{

      //! Current step length of the nonlinear solver
      double step_length_;

      /*! \brief Indicator if the current step is a default step
       *
       *  Only important for the internal elementwise update. */
      bool is_default_step_;

      /// number of system corrections (modified Newton direction method)
      int num_corr_mod_newton_;

      /// system correction type (e.g. in case of a SOC step, see the
      /// NOX::NLN::INNER::StatusTest::Filter method)
      NOX::NLN::CorrectionType corr_type_;

      //!@}

      //! @name time integration parameters
      //!@{

      //! time integration factor for the displacements
      double timintfactor_disp_;

      //! time integration factor for the velocities
      double timintfactor_vel_;

      //!@}

      //! @name references to output data container
      //!@{

      //! element volume data vector
      Teuchos::RCP<Epetra_Vector> elevolumes_ptr_;

      //! stress data vector
      Teuchos::RCP<std::vector<char>> stressdata_ptr_;

      //! postprocessed nodal stress data vector
      Teuchos::RCP<Epetra_MultiVector> stressdata_postprocessed_nodal_ptr_;

      //! postprocessed element stress data vector
      Teuchos::RCP<Epetra_MultiVector> stressdata_postprocessed_element_ptr_;

      //! strain data vector
      Teuchos::RCP<std::vector<char>> straindata_ptr_;

      //! postprocessed nodal strain data vector
      Teuchos::RCP<Epetra_MultiVector> straindata_postprocessed_nodal_ptr_;

      //! postprocessed element strain data vector
      Teuchos::RCP<Epetra_MultiVector> straindata_postprocessed_element_ptr_;

      //! strain data vector
      Teuchos::RCP<std::vector<char>> plastic_straindata_ptr_;

      //! coupling stress data vector
      //! e.g. in TSI: couplstress corresponds to thermal stresses
      Teuchos::RCP<std::vector<char>> couplstressdata_ptr_;

      //! optional quantity data vector
      Teuchos::RCP<std::vector<char>> optquantitydata_ptr_;

      //! system energy, stored separately by type
      std::map<enum STR::EnergyType, double> energy_data_;

      //! Manager of gauss point data output
      Teuchos::RCP<GaussPointDataOutputManager> gauss_point_data_manager_ptr_;

      //!@}

      //! map holding the force/rhs norm type of the active quantities
      quantity_norm_type_map normtype_force_;

      //! map holding the update norm type of the active quantities
      quantity_norm_type_map normtype_update_;

      //! map holding the dof number of the the active quantities on the current processor
      std::map<enum NOX::NLN::StatusTest::QuantityType, std::size_t> my_dof_number_;

      /*! map holding the absolute tolerance for the wrms status test of the active
       *  quantities on the current processor */
      std::map<enum NOX::NLN::StatusTest::QuantityType, double> atol_wrms_;

      /*! map holding the relative tolerance for the wrms status test of the active
       *  quantities on the current processor */
      std::map<enum NOX::NLN::StatusTest::QuantityType, double> rtol_wrms_;

      //! partial update norm of the current processor
      std::map<enum NOX::NLN::StatusTest::QuantityType, double> my_update_norm_;

      //! partial relative mean square norm of the current processor
      std::map<enum NOX::NLN::StatusTest::QuantityType, double> my_rms_norm_;

      //! global partial solution norm of the previous step
      std::map<enum NOX::NLN::StatusTest::QuantityType, double> my_prev_sol_norm_;

      //! read-only access to the structural dynamic parameters
      Teuchos::RCP<const STR::TIMINT::BaseDataSDyn> sdyn_ptr_;

      //! read-only access to the input/output parameters
      Teuchos::RCP<const STR::TIMINT::BaseDataIO> io_ptr_;

      //! read-only access to the global state data container
      Teuchos::RCP<const STR::TIMINT::BaseDataGlobalState> gstate_ptr_;

      //! read-only access to the timint object
      Teuchos::RCP<const STR::TIMINT::Base> timint_ptr_;

      //! read-only access to the epetra communicator
      Teuchos::RCP<const Epetra_Comm> comm_ptr_;

      //! beam data container pointer
      Teuchos::RCP<BeamData> beam_data_ptr_;

      //! contact data container
      Teuchos::RCP<ContactData> contact_data_ptr_;

      //! brownian dynamic data container
      Teuchos::RCP<BrownianDynData> browniandyn_data_ptr_;

      //! pointer to a model evaluator object
      const Generic* model_ptr_;
    };  // class Data


    /*! data container holding special parameters required for the evaluation of beam elements
     *
     * \author Maximilian Grill
     * \date 08/16 */
    class BeamData : public STR::ELEMENTS::BeamParamsInterface
    {
     public:
      //! constructor
      BeamData();

      //! initialize the stuff coming from outside
      void Init();

      //! setup member variables
      void Setup();

      //! @name Derived STR::ELEMENTS::BeamParamsInterface accessors
      //!@{

      //! get the Lie group GenAlpha time integration parameters [derived]
      [[nodiscard]] inline double GetBeta() const override
      {
        CheckInitSetup();
        return beta_;
      }

      [[nodiscard]] inline double GetGamma() const override
      {
        CheckInitSetup();
        return gamma_;
      }

      [[nodiscard]] inline double GetAlphaf() const override
      {
        CheckInitSetup();
        return alphaf_;
      }

      [[nodiscard]] inline double GetAlpham() const override
      {
        CheckInitSetup();
        return alpham_;
      }

      //!@}

      /*! @name set routines which are used to set the parameters of the data container
       *
       *  These functions are not allowed to be called by the elements! */
      //!@{

      //! set the Lie group GenAlpha time integration parameters
      inline void SetBeta(const double& beta) { beta_ = beta; }
      inline void SetGamma(const double& gamma) { gamma_ = gamma; }
      inline void SetAlphaf(const double& alphaf) { alphaf_ = alphaf; }
      inline void SetAlpham(const double& alpham) { alpham_ = alpham; }

      //!@}

     protected:
      //! returns the #isinit_ flag
      inline const bool& IsInit() const { return isinit_; };

      //! returns the #issetup_ flag
      inline const bool& IsSetup() const { return issetup_; };

      //! Checks the init and setup status
      inline void CheckInitSetup() const
      {
        FOUR_C_ASSERT(IsInit() and IsSetup(), "Call Init() and Setup() first!");
      }

      //! Checks the init status
      inline void CheckInit() const { FOUR_C_ASSERT(IsInit(), "Init() has not been called, yet!"); }

     private:
      bool isinit_;

      bool issetup_;

      //! @name time integration parameters
      //!@{

      /*! \brief Lie-group Generalized-\f$\alpha\f$ parameters
       *
       * These parameters are needed for element-internal updates of angular velocities and
       * accelerations in case of non-additive rotation vector DOFs.
       *
       * See Lie-group Generalized-\f$\alpha\f$ time integration for details.
       *
       * \sa STR::IMPLICIT::GenAlphaLieGroup
       */
      double beta_;
      double gamma_;
      double alphaf_;
      double alpham_;

      //!@}

    };  // class BeamData


    /*--------------------------------------------------------------------------*/
    /*! Contact data container for the contact model evaluation procedure.
     *
     * \author Michael Hiermeier
     * \date 04/16 */
    class ContactData : public CONTACT::ParamsInterface
    {
     public:
      //! constructor
      ContactData();

      //! initialize the stuff coming from outside
      void Init(const Teuchos::RCP<const STR::MODELEVALUATOR::Data>& str_data_ptr);

      //! setup member variables
      void Setup();

      //! returns the mortar/contact action type
      [[nodiscard]] inline enum MORTAR::ActionType GetActionType() const override
      {
        CheckInitSetup();
        return mortar_action_;
      };

      //! get the nonlinear iteration number
      [[nodiscard]] int GetNlnIter() const override
      {
        CheckInit();
        return str_data_ptr_->GetNlnIter();
      };

      //! get the current step counter \f$(n+1)\f$
      [[nodiscard]] int GetStepNp() const override
      {
        CheckInit();
        return str_data_ptr_->GetStepNp();
      };

      [[nodiscard]] bool IsPredictor() const override
      {
        CheckInit();
        return str_data_ptr_->IsPredictor();
      };

      /// derived
      NOX::NLN::CorrectionType GetCorrectionType() const override
      {
        CheckInit();
        return str_data_ptr_->GetCorrectionType();
      }

      /// derived
      int GetNumberOfModifiedNewtonCorrections() const override
      {
        CheckInit();
        return str_data_ptr_->GetNumberOfModifiedNewtonCorrections();
      }

      /*! \brief Get the current active predictor type
       *
       * If no predictor is active, \c pred_vague will be returned.
       *
       * @return Type of predictor
       *
       * \author hiermeier \date 02/18 */
      [[nodiscard]] enum INPAR::STR::PredEnum GetPredictorType() const override
      {
        CheckInit();
        return str_data_ptr_->GetPredictorType();
      }

      //! get the current step length [derived]
      [[nodiscard]] inline double GetStepLength() const override
      {
        CheckInit();
        return str_data_ptr_->GetStepLength();
      };

      //! get the is_default_step indicator [derived]
      [[nodiscard]] inline bool IsDefaultStep() const override
      {
        CheckInit();
        return str_data_ptr_->IsDefaultStep();
      };

      //! is the current state the predictor state?
      inline bool IsPredictorState() const override
      {
        CheckInit();
        return str_data_ptr_->IsPredictorState();
      }

      //! get the current time step [derived]
      [[nodiscard]] inline double GetDeltaTime() const override
      {
        CheckInit();
        return str_data_ptr_->GetDeltaTime();
      }

      //! get reference to the set model evaluator
      [[nodiscard]] const Generic& GetModelEvaluator() const override
      {
        CheckInit();
        return str_data_ptr_->GetModelEvaluator();
      }

      //! get output file name
      [[nodiscard]] std::string GetOutputFilePath() const override;

      //! get variational approach enumerator
      [[nodiscard]] enum INPAR::CONTACT::VariationalApproach GetVariationalApproachType()
          const override
      {
        return var_type_;
      }

      //! set variational approach enumerator
      void SetVariationalApproachType(
          const enum INPAR::CONTACT::VariationalApproach var_type) override
      {
        var_type_ = var_type;
      }

      //! set coupling mode enumerator
      [[nodiscard]] enum INPAR::CONTACT::CouplingScheme GetCouplingScheme() const override
      {
        return coupling_scheme_;
      }

      //! set coupling mode enumerator
      void SetCouplingScheme(const enum INPAR::CONTACT::CouplingScheme scheme) override
      {
        coupling_scheme_ = scheme;
      }

      /*! \brief Get time step number from which the current simulation has been restarted
       *
       * Equal to 0 if no restart has been performed.
       */
      [[nodiscard]] int GetRestartStep() const override
      {
        CheckInit();
        return str_data_ptr_->GetRestartStep();
      }

      /*! @name set routines which are used to set the parameters of the data container
       *
       *  These functions are not allowed to be called by the elements! */
      //! @{

      //! set the action type
      inline void SetActionType(const enum MORTAR::ActionType& actiontype)
      {
        mortar_action_ = actiontype;
      }

      //! @}

     protected:
      //! returns the isinit_ flag
      inline const bool& IsInit() const { return isinit_; };

      //! returns the issetup_ flag
      inline const bool& IsSetup() const { return issetup_; };

      //! Checks the init and setup status
      inline void CheckInitSetup() const
      {
        FOUR_C_ASSERT(IsInit() and IsSetup(), "Call Init() and Setup() first!");
      }

      //! Checks the init status
      inline void CheckInit() const { FOUR_C_ASSERT(IsInit(), "Init() has not been called, yet!"); }

      //! Time integration strategy
      inline const STR::TIMINT::Base& TimInt() const
      {
        CheckInit();
        return str_data_ptr_->TimInt();
      }

      //! Structural dynamic data
      inline const STR::TIMINT::BaseDataSDyn& SDyn() const
      {
        CheckInit();
        return str_data_ptr_->SDyn();
      }

      //! input/ouput parameters
      inline const STR::TIMINT::BaseDataIO& InOutput() const
      {
        CheckInit();
        return str_data_ptr_->InOutput();
      }

      //! global state variables
      inline const STR::TIMINT::BaseDataGlobalState& GState() const
      {
        CheckInit();
        return str_data_ptr_->GState();
      }

     private:
      bool isinit_;

      bool issetup_;

      enum MORTAR::ActionType mortar_action_;

      enum INPAR::CONTACT::VariationalApproach var_type_;

      enum INPAR::CONTACT::CouplingScheme coupling_scheme_;

      Teuchos::RCP<const STR::MODELEVALUATOR::Data> str_data_ptr_;

    };  // class ContactData

    /*! Brownian dynamic data container for the model evaluation procedure.
     *
     * \author Jonas Eichinger
     * \date 06/16 */
    class BrownianDynData : public BROWNIANDYN::ParamsInterface
    {
     public:
      //! constructor
      BrownianDynData();

      //! initialize the stuff coming from outside
      void Init(Teuchos::RCP<const STR::MODELEVALUATOR::Data> const& str_data_ptr);

      //! setup member variables
      void Setup();

      //! Structural dynamic data
      inline STR::TIMINT::BaseDataSDyn const& SDyn() const
      {
        CheckInit();
        return str_data_ptr_->SDyn();
      }

      /// thermal energy
      double const& KT() const
      {
        CheckInitSetup();
        return kt_;
      };

      //! get specified time curve number of imposed Dirichlet BCs
      void ResizeRandomForceMVector(
          Teuchos::RCP<DRT::Discretization> discret_ptr, int maxrandnumelement);

      //! get mutable random force vector
      Teuchos::RCP<Epetra_MultiVector>& GetRandomForces()
      {
        CheckInitSetup();
        return randomforces_;
      };

      /// ~ 1e-3 / 2.27 according to cyron2011 eq 52 ff, viscosity of surrounding fluid
      double const& MaxRandForce() const
      {
        CheckInitSetup();
        return maxrandforce_;
      };

      /// thermal energy
      double const& TimeStepConstRandNumb() const
      {
        CheckInitSetup();
        return timeintconstrandnumb_;
      };
      //! @}

      /*! @name set routines which are allowed to be called by the elements
       */
      //! @{
      Teuchos::RCP<Epetra_MultiVector> const& GetRandomForces() const override
      {
        CheckInitSetup();
        return randomforces_;
      };

      /// ~ 1e-3 / 2.27 according to cyron2011 eq 52 ff, viscosity of surrounding fluid
      double const& GetViscosity() const override
      {
        CheckInitSetup();
        return viscosity_;
      };

      /// the way how damping coefficient values for beams are specified
      [[nodiscard]] INPAR::BROWNIANDYN::BeamDampingCoefficientSpecificationType
      HowBeamDampingCoefficientsAreSpecified() const override
      {
        CheckInitSetup();
        return beam_damping_coeff_specified_via_;
      }

      /// get prefactors for damping coefficients of beams if they are specified via input file
      [[nodiscard]] std::vector<double> const& GetBeamDampingCoefficientPrefactorsFromInputFile()
          const override
      {
        CheckInitSetup();
        return beams_damping_coefficient_prefactors_perunitlength_;
      };

      //! get vector holding periodic bounding box object
      [[nodiscard]] Teuchos::RCP<CORE::GEO::MESHFREE::BoundingBox> const& GetPeriodicBoundingBox()
          const override
      {
        CheckInitSetup();
        return str_data_ptr_->SDyn().GetPeriodicBoundingBox();
      }
      //! @}

     protected:
      //! returns the isinit_ flag
      inline const bool& IsInit() const { return isinit_; };

      //! returns the issetup_ flag
      inline const bool& IsSetup() const { return issetup_; };

      //! Checks the init and setup status
      inline void CheckInitSetup() const
      {
        FOUR_C_ASSERT(IsInit() and IsSetup(), "Call Init() and Setup() first!");
      }

      //! Checks the init status
      inline void CheckInit() const { FOUR_C_ASSERT(IsInit(), "Init() has not been called, yet!"); }

     private:
      bool isinit_;

      bool issetup_;

      Teuchos::RCP<const STR::MODELEVALUATOR::Data> str_data_ptr_;

      /// ~ 1e-3 / 2.27 according to cyron2011 eq 52 ff, viscosity of surrounding fluid
      double viscosity_;
      /// thermal energy
      double kt_;
      /// any random force beyond MAXRANDFORCE*(standdev) will be omitted and redrawn. -1.0 means no
      /// bounds.
      double maxrandforce_;
      /// within this time interval the random numbers remain constant. -1.0 means no prescribed
      /// time interval
      double timeintconstrandnumb_;

      /// the way how damping coefficient values for beams are specified
      INPAR::BROWNIANDYN::BeamDampingCoefficientSpecificationType beam_damping_coeff_specified_via_;

      /// prefactors for damping coefficients of beams if they are specified via input file
      /// (per unit length, NOT yet multiplied by viscosity)
      std::vector<double> beams_damping_coefficient_prefactors_perunitlength_;

      /// multiVector holding random forces
      Teuchos::RCP<Epetra_MultiVector> randomforces_;
    };

  }  // namespace MODELEVALUATOR
}  // namespace STR


FOUR_C_NAMESPACE_CLOSE

#endif