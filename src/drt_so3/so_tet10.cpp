/*!----------------------------------------------------------------------**##
\file so_tet10.cpp
\brief

<pre>
Maintainer: Moritz Frenzel
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15240
writen by : Alexander Volf
			alexander.volf@mytum.de
</pre>

*----------------------------------------------------------------------*/
#ifdef D_SOLID3
#ifdef CCADISCRET

#include "so_tet10.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_dserror.H"




/*----------------------------------------------------------------------***
 |  ctor (public)                                              maf 04/07|
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::So_tet10::So_tet10(int id, int owner) :
DRT::Element(id,element_so_tet10,owner),
material_(0),
data_()
{
  ngp_[0] = ngp_[1] = ngp_[2] = 0; //whatis ngp_ ???????
  surfaces_.resize(0);
  surfaceptrs_.resize(0);
  lines_.resize(0);
  lineptrs_.resize(0);
  return;
}

/*----------------------------------------------------------------------***
 |  copy-ctor (public)                                         maf 04/07|
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::So_tet10::So_tet10(const DRT::ELEMENTS::So_tet10& old) :
DRT::Element(old),
material_(old.material_),
data_(old.data_),
surfaces_(old.surfaces_),
surfaceptrs_(old.surfaceptrs_),
lines_(old.lines_),
lineptrs_(old.lineptrs_)
{
  for (int i=0; i<3; ++i) ngp_[i] = old.ngp_[i];
  return;
}

/*----------------------------------------------------------------------***
 |  Deep copy this instance of Solid3 and return pointer to it (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::So_tet10::Clone() const
{
  DRT::ELEMENTS::So_tet10* newelement = new DRT::ELEMENTS::So_tet10(*this);
  return newelement;
}

/*----------------------------------------------------------------------***
 |                                                             (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::ELEMENTS::So_tet10::Shape() const
{
  return tet10;
}

/*----------------------------------------------------------------------***
 |  Pack data                                                  (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_tet10::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class Element
  vector<char> basedata(0);
  Element::Pack(basedata);
  AddtoPack(data,basedata);
  // ngp_
  //AddtoPack(data,ngp_,3*sizeof(int));
  // material_
  AddtoPack(data,material_);
  // stresstype_
  AddtoPack(data,stresstype_);
  // kintype_
  AddtoPack(data,kintype_);

  // rewind flags
  AddtoPack(data,donerewinding_);


  vector<char> tmp(0);
  data_.Pack(tmp);
  AddtoPack(data,tmp);

  return;
}


/*----------------------------------------------------------------------***
 |  Unpack data                                                (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_tet10::Unpack(const vector<char>& data)
{
  int position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // extract base class Element
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  Element::Unpack(basedata);
  // ngp_
  //ExtractfromPack(position,data,ngp_,3*sizeof(int));
  // material_
  ExtractfromPack(position,data,material_);
  // stresstype_
  ExtractfromPack(position,data,stresstype_);
  // kintype_
  ExtractfromPack(position,data,kintype_);
  // rewinding flags
  ExtractfromPack(position,data,donerewinding_);
  // data_
  vector<char> tmp(0);
  ExtractfromPack(position,data,tmp);
  data_.Unpack(tmp);

  if (position != (int)data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}


/*----------------------------------------------------------------------*
 |  extrapolation of quantities at the GPs to the nodes      lw 03/08   |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_tet10::so_tet10_expol(Epetra_SerialDenseMatrix& stresses,
                                             Epetra_SerialDenseMatrix& nodalstresses)
{
  static Epetra_SerialDenseMatrix expol(NUMNOD_SOTET10,NUMGPT_SOTET10);
  static bool isfilled;

  if (isfilled==true)
  {
    nodalstresses.Multiply('N','N',1.0,expol,stresses,0.0);
  }
  else
  {
    double sq5=sqrt(5);
    expol(0,0)= (0.75+0.05*sq5)*sq5;
    expol(0,1)=-(0.25-0.05*sq5)*sq5;
    expol(0,2)=-(0.25-0.05*sq5)*sq5;
    expol(0,3)=-(0.25-0.05*sq5)*sq5;

    expol(1,0)=-(0.25-0.05*sq5)*sq5;
    expol(1,1)= (0.75+0.05*sq5)*sq5;
    expol(1,2)=-(0.25-0.05*sq5)*sq5;
    expol(1,3)=-(0.25-0.05*sq5)*sq5;

    expol(2,0)=-(0.25-0.05*sq5)*sq5;
    expol(2,1)=-(0.25-0.05*sq5)*sq5;
    expol(2,2)= (0.75+0.05*sq5)*sq5;
    expol(2,3)=-(0.25-0.05*sq5)*sq5;

    expol(3,0)=-(0.25-0.05*sq5)*sq5;
    expol(3,1)=-(0.25-0.05*sq5)*sq5;
    expol(3,2)=-(0.25-0.05*sq5)*sq5;
    expol(3,3)= (0.75+0.05*sq5)*sq5;

    expol(4,0)= (0.25+0.05*sq5)*sq5;
    expol(4,1)= (0.25+0.05*sq5)*sq5;
    expol(4,2)=-(0.25-0.05*sq5)*sq5;
    expol(4,3)=-(0.25-0.05*sq5)*sq5;

    expol(5,0)=-(0.25-0.05*sq5)*sq5;
    expol(5,1)= (0.25+0.05*sq5)*sq5;
    expol(5,2)= (0.25+0.05*sq5)*sq5;
    expol(5,3)=-(0.25-0.05*sq5)*sq5;

    expol(6,0)= (0.25+0.05*sq5)*sq5;
    expol(6,1)=-(0.25-0.05*sq5)*sq5;
    expol(6,2)= (0.25+0.05*sq5)*sq5;
    expol(6,3)=-(0.25-0.05*sq5)*sq5;

    expol(7,0)= (0.25+0.05*sq5)*sq5;
    expol(7,1)=-(0.25-0.05*sq5)*sq5;
    expol(7,2)=-(0.25-0.05*sq5)*sq5;
    expol(7,3)= (0.25+0.05*sq5)*sq5;

    expol(8,0)=-(0.25-0.05*sq5)*sq5;
    expol(8,1)= (0.25+0.05*sq5)*sq5;
    expol(8,2)=-(0.25-0.05*sq5)*sq5;
    expol(8,3)= (0.25+0.05*sq5)*sq5;

    expol(9,0)=-(0.25-0.05*sq5)*sq5;
    expol(9,1)=-(0.25-0.05*sq5)*sq5;
    expol(9,2)= (0.25+0.05*sq5)*sq5;
    expol(9,3)= (0.25+0.05*sq5)*sq5;

    nodalstresses.Multiply('N','N',1.0,expol,stresses,0.0);

    isfilled = true;
  }
}


/*----------------------------------------------------------------------***
 |  dtor (public)                                              maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::So_tet10::~So_tet10()
{
  return;
}


/*----------------------------------------------------------------------***
 |  print this element (public)                                maf 04/07|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::So_tet10::Print(ostream& os) const
{
  os << "So_tet10 ";
  Element::Print(os);
  cout << endl;
  cout << data_;
  return;
}

/*------------------------------------------------------------------------***
 |  allocate and return So_tet10Register (public)               volf 06/07|
 *------------------------------------------------------------------------*/
