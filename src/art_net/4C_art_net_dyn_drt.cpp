/*----------------------------------------------------------------------*/
/*! \file
\brief Main control routine for all arterial network  solvers,

     including instationary solvers based on

     o


\level 3

*----------------------------------------------------------------------*/

#include "4C_art_net_dyn_drt.hpp"

#include "4C_adapter_art_net.hpp"
#include "4C_art_net_artery_resulttest.hpp"
#include "4C_art_net_utils.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_validparameters.hpp"
#include "4C_io_control.hpp"
#include "4C_lib_resulttest.hpp"
#include "4C_lib_utils_createdis.hpp"

#include <Teuchos_StandardParameterEntryValidators.hpp>
#include <Teuchos_TimeMonitor.hpp>

#include <cstdlib>
#include <ctime>
#include <iostream>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 * Main control routine for arterial network including various solvers:
 *
 *        o
 *
 *----------------------------------------------------------------------*/
void dyn_art_net_drt() { dyn_art_net_drt(false); }

Teuchos::RCP<ADAPTER::ArtNet> dyn_art_net_drt(bool CoupledTo3D)
{
  if (GLOBAL::Problem::Instance()->DoesExistDis("artery") == false)
  {
    return Teuchos::null;
  }

  // define the discretization names
  const std::string artery_disname = "artery";
  const std::string scatra_disname = "artery_scatra";

  // access the problem
  GLOBAL::Problem* problem = GLOBAL::Problem::Instance();

  // -------------------------------------------------------------------
  // access the discretization
  // -------------------------------------------------------------------
  Teuchos::RCP<DRT::Discretization> actdis = Teuchos::null;

  actdis = problem->GetDis(artery_disname);

  // -------------------------------------------------------------------
  // set degrees of freedom in the discretization
  // -------------------------------------------------------------------
  if (!actdis->Filled()) actdis->FillComplete();

  // -------------------------------------------------------------------
  // If discretization is empty, then return empty time integration
  // -------------------------------------------------------------------
  if (actdis->NumGlobalElements() < 1)
  {
    return Teuchos::null;
  }

  // -------------------------------------------------------------------
  // context for output and restart
  // -------------------------------------------------------------------
  Teuchos::RCP<IO::DiscretizationWriter> output = actdis->Writer();
  output->WriteMesh(0, 0.0);

  // -------------------------------------------------------------------
  // set some pointers and variables
  // -------------------------------------------------------------------
  const Teuchos::ParameterList& artdyn = problem->ArterialDynamicParams();

  if (actdis->Comm().MyPID() == 0) INPUT::PrintDefaultParameters(IO::cout, artdyn);

  // -------------------------------------------------------------------
  // create a solver
  // -------------------------------------------------------------------
  // get the solver number
  const int linsolvernumber = artdyn.get<int>("LINEAR_SOLVER");
  // check if the solver has a valid solver number
  if (linsolvernumber == (-1))
    FOUR_C_THROW(
        "no linear solver defined. Please set LINEAR_SOLVER in ARTERIAL DYNAMIC to a valid "
        "number!");

  // solution output
  if (artdyn.get<std::string>("SOLVESCATRA") == "yes")
  {
    if (actdis->Comm().MyPID() == 0)
    {
      std::cout << "<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>" << std::endl;
      std::cout << "<  ARTERY:  ScaTra coupling present  >" << std::endl;
      std::cout << "<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>" << std::endl;
    }
    Teuchos::RCP<DRT::Discretization> scatradis = problem->GetDis(scatra_disname);
    // fill scatra discretization by cloning artery discretization
    DRT::UTILS::CloneDiscretization<ART::ArteryScatraCloneStrategy>(actdis, scatradis);
    scatradis->FillComplete();

    // the problem is one way coupled, scatra needs only artery

    // build a proxy of the structure discretization for the scatra field
    Teuchos::RCP<DRT::DofSetInterface> arterydofset = actdis->GetDofSetProxy();

    // check if ScatraField has 2 discretizations, so that coupling is possible
    if (scatradis->AddDofSet(arterydofset) != 1)
      FOUR_C_THROW("unexpected dof sets in scatra field");

    scatradis->FillComplete(true, false, false);
  }
  else
  {
    if (actdis->Comm().MyPID() == 0)
    {
      std::cout << "<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>" << std::endl;
      std::cout << "<  ARTERY: no ScaTra coupling present  >" << std::endl;
      std::cout << "<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>" << std::endl;
    }
  }

  // flag for writing the hemodynamic physiological results
  // arterytimeparams.set ("write stresses"
  // ,CORE::UTILS::IntegralValue<int>(ioflags,"HEMO_PHYS_RESULTS"));
  //---------------------- A method to initialize the flow inside the
  //                       arteries.
  //  int init = CORE::UTILS::IntegralValue<int> (artdyn,"INITIALFIELD");

  // -------------------------------------------------------------------
  // algorithm construction depending on
  // time-integration (or stationary) scheme
  // -------------------------------------------------------------------
  INPAR::ARTDYN::TimeIntegrationScheme timintscheme =
      CORE::UTILS::IntegralValue<INPAR::ARTDYN::TimeIntegrationScheme>(artdyn, "DYNAMICTYP");

  // build art net time integrator
  Teuchos::RCP<ADAPTER::ArtNet> artnettimint =
      ART::UTILS::CreateAlgorithm(timintscheme, actdis, linsolvernumber, artdyn, artdyn, output);

  // initialize
  artnettimint->Init(artdyn, artdyn, scatra_disname);

  // Initialize state save vectors
  if (CoupledTo3D)
  {
    artnettimint->InitSaveState();
  }

  // initial field from restart or calculated by given function
  const int restart = GLOBAL::Problem::Instance()->Restart();
  if (restart && !CoupledTo3D)
  {
    // read the restart information, set vectors and variables
    artnettimint->ReadRestart(restart);
  }
  else
  {
    // artnetexplicit.SetInitialData(init,startfuncno);
  }

  // assign materials
  // note: to be done after potential restart, as in ReadRestart()
  //       the secondary material is destroyed
  if (artdyn.get<std::string>("SOLVESCATRA") == "yes")
    ART::UTILS::AssignMaterialPointers(artery_disname, scatra_disname);

  if (!CoupledTo3D)
  {
    // call time-integration (or stationary) scheme
    Teuchos::RCP<Teuchos::ParameterList> param_temp;
    artnettimint->Integrate(CoupledTo3D, param_temp);

    // result test
    artnettimint->TestResults();

    return artnettimint;
    //    return  Teuchos::null;
  }
  else
  {
    return artnettimint;
  }

}  // end of dyn_art_net_drt()

FOUR_C_NAMESPACE_CLOSE
