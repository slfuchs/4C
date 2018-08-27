/*-----------------------------------------------------------*/
/*!
\file beaminteraction_submodel_evaluator_beamcontact.cpp

\brief class for submodel beam contact

\maintainer Jonas Eichinger, Maximilian Grill

\level 3

*/
/*-----------------------------------------------------------*/

#include "../drt_beaminteraction/beaminteraction_submodel_evaluator_beamcontact.H"
#include "../drt_beaminteraction/beam_contact_pair.H"
#include "../drt_beaminteraction/beam_contact_evaluation_data.H"
#include "../drt_beaminteraction/beam_contact_params.H"
#include "../drt_beaminteraction/beam_contact_runtime_vtk_output_params.H"
#include "../drt_beaminteraction/beaminteraction_calc_utils.H"
#include "../drt_beaminteraction/str_model_evaluator_beaminteraction_datastate.H"

#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_globalproblem.H"

#include "../drt_io/io.H"
#include "../drt_io/io_control.H"
#include "../drt_io/io_pstream.H"
#include "../drt_io/runtime_vtp_writer.H"

#include "../drt_structure_new/str_timint_basedataglobalstate.H"
#include "../drt_structure_new/str_timint_basedataio.H"

#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_serialdensematrix.H"
#include "../linalg/linalg_serialdensevector.H"
#include "../linalg/linalg_fixedsizematrix.H"

#include "../drt_inpar/inpar_beamcontact.H"

#include "../drt_binstrategy/binning_strategy.H"

#include "../drt_beam3/beam3_base.H"
#include "../drt_rigidsphere/rigidsphere.H"

#include <Teuchos_TimeMonitor.hpp>

