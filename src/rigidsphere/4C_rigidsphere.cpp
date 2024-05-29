/*----------------------------------------------------------------------------*/
/*! \file

\brief spherical particle element for brownian dynamics

\level 3

*/
/*----------------------------------------------------------------------------*/

#include "4C_rigidsphere.hpp"

#include "4C_beaminteraction_link_pinjointed.hpp"
#include "4C_comm_utils_factory.hpp"
#include "4C_discretization_fem_general_largerotations.hpp"
#include "4C_discretization_fem_general_utils_fem_shapefunctions.hpp"
#include "4C_discretization_fem_general_utils_integration.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_browniandyn.hpp"
#include "4C_inpar_validparameters.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_lib_discret.hpp"
#include "4C_linalg_fixedsizematrix.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_structure_new_elements_paramsinterface.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN


DRT::ELEMENTS::RigidsphereType DRT::ELEMENTS::RigidsphereType::instance_;

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::RigidsphereType& DRT::ELEMENTS::RigidsphereType::Instance() { return instance_; }

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CORE::COMM::ParObject* DRT::ELEMENTS::RigidsphereType::Create(const std::vector<char>& data)
{
  DRT::ELEMENTS::Rigidsphere* object = new DRT::ELEMENTS::Rigidsphere(-1, -1);
  object->Unpack(data);
  return (object);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<CORE::Elements::Element> DRT::ELEMENTS::RigidsphereType::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == "RIGIDSPHERE")
  {
    Teuchos::RCP<CORE::Elements::Element> ele =
        Teuchos::rcp(new DRT::ELEMENTS::Rigidsphere(id, owner));
    return (ele);
  }
  return (Teuchos::null);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<CORE::Elements::Element> DRT::ELEMENTS::RigidsphereType::Create(
    const int id, const int owner)
{
  return (Teuchos::rcp(new Rigidsphere(id, owner)));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::RigidsphereType::nodal_block_information(
    CORE::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = 3;
  nv = 3;
  dimns = 3;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CORE::LINALG::SerialDenseMatrix DRT::ELEMENTS::RigidsphereType::ComputeNullSpace(
    CORE::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  CORE::LINALG::SerialDenseMatrix nullspace;
  FOUR_C_THROW("method ComputeNullSpace not implemented!");
  return nullspace;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::RigidsphereType::setup_element_definition(
    std::map<std::string, std::map<std::string, INPUT::LineDefinition>>& definitions)
{
  std::map<std::string, INPUT::LineDefinition>& defs = definitions["RIGIDSPHERE"];

  defs["POINT1"] = INPUT::LineDefinition::Builder()
                       .AddIntVector("POINT1", 1)
                       .AddNamedDouble("RADIUS")
                       .AddNamedDouble("DENSITY")
                       .Build();
}

/*----------------------------------------------------------------------*
 |  ctor (public)                                            meier 05/12|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Rigidsphere::Rigidsphere(int id, int owner)
    : CORE::Elements::Element(id, owner), radius_(0.0), rho_(0.0)
{
  mybondstobeams_.clear();
}
/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       meier 05/12|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Rigidsphere::Rigidsphere(const DRT::ELEMENTS::Rigidsphere& old)
    : CORE::Elements::Element(old), radius_(old.radius_), rho_(old.rho_)
{
  mybondstobeams_.clear();
  if (old.mybondstobeams_.size())
  {
    for (auto const& iter : old.mybondstobeams_)
    {
      if (iter.second != Teuchos::null)
        mybondstobeams_[iter.first] =
            Teuchos::rcp_dynamic_cast<BEAMINTERACTION::BeamLinkPinJointed>(iter.second->Clone());
      else
        FOUR_C_THROW("something went wrong, I am sorry. Please go debugging.");
    }
  }
  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance of Rigidsphere and return pointer to it (public) |
 |                                                            meier 05/12 |
 *----------------------------------------------------------------------*/
CORE::Elements::Element* DRT::ELEMENTS::Rigidsphere::Clone() const
{
  DRT::ELEMENTS::Rigidsphere* newelement = new DRT::ELEMENTS::Rigidsphere(*this);
  return (newelement);
}



/*----------------------------------------------------------------------*
 |  print this element (public)                              meier 05/12
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::Print(std::ostream& os) const { return; }


/*----------------------------------------------------------------------*
 |                                                             (public) |
 |                                                          meier 05/12 |
 *----------------------------------------------------------------------*/
CORE::FE::CellType DRT::ELEMENTS::Rigidsphere::Shape() const
{
  return (CORE::FE::CellType::point1);
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                           meier 05/12/
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);
  // add base class Element
  Element::Pack(data);

  // add all class variables
  AddtoPack(data, radius_);
  AddtoPack(data, rho_);

  AddtoPack(data, static_cast<int>(mybondstobeams_.size()));
  for (auto const& iter : mybondstobeams_) iter.second->Pack(data);

  return;
}

/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                           meier 05/12|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class Element
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  Element::Unpack(basedata);


  // extract all class variables
  ExtractfromPack(position, data, radius_);
  ExtractfromPack(position, data, rho_);

  int unsigned numbonds = ExtractInt(position, data);
  for (int unsigned i = 0; i < numbonds; ++i)
  {
    std::vector<char> tmp;
    ExtractfromPack(position, data, tmp);
    Teuchos::RCP<CORE::COMM::ParObject> object = Teuchos::rcp(CORE::COMM::Factory(tmp), true);
    Teuchos::RCP<BEAMINTERACTION::BeamLinkPinJointed> link =
        Teuchos::rcp_dynamic_cast<BEAMINTERACTION::BeamLinkPinJointed>(object);
    if (link == Teuchos::null) FOUR_C_THROW("Received object is not a beam to beam linkage");
    mybondstobeams_[link->Id()] = link;
  }

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", static_cast<int>(data.size()), position);
  return;
}

/*----------------------------------------------------------------------*
 |  get vector of lines (public)                             meier 02/14|
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<CORE::Elements::Element>> DRT::ELEMENTS::Rigidsphere::Lines()
{
  return {Teuchos::rcpFromRef(*this)};
}


/*----------------------------------------------------------------------*
 |  Initialize (public)                                      meier 05/12|
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::RigidsphereType::Initialize(DRT::Discretization& dis) { return 0; }

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::set_params_interface_ptr(const Teuchos::ParameterList& p)
{
  if (p.isParameter("interface"))
    interface_ptr_ = Teuchos::rcp_dynamic_cast<STR::ELEMENTS::ParamsInterface>(
        p.get<Teuchos::RCP<CORE::Elements::ParamsInterface>>("interface"));
  else
    interface_ptr_ = Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<CORE::Elements::ParamsInterface> DRT::ELEMENTS::Rigidsphere::ParamsInterfacePtr()
{
  return interface_ptr_;
}

FOUR_C_NAMESPACE_CLOSE
