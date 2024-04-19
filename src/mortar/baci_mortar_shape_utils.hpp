/*-----------------------------------------------------------------------*/
/*! \file
\brief file for mortar shape utils --> shape calculations

\level 1

*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | definitions                                              farah 01/14 |
 *----------------------------------------------------------------------*/
#ifndef FOUR_C_MORTAR_SHAPE_UTILS_HPP
#define FOUR_C_MORTAR_SHAPE_UTILS_HPP

/*----------------------------------------------------------------------*
 | Header                                                   farah 01/14 |
 *----------------------------------------------------------------------*/
#include "baci_config.hpp"

#include "baci_discretization_fem_general_utils_local_connectivity_matrices.hpp"
#include "baci_discretization_fem_general_utils_nurbs_shapefunctions.hpp"
#include "baci_linalg_utils_densematrix_inverse.hpp"
#include "baci_linalg_utils_densematrix_multiply.hpp"
#include "baci_mortar_element.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 | Utils                                                    farah 01/14 |
 *----------------------------------------------------------------------*/
namespace MORTAR
{
  class Node;

  namespace UTILS
  {
    // ----------------------------------------------------------------------
    // TAW 01/14
    // function declarations
    // modern compilers expect (template) functions properly declared first!
    // old gcc is a little bit sloppy when handling template functions even
    // though i'm quite sure that it would not have accepted the code before
    // when there would not have been template parameters, which made it auto-
    // matically declare the missing function declarations.
    // Note: do not forget to update these declarations in case if you change
    //       the parameters! You will get strange compiler errors then!
    // ----------------------------------------------------------------------

    template <class V>
    void mortar_shape_function_1D(V& funct,      ///< to be filled with shape function values
        const double& r,                         ///< r coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    );

    template <class V>
    void mortar_dualshape_function_1D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& r,                         ///< r coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    );

    template <class V>
    void mortar_shape_function_2D(V& funct,      ///< to be filled with shape function values
        const double& xi0,                       ///< xi0 coordinate
        const double& xi1,                       ///< xi1 coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    );

    template <class V>
    void mortar_dualshape_function_2D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,                       ///< xi0 coordinate
        const double& xi1,                       ///< xi1 coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    );

    // nurbs shape function evaluation
    template <class V>
    void mortar_nurbs_shape_function_1D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    );

    template <class V>
    void mortar_nurbs_shape_function_2D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const double& xi1,               ///< xi0 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    );

    template <class V>
    void mortar_nurbs_dualshape_function_1D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    );

    template <class V>
    void mortar_nurbs_dualshape_function_2D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const double& xi1,               ///< xi1 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    );

    // ----------------------------------------------------------------------
    // function definitions
    // ----------------------------------------------------------------------

