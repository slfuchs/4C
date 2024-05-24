/*-----------------------------------------------------------*/
/*! \file

\brief Input/output data container for the structural (time) integration


\level 3

*/
/*-----------------------------------------------------------*/


#ifndef FOUR_C_STRUCTURE_NEW_TIMINT_BASEDATAIO_HPP
#define FOUR_C_STRUCTURE_NEW_TIMINT_BASEDATAIO_HPP

#include "4C_config.hpp"

#include "4C_inpar_structure.hpp"
#include "4C_solver_nonlin_nox_abstract_prepostoperator.hpp"

#include <fstream>

namespace NOX
{
  namespace LineSearch
  {
    class Generic;
  }  // namespace LineSearch
}  // namespace NOX

namespace Teuchos
{
  class ParameterList;
}  // namespace Teuchos

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace IO
{
  class DiscretizationWriter;
  class EveryIterationWriterInterface;
  class EveryIterationWriter;
}  // namespace IO

namespace STR
{
  namespace TIMINT
  {
    class ParamsRuntimeOutput;
    class ParamsRuntimeVtpOutput;
    class ParamsMonitorDBC;

    /** \brief Input/output data container for the structural (time) integration
     *
     * This data container holds everything, which refers directly to the
     * input/output writer and the screen output.
     *
     * \author Michael Hiermeier */
    class BaseDataIO
    {
     public:
      /// constructor
      BaseDataIO();

      /// destructor
      virtual ~BaseDataIO() = default;

      /// initialize the class variables
      void Init(const Teuchos::ParameterList& IOParams, const Teuchos::ParameterList& sDynParams,
          const Teuchos::ParameterList& xParams, Teuchos::RCP<IO::DiscretizationWriter> output);

      /// setup new class variables
      void Setup();

     protected:
      /// get the init indicator status
      virtual const bool& is_init() const { return isinit_; };

      /// get the setup indicator status
      virtual const bool& is_setup() const { return issetup_; };

      /// Check if Init() and Setup() have been called, yet.
      virtual void check_init_setup() const;

     public:
      /// get the binary output writer
      Teuchos::RCP<IO::DiscretizationWriter> GetOutputPtr()
      {
        check_init_setup();
        return output_;
      };

      /// get the binary output writer
      Teuchos::RCP<const IO::DiscretizationWriter> GetOutputPtr() const
      {
        check_init_setup();
        return output_;
      }

      /// get the data container for parameters regarding output at runtime
      Teuchos::RCP<const ParamsRuntimeOutput> get_runtime_output_params() const
      {
        check_init_setup();
        return params_runtime_vtk_output_;
      };

      /// get the data container for parameters regarding output at runtime
      Teuchos::RCP<const ParamsRuntimeVtpOutput> get_runtime_vtp_output_params() const
      {
        check_init_setup();
        return params_runtime_vtp_output_;
      };

      /// get the data container for parameters regarding output at runtime
      Teuchos::RCP<const ParamsMonitorDBC> GetMonitorDBCParams() const
      {
        check_init_setup();
        return params_monitor_dbc_;
      };

      /// \brief return TRUE if the results shall be written for this load/time \c step
      bool write_results_for_this_step(const int step) const;

      [[nodiscard]] bool is_write_results_enabled() const;

      /// \brief return TRUE if runtime vtk results shall be written for this load/time \c step
      bool write_runtime_vtk_results_for_this_step(const int step) const;

      [[nodiscard]] bool is_runtime_output_enabled() const;

      /// \brief return TRUE if runtime vtp results shall be written for this load/time \c step
      bool write_runtime_vtp_results_for_this_step(const int step) const;

      /// \brief return TRUE if the restart state shall be written for this load/time step
      bool should_write_restart_for_step(int step) const;

      /// \brief return TRUE if reaction forces shall be written for this load/time step
      bool should_write_reaction_forces_for_this_step(int step) const;

      /// \brief return TRUE if stress and strain data shall be written for this load/time step
      bool should_write_stress_strain_for_this_step(int step) const;

      /// \brief return TRUE if energy data shall be written for this load/time step
      bool should_write_energy_for_this_step(int step) const;

      /// \brief return the number of the load/time step for which the results have been written
      int get_last_written_results() const;

