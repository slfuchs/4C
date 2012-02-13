/*!----------------------------------------------------------------------
\file so_hex8_evaluate.cpp
\brief

<pre>
Maintainer: Moritz Frenzel
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15240
</pre>

*----------------------------------------------------------------------*/
#ifdef D_SOLID3
#ifdef CCADISCRET

#include "so_hex8.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_timecurve.H"
#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_serialdensevector.H"
#include "Epetra_SerialDenseSolver.h"
#include "../drt_mat/visconeohooke.H"
#include "../drt_mat/aaaneohooke_stopro.H"
#include "../drt_mat/viscoanisotropic.H"
#include "../drt_mat/aaaraghavanvorp_damage.H"
#include "../drt_mat/plasticneohooke.H"
#include "../drt_mat/plasticlinelast.H"
#include "../drt_mat/thermoplasticlinelast.H"
#include "../drt_mat/robinson.H"
#include "../drt_mat/holzapfelcardiovascular.H"
#include "../drt_mat/humphreycardiovascular.H"
#include "../drt_mat/growth_ip.H"
#include "../drt_mat/constraintmixture.H"
#include "../drt_mat/micromaterial.H"
#include "../drt_mortar/mortar_analytical.H"
#include "../drt_potential/drt_potential_manager.H"
#include "../drt_patspec/patspec.H"
#include "../drt_lib/drt_globalproblem.H"

#include <Teuchos_StandardParameterEntryValidators.hpp>

// inverse design object
#include "inversedesign.H"

using namespace std;
using namespace LINALG; // our linear algebra
using POTENTIAL::PotentialManager; // potential manager


