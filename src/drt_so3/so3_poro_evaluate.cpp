/*!----------------------------------------------------------------------
\file so3_poro_evaluate.cpp
\brief

<pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15251
</pre>

*----------------------------------------------------------------------*/

#include "so3_poro.H"
#include "so3_poro_eletypes.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_timecurve.H"
#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_serialdensevector.H"
#include "Epetra_SerialDenseSolver.h"
#include <iterator>

#include "../drt_mat/fluidporo.H"
#include "../drt_mat/structporo.H"

#include "../drt_inpar/inpar_structure.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_fem_general/drt_utils_gder2.H"
#include "../drt_lib/drt_globalproblem.H"

#include "../drt_fem_general/drt_utils_integration.H"

#include "../drt_nurbs_discret/drt_nurbs_utils.H"
#include "../drt_fem_general/drt_utils_nurbs_shapefunctions.H"

#include "../drt_poroelast/poroelast_utils.H"

//#include "Sacado.hpp"

/*----------------------------------------------------------------------*
 |  preevaluate the element (public)                    vuong 03/12      |
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::PreEvaluate(Teuchos::ParameterList& params,
                                        DRT::Discretization&      discretization,
                                        DRT::Element::LocationArray& la)
{
  if(scatracoupling_)
  {
    if(la.Size()>2)
    {
      //ask for the number ofs dofs of second dofset (fluid)
      const int numdofpernode = discretization.NumDof(1,Nodes()[0]);

      if (la[1].Size() != numnod_*numdofpernode)
        dserror("calc_struct_nlnstiff: Location vector length for velocities does not match!");

      if (discretization.HasState(1,"scalar"))
      {
        // check if you can get the scalar state
        Teuchos::RCP<const Epetra_Vector> scalarnp
          = discretization.GetState(1,"scalar");

        if (scalarnp==Teuchos::null)
          dserror("calc_struct_nlnstiff: Cannot get state vector 'fluidvel' ");

        // extract local values of the global vectors
        Teuchos::RCP<std::vector<double> >myscalar = Teuchos::rcp(new std::vector<double>(la[1].lm_.size()) );
        DRT::UTILS::ExtractMyValues(*scalarnp,*myscalar,la[1].lm_);

        params.set<Teuchos::RCP<std::vector<double> > >("scalar",myscalar);
      }
    }
    else
    {
      const double time = params.get("total time",0.0);
    // find out whether we will use a time curve and get the factor
      int num = 0; // TO BE READ FROM INPUTFILE AT EACH ELEMENT!!!
      std::vector<double> xrefe; xrefe.resize(3);
      DRT::Node** nodes = Nodes();
      // get displacements of this element
    //  DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
     for (int i=0; i<numnod_; ++i){
        const double* x = nodes[i]->X();
        xrefe [0] +=  x[0]/numnod_;
        xrefe [1] +=  x[1]/numnod_;
        xrefe [2] +=  x[2]/numnod_;

      }
      const double* coordgpref = &xrefe[0];
      double functfac = DRT::Problem::Instance()->Funct(num).Evaluate(0,coordgpref,time,NULL);
      params.set<double>("scalar",functfac);
    }
  }
  else
  {
    /*
    Teuchos::RCP<std::vector<double> >xrefe = Teuchos::rcp(new std::vector<double>(3));
    DRT::Node** nodes = Nodes();
    for (int i=0; i<numnod_; ++i){
        const double* x = nodes[i]->X();
        (*xrefe)[0] +=  x[0]/numnod_;
        (*xrefe)[1] +=  x[1]/numnod_;
        (*xrefe)[2] +=  x[2]/numnod_;

     }
     params.set<Teuchos::RCP<std::vector<double> > >("position",xrefe);
     */
    //do nothing
  }//if(scatracoupling_)
  return;
}

