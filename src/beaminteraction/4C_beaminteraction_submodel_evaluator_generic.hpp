/*-----------------------------------------------------------*/
/*! \file

\brief Generic class for all beaminteraction submodel evaluators.


\level 3

*/
/*-----------------------------------------------------------*/


#ifndef FOUR_C_BEAMINTERACTION_SUBMODEL_EVALUATOR_GENERIC_HPP
#define FOUR_C_BEAMINTERACTION_SUBMODEL_EVALUATOR_GENERIC_HPP

#include "4C_config.hpp"

#include "4C_beaminteraction_str_model_evaluator.hpp"
#include "4C_inpar_beaminteraction.hpp"

namespace NOX
{
  namespace Solver
  {
    class Generic;
  }
}  // namespace NOX

FOUR_C_NAMESPACE_OPEN

// forward declaration
namespace Core::Binstrategy
{
  class BinningStrategy;
}
namespace Core::IO
{
  class DiscretizationWriter;
  class DiscretizationReader;
}  // namespace Core::IO
namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE
namespace Core::Geo
{
  namespace MeshFree
  {
    class BoundingBox;
  }
}  // namespace Core::Geo
namespace Solid
{
  namespace TimeInt
  {
    class BaseDataGlobalState;
    class BaseDataIO;
  }  // namespace TimeInt
  namespace MODELEVALUATOR
  {
    class BeamInteractionDataState;
  }
}  // namespace Solid
namespace BEAMINTERACTION
{
  class BeamCrosslinkerHandler;

  namespace UTILS
  {
    class MapExtractor;
  }
  namespace SUBMODELEVALUATOR
  {
    class Crosslinking;

    /*! \brief This is the abstract base class of all submodel evaluators for a beaminteraction
     * problem
     *
     *  This class summarizes the functionality which all submodel evaluators share
     *  and/or have to implement. Look in the derived classes for examples. A minimal
     *  example can be found at \ref BEAMINTERACTION::SUBMODELEVALUATOR::Crosslinking.
     */
    class Generic
    {
     public:
      //! constructor
      Generic();

      //! destructor
      virtual ~Generic() = default;

      //! initialize the class variables
      virtual void init(Teuchos::RCP<Core::FE::Discretization> const& ia_discret,
          Teuchos::RCP<Core::FE::Discretization> const& bindis,
          Teuchos::RCP<Solid::TimeInt::BaseDataGlobalState> const& gstate,
          Teuchos::RCP<Solid::TimeInt::BaseDataIO> const& gio_ptr,
          Teuchos::RCP<Solid::MODELEVALUATOR::BeamInteractionDataState> const& ia_gstate_ptr,
          Teuchos::RCP<BEAMINTERACTION::BeamCrosslinkerHandler> const& beamcrosslinkerhandler,
          Teuchos::RCP<Core::Binstrategy::BinningStrategy> binstrategy,
          Teuchos::RCP<Core::Geo::MeshFree::BoundingBox> const& periodic_boundingbox,
          Teuchos::RCP<BEAMINTERACTION::UTILS::MapExtractor> const& eletypeextractor);

      //! setup class variables
      virtual void setup() = 0;

     protected:
      //! Returns true, if init() has been called
      inline const bool& is_init() const { return isinit_; };

      //! Returns true, if setup() has been called
      inline const bool& is_setup() const { return issetup_; };

      //! Checks, if init() and setup() have been called
      virtual void check_init_setup() const;

      virtual void check_init() const;

     public:
      //! Returns the type of the current model evaluator
      virtual Inpar::BEAMINTERACTION::SubModelType type() const = 0;

      //! \brief reset model specific variables (without jacobian)
      virtual void reset() = 0;

      //! \brief Evaluate the current right-hand-side at \f$t_{n+1}\f$
      virtual bool evaluate_force() = 0;

      //! \brief Evaluate the current tangential stiffness matrix at \f$t_{n+1}\f$
      virtual bool evaluate_stiff() = 0;

      //! \brief Evaluate the current right-hand-side vector and tangential stiffness matrix at
      //! \f$t_{n+1}\f$
      virtual bool evaluate_force_stiff() = 0;

      //! update state
      virtual void update_step_state(const double& timefac_n) = 0;

      //! pre update step element
      virtual bool pre_update_step_element(bool beam_redist) = 0;

      //! update step element
      virtual void update_step_element(bool repartition_was_done) = 0;

      //! post update step element
      virtual void post_update_step_element() = 0;

      //! get contributions to system energy
      virtual std::map<Solid::EnergyType, double> get_energy() const = 0;

      //! write submodel specific output
      virtual void output_step_state(Core::IO::DiscretizationWriter& iowriter) const = 0;

      //! write submodel specific output during runtime
      virtual void runtime_output_step_state() const = 0;

      //! reset routine for model evlaluator
      virtual void reset_step_state() = 0;

      //! \brief write model specific restart
      virtual void write_restart(Core::IO::DiscretizationWriter& ia_writer,
          Core::IO::DiscretizationWriter& bin_writer) const = 0;

      /*! \brief read model specific restart information
       *
       *  \param ioreader (in) : input reader*/
      virtual void read_restart(Core::IO::DiscretizationReader& ia_writer,
          Core::IO::DiscretizationReader& bin_writer) = 0;

      //! \brief do stuff pre reading of model specific restart information
      virtual void pre_read_restart() = 0;