/*----------------------------------------------------------------------*
 |  evaluate the element (public)                              maf 04/07|
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::So_hex8::Evaluate(ParameterList&           params,
                                    DRT::Discretization&      discretization,
                                    vector<int>&              lm,
                                    Epetra_SerialDenseMatrix& elemat1_epetra,
                                    Epetra_SerialDenseMatrix& elemat2_epetra,
                                    Epetra_SerialDenseVector& elevec1_epetra,
                                    Epetra_SerialDenseVector& elevec2_epetra,
                                    Epetra_SerialDenseVector& elevec3_epetra)
{
  LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8> elemat1(elemat1_epetra.A(),true);
  LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8> elemat2(elemat2_epetra.A(),true);
  LINALG::Matrix<NUMDOF_SOH8,1> elevec1(elevec1_epetra.A(),true);
  LINALG::Matrix<NUMDOF_SOH8,1> elevec2(elevec2_epetra.A(),true);
  // elevec3 is not used anyway

  // start with "none"
  DRT::ELEMENTS::So_hex8::ActionType act = So_hex8::none;

  // get the required action
  string action = params.get<string>("action","none");
  if (action == "none") dserror("No action supplied");
  else if (action=="calc_struct_linstiff")                        act = So_hex8::calc_struct_linstiff;
  else if (action=="calc_struct_nlnstiff")                        act = So_hex8::calc_struct_nlnstiff;
  else if (action=="calc_struct_internalforce")                   act = So_hex8::calc_struct_internalforce;
  else if (action=="calc_struct_linstiffmass")                    act = So_hex8::calc_struct_linstiffmass;
  else if (action=="calc_struct_nlnstiffmass")                    act = So_hex8::calc_struct_nlnstiffmass;
  else if (action=="calc_struct_nlnstifflmass")                   act = So_hex8::calc_struct_nlnstifflmass;
  else if (action=="calc_struct_nlnstiff_gemm")                   act = So_hex8::calc_struct_nlnstiff_gemm;
  else if (action=="calc_struct_stress")                          act = So_hex8::calc_struct_stress;
  else if (action=="calc_struct_eleload")                         act = So_hex8::calc_struct_eleload;
  else if (action=="calc_struct_fsiload")                         act = So_hex8::calc_struct_fsiload;
  else if (action=="calc_struct_update_istep")                    act = So_hex8::calc_struct_update_istep;
  else if (action=="calc_struct_update_imrlike")                  act = So_hex8::calc_struct_update_imrlike;
  else if (action=="calc_struct_reset_istep")                     act = So_hex8::calc_struct_reset_istep;
  else if (action=="calc_struct_reset_discretization")            act = So_hex8::calc_struct_reset_discretization;
  else if (action=="calc_struct_energy")                          act = So_hex8::calc_struct_energy;
  else if (action=="calc_struct_errornorms")                      act = So_hex8::calc_struct_errornorms;
  else if (action=="multi_eas_init")                              act = So_hex8::multi_eas_init;
  else if (action=="multi_eas_set")                               act = So_hex8::multi_eas_set;
  else if (action=="multi_readrestart")                           act = So_hex8::multi_readrestart;
  else if (action=="multi_calc_dens")                             act = So_hex8::multi_calc_dens;
  else if (action=="postprocess_stress")                          act = So_hex8::postprocess_stress;
  else if (action=="calc_potential_stiff")                        act = So_hex8::calc_potential_stiff;
  else if (action=="calc_struct_prestress_update")                act = So_hex8::prestress_update;
  else if (action=="calc_struct_inversedesign_update")            act = So_hex8::inversedesign_update;
  else if (action=="calc_struct_inversedesign_switch")            act = So_hex8::inversedesign_switch;
  else if (action=="calc_global_gpstresses_map")                  act = So_hex8::calc_global_gpstresses_map;
  else if (action=="calc_poroelast_nlnstiff")                     act = So_hex8::calc_poroelast_nlnstiff;
  else dserror("Unknown type of action for So_hex8");

  // check for patient specific data
  PATSPEC::GetILTDistance(Id(),params,discretization);
  PATSPEC::GetLocalRadius(Id(),params,discretization);

  // what should the element do
  switch(act)
  {
    //==================================================================================
    // linear stiffness
    case calc_struct_linstiff:
    {
      // need current displacement and residual forces
      vector<double> mydisp(lm.size());
      for (unsigned i=0; i<mydisp.size(); ++i) mydisp[i] = 0.0;
      vector<double> myres(lm.size());
      for (unsigned i=0; i<myres.size(); ++i) myres[i] = 0.0;
      soh8_nlnstiffmass(lm,mydisp,myres,&elemat1,NULL,&elevec1,NULL,NULL,NULL,params,
                        INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
    }
    break;

    //==================================================================================
    // nonlinear stiffness and internal force vector
    case calc_struct_nlnstiff:
    {
      // need current displacement and residual forces
      RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      if (disp==null || res==null) dserror("Cannot get state vectors 'displacement' and/or residual");
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);
      LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* matptr = NULL;
      if (elemat1.IsInitialized()) matptr = &elemat1;

      // default: geometrically non-linear analysis with Total Lagrangean approach
      if (kintype_ == DRT::ELEMENTS::So_hex8::soh8_totlag)
      {
        if (pstype_==INPAR::STR::prestress_id && time_ <= pstime_) // inverse design analysis
          invdesign_->soh8_nlnstiffmass(this,lm,mydisp,myres,matptr,NULL,&elevec1,NULL,NULL,params,
                                        INPAR::STR::stress_none,INPAR::STR::strain_none);

        else // standard analysis
          soh8_nlnstiffmass(lm,mydisp,myres,matptr,NULL,&elevec1,NULL,NULL,NULL,params,
                            INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }
      // special case: geometric linear
      else  // (kintype_ == DRT::ELEMENTS::So_hex8::soh8_geolin)
      {
        soh8_linstiffmass(lm,mydisp,myres,NULL,matptr,NULL,&elevec1,NULL,NULL,NULL,params,
                          INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }

    }
    break;

    //==================================================================================
    // internal force vector only
    case calc_struct_internalforce:
    {
      // need current displacement and residual forces
      RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      if (disp==null || res==null) dserror("Cannot get state vectors 'displacement' and/or residual");
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);
      // create a dummy element matrix to apply linearised EAS-stuff onto
      LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8> myemat(true);

      // default: geometrically non-linear analysis with Total Lagrangean approach
      if (kintype_ == DRT::ELEMENTS::So_hex8::soh8_totlag)
      {
        soh8_nlnstiffmass(lm,mydisp,myres,&myemat,NULL,&elevec1,NULL,NULL,NULL,params,
                        INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }
      // special case: geometric linear
      else  // (kintype_ == DRT::ELEMENTS::So_hex8::soh8_geolin)
      {
        soh8_linstiffmass(lm,mydisp,myres,NULL,&myemat,NULL,&elevec1,NULL,NULL,NULL,params,
          INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }

    }
    break;

    //==================================================================================
    // linear stiffness and consistent mass matrix
    case calc_struct_linstiffmass:
    {
      // need current displacement and residual forces
      RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      if (disp==null || res==null) dserror("Cannot get state vectors 'displacement' and/or residual");
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);

      // standard analysis
      soh8_linstiffmass(lm,mydisp,myres,NULL,&elemat1,&elemat2,&elevec1,NULL,NULL,NULL,params,
                        INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
    }
    break;

    //==================================================================================
    // nonlinear stiffness, internal force vector, and consistent mass matrix
    case calc_struct_nlnstiffmass:
    case calc_struct_nlnstifflmass:
    {
      // need current displacement and residual forces
      RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      if (disp==null || res==null) dserror("Cannot get state vectors 'displacement' and/or residual");
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);

      // default: geometrically non-linear analysis with Total Lagrangean approach
      if (kintype_ == DRT::ELEMENTS::So_hex8::soh8_totlag)
      {
        if (pstype_==INPAR::STR::prestress_id && time_ <= pstime_) // inverse design analysis
          invdesign_->soh8_nlnstiffmass(this,lm,mydisp,myres,&elemat1,&elemat2,&elevec1,NULL,NULL,params,
                                        INPAR::STR::stress_none,INPAR::STR::strain_none);
        else // standard analysis
        soh8_nlnstiffmass(lm,mydisp,myres,&elemat1,&elemat2,&elevec1,NULL,NULL,NULL,params,
                          INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }
      // special case: geometric linear
      else  // (kintype_ == DRT::ELEMENTS::So_hex8::soh8_geolin)
      {
        soh8_linstiffmass(lm,mydisp,myres,NULL,&elemat1,&elemat2,&elevec1,NULL,NULL,NULL,params,
                          INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }

      if (act==calc_struct_nlnstifflmass) soh8_lumpmass(&elemat2);

    }
    break;

    //==================================================================================
    // nonlinear stiffness, internal force vector (GEMM)
    case calc_struct_nlnstiff_gemm:
    {
      // need old displacement, current displacement and residual forces
      RCP<const Epetra_Vector> dispo = discretization.GetState("old displacement");
      RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      if (dispo == null || disp==null || res==null)
        dserror("Cannot get state vectors '(old) displacement' and/or residual");
      std::vector<double> mydispo(lm.size());
      DRT::UTILS::ExtractMyValues(*dispo,mydispo,lm);
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);

      // default: geometrically non-linear analysis with Total Lagrangean approach
      if (kintype_ == DRT::ELEMENTS::So_hex8::soh8_totlag)
      {
        soh8_nlnstiffmass_gemm(lm,mydispo,mydisp,myres,&elemat1,NULL,&elevec1,NULL,NULL,NULL,params,
                               INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);
      }
      // special case: geometric linear
      else  // (kintype_ == DRT::ELEMENTS::So_hex8::soh8_geolin)
      {
        dserror("ERROR: Generalized EMM only makes sense in nonlinear realm");
      }

     break;
    }

    //==================================================================================
    // evaluate stresses and strains at gauss points
    case calc_struct_stress:
    {
      // nothing to do for ghost elements
      if (discretization.Comm().MyPID()==Owner())
      {
        RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
        RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
        RCP<vector<char> > stressdata = params.get<RCP<vector<char> > >("stress", null);
        RCP<vector<char> > straindata = params.get<RCP<vector<char> > >("strain", null);
        RCP<vector<char> > plstraindata = params.get<RCP<vector<char> > >("plstrain", null);
        if (disp==null) dserror("Cannot get state vectors 'displacement'");
        if (stressdata==null) dserror("Cannot get 'stress' data");
        if (straindata==null) dserror("Cannot get 'strain' data");
        if (plstraindata==null) dserror("Cannot get 'plastic strain' data");
        vector<double> mydisp(lm.size());
        DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
        vector<double> myres(lm.size());
        DRT::UTILS::ExtractMyValues(*res,myres,lm);
        LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> stress;
        LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> strain;
        LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> plstrain;
        INPAR::STR::StressType iostress = DRT::INPUT::get<INPAR::STR::StressType>(params, "iostress", INPAR::STR::stress_none);
        INPAR::STR::StrainType iostrain = DRT::INPUT::get<INPAR::STR::StrainType>(params, "iostrain", INPAR::STR::strain_none);
        INPAR::STR::StrainType ioplstrain = DRT::INPUT::get<INPAR::STR::StrainType>(params, "ioplstrain", INPAR::STR::strain_none);

        // default: geometrically non-linear analysis with Total Lagrangean approach
        if (kintype_ == DRT::ELEMENTS::So_hex8::soh8_totlag)
        {
          if (pstype_==INPAR::STR::prestress_id && time_ <= pstime_) // inverse design analysis
            invdesign_->soh8_nlnstiffmass(this,lm,mydisp,myres,NULL,NULL,NULL,&stress,&strain,params,iostress,iostrain);

          else // standard analysis
            soh8_nlnstiffmass(lm,mydisp,myres,NULL,NULL,NULL,&stress,&strain,&plstrain,params,iostress,iostrain,ioplstrain);
        }
        // if a linear analysis is desired
        else  // (kintype_ == DRT::ELEMENTS::So_hex8::soh8_geolin)
        {
          soh8_linstiffmass(lm,mydisp,myres,NULL,NULL,NULL,NULL,&stress,&strain,&plstrain,params,iostress,iostrain,ioplstrain);
        }

        {
          DRT::PackBuffer data;
          AddtoPack(data, stress);
          data.StartPacking();
          AddtoPack(data, stress);
          std::copy(data().begin(),data().end(),std::back_inserter(*stressdata));
        }

        {
          DRT::PackBuffer data;
          AddtoPack(data, strain);
          data.StartPacking();
          AddtoPack(data, strain);
          std::copy(data().begin(),data().end(),std::back_inserter(*straindata));
        }

        {
          DRT::PackBuffer data;
          AddtoPack(data, plstrain);
          data.StartPacking();
          AddtoPack(data, plstrain);
          std::copy(data().begin(),data().end(),std::back_inserter(*plstraindata));
        }
      }
    }
    break;

    //==================================================================================
    // postprocess stresses/strains at gauss points
    // note that in the following, quantities are always referred to as
    // "stresses" etc. although they might also apply to strains
    // (depending on what this routine is called for from the post filter)
    case postprocess_stress:
    {
      // nothing to do for ghost elements
      if (discretization.Comm().MyPID()==Owner())
      {
        const RCP<map<int,RCP<Epetra_SerialDenseMatrix> > > gpstressmap=
          params.get<RCP<map<int,RCP<Epetra_SerialDenseMatrix> > > >("gpstressmap",null);
        if (gpstressmap==null)
          dserror("no gp stress/strain map available for postprocessing");
        string stresstype = params.get<string>("stresstype","ndxyz");
        int gid = Id();
        LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> gpstress(((*gpstressmap)[gid])->A(),true);
        RCP<Epetra_MultiVector> poststress=params.get<RCP<Epetra_MultiVector> >("poststress",null);
        if (poststress==null)
          dserror("No element stress/strain vector available");
        if (stresstype=="ndxyz")
        {
          // extrapolate stresses/strains at Gauss points to nodes
          soh8_expol(gpstress, *poststress);
        }
        else if (stresstype=="cxyz")
        {
          const Epetra_BlockMap& elemap = poststress->Map();
          int lid = elemap.LID(Id());
          if (lid!=-1)
          {
            for (int i = 0; i < NUMSTR_SOH8; ++i)
            {
              double& s = (*((*poststress)(i)))[lid]; // resolve pointer for faster access
              s = 0.;
              for (int j = 0; j < NUMGPT_SOH8; ++j)
              {
                s += gpstress(j,i);
              }
              s *= 1.0/NUMGPT_SOH8;
            }
          }
        }
        else
        {
          dserror("unknown type of stress/strain output on element level");
        }
      }
    }
    break;

    //==================================================================================
    case calc_struct_eleload:
      dserror("this method is not supposed to evaluate a load, use EvaluateNeumann(...)");
    break;

    //==================================================================================
    case calc_struct_fsiload:
      dserror("Case not yet implemented");
    break;

    //==================================================================================
    case calc_struct_update_istep:
    {
      // determine new fiber directions
      RCP<MAT::Material> mat = Material();
      bool remodel;
      const Teuchos::ParameterList& patspec = DRT::Problem::Instance()->PatSpecParams();
      remodel = DRT::INPUT::IntegralValue<int>(patspec,"REMODEL");
      if (remodel &&
          ((mat->MaterialType() == INPAR::MAT::m_holzapfelcardiovascular) ||
           (mat->MaterialType() == INPAR::MAT::m_humphreycardiovascular) ||
           (mat->MaterialType() == INPAR::MAT::m_constraintmixture)))// && timen_ <= timemax_ && stepn_ <= stepmax_)
      {
        RefCountPtr<const Epetra_Vector> disp = discretization.GetState("displacement");
        if (disp==null) dserror("Cannot get state vectors 'displacement'");
        vector<double> mydisp(lm.size());
        DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
        soh8_remodel(lm,mydisp,params,mat);
      }
      // do something with internal EAS, etc parameters
      if (eastype_ != soh8_easnone)
      {
        Epetra_SerialDenseMatrix* alpha = data_.GetMutable<Epetra_SerialDenseMatrix>("alpha");  // Alpha_{n+1}
        Epetra_SerialDenseMatrix* alphao = data_.GetMutable<Epetra_SerialDenseMatrix>("alphao");  // Alpha_n
        // alphao := alpha
        switch(eastype_) {
        case DRT::ELEMENTS::So_hex8::soh8_easfull : LINALG::DENSEFUNCTIONS::update<double,soh8_easfull, 1>(*alphao,*alpha); break;
        case DRT::ELEMENTS::So_hex8::soh8_easmild : LINALG::DENSEFUNCTIONS::update<double,soh8_easmild, 1>(*alphao,*alpha); break;
        case DRT::ELEMENTS::So_hex8::soh8_eassosh8: LINALG::DENSEFUNCTIONS::update<double,soh8_eassosh8,1>(*alphao,*alpha); break;
        case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
        default: dserror("Don't know what to do with EAS type %d", eastype_); break;
        }
      }
      // Update of history for visco material
      if (mat->MaterialType() == INPAR::MAT::m_visconeohooke)
      {
        MAT::ViscoNeoHooke* visco = static_cast <MAT::ViscoNeoHooke*>(mat.get());
        visco->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_viscoanisotropic)
      {
        MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(mat.get());
        visco->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_aaaraghavanvorp_damage)
      {
        MAT::AAAraghavanvorp_damage* aaadamage = static_cast <MAT::AAAraghavanvorp_damage*>(mat.get());
        aaadamage->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_plneohooke)
      {
        MAT::PlasticNeoHooke* plastic = static_cast <MAT::PlasticNeoHooke*>(mat.get());
        plastic->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_pllinelast)
      {
        MAT::PlasticLinElast* pllinelast = static_cast <MAT::PlasticLinElast*>(mat.get());
        pllinelast->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_thermopllinelast)
      {
        MAT::ThermoPlasticLinElast* thrpllinelast = static_cast <MAT::ThermoPlasticLinElast*>(mat.get());
        thrpllinelast->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_vp_robinson)
      {
        MAT::Robinson* robinson = static_cast <MAT::Robinson*>(mat.get());
        robinson->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_growth)
      {
        MAT::Growth* grow = static_cast <MAT::Growth*>(mat.get());
        grow->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_constraintmixture)
      {
        MAT::ConstraintMixture* comix = static_cast <MAT::ConstraintMixture*>(mat.get());
        comix->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_struct_multiscale)
      {
        MAT::MicroMaterial* micro = static_cast<MAT::MicroMaterial*>(mat.get());
        micro->Update();
      }
    }
    break;

    //==================================================================================
    case calc_struct_update_imrlike:
    {
      // determine new fiber directions
      RCP<MAT::Material> mat = Material();
      bool remodel;
      const Teuchos::ParameterList& patspec = DRT::Problem::Instance()->PatSpecParams();
      remodel = DRT::INPUT::IntegralValue<int>(patspec,"REMODEL");
      if (remodel &&
          ((mat->MaterialType() == INPAR::MAT::m_holzapfelcardiovascular) ||
           (mat->MaterialType() == INPAR::MAT::m_humphreycardiovascular) ||
           (mat->MaterialType() == INPAR::MAT::m_constraintmixture)))// && timen_ <= timemax_ && stepn_ <= stepmax_)
      {
        RefCountPtr<const Epetra_Vector> disp = discretization.GetState("displacement");
        if (disp==null) dserror("Cannot get state vectors 'displacement'");
        vector<double> mydisp(lm.size());
        DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
        soh8_remodel(lm,mydisp,params,mat);
      }
      // do something with internal EAS, etc parameters
      // this depends on the applied solution technique (static, generalised-alpha,
      // or other time integrators)
      if (eastype_ != soh8_easnone)
      {
        double alphaf = params.get<double>("alpha f", 0.0);  // generalised-alpha TIS parameter alpha_f
        Epetra_SerialDenseMatrix* alpha = data_.GetMutable<Epetra_SerialDenseMatrix>("alpha");  // Alpha_{n+1-alphaf}
        Epetra_SerialDenseMatrix* alphao = data_.GetMutable<Epetra_SerialDenseMatrix>("alphao");  // Alpha_n
        switch(eastype_) {
        case DRT::ELEMENTS::So_hex8::soh8_easfull:
          // alphao = (-alphaf/(1.0-alphaf))*alphao  + 1.0/(1.0-alphaf) * alpha
          LINALG::DENSEFUNCTIONS::update<double,soh8_easfull,1>(-alphaf/(1.0-alphaf),*alphao,1.0/(1.0-alphaf),*alpha);
          LINALG::DENSEFUNCTIONS::update<double,soh8_easfull,1>(*alpha,*alphao); // alpha := alphao
          break;
        case DRT::ELEMENTS::So_hex8::soh8_easmild:
          // alphao = (-alphaf/(1.0-alphaf))*alphao  + 1.0/(1.0-alphaf) * alpha
          LINALG::DENSEFUNCTIONS::update<double,soh8_easmild,1>(-alphaf/(1.0-alphaf),*alphao,1.0/(1.0-alphaf),*alpha);
          LINALG::DENSEFUNCTIONS::update<double,soh8_easmild,1>(*alpha,*alphao); // alpha := alphao
          break;
        case DRT::ELEMENTS::So_hex8::soh8_eassosh8:
          // alphao = (-alphaf/(1.0-alphaf))*alphao  + 1.0/(1.0-alphaf) * alpha
          LINALG::DENSEFUNCTIONS::update<double,soh8_eassosh8,1>(-alphaf/(1.0-alphaf),*alphao,1.0/(1.0-alphaf),*alpha);
          LINALG::DENSEFUNCTIONS::update<double,soh8_eassosh8,1>(*alpha,*alphao); // alpha := alphao
          break;
        case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
        default: dserror("Don't know what to do with EAS type %d", eastype_); break;
        }
      }
      // Update of history for visco material
      if (mat->MaterialType() == INPAR::MAT::m_visconeohooke)
      {
        MAT::ViscoNeoHooke* visco = static_cast <MAT::ViscoNeoHooke*>(mat.get());
        visco->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_viscoanisotropic)
      {
        MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(mat.get());
        visco->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_aaaraghavanvorp_damage)
      {
        MAT::AAAraghavanvorp_damage* aaadamage = static_cast <MAT::AAAraghavanvorp_damage*>(mat.get());
        aaadamage->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_plneohooke)
      {
        MAT::PlasticNeoHooke* plastic = static_cast <MAT::PlasticNeoHooke*>(mat.get());
        plastic->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_pllinelast)
      {
        MAT::PlasticLinElast* pllinelast = static_cast <MAT::PlasticLinElast*>(mat.get());
        pllinelast->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_thermopllinelast)
      {
        MAT::ThermoPlasticLinElast* thrpllinelast = static_cast <MAT::ThermoPlasticLinElast*>(mat.get());
        thrpllinelast->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_vp_robinson)
      {
        MAT::Robinson* robinson = static_cast <MAT::Robinson*>(mat.get());
        robinson->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_growth)
      {
        MAT::Growth* grow = static_cast <MAT::Growth*>(mat.get());
        grow->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_constraintmixture)
      {
        MAT::ConstraintMixture* comix = static_cast <MAT::ConstraintMixture*>(mat.get());
        comix->Update();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_struct_multiscale)
      {
        MAT::MicroMaterial* micro = static_cast<MAT::MicroMaterial*>(mat.get());
        micro->Update();
      }
    }
    break;

    //==================================================================================
    case calc_struct_reset_istep:
    {
      // do something with internal EAS, etc parameters
      if (eastype_ != soh8_easnone)
      {
        Epetra_SerialDenseMatrix* alpha = data_.GetMutable<Epetra_SerialDenseMatrix>("alpha");  // Alpha_{n+1}
        Epetra_SerialDenseMatrix* alphao = data_.GetMutable<Epetra_SerialDenseMatrix>("alphao");  // Alpha_n
        switch(eastype_) {
        case DRT::ELEMENTS::So_hex8::soh8_easfull : LINALG::DENSEFUNCTIONS::update<double,soh8_easfull,1> (*alpha, *alphao); break;
        case DRT::ELEMENTS::So_hex8::soh8_easmild : LINALG::DENSEFUNCTIONS::update<double,soh8_easmild,1> (*alpha, *alphao); break;
        case DRT::ELEMENTS::So_hex8::soh8_eassosh8: LINALG::DENSEFUNCTIONS::update<double,soh8_eassosh8,1>(*alpha, *alphao); break;
        case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
        default: dserror("Don't know what to do with EAS type %d", eastype_); break;
        }
      }
      // Reset of history for visco material
      RCP<MAT::Material> mat = Material();
      if (mat->MaterialType() == INPAR::MAT::m_visconeohooke)
      {
        MAT::ViscoNeoHooke* visco = static_cast <MAT::ViscoNeoHooke*>(mat.get());
        visco->Reset();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_viscoanisotropic)
      {
        MAT::ViscoAnisotropic* visco = static_cast <MAT::ViscoAnisotropic*>(mat.get());
        visco->Reset();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_aaaraghavanvorp_damage)
      {
        MAT::AAAraghavanvorp_damage* aaadamage = static_cast <MAT::AAAraghavanvorp_damage*>(mat.get());
        aaadamage->Reset();
      }
      else if (mat->MaterialType() == INPAR::MAT::m_plneohooke)
      {
        MAT::PlasticNeoHooke* plastic = static_cast <MAT::PlasticNeoHooke*>(mat.get());
        plastic->Update();
      }
    }
    break;

    //==================================================================================
    case calc_struct_reset_discretization:
    {
      // Reset of history for materials
      RCP<MAT::Material> mat = Material();
      if (mat->MaterialType() == INPAR::MAT::m_constraintmixture)
      {
        MAT::ConstraintMixture* comix = static_cast <MAT::ConstraintMixture*>(mat.get());
        comix->SetupHistory(NUMGPT_SOH8);
      }
      // Reset prestress
      if (pstype_==INPAR::STR::prestress_mulf)
      {
        time_ = 0.0;
        LINALG::Matrix<3,3> Id(true);
        Id(0,0) = Id(1,1) = Id(2,2) = 1.0;
        for (int gp=0; gp<NUMGPT_SOH8; ++gp)
        {
          prestress_->MatrixtoStorage(gp,Id,prestress_->FHistory());
          prestress_->MatrixtoStorage(gp,invJ_[gp],prestress_->JHistory());
        }
      }
      if (pstype_==INPAR::STR::prestress_id)
        dserror("Reset of Inverse Design not yet implemented");
    }
    break;

    //==================================================================================
    case calc_struct_energy:
    {
      // check length of elevec1
      if (elevec1_epetra.Length() < 1) dserror("The given result vector is too short.");

      // not yet implemented for EAS case
      if (eastype_ != soh8_easnone) dserror("Internal energy not yet implemented for EAS.");

      // check material law and strains
      RCP<MAT::Material> mat = Material();
      if (mat->MaterialType() == INPAR::MAT::m_stvenant)
      {
        // declaration of variables
        double intenergy = 0.0;

        // shape functions and Gauss weights
        const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();
        const static std::vector<double> weights = soh8_weights();

        // get displacements of this processor
        RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
        if (disp==null) dserror("Cannot get state displacement vector");

        // get displacements of this element
        vector<double> mydisp(lm.size());
        DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

        // update element geometry
        LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xrefe;  // material coord. of element
        LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xcurr;  // current  coord. of element

        DRT::Node** nodes = Nodes();
        for (int i=0; i<NUMNOD_SOH8; ++i)
        {
          xrefe(i,0) = nodes[i]->X()[0];
          xrefe(i,1) = nodes[i]->X()[1];
          xrefe(i,2) = nodes[i]->X()[2];

          xcurr(i,0) = xrefe(i,0) + mydisp[i*NODDOF_SOH8+0];
          xcurr(i,1) = xrefe(i,1) + mydisp[i*NODDOF_SOH8+1];
          xcurr(i,2) = xrefe(i,2) + mydisp[i*NODDOF_SOH8+2];
        }

        // loop over all Gauss points
        for (int gp=0; gp<NUMGPT_SOH8; gp++)
        {
          // Gauss weights and Jacobian determinant
          double fac = detJ_[gp] * weights[gp];

          /* get the inverse of the Jacobian matrix which looks like:
          **            [ x_,r  y_,r  z_,r ]^-1
          **     J^-1 = [ x_,s  y_,s  z_,s ]
          **            [ x_,t  y_,t  z_,t ]
          */
          // compute derivatives N_XYZ at gp w.r.t. material coordinates
          // by N_XYZ = J^-1 * N_rst
          LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_XYZ(true);
          N_XYZ.Multiply(invJ_[gp],derivs[gp]);

          // (material) deformation gradient F = d xcurr / d xrefe = xcurr^T * N_XYZ^T
          LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> defgrd(true);
          defgrd.MultiplyTT(xcurr,N_XYZ);

          // right Cauchy-Green tensor = F^T * F
          LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> cauchygreen;
          cauchygreen.MultiplyTN(defgrd,defgrd);

          // Green-Lagrange strains matrix E = 0.5 * (Cauchygreen - Identity)
          // GL strain vector glstrain={E11,E22,E33,2*E12,2*E23,2*E31}
          LINALG::Matrix<NUMSTR_SOH8,1> glstrain;
          glstrain(0) = 0.5 * (cauchygreen(0,0) - 1.0);
          glstrain(1) = 0.5 * (cauchygreen(1,1) - 1.0);
          glstrain(2) = 0.5 * (cauchygreen(2,2) - 1.0);
          glstrain(3) = cauchygreen(0,1);
          glstrain(4) = cauchygreen(1,2);
          glstrain(5) = cauchygreen(2,0);

          // Plastic Green-Lagrange strains matrix E_p = 0.5 * (Cauchygreen_p - Identity)
          // Plastic GL strain vector glstrain={E_p11,E_p22,E_p33,2*E_p12,2*E_p23,2*E_p31}
          LINALG::Matrix<NUMSTR_SOH8,1> plglstrain;

          // compute Second Piola Kirchhoff Stress Vector and Constitutive Matrix
          double density = 0.0;
          LINALG::Matrix<NUMSTR_SOH8,NUMSTR_SOH8> cmat(true);
          LINALG::Matrix<NUMSTR_SOH8,1> stress(true);
          soh8_mat_sel(&stress,&cmat,&density,&glstrain,&plglstrain,&defgrd,gp,params);

          // compute GP contribution to internal energy
          intenergy += 0.5 * fac * stress.Dot(glstrain);
        }

        // return result
        elevec1_epetra(0) = intenergy;

        // print internal energy of this element
        //cout << endl << "Internal energy of element # " << this->Id() << ":" << IntEn << endl;
      }
      else
        dserror("ERROR: Internal energy for this material type has not been implemented yet.");
    }
    break;

    //==================================================================================
    case calc_struct_errornorms:
    {
      // IMPORTANT NOTES (popp 10/2010):
      // - error norms are based on a small deformation assumption (linear elasticity)
      // - extension to finite deformations would be possible without difficulties,
      //   however analytical solutions are extremely rare in the nonlinear realm
      // - only implemented for purely displacement-based version, not yet for EAS
      // - only implemented for SVK material (relevant for energy norm only, L2 and
      //   H1 norms are of course valid for arbitrary materials)
      // - analytical solutions are currently stored in a repository in the MORTAR
      //   namespace, however they could (should?) be moved to a more general location

      // check length of elevec1
      if (elevec1_epetra.Length() < 3) dserror("The given result vector is too short.");

      // not yet implemented for EAS case
      if (eastype_ != soh8_easnone) dserror("Error norms not yet implemented for EAS.");

      // check material law
      RCP<MAT::Material> mat = Material();

      //******************************************************************
      // only for St.Venant Kirchhoff material
      //******************************************************************
      if (mat->MaterialType() == INPAR::MAT::m_stvenant)
      {
        // declaration of variables
        double l2norm = 0.0;
        double h1norm = 0.0;
        double energynorm = 0.0;

        // shape functions, derivatives and integration weights
        const static vector<LINALG::Matrix<NUMNOD_SOH8,1> > vals = soh8_shapefcts();
        const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();
        const static std::vector<double> weights = soh8_weights();

        // get displacements and extract values of this element
        RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
        if (disp==null) dserror("Cannot get state displacement vector");
        vector<double> mydisp(lm.size());
        DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

        // nodal displacement vector
        LINALG::Matrix<NUMDOF_SOH8,1> nodaldisp;
        for (int i=0; i<NUMDOF_SOH8; ++i) nodaldisp(i,0) = mydisp[i];

        // reference geometry (nodal positions)
        LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xrefe;
        DRT::Node** nodes = Nodes();
        for (int i=0; i<NUMNOD_SOH8; ++i)
        {
          xrefe(i,0) = nodes[i]->X()[0];
          xrefe(i,1) = nodes[i]->X()[1];
          xrefe(i,2) = nodes[i]->X()[2];
        }

        // deformation gradient = identity tensor (geometrically linear case!)
        LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> defgrd(true);
        for (int i=0;i<NUMDIM_SOH8;++i) defgrd(i,i) = 1;

        //----------------------------------------------------------------
        // loop over all Gauss points
        //----------------------------------------------------------------
        for (int gp=0; gp<NUMGPT_SOH8; gp++)
        {
          // Gauss weights and Jacobian determinant
          double fac = detJ_[gp] * weights[gp];

          // Gauss point in reference configuration
          LINALG::Matrix<NUMDIM_SOH8,1> xgp(true);
          for (int k=0;k<NUMDIM_SOH8;++k)
            for (int n=0;n<NUMNOD_SOH8;++n)
              xgp(k,0) += (vals[gp])(n) * xrefe(n,k);

          //**************************************************************
          // get analytical solution
          LINALG::Matrix<NUMDIM_SOH8,1> uanalyt(true);
          LINALG::Matrix<NUMSTR_SOH8,1> strainanalyt(true);
          LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> derivanalyt(true);

          MORTAR::AnalyticalSolutions3D(xgp,uanalyt,strainanalyt,derivanalyt);
          //**************************************************************

          //--------------------------------------------------------------
          // (1) L2 norm
          //--------------------------------------------------------------

          // compute displacements at GP
          LINALG::Matrix<NUMDIM_SOH8,1> ugp(true);
          for (int k=0;k<NUMDIM_SOH8;++k)
            for (int n=0;n<NUMNOD_SOH8;++n)
              ugp(k,0) += (vals[gp])(n) * nodaldisp(NODDOF_SOH8*n+k,0);

          // displacement error
          LINALG::Matrix<NUMDIM_SOH8,1> uerror(true);
          for (int k=0;k<NUMDIM_SOH8;++k)
            uerror(k,0) = uanalyt(k,0) - ugp(k,0);

          // compute GP contribution to L2 error norm
          l2norm += fac * uerror.Dot(uerror);

          //--------------------------------------------------------------
          // (2) H1 norm
          //--------------------------------------------------------------

          // compute derivatives N_XYZ at GP w.r.t. material coordinates
          // by N_XYZ = J^-1 * N_rst
          LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_XYZ(true);
          N_XYZ.Multiply(invJ_[gp],derivs[gp]);

          // compute partial derivatives at GP
          LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> derivgp(true);
          for (int l=0;l<NUMDIM_SOH8;++l)
            for (int m=0;m<NUMDIM_SOH8;++m)
              for (int k=0;k<NUMNOD_SOH8;++k)
                derivgp(l,m) += N_XYZ(m,k) * nodaldisp(NODDOF_SOH8*k+l,0);

          // derivative error
          LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> deriverror(true);
          for (int k=0;k<NUMDIM_SOH8;++k)
            for (int m=0;m<NUMDIM_SOH8;++m)
              deriverror(k,m) = derivanalyt(k,m) - derivgp(k,m);

          // compute GP contribution to H1 error norm
          h1norm += fac * deriverror.Dot(deriverror);
          h1norm += fac * uerror.Dot(uerror);

          //--------------------------------------------------------------
          // (3) Energy norm
          //--------------------------------------------------------------

          // compute linear B-operator
          LINALG::Matrix<NUMSTR_SOH8,NUMDOF_SOH8> bop;
          for (int i=0; i<NUMNOD_SOH8; ++i)
          {
            bop(0,NODDOF_SOH8*i+0) = N_XYZ(0,i);
            bop(0,NODDOF_SOH8*i+1) = 0.0;
            bop(0,NODDOF_SOH8*i+2) = 0.0;
            bop(1,NODDOF_SOH8*i+0) = 0.0;
            bop(1,NODDOF_SOH8*i+1) = N_XYZ(1,i);
            bop(1,NODDOF_SOH8*i+2) = 0.0;
            bop(2,NODDOF_SOH8*i+0) = 0.0;
            bop(2,NODDOF_SOH8*i+1) = 0.0;
            bop(2,NODDOF_SOH8*i+2) = N_XYZ(2,i);

            bop(3,NODDOF_SOH8*i+0) = N_XYZ(1,i);
            bop(3,NODDOF_SOH8*i+1) = N_XYZ(0,i);
            bop(3,NODDOF_SOH8*i+2) = 0.0;
            bop(4,NODDOF_SOH8*i+0) = 0.0;
            bop(4,NODDOF_SOH8*i+1) = N_XYZ(2,i);
            bop(4,NODDOF_SOH8*i+2) = N_XYZ(1,i);
            bop(5,NODDOF_SOH8*i+0) = N_XYZ(2,i);
            bop(5,NODDOF_SOH8*i+1) = 0.0;
            bop(5,NODDOF_SOH8*i+2) = N_XYZ(0,i);
          }

          // compute linear strain at GP
          LINALG::Matrix<NUMSTR_SOH8,1> straingp(true);
          straingp.Multiply(bop,nodaldisp);

          // strain error
          LINALG::Matrix<NUMSTR_SOH8,1> strainerror(true);
          for (int k=0;k<NUMSTR_SOH8;++k)
            strainerror(k,0) = strainanalyt(k,0) - straingp(k,0);

          // compute stress vector and constitutive matrix
          double density = 0.0;
          LINALG::Matrix<NUMSTR_SOH8,NUMSTR_SOH8> cmat(true);
          LINALG::Matrix<NUMSTR_SOH8,1> stress(true);
          LINALG::Matrix<NUMSTR_SOH8,1> plglstrain(true);
          soh8_mat_sel(&stress,&cmat,&density,&strainerror,&plglstrain,&defgrd,gp,params);

          // compute GP contribution to energy error norm
          energynorm += fac * stress.Dot(strainerror);

          //cout << "UAnalytical:      " << uanalyt << endl;
          //cout << "UDiscrete:        " << ugp << endl;
          //cout << "StrainAnalytical: " << strainanalyt << endl;
          //cout << "StrainDiscrete:   " << straingp << endl;
          //cout << "DerivAnalytical:  " << derivanalyt << endl;
          //cout << "DerivDiscrete:    " << derivgp << endl;
          //cout << endl;
        }
        //----------------------------------------------------------------

        // return results
        elevec1_epetra(0) = l2norm;
        elevec1_epetra(1) = h1norm;
        elevec1_epetra(2) = energynorm;
      }
      else
        dserror("ERROR: Error norms only implemented for SVK material");
    }
    break;

    //==================================================================================
  case multi_calc_dens:
  {
    soh8_homog(params);
  }
  break;

  //==================================================================================
  // in case of multi-scale problems, possible EAS internal data on microscale
  // have to be stored in every macroscopic Gauss point
  // allocation and initializiation of these data arrays can only be
  // done in the elements that know the number of EAS parameters
  case multi_eas_init:
  {
    soh8_eas_init_multi(params);
  }
  break;

  //==================================================================================
  // in case of multi-scale problems, possible EAS internal data on microscale
  // have to be stored in every macroscopic Gauss point
  // before any microscale simulation, EAS internal data has to be
  // set accordingly
  case multi_eas_set:
  {
    soh8_set_eas_multi(params);
  }
  break;

  //==================================================================================
  // read restart of microscale
  case multi_readrestart:
  {
    soh8_read_restart_multi();
  }
  break;

  //==================================================================================
  // compute additional stresses due to intermolecular potential forces
  case calc_potential_stiff:
  {
    RCP<PotentialManager> potentialmanager =
      params.get<RCP<PotentialManager> >("pot_man", null);
    if (potentialmanager==null)
      dserror("No PotentialManager in Solid3 Surface available");

    RCP<DRT::Condition> cond = params.get<RCP<DRT::Condition> >("condition",null);
    if (cond==null)
      dserror("Condition not available in Solid3 Surface");

    if (cond->Type()==DRT::Condition::LJ_Potential_Volume) // Lennard-Jones potential
    {
      potentialmanager->StiffnessAndInternalForcesPotential(this, DRT::UTILS::intrule_hex_8point, params, lm, elemat1_epetra, elevec1_epetra);
    }
    if (cond->Type()==DRT::Condition::VanDerWaals_Potential_Volume) // Van der Walls forces
    {
      potentialmanager->StiffnessAndInternalForcesPotential(this, DRT::UTILS::intrule_hex_8point, params, lm, elemat1_epetra, elevec1_epetra);
    }
    if( cond->Type()!=DRT::Condition::LJ_Potential_Volume &&
        cond->Type()!=DRT::Condition::VanDerWaals_Potential_Volume)
      dserror("Unknown condition type %d",cond->Type());
  }
  break;

  //==================================================================================
  case prestress_update:
  {
    time_ = params.get<double>("total time");
    RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
    if (disp==null) dserror("Cannot get displacement state");
    vector<double> mydisp(lm.size());
    DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

    // build def gradient for every gauss point
    LINALG::SerialDenseMatrix gpdefgrd(NUMGPT_SOH8,9);
    DefGradient(mydisp,gpdefgrd,*prestress_);

    // update deformation gradient and put back to storage
    LINALG::Matrix<3,3> deltaF;
    LINALG::Matrix<3,3> Fhist;
    LINALG::Matrix<3,3> Fnew;
    for (int gp=0; gp<NUMGPT_SOH8; ++gp)
    {
      prestress_->StoragetoMatrix(gp,deltaF,gpdefgrd);
      prestress_->StoragetoMatrix(gp,Fhist,prestress_->FHistory());
      Fnew.Multiply(deltaF,Fhist);
      prestress_->MatrixtoStorage(gp,Fnew,prestress_->FHistory());
      // if(gp ==1)
      // {
      //cout << "Fhist  " << Fhist << endl;
      //cout << "Fhnew  " << new << endl;
      // }
    }

    // push-forward invJ for every gaussian point
    UpdateJacobianMapping(mydisp,*prestress_);
  }
  break;
  //==================================================================================
  case inversedesign_update:
  {
    time_ = params.get<double>("total time");
    RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
    if (disp==null) dserror("Cannot get displacement state");
    vector<double> mydisp(lm.size());
    DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
    invdesign_->soh8_StoreMaterialConfiguration(this,mydisp);
    invdesign_->IsInit() = true; // this is to make the restart work
  }
  break;
  //==================================================================================
  case inversedesign_switch:
  {
    time_ = params.get<double>("total time");
  }
  break;
  //==================================================================================
  // evaluate stresses and strains at gauss points and store gpstresses in map <EleId, gpstresses >
  case calc_global_gpstresses_map:
  {
    // nothing to do for ghost elements
    if (discretization.Comm().MyPID()==Owner())
    {
      RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      RCP<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      RCP<vector<char> > stressdata = params.get<RCP<vector<char> > >("stress", null);
      RCP<vector<char> > straindata = params.get<RCP<vector<char> > >("strain", null);
      RCP<vector<char> > plstraindata = params.get<RCP<vector<char> > >("plstrain", null);
      if (disp==null) dserror("Cannot get state vectors 'displacement'");
      if (stressdata==null) dserror("Cannot get 'stress' data");
      if (straindata==null) dserror("Cannot get 'strain' data");
      if (plstraindata==null) dserror("Cannot get 'plastic strain' data");
      const RCP<map<int,RCP<Epetra_SerialDenseMatrix> > > gpstressmap=
        params.get<RCP<map<int,RCP<Epetra_SerialDenseMatrix> > > >("gpstressmap",null);
      if (gpstressmap==null)
        dserror("no gp stress map available for writing gpstresses");
      const RCP<map<int,RCP<Epetra_SerialDenseMatrix> > > gpstrainmap=
        params.get<RCP<map<int,RCP<Epetra_SerialDenseMatrix> > > >("gpstrainmap",null);
      if (gpstrainmap==null)
        dserror("no gp strain map available for writing gpstrains");
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> stress;
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> strain;
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8> plstrain;
      INPAR::STR::StressType iostress = DRT::INPUT::get<INPAR::STR::StressType>(params, "iostress", INPAR::STR::stress_none);
      INPAR::STR::StrainType iostrain = DRT::INPUT::get<INPAR::STR::StrainType>(params, "iostrain", INPAR::STR::strain_none);
      INPAR::STR::StrainType ioplstrain = DRT::INPUT::get<INPAR::STR::StrainType>(params, "ioplstrain", INPAR::STR::strain_none);
      //

      // if a linear analysis is desired
      if (kintype_ == DRT::ELEMENTS::So_hex8::soh8_geolin)
        soh8_linstiffmass(lm,mydisp,myres,NULL,NULL,NULL,NULL,&stress,&strain,&plstrain,params,iostress,iostrain,ioplstrain);
      // standard is: geometrically non-linear with Total Lagrangean approach
      else
      {
        if (pstype_==INPAR::STR::prestress_id && time_ <= pstime_) // inverse design analysis
          invdesign_->soh8_nlnstiffmass(this,lm,mydisp,myres,NULL,NULL,NULL,&stress,&strain,params,iostress,iostrain);

        else // standard analysis
          soh8_nlnstiffmass(lm,mydisp,myres,NULL,NULL,NULL,&stress,&strain,&plstrain,params,iostress,iostrain,ioplstrain);
      }
      // add stresses to global map
      //get EleID Id()
      int gid = Id();
      RCP<Epetra_SerialDenseMatrix> gpstress = rcp(new Epetra_SerialDenseMatrix);
      gpstress->Shape(NUMGPT_SOH8,NUMSTR_SOH8);

      //move stresses to serial dense matrix
      for(int i=0;i<NUMGPT_SOH8;i++)
      {
        for(int j=0;j<NUMSTR_SOH8;j++)
        {
          (*gpstress)(i,j)=stress(i,j);
        }
      }

      //strains
      RCP<Epetra_SerialDenseMatrix> gpstrain = rcp(new Epetra_SerialDenseMatrix);
      gpstrain->Shape(NUMGPT_SOH8,NUMSTR_SOH8);

      //move stresses to serial dense matrix
      for(int i=0;i<NUMGPT_SOH8;i++)
      {
        for(int j=0;j<NUMSTR_SOH8;j++)
        {
          (*gpstrain)(i,j)=strain(i,j);
        }
      }

      //add to map
      (*gpstressmap)[gid]=gpstress;
      (*gpstrainmap)[gid]=gpstrain;

      {
        DRT::PackBuffer data;
        AddtoPack(data, stress);
        data.StartPacking();
        AddtoPack(data, stress);
        std::copy(data().begin(),data().end(),std::back_inserter(*stressdata));
      }

      {
        DRT::PackBuffer data;
        AddtoPack(data, strain);
        data.StartPacking();
        AddtoPack(data, strain);
        std::copy(data().begin(),data().end(),std::back_inserter(*straindata));
      }
      
      {
        DRT::PackBuffer data;
        AddtoPack(data, plstrain);
        data.StartPacking();
        AddtoPack(data, plstrain);
        std::copy(data().begin(),data().end(),std::back_inserter(*plstraindata));
      }
    }
  }
  break;

  //==================================================================================
  // nonlinear stiffness and internal force vector for poroelasticity
  case calc_poroelast_nlnstiff:
  {
    // stiffness
    LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8> elemat1(elemat1_epetra.A(),true);
    LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8> elemat2(elemat2_epetra.A(),true);
    // internal force vector
    LINALG::Matrix<NUMDOF_SOH8,1> elevec1(elevec1_epetra.A(),true);
    LINALG::Matrix<NUMDOF_SOH8,1> elevec2(elevec2_epetra.A(),true);
    // elemat2,elevec2+3 are not used anyway

    // need current displacement and residual forces
    Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState(0,"displacement");
    Teuchos::RCP<const Epetra_Vector> res  = discretization.GetState(0,"residual displacement");
    if (disp==null )
      dserror("Cannot get state vector 'displacement' ");
    // build the location vector only for the structure field
    // vector<int> lm = la[0].lm_;
    vector<double> mydisp((lm).size());
    DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);  // global, local, lm
    vector<double> myres(lm.size());
    DRT::UTILS::ExtractMyValues(*res,myres,lm);
    LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* matptr = NULL;
    if (elemat1.IsInitialized()) matptr = &elemat1;
    // call the well-known soh8_nlnstiffmass for the normal structure solution
      soh8_nlnstiffmass(lm,mydisp,myres,matptr,NULL,&elevec1,NULL,NULL,NULL,params,
                        INPAR::STR::stress_none,INPAR::STR::strain_none,INPAR::STR::strain_none);

	  // need current fluid state,
	  // call the fluid discretization: fluid equates 2nd dofset
	  // disassemble velocities and pressures

	    /*
  if (discretization.HasState(1,"fluidveln"))
	  {
		// the temperature field has only one dof per node, disregarded by the
		// dimension of the problem
		const int numdofpernode_ = NumDofPerNode(1,*(Nodes()[0]));
		// number of nodes per element
		const int nen_ = 8;

	    LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> myvelnp(true);
	    LINALG::Matrix<NUMNOD_SOH8,1>    myepreaf(true);
		// check if you can get the temperature state
		Teuchos::RCP<const Epetra_Vector> veln
		 = discretization.GetState(1,"fluidveln");
		if (velnp==Teuchos::null)
		  dserror("Cannot get state vector 'fluidveln'");

		//if (la[1].Size() != nen_*numdofpernode_)
		//  dserror("Location vector length for velocities does not match!");

	      // extract local values of the global vectors
	      std::vector<double> mymatrix(la[1].lm_.size());
	      DRT::UTILS::ExtractMyValues(*velnp,mymatrix,la[1].lm_);

	      for (int inode=0; inode<NUMNOD_SOH8; ++inode)  // number of nodes
	      {

	          for(int idim=0; idim<NUMDIM_SOH8; ++idim) // number of dimensions
	          {
	            (myvelnp)(idim,inode) = mymatrix[idim+(inode*numdofpernode_)];
	          }  // end for(idim)

	            (myepreaf)(inode,0) = mymatrix[NUMDIM_SOH8+(inode*numdofpernode_)];
      std::copy(data().begin(),data().end(),std::back_inserter(*plstraindata));
    }
	      }

		soh8_nlnstiff_poroelast(lm,mydisp,myvelnp,myepreaf,&elemat1,&elemat2,&elevec1,&elevec2,NULL,NULL,params,
	                       INPAR::STR::stress_none,INPAR::STR::strain_none);
	  }*/

  }
  break;

    //==================================================================================
  default:
    dserror("Unknown type of action for So_hex8");
  }
  return 0;
}