/*----------------------------------------------------------------------*
 |  evaluate the element (public)                      vuong 03/12     |
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::So3_Poro< so3_ele, distype>::Evaluate(Teuchos::ParameterList& params,
                                    DRT::Discretization&      discretization,
                                    DRT::Element::LocationArray& la,
                                    Epetra_SerialDenseMatrix& elemat1_epetra,
                                    Epetra_SerialDenseMatrix& elemat2_epetra,
                                    Epetra_SerialDenseVector& elevec1_epetra,
                                    Epetra_SerialDenseVector& elevec2_epetra,
                                    Epetra_SerialDenseVector& elevec3_epetra)
{
  if(not init_)
    dserror("internal element data not initialized!");
  // start with "none"
  typename So3_Poro::ActionType act = So3_Poro::none;

  // get the required action
  std::string action = params.get<std::string>("action","none");
  if (action == "none") dserror("No action supplied");
  else if (action=="calc_struct_multidofsetcoupling")   act = So3_Poro::calc_struct_multidofsetcoupling;
  else if (action=="calc_struct_poroscatracoupling")   act = So3_Poro::calc_struct_poroscatracoupling;

  // what should the element do
  switch(act)
  {
  //==================================================================================
  // off diagonal terms in stiffness matrix for monolithic coupling
  case So3_Poro::calc_struct_multidofsetcoupling:
  {
    MyEvaluate(params,
                      discretization,
                      la,
                      elemat1_epetra,
                      elemat2_epetra,
                      elevec1_epetra,
                      elevec2_epetra,
                      elevec3_epetra);
  }
  break;
  case So3_Poro::calc_struct_poroscatracoupling:
    //no coupling-> return
  break;
  //==================================================================================
  default:
  {
    //in some cases we need to write/change some data before evaluating
    PreEvaluate(params,
                discretization,
                la);

    //evaluate parent solid element
    so3_ele::Evaluate(params,
                      discretization,
                      la[0].lm_,
                      elemat1_epetra,
                      elemat2_epetra,
                      elevec1_epetra,
                      elevec2_epetra,
                      elevec3_epetra);

    //add volume coupling specific terms
   MyEvaluate(params,
              discretization,
              la,
              elemat1_epetra,
              elemat2_epetra,
              elevec1_epetra,
              elevec2_epetra,
              elevec3_epetra);
  }
  break;
  } // action

  return 0;
}

/*----------------------------------------------------------------------*
 |  evaluate the element (protected)                    vuong 03/12       |
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::So3_Poro<so3_ele,distype>::MyEvaluate(
                                    Teuchos::ParameterList&      params,
                                    DRT::Discretization&         discretization,
                                    DRT::Element::LocationArray& la,
                                    Epetra_SerialDenseMatrix&    elemat1_epetra,
                                    Epetra_SerialDenseMatrix&    elemat2_epetra,
                                    Epetra_SerialDenseVector&    elevec1_epetra,
                                    Epetra_SerialDenseVector&    elevec2_epetra,
                                    Epetra_SerialDenseVector&    elevec3_epetra
                                    )
{
  // start with "none"
  ActionType act = none;

  // get the required action
  std::string action = params.get<std::string>("action","none");
  if (action == "none") dserror("No action supplied");
  else if (action=="calc_struct_internalforce")         act = calc_struct_internalforce;
  else if (action=="calc_struct_nlnstiff")              act = calc_struct_nlnstiff;
  else if (action=="calc_struct_nlnstiffmass")          act = calc_struct_nlnstiffmass;
  else if (action=="calc_struct_multidofsetcoupling")   act = calc_struct_multidofsetcoupling;
  else if (action=="calc_struct_stress")                act = calc_struct_stress;
  //else if (action=="postprocess_stress")                act = postprocess_stress;

  // what should the element do
  switch(act)
  {
  //==================================================================================
  // nonlinear stiffness, damping and internal force vector for poroelasticity
  case calc_struct_nlnstiff:
  {
    // stiffness
    LINALG::Matrix<numdof_,numdof_> elemat1(elemat1_epetra.A(),true);
    //damping
    LINALG::Matrix<numdof_,numdof_> elemat2(elemat2_epetra.A(),true);
    // internal force vector
    LINALG::Matrix<numdof_,1> elevec1(elevec1_epetra.A(),true);
    //LINALG::Matrix<numdof_,1> elevec2(elevec2_epetra.A(),true);
    // elemat2,elevec2+3 are not used anyway

    std::vector<int> lm = la[0].lm_;

    LINALG::Matrix<numdim_,numnod_> mydisp(true);
    ExtractValuesFromGlobalVector(discretization,0,lm, &mydisp, NULL,"displacement");

    LINALG::Matrix<numdof_,numdof_>* matptr = NULL;
    if (elemat1.IsInitialized()) matptr = &elemat1;

    enum INPAR::STR::DampKind damping = params.get<enum INPAR::STR::DampKind>("damping",INPAR::STR::damp_none);
    LINALG::Matrix<numdof_,numdof_>* matptr2 = NULL;
    if (elemat2.IsInitialized() and (damping==INPAR::STR::damp_material) ) matptr2 = &elemat2;

    if(la.Size()>1)
    {
      // need current fluid state,
      // call the fluid discretization: fluid equates 2nd dofset
      // disassemble velocities and pressures
      LINALG::Matrix<numdim_,numnod_> myvel(true);
      LINALG::Matrix<numdim_,numnod_> myfluidvel(true);
      LINALG::Matrix<numnod_,1> myepreaf(true);

      if (discretization.HasState(0,"velocity"))
        ExtractValuesFromGlobalVector(discretization,0,la[0].lm_, &myvel, NULL,"velocity");

      if (discretization.HasState(1,"fluidvel"))
        // extract local values of the global vectors
        ExtractValuesFromGlobalVector(discretization,1,la[1].lm_, &myfluidvel, &myepreaf,"fluidvel");

      //calculate tangent stiffness matrix
      nlnstiff_poroelast(lm,mydisp,myvel,myfluidvel,myepreaf,matptr,matptr2,&elevec1,//NULL,
          //NULL,NULL,
          params);
    }
  }
  break;

  //==================================================================================
  // nonlinear stiffness, mass matrix and internal force vector for poroelasticity
  case calc_struct_nlnstiffmass:
  {

    // stiffness
    LINALG::Matrix<numdof_,numdof_> elemat1(elemat1_epetra.A(),true);
    // mass
    //LINALG::Matrix<numdof_,numdof_> elemat2(elemat2_epetra.A(),true);
    // internal force vector
    LINALG::Matrix<numdof_,1> elevec1(elevec1_epetra.A(),true);
    //LINALG::Matrix<numdof_,1> elevec2(elevec2_epetra.A(),true);
    // elemat2,elevec2+3 are not used anyway

    // build the location vector only for the structure field
    std::vector<int> lm = la[0].lm_;

    LINALG::Matrix<numdim_,numnod_> mydisp(true);
    ExtractValuesFromGlobalVector(discretization,0,la[0].lm_, &mydisp, NULL,"displacement");

    LINALG::Matrix<numdof_,numdof_>* matptr = NULL;
    if (elemat1.IsInitialized()) matptr = &elemat1;

    if(isNurbs_)
    {
      // access knots and weights for this element
      bool zero_size = DRT::NURBS::GetMyNurbsKnotsAndWeights(discretization,this,myknots_,weights_);

      // if we have a zero sized element due to a interpolated point -> exit here
      if(zero_size)
        return 0;
    }

    // need current fluid state,
    // call the fluid discretization: fluid equates 2nd dofset
    // disassemble velocities and pressures

    if(la.Size()>1)
    {
      LINALG::Matrix<numdim_,numnod_> myvel(true);
      LINALG::Matrix<numdim_,numnod_> myfluidvel(true);
      LINALG::Matrix<numnod_,1> myepreaf(true);

      if (discretization.HasState(0,"velocity"))
        ExtractValuesFromGlobalVector(discretization,0,la[0].lm_, &myvel, NULL,"velocity");

      if (discretization.HasState(1,"fluidvel"))
        // extract local values of the global vectors
        ExtractValuesFromGlobalVector(discretization,1,la[1].lm_, &myfluidvel, &myepreaf,"fluidvel");

      //calculate tangent stiffness matrix
      nlnstiff_poroelast(lm,mydisp,myvel,myfluidvel,myepreaf,matptr,NULL,&elevec1,//NULL,
          //NULL,NULL,
          params);
    }

  }
  break;

  //==================================================================================
  // coupling terms in force-vector and stiffness matrix for poroelasticity
  case calc_struct_multidofsetcoupling:
  {
    // stiffness
    LINALG::Matrix<numdof_,(numdim_+1)*numnod_> elemat1(elemat1_epetra.A(),true);
    //LINALG::Matrix<numdof_,(numdim_+1)*numnod_> elemat2(elemat2_epetra.A(),true);

    // internal force vector
    //LINALG::Matrix<numdof_,1> elevec1(elevec1_epetra.A(),true);
    //LINALG::Matrix<numdof_,1> elevec2(elevec2_epetra.A(),true);

    // elemat2,elevec2+3 are not used anyway

    // build the location vector only for the structure field
    std::vector<int> lm = la[0].lm_;

    LINALG::Matrix<numdof_,(numdim_+1)*numnod_>* matptr = NULL;
    if (elemat1.IsInitialized()) matptr = &elemat1;

    if(isNurbs_)
    {
      // access knots and weights for this element
      bool zero_size = DRT::NURBS::GetMyNurbsKnotsAndWeights(discretization,this,myknots_,weights_);

      // if we have a zero sized element due to a interpolated point -> exit here
      if(zero_size)
        return 0;
    }

    // need current fluid state,
    // call the fluid discretization: fluid equates 2nd dofset
    // disassemble velocities and pressures
    if (discretization.HasState(1,"fluidvel"))
    {
      LINALG::Matrix<numdim_,numnod_> myvel(true);
      LINALG::Matrix<numdim_,numnod_> myfluidvel(true);
      LINALG::Matrix<numnod_,1> myepreaf(true);

      LINALG::Matrix<numdim_,numnod_> mydisp(true);
      ExtractValuesFromGlobalVector(discretization,0,la[0].lm_, &mydisp, NULL,"displacement");

      if (discretization.HasState(0,"velocity"))
        ExtractValuesFromGlobalVector(discretization,0,la[0].lm_, &myvel, NULL,"velocity");

      if (discretization.HasState(1,"fluidvel"))
        // extract local values of the global vectors
        ExtractValuesFromGlobalVector(discretization,1,la[1].lm_, &myfluidvel, &myepreaf,"fluidvel");

      coupling_poroelast(lm,mydisp,myvel,myfluidvel,myepreaf,matptr,//NULL,
          NULL,NULL,params);
    }

  }
  break;

  //==================================================================================
  // nonlinear stiffness and internal force vector for poroelasticity
  case calc_struct_internalforce:
  {
    // stiffness
    LINALG::Matrix<numdof_,numdof_> elemat1(elemat1_epetra.A(),true);
    LINALG::Matrix<numdof_,numdof_> elemat2(elemat2_epetra.A(),true);
    // internal force vector
    LINALG::Matrix<numdof_,1> elevec1(elevec1_epetra.A(),true);
    LINALG::Matrix<numdof_,1> elevec2(elevec2_epetra.A(),true);
    // elemat2,elevec2+3 are not used anyway

    // build the location vector only for the structure field
    std::vector<int> lm = la[0].lm_;

    LINALG::Matrix<numdim_,numnod_> mydisp(true);
    ExtractValuesFromGlobalVector(discretization,0,lm, &mydisp, NULL,"displacement");

    // need current fluid state,
    // call the fluid discretization: fluid equates 2nd dofset
    // disassemble velocities and pressures
    if (discretization.HasState(1,"fluidvel"))
    {
      // extract local values of the global vectors
      LINALG::Matrix<numdim_,numnod_> myfluidvel(true);
      LINALG::Matrix<numnod_,1> myepreaf(true);
      ExtractValuesFromGlobalVector(discretization,1,la[1].lm_, &myfluidvel, &myepreaf,"fluidvel");

      LINALG::Matrix<numdim_,numnod_> myvel(true);
      ExtractValuesFromGlobalVector(discretization,0,la[0].lm_, &myvel, NULL,"velocity");

      //calculate tangent stiffness matrix
      nlnstiff_poroelast(lm,mydisp,myvel,myfluidvel,myepreaf,NULL,NULL,&elevec1,//NULL,
          //NULL,NULL,
          params);
    }
  }
  break;

  //==================================================================================
  // evaluate stresses and strains at gauss points
  case calc_struct_stress:
  {
    // nothing to do for ghost elements
    if (discretization.Comm().MyPID()==so3_ele::Owner())
    {
      // get the location vector only for the structure field
      std::vector<int> lm = la[0].lm_;

      LINALG::Matrix<numdim_,numnod_> mydisp(true);
      ExtractValuesFromGlobalVector(discretization,0,lm, &mydisp, NULL,"displacement");

      Teuchos::RCP<std::vector<char> > couplstressdata
        = params.get<Teuchos::RCP<std::vector<char> > >("couplstress", Teuchos::null);

      if (couplstressdata==Teuchos::null) dserror("Cannot get 'couplstress' data");

      // initialize the coupling stress
      Epetra_SerialDenseMatrix couplstress(numgpt_,numstr_);

      INPAR::STR::StressType iocouplstress
        = DRT::INPUT::get<INPAR::STR::StressType>(params, "iocouplstress",
            INPAR::STR::stress_none);

      // need current fluid state,
      // call the fluid discretization: fluid equates 2nd dofset
      // disassemble velocities and pressures
      if (discretization.HasState(1,"fluidvel"))
      {
        // extract local values of the global vectors
        LINALG::Matrix<numdim_,numnod_> myfluidvel(true);
        LINALG::Matrix<numnod_,1> myepreaf(true);
        ExtractValuesFromGlobalVector(discretization,1,la[1].lm_, &myfluidvel, &myepreaf,"fluidvel");

        couplstress_poroelast(mydisp,
                              myfluidvel,
                              myepreaf,
                              &couplstress,
                              NULL,
                              params,
                              iocouplstress);
      }

      // pack the data for postprocessing
      {
        DRT::PackBuffer data;
        // get the size of stress
        so3_ele::AddtoPack(data, couplstress);
        data.StartPacking();
        // pack the stresses
        so3_ele::AddtoPack(data, couplstress);
        std::copy(data().begin(),data().end(),std::back_inserter(*couplstressdata));
      }
    }  // end proc Owner
  }  // calc_struct_stress
  break;
  //==================================================================================
  default:
    //do nothing (no error because there are some actions the poro element is supposed to ignore)
    break;
  } // action
  return 0;
}


/*----------------------------------------------------------------------*
 |  evaluate only the poroelasticity fraction for the element (protected)  vuong 03/12 |
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::nlnstiff_poroelast(
    std::vector<int>&                           lm,           // location matrix
    LINALG::Matrix<numdim_, numnod_>&           disp,         // current displacements
    LINALG::Matrix<numdim_, numnod_>&           vel,          // current velocities
    LINALG::Matrix<numdim_, numnod_> &          evelnp,       // current fluid velocities
    LINALG::Matrix<numnod_, 1> &                epreaf,       // current fluid pressure
    LINALG::Matrix<numdof_, numdof_>*           stiffmatrix,  // element stiffness matrix
    LINALG::Matrix<numdof_, numdof_>*           reamatrix,    // element reactive matrix
    LINALG::Matrix<numdof_, 1>*                 force,        // element internal force vector
    //LINALG::Matrix<numgptpar_, numstr_>* elestress, // stresses at GP
    //LINALG::Matrix<numgptpar_, numstr_>* elestrain, // strains at GP
    Teuchos::ParameterList&                     params        // algorithmic parameters e.g. time
 //   const INPAR::STR::StressType       iostress     // stress output option
    )
{
  GetMaterials();

  // update element geometry
  LINALG::Matrix<numdim_,numnod_> xrefe; // material coord. of element
  LINALG::Matrix<numdim_,numnod_> xcurr; // current  coord. of element

  DRT::Node** nodes = Nodes();
  for (int i=0; i<numnod_; ++i)
  {
    const double* x = nodes[i]->X();
    for(int j=0; j<numdim_;j++)
    {
      xrefe(j,i) = x[j];
      xcurr(j,i) = xrefe(j,i) + disp(j,i);
    }
  }

  //initialize element matrizes and vectors
  LINALG::Matrix<numdof_,numdof_> erea_v(true);

  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/
  GaussPointLoop(  params,
                        xrefe,
                        xcurr,
                        disp,
                        vel,
                        evelnp,
                        epreaf,
                        NULL,
                        erea_v,
                        stiffmatrix,
                        force);

  // update stiffness matrix
  if (stiffmatrix != NULL)
  {
    if ( reamatrix != NULL )
    {
      /* additional "reactive darcy-term"
       detJ * w(gp) * ( J * reacoeff * phi^2  ) * D(v_s)
       */
      reamatrix->Update(1.0,erea_v,1.0);
    }
  }

  return;
}  // nlnstiff_poroelast()

/*---------------------------------------------------------------------------------*
 |  evaluate only the poroelasticity fraction for the element (protected)   vuong 03/12|
 *----------------------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::GaussPointLoop(
                                    Teuchos::ParameterList&                 params,
                                    const LINALG::Matrix<numdim_,numnod_>&  xrefe,
                                    const LINALG::Matrix<numdim_,numnod_>&  xcurr,
                                    const LINALG::Matrix<numdim_,numnod_>&  nodaldisp,
                                    const LINALG::Matrix<numdim_,numnod_>&  nodalvel,
                                    const LINALG::Matrix<numdim_,numnod_> & evelnp,
                                    const LINALG::Matrix<numnod_,1> &       epreaf,
                                    const LINALG::Matrix<numnod_, 1>*       porosity_dof,
                                    LINALG::Matrix<numdof_,numdof_>&        erea_v,
                                    LINALG::Matrix<numdof_, numdof_>*       stiffmatrix,
                                    LINALG::Matrix<numdof_,1>*              force
                                        )
{

  LINALG::Matrix<numdim_,numnod_> N_XYZ;
  // build deformation gradient wrt to material configuration
  // in case of prestressing, build defgrd wrt to last stored configuration
  // CAUTION: defgrd(true): filled with zeros!
  LINALG::Matrix<numdim_,numdim_> defgrd(true);
  LINALG::Matrix<numnod_,1> shapefct;
  LINALG::Matrix<numdim_,numnod_> deriv ;

  LINALG::Matrix<numstr_,1> fstress(true);

  for (int gp=0; gp<numgpt_; ++gp)
  {
    //evaluate shape functions and derivatives at integration point
    ComputeShapeFunctionsAndDerivatives(gp,shapefct,deriv,N_XYZ);

    //jacobian determinant of transformation between spatial and material space "|dx/dX|"
    const double J = ComputeJacobianDeterminant(gp,xcurr,deriv);

    //----------------------------------------------------
    // pressure at integration point
    double press = shapefct.Dot(epreaf);

    // structure displacement and velocity at integration point
    LINALG::Matrix<numdim_,1> velint(true);

    for(int i=0; i<numnod_; i++)
      for(int j=0; j<numdim_; j++)
        velint(j) += nodalvel(j,i) * shapefct(i);

    // fluid velocity at integration point
    LINALG::Matrix<numdim_,1> fvelint;
    fvelint.Multiply(evelnp,shapefct);

    // material fluid velocity gradient at integration point
    LINALG::Matrix<numdim_,numdim_>              fvelder;
    fvelder.MultiplyNT(evelnp,N_XYZ);

    // pressure gradient at integration point
    LINALG::Matrix<numdim_,1> Gradp;
    Gradp.Multiply(N_XYZ,epreaf);

    // (material) deformation gradient F = d xcurr / d xrefe = xcurr * N_XYZ^T
    ComputeDefGradient(defgrd,N_XYZ,xcurr);

    // non-linear B-operator
    LINALG::Matrix<numstr_,numdof_> bop;
    ComputeBOperator(bop,defgrd,N_XYZ);

    // Right Cauchy-Green tensor = F^T * F
    LINALG::Matrix<numdim_,numdim_> cauchygreen;
    cauchygreen.MultiplyTN(defgrd,defgrd);

    // inverse Right Cauchy-Green tensor
    LINALG::Matrix<numdim_,numdim_> C_inv(false);
    C_inv.Invert(cauchygreen);

    // inverse deformation gradient F^-1
    LINALG::Matrix<numdim_,numdim_> defgrd_inv(false);
    defgrd_inv.Invert(defgrd);

    //------linearization of jacobi determinant detF=J w.r.t. strucuture displacement   dJ/d(us) = dJ/dF : dF/dus = J * F^-T * N,X
    LINALG::Matrix<1,numdof_> dJ_dus ;
    ComputeLinearizationOfJacobian(dJ_dus,J,N_XYZ,defgrd_inv);

    // compute some auxiliary matrixes for computation of linearization
    //dF^-T/dus
    LINALG::Matrix<numdim_*numdim_,numdof_> dFinvTdus(true);
    //F^-T * Grad p
    LINALG::Matrix<numdim_,1> Finvgradp;
    //dF^-T/dus * Grad p
    LINALG::Matrix<numdim_,numdof_> dFinvdus_gradp(true);
    //dC^-1/dus * Grad p
    LINALG::Matrix<numstr_,numdof_> dCinv_dus (true);

    ComputeAuxiliaryValues(N_XYZ,defgrd_inv,C_inv,Gradp,dFinvTdus,Finvgradp,dFinvdus_gradp,dCinv_dus);

    //linearization of porosity w.r.t structure displacement d\phi/d(us) = d\phi/dJ*dJ/d(us)
    LINALG::Matrix<1,numdof_> dphi_dus;
    double porosity=0.0;

    ComputePorosityAndLinearization(params,press,J,gp,shapefct,porosity_dof,dJ_dus,porosity,dphi_dus);

    // **********************fill stiffness matrix and force vector+++++++++++++++++++++++++
    if(fluidmat_->Type() == MAT::PAR::darcy_brinkman)
    {
      FillMatrixAndVectorsBrinkman(
                                    gp,
                                    J,
                                    porosity,
                                    fvelder,
                                    defgrd_inv,
                                    bop,
                                    C_inv,
                                    dphi_dus,
                                    dJ_dus,
                                    dCinv_dus,
                                    dFinvTdus,
                                    stiffmatrix,
                                    force,
                                    fstress);
    }

    FillMatrixAndVectors(   gp,
                            shapefct,
                            N_XYZ,
                            J,
                            press,
                            porosity,
                            velint,
                            fvelint,
                            fvelder,
                            defgrd_inv,
                            bop,
                            C_inv,
                            Finvgradp,
                            dphi_dus,
                            dJ_dus,
                            dCinv_dus,
                            dFinvdus_gradp,
                            dFinvTdus,
                            erea_v,
                            stiffmatrix,
                            force,
                            fstress);

    /* =========================================================================*/
  }/* ==================================================== end of Loop over GP */
  /* =========================================================================*/
}