RefCountPtr<DRT::ElementRegister> DRT::ELEMENTS::So_tet10::ElementRegister() const
{
  return rcp(new DRT::ELEMENTS::Sotet10Register(Type()));
}

  /*====================================================================*/
  /* 10-node tetrahedra node topology*/
  /*--------------------------------------------------------------------*/
  /* parameter coordinates (ksi1, ksi2, ksi3, ksi4) of nodes
   * of a common tetrahedron [-1,1]x[-1,1]x[-1,1]
   *  10-node hexahedron: node 0,1,...,9
   *
   * -----------------------
   *- this is the numbering used in GiD & EXODUS!!
   *      3-
   *      |\ ---
   *      |  \    --9
   *      |    \      ---
   *      |      \        -2
   *      |        \       /\
   *      |          \   /   \
   *      7            8      \
   *      |          /   \     \
   *      |        6       \    5
   *      |      /           \   \
   *      |    /               \  \
   *      |  /                   \ \
   *      |/                       \\
   *      0------------4-------------1
   */
  /*====================================================================*/

/*----------------------------------------------------------------------***
 |  get vector of volumes (length 1) (public)                  maf 04/07|
 *----------------------------------------------------------------------*/
DRT::Element** DRT::ELEMENTS::So_tet10::Volumes()
{
  volume_.resize(1);
  return 0;
}


 /*----------------------------------------------------------------------**#
 |  get vector of surfaces (public)                             maf 04/07|
 |  surface normals always point outward                                 |
 *----------------------------------------------------------------------*/
