/*----------------------------------------------------------------------*/
/*!
 * \file invana_auglagr.cpp

<pre>
Maintainer: Sebastian Kehl
            kehl@mhpc.mw.tum.de
            089 - 289-10361
</pre>
*/
/*----------------------------------------------------------------------*/
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "invana_auglagr.H"
#include "invana_utils.H"

#include "matpar_manager.H"
#include "objective_funct.H"
#include "regularization_base.H"
#include "optimizer_base.H"

#include "../drt_adapter/ad_str_invana.H"
#include "timint_adjoint.H"
#include "timint_adjoint_prestress.H"

#include "../drt_io/io_control.H"
#include "../drt_io/io_pstream.H"
#include "../drt_io/io_hdf.H"
#include "../drt_io/io.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_utils_timintmstep.H"
#include "../linalg/linalg_utils.H"
#include "../drt_inpar/inpar_statinvanalysis.H"
#include "../drt_inpar/drt_validparameters.H"

#include <Teuchos_ParameterList.hpp>


/*----------------------------------------------------------------------*/
/* standard constructor                                      keh 09/14  */
/*----------------------------------------------------------------------*/
INVANA::InvanaAugLagr::InvanaAugLagr():
  InvanaBase(),
dis_(Teuchos::null),
disdual_(Teuchos::null),
disdualp_(Teuchos::null),
timestep_(0.0),
msteps_(0),
pstype_(INPAR::STR::prestress_none),
pstime_(0.0),
fprestart_(0),
itertopc_(10)
{
  const Teuchos::ParameterList& sdyn  = DRT::Problem::Instance()->StructuralDynamicParams();
  const Teuchos::ParameterList& invp = DRT::Problem::Instance()->StatInverseAnalysisParams();

  // this is supposed to be the number of simulation steps in the primal AND the dual problem
  msteps_ = sdyn.get<int>("NUMSTEP");
  timestep_ = sdyn.get<double>("TIMESTEP");

  // prestress stuff
  pstype_ = DRT::INPUT::IntegralValue<INPAR::STR::PreStress>(sdyn,"PRESTRESS");
  pstime_ = sdyn.get<double>("PRESTRESSTIME");

  // initialize the vector of time steps according to the structural dynamic params
  time_.resize(msteps_,0.0);
  for (int i=0; i< msteps_; i++)
    time_[i] = (i+1)*timestep_;

  fprestart_ = invp.get<int>("FPRESTART");
  itertopc_ = invp.get<int>("ITERTOPC");

  fpcounter=0;

}

/*----------------------------------------------------------------------*/
/* Setup                                                     keh 09/14  */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::Setup()
{
  if (not Discret()->Filled() || not Discret()->HaveDofs())
    dserror("Discretisation is not complete or has no dofs!");

  // initialize "state" vectors
  dis_ = Teuchos::rcp(new Epetra_MultiVector(*(Discret()->DofRowMap()),msteps_,true));
  disdual_ = Teuchos::rcp(new Epetra_MultiVector(*(Discret()->DofRowMap()),msteps_,true));
  disdualp_ = Teuchos::rcp(new Epetra_MultiVector(*(Discret()->DofRowMap()),msteps_,true));

  //output for the forward problem
  std::string filename = DRT::Problem::Instance()->OutputControlFile()->FileName();
  std::string prefix = DRT::Problem::Instance()->OutputControlFile()->FileNameOnlyPrefix();
  size_t pos = filename.rfind('/');
  size_t pos2 = prefix.rfind('-');
  std::string filenameout = filename.substr(0,pos+1) + prefix.substr(0,pos2) + "_forward" + filename.substr(pos+1+prefix.length());

  Teuchos::RCP<IO::OutputControl> controlfile =
    Teuchos::rcp(new IO::OutputControl(
      Discret()->Comm(),
      DRT::Problem::Instance()->ProblemName(),
      DRT::Problem::Instance()->SpatialApproximation(),
      DRT::Problem::Instance()->OutputControlFile()->InputFileName(),
      filenameout,
      DRT::Problem::Instance()->NDim(),
      fprestart_,
      DRT::Problem::Instance()->OutputControlFile()->FileSteps(),
      DRT::INPUT::IntegralValue<int>(DRT::Problem::Instance()->IOParams(),"OUTPUT_BIN")
    )
  );
  // give the discretization another controlfile for output
  Discret()->Writer()->SetOutput(controlfile);

  return;
}

/*----------------------------------------------------------------------*/
/* MStep EpetraVector to EpetraMultiVector                   keh 10/13  */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::MStepEpetraToEpetraMulti(Teuchos::RCP<DRT::UTILS::TimIntMStep<Epetra_Vector> > mstepvec,
                                                            Teuchos::RCP<Epetra_MultiVector> multivec)
{
  for (int i=0; i<msteps_; i++)
    (*multivec)(i)->Update(1.0, *(*mstepvec)(-(msteps_-i-1)),0.0);
}

