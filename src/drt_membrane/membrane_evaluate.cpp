/*!----------------------------------------------------------------------
\file membrane_evaluate.cpp
\brief

\level 3

<pre>
\maintainer Fabian Bräu
            braeu@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>

\brief Nonlinear Membrane Finite Element evaluation

*----------------------------------------------------------------------*/
#include "membrane.H"
#include "../drt_lib/standardtypes_cpp.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_exporter.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_utils.H"
#include "../linalg/linalg_utils.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_lib/drt_timecurve.H"
#include "../drt_mat/material.H"
#include "../drt_mat/stvenantkirchhoff.H"
#include "../drt_mat/compogden.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_contact/contact_analytical.H"
#include "../linalg/linalg_fixedsizematrix.H"


/*----------------------------------------------------------------------*
 |  evaluate the element (public)                          fbraeu 06/16 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::Membrane<distype>::Evaluate(Teuchos::ParameterList&   params,
                                               DRT::Discretization&      discretization,
                                               std::vector<int>&         lm,
                                               Epetra_SerialDenseMatrix& elemat1_epetra,
                                               Epetra_SerialDenseMatrix& elemat2_epetra,
                                               Epetra_SerialDenseVector& elevec1_epetra,
                                               Epetra_SerialDenseVector& elevec2_epetra,
                                               Epetra_SerialDenseVector& elevec3_epetra)
{
  DRT::ELEMENTS::Membrane<distype>::ActionType act = Membrane<distype>::none;

  // determine size of each element matrix
  LINALG::Matrix<numdof_,numdof_> elemat1(elemat1_epetra.A(),true);
  LINALG::Matrix<numdof_,numdof_> elemat2(elemat2_epetra.A(),true);
  LINALG::Matrix<numdof_,1> elevec1(elevec1_epetra.A(),true);
  LINALG::Matrix<numdof_,1> elevec2(elevec2_epetra.A(),true);
  LINALG::Matrix<numdof_,1> elevec3(elevec3_epetra.A(),true);

  // get the action required
  std::string action = params.get<std::string>("action","none");
  if (action == "none") dserror("No action supplied");
  else if (action=="calc_struct_nlnstiff")                        act = Membrane::calc_struct_nlnstiff;
  else if (action=="calc_struct_nlnstiffmass")                    act = Membrane::calc_struct_nlnstiffmass;
  else if (action=="calc_struct_update_istep")                    act = Membrane::calc_struct_update_istep;
  else if (action=="calc_struct_reset_istep")                     act = Membrane::calc_struct_reset_istep;
  else if (action=="calc_struct_stress")                          act = Membrane::calc_struct_stress;
  else if (action=="postprocess_stress")                          act = Membrane::postprocess_stress;
  else if (action=="calc_cur_normal_at_point")                    act = Membrane::calc_cur_normal_at_point;
  else {dserror("Unknown type of action for Membrane");}

  switch(act)
  {
    /*===============================================================================*
     | calc_struct_nlnstiff                                                          |
     *===============================================================================*/
    case calc_struct_nlnstiff:
    {
      // need current displacement
      Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      if (disp==Teuchos::null) dserror("Cannot get state vector 'displacement'");
      std::vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      LINALG::Matrix<numdof_,numdof_>* matptr = NULL;
      if (elemat1.IsInitialized()) matptr = &elemat1;

      mem_nlnstiffmass(lm,mydisp,matptr,NULL,&elevec1,NULL,NULL,params,INPAR::STR::stress_none,INPAR::STR::strain_none);
    }
    break;

    /*===============================================================================*
     | calc_struct_nlnstiffmass                                                      |
     *===============================================================================*/
    case calc_struct_nlnstiffmass: // do mass, stiffness and internal forces
    {
      // need current displacement
      Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      if (disp==Teuchos::null) dserror("Cannot get state vector 'displacement'");
      std::vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
      LINALG::Matrix<numdof_,numdof_>* matptr = NULL;
      if (elemat1.IsInitialized()) matptr = &elemat1;

      mem_nlnstiffmass(lm,mydisp,matptr,&elemat2,&elevec1,NULL,NULL,params,INPAR::STR::stress_none,INPAR::STR::strain_none);
    }
    break;

    /*===============================================================================*
     | calc_struct_update_istep                                                      |
     *===============================================================================*/
    case calc_struct_update_istep:
    {
      // Update materials
      SolidMaterial()->Update();
    }
    break;

    /*===============================================================================*
     | calc_struct_reset_istep                                                       |
     *===============================================================================*/
    case calc_struct_reset_istep:
    {
      // Reset of history (if needed)
      SolidMaterial()->ResetStep();
    }
    break;

    /*===============================================================================*
     | calc_struct_stress                                                            |
     *===============================================================================*/
    case calc_struct_stress:
    {
      // nothing to do for ghost elements
      if (discretization.Comm().MyPID()==Owner())
      {
        // need current displacement
        Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
        if (disp==Teuchos::null) dserror("Cannot get state vectors 'displacement'");
        std::vector<double> mydisp(lm.size());
        DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

        Teuchos::RCP<std::vector<char> > stressdata = params.get<Teuchos::RCP<std::vector<char> > >("stress",Teuchos::null);
        Teuchos::RCP<std::vector<char> > straindata = params.get<Teuchos::RCP<std::vector<char> > >("strain",Teuchos::null);

        if (stressdata==Teuchos::null) dserror("Cannot get 'stress' data");
        if (straindata==Teuchos::null) dserror("Cannot get 'strain' data");

        LINALG::Matrix<numgpt_post_,6> stress;
        LINALG::Matrix<numgpt_post_,6> strain;

        INPAR::STR::StressType iostress = DRT::INPUT::get<INPAR::STR::StressType>(params, "iostress", INPAR::STR::stress_none);
        INPAR::STR::StrainType iostrain = DRT::INPUT::get<INPAR::STR::StrainType>(params, "iostrain", INPAR::STR::strain_none);

        // determine strains and/or stresses
        mem_nlnstiffmass(lm,mydisp,NULL,NULL,NULL,&stress,&strain,params,iostress,iostrain);

        // add data to pack
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
      }
    }
    break;

    /*===============================================================================*
     | postprocess_stress                                                            |
     *===============================================================================*/
    case postprocess_stress:
    {
      const Teuchos::RCP<std::map<int,Teuchos::RCP<Epetra_SerialDenseMatrix> > > gpstressmap=
        params.get<Teuchos::RCP<std::map<int,Teuchos::RCP<Epetra_SerialDenseMatrix> > > >("gpstressmap",Teuchos::null);
      if (gpstressmap==Teuchos::null)
        dserror("no gp stress/strain map available for postprocessing");

      std::string stresstype = params.get<std::string>("stresstype","ndxyz");

      int gid = Id();
      LINALG::Matrix<numgpt_post_,6> gpstress(((*gpstressmap)[gid])->A(),true);

      Teuchos::RCP<Epetra_MultiVector> poststress=params.get<Teuchos::RCP<Epetra_MultiVector> >("poststress",Teuchos::null);
      if (poststress==Teuchos::null)
        dserror("No element stress/strain vector available");

      if (stresstype=="ndxyz")
      {
        // extrapolation matrix, static because equal for all elements of the same discretizations type
        static LINALG::Matrix<numnod_,numgpt_post_> extrapol;

        // fill extrapolation matrix just once, equal for all elements
        static bool isfilled;

        if (isfilled==false)
        {
          // check for correct gaussrule
          if (intpoints_.nquad!=numgpt_post_)
            dserror("number of gauss points of gaussrule_ does not match numgpt_post_ used for postprocessing");

          // allocate vector for shape functions and matrix for derivatives at gp
          LINALG::Matrix<numnod_,1> shapefcts(true);

          // loop over the nodes and gauss points
          // interpolation matrix, inverted later to be the extrapolation matrix
          for (int nd=0;nd<numnod_;++nd)
          {
            // gaussian coordinates
            const double e1 = intpoints_.qxg[nd][0];
            const double e2 = intpoints_.qxg[nd][1];

            // shape functions for the extrapolated coordinates
            LINALG::Matrix<numgpt_post_,1> funct;
            DRT::UTILS::shape_function_2D(funct,e1,e2,Shape());

            for (int i=0;i<numgpt_post_;++i)
              extrapol(nd,i) = funct(i);
          }

          // fixedsizesolver for inverting extrapol
          LINALG::FixedSizeSerialDenseSolver<numnod_,numgpt_post_,1> solver;
          solver.SetMatrix(extrapol);
          int err = solver.Invert();
          if (err != 0.)
          dserror("Matrix extrapol is not invertible");

          // matrix is filled
          isfilled = true;
        }

        // extrapolate the nodal stresses for current element
        LINALG::Matrix<numnod_,6> nodalstresses;
        nodalstresses.Multiply(1.0,extrapol,gpstress,0.0);

        // "assembly" of extrapolated nodal stresses
        for (int i=0;i<numnod_;++i)
        {
          int gid = NodeIds()[i];
          if (poststress->Map().MyGID(NodeIds()[i])) // rownode
          {
            int lid = poststress->Map().LID(gid);
            int myadjele = Nodes()[i]->NumElement();
            for (int j=0;j<6;j++)
              (*((*poststress)(j)))[lid] += nodalstresses(i,j)/myadjele;
          }
        }
      }
      else if (stresstype=="cxyz")
      {
        // averaging of stresses/strains from gauss points to element
        const Epetra_BlockMap& elemap = poststress->Map();
        int lid = elemap.LID(Id());
        if (lid!=-1)
        {
          for (int i = 0; i < 6; ++i)
          {
            double& s = (*((*poststress)(i)))[lid]; // resolve pointer for faster access
            s = 0.;
            for (int j = 0; j < numgpt_post_; ++j)
            {
              s += gpstress(j,i);
            }
            s *= 1.0/numgpt_post_;
          }
        }
      }
      else
      {
        dserror("unknown type of stress/strain output on element level");
      }
    }
    break;

    /*===============================================================================*
     | calc_cur_normal_at_point (vector not normalized)                              |
     *===============================================================================*/
    case calc_cur_normal_at_point:
    {
      // need current displacement
      Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState("displacement");
      if (disp==Teuchos::null) dserror("Cannot get state vector 'displacement'");
      std::vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

      // get reference configuration and determine current configuration
      LINALG::Matrix<numnod_,noddof_> xrefe(true);
      LINALG::Matrix<numnod_,noddof_> xcurr(true);

      mem_configuration(mydisp,xrefe,xcurr);

      // get target position in parameter space
      const double target_xi = elevec2_epetra[0];
      const double target_eta = elevec2_epetra[1];

      // allocate vector for shape functions and matrix for derivatives at target point
      LINALG::Matrix<numnod_,1> shapefcts(true);
      LINALG::Matrix<numdim_, numnod_> derivs(true);

      // get shape functions and derivatives in the plane of the element
      DRT::UTILS::shape_function_2D(shapefcts,target_xi,target_eta,Shape());
      DRT::UTILS::shape_function_2D_deriv1(derivs,target_xi,target_eta,Shape());

      /*===============================================================================*
       | orthonormal base (t1,t2,tn) in the undeformed configuration at current GP     |
       *===============================================================================*/

      LINALG::Matrix<numdim_,numnod_> derivs_ortho(true);
      double G1G2_cn;
      LINALG::Matrix<noddof_,1> dXds1(true);
      LINALG::Matrix<noddof_,1> dXds2(true);
      LINALG::Matrix<noddof_,1> dxds1(true);
      LINALG::Matrix<noddof_,1> dxds2(true);
      LINALG::Matrix<noddof_,noddof_> Q_trafo(true);

      mem_orthonormalbase(xrefe,xcurr,derivs,derivs_ortho,G1G2_cn,dXds1,dXds2,dxds1,dxds2,Q_trafo);

      // determine normal vector, not normalized!
      // determine cross product x,1 x x,2
      LINALG::Matrix<noddof_,1> xcurr_cross(true);
      xcurr_cross(0) = dxds1(1)*dxds2(2)-dxds1(2)*dxds2(1);
      xcurr_cross(1) = dxds1(2)*dxds2(0)-dxds1(0)*dxds2(2);
      xcurr_cross(2) = dxds1(0)*dxds2(1)-dxds1(1)*dxds2(0);

      xcurr_cross.Scale(-1.0);  //FUCHS

      // give back normal vector
      // ATTENTION: vector not normalized
      elevec1_epetra[0] = xcurr_cross(0);
      elevec1_epetra[1] = xcurr_cross(1);
      elevec1_epetra[2] = xcurr_cross(2);
    }
    break;

    /*===============================================================================*
     | default                                                                       |
     *===============================================================================*/
    default:
      dserror("Unknown type of action for Membrane");
    break;
  }

  return 0;
}


