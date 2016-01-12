/*!----------------------------------------------------------------------
\file ssi_partitioned_2wc_protrusionformation.cpp

\brief specialization of ssi2wc, which includes "structale"-surface growth

<pre>
Maintainers: Andreas Rauch
             rauch@lnm.mw.tum.de
             http://www.lnm.mw.tum.de
             089 - 289 -15240
</pre>
*----------------------------------------------------------------------*/
#include "ssi_partitioned_2wc_protrusionformation.H"

#include "../drt_lib/drt_globalproblem.H"

#include "../drt_adapter/ad_ale_fluid.H"
#include "../drt_adapter/adapter_coupling.H"
#include "../drt_adapter/ad_str_fsiwrapper.H"

#include "../drt_structure/stru_aux.H"
#include "../drt_wear/wear_utils.H"
#include "../drt_lib/drt_utils_createdis.H"
#include "../drt_ale/ale_utils_clonestrategy.H"

#include "../drt_w1/wall1.H"

#include "../drt_so3/so_hex8.H"
#include "../drt_so3/so_hex20.H"
#include "../drt_so3/so_hex27.H"
#include "../drt_so3/so_tet4.H"
#include "../drt_so3/so_tet10.H"
#include "../drt_so3/so3_scatra.H"

/*----------------------------------------------------------------------*
 | constructor                                               rauch 01/16 |
 *----------------------------------------------------------------------*/
SSI::SSI_Part2WC_PROTRUSIONFORMATION::SSI_Part2WC_PROTRUSIONFORMATION(const Epetra_Comm& comm,
    const Teuchos::ParameterList& globaltimeparams,
    const Teuchos::ParameterList& scatraparams,
    const Teuchos::ParameterList& structparams,
    const std::string struct_disname,
    const std::string scatra_disname)
  : SSI_Part2WC(comm, globaltimeparams, scatraparams, structparams,struct_disname,scatra_disname),
    growthincrement_(Teuchos::rcp(new Epetra_Vector(*AleField()->Interface()->Map(AleField()->Interface()->cond_fsi)), true)),
    delta_ale_(Teuchos::rcp(new Epetra_Vector(AleField()->Dispnp()->Map(), true)) )
{
  specialized_structure_ = Teuchos::rcp_dynamic_cast<ADAPTER::FSIStructureWrapper>(StructureField());
  if(specialized_structure_ == Teuchos::null)
    dserror("cast from ADAPTER::Structure to ADAPTER::FSIStructureWrapper failed");

  // ask base algorithm for the ale time integrator
  Teuchos::RCP<ADAPTER::AleBaseAlgorithm> ale = Teuchos::rcp(new ADAPTER::AleBaseAlgorithm(DRT::Problem::Instance()->SSIControlParams(), DRT::Problem::Instance()->GetDis("ale")));
  ale_ =  Teuchos::rcp_dynamic_cast<ADAPTER::AleFluidWrapper>(ale->AleField());
  if(ale_ == Teuchos::null)
    dserror("cast from ADAPTER::Ale to ADAPTER::AleFsiWrapper failed");
  // create empty operator
  AleField()->CreateSystemMatrix();

  // build coupling objects for dof transfer between structure and ale
  const int ndim = DRT::Problem::Instance()->NDim();

  // create ale-struct coupling
  const Epetra_Map* structdofmap =
      StructureField()->Discretization()->NodeRowMap();
  const Epetra_Map* aledofmap = AleField()->Discretization()->NodeRowMap();

  // if there are two identical nodes (i.e. for initial contact) the nodes matching creates an error !!!
  coupalestru_ = Teuchos::rcp(new ADAPTER::Coupling());
  coupalestru_->SetupCoupling(*AleField()->Discretization(),
      *StructureField()->Discretization(), *aledofmap, *structdofmap, ndim, true, 1e-06);

  //create interface coupling
  coupstrualei_ = Teuchos::rcp(new ADAPTER::Coupling());
  coupstrualei_->SetupConditionCoupling(
      *SpecStructureField()->Discretization(),
      SpecStructureField()->Interface()->FSICondMap(),
      *AleField()->Discretization(),
      AleField()->Interface()->Map(AleField()->Interface()->cond_fsi),
      "FSICoupling", ndim);

}


