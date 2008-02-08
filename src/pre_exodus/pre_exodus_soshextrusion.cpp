/*----------------------------------------------------------------------*/
/*!
\file pre_exodus_soshextrusion.cpp

\brief solid-shell body creation by extruding surface 

<pre>
Maintainer: Moritz
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/frenzel
            089 - 289-15240
</pre>

Here everything related with solid-shell body extrusion
*/
/*----------------------------------------------------------------------*/
#ifdef D_EXODUS
#include "pre_exodus_soshextrusion.H"
#include "pre_exodus_reader.H"
#include "pre_node.H"
#include "pre_element.H"

using namespace std;
using namespace Teuchos;


/* Method to extrude a surface to become a volumetric body */
EXODUS::Mesh EXODUS::SolidShellExtrusion(EXODUS::Mesh basemesh, double thickness, int layers)
{
  int highestnid = basemesh.GetNumNodes() +1;
  map<int,vector<double> > newnodes;          // here the new nodes ar stored
  map<int,EXODUS::ElementBlock> neweblocks;   // here the new EBlocks are stores
  int highestblock = basemesh.GetNumElementBlocks();
  map<int,EXODUS::ElementBlock> ebs = basemesh.GetElementBlocks();
  map<int,EXODUS::ElementBlock>::const_iterator i_ebs;
  map<int,EXODUS::ElementBlock> extrudeblocks;
  
  // loop through all EBlocks to check for extrusion blocks
  for (i_ebs = ebs.begin(); i_ebs != ebs.end(); ++i_ebs ){
    bool toextrude = CheckExtrusion(i_ebs->second);
    if (toextrude) extrudeblocks.insert(pair<int,ElementBlock>(i_ebs->first,i_ebs->second));
  }
  
  // loop all existing extrudeblocks
  for (i_ebs = extrudeblocks.begin(); i_ebs != extrudeblocks.end(); ++i_ebs){
    // get existing eblock
    EXODUS::ElementBlock extrudeblock = i_ebs->second;
    const map<int,vector<int> > ele_conn = extrudeblock.GetEleConn();
    //extrudeblock.Print(cout);
    
    // Create Node to Element Connectivity
    const map<int,set<int> > node_conn = NodeToEleConn(extrudeblock);
    
    // Create Element to Element Connectivity (sharing an edge)
    const map<int,vector<int> > ele_neighbor = EleNeighbors(extrudeblock,node_conn,basemesh);
    
    // loop through all its elements to create new connectivity  ***************
    map<int,vector<int> > newconn;
    int newele = 0;
    map<int,vector<int> > node_pair; // stores new node id with base node id
    
    // set of elements already done
    set<int> doneles;
    set<int>::iterator i_doneles;
    
   // here we store a potential set of eles still to extrude
    set<int> todo_eles;
    set<int>::iterator i_todo_eles;
    todo_eles.insert(0); // lets insert the very first one for a start
    
    // for the first element we set up everything *****************************
    vector<int> actelenodes = extrudeblock.GetEleNodes(0);
    vector<int>::const_iterator i_node;
    int newid;
    
    // store one mean element normal to define and check for "outside"-direction
    //map<int,vector<double> > ele_normals;
    // store average node normals for each node
    map<int,vector<double> > node_normals;
    
    // calculate the normal at the first node which defines the "outside" dir
    vector<int> myNodeNbrs = FindNodeNeighbors(actelenodes,actelenodes.front());
    vector<double> first_normal = Normal(myNodeNbrs[1],actelenodes.front(),myNodeNbrs[0],basemesh);
    
    // create a new element
    vector<int> newelenodes;
    // create a vector of nodes for each layer which will form layered eles
    vector<vector<int> > layer_nodes(layers+1);
    // store all of elements node normals
    //vector<vector<double> > ele_nodenormals;
    for (i_node=actelenodes.begin(); i_node < actelenodes.end(); ++i_node){
      newid = highestnid; ++ highestnid; // here just raise for each basenode
      
      // place new node at new position
      vector<double> actcoords = basemesh.GetNodeExo(*i_node); //curr position
      // new base position equals actele (matching mesh!)
      const vector<double> newcoords = actcoords;
      int newExoNid = ExoToStore(newid);
      // put new coords into newnode map
      newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
      
      // put new node into map of OldNodeToNewNode
      vector<int> newids(1,newid);
      node_pair.insert(std::pair<int,vector<int> >(*i_node,newids));
      // insert node into base layer
      layer_nodes[0].push_back(newid);
      
      // calculate extruding direction
      vector<double> normal = NodeToAvgNormal(*i_node,actelenodes,first_normal,node_conn,extrudeblock,basemesh);
      node_normals.insert(pair<int,vector<double> >(*i_node,normal));
      //ele_nodenormals.push_back(normal);
      
      // create layers at this node location
      for (int i_layer = 1; i_layer <= layers; ++i_layer) {
        const vector<double> newcoords = ExtrudeNodeCoords(actcoords, thickness, i_layer, layers, normal);
        //newid += i_layer*nod_per_basele;
        // numbering of new ids nodewise not layerwise as may be expected
        newid = highestnid; ++ highestnid; 
        int newExoNid = ExoToStore(newid);
        // put new coords into newnode map
        newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
        // put new node into map of OldNodeToNewNode
        node_pair[*i_node].push_back(newid);
        // finally store this node where it will be connected to an ele
        layer_nodes[i_layer].push_back(newid);
      }
    }    
    //highestnid += nod_per_basele*(layers); // correct to account for layers
    
    // Ele Normal of first ele is mean of node normals
    //ele_normals.insert(pair<int,vector<double> >(0,MeanVec(ele_nodenormals)));
    
    doneles.insert(0);    // the first element is done ************************
    // form every new layer element
    for (int i_layer = 0; i_layer < layers; ++i_layer){
      vector<int> basenodes = layer_nodes[i_layer];
      vector<int> ceilnodes = layer_nodes[i_layer+1];
      newelenodes.insert(newelenodes.begin(),basenodes.begin(),basenodes.end());
      newelenodes.insert(newelenodes.end(),ceilnodes.begin(),ceilnodes.end());
      // insert new element into new connectivity
      newconn.insert(pair<int,vector<int> >(newele,newelenodes));
      ++ newele;
      newelenodes.clear();
    }
    layer_nodes.clear();
     
    
    while (todo_eles.size() > 0){ // start of going through ele neighbors///////
      
      // find an actele still to do
      i_todo_eles=todo_eles.begin();
      int actele = *i_todo_eles;
      // delete actele from todo_eles list
      todo_eles.erase(i_todo_eles);
      
      // get nodes of actual element
      vector<int> actelenodes = extrudeblock.GetEleNodes(actele);
      
      // get edge neighbors
      vector<int> actneighbors = ele_neighbor.at(actele);
      
      // get elenormal as reference direction for neighbors
      //vector<double> ele_normal = ele_normals.find(actele);
      
      vector<int>::const_iterator i_nbr;
      unsigned int edge = 0; // edge counter
      for (i_nbr = actneighbors.begin(); i_nbr < actneighbors.end(); ++ i_nbr){
        int actneighbor = *i_nbr;
        // check for undone neighbor
        i_doneles = doneles.find(actneighbor);
        if ((actneighbor != -1) && (i_doneles == doneles.end())){
          vector<int> actneighbornodes = ele_conn.find(actneighbor)->second;
          // lets extrude *****************************************************
          
          // get nodepair of actual edge
          int firstedgenode = actelenodes.at(edge);
          int secedgenode;
          // switch in case of last to first node edge
          if (edge == actelenodes.size()-1) secedgenode = actelenodes.at(0);
          else secedgenode = actelenodes.at(edge+1);
          
          // create a vector of nodes for each layer which will form layered eles
          vector<vector<int> > layer_nodes(layers+1);
          
          /* the new elements orientation is opposite the current one
           * therefore the FIRST node is secedgenode */
          
          // check if new node already exists
          if (node_pair.find(secedgenode)==node_pair.end()){
            // lets create a new node
            newid = highestnid; ++ highestnid;
            // place new node at new position
            vector<double> actcoords = basemesh.GetNodeExo(secedgenode); //curr position
            // new base position equals actele (matching mesh!)
            const vector<double> newcoords = actcoords;
            int newExoNid = ExoToStore(newid);
            // put new coords into newnode map
            newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
            
            // put new node into map of OldNodeToNewNode
            vector<int> newids(1,newid);
            node_pair.insert(std::pair<int,vector<int> >(secedgenode,newids));
            
            // insert node into base layer
            layer_nodes[0].push_back(newid);
            
            // get reference direction for new avg normal here firstedgenode
            vector<double> refnormal = node_normals.find(firstedgenode)->second;
            // calculate extruding direction
            vector<double> normal = NodeToAvgNormal(secedgenode,actneighbornodes,refnormal,node_conn,extrudeblock,basemesh);
            node_normals.insert(pair<int,vector<double> >(secedgenode,normal));

            // create layers at this node location
            for (int i_layer = 1; i_layer <= layers; ++i_layer) {
              const vector<double> newcoords = ExtrudeNodeCoords(actcoords, thickness, i_layer, layers, normal);
              newid = highestnid; ++ highestnid;
              int newExoNid = ExoToStore(newid);
              // put new coords into newnode map
              newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
              
              // put new node into map of OldNodeToNewNode
              node_pair[secedgenode].push_back(newid);
              // finally store this node where it will be connected to an ele
              layer_nodes[i_layer].push_back(newid);
            }
            
          } else {
            for (int i_layer = 0; i_layer <= layers; ++i_layer) {
              newid = node_pair.find(secedgenode)->second[i_layer];
              // finally store this node where it will be connected to an ele
              layer_nodes[i_layer].push_back(newid);
            }
          }
          
          /* the new elements orientation is opposite the current one
           * therefore the SECOND node is firstedgenode */
          
          // check if new node already exists
          if (node_pair.find(firstedgenode)==node_pair.end()){
            // lets create a new node
            newid = highestnid; ++ highestnid;
            // place new node at new position
            vector<double> actcoords = basemesh.GetNodeExo(firstedgenode); //curr position
            // new base position equals actele (matching mesh!)
            const vector<double> newcoords = actcoords;
            int newExoNid = ExoToStore(newid);
            // put new coords into newnode map
            newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
            
            // put new node into map of OldNodeToNewNode
            vector<int> newids(1,newid);
            node_pair.insert(std::pair<int,vector<int> >(firstedgenode,newids));
            
            // insert node into base layer
            layer_nodes[0].push_back(newid);

            // get reference direction for new avg normal here secedgenode
            vector<double> refnormal = node_normals.find(secedgenode)->second;
            // calculate extruding direction
            vector<double> normal = NodeToAvgNormal(firstedgenode,actneighbornodes,refnormal,node_conn,extrudeblock,basemesh);
            node_normals.insert(pair<int,vector<double> >(firstedgenode,normal));

            // create layers at this node location
            for (int i_layer = 1; i_layer <= layers; ++i_layer) {
              const vector<double> newcoords = ExtrudeNodeCoords(actcoords, thickness, i_layer, layers, normal);
              newid = highestnid; ++ highestnid;
              int newExoNid = ExoToStore(newid);
              // put new coords into newnode map
              newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
              
              // put new node into map of OldNodeToNewNode
              node_pair[firstedgenode].push_back(newid);
              // finally store this node where it will be connected to an ele
              layer_nodes[i_layer].push_back(newid);
            }
          } else {
            for (int i_layer = 0; i_layer <= layers; ++i_layer) {
              newid = node_pair.find(firstedgenode)->second[i_layer];
              // finally store this node where it will be connected to an ele
              layer_nodes[i_layer].push_back(newid);
            }
          }
          
          /* the new elements orientation is opposite the current one
           * therefore the THIRD node is gained from the neighbor ele */
          
          vector<int> actnbrnodes = extrudeblock.GetEleNodes(actneighbor);
          int thirdnode = FindEdgeNeighbor(actnbrnodes,firstedgenode,secedgenode);
          
          // check if new node already exists
          if (node_pair.find(thirdnode)==node_pair.end()){
            // lets create a new node
            newid = highestnid; ++ highestnid;
            // place new node at new position
            vector<double> actcoords = basemesh.GetNodeExo(thirdnode); //curr position
            // new base position equals actele (matching mesh!)
            const vector<double> newcoords = actcoords;
            int newExoNid = ExoToStore(newid);
            // put new coords into newnode map
            newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
            
            // put new node into map of OldNodeToNewNode
            vector<int> newids(1,newid);
            node_pair.insert(std::pair<int,vector<int> >(thirdnode,newids));
            
            // insert node into base layer
            layer_nodes[0].push_back(newid);

            // get reference direction for new avg normal here firstedgenode
            vector<double> refnormal = node_normals.find(firstedgenode)->second;
            // calculate extruding direction
            vector<double> normal = NodeToAvgNormal(thirdnode,actneighbornodes,refnormal,node_conn,extrudeblock,basemesh);
            node_normals.insert(pair<int,vector<double> >(thirdnode,normal));

            // create layers at this node location
            for (int i_layer = 1; i_layer <= layers; ++i_layer) {
              const vector<double> newcoords = ExtrudeNodeCoords(actcoords, thickness, i_layer, layers, normal);
              newid = highestnid; ++ highestnid;
              int newExoNid = ExoToStore(newid);
              // put new coords into newnode map
              newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
              
              // put new node into map of OldNodeToNewNode
              node_pair[thirdnode].push_back(newid);
              // finally store this node where it will be connected to an ele
              layer_nodes[i_layer].push_back(newid);
            }
          } else {
            for (int i_layer = 0; i_layer <= layers; ++i_layer) {
              newid = node_pair.find(thirdnode)->second[i_layer];
              // finally store this node where it will be connected to an ele
              layer_nodes[i_layer].push_back(newid);
            }
          }
          
          if (actelenodes.size() > 3){ // in case of not being a tri3
            
            /* the new elements orientation is opposite the current one
             * therefore the FOURTH node is gained from the neighbor ele */

            int fourthnode = FindEdgeNeighbor(actnbrnodes,thirdnode,firstedgenode);
            
            // check if new node already exists
            if (node_pair.find(fourthnode)==node_pair.end()){
              // lets create a new node
              newid = highestnid; ++ highestnid;
              // place new node at new position
              vector<double> actcoords = basemesh.GetNodeExo(fourthnode); //curr position
              // new base position equals actele (matching mesh!)
              const vector<double> newcoords = actcoords;
              int newExoNid = ExoToStore(newid);
              // put new coords into newnode map
              newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
              
              // put new node into map of OldNodeToNewNode
              vector<int> newids(1,newid);
              node_pair.insert(std::pair<int,vector<int> >(fourthnode,newids));
              
              // insert node into base layer
              layer_nodes[0].push_back(newid);
              
              // get reference direction for new avg normal here firstedgenode
              vector<double> refnormal = node_normals.find(thirdnode)->second;
              // calculate extruding direction
              vector<double> normal = NodeToAvgNormal(fourthnode,actneighbornodes,refnormal,node_conn,extrudeblock,basemesh);
              node_normals.insert(pair<int,vector<double> >(fourthnode,normal));

              // create layers at this node location
              for (int i_layer = 1; i_layer <= layers; ++i_layer) {
                const vector<double> newcoords = ExtrudeNodeCoords(actcoords, thickness, i_layer, layers, normal);
                newid = highestnid; ++ highestnid;
                int newExoNid = ExoToStore(newid);
                // put new coords into newnode map
                newnodes.insert(pair<int,vector<double> >(newExoNid,newcoords));
                
                // put new node into map of OldNodeToNewNode
                node_pair[fourthnode].push_back(newid);
                // finally store this node where it will be connected to an ele
                layer_nodes[i_layer].push_back(newid);
              }
            } else {
              for (int i_layer = 0; i_layer <= layers; ++i_layer) {
                newid = node_pair.find(fourthnode)->second[i_layer];
                // finally store this node where it will be connected to an ele
                layer_nodes[i_layer].push_back(newid);
              }
            }
          } // end of 4-node case
          
          // form every new layer element
          for (int i_layer = 0; i_layer < layers; ++i_layer){
            vector<int> basenodes = layer_nodes[i_layer];
            vector<int> ceilnodes = layer_nodes[i_layer+1];
            newelenodes.insert(newelenodes.begin(),basenodes.begin(),basenodes.end());
            newelenodes.insert(newelenodes.end(),ceilnodes.begin(),ceilnodes.end());
            // insert new element into new connectivity
            newconn.insert(pair<int,vector<int> >(newele,newelenodes));
            ++ newele;
            newelenodes.clear();
          }
          layer_nodes.clear();

          // insert actneighbor into done elements
          doneles.insert(actneighbor);
          
          // neighbor eles are possible next "center" eles
          todo_eles.insert(actneighbor);  
          
        }// end of if undone->extrude this neighbor, next neighbor *************
        ++ edge; // next element edge
        
      }// end of this "center" - element ///////////////////////////////////////
      
    }// end of extruding all elements in block
   
    //PrintMap(cout,node_normals);
    // create new Element Block
    std::ostringstream blockname;
    blockname << "extrude" << i_ebs->first;
    ElementBlock::Shape newshape;
    switch (extrudeblock.GetShape()) {
      case ElementBlock::shell4: newshape = ElementBlock::hex8; break;
      case ElementBlock::tri3: newshape = ElementBlock::wedge6; break;
      default: dserror("unrecognized EBlock Shape"); break;
    }

    EXODUS::ElementBlock neweblock(newshape,newconn,blockname.str());
    int newblockID = highestblock + i_ebs->first;
    neweblocks.insert(pair<int,EXODUS::ElementBlock>(newblockID,neweblock));
    
  } // end of extruding elementblocks
  
  string newtitle = "extrusion";
  map<int,EXODUS::NodeSet> emptynodeset;
  map<int,EXODUS::SideSet> emptysideset;

  EXODUS::Mesh extruded_mesh(basemesh,newnodes,neweblocks,emptynodeset,emptysideset,newtitle);
  
  return extruded_mesh;
}