/*----------------------------------------------------------------------*
 |  Integrate a Volume Neumann boundary condition (public)     maf 04/07|
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::So_hex8::EvaluateNeumann(ParameterList&            params,
                                            DRT::Discretization&      discretization,
                                            DRT::Condition&           condition,
                                            vector<int>&              lm,
                                            Epetra_SerialDenseVector& elevec1,
                                            Epetra_SerialDenseMatrix* elemat1)
{
  // get values and switches from the condition
  const vector<int>*    onoff = condition.Get<vector<int> >   ("onoff");
  const vector<double>* val   = condition.Get<vector<double> >("val"  );

  /*
  **    TIME CURVE BUSINESS
  */
  // find out whether we will use a time curve
  bool usetime = true;
  const double time = params.get("total time",-1.0);
  if (time<0.0) usetime = false;

  // find out whether we will use a time curve and get the factor
  const vector<int>* curve = condition.Get<vector<int> >("curve");
  int curvenum = -1;
  if (curve) curvenum = (*curve)[0];
  double curvefac = 1.0;
  if (curvenum>=0 && usetime)
    curvefac = DRT::Problem::Instance()->Curve(curvenum).f(time);
  // **

  // (SPATIAL) FUNCTION BUSINESS
  const std::vector<int>* funct = condition.Get<std::vector<int> >("funct");
  LINALG::Matrix<NUMDIM_SOH8,1> xrefegp(false);
  bool havefunct = false;
  if (funct)
    for (int dim=0; dim<NUMDIM_SOH8; dim++)
      if ((*funct)[dim] > 0)
        havefunct = havefunct or true;


  /* ============================================================================*
  ** CONST SHAPE FUNCTIONS, DERIVATIVES and WEIGHTS for HEX_8 with 8 GAUSS POINTS*
  ** ============================================================================*/
  const static vector<LINALG::Matrix<NUMNOD_SOH8,1> > shapefcts = soh8_shapefcts();
  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();
  const static vector<double> gpweights = soh8_weights();
  /* ============================================================================*/

  // update element geometry
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xrefe;  // material coord. of element
  DRT::Node** nodes = Nodes();
  for (int i=0; i<NUMNOD_SOH8; ++i){
    const double* x = nodes[i]->X();
    xrefe(i,0) = x[0];
    xrefe(i,1) = x[1];
    xrefe(i,2) = x[2];
  }
  /* ================================================= Loop over Gauss Points */
  for (int gp=0; gp<NUMGPT_SOH8; ++gp) {

    // compute the Jacobian matrix
    LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> jac;
    jac.Multiply(derivs[gp],xrefe);

    // compute determinant of Jacobian
    const double detJ = jac.Determinant();
    if (detJ == 0.0) dserror("ZERO JACOBIAN DETERMINANT");
    else if (detJ < 0.0) dserror("NEGATIVE JACOBIAN DETERMINANT");

    // material/reference co-ordinates of Gauss point
    if (havefunct) {
      for (int dim=0; dim<NUMDIM_SOH8; dim++) {
        xrefegp(dim) = 0.0;
        for (int nodid=0; nodid<NUMNOD_SOH8; ++nodid)
          xrefegp(dim) += shapefcts[gp](nodid) * xrefe(nodid,dim);
      }
    }

    // integration factor
    const double fac = gpweights[gp] * curvefac * detJ;
    // distribute/add over element load vector
    for(int dim=0; dim<NUMDIM_SOH8; dim++) {
      // function evaluation
      const int functnum = (funct) ? (*funct)[dim] : -1;
      const double functfac
        = (functnum>0)
        ? DRT::Problem::Instance()->Funct(functnum-1).Evaluate(dim,xrefegp.A(),0.0,NULL)
        : 1.0;
      const double dim_fac = (*onoff)[dim] * (*val)[dim] * fac * functfac;
      for (int nodid=0; nodid<NUMNOD_SOH8; ++nodid) {
        elevec1[nodid*NUMDIM_SOH8+dim] += shapefcts[gp](nodid) * dim_fac;
      }
    }

  }/* ==================================================== end of Loop over GP */

  return 0;
} // DRT::ELEMENTS::So_hex8::EvaluateNeumann


