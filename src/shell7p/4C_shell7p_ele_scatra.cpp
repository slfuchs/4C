/*! \file
\brief A 2D shell element with ScaTra functionality

\level 3
*/

#include "4C_shell7p_ele_scatra.hpp"

#include "4C_comm_pack_helpers.hpp"
#include "4C_comm_utils_factory.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_fem_general_cell_type.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_mat_so3_material.hpp"
#include "4C_shell7p_ele_factory.hpp"
#include "4C_shell7p_ele_interface_serializable.hpp"
#include "4C_shell7p_line.hpp"
#include "4C_shell7p_utils.hpp"

#include <Teuchos_ParameterList.hpp>

FOUR_C_NAMESPACE_OPEN

namespace
{
  template <typename Interface>
  void try_pack_interface(const Interface& interface, Core::Communication::PackBuffer& data)
  {
    std::shared_ptr<Discret::ELEMENTS::Shell::Serializable> serializable_interface =
        std::dynamic_pointer_cast<Discret::ELEMENTS::Shell::Serializable>(interface);
    if (serializable_interface != nullptr) serializable_interface->pack(data);
  }

  template <typename Interface>
  void try_unpack_interface(Interface& interface, Core::Communication::UnpackBuffer& buffer)
  {
    std::shared_ptr<Discret::ELEMENTS::Shell::Serializable> serializable_shell_interface =
        std::dynamic_pointer_cast<Discret::ELEMENTS::Shell::Serializable>(interface);
    if (serializable_shell_interface != nullptr) serializable_shell_interface->unpack(buffer);
  }

}  // namespace

Discret::ELEMENTS::Shell7pScatraType Discret::ELEMENTS::Shell7pScatraType::instance_;


Discret::ELEMENTS::Shell7pScatraType& Discret::ELEMENTS::Shell7pScatraType::instance()
{
  return instance_;
}

Core::Communication::ParObject* Discret::ELEMENTS::Shell7pScatraType::create(
    Core::Communication::UnpackBuffer& buffer)
{
  auto* object = new Discret::ELEMENTS::Shell7pScatra(-1, -1);
  object->unpack(buffer);
  return object;
}

Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::Shell7pScatraType::create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == "SHELL7PSCATRA") return create(id, owner);
  return Teuchos::null;
}

Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::Shell7pScatraType::create(
    const int id, const int owner)
{
  return Teuchos::make_rcp<Discret::ELEMENTS::Shell7pScatra>(id, owner);
}

