/*!----------------------------------------------------------------------
\file drt_discret_hdg.cpp

\brief Implementation a class to manage an enhanced discretization including all faces for HDG


</pre>

<pre>
\brief Implementation
\level 2
\maintainer Martin Kronbichler
            http://www.lnm.mw.tum.de
            089 - 289-15235
</pre>

*----------------------------------------------------------------------*/

#include "drt_discret_hdg.H"

#include "drt_exporter.H"
#include "drt_globalproblem.H"
#include "../drt_fem_general/drt_utils_local_connectivity_matrices.H"
#include "../drt_fluid_ele/fluid_ele_action.H"
#include "../drt_acou/acou_ele.H"
#include "../drt_acou/acou_ele_action.H"

#include "../linalg/linalg_utils.H"


DRT::DiscretizationHDG::DiscretizationHDG(const std::string name,
                                          Teuchos::RCP<Epetra_Comm> comm)
  :
  DiscretizationFaces(name, comm)
{
  this->doboundaryfaces_ = true;
}

/*----------------------------------------------------------------------*
 |  Finalize construction (public)                     kronbichler 12/13|
 *----------------------------------------------------------------------*/
int DRT::DiscretizationHDG::FillComplete(bool assigndegreesoffreedom,
                                         bool initelements,
                                         bool doboundaryconditions)

{
  // call FillComleteFaces of base class with create_faces set to true
  this->FillCompleteFaces(assigndegreesoffreedom, initelements,
                          doboundaryconditions, true);

  // get the correct face orientation from the owner. since the elements in general do not allow
  // packing, extract the node ids, communicate them, and change the node ids in the element
  Exporter nodeexporter( *facerowmap_, *facecolmap_, Comm() );
  std::map<int, std::vector<int> > nodeIds, trafoMap;
  for (std::map<int, Teuchos::RCP<DRT::FaceElement> >::const_iterator f=faces_.begin();
       f!= faces_.end(); ++f)
  {
    std::vector<int> ids(f->second->NumNode());
    for (int i=0; i<f->second->NumNode(); ++i)
      ids[i] = f->second->NodeIds()[i];
    nodeIds[f->first] = ids;
    trafoMap[f->first] = f->second->GetLocalTrafoMap();
  }

  nodeexporter.Export( nodeIds );
  nodeexporter.Export( trafoMap );

  for (std::map<int, Teuchos::RCP<DRT::FaceElement> >::iterator f=faces_.begin();
       f!= faces_.end(); ++f)
  {
    if ( f->second->Owner() == Comm().MyPID() )
      continue;
    std::vector<int> &ids = nodeIds[f->first];
    dsassert(ids.size() > 0, "Lost a face during communication");
    f->second->SetNodeIds(ids.size(), &ids[0]);
    f->second->SetLocalTrafoMap(trafoMap[f->first]);

    // refresh node pointers if they have been set up
    DRT::Node** oldnodes = f->second->Nodes();
    if (oldnodes != 0)
    {
      std::vector<DRT::Node*> nodes(ids.size(), 0);

      for (unsigned int i=0; i<ids.size(); ++i) {
        for (unsigned int j=0; j<ids.size(); ++j)
          if (oldnodes[j]->Id() == ids[i]) {
            nodes[i] = oldnodes[j];
          }
        dsassert(nodes[i] != 0, "Could not find node.");
      }
      f->second->BuildNodalPointers(&nodes[0]);
    }

    // check master/slave relation of current face in terms of the local trafo map
    dsassert(f->second->ParentMasterElement() != NULL, "Unexpected topology between face and parent");
    const int * nodeIdsMaster = f->second->ParentMasterElement()->NodeIds();
    const int * nodeIds = f->second->NodeIds();

    std::vector<std::vector<int> > faceNodeOrder =
        DRT::UTILS::getEleNodeNumberingFaces(f->second->ParentMasterElement()->Shape());

    bool exchangeMasterAndSlave = false;
    for (int i=0; i<f->second->NumNode(); ++i)
    {
      // TODO (MK): check that this is enough also on periodic B.C. where the
      // node ids are different in any case...
      if (nodeIdsMaster[faceNodeOrder[f->second->FaceMasterNumber()][i]] != nodeIds[i])
        exchangeMasterAndSlave = true;
    }
    if (exchangeMasterAndSlave)
    {
      DRT::Element* faceMaster = f->second->ParentMasterElement();
      const int faceMasterNo = f->second->FaceMasterNumber();
      // new master element might be NULL on MPI computations
      f->second->SetParentMasterElement(f->second->ParentSlaveElement(),
                                        f->second->ParentSlaveElement() != NULL ?
                                        f->second->FaceSlaveNumber() : -1);
      f->second->SetParentSlaveElement(faceMaster, faceMasterNo);
    }
  }

  return 0;
}