    /*----------------------------------------------------------------------*
     |  Evaluate displacement shape functions                     popp 01/08|
     *----------------------------------------------------------------------*/
    template <class V>
    void EvaluateShape_Displ(const double* xi, V& val, MORTAR::Element& ele, bool dualquad)
    {
      if (!xi) FOUR_C_THROW("EvaluateShape called with xi=nullptr");

      //! ns_: number of element nodes
      int nnode = ele.NumNode();

      // get node number and node pointers
      DRT::Node** mynodes = ele.Nodes();
      if (!mynodes) FOUR_C_THROW("EvaluateShapeLagMult: Null pointer!");

      // one-noded elements are directly processed here, shape independent evaluation possible
      // valid for 1- and 2-dimensional interfaces
      if (nnode == 1)
      {
        mortar_shape_function_2D(val, -1.0, -1.0, MORTAR::Element::p0);
        return;
      }

      // check for boundary nodes
      bool bound = false;
      for (int i = 0; i < nnode; ++i)
      {
        Node* mymrtrnode = dynamic_cast<Node*>(mynodes[i]);
        if (!mymrtrnode) FOUR_C_THROW("EvaluateShapeLagMult: Null pointer!");
        bound += mymrtrnode->IsOnBound();
      }

      switch (ele.Shape())
      {
        // 2D linear case (2noded line element)
        case CORE::FE::CellType::line2:
        {
          if (nnode != 2) FOUR_C_THROW("Inconsistency in EvaluateShape");
          mortar_shape_function_1D(val, xi[0], MORTAR::Element::lin1D);
          break;
        }
        // 2D quadratic case (3noded line element)
        case CORE::FE::CellType::line3:
        {
          if (nnode != 3) FOUR_C_THROW("Inconsistency in EvaluateShape");
          if (dualquad && !bound)
            FOUR_C_THROW(
                "There is no quadratic interpolation for dual shape functions for 2-D problems "
                "with quadratic elements available!");
          else if (dualquad && bound)
            mortar_shape_function_1D(val, xi[0], MORTAR::Element::quad1D_hierarchical);
          else
            mortar_shape_function_1D(val, xi[0], MORTAR::Element::quad1D);
          break;
        }
        // 3D linear case (3noded triangular element)
        case CORE::FE::CellType::tri3:
        {
          if (nnode != 3) FOUR_C_THROW("Inconsistency in EvaluateShape");
          mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::lin2D);
          break;
        }
        // 3D bilinear case (4noded quadrilateral element)
        case CORE::FE::CellType::quad4:
        {
          if (nnode != 4) FOUR_C_THROW("Inconsistency in EvaluateShape");
          mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::bilin2D);
          break;
        }
        // 3D quadratic case (6noded triangular element)
        case CORE::FE::CellType::tri6:
        {
          if (nnode != 6) FOUR_C_THROW("Inconsistency in EvaluateShape");
          if (dualquad && !bound)
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::quad2D_modified);
          else if (dualquad && bound)
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::quad2D_hierarchical);
          else
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::quad2D);
          break;
        }
        // 3D serendipity case (8noded quadrilateral element)
        case CORE::FE::CellType::quad8:
        {
          if (nnode != 8) FOUR_C_THROW("Inconsistency in EvaluateShape");
          if (dualquad && !bound)
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::serendipity2D_modified);
          else if (dualquad && bound)
            mortar_shape_function_2D(
                val, xi[0], xi[1], MORTAR::Element::serendipity2D_hierarchical);
          else
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::serendipity2D);
          break;
        }
        // 3D biquadratic case (9noded quadrilateral element)
        case CORE::FE::CellType::quad9:
        {
          if (nnode != 9) FOUR_C_THROW("Inconsistency in EvaluateShape");
          if (dualquad && !bound)
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::biquad2D_modified);
          else if (dualquad && bound)
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::biquad2D_hierarchical);
          else
            mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::biquad2D);
          break;
        }
        //==================================================
        //                     NURBS
        //==================================================
        case CORE::FE::CellType::nurbs2:
        {
          if (nnode != 2) FOUR_C_THROW("Inconsistency in EvaluateShape");
          mortar_nurbs_shape_function_1D(val, ele, xi[0], CORE::FE::CellType::nurbs2);
          break;
        }
        case CORE::FE::CellType::nurbs3:
        {
          if (nnode != 3) FOUR_C_THROW("Inconsistency in EvaluateShape");
          mortar_nurbs_shape_function_1D(val, ele, xi[0], CORE::FE::CellType::nurbs3);
          break;
        }

        // 2D -- nurbs9
        case CORE::FE::CellType::nurbs9:
        {
          if (nnode != 9) FOUR_C_THROW("Inconsistency in EvaluateShape");
          mortar_nurbs_shape_function_2D(val, ele, xi[0], xi[1], CORE::FE::CellType::nurbs9);
          break;
        }
        // unknown case
        default:
        {
          FOUR_C_THROW("EvaluateShape called for unknown MORTAR::Element type");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  Evaluate Lagrange multiplier shape functions              popp 12/07|
     *----------------------------------------------------------------------*/
    template <class V>
    void EvaluateShape_LM(const INPAR::MORTAR::ShapeFcn& lmtype, const double* xi, V& val,
        MORTAR::Element& ele, const int& valdim)
    {
      if (!xi) FOUR_C_THROW("EvaluateShapeLagMult called with xi=nullptr");

      // dual LM shape functions or not
      bool dual = false;
      if (lmtype == INPAR::MORTAR::shape_dual || lmtype == INPAR::MORTAR::shape_petrovgalerkin)
        dual = true;

      // get node number and node pointers
      DRT::Node** mynodes = ele.Nodes();
      if (!mynodes) FOUR_C_THROW("EvaluateShapeLagMult: Null pointer!");

      // one-noded elements are directly processed here, shape independent evaluation possible
      // valid for 1- and 2-dimensional interfaces
      if (ele.NumNode() == 1)
      {
        mortar_shape_function_2D(val, -1.0, -1.0, MORTAR::Element::p0);
        return;
      }

      switch (ele.Shape())
      {
        // 2D linear case (2noded line element)
        case CORE::FE::CellType::line2:
        {
          if (valdim != 2) FOUR_C_THROW("Inconsistency in EvaluateShape");

          if (dual)
            mortar_dualshape_function_1D(val, ele, xi[0], MORTAR::Element::lindual1D);
          else
            mortar_shape_function_1D(val, xi[0], MORTAR::Element::lin1D);

          break;
        }

        // 2D quadratic case (3noded line element)
        case CORE::FE::CellType::line3:
        {
          if (valdim != 3) FOUR_C_THROW("Inconsistency in EvaluateShape");

          if (dual)
            mortar_dualshape_function_1D(val, ele, xi[0], MORTAR::Element::quaddual1D);
          else
            mortar_shape_function_1D(val, xi[0], MORTAR::Element::quad1D);
          break;
        }

        // 3D cases
        case CORE::FE::CellType::tri3:
        case CORE::FE::CellType::quad4:
        case CORE::FE::CellType::tri6:
        case CORE::FE::CellType::quad8:
        case CORE::FE::CellType::quad9:
        {
          // dual Lagrange multipliers
          if (dual)
          {
            if (ele.Shape() == CORE::FE::CellType::tri3)
              mortar_dualshape_function_2D(val, ele, xi[0], xi[1], MORTAR::Element::lindual2D);
            else if (ele.Shape() == CORE::FE::CellType::quad4)
              mortar_dualshape_function_2D(val, ele, xi[0], xi[1], MORTAR::Element::bilindual2D);
            else if (ele.Shape() == CORE::FE::CellType::tri6)
              mortar_dualshape_function_2D(val, ele, xi[0], xi[1], MORTAR::Element::quaddual2D);
            else if (ele.Shape() == CORE::FE::CellType::quad8)
              mortar_dualshape_function_2D(
                  val, ele, xi[0], xi[1], MORTAR::Element::serendipitydual2D);
            else /*Shape()==quad9*/
              mortar_dualshape_function_2D(val, ele, xi[0], xi[1], MORTAR::Element::biquaddual2D);
          }

          // standard Lagrange multipliers
          else
          {
            if (ele.Shape() == CORE::FE::CellType::tri3)
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::lin2D);
            else if (ele.Shape() == CORE::FE::CellType::quad4)
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::bilin2D);
            else if (ele.Shape() == CORE::FE::CellType::tri6)
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::quad2D);
            else if (ele.Shape() == CORE::FE::CellType::quad8)
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::serendipity2D);
            else /*Shape()==quad9*/
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::biquad2D);
          }

          break;
        }

        //==================================================
        //                     NURBS
        //==================================================

        // 1D -- nurbs2
        case CORE::FE::CellType::nurbs2:
        {
          if (valdim != 2) FOUR_C_THROW("Inconsistency in EvaluateShape");

          if (dual)
            FOUR_C_THROW("no dual shape functions provided for nurbs!");
          else
            mortar_nurbs_shape_function_1D(val, ele, xi[0], CORE::FE::CellType::nurbs2);

          break;
        }

        // 1D -- nurbs3
        case CORE::FE::CellType::nurbs3:
        {
          if (valdim != 3) FOUR_C_THROW("Inconsistency in EvaluateShape");

          if (dual)
            mortar_nurbs_dualshape_function_1D(val, ele, xi[0], CORE::FE::CellType::nurbs3);
          else
            mortar_nurbs_shape_function_1D(val, ele, xi[0], CORE::FE::CellType::nurbs3);

          break;
        }

        // 2D -- nurbs9
        case CORE::FE::CellType::nurbs9:
        {
          if (valdim != 9) FOUR_C_THROW("Inconsistency in EvaluateShape");

          if (dual)
            mortar_nurbs_dualshape_function_2D(val, ele, xi[0], xi[1], CORE::FE::CellType::nurbs9);
          else
            mortar_nurbs_shape_function_2D(val, ele, xi[0], xi[1], CORE::FE::CellType::nurbs9);

          break;
        }

        // unknown case
        default:
        {
          FOUR_C_THROW("EvaluateShapeLagMult called for unknown element type");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  Evaluate Lagrange multiplier shape functions             seitz 09/17|
     |  THIS IS A SPECIAL VERSION FOR 3D QUADRATIC MORTAR WITH CONST LM!    |
     *----------------------------------------------------------------------*/
    template <class V>
    void EvaluateShape_LM_Const(const INPAR::MORTAR::ShapeFcn& lmtype, const double* xi, V& val,
        MORTAR::Element& ele, const int& valdim)
    {
      switch (ele.Shape())
      {
        case CORE::FE::CellType::line3:
          val(0) = val(1) = 0.;
          val(2) = 1.;
          break;
        case CORE::FE::CellType::quad9:
          val(0) = val(1) = val(2) = val(3) = val(4) = val(5) = val(6) = val(7) = 0.;
          val(8) = 1.;
          break;
        default:
          FOUR_C_THROW("shape not supported");
      }
    }

    /*----------------------------------------------------------------------*
     |  Evaluate Lagrange multiplier shape functions              popp 12/07|
     |  THIS IS A SPECIAL VERSION FOR 3D QUADRATIC MORTAR WITH LIN LM!      |
     *----------------------------------------------------------------------*/
    template <class V>
    void EvaluateShape_LM_Lin(const INPAR::MORTAR::ShapeFcn& lmtype, const double* xi, V& val,
        MORTAR::Element& ele, const int& valdim)
    {
      if (!xi) FOUR_C_THROW("EvaluateShapeLagMultLin called with xi=nullptr");
      if (!ele.IsSlave()) FOUR_C_THROW("EvaluateShapeLagMultLin called for master element");

      // check for feasible element types (line3,tri6, quad8 or quad9)
      if (ele.Shape() != CORE::FE::CellType::line3 && ele.Shape() != CORE::FE::CellType::tri6 &&
          ele.Shape() != CORE::FE::CellType::quad8 && ele.Shape() != CORE::FE::CellType::quad9)
        FOUR_C_THROW("Linear LM interpolation only for quadratic finite elements");

      // dual shape functions or not
      bool dual = false;
      if (lmtype == INPAR::MORTAR::shape_dual || lmtype == INPAR::MORTAR::shape_petrovgalerkin)
        dual = true;

      // get node number and node pointers
      DRT::Node** mynodes = ele.Nodes();
      if (!mynodes) FOUR_C_THROW("EvaluateShapeLagMult: Null pointer!");

      // check for boundary nodes
      bool bound = false;
      for (int i = 0; i < ele.NumNode(); ++i)
      {
        Node* mymrtrnode = dynamic_cast<Node*>(mynodes[i]);
        if (!mymrtrnode) FOUR_C_THROW("EvaluateShapeLagMult: Null pointer!");
        bound += mymrtrnode->IsOnBound();
      }

      // all nodes are interior: use unmodified shape functions
      if (!bound)
      {
        FOUR_C_THROW("You should not be here...");
      }

      switch (ele.Shape())
      {
        // 2D quadratic case (quadratic line)
        case CORE::FE::CellType::line3:
        {
          // the middle node is defined as slave boundary (=master)
          // dual Lagrange multipliers
          if (dual)
            mortar_dualshape_function_1D(val, ele, xi[0], MORTAR::Element::quaddual1D_only_lin);
          // standard Lagrange multipliers
          else
            mortar_shape_function_1D(val, xi[0], MORTAR::Element::quad1D_only_lin);

          break;
        }

        // 3D quadratic cases (quadratic triangle, biquadratic and serendipity quad)
        case CORE::FE::CellType::tri6:
        case CORE::FE::CellType::quad8:
        case CORE::FE::CellType::quad9:
        {
          // the edge nodes are defined as slave boundary (=master)
          // dual Lagrange multipliers
          if (dual)
          {
            // FOUR_C_THROW("Quad->Lin modification of dual LM shape functions not yet
            // implemented");
            if (ele.Shape() == CORE::FE::CellType::tri6)
              mortar_dualshape_function_2D(
                  val, ele, xi[0], xi[1], MORTAR::Element::quaddual2D_only_lin);
            else if (ele.Shape() == CORE::FE::CellType::quad8)
              mortar_dualshape_function_2D(
                  val, ele, xi[0], xi[1], MORTAR::Element::serendipitydual2D_only_lin);
            else /*Shape()==quad9*/
              mortar_dualshape_function_2D(
                  val, ele, xi[0], xi[1], MORTAR::Element::biquaddual2D_only_lin);
          }

          // standard Lagrange multipliers
          else
          {
            if (ele.Shape() == CORE::FE::CellType::tri6)
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::quad2D_only_lin);
            else if (ele.Shape() == CORE::FE::CellType::quad8)
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::serendipity2D_only_lin);
            else /*Shape()==quad9*/
              mortar_shape_function_2D(val, xi[0], xi[1], MORTAR::Element::biquad2D_only_lin);
          }

          break;
        }

        // unknown case
        default:
        {
          FOUR_C_THROW("EvaluateShapeLagMult called for unknown element type");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate std. shape 1D                                   farah 01/14|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_shape_function_1D(V& funct,      ///< to be filled with shape function values
        const double& r,                         ///< r coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    )
    {
      switch (shape)
      {
        // *********************************************************************
        // 1D standard linear shape functions (line2)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::lin1D:
        {
          funct(0) = 0.5 * (1.0 - r);
          funct(1) = 0.5 * (1.0 + r);
          break;
        }
        // *********************************************************************
        // 1D modified standard shape functions (const replacing linear, line2)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // *********************************************************************
        case MORTAR::Element::lin1D_edge0:
        {
          funct(0) = 0.0;
          funct(1) = 1.0;
          break;
        }
        // *********************************************************************
        // 1D modified standard shape functions (const replacing linear, line2)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // *********************************************************************
        case MORTAR::Element::lin1D_edge1:
        {
          funct(0) = 1.0;
          funct(1) = 0.0;
          break;
        }
        // *********************************************************************
        // 1D standard quadratic shape functions (line3)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::quad1D:
        {
          funct(0) = 0.5 * r * (r - 1.0);
          funct(1) = 0.5 * r * (r + 1.0);
          funct(2) = (1.0 - r) * (1.0 + r);
          break;
        }
        // *********************************************************************
        // 1D modified (hierarchical) shape functions (line3)
        // (used in combination with linear dual LM field in 2D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::quad1D_hierarchical:
        {
          funct(0) = 0.5 * (1 - r);
          funct(1) = 0.5 * (1 + r);
          funct(2) = (1.0 - r) * (1.0 + r);

          break;
        }
        // *********************************************************************
        // 1D modified standard shape functions (linear replacing quad, line3)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // *********************************************************************
        case MORTAR::Element::quad1D_edge0:
        {
          funct(0) = 0.0;
          funct(1) = r;
          funct(2) = 1.0 - r;
          break;
        }
        // *********************************************************************
        // 1D modified standard shape functions (linear replacing quad, line3)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // *********************************************************************
        case MORTAR::Element::quad1D_edge1:
        {
          funct(0) = -r;
          funct(1) = 0.0;
          funct(2) = 1.0 + r;
          break;
        }
        // *********************************************************************
        // 1D linear part of standard quadratic shape functions (line3)
        // (used for linear interpolation of std LM field in 2D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::quad1D_only_lin:
        {
          funct(0) = 0.5 * (1 - r);
          funct(1) = 0.5 * (1 + r);
          funct(2) = 0.0;
          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown\n");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate dual shape 1D                                   farah 01/14|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_dualshape_function_1D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& r,                         ///< r coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    )
    {
      switch (shape)
      {
        // *********************************************************************
        // 1D dual linear shape functions (line2)
        // (used for interpolation of Lagrange multiplier field)
        // *********************************************************************
        case MORTAR::Element::lindual1D:
        {
          // use element-based dual shape functions if no coefficient matrix is stored
          if (ele.MoData().DualShape() == Teuchos::null)
          {
            funct(0) = 0.5 * (1 - 3.0 * r);
            funct(1) = 0.5 * (1 + 3.0 * r);
          }

          // pre-calculated consistent dual shape functions
          else
          {
            if (ele.MoData().DualShape()->numCols() != 2 &&
                ele.MoData().DualShape()->numRows() != 2)
              FOUR_C_THROW("Dual shape functions coefficient matrix calculated in the wrong size");

            CORE::LINALG::Matrix<2, 1> stdval;
            double xi[1] = {r};
            EvaluateShape_Displ(xi, stdval, ele, false);

            CORE::LINALG::SerialDenseMatrix& ae = *(ele.MoData().DualShape());

            for (int i = 0; i < 2; ++i)
            {
              funct(i) = 0.0;
              for (int j = 0; j < 2; ++j)
              {
                funct(i) += stdval(j) * ae(i, j);
              }
            }
          }
          break;
        }
        // *********************************************************************
        // 1D modified dual shape functions (const replacing linear, line2)
        // (used for interpolation of Lagrange multiplier field near boundaries)
        // *********************************************************************
        case MORTAR::Element::lindual1D_edge0:
        {
          funct(0) = 0.0;
          funct(1) = 1.0;
          break;
        }
        // *********************************************************************
        // 1D modified dual shape functions (const replacing linear, line2)
        // (used for interpolation of Lagrange multiplier field near boundaries)
        // *********************************************************************
        case MORTAR::Element::lindual1D_edge1:
        {
          funct(0) = 1.0;
          funct(1) = 0.0;
          break;
        }
        // *********************************************************************
        // 1D dual quadratic shape functions (line3)
        // (used for interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // *********************************************************************
        case MORTAR::Element::quaddual1D:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 3;

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());

          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::Matrix<nnodes, nnodes> de(true);
          CORE::LINALG::Matrix<nnodes, nnodes> ae;

          CORE::LINALG::Matrix<nnodes, 1> stdval;
          CORE::LINALG::Matrix<nnodes, 1> valtemp;

          for (int i = 0; i < nnodes; ++i) valtemp(i) = 0.0;

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};

            EvaluateShape_Displ(gpc, stdval, ele, false);

            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * stdval(j) * stdval(k) * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * stdval(j) * detg;
              }
            }
          }

          // get solution matrix with dual parameters
          CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);

          // evaluate dual shape functions at loc. coord. xi
          // need standard shape functions at xi first
          double xi[1] = {r};
          EvaluateShape_Displ(xi, stdval, ele, false);

          // evaluate dual shape functions
          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) valtemp(i) += ae(i, j) * stdval(j);

          funct(0) = valtemp(0);
          funct(1) = valtemp(1);
          funct(2) = valtemp(2);

          break;
        }
        // *********************************************************************
        // 1D dual quadratic shape functions (line3)
        // (used for LINEAR interpolation of Lagrange multiplier field)
        // (used for interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // *********************************************************************
        case MORTAR::Element::quaddual1D_only_lin:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 3;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::SerialDenseMatrix de(nnodes, nnodes, true);
          CORE::LINALG::SerialDenseMatrix ae(nnodes, nnodes);

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.ShapeFunctions(MORTAR::Element::quad1D_only_lin, gpc, valquad, derivquad);

            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // how many non-zero nodes
          const int nnodeslin = 2;

          // reduce me to non-zero nodes before inverting
          CORE::LINALG::Matrix<nnodeslin, nnodeslin> melin;
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) melin(j, k) = me(j, k);

          // invert bi-ortho matrix melin
          CORE::LINALG::Inverse(melin);

          // re-inflate inverse of melin to full size
          CORE::LINALG::SerialDenseMatrix invme(nnodes, nnodes, true);
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) invme(j, k) = melin(j, k);

          // get solution matrix with dual parameters
          CORE::LINALG::multiply(ae, de, invme);

          // evaluate dual shape functions at loc. coord. xi
          double xi[1] = {r};
          ele.ShapeFunctions(MORTAR::Element::quad1D_only_lin, xi, valquad, derivquad);

          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        // *********************************************************************
        // 1D modified dual shape functions (linear replacing quad, line3)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // (only form a basis and have to be adapted for distorted elements)
        // *********************************************************************
        case MORTAR::Element::dual1D_base_for_edge0:
        {
          funct(0) = r;
          funct(1) = 1.0 - r;
          break;
        }
        // *********************************************************************
        // 1D modified dual shape functions (linear replacing quad, line3)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // (only form a basis and have to be adapted for distorted elements)
        // *********************************************************************
        case MORTAR::Element::dual1D_base_for_edge1:
        {
          funct(0) = -r;
          funct(1) = 1.0 + r;
          break;
        }
        // *********************************************************************
        // 1D modified dual shape functions (linear replacing quad, line3)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // (including adaption process for distorted elements)
        // *********************************************************************
        case MORTAR::Element::quaddual1D_edge0:
        {
          // establish fundamental data
          double detg = 0.0;
          int nnodes = ele.NumNode();

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 1);
          CORE::LINALG::SerialDenseVector vallin(nnodes - 1);
          CORE::LINALG::SerialDenseMatrix derivlin(nnodes - 1, 1);
          CORE::LINALG::SerialDenseVector valtemp(nnodes, true);
          CORE::LINALG::SerialDenseMatrix derivtemp(nnodes, 1, true);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());

          CORE::LINALG::SerialDenseMatrix me(nnodes - 1, nnodes - 1, true);
          CORE::LINALG::SerialDenseMatrix de(nnodes - 1, nnodes - 1, true);

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), 0.0};
            ele.ShapeFunctions(MORTAR::Element::quad1D, gpc, valquad, derivquad);
            ele.ShapeFunctions(MORTAR::Element::dual1D_base_for_edge0, gpc, vallin, derivlin);
            detg = ele.Jacobian(gpc);

            for (int j = 1; j < nnodes; ++j)
              for (int k = 1; k < nnodes; ++k)
              {
                me(j - 1, k - 1) += integrator.Weight(i) * vallin[j - 1] * valquad[k] * detg;
                de(j - 1, k - 1) += (j == k) * integrator.Weight(i) * valquad[k] * detg;
              }
          }

          // invert bi-ortho matrix me
          // CAUTION: This is a non-symmetric inverse operation!
          double detme = me(0, 0) * me(1, 1) - me(0, 1) * me(1, 0);
          CORE::LINALG::SerialDenseMatrix meold(nnodes - 1, nnodes - 1);
          meold = me;
          me(0, 0) = 1 / detme * meold(1, 1);
          me(0, 1) = -1 / detme * meold(0, 1);
          me(1, 0) = -1 / detme * meold(1, 0);
          me(1, 1) = 1 / detme * meold(0, 0);

          // get solution matrix with dual parameters
          CORE::LINALG::SerialDenseMatrix ae(nnodes - 1, nnodes - 1);
          CORE::LINALG::multiply(ae, de, me);

          // evaluate dual shape functions at loc. coord. r
          double xi[1] = {r};
          ele.ShapeFunctions(MORTAR::Element::dual1D_base_for_edge0, xi, vallin, derivlin);
          for (int i = 1; i < nnodes; ++i)
            for (int j = 1; j < nnodes; ++j)
            {
              valtemp[i] += ae(i - 1, j - 1) * vallin[j - 1];
              derivtemp(i, 0) += ae(i - 1, j - 1) * derivlin(j - 1, 0);
            }

          funct(0) = 0.0;
          funct(1) = valtemp[1];
          funct(2) = valtemp[2];

          break;
        }
        // *********************************************************************
        // 1D modified dual shape functions (linear replacing quad, line3)
        // (used for interpolation of Lagrange mult. field near boundaries)
        // (including adaption process for distorted elements)
        // *********************************************************************
        case MORTAR::Element::quaddual1D_edge1:
        {
          // establish fundamental data
          double detg = 0.0;
          int nnodes = ele.NumNode();

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 1);
          CORE::LINALG::SerialDenseVector vallin(nnodes - 1);
          CORE::LINALG::SerialDenseMatrix derivlin(nnodes - 1, 1);
          CORE::LINALG::SerialDenseVector valtemp(nnodes, true);
          CORE::LINALG::SerialDenseMatrix derivtemp(nnodes, 1, true);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());

          CORE::LINALG::SerialDenseMatrix me(nnodes - 1, nnodes - 1, true);
          CORE::LINALG::SerialDenseMatrix de(nnodes - 1, nnodes - 1, true);

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), 0.0};
            ele.ShapeFunctions(MORTAR::Element::quad1D, gpc, valquad, derivquad);
            ele.ShapeFunctions(MORTAR::Element::dual1D_base_for_edge1, gpc, vallin, derivlin);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes - 1; ++j)
              for (int k = 0; k < nnodes - 1; ++k)
              {
                me(j, k) += integrator.Weight(i) * vallin[j] * valquad[2 * k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[2 * k] * detg;
              }
          }

          // invert bi-ortho matrix me
          // CAUTION: This is a non-symmetric inverse operation!
          double detme = me(0, 0) * me(1, 1) - me(0, 1) * me(1, 0);
          CORE::LINALG::SerialDenseMatrix meold(nnodes - 1, nnodes - 1);
          meold = me;
          me(0, 0) = 1 / detme * meold(1, 1);
          me(0, 1) = -1 / detme * meold(0, 1);
          me(1, 0) = -1 / detme * meold(1, 0);
          me(1, 1) = 1 / detme * meold(0, 0);

          // get solution matrix with dual parameters
          CORE::LINALG::SerialDenseMatrix ae(nnodes - 1, nnodes - 1);
          CORE::LINALG::multiply(ae, de, me);

          // evaluate dual shape functions at loc. coord. r
          double xi[1] = {r};
          ele.ShapeFunctions(MORTAR::Element::dual1D_base_for_edge1, xi, vallin, derivlin);
          for (int i = 0; i < nnodes - 1; ++i)
            for (int j = 0; j < nnodes - 1; ++j)
            {
              valtemp[2 * i] += ae(i, j) * vallin[j];
              derivtemp(2 * i, 0) += ae(i, j) * derivlin(j, 0);
            }

          funct(0) = valtemp[0];
          funct(1) = 0.0;
          funct(2) = valtemp[2];

          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown\n");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate std. shape 2D                                   farah 01/14|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_shape_function_2D(V& funct,      ///< to be filled with shape function values
        const double& xi0,                       ///< xi0 coordinate
        const double& xi1,                       ///< xi1 coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    )
    {
      switch (shape)
      {
        // *********************************************************************
        // constant shape function per element
        // (used for interpolation of LM field and primary variable)
        // *********************************************************************
        case MORTAR::Element::p0:
        {
          funct(0) = 1;
          break;
        }
        // *********************************************************************
        // 2D standard linear shape functions (tri3)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::lin2D:
        {
          funct(0) = 1 - xi0 - xi1;
          funct(1) = xi0;
          funct(2) = xi1;
          break;
        }
        // *********************************************************************
        // 2D standard bilinear shape functions (quad4)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::bilin2D:
        {
          funct(0) = 0.25 * (1 - xi0) * (1 - xi1);
          funct(1) = 0.25 * (1 + xi0) * (1 - xi1);
          funct(2) = 0.25 * (1 + xi0) * (1 + xi1);
          funct(3) = 0.25 * (1 - xi0) * (1 + xi1);
          break;
        }
        // *********************************************************************
        // 2D standard quadratic shape functions (tri6)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::quad2D:
        {
          const double r = xi0;
          const double s = xi1;
          const double t1 = 1.0 - r - s;
          const double t2 = r;
          const double t3 = s;

          funct(0) = t1 * (2.0 * t1 - 1.0);
          funct(1) = t2 * (2.0 * t2 - 1.0);
          funct(2) = t3 * (2.0 * t3 - 1.0);
          funct(3) = 4.0 * t2 * t1;
          funct(4) = 4.0 * t2 * t3;
          funct(5) = 4.0 * t3 * t1;

          break;
        }
        // *********************************************************************
        // 2D modified quadratic shape functions (tri6)
        // (used in combination with quadr dual LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::quad2D_modified:
        {
          const double r = xi0;
          const double s = xi1;
          const double t1 = 1.0 - r - s;
          const double t2 = r;
          const double t3 = s;

          CORE::LINALG::Matrix<6, 1> valtmp;

          valtmp(0) = t1 * (2.0 * t1 - 1.0);
          valtmp(1) = t2 * (2.0 * t2 - 1.0);
          valtmp(2) = t3 * (2.0 * t3 - 1.0);
          valtmp(3) = 4.0 * t2 * t1;
          valtmp(4) = 4.0 * t2 * t3;
          valtmp(5) = 4.0 * t3 * t1;

          // define constant modification factor 1/5
          // (NOTE: lower factors, e.g. 1/12 would be sufficient here
          // as well, but in order to be globally continuous for mixed
          // meshes with tet10/hex20 elements, we always choose 1/5.)
          double fac = 1.0 / 5.0;

          // apply constant modification at vertex nodes and PoU
          funct(0) = valtmp(0) + (valtmp(3) + valtmp(5)) * fac;
          funct(1) = valtmp(1) + (valtmp(3) + valtmp(4)) * fac;
          funct(2) = valtmp(2) + (valtmp(4) + valtmp(5)) * fac;
          funct(3) = valtmp(3) * (1.0 - 2.0 * fac);
          funct(4) = valtmp(4) * (1.0 - 2.0 * fac);
          funct(5) = valtmp(5) * (1.0 - 2.0 * fac);

          break;
        }
        // *********************************************************************
        // 2D modified (hierarchical) quadratic shape functions (tri6)
        // (used in combination with linear dual LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::quad2D_hierarchical:
        {
          const double r = xi0;
          const double s = xi1;
          const double t1 = 1.0 - r - s;
          const double t2 = r;
          const double t3 = s;

          funct(0) = t1;
          funct(1) = t2;
          funct(2) = t3;
          funct(3) = 4.0 * t2 * t1;
          funct(4) = 4.0 * t2 * t3;
          funct(5) = 4.0 * t3 * t1;

          break;
        }
        // *********************************************************************
        // 2D linear part of standard quadratic shape functions (tri6)
        // (used for linear interpolation of std. LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::quad2D_only_lin:
        {
          funct(0) = 1 - xi0 - xi1;
          funct(1) = xi0;
          funct(2) = xi1;
          funct(3) = 0.0;
          funct(4) = 0.0;
          funct(5) = 0.0;

          break;
        }
        // *********************************************************************
        // 2D serendipity shape functions (quad8)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::serendipity2D:
        {
          const double r = xi0;
          const double s = xi1;
          const double rp = 1.0 + r;
          const double rm = 1.0 - r;
          const double sp = 1.0 + s;
          const double sm = 1.0 - s;
          const double r2 = 1.0 - r * r;
          const double s2 = 1.0 - s * s;

          // values for centernodes are straight forward
          //      0.5*(1-xi*xi)*(1-eta) (0 for xi=+/-1 and eta=+/-1/0
          //                             0 for xi=0    and eta= 1
          //                             1 for xi=0    and eta=-1    )
          // use shape functions on centernodes to zero out the corner node
          // shape functions on the centernodes
          // (0.5 is the value of the linear shape function in the centernode)
          //
          //  0.25*(1-xi)*(1-eta)-0.5*funct[neighbor1]-0.5*funct[neighbor2]

          funct(0) = 0.25 * (rm * sm - (r2 * sm + s2 * rm));
          funct(1) = 0.25 * (rp * sm - (r2 * sm + s2 * rp));
          funct(2) = 0.25 * (rp * sp - (s2 * rp + r2 * sp));
          funct(3) = 0.25 * (rm * sp - (r2 * sp + s2 * rm));
          funct(4) = 0.5 * r2 * sm;
          funct(5) = 0.5 * s2 * rp;
          funct(6) = 0.5 * r2 * sp;
          funct(7) = 0.5 * s2 * rm;

          break;
        }
        // *********************************************************************
        // 2D modified serendipity shape functions (quad8)
        // (used in combination with quadr dual LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::serendipity2D_modified:
        {
          const double r = xi0;
          const double s = xi1;
          const double rp = 1.0 + r;
          const double rm = 1.0 - r;
          const double sp = 1.0 + s;
          const double sm = 1.0 - s;
          const double r2 = 1.0 - r * r;
          const double s2 = 1.0 - s * s;

          // values for centernodes are straight forward
          //      0.5*(1-xi*xi)*(1-eta) (0 for xi=+/-1 and eta=+/-1/0
          //                             0 for xi=0    and eta= 1
          //                             1 for xi=0    and eta=-1    )
          // use shape functions on centernodes to zero out the corner node
          // shape functions on the centernodes
          // (0.5 is the value of the linear shape function in the centernode)
          //
          //  0.25*(1-xi)*(1-eta)-0.5*funct[neighbor1]-0.5*funct[neighbor2]

          CORE::LINALG::Matrix<8, 1> valtmp;

          valtmp(0) = 0.25 * (rm * sm - (r2 * sm + s2 * rm));
          valtmp(1) = 0.25 * (rp * sm - (r2 * sm + s2 * rp));
          valtmp(2) = 0.25 * (rp * sp - (s2 * rp + r2 * sp));
          valtmp(3) = 0.25 * (rm * sp - (r2 * sp + s2 * rm));
          valtmp(4) = 0.5 * r2 * sm;
          valtmp(5) = 0.5 * s2 * rp;
          valtmp(6) = 0.5 * r2 * sp;
          valtmp(7) = 0.5 * s2 * rm;

          // define constant modification factor 1/5
          const double fac = 1.0 / 5.0;

          // apply constant modification at vertex nodes and PoU
          funct(0) = valtmp(0) + (valtmp(4) + valtmp(7)) * fac;
          funct(1) = valtmp(1) + (valtmp(4) + valtmp(5)) * fac;
          funct(2) = valtmp(2) + (valtmp(5) + valtmp(6)) * fac;
          funct(3) = valtmp(3) + (valtmp(6) + valtmp(7)) * fac;
          funct(4) = valtmp(4) * (1.0 - 2.0 * fac);
          funct(5) = valtmp(5) * (1.0 - 2.0 * fac);
          funct(6) = valtmp(6) * (1.0 - 2.0 * fac);
          funct(7) = valtmp(7) * (1.0 - 2.0 * fac);

          break;
        }
        // *********************************************************************
        // 2D modified (hierarchical) serendipity shape functions (quad8)
        // (used in combination with linear dual LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::serendipity2D_hierarchical:
        {
          const double r = xi0;
          const double s = xi1;
          const double rp = 1.0 + r;
          const double rm = 1.0 - r;
          const double sp = 1.0 + s;
          const double sm = 1.0 - s;
          const double r2 = 1.0 - r * r;
          const double s2 = 1.0 - s * s;

          funct(0) = 0.25 * rm * sm;
          funct(1) = 0.25 * rp * sm;
          funct(2) = 0.25 * rp * sp;
          funct(3) = 0.25 * rm * sp;
          funct(4) = 0.5 * r2 * sm;
          funct(5) = 0.5 * s2 * rp;
          funct(6) = 0.5 * r2 * sp;
          funct(7) = 0.5 * s2 * rm;

          break;
        }
        // *********************************************************************
        // 2D bilinear part of serendipity quadratic shape functions (quad8)
        // (used for linear interpolation of std LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::serendipity2D_only_lin:
        {
          funct(0) = 0.25 * (1 - xi0) * (1 - xi1);
          funct(1) = 0.25 * (1 + xi0) * (1 - xi1);
          funct(2) = 0.25 * (1 + xi0) * (1 + xi1);
          funct(3) = 0.25 * (1 - xi0) * (1 + xi1);
          funct(4) = 0.0;
          funct(5) = 0.0;
          funct(6) = 0.0;
          funct(7) = 0.0;

          break;
        }
        // *********************************************************************
        // 2D standard biquadratic shape functions (quad9)
        // (used for interpolation of displacement field)
        // *********************************************************************
        case MORTAR::Element::biquad2D:
        {
          const double r = xi0;
          const double s = xi1;
          const double rp = 1.0 + r;
          const double rm = 1.0 - r;
          const double sp = 1.0 + s;
          const double sm = 1.0 - s;
          const double r2 = 1.0 - r * r;
          const double s2 = 1.0 - s * s;
          const double rh = 0.5 * r;
          const double sh = 0.5 * s;
          const double rs = rh * sh;

          funct(0) = rs * rm * sm;
          funct(1) = -rs * rp * sm;
          funct(2) = rs * rp * sp;
          funct(3) = -rs * rm * sp;
          funct(4) = -sh * sm * r2;
          funct(5) = rh * rp * s2;
          funct(6) = sh * sp * r2;
          funct(7) = -rh * rm * s2;
          funct(8) = r2 * s2;

          break;
        }
        // *********************************************************************
        // 2D standard biquadratic shape functions (quad9)
        // (used in combination with quadr dual LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::biquad2D_modified:
        {
          const double r = xi0;
          const double s = xi1;
          const double rp = 1.0 + r;
          const double rm = 1.0 - r;
          const double sp = 1.0 + s;
          const double sm = 1.0 - s;
          const double r2 = 1.0 - r * r;
          const double s2 = 1.0 - s * s;
          const double rh = 0.5 * r;
          const double sh = 0.5 * s;
          const double rs = rh * sh;

          CORE::LINALG::Matrix<9, 1> valtmp;

          valtmp(0) = rs * rm * sm;
          valtmp(1) = -rs * rp * sm;
          valtmp(2) = rs * rp * sp;
          valtmp(3) = -rs * rm * sp;
          valtmp(4) = -sh * sm * r2;
          valtmp(5) = rh * rp * s2;
          valtmp(6) = sh * sp * r2;
          valtmp(7) = -rh * rm * s2;
          valtmp(8) = r2 * s2;

          // define constant modification factor
          // (CURRENTLY NOT USED -> ZERO)
          const double fac = 0.0;

          // apply constant modification at vertex nodes and PoU
          funct(0) = valtmp(0) + (valtmp(4) + valtmp(7)) * fac + 0.5 * valtmp(8) * fac;
          funct(1) = valtmp(1) + (valtmp(4) + valtmp(5)) * fac + 0.5 * valtmp(8) * fac;
          funct(2) = valtmp(2) + (valtmp(5) + valtmp(6)) * fac + 0.5 * valtmp(8) * fac;
          funct(3) = valtmp(3) + (valtmp(6) + valtmp(7)) * fac + 0.5 * valtmp(8) * fac;
          funct(4) = valtmp(4) * (1.0 - 2.0 * fac);
          funct(5) = valtmp(5) * (1.0 - 2.0 * fac);
          funct(6) = valtmp(6) * (1.0 - 2.0 * fac);
          funct(7) = valtmp(7) * (1.0 - 2.0 * fac);
          funct(8) = valtmp(8) * (1.0 - 4.0 * 0.5 * fac);

          break;
        }
        // *********************************************************************
        // 2D standard biquadratic shape functions (quad9)
        // (used in combination with linear dual LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::biquad2D_hierarchical:
        {
          const double r = xi0;
          const double s = xi1;
          const double rp = 1.0 + r;
          const double rm = 1.0 - r;
          const double sp = 1.0 + s;
          const double sm = 1.0 - s;
          const double r2 = 1.0 - r * r;
          const double s2 = 1.0 - s * s;
          const double rh = 0.5 * r;
          const double sh = 0.5 * s;
          // const double rs=rh*sh;

          funct(0) = 0.25 * rm * sm;
          funct(1) = 0.25 * rp * sm;
          funct(2) = 0.25 * rp * sp;
          funct(3) = 0.25 * rm * sp;
          funct(4) = -sh * sm * r2;
          funct(5) = rh * rp * s2;
          funct(6) = sh * sp * r2;
          funct(7) = -rh * rm * s2;
          funct(8) = r2 * s2;

          break;
        }
        // *********************************************************************
        // 2D bilinear part of biquadratic quadratic shape functions (quad9)
        // (used for linear interpolation of std LM field in 3D quadratic mortar)
        // *********************************************************************
        case MORTAR::Element::biquad2D_only_lin:
        {
          funct(0) = 0.25 * (1 - xi0) * (1 - xi1);
          funct(1) = 0.25 * (1 + xi0) * (1 - xi1);
          funct(2) = 0.25 * (1 + xi0) * (1 + xi1);
          funct(3) = 0.25 * (1 - xi0) * (1 + xi1);
          funct(4) = 0.0;
          funct(5) = 0.0;
          funct(6) = 0.0;
          funct(7) = 0.0;
          funct(8) = 0.0;

          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown\n");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate dual shape 2D                                   farah 01/14|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_dualshape_function_2D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,                       ///< xi0 coordinate
        const double& xi1,                       ///< xi1 coordinate
        const MORTAR::Element::ShapeType& shape  ///< distinguish between mortar shape
    )
    {
      switch (shape)
      {
        // *********************************************************************
        // 2D dual linear shape functions (tri3)
        // (used for interpolation of Lagrange multiplier field)
        // *********************************************************************
        case MORTAR::Element::lindual2D:
        {
          if (ele.MoData().DualShape() == Teuchos::null)
          {
            funct(0) = 3.0 - 4.0 * xi0 - 4.0 * xi1;
            funct(1) = 4.0 * xi0 - 1.0;
            funct(2) = 4.0 * xi1 - 1.0;
          }
          else
          {
            const int nnodes = 3;
            // get solution matrix with dual parameters
            CORE::LINALG::SerialDenseMatrix ae(nnodes, nnodes);
            // get dual shape functions coefficient matrix from data container
            ae = *(ele.MoData().DualShape());

            // evaluate dual shape functions at loc. coord. xi
            // need standard shape functions at xi first
            CORE::LINALG::Matrix<nnodes, 1> stdval;
            double xi[2] = {xi0, xi1};
            EvaluateShape_Displ(xi, stdval, ele, false);

            // evaluate dual shape functions
            CORE::LINALG::Matrix<nnodes, 1> valtemp;
            valtemp.Clear();
            for (int i = 0; i < nnodes; ++i)
              for (int j = 0; j < nnodes; ++j) valtemp(i) += ae(i, j) * stdval(j);

            funct(0) = valtemp(0);
            funct(1) = valtemp(1);
            funct(2) = valtemp(2);
          }
          break;
        }
        // *********************************************************************
        // 2D dual bilinear shape functions (quad4)
        // (used for interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // *********************************************************************
        case MORTAR::Element::bilindual2D:
        {
          const int nnodes = 4;
          // get solution matrix with dual parameters
          CORE::LINALG::SerialDenseMatrix ae(nnodes, nnodes);
          CORE::LINALG::Matrix<nnodes, 1> stdval;

          // no pre-computed dual shape functions
          if (ele.MoData().DualShape() == Teuchos::null)
          {
            // establish fundamental data
            double detg = 0.0;

            // compute entries to bi-ortho matrices me/de with Gauss quadrature
            MORTAR::ElementIntegrator integrator(ele.Shape());

            CORE::LINALG::Matrix<nnodes, nnodes> me(true);
            CORE::LINALG::Matrix<nnodes, nnodes> de(true);

            for (int i = 0; i < integrator.nGP(); ++i)
            {
              double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
              EvaluateShape_Displ(gpc, stdval, ele, false);
              detg = ele.Jacobian(gpc);

              for (int j = 0; j < nnodes; ++j)
                for (int k = 0; k < nnodes; ++k)
                {
                  me(j, k) += integrator.Weight(i) * stdval(j) * stdval(k) * detg;
                  de(j, k) += (j == k) * integrator.Weight(i) * stdval(j) * detg;
                }
            }

            // invert bi-ortho matrix me
            CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);
          }

          // pre-computed dual shape functions
          else
          {
            // get dual shape functions coefficient matrix from data container
            ae = *(ele.MoData().DualShape());
          }

          // evaluate dual shape functions at loc. coord. xi
          // need standard shape functions at xi first
          double xi[2] = {xi0, xi1};
          EvaluateShape_Displ(xi, stdval, ele, false);

          // evaluate dual shape functions
          CORE::LINALG::Matrix<nnodes, 1> valtemp(true);
          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) valtemp(i) += ae(i, j) * stdval(j);

          funct(0) = valtemp(0);
          funct(1) = valtemp(1);
          funct(2) = valtemp(2);
          funct(3) = valtemp(3);

          break;
        }
        // *********************************************************************
        // 2D dual quadratic shape functions (tri6)
        // (used for interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // (including modification of displacement shape functions)
        // *********************************************************************
        case MORTAR::Element::quaddual2D:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 6;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::Matrix<nnodes, nnodes> de(true);
          CORE::LINALG::Matrix<nnodes, nnodes> ae;

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.EvaluateShape(gpc, valquad, derivquad, nnodes, true);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // get solution matrix with dual parameters
          CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);

          // evaluate dual shape functions at loc. coord. xi
          double xi[2] = {xi0, xi1};
          ele.EvaluateShape(xi, valquad, derivquad, nnodes, true);
          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        // *********************************************************************
        // 2D dual serendipity shape functions (quad8)
        // (used for interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // (including modification of displacement shape functions)
        // *********************************************************************
        case MORTAR::Element::serendipitydual2D:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 8;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::Matrix<nnodes, nnodes> de(true);
          CORE::LINALG::Matrix<nnodes, nnodes> ae;

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.EvaluateShape(gpc, valquad, derivquad, nnodes, true);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // get solution matrix with dual parameters
          CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);

          // evaluate dual shape functions at loc. coord. xi
          double xi[2] = {xi0, xi1};
          ele.EvaluateShape(xi, valquad, derivquad, nnodes, true);
          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        // *********************************************************************
        // 2D dual biquadratic shape functions (quad9)
        // (used for interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // (including modification of displacement shape functions)
        // *********************************************************************
        case MORTAR::Element::biquaddual2D:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 9;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::Matrix<nnodes, nnodes> de(true);
          CORE::LINALG::Matrix<nnodes, nnodes> ae;

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.EvaluateShape(gpc, valquad, derivquad, nnodes, true);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // get solution matrix with dual parameters
          CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);

          // evaluate dual shape functions at loc. coord. xi
          double xi[2] = {xi0, xi1};
          ele.EvaluateShape(xi, valquad, derivquad, nnodes, true);
          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        // *********************************************************************
        // 2D dual quadratic shape functions (tri6)
        // (used for LINEAR interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // (including modification of displacement shape functions)
        // *********************************************************************
        case MORTAR::Element::quaddual2D_only_lin:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 6;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::SerialDenseMatrix de(nnodes, nnodes, true);

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.ShapeFunctions(MORTAR::Element::quad2D_only_lin, gpc, valquad, derivquad);

            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // how many non-zero nodes
          const int nnodeslin = 3;

          // reduce me to non-zero nodes before inverting
          CORE::LINALG::Matrix<nnodeslin, nnodeslin> melin;
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) melin(j, k) = me(j, k);

          // invert bi-ortho matrix melin
          CORE::LINALG::Inverse(melin);

          // re-inflate inverse of melin to full size
          CORE::LINALG::SerialDenseMatrix invme(nnodes, nnodes, true);
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) invme(j, k) = melin(j, k);

          // get solution matrix with dual parameters
          CORE::LINALG::SerialDenseMatrix ae(nnodes, nnodes);
          CORE::LINALG::multiply(ae, de, invme);

          // evaluate dual shape functions at loc. coord. xi
          double xi[2] = {xi0, xi1};

          ele.ShapeFunctions(MORTAR::Element::quad2D_only_lin, xi, valquad, derivquad);

          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        // *********************************************************************
        // 2D dual serendipity shape functions (quad8)
        // (used for LINEAR interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // (including modification of displacement shape functions)
        // *********************************************************************
        case MORTAR::Element::serendipitydual2D_only_lin:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 8;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::SerialDenseMatrix de(nnodes, nnodes, true);

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.ShapeFunctions(MORTAR::Element::serendipity2D_only_lin, gpc, valquad, derivquad);

            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // how many non-zero nodes
          const int nnodeslin = 4;

          // reduce me to non-zero nodes before inverting
          CORE::LINALG::Matrix<nnodeslin, nnodeslin> melin;
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) melin(j, k) = me(j, k);

          // invert bi-ortho matrix melin
          CORE::LINALG::Inverse(melin);

          // re-inflate inverse of melin to full size
          CORE::LINALG::SerialDenseMatrix invme(nnodes, nnodes, true);
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) invme(j, k) = melin(j, k);

          // get solution matrix with dual parameters
          CORE::LINALG::SerialDenseMatrix ae(nnodes, nnodes);
          CORE::LINALG::multiply(ae, de, invme);

          // evaluate dual shape functions at loc. coord. xi
          double xi[2] = {xi0, xi1};
          ele.ShapeFunctions(MORTAR::Element::serendipity2D_only_lin, xi, valquad, derivquad);

          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        // *********************************************************************
        // 2D dual biquadratic shape functions (quad9)
        // (used for LINEAR interpolation of Lagrange multiplier field)
        // (including adaption process for distorted elements)
        // (including modification of displacement shape functions)
        // *********************************************************************
        case MORTAR::Element::biquaddual2D_only_lin:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 9;

          // empty shape function vals + derivs
          CORE::LINALG::SerialDenseVector valquad(nnodes);
          CORE::LINALG::SerialDenseMatrix derivquad(nnodes, 2);

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(ele.Shape());
          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::SerialDenseMatrix de(nnodes, nnodes, true);

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            ele.ShapeFunctions(MORTAR::Element::biquad2D_only_lin, gpc, valquad, derivquad);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * valquad[j] * valquad[k] * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * valquad[j] * detg;
              }
            }
          }

          // how many non-zero nodes
          const int nnodeslin = 4;

          // reduce me to non-zero nodes before inverting
          CORE::LINALG::Matrix<nnodeslin, nnodeslin> melin;
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) melin(j, k) = me(j, k);

          // invert bi-ortho matrix melin
          CORE::LINALG::Inverse(melin);
          // re-inflate inverse of melin to full size
          CORE::LINALG::SerialDenseMatrix invme(nnodes, nnodes, true);
          for (int j = 0; j < nnodeslin; ++j)
            for (int k = 0; k < nnodeslin; ++k) invme(j, k) = melin(j, k);

          // get solution matrix with dual parameters
          CORE::LINALG::SerialDenseMatrix ae(nnodes, nnodes);
          CORE::LINALG::multiply(ae, de, invme);

          // evaluate dual shape functions at loc. coord. xi
          double xi[2] = {xi0, xi1};
          ele.ShapeFunctions(MORTAR::Element::biquad2D_only_lin, xi, valquad, derivquad);

          for (int i = 0; i < nnodes; ++i) funct(i) = 0;

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) funct(i) += ae(i, j) * valquad[j];

          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown\n");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate std. nurbs shape 1D                             farah 05/14|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_nurbs_shape_function_1D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    )
    {
      switch (shape)
      {
        //*********************************************
        // 1D -- nurbs2
        //*********************************************
        case CORE::FE::CellType::nurbs2:
        {
          CORE::LINALG::SerialDenseVector weights(ele.NumNode());
          for (int inode = 0; inode < ele.NumNode(); ++inode)
            weights(inode) = dynamic_cast<MORTAR::Node*>(ele.Nodes()[inode])->NurbsW();

          CORE::LINALG::SerialDenseMatrix auxderiv(1, ele.NumNode());
          CORE::FE::NURBS::nurbs_get_1D_funct_deriv(
              funct, auxderiv, xi0, ele.Knots()[0], weights, CORE::FE::CellType::nurbs2);
          break;
        }
        //*********************************************
        // 1D -- nurbs3
        //*********************************************
        case CORE::FE::CellType::nurbs3:
        {
          CORE::LINALG::SerialDenseVector weights(ele.NumNode());
          for (int inode = 0; inode < ele.NumNode(); ++inode)
            weights(inode) = dynamic_cast<MORTAR::Node*>(ele.Nodes()[inode])->NurbsW();

          CORE::LINALG::SerialDenseMatrix auxderiv(1, ele.NumNode());
          CORE::FE::NURBS::nurbs_get_1D_funct_deriv(
              funct, auxderiv, xi0, ele.Knots()[0], weights, CORE::FE::CellType::nurbs3);
          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown\n");
          break;
        }
      }

      return;
    }


    /*----------------------------------------------------------------------*
     |  evaluate std. nurbs shape 2D                             seitz 02/15|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_nurbs_shape_function_2D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const double& xi1,               ///< xi0 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    )
    {
      switch (shape)
      {
        //*********************************************
        // 1D -- nurbs2
        //*********************************************
        case CORE::FE::CellType::nurbs4:
        {
          FOUR_C_THROW("stop");
          break;
        }
        //*********************************************
        // 1D -- nurbs3
        //*********************************************
        case CORE::FE::CellType::nurbs9:
        {
          CORE::LINALG::SerialDenseVector weights(ele.NumNode());
          for (int inode = 0; inode < ele.NumNode(); ++inode)
            weights(inode) = dynamic_cast<MORTAR::Node*>(ele.Nodes()[inode])->NurbsW();

          CORE::LINALG::SerialDenseVector uv(2);
          uv(0) = xi0;
          uv(1) = xi1;

          CORE::LINALG::SerialDenseMatrix auxderiv(2, ele.NumNode());
          CORE::FE::NURBS::nurbs_get_2D_funct_deriv(
              funct, auxderiv, uv, ele.Knots(), weights, CORE::FE::CellType::nurbs9);
          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown: %d\n", shape);
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate dual nurbs shape 1D                             farah 05/14|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_nurbs_dualshape_function_1D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    )
    {
      switch (shape)
      {
        case CORE::FE::CellType::nurbs3:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 3;

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(shape);

          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::Matrix<nnodes, nnodes> de(true);
          CORE::LINALG::Matrix<nnodes, nnodes> ae;

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            mortar_nurbs_shape_function_1D(funct, ele, gpc[0], CORE::FE::CellType::nurbs3);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * funct(j) * funct(k) * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * funct(j) * detg;
              }
            }
          }

          // get solution matrix with dual parameters
          CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);

          // evaluate dual shape functions at loc. coord. xi
          // need standard shape functions at xi first
          mortar_nurbs_shape_function_1D(funct, ele, xi0, CORE::FE::CellType::nurbs3);

          // evaluate dual shape functions
          CORE::LINALG::Matrix<nnodes, 1> valtemp(true);

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) valtemp(i) += ae(i, j) * funct(j);

          funct(0) = valtemp(0);
          funct(1) = valtemp(1);
          funct(2) = valtemp(2);

          break;
        }
        default:
        {
          FOUR_C_THROW("shape unknown\n");
          break;
        }
      }

      return;
    }

    /*----------------------------------------------------------------------*
     |  evaluate dual nurbs shape 2D                             seitz 02/15|
     *----------------------------------------------------------------------*/
    template <class V>
    void mortar_nurbs_dualshape_function_2D(V& funct,  ///< to be filled with shape function values
        MORTAR::Element& ele,
        const double& xi0,               ///< xi0 coordinate
        const double& xi1,               ///< xi1 coordinate
        const CORE::FE::CellType& shape  ///< distinguish between shape
    )
    {
      switch (shape)
      {
        case CORE::FE::CellType::nurbs9:
        {
          // establish fundamental data
          double detg = 0.0;
          const int nnodes = 9;

          // compute entries to bi-ortho matrices me/de with Gauss quadrature
          MORTAR::ElementIntegrator integrator(shape);

          CORE::LINALG::Matrix<nnodes, nnodes> me(true);
          CORE::LINALG::Matrix<nnodes, nnodes> de(true);
          CORE::LINALG::Matrix<nnodes, nnodes> ae;

          for (int i = 0; i < integrator.nGP(); ++i)
          {
            double gpc[2] = {integrator.Coordinate(i, 0), integrator.Coordinate(i, 1)};
            mortar_nurbs_shape_function_2D(funct, ele, gpc[0], gpc[1], shape);
            detg = ele.Jacobian(gpc);

            for (int j = 0; j < nnodes; ++j)
            {
              for (int k = 0; k < nnodes; ++k)
              {
                me(j, k) += integrator.Weight(i) * funct(j) * funct(k) * detg;
                de(j, k) += (j == k) * integrator.Weight(i) * funct(j) * detg;
              }
            }
          }

          // get solution matrix with dual parameters
          CORE::LINALG::InvertAndMultiplyByCholesky<nnodes>(me, de, ae);

          // evaluate dual shape functions
          CORE::LINALG::Matrix<nnodes, 1> valtemp(true);

          // evaluate dual shape functions at loc. coord. xi
          // need standard shape functions at xi first
          mortar_nurbs_shape_function_2D(funct, ele, xi0, xi1, CORE::FE::CellType::nurbs9);

          for (int i = 0; i < nnodes; ++i)
            for (int j = 0; j < nnodes; ++j) valtemp(i) += ae(i, j) * funct(j);

          for (int i = 0; i < nnodes; ++i) funct(i) = valtemp(i);

          break;
        }
        default:
          FOUR_C_THROW("unknown shape");
          break;
      }
    }

  }  // namespace UTILS
}  // namespace MORTAR

FOUR_C_NAMESPACE_CLOSE

#endif