      /// \brief sets the last written load/time step for which the results have been written
      void set_last_written_results(int step);

      /// @name Get printing and output parameters/files
      ///@{
      /// get the output file for energy
      std::ostream& get_energy_output_stream()
      {
        check_init_setup();

        FOUR_C_ASSERT(!energyfile_.is_null(), "energy file stream uninitialized");

        return *energyfile_;
      };

      /// Is GMSH output of displacements required?
      const bool& IsGmsh() const
      {
        check_init_setup();
        return gmsh_out_;
      };

      /// Shall we print the logo?
      const bool& IsLogo() const
      {
        check_init_setup();
        return printlogo_;
      };

      /// Shall we print intermediate iterations during solution?
      const bool& is_print_intermediate_iterations() const
      {
        check_init_setup();
        return printiter_;
      };

      /// Shall we write output every iteration?
      const bool& IsOutputEveryIter() const
      {
        check_init_setup();
        return outputeveryiter_;
      };

      /// Shall we write surfactant output?
      const bool& IsSurfactantOutput() const
      {
        check_init_setup();
        return writesurfactant_;
      };

      /// Shall we write the current state?
      const bool& IsWriteState() const
      {
        check_init_setup();
        return writestate_;
      };

      /// Shall we write the velocities and accelerations?
      const bool& IsWriteVelAcc() const
      {
        check_init_setup();
        return writevelacc_;
      };

      /// Shall we write the current element volume?
      bool is_write_current_ele_volume() const
      {
        check_init_setup();
        return writecurrentelevolume_;
      }

      /// Shall we write the jacobian to MATLAB?
      bool is_write_jacobian_to_matlab() const
      {
        check_init_setup();
        return writejac2matlab_;
      }

      /// Shall we compute and write the condition number?
      INPAR::STR::ConditionNumber ConditionNumberType() const
      {
        check_init_setup();
        return conditionnumbertype_;
      }

      /// Is this the first output of the current run?
      const bool& IsFirstOutputOfRun() const
      {
        check_init_setup();
        return firstoutputofrun_;
      };

      /// Print infos to standard out every n step
      const int& get_print2_screen_every_n_step() const
      {
        check_init_setup();
        return printscreen_;
      };

      /// Get the output counter for OutputEveryIter
      const int& get_oei_output_counter() const
      {
        check_init_setup();
        return outputcounter_;
      };

      /// returns the offset added to the current step to shift the steps to be written
      int get_write_timestep_offset() const
      {
        check_init_setup();
        return writetimestepoffset_;
      }

      /// write restart every given step. if 0, restart is not written
      const int& get_write_restart_every_n_step() const
      {
        check_init_setup();
        return writerestartevery_;
      };

      /// write state/stress/strain every given step
      const int& get_write_results_every_n_step() const
      {
        check_init_setup();
        return writeresultsevery_;
      };

      /// write system energy every given step
      const int& get_write_energy_every_n_step() const
      {
        check_init_setup();
        return writeenergyevery_;
      }

      /// get stress output type
      const INPAR::STR::StressType& GetStressOutputType() const
      {
        check_init_setup();
        return writestress_;
      }

      /// get output type of coupling stress
      const INPAR::STR::StressType& get_coupling_stress_output_type() const
      {
        check_init_setup();
        return writecouplstress_;
      }

      /// get strain output type
      const INPAR::STR::StrainType& GetStrainOutputType() const
      {
        check_init_setup();
        return writestrain_;
      }

      /// get plastic strain output type
      const INPAR::STR::StrainType& get_plastic_strain_output_type() const
      {
        check_init_setup();
        return writeplstrain_;
      };

      /// get optional quantity output type
      const INPAR::STR::OptQuantityType& get_opt_quantity_output_type() const
      {
        check_init_setup();
        return writeoptquantity_;
      }
      ///@}

      /// set the flag indicator firstoutputofrun_
      void SetFirstOutputOfRun(const bool& firstoutputofrun)
      {
        check_init_setup();
        firstoutputofrun_ = firstoutputofrun;
      }

      /// Initialize and setup the every iteration output writer
      void init_setup_every_iteration_writer(
          IO::EveryIterationWriterInterface* interface, Teuchos::ParameterList& p_nox);

      /// initialize the output of system energy
      void setup_energy_output_file();