/*----------------------------------------------------------------------*
 |  init the element jacobian mapping (protected)              gee 04/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::InitJacobianMapping()
{
  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xrefe;
  for (int i=0; i<NUMNOD_SOH8; ++i)
  {
    xrefe(i,0) = Nodes()[i]->X()[0];
    xrefe(i,1) = Nodes()[i]->X()[1];
    xrefe(i,2) = Nodes()[i]->X()[2];
  }
  invJ_.resize(NUMGPT_SOH8);
  detJ_.resize(NUMGPT_SOH8);
  for (int gp=0; gp<NUMGPT_SOH8; ++gp)
  {
    //invJ_[gp].Shape(NUMDIM_SOH8,NUMDIM_SOH8);
    invJ_[gp].Multiply(derivs[gp],xrefe);
    detJ_[gp] = invJ_[gp].Invert();
    if (detJ_[gp] <= 0.0) dserror("Element Jacobian mapping %10.5e <= 0.0",detJ_[gp]);

    if (pstype_==INPAR::STR::prestress_mulf && pstime_ >= time_)
      if (!(prestress_->IsInit()))
        prestress_->MatrixtoStorage(gp,invJ_[gp],prestress_->JHistory());

    if (pstype_==INPAR::STR::prestress_id && pstime_ < time_)
      if (!(invdesign_->IsInit()))
      {
        //printf("Ele %d id use InitJacobianMapping pstime < time %10.5e < %10.5e\n",Id(),pstime_,time_);
        invdesign_->MatrixtoStorage(gp,invJ_[gp],invdesign_->JHistory());
        invdesign_->DetJHistory()[gp] = detJ_[gp];
      }
  }

  if (pstype_==INPAR::STR::prestress_mulf && pstime_ >= time_)
    prestress_->IsInit() = true;

  if (pstype_==INPAR::STR::prestress_id && pstime_ < time_)
    invdesign_->IsInit() = true;

  return;
}

/*----------------------------------------------------------------------*
 |  evaluate the element (private)                             maf 04/07|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::soh8_nlnstiffmass(
      vector<int>&              lm,             // location matrix
      vector<double>&           disp,           // current displacements
      vector<double>&           residual,       // current residual displ
      LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* stiffmatrix, // element stiffness matrix
      LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* massmatrix,  // element mass matrix
      LINALG::Matrix<NUMDOF_SOH8,1>* force,                 // element internal force vector
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8>* elestress,   // stresses at GP
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8>* elestrain,   // strains at GP
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8>* eleplstrain, // plastic strains at GP
      ParameterList&            params,         // algorithmic parameters e.g. time
      const INPAR::STR::StressType   iostress,  // stress output option
      const INPAR::STR::StrainType   iostrain,  // strain output option
      const INPAR::STR::StrainType   ioplstrain)  // plastic strain output option
{
/* ============================================================================*
** CONST SHAPE FUNCTIONS, DERIVATIVES and WEIGHTS for HEX_8 with 8 GAUSS POINTS*
** ============================================================================*/
  const static vector<LINALG::Matrix<NUMNOD_SOH8,1> > shapefcts = soh8_shapefcts();
  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();
  const static vector<double> gpweights = soh8_weights();