/*----------------------------------------------------------------------*/
/* Mstep double to std::vector<double>                       keh 10/13  */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::MStepDToStdVecD(Teuchos::RCP<DRT::UTILS::TimIntMStep<double> > mstepvec,
                                                 std::vector<double>* stdvec)
{
  for (int i=0; i<msteps_; i++)
    (*stdvec)[i] = *(*mstepvec)(-(msteps_-i-1));

}

/*----------------------------------------------------------------------*/
/* solve primal problem                                      keh 10/13  */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::SolveForwardProblem()
{
  // use the same control file for every run since usually the last one is of interest
  Discret()->Writer()->OverwriteResultFile();

  // get input lists
  const Teuchos::ParameterList& sdyn = DRT::Problem::Instance()->StructuralDynamicParams();

  // major switch to different time integrators
  switch (DRT::INPUT::IntegralValue<INPAR::STR::DynamicType>(sdyn,"DYNAMICTYP"))
  {
    case INPAR::STR::dyna_statics:
    {

      ADAPTER::StructureBaseAlgorithm adapterbase(sdyn,const_cast<Teuchos::ParameterList&>(sdyn), Discret());
      Teuchos::RCP<ADAPTER::StructureInvana> structadaptor =
          Teuchos::rcp_dynamic_cast<ADAPTER::StructureInvana>(adapterbase.StructureField());

      // do restart but the one which is explicitly given in the INVERSE ANALYSIS section
      if (fprestart_)
      {
        dserror("Restarting from within a timestep of the forward problem needs some tweaking first!");
        structadaptor->ReadRestart(fprestart_);
      }

      if (Optimizer()->Runc()>itertopc_)
      {
        std::vector<double> mtime = ObjectiveFunct()->MeasuredTime();
        for (int i=0; i<(int)time_.size(); i++)
        {
          //find whether measurements exist for this simulation timestep
          int mstep=ObjectiveFunct()->FindStep(time_[i]);

          // do this step only in case of measurements or when it is the last prestress step
          if (mstep!=-1 or (time_[i]>pstime_-structadaptor->Dt() and time_[i]<pstime_))
          {
            structadaptor->SetTimeStepStateOld(time_[i]-structadaptor->Dt(),i,
                Teuchos::rcp((*dis_)(i),false),
                Teuchos::rcp((*dis_)(i),false)); // veln is not used so far so just put disn

            structadaptor->Integrate();
          }
        }
      }
      else
      {
        structadaptor->Integrate();
      }

      // get displacement and time
      MStepEpetraToEpetraMulti(structadaptor->DispSteps(), dis_);
      MStepDToStdVecD(structadaptor->TimeSteps(), &time_);

      break;
    }
    case INPAR::STR::dyna_genalpha:
    case INPAR::STR::dyna_onesteptheta:
    case INPAR::STR::dyna_gemm:
    case INPAR::STR::dyna_expleuler:
    case INPAR::STR::dyna_centrdiff:
    case INPAR::STR::dyna_ab2:
    case INPAR::STR::dyna_euma:
    case INPAR::STR::dyna_euimsto:
      dserror("inverse analysis for statics only so far");
      break;
    default:
      dserror("unknown time integration scheme '%s'", sdyn.get<std::string>("DYNAMICTYP").c_str());
      break;
  }

  fpcounter+=1;
  return;
}


/*----------------------------------------------------------------------*/
/* solve dual problem                                       keh 10/13   */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::SolveAdjointProblem()
{
  //Setup RHS for the adjoints
  Teuchos::RCP<Epetra_Vector> objgrad = Teuchos::rcp(new Epetra_Vector(*(Discret()->DofRowMap()),true));

  std::vector<double> mtime = ObjectiveFunct()->MeasuredTime();
  Teuchos::RCP<Epetra_MultiVector> adjrhs =
      Teuchos::rcp(new Epetra_MultiVector(*(Discret()->DofRowMap()),(int)mtime.size(),true));

  for (int i=0; i<(int)time_.size(); i++)
  {
    //find whether measurements exist for this simulation timestep
    int mstep=ObjectiveFunct()->FindStep(time_[i]);
    if (mstep!=-1)
    {
      ObjectiveFunct()->EvaluateGradient(Teuchos::rcp((*dis_)(i),false),time_[i],objgrad);
      (*adjrhs)(mstep)->Scale(1.0,*objgrad);
    }
  }

  //initialize adjoint time integration with RHS as input
  Teuchos::RCP<STR::TimIntAdjoint> timintadj;
  if (pstype_==INPAR::STR::prestress_none)
    timintadj = Teuchos::rcp(new STR::TimIntAdjoint(Discret()));
  else if(pstype_==INPAR::STR::prestress_mulf)
    timintadj = Teuchos::rcp(new STR::TimIntAdjointPrestress(Discret()));

  timintadj->SetupAdjoint(adjrhs, mtime, dis_, time_);

  // adjoint time integration
  timintadj->Integrate();

  // get the solution
  disdual_->Update(1.0,*timintadj->ExtractSolution(),0.0);

  if (pstype_==INPAR::STR::prestress_mulf)
  {
    disdualp_->Update(1.0,*timintadj->ExtractPrestressSolution(),0.0);
  }

}

