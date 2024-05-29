/*----------------------------------------------------------------------------*/
/*! \file

\brief A nurbs implementation of the ale3 element

\level 2

*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include "4C_ale_ale3_nurbs.hpp"

#include "4C_so3_nullspace.hpp"

FOUR_C_NAMESPACE_OPEN

DRT::ELEMENTS::NURBS::Ale3NurbsType DRT::ELEMENTS::NURBS::Ale3NurbsType::instance_;

DRT::ELEMENTS::NURBS::Ale3NurbsType& DRT::ELEMENTS::NURBS::Ale3NurbsType::Instance()
{
  return instance_;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::COMM::ParObject* DRT::ELEMENTS::NURBS::Ale3NurbsType::Create(const std::vector<char>& data)
{
  DRT::ELEMENTS::NURBS::Ale3Nurbs* object = new DRT::ELEMENTS::NURBS::Ale3Nurbs(-1, -1);
  object->Unpack(data);
  return object;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<CORE::Elements::Element> DRT::ELEMENTS::NURBS::Ale3NurbsType::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == "ALE3")
  {
    if (eledistype == "NURBS8" || eledistype == "NURBS27")
    {
      return Teuchos::rcp(new DRT::ELEMENTS::NURBS::Ale3Nurbs(id, owner));
    }
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<CORE::Elements::Element> DRT::ELEMENTS::NURBS::Ale3NurbsType::Create(
    const int id, const int owner)
{
  return Teuchos::rcp(new DRT::ELEMENTS::NURBS::Ale3Nurbs(id, owner));
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::NURBS::Ale3NurbsType::nodal_block_information(
    CORE::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = 3;
  dimns = 6;
  nv = 3;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::LINALG::SerialDenseMatrix DRT::ELEMENTS::NURBS::Ale3NurbsType::ComputeNullSpace(
    CORE::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  return ComputeSolid3DNullSpace(node, x0);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::NURBS::Ale3Nurbs::Ale3Nurbs(int id, int owner) : DRT::ELEMENTS::Ale3::Ale3(id, owner)
{
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::NURBS::Ale3Nurbs::Ale3Nurbs(const DRT::ELEMENTS::NURBS::Ale3Nurbs& old)
    : DRT::ELEMENTS::Ale3::Ale3(old)
{
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::NURBS::Ale3Nurbs::Print(std::ostream& os) const
{
  os << "Ale3Nurbs ";
  Element::Print(os);
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::FE::CellType DRT::ELEMENTS::NURBS::Ale3Nurbs::Shape() const
{
  switch (num_node())
  {
    case 8:
      return CORE::FE::CellType::nurbs8;
    case 27:
      return CORE::FE::CellType::nurbs27;
    default:
      FOUR_C_THROW("unexpected number of nodes %d", num_node());
      break;
  }
}

FOUR_C_NAMESPACE_CLOSE
