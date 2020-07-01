/*----------------------------------------------------------------------*/
/*! \file
\brief time-integration scheme with extensions for
       cardiac monodomain problems

\level 2


*/
/*----------------------------------------------------------------------*/

#include "scatra_timint_cardiac_monodomain_scheme.H"
#include "../drt_scatra_ele/scatra_ele_action.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_io/io.H"


/*----------------------------------------------------------------------*
 |  Constructor (public)                                     ljag 01/14 |
 *----------------------------------------------------------------------*/
SCATRA::TimIntCardiacMonodomainOST::TimIntCardiacMonodomainOST(
    Teuchos::RCP<DRT::Discretization> actdis, Teuchos::RCP<LINALG::Solver> solver,
    Teuchos::RCP<Teuchos::ParameterList> params,
    Teuchos::RCP<Teuchos::ParameterList> sctratimintparams,
    Teuchos::RCP<Teuchos::ParameterList> extraparams, Teuchos::RCP<IO::DiscretizationWriter> output)
    : ScaTraTimIntImpl(actdis, solver, sctratimintparams, extraparams, output),
      TimIntCardiacMonodomain(actdis, solver, params, sctratimintparams, extraparams, output),
      TimIntOneStepTheta(actdis, solver, sctratimintparams, extraparams, output)
{
  return;
}

/*----------------------------------------------------------------------*
| Destructor dtor (public)                                   ehrl 01/14 |
*-----------------------------------------------------------------------*/
SCATRA::TimIntCardiacMonodomainOST::~TimIntCardiacMonodomainOST() { return; }

/*----------------------------------------------------------------------*
 |  initialize time integration                              ehrl 01/14 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainOST::Setup()
{
  // call Setup()-functions of base classes
  // note: this order is important
  TimIntOneStepTheta::Setup();

  TimIntCardiacMonodomain::Setup();

  return;
}

/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next timestep       |
 |                                                            gjb 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainOST::Update(const int num)
{
  // Standard Update
  TimIntOneStepTheta::Update(num);

  // time update of myocard material
  TimIntCardiacMonodomain::ElementMaterialTimeUpdate();

  return;
}

/*----------------------------------------------------------------------*
 | write additional data required for restart                 gjb 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainOST::OutputRestart() const
{
  // Call function from baseclass
  TimIntOneStepTheta::OutputRestart();

  // Cardiac Monodomain specific
  output_->WriteMesh(
      step_, time_);  // add info to control file for reading all variables in restart

  return;
}

/*----------------------------------------------------------------------*
 |                                                            gjb 08/08 |
 -----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainOST::ReadRestart(
    const int step, Teuchos::RCP<IO::InputControl> input)
{
  // Call function from baseclass
  TimIntOneStepTheta::ReadRestart(step, input);

  Teuchos::RCP<IO::DiscretizationReader> reader(Teuchos::null);
  if (input == Teuchos::null)
    reader = Teuchos::rcp(new IO::DiscretizationReader(discret_, step));
  else
    reader = Teuchos::rcp(new IO::DiscretizationReader(discret_, input, step));

  // Cardiac Monodomain specific
  reader->ReadVector(activation_time_np_, "activation_time_np");
  reader->ReadHistoryData(step);  // Read all saved data in nodes and elements und call nodal and
                                  // element Unpacking each global variable has to be read

  return;
}

/*--------------------------------------------------------------------------*
 | add global state vectors specific for time-integration scheme  hoe 06/16 |
 *--------------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainOST::AddTimeIntegrationSpecificVectors(
    bool forcedincrementalsolver)
{
  // Call function from baseclass
  TimIntOneStepTheta::AddTimeIntegrationSpecificVectors(forcedincrementalsolver);
  discret_->SetState("phin", phin_);

  return;
}

/*----------------------------------------------------------------------*
 |  Constructor (public)                                     ljag 01/14 |
 *----------------------------------------------------------------------*/
SCATRA::TimIntCardiacMonodomainBDF2::TimIntCardiacMonodomainBDF2(
    Teuchos::RCP<DRT::Discretization> actdis, Teuchos::RCP<LINALG::Solver> solver,
    Teuchos::RCP<Teuchos::ParameterList> params,
    Teuchos::RCP<Teuchos::ParameterList> sctratimintparams,
    Teuchos::RCP<Teuchos::ParameterList> extraparams, Teuchos::RCP<IO::DiscretizationWriter> output)
    : ScaTraTimIntImpl(actdis, solver, sctratimintparams, extraparams, output),
      TimIntCardiacMonodomain(actdis, solver, params, sctratimintparams, extraparams, output),
      TimIntBDF2(actdis, solver, sctratimintparams, extraparams, output)
{
  return;
}

/*----------------------------------------------------------------------*
| Destructor dtor (public)                                   ehrl 01/14 |
*-----------------------------------------------------------------------*/
SCATRA::TimIntCardiacMonodomainBDF2::~TimIntCardiacMonodomainBDF2() { return; }