/*-----------------------------------------------------------------------*
 |  Integrate a Surface Neumann boundary condition (public) fbraeu 06/16 |
 *-----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::Membrane<distype>::EvaluateNeumann(Teuchos::ParameterList&   params,
                                                      DRT::Discretization&      discretization,
                                                      DRT::Condition&           condition,
                                                      std::vector<int>&         lm,
                                                      Epetra_SerialDenseVector& elevec1_epetra,
                                                      Epetra_SerialDenseMatrix* elemat1_epetra)
{
  // get values and switches from the condition
  const std::vector<int>*    onoff = condition.Get<std::vector<int> >   ("onoff");
  const std::vector<double>* val   = condition.Get<std::vector<double> >("val");

  // find out whether we will use a time curve
  bool usetime = true;
  const double time = params.get("total time",-1.0);
  if (time<0.0) usetime = false;

  // ensure that at least as many curves/functs as dofs are available
  if (int(onoff->size()) < noddof_)
    dserror("Fewer functions or curves defined than the element has dofs.");

  // check membrane pressure input
  for (int checkdof = 1; checkdof < int(onoff->size()); ++checkdof)
    if ((*onoff)[checkdof] != 0) dserror("membrane pressure on 1st dof only!");

  // find out whether we will use time curves and get the factors
  const std::vector<int>* curve  = condition.Get<std::vector<int> >("curve");
  std::vector<double> curvefacs(noddof_, 1.0);
  for (int i=0; i < noddof_; ++i)
  {
    const int curvenum = (curve) ? (*curve)[i] : -1;
    if (curvenum>=0 && usetime)
      curvefacs[i] = DRT::Problem::Instance()->Curve(curvenum).f(time);
  }

  // determine current pressure
  double pressure;
  if ((*onoff)[0]) pressure = (*val)[0]*curvefacs[0];
  else pressure = 0.0;

  // need displacement new
  Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState("displacement new");
  if (disp==Teuchos::null) dserror("Cannot get state vector 'displacement new'");
  std::vector<double> mydisp(lm.size());
  DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

  // get reference configuration and determine current configuration
  LINALG::Matrix<numnod_,noddof_> xrefe(true);
  LINALG::Matrix<numnod_,noddof_> xcurr(true);

  mem_configuration(mydisp,xrefe,xcurr);

  /*===============================================================================*
   | loop over the gauss points                                                    |
   *===============================================================================*/

  // allocate vector for shape functions and matrix for derivatives at gp
  LINALG::Matrix<numnod_,1> shapefcts(true);
  LINALG::Matrix<numdim_, numnod_> derivs(true);

  for (int gp=0; gp<intpoints_.nquad; ++gp)
  {
    // get gauss points from integration rule
    double xi_gp = intpoints_.qxg[gp][0];
    double eta_gp = intpoints_.qxg[gp][1];

    // get gauss weight at current gp
    double gpweight = intpoints_.qwgt[gp];

    // get shape functions and derivatives in the plane of the element
    DRT::UTILS::shape_function_2D(shapefcts,xi_gp,eta_gp,Shape());
    DRT::UTILS::shape_function_2D_deriv1(derivs,xi_gp,eta_gp,Shape());

    /*===============================================================================*
     | orthonormal base (t1,t2,tn) in the undeformed configuration at current GP     |
     *===============================================================================*/

    LINALG::Matrix<numdim_,numnod_> derivs_ortho(true);
    double G1G2_cn;
    LINALG::Matrix<noddof_,1> dXds1(true);
    LINALG::Matrix<noddof_,1> dXds2(true);
    LINALG::Matrix<noddof_,1> dxds1(true);
    LINALG::Matrix<noddof_,1> dxds2(true);
    LINALG::Matrix<noddof_,noddof_> Q_trafo(true);

    mem_orthonormalbase(xrefe,xcurr,derivs,derivs_ortho,G1G2_cn,dXds1,dXds2,dxds1,dxds2,Q_trafo);

    // determine cross product x,1 x x,2
    LINALG::Matrix<noddof_,1> xcurr_cross(true);
    xcurr_cross(0) = dxds1(1)*dxds2(2)-dxds1(2)*dxds2(1);
    xcurr_cross(1) = dxds1(2)*dxds2(0)-dxds1(0)*dxds2(2);
    xcurr_cross(2) = dxds1(0)*dxds2(1)-dxds1(1)*dxds2(0);

    // determine cross product X,1 x X,2
    LINALG::Matrix<noddof_,1> xrefe_cross(true);
    xrefe_cross(0) = dXds1(1)*dXds2(2)-dXds1(2)*dXds2(1);
    xrefe_cross(1) = dXds1(2)*dXds2(0)-dXds1(0)*dXds2(2);
    xrefe_cross(2) = dXds1(0)*dXds2(1)-dXds1(1)*dXds2(0);

    // euclidian norm of xref_cross
    double xrefe_cn = xrefe_cross.Norm2();

    // integration factor
    double fac = (pressure * G1G2_cn * gpweight) / xrefe_cn;

    // loop over all 4 nodes
    for (int i=0; i<numnod_; ++i)
    {
      // assemble external force vector
      elevec1_epetra[noddof_*i+0] += fac * xcurr_cross(0) * (shapefcts)(i);
      elevec1_epetra[noddof_*i+1] += fac * xcurr_cross(1) * (shapefcts)(i);
      elevec1_epetra[noddof_*i+2] += fac * xcurr_cross(2) * (shapefcts)(i);

      // evaluate external stiffness matrix if needed
      if (elemat1_epetra != NULL)
      {
        // determine P matrix for all 4 nodes, Gruttmann92 equation (41) and directly fill up elemat1_epetra
        for (int j=0; j<numnod_; ++j)
        {
          double p1_ij = (dxds1(0)*derivs_ortho(1,i)-dxds2(0)*derivs_ortho(0,i))*(shapefcts)(j);
          double p2_ij = (dxds1(1)*derivs_ortho(1,i)-dxds2(1)*derivs_ortho(0,i))*(shapefcts)(j);
          double p3_ij = (dxds1(2)*derivs_ortho(1,i)-dxds2(2)*derivs_ortho(0,i))*(shapefcts)(j);

          // entries of P matrix are in round brackets
          (*elemat1_epetra)(noddof_*i+0,noddof_*j+1) += fac * -p3_ij;
          (*elemat1_epetra)(noddof_*i+0,noddof_*j+2) += fac * +p2_ij;
          (*elemat1_epetra)(noddof_*i+1,noddof_*j+0) += fac * +p3_ij;
          (*elemat1_epetra)(noddof_*i+1,noddof_*j+2) += fac * -p1_ij;
          (*elemat1_epetra)(noddof_*i+2,noddof_*j+0) += fac * -p2_ij;
          (*elemat1_epetra)(noddof_*i+2,noddof_*j+1) += fac * +p1_ij;
        }
      }
    }
  }
  return 0;
}