/*--------------------------------------------------------------------------------*
 |  evaluate only the poroelasticity fraction for the element (protected)  vuong 03/12 |
 *----------------------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::coupling_poroelast(
    std::vector<int>&                                 lm,            // location matrix
    LINALG::Matrix<numdim_, numnod_>&                 disp,          // current displacements
    LINALG::Matrix<numdim_, numnod_>&                 vel,           // current velocities
    LINALG::Matrix<numdim_, numnod_> &                evelnp,        //current fluid velocity
    LINALG::Matrix<numnod_, 1> &                      epreaf,        //current fluid pressure
    LINALG::Matrix<numdof_, (numdim_ + 1) * numnod_>* stiffmatrix,   // element stiffness matrix
    LINALG::Matrix<numdof_, (numdim_ + 1) * numnod_>* reamatrix,     // element reactive matrix
    LINALG::Matrix<numdof_, 1>*                       force,         // element internal force vector
    Teuchos::ParameterList&                           params)        // algorithmic parameters e.g. time
{
  //=============================get parameters

  GetMaterials();

  //=======================================================================

  // update element geometry
  LINALG::Matrix<numdim_,numnod_> xrefe; // material coord. of element
  LINALG::Matrix<numdim_,numnod_> xcurr; // current  coord. of element

  DRT::Node** nodes = Nodes();
  for (int i=0; i<numnod_; ++i)
  {
    const double* x = nodes[i]->X();
    for(int j=0; j<numdim_;j++)
    {
      xrefe(j,i) = x[j];
      xcurr(j,i) = xrefe(j,i) + disp(j,i);
    }
  }
  //initialize element matrizes
  LINALG::Matrix<numdof_,numnod_> ecoupl_p(true);
  LINALG::Matrix<numdof_,numdof_> ecoupl_v(true);

  /* =========================================================================*/
  /* ================================================= Loop over Gauss Points */
  /* =========================================================================*/

  GaussPointLoopOD( params,
                         xrefe,
                         xcurr,
                         disp,
                         vel,
                         evelnp,
                         epreaf,
                         stiffmatrix );

  if (stiffmatrix != NULL)
  {
    //TODO
    // build tangent coupling matrix : effective dynamic stiffness coupling matrix
    //    K_{Teffdyn} = 1/dt C
    //                + theta K_{T}
    const double theta = params.get<double>("theta");

    stiffmatrix->Scale(theta);
  }

  return;

}  // coupling_poroelast()

/*---------------------------------------------------------------------------------*
 |  evaluate only the poroelasticity fraction for the element (protected)  vuong 03/12|
 *------------------------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::GaussPointLoopOD(
                Teuchos::ParameterList&                           params,
                const LINALG::Matrix<numdim_,numnod_>&            xrefe,
                const LINALG::Matrix<numdim_,numnod_>&            xcurr,
                const LINALG::Matrix<numdim_,numnod_>&            nodaldisp,
                const LINALG::Matrix<numdim_,numnod_>&            nodalvel,
                const LINALG::Matrix<numdim_,numnod_>&            evelnp,
                const LINALG::Matrix<numnod_,1> &                 epreaf,
                LINALG::Matrix<numdof_, (numdim_ + 1) * numnod_>* stiffmatrix
                                        )
{

  LINALG::Matrix<numdim_,numnod_> N_XYZ;       //  first derivatives at gausspoint w.r.t. X, Y,Z
  // build deformation gradient wrt to material configuration
  // in case of prestressing, build defgrd wrt to last stored configuration
  // CAUTION: defgrd(true): filled with zeros!
  LINALG::Matrix<numdim_,numdim_> defgrd(true); //  deformation gradiant evaluated at gauss point
  LINALG::Matrix<numnod_,1> shapefct;           //  shape functions evalulated at gauss point
  LINALG::Matrix<numdim_,numnod_> deriv(true);  //  first derivatives at gausspoint w.r.t. r,s,t

  for (int gp=0; gp<numgpt_; ++gp)
  {
    //evaluate shape functions and derivatives at integration point
    ComputeShapeFunctionsAndDerivatives(gp,shapefct,deriv,N_XYZ);
    //evaluate second derivatives of shape functions at integration point
    //ComputeSecondDerivativesOfShapeFunctions(gp,xrefe,deriv,deriv2,N_XYZ,N_XYZ2);

    const double J = ComputeJacobianDeterminant(gp,xcurr,deriv);

    // (material) deformation gradient F = d xcurr / d xrefe = xcurr * N_XYZ^T
    ComputeDefGradient(defgrd,N_XYZ,xcurr);

    // non-linear B-operator
    LINALG::Matrix<numstr_,numdof_> bop;
    ComputeBOperator(bop,defgrd,N_XYZ);

    // -----------------Right Cauchy-Green tensor = F^T * F
    LINALG::Matrix<numdim_,numdim_> cauchygreen;
    cauchygreen.MultiplyTN(defgrd,defgrd);

    //------------------ inverse Right Cauchy-Green tensor
    LINALG::Matrix<numdim_,numdim_> C_inv(false);
    C_inv.Invert(cauchygreen);

    //---------------- get pressure at integration point
    double press = shapefct.Dot(epreaf);

    //------------------ get material pressure gradient at integration point
    LINALG::Matrix<numdim_,1> Gradp;
    Gradp.Multiply(N_XYZ,epreaf);

    //--------------------- get fluid velocity at integration point
    LINALG::Matrix<numdim_,1> fvelint;
    fvelint.Multiply(evelnp,shapefct);

    // material fluid velocity gradient at integration point
    LINALG::Matrix<numdim_,numdim_>              fvelder;
    fvelder.MultiplyNT(evelnp,N_XYZ);

    //! ----------------structure velocity at integration point
    LINALG::Matrix<numdim_,1> velint(true);
    for(int i=0; i<numnod_; i++)
      for(int j=0; j<numdim_; j++)
        velint(j) += nodalvel(j,i) * shapefct(i);

    // inverse deformation gradient F^-1
    LINALG::Matrix<numdim_,numdim_> defgrd_inv(false);
    defgrd_inv.Invert(defgrd);

    //**************************************************+auxilary variables for computing the porosity and linearization
    double dphi_dp=0.0;
    double porosity=0.0;

    ComputePorosityAndLinearizationOD(params,
                                      press,
                                      J,
                                      gp,
                                      shapefct,
                                      NULL,
                                      porosity,
                                      dphi_dp);

    // **********************evaluate stiffness matrix and force vector+++++++++++++++++++++++++

    FillMatrixAndVectorsOD(
                              gp,
                              shapefct,
                              N_XYZ,
                              J,
                              porosity,
                              dphi_dp,
                              velint,
                              fvelint,
                              defgrd_inv,
                              Gradp,
                              bop,
                              C_inv,
                              stiffmatrix);

    if(fluidmat_->Type() == MAT::PAR::darcy_brinkman)
    {
      FillMatrixAndVectorsBrinkmanOD(
                                      gp,
                                      shapefct,
                                      N_XYZ,
                                      J,
                                      porosity,
                                      dphi_dp,
                                      fvelder,
                                      defgrd_inv,
                                      bop,
                                      C_inv,
                                      stiffmatrix);
    }//darcy-brinkman
    /* =========================================================================*/
  }/* ==================================================== end of Loop over GP */
  /* =========================================================================*/
}