void Discret::ELEMENTS::Shell7pScatraType::setup_element_definition(
    std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
{
  std::map<std::string, Input::LineDefinition>& defsgeneral = definitions["SHELL7PSCATRA"];

  defsgeneral["QUAD4"] = Input::LineDefinition::Builder()
                             .add_int_vector("QUAD4", 4)
                             .add_named_int("MAT")
                             .add_named_double("THICK")
                             .add_named_string("EAS")
                             .add_string("EAS2")
                             .add_string("EAS3")
                             .add_string("EAS4")
                             .add_string("EAS5")
                             .add_named_double("SDC")
                             .add_optional_tag("ANS")
                             .add_optional_named_double_vector("RAD", 3)
                             .add_optional_named_double_vector("AXI", 3)
                             .add_optional_named_double_vector("CIR", 3)
                             .add_optional_named_double_vector("FIBER1", 3)
                             .add_optional_named_double_vector("FIBER2", 3)
                             .add_optional_named_double_vector("FIBER3", 3)
                             .add_optional_named_string("TYPE")
                             .build();

  defsgeneral["QUAD8"] = Input::LineDefinition::Builder()
                             .add_int_vector("QUAD8", 8)
                             .add_named_int("MAT")
                             .add_named_double("THICK")
                             .add_named_string("EAS")
                             .add_string("EAS2")
                             .add_string("EAS3")
                             .add_string("EAS4")
                             .add_string("EAS5")
                             .add_named_double("SDC")
                             .add_optional_tag("ANS")
                             .add_optional_named_double_vector("RAD", 3)
                             .add_optional_named_double_vector("AXI", 3)
                             .add_optional_named_double_vector("CIR", 3)
                             .add_optional_named_double_vector("FIBER1", 3)
                             .add_optional_named_double_vector("FIBER2", 3)
                             .add_optional_named_double_vector("FIBER3", 3)
                             .add_optional_named_string("TYPE")
                             .build();

  defsgeneral["QUAD9"] = Input::LineDefinition::Builder()
                             .add_int_vector("QUAD9", 9)
                             .add_named_int("MAT")
                             .add_named_double("THICK")
                             .add_named_string("EAS")
                             .add_string("EAS2")
                             .add_string("EAS3")
                             .add_string("EAS4")
                             .add_string("EAS5")
                             .add_named_double("SDC")
                             .add_optional_tag("ANS")
                             .add_optional_named_double_vector("RAD", 3)
                             .add_optional_named_double_vector("AXI", 3)
                             .add_optional_named_double_vector("CIR", 3)
                             .add_optional_named_double_vector("FIBER1", 3)
                             .add_optional_named_double_vector("FIBER2", 3)
                             .add_optional_named_double_vector("FIBER3", 3)
                             .add_optional_named_string("TYPE")
                             .build();

  defsgeneral["TRI3"] = Input::LineDefinition::Builder()
                            .add_int_vector("TRI3", 3)
                            .add_named_int("MAT")
                            .add_named_double("THICK")
                            .add_named_double("SDC")
                            .add_optional_named_double_vector("RAD", 3)
                            .add_optional_named_double_vector("AXI", 3)
                            .add_optional_named_double_vector("CIR", 3)
                            .add_optional_named_double_vector("FIBER1", 3)
                            .add_optional_named_double_vector("FIBER2", 3)
                            .add_optional_named_double_vector("FIBER3", 3)
                            .add_optional_named_string("TYPE")
                            .build();

  defsgeneral["TRI6"] = Input::LineDefinition::Builder()
                            .add_int_vector("TRI6", 6)
                            .add_named_int("MAT")
                            .add_named_double("THICK")
                            .add_named_double("SDC")
                            .add_optional_named_double_vector("RAD", 3)
                            .add_optional_named_double_vector("AXI", 3)
                            .add_optional_named_double_vector("CIR", 3)
                            .add_optional_named_double_vector("FIBER1", 3)
                            .add_optional_named_double_vector("FIBER2", 3)
                            .add_optional_named_double_vector("FIBER3", 3)
                            .add_optional_named_string("TYPE")
                            .build();
}

int Discret::ELEMENTS::Shell7pScatraType::initialize(Core::FE::Discretization& dis)
{
  Solid::UTILS::Shell::Director::setup_shell_element_directors(*this, dis);

  return 0;
}



Core::LinAlg::SerialDenseMatrix Discret::ELEMENTS::Shell7pScatraType::compute_null_space(
    Core::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  auto* shell = dynamic_cast<Discret::ELEMENTS::Shell7pScatra*>(node.elements()[0]);
  if (!shell) FOUR_C_THROW("Cannot cast to Shell");
  int j;
  for (j = 0; j < shell->num_node(); ++j)
    if (shell->nodes()[j]->id() == node.id()) break;
  if (j == shell->num_node()) FOUR_C_THROW("Can't find matching node..!");
  double half_thickness = shell->get_thickness() / 2.0;

  // set director
  const Core::LinAlg::SerialDenseMatrix nodal_directors = shell->get_directors();
  Core::LinAlg::Matrix<Shell::Internal::num_dim, 1> director(true);
  for (int dim = 0; dim < Shell::Internal::num_dim; ++dim)
    director(dim, 0) = nodal_directors(j, dim) * half_thickness;

  return Solid::UTILS::Shell::compute_shell_null_space(node, x0, director);
}

void Discret::ELEMENTS::Shell7pScatraType::nodal_block_information(
    Core::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  Solid::UTILS::Shell::nodal_block_information_shell(dwele, numdf, dimns, nv, np);
}


Discret::ELEMENTS::Shell7pScatra::Shell7pScatra(const Discret::ELEMENTS::Shell7pScatra& other)
    : Core::Elements::Element(other),
      distype_(other.distype_),
      interface_ptr_(other.interface_ptr_),
      eletech_(other.eletech_),
      thickness_(other.thickness_),
      nodal_directors_(other.nodal_directors_),
      material_post_setup_(other.material_post_setup_),
      impltype_(other.impltype_)
{
  // reset shell calculation interface
  shell_interface_ = Shell7pFactory::provide_shell7p_calculation_interface(other, other.eletech_);
}


Discret::ELEMENTS::Shell7pScatra& Discret::ELEMENTS::Shell7pScatra::operator=(
    const Discret::ELEMENTS::Shell7pScatra& other)
{
  if (this == &other) return *this;
  Core::Elements::Element::operator=(other);
  distype_ = other.distype_;
  interface_ptr_ = other.interface_ptr_;
  eletech_ = other.eletech_;
  thickness_ = other.thickness_;
  nodal_directors_ = other.nodal_directors_;
  material_post_setup_ = other.material_post_setup_;
  impltype_ = other.impltype_;

  shell_interface_ = Shell7pFactory::provide_shell7p_calculation_interface(other, other.eletech_);
  return *this;
}

Core::Elements::Element* Discret::ELEMENTS::Shell7pScatra::clone() const
{
  auto* newelement = new Discret::ELEMENTS::Shell7pScatra(*this);
  return newelement;
}

void Discret::ELEMENTS::Shell7pScatra::pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = unique_par_object_id();
  add_to_pack(data, type);
  // add base class Element
  Core::Elements::Element::pack(data);
  // discretization type
  add_to_pack(data, (int)distype_);
  // element technology
  add_to_pack(data, eletech_);
  // thickness in reference frame
  add_to_pack(data, thickness_);
  // nodal_directors
  add_to_pack(data, nodal_directors_);
  // Setup flag for material post setup
  data.add_to_pack(material_post_setup_);
  // pack impltype
  add_to_pack(data, impltype_);
  // optional data, e.g., EAS data, current thickness,..
  try_pack_interface(shell_interface_, data);
}


void Discret::ELEMENTS::Shell7pScatra::unpack(Core::Communication::UnpackBuffer& buffer)
{
  Core::Communication::extract_and_assert_id(buffer, unique_par_object_id());

  // extract base class Element
  std::vector<char> basedata(0);
  extract_from_pack(buffer, basedata);
  Core::Communication::UnpackBuffer base_buffer(basedata);
  Element::unpack(base_buffer);
  // discretization type
  distype_ = static_cast<Core::FE::CellType>(extract_int(buffer));
  // element technology
  extract_from_pack(buffer, eletech_);
  // thickness in reference frame
  extract_from_pack(buffer, thickness_);
  // nodal director
  extract_from_pack(buffer, nodal_directors_);
  // Setup flag for material post setup
  extract_from_pack(buffer, material_post_setup_);
  // extract impltype
  impltype_ = static_cast<Inpar::ScaTra::ImplType>(extract_int(buffer));
  // reset shell calculation interface
  shell_interface_ = Shell7pFactory::provide_shell7p_calculation_interface(*this, eletech_);

  try_unpack_interface(shell_interface_, buffer);
  FOUR_C_THROW_UNLESS(buffer.at_end(), "Buffer not fully consumed.");
}

Teuchos::RCP<Mat::So3Material> Discret::ELEMENTS::Shell7pScatra::solid_material(int nummat) const
{
  return Teuchos::rcp_dynamic_cast<Mat::So3Material>(
      Core::Elements::Element::material(nummat), true);
}

void Discret::ELEMENTS::Shell7pScatra::set_params_interface_ptr(const Teuchos::ParameterList& p)
{
  if (p.isParameter("interface"))
  {
    interface_ptr_ = Teuchos::rcp_dynamic_cast<Solid::ELEMENTS::ParamsInterface>(
        p.get<Teuchos::RCP<Core::Elements::ParamsInterface>>("interface"));
  }
  else
  {
    interface_ptr_ = Teuchos::null;
  }
}


void Discret::ELEMENTS::Shell7pScatra::vis_names(std::map<std::string, int>& names)
{
  std::string result_thickness = "thickness";
  names[result_thickness] = 1;
  solid_material()->vis_names(names);
}  // vis_names()


