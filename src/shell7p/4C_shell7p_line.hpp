/*! \file

\brief Line element associated to the shell 7-Parameter element

\level 3
*/

#ifndef FOUR_C_SHELL7P_LINE_HPP
#define FOUR_C_SHELL7P_LINE_HPP

#include "4C_config.hpp"

#include "4C_discretization_fem_general_element.hpp"
#include "4C_discretization_fem_general_element_integration_select.hpp"
#include "4C_discretization_fem_general_node.hpp"
#include "4C_discretization_fem_general_utils_gausspoints.hpp"
#include "4C_shell7p_ele.hpp"

FOUR_C_NAMESPACE_OPEN

namespace DRT::ELEMENTS
{
  class Shell7pLineType : public CORE::Elements::ElementType
  {
   public:
    [[nodiscard]] std::string Name() const override { return "Shell7pLineType"; }

    static Shell7pLineType& Instance();

    Teuchos::RCP<CORE::Elements::Element> Create(const int id, const int owner) override;

    void nodal_block_information(
        CORE::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np) override
    {
    }

    CORE::LINALG::SerialDenseMatrix ComputeNullSpace(
        CORE::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp) override
    {
      Teuchos::SerialDenseMatrix<int, double> nullspace;
      FOUR_C_THROW("method ComputeNullSpace not implemented!");
    }

   private:
    static Shell7pLineType instance_;
  };

  /*!
  \brief An element representing a line edge of a Shell element

  \note This is a pure Neumann boundary condition element. It's only
        purpose is to evaluate line Neumann boundary conditions that might be
        adjacent to a parent Shell element. It therefore does not implement
        the CORE::Elements::Element::Evaluate method and does not have its own ElementRegister
  class.

  */
  class Shell7pLine : public CORE::Elements::FaceElement
  {
   public:
    //! @name Friends
    friend class Shell7pLineType;

    //! @name Constructors and destructors related methods
    //! @{
    /*!
    \brief Standard Constructor

    @param id (in) : A unique global id
    @param owner (in) : Processor owning this line
    @param nnode (in) : Number of nodes attached to this element
    @param nodeids (in) : global ids of nodes attached to this element
    @param nodes (in) : the discretizations map of nodes to build ptrs to nodes
    @param parent (in) : The parent shell element of this line
    @param lline (in) : the local line number of this line w.r.t. the parent element
    */
    Shell7pLine(int id, int owner, int nnode, const int* nodeids, CORE::Nodes::Node** nodes,
        CORE::Elements::Element* parent, const int lline);

    ///! copy constructor
    Shell7pLine(const Shell7pLine& old);


    //! copy assignment operator
    Shell7pLine& operator=(const Shell7pLine& other) = default;

    //! move constructor
    Shell7pLine(Shell7pLine&& other) noexcept = default;

    //! move assignment operator
    Shell7pLine& operator=(Shell7pLine&& other) noexcept = default;
    //! @}

    [[nodiscard]] CORE::Elements::Element* Clone() const override;

    [[nodiscard]] inline int UniqueParObjectId() const override
    {
      return Shell7pLineType::Instance().UniqueParObjectId();
    };

    void Pack(CORE::COMM::PackBuffer& data) const override;

    void Unpack(const std::vector<char>& data) override;

    //! @name Access methods
    //! @{
    [[nodiscard]] CORE::FE::CellType Shape() const override;

    [[nodiscard]] int NumDofPerNode(const CORE::Nodes::Node& node) const override
    {
      return node_dof_;
    }

    [[nodiscard]] int num_dof_per_element() const override { return 0; }


    [[nodiscard]] DRT::ELEMENTS::Shell7p* parent_element() const
    {
      CORE::Elements::Element* parent = this->CORE::Elements::FaceElement::parent_element();
      // make sure the static cast below is really valid
      FOUR_C_ASSERT(dynamic_cast<DRT::ELEMENTS::Shell7p*>(parent) != nullptr,
          "Parent element is no shell element");
      return static_cast<DRT::ELEMENTS::Shell7p*>(parent);
    }

    void Print(std::ostream& os) const override;

    [[nodiscard]] CORE::Elements::ElementType& ElementType() const override
    {
      return Shell7pLineType::Instance();
    }
    //@}

    //! @name Evaluate methods
    //! @{
    int evaluate_neumann(Teuchos::ParameterList& params, DRT::Discretization& discretization,
        CORE::Conditions::Condition& condition, std::vector<int>& dof_index_array,
        CORE::LINALG::SerialDenseVector& elevec1,
        CORE::LINALG::SerialDenseMatrix* elemat1 = nullptr) override;

    //@}

   private:
    //! gaussian integration to be used
    CORE::FE::GaussRule1D gaussrule_;

    void line_integration(double& dL, const CORE::LINALG::SerialDenseMatrix& x,
        const CORE::LINALG::SerialDenseMatrix& deriv);

    /*!
    \brief Create matrix with material configuration

    @param x  (in/out)  : nodal coords in material frame
     */
    inline void material_configuration(CORE::LINALG::SerialDenseMatrix& x) const
    {
      for (int i = 0; i < num_node(); ++i)
      {
        x(i, 0) = Nodes()[i]->X()[0];
        x(i, 1) = Nodes()[i]->X()[1];
        x(i, 2) = Nodes()[i]->X()[2];
      }
    }

    static constexpr int num_dim_ = 3;
    static constexpr int node_dof_ = 6;
  };  // class Shell7pLine

}  // namespace DRT::ELEMENTS

FOUR_C_NAMESPACE_CLOSE

#endif
