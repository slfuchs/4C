/*---------------------------------------------------------------------*/
/*! \file

\brief Sidehandle represents a side original loaded into the cut, internal it can be split into
subsides

\level 3


*----------------------------------------------------------------------*/

#ifndef FOUR_C_CUT_SIDEHANDLE_HPP
#define FOUR_C_CUT_SIDEHANDLE_HPP

#include "4C_config.hpp"

#include "4C_cut_boundarycell.hpp"
#include "4C_cut_side.hpp"

FOUR_C_NAMESPACE_OPEN

namespace CORE::GEO
{
  namespace CUT
  {
    class Node;
    class Side;
    class Mesh;

    /*!
    \brief Outside world interface to the side. This breaks the quadratic side into linear sides
     */
    class SideHandle
    {
     public:
      virtual ~SideHandle() = default;
      /*!
      \brief Get the shape of this sides
       */
      virtual CORE::FE::CellType Shape() = 0;

      /*!
      \brief Get the coordinates of the nodes of this side
       */
      virtual void Coordinates(CORE::LINALG::SerialDenseMatrix& xyze) = 0;

      /*!
      \brief Get the local coordinates "rst"from the global coordinates "xyz" with respect to this
      side. Since side is 2D, the local coordinates will have only two coordinates
      */
      virtual void LocalCoordinates(
          const CORE::LINALG::Matrix<3, 1>& xyz, CORE::LINALG::Matrix<2, 1>& rst) = 0;

      /*!
      \brief For a quadratic side, get the resulting linear sides
       */
      virtual void CollectSides(plain_side_set& sides) = 0;


      /*!
      \brief Gets all facets of a side
       */
      virtual void Facets(std::vector<Facet*>& facets) = 0;

      /*!
      \brief Get the Gaussian rule projected on the side. Not used now
       */
      template <CORE::FE::CellType distype>
      Teuchos::RCP<CORE::FE::GaussPoints> CreateProjected(BoundaryCell* bc)
      {
        const unsigned nen = CORE::FE::num_nodes<distype>;

        CORE::LINALG::Matrix<2, nen> xie;

        const std::vector<CORE::GEO::CUT::Point*>& cpoints = bc->Points();
        if (cpoints.size() != nen) FOUR_C_THROW("non-matching number of points");

        for (unsigned i = 0; i < nen; ++i)
        {
          CORE::GEO::CUT::Point* p = cpoints[i];
          const CORE::LINALG::Matrix<2, 1>& xi = LocalCoordinates(p);
          std::copy(xi.A(), xi.A() + 2, &xie(0, i));
        }

        Teuchos::RCP<CORE::FE::GaussPoints> gp =
            CORE::FE::GaussIntegration::CreateProjected<distype>(xie, bc->CubatureDegree());
        return gp;
      }

      /*!
      \brief Get the local coordinates of point p with respect to this side. Since side is 2D, the
      local coordinates will have only two coordinates
       */
      const CORE::LINALG::Matrix<2, 1>& LocalCoordinates(Point* p)
      {
        std::map<Point*, CORE::LINALG::Matrix<2, 1>>::iterator i = local_coordinates_.find(p);
        if (i != local_coordinates_.end())
        {
          return i->second;
        }
        CORE::LINALG::Matrix<2, 1>& rst = local_coordinates_[p];
        CORE::LINALG::Matrix<3, 1> xyz;
        p->Coordinates(xyz.A());
        LocalCoordinates(xyz, rst);
        return rst;
      }

      /// Remove the SubSidePointer of given side from this Sidehandle
      virtual void RemoveSubSidePointer(const Side* side)
      {
        FOUR_C_THROW("RemoveSubSidePointer: Not available in base class!");
      }

      /// Add the SubSidePointer of given side to this Sidehandle
      virtual void AddSubSidePointer(Side* side)
      {
        FOUR_C_THROW("AddSubSidePointer: Not available in base class!");
      }

      /// Add the SubSide in to the unphysical list
      virtual void MarkSubSideunphysical(Side* side)
      {
        FOUR_C_THROW("SetSubSidePointertounphysical: Not available in base class!");
      }