bool EXODUS::CheckExtrusion(const EXODUS::ElementBlock eblock)
{
  const EXODUS::ElementBlock::Shape myshape = eblock.GetShape();
  const string myname = eblock.GetName();
  if (myname.compare(0,7,"extrude") == 0)
    if ((myshape == EXODUS::ElementBlock::shell4)
    || (myshape == EXODUS::ElementBlock::tri3)) return true;
  return false;
}

vector<int> EXODUS::RiseNodeIds(int highestid, map<int,int> pair, const vector<int> elenodes)
{
  vector<int> newnodes(elenodes.size());
//  int newid;
//  map<int,int>::iterator it;
//  vector<int>::const_iterator i_node;
//  for (i_node=elenodes.begin(); i_node < elenodes.end(); ++i_node){
//    if (pair.find(*i_node)==pair.end()){ newid = highestid; ++ highestid;}
//    else newid = pair.find(*i_node)->second;
//    newnodes.push_back(newid);
//    pair.insert(std::pair<int,int>(*i_node,newid));
//  }
  return newnodes;
}

vector<double> EXODUS::ExtrudeNodeCoords(const vector<double> basecoords,
    const double distance, const int layer, const int numlayers, const vector<double> normal)
{
  vector<double> newcoords(3);
  
  double actdistance = layer * distance/numlayers; 
  
  newcoords[0] = basecoords[0] + actdistance*normal.at(0);
  newcoords[1] = basecoords[1] + actdistance*normal.at(1);
  newcoords[2] = basecoords[2] + actdistance*normal.at(2);
  return newcoords;
}

