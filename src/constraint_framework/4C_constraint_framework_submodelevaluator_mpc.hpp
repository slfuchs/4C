/*-----------------------------------------------------------*/
/*! \file

\brief  Manage linear multipoint constraint equations including
        periodic displacement boundary conditions.

\level 3
 */
/*-----------------------------------------------------------*/

#ifndef FOUR_C_CONSTRAINT_FRAMEWORK_SUBMODELEVALUATOR_MPC_HPP
#define FOUR_C_CONSTRAINT_FRAMEWORK_SUBMODELEVALUATOR_MPC_HPP

#include "4C_config.hpp"

#include "4C_constraint_framework_submodelevaluator_base.hpp"
#include "4C_inpar_mpc_rve.hpp"
#include "4C_io_pstream.hpp"
#include "4C_lib_discret.hpp"
#include "4C_structure_new_model_evaluator_generic.hpp"

#include <boost/algorithm/string.hpp>
#include <Epetra_CrsMatrix.h>
#include <Teuchos_RCPDecl.hpp>

#include <map>

FOUR_C_NAMESPACE_OPEN


namespace CONSTRAINTS::SUBMODELEVALUATOR
{
  class MultiPointConstraintEquationBase;


  class RveMultiPointConstraintManager : public ConstraintBase
  {
   public:
    /*!
    \brief Standard Constructor
    */
    RveMultiPointConstraintManager(
        Teuchos::RCP<const DRT::Discretization> disc_ptr, CORE::LINALG::SparseMatrix* st_ptr);

    //! @name Public evaluation methods

    /*!
      \brief Perform basic checks of the input conditions and parameters
    */
    void CheckInput();

    /*!
     * \brief Reset the constraint stiffness matrix and delete node pairs
     */
    void Reset() override;

    //@}

   private:
    //! @name member variables

    //! Map of the Corner Node IDs Ni
    std::map<std::string, DRT::Node*> rve_ref_node_map_;

    //! RVE reference length vectors
    std::array<double, 2> r_xmxp_, r_ymyp_;

    //! Vector with all Conditions
    std::vector<Teuchos::RCP<DRT::Condition>> point_linear_coupled_equation_conditions_,
        point_periodic_rve_ref_conditions_, line_periodic_rve_conditions_,
        surface_periodic_rve_conditions_;

    //! Tolerance for the opposing edge node search
    double node_search_toler_ = 0.25;  // #ToDo: Add .dat parameter

    //! Parameter List for the rveType
    Teuchos::ParameterList mpc_parameter_list_;

    //! Dimension of the rve boundary
    enum INPAR::RVE_MPC::RveDimension rve_dim_;

    //! Type of reference vector definition
    enum INPAR::RVE_MPC::RveReferenceDeformationDefinition rve_ref_type_;

    //@}

   private:
    //! @name Private evaluation methods

    //! find the opposite edge node pairs of the periodic rve
    void BuildPeriodicMPCs(std::map<std::string, const std::vector<int>*>& rveBoundaryNodeIdMap_,
        std::map<std::string, int>& rveCornerNodeIdMap_);

    //! add linear mpcs to the mpcList
    int BuildLinearMPCs();

    //! find a node that is member of edge1 and edge2
    int FindPeriodicRveCornerNodes(const std::vector<int>* edge1, const std::vector<int>* edge2);

    //! find a node that is member of surf1 thru surf3
    int FindPeriodicRveCornerNodes(const std::vector<int>* surf1, const std::vector<int>* surf2,
        const std::vector<int>* surf3);

    //! find the nodes containted i a mpc for the pbcs
    int FindOppositeEdgeNode(const int nodeID, INPAR::RVE_MPC::RveEdgeIdentifiers edge,
        std::map<std::string, const std::vector<int>*>& rveBoundaryNodeIdMap_);

    //! find the corner nodes of the periodic rve
    void BuildPeriodicRveCornerNodeMap(
        std::map<std::string, const std::vector<int>*>& rveBoundaryNodeIdMap,
        std::map<std::string, int>& rveCornerNodeIdMap_);

    //! retrive design line periodic rve boundary condition
    void BuildPeriodicRveBoundaryNodeMap(
        std::map<std::string, const std::vector<int>*>& rveBoundaryNodeIdMap_);
    //@}
  };
}  // namespace CONSTRAINTS::SUBMODELEVALUATOR
FOUR_C_NAMESPACE_CLOSE
#endif
