/*---------------------------------------------------------------------------*/
/*! \file
\brief heat source handler for smoothed particle hydrodynamics (SPH) interactions
\level 3
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
#ifndef FOUR_C_PARTICLE_INTERACTION_SPH_HEATSOURCE_HPP
#define FOUR_C_PARTICLE_INTERACTION_SPH_HEATSOURCE_HPP

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "4C_config.hpp"

#include "4C_inpar_particle.hpp"
#include "4C_particle_engine_enums.hpp"
#include "4C_particle_engine_typedefs.hpp"

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | forward declarations                                                      |
 *---------------------------------------------------------------------------*/
namespace PARTICLEENGINE
{
  class ParticleEngineInterface;
  class ParticleContainerBundle;
}  // namespace PARTICLEENGINE

namespace PARTICLEINTERACTION
{
  class MaterialHandler;
  class SPHNeighborPairs;
}  // namespace PARTICLEINTERACTION

namespace MAT
{
  namespace PAR
  {
    class ParticleMaterialThermo;
  }
}  // namespace MAT

/*---------------------------------------------------------------------------*
 | class declarations                                                        |
 *---------------------------------------------------------------------------*/
namespace PARTICLEINTERACTION
{
  class SPHHeatSourceBase
  {
   public:
    //! constructor
    explicit SPHHeatSourceBase(const Teuchos::ParameterList& params);

    //! virtual destructor
    virtual ~SPHHeatSourceBase() = default;

    //! init heat source handler
    virtual void Init();

    //! setup heat source handler
    virtual void Setup(
        const std::shared_ptr<PARTICLEENGINE::ParticleEngineInterface> particleengineinterface,
        const std::shared_ptr<PARTICLEINTERACTION::MaterialHandler> particlematerial,
        const std::shared_ptr<PARTICLEINTERACTION::SPHNeighborPairs> neighborpairs);

    //! evaluate heat source
    virtual void EvaluateHeatSource(const double& evaltime) const = 0;

   protected:
    //! smoothed particle hydrodynamics specific parameter list
    const Teuchos::ParameterList& params_sph_;

    //! interface to particle engine
    std::shared_ptr<PARTICLEENGINE::ParticleEngineInterface> particleengineinterface_;

    //! particle container bundle
    PARTICLEENGINE::ParticleContainerBundleShrdPtr particlecontainerbundle_;

    //! particle material handler
    std::shared_ptr<PARTICLEINTERACTION::MaterialHandler> particlematerial_;

    //! neighbor pair handler
    std::shared_ptr<PARTICLEINTERACTION::SPHNeighborPairs> neighborpairs_;

    //! pointer to thermo material of particle types
    std::vector<const MAT::PAR::ParticleMaterialThermo*> thermomaterial_;

    //! heat source function number
    const int heatsourcefctnumber_;

    //! set of absorbing particle types
    std::set<PARTICLEENGINE::TypeEnum> absorbingtypes_;

    //! set of non-absorbing particle types
    std::set<PARTICLEENGINE::TypeEnum> nonabsorbingtypes_;
  };

  class SPHHeatSourceVolume : public SPHHeatSourceBase
  {
   public:
    //! constructor
    explicit SPHHeatSourceVolume(const Teuchos::ParameterList& params);

    //! evaluate heat source
    void EvaluateHeatSource(const double& evaltime) const override;
  };

  class SPHHeatSourceSurface : public SPHHeatSourceBase
  {
   public:
    //! constructor
    explicit SPHHeatSourceSurface(const Teuchos::ParameterList& params);

    //! init heat source handler
    void Init() override;

    //! evaluate heat source
    void EvaluateHeatSource(const double& evaltime) const override;

   private:
    //! heat source direction vector
    std::vector<double> direction_;

    //! evaluate heat source direction
    bool eval_direction_;
  };

}  // namespace PARTICLEINTERACTION

/*---------------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif
