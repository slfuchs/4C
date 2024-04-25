/*---------------------------------------------------------------------*/
/*! \file

\brief Class to triangulate facets

\level 3


*----------------------------------------------------------------------*/

#ifndef FOUR_C_CUT_TRIANGULATEFACET_HPP
#define FOUR_C_CUT_TRIANGULATEFACET_HPP

#include "4C_config.hpp"

#include "4C_cut_side.hpp"

#include <list>
#include <vector>

FOUR_C_NAMESPACE_OPEN

namespace CORE::GEO
{
  namespace CUT
  {
    class Point;
    class Facet;

    /*!
    \brief A class to split a facet into tri and quad cells
     */
    class TriangulateFacet
    {
     public:
      /*!
      \brief Constructor for a facet without holes
       */
      TriangulateFacet(std::vector<Point *> ptlist) : ptlist_(ptlist) {}

      /*!
      \brief Constructor for a facet with holes
       */
      TriangulateFacet(std::vector<Point *> ptlist, std::vector<std::vector<Point *>> inlists)
          : ptlist_(ptlist)
      {
        if (Hasequal_ptlist_inlist(ptlist, inlists)) return;
        for (std::vector<std::vector<Point *>>::iterator i = inlists.begin(); i != inlists.end();
             ++i)
        {
          std::vector<Point *> inlist = *i;
          inlists_.push_back(inlist);
        }
      }

      /*!
      \brief Split the facet into appropriate number of tri and quad
       */
      void SplitFacet();

      /*!
      \brief A general facet is triangulated with ear clipping method.
      When triOnly=true calls conventional Earclipping method. Otherwise it creates both Tri and
      Quad cells to reduce the number of Gaussian points
       */
      void EarClipping(std::vector<int> ptConcavity,
          bool triOnly = false,           // create triangles only?
          bool DeleteInlinePts = false);  // how to deal with collinear points?

      /*!
      \brief Ear Clipping is a triangulation method for simple polygons (convex, concave, with
      holes). It is simple and robust but not very efficient (O(n^2)). As input parameter the outer
      polygon (ptlist_) and the inner polygons (inlists_) are required. Triangles will be generated
      as output, which are all combined in one vector (split_).
      */
      void EarClippingWithHoles(Side *parentside);

      /*!
      \brief Returns Tri and Quad cells that are created by facet splitting
       */
      std::vector<std::vector<Point *>> GetSplitCells() { return split_; }

     private:
      /*!
      \brief The cyles ptlist and inlists are equal
      */
      bool Hasequal_ptlist_inlist(
          std::vector<Point *> ptlist, std::vector<std::vector<Point *>> inlists);

      /*!
      \brief Split a concave 4 noded facet into a 2 tri
      */
      void Split4nodeFacet(std::vector<Point *> &poly, bool callFromSplitAnyFacet = false);

      /*!
      \brief Split a convex facet or a facet with only one concave point into 1 Tri and few Quad
      cells
       */
      void SplitConvex_1ptConcave_Facet(std::vector<int> ptConcavity);

      /*!
      \brief A concave facet which has more than 2 concavity points are split into appropriate cells
      */
      void SplitGeneralFacet(std::vector<int> ptConcavity);

      /*!
      \brief check whether any two adjacent polygonal points are concave
       */
      bool HasTwoContinuousConcavePts(std::vector<int> ptConcavity);

      //! Restores last ear that was deleted during triangulation
      void RestoreLastEar(int ear_head_index, std::vector<int> &ptConcavity);

      //! Goes clockwise from the the only no on-line point on the triangle and generates thin
      //! triangles
      void SplitTriangleWithPointsOnLine(unsigned int start_id);

      //! Find second best ear, from the ones we discarded during the first check on the first round
      //! of earclipping
      unsigned int FindSecondBestEar(
          std::vector<std::pair<std::vector<Point *>, unsigned int>> &ears,
          const std::vector<int> &reflex);

      //! Corner points of the facet
      std::vector<Point *> ptlist_;

      //! Points describing holes in this facet
      std::list<std::vector<Point *>> inlists_;

      //! Holds the split Tri and Quad cells
      std::vector<std::vector<Point *>> split_;
    };
  }  // namespace CUT
}  // namespace CORE::GEO

FOUR_C_NAMESPACE_CLOSE

#endif
