/*----------------------------------------------------------------------*/
/*! \file

\brief A class providing coupling capabilities based on non-linear
       mortar methods

\level 1


*----------------------------------------------------------------------*/

#ifndef FOUR_C_ADAPTER_COUPLING_NONLIN_MORTAR_HPP
#define FOUR_C_ADAPTER_COUPLING_NONLIN_MORTAR_HPP

/*---------------------------------------------------------------------*
 | headers                                                 farah 10/14 |
 *---------------------------------------------------------------------*/
#include "4C_config.hpp"

#include "4C_coupling_adapter_mortar.hpp"
#include "4C_discretization_condition.hpp"
#include "4C_utils_exceptions.hpp"

#include <Epetra_Comm.h>
#include <Epetra_Map.h>
#include <Epetra_Vector.h>
#include <Teuchos_ParameterListAcceptorDefaultBase.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------*
 | forward declarations                                    farah 10/14 |
 *---------------------------------------------------------------------*/
namespace DRT
{
  class Condition;
  class Discretization;
  class Node;
  class Element;
}  // namespace DRT

namespace CONTACT
{
  class Interface;
}

namespace CORE::LINALG
{
  class SparseMatrix;
}

namespace ADAPTER
{
  class CouplingNonLinMortar : public CORE::ADAPTER::CouplingMortar
  {
   public:
    /**
     * Construct nonlinear coupling with basic parameters. The remaining information is passed in
     * Setup().
     */
    CouplingNonLinMortar(int spatial_dimension, Teuchos::ParameterList mortar_coupling_params,
        Teuchos::ParameterList contact_dynamic_params,
        CORE::FE::ShapeFunctionType shape_function_type);

    /*!
    \brief initialize routine

    */
    virtual void Setup(Teuchos::RCP<DRT::Discretization> masterdis,
        Teuchos::RCP<DRT::Discretization> slavedis, std::vector<int> coupleddof,
        const std::string& couplingcond);

    virtual void SetupSpringDashpot(Teuchos::RCP<DRT::Discretization> masterdis,
        Teuchos::RCP<DRT::Discretization> slavedis,
        Teuchos::RCP<CORE::Conditions::Condition> spring, const int coupling_id,
        const Epetra_Comm& comm);

    virtual void IntegrateLinD(const std::string& statename, const Teuchos::RCP<Epetra_Vector> vec,
        const Teuchos::RCP<Epetra_Vector> veclm);

    virtual void IntegrateLinDM(const std::string& statename, const Teuchos::RCP<Epetra_Vector> vec,
        const Teuchos::RCP<Epetra_Vector> veclm);

    virtual void IntegrateAll(const std::string& statename, const Teuchos::RCP<Epetra_Vector> vec,
        const Teuchos::RCP<Epetra_Vector> veclm);

    virtual void EvaluateSliding(const std::string& statename,
        const Teuchos::RCP<Epetra_Vector> vec, const Teuchos::RCP<Epetra_Vector> veclm);

    virtual void PrintInterface(std::ostream& os);

    virtual Teuchos::RCP<CORE::LINALG::SparseMatrix> DLinMatrix()
    {
      if (DLin_ == Teuchos::null) FOUR_C_THROW("ERROR: DLin Matrix is null pointer!");
      return DLin_;
    };

    virtual Teuchos::RCP<CORE::LINALG::SparseMatrix> MLinMatrix()
    {
      if (MLin_ == Teuchos::null) FOUR_C_THROW("ERROR: MLin Matrix is null pointer!");
      return MLin_;
    };

    virtual Teuchos::RCP<CORE::LINALG::SparseMatrix> HMatrix()
    {
      if (H_ == Teuchos::null) FOUR_C_THROW("ERROR: H Matrix is null pointer!");
      return H_;
    };

    virtual Teuchos::RCP<CORE::LINALG::SparseMatrix> TMatrix()
    {
      if (T_ == Teuchos::null) FOUR_C_THROW("ERROR: T Matrix is null pointer!");
      return T_;
    };

    virtual Teuchos::RCP<CORE::LINALG::SparseMatrix> NMatrix()
    {
      if (N_ == Teuchos::null) FOUR_C_THROW("ERROR: N Matrix is null pointer!");
      return N_;
    };

    // create projection operator Dinv*M
    void CreateP() override;

    virtual Teuchos::RCP<Epetra_Vector> Gap()
    {
      if (gap_ == Teuchos::null) FOUR_C_THROW("ERROR: gap vector is null pointer!");
      return gap_;
    };