void DRT::DiscretizationHDG::DoDirichletCondition(DRT::Condition&             cond,
                                                  const bool                  usetime,
                                                  const double                time,
                                                  Teuchos::RCP<Epetra_Vector> systemvector,
                                                  Teuchos::RCP<Epetra_Vector> systemvectord,
                                                  Teuchos::RCP<Epetra_Vector> systemvectordd,
                                                  Teuchos::RCP<Epetra_Vector> toggle,
                                                  Teuchos::RCP<std::set<int> > dbcgids)
{
  Discretization::DoDirichletCondition(cond, usetime,time,systemvector,systemvectord,systemvectordd,toggle,dbcgids);
  if (FaceRowMap() == NULL)
    return;

  const std::vector<int>* nodeids = cond.Nodes();
  if (!nodeids) dserror("Dirichlet condition does not have nodal cloud");
  const std::vector<int>*    curve  = cond.Get<std::vector<int> >("curve");
  const std::vector<int>*    funct  = cond.Get<std::vector<int> >("funct");
  const std::vector<int>*    onoff  = cond.Get<std::vector<int> >("onoff");
  const std::vector<double>* val    = cond.Get<std::vector<double> >("val");

  // determine highest degree of time derivative
  // and first existent system vector to apply DBC to
  unsigned deg = 0;  // highest degree of requested time derivative
  Teuchos::RCP<Epetra_Vector> systemvectoraux = Teuchos::null;  // auxiliar system vector
  if (systemvector != Teuchos::null)
  {
    deg = 0;
    systemvectoraux = systemvector;
  }
  if (systemvectord != Teuchos::null)
  {
    deg = 1;
    if (systemvectoraux == Teuchos::null)
      systemvectoraux = systemvectord;
  }
  if (systemvectordd != Teuchos::null)
  {
    deg = 2;
    if (systemvectoraux == Teuchos::null)
      systemvectoraux = systemvectordd;
  }
  dsassert(systemvectoraux!=Teuchos::null, "At least one vector must be unequal to null");

  // factor given by time curve
  std::vector<std::vector<double> > curvefacs(onoff->size());
  for (unsigned int j=0; j<onoff->size(); ++j)
  {
    int curvenum = -1;
    if (curve) curvenum = (*curve)[j];
    if (curvenum>=0 && usetime)
      curvefacs[j] = DRT::Problem::Instance()->Curve(curvenum).FctDer(time,deg);
    else
    {
      curvefacs[j].resize(deg+1, 1.0);
      for (unsigned i=1; i<(deg+1); ++i) curvefacs[j][i] = 0.0;
    }
  }

  if (NumMyRowFaces() > 0)
  {
    Epetra_SerialDenseVector elevec1, elevec2, elevec3;
    Epetra_SerialDenseMatrix elemat1, elemat2;
    std::vector<int> dummy;
    Teuchos::ParameterList initParams;
    if(DRT::Problem::Instance(0)->ProblemType()==prb_acou)
      initParams.set<int>("action", ACOU::project_dirich_field);
    else
      initParams.set<int>("action", FLD::project_fluid_field); // TODO: Introduce a general action type that is valid for all problems
    if (funct != NULL) {
      Teuchos::Array<int> functarray(*funct);
      initParams.set("funct",functarray);
    }
    Teuchos::Array<int> onoffarray(*onoff);
    initParams.set("onoff",onoffarray);
    initParams.set("time",time);

    bool pressureDone = this->Comm().MyPID() != 0;

    for (int i=0; i<NumMyRowFaces(); ++i)
    {
      const DRT::FaceElement* faceele = dynamic_cast<const DRT::FaceElement*>(lRowFace(i));
      const unsigned int dofperface = faceele->ParentMasterElement()->NumDofPerFace(faceele->FaceMasterNumber());
      // const unsigned int dimension = DRT::UTILS::getDimension(lRowFace(i)->ParentMasterElement()->Shape());
      const unsigned int dofpercomponent = faceele->ParentMasterElement()->NumDofPerComponent(faceele->FaceMasterNumber());
      const unsigned int component = dofperface / dofpercomponent;

      if (onoff->size() <= component || (*onoff)[component] == 0)
        pressureDone = true;
      if (!pressureDone) {
        if (this->NumMyRowElements() > 0 && this->Comm().MyPID()==0) {
          std::vector<int> predof = this->Dof(0, lRowElement(0));
          const int gid = predof[0];
          const int lid = this->DofRowMap(0)->LID(gid);
          // amend vector of DOF-IDs which are Dirichlet BCs
          if (systemvector != Teuchos::null)
            (*systemvector)[lid] = 0;
          if (systemvectord != Teuchos::null)
            (*systemvectord)[lid] = 0;
          if (systemvectordd != Teuchos::null)
            (*systemvectordd)[lid] = 0;
          // set toggle vector
          if (toggle != Teuchos::null)
            (*toggle)[lid] = 1.0;
          // amend vector of DOF-IDs which are Dirichlet BCs
          if (dbcgids != Teuchos::null)
            (*dbcgids).insert(gid);
          pressureDone = true;
        }
      }

      int nummynodes = lRowFace(i)->NumNode();
      const int * mynodes = lRowFace(i)->NodeIds();

      // do only faces where all nodes are present in the node list
      bool faceRelevant = true;
      for (int j=0; j<nummynodes; ++j)
        if (!cond.ContainsNode(mynodes[j]))
        {
          faceRelevant = false;
          break;
        }
      if (!faceRelevant) continue;

      initParams.set<unsigned int>("faceconsider",
          static_cast<unsigned int>(faceele->FaceMasterNumber()));
      if (static_cast<unsigned int>(elevec1.M()) != dofperface)
        elevec1.Shape(dofperface, 1);
      std::vector<int> dofs = this->Dof(0,lRowFace(i));

      bool do_evaluate = false;
      if (funct != NULL)
        for (unsigned int i=0; i<component; ++i)
          if ((*funct)[i] > 0)
            do_evaluate = true;

      if (do_evaluate)
        faceele->ParentMasterElement()->Evaluate(initParams,*this,dummy,elemat1,elemat2,elevec1,elevec2,elevec3);
      else
        for (unsigned int i=0; i<dofperface; ++i)
          elevec1(i) = 1.;


      for (unsigned int i=0; i<dofperface; ++i) {
        int onesetj = i / dofpercomponent;
        if ((*onoff)[onesetj]==0)
        {
          const int lid = (*systemvectoraux).Map().LID(dofs[i]);
          if (lid<0) dserror("Global id %d not on this proc in system vector",dofs[i]);
          if (toggle!=Teuchos::null)
            (*toggle)[lid] = 0.0;
          // get rid of entry in DBC map - if it exists
          if (dbcgids != Teuchos::null)
            (*dbcgids).erase(dofs[i]);
          continue;
        }

        // get global id
        const int gid = dofs[i];

        // assign value
        const int lid = (*systemvectoraux).Map().LID(gid);
        if (lid<0) dserror("Global id %d not on this proc in system vector",gid);
        if (systemvector != Teuchos::null)
          (*systemvector)[lid] = (*val)[onesetj] * elevec1(i) * curvefacs[onesetj][0];
        if (systemvectord != Teuchos::null)
          (*systemvectord)[lid] = (*val)[onesetj] * elevec1(i) * curvefacs[onesetj][1];
        if (systemvectordd != Teuchos::null)
          (*systemvectordd)[lid] = (*val)[onesetj] * elevec1(i) * curvefacs[onesetj][2];
        // set toggle vector
        if (toggle != Teuchos::null)
          (*toggle)[lid] = 1.0;
        // amend vector of DOF-IDs which are Dirichlet BCs
        if (dbcgids != Teuchos::null)
          (*dbcgids).insert(gid);
      }
    } // loop over faces
  }
}

