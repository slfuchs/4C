/*----------------------------------------------------------------------*/
/*! \file

\brief   This is basically a (3d-) node with an additional fiber direction.

\level 2
*----------------------------------------------------------------------*/

#include "4C_fiber_node.hpp"

#include "4C_fiber_nodal_fiber_holder.hpp"

#include <utility>

FOUR_C_NAMESPACE_OPEN

DRT::FIBER::FiberNodeType DRT::FIBER::FiberNodeType::instance_;


CORE::COMM::ParObject* DRT::FIBER::FiberNodeType::Create(const std::vector<char>& data)
{
  std::vector<double> dummy_coords(3, 999.0);
  std::map<FIBER::CoordinateSystemDirection, std::array<double, 3>> coordinateSystemDirections;
  std::vector<std::array<double, 3>> fibers;
  std::map<FIBER::AngleType, double> angles;
  auto* object =
      new DRT::FIBER::FiberNode(-1, dummy_coords, coordinateSystemDirections, fibers, angles, -1);
  object->Unpack(data);
  return object;
}

DRT::FIBER::FiberNode::FiberNode(int id, const std::vector<double>& coords,
    std::map<FIBER::CoordinateSystemDirection, std::array<double, 3>> coordinateSystemDirections,
    std::vector<std::array<double, 3>> fibers, std::map<FIBER::AngleType, double> angles,
    const int owner)
    : CORE::Nodes::Node(id, coords, owner),
      coordinateSystemDirections_(std::move(coordinateSystemDirections)),
      fibers_(std::move(fibers)),
      angles_(std::move(angles))
{
}

/*
  Deep copy the derived class and return pointer to it
*/
DRT::FIBER::FiberNode* DRT::FIBER::FiberNode::Clone() const
{
  auto* newfn = new DRT::FIBER::FiberNode(*this);

  return newfn;
}

/*
  Pack this class so it can be communicated

  Pack and Unpack are used to communicate this fiber node

*/
void DRT::FIBER::FiberNode::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  CORE::Nodes::Node::AddtoPack(data, type);
  // add base class of fiber node
  CORE::Nodes::Node::Pack(data);

  // Add fiber data
  CORE::COMM::ParObject::AddtoPack(data, fibers_);
  CORE::COMM::ParObject::AddtoPack(data, coordinateSystemDirections_);
  CORE::COMM::ParObject::AddtoPack(data, angles_);
}

/*
  Unpack data from a char vector into this class

  Pack and Unpack are used to communicate this fiber node
*/
void DRT::FIBER::FiberNode::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class Node
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  CORE::Nodes::Node::Unpack(basedata);

  // extract fiber data
  CORE::COMM::ParObject::ExtractfromPack(position, data, fibers_);
  CORE::COMM::ParObject::ExtractfromPack(position, data, coordinateSystemDirections_);
  CORE::COMM::ParObject::ExtractfromPack(position, data, angles_);
}

/*
  Print this fiber node
*/
void DRT::FIBER::FiberNode::Print(std::ostream& os) const
{
  os << "Fiber Node :";
  CORE::Nodes::Node::Print(os);
  os << "(" << fibers_.size() << " fibers, " << angles_.size() << " angles)";
}

FOUR_C_NAMESPACE_CLOSE
