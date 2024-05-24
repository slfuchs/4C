/*----------------------------------------------------------------------*/
/*! \file
 \brief definition of porofluidmultiphase elements

   \level 3

 *----------------------------------------------------------------------*/

#ifndef FOUR_C_POROFLUIDMULTIPHASE_ELE_HPP
#define FOUR_C_POROFLUIDMULTIPHASE_ELE_HPP


#include "4C_config.hpp"

#include "4C_lib_element.hpp"
#include "4C_lib_elementtype.hpp"
#include "4C_linalg_serialdensematrix.hpp"

FOUR_C_NAMESPACE_OPEN

namespace DRT
{
  namespace ELEMENTS
  {
    class PoroFluidMultiPhaseType : public DRT::ElementType
    {
     public:
      std::string Name() const override { return "PoroFluidMultiPhaseType"; }

      /// singleton instance method
      static PoroFluidMultiPhaseType& Instance();

      /// create an element from data
      CORE::COMM::ParObject* Create(const std::vector<char>& data) override;

      /// create an element from a dat file specifier
      Teuchos::RCP<DRT::Element> Create(const std::string eletype, const std::string eledistype,
          const int id, const int owner) override;

      /// create an empty element
      Teuchos::RCP<DRT::Element> Create(const int id, const int owner) override;

      /// nodal block information to create a null space description
      void nodal_block_information(
          DRT::Element* dwele, int& numdf, int& dimns, int& nv, int& np) override;

      /// do the null space computation
      CORE::LINALG::SerialDenseMatrix ComputeNullSpace(
          DRT::Node& node, const double* x0, const int numdof, const int dimnsp) override;

      /// setup the dat file input line definitions for this type of element
      void setup_element_definition(
          std::map<std::string, std::map<std::string, INPUT::LineDefinition>>& definitions)
          override;

      /// initialize element
      int Initialize(DRT::Discretization& dis) override;

      /// pre-evaluation
      void pre_evaluate(DRT::Discretization& dis, Teuchos::ParameterList& p,
          Teuchos::RCP<CORE::LINALG::SparseOperator> systemmatrix1,
          Teuchos::RCP<CORE::LINALG::SparseOperator> systemmatrix2,
          Teuchos::RCP<Epetra_Vector> systemvector1, Teuchos::RCP<Epetra_Vector> systemvector2,
          Teuchos::RCP<Epetra_Vector> systemvector3) override;

     private:
      /// the actual instance
      static PoroFluidMultiPhaseType instance_;
    };

    /*!
    \brief The PoroFluidMultiPhase element
    */
    class PoroFluidMultiPhase : public DRT::Element
    {
      //! @name Friends
      friend class PoroFluidMultiPhaseType;

     public:
      //! @name Constructors and destructors and related methods

      /*!
      \brief Standard Constructor
      */
      PoroFluidMultiPhase(int id,  ///< A unique global id of this element
          int owner                ///< processor id who owns a certain instance of this class
      );

      /*!
      \brief Copy Constructor

      Makes a deep copy of a Element

      */
      PoroFluidMultiPhase(const PoroFluidMultiPhase& old);

      /*!
      \brief Deep copy this instance of PoroFluidMultiPhase and return pointer to the copy

      The Clone() method is used from the virtual base class Element in cases
      where the type of the derived class is unknown and a copy-ctor is needed

      */
      DRT::Element* Clone() const override;


      //@}

      /// Set element material
      /*!
        Material numbers are read from the input file. The element stores
        a corresponding material object. These material objects can be
        anything from very simple (just a little calculation) to highly
        sophisticated with history data. The material is packed and
        unpacked along with its element.

        \param matnum : material number from input file

        \note reimplementation of this method, due to initialising
              numdofpernode_, since the material is known now.
       */
      void SetMaterial(const int index, Teuchos::RCP<CORE::MAT::Material> mat) override;

      /*!
      \brief Get shape type of element
      */
      CORE::FE::CellType Shape() const override;

      /*!
      \brief set discretization type of element
      */
      virtual void SetDisType(CORE::FE::CellType shape)
      {
        distype_ = shape;
        return;
      };

      /*!
      \brief Return number of lines of this element
      */
      int NumLine() const override;

      /*!
      \brief Return number of surfaces of this element
      */
      int NumSurface() const override;

      /*!
      \brief Return number of volumes of this element
      */
      int NumVolume() const override;

      /*!
      \brief Get vector of Teuchos::RCPs to the lines of this element
      */
      std::vector<Teuchos::RCP<DRT::Element>> Lines() override;

      /*!
      \brief Get vector of Teuchos::RCPs to the surfaces of this element
      */
      std::vector<Teuchos::RCP<DRT::Element>> Surfaces() override;

      /*!
      \brief Return unique ParObject id

      every class implementing ParObject needs a unique id defined at the
      top of this file.
      */
      int UniqueParObjectId() const override
      {
        return PoroFluidMultiPhaseType::Instance().UniqueParObjectId();
      }

      /*!
      \brief Pack this class so it can be communicated

      \ref Pack and \ref Unpack are used to communicate this element

      */
      void Pack(CORE::COMM::PackBuffer& data) const override;

      /*!
      \brief Unpack data from a char vector into this class

      \ref Pack and \ref Unpack are used to communicate this element
      */
      void Unpack(const std::vector<char>& data) override;

      //! @name Access methods


      /*!
      \brief Get number of degrees of freedom of a certain node
             (implements pure virtual DRT::Element)

      The element decides how many degrees of freedom its nodes must have.
      As this may vary along a simulation, the element can re-decide the
      number of degrees of freedom per node along the way for each of it's nodes
      separately.
      */
      int NumDofPerNode(const DRT::Node& node) const override
      {
        if (numdofpernode_ < 1) FOUR_C_THROW("NumDofPerNode is < 1");
        return numdofpernode_;
      }

      /*!
      \brief Get number of degrees of freedom per element
             (implements pure virtual DRT::Element)

      The element decides how many element degrees of freedom it has.
      It can re-decide along the way of a simulation.

      \note Element degrees of freedom mentioned here are dofs that are visible
            at the level of the total system of equations. Purely internal
            element dofs that are condensed internally should NOT be considered.
      */
      int NumDofPerElement() const override { return 0; }

      /*!
      \brief Print this element
      */
      void Print(std::ostream& os) const override;

      /*!
      \brief Return ElementType
      */
      DRT::ElementType& ElementType() const override { return PoroFluidMultiPhaseType::Instance(); }

      //@}

      //! @name Input and Creation

      /*!
      \brief Read input for this element
      */
      bool ReadElement(const std::string& eletype, const std::string& distype,
          INPUT::LineDefinition* linedef) override;

      //@}

      //! @name Evaluation

      /*!
      \brief Evaluate an element (multiple dofset version)

      An element derived from this class uses the Evaluate method to receive commands
      and parameters from some control routine in params and evaluates element matrices and
      vectors accoring to the command in params.

      \note This class implements a dummy of this method that prints a warning and
            returns false.

      \param params (in/out)    : ParameterList for communication between control routine
                                  and elements
      \param discretization (in): A reference to the underlying discretization
      \param la (in)            : location data for all dofsets of the discretization
      \param elemat1 (out)      : matrix to be filled by element depending on commands
                                  given in params
      \param elemat2 (out)      : matrix to be filled by element depending on commands
                                  given in params
      \param elevec1 (out)      : vector to be filled by element depending on commands
                                  given in params
      \param elevec2 (out)      : vector to be filled by element depending on commands
                                  given in params
      \param elevec3 (out)      : vector to be filled by element depending on commands
                                  given in params
      \return 0 if successful, negative otherwise
      */
      int Evaluate(Teuchos::ParameterList& params, DRT::Discretization& discretization,
          LocationArray& la, CORE::LINALG::SerialDenseMatrix& elemat1,
          CORE::LINALG::SerialDenseMatrix& elemat2, CORE::LINALG::SerialDenseVector& elevec1,
          CORE::LINALG::SerialDenseVector& elevec2,
          CORE::LINALG::SerialDenseVector& elevec3) override;

      /*!
      \brief Evaluate a Neumann boundary condition

      this method evaluates a surfaces Neumann condition on the shell element

      \param params (in/out)    : ParameterList for communication between control routine
                                  and elements
      \param discretization (in): A reference to the underlying discretization
      \param condition (in)     : The condition to be evaluated
      \param lm (in)            : location vector of this element
      \param elevec1 (out)      : vector to be filled by element. If nullptr on input,

      \return 0 if successful, negative otherwise
      */
      int evaluate_neumann(Teuchos::ParameterList& params, DRT::Discretization& discretization,
          CORE::Conditions::Condition& condition, std::vector<int>& lm,
          CORE::LINALG::SerialDenseVector& elevec1,
          CORE::LINALG::SerialDenseMatrix* elemat1 = nullptr) override;

      //@}

     protected:
      virtual void Initialize();

     private:
      //! the element discretization type (shape)
      CORE::FE::CellType distype_;

      /*!
       * \brief number of dofs per node (for systems of transport equations)
       * (storage neccessary because we dont know the material in the post filters anymore)
       */
      int numdofpernode_;

      //! don't want = operator
      PoroFluidMultiPhase& operator=(const PoroFluidMultiPhase& old);
    };  // class PoroFluidMultiPhase


