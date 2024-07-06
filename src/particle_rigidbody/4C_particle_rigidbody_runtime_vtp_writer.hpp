/*---------------------------------------------------------------------------*/
/*! \file
\brief write visualization output for rigid bodies in vtk/vtp format at runtime
\level 1
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
#ifndef FOUR_C_PARTICLE_RIGIDBODY_RUNTIME_VTP_WRITER_HPP
#define FOUR_C_PARTICLE_RIGIDBODY_RUNTIME_VTP_WRITER_HPP

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "4C_config.hpp"

#include "4C_io_visualization_manager.hpp"

#include <Epetra_Comm.h>

#include <memory>

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | forward declarations                                                      |
 *---------------------------------------------------------------------------*/
namespace Core::IO
{
  class DiscretizationReader;
}  // namespace Core::IO

namespace ParticleRigidBody
{
  class RigidBodyDataState;
}

/*---------------------------------------------------------------------------*
 | class declarations                                                        |
 *---------------------------------------------------------------------------*/
namespace ParticleRigidBody
{
  /*!
   * \brief rigid body runtime vtp writer class
   *
   * A class that writes visualization output for rigid bodies in vtk/vtp format at runtime.
   *
   * \author Sebastian Fuchs \date 09/2020
   */
  class RigidBodyRuntimeVtpWriter final
  {
   public:
    /*!
     * \brief constructor
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \param[in] comm communicator
     */
    explicit RigidBodyRuntimeVtpWriter(const Epetra_Comm& comm);

    /*!
     * \brief init rigid body runtime vtp writer
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \param[in] rigidbodydatastate rigid body data state container
     */
    void init(const std::shared_ptr<ParticleRigidBody::RigidBodyDataState> rigidbodydatastate);

    /*!
     * \brief read restart of runtime vtp writer
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \param[in] reader discretization reader
     */
    void read_restart(const std::shared_ptr<Core::IO::DiscretizationReader> reader);

    /*!
     * \brief set positions and states of rigid bodies
     *
     * Set positions and states of rigid bodies owned by this processor.
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \param[in] ownedrigidbodies owned rigid bodies by this processor
     */
    void set_rigid_body_positions_and_states(const std::vector<int>& ownedrigidbodies);

    /*!
     * \brief Write the visualization files to disk
     */
    void write_to_disk(const double time, const unsigned int timestep_number);

   private:
    //! communicator
    const Epetra_Comm& comm_;

    //! setup time of runtime vtp writer
    double setuptime_;

    //! rigid body data state container
    std::shared_ptr<ParticleRigidBody::RigidBodyDataState> rigidbodydatastate_;

    //! visualization manager
    std::shared_ptr<Core::IO::VisualizationManager> visualization_manager_;
  };

}  // namespace ParticleRigidBody

/*---------------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif
