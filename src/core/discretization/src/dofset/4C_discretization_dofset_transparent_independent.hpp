/*----------------------------------------------------------------------*/
/*! \file

\brief A set of degrees of freedom on two discretizations

\level 2


*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_DISCRETIZATION_DOFSET_TRANSPARENT_INDEPENDENT_HPP
#define FOUR_C_DISCRETIZATION_DOFSET_TRANSPARENT_INDEPENDENT_HPP


#include "4C_config.hpp"

#include "4C_discretization_dofset_independent.hpp"
#include "4C_discretization_dofset_transparent.hpp"

FOUR_C_NAMESPACE_OPEN

namespace DRT
{
  class Discretization;
}

namespace CORE::GEO
{
  class CutWizard;
}

namespace CORE::Dofsets
{
  /// Alias dofset that shares dof numbers with another dofset
  /*!
  A special set of degrees of freedom, implemented in order to assign the same degrees of freedom to
  nodes belonging to two discretizations. This way two discretizations can assemble into the same
  position of the system matrix. As internal variable it holds a source discretization
  (Constructor). If such a nodeset is assigned to a sub-discretization, its dofs are assigned
  according to the dofs of the source.

  */
  class TransparentIndependentDofSet : public IndependentDofSet, public TransparentDofSet
  {
   public:
    /*!
    \brief Standard Constructor
    */
    explicit TransparentIndependentDofSet(
        Teuchos::RCP<DRT::Discretization> sourcedis, bool parallel);



    /// create a copy of this object
    Teuchos::RCP<DofSet> Clone() override { return Teuchos::rcp(new IndependentDofSet(*this)); }

    /// Assign dof numbers to all elements and nodes of the discretization.
    int assign_degrees_of_freedom(
        const DRT::Discretization& dis, const unsigned dspos, const int start) override;

   protected:
    int NumDofPerNode(const CORE::Nodes::Node& node) const override;


  };  // class TransparentDofSet
}  // namespace CORE::Dofsets

FOUR_C_NAMESPACE_CLOSE

#endif