/* ============================================================================*/

  // check for prestressing
  if (pstype_ != INPAR::STR::prestress_none && eastype_ != soh8_easnone)
    dserror("No way you can do mulf or id prestressing with EAS turned on!");

  // update element geometry
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xrefe;  // material coord. of element
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xcurr;  // current  coord. of element
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xdisp;

  DRT::Node** nodes = Nodes();
  for (int i=0; i<NUMNOD_SOH8; ++i)
  {
    const double* x = nodes[i]->X();
    xrefe(i,0) = x[0];
    xrefe(i,1) = x[1];
    xrefe(i,2) = x[2];

    xcurr(i,0) = xrefe(i,0) + disp[i*NODDOF_SOH8+0];
    xcurr(i,1) = xrefe(i,1) + disp[i*NODDOF_SOH8+1];
    xcurr(i,2) = xrefe(i,2) + disp[i*NODDOF_SOH8+2];

    if (pstype_==INPAR::STR::prestress_mulf)
    {
      xdisp(i,0) = disp[i*NODDOF_SOH8+0];
      xdisp(i,1) = disp[i*NODDOF_SOH8+1];
      xdisp(i,2) = disp[i*NODDOF_SOH8+2];
    }
  }

  /*
  ** EAS Technology: declare, intialize, set up, and alpha history -------- EAS
  */
  // in any case declare variables, sizes etc. only in eascase
  Epetra_SerialDenseMatrix* alpha = NULL;  // EAS alphas
  vector<Epetra_SerialDenseMatrix>* M_GP = NULL;   // EAS matrix M at all GPs
  LINALG::SerialDenseMatrix M;      // EAS matrix M at current GP
  Epetra_SerialDenseVector feas;    // EAS portion of internal forces
  Epetra_SerialDenseMatrix Kaa;     // EAS matrix Kaa
  Epetra_SerialDenseMatrix Kda;     // EAS matrix Kda
  double detJ0;                     // detJ(origin)
  Epetra_SerialDenseMatrix* oldfeas = NULL;   // EAS history
  Epetra_SerialDenseMatrix* oldKaainv = NULL; // EAS history
  Epetra_SerialDenseMatrix* oldKda = NULL;    // EAS history

  // transformation matrix T0, maps M-matrix evaluated at origin
  // between local element coords and global coords
  // here we already get the inverse transposed T0
  LINALG::Matrix<NUMSTR_SOH8,NUMSTR_SOH8> T0invT;  // trafo matrix

  if (eastype_ != soh8_easnone)
  {
    /*
    ** EAS Update of alphas:
    ** the current alphas are (re-)evaluated out of
    ** Kaa and Kda of previous step to avoid additional element call.
    ** This corresponds to the (innermost) element update loop
    ** in the nonlinear FE-Skript page 120 (load-control alg. with EAS)
    */
    alpha = data_.GetMutable<Epetra_SerialDenseMatrix>("alpha");   // get alpha of previous iteration
    oldfeas = data_.GetMutable<Epetra_SerialDenseMatrix>("feas");
    oldKaainv = data_.GetMutable<Epetra_SerialDenseMatrix>("invKaa");
    oldKda = data_.GetMutable<Epetra_SerialDenseMatrix>("Kda");
    if (!alpha || !oldKaainv || !oldKda || !oldfeas) dserror("Missing EAS history-data");

    // we need the (residual) displacement at the previous step
    LINALG::SerialDenseVector res_d(NUMDOF_SOH8);
    for (int i = 0; i < NUMDOF_SOH8; ++i)
    {
      res_d(i) = residual[i];
    }
    // add Kda . res_d to feas
    // new alpha is: - Kaa^-1 . (feas + Kda . old_d), here: - Kaa^-1 . feas
    switch(eastype_)
    {
    case DRT::ELEMENTS::So_hex8::soh8_easfull:
      LINALG::DENSEFUNCTIONS::multiply<double,soh8_easfull,NUMDOF_SOH8,1>(1.0, *oldfeas, 1.0, *oldKda, res_d);
      LINALG::DENSEFUNCTIONS::multiply<double,soh8_easfull,soh8_easfull,1>(1.0,*alpha,-1.0,*oldKaainv,*oldfeas);
      break;
    case DRT::ELEMENTS::So_hex8::soh8_easmild:
      LINALG::DENSEFUNCTIONS::multiply<double,soh8_easmild,NUMDOF_SOH8,1>(1.0, *oldfeas, 1.0, *oldKda, res_d);
      LINALG::DENSEFUNCTIONS::multiply<double,soh8_easmild,soh8_easmild,1>(1.0,*alpha,-1.0,*oldKaainv,*oldfeas);
      break;
    case DRT::ELEMENTS::So_hex8::soh8_eassosh8:
      LINALG::DENSEFUNCTIONS::multiply<double,soh8_eassosh8,NUMDOF_SOH8,1>(1.0, *oldfeas, 1.0, *oldKda, res_d);
      LINALG::DENSEFUNCTIONS::multiply<double,soh8_eassosh8,soh8_eassosh8,1>(1.0,*alpha,-1.0,*oldKaainv,*oldfeas);
      break;
    case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
    default: dserror("Don't know what to do with EAS type %d", eastype_); break;
    }
    /* end of EAS Update ******************/

    // EAS portion of internal forces, also called enhacement vector s or Rtilde
    feas.Size(neas_);

    // EAS matrix K_{alpha alpha}, also called Dtilde
    Kaa.Shape(neas_,neas_);

    // EAS matrix K_{d alpha}
    Kda.Shape(neas_,NUMDOF_SOH8);


    /* evaluation of EAS variables (which are constant for the following):
    ** -> M defining interpolation of enhanced strains alpha, evaluated at GPs
    ** -> determinant of Jacobi matrix at element origin (r=s=t=0.0)
    ** -> T0^{-T}
    */
    soh8_eassetup(&M_GP,detJ0,T0invT,xrefe);
  } // -------------------------------------------------------------------- EAS


  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_XYZ;
  // build deformation gradient wrt to material configuration
  // in case of prestressing, build defgrd wrt to last stored configuration
  LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> defgrd(false);
  for (int gp=0; gp<NUMGPT_SOH8; ++gp)
  {

    /* get the inverse of the Jacobian matrix which looks like:
    **            [ x_,r  y_,r  z_,r ]^-1
    **     J^-1 = [ x_,s  y_,s  z_,s ]
    **            [ x_,t  y_,t  z_,t ]
    */
    // compute derivatives N_XYZ at gp w.r.t. material coordinates
    // by N_XYZ = J^-1 * N_rst
    N_XYZ.Multiply(invJ_[gp],derivs[gp]);
    double detJ = detJ_[gp];

    if (pstype_==INPAR::STR::prestress_mulf)
    {
      // get Jacobian mapping wrt to the stored configuration
      LINALG::Matrix<3,3> invJdef;
      prestress_->StoragetoMatrix(gp,invJdef,prestress_->JHistory());
      // get derivatives wrt to last spatial configuration
      LINALG::Matrix<3,8> N_xyz;
      N_xyz.Multiply(invJdef,derivs[gp]);

      // build multiplicative incremental defgrd
      defgrd.MultiplyTT(xdisp,N_xyz);
      defgrd(0,0) += 1.0;
      defgrd(1,1) += 1.0;
      defgrd(2,2) += 1.0;

      // get stored old incremental F
      LINALG::Matrix<3,3> Fhist;
      prestress_->StoragetoMatrix(gp,Fhist,prestress_->FHistory());

      // build total defgrd = delta F * F_old
      LINALG::Matrix<3,3> Fnew;
      Fnew.Multiply(defgrd,Fhist);
      defgrd = Fnew;
    }
    else
      // (material) deformation gradient F = d xcurr / d xrefe = xcurr^T * N_XYZ^T
      defgrd.MultiplyTT(xcurr,N_XYZ);

    if (pstype_==INPAR::STR::prestress_id && pstime_ < time_)
    {
      //printf("Ele %d entering id poststress\n",Id());
      // make the multiplicative update so that defgrd refers to
      // the reference configuration that resulted from the inverse
      // design analysis
      LINALG::Matrix<3,3> Fhist;
      invdesign_->StoragetoMatrix(gp,Fhist,invdesign_->FHistory());
      LINALG::Matrix<3,3> tmp3x3;
      tmp3x3.Multiply(defgrd,Fhist);
      defgrd = tmp3x3;

      // make detJ and invJ refer to the ref. configuration that resulted from
      // the inverse design analysis
      detJ = invdesign_->DetJHistory()[gp];
      invdesign_->StoragetoMatrix(gp,tmp3x3,invdesign_->JHistory());
      N_XYZ.Multiply(tmp3x3,derivs[gp]);
    }

    // Right Cauchy-Green tensor = F^T * F
    LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> cauchygreen;
    cauchygreen.MultiplyTN(defgrd,defgrd);

    // Green-Lagrange strains matrix E = 0.5 * (Cauchygreen - Identity)
    // GL strain vector glstrain={E11,E22,E33,2*E12,2*E23,2*E31}
    Epetra_SerialDenseVector glstrain_epetra(NUMSTR_SOH8);
    LINALG::Matrix<NUMSTR_SOH8,1> glstrain(glstrain_epetra.A(),true);
    glstrain(0) = 0.5 * (cauchygreen(0,0) - 1.0);
    glstrain(1) = 0.5 * (cauchygreen(1,1) - 1.0);
    glstrain(2) = 0.5 * (cauchygreen(2,2) - 1.0);
    glstrain(3) = cauchygreen(0,1);
    glstrain(4) = cauchygreen(1,2);
    glstrain(5) = cauchygreen(2,0);

    // EAS technology: "enhance the strains"  ----------------------------- EAS
    if (eastype_ != soh8_easnone)
    {
      M.LightShape(NUMSTR_SOH8,neas_);
      // map local M to global, also enhancement is refered to element origin
      // M = detJ0/detJ T0^{-T} . M
      //Epetra_SerialDenseMatrix Mtemp(M); // temp M for Matrix-Matrix-Product
      // add enhanced strains = M . alpha to GL strains to "unlock" element
      switch(eastype_)
      {
      case DRT::ELEMENTS::So_hex8::soh8_easfull:
        LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,NUMSTR_SOH8,soh8_easfull>(M.A(), detJ0/detJ, T0invT.A(), (M_GP->at(gp)).A());
        LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,soh8_easfull,1>(1.0,glstrain.A(),1.0,M.A(),alpha->A());
        break;
      case DRT::ELEMENTS::So_hex8::soh8_easmild:
        LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,NUMSTR_SOH8,soh8_easmild>(M.A(), detJ0/detJ, T0invT.A(), (M_GP->at(gp)).A());
        LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,soh8_easmild,1>(1.0,glstrain.A(),1.0,M.A(),alpha->A());
        break;
      case DRT::ELEMENTS::So_hex8::soh8_eassosh8:
        LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,NUMSTR_SOH8,soh8_eassosh8>(M.A(), detJ0/detJ, T0invT.A(), (M_GP->at(gp)).A());
        LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,soh8_eassosh8,1>(1.0,glstrain.A(),1.0,M.A(),alpha->A());
        break;
      case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
      default: dserror("Don't know what to do with EAS type %d", eastype_); break;
      }
    } // ------------------------------------------------------------------ EAS

    // return gp strains (only in case of stress/strain output)
    switch (iostrain)
    {
    case INPAR::STR::strain_gl:
    {
      if (elestrain == NULL) dserror("strain data not available");
      for (int i = 0; i < 3; ++i)
        (*elestrain)(gp,i) = glstrain(i);
      for (int i = 3; i < 6; ++i)
        (*elestrain)(gp,i) = 0.5 * glstrain(i);
    }
    break;
    case INPAR::STR::strain_ea:
    {
      if (elestrain == NULL) dserror("strain data not available");
      // rewriting Green-Lagrange strains in matrix format
      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> gl;
      gl(0,0) = glstrain(0);
      gl(0,1) = 0.5*glstrain(3);
      gl(0,2) = 0.5*glstrain(5);
      gl(1,0) = gl(0,1);
      gl(1,1) = glstrain(1);
      gl(1,2) = 0.5*glstrain(4);
      gl(2,0) = gl(0,2);
      gl(2,1) = gl(1,2);
      gl(2,2) = glstrain(2);

      // inverse of deformation gradient
      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> invdefgrd;
      invdefgrd.Invert(defgrd);

      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> temp;
      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> euler_almansi;
      temp.Multiply(gl,invdefgrd);
      euler_almansi.MultiplyTN(invdefgrd,temp);

      (*elestrain)(gp,0) = euler_almansi(0,0);
      (*elestrain)(gp,1) = euler_almansi(1,1);
      (*elestrain)(gp,2) = euler_almansi(2,2);
      (*elestrain)(gp,3) = euler_almansi(0,1);
      (*elestrain)(gp,4) = euler_almansi(1,2);
      (*elestrain)(gp,5) = euler_almansi(0,2);
    }
    break;
    case INPAR::STR::strain_none:
      break;
    default:
      dserror("requested strain type not available");
    }

    /* non-linear B-operator (may so be called, meaning
    ** of B-operator is not so sharp in the non-linear realm) *
    ** B = F . Bl *
    **
    **      [ ... | F_11*N_{,1}^k  F_21*N_{,1}^k  F_31*N_{,1}^k | ... ]
    **      [ ... | F_12*N_{,2}^k  F_22*N_{,2}^k  F_32*N_{,2}^k | ... ]
    **      [ ... | F_13*N_{,3}^k  F_23*N_{,3}^k  F_33*N_{,3}^k | ... ]
    ** B =  [ ~~~   ~~~~~~~~~~~~~  ~~~~~~~~~~~~~  ~~~~~~~~~~~~~   ~~~ ]
    **      [       F_11*N_{,2}^k+F_12*N_{,1}^k                       ]
    **      [ ... |          F_21*N_{,2}^k+F_22*N_{,1}^k        | ... ]
    **      [                       F_31*N_{,2}^k+F_32*N_{,1}^k       ]
    **      [                                                         ]
    **      [       F_12*N_{,3}^k+F_13*N_{,2}^k                       ]
    **      [ ... |          F_22*N_{,3}^k+F_23*N_{,2}^k        | ... ]
    **      [                       F_32*N_{,3}^k+F_33*N_{,2}^k       ]
    **      [                                                         ]
    **      [       F_13*N_{,1}^k+F_11*N_{,3}^k                       ]
    **      [ ... |          F_23*N_{,1}^k+F_21*N_{,3}^k        | ... ]
    **      [                       F_33*N_{,1}^k+F_31*N_{,3}^k       ]
    */
    LINALG::Matrix<NUMSTR_SOH8,NUMDOF_SOH8> bop;
    for (int i=0; i<NUMNOD_SOH8; ++i)
    {
      bop(0,NODDOF_SOH8*i+0) = defgrd(0,0)*N_XYZ(0,i);
      bop(0,NODDOF_SOH8*i+1) = defgrd(1,0)*N_XYZ(0,i);
      bop(0,NODDOF_SOH8*i+2) = defgrd(2,0)*N_XYZ(0,i);
      bop(1,NODDOF_SOH8*i+0) = defgrd(0,1)*N_XYZ(1,i);
      bop(1,NODDOF_SOH8*i+1) = defgrd(1,1)*N_XYZ(1,i);
      bop(1,NODDOF_SOH8*i+2) = defgrd(2,1)*N_XYZ(1,i);
      bop(2,NODDOF_SOH8*i+0) = defgrd(0,2)*N_XYZ(2,i);
      bop(2,NODDOF_SOH8*i+1) = defgrd(1,2)*N_XYZ(2,i);
      bop(2,NODDOF_SOH8*i+2) = defgrd(2,2)*N_XYZ(2,i);
      /* ~~~ */
      bop(3,NODDOF_SOH8*i+0) = defgrd(0,0)*N_XYZ(1,i) + defgrd(0,1)*N_XYZ(0,i);
      bop(3,NODDOF_SOH8*i+1) = defgrd(1,0)*N_XYZ(1,i) + defgrd(1,1)*N_XYZ(0,i);
      bop(3,NODDOF_SOH8*i+2) = defgrd(2,0)*N_XYZ(1,i) + defgrd(2,1)*N_XYZ(0,i);
      bop(4,NODDOF_SOH8*i+0) = defgrd(0,1)*N_XYZ(2,i) + defgrd(0,2)*N_XYZ(1,i);
      bop(4,NODDOF_SOH8*i+1) = defgrd(1,1)*N_XYZ(2,i) + defgrd(1,2)*N_XYZ(1,i);
      bop(4,NODDOF_SOH8*i+2) = defgrd(2,1)*N_XYZ(2,i) + defgrd(2,2)*N_XYZ(1,i);
      bop(5,NODDOF_SOH8*i+0) = defgrd(0,2)*N_XYZ(0,i) + defgrd(0,0)*N_XYZ(2,i);
      bop(5,NODDOF_SOH8*i+1) = defgrd(1,2)*N_XYZ(0,i) + defgrd(1,0)*N_XYZ(2,i);
      bop(5,NODDOF_SOH8*i+2) = defgrd(2,2)*N_XYZ(0,i) + defgrd(2,0)*N_XYZ(2,i);
    }

    /* call material law cccccccccccccccccccccccccccccccccccccccccccccccccccccc
    ** Here all possible material laws need to be incorporated,
    ** the stress vector, a C-matrix, and a density must be retrieved,
    ** every necessary data must be passed.
    */
    double density = 0.0;
    LINALG::Matrix<NUMSTR_SOH8,NUMSTR_SOH8> cmat(true);
    LINALG::Matrix<NUMSTR_SOH8,1> stress(true);
    LINALG::Matrix<NUMSTR_SOH8,1> plglstrain(true);
    soh8_mat_sel(&stress,&cmat,&density,&glstrain,&plglstrain,&defgrd,gp,params);
    // end of call material law ccccccccccccccccccccccccccccccccccccccccccccccc

    // return gp plastic strains (only in case of plastic strain output)
     switch (ioplstrain)
     {
     case INPAR::STR::strain_gl:
     {
       if (eleplstrain == NULL) dserror("plastic strain data not available");
       for (int i = 0; i < 3; ++i)
         (*eleplstrain)(gp,i) = plglstrain(i);
       for (int i = 3; i < 6; ++i)
         (*eleplstrain)(gp,i) = 0.5 * plglstrain(i);
     }
     break;
     case INPAR::STR::strain_ea:
     {
       if (eleplstrain == NULL) dserror("plastic strain data not available");
       // rewriting Green-Lagrange strains in matrix format
       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> gl;
       gl(0,0) = plglstrain(0);
       gl(0,1) = 0.5*plglstrain(3);
       gl(0,2) = 0.5*plglstrain(5);
       gl(1,0) = gl(0,1);
       gl(1,1) = plglstrain(1);
       gl(1,2) = 0.5*plglstrain(4);
       gl(2,0) = gl(0,2);
       gl(2,1) = gl(1,2);
       gl(2,2) = plglstrain(2);

       // inverse of deformation gradient
       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> invdefgrd;
       invdefgrd.Invert(defgrd);

       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> temp;
       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> euler_almansi;
       temp.Multiply(gl,invdefgrd);
       euler_almansi.MultiplyTN(invdefgrd,temp);

       (*eleplstrain)(gp,0) = euler_almansi(0,0);
       (*eleplstrain)(gp,1) = euler_almansi(1,1);
       (*eleplstrain)(gp,2) = euler_almansi(2,2);
       (*eleplstrain)(gp,3) = euler_almansi(0,1);
       (*eleplstrain)(gp,4) = euler_almansi(1,2);
       (*eleplstrain)(gp,5) = euler_almansi(0,2);
     }
     break;
     case INPAR::STR::strain_none:
       break;
     default:
       dserror("requested plastic strain type not available");
     }

    // return gp stresses
    switch (iostress)
    {
    case INPAR::STR::stress_2pk:
    {
      if (elestress == NULL) dserror("stress data not available");
      for (int i = 0; i < NUMSTR_SOH8; ++i)
        (*elestress)(gp,i) = stress(i);
    }
    break;
    case INPAR::STR::stress_cauchy:
    {
      if (elestress == NULL) dserror("stress data not available");
      const double detF = defgrd.Determinant();

      LINALG::Matrix<3,3> pkstress;
      pkstress(0,0) = stress(0);
      pkstress(0,1) = stress(3);
      pkstress(0,2) = stress(5);
      pkstress(1,0) = pkstress(0,1);
      pkstress(1,1) = stress(1);
      pkstress(1,2) = stress(4);
      pkstress(2,0) = pkstress(0,2);
      pkstress(2,1) = pkstress(1,2);
      pkstress(2,2) = stress(2);

      LINALG::Matrix<3,3> temp;
      LINALG::Matrix<3,3> cauchystress;
      temp.Multiply(1.0/detF,defgrd,pkstress,0.0);
      cauchystress.MultiplyNT(temp,defgrd);

      (*elestress)(gp,0) = cauchystress(0,0);
      (*elestress)(gp,1) = cauchystress(1,1);
      (*elestress)(gp,2) = cauchystress(2,2);
      (*elestress)(gp,3) = cauchystress(0,1);
      (*elestress)(gp,4) = cauchystress(1,2);
      (*elestress)(gp,5) = cauchystress(0,2);
    }
    break;
    case INPAR::STR::stress_none:
      break;
    default:
      dserror("requested stress type not available");
    }

    double detJ_w = detJ*gpweights[gp];
    if (force != NULL && stiffmatrix != NULL)
    {
      // integrate internal force vector f = f + (B^T . sigma) * detJ * w(gp)
      force->MultiplyTN(detJ_w, bop, stress, 1.0);
      // integrate `elastic' and `initial-displacement' stiffness matrix
      // keu = keu + (B^T . C . B) * detJ * w(gp)
      LINALG::Matrix<6,NUMDOF_SOH8> cb;
      cb.Multiply(cmat,bop);
      stiffmatrix->MultiplyTN(detJ_w,bop,cb,1.0);

      // integrate `geometric' stiffness matrix and add to keu *****************
      LINALG::Matrix<6,1> sfac(stress); // auxiliary integrated stress
      sfac.Scale(detJ_w); // detJ*w(gp)*[S11,S22,S33,S12=S21,S23=S32,S13=S31]
      vector<double> SmB_L(3); // intermediate Sm.B_L
      // kgeo += (B_L^T . sigma . B_L) * detJ * w(gp)  with B_L = Ni,Xj see NiliFEM-Skript
      for (int inod=0; inod<NUMNOD_SOH8; ++inod) {
        SmB_L[0] = sfac(0) * N_XYZ(0, inod) + sfac(3) * N_XYZ(1, inod)
            + sfac(5) * N_XYZ(2, inod);
        SmB_L[1] = sfac(3) * N_XYZ(0, inod) + sfac(1) * N_XYZ(1, inod)
            + sfac(4) * N_XYZ(2, inod);
        SmB_L[2] = sfac(5) * N_XYZ(0, inod) + sfac(4) * N_XYZ(1, inod)
            + sfac(2) * N_XYZ(2, inod);
        for (int jnod=0; jnod<NUMNOD_SOH8; ++jnod) {
          double bopstrbop = 0.0; // intermediate value
          for (int idim=0; idim<NUMDIM_SOH8; ++idim)
            bopstrbop += N_XYZ(idim, jnod) * SmB_L[idim];
          (*stiffmatrix)(3*inod+0,3*jnod+0) += bopstrbop;
          (*stiffmatrix)(3*inod+1,3*jnod+1) += bopstrbop;
          (*stiffmatrix)(3*inod+2,3*jnod+2) += bopstrbop;
        }
      } // end of integrate `geometric' stiffness******************************

      // EAS technology: integrate matrices --------------------------------- EAS
      if (eastype_ != soh8_easnone)
      {
        // integrate Kaa: Kaa += (M^T . cmat . M) * detJ * w(gp)
        // integrate Kda: Kda += (M^T . cmat . B) * detJ * w(gp)
        // integrate feas: feas += (M^T . sigma) * detJ *wp(gp)
        LINALG::SerialDenseMatrix cM(NUMSTR_SOH8, neas_); // temporary c . M
        switch(eastype_)
        {
        case DRT::ELEMENTS::So_hex8::soh8_easfull:
          LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,NUMSTR_SOH8,soh8_easfull>(cM.A(), cmat.A(), M.A());
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_easfull,NUMSTR_SOH8,soh8_easfull>(1.0, Kaa, detJ_w, M, cM);
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_easfull,NUMSTR_SOH8,NUMDOF_SOH8>(1.0, Kda.A(), detJ_w, M.A(), cb.A());
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_easfull,NUMSTR_SOH8,1>(1.0, feas.A(), detJ_w, M.A(), stress.A());
          break;
        case DRT::ELEMENTS::So_hex8::soh8_easmild:
          LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,NUMSTR_SOH8,soh8_easmild>(cM.A(), cmat.A(), M.A());
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_easmild,NUMSTR_SOH8,soh8_easmild>(1.0, Kaa, detJ_w, M, cM);
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_easmild,NUMSTR_SOH8,NUMDOF_SOH8>(1.0, Kda.A(), detJ_w, M.A(), cb.A());
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_easmild,NUMSTR_SOH8,1>(1.0, feas.A(), detJ_w, M.A(), stress.A());
          break;
        case DRT::ELEMENTS::So_hex8::soh8_eassosh8:
          LINALG::DENSEFUNCTIONS::multiply<double,NUMSTR_SOH8,NUMSTR_SOH8,soh8_eassosh8>(cM.A(), cmat.A(), M.A());
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_eassosh8,NUMSTR_SOH8,soh8_eassosh8>(1.0, Kaa, detJ_w, M, cM);
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_eassosh8,NUMSTR_SOH8,NUMDOF_SOH8>(1.0, Kda.A(), detJ_w, M.A(), cb.A());
          LINALG::DENSEFUNCTIONS::multiplyTN<double,soh8_eassosh8,NUMSTR_SOH8,1>(1.0, feas.A(), detJ_w, M.A(), stress.A());
          break;
        case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
        default: dserror("Don't know what to do with EAS type %d", eastype_); break;
        }
      } // ---------------------------------------------------------------- EAS
    }

    if (massmatrix != NULL) // evaluate mass matrix +++++++++++++++++++++++++
    {
      // integrate consistent mass matrix
      const double factor = detJ_w * density;
      double ifactor, massfactor;
      for (int inod=0; inod<NUMNOD_SOH8; ++inod)
      {
        ifactor = shapefcts[gp](inod) * factor;
        for (int jnod=0; jnod<NUMNOD_SOH8; ++jnod)
        {
          massfactor = shapefcts[gp](jnod) * ifactor;     // intermediate factor
          (*massmatrix)(NUMDIM_SOH8*inod+0,NUMDIM_SOH8*jnod+0) += massfactor;
          (*massmatrix)(NUMDIM_SOH8*inod+1,NUMDIM_SOH8*jnod+1) += massfactor;
          (*massmatrix)(NUMDIM_SOH8*inod+2,NUMDIM_SOH8*jnod+2) += massfactor;
        }
      }

    } // end of mass matrix +++++++++++++++++++++++++++++++++++++++++++++++++++
   /* =========================================================================*/
  }/* ==================================================== end of Loop over GP */
   /* =========================================================================*/

  if (force != NULL && stiffmatrix != NULL)
  {
    // EAS technology: ------------------------------------------------------ EAS
    // subtract EAS matrices from disp-based Kdd to "soften" element
    if (eastype_ != soh8_easnone)
    {
      // we need the inverse of Kaa
      Epetra_SerialDenseSolver solve_for_inverseKaa;
      solve_for_inverseKaa.SetMatrix(Kaa);
      solve_for_inverseKaa.Invert();
      // EAS-stiffness matrix is: Kdd - Kda^T . Kaa^-1 . Kda
      // EAS-internal force is: fint - Kda^T . Kaa^-1 . feas

      LINALG::SerialDenseMatrix KdaKaa(NUMDOF_SOH8, neas_); // temporary Kda.Kaa^{-1}
      switch(eastype_)
      {
      case DRT::ELEMENTS::So_hex8::soh8_easfull:
        LINALG::DENSEFUNCTIONS::multiplyTN<double,NUMDOF_SOH8,soh8_easfull,soh8_easfull>(KdaKaa, Kda, Kaa);
        LINALG::DENSEFUNCTIONS::multiply<double,NUMDOF_SOH8,soh8_easfull,NUMDOF_SOH8>(1.0, stiffmatrix->A(), -1.0, KdaKaa.A(), Kda.A());
        LINALG::DENSEFUNCTIONS::multiply<double,NUMDOF_SOH8,soh8_easfull,1>(1.0, force->A(), -1.0, KdaKaa.A(), feas.A());
        break;
      case DRT::ELEMENTS::So_hex8::soh8_easmild:
        LINALG::DENSEFUNCTIONS::multiplyTN<double,NUMDOF_SOH8,soh8_easmild,soh8_easmild>(KdaKaa, Kda, Kaa);
        LINALG::DENSEFUNCTIONS::multiply<double,NUMDOF_SOH8,soh8_easmild,NUMDOF_SOH8>(1.0, stiffmatrix->A(), -1.0, KdaKaa.A(), Kda.A());
        LINALG::DENSEFUNCTIONS::multiply<double,NUMDOF_SOH8,soh8_easmild,1>(1.0, force->A(), -1.0, KdaKaa.A(), feas.A());
        break;
      case DRT::ELEMENTS::So_hex8::soh8_eassosh8:
        LINALG::DENSEFUNCTIONS::multiplyTN<double,NUMDOF_SOH8,soh8_eassosh8,soh8_eassosh8>(KdaKaa, Kda, Kaa);
        LINALG::DENSEFUNCTIONS::multiply<double,NUMDOF_SOH8,soh8_eassosh8,NUMDOF_SOH8>(1.0, stiffmatrix->A(), -1.0, KdaKaa.A(), Kda.A());
        LINALG::DENSEFUNCTIONS::multiply<double,NUMDOF_SOH8,soh8_eassosh8,1>(1.0, force->A(), -1.0, KdaKaa.A(), feas.A());
        break;
      case DRT::ELEMENTS::So_hex8::soh8_easnone: break;
      default: dserror("Don't know what to do with EAS type %d", eastype_); break;
      }

      // store current EAS data in history
      for (int i=0; i<neas_; ++i)
      {
        for (int j=0; j<neas_; ++j)
          (*oldKaainv)(i,j) = Kaa(i,j);
        for (int j=0; j<NUMDOF_SOH8; ++j)
          (*oldKda)(i,j) = Kda(i,j);
        (*oldfeas)(i,0) = feas(i);
      }
    } // -------------------------------------------------------------------- EAS
  }
  return;
} // DRT::ELEMENTS::So_hex8::soh8_nlnstiffmass