      /// Is this side and unphysical subside
      virtual bool IsunphysicalSubSide(Side* side)
      {
        FOUR_C_THROW("IsunphysicalSubSide: Not available in base class!");
        return false;  // dummy
      }

      /// Does this sidehandle have unphysical subsides
      virtual bool HasunphysicalSubSide()
      {
        FOUR_C_THROW("HasunphysicalSubSide: Not available in base class!");
        return false;  // dummy
      }

      virtual const std::vector<Node*>& GetNodes() const
      {
        FOUR_C_THROW("GetNodes: Not available in base class!");
        static const std::vector<Node*> dummy;
        return dummy;  // dummy
      }

     private:
      std::map<Point*, CORE::LINALG::Matrix<2, 1>> local_coordinates_;
    };

    /// linear side handle
    class LinearSideHandle : public SideHandle
    {
     public:
      LinearSideHandle() : side_(nullptr) {}

      explicit LinearSideHandle(Side* s) : side_(s) {}

      CORE::FE::CellType Shape() override { return side_->Shape(); }

      void Coordinates(CORE::LINALG::SerialDenseMatrix& xyze) override
      {
        xyze.reshape(3, side_->Nodes().size());
        side_->Coordinates(xyze.values());
      }

      void LocalCoordinates(
          const CORE::LINALG::Matrix<3, 1>& xyz, CORE::LINALG::Matrix<2, 1>& rs) override
      {
        CORE::LINALG::Matrix<3, 1> rst;
        side_->LocalCoordinates(xyz, rst);
        rs(0) = rst(0);
        rs(1) = rst(1);
      }

      void CollectSides(plain_side_set& sides) override { sides.insert(side_); }

      /// gets all facets of a linear side
      void Facets(std::vector<Facet*>& facets) override
      {
        std::vector<Facet*> sidefacets = side_->Facets();
        for (std::vector<Facet*>::iterator i = sidefacets.begin(); i != sidefacets.end(); ++i)
        {
          Facet* sidefacet = *i;
          facets.push_back(sidefacet);
        }
      }

      /// Get the nodes of the Sidehandle
      const std::vector<Node*>& GetNodes() const override { return side_->Nodes(); }


     private:
      Side* side_;
    };

    /// quadratic side handle
    class QuadraticSideHandle : public SideHandle
    {
     public:
      void Coordinates(CORE::LINALG::SerialDenseMatrix& xyze) override
      {
        xyze.reshape(3, nodes_.size());
        for (std::vector<Node*>::iterator i = nodes_.begin(); i != nodes_.end(); ++i)
        {
          Node* n = *i;
          n->Coordinates(&xyze(0, i - nodes_.begin()));
        }
      }

      void CollectSides(plain_side_set& sides) override
      {
        std::copy(subsides_.begin(), subsides_.end(), std::inserter(sides, sides.begin()));
      }

      /// gets all facets of a quadratic side
      void Facets(std::vector<Facet*>& facets) override
      {
        for (std::vector<Side*>::iterator i = subsides_.begin(); i != subsides_.end(); ++i)
        {
          Side* subside = *i;
          std::vector<Facet*> sidefacets = subside->Facets();
          for (std::vector<Facet*>::iterator i = sidefacets.begin(); i != sidefacets.end(); ++i)
          {
            Facet* sidefacet = *i;
            facets.push_back(sidefacet);
          }
        }
      }

      /// Remove the SubSidePointer of given side from this Sidehandle
      void RemoveSubSidePointer(const Side* side) override
      {
        std::vector<Side*>::iterator tmpssit = subsides_.end();
        for (std::vector<Side*>::iterator ssit = subsides_.begin(); ssit != subsides_.end(); ++ssit)
        {
          if ((*ssit) == side)
          {
            tmpssit = ssit;
            break;
          }
        }
        if (tmpssit != subsides_.end())
          subsides_.erase(tmpssit);
        else
          FOUR_C_THROW("RemoveSubSidePointer: Couldn't identify subside!");
      }