     protected:
      /// @name variables for internal use only
      ///@{
      ///
      bool isinit_;

      bool issetup_;
      ///@}

     private:
      /// @name Printing and output
      ///@{

      /// binary output
      Teuchos::RCP<IO::DiscretizationWriter> output_;

      /// additional output writer for the Newton steps
      Teuchos::RCP<IO::EveryIterationWriter> writer_every_iter_;

      /// data container for input parameters related to VTK output at runtime
      Teuchos::RCP<ParamsRuntimeOutput> params_runtime_vtk_output_;

      /// data container for input parameters related to VTP output at runtime
      Teuchos::RCP<ParamsRuntimeVtpOutput> params_runtime_vtp_output_;

      /// data container for input parameters related to monitoring of reaction forces
      Teuchos::RCP<ParamsMonitorDBC> params_monitor_dbc_;
      /// outputfile for energy
      Teuchos::RCP<std::ofstream> energyfile_;

      /// Is GMSH output of displacements required?
      bool gmsh_out_;

      /// print the logo (or not)?
      bool printlogo_;

      /// print intermediate iterations during solution
      bool printiter_;

      /// write output every iteration (Newton, line search, load step, etc.)
      bool outputeveryiter_;

      /// write surfactant output
      bool writesurfactant_;

      /// write state on/off
      bool writestate_;

      /// write velocity and acceleration on/off
      bool writevelacc_;

      /// write jacobian to MATLAB
      bool writejac2matlab_;

      /// flag whether this output step is the first one (restarted or not)
      bool firstoutputofrun_;

      /// flag element volume on/off
      bool writecurrentelevolume_ = false;

      /// print infos to standard out every n steps
      int printscreen_;

      /// output counter for OutputEveryIter
      int outputcounter_;

      /// offset added on the current step to determine if output/restart should be written
      int writetimestepoffset_ = 0;

      /// write restart every given step. if 0, restart is not written
      int writerestartevery_;

      /// write state/stress/strain every given step
      int writeresultsevery_;

      /// write system energy every given step
      int writeenergyevery_;

      /// timestep of the last written results
      int lastwrittenresultsstep_;

      /// stress output type
      INPAR::STR::StressType writestress_;

      /// output type of coupling stress
      INPAR::STR::StressType writecouplstress_;

      /// strain output type
      INPAR::STR::StrainType writestrain_;

      /// plastic strain output type
      INPAR::STR::StrainType writeplstrain_;

      /// optional quantity type
      INPAR::STR::OptQuantityType writeoptquantity_;

      INPAR::STR::ConditionNumber conditionnumbertype_;

      Teuchos::RCP<Teuchos::ParameterList> p_io_every_iteration_;

      ///@}
    };  // class BaseDataIO
  }     // namespace TIMINT
}  // namespace STR

namespace NOX
{
  namespace NLN
  {
    namespace Solver
    {
      namespace PrePostOp
      {
        namespace TIMINT
        {
          /*! \brief Helper class to write the output each Newton step
           *
           *  This class is an implementation of the ::NOX::Abstract::PrePostOperator
           *  and is used to modify the step() routine of the given ::NOX::Solver::Generic
           *  class.
           *  It's called by the wrapper classes ::NOX::Solver::PrePostOperator and
           *  NOX::PrePostOperatorVector.
           *
           *  \author Michael Hiermeier \date 03/17 */
          class WriteOutputEveryIteration : public NOX::NLN::Abstract::PrePostOperator
          {
           public:
            /// constructor
            WriteOutputEveryIteration(IO::EveryIterationWriter& every_iter_writer);


            /// called at the very beginning of a Newton loop
            void runPreSolve(const ::NOX::Solver::Generic& solver) override;

            /// called in the end of each Newton step
            void runPostIterate(const ::NOX::Solver::Generic& solver) override;

            /// called before the step is reduced in a line search routine
            void run_pre_modify_step_length(const ::NOX::Solver::Generic& solver,
                const ::NOX::LineSearch::Generic& linesearch) override;

           private:
            IO::EveryIterationWriter& every_iter_writer_;
          };
        }  // namespace TIMINT
      }    // namespace PrePostOp
    }      // namespace Solver
  }        // namespace NLN
}  // namespace NOX



FOUR_C_NAMESPACE_CLOSE

#endif