const map<int,set<int> > EXODUS::NodeToEleConn(const EXODUS::ElementBlock eblock)
{
  map<int,set<int> > node_conn;
  const map<int,vector<int> > ele_conn = eblock.GetEleConn();
  map<int,vector<int> >::const_iterator i_ele;
  
  // loop all elements for their nodes
  for (i_ele = ele_conn.begin(); i_ele != ele_conn.end(); ++i_ele){
    vector<int> elenodes = i_ele->second;
    vector<int>::iterator i_node;
    // loop all nodes within element
    for (i_node = elenodes.begin(); i_node < elenodes.end(); ++i_node){
      int nodeid = *i_node;
      // add this ele_id into set of nodeid
      node_conn[nodeid].insert(i_ele->first);
    }
  }
  return node_conn;
}

const map<int,vector<int> > EXODUS::EleNeighbors(EXODUS::ElementBlock eblock, const map<int,set<int> >& node_conn,
    const EXODUS::Mesh& basemesh)
{
  map<int,vector<int> > eleneighbors;
  const map<int,vector<int> > ele_conn = eblock.GetEleConn();
  map<int,vector<int> >::const_iterator i_ele;

  // loop all elements
  for (i_ele = ele_conn.begin(); i_ele != ele_conn.end(); ++i_ele){
    int acteleid = i_ele->first;
    vector<int> actelenodes = i_ele->second;
    vector<int>::iterator i_node;
    map<int,set<int> > elepatch; // consists of all elements connected by shared nodes
    // loop all nodes within element
    for (i_node = actelenodes.begin(); i_node < actelenodes.end(); ++i_node){
      int nodeid = *i_node;
      // find all elements connected to this node
      const set<int> eles = node_conn.find(nodeid)->second;
      // add these eles into patch
      elepatch[nodeid].insert(eles.begin(),eles.end());
    }

    // default case: no neighbors at any edge
    vector<int> defaultnbrs(actelenodes.size(),-1);
    eleneighbors.insert(pair<int,vector<int> >(acteleid,defaultnbrs));
    int edgeid = 0;
    // now select those elements out of the patch which share an edge
    for (i_node = actelenodes.begin(); i_node < actelenodes.end(); ++i_node){
      int firstedgenode = *i_node;
      int secedgenode;
      // edge direction according to order in elenodes, plus last to first
      if (i_node == (actelenodes.end()-1)) secedgenode = *actelenodes.begin();
      else secedgenode = *(i_node + 1);
      // find all elements connected to the first node
      const set<int> firsteles = elepatch.find(firstedgenode)->second;
      // loop over these elements to find the one sharing the secondedgenode
      set<int>::const_iterator it;
      for(it = firsteles.begin(); it != firsteles.end(); ++it){
        const int trialele = *it;
        if (trialele != acteleid){
          vector<int> neighbornodes = ele_conn.find(trialele)->second;
          bool found = FindinVec(secedgenode,neighbornodes);
          if (found) {
            eleneighbors[acteleid].at(edgeid) = trialele;
            break;
          }
        }
      }
      edgeid ++;
    } // end of loop i_nodes
  } // end of loop all elements
  return eleneighbors;
}