      //! \brief do stuff post reading of model specific restart information
      virtual void post_read_restart() = 0;

      /*! \brief Executed at the end of the ::NOX::Solver::Generic::Step() (f.k.a. Iterate()) method
       *
       *  \param solver (in) : reference to the non-linear nox solver object (read-only)
       *
       *  \author grill, hiermeier \date 10/17 */
      virtual void run_post_iterate(const ::NOX::Solver::Generic& solver) = 0;

      //! reset routine for model evlaluator
      virtual void init_submodel_dependencies(
          Teuchos::RCP<Solid::MODELEVALUATOR::BeamInteraction::Map> const submodelvector) = 0;

      //! \brief add subproblem specific contributions to bin col map
      virtual void add_bins_to_bin_col_map(std::set<int>& colbins) = 0;

      //! \brief add subproblem specific contributions to bin col map
      virtual void add_bins_with_relevant_content_for_ia_discret_col_map(
          std::set<int>& colbins) const = 0;

      //! \brief add subproblem specific contributions to bin col map
      virtual void get_half_interaction_distance(double& half_interaction_distance) = 0;

      //! \brief do submodel specific stuff after partitioning
      virtual bool post_partition_problem() { return false; };

      //! \brief do submodel specific stuff after setup
      virtual void post_setup() = 0;

      //! @name internal accessors
      //! @{
      //! Returns the (structural) discretization
      Core::FE::Discretization& discret();
      Teuchos::RCP<Core::FE::Discretization>& discret_ptr();
      Teuchos::RCP<const Core::FE::Discretization> discret_ptr() const;
      Core::FE::Discretization const& discret() const;

      Core::FE::Discretization& bin_discret();
      Teuchos::RCP<Core::FE::Discretization>& bin_discret_ptr();
      Teuchos::RCP<const Core::FE::Discretization> bin_discret_ptr() const;
      Core::FE::Discretization const& bin_discret() const;

      //! Returns the global state data container
      Solid::TimeInt::BaseDataGlobalState& g_state();
      Teuchos::RCP<Solid::TimeInt::BaseDataGlobalState>& g_state_ptr();
      Solid::TimeInt::BaseDataGlobalState const& g_state() const;

      //! Returns the global input/output data container
      Solid::TimeInt::BaseDataIO& g_in_output();
      Solid::TimeInt::BaseDataIO const& g_in_output() const;

      //! Returns the global state data container
      Solid::MODELEVALUATOR::BeamInteractionDataState& beam_interaction_data_state();
      Teuchos::RCP<Solid::MODELEVALUATOR::BeamInteractionDataState>&
      beam_interaction_data_state_ptr();
      Solid::MODELEVALUATOR::BeamInteractionDataState const& beam_interaction_data_state() const;

      BEAMINTERACTION::BeamCrosslinkerHandler& beam_crosslinker_handler();
      Teuchos::RCP<BEAMINTERACTION::BeamCrosslinkerHandler>& beam_crosslinker_handler_ptr();
      BEAMINTERACTION::BeamCrosslinkerHandler const& beam_crosslinker_handler() const;

      Core::Binstrategy::BinningStrategy& bin_strategy();
      Teuchos::RCP<Core::Binstrategy::BinningStrategy>& bin_strategy_ptr();
      Core::Binstrategy::BinningStrategy const& bin_strategy() const;

      Core::Geo::MeshFree::BoundingBox& periodic_bounding_box();
      Teuchos::RCP<Core::Geo::MeshFree::BoundingBox>& periodic_bounding_box_ptr();
      Core::Geo::MeshFree::BoundingBox const& periodic_bounding_box() const;

      BEAMINTERACTION::UTILS::MapExtractor& ele_type_map_extractor();
      Teuchos::RCP<BEAMINTERACTION::UTILS::MapExtractor>& ele_type_map_extractor_ptr();
      BEAMINTERACTION::UTILS::MapExtractor const& ele_type_map_extractor() const;

      //! @}
     protected:
      //! init flag
      bool isinit_;

      //! setup flag
      bool issetup_;

     private:
      //! pointer to the interaction discretization
      Teuchos::RCP<Core::FE::Discretization> discret_ptr_;

      //! pointer to the interaction discretization
      Teuchos::RCP<Core::FE::Discretization> bindis_ptr_;

      //! pointer to the global state data container
      Teuchos::RCP<Solid::TimeInt::BaseDataGlobalState> gstate_ptr_;

      //! pointer to input/ouput data container
      Teuchos::RCP<Solid::TimeInt::BaseDataIO> gio_ptr_;

      //! pointer to the global state data container
      Teuchos::RCP<Solid::MODELEVALUATOR::BeamInteractionDataState> beaminteractiondatastate_;

      //! beam crosslinker handler
      Teuchos::RCP<BEAMINTERACTION::BeamCrosslinkerHandler> beam_crosslinker_handler_;

      //! binning strategy
      Teuchos::RCP<Core::Binstrategy::BinningStrategy> binstrategy_;

      //! periodic bounding box
      Teuchos::RCP<Core::Geo::MeshFree::BoundingBox> periodic_boundingbox_;

      /// map extractor for split of different element types
      Teuchos::RCP<BEAMINTERACTION::UTILS::MapExtractor> eletypeextractor_;

    };  // class Generic

  }  // namespace SUBMODELEVALUATOR
}  // namespace BEAMINTERACTION

FOUR_C_NAMESPACE_CLOSE

#endif