#include <NOX_Solver_Generic.H>

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::BeamContact()
    : beam_contact_params_ptr_(Teuchos::null),
      beam_contact_evaluation_data_ptr_(Teuchos::null),
      contact_elepairs_(Teuchos::null)
{
  // clear stl stuff
  nearby_elements_map_.clear();
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::Setup()
{
  CheckInit();

  // build a new data container to manage beam contact parameters
  beam_contact_params_ptr_ = Teuchos::rcp( new BEAMINTERACTION::BeamContactParams() );

  // build a new data container to manage beam contact evaluation data that can not be stored pairwise
  beam_contact_evaluation_data_ptr_ = Teuchos::rcp( new BEAMINTERACTION::BeamContactEvaluationData() );

  // build runtime vtp writer if desired
  if ( (bool) DRT::INPUT::IntegralValue<int>(DRT::Problem::Instance()->BeamContactParams().sublist(
      "RUNTIME VTK OUTPUT"),"VTK_OUTPUT_BEAM_CONTACT") )
  {
    beam_contact_params_ptr_->BuildBeamContactRuntimeVtkOutputParams();

    InitOutputRuntimeVtpBeamContact();
  }


  contactelementtypes_.clear();

  if ( DRT::INPUT::IntegralValue<INPAR::BEAMINTERACTION::Strategy>(
       DRT::Problem::Instance()->BeamInteractionParams().sublist("BEAM TO BEAM CONTACT"), "STRATEGY") != INPAR::BEAMINTERACTION::bstr_none
     )
  {
    contactelementtypes_.push_back(BINSTRATEGY::UTILS::Beam);

    beam_contact_params_ptr_->BuildBeamToBeamContactParams();
  }

  if ( DRT::INPUT::IntegralValue<INPAR::BEAMINTERACTION::Strategy>(
       DRT::Problem::Instance()->BeamInteractionParams().sublist("BEAM TO SPHERE CONTACT"), "STRATEGY") != INPAR::BEAMINTERACTION::bstr_none
     )
  {
    contactelementtypes_.push_back(BINSTRATEGY::UTILS::RigidSphere);

    beam_contact_params_ptr_->BuildBeamToSphereContactParams();
  }

  if ( DRT::INPUT::IntegralValue<INPAR::BEAMINTERACTION::Strategy>(
       DRT::Problem::Instance()->BeamInteractionParams().sublist("BEAM TO SOLID CONTACT"), "STRATEGY") != INPAR::BEAMINTERACTION::bstr_none
     )
  {
    contactelementtypes_.push_back(BINSTRATEGY::UTILS::Solid);

    beam_contact_params_ptr_->BuildBeamToSolidVolumeMeshtyingParams();
    beam_contact_evaluation_data_ptr_->BuildBeamToSolidVolumeMeshtyingEvaluationData();
  }


  // set flag
  issetup_ = true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::PostSetup()
{

  CheckInitSetup();

  // Todo really needed here? maybe find better place
  // ensure that contact is evaluated correctly at beginning of first time step (initial overlap)
  nearby_elements_map_.clear();
  FindAndStoreNeighboringElements();
  CreateBeamContactElementPairs();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::InitSubmodelDependencies(
    Teuchos::RCP<STR::MODELEVALUATOR::BeamInteraction::Map>  const submodelmap)
{
  CheckInitSetup();
  // no active influence on other submodels
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::Reset()
{
  CheckInitSetup();

  std::vector<Teuchos::RCP<BEAMINTERACTION::BeamContactPair> >::const_iterator iter;
  for ( iter = contact_elepairs_.begin(); iter != contact_elepairs_.end(); ++iter )
  {
    Teuchos::RCP<BEAMINTERACTION::BeamContactPair> elepairptr = *iter;

    std::vector<const DRT::Element*> element_ptr(2);

    element_ptr[0] = elepairptr->Element1();
    element_ptr[1] = elepairptr->Element2();

    // element Dof values relevant for centerline interpolation
    std::vector< std::vector<double> > element_posdofvec_absolutevalues(2);

    for ( unsigned int ielement = 0; ielement < 2; ++ielement )
    {
      // extract the Dof values of this element from displacement vector
      BEAMINTERACTION::UTILS::ExtractPosDofVecAbsoluteValues(
          Discret(),
          element_ptr[ielement],
          BeamInteractionDataStatePtr()->GetMutableDisColNp(),
          element_posdofvec_absolutevalues[ielement]);
    }

    // update the Dof values in the interaction element pair object
    elepairptr->ResetState(
        element_posdofvec_absolutevalues[0],
        element_posdofvec_absolutevalues[1]);
  }

}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::EvaluateForce()
{

  CheckInitSetup();

  // resulting discrete element force vectors of the two interacting elements
  std::vector< LINALG::SerialDenseVector > eleforce(2);

  // resulting discrete force vectors (centerline DOFs only!) of the two
  // interacting elements
  std::vector< LINALG::SerialDenseVector > eleforce_centerlineDOFs(2);

  std::vector< std::vector<LINALG::SerialDenseMatrix> > dummystiff;

  // element gids of interacting elements
  std::vector<int> elegids(2);

  // are non-zero force values returned which need assembly?
  bool pair_is_active = false;


  for ( auto& elepairptr : contact_elepairs_ )
  {
    // PreEvaluate the pair
    elepairptr->PreEvaluate();
  }


  for ( auto& elepairptr : contact_elepairs_ )
  {
    // Evaluate the pair and check if there is active contact
    pair_is_active = elepairptr->Evaluate(
        &eleforce_centerlineDOFs[0],
        &eleforce_centerlineDOFs[1],
        NULL,
        NULL,
        NULL,
        NULL);

    if (pair_is_active)
    {
      elegids[0] = elepairptr->Element1()->Id();
      elegids[1] = elepairptr->Element2()->Id();

      // assemble force vector affecting the centerline DoFs only
      // into element force vector ('all DoFs' format, as usual)
      BEAMINTERACTION::UTILS::AssembleCenterlineDofForceStiffIntoElementForceStiff(
          Discret(),
          elegids,
          eleforce_centerlineDOFs,
          dummystiff,
          &eleforce,
          NULL);

      // Fixme
      eleforce[0].Scale(-1.0);
      eleforce[1].Scale(-1.0);

      // assemble the contributions into force vector class variable
      // f_crosslink_np_ptr_, i.e. in the DOFs of the connected nodes
      BEAMINTERACTION::UTILS::FEAssembleEleForceStiffIntoSystemVectorMatrix( Discret(), elegids,
          eleforce, dummystiff, BeamInteractionDataStatePtr()->GetMutableForceNp(), Teuchos::null);
    }

  }
  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::EvaluateStiff()
{

  CheckInitSetup();

  // linearizations
  std::vector< std::vector<LINALG::SerialDenseMatrix> > elestiff( 2,
      std::vector<LINALG::SerialDenseMatrix>(2) );

  // linearizations (centerline DOFs only!)
  std::vector< std::vector< LINALG::SerialDenseMatrix > > elestiff_centerlineDOFs ( 2,
      std::vector<LINALG::SerialDenseMatrix>(2) );

  std::vector< LINALG::SerialDenseVector > dummyforce;

  // element gids of interacting elements
  std::vector<int> elegids(2);

  // are non-zero stiffness values returned which need assembly?
  bool pair_is_active = false;


  for ( auto& elepairptr : contact_elepairs_ )
  {
    // PreEvaluate the pair
    elepairptr->PreEvaluate();
  }

  for ( auto& elepairptr : contact_elepairs_ )
  {
    // Evaluate the pair and check if there is active contact
    pair_is_active = elepairptr->Evaluate(
        NULL,
        NULL,
        &elestiff_centerlineDOFs[0][0],
        &elestiff_centerlineDOFs[0][1],
        &elestiff_centerlineDOFs[1][0],
        &elestiff_centerlineDOFs[1][1]);

    if (pair_is_active)
    {
      elegids[0] = elepairptr->Element1()->Id();
      elegids[1] = elepairptr->Element2()->Id();

      // assemble stiffness matrix affecting the centerline DoFs only
      // into element stiffness matrix ('all DoFs' format, as usual)
      BEAMINTERACTION::UTILS::AssembleCenterlineDofForceStiffIntoElementForceStiff(
          Discret(),
          elegids,
          dummyforce,
          elestiff_centerlineDOFs,
          NULL,
          &elestiff);

      // assemble the contributions into force vector class variable
      // f_crosslink_np_ptr_, i.e. in the DOFs of the connected nodes
      BEAMINTERACTION::UTILS::FEAssembleEleForceStiffIntoSystemVectorMatrix( Discret(),
          elegids, dummyforce, elestiff, Teuchos::null, BeamInteractionDataStatePtr()->GetMutableStiff());
    }

  }
  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::EvaluateForceStiff()
{

  CheckInitSetup();

  // resulting discrete element force vectors of the two interacting elements
  std::vector< LINALG::SerialDenseVector > eleforce(2);

  // resulting discrete force vectors (centerline DOFs only!) of the two
  // interacting elements
  std::vector< LINALG::SerialDenseVector > eleforce_centerlineDOFs(2);

  // linearizations
  std::vector< std::vector<LINALG::SerialDenseMatrix> > elestiff( 2,
      std::vector<LINALG::SerialDenseMatrix>(2) );

  // linearizations (centerline DOFs only!)
  std::vector< std::vector< LINALG::SerialDenseMatrix > > elestiff_centerlineDOFs ( 2,
      std::vector<LINALG::SerialDenseMatrix>(2) );

  // element gids of interacting elements
  std::vector<int> elegids(2);

  // are non-zero stiffness values returned which need assembly?
  bool pair_is_active = false;


  for ( auto& elepairptr : contact_elepairs_ )
  {
    // PreEvaluate the pair
    elepairptr->PreEvaluate();
  }

  for ( auto& elepairptr : contact_elepairs_ )
  {
    // Evaluate the pair and check if there is active contact
    pair_is_active = elepairptr->Evaluate(
        &eleforce_centerlineDOFs[0],
        &eleforce_centerlineDOFs[1],
        &elestiff_centerlineDOFs[0][0],
        &elestiff_centerlineDOFs[0][1],
        &elestiff_centerlineDOFs[1][0],
        &elestiff_centerlineDOFs[1][1] );

    if (pair_is_active)
    {
      elegids[0] = elepairptr->Element1()->Id();
      elegids[1] = elepairptr->Element2()->Id();

      // assemble force vector and stiffness matrix affecting the centerline DoFs only
      // into element force vector and stiffness matrix ('all DoFs' format, as usual)
      BEAMINTERACTION::UTILS::AssembleCenterlineDofForceStiffIntoElementForceStiff(
          Discret(),
          elegids,
          eleforce_centerlineDOFs,
          elestiff_centerlineDOFs,
          &eleforce,
          &elestiff);


      // Fixme
      eleforce[0].Scale(-1.0);
      eleforce[1].Scale(-1.0);

      // assemble the contributions into force vector class variable
      // f_crosslink_np_ptr_, i.e. in the DOFs of the connected nodes
      BEAMINTERACTION::UTILS::FEAssembleEleForceStiffIntoSystemVectorMatrix( Discret(),
          elegids, eleforce, elestiff, BeamInteractionDataStatePtr()->GetMutableForceNp(),
          BeamInteractionDataStatePtr()->GetMutableStiff() );

    }

  }

  PrintActiveBeamContactSet( IO::cout.os(IO::verbose) );

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::UpdateStepState(
    const double& timefac_n)
{
  CheckInitSetup();

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::PreUpdateStepElement(
    bool beam_redist )
{
  CheckInitSetup();

  /* Fixme
   * writing vtk output needs to be done BEFORE updating (and thus clearing
   * element pairs)
   * move this to RuntimeOutputStepState as soon as we keep element pairs
   * from previous time step */
  /* Fixme
   * writing this output also must be done BEFORE re-distribution which
   * currently happens in STR::MODELEVALUATOR::BeamInteraction::UpdateStepElement()
   * before calling UpdateStepElement() on all submodels.
   * Hence, the only option currently is to call it from PreUpdateStepElement() */
  /* Note: another option would be to not use any data from state vectors or elements and only
   * write previously computed and (locally) stored data at this point. Like
   * this, it works in SUBMODELEVALUATOR::BeamPotential */
  if ( vtp_writer_ptr_ != Teuchos::null and
      GState().GetStepNp() %
      BeamContactParams().BeamContactRuntimeVtkOutputParams()->OutputIntervalInSteps() == 0 )
    WriteTimeStepOutputRuntimeVtpBeamContact();

  // not repartition of binning discretization necessary
  return false;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::UpdateStepElement(
    bool repartition_was_done )
{
  CheckInitSetup();

  PrintActiveBeamContactSet( IO::cout.os(IO::standard) );

  nearby_elements_map_.clear();
  FindAndStoreNeighboringElements();
  CreateBeamContactElementPairs();

}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::PostUpdateStepElement()
{
  CheckInitSetup();

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
std::map< STR::EnergyType, double >
BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::GetEnergy() const
{
  CheckInitSetup();

  std::map< STR::EnergyType, double > contact_penalty_potential;

  for ( auto& elepairptr : contact_elepairs_ )
  {
    contact_penalty_potential[STR::beam_contact_penalty_potential] += elepairptr->GetEnergy();
  }

  return contact_penalty_potential;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::OutputStepState(
    IO::DiscretizationWriter& iowriter) const
{

}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::RuntimeOutputStepState() const
{

}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::InitOutputRuntimeVtpBeamContact()
{
  CheckInit();

  vtp_writer_ptr_ = Teuchos::rcp( new RuntimeVtpWriter() );

  // Todo: we need a better upper bound for total number of time steps here
  // however, this 'only' affects the number of leading zeros in the vtk file names
  unsigned int num_timesteps_in_simulation_upper_bound = 1000000;

  if ( BeamContactParams().BeamContactRuntimeVtkOutputParams()->OutputEveryIteration() )
    num_timesteps_in_simulation_upper_bound *= 10000;

  // determine path of output directory
  const std::string outputfilename( DRT::Problem::Instance()->OutputControlFile()->FileName() );

  size_t pos = outputfilename.find_last_of("/");

  if (pos == outputfilename.npos)
    pos = 0ul;
  else
    pos++;

  const std::string output_directory_path( outputfilename.substr(0ul, pos) );


  // initialize the writer object
  vtp_writer_ptr_->Initialize(
      Discret().Comm().MyPID(),
      Discret().Comm().NumProc(),
      num_timesteps_in_simulation_upper_bound,
      output_directory_path,
      DRT::Problem::Instance()->OutputControlFile()->FileNameOnlyPrefix(),
      "beam-contact",
      DRT::Problem::Instance()->OutputControlFile()->RestartName(),
      GState().GetTimeN(),
      BeamContactParams().BeamContactRuntimeVtkOutputParams()->WriteBinaryOutput() );
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
WriteTimeStepOutputRuntimeVtpBeamContact() const
{
  CheckInitSetup();

  if ( not BeamContactParams().BeamContactRuntimeVtkOutputParams()->OutputEveryIteration() )
    WriteOutputRuntimeVtpBeamContact( GState().GetStepN(), GState().GetTimeN() );
  else
    WriteOutputRuntimeVtpBeamContact( 10000 * GState().GetStepN(), GState().GetTimeN() );
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
WriteIterationOutputRuntimeVtpBeamContact( int iteration_number ) const
{
  CheckInitSetup();

  const int augmented_timestep_number_incl_iteration_count =
      10000 * GState().GetStepN() +
      1 * iteration_number;

  const double augmented_time_incl_iteration_count =
      GState().GetTimeN() +
      1e-8 * iteration_number;

  WriteOutputRuntimeVtpBeamContact( augmented_timestep_number_incl_iteration_count,
      augmented_time_incl_iteration_count );
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::WriteOutputRuntimeVtpBeamContact(
    int timestep_number, double time ) const
{
  CheckInitSetup();

  const unsigned int num_spatial_dimensions = 3;

  // reset time and time step and geometry name in the writer object
  vtp_writer_ptr_->SetupForNewTimeStepAndGeometry( time, timestep_number, "beam-contact" );

  // number of active point contact point pairs * 2 = number of row points for writer object
  unsigned int num_row_points = 0;

  // loop over contact pairs and retrieve number of all active contact point pairs
  std::vector<Teuchos::RCP<BEAMINTERACTION::BeamContactPair> >::const_iterator pair_iter;
  for ( pair_iter=contact_elepairs_.begin(); pair_iter!=contact_elepairs_.end(); ++pair_iter )
  {
    num_row_points += 2 * (*pair_iter)->GetNumAllActiveContactPointPairs();
  }

  // get and prepare storage for point coordinate values
  std::vector<double>& point_coordinates = vtp_writer_ptr_->GetMutablePointCoordinateVector();
  point_coordinates.clear();
  point_coordinates.reserve( num_spatial_dimensions * num_row_points );

  // contact force values: collect data and append to visualization results if desired
  std::vector<double> contact_force_vector(0);
  contact_force_vector.reserve( num_spatial_dimensions * num_row_points );

  // gap values: collect data and append to visualization results if desired
  std::vector<double> gaps(0);
  gaps.reserve( num_row_points );

  // loop over my points and collect the geometry/grid data, i.e. contact points
  std::vector<LINALG::TMatrix<double,3,1> > coordinates_ele1_this_pair;
  std::vector<LINALG::TMatrix<double,3,1> > coordinates_ele2_this_pair;

  std::vector<double> contact_forces_this_pair;
  std::vector<double> gaps_this_pair;

  // loop over contact pairs and retrieve all active contact point coordinates
  for ( pair_iter=contact_elepairs_.begin(); pair_iter!=contact_elepairs_.end(); ++pair_iter )
  {
    if ( (*pair_iter)->GetContactFlag() == true )
    {
      // active contact points of element 1 and element 2
      (*pair_iter)->GetAllActiveContactPointCoordsElement1( coordinates_ele1_this_pair );
      (*pair_iter)->GetAllActiveContactPointCoordsElement2( coordinates_ele2_this_pair );
      (*pair_iter)->GetAllActiveContactForces( contact_forces_this_pair );
      (*pair_iter)->GetAllActiveContactGaps( gaps_this_pair );

      const unsigned int num_active_point_pairs =
          (unsigned int) coordinates_ele1_this_pair.size();

      dsassert( num_active_point_pairs == (unsigned int) coordinates_ele2_this_pair.size(),
          "number of active points on element 1 does not match number of active points "
          "on element 2!" );

      dsassert( num_active_point_pairs == (unsigned int) contact_forces_this_pair.size(),
          "number of active points on element 1 does not match number of contact forces!" );

      dsassert( num_active_point_pairs == (unsigned int) gaps_this_pair.size(),
          "number of active points on element 1 does not match number of gap values!" );


      for ( unsigned int ipointpair=0; ipointpair < num_active_point_pairs; ++ipointpair )
      {
        LINALG::TMatrix<double,3,1> normal_vector;
        normal_vector.Update(1.0, coordinates_ele1_this_pair[ipointpair],
            -1.0, coordinates_ele2_this_pair[ipointpair]);

        // contact point on first element
        for (unsigned int idim=0; idim<num_spatial_dimensions; ++idim)
        {
          point_coordinates.push_back( coordinates_ele1_this_pair[ipointpair](idim) );

          contact_force_vector.push_back( contact_forces_this_pair[ipointpair]
                                          * normal_vector(idim) );
        }
        gaps.push_back( gaps_this_pair[ipointpair] );

        // contact point on second element
        for (unsigned int idim=0; idim<num_spatial_dimensions; ++idim)
        {
          point_coordinates.push_back( coordinates_ele2_this_pair[ipointpair](idim) );

          contact_force_vector.push_back( -1.0 * contact_forces_this_pair[ipointpair]
                                          * normal_vector(idim) );
        }
        gaps.push_back( gaps_this_pair[ipointpair] );

      }

    }
  }


  // append all desired output data to the writer object's storage
  if ( BeamContactParams().BeamContactRuntimeVtkOutputParams()->IsWriteContactForces() )
  {
    vtp_writer_ptr_->AppendVisualizationPointDataVector( contact_force_vector,
        num_spatial_dimensions, "force" );
  }
  if ( BeamContactParams().BeamContactRuntimeVtkOutputParams()->IsWriteGaps() )
  {
    vtp_writer_ptr_->AppendVisualizationPointDataVector( gaps,
        1, "gap" );
  }

  // finalize everything and write all required vtk files to filesystem
  vtp_writer_ptr_->WriteFiles();


  // write a collection file summarizing all previously written files
  vtp_writer_ptr_->WriteCollectionFileOfAllWrittenFiles(
     DRT::Problem::Instance()->OutputControlFile()->FileNameOnlyPrefix() +
     "-beam-contact" );
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::ResetStepState()
{
  CheckInitSetup();

}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::WriteRestart(
    IO::DiscretizationWriter & ia_writer,
    IO::DiscretizationWriter & bin_writer) const
{
  // empty
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::PreReadRestart()
{
  // empty
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::ReadRestart(
    IO::DiscretizationReader & ia_reader,
    IO::DiscretizationReader & bin_reader)
{
  // empty
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::PostReadRestart()
{
  CheckInitSetup();
  nearby_elements_map_.clear();
  FindAndStoreNeighboringElements();
  CreateBeamContactElementPairs();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::RunPostIterate(
    const NOX::Solver::Generic& solver )
{
  CheckInitSetup();

  if ( vtp_writer_ptr_ != Teuchos::null and
      BeamContactParams().BeamContactRuntimeVtkOutputParams()->OutputEveryIteration() )
    WriteIterationOutputRuntimeVtpBeamContact( solver.getNumIterations() );
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::AddBinsToBinColMap(
    std::set< int >& colbins)
{
  CheckInitSetup();
  // nothing to do
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
    AddBinsWithRelevantContentForIaDiscretColMap( std::set< int >& colbins ) const
{
  CheckInitSetup();
  // nothing to do
}

/*-------------------------------------------------------------------------------*
 *-------------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
    GetHalfInteractionDistance( double & half_interaction_distance )
{
  CheckInitSetup();

  // todo: choose meaningful safety factor
  double safe_fac = 1.5;

  // loop over all beams to get largest interaction radius
  double locmax_ia_distance = 0.0;
  double curr_ia_distance = 0.0;
  int const numroweles = EleTypeMapExtractorPtr()->BeamMap()->NumMyElements();
  for( int rowele_i = 0; rowele_i < numroweles; ++rowele_i )
  {
    int const elegid = EleTypeMapExtractorPtr()->BeamMap()->GID(rowele_i);
    DRT::ELEMENTS::Beam3Base * currele =
        dynamic_cast< DRT::ELEMENTS::Beam3Base *>( DiscretPtr()->gElement(elegid) );

    curr_ia_distance = currele->GetCircularCrossSectionRadiusForInteractions();

    if ( curr_ia_distance > locmax_ia_distance )
      locmax_ia_distance = curr_ia_distance;
  }

  // get global maximum
  double globalmax_beam_ia_distance = 0.0;
  // build sum over all procs
  MPI_Allreduce( & locmax_ia_distance, & globalmax_beam_ia_distance, 1,
      MPI_DOUBLE, MPI_MAX, dynamic_cast<const Epetra_MpiComm*>( &(Discret().Comm() ) )->Comm() );

  // i) beam to beam contact
  if ( HaveContactType(BINSTRATEGY::UTILS::Beam) )
  {
    // safety factor
    globalmax_beam_ia_distance *= safe_fac;

    half_interaction_distance = ( globalmax_beam_ia_distance > half_interaction_distance ) ?
        globalmax_beam_ia_distance : half_interaction_distance;

    // some screen output
    if ( GState().GetMyRank() == 0 )
      std::cout << " beam to beam contact half interaction distance " << globalmax_beam_ia_distance << std::endl;
  }

  // ii) beam to sphere contact
  if ( HaveContactType(BINSTRATEGY::UTILS::RigidSphere) )
  {
    // loop over all spheres
    double curr_ia_dist = 0.0;
    double loc_max_ia_dist = 0.0;
    int unsigned const numrowsphereeles = EleTypeMapExtractor().SphereMap()->NumMyElements();
    for( unsigned int rowele_i = 0; rowele_i < numrowsphereeles; ++rowele_i )
    {
      int const elegid = EleTypeMapExtractor().SphereMap()->GID(rowele_i);
      DRT::ELEMENTS::Rigidsphere * sphere =
          dynamic_cast< DRT::ELEMENTS::Rigidsphere * >( Discret().gElement(elegid) );

      curr_ia_dist = sphere->Radius() + globalmax_beam_ia_distance;

      // update distance
      loc_max_ia_dist = ( curr_ia_dist > loc_max_ia_dist ) ? curr_ia_dist : loc_max_ia_dist;
    }

    // get global maximum
    double spherebeamlinking_half_interaction_distance_global = 0.0;
    // build sum over all procs
    MPI_Allreduce( &loc_max_ia_dist, &spherebeamlinking_half_interaction_distance_global, 1,
        MPI_DOUBLE, MPI_MAX, dynamic_cast<const Epetra_MpiComm*>( &(Discret().Comm() ) )->Comm() );

    half_interaction_distance = ( spherebeamlinking_half_interaction_distance_global > half_interaction_distance ) ?
        spherebeamlinking_half_interaction_distance_global : half_interaction_distance;

    // some screen output
    if ( GState().GetMyRank() == 0 )
      IO::cout(IO::verbose) << " sphere to beam contact half interaction distance "
          << spherebeamlinking_half_interaction_distance_global << IO::endl;
  }

  // iii) beam to solid contact
  if ( HaveContactType(BINSTRATEGY::UTILS::Solid) )
  {
    dserror("Not yet implemented for beam to solid contact");
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::HaveContactType(
    BINSTRATEGY::UTILS::BinContentType const & contacttype) const
{
  CheckInit();
  return ( std::find(contactelementtypes_.begin(), contactelementtypes_.end(), contacttype) != contactelementtypes_.end() );
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::FindAndStoreNeighboringElements()
{
  // measure time for evaluating this function
  TEUCHOS_FUNC_TIME_MONITOR("BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::FindAndStoreNeighboringElements");

  CheckInit();

  // loop over all row beam elements
  // note: like this we ensure that first element of pair is always a beam element, also only
  // only beam to something contact considered
  int const numroweles = EleTypeMapExtractorPtr()->BeamMap()->NumMyElements();
  for( int rowele_i = 0; rowele_i < numroweles; ++rowele_i )
  {
    int const elegid = EleTypeMapExtractorPtr()->BeamMap()->GID(rowele_i);
    DRT::Element* currele = DiscretPtr()->gElement(elegid);

    // (unique) set of neighboring bins for all col bins assigned to current element
    std::set<int> neighboring_binIds;

    // loop over all bins touched by currele
    std::set<int>::const_iterator biniter;
    for( biniter = BeamInteractionDataStatePtr()->GetRowEleToBinSet(elegid).begin();
        biniter != BeamInteractionDataStatePtr()->GetRowEleToBinSet(elegid).end(); ++biniter )
    {
      std::vector<int> loc_neighboring_binIds;
      loc_neighboring_binIds.reserve(27);

      // do not check on existence here -> shifted to GetBinContent
      BinStrategyPtr()->GetNeighborAndOwnBinIds( *biniter, loc_neighboring_binIds );

      // build up comprehensive unique set of neighboring bins
      neighboring_binIds.insert( loc_neighboring_binIds.begin(), loc_neighboring_binIds.end() );
    }
    // get set of elements that reside in neighboring bins
    std::vector<int> glob_neighboring_binIds( neighboring_binIds.begin(), neighboring_binIds.end() );
    std::set<DRT::Element*> neighboring_elements;
    BinStrategyPtr()->GetBinContent( neighboring_elements, contactelementtypes_, glob_neighboring_binIds );

    // sort out elements that should not be considered in contact evaluation
    SelectElesToBeConsideredForContactEvaluation( currele, neighboring_elements );

    nearby_elements_map_[elegid] = neighboring_elements;
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::SelectElesToBeConsideredForContactEvaluation(
    DRT::Element*            currele,
    std::set<DRT::Element*>& neighbors) const
{
  CheckInit();

  // sort out elements that should not be considered in contact evaluation
  std::set<DRT::Element*>::iterator eiter;
  for( eiter = neighbors.begin(); eiter != neighbors.end();)
  {
    bool toerase = false;
    // 1) ensure each contact only evaluated once (keep in mind that we are
    //    using FEMatrices and FEvectors -> || (*eiter)->Owner() != myrank not necessary)
    // note: as we are only looping over beam elements, only beam to beam contact needs id check here
    if ( dynamic_cast<DRT::ELEMENTS::Beam3Base*>(*eiter) != NULL and not ( currele->Id() < (*eiter)->Id() ) )
    {
      toerase = true;
    }
    // 2) ensure that two elements sharing the same node do not get into contact
    else
    {
      for ( int i = 0; i < 2; ++i )
        for ( int j = 0; j < 2 ; ++j )
          if ( (*eiter)->NodeIds()[i] == currele->NodeIds()[j] )
            toerase = true;
    }

    if ( toerase )
      neighbors.erase(eiter++);
    else
      ++eiter;
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
    CreateBeamContactElementPairs()
{

  // Todo maybe keep existing pairs and reuse them ?
  contact_elepairs_.clear();

  std::map<int, std::set<DRT::Element*> >::const_iterator nearbyeleiter;

  for ( nearbyeleiter = nearby_elements_map_.begin(); nearbyeleiter != nearby_elements_map_.end(); ++nearbyeleiter )
  {
    const int elegid = nearbyeleiter->first;
    std::vector< DRT::Element const *> ele_ptrs(2);
    ele_ptrs[0] = DiscretPtr()->gElement(elegid);

#ifdef DEBUG
    if ( dynamic_cast< DRT::ELEMENTS::Beam3Base const* >(ele_ptrs[0]) == NULL )
      dserror("first element of element pair must be a beam element");
#endif

    std::set<DRT::Element*>::const_iterator secondeleiter;
    for ( secondeleiter = nearbyeleiter->second.begin(); secondeleiter != nearbyeleiter->second.end(); ++secondeleiter )
    {
      ele_ptrs[1] = *secondeleiter;

      // construct, init and setup contact pairs
      Teuchos::RCP<BEAMINTERACTION::BeamContactPair> newbeaminteractionpair =
          BEAMINTERACTION::BeamContactPair::Create(ele_ptrs);
      newbeaminteractionpair->Init( beam_contact_evaluation_data_ptr_, beam_contact_params_ptr_, ele_ptrs );
      newbeaminteractionpair->Setup();

      // add to list of current contact pairs
      contact_elepairs_.push_back(newbeaminteractionpair);
    }
  }

  IO::cout(IO::standard) << "PID " << std::setw(2) << std::right << GState().GetMyRank()
      << " currently monitors " << std::setw(5) << std::right <<
      contact_elepairs_.size() << " beam contact pairs" << IO::endl;

}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
    PrintAllBeamContactElementPairs( std::ostream& out ) const
{
  out << "\n\nCurrent BeamContactElementPairs: ";
  std::vector<Teuchos::RCP<BEAMINTERACTION::BeamContactPair> >::const_iterator iter;
  for (iter=contact_elepairs_.begin(); iter!=contact_elepairs_.end(); ++iter)
    (*iter)->Print(out);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact::
    PrintActiveBeamContactSet( std::ostream& out ) const
{
  bool atleastoneactivepair = false;

  for ( auto& elepairptr : contact_elepairs_ )
    if ( elepairptr->GetContactFlag() == true )
      atleastoneactivepair = true;


  if ( atleastoneactivepair )
  {
    out << "\n    Active Beam-To-? Contact Set (PID " << GState().GetMyRank()
        << "):-----------------------------------------\n";
    out << "    ID1            ID2              T    xi       eta      angle    gap         force\n";


    for ( auto& elepairptr : contact_elepairs_ )
      elepairptr->PrintSummaryOneLinePerActiveSegmentPair(out);

    out << std::endl;
  }

}
