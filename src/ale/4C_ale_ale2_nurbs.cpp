/*----------------------------------------------------------------------------*/
/*! \file

\brief Nurbs verison of 2D ALE element

\level 3

*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include "4C_ale_ale2_nurbs.hpp"

FOUR_C_NAMESPACE_OPEN

DRT::ELEMENTS::NURBS::Ale2NurbsType DRT::ELEMENTS::NURBS::Ale2NurbsType::instance_;

DRT::ELEMENTS::NURBS::Ale2NurbsType& DRT::ELEMENTS::NURBS::Ale2NurbsType::Instance()
{
  return instance_;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::COMM::ParObject* DRT::ELEMENTS::NURBS::Ale2NurbsType::Create(const std::vector<char>& data)
{
  DRT::ELEMENTS::NURBS::Ale2Nurbs* object = new DRT::ELEMENTS::NURBS::Ale2Nurbs(-1, -1);
  object->Unpack(data);
  return object;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::NURBS::Ale2NurbsType::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == "ALE2")
  {
    if (eledistype == "NURBS4" || eledistype == "NURBS9")
    {
      return Teuchos::rcp(new DRT::ELEMENTS::NURBS::Ale2Nurbs(id, owner));
    }
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::NURBS::Ale2NurbsType::Create(
    const int id, const int owner)
{
  return Teuchos::rcp(new DRT::ELEMENTS::NURBS::Ale2Nurbs(id, owner));
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::NURBS::Ale2Nurbs::Ale2Nurbs(int id, int owner) : DRT::ELEMENTS::Ale2::Ale2(id, owner)
{
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::NURBS::Ale2Nurbs::Ale2Nurbs(const DRT::ELEMENTS::NURBS::Ale2Nurbs& old)
    : DRT::ELEMENTS::Ale2::Ale2(old)
{
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::NURBS::Ale2Nurbs::Print(std::ostream& os) const
{
  os << "Ale2Nurbs ";
  Element::Print(os);
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::FE::CellType DRT::ELEMENTS::NURBS::Ale2Nurbs::Shape() const
{
  switch (num_node())
  {
    case 4:
      return CORE::FE::CellType::nurbs4;
    case 9:
      return CORE::FE::CellType::nurbs9;
    default:
      FOUR_C_THROW("unexpected number of nodes %d", num_node());
      break;
  }
}

FOUR_C_NAMESPACE_CLOSE
