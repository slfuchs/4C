/*---------------------------------------------------------------------------*/
/*! \file
\brief discrete element method (DEM) interaction handler
\level 3
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "4C_particle_interaction_dem.hpp"

#include "4C_global_data.hpp"
#include "4C_io_runtime_csv_writer.hpp"
#include "4C_particle_engine_container.hpp"
#include "4C_particle_engine_interface.hpp"
#include "4C_particle_interaction_dem_adhesion.hpp"
#include "4C_particle_interaction_dem_contact.hpp"
#include "4C_particle_interaction_dem_history_pairs.hpp"
#include "4C_particle_interaction_dem_neighbor_pairs.hpp"
#include "4C_particle_interaction_material_handler.hpp"
#include "4C_particle_interaction_runtime_writer.hpp"
#include "4C_particle_interaction_utils.hpp"
#include "4C_particle_wall_interface.hpp"

#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
PARTICLEINTERACTION::ParticleInteractionDEM::ParticleInteractionDEM(
    const Epetra_Comm& comm, const Teuchos::ParameterList& params)
    : PARTICLEINTERACTION::ParticleInteractionBase(comm, params),
      params_dem_(params.sublist("DEM")),
      writeparticleenergy_(CORE::UTILS::IntegralValue<int>(params_dem_, "WRITE_PARTICLE_ENERGY"))
{
  // empty constructor
}

PARTICLEINTERACTION::ParticleInteractionDEM::~ParticleInteractionDEM() = default;

void PARTICLEINTERACTION::ParticleInteractionDEM::Init()
{
  // call base class init
  ParticleInteractionBase::Init();

  // init neighbor pair handler
  init_neighbor_pair_handler();

  // init history pair handler
  init_history_pair_handler();

  // init contact handler
  init_contact_handler();

  // init adhesion handler
  init_adhesion_handler();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::Setup(
    const std::shared_ptr<PARTICLEENGINE::ParticleEngineInterface> particleengineinterface,
    const std::shared_ptr<PARTICLEWALL::WallHandlerInterface> particlewallinterface)
{
  // call base class setup
  ParticleInteractionBase::Setup(particleengineinterface, particlewallinterface);

  // setup neighbor pair handler
  neighborpairs_->Setup(particleengineinterface, particlewallinterface);

  // setup history pair handler
  historypairs_->Setup(particleengineinterface);

  // setup contact handler
  contact_->Setup(particleengineinterface, particlewallinterface, particlematerial_,
      particleinteractionwriter_, neighborpairs_, historypairs_);

  // setup adhesion handler
  if (adhesion_)
    adhesion_->Setup(particleengineinterface, particlewallinterface, particleinteractionwriter_,
        neighborpairs_, historypairs_, contact_->get_normal_contact_stiffness());

  // setup particle interaction writer
  setup_particle_interaction_writer();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::WriteRestart() const
{
  // call base class function
  ParticleInteractionBase::WriteRestart();

  // write restart of history pair handler
  historypairs_->WriteRestart();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::read_restart(
    const std::shared_ptr<IO::DiscretizationReader> reader)
{
  // call base class function
  ParticleInteractionBase::read_restart(reader);

  // read restart of history pair handler
  historypairs_->read_restart(reader);
}

void PARTICLEINTERACTION::ParticleInteractionDEM::insert_particle_states_of_particle_types(
    std::map<PARTICLEENGINE::TypeEnum, std::set<PARTICLEENGINE::StateEnum>>& particlestatestotypes)
{
  // iterate over particle types
  for (auto& typeIt : particlestatestotypes)
  {
    // set of particle states for current particle type
    std::set<PARTICLEENGINE::StateEnum>& particlestates = typeIt.second;

    // insert states of regular phase particles
    particlestates.insert({PARTICLEENGINE::Force, PARTICLEENGINE::Mass, PARTICLEENGINE::Radius});
  }

  // states for contact evaluation scheme
  contact_->insert_particle_states_of_particle_types(particlestatestotypes);
}

void PARTICLEINTERACTION::ParticleInteractionDEM::SetInitialStates()
{
  // set initial radius
  set_initial_radius();

  // set initial mass
  set_initial_mass();

  // set initial inertia
  set_initial_inertia();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::PreEvaluateTimeStep()
{
  TEUCHOS_FUNC_TIME_MONITOR("PARTICLEINTERACTION::ParticleInteractionDEM::PreEvaluateTimeStep");
}

void PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_interactions()
{
  TEUCHOS_FUNC_TIME_MONITOR("PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_interactions");

  // clear force and moment states of particles
  clear_force_and_moment_states();

  // evaluate neighbor pairs
  neighborpairs_->evaluate_neighbor_pairs();

  // evaluate adhesion neighbor pairs
  if (adhesion_) neighborpairs_->evaluate_neighbor_pairs_adhesion(adhesion_->GetAdhesionDistance());

  // check critical time step
  contact_->check_critical_time_step();

  // add contact contribution to force and moment field
  contact_->add_force_and_moment_contribution();

  // add adhesion contribution to force field
  if (adhesion_) adhesion_->add_force_contribution();

  // compute acceleration from force and moment
  compute_acceleration();

  // update history pairs
  historypairs_->UpdateHistoryPairs();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::post_evaluate_time_step(
    std::vector<PARTICLEENGINE::ParticleTypeToType>& particlesfromphasetophase)
{
  TEUCHOS_FUNC_TIME_MONITOR("PARTICLEINTERACTION::ParticleInteractionDEM::post_evaluate_time_step");

  // evaluate particle energy
  if (particleinteractionwriter_->get_current_write_result_flag() and writeparticleenergy_)
    evaluate_particle_energy();
}

double PARTICLEINTERACTION::ParticleInteractionDEM::max_interaction_distance() const
{
  // particle contact interaction distance
  double interactiondistance = 2.0 * MaxParticleRadius();

  // add adhesion distance
  if (adhesion_) interactiondistance += adhesion_->GetAdhesionDistance();

  return interactiondistance;
}

void PARTICLEINTERACTION::ParticleInteractionDEM::distribute_interaction_history() const
{
  // distribute history pairs
  historypairs_->distribute_history_pairs();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::communicate_interaction_history() const
{
  // communicate history pairs
  historypairs_->communicate_history_pairs();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::set_current_step_size(
    const double currentstepsize)
{
  // call base class method
  ParticleInteractionBase::set_current_step_size(currentstepsize);

  // set current step size
  contact_->set_current_step_size(currentstepsize);
}

void PARTICLEINTERACTION::ParticleInteractionDEM::init_neighbor_pair_handler()
{
  // create neighbor pair handler
  neighborpairs_ = std::make_shared<PARTICLEINTERACTION::DEMNeighborPairs>();

  // init neighbor pair handler
  neighborpairs_->Init();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::init_history_pair_handler()
{
  // create history pair handler
  historypairs_ = std::make_shared<PARTICLEINTERACTION::DEMHistoryPairs>(comm_);

  // init history pair handler
  historypairs_->Init();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::init_contact_handler()
{
  // create contact handler
  contact_ = std::unique_ptr<PARTICLEINTERACTION::DEMContact>(
      new PARTICLEINTERACTION::DEMContact(params_dem_));

  // init contact handler
  contact_->Init();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::init_adhesion_handler()
{
  // get type of adhesion law
  INPAR::PARTICLE::AdhesionLaw adhesionlaw =
      CORE::UTILS::IntegralValue<INPAR::PARTICLE::AdhesionLaw>(params_dem_, "ADHESIONLAW");

  // create adhesion handler
  if (adhesionlaw != INPAR::PARTICLE::NoAdhesion)
    adhesion_ = std::unique_ptr<PARTICLEINTERACTION::DEMAdhesion>(
        new PARTICLEINTERACTION::DEMAdhesion(params_dem_));

  // init adhesion handler
  if (adhesion_) adhesion_->Init();
}

void PARTICLEINTERACTION::ParticleInteractionDEM::setup_particle_interaction_writer()
{
  if (writeparticleenergy_)
  {
    // register specific runtime csv writer
    particleinteractionwriter_->register_specific_runtime_csv_writer("particle-energy");

    // get specific runtime csv writer
    IO::RuntimeCsvWriter* runtime_csv_writer =
        particleinteractionwriter_->get_specific_runtime_csv_writer("particle-energy");

    // register all data vectors
    runtime_csv_writer->register_data_vector("kin_energy", 1, 10);
    runtime_csv_writer->register_data_vector("grav_pot_energy", 1, 10);
    runtime_csv_writer->register_data_vector("elast_pot_energy", 1, 10);
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::set_initial_radius()
{
  // get allowed bounds for particle radius
  double r_min = params_dem_.get<double>("MIN_RADIUS");
  double r_max = params_dem_.get<double>("MAX_RADIUS");

  // safety checks
  if (r_min < 0.0) FOUR_C_THROW("negative minimum allowed particle radius!");
  if (not(r_max > 0.0)) FOUR_C_THROW("non-positive maximum allowed particle radius!");
  if (r_min > r_max)
    FOUR_C_THROW("minimum allowed particle radius larger than maximum allowed particle radius!");

  // get type of initial particle radius assignment
  INPAR::PARTICLE::InitialRadiusAssignment radiusdistributiontype =
      CORE::UTILS::IntegralValue<INPAR::PARTICLE::InitialRadiusAssignment>(
          params_dem_, "INITIAL_RADIUS");

  switch (radiusdistributiontype)
  {
    // particle radius from particle material
    case INPAR::PARTICLE::RadiusFromParticleMaterial:
    {
      // iterate over particle types
      for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
      {
        // get container of owned particles of current particle type
        PARTICLEENGINE::ParticleContainer* container =
            particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

        // get number of particles stored in container
        const int particlestored = container->ParticlesStored();

        // no owned particles of current particle type
        if (particlestored <= 0) continue;

        // get material for current particle type
        const MAT::PAR::ParticleMaterialBase* material =
            particlematerial_->get_ptr_to_particle_mat_parameter(type_i);

        // safety checks
        if (material->initRadius_ < r_min)
          FOUR_C_THROW("material particle radius smaller than minimum allowed particle radius!");

        if (material->initRadius_ > r_max)
          FOUR_C_THROW("material particle radius larger than maximum allowed particle radius!");

        // (initial) radius of current phase
        std::vector<double> initradius(1);
        initradius[0] = material->initRadius_;

        // set initial radius for all particles of current type
        container->set_state(initradius, PARTICLEENGINE::Radius);
      }

      break;
    }
    // particle radius from particle input
    case INPAR::PARTICLE::RadiusFromParticleInput:
    {
      // note: particle radius set as read in from input file, only safety checks here

      // iterate over particle types
      for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
      {
        // get container of owned particles of current particle type
        PARTICLEENGINE::ParticleContainer* container =
            particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

        // get number of particles stored in container
        const int particlestored = container->ParticlesStored();

        // no owned particles of current particle type
        if (particlestored <= 0) continue;

        // safety checks
        if (container->GetMinValueOfState(PARTICLEENGINE::Radius) < r_min)
          FOUR_C_THROW("minimum particle radius smaller than minimum allowed particle radius!");

        if (container->GetMaxValueOfState(PARTICLEENGINE::Radius) > r_max)
          FOUR_C_THROW("maximum particle radius larger than maximum allowed particle radius!");
      }

      break;
    }
    // normal or log-normal random particle radius distribution
    case INPAR::PARTICLE::NormalRadiusDistribution:
    case INPAR::PARTICLE::LogNormalRadiusDistribution:
    {
      // get sigma of random particle radius distribution
      double sigma = params_dem_.get<double>("RADIUSDISTRIBUTION_SIGMA");

      // safety check
      if (not(sigma > 0.0))
        FOUR_C_THROW("non-positive sigma of random particle radius distribution!");

      // iterate over particle types
      for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
      {
        // get container of owned particles of current particle type
        PARTICLEENGINE::ParticleContainer* container =
            particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

        // get number of particles stored in container
        const int particlestored = container->ParticlesStored();

        // no owned particles of current particle type
        if (particlestored <= 0) continue;

        // get material for current particle type
        const MAT::PAR::ParticleMaterialBase* material =
            particlematerial_->get_ptr_to_particle_mat_parameter(type_i);

        // get pointer to particle state
        double* radius = container->GetPtrToState(PARTICLEENGINE::Radius, 0);

        // determine mu of random particle radius distribution
        const double mu = (radiusdistributiontype == INPAR::PARTICLE::NormalRadiusDistribution)
                              ? material->initRadius_
                              : std::log(material->initRadius_);

        // initialize random number generator
        GLOBAL::Problem::Instance()->Random()->SetMeanVariance(mu, sigma);

        // iterate over particles stored in container
        for (int i = 0; i < particlestored; ++i)
        {
          // generate random value
          const double randomvalue = GLOBAL::Problem::Instance()->Random()->Normal();

          // set normal or log-normal distributed random value for particle radius
          radius[i] = (radiusdistributiontype == INPAR::PARTICLE::NormalRadiusDistribution)
                          ? randomvalue
                          : std::exp(randomvalue);

          // adjust radius to allowed bounds
          if (radius[i] > r_max)
            radius[i] = r_max;
          else if (radius[i] < r_min)
            radius[i] = r_min;
        }
      }
      break;
    }
    default:
    {
      FOUR_C_THROW("invalid type of (random) particle radius distribution!");
      break;
    }
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::set_initial_mass()
{
  // iterate over particle types
  for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainer* container =
        particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // get material for current particle type
    const MAT::PAR::ParticleMaterialBase* material =
        particlematerial_->get_ptr_to_particle_mat_parameter(type_i);

    // get pointer to particle states
    const double* radius = container->GetPtrToState(PARTICLEENGINE::Radius, 0);
    double* mass = container->GetPtrToState(PARTICLEENGINE::Mass, 0);

    // compute mass via particle volume and initial density
    const double fac = material->initDensity_ * 4.0 / 3.0 * M_PI;
    for (int i = 0; i < particlestored; ++i) mass[i] = fac * UTILS::Pow<3>(radius[i]);
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::set_initial_inertia()
{
  // iterate over particle types
  for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainer* container =
        particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // no inertia state for current particle type
    if (not container->HaveStoredState(PARTICLEENGINE::Inertia)) continue;

    // get pointer to particle states
    const double* radius = container->GetPtrToState(PARTICLEENGINE::Radius, 0);
    const double* mass = container->GetPtrToState(PARTICLEENGINE::Mass, 0);
    double* inertia = container->GetPtrToState(PARTICLEENGINE::Inertia, 0);

    // compute mass via particle volume and initial density
    for (int i = 0; i < particlestored; ++i) inertia[i] = 0.4 * mass[i] * UTILS::Pow<2>(radius[i]);
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::clear_force_and_moment_states() const
{
  // iterate over particle types
  for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainer* container =
        particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

    // clear force of all particles
    container->ClearState(PARTICLEENGINE::Force);

    // clear moment of all particles
    if (container->HaveStoredState(PARTICLEENGINE::Moment))
      container->ClearState(PARTICLEENGINE::Moment);
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::compute_acceleration() const
{
  TEUCHOS_FUNC_TIME_MONITOR("PARTICLEINTERACTION::ParticleInteractionDEM::compute_acceleration");

  // iterate over particle types
  for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainer* container =
        particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // get particle state dimension
    const int statedim = container->GetStateDim(PARTICLEENGINE::Acceleration);

    // get pointer to particle states
    const double* radius = container->GetPtrToState(PARTICLEENGINE::Radius, 0);
    const double* mass = container->GetPtrToState(PARTICLEENGINE::Mass, 0);
    const double* force = container->GetPtrToState(PARTICLEENGINE::Force, 0);
    const double* moment = container->CondGetPtrToState(PARTICLEENGINE::Moment, 0);
    double* acc = container->GetPtrToState(PARTICLEENGINE::Acceleration, 0);
    double* angacc = container->CondGetPtrToState(PARTICLEENGINE::AngularAcceleration, 0);

    // compute acceleration
    for (int i = 0; i < particlestored; ++i)
      UTILS::VecAddScale(&acc[statedim * i], (1.0 / mass[i]), &force[statedim * i]);

    // compute angular acceleration
    if (angacc and moment)
    {
      for (int i = 0; i < particlestored; ++i)
        UTILS::VecAddScale(&angacc[statedim * i],
            (5.0 / (2.0 * mass[i] * UTILS::Pow<2>(radius[i]))), &moment[statedim * i]);
    }
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_particle_energy() const
{
  TEUCHOS_FUNC_TIME_MONITOR(
      "PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_particle_energy");

  // evaluate particle kinetic energy contribution
  std::vector<double> kinenergy(1, 0.0);
  {
    std::vector<double> localkinenergy(1, 0.0);
    evaluate_particle_kinetic_energy(localkinenergy[0]);
    comm_.SumAll(localkinenergy.data(), kinenergy.data(), 1);
  }

  // evaluate particle gravitational potential energy contribution
  std::vector<double> gravpotenergy(1, 0.0);
  {
    std::vector<double> localgravpotenergy(1, 0.0);
    evaluate_particle_gravitational_potential_energy(localgravpotenergy[0]);
    comm_.SumAll(localgravpotenergy.data(), gravpotenergy.data(), 1);
  }

  // evaluate elastic potential energy contribution
  std::vector<double> elastpotenergy(1, 0.0);
  {
    std::vector<double> localelastpotenergy(1, 0.0);
    contact_->evaluate_elastic_potential_energy(localelastpotenergy[0]);
    comm_.SumAll(localelastpotenergy.data(), elastpotenergy.data(), 1);
  }

  // get specific runtime csv writer
  IO::RuntimeCsvWriter* runtime_csv_writer =
      particleinteractionwriter_->get_specific_runtime_csv_writer("particle-energy");

  // append data vector
  runtime_csv_writer->AppendDataVector("kin_energy", kinenergy);
  runtime_csv_writer->AppendDataVector("grav_pot_energy", gravpotenergy);
  runtime_csv_writer->AppendDataVector("elast_pot_energy", elastpotenergy);
}

void PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_particle_kinetic_energy(
    double& kineticenergy) const
{
  TEUCHOS_FUNC_TIME_MONITOR(
      "PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_particle_kinetic_energy");

  // iterate over particle types
  for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainer* container =
        particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // get particle state dimension
    const int statedim = container->GetStateDim(PARTICLEENGINE::Position);

    // get pointer to particle states
    const double* radius = container->GetPtrToState(PARTICLEENGINE::Radius, 0);
    const double* mass = container->GetPtrToState(PARTICLEENGINE::Mass, 0);
    const double* vel = container->GetPtrToState(PARTICLEENGINE::Velocity, 0);
    double* angvel = container->CondGetPtrToState(PARTICLEENGINE::AngularVelocity, 0);

    // add translational kinetic energy contribution
    for (int i = 0; i < particlestored; ++i)
      kineticenergy += 0.5 * mass[i] * UTILS::VecDot(&vel[statedim * i], &vel[statedim * i]);

    // add rotational kinetic energy contribution
    if (angvel)
    {
      for (int i = 0; i < particlestored; ++i)
        kineticenergy += 0.5 * (0.4 * mass[i] * UTILS::Pow<2>(radius[i])) *
                         UTILS::VecDot(&angvel[statedim * i], &angvel[statedim * i]);
    }
  }
}

void PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_particle_gravitational_potential_energy(
    double& gravitationalpotentialenergy) const
{
  TEUCHOS_FUNC_TIME_MONITOR(
      "PARTICLEINTERACTION::ParticleInteractionDEM::evaluate_particle_gravitational_potential_"
      "energy");

  // iterate over particle types
  for (const auto& type_i : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainer* container =
        particlecontainerbundle_->get_specific_container(type_i, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // get particle state dimension
    const int statedim = container->GetStateDim(PARTICLEENGINE::Position);

    // get pointer to particle states
    const double* pos = container->GetPtrToState(PARTICLEENGINE::Position, 0);
    const double* mass = container->GetPtrToState(PARTICLEENGINE::Mass, 0);

    // add gravitational potential energy contribution
    for (int i = 0; i < particlestored; ++i)
      gravitationalpotentialenergy -= mass[i] * UTILS::VecDot(gravity_.data(), &pos[statedim * i]);
  }
}

FOUR_C_NAMESPACE_CLOSE