/*----------------------------------------------------------------------*
 | AssignGlobalIDs                                        schoeder 06/14|
 *----------------------------------------------------------------------*/
void DRT::DiscretizationHDG::AssignGlobalIDs(const Epetra_Comm& comm,
                                             const std::map< std::vector<int>, Teuchos::RCP<DRT::Element> >& elementmap,
                                             std::map< int, Teuchos::RCP<DRT::Element> >& finalelements )
{
  // The point here is to make sure the element gid are the same on any
  // parallel distribution of the elements. Thus we allreduce thing to
  // processor 0 and sort the element descriptions (vectors of nodal ids)
  // there. We also communicate the element degree! This is the difference
  // the base class function!
  //
  // This routine has not been optimized for efficiency. I don't think that is
  // needed.
  //
  // pack elements on all processors

  int size = 0;
  std::map<std::vector<int>, Teuchos::RCP<DRT::Element> >::const_iterator elemsiter;
  for (elemsiter=elementmap.begin();
       elemsiter!=elementmap.end();
       ++elemsiter)
  {
    size += elemsiter->first.size()+2;
  }

  std::vector<int> sendblock;
  sendblock.reserve(size);
  for (elemsiter=elementmap.begin();
       elemsiter!=elementmap.end();
       ++elemsiter)
  {
    sendblock.push_back(elemsiter->first.size());
    sendblock.push_back(elemsiter->second->Degree());
    std::copy(elemsiter->first.begin(), elemsiter->first.end(), std::back_inserter(sendblock));
  }

  // communicate elements to processor 0

  int mysize = sendblock.size();
  comm.SumAll(&mysize,&size,1);
  int mypos = LINALG::FindMyPos(sendblock.size(),comm);

  std::vector<int> send(size);
  std::fill(send.begin(),send.end(),0);
  std::copy(sendblock.begin(),sendblock.end(),&send[mypos]);
  sendblock.clear();
  std::vector<int> recv(size);
  comm.SumAll(&send[0],&recv[0],size);
  send.clear();

  // unpack, unify and sort elements on processor 0

  if (comm.MyPID()==0)
  {
    std::map<std::vector<int>,int > elementsanddegree;
    int index = 0;
    while (index < static_cast<int>(recv.size()))
    {

      int esize = recv[index];
      int degree = recv[index+1];
      index += 2;
      std::vector<int> element;
      element.reserve(esize);
      std::copy(&recv[index], &recv[index+esize], std::back_inserter(element));
      index += esize;

      // check if we already have this and if so, check for max degree
      std::map<std::vector<int>, int>::const_iterator iter =elementsanddegree.find(element);
      if(iter!=elementsanddegree.end())
      {
        degree = iter->second>degree ? iter->second : degree;
        elementsanddegree.erase(element); // is only inserted in the next line, if the entry does not exist
      }
      elementsanddegree.insert(std::pair<std::vector<int>,int >(element,degree));
    }
    recv.clear();

    // pack again to distribute pack to all processors
    std::map<std::vector<int>, int>::const_iterator iter;
    send.reserve(index);

    for(iter=elementsanddegree.begin();iter!=elementsanddegree.end();++iter)
    {
      send.push_back(iter->first.size());
      send.push_back(iter->second);
      std::copy(iter->first.begin(), iter->first.end(), std::back_inserter(send));
    }
    size = send.size();
  }
  else
  {
    recv.clear();
  }

  // broadcast sorted elements to all processors

  comm.Broadcast(&size,1,0);
  send.resize(size);
  comm.Broadcast(&send[0],send.size(),0);

  // Unpack sorted elements. Take element position for gid.

  int index = 0;
  int gid = 0;
  while (index < static_cast<int>(send.size()))
  {
    int esize = send[index];
    int degree = send[index+1];
    index += 2;
    std::vector<int> element;
    element.reserve(esize);
    std::copy(&send[index], &send[index+esize], std::back_inserter(element));
    index += esize;

    // set gid to my elements
    std::map<std::vector<int>, Teuchos::RCP<DRT::Element> >::const_iterator iter = elementmap.find(element);
    if (iter!=elementmap.end())
    {
      iter->second->SetId(gid);
      // TODO visc eles, fluid hdg eles
      Teuchos::RCP<DRT::ELEMENTS::AcouIntFace> acouele = Teuchos::rcp_dynamic_cast<DRT::ELEMENTS::AcouIntFace>(iter->second);
      if(acouele!=Teuchos::null) acouele->SetDegree(degree);

      finalelements[gid] = iter->second;
    }

    gid += 1;
  }
}