DRT::Element** DRT::ELEMENTS::So_tet10::Surfaces()
{

  const int nsurf = NumSurface();
  surfaces_.resize(nsurf);
  surfaceptrs_.resize(nsurf);
  int nodeids[100];
  DRT::Node* nodes[100];

  nodeids[0] = NodeIds()[0];
  nodeids[1] = NodeIds()[1];
  nodeids[2] = NodeIds()[3];
  nodeids[3] = NodeIds()[4];
  nodeids[4] = NodeIds()[8];
  nodeids[5] = NodeIds()[7];
  nodes[0] = Nodes()[0];
  nodes[1] = Nodes()[1];
  nodes[2] = Nodes()[3];
  nodes[3] = Nodes()[4];
  nodes[4] = Nodes()[8];
  nodes[5] = Nodes()[7];
  surfaces_[0] =
    rcp(new DRT::ELEMENTS::Sotet10Surface(0,Owner(),6,nodeids,nodes,this,0));
  surfaceptrs_[0] = surfaces_[0].get();

  nodeids[0] = NodeIds()[1];
  nodeids[1] = NodeIds()[2];
  nodeids[2] = NodeIds()[3];
  nodeids[3] = NodeIds()[5];
  nodeids[4] = NodeIds()[9];
  nodeids[5] = NodeIds()[8];
  nodes[0] = Nodes()[1];
  nodes[1] = Nodes()[2];
  nodes[2] = Nodes()[3];
  nodes[3] = Nodes()[5];
  nodes[4] = Nodes()[9];
  nodes[5] = Nodes()[8];
  surfaces_[1] =
    rcp(new DRT::ELEMENTS::Sotet10Surface(1,Owner(),6,nodeids,nodes,this,1));
  surfaceptrs_[1] = surfaces_[1].get();

  nodeids[0] = NodeIds()[0];
  nodeids[1] = NodeIds()[3];
  nodeids[2] = NodeIds()[2];
  nodeids[3] = NodeIds()[7];
  nodeids[4] = NodeIds()[9];
  nodeids[5] = NodeIds()[6];
  nodes[0] = Nodes()[0];
  nodes[1] = Nodes()[3];
  nodes[2] = Nodes()[2];
  nodes[3] = Nodes()[7];
  nodes[4] = Nodes()[9];
  nodes[5] = Nodes()[6];
  surfaces_[2] =
    rcp(new DRT::ELEMENTS::Sotet10Surface(2,Owner(),6,nodeids,nodes,this,2));
  surfaceptrs_[2] = surfaces_[2].get();

  nodeids[0] = NodeIds()[0];
  nodeids[1] = NodeIds()[2];
  nodeids[2] = NodeIds()[1];
  nodeids[3] = NodeIds()[6];
  nodeids[4] = NodeIds()[5];
  nodeids[5] = NodeIds()[4];
  nodes[0] = Nodes()[0];
  nodes[1] = Nodes()[2];
  nodes[2] = Nodes()[1];
  nodes[3] = Nodes()[6];
  nodes[4] = Nodes()[5];
  nodes[5] = Nodes()[4];
  surfaces_[3] =
    rcp(new DRT::ELEMENTS::Sotet10Surface(3,Owner(),6,nodeids,nodes,this,3));
  surfaceptrs_[3] = surfaces_[3].get();

  return (DRT::Element**)(&(surfaceptrs_[0]));

  return 0;
}

/*----------------------------------------------------------------------***++
 |  get vector of lines (public)                               maf 04/07|
 *----------------------------------------------------------------------*/
