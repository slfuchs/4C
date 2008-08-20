/*!----------------------------------------------------------------------
\file contactstrustatic.cpp
\brief Solution routine for nonlinear structural statics with contact

<pre>
Maintainer: Alexander Popp
            popp@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15264
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <ctime>
#include <cstdlib>
#include <iostream>

#include <Teuchos_StandardParameterEntryValidators.hpp>

#ifdef PARALLEL
#include <mpi.h>
#endif

#include "contactdefines.H"
#include "drt_contact_manager.H"
#include "../drt_structure/stru_static_drt.H"
#include "../drt_io/io.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/linalg_sparsematrix.H"
#include "../drt_structure/stru_resulttest.H"



/*----------------------------------------------------------------------*
 |                                                         maf 05/07    |
 | general problem data                                                 |
 | global variable GENPROB genprob is defined in global_control.c       |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;

/*!----------------------------------------------------------------------
\brief file pointers

<pre>                                                           maf 05/07
This structure struct _FILES allfiles is defined in input_control_global.c
and the type is in standardtypes.h
It holds all file pointers and some variables needed for the FRSYSTEM
</pre>
*----------------------------------------------------------------------*/
extern struct _FILES  allfiles;

/*----------------------------------------------------------------------*
 | global variable *solv, vector of lenght numfld of structures SOLVAR  |
 | defined in solver_control.c                                          |
 |                                                                      |
 |                                                         maf 05/07    |
 *----------------------------------------------------------------------*/
extern struct _SOLVAR  *solv;

/*----------------------------------------------------------------------*
 |                                                         maf 05/07    |
 | pointer to allocate static variables if needed                       |
 | defined in global_control.c                                          |
 *----------------------------------------------------------------------*/
extern struct _STATIC_VAR  *statvar;