      /// Add the SubSidePointer of given side to this Sidehandle
      void AddSubSidePointer(Side* side) override
      {
        std::vector<Side*>::iterator tmpssit = subsides_.end();
        for (std::vector<Side*>::iterator ssit = subsides_.begin(); ssit != subsides_.end(); ++ssit)
        {
          if ((*ssit) == side)
          {
            tmpssit = ssit;
            break;
          }
        }
        if (tmpssit == subsides_.end())
          subsides_.push_back(side);
        else
          return;
      }

      /// Add the SubSide in to the unphysical list
      void MarkSubSideunphysical(Side* side) override
      {
        std::vector<Side*>::iterator tmpssit = subsides_.end();
        for (std::vector<Side*>::iterator ssit = subsides_.begin(); ssit != subsides_.end(); ++ssit)
        {
          if ((*ssit) == side)
          {
            tmpssit = ssit;
            break;
          }
        }
        if (tmpssit == subsides_.end())
          FOUR_C_THROW(
              "MarkSubSideunphysical failed, your side is not a Subside of the "
              "QuadraticSideHandle!");
        else
        {
          unphysical_subsides_.push_back(side);
        }
        return;
      }

      /// Is this side and unphysical subside
      bool IsunphysicalSubSide(Side* side) override
      {
        for (std::vector<Side*>::iterator ssit = unphysical_subsides_.begin();
             ssit != unphysical_subsides_.end(); ++ssit)
        {
          if ((*ssit) == side) return true;
        }
        return false;
      }

      /// Does this sidehandle have unphysical subsides
      bool HasunphysicalSubSide() override { return unphysical_subsides_.size(); }

      /// Get the nodes of the Sidehandle
      const std::vector<Node*>& GetNodes() const override { return nodes_; }

     protected:
      std::vector<Side*> subsides_;
      std::vector<Node*> nodes_;

      /// all unphysical subsides of the side handle
      std::vector<Side*> unphysical_subsides_;
    };

    /// tri6 side handle
    class Tri6SideHandle : public QuadraticSideHandle
    {
     public:
      Tri6SideHandle(Mesh& mesh, int sid, const std::vector<int>& node_ids);

      CORE::FE::CellType Shape() override { return CORE::FE::CellType::tri6; }

      void LocalCoordinates(
          const CORE::LINALG::Matrix<3, 1>& xyz, CORE::LINALG::Matrix<2, 1>& rst) override;
    };

    /// quad4 side handle
    /*!
     * quad4 need to be split in 3 tri3 in order to avoid subtle ambiguities.
     */
    class Quad4SideHandle : public QuadraticSideHandle
    {
     public:
      Quad4SideHandle(Mesh& mesh, int sid, const std::vector<int>& node_ids);

      CORE::FE::CellType Shape() override { return CORE::FE::CellType::quad4; }

      void LocalCoordinates(
          const CORE::LINALG::Matrix<3, 1>& xyz, CORE::LINALG::Matrix<2, 1>& rst) override;
    };

    /// quad8 side handle
    class Quad8SideHandle : public QuadraticSideHandle
    {
     public:
      Quad8SideHandle(Mesh& mesh, int sid, const std::vector<int>& node_ids, bool iscutside = true);

      CORE::FE::CellType Shape() override { return CORE::FE::CellType::quad8; }

      void LocalCoordinates(
          const CORE::LINALG::Matrix<3, 1>& xyz, CORE::LINALG::Matrix<2, 1>& rst) override;
    };

    /// quad9 side handle
    class Quad9SideHandle : public QuadraticSideHandle
    {
     public:
      Quad9SideHandle(Mesh& mesh, int sid, const std::vector<int>& node_ids, bool iscutside = true);

      CORE::FE::CellType Shape() override { return CORE::FE::CellType::quad9; }

      void LocalCoordinates(
          const CORE::LINALG::Matrix<3, 1>& xyz, CORE::LINALG::Matrix<2, 1>& rst) override;
    };

  }  // namespace CUT
}  // namespace CORE::GEO

FOUR_C_NAMESPACE_CLOSE

#endif