/*----------------------------------------------------------------------*
 |  evaluate the element (private)                         fbraeu 06/16 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_nlnstiffmass(
    std::vector<int>&                      lm,                    // location matrix
    std::vector<double>&                   disp,                  // current displacements
    LINALG::Matrix<numdof_,numdof_>*       stiffmatrix,           // element stiffness matrix
    LINALG::Matrix<numdof_,numdof_>*       massmatrix,            // element mass matrix
    LINALG::Matrix<numdof_,1>*             force,                 // element internal force vector
    LINALG::Matrix<numgpt_post_,6>*             elestress,             // stresses at GP
    LINALG::Matrix<numgpt_post_,6>*             elestrain,             // strains at GP
    Teuchos::ParameterList&                params,                // algorithmic parameters e.g. time
    const INPAR::STR::StressType           iostress,              // stress output option
    const INPAR::STR::StrainType           iostrain)              // strain output option
{
  // get reference configuration and determine current configuration
  LINALG::Matrix<numnod_,noddof_> xrefe(true);
  LINALG::Matrix<numnod_,noddof_> xcurr(true);

  mem_configuration(disp,xrefe,xcurr);

  /*===============================================================================*
   | loop over the gauss points                                                    |
   *===============================================================================*/

  // allocate vector for shape functions and matrix for derivatives at gp
  LINALG::Matrix<numnod_,1> shapefcts(true);
  LINALG::Matrix<numdim_, numnod_> derivs(true);

  for (int gp=0; gp<intpoints_.nquad; ++gp)
  {
    // set current gauss point
    params.set<int>("gp",gp);

    // get gauss points from integration rule
    double xi_gp = intpoints_.qxg[gp][0];
    double eta_gp = intpoints_.qxg[gp][1];

    // get gauss weight at current gp
    double gpweight = intpoints_.qwgt[gp];

    // get shape functions and derivatives in the plane of the element
    DRT::UTILS::shape_function_2D(shapefcts,xi_gp,eta_gp,Shape());
    DRT::UTILS::shape_function_2D_deriv1(derivs,xi_gp,eta_gp,Shape());

    /*===============================================================================*
     | orthonormal base (t1,t2,tn) in the undeformed configuration at current GP     |
     *===============================================================================*/

    LINALG::Matrix<numdim_,numnod_> derivs_ortho(true);
    double G1G2_cn;
    LINALG::Matrix<noddof_,1> dXds1(true);
    LINALG::Matrix<noddof_,1> dXds2(true);
    LINALG::Matrix<noddof_,1> dxds1(true);
    LINALG::Matrix<noddof_,1> dxds2(true);
    LINALG::Matrix<noddof_,noddof_> Q_trafo(true);

    mem_orthonormalbase(xrefe,xcurr,derivs,derivs_ortho,G1G2_cn,dXds1,dXds2,dxds1,dxds2,Q_trafo);

    /*===============================================================================*
     | surface deformation gradient                                                  |
     *===============================================================================*/

    // surface deformation gradient in 3 dimensions in global coordinates
    LINALG::Matrix<noddof_,noddof_> defgrd_global(true);

    // surface deformation gradient in 3 dimensions in local coordinates
    LINALG::Matrix<noddof_,noddof_> defgrd_local(true);

    // principle stretch in thickness direction
    double lambda3 = 1.0;

    // Remark:
    // incompressibility condition to get principle stretch in thickness direction
    // can be considered as an initialization of the Newton-Raphson procedure in mem_Material3dPlane(...)
    // where the full stress state is reduced to a plane stress by varying the entries of the Green-Lagrange strain tensor
    lambda3 = std::sqrt(1.0/(dxds1.Dot(dxds1)*dxds2.Dot(dxds2)-std::pow(dxds1.Dot(dxds2),2.0)));

    // surface deformation gradient in 3 dimensions in global coordinates
    mem_defgrd_global(dXds1,dXds2,dxds1,dxds2,lambda3,defgrd_global);

    // surface deformation gradient in 3 dimensions in local coordinates
    mem_globaltolocal(Q_trafo,defgrd_global,defgrd_local);

    /*===============================================================================*
     | right cauchygreen tensor in local coordinates                                 |
     *===============================================================================*/

    // calculate three dimensional right cauchy-green strain tensor in orthonormal base
    LINALG::Matrix<noddof_,noddof_> cauchygreen_local(true);
    cauchygreen_local.MultiplyTN(1.0,defgrd_local,defgrd_local,0.0);

    /*===============================================================================*
     | call material law                                                             |
     *===============================================================================*/

    // 2nd piola kirchhoff stress vector under plane stress assumption
    LINALG::Matrix<3,1> pkstress(true);

    // material tangent matrix for plane stress
    LINALG::Matrix<3,3> cmat(true);

    /*===============================================================================*
     | standard evaluation                                                           |
     *===============================================================================*/
    // call 3 dimensional material law and reduce to a plane stress state
    // no incompressibility fulfilled here (use \nue close to 0.5 or volumetric strain energy function)
    mem_Material3dPlane(dXds1,dXds2,dxds1,dxds2,defgrd_global,cauchygreen_local,pkstress,cmat,Q_trafo,params);

    // update principle stretch in thickness direction as cauchygreen_local(2,2) changes in the material evaluation
    lambda3 = std::sqrt(cauchygreen_local(2,2));

    // update surface deformation gradient in 3 dimensions in global coordinates
    mem_defgrd_global(dXds1,dXds2,dxds1,dxds2,lambda3,defgrd_global);

    // update surface deformation gradient in 3 dimensions in local coordinates
    mem_globaltolocal(Q_trafo,defgrd_global,defgrd_local);

    /*===============================================================================*
     | update current thickness at gp                                                |
     *===============================================================================*/
    curr_thickness_[gp] = lambda3*thickness_;

    /*===============================================================================*
     | calculate force, stiffness matrix and mass matrix                             |
     *===============================================================================*/

    // evaluate stiffness matrix and force vector if needed
    if (stiffmatrix != NULL && force != NULL)
    {
      // determine B matrix and G matrix for all 4 nodes, Gruttmann1992 equation (36) and (40)
      LINALG::Matrix<noddof_,numdof_> B_matrix(true);
      LINALG::Matrix<numdof_,numdof_> G_matrix(true);
      double g_ij;

      for (int i=0; i<numnod_; ++i)
      {
        B_matrix(0,noddof_*i+0) = derivs_ortho(0,i)*dxds1(0);
        B_matrix(1,noddof_*i+0) = derivs_ortho(1,i)*dxds2(0);
        B_matrix(2,noddof_*i+0) = derivs_ortho(0,i)*dxds2(0)+derivs_ortho(1,i)*dxds1(0);

        B_matrix(0,noddof_*i+1) = derivs_ortho(0,i)*dxds1(1);
        B_matrix(1,noddof_*i+1) = derivs_ortho(1,i)*dxds2(1);
        B_matrix(2,noddof_*i+1) = derivs_ortho(0,i)*dxds2(1)+derivs_ortho(1,i)*dxds1(1);

        B_matrix(0,noddof_*i+2) = derivs_ortho(0,i)*dxds1(2);
        B_matrix(1,noddof_*i+2) = derivs_ortho(1,i)*dxds2(2);
        B_matrix(2,noddof_*i+2) = derivs_ortho(0,i)*dxds2(2)+derivs_ortho(1,i)*dxds1(2);

        for (int j=0; j<numnod_; ++j)
        {
          g_ij = pkstress(0)*derivs_ortho(0,i)*derivs_ortho(0,j) + pkstress(1)*derivs_ortho(1,i)*derivs_ortho(1,j) + pkstress(2)*(derivs_ortho(0,i)*derivs_ortho(1,j)+derivs_ortho(1,i)*derivs_ortho(0,j));
          G_matrix(noddof_*i+0,noddof_*j+0) = g_ij;
          G_matrix(noddof_*i+1,noddof_*j+1) = g_ij;
          G_matrix(noddof_*i+2,noddof_*j+2) = g_ij;
        }
      }

      double fac = gpweight*thickness_*G1G2_cn;

      // determine force and stiffness matrix, Gruttmann1992 equation (37) and (39)
      force->MultiplyTN(fac,B_matrix,pkstress,1.0);

      LINALG::Matrix<numdof_,noddof_> temp(true);
      temp.MultiplyTN(1.0,B_matrix,cmat,0.0);
      LINALG::Matrix<numdof_,numdof_> temp2(true);
      temp2.Multiply(1.0,temp,B_matrix,0.0);
      temp2.Update(1.0,G_matrix,1.0);

      stiffmatrix->Update(fac,temp2,1.0);
    }

    // evaluate massmatrix if needed, just valid for a constant density
    if (massmatrix != NULL)
    {
      // get density
      double density = SolidMaterial()->Density();

      // integrate consistent mass matrix
      const double factor = gpweight*thickness_*G1G2_cn * density;
      double ifactor = 0.0;
      double massfactor = 0.0;

      for (int i=0; i<numnod_; ++i)
      {
        ifactor = shapefcts(i) * factor;

        for (int j=0; j<numnod_; ++j)
        {
          massfactor = shapefcts(j) * ifactor;     // intermediate factor

          (*massmatrix)(noddof_*i+0,noddof_*j+0) += massfactor;
          (*massmatrix)(noddof_*i+1,noddof_*j+1) += massfactor;
          (*massmatrix)(noddof_*i+2,noddof_*j+2) += massfactor;
        }
      }

      //check for non constant mass matrix
      if (SolidMaterial()->VaryingDensity())
      {
        dserror("Varying Density not supported for Membrane");
      }
    }

    /*===============================================================================*
     | return gp strains (only in case of stress/strain output)                      |
     *===============================================================================*/
    switch (iostrain)
    {
    // Green-Lagrange strains
    case INPAR::STR::strain_gl:
    {
      if (elestrain == NULL) dserror("strain data not available");

      // transform local cauchygreen to global coordinates
      LINALG::Matrix<noddof_,noddof_> cauchygreen_global(true);
      mem_localtoglobal(Q_trafo,cauchygreen_local,cauchygreen_global);

      // green-lagrange strain tensor in global coordinates
      LINALG::Matrix<noddof_,noddof_> glstrain_global(true);
      glstrain_global(0,0) = 0.5*(cauchygreen_global(0,0)-1.0);
      glstrain_global(1,1) = 0.5*(cauchygreen_global(1,1)-1.0);
      glstrain_global(2,2) = 0.5*(cauchygreen_global(2,2)-1.0);
      glstrain_global(0,1) = 0.5*cauchygreen_global(0,1);
      glstrain_global(0,2) = 0.5*cauchygreen_global(0,2);
      glstrain_global(1,2) = 0.5*cauchygreen_global(1,2);
      glstrain_global(1,0) = glstrain_global(0,1);
      glstrain_global(2,0) = glstrain_global(0,2);
      glstrain_global(2,1) = glstrain_global(1,2);

      (*elestrain)(gp,0) = glstrain_global(0,0);
      (*elestrain)(gp,1) = glstrain_global(1,1);
      (*elestrain)(gp,2) = glstrain_global(2,2);
      (*elestrain)(gp,3) = glstrain_global(0,1);
      (*elestrain)(gp,4) = glstrain_global(1,2);
      (*elestrain)(gp,5) = glstrain_global(0,2);
    }
    break;
    // Euler-Almansi strains
    case INPAR::STR::strain_ea:
    {
      if (elestrain == NULL) dserror("strain data not available");

      // transform local cauchygreen to global coordinates
      LINALG::Matrix<noddof_,noddof_> cauchygreen_global(true);
      mem_localtoglobal(Q_trafo,cauchygreen_local,cauchygreen_global);

      // green-lagrange strain tensor in global coordinates
      LINALG::Matrix<noddof_,noddof_> glstrain_global(true);
      glstrain_global(0,0) = 0.5*(cauchygreen_global(0,0)-1);
      glstrain_global(1,1) = 0.5*(cauchygreen_global(1,1)-1);
      glstrain_global(2,2) = 0.5*(cauchygreen_global(2,2)-1);
      glstrain_global(0,1) = 0.5*cauchygreen_global(0,1);
      glstrain_global(0,2) = 0.5*cauchygreen_global(0,2);
      glstrain_global(1,2) = 0.5*cauchygreen_global(1,2);
      glstrain_global(1,0) = glstrain_global(0,1);
      glstrain_global(2,0) = glstrain_global(0,2);
      glstrain_global(2,1) = glstrain_global(1,2);

      // pushforward of gl strains to ea strains
      LINALG::Matrix<noddof_,noddof_> euler_almansi(true);
      mem_GLtoEA(glstrain_global,defgrd_global,euler_almansi);

      (*elestrain)(gp,0) = euler_almansi(0,0);
      (*elestrain)(gp,1) = euler_almansi(1,1);
      (*elestrain)(gp,2) = euler_almansi(2,2);
      (*elestrain)(gp,3) = euler_almansi(0,1);
      (*elestrain)(gp,4) = euler_almansi(1,2);
      (*elestrain)(gp,5) = euler_almansi(0,2);
    }
    break;
    // Logarithmic strains
    case INPAR::STR::strain_log:
    {
      if (elestrain == NULL) dserror("strain data not available");

      // the Eularian logarithmic strain is defined as the natural logarithm of the left stretch tensor [1,2]:
      // e_{log} = e_{hencky} = ln (\mathbf{V}) = \sum_{i=1}^3 (ln \lambda_i) \mathbf{n}_i \otimes \mathbf{n}_i
      // References:
      // [1] H. Xiao, Beijing, China, O. T. Bruhns and A. Meyers (1997) Logarithmic strain, logarithmic spin and logarithmic rate, Eq. 5
      // [2] Caminero et al. (2011) Modeling large strain anisotropic elasto-plasticity with logarithmic strain and stress measures, Eq. 70

      // transform local cauchygreen to global coordinates
      LINALG::Matrix<noddof_,noddof_> cauchygreen_global(true);
      mem_localtoglobal(Q_trafo,cauchygreen_local,cauchygreen_global);

      // eigenvalue decomposition (from elasthyper.cpp)
      LINALG::Matrix<3,3> prstr2(true);  // squared principal stretches
      LINALG::Matrix<3,1> prstr(true);   // principal stretch
      LINALG::Matrix<3,3> prdir(true);   // principal directions
      LINALG::SYEV(cauchygreen_global,prstr2,prdir);

      // THE principal stretches
      for (int al=0; al<3; ++al) prstr(al) = std::sqrt(prstr2(al,al));

      // populating the logarithmic strain matrix
      LINALG::Matrix<noddof_,noddof_> lnv(true);

      // checking if cauchy green is correctly determined to ensure eigenvectors in correct direction
      // i.e. a flipped eigenvector is also a valid solution
      // C = \sum_{i=1}^3 (\lambda_i^2) \mathbf{n}_i \otimes \mathbf{n}_i
      LINALG::Matrix<noddof_,noddof_> tempCG(true);

      for (int k=0; k < 3; ++k)
      {
        double n_00, n_01, n_02, n_11, n_12, n_22 = 0.0;

        n_00 = prdir(0,k)*prdir(0,k);
        n_01 = prdir(0,k)*prdir(1,k);
        n_02 = prdir(0,k)*prdir(2,k);
        n_11 = prdir(1,k)*prdir(1,k);
        n_12 = prdir(1,k)*prdir(2,k);
        n_22 = prdir(2,k)*prdir(2,k);

        // only compute the symmetric components from a single eigenvector,
        // because eigenvalue directions are not consistent (it can be flipped)
        tempCG(0,0) += (prstr(k))*(prstr(k))*n_00;
        tempCG(0,1) += (prstr(k))*(prstr(k))*n_01;
        tempCG(0,2) += (prstr(k))*(prstr(k))*n_02;
        tempCG(1,0) += (prstr(k))*(prstr(k))*n_01; // symmetry
        tempCG(1,1) += (prstr(k))*(prstr(k))*n_11;
        tempCG(1,2) += (prstr(k))*(prstr(k))*n_12;
        tempCG(2,0) += (prstr(k))*(prstr(k))*n_02; // symmetry
        tempCG(2,1) += (prstr(k))*(prstr(k))*n_12; // symmetry
        tempCG(2,2) += (prstr(k))*(prstr(k))*n_22;

        // Computation of the Logarithmic strain tensor

        lnv(0,0) += (std::log(prstr(k)))*n_00;
        lnv(0,1) += (std::log(prstr(k)))*n_01;
        lnv(0,2) += (std::log(prstr(k)))*n_02;
        lnv(1,0) += (std::log(prstr(k)))*n_01; // symmetry
        lnv(1,1) += (std::log(prstr(k)))*n_11;
        lnv(1,2) += (std::log(prstr(k)))*n_12;
        lnv(2,0) += (std::log(prstr(k)))*n_02; // symmetry
        lnv(2,1) += (std::log(prstr(k)))*n_12; // symmetry
        lnv(2,2) += (std::log(prstr(k)))*n_22;
      }

      // compare CG computed with deformation gradient with CG computed
      // with eigenvalues and -vectors to determine/ensure the correct
      // orientation of the eigen vectors
      LINALG::Matrix<noddof_,noddof_> diffCG(true);

      for (int i=0; i < 3; ++i)
        {
          for (int j=0; j < 3; ++j)
            {
              diffCG(i,j) = cauchygreen_global(i,j)-tempCG(i,j);
              // the solution to this problem is to evaluate the cauchygreen tensor with
              // tempCG computed with every combination of eigenvector orientations -- up to nine comparisons
              if (diffCG(i,j) > 1e-10) dserror("eigenvector orientation error with the diffCG giving problems: %10.5e \n BUILD SOLUTION TO FIX IT",diffCG(i,j));
            }
        }

      (*elestrain)(gp,0) = lnv(0,0);
      (*elestrain)(gp,1) = lnv(1,1);
      (*elestrain)(gp,2) = lnv(2,2);
      (*elestrain)(gp,3) = lnv(0,1);
      (*elestrain)(gp,4) = lnv(1,2);
      (*elestrain)(gp,5) = lnv(0,2);
    }
    break;
    // no strain output
    case INPAR::STR::strain_none:
      break;
    default:
      dserror("requested strain type not available");
      break;
    }

    /*===============================================================================*
     | return gp stresses (only in case of stress/strain output)                     |
     *===============================================================================*/
    switch (iostress)
    {
    // 2nd Piola-Kirchhoff stresses
    case INPAR::STR::stress_2pk:
    {
      if (elestress == NULL) dserror("stress data not available");

      // 2nd Piola-Kirchhoff stress in tensor notation, plane stress meaning entries in 2i and i2 are zero for i=0,1,2
      LINALG::Matrix<noddof_,noddof_> pkstress_local(true);
      pkstress_local(0,0) = pkstress(0);
      pkstress_local(1,1) = pkstress(1);
      pkstress_local(0,1) = pkstress(2);
      pkstress_local(1,0) = pkstress(2);

      // determine 2nd Piola-Kirchhoff stresses in global coordinates
      LINALG::Matrix<noddof_,noddof_> pkstress_global(true);
      mem_localtoglobal(Q_trafo,pkstress_local,pkstress_global);

      (*elestress)(gp,0) = pkstress_global(0,0);
      (*elestress)(gp,1) = pkstress_global(1,1);
      (*elestress)(gp,2) = pkstress_global(2,2);
      (*elestress)(gp,3) = pkstress_global(0,1);
      (*elestress)(gp,4) = pkstress_global(1,2);
      (*elestress)(gp,5) = pkstress_global(0,2);
    }
    break;
    // Cauchy stresses
    case INPAR::STR::stress_cauchy:
    {
      if (elestress == NULL) dserror("stress data not available");

      // 2nd Piola-Kirchhoff stress in tensor notation, plane stress meaning entries in 2i and i2 are zero for i=0,1,2
      LINALG::Matrix<noddof_,noddof_> pkstress_local(true);
      pkstress_local(0,0) = pkstress(0);
      pkstress_local(1,1) = pkstress(1);
      pkstress_local(0,1) = pkstress(2);
      pkstress_local(1,0) = pkstress(2);

      // determine 2nd Piola-Kirchhoff stresses in global coordinates
      LINALG::Matrix<noddof_,noddof_> pkstress_global(true);
      mem_localtoglobal(Q_trafo,pkstress_local,pkstress_global);

      LINALG::Matrix<3,3> cauchy(true);
      mem_PK2toCauchy(pkstress_global,defgrd_global,cauchy);

      (*elestress)(gp,0) = cauchy(0,0);
      (*elestress)(gp,1) = cauchy(1,1);
      (*elestress)(gp,2) = cauchy(2,2);
      (*elestress)(gp,3) = cauchy(0,1);
      (*elestress)(gp,4) = cauchy(1,2);
      (*elestress)(gp,5) = cauchy(0,2);
    }
    break;
    // no stress output
    case INPAR::STR::stress_none:
      break;
    default:
      dserror("requested stress type not available");
      break;
    }

  }
  return;

} // DRT::ELEMENTS::Membrane::membrane_nlnstiffmass

