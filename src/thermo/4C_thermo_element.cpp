/*----------------------------------------------------------------------*/
/*! \file
\brief basic thermo element

\level 1
*/

/*----------------------------------------------------------------------*
 | headers                                                    gjb 01/08 |
 *----------------------------------------------------------------------*/
#include "4C_thermo_element.hpp"

#include "4C_comm_utils_factory.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_fem_general_element.hpp"
#include "4C_global_data.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_mat_fourieriso.hpp"
#include "4C_mat_thermostvenantkirchhoff.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

Discret::ELEMENTS::ThermoType Discret::ELEMENTS::ThermoType::instance_;

Discret::ELEMENTS::ThermoType& Discret::ELEMENTS::ThermoType::instance() { return instance_; }

/*----------------------------------------------------------------------*
 | create the new element type (public)                      dano 09/09 |
 | is called in ElementRegisterType                                     |
 *----------------------------------------------------------------------*/
Core::Communication::ParObject* Discret::ELEMENTS::ThermoType::create(const std::vector<char>& data)
{
  Discret::ELEMENTS::Thermo* object = new Discret::ELEMENTS::Thermo(-1, -1);
  object->unpack(data);
  return object;
}  // Create()


/*----------------------------------------------------------------------*
 | create the new element type (public)                      dano 09/09 |
 | is called from ParObjectFactory                                      |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::ThermoType::create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == "THERMO")
  {
    Teuchos::RCP<Core::Elements::Element> ele =
        Teuchos::rcp(new Discret::ELEMENTS::Thermo(id, owner));
    return ele;
  }
  return Teuchos::null;
}  // Create()


/*----------------------------------------------------------------------*
 | create the new element type (public)                      dano 09/09 |
 | virtual method of ElementType                                        |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::ThermoType::create(
    const int id, const int owner)
{
  Teuchos::RCP<Core::Elements::Element> ele =
      Teuchos::rcp(new Discret::ELEMENTS::Thermo(id, owner));
  return ele;
}  // Create()


/*----------------------------------------------------------------------*
 |                                                           dano 08/12 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::ThermoType::nodal_block_information(
    Core::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = dwele->num_dof_per_node(*(dwele->nodes()[0]));
  dimns = numdf;
  nv = numdf;
}  // nodal_block_information()


/*----------------------------------------------------------------------*
 | ctor (public)                                             dano 08/12 |
 *----------------------------------------------------------------------*/
Core::LinAlg::SerialDenseMatrix Discret::ELEMENTS::ThermoType::compute_null_space(
    Core::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  Core::LinAlg::SerialDenseMatrix nullspace;
  FOUR_C_THROW("method ComputeNullSpace not implemented!");
  return nullspace;
}