/*------------------------------------------------------------------------------------*
 |  evaluate only the poroelasticity fraction for the element (protected)   vuong 03/12|
 *----------------------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::couplstress_poroelast(
    LINALG::Matrix<numdim_, numnod_>&   disp,         // current displacements
    LINALG::Matrix<numdim_, numnod_> &  evelnp,       // current fluid velocities
    LINALG::Matrix<numnod_, 1> &        epreaf,       // current fluid pressure
    Epetra_SerialDenseMatrix*           elestress,    // stresses at GP
    Epetra_SerialDenseMatrix*           elestrain,    // strains at GP
    Teuchos::ParameterList&             params,       // algorithmic parameters e.g. time
    const INPAR::STR::StressType        iostress      // stress output option
    )
{

  // update element geometry
  LINALG::Matrix<numdim_,numnod_> xrefe; // material coord. of element
  LINALG::Matrix<numdim_,numnod_> xcurr; // current  coord. of element

  DRT::Node** nodes = Nodes();
  for (int i=0; i<numnod_; ++i)
  {
    const double* x = nodes[i]->X();
    for(int j=0; j<numdim_;j++)
    {
      xrefe(j,i) = x[j];
      xcurr(j,i) = xrefe(j,i) + disp(j,i);
    }
  }

  //get structure material
  Teuchos::RCP< MAT::StructPoro > structmat = Teuchos::rcp_dynamic_cast<MAT::StructPoro>(Material());
  if(structmat->MaterialType() != INPAR::MAT::m_structporo)
    dserror("invalid structure material for poroelasticity");

  LINALG::Matrix<numnod_,1> shapefct;
  LINALG::Matrix<numdim_,numdim_> defgrd(true);
  LINALG::Matrix<numdim_,numnod_> N_XYZ;
  LINALG::Matrix<numdim_,numnod_> deriv ;

  DRT::UTILS::GaussRule3D gaussrule = DRT::UTILS::intrule3D_undefined;
  switch(distype)
  {
  case DRT::Element::hex8 :
    gaussrule = DRT::UTILS::intrule_hex_8point;
    break;
  case DRT::Element::hex27 :
    gaussrule = DRT::UTILS::intrule_hex_27point;
    break;
  default:
    break;
  }
  const DRT::UTILS::IntegrationPoints3D  intpoints(gaussrule);

  for (int gp=0; gp<numgpt_; ++gp)
  {
    //DRT::UTILS::shape_function<distype>(xsi_[gp],shapefct);
    //DRT::UTILS::shape_function_deriv1<distype>(xsi_[gp],deriv);

    const double e1 = intpoints.qxg[gp][0];
    const double e2 = intpoints.qxg[gp][1];
    const double e3 = intpoints.qxg[gp][2];

    DRT::UTILS::shape_function_3D       (shapefct,e1,e2,e3,distype);
    DRT::UTILS::shape_function_3D_deriv1(deriv,e1,e2,e3,distype);

    /* get the inverse of the Jacobian matrix which looks like:
     **            [ X_,r  Y_,r  Z_,r ]^-1
     **     J^-1 = [ X_,s  Y_,s  Z_,s ]
     **            [ X_,t  Y_,t  Z_,t ]
     */
    LINALG::Matrix<numdim_,numdim_> invJ;
    invJ.MultiplyNT(deriv,xrefe);

    // compute derivatives N_XYZ at gp w.r.t. material coordinates
    // by N_XYZ = J^-1 * N_rst
    N_XYZ.Multiply(invJ,deriv); // (6.21)

    // (material) deformation gradient F = d xcurr / d xrefe = xcurr * N_XYZ^T
    ComputeDefGradient(defgrd,N_XYZ,xcurr);

    //----------------------------------------------------
    // pressure at integration point
    double press = shapefct.Dot(epreaf);

    // fluid velocity at integration point
    LINALG::Matrix<numdim_,1> fvelint;
    fvelint.Multiply(evelnp,shapefct);

    LINALG::Matrix<numstr_,1> couplstress(true);

    structmat->CouplStress(defgrd,fvelint,press,couplstress);

    // return gp stresses
    switch (iostress)
    {
    case INPAR::STR::stress_2pk:
    {
      if (elestress==NULL) dserror("stress data not available");
      for (int i=0; i<numstr_; ++i)
        (*elestress)(gp,i) = couplstress(i);
    }
    break;
    case INPAR::STR::stress_cauchy:
    {
      if (elestress==NULL) dserror("stress data not available");

      // push forward of material stress to the spatial configuration
      LINALG::Matrix<numdim_,numdim_> cauchycouplstress;
      PK2toCauchy(couplstress,defgrd,cauchycouplstress);

      (*elestress)(gp,0) = cauchycouplstress(0,0);
      (*elestress)(gp,1) = cauchycouplstress(1,1);
      (*elestress)(gp,2) = cauchycouplstress(2,2);
      (*elestress)(gp,3) = cauchycouplstress(0,1);
      (*elestress)(gp,4) = cauchycouplstress(1,2);
      (*elestress)(gp,5) = cauchycouplstress(0,2);
    }
    break;
    case INPAR::STR::stress_none:
      break;

    default:
      dserror("requested stress type not available");
      break;
    }
  }

}//couplstress_poroelast