/*----------------------------------------------------------------------*/
/* evaluate the value and/or gradient of the problem        keh 10/13   */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::Evaluate(const Epetra_MultiVector& sol, double* val, Teuchos::RCP<Epetra_MultiVector> gradient)
{
  Matman()->ReplaceParams(sol);
  if (Optimizer()->Runc()<=itertopc_)
    ResetDiscretization();

  if ( gradient != Teuchos::null or val!=NULL )
  {
    SolveForwardProblem();
    EvaluateError(sol,val);

    if (gradient != Teuchos::null)
    {
      SolveAdjointProblem();
      EvaluateGradient(sol,gradient);
      //gradient->Print(std::cout);
      //EvaluateGradientFD(sol,gradient);
      //gradient->Print(std::cout);
      //exit(0);
    }
  }
}

/*----------------------------------------------------------------------*/
/* evaluate gradient of the objective function              keh 10/13   */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::EvaluateGradient(const Epetra_MultiVector& sol, Teuchos::RCP<Epetra_MultiVector> gradient)
{
  //zero out gradient vector initially
  gradient->Scale(0.0);

  Teuchos::RCP<Epetra_Vector> zeros;
  zeros = LINALG::CreateVector(*(Discret()->DofRowMap()), true);

  //loop the time steps
  for (int j=0; j<msteps_; j++)
  {
    Discret()->SetState(0, "displacement", Teuchos::rcp((*dis_)(j),false));
    Discret()->SetState(0, "residual displacement", zeros);
    Discret()->SetState(0, "dual displacement", Teuchos::rcp((*disdual_)(j),false));
    Matman()->AddEvaluate(time_[j], gradient);

    Discret()->ClearState();

    if (pstype_==INPAR::STR::prestress_mulf)
    {
      int stepps=(int)(pstime_/(timestep_));
      Discret()->SetState(0, "displacement", Teuchos::rcp((*dis_)(stepps-1),false));
      Discret()->SetState(0, "residual displacement", zeros);
      Discret()->SetState(0, "dual displacement", Teuchos::rcp((*disdualp_)(j),false));
      Matman()->AddEvaluate(time_[stepps-1], gradient);
    }
  }
  Matman()->Finalize(gradient);


  if (Regman() != Teuchos::null)
    Regman()->EvaluateGradient(sol,gradient);

}

/*----------------------------------------------------------------------*/
/* evaluate gradient of the objective function using FD     keh 12/14   */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::EvaluateGradientFD(const Epetra_MultiVector& sol, Teuchos::RCP<Epetra_MultiVector> gradient)
{
  if (Discret()->Comm().NumProc()!=1)
    dserror("FD gradient evaluation is only implemented for single processor simulations");

  //zero out gradient vector initially
  gradient->Scale(0.0);

  // get a non const copy of the parameters
  Teuchos::RCP<Epetra_MultiVector> params_0=Teuchos::rcp(new Epetra_MultiVector(sol));

  //Evaluate the reference solution:
  double val_0;
  Matman()->ReplaceParams(sol);
  ResetDiscretization();
  SolveForwardProblem();
  EvaluateError(*params_0,&val_0);

  //do the perturbation loop
  double alpha=1.0e-7;
  double beta=1.0e-12;
  double dp=0.0; // perturbation parameter
  double val_p;
  for (int j=0; j<params_0->MyLength(); j++)
  {
    dp=(*(*params_0)(0))[j]*alpha+beta;
    params_0->SumIntoGlobalValue(j,0,-dp);
    Matman()->ReplaceParams(*params_0);
    ResetDiscretization();
    SolveForwardProblem();
    EvaluateError(*params_0,&val_p);
    gradient->ReplaceGlobalValue(j,0,(val_0-val_p)/dp);
    params_0->Update(1.0,sol,0.0);
  }

}

/*----------------------------------------------------------------------*/
/* Reset the discretization                                 keh 10/13   */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::ResetDiscretization()
{
  Teuchos::ParameterList p;
  p.set("action","calc_struct_reset_all");
  Discret()->Evaluate(p,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);
}

/*----------------------------------------------------------------------*/
/* Evaluate the objective function                          keh 10/13   */
/*----------------------------------------------------------------------*/
void INVANA::InvanaAugLagr::EvaluateError(const Epetra_MultiVector& sol, double* val)
{
  *val=0.0;

  double toadd;
  for (int i=0; i< (int)time_.size(); i++)
  {
    //find whether measurements exist for this simulation timestep
    int mstep=ObjectiveFunct()->FindStep(time_[i]);
    if (mstep!=-1)
    {
      ObjectiveFunct()->Evaluate(Teuchos::rcp((*dis_)(i),false),time_[i], toadd);
      *val+=toadd;
    }
  }

  if (Regman() != Teuchos::null)
    Regman()->Evaluate(sol,val);
}
