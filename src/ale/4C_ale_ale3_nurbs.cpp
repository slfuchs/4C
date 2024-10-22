// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_ale_ale3_nurbs.hpp"

#include "4C_so3_nullspace.hpp"

FOUR_C_NAMESPACE_OPEN

Discret::ELEMENTS::Nurbs::Ale3NurbsType Discret::ELEMENTS::Nurbs::Ale3NurbsType::instance_;

Discret::ELEMENTS::Nurbs::Ale3NurbsType& Discret::ELEMENTS::Nurbs::Ale3NurbsType::instance()
{
  return instance_;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Core::Communication::ParObject* Discret::ELEMENTS::Nurbs::Ale3NurbsType::create(
    Core::Communication::UnpackBuffer& buffer)
{
  Discret::ELEMENTS::Nurbs::Ale3Nurbs* object = new Discret::ELEMENTS::Nurbs::Ale3Nurbs(-1, -1);
  object->unpack(buffer);
  return object;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::Nurbs::Ale3NurbsType::create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == "ALE3")
  {
    if (eledistype == "NURBS8" || eledistype == "NURBS27")
    {
      return Teuchos::make_rcp<Discret::ELEMENTS::Nurbs::Ale3Nurbs>(id, owner);
    }
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::Nurbs::Ale3NurbsType::create(
    const int id, const int owner)
{
  return Teuchos::make_rcp<Discret::ELEMENTS::Nurbs::Ale3Nurbs>(id, owner);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void Discret::ELEMENTS::Nurbs::Ale3NurbsType::nodal_block_information(
    Core::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = 3;
  dimns = 6;
  nv = 3;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Core::LinAlg::SerialDenseMatrix Discret::ELEMENTS::Nurbs::Ale3NurbsType::compute_null_space(
    Core::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  return compute_solid_3d_null_space(node, x0);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Discret::ELEMENTS::Nurbs::Ale3Nurbs::Ale3Nurbs(int id, int owner)
    : Discret::ELEMENTS::Ale3::Ale3(id, owner)
{
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Discret::ELEMENTS::Nurbs::Ale3Nurbs::Ale3Nurbs(const Discret::ELEMENTS::Nurbs::Ale3Nurbs& old)
    : Discret::ELEMENTS::Ale3::Ale3(old)
{
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void Discret::ELEMENTS::Nurbs::Ale3Nurbs::print(std::ostream& os) const
{
  os << "Ale3Nurbs ";
  Element::print(os);
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Core::FE::CellType Discret::ELEMENTS::Nurbs::Ale3Nurbs::shape() const
{
  switch (num_node())
  {
    case 8:
      return Core::FE::CellType::nurbs8;
    case 27:
      return Core::FE::CellType::nurbs27;
    default:
      FOUR_C_THROW("unexpected number of nodes %d", num_node());
      break;
  }
}

FOUR_C_NAMESPACE_CLOSE