    //=======================================================================
    //=======================================================================
    //=======================================================================
    //=======================================================================

    class PoroFluidMultiPhaseBoundaryType : public DRT::ElementType
    {
     public:
      std::string Name() const override { return "PoroFluidMultiPhaseBoundaryType"; }

      static PoroFluidMultiPhaseBoundaryType& Instance();

      Teuchos::RCP<DRT::Element> Create(const int id, const int owner) override;

      void nodal_block_information(
          DRT::Element* dwele, int& numdf, int& dimns, int& nv, int& np) override
      {
      }

      CORE::LINALG::SerialDenseMatrix ComputeNullSpace(
          DRT::Node& node, const double* x0, const int numdof, const int dimnsp) override
      {
        CORE::LINALG::SerialDenseMatrix nullspace;
        FOUR_C_THROW("method ComputeNullSpace not implemented");
        return nullspace;
      }

     private:
      static PoroFluidMultiPhaseBoundaryType instance_;
    };


    /*!
    \brief An element representing a boundary element of a PoroFluidMultiPhase element

    \note This is a pure boundary condition element. Its only
          purpose is to evaluate certain boundary conditions that might be
          adjacent to a parent PoroFluidMultiPhase element.
    */
    class PoroFluidMultiPhaseBoundary : public DRT::FaceElement
    {
     public:
      //! @name Constructors and destructors and related methods

      /*!
      \brief Standard Constructor

      \param id : A unique global id
      \param owner: Processor owning this surface
      \param nnode: Number of nodes attached to this element
      \param nodeids: global ids of nodes attached to this element
      \param nodes: the discretizations map of nodes to build ptrs to nodes from
      \param parent: The parent PoroFluidMultiPhase element of this surface
      \param lsurface: the local surface number of this surface w.r.t. the parent element
      */
      PoroFluidMultiPhaseBoundary(int id, int owner, int nnode, const int* nodeids,
          DRT::Node** nodes, DRT::ELEMENTS::PoroFluidMultiPhase* parent, const int lsurface);

      /*!
      \brief Copy Constructor

      Makes a deep copy of a Element

      */
      PoroFluidMultiPhaseBoundary(const PoroFluidMultiPhaseBoundary& old);

      /*!
      \brief Deep copy this instance of an element and return pointer to the copy

      The Clone() method is used from the virtual base class Element in cases
      where the type of the derived class is unknown and a copy-ctor is needed

      */
      DRT::Element* Clone() const override;

      /*!
      \brief Get shape type of element
      */
      CORE::FE::CellType Shape() const override;

      /*!
      \brief Return number of lines of boundary element
      */
      int NumLine() const override;

      /*!
      \brief Return number of surfaces of boundary element
       */
      int NumSurface() const override;

      /*!
      \brief Get vector of Teuchos::RCPs to the lines of this element

      */
      std::vector<Teuchos::RCP<DRT::Element>> Lines() override;

      /*!
      \brief Get vector of Teuchos::RCPs to the surfaces of this element
      */
      std::vector<Teuchos::RCP<DRT::Element>> Surfaces() override;

      /*!
      \brief Return unique ParObject id

      every class implementing ParObject needs a unique id defined at the
      top of the parobject.H file.
      */
      int UniqueParObjectId() const override
      {
        return PoroFluidMultiPhaseBoundaryType::Instance().UniqueParObjectId();
      }

      /*!
      \brief Pack this class so it can be communicated

      \ref Pack and \ref Unpack are used to communicate this element

      */
      void Pack(CORE::COMM::PackBuffer& data) const override;

      /*!
      \brief Unpack data from a char vector into this class

      \ref Pack and \ref Unpack are used to communicate this element

      */
      void Unpack(const std::vector<char>& data) override;


      //@}

      //! @name Access methods


      /*!
      \brief Get number of degrees of freedom of a certain node
             (implements pure virtual DRT::Element)

      The element decides how many degrees of freedom its nodes must have.
      As this may vary along a simulation, the element can re-decide the
      number of degrees of freedom per node along the way for each of it's nodes
      separately.
      */
      int NumDofPerNode(const DRT::Node& node) const override
      {
        return parent_element()->NumDofPerNode(node);
      }

      //! Return a pointer to the parent element of this boundary element
      virtual DRT::ELEMENTS::PoroFluidMultiPhase* parent_element() const
      {
        DRT::Element* parent = DRT::FaceElement::parent_element();
        // make sure the static cast below is really valid
        FOUR_C_ASSERT(dynamic_cast<DRT::ELEMENTS::PoroFluidMultiPhase*>(parent) != nullptr,
            "Master element is no PoroFluidMultiPhase element");
        return static_cast<DRT::ELEMENTS::PoroFluidMultiPhase*>(parent);
      }