    /// the mortar interface itself
    Teuchos::RCP<CONTACT::Interface> Interface() const { return interface_; }

   protected:
    /*!
    \brief Read Mortar Condition

    */
    virtual void ReadMortarCondition(Teuchos::RCP<DRT::Discretization> masterdis,
        Teuchos::RCP<DRT::Discretization> slavedis, std::vector<int> coupleddof,
        const std::string& couplingcond, Teuchos::ParameterList& input,
        std::map<int, DRT::Node*>& mastergnodes, std::map<int, DRT::Node*>& slavegnodes,
        std::map<int, Teuchos::RCP<DRT::Element>>& masterelements,
        std::map<int, Teuchos::RCP<DRT::Element>>& slaveelements);

    /*!
    \brief Add Mortar Nodes

    */
    virtual void AddMortarNodes(Teuchos::RCP<DRT::Discretization> masterdis,
        Teuchos::RCP<DRT::Discretization> slavedis, std::vector<int> coupleddof,
        Teuchos::ParameterList& input, std::map<int, DRT::Node*>& mastergnodes,
        std::map<int, DRT::Node*>& slavegnodes,
        std::map<int, Teuchos::RCP<DRT::Element>>& masterelements,
        std::map<int, Teuchos::RCP<DRT::Element>>& slaveelements,
        Teuchos::RCP<CONTACT::Interface>& interface, int numcoupleddof);

    /*!
    \brief Add Mortar Elements

    */
    virtual void AddMortarElements(Teuchos::RCP<DRT::Discretization> masterdis,
        Teuchos::RCP<DRT::Discretization> slavedis, Teuchos::ParameterList& input,
        std::map<int, Teuchos::RCP<DRT::Element>>& masterelements,
        std::map<int, Teuchos::RCP<DRT::Element>>& slaveelements,
        Teuchos::RCP<CONTACT::Interface>& interface, int numcoupleddof);

    /*!
    \brief complete interface, store as internal variable
           store maps as internal variable and do parallel redist.

    */
    virtual void CompleteInterface(
        Teuchos::RCP<DRT::Discretization> masterdis, Teuchos::RCP<CONTACT::Interface>& interface);

    /*!
    \brief initialize matrices (interla variables)

    */
    virtual void InitMatrices();

    /*!
    \brief create strategy object if required

    */
    virtual void CreateStrategy(Teuchos::RCP<DRT::Discretization> masterdis,
        Teuchos::RCP<DRT::Discretization> slavedis, Teuchos::ParameterList& input,
        int numcoupleddof);

    /*!
    \brief transform back to initial parallel distribution

    */
    virtual void MatrixRowColTransform();

    /// check setup call
    const bool& IsSetup() const { return issetup_; };

    /// check init and setup call
    void CheckSetup() const override
    {
      if (!IsSetup()) FOUR_C_THROW("ERROR: Call Setup() first!");
    }

   protected:
    bool issetup_;                    ///< check for setup
    Teuchos::RCP<Epetra_Comm> comm_;  ///< communicator
    int myrank_;                      ///< my proc id

    Teuchos::RCP<Epetra_Map> slavenoderowmap_;  ///< map of slave row nodes (after parallel redist.)
    Teuchos::RCP<Epetra_Map>
        pslavenoderowmap_;                  ///< map of slave row nodes (before parallel redist.)
    Teuchos::RCP<Epetra_Map> smdofrowmap_;  ///< map of sm merged row dofs (after parallel redist.)
    Teuchos::RCP<Epetra_Map>
        psmdofrowmap_;  ///< map of sm merged row dofs (before parallel redist.)

    Teuchos::RCP<CORE::LINALG::SparseMatrix> DLin_;  ///< linearization of D matrix
    Teuchos::RCP<CORE::LINALG::SparseMatrix> MLin_;  ///< linearization of M matrix

    Teuchos::RCP<CORE::LINALG::SparseMatrix>
        H_;  ///< Matrix containing the tangent derivatives with respect to slave dofs
    Teuchos::RCP<CORE::LINALG::SparseMatrix>
        T_;  ///< Matrix containing the tangent vectors of the slave nodes
    Teuchos::RCP<CORE::LINALG::SparseMatrix>
        N_;                            ///< Matrix containing the (weighted) gap derivatives
                                       ///< with respect to master and slave dofs
    Teuchos::RCP<Epetra_Vector> gap_;  ///< gap vector

    Teuchos::RCP<CONTACT::Interface> interface_;  ///< interface
  };
}  // namespace ADAPTER

FOUR_C_NAMESPACE_CLOSE

#endif