/*----------------------------------------------------------------------*
 |  initialize time integration                              ehrl 01/14 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainBDF2::Setup()
{
  // call Setup()-functions of base classes
  // note: this order is important
  TimIntBDF2::Setup();

  TimIntCardiacMonodomain::Setup();

  return;
}

/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next timestep       |
 |                                                            gjb 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainBDF2::Update(const int num)
{
  // Standard Update
  TimIntBDF2::Update(num);

  // time update of myocard material
  TimIntCardiacMonodomain::ElementMaterialTimeUpdate();

  return;
}

/*----------------------------------------------------------------------*
 | write additional data required for restart                 gjb 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainBDF2::OutputRestart() const
{
  // Call function from baseclass
  TimIntBDF2::OutputRestart();

  // Cardiac Monodomain specific
  output_->WriteMesh(
      step_, time_);  // add info to control file for reading all variables in restart

  return;
}


/*----------------------------------------------------------------------*
 |                                                            gjb 08/08 |
 -----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainBDF2::ReadRestart(
    const int step, Teuchos::RCP<IO::InputControl> input)
{
  // Call function from baseclass
  TimIntBDF2::ReadRestart(step, input);

  Teuchos::RCP<IO::DiscretizationReader> reader(Teuchos::null);
  if (input == Teuchos::null)
    reader = Teuchos::rcp(new IO::DiscretizationReader(discret_, step));
  else
    reader = Teuchos::rcp(new IO::DiscretizationReader(discret_, input, step));

  // Cardiac Monodomain specific
  reader->ReadVector(activation_time_np_, "activation_time_np");
  reader->ReadHistoryData(step);  // Read all saved data in nodes and elements und call nodal and
                                  // element Unpacking each global variable has to be read

  return;
}


/*----------------------------------------------------------------------*
 |  Constructor (public)                                     ljag 01/14 |
 *----------------------------------------------------------------------*/
SCATRA::TimIntCardiacMonodomainGenAlpha::TimIntCardiacMonodomainGenAlpha(
    Teuchos::RCP<DRT::Discretization> actdis, Teuchos::RCP<LINALG::Solver> solver,
    Teuchos::RCP<Teuchos::ParameterList> params,
    Teuchos::RCP<Teuchos::ParameterList> sctratimintparams,
    Teuchos::RCP<Teuchos::ParameterList> extraparams, Teuchos::RCP<IO::DiscretizationWriter> output)
    : ScaTraTimIntImpl(actdis, solver, sctratimintparams, extraparams, output),
      TimIntCardiacMonodomain(actdis, solver, params, sctratimintparams, extraparams, output),
      TimIntGenAlpha(actdis, solver, sctratimintparams, extraparams, output)
{
  return;
}

/*----------------------------------------------------------------------*
| Destructor dtor (public)                                   ehrl 01/14 |
*-----------------------------------------------------------------------*/
SCATRA::TimIntCardiacMonodomainGenAlpha::~TimIntCardiacMonodomainGenAlpha() { return; }

/*----------------------------------------------------------------------*
 |  initialize time integration                              ehrl 01/14 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainGenAlpha::Setup()
{
  // call Init()-functions of base classes
  // note: this order is important
  TimIntGenAlpha::Setup();

  TimIntCardiacMonodomain::Setup();

  return;
}

/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next timestep       |
 |                                                            gjb 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainGenAlpha::Update(const int num)
{
  // Standard Update
  TimIntGenAlpha::Update(num);

  // time update of myocard material
  TimIntCardiacMonodomain::ElementMaterialTimeUpdate();

  return;
}

/*----------------------------------------------------------------------*
 | write additional data required for restart                 gjb 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainGenAlpha::OutputRestart() const
{
  // Call function from baseclass
  TimIntGenAlpha::OutputRestart();

  // Cardiac Monodomain specific
  output_->WriteMesh(
      step_, time_);  // add info to control file for reading all variables in restart

  return;
}


/*----------------------------------------------------------------------*
 |                                                            gjb 08/08 |
 -----------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainGenAlpha::ReadRestart(
    const int step, Teuchos::RCP<IO::InputControl> input)
{
  // Call function from baseclass
  TimIntGenAlpha::ReadRestart(step, input);

  IO::DiscretizationReader reader(discret_, step);

  // Cardiac Monodomain specific
  reader.ReadVector(activation_time_np_, "activation_time_np");
  reader.ReadHistoryData(step);  // Read all saved data in nodes and elements und call nodal and
                                 // element Unpacking each global variable has to be read

  return;
}

/*--------------------------------------------------------------------------*
 | add global state vectors specific for time-integration scheme  hoe 12/16 |
 *--------------------------------------------------------------------------*/
void SCATRA::TimIntCardiacMonodomainGenAlpha::AddTimeIntegrationSpecificVectors(
    bool forcedincrementalsolver)
{
  // Call function from baseclass
  TimIntGenAlpha::AddTimeIntegrationSpecificVectors(forcedincrementalsolver);

  if (incremental_ or forcedincrementalsolver) discret_->SetState("phin", phin_);

  return;
}
