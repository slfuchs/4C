/*!-----------------------------------------------------------------------------------------------*
\file cut_output.cpp

\brief Handles file writing of all cut related stuff

<pre>
Maintainer: Sudhakar
            sudhakar@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15257
</pre>
 *------------------------------------------------------------------------------------------------*/

#include<iosfwd>
#include<vector>

#include "cut_output.H"
#include "cut_element.H"
#include "cut_line.H"
#include "cut_edge.H"
#include "cut_point.H"
#include "cut_cycle.H"

//Needed for LS info.
#include "cut_facet.H"
#include "cut_kernel.H"
#include "cut_volumecell.H"
#include "cut_boundarycell.H"

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given element                                           sudhakar 03/14
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshElementDump( std::ofstream & file, Element * ele , bool to_local)
{
  const std::vector<Node*> & nodes = ele->Nodes();
  char elementtype;
  switch ( nodes.size() )
  {
  case 8:
    elementtype = 'H';
    break;
  case 4:
    elementtype = 'S';
    break;
  case 6:
    elementtype = 'I';
    break;
  default:
    std::stringstream str;
    str << "unknown element type in GmshElementDump for " << nodes.size() << " nodes!";
    throw std::runtime_error( str.str() );
  }
  GmshElementDump( file, nodes, elementtype , to_local, ele );
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given element                                           sudhakar 03/14
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshElementDump( std::ofstream & file, const std::vector<GEO::CUT::Node*> & nodes, char elementtype , bool to_local, Element* ele)
{
  file << "S" << elementtype
       << "(";
  for ( std::vector<GEO::CUT::Node*>::const_iterator i=nodes.begin(); i!=nodes.end(); ++i )
  {
    if ( i!=nodes.begin() )
      file << ",";
    GmshWriteCoords( file, *i, to_local, ele);
  }
  file << "){";
  for ( std::vector<GEO::CUT::Node*>::const_iterator i=nodes.begin(); i!=nodes.end(); ++i )
  {
    GEO::CUT::Node * n = *i;
    GEO::CUT::Point * p = n->point();
    if ( i!=nodes.begin() )
      file << ",";
    file << p->Position();
  }
  file << "};\n";
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given side                                              sudhakar 03/14
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshSideDump( std::ofstream & file, const Side* s , bool to_local, Element* ele)
{
  const std::vector<Node*> & nodes = s->Nodes();
  char elementtype;
  switch ( nodes.size() )
  {
  case 0:
    return; //I'm a Levelset Side - do nothing!
  case 3:
    elementtype = 'T';
    break;
  case 4:
    elementtype = 'Q';
    break;
  default:
    std::stringstream str;
    str << "unknown element type in GmshSideDump for " << nodes.size() << " nodes!";
    throw std::runtime_error( str.str() );
  }
  GmshElementDump( file, nodes, elementtype, to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given side                                                ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshTriSideDump( std::ofstream & file, std::vector<Point*> points , bool to_local, Element* ele)
{
  char elementtype;
  switch ( points.size() )
  {
  case 3:
    elementtype = 'T';
    break;
  case 4:
    elementtype = 'Q';
    break;
  default:
  {
    std::stringstream str;
    str << "unknown element type in GmshTriSideDump for " << points.size() << " points!";
    throw std::runtime_error( str.str() );
    return;
  }
  }

  file << "S" << elementtype
       << "(";
  for (uint i = 0; i < points.size(); ++i)
  {
    if ( i!= 0 )
      file << ",";
    GmshWriteCoords(file, points[i],to_local, ele);
  }
  file << "){";
  for (uint i = 0; i < points.size(); ++i)
  {
    GEO::CUT::Point * p = points[i];
    if ( i!=0 )
      file << ",";
    file << p->Position();
  }
  file << "};\n";
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given facet                                                ager 08/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshFacetDump( std::ofstream & file, Facet* facet, const std::string& visualizationtype, bool print_all, bool to_local, Element* ele)
{
  if (to_local)
  {
    if (ele == NULL)
      dserror("GmshWriteCoords: Didn't get a parent element for the Coordinate!");
  }

  if (visualizationtype == "sides")
  {
    if(facet->IsTriangulated())
    {
      for (unsigned j = 0; j<facet->Triangulation().size() ; ++j)
      {
        GEO::CUT::OUTPUT::GmshTriSideDump(file, facet->Triangulation()[j], to_local, ele);
      }
    }
    else if(facet->IsFacetSplit())
    {
      for (unsigned j = 0; j < facet->GetSplitCells().size() ; ++j)
      {
        GEO::CUT::OUTPUT::GmshTriSideDump(file, facet->GetSplitCells()[j], to_local, ele);
      }
    }
    else if (facet->BelongsToLevelSetSide() || facet->CornerPoints().size() == 3 || facet->CornerPoints().size() == 4)
    {
      GEO::CUT::OUTPUT::GmshTriSideDump(file, facet->CornerPoints(), to_local, ele);
    }
    else if (facet->CornerPoints().size() > 2 && print_all) //do mitpoint triangulation just for visualization reasons! (not usedfull if you want to check if a triangulation exists!)
    {
      std::vector<double> xmid(3,0);
      for (unsigned int i = 0; i < facet->CornerPoints().size(); ++i)
      {
      for (unsigned int dim = 0; dim < 3; ++dim)
        xmid[dim] += facet->CornerPoints()[i]->X()[dim];
      }
      for (unsigned int dim = 0; dim < 3; ++dim)
        xmid[dim] = xmid[dim]/facet->CornerPoints().size();

      Point midpoint = Point(-1, &xmid[0], NULL,NULL,0.0);

      std::vector<Point*> tri;
      for (unsigned int i = 0; i < facet->CornerPoints().size(); ++i)
      {
        tri.clear();
        tri.push_back(facet->CornerPoints()[i]);
        tri.push_back(facet->CornerPoints()[(i+1)%facet->CornerPoints().size()]);
        tri.push_back(&midpoint);
        GEO::CUT::OUTPUT::GmshTriSideDump(file, tri, to_local, ele);
      }
    }
  }
  else if (visualizationtype == "lines")
  {
    for (uint pidx = 0; pidx < facet->Points().size(); ++pidx)
    {
      GEO::CUT::OUTPUT::GmshLineDump(file,facet->Points()[pidx], facet->Points()[(pidx+1)%facet->Points().size()],facet->SideId(), to_local, ele);
    }
  }
  else if (visualizationtype == "points")
  {
    for (uint pidx = 0; pidx < facet->Points().size(); ++pidx)
      GEO::CUT::OUTPUT::GmshPointDump( file, facet->Points()[pidx], facet->SideId(), to_local, ele);
  }
  else
    dserror("GmshFacetDump: unknown visualizationtype!");
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given volumecell                                                ager 08/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshVolumecellDump( std::ofstream & file, VolumeCell* VC, const std::string& visualizationtype, bool print_all, bool to_local, Element* ele)
{
      for( plain_facet_set::const_iterator j=VC->Facets().begin(); j!=VC->Facets().end(); j++ )
        GmshFacetDump(file,*j,visualizationtype,print_all, to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given cycle                                                ager 08/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshCycleDump( std::ofstream & file, Cycle* cycle, const std::string& visualizationtype, bool to_local, Element* ele)
{
  if (visualizationtype == "points")
  {
    for ( unsigned i=0; i!=(*cycle)().size(); ++i )
      GmshPointDump( file, (*cycle)()[i], to_local, ele);
  }
  else if (visualizationtype == "lines")
  {
    for ( unsigned i=0; i!=(*cycle)().size(); ++i )
      GmshLineDump( file, (*cycle)()[i], (*cycle)()[(i+1)%(*cycle)().size()], to_local, ele);
  }
  else
    dserror("GmshFacetDump: unknown visualizationtype!");
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of element along with all its cut sides                sudhakar 03/14
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshCompleteCutElement( std::ofstream & file, Element * ele ,bool to_local)
{
  // write details of background element
  GmshNewSection( file, "Element");
  GmshElementDump( file, ele, to_local);

  // write details of points
  GmshNewSection( file, "Points",true);
  const std::vector<Point*> & ps = ele->Points();
  for( std::vector<Point*>::const_iterator its = ps.begin(); its != ps.end(); its++ )
    GmshPointDump(file,*its,to_local,ele);

  // write details of cut sides
  GmshNewSection( file, "Cut_Facets",true);
  const plain_facet_set & facets = ele->Facets();
  for( plain_facet_set::const_iterator its = facets.begin(); its != facets.end(); its++ )
  {
    if ((*its)->ParentSide()->IsCutSide())
      GmshFacetDump(file,*its,"sides",true,to_local, ele);
  }

  // write details of cut sides
  GmshNewSection( file, "Ele_Facets",true);
  for( plain_facet_set::const_iterator its = facets.begin(); its != facets.end(); its++ )
  {
    if (!(*its)->ParentSide()->IsCutSide())
      GmshFacetDump(file,*its,"sides",true,to_local, ele);
  }

  // write details of volumecells
  GmshNewSection( file, "Volumecells",true);
  const plain_volumecell_set & vcs = ele->VolumeCells();
  for( plain_volumecell_set::const_iterator its = vcs.begin(); its != vcs.end(); its++ )
    GmshVolumecellDump(file,*its,"sides",true,to_local, ele);

  // write details of cut sides
  GmshNewSection( file, "Cut sides",true);
  const plain_side_set & cutsides = ele->CutSides();
  for( plain_side_set::const_iterator its = cutsides.begin(); its != cutsides.end(); its++ )
    GmshSideDump( file, *its, to_local, ele );
  GmshEndSection( file);

  if(ele->HasLevelSetSide())
  {
    GmshNewSection( file, "LevelSetValues");
    GmshLevelSetValueDump(file,ele,true, to_local); //true -> dumps LS values at nodes as well.

    GmshNewSection( file, "LevelSetGradient",true);
    GmshLevelSetGradientDump(file,ele, to_local);

    GmshNewSection( file, "LevelSetOrientation",true);
    GmshLevelSetOrientationDump(file,ele, to_local);

    GmshNewSection( file, "LevelSetZeroShape",true);
    GmshLevelSetValueZeroSurfaceDump(file,ele, to_local);
    GmshEndSection( file);
  }
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given line                                           ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLineDump( std::ofstream & file, GEO::CUT::Line*  line, bool to_local, Element* ele)
{
     GmshLineDump(  file, line->BeginPoint(), line->EndPoint(), to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given line                                           ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLineDump( std::ofstream & file, GEO::CUT::Point* p1, GEO::CUT::Point* p2, bool to_local, Element* ele)
{
     GmshLineDump(  file, p1, p2, p1->Id(), p2->Id(), to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given line                                           ager 08/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLineDump( std::ofstream & file, GEO::CUT::Point* p1, GEO::CUT::Point* p2 ,int idx1,int idx2, bool to_local, Element* ele)
{
     file << "SL (";
     GmshWriteCoords( file, p1, to_local, ele);
     file << ",";
     GmshWriteCoords( file, p2, to_local, ele);
     file << "){";
     file << idx1<< ",";
     file << idx2;
     file << "};\n";
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given edge                                           ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshEdgeDump( std::ofstream & file, GEO::CUT::Edge*  edge, bool to_local, Element* ele)
{
  GmshLineDump(  file, edge->BeginNode()->point(), edge->EndNode()->point(),edge->BeginNode()->Id(), to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given node                                           ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshNodeDump( std::ofstream & file, GEO::CUT::Node*  node, bool to_local, Element* ele)
{
  GmshPointDump(file,node->point(),node->Id(), to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given point                                           ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshPointDump( std::ofstream & file, GEO::CUT::Point*  point, int idx, bool to_local, Element* ele)
{
  file << "SP (";
  GmshWriteCoords( file, point, to_local, ele);
  file << "){";
  file << idx;
  file << "};\n";
}


/*--------------------------------------------------------------------------------------*
 * Write GMSH output of given point                                           ager 04/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshPointDump( std::ofstream & file, GEO::CUT::Point*  point, bool to_local, Element* ele)
{
  GmshPointDump(file,point, point->Position(), to_local, ele);
}

/*--------------------------------------------------------------------------------------*
 * Write Level Set Gradient for given Element
 *
 * The gradients are written at the midpoint of the facets and if the facet is triangulated,
 * also in the midpoint of the triangles.
 *                                                                           winter 07/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLevelSetGradientDump( std::ofstream & file, Element * ele , bool to_local)
{
  const plain_facet_set facets = ele->Facets();
  for(plain_facet_set::const_iterator j=facets.begin();j!=facets.end();j++)
  {
    Facet *facet = *j;

    std::vector<double> normal_triag_midp;
    if(facet->OnCutSide())
    {
      LINALG::Matrix<3,1> facet_triang_midpoint_coord(true);

      if(facet->IsTriangulated())
      {
        std::vector<std::vector<Point*> > facet_triang = facet->Triangulation();
        Point* facet_triang_midpoint = (facet_triang[0])[0];

        facet_triang_midpoint->Coordinates(&facet_triang_midpoint_coord(0,0));
        normal_triag_midp = ele->GetLevelSetGradient(facet_triang_midpoint_coord);

        for(std::vector<std::vector<Point*> >::iterator k=facet_triang.begin(); k!=facet_triang.end();k++)
        {
          std::vector<Point*> facet_triang_tri = *k;

          LINALG::Matrix<3,1> cur;
          LINALG::Matrix<3,1> f_triang_tri_midp(true);
          for( std::vector<Point*>::iterator i=facet_triang_tri.begin();i!=facet_triang_tri.end();i++ )
          {
            Point* p1 = *i;
            p1->Coordinates(cur.A());
            f_triang_tri_midp.Update(1.0,cur,1.0);
          }
          f_triang_tri_midp.Scale(1.0/facet_triang_tri.size());

          std::vector<double> normal = ele->GetLevelSetGradient(f_triang_tri_midp);

          GmshVector(file,f_triang_tri_midp,normal,true, to_local, ele);
        }
      }
      else
      {
        LINALG::Matrix<3,1> cur;
        std::vector<Point*> pts =facet->Points();
        for( std::vector<Point*>::iterator i=pts.begin();i!=pts.end();i++ )
        {
          Point* p1 = *i;
          p1->Coordinates(cur.A());
          facet_triang_midpoint_coord.Update(1.0,cur,1.0);
        }
        facet_triang_midpoint_coord.Scale(1.0/pts.size());
        normal_triag_midp = ele->GetLevelSetGradient(facet_triang_midpoint_coord);
      }

      std::vector<double> normal = ele->GetLevelSetGradient(facet_triang_midpoint_coord);
      GmshVector(file,facet_triang_midpoint_coord,normal,true, to_local, ele);

      //Write Corner-points of LS:
      std::vector<Point*> cornerpts = facet->CornerPoints();
      for(std::vector<Point*>::iterator i=cornerpts.begin();i!=cornerpts.end();i++)
      {
        LINALG::Matrix<3,1> cornercoord;
        Point* p1 = *i;
        p1->Coordinates(cornercoord.A());
        std::vector<double> normal = ele->GetLevelSetGradient(cornercoord);

        GmshVector(file,cornercoord,normal,true, to_local, ele);
      }
    }
  }
}

/*--------------------------------------------------------------------------------------*
 * Write Level Set Values for given Element
 *
 * The LS-value written at the midpoint of the facets and if the facet is triangulated,
 * also in the midpoint of the triangles.
 * Values at the nodes are also written.
 *                                                                           winter 07/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLevelSetValueDump( std::ofstream & file, Element * ele, bool dumpnodevalues , bool to_local)
{
  const plain_facet_set facets = ele->Facets();
  for(plain_facet_set::const_iterator j=facets.begin();j!=facets.end();j++)
  {
    Facet *facet = *j;

    if(facet->OnCutSide())
    {
      LINALG::Matrix<3,1> facet_triang_midpoint_coord(true);

      if(facet->IsTriangulated())
      {
        std::vector<std::vector<Point*> > facet_triang = facet->Triangulation();
        for(std::vector<std::vector<Point*> >::iterator k=facet_triang.begin(); k!=facet_triang.end();k++)
        {
          std::vector<Point*> facet_triang_tri = *k;

          LINALG::Matrix<3,1> cur;
          LINALG::Matrix<3,1> f_triang_tri_midp(true);
          for( std::vector<Point*>::iterator i=facet_triang_tri.begin();i!=facet_triang_tri.end();i++ )
          {
            Point* p1 = *i;
            p1->Coordinates(cur.A());
            f_triang_tri_midp.Update(1.0,cur,1.0);
          }
          f_triang_tri_midp.Scale(1.0/facet_triang_tri.size());

          double ls_value = ele->GetLevelSetValue(f_triang_tri_midp);
          GmshScalar(file, f_triang_tri_midp, ls_value, to_local, ele);

        }
        Point* facet_triang_midpoint = (facet_triang[0])[0];
        facet_triang_midpoint->Coordinates(&facet_triang_midpoint_coord(0,0));
      }
      else
      {
        LINALG::Matrix<3,1> cur;
        std::vector<Point*> pts =facet->Points();
        for( std::vector<Point*>::iterator i=pts.begin();i!=pts.end();i++ )
        {
          Point* p1 = *i;
          p1->Coordinates(cur.A());
          facet_triang_midpoint_coord.Update(1.0,cur,1.0);
        }
        facet_triang_midpoint_coord.Scale(1.0/pts.size());
      }

      double ls_value = ele->GetLevelSetValue(facet_triang_midpoint_coord);
      GmshScalar(file, facet_triang_midpoint_coord, ls_value, to_local, ele);

    }
  }

  if(dumpnodevalues)
  {
    std::vector<Node*> nodes = ele->Nodes();
    for( std::vector<Node*>::iterator j=nodes.begin();j!=nodes.end();j++ )
    {
      Node* node = *j;
      LINALG::Matrix<3,1> node_coord(true);
      node->Coordinates(&node_coord(0,0));

      GmshScalar(file, node_coord, node->LSV(), to_local, ele);
    }
  }
}

/*--------------------------------------------------------------------------------------*
 * Write Level Set Values for given Element
 *
 * The LS-value written at the midpoint of the facets and if the facet is triangulated,
 * also in the midpoint of the triangles.
 * Values at the nodes are also written.
 *                                                                           winter 07/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLevelSetValueZeroSurfaceDump( std::ofstream & file, Element * ele, bool to_local)
{
  std::vector<double> lsv_value(8);

  std::vector<Node*> nodes = ele->Nodes();
  int mm=0;
  for( std::vector<Node*>::iterator j=nodes.begin();j!=nodes.end();j++ )
  {
    Node* node = *j;
    lsv_value[mm] = node->LSV();
    mm++;
  }

  double lsv_max = lsv_value[0];
  double lsv_min = lsv_value[0];

  for(unsigned l=1; l<lsv_value.size(); l++)
  {
    if(lsv_max<lsv_value[l])
      lsv_max = lsv_value[l];

    if(lsv_min>lsv_value[l])
      lsv_min = lsv_value[l];
  }

  //localcoord [-1,-1,-1] x [1,1,1]
  int z_sp =150;
  int y_sp =150;
  int x_sp =150;


  double fac = 5*1e-3;
  double tolerance = (lsv_max-lsv_min)*fac;//(0.001;

//  double* x(3);
  LINALG::Matrix<3,1> coord;

  for(int i = 0; i < x_sp; i++)
  {
    //std::cout << "i: " << i << std::endl;
    coord(0,0) = -1.0 + (2.0/(double(x_sp)-1))*double(i);
    for(int j = 0; j < y_sp; j++)
    {
      //std::cout << "j: " << j << std::endl;
      coord(1,0) = -1.0 + (2.0/(double(y_sp)-1))*double(j);

      for(int k = 0; k < z_sp; k++)
      {
        //std::cout << "k: " << k << std::endl;
        coord(2,0) = -1.0 + (2.0/(double(z_sp)-1))*double(k);

        double ls_value = ele->GetLevelSetValue(coord,true);
        if(fabs(ls_value) < tolerance )
        {
          LINALG::Matrix<3,1> coord_global;
          ele->GlobalCoordinates(coord,coord_global);
          GmshScalar(file, coord_global, ls_value,to_local,ele);
        }
      }
    }
  }
}


/*--------------------------------------------------------------------------------------*
 * Write Level Set Gradient Orientation of Boundary-Cell Normal and LevelSet
 *                                                                           winter 07/15
 *--------------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshLevelSetOrientationDump( std::ofstream & file, Element * ele, bool to_local)
{
  const plain_volumecell_set volcells = ele->VolumeCells();
  for(plain_volumecell_set::const_iterator i=volcells.begin();i!=volcells.end();i++)
  {
    VolumeCell *volcell = *i;

    if(volcell->Position()==GEO::CUT::Point::inside)
      continue;

    //    const plain_facet_set facets = volcells->Facets();

    volcell->BoundaryCells();
    plain_boundarycell_set bc_cells = volcell->BoundaryCells();
    for ( plain_boundarycell_set::iterator j=bc_cells.begin();
        j!=bc_cells.end();
        ++j )
    {
      BoundaryCell * bc = *j;

      //      Facet *facet = *bc->GetFacet();
      LINALG::Matrix<3,1> midpoint_bc;
      bc->ElementCenter(midpoint_bc);

      LINALG::Matrix<3,1> normal_bc;
      LINALG::Matrix<2,1> xsi;
      bc->Normal(xsi,normal_bc);

      std::vector<std::vector<double> > coords_bc = bc->CoordinatesV();
      //const Epetra_SerialDenseMatrix ls_coordEp = bc->Coordinates();
      LINALG::Matrix<3,1> ls_coord(true);
      ls_coord(0,0) = coords_bc[1][0];
      ls_coord(1,0) = coords_bc[1][1];
      ls_coord(2,0) = coords_bc[1][2];

      std::vector<double> normal_ls = ele->GetLevelSetGradient(ls_coord);

      double dotProduct = normal_ls[0]*normal_bc(0,0) + normal_ls[1]*normal_bc(1,0) + normal_ls[2]*normal_bc(2,0);
      GmshScalar(file,midpoint_bc,dotProduct/fabs(dotProduct),to_local,ele);

    }
  }
}

/*!
\brief Write Eqn of plane normal for facet (used for DirectDivergence).
 */

void GEO::CUT::OUTPUT::GmshEqnPlaneNormalDump(std::ofstream & file, Element * ele, bool normalize, bool to_local)
{
  const plain_facet_set facets = ele->Facets();
  for(plain_facet_set::const_iterator j=facets.begin();j!=facets.end();j++)
  {
    Facet *facet = *j;
    GmshEqnPlaneNormalDump(file, facet, normalize,to_local,ele);
  }
}

/*!
\brief Write Eqn of plane normal for all facets (used for DirectDivergence).
 */

void GEO::CUT::OUTPUT::GmshEqnPlaneNormalDump(std::ofstream & file, Facet * facet, bool normalize, bool to_local, Element* ele)
{
  LINALG::Matrix<3,1> facet_triang_midpoint_coord(true);
  std::vector<Point*> f_cornpts = facet->CornerPoints();
  std::vector<double> eqn_plane = GetEqOfPlane(f_cornpts);

  if(facet->IsTriangulated())
  {
    std::vector<std::vector<Point*> > facet_triang = facet->Triangulation();
    Point* facet_triang_midpoint = (facet_triang[0])[0];
    facet_triang_midpoint->Coordinates(&facet_triang_midpoint_coord(0,0));

    for(std::vector<std::vector<Point*> >::iterator k=facet_triang.begin(); k!=facet_triang.end();k++)
    {
      std::vector<Point*> facet_triang_tri = *k;

      LINALG::Matrix<3,1> cur;
      LINALG::Matrix<3,1> f_triang_tri_midp(true);
      for( std::vector<Point*>::iterator i=facet_triang_tri.begin();i!=facet_triang_tri.end();i++ )
      {
        Point* p1 = *i;
        p1->Coordinates(cur.A());
        f_triang_tri_midp.Update(1.0,cur,1.0);
      }
      f_triang_tri_midp.Scale(1.0/facet_triang_tri.size());

      GmshVector(file,f_triang_tri_midp,GetEqOfPlane(facet_triang_tri),normalize,to_local,ele);
    }
  }
  else
  {
    LINALG::Matrix<3,1> cur;
    std::vector<Point*> pts =facet->Points();
    for( std::vector<Point*>::iterator i=pts.begin();i!=pts.end();i++ )
    {
      Point* p1 = *i;
      p1->Coordinates(cur.A());
      facet_triang_midpoint_coord.Update(1.0,cur,1.0);
    }
    facet_triang_midpoint_coord.Scale(1.0/pts.size());
  }

  GmshVector(file,facet_triang_midpoint_coord,eqn_plane,normalize,to_local,ele);
}



void GEO::CUT::OUTPUT::GmshScalar( std::ofstream & file, LINALG::Matrix<3,1> coord, double scalar, bool to_local, Element* ele) //use gmshpoint?
{
  file << "SP(";
  GmshWriteCoords( file, coord , to_local, ele);
  file << "){";
  file << scalar;
  file << "};\n";
}

void GEO::CUT::OUTPUT::GmshVector( std::ofstream & file, LINALG::Matrix<3,1> coord, std::vector<double> vector, bool normalize, bool to_local, Element* ele)
{
  file << "VP(";
  GmshWriteCoords( file, coord, to_local, ele);
  file << "){";

  if(normalize)
  {
    double norm2 = vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2];
    double norm = sqrt(norm2);
    vector[0] = vector[0]/norm;
    vector[1] = vector[1]/norm;
    vector[2] = vector[2]/norm;
  }
  GmshWriteCoords( file , vector, to_local, ele);
  file << "};\n";
}

void GEO::CUT::OUTPUT::GmshWriteCoords(std::ofstream & file, std::vector<double> coord, bool to_local, Element* ele)
{
  if (to_local)
  {
    if (ele == NULL)
      dserror("GmshWriteCoords: Didn't get a parent element for the Coordinate!");

    LINALG::Matrix<3,1> xyz, rst;
    xyz(0,0) = coord[0];
    xyz(1,0) = coord[1];
    xyz(2,0) = coord[2];
    ele->LocalCoordinates(xyz, rst);
    GmshWriteCoords( file , rst, false, NULL); //rst are already local coords!
    return;
  }
  file << coord[0] << "," << coord[1] << "," << coord[2];
}

void GEO::CUT::OUTPUT::GmshWriteCoords(std::ofstream & file, LINALG::Matrix<3,1> coord, bool to_local, Element* ele)
{
  if (to_local)
  {
    if (ele == NULL)
      dserror("GmshWriteCoords: Didn't get a parent element for the Coordinate!");

    LINALG::Matrix<3,1> xyz = coord;
    ele->LocalCoordinates(xyz, coord);
  }
  file << coord( 0, 0 ) << "," << coord( 1, 0 ) << "," << coord( 2, 0 );
}

void GEO::CUT::OUTPUT::GmshWriteCoords(std::ofstream & file, Node* node, bool to_local, Element* ele)
{
  GmshWriteCoords( file, node->point(),to_local, ele);
}


void GEO::CUT::OUTPUT::GmshWriteCoords(std::ofstream & file, Point* point, bool to_local, Element* ele)
{
  LINALG::Matrix<3, 1> coord;
  point->Coordinates(coord.A());

  GmshWriteCoords( file, coord,to_local, ele);
}

std::string GEO::CUT::OUTPUT::GenerateGmshOutputFilename(const std::string& filename_tail)
{
//  std::string filename = DRT::Problem::Instance()->OutputControlFile()->FileName();
  std::string filename("xxx");
  filename.append(filename_tail);
  return filename;
}

void GEO::CUT::OUTPUT::GmshNewSection(std::ofstream & file, const std::string & section, bool first_endsection)
{
  if (first_endsection)
    file << "};\n";
  file << "View \"" << section << "\" {\n";
}

void GEO::CUT::OUTPUT::GmshEndSection(std::ofstream & file, bool close_file)
{
  file << "};\n";
  if (close_file)
    file.close();
}

std::vector<double> GEO::CUT::OUTPUT::GetEqOfPlane(std::vector<Point*> pts)
{
  int mm = 0;

  std::vector< std::vector<double> > corners( pts.size() );

  for(std::vector<Point*>::iterator k=pts.begin(); k!=pts.end();k++)
  {
    Point* p1 = *k;
    LINALG::Matrix<3,1> cur;
    p1->Coordinates(cur.A());

    std::vector<double> pt(3);

    pt[0] = cur(0,0);
    pt[1] = cur(1,0);
    pt[2] = cur(2,0);

    corners[mm] = pt;
    mm++;
  }
  return KERNEL::EqnPlaneOfPolygon( corners );
}


/*-------------------------------------------------------------------------------*
 * Write cuttest for this element!                                     ager 04/15
 *-------------------------------------------------------------------------------*/
void GEO::CUT::OUTPUT::GmshElementCutTest( std::ofstream & file, GEO::CUT::Element* ele, bool haslevelsetside)
{
  std::cout << "Write Cut Test for Element " << ele->Id() << " ... " << std::flush;

  // -- 1 -- header of cut_test -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  file << "// This test was automatically generated by CUT::OUTPUT::GmshElementCutTest(), " << "\n";
  file << "// as the cut crashed for this configuration!" << "\n";
  file << "" << "\n";
  file << "#include <iostream>" << "\n";
  file << "#include <map>" << "\n";
  file << "#include <string>" << "\n";
  file << "#include <vector>" << "\n";
  file << "" << "\n";
  file << "#include \"cut_test_utils.H\"" << "\n";
  file << "" << "\n";
  file << "#include \"../../src/drt_cut/cut_side.H\"" << "\n";
  file << "#include \"../../src/drt_cut/cut_meshintersection.H\"" << "\n";
  file << "#include \"../../src/drt_cut/cut_levelsetintersection.H\"" << "\n";
  file << "#include \"../../src/drt_cut/cut_combintersectio.H\"" << "\n";
  file << "#include \"../../src/drt_cut/cut_tetmeshintersection.H\"" << "\n";
  file << "#include \"../../src/drt_cut/cut_options.H\"" << "\n";
  file << "#include \"../../src/drt_cut/cut_volumecell.H\"" << "\n";
  file << "" << "\n";
  file << "#include \"../../src/drt_fem_general/drt_utils_local_connectivity_matrices.H\"" << "\n";
  file << "" << "\n";
  file << "void test_bacigenerated_" << ele->Id() << "()" << "\n";
  file << "{" << "\n";
  if(not haslevelsetside)
    file << "  GEO::CUT::MeshIntersection intersection;" << "\n";
  else
    file << "  GEO::CUT::CombIntersection intersection(-1);" << "\n";
  file << "  std::vector<int> nids;" << "\n";
  file << "" << "\n";
  file << "  int sidecount = 0;" << "\n";
  file << "  std::vector<double> lsvs("<<ele->Nodes().size()<<");" << "\n";

  // --- get all neighboring elements and cutsides
  plain_element_set eles;
  plain_side_set csides;
  for (std::vector<Side*>::const_iterator sit = ele->Sides().begin(); sit != ele->Sides().end(); ++sit)
  {
    for (plain_element_set::const_iterator eit = (*sit)->Elements().begin(); eit != (*sit)->Elements().end(); ++eit)
    {
      eles.insert(*eit);
      for (plain_side_set::const_iterator csit = (*eit)->CutSides().begin(); csit != (*eit)->CutSides().end(); ++csit)
        csides.insert(*csit);
    }
  }


  if(not haslevelsetside)
  {
    // -- 2 -- add sides -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    const plain_side_set & cutsides = csides;
    for (plain_side_set::const_iterator i = cutsides.begin(); i != cutsides.end();++i)
    {
      file << "  {" << "\n";
      file << "    Epetra_SerialDenseMatrix tri3_xyze( 3, 3 );" << "\n";
      file << "" << "\n";
      Side * s = *i;
      const std::vector<Node*> & side_nodes = s->Nodes();
      int nodelid = -1;
      file << "    nids.clear();" << "\n";
      for (std::vector<Node*>::const_iterator j = side_nodes.begin(); j != side_nodes.end(); ++j)
      {
        nodelid++;
        Node * n = *j;
        for (int dim = 0; dim < 3; ++dim)
        {
          file << "    tri3_xyze(" << dim << "," << nodelid << ") = " << n->point()->X()[dim] << ";" << "\n";
        }
        file << "    nids.push_back( " << n->Id() << " );" << "\n";
      }
      file << "    intersection.AddCutSide( ++sidecount, nids, tri3_xyze, DRT::Element::tri3 );" << "\n";
      file << "  }" << "\n";
    }
  }
  else
  {
    file << "  ci.AddLevelSetSide(1);" << "\n";
    for (uint i = 0; i < ele->Nodes().size(); ++i)
    {
      file << "  lsvs[" << i << "] = " << ele->Nodes()[i]->LSV() << ";" << "\n";
    }
  }

  // -- 3 -- add background element -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
  const plain_element_set & elements = eles;
  for (plain_element_set::const_iterator eit = elements.begin(); eit != elements.end(); ++eit)
  {
    Element* aele = *eit;
    file << "  {" << "\n";
    file << "  Epetra_SerialDenseMatrix hex" << aele->Nodes().size() <<"_xyze( 3, " << aele->Nodes().size() << " );" << "\n";
    file << "" << "\n";
    file << "    nids.clear();" << "\n";
    for (uint i = 0; i < aele->Nodes().size(); ++i)
    {
      for (uint dim = 0; dim < 3; ++dim)
      {
        file << "  hex8_xyze(" << dim << "," << i << ") = " << aele->Nodes()[i]->point()->X()[dim] << ";" << "\n";
      }
      file << "  nids.push_back( " << aele->Nodes()[i]->Id() << " );" << "\n";
    }
    file << "" << "\n";
    if(not haslevelsetside)
      file << "  intersection.AddElement( " << aele->Id() << ", nids, hex8_xyze, DRT::Element::hex8);" << "\n";
    else
      file << "  intersection.AddElement( " << aele->Id() << ", nids, hex8_xyze, DRT::Element::hex8, &lsvs[0], false );" << "\n";
    file << "  }" << "\n";
    file << "" << "\n";
  }
  file << "  intersection.Status();" << "\n";
  file << "" << "\n";
  file << "  intersection.CutTest_Cut( true);" << "\n";
  file << "  intersection.Cut_Finalize( true, INPAR::CUT::VCellGaussPts_Tessellation, INPAR::CUT::BCellGaussPts_Tessellation, false, true );" << "\n";
  file << "" << "\n";

  if(not haslevelsetside && 0)
  {
    // -- 4 -- compare integration methods -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
    file << "  std::vector<double> tessVol,momFitVol,dirDivVol;" << "\n";
    file << "" << "\n";
    file << "  GEO::CUT::Mesh mesh = intersection.NormalMesh();" << "\n";
    file << "  const std::list<Teuchos::RCP<GEO::CUT::VolumeCell> > & other_cells = mesh.VolumeCells();" << "\n";
    file << "  for ( std::list<Teuchos::RCP<GEO::CUT::VolumeCell> >::const_iterator i=other_cells.begin();" << "\n";
    file << "        i!=other_cells.end();" << "\n";
    file << "        ++i )" << "\n";
    file << "  {" << "\n";
    file << "    GEO::CUT::VolumeCell * vc = &**i;" << "\n";
    file << "    tessVol.push_back(vc->Volume());" << "\n";
    file << "  }" << "\n";
    file << "" << "\n";
    file << "  intersection.Status();" << "\n";
    file << "" << "\n";
    file << "  for ( std::list<Teuchos::RCP<GEO::CUT::VolumeCell> >::const_iterator i=other_cells.begin();" << "\n";
    file << "        i!=other_cells.end();" << "\n";
    file << "        ++i )" << "\n";
    file << "  {" << "\n";
    file << "    GEO::CUT::VolumeCell * vc = &**i;" << "\n";
    file << "    vc->MomentFitGaussWeights(vc->ParentElement(),mesh,true,INPAR::CUT::BCellGaussPts_Tessellation);" << "\n";
    file << "    momFitVol.push_back(vc->Volume());" << "\n";
    file << "  }" << "\n";
    file << "" << "\n";
    file << "  for ( std::list<Teuchos::RCP<GEO::CUT::VolumeCell> >::const_iterator i=other_cells.begin();" << "\n";
    file << "           i!=other_cells.end();" << "\n";
    file << "           ++i )" << "\n";
    file << "   {" << "\n";
    file << "     GEO::CUT::VolumeCell * vc = &**i;" << "\n";
    file << "     vc->DirectDivergenceGaussRule(vc->ParentElement(),mesh,true,INPAR::CUT::BCellGaussPts_Tessellation);" << "\n";
    file << "     dirDivVol.push_back(vc->Volume());" << "\n";
    file << "   }" << "\n";
    file << "" << "\n";
    file << "  std::cout<<\"the volumes predicted by\\n tessellation \\t MomentFitting \\t DirectDivergence\\n\";" << "\n";
    file << "  for(unsigned i=0;i<tessVol.size();i++)" << "\n";
    file << "  {" << "\n";
    file << "    std::cout<<tessVol[i]<<\"\\t\"<<momFitVol[i]<<\"\\t\"<<dirDivVol[i]<<\"\\n\";" << "\n";
    file << "    if( fabs(tessVol[i]-momFitVol[i])>1e-9 || fabs(dirDivVol[i]-momFitVol[i])>1e-9 )" << "\n";
    file << "    {" << "\n";
    file << "      mesh.DumpGmsh(\"Cuttest_Debug_Output.pos\");" << "\n";
    file << "      intersection.CutMesh().GetElement(1)->DebugDump();" << "\n";
    file << "      dserror(\"volume predicted by either one of the method is wrong\");" << "\n";
    file << "      }" << "\n";
    file << "    }" << "\n";
  }
    file << "}" << "\n";
    std::cout << "done " << std::endl;
}