bool Discret::ELEMENTS::Shell7pScatra::vis_data(const std::string& name, std::vector<double>& data)
{
  // Put the owner of this element into the file (use base class method for this)
  if (Core::Elements::Element::vis_data(name, data)) return true;

  shell_interface_->vis_data(name, data);

  return solid_material()->vis_data(name, data, id());

}  // vis_data()


void Discret::ELEMENTS::Shell7pScatra::print(std::ostream& os) const
{
  os << "Shell7pScatra ";
  os << " discretization type: " << Core::FE::cell_type_to_string(distype_).c_str();
  Element::print(os);
}

std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::Shell7pScatra::lines()
{
  return Core::Communication::element_boundary_factory<Shell7pLine, Shell7pScatra>(
      Core::Communication::buildLines, *this);
}

std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::Shell7pScatra::surfaces()
{
  return {Teuchos::rcpFromRef(*this)};
}

int Discret::ELEMENTS::Shell7pScatra::num_line() const
{
  return Core::FE::get_number_of_element_lines(distype_);
}


int Discret::ELEMENTS::Shell7pScatra::num_surface() const { return 1; }


bool Discret::ELEMENTS::Shell7pScatra::read_element(const std::string& eletype,
    const std::string& distype, const Core::IO::InputParameterContainer& container)
{
  Solid::ELEMENTS::ShellData shell_data = {};

  // set discretization type
  distype_ = Core::FE::string_to_cell_type(distype);

  // set thickness in reference frame
  thickness_ = container.get<double>("THICK");
  if (thickness_ <= 0) FOUR_C_THROW("Shell element thickness needs to be > 0");
  shell_data.thickness = thickness_;

  // extract number of EAS parameters for different locking types
  Solid::ELEMENTS::ShellLockingTypes locking_types = {};
  if (container.get_if<std::string>("EAS") != nullptr)
  {
    eletech_.insert(Inpar::Solid::EleTech::eas);
    Solid::UTILS::Shell::read_element::read_and_set_locking_types(
        distype_, container, locking_types);
  }

  // set calculation interface pointer
  shell_interface_ = Shell7pFactory::provide_shell7p_calculation_interface(*this, eletech_);

  // read and set ANS technology for element
  if (distype_ == Core::FE::CellType::quad4 or distype_ == Core::FE::CellType::quad6 or
      distype_ == Core::FE::CellType::quad9)
  {
    if (container.get<bool>("ANS"))
    {
      shell_data.num_ans = Solid::UTILS::Shell::read_element::read_and_set_num_ans(distype_);
    }
  }

  // read SDC
  shell_data.sdc = container.get<double>("SDC");

  // read and set number of material model
  set_material(
      0, Mat::factory(Solid::UTILS::Shell::read_element::read_and_set_element_material(container)));

  // setup shell calculation interface
  shell_interface_->setup(*this, *solid_material(), container, locking_types, shell_data);
  if (!material_post_setup_)
  {
    shell_interface_->material_post_setup(*this, *solid_material());
    material_post_setup_ = true;
  }
  // read implementation type for scatra
  auto impltype = container.get<std::string>("TYPE");

  if (impltype == "Undefined")
    impltype_ = Inpar::ScaTra::impltype_undefined;
  else if (impltype == "AdvReac")
    impltype_ = Inpar::ScaTra::impltype_advreac;
  else if (impltype == "CardMono")
    impltype_ = Inpar::ScaTra::impltype_cardiac_monodomain;
  else if (impltype == "Chemo")
    impltype_ = Inpar::ScaTra::impltype_chemo;
  else if (impltype == "ChemoReac")
    impltype_ = Inpar::ScaTra::impltype_chemoreac;
  else if (impltype == "Loma")
    impltype_ = Inpar::ScaTra::impltype_loma;
  else if (impltype == "RefConcReac")
    impltype_ = Inpar::ScaTra::impltype_refconcreac;
  else if (impltype == "Std")
    impltype_ = Inpar::ScaTra::impltype_std;
  else
    FOUR_C_THROW("Invalid implementation type for Shell7pScatra elements!");

  return true;
}

FOUR_C_NAMESPACE_CLOSE