vector<double> EXODUS::NodeToAvgNormal(const int node,
                             const vector<int> elenodes,
                             const vector<double> refnormdir,
                             const map<int,set<int> >& nodetoele,
                             const EXODUS::ElementBlock& eblock,
                             const EXODUS::Mesh& basemesh)
{
    vector<int> myNodeNbrs = FindNodeNeighbors(elenodes,node);
    
    // calculate normal at node
    vector<double> normal = Normal(myNodeNbrs[1],node,myNodeNbrs[0],basemesh);
    CheckNormDir(normal,refnormdir);
    
    // look at neighbors
    vector<vector<double> > nbr_normals;
    const set<int> nbreles = nodetoele.find(node)->second;
    set<int>::const_iterator i_nbr;
    for (i_nbr=nbreles.begin(); i_nbr !=nbreles.end(); ++i_nbr){
      int nbr = *i_nbr;
      vector<int> nbrele = eblock.GetEleNodes(nbr);
      if (nbrele != elenodes){ // otherwise it's me, not a neighbor!
        vector<int> n_nbrs = FindNodeNeighbors(nbrele,node);
        vector<double> nbr_normal = Normal(n_nbrs[1],node,n_nbrs[0],basemesh);
        // check whether nbr_normal points into approx same dir
        CheckNormDir(nbr_normal,refnormdir);
        nbr_normals.push_back(nbr_normal);
      }
    }
    
    // average node normal with all neighbors
    vector<double> myavgnormal = AverageNormal(normal,nbr_normals);
    
  return myavgnormal;
}