/*----------------------------------------------------------------------*
 |  evaluate the element for GEMM (private)                   popp 09/11|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::soh8_nlnstiffmass_gemm(
      vector<int>&              lm,             // location matrix
      vector<double>&           dispo,          // old displacements
      vector<double>&           disp,           // current displacements
      vector<double>&           residual,       // current residual displ
      LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* stiffmatrix, // element stiffness matrix
      LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* massmatrix,  // element mass matrix
      LINALG::Matrix<NUMDOF_SOH8,1>* force,                 // element internal force vector
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8>* elestress,   // stresses at GP
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8>* elestrain,   // strains at GP
      LINALG::Matrix<NUMGPT_SOH8,NUMSTR_SOH8>* eleplstrain, // plastic strains at GP
      ParameterList&            params,         // algorithmic parameters e.g. time
      const INPAR::STR::StressType   iostress,  // stress output option
      const INPAR::STR::StrainType   iostrain,  // strain output option
      const INPAR::STR::StrainType   ioplstrain)  // plastic strain output option
{
/* ============================================================================*
** CONST SHAPE FUNCTIONS, DERIVATIVES and WEIGHTS for HEX_8 with 8 GAUSS POINTS*
** ============================================================================*/
  const static vector<LINALG::Matrix<NUMNOD_SOH8,1> > shapefcts = soh8_shapefcts();
  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();
  const static vector<double> gpweights = soh8_weights();
/* ============================================================================*/

  // check for prestressing or EAS
  if (pstype_ != INPAR::STR::prestress_none || eastype_ != soh8_easnone)
    dserror("GEMM for Sohex8 not (yet) compatible with EAS / prestressing!");

  // GEMM coefficients
  const double gemmalphaf = params.get<double>("alpha f");
  const double gemmxi = params.get<double>("xi");

  // update element geometry
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xrefe;  // material coord. of element
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xcurr;  // current  coord. of element
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xcurro; // old  coord. of element
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xdisp;

  DRT::Node** nodes = Nodes();
  for (int i=0; i<NUMNOD_SOH8; ++i)
  {
    const double* x = nodes[i]->X();
    xrefe(i,0) = x[0];
    xrefe(i,1) = x[1];
    xrefe(i,2) = x[2];

    xcurr(i,0) = xrefe(i,0) + disp[i*NODDOF_SOH8+0];
    xcurr(i,1) = xrefe(i,1) + disp[i*NODDOF_SOH8+1];
    xcurr(i,2) = xrefe(i,2) + disp[i*NODDOF_SOH8+2];

    xcurro(i,0) = xrefe(i,0) + dispo[i*NODDOF_SOH8+0];
    xcurro(i,1) = xrefe(i,1) + dispo[i*NODDOF_SOH8+1];
    xcurro(i,2) = xrefe(i,2) + dispo[i*NODDOF_SOH8+2];
  }

  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_XYZ;
  // build deformation gradient wrt to material configuration
  LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> defgrd(false);
  LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> defgrdo(false);
  for (int gp=0; gp<NUMGPT_SOH8; ++gp)
  {
    /* get the inverse of the Jacobian matrix which looks like:
    **            [ x_,r  y_,r  z_,r ]^-1
    **     J^-1 = [ x_,s  y_,s  z_,s ]
    **            [ x_,t  y_,t  z_,t ]
    */
    // compute derivatives N_XYZ at gp w.r.t. material coordinates
    // by N_XYZ = J^-1 * N_rst
    N_XYZ.Multiply(invJ_[gp],derivs[gp]);
    double detJ = detJ_[gp];

    // (material) deformation gradient F = d xcurr / d xrefe = xcurr^T * N_XYZ^T
    defgrd.MultiplyTT(xcurr,N_XYZ);
    defgrdo.MultiplyTT(xcurro,N_XYZ);

    // Right Cauchy-Green tensor = F^T * F
    LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> cauchygreen;
    LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> cauchygreeno;
    cauchygreen.MultiplyTN(defgrd,defgrd);
    cauchygreeno.MultiplyTN(defgrdo,defgrdo);

    // Green-Lagrange strains matrix E = 0.5 * (Cauchygreen - Identity)
    // GL strain vector glstrain={E11,E22,E33,2*E12,2*E23,2*E31}
    Epetra_SerialDenseVector glstrain_epetra(NUMSTR_SOH8);
    LINALG::Matrix<NUMSTR_SOH8,1> glstrain(glstrain_epetra.A(),true);
    Epetra_SerialDenseVector glstrain_epetrao(NUMSTR_SOH8);
    LINALG::Matrix<NUMSTR_SOH8,1> glstraino(glstrain_epetrao.A(),true);
    glstrain(0) = 0.5 * (cauchygreen(0,0) - 1.0);
    glstrain(1) = 0.5 * (cauchygreen(1,1) - 1.0);
    glstrain(2) = 0.5 * (cauchygreen(2,2) - 1.0);
    glstrain(3) = cauchygreen(0,1);
    glstrain(4) = cauchygreen(1,2);
    glstrain(5) = cauchygreen(2,0);
    glstraino(0) = 0.5 * (cauchygreeno(0,0) - 1.0);
    glstraino(1) = 0.5 * (cauchygreeno(1,1) - 1.0);
    glstraino(2) = 0.5 * (cauchygreeno(2,2) - 1.0);
    glstraino(3) = cauchygreeno(0,1);
    glstraino(4) = cauchygreeno(1,2);
    glstraino(5) = cauchygreeno(2,0);

    // return gp strains (only in case of stress/strain output)
    switch (iostrain)
    {
    case INPAR::STR::strain_gl:
    {
      if (elestrain == NULL) dserror("strain data not available");
      for (int i = 0; i < 3; ++i)
        (*elestrain)(gp,i) = glstrain(i);
      for (int i = 3; i < 6; ++i)
        (*elestrain)(gp,i) = 0.5 * glstrain(i);
    }
    break;
    case INPAR::STR::strain_ea:
    {
      if (elestrain == NULL) dserror("strain data not available");
      // rewriting Green-Lagrange strains in matrix format
      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> gl;
      gl(0,0) = glstrain(0);
      gl(0,1) = 0.5*glstrain(3);
      gl(0,2) = 0.5*glstrain(5);
      gl(1,0) = gl(0,1);
      gl(1,1) = glstrain(1);
      gl(1,2) = 0.5*glstrain(4);
      gl(2,0) = gl(0,2);
      gl(2,1) = gl(1,2);
      gl(2,2) = glstrain(2);

      // inverse of deformation gradient
      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> invdefgrd;
      invdefgrd.Invert(defgrd);

      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> temp;
      LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> euler_almansi;
      temp.Multiply(gl,invdefgrd);
      euler_almansi.MultiplyTN(invdefgrd,temp);

      (*elestrain)(gp,0) = euler_almansi(0,0);
      (*elestrain)(gp,1) = euler_almansi(1,1);
      (*elestrain)(gp,2) = euler_almansi(2,2);
      (*elestrain)(gp,3) = euler_almansi(0,1);
      (*elestrain)(gp,4) = euler_almansi(1,2);
      (*elestrain)(gp,5) = euler_almansi(0,2);
    }
    break;
    case INPAR::STR::strain_none:
      break;
    default:
      dserror("requested strain type not available");
    }

    /* non-linear B-operator (may so be called, meaning
    ** of B-operator is not so sharp in the non-linear realm) *
    ** B = F . Bl *
    **
    **      [ ... | F_11*N_{,1}^k  F_21*N_{,1}^k  F_31*N_{,1}^k | ... ]
    **      [ ... | F_12*N_{,2}^k  F_22*N_{,2}^k  F_32*N_{,2}^k | ... ]
    **      [ ... | F_13*N_{,3}^k  F_23*N_{,3}^k  F_33*N_{,3}^k | ... ]
    ** B =  [ ~~~   ~~~~~~~~~~~~~  ~~~~~~~~~~~~~  ~~~~~~~~~~~~~   ~~~ ]
    **      [       F_11*N_{,2}^k+F_12*N_{,1}^k                       ]
    **      [ ... |          F_21*N_{,2}^k+F_22*N_{,1}^k        | ... ]
    **      [                       F_31*N_{,2}^k+F_32*N_{,1}^k       ]
    **      [                                                         ]
    **      [       F_12*N_{,3}^k+F_13*N_{,2}^k                       ]
    **      [ ... |          F_22*N_{,3}^k+F_23*N_{,2}^k        | ... ]
    **      [                       F_32*N_{,3}^k+F_33*N_{,2}^k       ]
    **      [                                                         ]
    **      [       F_13*N_{,1}^k+F_11*N_{,3}^k                       ]
    **      [ ... |          F_23*N_{,1}^k+F_21*N_{,3}^k        | ... ]
    **      [                       F_33*N_{,1}^k+F_31*N_{,3}^k       ]
    */
    LINALG::Matrix<NUMSTR_SOH8,NUMDOF_SOH8> bop;
    for (int i=0; i<NUMNOD_SOH8; ++i)
    {
      bop(0,NODDOF_SOH8*i+0) = defgrd(0,0)*N_XYZ(0,i);
      bop(0,NODDOF_SOH8*i+1) = defgrd(1,0)*N_XYZ(0,i);
      bop(0,NODDOF_SOH8*i+2) = defgrd(2,0)*N_XYZ(0,i);
      bop(1,NODDOF_SOH8*i+0) = defgrd(0,1)*N_XYZ(1,i);
      bop(1,NODDOF_SOH8*i+1) = defgrd(1,1)*N_XYZ(1,i);
      bop(1,NODDOF_SOH8*i+2) = defgrd(2,1)*N_XYZ(1,i);
      bop(2,NODDOF_SOH8*i+0) = defgrd(0,2)*N_XYZ(2,i);
      bop(2,NODDOF_SOH8*i+1) = defgrd(1,2)*N_XYZ(2,i);
      bop(2,NODDOF_SOH8*i+2) = defgrd(2,2)*N_XYZ(2,i);
      /* ~~~ */
      bop(3,NODDOF_SOH8*i+0) = defgrd(0,0)*N_XYZ(1,i) + defgrd(0,1)*N_XYZ(0,i);
      bop(3,NODDOF_SOH8*i+1) = defgrd(1,0)*N_XYZ(1,i) + defgrd(1,1)*N_XYZ(0,i);
      bop(3,NODDOF_SOH8*i+2) = defgrd(2,0)*N_XYZ(1,i) + defgrd(2,1)*N_XYZ(0,i);
      bop(4,NODDOF_SOH8*i+0) = defgrd(0,1)*N_XYZ(2,i) + defgrd(0,2)*N_XYZ(1,i);
      bop(4,NODDOF_SOH8*i+1) = defgrd(1,1)*N_XYZ(2,i) + defgrd(1,2)*N_XYZ(1,i);
      bop(4,NODDOF_SOH8*i+2) = defgrd(2,1)*N_XYZ(2,i) + defgrd(2,2)*N_XYZ(1,i);
      bop(5,NODDOF_SOH8*i+0) = defgrd(0,2)*N_XYZ(0,i) + defgrd(0,0)*N_XYZ(2,i);
      bop(5,NODDOF_SOH8*i+1) = defgrd(1,2)*N_XYZ(0,i) + defgrd(1,0)*N_XYZ(2,i);
      bop(5,NODDOF_SOH8*i+2) = defgrd(2,2)*N_XYZ(0,i) + defgrd(2,0)*N_XYZ(2,i);
    }
    LINALG::Matrix<NUMSTR_SOH8,NUMDOF_SOH8> bopo;
    for (int i=0; i<NUMNOD_SOH8; ++i)
    {
      bopo(0,NODDOF_SOH8*i+0) = defgrdo(0,0)*N_XYZ(0,i);
      bopo(0,NODDOF_SOH8*i+1) = defgrdo(1,0)*N_XYZ(0,i);
      bopo(0,NODDOF_SOH8*i+2) = defgrdo(2,0)*N_XYZ(0,i);
      bopo(1,NODDOF_SOH8*i+0) = defgrdo(0,1)*N_XYZ(1,i);
      bopo(1,NODDOF_SOH8*i+1) = defgrdo(1,1)*N_XYZ(1,i);
      bopo(1,NODDOF_SOH8*i+2) = defgrdo(2,1)*N_XYZ(1,i);
      bopo(2,NODDOF_SOH8*i+0) = defgrdo(0,2)*N_XYZ(2,i);
      bopo(2,NODDOF_SOH8*i+1) = defgrdo(1,2)*N_XYZ(2,i);
      bopo(2,NODDOF_SOH8*i+2) = defgrdo(2,2)*N_XYZ(2,i);
      /* ~~~ */
      bopo(3,NODDOF_SOH8*i+0) = defgrdo(0,0)*N_XYZ(1,i) + defgrdo(0,1)*N_XYZ(0,i);
      bopo(3,NODDOF_SOH8*i+1) = defgrdo(1,0)*N_XYZ(1,i) + defgrdo(1,1)*N_XYZ(0,i);
      bopo(3,NODDOF_SOH8*i+2) = defgrdo(2,0)*N_XYZ(1,i) + defgrdo(2,1)*N_XYZ(0,i);
      bopo(4,NODDOF_SOH8*i+0) = defgrdo(0,1)*N_XYZ(2,i) + defgrdo(0,2)*N_XYZ(1,i);
      bopo(4,NODDOF_SOH8*i+1) = defgrdo(1,1)*N_XYZ(2,i) + defgrdo(1,2)*N_XYZ(1,i);
      bopo(4,NODDOF_SOH8*i+2) = defgrdo(2,1)*N_XYZ(2,i) + defgrdo(2,2)*N_XYZ(1,i);
      bopo(5,NODDOF_SOH8*i+0) = defgrdo(0,2)*N_XYZ(0,i) + defgrdo(0,0)*N_XYZ(2,i);
      bopo(5,NODDOF_SOH8*i+1) = defgrdo(1,2)*N_XYZ(0,i) + defgrdo(1,0)*N_XYZ(2,i);
      bopo(5,NODDOF_SOH8*i+2) = defgrdo(2,2)*N_XYZ(0,i) + defgrdo(2,0)*N_XYZ(2,i);
    }

    // GEMM: computed averaged mid-point quantities

    // non-linear mid-B-operator
    // B_m = (1.0-gemmalphaf)*B_{n+1} + gemmalphaf*B_{n}
    LINALG::Matrix<NUMSTR_SOH8,NUMDOF_SOH8> bopm;
    bopm.Update(1.0-gemmalphaf,bop,gemmalphaf,bopo);

    // mid-strain GL vector
    // E_m = (1.0-gemmalphaf+gemmxi)*E_{n+1} + (gemmalphaf-gemmxi)*E_n
    Epetra_SerialDenseVector glstrain_epetram(NUMSTR_SOH8);
    LINALG::Matrix<NUMSTR_SOH8,1> glstrainm(glstrain_epetram.A(),true);
    glstrainm.Update(1.0-gemmalphaf+gemmxi,glstrain,gemmalphaf-gemmxi,glstraino);

    /* call material law cccccccccccccccccccccccccccccccccccccccccccccccccccccc
    ** Here all possible material laws need to be incorporated,
    ** the stress vector, a C-matrix, and a density must be retrieved,
    ** every necessary data must be passed.
    */
    double density = 0.0;
    LINALG::Matrix<NUMSTR_SOH8,NUMSTR_SOH8> cmat(true);
    LINALG::Matrix<NUMSTR_SOH8,1> stressm(true);
    LINALG::Matrix<NUMSTR_SOH8,1> plglstrain(true);
    // call material law
    RCP<MAT::Material> mat = Material();
    if (mat->MaterialType() == INPAR::MAT::m_stvenant)
      soh8_mat_sel(&stressm,&cmat,&density,&glstrainm,&plglstrain,&defgrd,gp,params);
    else
      dserror("It must be St.Venant-Kirchhoff material for GEMM.");
    // end of call material law ccccccccccccccccccccccccccccccccccccccccccccccc

    // return gp plastic strains (only in case of plastic strain output)
     switch (ioplstrain)
     {
     case INPAR::STR::strain_gl:
     {
       if (eleplstrain == NULL) dserror("plastic strain data not available");
       for (int i = 0; i < 3; ++i)
         (*eleplstrain)(gp,i) = plglstrain(i);
       for (int i = 3; i < 6; ++i)
         (*eleplstrain)(gp,i) = 0.5 * plglstrain(i);
     }
     break;
     case INPAR::STR::strain_ea:
     {
       if (eleplstrain == NULL) dserror("plastic strain data not available");
       // rewriting Green-Lagrange strains in matrix format
       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> gl;
       gl(0,0) = plglstrain(0);
       gl(0,1) = 0.5*plglstrain(3);
       gl(0,2) = 0.5*plglstrain(5);
       gl(1,0) = gl(0,1);
       gl(1,1) = plglstrain(1);
       gl(1,2) = 0.5*plglstrain(4);
       gl(2,0) = gl(0,2);
       gl(2,1) = gl(1,2);
       gl(2,2) = plglstrain(2);

       // inverse of deformation gradient
       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> invdefgrd;
       invdefgrd.Invert(defgrd);

       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> temp;
       LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> euler_almansi;
       temp.Multiply(gl,invdefgrd);
       euler_almansi.MultiplyTN(invdefgrd,temp);

       (*eleplstrain)(gp,0) = euler_almansi(0,0);
       (*eleplstrain)(gp,1) = euler_almansi(1,1);
       (*eleplstrain)(gp,2) = euler_almansi(2,2);
       (*eleplstrain)(gp,3) = euler_almansi(0,1);
       (*eleplstrain)(gp,4) = euler_almansi(1,2);
       (*eleplstrain)(gp,5) = euler_almansi(0,2);
     }
     break;
     case INPAR::STR::strain_none:
       break;
     default:
       dserror("requested plastic strain type not available");
     }

    // return gp stresses
    switch (iostress)
    {
    case INPAR::STR::stress_2pk:
    {
      if (elestress == NULL) dserror("stress data not available");
      for (int i = 0; i < NUMSTR_SOH8; ++i)
        (*elestress)(gp,i) = stressm(i);
    }
    break;
    case INPAR::STR::stress_cauchy:
    {
      if (elestress == NULL) dserror("stress data not available");
      const double detF = defgrd.Determinant();

      LINALG::Matrix<3,3> pkstress;
      pkstress(0,0) = stressm(0);
      pkstress(0,1) = stressm(3);
      pkstress(0,2) = stressm(5);
      pkstress(1,0) = pkstress(0,1);
      pkstress(1,1) = stressm(1);
      pkstress(1,2) = stressm(4);
      pkstress(2,0) = pkstress(0,2);
      pkstress(2,1) = pkstress(1,2);
      pkstress(2,2) = stressm(2);

      LINALG::Matrix<3,3> temp;
      LINALG::Matrix<3,3> cauchystress;
      temp.Multiply(1.0/detF,defgrd,pkstress,0.0);
      cauchystress.MultiplyNT(temp,defgrd);

      (*elestress)(gp,0) = cauchystress(0,0);
      (*elestress)(gp,1) = cauchystress(1,1);
      (*elestress)(gp,2) = cauchystress(2,2);
      (*elestress)(gp,3) = cauchystress(0,1);
      (*elestress)(gp,4) = cauchystress(1,2);
      (*elestress)(gp,5) = cauchystress(0,2);
    }
    break;
    case INPAR::STR::stress_none:
      break;
    default:
      dserror("requested stress type not available");
    }

    double detJ_w = detJ*gpweights[gp];
    if (force != NULL && stiffmatrix != NULL)
    {
      // integrate internal force vector f = f + (B^T . sigma) * detJ * w(gp)
      force->MultiplyTN(detJ_w, bopm, stressm, 1.0);

      // integrate `elastic' and `initial-displacement' stiffness matrix
      // keu = keu + (B^T . C . B) * detJ * w(gp)
      const double faceu = (1.0-gemmalphaf+gemmxi) * detJ_w;
      LINALG::Matrix<6,NUMDOF_SOH8> cb;
      cb.Multiply(cmat,bop); // B_{n+1} here!!!
      stiffmatrix->MultiplyTN(faceu,bopm,cb,1.0); // B_m here!!!

      // integrate `geometric' stiffness matrix
      const double facg = (1.0-gemmalphaf) * detJ_w;
      LINALG::Matrix<6,1> sfac(stressm); // auxiliary integrated stress
      sfac.Scale(facg); // detJ*w(gp)*[S11,S22,S33,S12=S21,S23=S32,S13=S31]
      vector<double> SmB_L(3); // intermediate Sm.B_L
      // kgeo += (B_L^T . sigma . B_L) * detJ * w(gp)  with B_L = Ni,Xj see NiliFEM-Skript
      for (int inod=0; inod<NUMNOD_SOH8; ++inod) {
        SmB_L[0] = sfac(0) * N_XYZ(0, inod) + sfac(3) * N_XYZ(1, inod)
            + sfac(5) * N_XYZ(2, inod);
        SmB_L[1] = sfac(3) * N_XYZ(0, inod) + sfac(1) * N_XYZ(1, inod)
            + sfac(4) * N_XYZ(2, inod);
        SmB_L[2] = sfac(5) * N_XYZ(0, inod) + sfac(4) * N_XYZ(1, inod)
            + sfac(2) * N_XYZ(2, inod);
        for (int jnod=0; jnod<NUMNOD_SOH8; ++jnod) {
          double bopstrbop = 0.0; // intermediate value
          for (int idim=0; idim<NUMDIM_SOH8; ++idim)
            bopstrbop += N_XYZ(idim, jnod) * SmB_L[idim];
          (*stiffmatrix)(3*inod+0,3*jnod+0) += bopstrbop;
          (*stiffmatrix)(3*inod+1,3*jnod+1) += bopstrbop;
          (*stiffmatrix)(3*inod+2,3*jnod+2) += bopstrbop;
        }
      } // end of integrate `geometric' stiffness******************************
    }

    if (massmatrix != NULL) // evaluate mass matrix +++++++++++++++++++++++++
    {
      // integrate consistent mass matrix
      const double factor = detJ_w * density;
      double ifactor, massfactor;
      for (int inod=0; inod<NUMNOD_SOH8; ++inod)
      {
        ifactor = shapefcts[gp](inod) * factor;
        for (int jnod=0; jnod<NUMNOD_SOH8; ++jnod)
        {
          massfactor = shapefcts[gp](jnod) * ifactor;     // intermediate factor
          (*massmatrix)(NUMDIM_SOH8*inod+0,NUMDIM_SOH8*jnod+0) += massfactor;
          (*massmatrix)(NUMDIM_SOH8*inod+1,NUMDIM_SOH8*jnod+1) += massfactor;
          (*massmatrix)(NUMDIM_SOH8*inod+2,NUMDIM_SOH8*jnod+2) += massfactor;
        }
      }

    } // end of mass matrix +++++++++++++++++++++++++++++++++++++++++++++++++++
   /* =========================================================================*/
  }/* ==================================================== end of Loop over GP */
   /* =========================================================================*/

  return;
} // DRT::ELEMENTS::So_hex8::soh8_nlnstiffmass_gemm