/*----------------------------------------------------------------------*
 | create the new element type (public)                      dano 09/09 |
 | is called from ParObjectFactory                                      |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::ThermoBoundaryType::create(
    const int id, const int owner)
{
  // return Teuchos::rcp(new Discret::ELEMENTS::ThermoBoundary(id,owner));
  return Teuchos::null;
}  // Create()


/*----------------------------------------------------------------------*
 | setup element                                             dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::ThermoType::setup_element_definition(
    std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
{
  std::map<std::string, Input::LineDefinition>& defs = definitions["THERMO"];

  defs["HEX8"] =
      Input::LineDefinition::Builder().add_int_vector("HEX8", 8).add_named_int("MAT").build();

  defs["HEX20"] =
      Input::LineDefinition::Builder().add_int_vector("HEX20", 20).add_named_int("MAT").build();

  defs["HEX27"] =
      Input::LineDefinition::Builder().add_int_vector("HEX27", 27).add_named_int("MAT").build();

  defs["TET4"] =
      Input::LineDefinition::Builder().add_int_vector("TET4", 4).add_named_int("MAT").build();

  defs["TET10"] =
      Input::LineDefinition::Builder().add_int_vector("TET10", 10).add_named_int("MAT").build();

  defs["WEDGE6"] =
      Input::LineDefinition::Builder().add_int_vector("WEDGE6", 6).add_named_int("MAT").build();

  defs["WEDGE15"] =
      Input::LineDefinition::Builder().add_int_vector("WEDGE15", 15).add_named_int("MAT").build();

  defs["PYRAMID5"] =
      Input::LineDefinition::Builder().add_int_vector("PYRAMID5", 5).add_named_int("MAT").build();

  defs["NURBS27"] =
      Input::LineDefinition::Builder().add_int_vector("NURBS27", 27).add_named_int("MAT").build();

  defs["QUAD4"] =
      Input::LineDefinition::Builder().add_int_vector("QUAD4", 4).add_named_int("MAT").build();

  defs["QUAD8"] =
      Input::LineDefinition::Builder().add_int_vector("QUAD8", 8).add_named_int("MAT").build();

  defs["QUAD9"] =
      Input::LineDefinition::Builder().add_int_vector("QUAD9", 9).add_named_int("MAT").build();

  defs["TRI3"] =
      Input::LineDefinition::Builder().add_int_vector("TRI3", 3).add_named_int("MAT").build();

  defs["TRI6"] =
      Input::LineDefinition::Builder().add_int_vector("TRI6", 6).add_named_int("MAT").build();

  defs["NURBS4"] =
      Input::LineDefinition::Builder().add_int_vector("NURBS4", 4).add_named_int("MAT").build();

  defs["NURBS9"] =
      Input::LineDefinition::Builder().add_int_vector("NURBS9", 9).add_named_int("MAT").build();

  defs["LINE2"] =
      Input::LineDefinition::Builder().add_int_vector("LINE2", 2).add_named_int("MAT").build();

  defs["LINE3"] =
      Input::LineDefinition::Builder().add_int_vector("LINE3", 3).add_named_int("MAT").build();
}  // setup_element_definition()


Discret::ELEMENTS::ThermoBoundaryType Discret::ELEMENTS::ThermoBoundaryType::instance_;

Discret::ELEMENTS::ThermoBoundaryType& Discret::ELEMENTS::ThermoBoundaryType::instance()
{
  return instance_;
}

/*----------------------------------------------------------------------*
 | ctor (public)                                             dano 09/09 |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::Thermo::Thermo(int id, int owner)
    : Core::Elements::Element(id, owner), distype_(Core::FE::CellType::dis_none)
{
  // default: geometrically linear, also including purely thermal probelm
  kintype_ = Inpar::Solid::KinemType::linear;
  return;
}  // ctor


/*----------------------------------------------------------------------*
 | copy-ctor (public)                                        dano 09/09 |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::Thermo::Thermo(const Discret::ELEMENTS::Thermo& old)
    : Core::Elements::Element(old), kintype_(old.kintype_), distype_(old.distype_)
{
  if (old.shape() == Core::FE::CellType::nurbs27) set_nurbs_element() = true;
  return;
}  // copy-ctor


/*----------------------------------------------------------------------*
 | deep copy this instance of Thermo and return              dano 09/09 |
 | pointer to it (public)                                               |
 *----------------------------------------------------------------------*/
Core::Elements::Element* Discret::ELEMENTS::Thermo::clone() const
{
  Discret::ELEMENTS::Thermo* newelement = new Discret::ELEMENTS::Thermo(*this);
  return newelement;
}  // clone()


/*----------------------------------------------------------------------*
 | return the shape of a Thermo element (public)             dano 09/09 |
 *----------------------------------------------------------------------*/
Core::FE::CellType Discret::ELEMENTS::Thermo::shape() const { return distype_; }  // Shape()


/*----------------------------------------------------------------------*
 | pack data (public)                                        dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::Thermo::pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = unique_par_object_id();
  add_to_pack(data, type);
  // add base class Element
  Element::pack(data);
  // kintype
  add_to_pack(data, kintype_);
  // distype
  add_to_pack(data, distype_);

  return;
}  // pack()


/*----------------------------------------------------------------------*
 | unpack data (public)                                      dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::Thermo::unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  Core::Communication::ExtractAndAssertId(position, data, unique_par_object_id());

  // extract base class Element
  std::vector<char> basedata(0);
  extract_from_pack(position, data, basedata);
  Element::unpack(basedata);
  // kintype_
  kintype_ = static_cast<Inpar::Solid::KinemType>(extract_int(position, data));
  // distype
  distype_ = static_cast<Core::FE::CellType>(extract_int(position, data));
  if (distype_ == Core::FE::CellType::nurbs27) set_nurbs_element() = true;

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", (int)data.size(), position);
  return;
}  // unpack()



/*----------------------------------------------------------------------*
 | print this element (public)                               dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::Thermo::print(std::ostream& os) const
{
  os << "Thermo element";
  Element::print(os);
  std::cout << std::endl;
  std::cout << "DiscretizationType:  " << Core::FE::CellTypeToString(distype_) << std::endl;
  std::cout << std::endl;
  std::cout << "Number DOF per Node: " << numdofpernode_ << std::endl;
  std::cout << std::endl;
  return;
}  // print()


/*----------------------------------------------------------------------*
 | get vector of lines (public)                              dano 09/09 |
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::Thermo::lines()
{
  return Core::Communication::GetElementLines<ThermoBoundary, Thermo>(*this);
}  // Lines()


/*----------------------------------------------------------------------*
 | get vector of surfaces (public)                           dano 09/09 |
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::Thermo::surfaces()
{
  return Core::Communication::GetElementSurfaces<ThermoBoundary, Thermo>(*this);
}  // Surfaces()

/*----------------------------------------------------------------------*
 | return names of visualization data (public)               dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::Thermo::vis_names(std::map<std::string, int>& names)
{
  // see whether we have additional data for visualization in our container
  for (int k = 0; k < numdofpernode_; k++)
  {
    std::ostringstream temp;
    temp << k;
  }  // loop over temperatures

  return;
}  // vis_names()


/*----------------------------------------------------------------------*
 | return visualization data (public)                        dano 09/09 |
 *----------------------------------------------------------------------*/