/*----------------------------------------------------------------------*
 |  Return names of visualization data (public)                fb 09/15 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::VisNames(std::map<std::string,int>& names)
{
  std::string result_thickness = "thickness";

  names[result_thickness] = 1;


  SolidMaterial()->VisNames(names);

 return;

} // DRT::ELEMENTS::Membrane::VisNames

/*----------------------------------------------------------------------*
 |  Return visualization data (public)                     fbraeu 06/16 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
bool DRT::ELEMENTS::Membrane<distype>::VisData(const std::string& name, std::vector<double>& data)
{
 // Put the owner of this element into the file (use base class method for this)
 if (DRT::Element::VisData(name,data))
   return true;

 if (name == "thickness")
 {
   if (data.size()!= 1) dserror("size mismatch");
   for(int gp=0; gp<intpoints_.nquad; gp++)
   {
     data[0] += curr_thickness_[gp];
   }
   data[0] = data[0]/intpoints_.nquad;

   return true;
 }

 return SolidMaterial()->VisData(name, data, intpoints_.nquad, this->Id());

} // DRT::ELEMENTS::Membrane::VisData

/*----------------------------------------------------------------------*
 |  get reference and current configuration                fbraeu 06/16 |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_configuration(const std::vector<double>&             disp,
                                                         LINALG::Matrix<numnod_,noddof_>&       xrefe,
                                                         LINALG::Matrix<numnod_,noddof_>&       xcurr)
{
  // get reference configuration and determine current configuration
  DRT::Node** nodes = Nodes();
  if (!nodes) dserror("Nodes() returned null pointer");

  for (int i = 0; i < numnod_; ++i)
  {
    const double* x = nodes[i]->X();
    xrefe(i,0) = x[0];
    xrefe(i,1) = x[1];
    xrefe(i,2) = x[2];

    xcurr(i,0) = xrefe(i,0)+disp[i*noddof_+0];
    xcurr(i,1) = xrefe(i,1)+disp[i*noddof_+1];
    xcurr(i,2) = xrefe(i,2)+disp[i*noddof_+2];
  }

  return;

} // DRT::ELEMENTS::Membrane::mem_configuration

/*------------------------------------------------------------------------------------------------------*
 |  introduce an orthonormal base in the undeformed configuration at current Gauss point   fbraeu 06/16 |
 *------------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_orthonormalbase(const LINALG::Matrix<numnod_,noddof_>&                      xrefe,
                                                           const LINALG::Matrix<numnod_,noddof_>&                      xcurr,
                                                           const LINALG::Matrix<numdim_,numnod_>&                      derivs,
                                                           LINALG::Matrix<numdim_,numnod_>&                            derivs_ortho,
                                                           double&                                                     G1G2_cn,
                                                           LINALG::Matrix<noddof_,1>&                                  dXds1,
                                                           LINALG::Matrix<noddof_,1>&                                  dXds2,
                                                           LINALG::Matrix<noddof_,1>&                                  dxds1,
                                                           LINALG::Matrix<noddof_,1>&                                  dxds2,
                                                           LINALG::Matrix<noddof_,noddof_>&                            Q_trafo)
{
  /*===============================================================================*
   | introduce an orthonormal base in the undeformed configuration as proposed in: |
   | Gruttmann, "Theory and finite element formulation of rubberlike membrane      |
   | shells using principal stretches", 1992                                       |
   *===============================================================================*/

  LINALG::Matrix<noddof_,numdim_> G12(true);
  G12.MultiplyTT(1.0,xrefe,derivs,0.0);

  // G1 and G2 Gruttmann1992 equation (43)
  LINALG::Matrix<noddof_,1> G1(true);
  G1(0) = G12(0,0);
  G1(1) = G12(1,0);
  G1(2) = G12(2,0);

  LINALG::Matrix<noddof_,1> G2(true);
  G2(0) = G12(0,1);
  G2(1) = G12(1,1);
  G2(2) = G12(2,1);

  // cross product G1xG2
  LINALG::Matrix<noddof_,1> G1G2_cross(true);
  G1G2_cross(0) = G1(1)*G2(2)-G1(2)*G2(1);
  G1G2_cross(1) = G1(2)*G2(0)-G1(0)*G2(2);
  G1G2_cross(2) = G1(0)*G2(1)-G1(1)*G2(0);

  // 2 norm of vectors
  G1G2_cn = G1G2_cross.Norm2();
  double G1_n = G1.Norm2();

  // Gruttmann1992 equation (44), orthonormal base vectors
  LINALG::Matrix<noddof_,1> tn(true);
  tn(0) = G1G2_cross(0)/G1G2_cn;
  tn(1) = G1G2_cross(1)/G1G2_cn;
  tn(2) = G1G2_cross(2)/G1G2_cn;

  LINALG::Matrix<noddof_,1> t1(true);
  t1(0) = G1(0)/G1_n;
  t1(1) = G1(1)/G1_n;
  t1(2) = G1(2)/G1_n;

  LINALG::Matrix<noddof_,1> t2(true);
  t2(0) = tn(1)*t1(2)-tn(2)*t1(1);
  t2(1) = tn(2)*t1(0)-tn(0)*t1(2);
  t2(2) = tn(0)*t1(1)-tn(1)*t1(0);

  LINALG::Matrix<noddof_,numdim_> t12(true);
  t12(0,0) = t1(0);
  t12(1,0) = t1(1);
  t12(2,0) = t1(2);
  t12(0,1) = t2(0);
  t12(1,1) = t2(1);
  t12(2,1) = t2(2);

  // Jacobian transformation matrix and its inverse, Gruttmann1992 equation (44b)
  // for the Trafo from local membrane orthonormal coordinates to global coordinates
  // It is not the Jacobian for the Trafo from the parameter space xi, eta to the global coords!
  LINALG::Matrix<numdim_,numdim_> J(true);
  J.MultiplyTN(1.0,G12,t12,0.0);

  LINALG::Matrix<numdim_,numdim_> Jinv(true);
  Jinv.Invert(J);

  // calclate derivatives of shape functions in orthonormal base, Gruttmann1992 equation (42)
  derivs_ortho.Multiply(1.0,Jinv,derivs,0.0);

  // derivative of the reference position wrt the orthonormal base
  LINALG::Matrix<noddof_,numdim_> dXds(true);
  dXds.MultiplyTT(1.0,xrefe,derivs_ortho,0.0);

  dXds1(0) = dXds(0,0);
  dXds1(1) = dXds(1,0);
  dXds1(2) = dXds(2,0);

  dXds2(0) = dXds(0,1);
  dXds2(1) = dXds(1,1);
  dXds2(2) = dXds(2,1);

  // derivative of the current position wrt the orthonormal base
  LINALG::Matrix<noddof_,numdim_> dxds(true);
  dxds.MultiplyTT(1.0,xcurr,derivs_ortho,0.0);

  dxds1(0) = dxds(0,0);
  dxds1(1) = dxds(1,0);
  dxds1(2) = dxds(2,0);

  dxds2(0) = dxds(0,1);
  dxds2(1) = dxds(1,1);
  dxds2(2) = dxds(2,1);

  // determine Trafo from local membrane orthonormal coordinates to global coordinates
  Q_trafo(0,0) = t1(0);
  Q_trafo(1,0) = t1(1);
  Q_trafo(2,0) = t1(2);
  Q_trafo(0,1) = t2(0);
  Q_trafo(1,1) = t2(1);
  Q_trafo(2,1) = t2(2);
  Q_trafo(0,2) = tn(0);
  Q_trafo(1,2) = tn(1);
  Q_trafo(2,2) = tn(2);

  return;

} // DRT::ELEMENTS::Membrane::mem_orthonormalbase