/*----------------------------------------------------------------------*
 |  lump mass matrix (private)                               bborn 07/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::soh8_lumpmass(LINALG::Matrix<NUMDOF_SOH8,NUMDOF_SOH8>* emass)
{
  // lump mass matrix
  if (emass != NULL)
  {
    // we assume #elemat2 is a square matrix
    for (unsigned int c=0; c<(*emass).N(); ++c)  // parse columns
    {
      double d = 0.0;
      for (unsigned int r=0; r<(*emass).M(); ++r)  // parse rows
      {
        d += (*emass)(r,c);  // accumulate row entries
        (*emass)(r,c) = 0.0;
      }
      (*emass)(c,c) = d;  // apply sum of row entries on diagonal
    }
  }
}

/*----------------------------------------------------------------------*
 |  Evaluate Hex8 Shape fcts at all 8 Gauss Points             maf 05/08|
 *----------------------------------------------------------------------*/
const vector<LINALG::Matrix<NUMNOD_SOH8,1> > DRT::ELEMENTS::So_hex8::soh8_shapefcts()
{
  vector<LINALG::Matrix<NUMNOD_SOH8,1> > shapefcts(NUMGPT_SOH8);
  // (r,s,t) gp-locations of fully integrated linear 8-node Hex
  const double gploc    = 1.0/sqrt(3.0);    // gp sampling point value for linear fct
  const double r[NUMGPT_SOH8] = {-gploc, gploc, gploc,-gploc,-gploc, gploc, gploc,-gploc};
  const double s[NUMGPT_SOH8] = {-gploc,-gploc, gploc, gploc,-gploc,-gploc, gploc, gploc};
  const double t[NUMGPT_SOH8] = {-gploc,-gploc,-gploc,-gploc, gploc, gploc, gploc, gploc};
  // fill up nodal f at each gp
  for (int i=0; i<NUMGPT_SOH8; ++i) {
    (shapefcts[i])(0) = (1.0-r[i])*(1.0-s[i])*(1.0-t[i])*0.125;
    (shapefcts[i])(1) = (1.0+r[i])*(1.0-s[i])*(1.0-t[i])*0.125;
    (shapefcts[i])(2) = (1.0+r[i])*(1.0+s[i])*(1.0-t[i])*0.125;
    (shapefcts[i])(3) = (1.0-r[i])*(1.0+s[i])*(1.0-t[i])*0.125;
    (shapefcts[i])(4) = (1.0-r[i])*(1.0-s[i])*(1.0+t[i])*0.125;
    (shapefcts[i])(5) = (1.0+r[i])*(1.0-s[i])*(1.0+t[i])*0.125;
    (shapefcts[i])(6) = (1.0+r[i])*(1.0+s[i])*(1.0+t[i])*0.125;
    (shapefcts[i])(7) = (1.0-r[i])*(1.0+s[i])*(1.0+t[i])*0.125;
  }
  return shapefcts;
}


/*----------------------------------------------------------------------*
 |  Evaluate Hex8 Shape fct derivs at all 8 Gauss Points       maf 05/08|
 *----------------------------------------------------------------------*/
const vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > DRT::ELEMENTS::So_hex8::soh8_derivs()
{
  vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs(NUMGPT_SOH8);
  // (r,s,t) gp-locations of fully integrated linear 8-node Hex
  const double gploc    = 1.0/sqrt(3.0);    // gp sampling point value for linear fct
  const double r[NUMGPT_SOH8] = {-gploc, gploc, gploc,-gploc,-gploc, gploc, gploc,-gploc};
  const double s[NUMGPT_SOH8] = {-gploc,-gploc, gploc, gploc,-gploc,-gploc, gploc, gploc};
  const double t[NUMGPT_SOH8] = {-gploc,-gploc,-gploc,-gploc, gploc, gploc, gploc, gploc};
  // fill up df w.r.t. rst directions (NUMDIM) at each gp
  for (int i=0; i<NUMGPT_SOH8; ++i) {
    // df wrt to r for each node(0..7) at each gp [i]
    (derivs[i])(0,0) = -(1.0-s[i])*(1.0-t[i])*0.125;
    (derivs[i])(0,1) =  (1.0-s[i])*(1.0-t[i])*0.125;
    (derivs[i])(0,2) =  (1.0+s[i])*(1.0-t[i])*0.125;
    (derivs[i])(0,3) = -(1.0+s[i])*(1.0-t[i])*0.125;
    (derivs[i])(0,4) = -(1.0-s[i])*(1.0+t[i])*0.125;
    (derivs[i])(0,5) =  (1.0-s[i])*(1.0+t[i])*0.125;
    (derivs[i])(0,6) =  (1.0+s[i])*(1.0+t[i])*0.125;
    (derivs[i])(0,7) = -(1.0+s[i])*(1.0+t[i])*0.125;

    // df wrt to s for each node(0..7) at each gp [i]
    (derivs[i])(1,0) = -(1.0-r[i])*(1.0-t[i])*0.125;
    (derivs[i])(1,1) = -(1.0+r[i])*(1.0-t[i])*0.125;
    (derivs[i])(1,2) =  (1.0+r[i])*(1.0-t[i])*0.125;
    (derivs[i])(1,3) =  (1.0-r[i])*(1.0-t[i])*0.125;
    (derivs[i])(1,4) = -(1.0-r[i])*(1.0+t[i])*0.125;
    (derivs[i])(1,5) = -(1.0+r[i])*(1.0+t[i])*0.125;
    (derivs[i])(1,6) =  (1.0+r[i])*(1.0+t[i])*0.125;
    (derivs[i])(1,7) =  (1.0-r[i])*(1.0+t[i])*0.125;

    // df wrt to t for each node(0..7) at each gp [i]
    (derivs[i])(2,0) = -(1.0-r[i])*(1.0-s[i])*0.125;
    (derivs[i])(2,1) = -(1.0+r[i])*(1.0-s[i])*0.125;
    (derivs[i])(2,2) = -(1.0+r[i])*(1.0+s[i])*0.125;
    (derivs[i])(2,3) = -(1.0-r[i])*(1.0+s[i])*0.125;
    (derivs[i])(2,4) =  (1.0-r[i])*(1.0-s[i])*0.125;
    (derivs[i])(2,5) =  (1.0+r[i])*(1.0-s[i])*0.125;
    (derivs[i])(2,6) =  (1.0+r[i])*(1.0+s[i])*0.125;
    (derivs[i])(2,7) =  (1.0-r[i])*(1.0+s[i])*0.125;
  }
  return derivs;
}

/*----------------------------------------------------------------------*
 |  Evaluate Hex8 Weights at all 8 Gauss Points                maf 05/08|
 *----------------------------------------------------------------------*/
const vector<double> DRT::ELEMENTS::So_hex8::soh8_weights()
{
  vector<double> weights(NUMGPT_SOH8);
  for (int i = 0; i < NUMGPT_SOH8; ++i) {
    weights[i] = 1.0;
  }
  return weights;
}