vector<double> EXODUS::AverageNormal(const vector<double> n, const vector<vector<double> > nbr_ns)
{
  // if node has no neighbor avgnormal is normal
  if (nbr_ns.size() == 0) return n;
  
  // else do averaging
  vector<double> avgn = n;
  vector<vector<double> >::const_iterator i_nbr;
  
  // define lower bound for (nearly) parallel normals
  const double para = 1.0e-12;
  
  for(i_nbr=nbr_ns.begin(); i_nbr < nbr_ns.end(); ++i_nbr){
    // cross-product with next neighbor normal
    vector<double> cross(3);
    vector<double> nbr_n = *i_nbr;
    cross[0] =    avgn[1]*nbr_n[2] - avgn[2]*nbr_n[1];
    cross[1] = - (avgn[0]*nbr_n[2] - avgn[2]*nbr_n[0]);
    cross[2] =    avgn[0]*nbr_n[1] - avgn[1]*nbr_n[0];
    double crosslength = cross[0]*cross[0] + cross[1]*cross[1] + cross[2]*cross[2];
    
    
    if (crosslength<para){
    // if almost parallel do the easy way: average = mean
      avgn[0] = 0.5 * (avgn[0] + nbr_n[0]);
      avgn[1] = 0.5 * (avgn[1] + nbr_n[1]);
      avgn[2] = 0.5 * (avgn[2] + nbr_n[2]);
     
    } else {
    // do the Bischoff-Way:
      // left length
      double leftl = avgn[0]*avgn[0] + avgn[1]*avgn[1] + avgn[2]*avgn[2];
      // right length
      double rightl = nbr_n[0]*nbr_n[0] + nbr_n[1]*nbr_n[1] + nbr_n[2]*nbr_n[2];
      // mean
      avgn[0] = 0.5 * (avgn[0] + nbr_n[0]);
      avgn[1] = 0.5 * (avgn[1] + nbr_n[1]);
      avgn[2] = 0.5 * (avgn[2] + nbr_n[2]);
      // mean length
      double avgl = avgn[0]*avgn[0] + avgn[1]*avgn[1] + avgn[2]*avgn[2];
      // scale by mean of left and right normal
      avgn[0] = avgn[0] * 0.5*(leftl+rightl)/avgl;
      avgn[1] = avgn[1] * 0.5*(leftl+rightl)/avgl;
      avgn[2] = avgn[2] * 0.5*(leftl+rightl)/avgl;
    }
  } // average with next neighbor
  
  // unit length:
  double length = sqrt(avgn[0]*avgn[0] + avgn[1]*avgn[1] + avgn[2]*avgn[2]);
  avgn[0] = avgn[0]/length;
  avgn[1] = avgn[1]/length;
  avgn[2] = avgn[2]/length;
  
  return avgn;
}