/*-------------------------------------------------------------------------------------------------*
 |  pushforward of 2nd PK stresses to Cauchy stresses at gp                           fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_PK2toCauchy(const LINALG::Matrix<3,3>&                      pkstress_global,
                                                       const LINALG::Matrix<noddof_,noddof_>&          defgrd_global,
                                                       LINALG::Matrix<3,3>&                            cauchy)
{
  // calculate the Jacobi-deterinant
  const double detF = defgrd_global.Determinant();

  // check determinant of deformation gradient
  if (detF==0) dserror("Zero Determinant of Deformation Gradient.");

  // determine the cauchy stresses
  LINALG::Matrix<noddof_,noddof_> temp;
  temp.Multiply((1.0/detF),defgrd_global,pkstress_global,0.0);
  cauchy.MultiplyNT(1.0,temp,defgrd_global,1.0);

  return;

} // DRT::ELEMENTS::Membrane::mem_PK2toCauchy

/*-------------------------------------------------------------------------------------------------*
 |  pushforward of Green-Lagrange to Euler-Almansi strains at gp                      fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_GLtoEA(const LINALG::Matrix<3,3>&                      glstrain_global,
                                                  const LINALG::Matrix<noddof_,noddof_>&          defgrd_global,
                                                  LINALG::Matrix<3,3>&                            euler_almansi)
{
  // check determinant of deformation gradient
  if (defgrd_global.Determinant()==0) dserror("Inverse of Deformation Gradient can not be calcualated due to a zero Determinant.");

  // inverse of deformation gradient
  LINALG::Matrix<noddof_,noddof_> invdefgrd(true);
  invdefgrd.Invert(defgrd_global);

  // determine the euler-almansi strains
  LINALG::Matrix<noddof_,noddof_> temp;
  temp.Multiply(1.0,glstrain_global,invdefgrd,0.0);
  euler_almansi.MultiplyTN(1.0,invdefgrd,temp,1.0);

  return;

} // DRT::ELEMENTS::Membrane::mem_GLtoEA

/*-------------------------------------------------------------------------------------------------*
 |  transforms local membrane surface tensor to tensor in global coords               fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_localtoglobal(const LINALG::Matrix<noddof_,noddof_>&    Q_trafo,
                                                         const LINALG::Matrix<noddof_,noddof_>&    local,
                                                         LINALG::Matrix<noddof_,noddof_>&          global)
{
  LINALG::Matrix<noddof_,noddof_> temp(true);
  temp.MultiplyNN(1.0,Q_trafo,local,0.0);
  global.MultiplyNT(1.0,temp,Q_trafo,0.0);

  return;

} // DRT::ELEMENTS::Membrane::mem_localtoglobal

/*-------------------------------------------------------------------------------------------------*
 |  transforms tensor in global coords to local membrane surface tensor               fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_globaltolocal(const LINALG::Matrix<noddof_,noddof_>&    Q_trafo,
                                                         const LINALG::Matrix<noddof_,noddof_>&    global,
                                                         LINALG::Matrix<noddof_,noddof_>&          local)
{
  LINALG::Matrix<noddof_,noddof_> temp(true);
  temp.MultiplyTN(1.0,Q_trafo,global,0.0);
  local.MultiplyNN(1.0,temp,Q_trafo,0.0);

  return;

} // DRT::ELEMENTS::Membrane::mem_globaltolocal

/*-------------------------------------------------------------------------------------------------*
 |  evaluate 3D Material law and reduce to plane stress state                         fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_Material3dPlane(const LINALG::Matrix<noddof_,1>&          dXds1,
                                                           const LINALG::Matrix<noddof_,1>&          dXds2,
                                                           const LINALG::Matrix<noddof_,1>&          dxds1,
                                                           const LINALG::Matrix<noddof_,1>&          dxds2,
                                                           LINALG::Matrix<noddof_,noddof_>&          defgrd_global,
                                                           LINALG::Matrix<noddof_,noddof_>&          cauchygreen_local,
                                                           LINALG::Matrix<3,1>&                      pkstress,
                                                           LINALG::Matrix<3,3>&                      cmat,
                                                           const LINALG::Matrix<noddof_,noddof_>&    Q_trafo,
                                                           Teuchos::ParameterList&                   params)
{
  /*===============================================================================*
   | the material law is called in global coords                                   |
   | for anisotropic materials the fiber is given in global coords                 |
   | the reduction to a plane stress state is fulfilled in local coords            |
   | in between the quantities are transformed                                     |
   *===============================================================================*/

  /*===============================================================================*
   | Transformation:  local --> global                                             |
   *===============================================================================*/

  // transform local cauchygreen to global coordinates
  LINALG::Matrix<noddof_,noddof_> cauchygreen_global(true);
  mem_localtoglobal(Q_trafo,cauchygreen_local,cauchygreen_global);

  // green-lagrange strain vector in global coordinates
  LINALG::Matrix<6,1> gl_global(true);
  gl_global(0) = 0.5*(cauchygreen_global(0,0)-1.0);
  gl_global(1) = 0.5*(cauchygreen_global(1,1)-1.0);
  gl_global(2) = 0.5*(cauchygreen_global(2,2)-1.0);
  gl_global(3) = cauchygreen_global(0,1);
  gl_global(4) = cauchygreen_global(1,2);
  gl_global(5) = cauchygreen_global(0,2);

  // call 3 dimensional material law in global coordinates
  LINALG::Matrix<6,6> cmat_global(true);
  LINALG::Matrix<6,1> pk_global(true);
  SolidMaterial()->Evaluate(&defgrd_global,&gl_global,params,&pk_global,&cmat_global,Id());

  // 2nd piola kirchhoff stress tensor in global coordinates in matrix notation
  LINALG::Matrix<noddof_,noddof_> pkstress_global(true);
  mem_voigttomatrix(pk_global,pkstress_global);

  /*===============================================================================*
   | Transformation:  global --> local                                             |
   *===============================================================================*/

  // transform global 2nd piola kirchhoff stress tensor to local coordinates
  LINALG::Matrix<noddof_,noddof_> pkstress_local(true);
  mem_globaltolocal(Q_trafo,pkstress_global,pkstress_local);

  // 2nd piola kirchhoff stress vector in local coordinates in "stress-like" Voigt notation
  LINALG::Matrix<6,1> pk_local(true);
  mem_matrixtovoigt(pkstress_local,pk_local);

  // determine transformation matrix cmat_trafo
  LINALG::Matrix<6,6> cmat_trafo(true);
  mem_cmat_trafo(Q_trafo,cmat_trafo);

  // transform global material tangent tensor to local coordinates
  LINALG::Matrix<6,6> cmat_local(true);
  mem_cmat_globaltolocal(cmat_trafo,cmat_global,cmat_local);


  // cauchygreen_local bears final values on:    C_{11},C_{22},C_{12}
  // cauchygreen_local bears initial guesses on: C_{33},C_{23},C_{31}
  // initial plane stress error
  double pserr = std::sqrt(pk_local(2)*pk_local(2) + pk_local(4)*pk_local(4) + pk_local(5)*pk_local(5));

  // make Newton-Raphson iteration to identify
  // C_{33},C_{23},C_{31} which satisfy S_{33}=S_{23}=S_{31}=0
  int i = 0;
  const double tol = 1.0E-10;
  const int n = 50;

  // working arrays
  LINALG::Matrix<3,3> crr(true);  // LHS // constitutive matrix of restraint components
                                         // this matrix needs to be zeroed out for further usage
                                         // in case the following while loop is entirely skipped during runtime
  LINALG::Matrix<3,1> rr(true);   // RHS // stress residual of restraint components
  LINALG::Matrix<3,1> ir(true);   // SOL // restraint strain components
  double lambda3 = 1.0;

  // the Newton-Raphson loop
  while ( (pserr > tol) and (i < n) )
  {
    // build sub-system crr.ir=rr to solve
    crr(0,0) = cmat_local(2,2);  crr(0,1) = cmat_local(2,4);  crr(0,2) = cmat_local(2,5);
    crr(1,0) = cmat_local(4,2);  crr(1,1) = cmat_local(4,4);  crr(1,2) = cmat_local(4,5);
    crr(2,0) = cmat_local(5,2);  crr(2,1) = cmat_local(5,4);  crr(2,2) = cmat_local(5,5);

    rr(0) = -pk_local(2);
    rr(1) = -pk_local(4);
    rr(2) = -pk_local(5);

    // solution: an in-plane inversion is used
    crr.Invert();
    ir.Multiply(1.0,crr,rr,0.0);

    // ir updates the glstrain_local vector but can be directly added to the right cauchy-green tensor as below

    // update right cauchy-green strain tensor
    cauchygreen_local(2,2) += 2.0*ir(0);
    cauchygreen_local(0,2) += 2.0*ir(2);
    cauchygreen_local(1,2) += 2.0*ir(1);
    cauchygreen_local(2,0) += 2.0*ir(2);
    cauchygreen_local(2,1) += 2.0*ir(1);

    /*===============================================================================*
     | Transformation:  local --> global                                             |
     *===============================================================================*/

    // principle stretch in thickness direction
    lambda3 = std::sqrt(cauchygreen_local(2,2));

    // update global surface deformation gradient
    mem_defgrd_global(dXds1,dXds2,dxds1,dxds2,lambda3,defgrd_global);

    // transform local cauchygreen to global coordinates
    mem_localtoglobal(Q_trafo,cauchygreen_local,cauchygreen_global);

    // Green-Lagrange strains in global coordinates in "stress-like" voigt notation
    gl_global(0) = 0.5*(cauchygreen_global(0,0)-1.0);
    gl_global(1) = 0.5*(cauchygreen_global(1,1)-1.0);
    gl_global(2) = 0.5*(cauchygreen_global(2,2)-1.0);
    gl_global(3) = cauchygreen_global(0,1);
    gl_global(4) = cauchygreen_global(1,2);
    gl_global(5) = cauchygreen_global(0,2);

    // call for new 3d stress response
    pk_global.Clear();        // must be blanked!!
    cmat_global.Clear();      // must be blanked!!
    SolidMaterial()->Evaluate(&defgrd_global,&gl_global,params,&pk_global,&cmat_global,Id());

    // 2nd piola kirchhoff stress tensor in global coordinates in matrix notation
    mem_voigttomatrix(pk_global,pkstress_global);

    /*===============================================================================*
     | Transformation:  global --> local                                             |
     *===============================================================================*/

    // transform global 2nd piola kirchhoff stress tensor to local coordinates
    mem_globaltolocal(Q_trafo,pkstress_global,pkstress_local);

    // 2nd piola kirchhoff stress vector in local coordinates
    mem_matrixtovoigt(pkstress_local,pk_local);

    // current plane stress error
    pserr = std::sqrt(pk_local(2)*pk_local(2) + pk_local(4)*pk_local(4) + pk_local(5)*pk_local(5));

    // transform global material tangent tensor to local coordinates
    mem_cmat_globaltolocal(cmat_trafo,cmat_global,cmat_local);

    // increment loop index
    i += 1;
  }

  // check if convergence was reached or the maximum number of iterations
  if ( (i >= n) and (pserr > tol) )
  {
    dserror("Failed to identify plane stress solution for Membrane after %d iterations",i);
  }
  else
  {
    // static condensation
    // The restraint strains E_{33},E_{23},E_{31} have been made
    // dependent on free strains E_{11},E_{22},E_{12}
    // --- with an implicit function.
    // Thus the effect of the linearization w.r.t. the
    // dependent strains must be added onto the free strains.
    LINALG::Matrix<3,3> cfr(true);
    cfr(0,0) = cmat_local(0,2);  cfr(0,1) = cmat_local(0,4);  cfr(0,2) = cmat_local(0,5);
    cfr(1,0) = cmat_local(1,2);  cfr(1,1) = cmat_local(1,4);  cfr(1,2) = cmat_local(1,5);
    cfr(2,0) = cmat_local(3,2);  cfr(2,1) = cmat_local(3,4);  cfr(2,2) = cmat_local(3,5);

    LINALG::Matrix<3,3> crrrf(true);
    crrrf.MultiplyNT(crr,cfr);

    LINALG::Matrix<3,3> cfrrrrf(true);
    cfrrrrf.MultiplyNN(cfr,crrrf);

    // update constitutive matrix of free components
    cmat_local(0,0) -= cfrrrrf(0,0);  cmat_local(0,1) -= cfrrrrf(0,1);  cmat_local(0,3) -= cfrrrrf(0,2);
    cmat_local(1,0) -= cfrrrrf(1,0);  cmat_local(1,1) -= cfrrrrf(1,1);  cmat_local(1,3) -= cfrrrrf(1,2);
    cmat_local(3,0) -= cfrrrrf(2,0);  cmat_local(3,1) -= cfrrrrf(2,1);  cmat_local(3,3) -= cfrrrrf(2,2);
  }

  // write 2nd Piola--Kirchhoff stress in 2d stress vector
  pkstress(0) = pk_local(0);
  pkstress(1) = pk_local(1);
  pkstress(2) = pk_local(3);

  // write material tangent matrix as 2d matrix
  cmat(0,0) = cmat_local(0,0); cmat(0,1) = cmat_local(0,1); cmat(0,2) = cmat_local(0,3);
  cmat(1,0) = cmat_local(1,0); cmat(1,1) = cmat_local(1,1); cmat(1,2) = cmat_local(1,3);
  cmat(2,0) = cmat_local(3,0); cmat(2,1) = cmat_local(3,1); cmat(2,2) = cmat_local(3,3);

  return;

} // DRT::ELEMENTS::Membrane::mem_Material3dPlane

