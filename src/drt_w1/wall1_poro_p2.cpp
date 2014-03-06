/*----------------------------------------------------------------------*/
/*!
 \file wall1_poro_p2.cpp

 \brief

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15251
 </pre>
 *----------------------------------------------------------------------*/


#include "wall1_poro_p2.H"
#include "wall1_poro_p2_eletypes.H"

#include "../drt_lib/drt_utils_factory.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::Wall1_PoroP2<distype>::Wall1_PoroP2(int id, int owner) :
DRT::ELEMENTS::Wall1_Poro<distype>(id,owner)
{
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                                  |
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::Wall1_PoroP2<distype>::Wall1_PoroP2(const DRT::ELEMENTS::Wall1_PoroP2<distype>& old) :
DRT::ELEMENTS::Wall1_Poro<distype>(old)
{

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::Element* DRT::ELEMENTS::Wall1_PoroP2<distype>::Clone() const
{
  DRT::ELEMENTS::Wall1_PoroP2<distype>* newelement = new DRT::ELEMENTS::Wall1_PoroP2<distype>(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Wall1_PoroP2<distype>::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm( data );
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  my::AddtoPack(data,type);

  // add base class Element
  my::Pack(data);

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Wall1_PoroP2<distype>::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  // extract type
  int type = 0;
  my::ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");

  // extract base class Element
  std::vector<char> basedata(0);
  my::ExtractfromPack(position,data,basedata);
  my::Unpack(basedata);

  if (position != data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
std::vector<Teuchos::RCP<DRT::Element> >  DRT::ELEMENTS::Wall1_PoroP2<distype>::Lines()
{
  // do NOT store line or surface elements inside the parent element
  // after their creation.
  // Reason: if a Redistribute() is performed on the discretization,
  // stored node ids and node pointers owned by these boundary elements might
  // have become illegal and you will get a nice segmentation fault ;-)

  // so we have to allocate new line elements:
  return DRT::UTILS::ElementBoundaryFactory<Wall1Line,Wall1_PoroP2>(DRT::UTILS::buildLines,this);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
std::vector<Teuchos::RCP<DRT::Element> >  DRT::ELEMENTS::Wall1_PoroP2<distype>::Surfaces()
{
  std::vector<Teuchos::RCP<Element> > surfaces(1);
  surfaces[0]= Teuchos::rcp(this, false);
  return surfaces;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::Wall1_PoroP2<distype>::Print(std::ostream& os) const
{
  os << "Wall1_PoroP2 ";
  Element::Print(os);
  std::cout << std::endl;
  std::cout << my::data_;
  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::Wall1_PoroP2<distype>::UniqueParObjectId() const
{
  switch(distype)
  {
  case DRT::Element::quad4:
    return DRT::ELEMENTS::WallQuad4PoroP2Type::Instance().UniqueParObjectId();
    break;
  case DRT::Element::quad9:
    return DRT::ELEMENTS::WallQuad9PoroP2Type::Instance().UniqueParObjectId();
    break;
  default:
    dserror("unknown element type");
    break;
  }
  return -1;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
template<DRT::Element::DiscretizationType distype>
DRT::ElementType & DRT::ELEMENTS::Wall1_PoroP2<distype>::ElementType() const
{
  {
    switch(distype)
    {
    case DRT::Element::quad4:
      return DRT::ELEMENTS::WallQuad4PoroP2Type::Instance();
      break;
    case DRT::Element::quad9:
      return DRT::ELEMENTS::WallQuad9PoroP2Type::Instance();
      break;
    default:
      dserror("unknown element type");
      break;
    }
    return DRT::ELEMENTS::WallQuad4PoroP2Type::Instance();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template class DRT::ELEMENTS::Wall1_PoroP2<DRT::Element::quad4>;
template class DRT::ELEMENTS::Wall1_PoroP2<DRT::Element::quad9>;