vector<double> EXODUS::Normal(int head1, int origin, int head2,const EXODUS::Mesh& basemesh)
{
  vector<double> normal(3);
  vector<double> h1 = basemesh.GetNodeExo(head1);
  vector<double> h2 = basemesh.GetNodeExo(head2);
  vector<double> o  = basemesh.GetNodeExo(origin);
  
  normal[0] =   ((h1[1]-o[1])*(h2[2]-o[2]) - (h1[2]-o[2])*(h2[1]-o[1]));
  normal[1] = - ((h1[0]-o[0])*(h2[2]-o[2]) - (h1[2]-o[2])*(h2[0]-o[0]));
  normal[2] =   ((h1[0]-o[0])*(h2[1]-o[1]) - (h1[1]-o[1])*(h2[0]-o[0]));
  
  double length = sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
  normal[0] = normal[0]/length;
  normal[1] = normal[1]/length;
  normal[2] = normal[2]/length;
  
  return normal;
}

void EXODUS::CheckNormDir(vector<double> checkn,const vector<double> refn)
{
  // compute scalar product of the two normals
  double scp = checkn[0]*refn.at(0) + checkn[1]*refn.at(1) + checkn[2]*refn.at(2);
  if (scp<0){
    checkn[0] = -checkn[0]; checkn[1] = -checkn[1]; checkn[2] = -checkn[2];
  }
}