/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::InitElement()
{
  LINALG::Matrix<numdim_,numnod_> deriv ;
  LINALG::Matrix<numnod_,numdim_> xrefe;
  for (int i=0; i<numnod_; ++i)
  {
    Node** nodes=Nodes();
    if(!nodes) dserror("Nodes() returned null pointer");
    xrefe(i,0) = Nodes()[i]->X()[0];
    xrefe(i,1) = Nodes()[i]->X()[1];
    xrefe(i,2) = Nodes()[i]->X()[2];
  }

  if(distype == DRT::Element::nurbs27)
    isNurbs_=true;

  invJ_.resize(numgpt_);
  detJ_.resize(numgpt_);
  xsi_.resize(numgpt_);

  //if(not isNurbs_)
    for (int gp=0; gp<numgpt_; ++gp)
    {
      const double* gpcoord = intpoints_.Point(gp);
      for (int idim=0;idim<numdim_;idim++)
      {
         xsi_[gp](idim) = gpcoord[idim];
      }

      if(not isNurbs_)
      {
        DRT::UTILS::shape_function_deriv1<distype>(xsi_[gp],deriv);

        invJ_[gp].Multiply(deriv,xrefe);
        detJ_[gp] = invJ_[gp].Invert();
        if (detJ_[gp] <= 0.0) dserror("Element Jacobian mapping %10.5e <= 0.0",detJ_[gp]);
      }
    }

  init_=true;

  scatracoupling_=false;

  PROBLEM_TYP probtype = DRT::Problem::Instance()->ProblemType();
  if(probtype == prb_poroscatra or probtype == prb_immersed_cell)
    scatracoupling_=true;

  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::PK2toCauchy(
  LINALG::Matrix<numstr_,1>& stress,
  LINALG::Matrix<numdim_,numdim_>& defgrd,
  LINALG::Matrix<numdim_,numdim_>& cauchystress
  )
{
  // calculate the Jacobi-determinant
  const double detF = (defgrd).Determinant();

  // sigma = 1/J . F . S . F^T
  LINALG::Matrix<numdim_,numdim_> pkstress;
  pkstress(0,0) = (stress)(0);
  pkstress(0,1) = (stress)(3);
  pkstress(0,2) = (stress)(5);
  pkstress(1,0) = pkstress(0,1);
  pkstress(1,1) = (stress)(1);
  pkstress(1,2) = (stress)(4);
  pkstress(2,0) = pkstress(0,2);
  pkstress(2,1) = pkstress(1,2);
  pkstress(2,2) = (stress)(2);

  LINALG::Matrix<numdim_,numdim_> temp;
  temp.Multiply((1.0/detF),(defgrd),pkstress);
  (cauchystress).MultiplyNT(temp,(defgrd));

}  // PK2toCauchy()

/*----------------------------------------------------------------------*
 |  extrapolation of quantities at the GPs to the nodes (not called at the moment)
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::stress_expol
(
    Epetra_SerialDenseMatrix& stresses,
    Epetra_MultiVector& expolstresses
)
{
  Epetra_SerialDenseMatrix expol(numnod_,numgpt_);

  //shape function
  LINALG::Matrix<numnod_,1> shapefct;
  // coordinates of node in the fictitious GP element
  LINALG::Matrix<numdim_,1> coord;

  switch(distype)
  {
  case DRT::Element::hex8 :
  case DRT::Element::hex27 :
  {
    if(numnod_ != numgpt_)
      dserror("same number of nodes and gauss points assumed, when extrapolating stress/strain");

    // loop over all nodes
    for (int ip=0; ip<numgpt_; ++ip)
    {
      // gaussian coordinates
      const double* e = intpoints_.Point(ip);

      for(int idim=0; idim<numdim_; idim++)
      {
        if (e[idim]!=0) coord(idim) = 1/e[idim];
        else       coord(idim) = 0;
      }

      DRT::UTILS::shape_function<distype>(coord,shapefct);

      // extrapolation matrix
      for(int i=0;i<numnod_;++i)
      {
        expol(ip,i) = shapefct(i);
      }
    }
  }
  break;

  default:
    dserror("extrapolation not implemented for this element type");
    break;
  }

  Epetra_SerialDenseMatrix nodalstresses(numnod_,numstr_);
  nodalstresses.Multiply('N','N',1.0,expol,stresses,0.0);

  // distribute nodal stresses to expolstress for assembling
  for (int i=0;i<numnod_;++i)
  {
    int gid = so3_ele::NodeIds()[i];
    if (expolstresses.Map().MyGID(so3_ele::NodeIds()[i])) // rownode
    {
      int myadjele = Nodes()[i]->NumElement();
      int lid = expolstresses.Map().LID(gid);
      for (int j=0;j<numstr_;j++)
        (*(expolstresses(j)))[lid] += nodalstresses(i,j)/myadjele;
    }
  }
}


/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputePorosityAndLinearization
(   Teuchos::ParameterList&                     params,
    const double&                               press,
    const double&                               J,
    const int&                                  gp,
    const LINALG::Matrix<numnod_,1>&            shapfct,
    const LINALG::Matrix<numnod_,1>*            myporosity,
    const LINALG::Matrix<1,numdof_>&            dJ_dus,
    double &                                    porosity,
    LINALG::Matrix<1,numdof_>&                  dphi_dus)
{
  double dphi_dJ=0.0;

  structmat_->ComputePorosity( params,
                              press,
                              J,
                              gp,
                              porosity,
                              NULL,      // dphi_dp not needed
                              &dphi_dJ,
                              NULL,      //dphi_dJdp not needed
                              NULL,      //dphi_dJJ not needed
                              NULL       //dphi_dpp not needed
                              );

  dphi_dus.Update( dphi_dJ , dJ_dus );

  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputePorosityAndLinearizationOD
(   Teuchos::ParameterList&          params,
    const double&                    press,
    const double&                    J,
    const int&                       gp,
    const LINALG::Matrix<numnod_,1>&       shapfct,
    const LINALG::Matrix<numnod_,1>*       myporosity,
    double &                         porosity,
    double &                         dphi_dp)
{
  structmat_->ComputePorosity( params,
                              press,
                              J,
                              gp,
                              porosity,
                              &dphi_dp,
                              NULL,       //dphi_dJ not needed
                              NULL,       //dphi_dJdp not needed
                              NULL,       //dphi_dJJ not needed
                              NULL        //dphi_dpp not needed
                              );

  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ExtractValuesFromGlobalVector(
                                    const DRT::Discretization&         discretization, ///< discretization
                                    const int&                         dofset,         ///< number of dofset
                                    const std::vector<int>&            lm,             ///< location vetor
                                    LINALG::Matrix<numdim_,numnod_> *  matrixtofill,   ///< vector field
                                    LINALG::Matrix<numnod_,1> *        vectortofill,   ///< scalar field
                                    const std::string                  state           ///< state of the global vector
)
{
  // get state of the global vector
  Teuchos::RCP<const Epetra_Vector> matrix_state = discretization.GetState(dofset,state);
  if(matrix_state == Teuchos::null)
    dserror("Cannot get state vector %s", state.c_str());

  //ask for the number of dofs of dofset
  const int numdofpernode = discretization.NumDof(dofset,Nodes()[0]);

  // extract local values of the global vectors
  std::vector<double> mymatrix(lm.size());
  DRT::UTILS::ExtractMyValues(*matrix_state,mymatrix,lm);

  for (int inode=0; inode<numnod_; ++inode)  // number of nodes
  {
    // fill a vector field via a pointer
    if (matrixtofill != NULL)
    {
      for(int idim=0; idim<numdim_; ++idim) // number of dimensions
      {
        (*matrixtofill)(idim,inode) = mymatrix[idim+(inode*numdofpernode)];
      }  // end for(idim)
    }
    // fill a scalar field via a pointer
    if (vectortofill != NULL)
      (*vectortofill)(inode,0) = mymatrix[numdim_+(inode*numdofpernode)];
  }
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::GetMaterials( )
{

  //get structure material
  if(structmat_==Teuchos::null)
  {
    structmat_ = Teuchos::rcp_dynamic_cast<MAT::StructPoro>(Material());
    if(structmat_->MaterialType() != INPAR::MAT::m_structporo and
       structmat_->MaterialType() != INPAR::MAT::m_structpororeaction and
       structmat_->MaterialType() != INPAR::MAT::m_structpororeactionECM)
      dserror("invalid structure material for poroelasticity");
  }

  //get fluid material
  if(fluidmat_==Teuchos::null)
  {
    //access second material in structure element
    if (so3_ele::NumMaterial() > 1)
    {
      fluidmat_ = Teuchos::rcp_dynamic_cast<MAT::FluidPoro>(so3_ele::Material(1));
      if(fluidmat_->MaterialType() != INPAR::MAT::m_fluidporo)
        dserror("invalid fluid material for poroelasticity");
    }
    else
      dserror("no second material defined for element %i",Id());
  }

  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputePorosity
(   Teuchos::ParameterList& params,
    double press,
    double J,
    int gp,
    double& porosity,
    double* dphi_dp,
    double* dphi_dJ,
    double* dphi_dJdp,
    double* dphi_dJJ,
    double* dphi_dpp,
    bool save)
{
  structmat_->ComputePorosity( params,
                              press,
                              J,
                              gp,
                              porosity,
                              dphi_dp,
                              dphi_dJ,
                              dphi_dJdp,
                              dphi_dJJ,
                              dphi_dpp,
                              save
                              );
  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputeSurfPorosity( Teuchos::ParameterList& params,
                       double press,
                       double J,
                       int surfnum,
                       int gp,
                       double& porosity,
                       double* dphi_dp,
                       double* dphi_dJ,
                       double* dphi_dJdp,
                       double* dphi_dJJ,
                       double* dphi_dpp,
                       bool save)
{
  structmat_->ComputeSurfPorosity( params,
                              press,
                              J,
                              surfnum,
                              gp,
                              porosity,
                              dphi_dp,
                              dphi_dJ,
                              dphi_dJdp,
                              dphi_dJJ,
                              dphi_dpp,
                              save
                              );
  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
double DRT::ELEMENTS::So3_Poro<so3_ele,distype>::RefPorosityTimeDeriv()
{
  return structmat_->RefPorosityTimeDeriv();
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputeShapeFunctionsAndDerivatives(
    const int & gp,
    LINALG::Matrix<numnod_,1>& shapefct,
    LINALG::Matrix<numdim_,numnod_>& deriv ,
    LINALG::Matrix<numdim_,numnod_>& N_XYZ)
{
  if(!isNurbs_)
  {
    DRT::UTILS::shape_function<distype>(xsi_[gp],shapefct);
    DRT::UTILS::shape_function_deriv1<distype>(xsi_[gp],deriv);
  }
  else
  {
    DRT::NURBS::UTILS::nurbs_get_funct_deriv
    (shapefct  ,
        deriv  ,
        xsi_[gp],
        myknots_,
        weights_,
        distype );

    LINALG::Matrix<numnod_,numdim_> xrefe;
    for (int i=0; i<numnod_; ++i)
    {
      Node** nodes=Nodes();
      if(!nodes) dserror("Nodes() returned null pointer");
      xrefe(i,0) = Nodes()[i]->X()[0];
      xrefe(i,1) = Nodes()[i]->X()[1];
      xrefe(i,2) = Nodes()[i]->X()[2];
    }

    invJ_[gp].Multiply(deriv,xrefe);
    detJ_[gp] = invJ_[gp].Invert();
    if (detJ_[gp] <= 0.0) dserror("Element Jacobian mapping %10.5e <= 0.0",detJ_[gp]);
  }

  /* get the inverse of the Jacobian matrix which looks like:
   **            [ X_,r  Y_,r  Z_,r ]^-1
   **     J^-1 = [ X_,s  Y_,s  Z_,s ]
   **            [ X_,t  Y_,t  Z_,t ]
   */

  // compute derivatives N_XYZ at gp w.r.t. material coordinates
  // by N_XYZ = J^-1 * N_rst
  N_XYZ.Multiply(invJ_[gp],deriv); // (6.21)

  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
//template<class so3_ele, DRT::Element::DiscretizationType distype>
//void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputeSecondDerivativesOfShapeFunctions(
//    const int & gp,
//    const LINALG::Matrix<numdim_,numnod_>& xrefe,
//    LINALG::Matrix<numdim_,numnod_>& deriv ,
//    LINALG::Matrix<numderiv2_,numnod_>& deriv2,
//    LINALG::Matrix<numdim_,numnod_>& N_XYZ,
//    LINALG::Matrix<numderiv2_,numnod_>& N_XYZ2)
//{
//
//  if( ishigherorder_ )
//  {
//    // transposed jacobian "dX/ds"
//    LINALG::Matrix<numdim_,numdim_> xjm0;
//    xjm0.MultiplyNT(deriv,xrefe);
//
//    if(!isNurbs_)
//    {
//      // get the second derivatives of standard element at current GP w.r.t. rst
//      DRT::UTILS::shape_function_deriv2<distype>(xsi_[gp],deriv2);
//      // get the second derivatives of standard element at current GP w.r.t. XYZ
//      DRT::UTILS::gder2<distype>(xjm0,N_XYZ,deriv2,xrefe,N_XYZ2);
//    }
//    else
//    {
//      DRT::NURBS::UTILS::nurbs_get_funct_deriv_deriv2
//      (funct_  ,
//          deriv  ,
//          deriv2 ,
//          xsi_    ,
//          myknots_,
//          weights_,
//          distype );
//    }
//  }
//  else
//  {
//    deriv2.Clear();
//    N_XYZ2.Clear();
//  }
//
//  return;
//}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
double DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputeJacobianDeterminant(
                                         const int & gp,
                                         const LINALG::Matrix<numdim_,numnod_>& xcurr,
                                         const   LINALG::Matrix<numdim_,numnod_>& deriv )
{
  // get Jacobian matrix and determinant w.r.t. spatial configuration
  //! transposed jacobian "dx/ds"
  LINALG::Matrix<numdim_,numdim_> xjm;
  //! inverse of transposed jacobian "ds/dx"
  LINALG::Matrix<numdim_,numdim_> xji;
  xjm.MultiplyNT(deriv,xcurr);
  const double det = xji.Invert(xjm);

  // determinant of deformationgradient det F = det ( d x / d X ) = det (dx/ds) * ( det(dX/ds) )^-1
  return det/detJ_[gp];
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>:: ComputeAuxiliaryValues(const LINALG::Matrix<numdim_,numnod_>& N_XYZ,
    const LINALG::Matrix<numdim_,numdim_>& defgrd_inv,
    const LINALG::Matrix<numdim_,numdim_>& C_inv,
    const LINALG::Matrix<numdim_,1>&  Gradp,
    LINALG::Matrix<numdim_*numdim_,numdof_>& dFinvTdus,
    LINALG::Matrix<numdim_,1>& Finvgradp,
    LINALG::Matrix<numdim_,numdof_>& dFinvdus_gradp,
    LINALG::Matrix<numstr_,numdof_>& dCinv_dus)
{
  //F^-T * Grad p
  Finvgradp.MultiplyTN(defgrd_inv, Gradp);

  if(so3_ele::kintype_!=INPAR::STR::kinem_linear)
  {
    //dF^-T/dus
    for (int i=0; i<numdim_; i++)
      for (int n =0; n<numnod_; n++)
        for(int j=0; j<numdim_; j++)
        {
          const int gid = numdim_ * n +j;
          for (int k=0; k<numdim_; k++)
            for(int l=0; l<numdim_; l++)
              dFinvTdus(i*numdim_+l, gid) += -defgrd_inv(l,j) * N_XYZ(k,n) * defgrd_inv(k,i);
        }

    //dF^-T/dus * Grad p
    for (int i=0; i<numdim_; i++)
      for (int n =0; n<numnod_; n++)
        for(int j=0; j<numdim_; j++)
        {
          const int gid = numdim_ * n +j;
          for(int l=0; l<numdim_; l++)
            dFinvdus_gradp(i, gid) += dFinvTdus(i*numdim_+l, gid)  * Gradp(l);
        }
  }

  for (int n=0; n<numnod_; ++n)
    for (int k=0; k<numdim_; ++k)
    {
      const int gid = n*numdim_+k;
      for (int i=0; i<numdim_; ++i)
      {
        dCinv_dus(0,gid) += -2*C_inv(0,i)*N_XYZ(i,n)*defgrd_inv(0,k);
        dCinv_dus(1,gid) += -2*C_inv(1,i)*N_XYZ(i,n)*defgrd_inv(1,k);
        dCinv_dus(2,gid) += -2*C_inv(2,i)*N_XYZ(i,n)*defgrd_inv(2,k);
        /* ~~~ */
        dCinv_dus(3,gid) += -C_inv(0,i)*N_XYZ(i,n)*defgrd_inv(1,k)-defgrd_inv(0,k)*N_XYZ(i,n)*C_inv(1,i);
        dCinv_dus(4,gid) += -C_inv(1,i)*N_XYZ(i,n)*defgrd_inv(2,k)-defgrd_inv(1,k)*N_XYZ(i,n)*C_inv(2,i);
        dCinv_dus(5,gid) += -C_inv(2,i)*N_XYZ(i,n)*defgrd_inv(0,k)-defgrd_inv(2,k)*N_XYZ(i,n)*C_inv(0,i);
      }
    }
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
inline void
DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputeBOperator(
                                                        LINALG::Matrix<numstr_,numdof_>& bop,
                                                        const LINALG::Matrix<numdim_,numdim_>& defgrd,
                                                        const LINALG::Matrix<numdim_,numnod_>& N_XYZ)
{

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
  for (int i=0; i<numnod_; ++i)
  {
    bop(0,noddof_*i+0) = defgrd(0,0)*N_XYZ(0,i);
    bop(0,noddof_*i+1) = defgrd(1,0)*N_XYZ(0,i);
    bop(0,noddof_*i+2) = defgrd(2,0)*N_XYZ(0,i);
    bop(1,noddof_*i+0) = defgrd(0,1)*N_XYZ(1,i);
    bop(1,noddof_*i+1) = defgrd(1,1)*N_XYZ(1,i);
    bop(1,noddof_*i+2) = defgrd(2,1)*N_XYZ(1,i);
    bop(2,noddof_*i+0) = defgrd(0,2)*N_XYZ(2,i);
    bop(2,noddof_*i+1) = defgrd(1,2)*N_XYZ(2,i);
    bop(2,noddof_*i+2) = defgrd(2,2)*N_XYZ(2,i);
    /* ~~~ */
    bop(3,noddof_*i+0) = defgrd(0,0)*N_XYZ(1,i) + defgrd(0,1)*N_XYZ(0,i);
    bop(3,noddof_*i+1) = defgrd(1,0)*N_XYZ(1,i) + defgrd(1,1)*N_XYZ(0,i);
    bop(3,noddof_*i+2) = defgrd(2,0)*N_XYZ(1,i) + defgrd(2,1)*N_XYZ(0,i);
    bop(4,noddof_*i+0) = defgrd(0,1)*N_XYZ(2,i) + defgrd(0,2)*N_XYZ(1,i);
    bop(4,noddof_*i+1) = defgrd(1,1)*N_XYZ(2,i) + defgrd(1,2)*N_XYZ(1,i);
    bop(4,noddof_*i+2) = defgrd(2,1)*N_XYZ(2,i) + defgrd(2,2)*N_XYZ(1,i);
    bop(5,noddof_*i+0) = defgrd(0,2)*N_XYZ(0,i) + defgrd(0,0)*N_XYZ(2,i);
    bop(5,noddof_*i+1) = defgrd(1,2)*N_XYZ(0,i) + defgrd(1,0)*N_XYZ(2,i);
    bop(5,noddof_*i+2) = defgrd(2,2)*N_XYZ(0,i) + defgrd(2,0)*N_XYZ(2,i);
  }

}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
inline void
DRT::ELEMENTS::So3_Poro<so3_ele,distype>::  ComputeLinearizationOfJacobian(
    LINALG::Matrix<1,numdof_>& dJ_dus,
    const double& J,
    const LINALG::Matrix<numdim_,numnod_>& N_XYZ,
    const LINALG::Matrix<numdim_,numdim_>& defgrd_inv)
{
  //------------------------------------ build F^-1 as vector 9x1
  LINALG::Matrix<numdim_*numdim_,1> defgrd_inv_vec;
  defgrd_inv_vec(0)=defgrd_inv(0,0);
  defgrd_inv_vec(1)=defgrd_inv(0,1);
  defgrd_inv_vec(2)=defgrd_inv(0,2);
  defgrd_inv_vec(3)=defgrd_inv(1,0);
  defgrd_inv_vec(4)=defgrd_inv(1,1);
  defgrd_inv_vec(5)=defgrd_inv(1,2);
  defgrd_inv_vec(6)=defgrd_inv(2,0);
  defgrd_inv_vec(7)=defgrd_inv(2,1);
  defgrd_inv_vec(8)=defgrd_inv(2,2);

  //--------------------------- build N_X operator (wrt material config)
  LINALG::Matrix<9,numdof_> N_X(true); // set to zero
  for (int i=0; i<numnod_; ++i)
  {
    N_X(0,3*i+0) = N_XYZ(0,i);
    N_X(1,3*i+1) = N_XYZ(0,i);
    N_X(2,3*i+2) = N_XYZ(0,i);

    N_X(3,3*i+0) = N_XYZ(1,i);
    N_X(4,3*i+1) = N_XYZ(1,i);
    N_X(5,3*i+2) = N_XYZ(1,i);

    N_X(6,3*i+0) = N_XYZ(2,i);
    N_X(7,3*i+1) = N_XYZ(2,i);
    N_X(8,3*i+2) = N_XYZ(2,i);
  }

  //------linearization of jacobi determinant detF=J w.r.t. strucuture displacement   dJ/d(us) = dJ/dF : dF/dus = J * F^-T * N,X
  dJ_dus.MultiplyTN(J,defgrd_inv_vec,N_X);

}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::FillMatrixAndVectors(
    const int &                                     gp,
    const LINALG::Matrix<numnod_,1>&                shapefct,
    const LINALG::Matrix<numdim_,numnod_>&          N_XYZ,
    const double&                                   J,
    const double&                                   press,
    const double&                                   porosity,
    const LINALG::Matrix<numdim_,1>&                velint,
    const LINALG::Matrix<numdim_,1>&                fvelint,
    const LINALG::Matrix<numdim_,numdim_>&          fvelder,
    const LINALG::Matrix<numdim_,numdim_>&          defgrd_inv,
    const LINALG::Matrix<numstr_,numdof_>&          bop,
    const LINALG::Matrix<numdim_,numdim_>&          C_inv,
    const LINALG::Matrix<numdim_,1>&                Finvgradp,
    const LINALG::Matrix<1,numdof_>&                dphi_dus,
    const LINALG::Matrix<1,numdof_>&                dJ_dus,
    const LINALG::Matrix<numstr_,numdof_>&          dCinv_dus,
    const LINALG::Matrix<numdim_,numdof_>&          dFinvdus_gradp,
    const LINALG::Matrix<numdim_*numdim_,numdof_>&  dFinvTdus,
    LINALG::Matrix<numdof_,numdof_>&                erea_v,
    LINALG::Matrix<numdof_, numdof_>*               stiffmatrix,
    LINALG::Matrix<numdof_,1>*                      force,
    LINALG::Matrix<numstr_,1>&                      fstress)
{
  const double detJ_w = detJ_[gp]*intpoints_.Weight(gp);

  //if (force != NULL or stiffmatrix != NULL or reamatrix != NULL )
  {
    //const double reacoeff = fluidmat_->ComputeReactionCoeff();

    LINALG::Matrix<numdim_,numdim_> matreatensor(true);
    LINALG::Matrix<numdim_,numdim_> reatensor(true);
    LINALG::Matrix<numdim_,numdim_> linreac_dphi(true);
    LINALG::Matrix<numdim_,numdim_> linreac_dJ(true);
    LINALG::Matrix<numdim_,1> reafvel(true);
    LINALG::Matrix<numdim_,1> reavel(true);
    {
      LINALG::Matrix<numdim_,numdim_> temp(true);
      fluidmat_->ComputeReactionTensor(matreatensor,J,porosity);
      fluidmat_->ComputeLinMatReactionTensor(linreac_dphi,linreac_dJ,J,porosity);
      temp.Multiply(1.0,matreatensor,defgrd_inv);
      reatensor.MultiplyTN(defgrd_inv,temp);
      reavel.Multiply(reatensor,velint);
      reafvel.Multiply(reatensor,fvelint);
    }

    for(int idim=0; idim<numdim_; idim++)
    {
      const double reafvel_idim = reafvel(idim);
      const double reavel_idim = reavel(idim);
      const double Finvgradp_idim = Finvgradp(idim);

      for (int inode=0; inode<numnod_; inode++)
      {
        const double fac = detJ_w* shapefct(inode);
        const double v = fac * porosity * porosity* J * J;
        const int fk = numdim_*inode;

        /*-------structure- fluid velocity coupling:  RHS
         "dracy-terms"
         - reacoeff * idim^2 *  phi^2 *  v^f
         */
        (*force)(fk+idim) += -v * reafvel_idim;

        /* "reactive dracy-terms"
         reacoeff * idim^2 *  phi^2 *  v^s
         */
        (*force)(fk+idim) += v * reavel_idim;

        /*-------structure- fluid pressure coupling: RHS
         *                        "pressure gradient terms"
         - idim *  F^-T * Grad(p) * phi
         */
        (*force)(fk+idim) += fac * J * Finvgradp_idim * ( - porosity);
      }
    }

    for(int idim=0; idim<numdim_; idim++)
    {
      for (int jdim=0; jdim<numdim_; jdim++)
      {
        const double reatensor_i_j = reatensor(idim,jdim);

        for (int inode=0; inode<numnod_; inode++)
        {
          const int fk = numdim_*inode;
          const double v = detJ_w* shapefct(inode) * porosity * porosity* J * J;

          for(int jnode=0; jnode<numnod_; jnode++)
          {
            const int fi = numdim_*jnode;

            /* additional "reactive darcy-term"
             detJ * w(gp) * ( idim^2 * reacoeff * phi^2  ) * D(v_s)
             */
            erea_v(fk+idim,fi+jdim) += v * reatensor_i_j * shapefct(jnode);
          }
        }
      }
    }

    for(int idim=0; idim<numdim_; idim++)
    {
      const double Finvgradp_j = Finvgradp(idim);

      for (int jdim=0; jdim<numdim_; jdim++)
      {
        for(int jnode=0; jnode<numnod_; jnode++)
        {
          const int fi = numdim_*jnode;

          const double val =  detJ_w* (
                                      - porosity * dJ_dus(fi+jdim) * Finvgradp_j
                                      - porosity * J * dFinvdus_gradp(idim, fi+jdim)
                                      - dphi_dus(fi+jdim) * J * Finvgradp_j
                                    );

          for (int inode=0; inode<numnod_; inode++)
          {
            /* additional "pressure gradient term"
             -  detJ * w(gp) * phi *  ( dJ/d(us) * F^-T * Grad(p) - J * d(F^-T)/d(us) *Grad(p) ) * D(us)
             - detJ * w(gp) * d(phi)/d(us) * J * F^-T * Grad(p) * D(us)
             */
            (*stiffmatrix)(numdim_*inode+idim,fi+jdim) += shapefct(inode) * val;
          }
        }
      }
    }

    for(int idim=0; idim<numdim_; idim++)
    {
      const double reavel_j = reavel(idim);
      const double reafvel_j = reafvel(idim);

      for (int jdim=0; jdim<numdim_; jdim++)
      {

        for(int jnode=0; jnode<numnod_; jnode++)
        {
          const int fi = numdim_*jnode;
          const double val = detJ_w*J * porosity *  2 * ( reavel_j - reafvel_j ) *
                            ( porosity * dJ_dus(fi+jdim) + J * dphi_dus(fi+jdim) );

          for (int inode=0; inode<numnod_; inode++)
          {
            /* additional "reactive darcy-term"
               detJ * w(gp) * 2 * ( dJ/d(us) * vs * reacoeff * phi^2 + J * reacoeff * phi * d(phi)/d(us) * vs ) * D(us)
             - detJ * w(gp) *  2 * ( J * dJ/d(us) * v^f * reacoeff * phi^2 + J * reacoeff * phi * d(phi)/d(us) * v^f ) * D(us)
             */
            (*stiffmatrix)(numdim_*inode+idim,fi+jdim) += shapefct(inode) * val;
          }
        }
      }
    }

    //check if derivatives of reaction tensor are zero --> significant speed up
    if (fluidmat_->PermeabilityFunction() == MAT::PAR::const_)
    {
      const double fac = detJ_w*porosity * porosity* J * J;
      for(int idim=0; idim<numdim_; idim++)
      {
        for (int jdim=0; jdim<numdim_; jdim++)
        {
          for(int jnode=0; jnode<numnod_; jnode++)
          {
            const int fi = numdim_*jnode;

             for (int inode=0; inode<numnod_; inode++)
            {
               double val = 0.0;
               for (int p=0; p<numdim_; ++p)
               {
                 const double velint_p = velint(p);
                 const double fvelint_p = fvelint(p);
                 for (int n=0; n<numdim_; ++n)
                 {
                   const double defgrd_inv_n_p = defgrd_inv(n,p);
                   const double dFinvTdus_n_p = dFinvTdus(p*numdim_+n,fi+jdim);
                   for (int m=0; m<numdim_; ++m)
                   {
                     val += fac* ( velint_p - fvelint_p ) * (
                                       dFinvTdus(idim*numdim_+m,fi+jdim) * matreatensor(m,n) * defgrd_inv_n_p
                                     + defgrd_inv(m,idim) * matreatensor(m,n) * dFinvTdus_n_p);
                   }
                 }
               }

              (*stiffmatrix)(numdim_*inode+idim,fi+jdim) += shapefct(inode) * val;
            }
          }
        }
      }
    }//const permeability function
    else
    {
      const double fac = detJ_w*porosity * porosity* J * J;
      for(int idim=0; idim<numdim_; idim++)
      {
        for (int jdim=0; jdim<numdim_; jdim++)
        {
          for(int jnode=0; jnode<numnod_; jnode++)
          {
            const int fi = numdim_*jnode;
            const double dphi_dus_fi_l = dphi_dus(fi+jdim);
            const double dJ_dus_fi_l = dJ_dus(fi+jdim);

            for (int inode=0; inode<numnod_; inode++)
            {
              double val = 0.0;
              for (int m=0; m<numdim_; ++m)
              {
                const double dFinvTdus_idim_m_fi_jdim = dFinvTdus(idim*numdim_+m,fi+jdim);
                const double defgrd_inv_m_idim = defgrd_inv(m,idim);
                for (int n=0; n<numdim_; ++n)
                {
                  const double matreatensor_m_n = matreatensor(m,n);
                  const double linreac_dphi_m_n = linreac_dphi(m,n);
                  const double linreac_dJ_m_n = linreac_dJ(m,n);

                  for (int p=0; p<numdim_; ++p)
                  {
                    val += fac* ( velint(p) - fvelint(p) ) * (
                        dFinvTdus_idim_m_fi_jdim * matreatensor_m_n * defgrd_inv(n,p)
                      + defgrd_inv_m_idim * matreatensor_m_n * dFinvTdus(p*numdim_+n,fi+jdim)
                      + defgrd_inv_m_idim * (
                          linreac_dphi_m_n * dphi_dus_fi_l + linreac_dJ_m_n * dJ_dus_fi_l
                      ) * defgrd_inv(n,p)
                      );
                  }
                }
              }
              (*stiffmatrix)(numdim_*inode+idim,fi+jdim) += val * shapefct(inode);
            }
          }
        }
      }
    }//any other permeability function

    //inverse Right Cauchy-Green tensor as vector
    LINALG::Matrix<numstr_,1> C_inv_vec;
    for(int i =0, k=0;i<numdim_; i++)
      for(int j =0;j<numdim_-i; j++,k++)
        C_inv_vec(k)=C_inv(i+j,j);

    //B^T . C^-1
    LINALG::Matrix<numdof_,1> cinvb(true);
    cinvb.MultiplyTN(bop,C_inv_vec);

    const double fac1 = -detJ_w * press;
    const double fac2= fac1 * J;

    // additional fluid stress term -(B^T . C^-1 * J * p^f * detJ * w(gp))
    force->Update(fac2,cinvb,1.0);

    LINALG::Matrix<numdof_,numdof_> tmp1;
    LINALG::Matrix<numdof_,numdof_> tmp2;

    tmp1.Multiply(fac1,cinvb,dJ_dus);
    tmp2.MultiplyTN(fac2,bop,dCinv_dus);

    // additional fluid stress- stiffness term -(B^T . C^-1 . dJ/d(us) * p^f * detJ * w(gp))
    stiffmatrix->Update(1.0,tmp1,1.0);

    // additional fluid stress- stiffness term -(B^T .  dC^-1/d(us) * J * p^f * detJ * w(gp))
    stiffmatrix->Update(1.0,tmp2,1.0);

    // integrate `geometric' stiffness matrix and add to keu *****************
    LINALG::Matrix<numstr_,1> sfac(C_inv_vec); // auxiliary integrated stress

    //scale and add viscous stress
    sfac.Update(detJ_w,fstress,fac2); // detJ*w(gp)*[S11,S22,S33,S12=S21,S23=S32,S13=S31]

    std::vector<double> SmB_L(3); // intermediate Sm.B_L
    // kgeo += (B_L^T . sigma . B_L) * detJ * w(gp)  with B_L = Ni,Xj see NiliFEM-Skript
    for (int inod=0; inod<numnod_; ++inod)
    {
      SmB_L[0] = sfac(0) * N_XYZ(0, inod) + sfac(3) * N_XYZ(1, inod)
      + sfac(5) * N_XYZ(2, inod);
      SmB_L[1] = sfac(3) * N_XYZ(0, inod) + sfac(1) * N_XYZ(1, inod)
      + sfac(4) * N_XYZ(2, inod);
      SmB_L[2] = sfac(5) * N_XYZ(0, inod) + sfac(4) * N_XYZ(1, inod)
      + sfac(2) * N_XYZ(2, inod);
      for (int jnod=0; jnod<numnod_; ++jnod)
      {
        double bopstrbop = 0.0; // intermediate value
        for (int idim=0; idim<numdim_; ++idim)
          bopstrbop += N_XYZ(idim, jnod) * SmB_L[idim];
        (*stiffmatrix)(numdim_*inod+0,numdim_*jnod+0) += bopstrbop;
        (*stiffmatrix)(numdim_*inod+1,numdim_*jnod+1) += bopstrbop;
        (*stiffmatrix)(numdim_*inod+2,numdim_*jnod+2) += bopstrbop;
      }
    } // end of integrate `geometric' stiffness******************************
  }
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::FillMatrixAndVectorsBrinkman(
    const int &                                     gp,
    const double&                                   J,
    const double&                                   porosity,
    const LINALG::Matrix<numdim_,numdim_>&          fvelder,
    const LINALG::Matrix<numdim_,numdim_>&          defgrd_inv,
    const LINALG::Matrix<numstr_,numdof_>&          bop,
    const LINALG::Matrix<numdim_,numdim_>&          C_inv,
    const LINALG::Matrix<1,numdof_>&                dphi_dus,
    const LINALG::Matrix<1,numdof_>&                dJ_dus,
    const LINALG::Matrix<numstr_,numdof_>&          dCinv_dus,
    const LINALG::Matrix<numdim_*numdim_,numdof_>&  dFinvTdus,
    LINALG::Matrix<numdof_, numdof_>*               stiffmatrix,
    LINALG::Matrix<numdof_,1>*                      force,
    LINALG::Matrix<numstr_,1>&                      fstress)
{
  double detJ_w = detJ_[gp]*intpoints_.Weight(gp);

  double visc = fluidmat_->Viscosity();
  LINALG::Matrix<numdim_,numdim_> CinvFvel;
  LINALG::Matrix<numdim_,numdim_> visctress1;
  CinvFvel.Multiply(C_inv,fvelder);
  visctress1.MultiplyNT(CinvFvel,defgrd_inv);
  LINALG::Matrix<numdim_,numdim_> visctress2(visctress1);
  visctress1.UpdateT(1.0,visctress2,1.0);

  fstress(0) = visctress1(0,0);
  fstress(1) = visctress1(1,1);
  fstress(2) = visctress1(2,2);
  fstress(3) = visctress1(0,1);
  fstress(4) = visctress1(1,2);
  fstress(5) = visctress1(2,0);

  fstress.Scale(detJ_w * visc * J * porosity);

  //B^T . C^-1
  LINALG::Matrix<numdof_,1> fstressb(true);
  fstressb.MultiplyTN(bop,fstress);

  //if (force != NULL )
  force->Update(1.0,fstressb,1.0);

  //evaluate viscous terms (for darcy-brinkman flow only)
  //if (stiffmatrix != NULL)
  {
    static LINALG::Matrix<numdim_,numdim_> tmp;
    tmp.MultiplyNT(fvelder,defgrd_inv);

    double fac = detJ_w * visc;

    LINALG::Matrix<numstr_,numdof_> fstress_dus (true);
    {
      const double tmp_0_0 = tmp(0,0);
      const double tmp_0_1 = tmp(0,1);
      const double tmp_0_2 = tmp(0,2);
      const double tmp_1_0 = tmp(1,0);
      const double tmp_1_1 = tmp(1,1);
      const double tmp_1_2 = tmp(1,2);
      const double tmp_2_0 = tmp(2,0);
      const double tmp_2_1 = tmp(2,1);
      const double tmp_2_2 = tmp(2,2);

      const double CinvFvel_0_0 = CinvFvel(0,0);
      const double CinvFvel_0_1 = CinvFvel(0,1);
      const double CinvFvel_0_2 = CinvFvel(0,2);
      const double CinvFvel_1_0 = CinvFvel(1,0);
      const double CinvFvel_1_1 = CinvFvel(1,1);
      const double CinvFvel_1_2 = CinvFvel(1,2);
      const double CinvFvel_2_0 = CinvFvel(2,0);
      const double CinvFvel_2_1 = CinvFvel(2,1);
      const double CinvFvel_2_2 = CinvFvel(2,2);

      for (int n=0; n<numnod_; ++n)
        for (int k=0; k<numdim_; ++k)
        {
          const int gid = n*numdim_+k;

          fstress_dus(0,gid) += 2*( dCinv_dus(0,gid)*tmp_0_0 + dCinv_dus(3,gid)*tmp_1_0 + dCinv_dus(5,gid)*tmp_2_0 );
          fstress_dus(1,gid) += 2*( dCinv_dus(3,gid)*tmp_0_1 + dCinv_dus(1,gid)*tmp_1_1 + dCinv_dus(4,gid)*tmp_2_1 );
          fstress_dus(2,gid) += 2*( dCinv_dus(5,gid)*tmp_0_2 + dCinv_dus(4,gid)*tmp_1_2 + dCinv_dus(2,gid)*tmp_2_2 );
          /* ~~~ */
          fstress_dus(3,gid) += + dCinv_dus(0,gid)*tmp_0_1 + dCinv_dus(3,gid)*tmp_1_1 + dCinv_dus(5,gid)*tmp_2_1
                                + dCinv_dus(3,gid)*tmp_0_0 + dCinv_dus(1,gid)*tmp_1_0 + dCinv_dus(4,gid)*tmp_2_0;
          fstress_dus(4,gid) += + dCinv_dus(3,gid)*tmp_0_2 + dCinv_dus(1,gid)*tmp_1_2 + dCinv_dus(4,gid)*tmp_2_2
                                + dCinv_dus(5,gid)*tmp_0_1 + dCinv_dus(4,gid)*tmp_1_1 + dCinv_dus(2,gid)*tmp_2_1;
          fstress_dus(5,gid) += + dCinv_dus(5,gid)*tmp_0_0 + dCinv_dus(4,gid)*tmp_1_0 + dCinv_dus(2,gid)*tmp_2_0
                                + dCinv_dus(0,gid)*tmp_0_2 + dCinv_dus(3,gid)*tmp_1_2 + dCinv_dus(5,gid)*tmp_2_2;

          fstress_dus(0,gid) +=  2*CinvFvel_0_0 * dFinvTdus(0*numdim_  ,gid)
                                +2*CinvFvel_0_1 * dFinvTdus(1*numdim_  ,gid)
                                +2*CinvFvel_0_2 * dFinvTdus(2*numdim_  ,gid);
          fstress_dus(1,gid) +=  2*CinvFvel_1_0 * dFinvTdus(0*numdim_+1,gid)
                                +2*CinvFvel_1_1 * dFinvTdus(1*numdim_+1,gid)
                                +2*CinvFvel_1_2 * dFinvTdus(2*numdim_+1,gid);
          fstress_dus(2,gid) +=  2*CinvFvel_2_0 * dFinvTdus(0*numdim_+2,gid)
                                +2*CinvFvel_2_1 * dFinvTdus(1*numdim_+2,gid)
                                +2*CinvFvel_2_2 * dFinvTdus(2*numdim_+2,gid);
          /* ~~~ */
          fstress_dus(3,gid) += + CinvFvel_0_0 * dFinvTdus(0*numdim_+1,gid)
                                + CinvFvel_1_0 * dFinvTdus(0*numdim_  ,gid)
                                + CinvFvel_0_1 * dFinvTdus(1*numdim_+1,gid)
                                + CinvFvel_1_1 * dFinvTdus(1*numdim_  ,gid)
                                + CinvFvel_0_2 * dFinvTdus(2*numdim_+1,gid)
                                + CinvFvel_1_2 * dFinvTdus(2*numdim_  ,gid);
          fstress_dus(4,gid) += + CinvFvel_1_0 * dFinvTdus(0*numdim_+2,gid)
                                + CinvFvel_2_0 * dFinvTdus(0*numdim_+1,gid)
                                + CinvFvel_1_1 * dFinvTdus(1*numdim_+2,gid)
                                + CinvFvel_2_1 * dFinvTdus(1*numdim_+1,gid)
                                + CinvFvel_1_2 * dFinvTdus(2*numdim_+2,gid)
                                + CinvFvel_2_2 * dFinvTdus(2*numdim_+1,gid);
          fstress_dus(5,gid) += + CinvFvel_2_0 * dFinvTdus(0*numdim_  ,gid)
                                + CinvFvel_0_0 * dFinvTdus(0*numdim_+2,gid)
                                + CinvFvel_2_1 * dFinvTdus(1*numdim_  ,gid)
                                + CinvFvel_0_1 * dFinvTdus(1*numdim_+2,gid)
                                + CinvFvel_2_2 * dFinvTdus(2*numdim_  ,gid)
                                + CinvFvel_0_2 * dFinvTdus(2*numdim_+2,gid);
        }
    }

    LINALG::Matrix<numdof_,numdof_> fluidstress_part;

    // additional viscous fluid stress- stiffness term (B^T . fstress . dJ/d(us) * porosity * detJ * w(gp))
    fluidstress_part.Multiply(fac*porosity,fstressb,dJ_dus);
    stiffmatrix->Update(1.0,fluidstress_part,1.0);

    // additional fluid stress- stiffness term (B^T .  d\phi/d(us) . fstress  * J * w(gp))
    fluidstress_part.Multiply(fac*J,fstressb,dphi_dus);
    stiffmatrix->Update(1.0,fluidstress_part,1.0);

    // additional fluid stress- stiffness term (B^T .  phi . dfstress/d(us)  * J * w(gp))
    fluidstress_part.MultiplyTN(detJ_w * visc * J * porosity,bop,fstress_dus);
    stiffmatrix->Update(1.0,fluidstress_part,1.0);
  }
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::FillMatrixAndVectorsOD(
    const int &                             gp,
    const LINALG::Matrix<numnod_,1>&        shapefct,
    const LINALG::Matrix<numdim_,numnod_>&  N_XYZ,
    const double&                           J,
    const double&                           porosity,
    const double&                           dphi_dp,
    const LINALG::Matrix<numdim_,1>&        velint,
    const LINALG::Matrix<numdim_,1>&        fvelint,
    const LINALG::Matrix<numdim_,numdim_>&  defgrd_inv,
    const LINALG::Matrix<numdim_,1>&        Gradp,
    const LINALG::Matrix<numstr_,numdof_>&  bop,
    const LINALG::Matrix<numdim_,numdim_>&  C_inv,
    LINALG::Matrix<numdof_, (numdim_ + 1) * numnod_>* stiffmatrix)
{
  double detJ_w = detJ_[gp]*intpoints_.Weight(gp);

  LINALG::Matrix<numdim_,numdim_> matreatensor(true);
  LINALG::Matrix<numdim_,numdim_> reatensor(true);
  LINALG::Matrix<numdim_,numdim_> linreac_dphi(true);
  LINALG::Matrix<numdim_,numdim_> linreac_dJ(true);
  LINALG::Matrix<numdim_,1> reafvel(true);
  LINALG::Matrix<numdim_,1> reavel(true);
  {
    LINALG::Matrix<numdim_,numdim_> temp(true);
    fluidmat_->ComputeReactionTensor(matreatensor,J,porosity);
    fluidmat_->ComputeLinMatReactionTensor(linreac_dphi,linreac_dJ,J,porosity);
    temp.Multiply(1.0,matreatensor,defgrd_inv);
    reatensor.MultiplyTN(defgrd_inv,temp);
    reavel.Multiply(reatensor,velint);
    reafvel.Multiply(reatensor,fvelint);
  }

  //-----------inverse Right Cauchy-Green tensor as vector in voigt notation
  LINALG::Matrix<numstr_,1> C_inv_vec(true);
  for(int i =0, k=0;i<numdim_; i++)
    for(int j =0;j<numdim_-i; j++,k++)
      C_inv_vec(k)=C_inv(i+j,j);

  //B^T . C^-1
  LINALG::Matrix<numdof_,1> cinvb(true);
  cinvb.MultiplyTN(bop,C_inv_vec);

  //F^-T * grad p
  LINALG::Matrix<numdim_,1> Finvgradp;
  Finvgradp.MultiplyTN(defgrd_inv, Gradp);

  //F^-T * N_XYZ
  LINALG::Matrix<numdim_,numnod_> FinvNXYZ;
  FinvNXYZ.MultiplyTN(defgrd_inv, N_XYZ);

  {
    const double fac = detJ_w * J * J * 2 * porosity * dphi_dp;
    for(int idim=0; idim<numdim_; idim++)
    {
      const double reafvel_idim = reafvel(idim);
      const double reavel_idim = reavel(idim);

      for(int jnode=0; jnode<numnod_; jnode++)
      {
        const int fkp1 = (numdim_ + 1)*jnode;

        const double val = fac * shapefct(jnode) * (reavel_idim - reafvel_idim);
        for (int inode=0; inode<numnod_; inode++)
        {
          /*-------structure- fluid pressure coupling:  "dracy-terms" + "reactive darcy-terms"
           - 2 * reacoeff * J * v^f * phi * d(phi)/dp  Dp
           + 2 * reacoeff * J * v^s * phi * d(phi)/dp  Dp
           */
          (*stiffmatrix)(numdim_*inode+idim, fkp1+numdim_ ) += shapefct(inode) * val;
        }
      }
    }
  }

  {
    for(int idim=0; idim<numdim_; idim++)
    {
      const double Finvgradp_idim = Finvgradp(idim);
      for(int jnode=0; jnode<numnod_; jnode++)
      {
        const int fkp1 = (numdim_ + 1)*jnode;

        const double val1 = detJ_w * ( -1.0) * J * shapefct(jnode);
        const double val2 = -1.0 * detJ_w * J * (   Finvgradp_idim * dphi_dp * shapefct(jnode)
                                                  + porosity * FinvNXYZ(idim,jnode) );

        for (int inode=0; inode<numnod_; inode++)
        {
          /*-------structure- fluid pressure coupling: "stress terms" + "pressure gradient terms"
           -B^T . ( -1*J*C^-1 ) * Dp
           - J * F^-T * dphi/dp * Dp - J * F^-T * d(Grad((p))/(dp) * phi * Dp
           */
          (*stiffmatrix)(numdim_*inode+idim, fkp1+numdim_ ) +=  val1 * cinvb(numdim_*inode+idim)
                                                   + val2 * shapefct(inode)
                                            ;

        }
      }
    }
  }

  //check if derivatives of reaction tensor are zero --> significant speed up
  if (fluidmat_->PermeabilityFunction() != MAT::PAR::const_)
  {
    const double fac = detJ_w * J * J * porosity * porosity * dphi_dp;
    for(int idim=0; idim<numdim_; idim++)
    {
      for(int jnode=0; jnode<numnod_; jnode++)
      {
        const int fkp1 = (numdim_ + 1)*jnode;
        const double shapefct_jnode = shapefct(jnode);

        for (int inode=0; inode<numnod_; inode++)
        {
          double val=0.0;
          for (int p=0; p<numdim_; ++p)
          {
            const double velint_fvelint_p = velint(p) - fvelint(p) ;
            for (int n=0; n<numdim_; ++n)
            {
              const double defgrd_inv_n_p = defgrd_inv(n,p);
              for (int m=0; m<numdim_; ++m)
              {
                val += fac * defgrd_inv(m,idim) * linreac_dphi(m,n) * defgrd_inv_n_p * velint_fvelint_p;
              }
            }
          }
          val*=shapefct_jnode;

          /*-------structure- fluid pressure coupling:   "reactive darcy-terms"
           + J * J * phi * phi * defgrd_^-T * d(mat_reacoeff)/d(phi) * defgrd_^-1 * (v^s-v^f) * d(phi)/dp Dp
           */
          (*stiffmatrix)(numdim_*inode+idim, fkp1+numdim_ ) +=  shapefct(inode) * val;
        }
      }
    }
  }

  {
    const double fac = detJ_w * J * J * porosity * porosity;
    for(int idim=0; idim<numdim_; idim++)
    {
      for(int jdim=0; jdim<numdim_; jdim++)
      {
        const double reatensor_idim_jdim = reatensor(idim,jdim);
        for(int jnode=0; jnode<numnod_; jnode++)
        {
          const double val = -1.0 * fac * shapefct(jnode) * reatensor_idim_jdim;

          /*-------structure- fluid velocity coupling:  "darcy-terms"
           -reacoeff * J * J *  phi^2 *  Dv^f
           */
          for (int inode=0; inode<numnod_; inode++)
            (*stiffmatrix)(numdim_*inode+idim, (numdim_ + 1)*jnode+jdim) += val * shapefct(inode);
        }
      }
    }
  }
}

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::FillMatrixAndVectorsBrinkmanOD(
    const int &                             gp,
    const LINALG::Matrix<numnod_,1>&        shapefct,
    const LINALG::Matrix<numdim_,numnod_>&  N_XYZ,
    const double&                           J,
    const double&                           porosity,
    const double&                           dphi_dp,
    const LINALG::Matrix<numdim_,numdim_>&  fvelder,
    const LINALG::Matrix<numdim_,numdim_>&  defgrd_inv,
    const LINALG::Matrix<numstr_,numdof_>&  bop,
    const LINALG::Matrix<numdim_,numdim_>&  C_inv,
    LINALG::Matrix<numdof_, (numdim_ + 1) * numnod_>* stiffmatrix)
{

  double detJ_w = detJ_[gp]*intpoints_.Weight(gp);//gpweights[gp];

  LINALG::Matrix<numstr_,1> fstress;

  double visc = fluidmat_->Viscosity();
  LINALG::Matrix<numdim_,numdim_> CinvFvel;
  LINALG::Matrix<numdim_,numdim_> tmp;
  CinvFvel.Multiply(C_inv,fvelder);
  tmp.MultiplyNT(CinvFvel,defgrd_inv);
  LINALG::Matrix<numdim_,numdim_> tmp2(tmp);
  tmp.UpdateT(1.0,tmp2,1.0);

  fstress(0) = tmp(0,0);
  fstress(1) = tmp(1,1);
  fstress(2) = tmp(2,2);
  fstress(3) = tmp(0,1);
  fstress(4) = tmp(1,2);
  fstress(5) = tmp(2,0);

  //B^T . \sigma
  LINALG::Matrix<numdof_,1> fstressb;
  fstressb.MultiplyTN(bop,fstress);
  LINALG::Matrix<numdim_,numnod_> N_XYZ_Finv;
  N_XYZ_Finv.Multiply(defgrd_inv,N_XYZ);

  //dfstress/dv^f
  LINALG::Matrix<numstr_,numdof_> dfstressb_dv;
  for(int j=0; j<numdim_; j++)
  {
    const double C_inv_0_j = C_inv(0,j);
    const double C_inv_1_j = C_inv(0,j);
    const double C_inv_2_j = C_inv(0,j);

    for (int i=0; i<numnod_; i++)
    {
      const int k = numdim_*i+j;
      const double N_XYZ_Finv_0_i = N_XYZ_Finv(0,i);
      const double N_XYZ_Finv_1_i = N_XYZ_Finv(0,i);
      const double N_XYZ_Finv_2_i = N_XYZ_Finv(0,i);

      dfstressb_dv(0,k) = 2 * N_XYZ_Finv_0_i * C_inv_0_j;
      dfstressb_dv(1,k) = 2 * N_XYZ_Finv_1_i * C_inv_1_j;
      dfstressb_dv(2,k) = 2 * N_XYZ_Finv_2_i * C_inv_2_j;
      //**********************************
      dfstressb_dv(3,k) = N_XYZ_Finv_0_i * C_inv_1_j + N_XYZ_Finv_1_i * C_inv_0_j;
      dfstressb_dv(4,k) = N_XYZ_Finv_1_i * C_inv_2_j + N_XYZ_Finv_2_i * C_inv_1_j;
      dfstressb_dv(5,k) = N_XYZ_Finv_2_i * C_inv_0_j + N_XYZ_Finv_0_i * C_inv_2_j;
    }
  }

  //B^T . dfstress/dv^f
  LINALG::Matrix<numdof_,numdof_> dfstressb_dv_bop(true);
  dfstressb_dv_bop.MultiplyTN(bop,dfstressb_dv);

  for (int i=0; i<numnod_; i++)
  {
    const int fi = noddof_*i;

    for(int j=0; j<numdim_; j++)
    {
      const double fstressb_i_j = fstressb(fi+j);

      for(int k=0; k<numnod_; k++)
      {
        const int fk = noddof_*k;
        const int fkp1 = (numdim_ + 1)*k;

        /*-------structure- fluid pressure coupling: "darcy-brinkman stress terms"
         B^T . ( \mu*J - d(phi)/(dp) * fstress ) * Dp
         */
        (*stiffmatrix)(fi+j, fkp1+numdim_ ) += detJ_w * fstressb_i_j * dphi_dp * visc * J * shapefct(k);
        for(int l=0; l<noddof_; l++)
        {
          /*-------structure- fluid velocity coupling: "darcy-brinkman stress terms"
           B^T . ( \mu*J - phi * dfstress/dv^f ) * Dp
           */
          (*stiffmatrix)(fi+j, fkp1+l) += detJ_w * visc * J * porosity * dfstressb_dv_bop(fi+j, fk+l);
        }
      }
    }
  }
}

/*-----------------------------------------------------------------------------*
 * compute deformation gradient                                     vuong 03/15|
 *----------------------------------------------------------------------------*/
template<class so3_ele, DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::So3_Poro<so3_ele,distype>::ComputeDefGradient(
    LINALG::Matrix<numdim_,numdim_>&       defgrd,   ///<<    (i) deformation gradient at gausspoint
    const LINALG::Matrix<numdim_,numnod_>& N_XYZ,    ///<<    (i) derivatives of shape functions w.r.t. reference coordinates
    const LINALG::Matrix<numdim_,numnod_>& xcurr     ///<<    (i) current position of gausspoint
  )
{
  if(so3_ele::kintype_==INPAR::STR::kinem_nonlinearTotLag) //total lagrange (nonlinear)
  {
    // (material) deformation gradient F = d xcurr / d xrefe = xcurr * N_XYZ^T
    defgrd.MultiplyNT(xcurr,N_XYZ); //  (6.17)
  }
  else if(so3_ele::kintype_==INPAR::STR::kinem_linear) //linear kinematics
  {
    defgrd.Clear();
    for(int i=0;i<numdim_;i++)
      defgrd(i,i) = 1.0;
  }
  else
    dserror("invalid kinematic type!");

  return;

}  // ComputeDefGradient

/*----------------------------------------------------------------------*
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/

#include "so3_poro_fwd.hpp"
