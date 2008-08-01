/*!----------------------------------------------------------------------
\file truss3.cpp
\brief three dimensional total Lagrange truss element

<pre>
Maintainer: Christian Cyron
            cyron@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15264
</pre>

*----------------------------------------------------------------------*/
#ifdef D_TRUSS3
#ifdef CCADISCRET

#include "truss3.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_elementregister.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_dserror.H"

/*----------------------------------------------------------------------*
 |  ctor (public)                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3::Truss3(int id, int owner) :
DRT::Element(id,element_truss3,owner),
data_(),
material_(0),
lrefe_(0),
crosssec_(0),

//note: for corotational approach integration for Neumann conditions only
//hence enough to integrate 3rd order polynomials exactly
gaussrule_(DRT::UTILS::intrule_line_2point)
{
  return;
}
/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3::Truss3(const DRT::ELEMENTS::Truss3& old) :
 DRT::Element(old),
 data_(old.data_),
 material_(old.material_),
 lrefe_(old.lrefe_),
 crosssec_(old.crosssec_),
 gaussrule_(old.gaussrule_)
{
  return;
}
/*----------------------------------------------------------------------*
 |  Deep copy this instance of Truss3 and return pointer to it (public) |
 |                                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::Truss3::Clone() const
{
  DRT::ELEMENTS::Truss3* newelement = new DRT::ELEMENTS::Truss3(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3::~Truss3()
{
  return;
}


/*----------------------------------------------------------------------*
 |  print this element (public)                              cyron 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Truss3::Print(ostream& os) const
{
  os << "Truss3 ";
  Element::Print(os);
  os << " gaussrule_: " << gaussrule_ << " ";
  return;
}

/*----------------------------------------------------------------------*
 |  allocate and return Truss3Register (public)               cyron 08/08|
 *----------------------------------------------------------------------*/
RefCountPtr<DRT::ElementRegister> DRT::ELEMENTS::Truss3::ElementRegister() const
{
  return rcp(new DRT::ELEMENTS::Truss3Register(Type()));
}


/*----------------------------------------------------------------------*
 |(public)                                                   cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::ELEMENTS::Truss3::Shape() const
{
  return line2;
}


/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                           cyron 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Truss3::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class Element
  vector<char> basedata(0);
  Element::Pack(basedata);
  AddtoPack(data,basedata);
  //material type
  AddtoPack(data,material_);
  //reference length
  AddtoPack(data,lrefe_);
  //cross section
  AddtoPack(data,crosssec_);
  // gaussrule_
  AddtoPack(data,gaussrule_); //implicit conversion from enum to integer
  vector<char> tmp(0);
  data_.Pack(tmp);
  AddtoPack(data,tmp);

  return;
}


/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                           cyron 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Truss3::Unpack(const vector<char>& data)
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
  //material type
  ExtractfromPack(position,data,material_);
  //reference length
  ExtractfromPack(position,data,lrefe_);
  //cross section
  ExtractfromPack(position,data,crosssec_);
  // gaussrule_
  int gausrule_integer;
  ExtractfromPack(position,data,gausrule_integer);
  gaussrule_ = DRT::UTILS::GaussRule1D(gausrule_integer); //explicit conversion from integer to enum
  vector<char> tmp(0);
  ExtractfromPack(position,data,tmp);
  data_.Unpack(tmp);

  if (position != (int)data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}

/*----------------------------------------------------------------------*
 |  get vector of lines (public)                              cyron 08/08|
 *----------------------------------------------------------------------*/
vector<RCP<DRT::Element> > DRT::ELEMENTS::Truss3::Lines()
{
  vector<RCP<Element> > lines(1);
  lines[0]= rcp(this, false);
  return lines;
}




//------------- class Truss3Register: -------------------------------------


/*----------------------------------------------------------------------*
 |  ctor (public)                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3Register::Truss3Register(DRT::Element::ElementType etype):
ElementRegister(etype)
{
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3Register::Truss3Register(
                               const DRT::ELEMENTS::Truss3Register& old) :
ElementRegister(old)
{
  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance return pointer to it               (public) |
 |                                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3Register* DRT::ELEMENTS::Truss3Register::Clone() const
{
  return new DRT::ELEMENTS::Truss3Register(*this);
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                            cyron 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Truss3Register::Pack(vector<char>& data) const
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


/*-----------------------------------------------------------------------*
 |  Unpack data (public)                                      cyron 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Truss3Register::Unpack(const vector<char>& data)
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


/*----------------------------------------------------------------------*
 |  dtor (public)                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Truss3Register::~Truss3Register()
{
  return;
}

/*----------------------------------------------------------------------*
 |  print (public)                                           cyron 08/08|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Truss3Register::Print(ostream& os) const
{
  os << "Truss3Register ";
  ElementRegister::Print(os);
  return;
}


int DRT::ELEMENTS::Truss3Register::Initialize(DRT::Discretization& dis)
{		
  //variable for nodal point coordinates in reference configuration
  BlitzVec6 xrefe;
  
  //setting beam reference director correctly
  for (int i=0; i<  dis.NumMyColElements(); ++i)
    {    
      //in case that current element is not a beam3 element there is nothing to do and we go back
      //to the head of the loop
      if (dis.lColElement(i)->Type() != DRT::Element::element_truss3) continue;
      
      //if we get so far current element is a beam3 element and  we get a pointer at it
      DRT::ELEMENTS::Truss3* currele = dynamic_cast<DRT::ELEMENTS::Truss3*>(dis.lColElement(i));
      if (!currele) dserror("cast to Truss3* failed");
      
      //getting element's reference coordinates     
      for (int k=0; k<2; ++k) //element has two nodes
        {
          xrefe(3*k + 0) = currele->Nodes()[k]->X()[0];
          xrefe(3*k + 1) = currele->Nodes()[k]->X()[1];
          xrefe(3*k + 2) = currele->Nodes()[k]->X()[2];
        }
      
      //length in reference configuration
      currele->lrefe_ = pow(pow(xrefe(3)-xrefe(0),2)+pow(xrefe(4)-xrefe(1),2)+pow(xrefe(5)-xrefe(2),2),0.5);  
          
    } //for (int i=0; i<dis_.NumMyColElements(); ++i)
	
  return 0;
}


#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_TRUSS3
