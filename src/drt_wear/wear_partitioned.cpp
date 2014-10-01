/*----------------------------------------------------------------------*/
/*!
\file wear_partitioned.cpp

\brief  Basis of all structure approaches with ale
        (Lagrangian step followed by Eulerian step )
<pre>
Maintainer: Philipp Farah
            farah@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15257
</pre>
*/

/*----------------------------------------------------------------------*
 | headers                                                  farah 11/13 |
 *----------------------------------------------------------------------*/
#include "wear_partitioned.H"
#include "wear_utils.H"

#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_elementtype.H"

#include "../drt_inpar/drt_validparameters.H"
#include "../drt_inpar/inpar_contact.H"
#include "../drt_inpar/inpar_wear.H"
#include "../drt_inpar/inpar_ale.H"

#include "../drt_w1/wall1.H"
#include "../drt_so3/so_hex8.H"
#include "../drt_so3/so_hex20.H"
#include "../drt_so3/so_hex27.H"
#include "../drt_so3/so_tet4.H"
#include "../drt_so3/so_tet10.H"

#include "../drt_contact/contact_manager.H"
#include "../drt_contact/contact_abstract_strategy.H"
#include "../drt_contact/contact_interface.H"
#include "../drt_contact/contact_node.H"
#include "../drt_contact/contact_defines.H"
#include "../drt_contact/meshtying_manager.H"
#include "../drt_contact/contact_wear_lagrange_strategy.H"
#include "../drt_contact/contact_wear_interface.H"
#include "../drt_contact/contact_integrator.H"
#include "../drt_contact/friction_node.H"
#include "../drt_contact/contact_element.H"
#include "../drt_mortar/mortar_manager_base.H"

#include "../drt_structure/stru_aux.H"

#include <Teuchos_StandardParameterEntryValidators.hpp>
#include "Epetra_SerialComm.h"

#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_sparsematrix.H"
#include "../linalg/linalg_solver.H"

#include "../drt_ale/ale_utils_mapextractor.H"
#include "../drt_ale/ale.H"

#include "../drt_adapter/adapter_coupling.H"

/*----------------------------------------------------------------------*
 | constructor (public)                                     farah 05/13 |
 *----------------------------------------------------------------------*/
WEAR::Partitioned::Partitioned(const Epetra_Comm& comm) :
    Algorithm(comm)
{
  const int ndim = DRT::Problem::Instance()->NDim();

  // create ale-struct coupling
  const Epetra_Map* structdofmap =
      StructureField()->Discretization()->NodeRowMap();
  const Epetra_Map* aledofmap = AleField().Discretization()->NodeRowMap();

  // if there are two identical nodes (i.e. for initial contact) the nodes matching creates an error !!!
  coupalestru_ = Teuchos::rcp(new ADAPTER::Coupling());
  coupalestru_->SetupCoupling(*AleField().Discretization(),
      *StructureField()->Discretization(), *aledofmap, *structdofmap, ndim);

  //create interface coupling
  coupstrualei_ = Teuchos::rcp(new ADAPTER::Coupling());
  coupstrualei_->SetupConditionCoupling(*StructureField()->Discretization(),
      StructureField()->Interface()->AleWearCondMap(),
      *AleField().Discretization(),
      AleField().Interface()->Map(AleField().Interface()->cond_ale_wear),
      "AleWear", ndim);

  // initialize intern variables for wear
  wearnp_i_ = Teuchos::rcp(new Epetra_Vector(
      *AleField().Interface()->Map(AleField().Interface()->cond_ale_wear)), true);
  wearnp_ip_ = Teuchos::rcp(new Epetra_Vector(
          *AleField().Interface()->Map(AleField().Interface()->cond_ale_wear)), true);
  wearincr_ = Teuchos::rcp(new Epetra_Vector(
          *AleField().Interface()->Map(AleField().Interface()->cond_ale_wear)), true);
  delta_ale_ = Teuchos::rcp(new Epetra_Vector(AleField().Dispnp()->Map(), true));

  alepara_ = DRT::Problem::Instance()->AleDynamicParams();
}


/*----------------------------------------------------------------------*
 | destructor (public)                                      farah 11/13 |
 *----------------------------------------------------------------------*/
WEAR::Partitioned::~Partitioned()
{

}


/*----------------------------------------------------------------------*
 | general time loop                                        farah 10/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::TimeLoop()
{
  // get wear paramter list
  const Teuchos::ParameterList& wearpara =
      DRT::Problem::Instance()->WearParams();
  double timeratio = wearpara.get<double>("WEAR_TIMERATIO");

  int counter = -1;
  bool alestep = false;

  // time loop
  while (NotFinished())
  {
    if ((int) (Step() / timeratio) > counter)
    {
      counter++;
      alestep = true;
    }

    if (DRT::INPUT::IntegralValue<INPAR::CONTACT::WearCoupAlgo>(wearpara,
        "WEAR_COUPALGO") == INPAR::CONTACT::wear_stagg)
      TimeLoopStagg(alestep);
    else if (DRT::INPUT::IntegralValue<INPAR::CONTACT::WearCoupAlgo>(wearpara,
        "WEAR_COUPALGO") == INPAR::CONTACT::wear_iterstagg)
      TimeLoopIterStagg();
    else
      dserror("ERROR: WEAR::TimeLoop: Algorithm not provided!");

    alestep = false;
  } // time loop
}


/*----------------------------------------------------------------------*
 | time loop for staggered coupling                         farah 11/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::TimeLoopIterStagg()
{
  // counter and print header
  IncrementTimeAndStep();
  PrintHeader();

  //prepare time step for both fields
  PrepareTimeStep();

  bool converged = false; // converged state?
  bool iterated  = false; // more than 1 iteration?
  int iter = 0; // iteration counter

  // stactic cast of mortar strategy to contact strategy
  MORTAR::StrategyBase& strategy = cmtman_->GetStrategy();
  CONTACT::WearLagrangeStrategy& cstrategy =
      static_cast<CONTACT::WearLagrangeStrategy&>(strategy);

  // reset waccu, wold and wcurr...
  cstrategy.UpdateWearDiscretIterate(false);

  /*************************************************************
   * Nonlinear iterations between Structure and ALE:           *
   * 1. Solve structure + contact to get wear                  *
   * 2. Apply wear increment (i+1 - i) onto ALE (add function) *
   * 3. Employ ALE disp incr (i+1 - i) and spat disp i to get  *
   *    abs mat disp for timestep n+1                          *
   * 4. Upadate spat disp from i to i+1                        *
   * 5. Check for convergence                                  *
   * 6. store ALE disp i = i+1                                 *
   *************************************************************/
  while (converged == false)
  {
    if (iter > 0)
      iterated = true;

    // 1. solution
    StructureField()->Solve();

    // 2. wear as interface displacements in ale dofs
    Teuchos::RCP<Epetra_Vector> idisale_s, idisale_m;
    InterfaceDisp(idisale_s, idisale_m);

    // merge the both wear vectors for master and slave side to one global vector
    MergeWear(idisale_s, idisale_m, wearincr_);

    // coupling of struct/mortar and ale dofs
    DispCoupling(wearincr_);

    // do ale step
    AleStep(wearincr_);

    // 3. application of mesh displacements to structural field,
    // update material displacements
    ApplyMeshDisplacement(iterated);

    // 4. update dispnp
    UpdateDispnp();

    // 5. convergence check fot current iteration
    converged = ConvergenceCheck(iter);

    // store old wear
    cstrategy.UpdateWearDiscretIterate(true);

    ++iter;
  } // end nonlin loop

  // update for structure and ale
  Update();

  // output for structure and ale
  Output();

  return;
}


