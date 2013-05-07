/*----------------------------------------------------------------------*/
/*!
 \file wall1_poro_eletypes.cpp

 \brief

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *----------------------------------------------------------------------*/


#include "wall1_poro_eletypes.H"
#include "wall1_poro.H"

#include "../drt_lib/drt_linedefinition.H"
#include "../drt_lib/drt_discret.H"

/*----------------------------------------------------------------------*
 |  QUAD 4 Element                                       |
 *----------------------------------------------------------------------*/

DRT::ELEMENTS::WallQuad4PoroType DRT::ELEMENTS::WallQuad4PoroType::instance_;


DRT::ParObject* DRT::ELEMENTS::WallQuad4PoroType::Create( const std::vector<char> & data )
{
  DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad4>* object = new DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad4>(-1,-1);
  object->Unpack(data);
  return object;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::WallQuad4PoroType::Create( const std::string eletype,
                                                            const std::string eledistype,
                                                            const int id,
                                                            const int owner )
{
  if ( eletype=="WALLQ4PORO" )
  {
    Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad4>(id,owner));
    return ele;
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::WallQuad4PoroType::Create( const int id, const int owner )
{
  Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad4>(id,owner));
  return ele;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::WallQuad4PoroType::SetupElementDefinition( std::map<std::string,std::map<std::string,DRT::INPUT::LineDefinition> > & definitions )
{

  std::map<std::string,std::map<std::string,DRT::INPUT::LineDefinition> >  definitions_wall;
  Wall1Type::SetupElementDefinition(definitions_wall);

  std::map<std::string, DRT::INPUT::LineDefinition>& defs_wall =
      definitions_wall["WALL"];

  std::map<std::string, DRT::INPUT::LineDefinition>& defs =
      definitions["WALLQ4PORO"];

  defs["QUAD4"]=defs_wall["QUAD4"];
}

/*----------------------------------------------------------------------*
 |  init the element (public)                                           |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::WallQuad4PoroType::Initialize(DRT::Discretization& dis)
{
  DRT::ELEMENTS::Wall1Type::Initialize(dis);
  for (int i=0; i<dis.NumMyColElements(); ++i)
  {
    if (dis.lColElement(i)->ElementType() != *this) continue;
    DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad4>* actele = dynamic_cast<DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad4>*>(dis.lColElement(i));
    if (!actele) dserror("cast to Wall1_Poro* failed");
    actele->InitElement();
  }
  return 0;
}

/*----------------------------------------------------------------------*
 |  QUAD 9 Element                                       |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::WallQuad9PoroType DRT::ELEMENTS::WallQuad9PoroType::instance_;


DRT::ParObject* DRT::ELEMENTS::WallQuad9PoroType::Create( const std::vector<char> & data )
{
  DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad9>* object = new DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad9>(-1,-1);
  object->Unpack(data);
  return object;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::WallQuad9PoroType::Create( const std::string eletype,
                                                            const std::string eledistype,
                                                            const int id,
                                                            const int owner )
{
  if ( eletype=="WALLQ9PORO" )
  {
    Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad9>(id,owner));
    return ele;
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::WallQuad9PoroType::Create( const int id, const int owner )
{
  Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad9>(id,owner));
  return ele;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::WallQuad9PoroType::SetupElementDefinition( std::map<std::string,std::map<std::string,DRT::INPUT::LineDefinition> > & definitions )
{

  std::map<std::string,std::map<std::string,DRT::INPUT::LineDefinition> >  definitions_wall;
  Wall1Type::SetupElementDefinition(definitions_wall);

  std::map<std::string, DRT::INPUT::LineDefinition>& defs_wall =
      definitions_wall["WALL"];

  std::map<std::string, DRT::INPUT::LineDefinition>& defs =
      definitions["WALLQ9PORO"];

  defs["QUAD9"]=defs_wall["QUAD9"];
}

/*----------------------------------------------------------------------*
 |  init the element (public)                                           |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::WallQuad9PoroType::Initialize(DRT::Discretization& dis)
{
  DRT::ELEMENTS::Wall1Type::Initialize(dis);
  for (int i=0; i<dis.NumMyColElements(); ++i)
  {
    if (dis.lColElement(i)->ElementType() != *this) continue;
    DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad9>* actele = dynamic_cast<DRT::ELEMENTS::Wall1_Poro<DRT::Element::quad9>*>(dis.lColElement(i));
    if (!actele) dserror("cast to Wall1_Poro* failed");
    actele->InitElement();
  }
  return 0;
}