/*----------------------------------------------------------------------*
 |  shape functions and derivatives for So_hex8                maf 04/07|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::soh8_shapederiv(
      LINALG::Matrix<NUMNOD_SOH8,NUMGPT_SOH8>** shapefct,   // pointer to pointer of shapefct
      LINALG::Matrix<NUMDOF_SOH8,NUMNOD_SOH8>** deriv,     // pointer to pointer of derivs
      LINALG::Matrix<NUMGPT_SOH8,1>** weights)   // pointer to pointer of weights
{
  // static matrix objects, kept in memory
  static LINALG::Matrix<NUMNOD_SOH8,NUMGPT_SOH8>  f;  // shape functions
  static LINALG::Matrix<NUMDOF_SOH8,NUMNOD_SOH8> df;  // derivatives
  static LINALG::Matrix<NUMGPT_SOH8,1> weightfactors;   // weights for each gp
  static bool fdf_eval;                      // flag for re-evaluate everything

  const double gploc    = 1.0/sqrt(3.0);    // gp sampling point value for linear fct
  const double gpw      = 1.0;              // weight at every gp for linear fct

  if (fdf_eval==true){             // if true f,df already evaluated
    *shapefct = &f;             // return adress of static object to target of pointer
    *deriv    = &df;            // return adress of static object to target of pointer
    *weights  = &weightfactors; // return adress of static object to target of pointer
    return;
  } else {
    // (r,s,t) gp-locations of fully integrated linear 8-node Hex
    const double r[NUMGPT_SOH8] = {-gploc, gploc, gploc,-gploc,-gploc, gploc, gploc,-gploc};
    const double s[NUMGPT_SOH8] = {-gploc,-gploc, gploc, gploc,-gploc,-gploc, gploc, gploc};
    const double t[NUMGPT_SOH8] = {-gploc,-gploc,-gploc,-gploc, gploc, gploc, gploc, gploc};
    const double w[NUMGPT_SOH8] = {   gpw,   gpw,   gpw,   gpw,   gpw,   gpw,   gpw,   gpw};

    // fill up nodal f at each gp
    for (int i=0; i<NUMGPT_SOH8; ++i) {
      f(0,i) = (1.0-r[i])*(1.0-s[i])*(1.0-t[i])*0.125;
      f(1,i) = (1.0+r[i])*(1.0-s[i])*(1.0-t[i])*0.125;
      f(2,i) = (1.0+r[i])*(1.0+s[i])*(1.0-t[i])*0.125;
      f(3,i) = (1.0-r[i])*(1.0+s[i])*(1.0-t[i])*0.125;
      f(4,i) = (1.0-r[i])*(1.0-s[i])*(1.0+t[i])*0.125;
      f(5,i) = (1.0+r[i])*(1.0-s[i])*(1.0+t[i])*0.125;
      f(6,i) = (1.0+r[i])*(1.0+s[i])*(1.0+t[i])*0.125;
      f(7,i) = (1.0-r[i])*(1.0+s[i])*(1.0+t[i])*0.125;
      weightfactors(i) = w[i]*w[i]*w[i]; // just for clarity how to get weight factors
    }

    // fill up df w.r.t. rst directions (NUMDIM) at each gp
    for (int i=0; i<NUMGPT_SOH8; ++i) {
        // df wrt to r "+0" for each node(0..7) at each gp [i]
        df(NUMDIM_SOH8*i+0,0) = -(1.0-s[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+0,1) =  (1.0-s[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+0,2) =  (1.0+s[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+0,3) = -(1.0+s[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+0,4) = -(1.0-s[i])*(1.0+t[i])*0.125;
        df(NUMDIM_SOH8*i+0,5) =  (1.0-s[i])*(1.0+t[i])*0.125;
        df(NUMDIM_SOH8*i+0,6) =  (1.0+s[i])*(1.0+t[i])*0.125;
        df(NUMDIM_SOH8*i+0,7) = -(1.0+s[i])*(1.0+t[i])*0.125;

        // df wrt to s "+1" for each node(0..7) at each gp [i]
        df(NUMDIM_SOH8*i+1,0) = -(1.0-r[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+1,1) = -(1.0+r[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+1,2) =  (1.0+r[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+1,3) =  (1.0-r[i])*(1.0-t[i])*0.125;
        df(NUMDIM_SOH8*i+1,4) = -(1.0-r[i])*(1.0+t[i])*0.125;
        df(NUMDIM_SOH8*i+1,5) = -(1.0+r[i])*(1.0+t[i])*0.125;
        df(NUMDIM_SOH8*i+1,6) =  (1.0+r[i])*(1.0+t[i])*0.125;
        df(NUMDIM_SOH8*i+1,7) =  (1.0-r[i])*(1.0+t[i])*0.125;

        // df wrt to t "+2" for each node(0..7) at each gp [i]
        df(NUMDIM_SOH8*i+2,0) = -(1.0-r[i])*(1.0-s[i])*0.125;
        df(NUMDIM_SOH8*i+2,1) = -(1.0+r[i])*(1.0-s[i])*0.125;
        df(NUMDIM_SOH8*i+2,2) = -(1.0+r[i])*(1.0+s[i])*0.125;
        df(NUMDIM_SOH8*i+2,3) = -(1.0-r[i])*(1.0+s[i])*0.125;
        df(NUMDIM_SOH8*i+2,4) =  (1.0-r[i])*(1.0-s[i])*0.125;
        df(NUMDIM_SOH8*i+2,5) =  (1.0+r[i])*(1.0-s[i])*0.125;
        df(NUMDIM_SOH8*i+2,6) =  (1.0+r[i])*(1.0+s[i])*0.125;
        df(NUMDIM_SOH8*i+2,7) =  (1.0-r[i])*(1.0+s[i])*0.125;
    }

    // return adresses of just evaluated matrices
    *shapefct = &f;             // return adress of static object to target of pointer
    *deriv = &df;               // return adress of static object to target of pointer
    *weights  = &weightfactors; // return adress of static object to target of pointer
    fdf_eval = true;               // now all arrays are filled statically
  }
  return;
}  // of soh8_shapederiv


/*----------------------------------------------------------------------*
 |  init the element (public)                                  gee 04/08|
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::So_hex8Type::Initialize(DRT::Discretization& dis)
{
  for (int i=0; i<dis.NumMyColElements(); ++i)
  {
    if (dis.lColElement(i)->ElementType() != *this) continue;
    DRT::ELEMENTS::So_hex8* actele = dynamic_cast<DRT::ELEMENTS::So_hex8*>(dis.lColElement(i));
    if (!actele) dserror("cast to So_hex8* failed");
    actele->InitJacobianMapping();
  }
  return 0;
}

/*----------------------------------------------------------------------*
 |  compute def gradient at every gaussian point (protected)   gee 07/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::DefGradient(const vector<double>& disp,
                                         Epetra_SerialDenseMatrix& gpdefgrd,
                                         DRT::ELEMENTS::PreStress& prestress)
{
  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();

  // update element geometry
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xdisp;  // current  coord. of element
  for (int i=0; i<NUMNOD_SOH8; ++i)
  {
    xdisp(i,0) = disp[i*NODDOF_SOH8+0];
    xdisp(i,1) = disp[i*NODDOF_SOH8+1];
    xdisp(i,2) = disp[i*NODDOF_SOH8+2];
  }

  for (int gp=0; gp<NUMGPT_SOH8; ++gp)
  {
    // get Jacobian mapping wrt to the stored deformed configuration
    LINALG::Matrix<3,3> invJdef;
    prestress.StoragetoMatrix(gp,invJdef,prestress.JHistory());

    // by N_XYZ = J^-1 * N_rst
    LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_xyz;
    N_xyz.Multiply(invJdef,derivs[gp]);

    // build defgrd (independent of xrefe!)
    LINALG::Matrix<3,3> defgrd;
    defgrd.MultiplyTT(xdisp,N_xyz);
    defgrd(0,0) += 1.0;
    defgrd(1,1) += 1.0;
    defgrd(2,2) += 1.0;

    prestress.MatrixtoStorage(gp,defgrd,gpdefgrd);
  }
  return;
}

/*----------------------------------------------------------------------*
 |  compute Jac.mapping wrt deformed configuration (protected) gee 07/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::UpdateJacobianMapping(
                                            const vector<double>& disp,
                                            DRT::ELEMENTS::PreStress& prestress)
{
  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();

  // get incremental disp
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xdisp;
  for (int i=0; i<NUMNOD_SOH8; ++i)
  {
    xdisp(i,0) = disp[i*NODDOF_SOH8+0];
    xdisp(i,1) = disp[i*NODDOF_SOH8+1];
    xdisp(i,2) = disp[i*NODDOF_SOH8+2];
  }

  LINALG::Matrix<3,3> invJhist;
  LINALG::Matrix<3,3> invJ;
  LINALG::Matrix<3,3> defgrd;
  LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_xyz;
  LINALG::Matrix<3,3> invJnew;
  for (int gp=0; gp<NUMGPT_SOH8; ++gp)
  {
    // get the invJ old state
    prestress.StoragetoMatrix(gp,invJhist,prestress.JHistory());
    // get derivatives wrt to invJhist
    N_xyz.Multiply(invJhist,derivs[gp]);
    // build defgrd \partial x_new / \parial x_old , where x_old != X
    defgrd.MultiplyTT(xdisp,N_xyz);
    defgrd(0,0) += 1.0;
    defgrd(1,1) += 1.0;
    defgrd(2,2) += 1.0;
    // make inverse of this defgrd
    defgrd.Invert();
    // push-forward of Jinv
    invJnew.MultiplyTN(defgrd,invJhist);
    // store new reference configuration
    prestress.MatrixtoStorage(gp,invJnew,prestress.JHistory());
  } // for (int gp=0; gp<NUMGPT_SOH8; ++gp)

  return;
}

/*----------------------------------------------------------------------*
 |  remodeling of fiber directions (protected)               tinkl 01/10|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_hex8::soh8_remodel(
      vector<int>&              lm,             // location matrix
      vector<double>&           disp,           // current displacements
      ParameterList&            params,         // algorithmic parameters e.g. time
      RCP<MAT::Material>        mat)            // material
{
// in a first step ommit everything with prestress and EAS!!

  const static vector<LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> > derivs = soh8_derivs();

  // update element geometry
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xcurr;  // current  coord. of element
  LINALG::Matrix<NUMNOD_SOH8,NUMDIM_SOH8> xdisp;
  DRT::Node** nodes = Nodes();
  for (int i=0; i<NUMNOD_SOH8; ++i)
  {
    const double* x = nodes[i]->X();
    xcurr(i,0) = x[0] + disp[i*NODDOF_SOH8+0];
    xcurr(i,1) = x[1] + disp[i*NODDOF_SOH8+1];
    xcurr(i,2) = x[2] + disp[i*NODDOF_SOH8+2];

    if (pstype_==INPAR::STR::prestress_mulf)
    {
      xdisp(i,0) = disp[i*NODDOF_SOH8+0];
      xdisp(i,1) = disp[i*NODDOF_SOH8+1];
      xdisp(i,2) = disp[i*NODDOF_SOH8+2];
    }
  }
  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  LINALG::Matrix<NUMDIM_SOH8,NUMNOD_SOH8> N_XYZ;
  // build deformation gradient wrt to material configuration
  LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> defgrd(false);
  for (int gp=0; gp<NUMGPT_SOH8; ++gp)
  {
    /* get the inverse of the Jacobian matrix which looks like:
    **            [ x_,r  y_,r  z_,r ]^-1
    **     J^-1 = [ x_,s  y_,s  z_,s ]
    **            [ x_,t  y_,t  z_,t ]
    */
    // compute derivatives N_XYZ at gp w.r.t. material coordinates
    // by N_XYZ = J^-1 * N_rst
    N_XYZ.Multiply(invJ_[gp],derivs[gp]);

    if (pstype_==INPAR::STR::prestress_mulf)
    {
      // get Jacobian mapping wrt to the stored configuration
      LINALG::Matrix<3,3> invJdef;
      prestress_->StoragetoMatrix(gp,invJdef,prestress_->JHistory());
      // get derivatives wrt to last spatial configuration
      LINALG::Matrix<3,8> N_xyz;
      N_xyz.Multiply(invJdef,derivs[gp]);

      // build multiplicative incremental defgrd
      defgrd.MultiplyTT(xdisp,N_xyz);
      defgrd(0,0) += 1.0;
      defgrd(1,1) += 1.0;
      defgrd(2,2) += 1.0;

      // get stored old incremental F
      LINALG::Matrix<3,3> Fhist;
      prestress_->StoragetoMatrix(gp,Fhist,prestress_->FHistory());

      // build total defgrd = delta F * F_old
      LINALG::Matrix<3,3> Fnew;
      Fnew.Multiply(defgrd,Fhist);
      defgrd = Fnew;
    }
    else
      // (material) deformation gradient F = d xcurr / d xrefe = xcurr^T * N_XYZ^T
      defgrd.MultiplyTT(xcurr,N_XYZ);

    // Right Cauchy-Green tensor = F^T * F
    LINALG::Matrix<NUMDIM_SOH8,NUMDIM_SOH8> cauchygreen;
    cauchygreen.MultiplyTN(defgrd,defgrd);

    // Green-Lagrange strains matrix E = 0.5 * (Cauchygreen - Identity)
    // GL strain vector glstrain={E11,E22,E33,2*E12,2*E23,2*E31}
    Epetra_SerialDenseVector glstrain_epetra(NUMSTR_SOH8);
    LINALG::Matrix<NUMSTR_SOH8,1> glstrain(glstrain_epetra.A(),true);
    glstrain(0) = 0.5 * (cauchygreen(0,0) - 1.0);
    glstrain(1) = 0.5 * (cauchygreen(1,1) - 1.0);
    glstrain(2) = 0.5 * (cauchygreen(2,2) - 1.0);
    glstrain(3) = cauchygreen(0,1);
    glstrain(4) = cauchygreen(1,2);
    glstrain(5) = cauchygreen(2,0);

    /* non-linear B-operator (may so be called, meaning
    ** of B-operator is not so sharp in the non-linear realm) *
    ** B = F . Bl *
    **
    **      [ ... | F_11*N_{,1}^k  F_21*N_{,1}^k  F_31*N_{,1}^k | ... ]
    **      [ ... | F_12*N_{,2}^k  F_22*N_{,2}^k  F_32*N_{,2}^k | ... ]
    **      [ ... | F_13*N_{,3}^k  F_23*N_{,3}^k  F_33*N_{,3}^k | ... ]
    ** B =  [ ~~~   ~~~~~~~~~~~~~  ~~~~~~~~~~~~~  ~~~~~~~~~~~~~   ~~~ ]
    **      [       F_11*N_{,2}^k+F_12*N_{,1}^k                       ]
    **      [ ... |          F_21*N_{,2}^k+F_22*N_{,1}^k        | ... ]
    **      [                       F_31*N_{,2}^k+F_32*N_{,1}^k       ]
    **      [                                                         ]
    **      [       F_12*N_{,3}^k+F_13*N_{,2}^k                       ]
    **      [ ... |          F_22*N_{,3}^k+F_23*N_{,2}^k        | ... ]
    **      [                       F_32*N_{,3}^k+F_33*N_{,2}^k       ]
    **      [                                                         ]
    **      [       F_13*N_{,1}^k+F_11*N_{,3}^k                       ]
    **      [ ... |          F_23*N_{,1}^k+F_21*N_{,3}^k        | ... ]
    **      [                       F_33*N_{,1}^k+F_31*N_{,3}^k       ]
    */
    LINALG::Matrix<NUMSTR_SOH8,NUMDOF_SOH8> bop;
    for (int i=0; i<NUMNOD_SOH8; ++i)
    {
      bop(0,NODDOF_SOH8*i+0) = defgrd(0,0)*N_XYZ(0,i);
      bop(0,NODDOF_SOH8*i+1) = defgrd(1,0)*N_XYZ(0,i);
      bop(0,NODDOF_SOH8*i+2) = defgrd(2,0)*N_XYZ(0,i);
      bop(1,NODDOF_SOH8*i+0) = defgrd(0,1)*N_XYZ(1,i);
      bop(1,NODDOF_SOH8*i+1) = defgrd(1,1)*N_XYZ(1,i);
      bop(1,NODDOF_SOH8*i+2) = defgrd(2,1)*N_XYZ(1,i);
      bop(2,NODDOF_SOH8*i+0) = defgrd(0,2)*N_XYZ(2,i);
      bop(2,NODDOF_SOH8*i+1) = defgrd(1,2)*N_XYZ(2,i);
      bop(2,NODDOF_SOH8*i+2) = defgrd(2,2)*N_XYZ(2,i);
      /* ~~~ */
      bop(3,NODDOF_SOH8*i+0) = defgrd(0,0)*N_XYZ(1,i) + defgrd(0,1)*N_XYZ(0,i);
      bop(3,NODDOF_SOH8*i+1) = defgrd(1,0)*N_XYZ(1,i) + defgrd(1,1)*N_XYZ(0,i);
      bop(3,NODDOF_SOH8*i+2) = defgrd(2,0)*N_XYZ(1,i) + defgrd(2,1)*N_XYZ(0,i);
      bop(4,NODDOF_SOH8*i+0) = defgrd(0,1)*N_XYZ(2,i) + defgrd(0,2)*N_XYZ(1,i);
      bop(4,NODDOF_SOH8*i+1) = defgrd(1,1)*N_XYZ(2,i) + defgrd(1,2)*N_XYZ(1,i);
      bop(4,NODDOF_SOH8*i+2) = defgrd(2,1)*N_XYZ(2,i) + defgrd(2,2)*N_XYZ(1,i);
      bop(5,NODDOF_SOH8*i+0) = defgrd(0,2)*N_XYZ(0,i) + defgrd(0,0)*N_XYZ(2,i);
      bop(5,NODDOF_SOH8*i+1) = defgrd(1,2)*N_XYZ(0,i) + defgrd(1,0)*N_XYZ(2,i);
      bop(5,NODDOF_SOH8*i+2) = defgrd(2,2)*N_XYZ(0,i) + defgrd(2,0)*N_XYZ(2,i);
    }

    /* call material law cccccccccccccccccccccccccccccccccccccccccccccccccccccc
    ** Here all possible material laws need to be incorporated,
    ** the stress vector, a C-matrix, and a density must be retrieved,
    ** every necessary data must be passed.
    */
    double density = 0.0;
    LINALG::Matrix<NUMSTR_SOH8,NUMSTR_SOH8> cmat(true);
    LINALG::Matrix<NUMSTR_SOH8,1> stress(true);
    LINALG::Matrix<NUMSTR_SOH8,1> plglstrain(true);
    soh8_mat_sel(&stress,&cmat,&density,&glstrain,&plglstrain,&defgrd,gp,params);
    // end of call material law ccccccccccccccccccccccccccccccccccccccccccccccc

    // Cauchy stress
    const double detF = defgrd.Determinant();

    LINALG::Matrix<3,3> pkstress;
    pkstress(0,0) = stress(0);
    pkstress(0,1) = stress(3);
    pkstress(0,2) = stress(5);
    pkstress(1,0) = pkstress(0,1);
    pkstress(1,1) = stress(1);
    pkstress(1,2) = stress(4);
    pkstress(2,0) = pkstress(0,2);
    pkstress(2,1) = pkstress(1,2);
    pkstress(2,2) = stress(2);

    LINALG::Matrix<3,3> temp(true);
    LINALG::Matrix<3,3> cauchystress(true);
    temp.Multiply(1.0/detF,defgrd,pkstress);
    cauchystress.MultiplyNT(temp,defgrd);

    // evaluate eigenproblem based on stress of previous step
    LINALG::Matrix<3,3> lambda(true);
    LINALG::Matrix<3,3> locsys(true);
    LINALG::SYEV(cauchystress,lambda,locsys);

    // modulation function acc. Hariton: tan g = 2nd max lambda / max lambda
    double newgamma = atan(lambda(1,1)/lambda(2,2));
    //compression in 2nd max direction, thus fibers are alligned to max principal direction
    if (lambda(1,1) < 0) newgamma = 0.0;

    if (mat->MaterialType() == INPAR::MAT::m_holzapfelcardiovascular) {
      MAT::HolzapfelCardio* holz = static_cast <MAT::HolzapfelCardio*>(mat.get());
      holz->EvaluateFiberVecs(gp,newgamma,locsys,defgrd);
    } else if (mat->MaterialType() == INPAR::MAT::m_humphreycardiovascular) {
      MAT::HumphreyCardio* hum = static_cast <MAT::HumphreyCardio*>(mat.get());
      hum->EvaluateFiberVecs(gp,locsys,defgrd);
    } else if (mat->MaterialType() == INPAR::MAT::m_constraintmixture) {
      MAT::ConstraintMixture* comi = static_cast <MAT::ConstraintMixture*>(mat.get());
      comi->EvaluateFiberVecs(gp,locsys,defgrd);
    } else dserror("material not implemented for remodeling");

  } // end loop over gauss points
}


#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_SOLID3

