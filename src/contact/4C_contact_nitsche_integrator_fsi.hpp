/*---------------------------------------------------------------------*/
/*! \file

\brief A class to perform integrations of nitsche related terms for the fsi contact case

\level 3


*/
/*---------------------------------------------------------------------*/
#ifndef FOUR_C_CONTACT_NITSCHE_INTEGRATOR_FSI_HPP
#define FOUR_C_CONTACT_NITSCHE_INTEGRATOR_FSI_HPP

#include "4C_config.hpp"

#include "4C_contact_nitsche_integrator.hpp"
#include "4C_linalg_fixedsizematrix.hpp"

FOUR_C_NAMESPACE_OPEN

namespace XFEM
{
  class XFluidContactComm;
}

namespace CONTACT
{
  class Element;

  class IntegratorNitscheFsi : public IntegratorNitsche
  {
   public:
    /*!
     \brief Constructor  with shape function specification

     Constructs an instance of this class using a specific type of shape functions.<br>
     Note that this is \b not a collective call as overlaps are
     integrated in parallel by individual processes.<br>
     Note also that this constructor relies heavily on the
     CORE::FE::IntegrationPoints structs to get Gauss points
     and corresponding weights.

     */
    IntegratorNitscheFsi(
        Teuchos::ParameterList& params, CORE::FE::CellType eletype, const Epetra_Comm& comm);
    //! @name Derived functions
    //! @{

    //! @name currently unsupported derived methods
    //! @{
    void IntegrateDerivSegment2D(MORTAR::Element& sele, double& sxia, double& sxib,
        MORTAR::Element& mele, double& mxia, double& mxib, const Epetra_Comm& comm,
        const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override
    {
      FOUR_C_THROW("Segment based integration is currently unsupported!");
    }

    void IntegrateDerivEle2D(MORTAR::Element& sele, std::vector<MORTAR::Element*> meles,
        bool* boundary_ele, const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override
    {
      FOUR_C_THROW("Element based integration in 2D is currently unsupported!");
    }

    void IntegrateDerivCell3DAuxPlane(MORTAR::Element& sele, MORTAR::Element& mele,
        Teuchos::RCP<MORTAR::IntCell> cell, double* auxn, const Epetra_Comm& comm,
        const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override
    {
      FOUR_C_THROW("The auxiliary plane 3-D coupling integration case is currently unsupported!");
    }
    //! @}

    /*!
     \brief First, reevaluate which gausspoints should be used
     Second, Build all integrals and linearizations without segmentation -- 3D
     (i.e. M, g, LinM, Ling and possibly D, LinD)
     */
    void IntegrateDerivEle3D(MORTAR::Element& sele, std::vector<MORTAR::Element*> meles,
        bool* boundary_ele, bool* proj_, const Epetra_Comm& comm,
        const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override;

    //! @}

   protected:
    /*!
     \brief Perform integration at GP
            This is where the distinction between methods should be,
            i.e. mortar, augmented, gpts,...
     */
    void IntegrateGP_3D(MORTAR::Element& sele, MORTAR::Element& mele,
        CORE::LINALG::SerialDenseVector& sval, CORE::LINALG::SerialDenseVector& lmval,
        CORE::LINALG::SerialDenseVector& mval, CORE::LINALG::SerialDenseMatrix& sderiv,
        CORE::LINALG::SerialDenseMatrix& mderiv, CORE::LINALG::SerialDenseMatrix& lmderiv,
        CORE::GEN::Pairedvector<int, CORE::LINALG::SerialDenseMatrix>& dualmap, double& wgt,
        double& jac, CORE::GEN::Pairedvector<int, double>& derivjac, double* normal,
        std::vector<CORE::GEN::Pairedvector<int, double>>& dnmap_unit, double& gap,
        CORE::GEN::Pairedvector<int, double>& deriv_gap, double* sxi, double* mxi,
        std::vector<CORE::GEN::Pairedvector<int, double>>& derivsxi,
        std::vector<CORE::GEN::Pairedvector<int, double>>& derivmxi) override;

    /*!
     \brief Perform integration at GP
            This is where the distinction between methods should be,
            i.e. mortar, augmented, gpts,...
     */
    void IntegrateGP_2D(MORTAR::Element& sele, MORTAR::Element& mele,
        CORE::LINALG::SerialDenseVector& sval, CORE::LINALG::SerialDenseVector& lmval,
        CORE::LINALG::SerialDenseVector& mval, CORE::LINALG::SerialDenseMatrix& sderiv,
        CORE::LINALG::SerialDenseMatrix& mderiv, CORE::LINALG::SerialDenseMatrix& lmderiv,
        CORE::GEN::Pairedvector<int, CORE::LINALG::SerialDenseMatrix>& dualmap, double& wgt,
        double& jac, CORE::GEN::Pairedvector<int, double>& derivjac, double* normal,
        std::vector<CORE::GEN::Pairedvector<int, double>>& dnmap_unit, double& gap,
        CORE::GEN::Pairedvector<int, double>& deriv_gap, double* sxi, double* mxi,
        std::vector<CORE::GEN::Pairedvector<int, double>>& derivsxi,
        std::vector<CORE::GEN::Pairedvector<int, double>>& derivmxi) override
    {
      FOUR_C_THROW("2d problems not available for IntegratorNitscheFsi, as CutFEM is only 3D!");
    }

   private:
    /*!
    \brief evaluate GPTS forces and linearization at this gp
    */
    template <int dim>
    void GPTSForces(MORTAR::Element& sele, MORTAR::Element& mele,
        const CORE::LINALG::SerialDenseVector& sval, const CORE::LINALG::SerialDenseMatrix& sderiv,
        const std::vector<CORE::GEN::Pairedvector<int, double>>& dsxi,
        const CORE::LINALG::SerialDenseVector& mval, const CORE::LINALG::SerialDenseMatrix& mderiv,
        const std::vector<CORE::GEN::Pairedvector<int, double>>& dmxi, const double jac,
        const CORE::GEN::Pairedvector<int, double>& jacintcellmap, const double wgt,
        const double gap, const CORE::GEN::Pairedvector<int, double>& dgapgp, const double* gpn,
        std::vector<CORE::GEN::Pairedvector<int, double>>& dnmap_unit, double* sxi, double* mxi);

    /// Update Element contact state -2...not_specified, -1...no_contact, 0...mixed, 1...contact
    void UpdateEleContactState(MORTAR::Element& sele, int state);

    /// Element contact state -2...not_specified, -1...no_contact, 0...mixed, 1...contact
    int ele_contact_state_;

    /// Xfluid Contact Communicator
    Teuchos::RCP<XFEM::XFluidContactComm> xf_c_comm_;
  };

  namespace UTILS
  {
    /// Compute Cauchy stress component sigma_{n dir} at local coord xsi
    double SolidCauchyAtXi(CONTACT::Element* cele,  ///< the contact element
        const CORE::LINALG::Matrix<2, 1>& xsi,      ///< local coord on the ele element
        const CORE::LINALG::Matrix<3, 1>& n,        ///< normal n
        const CORE::LINALG::Matrix<3, 1>& dir       ///< second directional vector
    );
  }  // namespace UTILS
}  // namespace CONTACT
FOUR_C_NAMESPACE_CLOSE

#endif