vector<double> EXODUS::MeanVec(const vector<vector<double> >baseVecs)
{
  if (baseVecs.size() == 0) dserror("baseVecs empty -> div by 0");
  vector<double> mean;
  vector<vector<double> >::const_iterator i_vec;
  //vector<double>::const_iterator i;
  for(i_vec=baseVecs.begin(); i_vec < baseVecs.end(); ++i_vec){
    const vector<double> vec = *i_vec;
    for(unsigned int i=0 ; i < vec.size(); ++i){
      mean[i] += vec.at(i);
    }
  }
  for(unsigned int i=0 ; i < mean.size(); ++i) mean[i] = mean[i] / baseVecs.size();
  return mean;
}

bool EXODUS::FindinVec(const int id, const vector<int> vec)
{
  vector<int>::const_iterator i;
  for(i=vec.begin(); i<vec.end(); ++i) if (*i == id) return true;
  return false;
}

int EXODUS::FindEdgeNeighbor(const vector<int> nodes, const int actnode, const int wrong_dir_node)
{
  // special case of very first node
  if (nodes.at(0) == actnode){
    if (nodes.at(1) == wrong_dir_node) return nodes.at(nodes.size());
    else return nodes.at(1); 
  }   else if (nodes.back() == actnode){
    // special case of very last node
    if (nodes.at(0) == wrong_dir_node) return nodes.at(nodes.size()-2);
    else return nodes.at(0);
  } else {
    // case of somewhere in between
    vector<int>::const_iterator i;
    for(i=nodes.begin(); i<nodes.end(); ++i){
      if (*i == actnode){
        if (*(i+1) == wrong_dir_node) return *(i-1);
        else return *(i+1);
      }
    }
  }
  return 0; // never reached!
}