/*----------------------------------------------------------------------*
 | time loop for oneway coupling                            farah 11/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::TimeLoopStagg(bool alestep)
{
  // stactic cast of mortar strategy to contact strategy
  MORTAR::StrategyBase& strategy = cmtman_->GetStrategy();
  CONTACT::WearLagrangeStrategy& cstrategy =
      static_cast<CONTACT::WearLagrangeStrategy&>(strategy);

  // counter and print header
  IncrementTimeAndStep();
  PrintHeader();

  //prepare time step for both fields
  PrepareTimeStep();

  /********************************************************************/
  /* START LAGRANGE STEP                                              */
  /* structural lagrange step with contact                            */
  /********************************************************************/

  // solution
  StructureField()->Solve();

  if (alestep)
  {
    if (Comm().MyPID() == 0)
      std::cout<< "========================= ALE STEP ========================="<< std::endl;

    /********************************************************************/
    /* COUPLING                                                         */
    /* Wear from structure solve as dirichlet for ALE                   */
    /********************************************************************/

    // wear as interface displacements in ale dofs
    Teuchos::RCP<Epetra_Vector> idisale_s, idisale_m, idisale_global;
    InterfaceDisp(idisale_s, idisale_m);

    // merge the both wear vectors for master and slave side to one global vector
    MergeWear(idisale_s, idisale_m, idisale_global);

    // coupling of struct/mortar and ale dofs
    DispCoupling(idisale_global);

    /********************************************************************/
    /* EULERIAN STEP                                                    */
    /* 1. mesh displacements due to wear from ALE system                */
    /* 2. mapping of results from "old" to "new" mesh                   */
    /********************************************************************/

    // do all step
    AleStep(idisale_global);

    // application of mesh displacements to structural field,
    // update spatial and material displacements
    ApplyMeshDisplacement();

    /********************************************************************/
    /* FINISH STEP:                                                     */
    /* Update and Write Output                                          */
    /********************************************************************/

    // update dispnp
    UpdateDispnp();

    cstrategy.UpdateWearDiscretIterate(false);
  }
  else
  {
    cstrategy.UpdateWearDiscretAccumulation(true);
  }

  // update for structure and ale
  Update();

  // output for structure and ale
  Output();

  return;
}


/*----------------------------------------------------------------------*
 | prepare time step for ale and structure                  farah 11/13 |
 *----------------------------------------------------------------------*/
bool WEAR::Partitioned::ConvergenceCheck(int iter)
{
  double Wincr   = 0.0;
  double ALEincr = 0.0;
  wearincr_->Norm2(&Wincr);
  delta_ale_->Norm2(&ALEincr);

  if (Comm().MyPID() == 0)
  {
    std::cout << "-----------------" << " Step " << iter + 1 << " --------------------" << std::endl;
    std::cout << "Wear incr.= " << Wincr << "         ALE incr.= " << ALEincr           << std::endl;
    std::cout << "---------------------------------------------"                        << std::endl;
  }

  if (abs(Wincr) < 1e-8 and abs(ALEincr) < 1e-8)
    return true;

  if (iter > 50)
    dserror("Staggered solution scheme for ale-wear problem unconverged within 50 nonlinear iteration steps!");

  return false;
}


/*----------------------------------------------------------------------*
 | prepare time step for ale and structure                  farah 11/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::PrepareTimeStep()
{
  // predict and solve structural system
  StructureField()->PrepareTimeStep();

  // prepare ale output: increase time step
  AleField().PrepareTimeStep();

  return;
}


/*----------------------------------------------------------------------*
 | update ale and structure                                 farah 11/13 |
 *---------------------------------------------------------------- ------*/
void WEAR::Partitioned::Update()
{
  // update at time step
  StructureField()->Update();

  // update
  AleField().Update();

  return;
}