/*----------------------------------------------------------------------*
 | Solve structure filed                                    rauch 01/16 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::DoStructStep()
{
  if (Comm().MyPID() == 0)
  {
    std::cout
        << "\n***********************\n STRUCTURE SOLVER \n***********************\n";
  }

  // Newton-Raphson iteration
  //1. solution
  StructureField()-> Solve();

  EvaluateGrowth();

  // do ale step
  DoAleStep(growthincrement_);

  // application of mesh displacements to structural field,
  // update material displacements
  UpdateMatConf();

  // update dispnp
  UpdateSpatConf();
}


/*----------------------------------------------------------------------*
 | Solve ale field                                        rauch 01/16 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::DoAleStep(Teuchos::RCP<Epetra_Vector> growthincrement)
{
  std::cout<<"==================  DoAleStep  ================== "<<std::endl;
  Teuchos::RCP<Epetra_Vector> dispnpstru = StructureToAle(
      StructureField()->Dispnp());

  AleField()->WriteAccessDispnp()->Update(1.0, *dispnpstru, 0.0);

  // application of interface displacements as dirichlet conditions
  AleField()->AddInterfaceDisplacements(growthincrement);

  // solve time step
  AleField()->TimeStep(ALE::UTILS::MapExtractor::dbc_set_part_fsi);

  return;
}


/*----------------------------------------------------------------------*
 |                                                          rauch 01/16 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::UpdateMatConf()
{

  // mesh displacement from solution of ALE field in structural dofs
  // first perform transformation from ale to structure dofs
  Teuchos::RCP<Epetra_Vector> disalenp = AleToStructure(AleField()->Dispnp());

  //std::cout<<"disalenp: "<<*disalenp<<std::endl;

  // vector of current spatial displacements
  Teuchos::RCP<const Epetra_Vector> dispnp = StructureField()->Dispnp(); // change to ExtractDispn() for overlap

  // material displacements
  Teuchos::RCP<Epetra_Vector> dismat = Teuchos::rcp(
      new Epetra_Vector(dispnp->Map()), true);

   // set state
  (StructureField()->Discretization())->SetState(0, "displacement", dispnp);

  // set state
  (StructureField()->Discretization())->SetState(0, "material_displacement",
      StructureField()->DispMat());

  disalenp->Update(-1.0, *dispnp, 1.0);
  delta_ale_->Update(1.0, *disalenp, 0.0);

  // loop over all row nodes to fill graph
  for (int k = 0; k < StructureField()->Discretization()->NumMyRowNodes(); ++k)
  {
    int gid = StructureField()->Discretization()->NodeRowMap()->GID(k);
    DRT::Node* node = StructureField()->Discretization()->gNode(gid);
    DRT::Element** ElementPtr = node->Elements();
    int numelement = node->NumElement();

    const int numdof = StructureField()->Discretization()->NumDof(node);

    // create Xmat for 3D problems
    double XMat[numdof];
    double XMesh[numdof];

    for(int dof = 0; dof < numdof; ++dof)
    {
      int dofgid = StructureField()->Discretization()->Dof(node,dof);
      int doflid = (dispnp->Map()).LID(dofgid);
      XMesh[dof] = node->X()[dof] + (*dispnp)[doflid] + (*disalenp)[doflid];
    }

    // create updated  XMat --> via nonlinear interpolation between nodes (like gp projection)
    AdvectionMap(XMat, XMesh, ElementPtr, numelement, true);

    // store in dispmat
    for(int dof = 0; dof < numdof; ++dof)
    {
      int dofgid = StructureField()->Discretization()->Dof(node,dof);
      int doflid = (dispnp->Map()).LID(dofgid);
      (*dismat)[doflid] = XMat[dof] - node->X()[dof];
    }
  } // end row node loop

  // apply material displacements to structural field
  // if advection map is not succesful --> use old xmat
  StructureField()->ApplyDisMat(dismat);

  return;
}


/*----------------------------------------------------------------------*
 |                                                          rauch 01/16 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::UpdateSpatConf()
{

  // mesh displacement from solution of ALE field in structural dofs
  // first perform transformation from ale to structure dofs
  Teuchos::RCP<Epetra_Vector> disalenp = AleToStructure(AleField()->Dispnp());

  // get structure dispnp vector
  Teuchos::RCP<Epetra_Vector> dispnp = StructureField()->WriteAccessDispnp(); // change to ExtractDispn() for overlap

  // update per absolute vector
  dispnp->Update(1.0, *disalenp, 0.0);

  return;
}


/*----------------------------------------------------------------------*/
// advection map assembly analogous to wear framework       rauch 01/16 |
/*----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::AdvectionMap(
    double* Xtarget,            // out
    double* Xsource,            // in
    DRT::Element** ElementPtr,  // in
    int numelements,            // in
    bool spatialtomaterial)     // in
{
  // get problem dimension
  const int ndim = DRT::Problem::Instance()->NDim();

  // define source and target configuration
  std::string sourceconf;
  std::string targetconf;

  if(spatialtomaterial)
  {
    sourceconf = "displacement";
    targetconf = "material_displacement";
  }
  else
  {
    sourceconf = "material_displacement";
    targetconf = "displacement";
  }

  // found element the spatial coordinate lies in
  bool found = false;

  // parameter space coordinates
  double e[3];
  double ge1 = 1e12;
  double ge2 = 1e12;
  double ge3 = 1e12;
  int gele   = 0;

  // loop over adjacent elements
  for (int jele = 0; jele < numelements; jele++)
  {
    // get element
    DRT::Element* actele = ElementPtr[jele];

    // get element location vector, dirichlet flags and ownerships
    DRT::Element::LocationArray la(1);
    actele->LocationVector(*(StructureField()->Discretization()), la, false);

    // get state
    Teuchos::RCP<const Epetra_Vector> dispsource =
        (StructureField()->Discretization())->GetState(sourceconf);
    Teuchos::RCP<const Epetra_Vector> disptarget =
        (StructureField()->Discretization())->GetState(targetconf);

    if (ndim == 2)
    {
      if (actele->Shape() == DRT::Element::quad4)
        WEAR::UTILS::av<DRT::Element::quad4>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::quad8)
        WEAR::UTILS::av<DRT::Element::quad8>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::quad9)
        WEAR::UTILS::av<DRT::Element::quad9>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::tri3)
        WEAR::UTILS::av<DRT::Element::tri3>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::tri6)
        WEAR::UTILS::av<DRT::Element::tri6>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else
        dserror("ERROR: shape function not supported!");

      // checks if the spatial coordinate lies within this element
      // if yes, returns the material displacements
//      w1ele->AdvectionMapElement(XMat1,XMat2,XMesh1,XMesh2,disp,dispmat, la,found,e1,e2);

      if (found == false)
      {
        if (abs(ge1) > 1.0 and abs(e[0]) < abs(ge1))
        {
          ge1 = e[0];
          gele = jele;
        }
        if (abs(ge2) > 1.0 and abs(e[1]) < abs(ge2))
        {
          ge2 = e[1];
          gele = jele;
        }
      }
    }
    else
    {
      if (actele->ElementType() == DRT::ELEMENTS::So_hex8Type::Instance() or
          actele->ElementType() == DRT::ELEMENTS::So_hex8ScatraType::Instance())
        WEAR::UTILS::av<DRT::Element::hex8>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_hex20Type::Instance())
        WEAR::UTILS::av<DRT::Element::hex20>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_hex27Type::Instance())
        WEAR::UTILS::av<DRT::Element::hex27>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_tet4Type::Instance())
        WEAR::UTILS::av<DRT::Element::tet4>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_tet10Type::Instance())
        WEAR::UTILS::av<DRT::Element::tet10>(actele, Xtarget, Xsource, dispsource, disptarget,
            la[0].lm_, found, e);
      else
        dserror("ERROR: element type not supported!");

      if (found == false)
      {
        //std::cout<<"e[0]="<<e[0]<<std::endl;          //Bettina: rausfinden was hier passiert
        //std::cout<<"ge1="<<ge1<<std::endl;
        if (abs(ge1) > 1.0 and abs(e[0]) < abs(ge1))
        {
          ge1 = e[0];
          gele = jele;
        }
        //std::cout<<"e[1]="<<e[1]<<std::endl;        //Bettina: rausfinden was hier passiert
        //std::cout<<"ge2="<<ge2<<std::endl;
        if (abs(ge2) > 1.0 and abs(e[1]) < abs(ge2))
        {
          ge2 = e[1];
          gele = jele;
        }
        //std::cout<<"e[2]="<<e[2]<<std::endl;        //Bettina: rausfinden was hier passiert
        //std::cout<<"ge3="<<ge3<<std::endl;
        if (abs(ge3) > 1.0 and abs(e[2]) < abs(ge3))
        {
          ge3 = e[2];
          gele = jele;
        }
      }
    }

    // leave when element is found
    if (found == true)
      return;

  } // end loop over adj elements

  // ****************************************
  //  if displ not into elements
  // ****************************************
  DRT::Element* actele = ElementPtr[gele];

  // get element location vector, dirichlet flags and ownerships
  DRT::Element::LocationArray la(1);
  actele->LocationVector(*(StructureField()->Discretization()), la, false);
  //std::cout<<"FOUND NO ELEMENT! but in the new loop"<<std::endl;      //Bettina: test
  // get state
  Teuchos::RCP<const Epetra_Vector> dispsource =
      (StructureField()->Discretization())->GetState(sourceconf);
  Teuchos::RCP<const Epetra_Vector> disptarget =
      (StructureField()->Discretization())->GetState(targetconf);

  if (ndim == 2)
  {
    if (actele->Shape() == DRT::Element::quad4)
      WEAR::UTILS::av<DRT::Element::quad4>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::quad8)
      WEAR::UTILS::av<DRT::Element::quad8>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::quad9)
      WEAR::UTILS::av<DRT::Element::quad9>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::tri3)
      WEAR::UTILS::av<DRT::Element::tri3>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::tri6)
      WEAR::UTILS::av<DRT::Element::tri6>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else
      dserror("ERROR: shape function not supported!");
  }
  else
  {
    if (actele->ElementType() == DRT::ELEMENTS::So_hex8Type::Instance() or
        actele->ElementType() == DRT::ELEMENTS::So_hex8ScatraType::Instance())
      WEAR::UTILS::av<DRT::Element::hex8>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_hex20Type::Instance())
      WEAR::UTILS::av<DRT::Element::hex20>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_hex27Type::Instance())
      WEAR::UTILS::av<DRT::Element::hex27>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_tet4Type::Instance())
      WEAR::UTILS::av<DRT::Element::tet4>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_tet10Type::Instance())
      WEAR::UTILS::av<DRT::Element::tet10>(actele, Xtarget, Xsource, dispsource, disptarget,
          la[0].lm_, found, e);
    else
      dserror("ERROR: element type not supported!");
  }


  // bye
  return;
}



/*----------------------------------------------------------------------*
 | transform from structure to ale map                      rauch 01/16 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SSI::SSI_Part2WC_PROTRUSIONFORMATION::StructureToAle(
    Teuchos::RCP<Epetra_Vector> vec)
{
  return AleStruCoupling()->SlaveToMaster(vec);
}


/*----------------------------------------------------------------------*
 | transform from structure to ale map                      rauch 01/16 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SSI::SSI_Part2WC_PROTRUSIONFORMATION::StructureToAle(
    Teuchos::RCP<const Epetra_Vector> vec)
{
  if (AleStruCoupling()==Teuchos::null)
    dserror("'!!!!!!!!!!!!!");
  return AleStruCoupling()->SlaveToMaster(vec);
}

/*----------------------------------------------------------------------*
 | transform from ale to structure map                      rauch 01/16 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SSI::SSI_Part2WC_PROTRUSIONFORMATION::AleToStructure(
    Teuchos::RCP<Epetra_Vector> vec)
{
  return AleStruCoupling()->MasterToSlave(vec);
}


/*----------------------------------------------------------------------*
 | transform from ale to structure map                      rauch 01/16 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> SSI::SSI_Part2WC_PROTRUSIONFORMATION::AleToStructure(
    Teuchos::RCP<const Epetra_Vector> vec)
{
  return AleStruCoupling()->MasterToSlave(vec);
}


/*----------------------------------------------------------------------*
 |                                                          rauch 01/16 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::SetupDiscretizations(const Epetra_Comm& comm, const std::string struct_disname, const std::string scatra_disname)
{
  // call SetupDiscretizations in base class
  SSI::SSI_Base::SetupDiscretizations(comm, struct_disname, scatra_disname);


  // new ale part
  DRT::Problem* problem = DRT::Problem::Instance();
  Teuchos::RCP<DRT::Discretization> structdis = problem->GetDis(struct_disname);

  // clone ale from structure for ssi-actin assembly
  // access the ale discretization
  Teuchos::RCP<DRT::Discretization> aledis = Teuchos::null;
  aledis = DRT::Problem::Instance()->GetDis("ale");
  if (!aledis->Filled()) aledis->FillComplete();

  // we use the structure discretization as layout for the ale discretization
  if (structdis->NumGlobalNodes()==0)
    dserror("ERROR: Structure discretization is empty!");

  // clone ale mesh from structure discretization
  if (aledis->NumGlobalNodes()==0)
  {
    DRT::UTILS::CloneDiscretization<ALE::UTILS::AleCloneStrategy>(structdis,aledis);
    // setup material in every ALE element
    Teuchos::ParameterList params;
    params.set<std::string>("action", "setup_material");
    aledis->Evaluate(params);
  }
  else
    dserror("ERROR: Reading an ALE mesh from the input file is not supported for this problem type.");

}


/*----------------------------------------------------------------------*
 | Calculate growth values                                  rauch 01/16 |
 *----------------------------------------------------------------------*/
void SSI::SSI_Part2WC_PROTRUSIONFORMATION::EvaluateGrowth()
{
  // vector management
  const Epetra_BlockMap& map = growthincrement_->Map();
  int nummygrowthvecentries = map.NumMyElements();

  // current step
  int step = StructureField()->Step();

  // value of growth
  double growthinc =   +0.0875013173;                     //negativer Growth -0.00000026445;
  double growthvalue = growthinc * step;

  if (step == 1) // todo itnum_ == 1)
  {

    // apply growth to vector
    for(int i=0; i<nummygrowthvecentries;++i)
    {
      if( i%3==0 )                                                  //Growth in x
        growthincrement_->ReplaceMyValue(i,0,growthvalue);

    }
  }
  else
  {
    for (int i=0; i<nummygrowthvecentries; ++i)
    {
      growthvalue = 0.0;
      growthincrement_->ReplaceMyValue(i,0,growthvalue);
    }
  }

  return;
}
