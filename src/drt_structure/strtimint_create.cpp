/*----------------------------------------------------------------------*/
/*!
\file strtimint_create.cpp
\brief Creation of structural time integrators in accordance with user's wishes

<pre>
Maintainer: Alexander Popp
            popp@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/

/*----------------------------------------------------------------------*/
/* headers */
#include <ctime>
#include <cstdlib>
#include <iostream>

#include <Teuchos_StandardParameterEntryValidators.hpp>

#include "strtimint_create.H"
#include "strtimint_statics.H"
#include "strtimint_prestress.H"
#include "strtimint_genalpha.H"
#include "strtimint_ost.H"
#include "strtimint_gemm.H"
#include "strtimint_expleuler.H"
#include "strtimint_centrdiff.H"
#include "../drt_particle/particle_timint_centrdiff.H"
#include "strtimint_ab2.H"
#include "strtimint_statmech.H"

#include "../drt_io/io.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_inpar/drt_validparameters.H"
#include "../linalg/linalg_utils.H"


/*======================================================================*/
/* create marching time integrator */
Teuchos::RCP<STR::TimInt> STR::TimIntCreate
(
  const Teuchos::ParameterList& ioflags,
  const Teuchos::ParameterList& sdyn,
  const Teuchos::ParameterList& xparams,
  Teuchos::RCP<DRT::Discretization>& actdis,
  Teuchos::RCP<LINALG::Solver>& solver,
  Teuchos::RCP<LINALG::Solver>& contactsolver,
  Teuchos::RCP<IO::DiscretizationWriter>& output
)
{

  // set default output
  Teuchos::RCP<STR::TimInt> sti = Teuchos::null;
  // try implicit integrators
  sti = TimIntImplCreate(ioflags, sdyn, xparams, actdis, solver, contactsolver, output);
  // if nothing found try explicit integrators
  if (sti == Teuchos::null)
  {
    sti = TimIntExplCreate(ioflags, sdyn, xparams, actdis, solver, contactsolver, output);
  }
  
  // deliver
  return sti;
}

/*======================================================================*/
/* create implicit marching time integrator */
Teuchos::RCP<STR::TimIntImpl> STR::TimIntImplCreate
(
  const Teuchos::ParameterList& ioflags,
  const Teuchos::ParameterList& sdyn,
  const Teuchos::ParameterList& xparams,
  Teuchos::RCP<DRT::Discretization>& actdis,
  Teuchos::RCP<LINALG::Solver>& solver,
  Teuchos::RCP<LINALG::Solver>& contactsolver,
  Teuchos::RCP<IO::DiscretizationWriter>& output
)
{
  Teuchos::RCP<STR::TimIntImpl> sti = Teuchos::null;

  // TODO: add contact solver...

  // check if we have a problem that needs to be prestressed
  INPAR::STR::PreStress pstype = DRT::INPUT::IntegralValue<INPAR::STR::PreStress>(sdyn,"PRESTRESS");
  if (pstype==INPAR::STR::prestress_mulf || pstype==INPAR::STR::prestress_id)
  {
    sti = Teuchos::rcp(new STR::TimIntPrestress(ioflags, sdyn, xparams, actdis,
						solver, contactsolver, output));
    return sti;
  }
  
  // create specific time integrator
  switch (DRT::INPUT::IntegralValue<INPAR::STR::DynamicType>(sdyn, "DYNAMICTYP"))
  {
    // Static analysis
    case INPAR::STR::dyna_statics :
    {
      sti = Teuchos::rcp(new STR::TimIntStatics(ioflags, sdyn, xparams,
                                                actdis, solver, contactsolver, output));
      break;
    }

    // Generalised-alpha time integration
    case INPAR::STR::dyna_genalpha :
    {
      sti = Teuchos::rcp(new STR::TimIntGenAlpha(ioflags, sdyn, xparams,
                                                 actdis, solver, contactsolver, output));
      break;
    }

    // One-step-theta (OST) time integration
    case INPAR::STR::dyna_onesteptheta :
    {
      sti = Teuchos::rcp(new STR::TimIntOneStepTheta(ioflags, sdyn, xparams,
                                                     actdis, solver, contactsolver, output));
      break;
    }

    // Generalised energy-momentum method (GEMM)
    case INPAR::STR::dyna_gemm :
    {
      sti = Teuchos::rcp(new STR::TimIntGEMM(ioflags, sdyn, xparams,
                                             actdis, solver, contactsolver, output));
      break;
    }

    // Statistical Mechanics Time Integration
    case INPAR::STR::dyna_statmech :
    {
      sti = Teuchos::rcp(new STR::TimIntStatMech(ioflags, sdyn, xparams,
                                                 actdis, solver, contactsolver, output));
      break;
    }

    // Everything else
    default :
    {
      // do nothing
      break;
    }
  } // end of switch(sdyn->Typ)

  // return the integrator
  return sti;
}

/*======================================================================*/
/* create explicit marching time integrator */
Teuchos::RCP<STR::TimIntExpl> STR::TimIntExplCreate
(
  const Teuchos::ParameterList& ioflags,
  const Teuchos::ParameterList& sdyn,
  const Teuchos::ParameterList& xparams,
  Teuchos::RCP<DRT::Discretization>& actdis,
  Teuchos::RCP<LINALG::Solver>& solver,
  Teuchos::RCP<LINALG::Solver>& contactsolver,
  Teuchos::RCP<IO::DiscretizationWriter>& output
)
{
  Teuchos::RCP<STR::TimIntExpl> sti = Teuchos::null;

  // what's the current problem type?
  PROBLEM_TYP probtype = DRT::Problem::Instance()->ProblemType();

  if (probtype == prb_fsi or
      probtype == prb_fsi_redmodels or
      probtype == prb_fsi_lung or
      probtype == prb_gas_fsi or
      probtype == prb_biofilm_fsi or
      probtype == prb_thermo_fsi or
      probtype == prb_fluid_fluid_fsi)
  {
    dserror("no explicit time integration with fsi");
  }

  // create specific time integrator
  switch (DRT::INPUT::IntegralValue<INPAR::STR::DynamicType>(sdyn, "DYNAMICTYP"))
  {
    // forward Euler time integration
    case INPAR::STR::dyna_expleuler :
    {
      sti = Teuchos::rcp(new STR::TimIntExplEuler(ioflags, sdyn, xparams,
                                            actdis, solver, contactsolver, output));
      break;
    }
    // central differences time integration
    case INPAR::STR::dyna_centrdiff:
    {
      sti = Teuchos::rcp(new STR::TimIntCentrDiff(ioflags, sdyn, xparams,
                                            actdis, solver, contactsolver, output));
      break;
    }
    // Adams-Bashforth 2nd order (AB2) time integration
    case INPAR::STR::dyna_ab2 :
    {
      sti = Teuchos::rcp(new STR::TimIntAB2(ioflags, sdyn, xparams,
                                            actdis, solver, contactsolver, output));
      break;
    }
    // central differences time integration for particles
    case INPAR::STR::dyna_particle_centrdiff:
    {
      sti = Teuchos::rcp(new PARTICLE::TimIntCentrDiff(ioflags, sdyn, xparams,
                                            actdis, solver, contactsolver, output));
      break;
    }

    // Everything else
    default :
    {
      // do nothing
      break;
    }
  } // end of switch(sdyn->Typ)

  // return the integrator
  return sti;
}

/*----------------------------------------------------------------------*/