/*----------------------------------------------------------------------*
 | update spatial displacements                             farah 11/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::UpdateDispnp()
{
  // mesh displacement from solution of ALE field in structural dofs
  // first perform transformation from ale to structure dofs
  Teuchos::RCP<Epetra_Vector> disalenp = AleToStructure(AleField().Dispnp());
  Teuchos::RCP<Epetra_Vector> disalen = AleToStructure(AleField().Dispn());

  // get structure dispnp vector
  Teuchos::RCP<Epetra_Vector> dispnp = StructureField()->WriteAccessDispnp(); // change to ExtractDispn() for overlap

  int aletype = DRT::INPUT::IntegralValue<int>(ParamsAle(), "ALE_TYPE");

  // for incremental lin ale --> in spatial conf.
  if (aletype == INPAR::ALE::incr_lin)
  {
    // update per absolute vector
    dispnp->Update(1.0, *disalenp, 0.0);
  }
  else if (aletype == INPAR::ALE::classic_lin)
  {
    // create increment between n and np
    disalenp->Update(-1.0, *disalen, 1.0);

    // update per increment
    dispnp->Update(1.0, *disalenp, 1.0);
  }

  return;
}


/*----------------------------------------------------------------------*
 | output ale and structure                                 farah 11/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::Output()
{
  // calculate stresses, strains, energies
  StructureField()->PrepareOutput();

  // write strcture output to screen and files
  StructureField()->Output();

  // output ale
  AleField().Output();

  return;
}


/*----------------------------------------------------------------------*
 | Perform Coupling from struct/mortar to ale dofs          farah 05/13 |
 | This is necessary due to the parallel redistribution                 |
 | of the contact interface                                             |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::DispCoupling(Teuchos::RCP<Epetra_Vector>& disinterface)
{
  //Teuchos::RCP<Epetra_Vector> aledofs = Teuchos::rcp(new Epetra_Vector(*AleField().Interface()->Map(AleField().Interface()->cond_ale_wear)),true);
  Teuchos::RCP<Epetra_Vector> strudofs = Teuchos::rcp(
      new Epetra_Vector(*StructureField()->Interface()->AleWearCondMap()),
      true);

  // change the parallel distribution from mortar interface to structure
  LINALG::Export(*disinterface, *strudofs);

  // perform coupling
  disinterface.reset();
  disinterface = coupstrualei_->MasterToSlave(strudofs);

  return;
}


/*----------------------------------------------------------------------*
 | Merge wear from slave and master surface to one          farah 06/13 |
 | wear vector                                                          |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::MergeWear(Teuchos::RCP<Epetra_Vector>& disinterface_s,
    Teuchos::RCP<Epetra_Vector>& disinterface_m,
    Teuchos::RCP<Epetra_Vector>& disinterface_g)
{
  MORTAR::StrategyBase& strategy = cmtman_->GetStrategy();
  CONTACT::CoAbstractStrategy& cstrategy =
      static_cast<CONTACT::CoAbstractStrategy&>(strategy);
  std::vector<Teuchos::RCP<CONTACT::CoInterface> > interface =
      cstrategy.ContactInterfaces();
  Teuchos::RCP<CONTACT::WearInterface> winterface = Teuchos::rcp_dynamic_cast<
      CONTACT::WearInterface>(interface[0]);
  if (winterface == Teuchos::null)
    dserror("Casting to WearInterface returned null!");

  disinterface_g = Teuchos::rcp(
      new Epetra_Vector(*winterface->Discret().DofRowMap()), true);
  Teuchos::RCP<Epetra_Vector> auxvector = Teuchos::rcp(
      new Epetra_Vector(*winterface->Discret().DofRowMap()), true);

  LINALG::Export(*disinterface_s, *disinterface_g);
  LINALG::Export(*disinterface_m, *auxvector);

  disinterface_g->Update(1.0, *auxvector, true);

  return;
}


/*----------------------------------------------------------------------*
 | Vector of interface displacements in ALE dofs            farah 05/13 |
 | Currently just for 1 interface                                       |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::InterfaceDisp(
    Teuchos::RCP<Epetra_Vector>& disinterface_s,
    Teuchos::RCP<Epetra_Vector>& disinterface_m)
{
  // get info about wear side
  INPAR::CONTACT::WearSide wside = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearSide>(DRT::Problem::Instance()->WearParams(),
      "BOTH_SIDED_WEAR");

  // get info about wear conf
  INPAR::CONTACT::WearConf wconf = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearConf>(DRT::Problem::Instance()->WearParams(),
      "WEARCOEFF_CONF");

  if(interfaces_.size()>1)
    dserror("ERROR: Wear algorithm not able to handle more than 1 interface yet!");

  if(wconf == INPAR::CONTACT::wear_conf_mat)
  {
    // redistribute int. according to spatial interfaces!
    RedistributeMatInterfaces();

    // 1. pull back slave wear to material conf.
    WearPullBackSlave(disinterface_s);

    // 2. pull back master wear to material conf.
    if (wside == INPAR::CONTACT::wear_both_map or
        wside == INPAR::CONTACT::wear_both_discr)
    {
      WearPullBackMaster(disinterface_m);
    }
    else
    {
      Teuchos::RCP<Epetra_Map> masterdofs = interfaces_[0]->MasterRowDofs();
      disinterface_m = Teuchos::rcp(new Epetra_Vector(*masterdofs, true));
    }
  }
  else if(wconf == INPAR::CONTACT::wear_conf_sp)
  {
    // postproc wear for spatial conf.
    WearSpatialSlave(disinterface_s);

    if(wside == INPAR::CONTACT::wear_both_discr)
    {
      WearSpatialMaster(disinterface_m);
    }
    else if (wside == INPAR::CONTACT::wear_both_map)
    {
      // redistribute int. according to spatial interfaces!
      RedistributeMatInterfaces();
      WearSpatialMasterMap(disinterface_s,disinterface_m);
    }
    else
    {
      Teuchos::RCP<Epetra_Map> masterdofs = interfaces_[0]->MasterRowDofs();
      disinterface_m = Teuchos::rcp(new Epetra_Vector(*masterdofs, true));
    }
  }
  else
    dserror("ERROR: Chosen wear configuration not supported!");

  return;
}


/*----------------------------------------------------------------------*
 | Wear in spatial conf.                                    farah 09/14 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::WearSpatialMasterMap(Teuchos::RCP<Epetra_Vector>& disinterface_s,
                                             Teuchos::RCP<Epetra_Vector>& disinterface_m)
{
  if(disinterface_s==Teuchos::null)
    dserror("ERROR: no slave wear for mapping!");

  // stactic cast of mortar strategy to contact strategy
  MORTAR::StrategyBase& strategy = cmtman_->GetStrategy();
  CONTACT::WearLagrangeStrategy& cstrategy =
      static_cast<CONTACT::WearLagrangeStrategy&>(strategy);

  for (int i=0; i<(int)interfaces_.size(); ++i)
  {
    Teuchos::RCP<CONTACT::WearInterface> winterface = Teuchos::rcp_dynamic_cast<
        CONTACT::WearInterface>(interfacesMat_[i]);
    if (winterface == Teuchos::null)
      dserror("ERROR: Casting to WearInterface returned null!");

    Teuchos::RCP<Epetra_Map> masterdofs  = interfaces_[i]->MasterRowDofs();
    Teuchos::RCP<Epetra_Map> slavedofs   = interfaces_[i]->SlaveRowDofs();
    Teuchos::RCP<Epetra_Map> activedofs  = interfaces_[i]->ActiveDofs();

    disinterface_m = Teuchos::rcp(new Epetra_Vector(*masterdofs, true));

    // different wear coefficients on both sides...
    double wearcoeff_s = interfaces_[i]->IParams().get<double>("WEARCOEFF", 0.0);
    double wearcoeff_m = interfaces_[i]->IParams().get<double>("WEARCOEFF_MASTER", 0.0);
    if (wearcoeff_s<1e-12)
      dserror("ERROR: wcoeff negative!!!");

    double fac = wearcoeff_m / (wearcoeff_s);

    Teuchos::RCP<Epetra_Vector> wear_master = Teuchos::rcp(new Epetra_Vector(*masterdofs,true));

    cstrategy.MMatrix()->Multiply(true,*disinterface_s,*wear_master);

    // 1. set state to material displacement state
    winterface->SetState("displacement",StructureField()->WriteAccessDispnp());

    // 2. initialize
    winterface->Initialize();

    // 3. calc N and areas
    winterface->SetElementAreas();
    winterface->EvaluateNodalNormals();

    // 6. init data container for d2 mat
    const Teuchos::RCP<Epetra_Map> masternodesmat =
        LINALG::AllreduceEMap(*(winterface->MasterRowNodes()));

    for (int i=0;i<masternodesmat->NumMyElements();++i) //for (int i=0;i<MasterRowNodes()->NumMyElements();++i)
    {
      int gid = masternodesmat->GID(i);
      DRT::Node* node = winterface->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* cnode = dynamic_cast<CONTACT::FriNode*>(node);

      if (cnode->IsSlave() == false)
      {
        // reset nodal Mortar maps
        for (int j=0;j<(int)((cnode->FriDataPlus().GetD2()).size());++j)
          (cnode->FriDataPlus().GetD2())[j].clear();

        (cnode->FriDataPlus().GetD2()).resize(0);
      }
    }

    // 8. evaluate dmat
    Teuchos::RCP<LINALG::SparseMatrix> dmat  =
        Teuchos::rcp(new LINALG::SparseMatrix(*masterdofs,100,true,false,LINALG::SparseMatrix::FE_MATRIX));

    for (int j=0; j<winterface->MasterColElements()->NumMyElements(); ++j)
    {
      int gid = winterface->MasterColElements()->GID(j);
      DRT::Element* ele = winterface->Discret().gElement(gid);
      if (!ele) dserror("ERROR: Cannot find ele with gid %",gid);
      CONTACT::CoElement* cele = dynamic_cast<CONTACT::CoElement*>(ele);

      Teuchos::RCP<CONTACT::CoIntegrator> integrator =
          Teuchos::rcp(new CONTACT::CoIntegrator(winterface->IParams(),cele->Shape(),Comm()));

      integrator->IntegrateD(*cele,Comm());
    }

    // 10. assemble dmat
    winterface->AssembleD2(*dmat);

    // 12. complete dmat
    dmat->Complete();

    LINALG::Solver solver(Comm());
    solver.Solve(dmat->EpetraMatrix(), disinterface_m, wear_master, true);
    disinterface_m->Scale(-fac);
  }

  return;
}


/*----------------------------------------------------------------------*
 | Wear in spatial conf.                                    farah 09/14 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::WearSpatialMaster(Teuchos::RCP<Epetra_Vector>& disinterface_m)
{
  // get info about wear conf
  INPAR::CONTACT::WearTimeScale wtime = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearTimeScale>(DRT::Problem::Instance()->WearParams(),
      "WEAR_TIMESCALE");

  for (int i=0; i<(int)interfaces_.size(); ++i)
  {
    Teuchos::RCP<Epetra_Map> masterdofs  = interfaces_[i]->MasterRowDofs();
    disinterface_m = Teuchos::rcp(new Epetra_Vector(*masterdofs, true));

    // FIRST: get the wear values and the normal directions for the interface
    // loop over all slave row nodes on the current interface
    for (int j=0; j<interfaces_[i]->MasterRowNodes()->NumMyElements(); ++j)
    {
      int gid = interfaces_[i]->MasterRowNodes()->GID(j);
      DRT::Node* node = interfaces_[i]->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* frinode = dynamic_cast<CONTACT::FriNode*>(node);

      // be aware of problem dimension
      int numdof = frinode->NumDof();
      if (dim_!=numdof)
        dserror("ERROR: Inconsistency Dim <-> NumDof");

      // nodal normal vector and wear
      double nn[3];
      double wear = 0.0;

      for (int j=0;j<3;++j)
        nn[j]=frinode->MoData().n()[j];

      if(wtime == INPAR::CONTACT::wear_time_different)
      {
        if (abs(frinode->FriDataPlus().wcurr()[0] + frinode->FriDataPlus().waccu()[0])>1e-12)
          wear = frinode->FriDataPlus().wcurr()[0]+frinode->FriDataPlus().waccu()[0];
        else
          wear=0.0;
      }
      else
      {
        if (abs(frinode->FriDataPlus().wcurr()[0])>1e-12)
          wear = frinode->FriDataPlus().wcurr()[0];
        else
          wear=0.0;
      }


      // find indices for DOFs of current node in Epetra_Vector
      // and put node values (normal and tangential stress components) at these DOFs
      std::vector<int> locindex(dim_);

      for (int dof=0;dof<dim_;++dof)
      {
         locindex[dof] = (disinterface_m->Map()).LID(frinode->Dofs()[dof]);
        (*disinterface_m)[locindex[dof]] = -wear * nn[dof];
      }
    }
  }
  return;
}


/*----------------------------------------------------------------------*
 | Wear in spatial conf.                                    farah 09/14 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::WearSpatialSlave(Teuchos::RCP<Epetra_Vector>& disinterface_s)
{
  // stactic cast of mortar strategy to contact strategy
  MORTAR::StrategyBase& strategy = cmtman_->GetStrategy();
  CONTACT::WearLagrangeStrategy& cstrategy =
      static_cast<CONTACT::WearLagrangeStrategy&>(strategy);

  INPAR::CONTACT::WearType wtype = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearType>(DRT::Problem::Instance()->WearParams(),
      "WEARTYPE");

  INPAR::CONTACT::WearTimeScale wtime = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearTimeScale>(DRT::Problem::Instance()->WearParams(),
      "WEAR_TIMESCALE");

  if (wtype != INPAR::CONTACT::wear_impl)
    cstrategy.StoreNodalQuantities(MORTAR::StrategyBase::wear);

  for (int i=0; i<(int)interfaces_.size(); ++i)
  {
    Teuchos::RCP<Epetra_Map> slavedofs  = interfaces_[i]->SlaveRowDofs();
    Teuchos::RCP<Epetra_Map> activedofs = interfaces_[i]->ActiveDofs();

    // additional spatial displacements
    disinterface_s = Teuchos::rcp(new Epetra_Vector(*slavedofs, true));

    // FIRST: get the wear values and the normal directions for the interface
    // loop over all slave row nodes on the current interface
    for (int j=0; j<interfaces_[i]->SlaveRowNodes()->NumMyElements(); ++j)
    {
      int gid = interfaces_[i]->SlaveRowNodes()->GID(j);
      DRT::Node* node = interfaces_[i]->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* frinode = dynamic_cast<CONTACT::FriNode*>(node);

      // be aware of problem dimension
      int numdof = frinode->NumDof();
      if (dim_!=numdof)
        dserror("ERROR: Inconsistency Dim <-> NumDof");

      // nodal normal vector and wear
      double nn[3];
      double wear = 0.0;

      for (int j=0;j<3;++j)
        nn[j]=frinode->MoData().n()[j];

      if (wtype == INPAR::CONTACT::wear_discr)
      {
        if(wtime == INPAR::CONTACT::wear_time_different)
        {
          if (abs(frinode->FriDataPlus().wcurr()[0] + frinode->FriDataPlus().waccu()[0])>1e-12)
            wear = frinode->FriDataPlus().wcurr()[0]+frinode->FriDataPlus().waccu()[0];
          else
            wear=0.0;
        }
        else
        {
          if (abs(frinode->FriDataPlus().wcurr()[0])>1e-12)
            wear = frinode->FriDataPlus().wcurr()[0];
          else
            wear=0.0;
        }
      }
      else if (wtype == INPAR::CONTACT::wear_expl or
               wtype == INPAR::CONTACT::wear_impl)
      {
        wear = frinode->FriDataPlus().Wear();
      }

      // find indices for DOFs of current node in Epetra_Vector
      // and put node values (normal and tangential stress components) at these DOFs
      std::vector<int> locindex(dim_);

      for (int dof=0;dof<dim_;++dof)
      {
         locindex[dof] = (disinterface_s->Map()).LID(frinode->Dofs()[dof]);
        (*disinterface_s)[locindex[dof]] = - wear * nn[dof];
      }
    }

    // un-weight for internal state approach
    if (wtype == INPAR::CONTACT::wear_expl or
        wtype == INPAR::CONTACT::wear_impl)
    {
      Teuchos::RCP<LINALG::SparseMatrix> daa,dai,dia,dii;
      Teuchos::RCP<Epetra_Map> gidofs;
      LINALG::SplitMatrix2x2(cstrategy.DMatrix(),activedofs,gidofs,activedofs,gidofs,daa,dai,dia,dii);

      Teuchos::RCP<Epetra_Vector> wear_vectora = Teuchos::rcp(new Epetra_Vector(*activedofs,true));
      Teuchos::RCP<Epetra_Vector> wear_vectori = Teuchos::rcp(new Epetra_Vector(*gidofs));
      LINALG::SplitVector(*slavedofs,*disinterface_s,activedofs,wear_vectora,gidofs,wear_vectori);

      Teuchos::RCP<Epetra_Vector> zref  = Teuchos::rcp(new Epetra_Vector(*activedofs));

      // solve with default solver
      LINALG::Solver solver(Comm());
      if(activedofs->NumMyElements())
        solver.Solve(daa->EpetraMatrix(), zref, wear_vectora, true);

      // different wear coefficients on both sides...
      double wearcoeff_s = interfaces_[0]->IParams().get<double>("WEARCOEFF", 0.0);
      double wearcoeff_m = interfaces_[0]->IParams().get<double>("WEARCOEFF_MASTER", 0.0);
      if (wearcoeff_s<1e-12)
        dserror("wcoeff negative!!!");
      double fac = wearcoeff_s / (wearcoeff_s+wearcoeff_m);
      zref->Scale(fac);

      disinterface_s = Teuchos::rcp(new Epetra_Vector(*slavedofs));
      LINALG::Export(*zref,*disinterface_s);
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 | Redistribute material interfaces acc. to cur interf.     farah 09/14 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::RedistributeMatInterfaces()
{
  // loop over all interfaces
  for (int m = 0; m < (int) interfaces_.size(); ++m)
  {
    if(interfaces_[m]->IsRedistributed())
    {
      if (Comm().MyPID() == 0)
      {
        std::cout << "===========================================" << std::endl;
        std::cout << "=======    Redistribute Mat. Int.   =======" << std::endl;
        std::cout << "===========================================" << std::endl;
      }
      Teuchos::RCP<CONTACT::WearInterface> winterface = Teuchos::rcp_dynamic_cast<
          CONTACT::WearInterface>(interfacesMat_[m]);

      // export nodes and elements to the row map
      winterface->Discret().ExportRowNodes(*interfaces_[m]->Discret().NodeRowMap());
      winterface->Discret().ExportRowElements(*interfaces_[m]->Discret().ElementRowMap());

      // export nodes and elements to the column map (create ghosting)
      winterface->Discret().ExportColumnNodes(*interfaces_[m]->Discret().NodeColMap());
      winterface->Discret().ExportColumnElements(*interfaces_[m]->Discret().ElementColMap());

      winterface->FillComplete();
      winterface->PrintParallelDistribution(m);

      if (Comm().MyPID() == 0)
      {
        std::cout << "===========================================" << std::endl;
        std::cout << "==============     Done!     ==============" << std::endl;
        std::cout << "===========================================" << std::endl;
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 | Pull-Back wear: W = w * ds/dS * N                        farah 09/14 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::WearPullBackSlave(Teuchos::RCP<Epetra_Vector>& disinterface_s)
{
  // stactic cast of mortar strategy to contact strategy
  MORTAR::StrategyBase& strategy = cmtman_->GetStrategy();
  CONTACT::WearLagrangeStrategy& cstrategy =
      dynamic_cast<CONTACT::WearLagrangeStrategy&>(strategy);

  INPAR::CONTACT::WearType wtype = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearType>(DRT::Problem::Instance()->WearParams(),
      "WEARTYPE");

  INPAR::CONTACT::WearTimeScale wtime = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearTimeScale>(DRT::Problem::Instance()->WearParams(),
      "WEAR_TIMESCALE");

  if (wtype != INPAR::CONTACT::wear_impl)
    cstrategy.StoreNodalQuantities(MORTAR::StrategyBase::wear);

  // loop over all interfaces
  for (int m = 0; m < (int) interfaces_.size(); ++m)
  {
    Teuchos::RCP<CONTACT::WearInterface> winterface = Teuchos::rcp_dynamic_cast<
        CONTACT::WearInterface>(interfaces_[m]);
    if (winterface == Teuchos::null)
      dserror("ERROR: Casting to WearInterface returned null!");

    // get slave row dofs as map
    Teuchos::RCP<Epetra_Map> slavedofs = winterface->SlaveRowDofs();
    // additional spatial displacements
    disinterface_s = Teuchos::rcp(new Epetra_Vector(*slavedofs, true));

    // call material interfaces and evaluate!
    // 1. set state to material displacement state
    interfacesMat_[m]->SetState("displacement",StructureField()->DispMat());

    // 2. initialize
    interfacesMat_[m]->Initialize();

    // 3. calc N and areas
    interfacesMat_[m]->SetElementAreas();
    interfacesMat_[m]->EvaluateNodalNormals();

    // 4. calc -w*N
    for (int j=0; j<winterface->SlaveRowNodes()->NumMyElements(); ++j)
    {
      int gid = winterface->SlaveRowNodes()->GID(j);
      DRT::Node* node = winterface->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* frinode = dynamic_cast<CONTACT::FriNode*>(node);

      int gidm = interfacesMat_[m]->SlaveRowNodes()->GID(j);
      DRT::Node* nodem = interfacesMat_[m]->Discret().gNode(gidm);
      if (!nodem) dserror("ERROR: Cannot find node with gid %",gidm);
      CONTACT::FriNode* frinodem = dynamic_cast<CONTACT::FriNode*>(nodem);

      // be aware of problem dimension
      int numdof = frinode->NumDof();
      if (dim_!=numdof)
        dserror("ERROR: Inconsistency Dim <-> NumDof");

      // nodal normal vector and wear
      double nn[3];
      double wear = 0.0;

      // get material normal
      for (int j=0;j<3;++j)
        nn[j]=frinodem->MoData().n()[j];

      if (wtype == INPAR::CONTACT::wear_discr)
      {
        if(wtime == INPAR::CONTACT::wear_time_different)
        {
          if (abs(frinode->FriDataPlus().wcurr()[0] + frinode->FriDataPlus().waccu()[0])>1e-12)
            wear = frinode->FriDataPlus().wcurr()[0]+frinode->FriDataPlus().waccu()[0];
          else
            wear=0.0;
        }
        else
        {
          if (abs(frinode->FriDataPlus().wcurr()[0])>1e-12)
            wear = frinode->FriDataPlus().wcurr()[0];
          else
            wear=0.0;
        }
      }
      else if (wtype == INPAR::CONTACT::wear_expl or
               wtype == INPAR::CONTACT::wear_impl)
      {
        wear = frinode->FriDataPlus().Wear();
      }

      // find indices for DOFs of current node in Epetra_Vector
      // and put node values (normal and tangential stress components) at these DOFs
      std::vector<int> locindex(dim_);

      for (int dof=0;dof<dim_;++dof)
      {
         locindex[dof] = (disinterface_s->Map()).LID(frinode->Dofs()[dof]);
        (*disinterface_s)[locindex[dof]] = -wear * nn[dof];
      }
    }

    // 5. evaluate dmat
    Teuchos::RCP<LINALG::SparseMatrix> dmat  = Teuchos::rcp(new LINALG::SparseMatrix(*slavedofs, 10));
    Teuchos::RCP<LINALG::SparseMatrix> dummy = Teuchos::rcp(new LINALG::SparseMatrix(*slavedofs, 10));

    for (int j=0; j<interfacesMat_[m]->SlaveColElements()->NumMyElements(); ++j)
    {
      int gid = interfacesMat_[m]->SlaveColElements()->GID(j);
      DRT::Element* ele = interfacesMat_[m]->Discret().gElement(gid);
      if (!ele) dserror("ERROR: Cannot find ele with gid %",gid);
      CONTACT::CoElement* cele = dynamic_cast<CONTACT::CoElement*>(ele);

      Teuchos::RCP<CONTACT::CoIntegrator> integrator =
          Teuchos::rcp(new CONTACT::CoIntegrator(interfacesMat_[m]->IParams(),cele->Shape(),Comm()));

      integrator->IntegrateD(*cele,Comm());
    }

    // 6. assemble dmat
    interfacesMat_[m]->AssembleDM(*dmat,*dummy,true);

    // 7. complete dmat
    dmat->Complete();

    // 8. area trafo:
    if (wtype == INPAR::CONTACT::wear_discr)
    {
      // multiply current D matrix with current wear
      Teuchos::RCP<Epetra_Vector> forcecurr = Teuchos::rcp(new Epetra_Vector(*slavedofs));
      cstrategy.DMatrix()->Multiply(false, *disinterface_s, *forcecurr);

      // LM in reference / current configuration
      Teuchos::RCP<Epetra_Vector> zref  = Teuchos::rcp(new Epetra_Vector(*slavedofs));

      // solve with default solver
      LINALG::Solver solver(Comm());
      solver.Solve(dmat->EpetraOperator(), zref, forcecurr, true);

      // store reference LM into global vector and nodes
      disinterface_s = zref;
    }
    else if (wtype == INPAR::CONTACT::wear_expl or
             wtype == INPAR::CONTACT::wear_impl)
    {
      Teuchos::RCP<Epetra_Vector> zref  = Teuchos::rcp(new Epetra_Vector(*slavedofs));

      // solve with default solver
      LINALG::Solver solver(Comm());
      solver.Solve(dmat->EpetraOperator(), zref, disinterface_s, true);

      // store reference LM into global vector and nodes
      disinterface_s = zref;

      // different wear coefficients on both sides...
      double wearcoeff_s = interfaces_[0]->IParams().get<double>("WEARCOEFF", 0.0);
      double wearcoeff_m = interfaces_[0]->IParams().get<double>("WEARCOEFF_MASTER", 0.0);
      if (wearcoeff_s<1e-12)
        dserror("ERROR: wcoeff negative!!!");

      double fac = wearcoeff_s / (wearcoeff_s+wearcoeff_m);
      disinterface_s->Scale(fac);
    }
    else
      dserror("ERROR: wrong wear type!");
  }
  return;
}


/*----------------------------------------------------------------------*
 | Pull-Back wear: W = w * ds/dS * N                        farah 09/14 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::WearPullBackMaster(Teuchos::RCP<Epetra_Vector>& disinterface_m)
{
  INPAR::CONTACT::WearType wtype = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearType>(DRT::Problem::Instance()->WearParams(),
      "WEARTYPE");

  INPAR::CONTACT::WearTimeScale wtime = DRT::INPUT::IntegralValue<
      INPAR::CONTACT::WearTimeScale>(DRT::Problem::Instance()->WearParams(),
      "WEAR_TIMESCALE");

  // loop over all interfaces
  for (int m = 0; m < (int) interfaces_.size(); ++m)
  {
    Teuchos::RCP<CONTACT::WearInterface> winterface = Teuchos::rcp_dynamic_cast<
        CONTACT::WearInterface>(interfaces_[m]);
    if (winterface == Teuchos::null)
      dserror("ERROR: Casting to WearInterface returned null!");

    Teuchos::RCP<CONTACT::WearInterface> winterfaceMat = Teuchos::rcp_dynamic_cast<
        CONTACT::WearInterface>(interfacesMat_[m]);
    if (winterfaceMat == Teuchos::null)
      dserror("ERROR: Casting to WearInterface returned null!");

    // get slave row dofs as map
    Teuchos::RCP<Epetra_Map> masterdofs = winterface->MasterRowDofs();
    // additional spatial displacements
    disinterface_m = Teuchos::rcp(new Epetra_Vector(*masterdofs, true));

    // call material interfaces and evaluate!
    // 1. set state to material displacement state
    winterfaceMat->SetState("displacement",StructureField()->DispMat());

    // 2. initialize
    winterfaceMat->Initialize();

    // 3. calc N and areas
    winterfaceMat->SetElementAreas();
    winterfaceMat->EvaluateNodalNormals();

    // 4. calc -w*N
    for (int j=0; j<winterface->MasterRowNodes()->NumMyElements(); ++j)
    {
      int gid = winterface->MasterRowNodes()->GID(j);
      DRT::Node* node = winterface->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* frinode = dynamic_cast<CONTACT::FriNode*>(node);

      int gidm = interfacesMat_[m]->MasterRowNodes()->GID(j);
      DRT::Node* nodem = interfacesMat_[m]->Discret().gNode(gidm);
      if (!nodem) dserror("ERROR: Cannot find node with gid %",gidm);
      CONTACT::FriNode* frinodem = dynamic_cast<CONTACT::FriNode*>(nodem);

      // be aware of problem dimension
      int numdof = frinode->NumDof();
      if (dim_!=numdof)
        dserror("ERROR: Inconsistency Dim <-> NumDof");

      // nodal normal vector and wear
      double nn[3];
      double wear = 0.0;

      // get material normal
      for (int j=0;j<3;++j)
        nn[j]=frinodem->MoData().n()[j];

      if (wtype == INPAR::CONTACT::wear_discr)
      {
        if(wtime == INPAR::CONTACT::wear_time_different)
        {
          if (abs(frinode->FriDataPlus().wcurr()[0] + frinode->FriDataPlus().waccu()[0])>1e-12)
            wear = frinode->FriDataPlus().wcurr()[0]+frinode->FriDataPlus().waccu()[0];
          else
            wear=0.0;
        }
        else
        {
          if (abs(frinode->FriDataPlus().wcurr()[0])>1e-12)
            wear = frinode->FriDataPlus().wcurr()[0];
          else
            wear=0.0;
        }
      }
      else if (wtype == INPAR::CONTACT::wear_expl)
      {
        wear = frinode->FriDataPlus().Wear();
      }

      // find indices for DOFs of current node in Epetra_Vector
      // and put node values (normal and tangential stress components) at these DOFs
      std::vector<int> locindex(dim_);

      for (int dof=0;dof<dim_;++dof)
      {
         locindex[dof] = (disinterface_m->Map()).LID(frinode->Dofs()[dof]);
        (*disinterface_m)[locindex[dof]] = -wear * nn[dof];
      }
    }

    // 5. init data container for d2 curr
    const Teuchos::RCP<Epetra_Map> masternodes =
        LINALG::AllreduceEMap(*(winterface->MasterRowNodes()));

    for (int i=0;i<masternodes->NumMyElements();++i) //for (int i=0;i<MasterRowNodes()->NumMyElements();++i)
    {
      int gid = masternodes->GID(i);
      DRT::Node* node = winterface->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* cnode = dynamic_cast<CONTACT::FriNode*>(node);

      if (cnode->IsSlave() == false)
      {
        // reset nodal Mortar maps
        for (int j=0;j<(int)((cnode->FriDataPlus().GetD2()).size());++j)
          (cnode->FriDataPlus().GetD2())[j].clear();

        (cnode->FriDataPlus().GetD2()).resize(0);
      }
    }

    // 6. init data container for d2 mat
    const Teuchos::RCP<Epetra_Map> masternodesmat =
        LINALG::AllreduceEMap(*(winterfaceMat->MasterRowNodes()));

    for (int i=0;i<masternodesmat->NumMyElements();++i) //for (int i=0;i<MasterRowNodes()->NumMyElements();++i)
    {
      int gid = masternodesmat->GID(i);
      DRT::Node* node = winterfaceMat->Discret().gNode(gid);
      if (!node) dserror("ERROR: Cannot find node with gid %",gid);
      CONTACT::FriNode* cnode = dynamic_cast<CONTACT::FriNode*>(node);

      if (cnode->IsSlave() == false)
      {
        // reset nodal Mortar maps
        for (int j=0;j<(int)((cnode->FriDataPlus().GetD2()).size());++j)
          (cnode->FriDataPlus().GetD2())[j].clear();

        (cnode->FriDataPlus().GetD2()).resize(0);
      }
    }

    // 7. evaluate dcur
    Teuchos::RCP<LINALG::SparseMatrix> dcur  =
        Teuchos::rcp(new LINALG::SparseMatrix(*masterdofs,100,true,false,LINALG::SparseMatrix::FE_MATRIX));
    for (int j=0; j<winterface->MasterColElements()->NumMyElements(); ++j)
    {
      int gid = winterface->MasterColElements()->GID(j);
      DRT::Element* ele = winterface->Discret().gElement(gid);
      if (!ele) dserror("ERROR: Cannot find ele with gid %",gid);
      CONTACT::CoElement* cele = dynamic_cast<CONTACT::CoElement*>(ele);

      Teuchos::RCP<CONTACT::CoIntegrator> integrator =
          Teuchos::rcp(new CONTACT::CoIntegrator(winterface->IParams(),cele->Shape(),Comm()));

      integrator->IntegrateD(*cele,Comm());
    }

    // 8. evaluate dmat
    Teuchos::RCP<LINALG::SparseMatrix> dmat  =
        Teuchos::rcp(new LINALG::SparseMatrix(*masterdofs,100,true,false,LINALG::SparseMatrix::FE_MATRIX));

    for (int j=0; j<winterfaceMat->MasterColElements()->NumMyElements(); ++j)
    {
      int gid = winterfaceMat->MasterColElements()->GID(j);
      DRT::Element* ele = winterfaceMat->Discret().gElement(gid);
      if (!ele) dserror("ERROR: Cannot find ele with gid %",gid);
      CONTACT::CoElement* cele = dynamic_cast<CONTACT::CoElement*>(ele);

      Teuchos::RCP<CONTACT::CoIntegrator> integrator =
          Teuchos::rcp(new CONTACT::CoIntegrator(winterfaceMat->IParams(),cele->Shape(),Comm()));

      integrator->IntegrateD(*cele,Comm());
    }

    // 9. assemble dcur
    winterface->AssembleD2(*dcur);

    // 10. assemble dmat
    winterfaceMat->AssembleD2(*dmat);

    // 11. complete dcur
    dcur->Complete();

    // 12. complete dmat
    dmat->Complete();

    // 13. area trafo:
    if (wtype == INPAR::CONTACT::wear_discr)
    {
      // multiply current D matrix with current wear
      Teuchos::RCP<Epetra_Vector> forcecurr = Teuchos::rcp(new Epetra_Vector(*masterdofs));
      dcur->Multiply(false, *disinterface_m, *forcecurr);

      // LM in reference / current configuration
      Teuchos::RCP<Epetra_Vector> zref  = Teuchos::rcp(new Epetra_Vector(*masterdofs));

      // solve with default solver
      LINALG::Solver solver(Comm());
      solver.Solve(dmat->EpetraOperator(), zref, forcecurr, true);

      // store reference LM into global vector and nodes
      disinterface_m = zref;
    }
    else if (wtype == INPAR::CONTACT::wear_expl or
             wtype == INPAR::CONTACT::wear_impl)
    {
      dserror("ERROR: not working yet!");
      Teuchos::RCP<Epetra_Vector> zref  = Teuchos::rcp(new Epetra_Vector(*masterdofs));

      // solve with default solver
      LINALG::Solver solver(Comm());
      solver.Solve(dmat->EpetraOperator(), zref, disinterface_m, true);

      // store reference LM into global vector and nodes
      disinterface_m = zref;
    }
    else
      dserror("ERROR: wrong wear type!");
  }
  return;
}


/*----------------------------------------------------------------------*
 | Application of mesh displacement                           mgit 07/11|
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::ApplyMeshDisplacement(bool iterated)
{
  // get problem dimension
  const int ndim = DRT::Problem::Instance()->NDim();

  // mesh displacement from solution of ALE field in structural dofs
  // first perform transformation from ale to structure dofs
  Teuchos::RCP<Epetra_Vector> disale  = AleToStructure(AleField().Dispnp());
  Teuchos::RCP<Epetra_Vector> disalen = AleToStructure(AleField().Dispn());

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

  int aletype = DRT::INPUT::IntegralValue<int>(ParamsAle(), "ALE_TYPE");

  // if classic lin: ale dispnp = material displ.
  if(aletype == INPAR::ALE::classic_lin)
  {
    dismat->Update(1.0, *disale, 0.0);
  }
  // if incr lin: advection map!
  else if (aletype == INPAR::ALE::incr_lin)
  {
    disale->Update(-1.0, *dispnp, 1.0);
    delta_ale_->Update(1.0, *disale, 0.0);

    // loop over all row nodes to fill graph
    for (int k = 0; k < StructureField()->Discretization()->NumMyRowNodes(); ++k)
    {
      int gid = StructureField()->Discretization()->NodeRowMap()->GID(k);

      DRT::Node* node = StructureField()->Discretization()->gNode(gid);
      DRT::Element** ElementPtr = node->Elements();

      int numelement = node->NumElement();

      // create Xmat for 3D problems
      double XMat[3];
      double XMesh[3];

      // local ID
      int locid = 0;

      XMat[0] = node->X()[0];
      XMat[1] = node->X()[1];
      if (ndim == 3)
        XMat[2] = node->X()[2];

      // get local id
      if (ndim == 2)
      {
        locid = (dispnp->Map()).LID(2 * gid);
        if (locid == -1)
          dserror("ERROR: LID not found on this proc");
      }
      else
      {
        locid = (dispnp->Map()).LID(3 * gid);
        if (locid == -1)
          dserror("ERROR: LID not found on this proc");
      }
      // reference node position + displacement t_n + delta displacement t_n+1
      XMesh[0] = node->X()[0] + (*dispnp)[locid]     + (*disale)[locid];
      XMesh[1] = node->X()[1] + (*dispnp)[locid + 1] + (*disale)[locid + 1];
      if (ndim == 3)
        XMesh[2] = node->X()[2] + (*dispnp)[locid + 2] + (*disale)[locid + 2];

      // create updated  XMat --> via nonlinear interpolation between nodes (like gp projection)
      AdvectionMap(XMat, XMesh, ElementPtr, numelement);

      // create delta displacement in material configuration
      (*dismat)[locid]     = XMat[0] - node->X()[0];
      (*dismat)[locid + 1] = XMat[1] - node->X()[1];
      if (ndim == 3)
        (*dismat)[locid + 2] = XMat[2] - node->X()[2];
    } // end row node loop
  }

  // apply material displacements to structural field
  // if advection map is not succesful --> use old xmat
  StructureField()->ApplyDisMat(dismat);

  return;
}


/*----------------------------------------------------------------------*
 | material coordinates evaluated from spatial ones         farah 12/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::AdvectionMap(double* XMat, double* XMesh,
    DRT::Element** ElementPtr, int numelements)
{
  // get problem dimension
  const int ndim = DRT::Problem::Instance()->NDim();

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
    // get element location vector
    DRT::Element::LocationArray la(1);
    actele->LocationVector(*(StructureField()->Discretization()), la, false);

    // get state
    Teuchos::RCP<const Epetra_Vector> disp =
        (StructureField()->Discretization())->GetState("displacement");
    Teuchos::RCP<const Epetra_Vector> dispmat =
        (StructureField()->Discretization())->GetState("material_displacement");

    if (ndim == 2)
    {
      if (actele->Shape() == DRT::Element::quad4)
        WEAR::UTILS::av<DRT::Element::quad4>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::quad8)
        WEAR::UTILS::av<DRT::Element::quad8>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::quad9)
        WEAR::UTILS::av<DRT::Element::quad9>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::tri3)
        WEAR::UTILS::av<DRT::Element::tri3>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->Shape() == DRT::Element::tri6)
        WEAR::UTILS::av<DRT::Element::tri6>(actele, XMat, XMesh, disp, dispmat,
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
      if (actele->ElementType() == DRT::ELEMENTS::So_hex8Type::Instance())
        WEAR::UTILS::av<DRT::Element::hex8>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_hex20Type::Instance())
        WEAR::UTILS::av<DRT::Element::hex20>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_hex27Type::Instance())
        WEAR::UTILS::av<DRT::Element::hex27>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_tet4Type::Instance())
        WEAR::UTILS::av<DRT::Element::tet4>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else if (actele->ElementType() == DRT::ELEMENTS::So_tet10Type::Instance())
        WEAR::UTILS::av<DRT::Element::tet10>(actele, XMat, XMesh, disp, dispmat,
            la[0].lm_, found, e);
      else
        dserror("ERROR: element type not supported!");

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
  // get element location vector
  DRT::Element::LocationArray la(1);
  actele->LocationVector(*(StructureField()->Discretization()), la, false);

  // get state
  Teuchos::RCP<const Epetra_Vector> disp =
      (StructureField()->Discretization())->GetState("displacement");
  Teuchos::RCP<const Epetra_Vector> dispmat =
      (StructureField()->Discretization())->GetState("material_displacement");

  if (ndim == 2)
  {
    if (actele->Shape() == DRT::Element::quad4)
      WEAR::UTILS::av<DRT::Element::quad4>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::quad8)
      WEAR::UTILS::av<DRT::Element::quad8>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::quad9)
      WEAR::UTILS::av<DRT::Element::quad9>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::tri3)
      WEAR::UTILS::av<DRT::Element::tri3>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->Shape() == DRT::Element::tri6)
      WEAR::UTILS::av<DRT::Element::tri6>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else
      dserror("ERROR: shape function not supported!");
  }
  else
  {
    if (actele->ElementType() == DRT::ELEMENTS::So_hex8Type::Instance())
      WEAR::UTILS::av<DRT::Element::hex8>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_hex20Type::Instance())
      WEAR::UTILS::av<DRT::Element::hex20>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_hex27Type::Instance())
      WEAR::UTILS::av<DRT::Element::hex27>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_tet4Type::Instance())
      WEAR::UTILS::av<DRT::Element::tet4>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else if (actele->ElementType() == DRT::ELEMENTS::So_tet10Type::Instance())
      WEAR::UTILS::av<DRT::Element::tet10>(actele, XMat, XMesh, disp, dispmat,
          la[0].lm_, found, e);
    else
      dserror("ERROR: element type not supported!");
  }

  // bye
  return;

}


/*----------------------------------------------------------------------*
 | Perform ALE step                                         farah 11/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::AleStep(Teuchos::RCP<Epetra_Vector> idisale_global)
{
  int aletype = DRT::INPUT::IntegralValue<int>(ParamsAle(), "ALE_TYPE");

  // for incremental lin ale --> in spatial conf.
  if (aletype == INPAR::ALE::incr_lin)
  {
    // system of equation
    AleField().BuildSystemMatrix();

    Teuchos::RCP<Epetra_Vector> dispnpstru = StructureToAle(
        StructureField()->Dispnp());
    AleField().WriteAccessDispnp()->Update(1.0, *(dispnpstru), 0.0);

    // application of interface displacements as dirichlet conditions
    AleField().ApplyInterfaceDisplacements(idisale_global);

    // solution
    AleField().SolveWear();
  }
  // classical lin in mat. conf --> not correct at all
  else if (aletype == INPAR::ALE::classic_lin)
  {
    // system of equation
    AleField().BuildSystemMatrix();

    // application of interface displacements as dirichlet conditions
    AleField().ApplyInterfaceDisplacements(idisale_global);

    // solution
    AleField().Solve();
  }
  else
    dserror("ERROR: Chosen ALE type not supported for wear problems");

  return;
}


/*----------------------------------------------------------------------*
 | transform from ale to structure map                      farah 11/13 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> WEAR::Partitioned::AleToStructure(
    Teuchos::RCP<Epetra_Vector> vec) const
{
  return coupalestru_->MasterToSlave(vec);
}


/*----------------------------------------------------------------------*
 | transform from ale to structure map                      farah 11/13 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> WEAR::Partitioned::AleToStructure(
    Teuchos::RCP<const Epetra_Vector> vec) const
{
  return coupalestru_->MasterToSlave(vec);
}


/*----------------------------------------------------------------------*
 | transform from ale to structure map                      farah 11/13 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> WEAR::Partitioned::StructureToAle(
    Teuchos::RCP<Epetra_Vector> vec) const
{
  return coupalestru_->SlaveToMaster(vec);
}


/*----------------------------------------------------------------------*
 | transform from ale to structure map                      farah 11/13 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> WEAR::Partitioned::StructureToAle(
    Teuchos::RCP<const Epetra_Vector> vec) const
{
  return coupalestru_->SlaveToMaster(vec);
}


/*----------------------------------------------------------------------*
 | read restart information for given time step (public)    farah 10/13 |
 *----------------------------------------------------------------------*/
void WEAR::Partitioned::ReadRestart(int step)
{
  StructureField()->ReadRestart(step);
  AleField().ReadRestart(step);
  SetTimeStep(StructureField()->TimeOld(), step);

  return;
}
/*----------------------------------------------------------------------*/
