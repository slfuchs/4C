/*----------------------------------------------------------------------*/
/*! \file

\brief routine to control execution phase

\level 1


*----------------------------------------------------------------------*/

#include "4C_ale_dyn.hpp"
#include "4C_art_net_dyn_drt.hpp"
#include "4C_ehl_dyn.hpp"
#include "4C_elch_dyn.hpp"
#include "4C_elemag_dyn.hpp"
#include "4C_fluid_dyn_nln_drt.hpp"
#include "4C_fpsi_dyn.hpp"
#include "4C_fs3i_dyn.hpp"
#include "4C_fsi_dyn.hpp"
#include "4C_global_data.hpp"
#include "4C_immersed_problem_dyn.hpp"
#include "4C_levelset_dyn.hpp"
#include "4C_loma_dyn.hpp"
#include "4C_lubrication_dyn.hpp"
#include "4C_particle_algorithm_sim.hpp"
#include "4C_pasi_dyn.hpp"
#include "4C_poroelast_dyn.hpp"
#include "4C_poroelast_scatra_dyn.hpp"
#include "4C_porofluidmultiphase_dyn.hpp"
#include "4C_poromultiphase_dyn.hpp"
#include "4C_poromultiphase_scatra_dyn.hpp"
#include "4C_red_airways_dyn_drt.hpp"
#include "4C_scatra_cardiac_monodomain_dyn.hpp"
#include "4C_scatra_dyn.hpp"
#include "4C_ssi_dyn.hpp"
#include "4C_ssti_dyn.hpp"
#include "4C_sti_dyn.hpp"
#include "4C_stru_multi_microstatic_npsupport.hpp"
#include "4C_structure_dyn_nln_drt.hpp"
#include "4C_thermo_dyn.hpp"
#include "4C_tsi_dyn.hpp"
#include "4C_wear_dyn.hpp"

/*----------------------------------------------------------------------*
 |  routine to control execution phase                   m.gee 6/01     |
 *----------------------------------------------------------------------*/
void ntacal()
{
  using namespace FourC;

  int restart = GLOBAL::Problem::Instance()->Restart();

  // choose the entry-routine depending on the problem type
  switch (GLOBAL::Problem::Instance()->GetProblemType())
  {
    case GLOBAL::ProblemType::structure:
    case GLOBAL::ProblemType::polymernetwork:
      caldyn_drt();
      break;
    case GLOBAL::ProblemType::fluid:
    case GLOBAL::ProblemType::fluid_redmodels:
      dyn_fluid_drt(restart);
      break;
    case GLOBAL::ProblemType::lubrication:
      lubrication_dyn(restart);
      break;
    case GLOBAL::ProblemType::ehl:
      ehl_dyn();
      break;
    case GLOBAL::ProblemType::scatra:
      scatra_dyn(restart);
      break;
    case GLOBAL::ProblemType::cardiac_monodomain:
      scatra_cardiac_monodomain_dyn(restart);
      break;
    case GLOBAL::ProblemType::sti:
      sti_dyn(restart);
      break;
    case GLOBAL::ProblemType::fluid_xfem:
      fluid_xfem_drt();
      break;
      break;
    case GLOBAL::ProblemType::fluid_ale:
      fluid_ale_drt();
      break;
    case GLOBAL::ProblemType::freesurf:
      fluid_freesurf_drt();
      break;

    case GLOBAL::ProblemType::fsi:
    case GLOBAL::ProblemType::fsi_redmodels:
    case GLOBAL::ProblemType::fsi_lung:
      fsi_ale_drt();
      break;
    case GLOBAL::ProblemType::fsi_xfem:
      xfsi_drt();
      break;
    case GLOBAL::ProblemType::fpsi_xfem:
      xfpsi_drt();
      break;
    case GLOBAL::ProblemType::gas_fsi:
    case GLOBAL::ProblemType::ac_fsi:
    case GLOBAL::ProblemType::biofilm_fsi:
    case GLOBAL::ProblemType::thermo_fsi:
    case GLOBAL::ProblemType::fps3i:
      fs3i_dyn();
      break;
    case GLOBAL::ProblemType::fbi:
      fsi_immersed_drt();
      break;

    case GLOBAL::ProblemType::ale:
      dyn_ale_drt();
      break;

    case GLOBAL::ProblemType::thermo:
      thr_dyn_drt();
      break;

    case GLOBAL::ProblemType::tsi:
      tsi_dyn_drt();
      break;

    case GLOBAL::ProblemType::loma:
      loma_dyn(restart);
      break;

    case GLOBAL::ProblemType::elch:
      elch_dyn(restart);
      break;

    case GLOBAL::ProblemType::art_net:
      dyn_art_net_drt();
      break;

    case GLOBAL::ProblemType::red_airways:
      dyn_red_airways_drt();
      break;

    case GLOBAL::ProblemType::struct_ale:
      wear_dyn_drt(restart);
      break;

    case GLOBAL::ProblemType::immersed_fsi:
      immersed_problem_drt();
      break;

    case GLOBAL::ProblemType::poroelast:
      poroelast_drt();
      break;
    case GLOBAL::ProblemType::poroscatra:
      poro_scatra_drt();
      break;
    case GLOBAL::ProblemType::porofluidmultiphase:
      porofluidmultiphase_dyn(restart);
      break;
    case GLOBAL::ProblemType::poromultiphase:
      poromultiphase_dyn(restart);
      break;
    case GLOBAL::ProblemType::poromultiphasescatra:
      poromultiphasescatra_dyn(restart);
      break;
    case GLOBAL::ProblemType::fpsi:
      fpsi_drt();
      break;
    case GLOBAL::ProblemType::ssi:
      ssi_drt();
      break;
    case GLOBAL::ProblemType::ssti:
      ssti_drt();
      break;
    case GLOBAL::ProblemType::redairways_tissue:
      redairway_tissue_dyn();
      break;

    case GLOBAL::ProblemType::particle:
      particle_drt();
      break;

    case GLOBAL::ProblemType::pasi:
      pasi_dyn();
      break;

    case GLOBAL::ProblemType::level_set:
      levelset_dyn(restart);
      break;

    case GLOBAL::ProblemType::np_support:
      STRUMULTI::np_support_drt();
      break;

    case GLOBAL::ProblemType::elemag:
      electromagnetics_drt();
      break;

    default:
      FOUR_C_THROW("solution of unknown problemtyp %d requested",
          GLOBAL::Problem::Instance()->GetProblemType());
      break;
  }
}