namespace CONTACT
{
/*----------------------------------------------------------------------*
 | structural nonlinear statics with contact                 popp 03/08 |
 *----------------------------------------------------------------------*/
void contact_stru_static_drt()
{
  // -------------------------------------------------------------------
  // access the discretization
  // -------------------------------------------------------------------
  RefCountPtr<DRT::Discretization> actdis = null;
  actdis = DRT::Problem::Instance()->Dis(genprob.numsf,0);

  // set degrees of freedom in the discretization
  if (!actdis->Filled()) actdis->FillComplete();

  // -------------------------------------------------------------------
  // get contact conditions
  // -------------------------------------------------------------------
  RCP<CONTACT::Manager> contactmanager;
  vector<DRT::Condition*> contactconditions(0);
  actdis->GetCondition("Contact",contactconditions);
  if (!contactconditions.size()) dserror("No contact boundary conditions present");

  // create contact manager to organize all contact-related things
  contactmanager = rcp(new CONTACT::Manager(*actdis));

  // get information on primal-dual active set strategy
  bool semismooth = (contactmanager->Params()).get<bool>("semismooth newton",false);
  
  // -------------------------------------------------------------------
  // get a communicator and myrank
  // -------------------------------------------------------------------
  const Epetra_Comm& Comm = actdis->Comm();
  const int myrank = Comm.MyPID();

  //----------------------------------------------------- get error file
  FILE* errfile = allfiles.out_err;

  // -------------------------------------------------------------------
  // set some pointers and variables
  // -------------------------------------------------------------------
  SOLVAR*         actsolv  = &solv[0];
  const Teuchos::ParameterList& ioflags  = DRT::Problem::Instance()->IOParams();

  //-----------------------------------------------------create a solver
  RefCountPtr<ParameterList> solveparams = rcp(new ParameterList());
  LINALG::Solver solver(solveparams,actdis->Comm(),allfiles.out_err);
  solver.TranslateSolverParameters(*solveparams,actsolv);
  actdis->ComputeNullSpaceIfNecessary(*solveparams);

  // -------------------------------------------------------------------
  // get a vector layout from the discretization to construct matching
  // vectors and matrices
  // -------------------------------------------------------------------
  const Epetra_Map* dofrowmap = actdis->DofRowMap();

  // -------------------------------------------------------------------
  // create empty stiffness matrix
  // -------------------------------------------------------------------
  // `81' is an initial guess for the bandwidth of the matrices
  // A better guess will be determined later.
  RefCountPtr<LINALG::SparseMatrix> stiff_mat = Teuchos::rcp(new LINALG::SparseMatrix(*dofrowmap,81));

  // -------------------------------------------------------------------
  // create empty vectors
  // -------------------------------------------------------------------
  // a zero vector of full length
  RefCountPtr<Epetra_Vector> zeros = LINALG::CreateVector(*dofrowmap,true);
  // vector of full length; for each component
  //                /  1   i-th DOF is supported, ie Dirichlet BC
  //    vector_i =  <
  //                \  0   i-th DOF is free
  RefCountPtr<Epetra_Vector> dirichtoggle = LINALG::CreateVector(*dofrowmap,true);
  // opposite of dirichtoggle vector, ie for each component
  //                /  0   i-th DOF is supported, ie Dirichlet BC
  //    vector_i =  <
  //                \  1   i-th DOF is free
  RefCountPtr<Epetra_Vector> invtoggle = LINALG::CreateVector(*dofrowmap,false);
  // displacements D_{n} at last time
  RefCountPtr<Epetra_Vector> dis = LINALG::CreateVector(*dofrowmap,true);

  // displacements D_{n+1} at new time
  RefCountPtr<Epetra_Vector> disn = LINALG::CreateVector(*dofrowmap,true);

  // iterative displacement increments IncD_{n+1}
  // also known as residual displacements
  RefCountPtr<Epetra_Vector> disi = LINALG::CreateVector(*dofrowmap,true);

  // internal force vector F_int at different times
  RefCountPtr<Epetra_Vector> fint = LINALG::CreateVector(*dofrowmap,true);
  // external force vector F_ext at last times
  RefCountPtr<Epetra_Vector> fext = LINALG::CreateVector(*dofrowmap,true);
  // external force vector F_{n+1} at new time
  RefCountPtr<Epetra_Vector> fextn = LINALG::CreateVector(*dofrowmap,true);

  // dynamic force residual at mid-time R_{n+1-alpha}
  // also known at out-of-balance-force
  RefCountPtr<Epetra_Vector> fresm = LINALG::CreateVector(*dofrowmap,false);

  if (statvar->nr_controltyp != control_load) dserror("Only load control implemented");

  /*
  ** solution control parameters are inherited from dynamic routine:
  ** dt     = stepsize
  ** istep  = load step index
  ** time   = redundant, equals istep*dt
  */
  //------------------------------------------ time integration parameters
  const double dt = statvar->stepsize;
  int istep = 0;
  double time = 0.0;  // we should add an input parameter
  double timen;

  // -------------------------------------------------------------------
  // context for output and restart
  // -------------------------------------------------------------------
  IO::DiscretizationWriter output(actdis);
  if (genprob.restart){
    int restartstep = genprob.restart;
    RefCountPtr<DRT::Discretization> rcpdiscret(actdis);
    rcpdiscret.release();
    IO::DiscretizationReader reader(rcpdiscret,restartstep);
    double rtime  = reader.ReadDouble("time");
    int    rstep = reader.ReadInt("step");
    if (rstep != restartstep) dserror("Time step on file not equal to given step");

    reader.ReadVector(dis, "displacement");
    //reader.ReadVector(fext, "fexternal");
    //reader.ReadMesh(restartstep);

    // read restart information for contact
    RCP<Epetra_Vector> zold = rcp(new Epetra_Vector(*(contactmanager->SlaveRowDofs())));
    RCP<Epetra_Vector> activetoggle =rcp(new Epetra_Vector(*(contactmanager->SlaveRowNodes())));
    reader.ReadVector(zold,"lagrmultold");
    reader.ReadVector(activetoggle,"activetoggle");
    
    // set old Lagrange multipliers for contact restart
    *(contactmanager->LagrMultOld())=*zold;
    contactmanager->StoreNodalQuantities(Manager::lmold);
    contactmanager->ReadRestart(activetoggle);
      
    // override current time and step with values from file
    time = rtime;
    istep = rstep;
  }

  // write mesh always at beginning of calc or restart
  output.WriteMesh(istep,time);


  //-------------------------------- calculate external force distribution
  //---- which is scaled by the load factor lambda (itself stays constant)
  {
    ParameterList params;
    // action for elements
    params.set("action","calc_struct_eleload");

    //other parameters needed by the elements
    params.set("total time",time);
    params.set("delta time",dt);

    // set vector values needed by elements
    actdis->ClearState();
    actdis->SetState("displacement",dis);
    // predicted dirichlet values
    // dis then also holds prescribed new dirichlet displacements
    actdis->EvaluateDirichlet(params,dis,null,null,dirichtoggle);
    actdis->ClearState();
    actdis->SetState("displacement",dis);
    // predicted rhs
    actdis->EvaluateNeumann(params,*fext); // *fext holds external force vector
    actdis->ClearState();
  }

  //----------------------- compute an inverse of the dirichtoggle vector
  invtoggle->PutScalar(1.0);
  invtoggle->Update(-1.0,*dirichtoggle,1.0);

  //----------------------- save Dirichlet B.C. status in Contact Manager
  // all CNodes on all interfaces then know if D.B.C.s are applied on their dofs
  contactmanager->StoreNodalQuantities(Manager::dirichlet,dirichtoggle);
    
  //------------------------------------------------- output initial state
  output.NewStep(istep, time);
  output.WriteVector("displacement", dis);
  output.WriteElementData();

  //---------------------------------------------- do "stress" calculation
  int mod_stress = istep % statvar->resevry_stress;
  string iostress;
  switch (Teuchos::getIntegralValue<STRUCT_STRESS_TYP>(ioflags,"STRUCT_STRESS"))
  {
  case struct_stress_none:
    iostress = "none";
    break;
  case struct_stress_cauchy:
    iostress = "cauchy";
    break;
  case struct_stress_pk:
    iostress = "2PK";
    break;
  default:
    iostress = "none";
    break;
  }
  string iostrain;
  switch (Teuchos::getIntegralValue<STRUCT_STRAIN_TYP>(ioflags,"STRUCT_STRAIN"))
  {
  case struct_strain_none:
    iostrain = "none";
    break;
  case struct_strain_gl:
    iostrain = "green_lagrange";
    break;
  case struct_strain_ea:
    iostrain = "euler_almansi";
    break;
  default:
    iostrain = "none";
    break;
  }
  if (!mod_stress && iostress!="none")
  {
    // create the parameters for the discretization
    ParameterList p;
    // action for elements
    p.set("action","calc_struct_stress");
    // other parameters that might be needed by the elements
    p.set("total time",timen);
    p.set("delta time",dt);
    Teuchos::RCP<std::vector<char> > stress = Teuchos::rcp(new std::vector<char>());
    Teuchos::RCP<std::vector<char> > strain = Teuchos::rcp(new std::vector<char>());
    p.set("stress", stress);
    p.set("strain", strain);
    if (iostress == "cauchy")   // output of Cauchy stresses instead of 2PK stresses
    {
      p.set("cauchy", true);
    }
    else
    {
      p.set("cauchy", false);
    }
    p.set("iostrain", iostrain);
    // set vector values needed by elements
    actdis->ClearState();
    actdis->SetState("residual displacement",zeros);
    actdis->SetState("displacement",dis);
    actdis->Evaluate(p,null,null,null,null,null);
    actdis->ClearState();
    if (iostress == "cauchy")
      output.WriteVector("gauss_cauchy_stresses_xyz",*stress,*(actdis->ElementColMap()));
    else
      output.WriteVector("gauss_2PK_stresses_xyz",*stress,*(actdis->ElementColMap()));
    if (iostrain != "none")
    {
      if (iostrain == "euler_almansi")
      {
        output.WriteVector("gauss_EA_strains_xyz",*strain,*(actdis->ElementColMap()));
      }
      else
      {
        output.WriteVector("gauss_GL_strains_xyz",*strain,*(actdis->ElementColMap()));
      }
    }
  }

  //---------------------------------------------end of output initial state

  //========================================== start of time/loadstep loop
  while ( istep < statvar->nstep)
  {
    //------------------------------------------------------- current time
    // we are at t_{n} == time; the new time is t_{n+1} == time+dt
    timen = time + dt;

    // iteration counter for Newton scheme
    int numiter=0;

    // initialize active set convergence status and step number
    contactmanager->ActiveSetConverged() = false;
    contactmanager->ActiveSetSteps() = 1;

    //********************************************************************
    // OPTIONS FOR PRIMAL-DUAL ACTIVE SET STRATEGY (PDASS)
    //********************************************************************
    // 1) SEMI-SMOOTH NEWTON
    // 2) FIXED-POINT APPROACH
    //********************************************************************
    
    //********************************************************************
    // 1) SEMI-SMOOTH NEWTON
    // The search for the correct active set (=contact nonlinearity) and
    // the large deformation linearization (=geometrical nonlinearity) are
    // merged into one semi-smooth Newton method and solved within ONE
    // iteration loop
    //********************************************************************
  if (semismooth)
  {
    //--------------------------------------------------- predicting state
    // constant predictor : displacement in domain
    disn->Update(1.0, *dis, 0.0);

    // eval fint and stiffness matrix at current istep
    // and apply new displacements at DBCs
    {
      // destroy and create new matrix
      stiff_mat->Zero();
      // create the parameters for the discretization
      ParameterList params;
      // action for elements
      params.set("action","calc_struct_nlnstiff");
      // other parameters needed by the elements
      params.set("total time",timen);  // load factor (pseudo time)
      params.set("delta time",dt);  // load factor increment (pseudo time increment)
      // set vector values needed by elements
      actdis->ClearState();
      actdis->SetState("residual displacement",disi);
      // predicted dirichlet values
      // disn then also holds prescribed new dirichlet displacements
      actdis->EvaluateDirichlet(params,disn,null,null,dirichtoggle);
      actdis->SetState("displacement",disn);
      fint->PutScalar(0.0);  // initialise internal force vector
      actdis->Evaluate(params,stiff_mat,null,fint,null,null);
      // predicted rhs
      fextn->PutScalar(0.0);  // initialize external force vector (load vect)
      actdis->EvaluateNeumann(params,*fextn); // *fext holds external force vector at current step

      actdis->ClearState();
    }

    // complete stiffness matrix
    stiff_mat->Complete();

    double stiffnorm;
    stiffnorm = stiff_mat->NormFrobenius();

    // evaluate residual at current istep
    // R{istep,numiter=0} = F_int{istep,numiter=0} - F_ext{istep}
    fresm->Update(1.0,*fint,-1.0,*fextn,0.0);

    // keep a copy of fresm for contact forces / equilibrium check
    RCP<Epetra_Vector> fresmcopy= rcp(new Epetra_Vector(*fresm));
    
    // friction  
    // reset displacement jumps (slave dofs)
    RCP<Epetra_Vector> jump = contactmanager->Jump();
    jump->Scale(0.0); 
    contactmanager->StoreNodalQuantities(Manager::jump);
    
    //-------------------------- make contact modifications to lhs and rhs
    fresm->Scale(-1.0);     // rhs = -R = -fresm
    contactmanager->SetState("displacement",disn);
    
    contactmanager->InitializeMortar(0);
    contactmanager->EvaluateMortar(0);
    
    contactmanager->Initialize(0);
    contactmanager->Evaluate(stiff_mat,fresm,0);
    
    // blank residual at DOFs on Dirichlet BC
    {
      Epetra_Vector fresmdbc(*fresm);
      fresm->Multiply(1.0,*invtoggle,fresmdbc,0.0);
    }
        
    //---------------------------------------------------- contact forces
    // (no resetting of LM necessary for semi-smooth Newton, as there
    // will never be a repetition of a time / load step!)
    contactmanager->ContactForces(fresmcopy);
    
#ifdef CONTACTGMSH2
    contactmanager->VisualizeGmsh(istep+1,0);
#endif // #ifdef CONTACTGMSH2
    
    //----------------------------------------------- build res/disi norm
    double norm;
    fresm->Norm2(&norm);
    double disinorm = 1.0;

    if (!myrank) cout << " Predictor residual forces " << norm << endl; fflush(stdout);

    // reset Newton iteration counter
    numiter=0;
    
    //===========================================start of equilibrium loop
    // this is a semi-smooth Newton method, as it not only includes the
    // geometrical nonlinearity but also the active set search
    //=====================================================================
    while (((norm > statvar->tolresid) || (disinorm > statvar->toldisp)
            || contactmanager->ActiveSetConverged()==false) && numiter < statvar->maxiter)
    {
      //----------------------- apply dirichlet BCs to system of equations
      disi->PutScalar(0.0);   // Useful? depends on solver and more
      LINALG::ApplyDirichlettoSystem(stiff_mat,disi,fresm,zeros,dirichtoggle);

      //Do usual newton step
      // solve for disi
      // Solve K . IncD = -R  ===>  IncD_{n+1}
      if (numiter==0)
      {
        solver.Solve(stiff_mat->EpetraMatrix(),disi,fresm,true,true);
      }
      else
      {
        solver.Solve(stiff_mat->EpetraMatrix(),disi,fresm,true,false);
      }

      //------------------------------------- recover disi and Lagr. Mult.
      {
        contactmanager->Recover(disi);
      }

      // update displacements
      // D_{istep,numiter+1} := D_{istep,numiter} + IncD_{numiter}
      disn->Update(1.0, *disi, 1.0);

      // compute internal forces and stiffness at current iterate numiter
      {
        // zero out stiffness
        stiff_mat->Zero();
        // create the parameters for the discretization
        ParameterList params;
        // action for elements
        params.set("action","calc_struct_nlnstiff");
        // other parameters needed by the elements
        params.set("total time",timen);  // load factor (pseudo time)
        params.set("delta time",dt);  // load factor increment (pseudo time increment)
        // set vector values needed by elements
        actdis->ClearState();
        actdis->SetState("residual displacement",disi);
        actdis->SetState("displacement",disn);
        fint->PutScalar(0.0);  // initialise internal force vector
        actdis->Evaluate(params,stiff_mat,null,fint,null,null);

        actdis->ClearState();
      }
      // complete stiffness matrix
      stiff_mat->Complete();

      // evaluate new residual fresm at current iterate numiter
      // R{istep,numiter} = F_int{istep,numiter} - F_ext{istep}
      fresm->Update(1.0,*fint,-1.0,*fextn,0.0);

      // keep a copy of fresm for contact forces / equilibrium check
      RCP<Epetra_Vector> fresmcopy= rcp(new Epetra_Vector(*fresm));
      
      //-------------------------make contact modifications to lhs and rhs
      //-------------------------------------------------update active set
      fresm->Scale(-1.0);     // rhs = -R = -fresm
      contactmanager->SetState("displacement",disn);
      
      contactmanager->InitializeMortar(numiter+1);
      contactmanager->EvaluateMortar(numiter+1);
      
      // this is the correct place to update the active set!!!
      // (on the one hand we need the new weighted gap vector g, which is
      // computed in EvaluateMortar() above and on the other hand we want to
      // run the Evaluate()routine below with the NEW active set already)
      contactmanager->UpdateActiveSetSemiSmooth(disn);
      
      contactmanager->Initialize(numiter+1);
      contactmanager->Evaluate(stiff_mat,fresm,numiter+1);
      
      // blank residual at DOFs on Dirichlet BC
      {
        Epetra_Vector fresmdbc(*fresm);
        fresm->Multiply(1.0,*invtoggle,fresmdbc,0.0);
      }
          
      //--------------------------------------------------- contact forces
      contactmanager->ContactForces(fresmcopy);
      
#ifdef CONTACTGMSH2
    contactmanager->VisualizeGmsh(istep+1,numiter+1);
#endif // #ifdef CONTACTGMSH2
    
      //for (int k=0;k<fint->MyLength();++k)
      //  cout << (*fint)[k] << " " << -(*fextn)[k] << " " << (*fc)[k] << endl;

      //---------------------------------------------- build res/disi norm
      fresm->Norm2(&norm);
      disi->Norm2(&disinorm);

      // a short message
      if (!myrank)
      {
        printf("numiter %d res-norm %e dis-norm %e \n",numiter+1, norm, disinorm);
        fprintf(errfile,"numiter %d res-norm %e dis-norm %e\n",numiter+1, norm, disinorm);
        fflush(stdout);
        fflush(errfile);
      }

      //--------------------------------- increment equilibrium loop index
      ++numiter;
      
    } //
    //============================================= end equilibrium loop

    //-------------------------------- test whether max iterations was hit
    if (statvar->maxiter == 1 && statvar->nstep == 1)
      printf("computed 1 step with 1 iteration: STATIC LINEAR SOLUTION\n");
    else if (numiter==statvar->maxiter)
      dserror("Newton unconverged in %d iterations",numiter);
  }

    //********************************************************************
    // 2) FIXED-POINT APPROACH
    // The search for the correct active set (=contact nonlinearity) is
    // represented by a fixed-point approach, whereas the large deformation
    // linearization (=geimetrical nonlinearity) is treated by a standard
    // Newton scheme. This yields TWO nested iteration loops
    //********************************************************************
  else
  {
    //============================================ start of active set loop
    while (contactmanager->ActiveSetConverged()==false)
    {
      //--------------------------------------------------- predicting state
      // constant predictor : displacement in domain
      disn->Update(1.0, *dis, 0.0);

      // eval fint and stiffness matrix at current istep
      // and apply new displacements at DBCs
      {
        // destroy and create new matrix
        stiff_mat->Zero();
        // create the parameters for the discretization
        ParameterList params;
        // action for elements
        params.set("action","calc_struct_nlnstiff");
        // other parameters needed by the elements
        params.set("total time",timen);  // load factor (pseudo time)
        params.set("delta time",dt);  // load factor increment (pseudo time increment)
        // set vector values needed by elements
        actdis->ClearState();
        actdis->SetState("residual displacement",disi);
        // predicted dirichlet values
        // disn then also holds prescribed new dirichlet displacements
        actdis->EvaluateDirichlet(params,disn,null,null,dirichtoggle);
        actdis->SetState("displacement",disn);
        fint->PutScalar(0.0);  // initialise internal force vector
        actdis->Evaluate(params,stiff_mat,null,fint,null,null);
        // predicted rhs
        fextn->PutScalar(0.0);  // initialize external force vector (load vect)
        actdis->EvaluateNeumann(params,*fextn); // *fext holds external force vector at current step

        actdis->ClearState();
      }

      // complete stiffness matrix
      stiff_mat->Complete();

      double stiffnorm;
      stiffnorm = stiff_mat->NormFrobenius();

      // evaluate residual at current istep
      // R{istep,numiter=0} = F_int{istep,numiter=0} - F_ext{istep}
      fresm->Update(1.0,*fint,-1.0,*fextn,0.0);

      // keep a copy of fresm for contact forces / equilibrium check
      RCP<Epetra_Vector> fresmcopy= rcp(new Epetra_Vector(*fresm));
      
      // reset Lagrange multipliers to last converged state
      // this resetting is necessary due to multiple active set steps
      RCP<Epetra_Vector> z = contactmanager->LagrMult();
      RCP<Epetra_Vector> zold = contactmanager->LagrMultOld();
      z->Update(1.0,*zold,0.0);
      contactmanager->StoreNodalQuantities(Manager::lmcurrent);
      
      // friction  
      // reset displacement jumps (slave dofs)
      RCP<Epetra_Vector> jump = contactmanager->Jump();
      jump->Scale(0.0); 
      contactmanager->StoreNodalQuantities(Manager::jump);
           
      //-------------------------- make contact modifications to lhs and rhs
      fresm->Scale(-1.0);     // rhs = -R = -fresm
      contactmanager->SetState("displacement",disn);
      
      contactmanager->InitializeMortar(0);
      contactmanager->EvaluateMortar(0);
      
      contactmanager->Initialize(0);
      contactmanager->Evaluate(stiff_mat,fresm,0);
      
      // blank residual at DOFs on Dirichlet BC
      {
        Epetra_Vector fresmdbc(*fresm);
        fresm->Multiply(1.0,*invtoggle,fresmdbc,0.0);
      }
          
      //---------------------------------------------------- contact forces
      contactmanager->ContactForces(fresmcopy);
      
#ifdef CONTACTGMSH2
    dserror("Gmsh Output for every iteration only implemented for semi-smooth Newton");
#endif // #ifdef CONTACTGMSH2
    
      //----------------------------------------------- build res/disi norm
      double norm;
      fresm->Norm2(&norm);
      double disinorm = 1.0;

      if (!myrank) cout << " Predictor residual forces " << norm << endl; fflush(stdout);

      // reset Newton iteration counter
      numiter=0;

      //===========================================start of equilibrium loop
      while (((norm > statvar->tolresid) || (disinorm > statvar->toldisp)) && numiter < statvar->maxiter)
      {
        //----------------------- apply dirichlet BCs to system of equations
        disi->PutScalar(0.0);   // Useful? depends on solver and more
        LINALG::ApplyDirichlettoSystem(stiff_mat,disi,fresm,zeros,dirichtoggle);

        //Do usual newton step
        // solve for disi
        // Solve K . IncD = -R  ===>  IncD_{n+1}
        if (numiter==0)
        {
          solver.Solve(stiff_mat->EpetraMatrix(),disi,fresm,true,true);
        }
        else
        {
          solver.Solve(stiff_mat->EpetraMatrix(),disi,fresm,true,false);
        }

        //------------------------------------- recover disi and Lagr. Mult.
        {
          contactmanager->Recover(disi);
        }

        // update displacements
        // D_{istep,numiter+1} := D_{istep,numiter} + IncD_{numiter}
        disn->Update(1.0, *disi, 1.0);

        // compute internal forces and stiffness at current iterate numiter
        {
          // zero out stiffness
          stiff_mat->Zero();
          // create the parameters for the discretization
          ParameterList params;
          // action for elements
          params.set("action","calc_struct_nlnstiff");
          // other parameters needed by the elements
          params.set("total time",timen);  // load factor (pseudo time)
          params.set("delta time",dt);  // load factor increment (pseudo time increment)
          // set vector values needed by elements
          actdis->ClearState();
          actdis->SetState("residual displacement",disi);
          actdis->SetState("displacement",disn);
          fint->PutScalar(0.0);  // initialise internal force vector
          actdis->Evaluate(params,stiff_mat,null,fint,null,null);

          actdis->ClearState();
        }
        // complete stiffness matrix
        stiff_mat->Complete();

        // evaluate new residual fresm at current iterate numiter
        // R{istep,numiter} = F_int{istep,numiter} - F_ext{istep}
        fresm->Update(1.0,*fint,-1.0,*fextn,0.0);

        // keep a copy of fresm for contact forces / equilibrium check
        RCP<Epetra_Vector> fresmcopy= rcp(new Epetra_Vector(*fresm));
           
        //-------------------------make contact modifications to lhs and rhs
        fresm->Scale(-1.0);     // rhs = -R = -fresm
        contactmanager->SetState("displacement",disn);
        
        contactmanager->InitializeMortar(numiter+1);
        contactmanager->EvaluateMortar(numiter+1);
        
        contactmanager->Initialize(numiter+1);
        contactmanager->Evaluate(stiff_mat,fresm,numiter+1);
        
        // blank residual at DOFs on Dirichlet BC
        {
          Epetra_Vector fresmdbc(*fresm);
          fresm->Multiply(1.0,*invtoggle,fresmdbc,0.0);
        }
            
        //--------------------------------------------------- contact forces
        contactmanager->ContactForces(fresmcopy);
              
        //for (int k=0;k<fint->MyLength();++k)
        //  cout << (*fint)[k] << " " << -(*fextn)[k] << " " << (*fc)[k] << endl;
              
        //---------------------------------------------- build res/disi norm
        fresm->Norm2(&norm);
        disi->Norm2(&disinorm);

        // a short message
        if (!myrank)
        {
          printf("numiter %d res-norm %e dis-norm %e \n",numiter+1, norm, disinorm);
          fprintf(errfile,"numiter %d res-norm %e dis-norm %e\n",numiter+1, norm, disinorm);
          fflush(stdout);
          fflush(errfile);
        }

        //--------------------------------- increment equilibrium loop index
        ++numiter;
      } //
      //============================================= end equilibrium loop

      //-------------------------------- test whether max iterations was hit
      if (statvar->maxiter == 1 && statvar->nstep == 1)
        printf("computed 1 step with 1 iteration: STATIC LINEAR SOLUTION\n");
      else if (numiter==statvar->maxiter)
        dserror("Newton unconverged in %d iterations",numiter);

      // update active set
      // (in the fixed-point-approach this is done only after convergence
      // of the Newton loop representing the geometrical nonlinearity)
      contactmanager->UpdateActiveSet(disn);
    }
    //================================================ end active set loop
  }
    //********************************************************************
    // END: options for primal-dual active set strategy (PDASS)
    //********************************************************************
  
    //---------------------------- determine new end-quantities and update
    // new displacements at t_{n+1} -> t_n
    // D_{n} := D_{n+1}
    dis->Update(1.0, *disn, 0.0);

    //----- update anything that needs to be updated at the element level
    {
      // create the parameters for the discretization
      ParameterList params;
      // action for elements
      params.set("action","calc_struct_update_istep");
      // other parameters that might be needed by the elements
      params.set("total time",timen);
      params.set("delta time",dt);
      actdis->Evaluate(params,null,null,null,null,null);
    }

    //------------------------------------------- increment time/load step
    ++istep;      // load step n := n + 1
    time += dt;   // load factor / pseudo time  t_n := t_{n+1} = t_n + Delta t

    //-------------------------------------------- print contact to screen
    contactmanager->PrintActiveSet();      
    
#ifdef CONTACTGMSH1
    contactmanager->VisualizeGmsh(istep);
#endif // #ifdef CONTACTGMSH1
  
    //-------------------------------- update contact Lagrange multipliers
    RCP<Epetra_Vector> stepz = contactmanager->LagrMult();
    RCP<Epetra_Vector> stepzold = contactmanager->LagrMultOld();
    stepzold->Update(1.0,*stepz,0.0);
    contactmanager->StoreNodalQuantities(Manager::lmold);

    //------------------------------------------------- write restart step
    bool isdatawritten = false;
    if (istep % statvar->resevery_restart==0)
    {
      output.WriteMesh(istep,time);
      output.NewStep(istep, time);
      output.WriteVector("displacement",dis);
      //output.WriteVector("fexternal", fext);
      isdatawritten = true;

      // write restart information for contact
      RCP<Epetra_Vector> zold = contactmanager->LagrMultOld();
      RCP<Epetra_Vector> activetoggle = contactmanager->WriteRestart();
      output.WriteVector("lagrmultold",zold);
      output.WriteVector("activetoggle",activetoggle);
          
      if (!myrank)
      {
        cout << "====== Restart written in step " << istep << endl;
        fflush(stdout);
        fprintf(errfile,"====== Restart written in step %d\n",istep);
        fflush(errfile);
      }
    }

    //----------------------------------------------------- output results
    int mod_disp   = istep % statvar->resevry_disp;
    if (!mod_disp && Teuchos::getIntegralValue<int>(ioflags,"STRUCT_DISP")==1 && !isdatawritten)
    {
      output.NewStep(istep, time);
      output.WriteVector("displacement", dis);
      output.WriteElementData();
      isdatawritten = true;
    }

    //---------------------------------------------- do stress calculation
    int mod_stress = istep % statvar->resevry_stress;
    string iostress;
    switch (Teuchos::getIntegralValue<STRUCT_STRESS_TYP>(ioflags,"STRUCT_STRESS"))
    {
    case struct_stress_none:
      iostress = "none";
      break;
    case struct_stress_cauchy:
      iostress = "cauchy";
      break;
    case struct_stress_pk:
      iostress = "2PK";
      break;
    default:
      iostress = "none";
      break;
    }
    string iostrain;
    switch (Teuchos::getIntegralValue<STRUCT_STRAIN_TYP>(ioflags,"STRUCT_STRAIN"))
    {
    case struct_strain_none:
      iostrain = "none";
      break;
    case struct_strain_gl:
      iostrain = "green_lagrange";
      break;
    case struct_strain_ea:
      iostrain = "euler_almansi";
      break;
    default:
      iostrain = "none";
      break;
    }
    if (!mod_stress && iostress!="none")
    {
      // create the parameters for the discretization
      ParameterList p;
      // action for elements
      p.set("action","calc_struct_stress");
      // other parameters that might be needed by the elements
      p.set("total time",timen);
      p.set("delta time",dt);
      Teuchos::RCP<std::vector<char> > stress = Teuchos::rcp(new std::vector<char>());
      Teuchos::RCP<std::vector<char> > strain = Teuchos::rcp(new std::vector<char>());
      p.set("stress", stress);
      p.set("strain", strain);
      if (iostress == "cauchy")   // output of Cauchy stresses instead of 2PK stresses
      {
        p.set("cauchy", true);
      }
      else
      {
        p.set("cauchy", false);
      }
      p.set("iostrain", iostrain);
      // set vector values needed by elements
      actdis->ClearState();
      actdis->SetState("residual displacement",zeros);
      actdis->SetState("displacement",dis);
      actdis->Evaluate(p,null,null,null,null,null);
      actdis->ClearState();
      if (iostress == "cauchy")
        output.WriteVector("gauss_cauchy_stresses_xyz",*stress,*(actdis->ElementColMap()));
      else
        output.WriteVector("gauss_2PK_stresses_xyz",*stress,*(actdis->ElementColMap()));
      if (iostrain != "none")
      {
        if (iostrain == "euler_almansi")
        {
          output.WriteVector("gauss_EA_strains_xyz",*strain,*(actdis->ElementColMap()));
        }
        else
        {
          output.WriteVector("gauss_GL_strains_xyz",*strain,*(actdis->ElementColMap()));
        }
      }
    }

    //---------------------------------------------------------- print out
    if (!myrank)
    {
      printf("step %6d | nstep %6d | time %-14.8E | dt %-14.8E | numiter %3d\n",
             istep,statvar->nstep,timen,dt,numiter);
      fprintf(errfile,"step %6d | nstep %6d | time %-14.8E | dt %-14.8E | numiter %3d\n",
              istep,statvar->nstep,timen,dt,numiter);
      printf("----------------------------------------------------------------------------------\n");
      fprintf(errfile,"----------------------------------------------------------------------------------\n");
      fflush(stdout);
      fflush(errfile);
    }

  }  //=============================================end time/loadstep loop

  // Structure Resulttests
  DRT::ResultTestManager testmanager(actdis->Comm());
  testmanager.AddFieldTest(rcp(new StruResultTest(actdis,dis,null,null)));
  testmanager.TestAll();

  //----------------------------- this is the end my lonely friend the end
  return;
} // end of contact_stru_static_drt()
} // namespace CONTACT

#endif  // #ifdef CCADISCRET