/*-------------------------------------------------------------------------------------------------*
 |  transformation matrix for material tangent tensor                                 fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_cmat_trafo(const LINALG::Matrix<noddof_,noddof_>&    Q_trafo,
                                                      LINALG::Matrix<6,6>&                      cmat_trafo)
{
  // strain like transformation matrix for the material elasticity matrix
  cmat_trafo(0,0) = Q_trafo(0,0)*Q_trafo(0,0);
  cmat_trafo(0,1) = Q_trafo(1,0)*Q_trafo(1,0);
  cmat_trafo(0,2) = Q_trafo(2,0)*Q_trafo(2,0);
  cmat_trafo(0,3) = 2.0*Q_trafo(0,0)*Q_trafo(1,0);
  cmat_trafo(0,4) = 2.0*Q_trafo(1,0)*Q_trafo(2,0);
  cmat_trafo(0,5) = 2.0*Q_trafo(0,0)*Q_trafo(2,0);

  cmat_trafo(1,0) = Q_trafo(0,1)*Q_trafo(0,1);
  cmat_trafo(1,1) = Q_trafo(1,1)*Q_trafo(1,1);
  cmat_trafo(1,2) = Q_trafo(2,1)*Q_trafo(2,1);
  cmat_trafo(1,3) = 2.0*Q_trafo(0,1)*Q_trafo(1,1);
  cmat_trafo(1,4) = 2.0*Q_trafo(1,1)*Q_trafo(2,1);
  cmat_trafo(1,5) = 2.0*Q_trafo(0,1)*Q_trafo(2,1);

  cmat_trafo(2,0) = Q_trafo(0,2)*Q_trafo(0,2);
  cmat_trafo(2,1) = Q_trafo(1,2)*Q_trafo(1,2);
  cmat_trafo(2,2) = Q_trafo(2,2)*Q_trafo(2,2);
  cmat_trafo(2,3) = 2.0*Q_trafo(0,2)*Q_trafo(1,2);
  cmat_trafo(2,4) = 2.0*Q_trafo(1,2)*Q_trafo(2,2);
  cmat_trafo(2,5) = 2.0*Q_trafo(0,2)*Q_trafo(2,2);

  cmat_trafo(3,0) = Q_trafo(0,0)*Q_trafo(0,1);
  cmat_trafo(3,1) = Q_trafo(1,0)*Q_trafo(1,1);
  cmat_trafo(3,2) = Q_trafo(2,0)*Q_trafo(2,1);
  cmat_trafo(3,3) = Q_trafo(0,0)*Q_trafo(1,1) + Q_trafo(1,0)*Q_trafo(0,1);
  cmat_trafo(3,4) = Q_trafo(1,0)*Q_trafo(2,1) + Q_trafo(2,0)*Q_trafo(1,1);
  cmat_trafo(3,5) = Q_trafo(0,0)*Q_trafo(2,1) + Q_trafo(2,0)*Q_trafo(0,1);

  cmat_trafo(4,0) = Q_trafo(0,1)*Q_trafo(0,2);
  cmat_trafo(4,1) = Q_trafo(1,1)*Q_trafo(1,2);
  cmat_trafo(4,2) = Q_trafo(2,1)*Q_trafo(2,2);
  cmat_trafo(4,3) = Q_trafo(0,1)*Q_trafo(1,2) + Q_trafo(1,1)*Q_trafo(0,2);
  cmat_trafo(4,4) = Q_trafo(1,1)*Q_trafo(2,2) + Q_trafo(2,1)*Q_trafo(1,2);
  cmat_trafo(4,5) = Q_trafo(0,1)*Q_trafo(2,2) + Q_trafo(2,1)*Q_trafo(0,2);

  cmat_trafo(5,0) = Q_trafo(0,0)*Q_trafo(0,2);
  cmat_trafo(5,1) = Q_trafo(1,0)*Q_trafo(1,2);
  cmat_trafo(5,2) = Q_trafo(2,0)*Q_trafo(2,2);
  cmat_trafo(5,3) = Q_trafo(0,0)*Q_trafo(1,2) + Q_trafo(1,0)*Q_trafo(0,2);
  cmat_trafo(5,4) = Q_trafo(1,0)*Q_trafo(2,2) + Q_trafo(2,0)*Q_trafo(1,2);
  cmat_trafo(5,5) = Q_trafo(0,0)*Q_trafo(2,2) + Q_trafo(2,0)*Q_trafo(0,2);

  return;

} // DRT::ELEMENTS::Membrane::mem_cmat_trafo

/*-------------------------------------------------------------------------------------------------*
 |  transforms material tangent tensor from global coords to local membrane surface   fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_cmat_globaltolocal(const LINALG::Matrix<6,6>&    cmat_trafo,
                                                              const LINALG::Matrix<6,6>&    cmat_global,
                                                              LINALG::Matrix<6,6>&          cmat_local)
{
  LINALG::Matrix<6,6> temp(true);
  temp.MultiplyNN(1.0,cmat_trafo,cmat_global,0.0);
  cmat_local.MultiplyNT(1.0,temp,cmat_trafo,0.0);

  return;

} // DRT::ELEMENTS::Membrane::mem_cmat_globaltolocal

/*-------------------------------------------------------------------------------------------------*
 |  transforms matrix to "stress-like" voigt notation                                 fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_matrixtovoigt(const LINALG::Matrix<3,3>&    matrix,
                                                         LINALG::Matrix<6,1>&          voigt)
{
  voigt(0) = matrix(0,0);
  voigt(1) = matrix(1,1);
  voigt(2) = matrix(2,2);
  voigt(3) = matrix(0,1);
  voigt(4) = matrix(1,2);
  voigt(5) = matrix(0,2);

  return;

} // DRT::ELEMENTS::Membrane::mem_matrixtovoigt

/*-------------------------------------------------------------------------------------------------*
 |  transforms "stress-like" voigt notation to matrix                                 fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_voigttomatrix(const LINALG::Matrix<6,1>&    voigt,
                                                         LINALG::Matrix<3,3>&          matrix)
{
  matrix(0,0) = voigt(0);
  matrix(1,1) = voigt(1);
  matrix(2,2) = voigt(2);
  matrix(0,1) = voigt(3);
  matrix(1,2) = voigt(4);
  matrix(0,2) = voigt(5);
  matrix(1,0) = voigt(3);
  matrix(2,1) = voigt(4);
  matrix(2,0) = voigt(5);

  return;

} // DRT::ELEMENTS::Membrane::mem_voigttomatrix

/*-------------------------------------------------------------------------------------------------*
 |  determine deformation gradient in global coordinates                              fbraeu 06/16 |
 *-------------------------------------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Membrane<distype>::mem_defgrd_global(const LINALG::Matrix<noddof_,1>&          dXds1,
                                                         const LINALG::Matrix<noddof_,1>&          dXds2,
                                                         const LINALG::Matrix<noddof_,1>&          dxds1,
                                                         const LINALG::Matrix<noddof_,1>&          dxds2,
                                                         const double&                             lambda3,
                                                         LINALG::Matrix<noddof_,noddof_>&          defgrd_global)
{
  // clear
  defgrd_global.Clear();

  // determine cross product x,1 x x,2
  LINALG::Matrix<noddof_,1> xcurr_cross(true);
  xcurr_cross(0) = dxds1(1)*dxds2(2)-dxds1(2)*dxds2(1);
  xcurr_cross(1) = dxds1(2)*dxds2(0)-dxds1(0)*dxds2(2);
  xcurr_cross(2) = dxds1(0)*dxds2(1)-dxds1(1)*dxds2(0);

  // normalize the cross product for the current configuration
  xcurr_cross.Scale(1.0/xcurr_cross.Norm2());

  // determine cross product X,1 x X,2, has unit length due to orthonormal basis
  LINALG::Matrix<noddof_,1> xrefe_cross(true);
  xrefe_cross(0) = dXds1(1)*dXds2(2)-dXds1(2)*dXds2(1);
  xrefe_cross(1) = dXds1(2)*dXds2(0)-dXds1(0)*dXds2(2);
  xrefe_cross(2) = dXds1(0)*dXds2(1)-dXds1(1)*dXds2(0);

  defgrd_global.MultiplyNT(1.0,dxds1,dXds1,0.0);
  defgrd_global.MultiplyNT(1.0,dxds2,dXds2,1.0);
  // scale third dimension by sqrt(rcg33), that equals the principle stretch lambda_3
  defgrd_global.MultiplyNT(lambda3,xcurr_cross,xrefe_cross,1.0);

  return;

} // DRT::ELEMENTS::Membrane::mem_defgrd_global

template class DRT::ELEMENTS::Membrane<DRT::Element::tri3>;
template class DRT::ELEMENTS::Membrane<DRT::Element::tri6>;
template class DRT::ELEMENTS::Membrane<DRT::Element::quad4>;
template class DRT::ELEMENTS::Membrane<DRT::Element::quad9>;