      /*!
      \brief Get number of degrees of freedom per element
             (implements pure virtual DRT::Element)

      The element decides how many element degrees of freedom it has.
      It can re-decide along the way of a simulation.

      \note Element degrees of freedom mentioned here are dofs that are visible
            at the level of the total system of equations. Purely internal
            element dofs that are condensed internally should NOT be considered.
      */
      int NumDofPerElement() const override { return 0; }

      /*!
      \brief Print this element
      */
      void Print(std::ostream& os) const override;

      /*!
      \brief Return ElementType
      */
      DRT::ElementType& ElementType() const override
      {
        return PoroFluidMultiPhaseBoundaryType::Instance();
      }

      //@}

      //! @name Evaluation

      /*!
      \brief Evaluate an element

      An element derived from this class uses the Evaluate method to receive commands
      and parameters from some control routine in params and evaluates element matrices and
      vectors accoring to the command in params.

      \note This class implements a dummy of this method that prints a warning and
            returns false.

      \param params (in/out)    : ParameterList for communication between control routine
                                  and elements
      \param discretization (in): A reference to the underlying discretization
      \param la (in)            : location data for all dofsets of the discretization
      \param elemat1 (out)      : matrix to be filled by element depending on commands
                                  given in params
      \param elemat2 (out)      : matrix to be filled by element depending on commands
                                  given in params
      \param elevec1 (out)      : vector to be filled by element depending on commands
                                  given in params
      \param elevec2 (out)      : vector to be filled by element depending on commands
                                  given in params
      \param elevec3 (out)      : vector to be filled by element depending on commands
                                  given in params
      \return 0 if successful, negative otherwise
      */
      int Evaluate(Teuchos::ParameterList& params, DRT::Discretization& discretization,
          LocationArray& la, CORE::LINALG::SerialDenseMatrix& elemat1,
          CORE::LINALG::SerialDenseMatrix& elemat2, CORE::LINALG::SerialDenseVector& elevec1,
          CORE::LINALG::SerialDenseVector& elevec2,
          CORE::LINALG::SerialDenseVector& elevec3) override;

      /*!
      \brief Evaluate an element

      An element derived from this class uses the Evaluate method to receive commands
      and parameters from some control routine in params and evaluates element matrices and
      vectors accoring to the command in params.

      \note This class implements a dummy of this method that prints a warning and
            returns false.

      \param params (in/out)    : ParameterList for communication between control routine
                                  and elements
      \param discretization (in): A reference to the underlying discretization
      \param lm (in)            : location vector of this element
      \param elemat1 (out)      : matrix to be filled by element depending on commands
                                  given in params
      \param elemat2 (out)      : matrix to be filled by element depending on commands
                                  given in params
      \param elevec1 (out)      : vector to be filled by element depending on commands
                                  given in params
      \param elevec2 (out)      : vector to be filled by element depending on commands
                                  given in params
      \param elevec3 (out)      : vector to be filled by element depending on commands
                                  given in params
      \return 0 if successful, negative otherwise
      */
      int Evaluate(Teuchos::ParameterList& params, DRT::Discretization& discretization,
          std::vector<int>& lm, CORE::LINALG::SerialDenseMatrix& elemat1,
          CORE::LINALG::SerialDenseMatrix& elemat2, CORE::LINALG::SerialDenseVector& elevec1,
          CORE::LINALG::SerialDenseVector& elevec2,
          CORE::LINALG::SerialDenseVector& elevec3) override;

      //@}

      //! @name Evaluate methods

      /*!
      \brief Evaluate a Neumann boundary condition

      this method evaluates a surface Neumann condition on the PoroFluidMultiPhase element

      \param params (in/out)    : ParameterList for communication between control routine
                                  and elements
      \param discretization (in): A reference to the underlying discretization
      \param condition (in)     : The condition to be evaluated
      \param lm (in)            : location vector of this element
      \param elevec1 (out)      : vector to be filled by element. If nullptr on input,

      \return 0 if successful, negative otherwise
      */
      int evaluate_neumann(Teuchos::ParameterList& params, DRT::Discretization& discretization,
          CORE::Conditions::Condition& condition, std::vector<int>& lm,
          CORE::LINALG::SerialDenseVector& elevec1,
          CORE::LINALG::SerialDenseMatrix* elemat1 = nullptr) override;

      //@}

     private:
      // don't want = operator
      PoroFluidMultiPhaseBoundary& operator=(const PoroFluidMultiPhaseBoundary& old);

    };  // class PoroFluidMultiPhaseBoundary


  }  // namespace ELEMENTS
}  // namespace DRT


FOUR_C_NAMESPACE_CLOSE

#endif