vector<int> EXODUS::FindNodeNeighbors(const vector<int> nodes,const int actnode)
{
  vector<int> neighbors(2);
  // special case of very first node
  if (nodes.at(0) == actnode) {
    neighbors[0] = nodes.back();
    neighbors[1] = nodes.at(1);
  } else if (nodes.back() == actnode) {
    neighbors[0] = nodes.at(nodes.size()-2);
    neighbors[1] = nodes.front();
  } else {
    // case of somewhere in between
    vector<int>::const_iterator i;
    for (i=nodes.begin(); i<nodes.end(); ++i) {
      if (*i == actnode) {
        neighbors[0] = *(i-1);
        neighbors[1] = *(i+1);
      }
    }
  }
  return neighbors;
}

void EXODUS::PrintMap(ostream& os,const map<int,vector<int> > mymap)
{
  map<int,vector<int> >::const_iterator iter;
  for(iter = mymap.begin(); iter != mymap.end(); ++iter)
  {
      os << iter->first << ": ";
      vector<int> actvec = iter->second;
      vector<int>::iterator i;
      for (i=actvec.begin(); i<actvec.end(); ++i) {
        os << *i << ",";
      }
      os << endl;
  }
}

void EXODUS::PrintMap(ostream& os,const map<int,set<int> > mymap)
{
  map<int,set<int> >::const_iterator iter;
  for(iter = mymap.begin(); iter != mymap.end(); ++iter)
  {
      os << iter->first << ": ";
      set<int> actset = iter->second;
      set<int>::iterator i;
      for (i=actset.begin(); i != actset.end(); ++i) {
        os << *i << ",";
      }
      os << endl;
  }
}

void EXODUS::PrintMap(ostream& os,const map<int,vector<double> > mymap)
{
  map<int,vector<double> >::const_iterator iter;
  for(iter = mymap.begin(); iter != mymap.end(); ++iter)
  {
      os << iter->first << ": ";
      vector<double> actvec = iter->second;
      vector<double>::iterator i;
      for (i=actvec.begin(); i<actvec.end(); ++i) {
        os << *i << ",";
      }
      os << endl;
  }
}

void EXODUS::PrintVec(ostream& os, const vector<int> actvec)
{
  vector<int>::const_iterator i;
  for (i=actvec.begin(); i<actvec.end(); ++i) {
    os << *i << ",";
  }
  os << endl;
}

void EXODUS::PrintVec(ostream& os, const vector<double> actvec)
{
  vector<double>::const_iterator i;
  for (i=actvec.begin(); i<actvec.end(); ++i) {
    os << *i << ",";
  }
  os << endl;
}

void EXODUS::PrintSet(ostream& os, const set<int> actset)
{
  set<int>::iterator i;
  for (i=actset.begin(); i != actset.end(); ++i) {
    os << *i << ",";
  }
  os << endl;
  
}


#endif