DRT::Element** DRT::ELEMENTS::So_tet10::Lines()
{
  const int nline = NumLine();
  lines_.resize(nline);
  lineptrs_.resize(nline);
  int nodeids[100];
  DRT::Node* nodes[100];

  nodeids[0] = NodeIds()[0];
  nodeids[1] = NodeIds()[1];
  nodeids[2] = NodeIds()[4];
  nodes[0] = Nodes()[0];
  nodes[1] = Nodes()[1];
  nodes[2] = Nodes()[4];
  lines_[0] =
    rcp(new DRT::ELEMENTS::Sotet10Line(0,Owner(),3,nodeids,nodes,this,0));
  lineptrs_[0] = lines_[0].get();

   nodeids[0] = NodeIds()[1];
  nodeids[1] = NodeIds()[2];
  nodeids[2] = NodeIds()[5];
  nodes[0] = Nodes()[1];
  nodes[1] = Nodes()[2];
  nodes[2] = Nodes()[5];
  lines_[1] =
    rcp(new DRT::ELEMENTS::Sotet10Line(1,Owner(),3,nodeids,nodes,this,1));
  lineptrs_[1] = lines_[1].get();

  nodeids[0] = NodeIds()[0];
  nodeids[1] = NodeIds()[2];
  nodeids[2] = NodeIds()[5];
  nodes[0] = Nodes()[0];
  nodes[1] = Nodes()[2];
  nodes[2] = Nodes()[6];
  lines_[2] =
    rcp(new DRT::ELEMENTS::Sotet10Line(2,Owner(),3,nodeids,nodes,this,2));
  lineptrs_[2] = lines_[2].get();

  nodeids[0] = NodeIds()[0];
  nodeids[1] = NodeIds()[3];
  nodeids[2] = NodeIds()[7];
  nodes[0] = Nodes()[0];
  nodes[1] = Nodes()[3];
  nodes[2] = Nodes()[7];
  lines_[3] =
    rcp(new DRT::ELEMENTS::Sotet10Line(3,Owner(),3,nodeids,nodes,this,3));
  lineptrs_[3] = lines_[3].get();

  nodeids[0] = NodeIds()[1];
  nodeids[1] = NodeIds()[3];
  nodeids[2] = NodeIds()[8];
  nodes[0] = Nodes()[1];
  nodes[1] = Nodes()[3];
  nodes[2] = Nodes()[8];
  lines_[4] =
    rcp(new DRT::ELEMENTS::Sotet10Line(4,Owner(),3,nodeids,nodes,this,4));
  lineptrs_[4] = lines_[4].get();

  nodeids[0] = NodeIds()[2];
  nodeids[1] = NodeIds()[3];
  nodeids[2] = NodeIds()[9];
  nodes[0] = Nodes()[2];
  nodes[1] = Nodes()[3];
  nodes[2] = Nodes()[9];
  lines_[5] =
    rcp(new DRT::ELEMENTS::Sotet10Line(5,Owner(),3,nodeids,nodes,this,5));
  lineptrs_[5] = lines_[5].get();

  return (DRT::Element**)(&(lineptrs_[0]));

  return 0;
}


//=======================================================================
//=======================================================================
//=======================================================================
//=======================================================================

/*----------------------------------------------------------------------***
 |  ctor (public)                                              maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Sotet10Register::Sotet10Register(DRT::Element::ElementType etype) :
ElementRegister(etype)
{
  return;
}

/*----------------------------------------------------------------------***
 |  copy-ctor (public)                                         maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Sotet10Register::Sotet10Register(
                               const DRT::ELEMENTS::Sotet10Register& old) :
ElementRegister(old)
{
  return;
}

/*----------------------------------------------------------------------***
 |  Deep copy this instance return pointer to it               (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Sotet10Register* DRT::ELEMENTS::Sotet10Register::Clone() const
{
//  return new DRT::ELEMENTS::Soh8Register(*this);
  return new DRT::ELEMENTS::Sotet10Register(*this);
}

/*----------------------------------------------------------------------***
 |  Pack data                                                  (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Sotet10Register::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class ElementRegister
  vector<char> basedata(0);
  ElementRegister::Pack(basedata);
  AddtoPack(data,basedata);

  return;
}


/*----------------------------------------------------------------------***
 |  Unpack data                                                (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
//void DRT::ELEMENTS::Soh8Register::Unpack(const vector<char>& data)
void DRT::ELEMENTS::Sotet10Register::Unpack(const vector<char>& data)
{
  int position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // base class ElementRegister
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  ElementRegister::Unpack(basedata);

  if (position != (int)data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}


/*----------------------------------------------------------------------***
 |  dtor (public)                                              maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Sotet10Register::~Sotet10Register()
{
  return;
}

/*----------------------------------------------------------------------***
 |  print (public)                                             maf 04/07|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Sotet10Register::Print(ostream& os) const
{
  os << "Sotet10Register ";
  ElementRegister::Print(os);
  return;
}

#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_SOLID3