bool Discret::ELEMENTS::Thermo::vis_data(const std::string& name, std::vector<double>& data)
{
  // Put the owner of this element into the file (use base class method for this)
  if (Core::Elements::Element::vis_data(name, data)) return true;

  return false;
}  // vis_data()

/*----------------------------------------------------------------------------*
 | ENDE Discret::ELEMENTS::Thermo
 *----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------*
 | ctor (public)                                             dano 09/09 |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::ThermoBoundary::ThermoBoundary(int id, int owner, int nnode, const int* nodeids,
    Core::Nodes::Node** nodes, Discret::ELEMENTS::Thermo* parent, const int lsurface)
    : Core::Elements::FaceElement(id, owner)
{
  set_node_ids(nnode, nodeids);
  build_nodal_pointers(nodes);
  set_parent_master_element(parent, lsurface);
  return;
}  // ctor


/*----------------------------------------------------------------------*
 | copy-ctor (public)                                        dano 09/09 |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::ThermoBoundary::ThermoBoundary(const Discret::ELEMENTS::ThermoBoundary& old)
    : Core::Elements::FaceElement(old)
{
  return;
}  // copy-ctor


/*----------------------------------------------------------------------*
 | deep copy this instance return pointer to it (public)     dano 09/09 |
 *----------------------------------------------------------------------*/
Core::Elements::Element* Discret::ELEMENTS::ThermoBoundary::clone() const
{
  Discret::ELEMENTS::ThermoBoundary* newelement = new Discret::ELEMENTS::ThermoBoundary(*this);
  return newelement;
}  // clone()


/*----------------------------------------------------------------------*
 | return shape of this element (public)                     dano 09/09 |
 *----------------------------------------------------------------------*/
Core::FE::CellType Discret::ELEMENTS::ThermoBoundary::shape() const
{
  switch (num_node())
  {
    case 2:
      return Core::FE::CellType::line2;
    case 3:
      if ((parent_element()->shape() == Core::FE::CellType::quad8) or
          (parent_element()->shape() == Core::FE::CellType::quad9))
        return Core::FE::CellType::line3;
      else
        return Core::FE::CellType::tri3;
    case 4:
      return Core::FE::CellType::quad4;
    case 6:
      return Core::FE::CellType::tri6;
    case 8:
      return Core::FE::CellType::quad8;
    case 9:
      if (parent_element()->shape() == Core::FE::CellType::hex27)
        return Core::FE::CellType::quad9;
      else if (parent_element()->shape() == Core::FE::CellType::nurbs27)
        return Core::FE::CellType::nurbs9;
      else
      {
        FOUR_C_THROW(
            "Your parent discretization type is %s. Ccurrently only hex27 and nurbs27 are "
            "implemented.",
            Core::FE::CellTypeToString(parent_element()->shape()).c_str());
      }
      break;
    default:
      FOUR_C_THROW("unexpected number of nodes %d", num_node());
  }
}  // Shape()


/*----------------------------------------------------------------------*
 | pack data (public)                                        dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::ThermoBoundary::pack(std::vector<char>& data) const
{
  FOUR_C_THROW("This ThermoBoundary element does not support communication");

  return;
}  // pack()


/*----------------------------------------------------------------------*
 | unpack data (public)                                      dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::ThermoBoundary::unpack(const std::vector<char>& data)
{
  FOUR_C_THROW("This ThermoBoundary element does not support communication");
  return;
}  // unpack()



/*----------------------------------------------------------------------*
 | print this element (public)                               dano 09/09 |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::ThermoBoundary::print(std::ostream& os) const
{
  os << "ThermoBoundary ";
  Element::print(os);
  return;
}  // print()


/*----------------------------------------------------------------------*
 | get vector of lines (public)                              dano 09/09 |
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::ThermoBoundary::lines()
{
  FOUR_C_THROW("Lines of ThermoBoundary not implemented");
}  // Lines()


/*----------------------------------------------------------------------*
 | get vector of lines (public)                              dano 09/09 |
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::ThermoBoundary::surfaces()
{
  FOUR_C_THROW("Surfaces of ThermoBoundary not implemented");
}  // Surfaces()

FOUR_C_NAMESPACE_CLOSE