/*----------------------------------------------------------------------*
 |  AddElementGhostLayer                               kronbichler 04/15|
 *----------------------------------------------------------------------*/
void
DRT::DiscretizationHDG::AddElementGhostLayer()
{
  if (!Filled())
    dserror("Discretization must be filled upon entry of AddElementGhostLayer");

  const int mypid = Comm().MyPID();

  // step 1: identify all nodes on row elements
  std::vector<std::pair<int,int> > nodeids;
  for (int ele=0; ele<NumMyRowElements(); ++ele)
    for (int n=0; n<lRowElement(ele)->NumNode(); ++n)
      nodeids.push_back(std::make_pair(lRowElement(ele)->Nodes()[n]->Id(),
                                       lRowElement(ele)->Nodes()[n]->Owner()));

  // sort and compress out duplicates
  std::sort(nodeids.begin(), nodeids.end());
  nodeids.resize(std::unique(nodeids.begin(), nodeids.end())-nodeids.begin());

  // step 2: Get patch of elements around each node. this info must be provided by the owner
  // of the node.

  // step 2a: create a map with data we want to import
  std::vector<int> indices;
  for (unsigned int i=0; i<nodeids.size(); ++i)
    if (nodeids[i].second != mypid)
      indices.push_back(nodeids[i].first);
  const int numindices = indices.size();
  if (numindices == 0)
    indices.push_back(-1);
  Epetra_Map targetmap(-1, numindices, &indices[0], 0, Comm());

  // step 2b: create a copy of the element topology for owned nodes
  std::map<int, std::vector<int> > nodetoelement;
  for (int n=0; n<NumMyRowNodes(); ++n)
  {
    std::vector<int> &elements = nodetoelement[lRowNode(n)->Id()];
    elements.resize(lRowNode(n)->NumElement());
    for (unsigned int e=0; e<elements.size(); ++e)
      elements[e] = lRowNode(n)->Elements()[e]->Id();
  }

  // step 3: do the communication
  {
    Exporter exporter(*NodeRowMap(), targetmap, Comm());
    exporter.Export(nodetoelement);
  }

  // step 4: collect the ids of the new set of col elements
  std::vector<int> newcolelements;
  for (int i=0; i<NumMyColElements(); ++i)
    newcolelements.push_back(lColElement(i)->Id());

  for (std::map<int, std::vector<int> >::const_iterator it=nodetoelement.begin();
      it != nodetoelement.end(); ++it)
    for (unsigned int e=0; e<it->second.size(); ++e)
      newcolelements.push_back(it->second[e]);

  nodetoelement.clear();

  // sort and compress out duplicates
  std::sort(newcolelements.begin(), newcolelements.end());
  newcolelements.resize(std::unique(newcolelements.begin(), newcolelements.end())-
      newcolelements.begin());
  const int numcolelements = newcolelements.size();
  if (numcolelements == 0)
    newcolelements.push_back(-1);
  Epetra_Map elecolmap(-1, numcolelements, &newcolelements[0], 0, Comm());

  // step 5: find node col map that matches the selected elements, similarly to elements
  std::map<int, std::vector<int> > elementtonode;
  for (int e=0; e<NumMyRowElements(); ++e)
  {
    std::vector<int> &nodes = elementtonode[lRowElement(e)->Id()];
    nodes.resize(lRowElement(e)->NumNode());
    for (unsigned int n=0; n<nodes.size(); ++n)
      nodes[n] = lRowElement(e)->Nodes()[n]->Id();
  }
  {
    Exporter exporter(*ElementRowMap(), elecolmap, Comm());
    exporter.Export(elementtonode);
  }
  std::vector<int> newcolnodes;
  for (std::map<int, std::vector<int> >::const_iterator it=elementtonode.begin();
       it != elementtonode.end(); ++it)
    for (unsigned int n=0; n<it->second.size(); ++n)
      newcolnodes.push_back(it->second[n]);

  elementtonode.clear();

  // sort and compress out duplicates
  std::sort(newcolnodes.begin(), newcolnodes.end());
  newcolnodes.resize(std::unique(newcolnodes.begin(), newcolnodes.end())-
                     newcolnodes.begin());
  const int numcolnodes = newcolnodes.size();
  if (numcolnodes == 0)
    newcolnodes.push_back(-1);
  Epetra_Map nodecolmap(-1, numcolnodes, &newcolnodes[0], 0, Comm());

  // step 6: pass information about new col elements to the discretization (i.e., myself)
  ExportColumnNodes(nodecolmap);
  ExportColumnElements(elecolmap);
  FillComplete();
}



/*----------------------------------------------------------------------*
 |  << operator                                        kronbichler 12/13|
 *----------------------------------------------------------------------*/
std::ostream& operator << (std::ostream& os, const DRT::DiscretizationHDG& dis)
{
  // print standard discretization info
  dis.Print(os);
  // print additional info about internal faces
  dis.PrintFaces(os);

  return os;
}

