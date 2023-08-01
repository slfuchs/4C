/*---------------------------------------------------------------------*/
/*! \file

\brief Internal implementation of RedAcinus element. Methods implemented here
       are called by acinus_evaluate.cpp by DRT::ELEMENTS::RedAcinus::Evaluate()
       with the corresponding action.


\level 3

*/
/*---------------------------------------------------------------------*/



#include "baci_red_airways_acinus_impl.H"

#include "baci_discretization_fem_general_utils_fem_shapefunctions.H"
#include "baci_discretization_fem_general_utils_gder2.H"
#include "baci_lib_discret.H"
#include "baci_lib_function.H"
#include "baci_lib_function_of_time.H"
#include "baci_lib_globalproblem.H"
#include "baci_lib_utils.H"
#include "baci_mat_air_0d_O2_saturation.H"
#include "baci_mat_list.H"
#include "baci_mat_maxwell_0d_acinus.H"
#include "baci_mat_newtonianfluid.H"
#include "baci_mat_par_bundle.H"
#include "baci_red_airways_elem_params.h"
#include "baci_red_airways_evaluation_data.h"

#include <fstream>
#include <iomanip>


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::RedAcinusImplInterface* DRT::ELEMENTS::RedAcinusImplInterface::Impl(
    DRT::ELEMENTS::RedAcinus* red_acinus)
{
  switch (red_acinus->Shape())
  {
    case DRT::Element::line2:
    {
      static AcinusImpl<DRT::Element::line2>* acinus;
      if (acinus == NULL)
      {
        acinus = new AcinusImpl<DRT::Element::line2>;
      }
      return acinus;
    }
    default:
      dserror("shape %d (%d nodes) not supported", red_acinus->Shape(), red_acinus->NumNode());
      break;
  }
  return NULL;
}


/*----------------------------------------------------------------------*
  | constructor (public)                                    ismail 01/10 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::AcinusImpl<distype>::AcinusImpl()
{
}


/*----------------------------------------------------------------------*
 |  calculate element matrix and right hand side (private)  ismail 01/10|
 |                                                                      |
 *----------------------------------------------------------------------*/
/*!
        \brief calculate element matrix and rhs

\param ele              (i) the element those matrix is calculated
        \param eqnp             (i) nodal volumetric flow rate at n+1
        \param evelnp           (i) nodal velocity at n+1
        \param eareanp          (i) nodal cross-sectional area at n+1
        \param eprenp           (i) nodal pressure at n+1
        \param estif            (o) element matrix to calculate
        \param eforce           (o) element rhs to calculate
        \param material         (i) acinus material/dimesion
        \param time             (i) current simulation time
        \param dt               (i) timestep
        */
template <DRT::Element::DiscretizationType distype>
void Sysmat(DRT::ELEMENTS::RedAcinus* ele, CORE::LINALG::SerialDenseVector& epnp,
    CORE::LINALG::SerialDenseVector& epn, CORE::LINALG::SerialDenseVector& epnm,
    CORE::LINALG::SerialDenseMatrix& sysmat, CORE::LINALG::SerialDenseVector& rhs,
    Teuchos::RCP<const MAT::Material> material, DRT::REDAIRWAYS::ElemParams& params, double time,
    double dt)
{
  // Decide which acinus material should be used
  if ((material->MaterialType() == INPAR::MAT::m_0d_maxwell_acinus_neohookean) ||
      (material->MaterialType() == INPAR::MAT::m_0d_maxwell_acinus_exponential) ||
      (material->MaterialType() == INPAR::MAT::m_0d_maxwell_acinus_doubleexponential) ||
      (material->MaterialType() == INPAR::MAT::m_0d_maxwell_acinus_ogden))
  {
    double VolAcinus;
    ele->getParams("AcinusVolume", VolAcinus);
    double volAlvDuct;
    ele->getParams("AlveolarDuctVolume", volAlvDuct);
    const double NumOfAcini = double(floor(VolAcinus / volAlvDuct));

    const Teuchos::RCP<MAT::Maxwell_0d_acinus> acinus_mat =
        Teuchos::rcp_dynamic_cast<MAT::Maxwell_0d_acinus>(ele->Material());

    // Evaluate material law for acinus
    acinus_mat->Evaluate(epnp, epn, epnm, sysmat, rhs, params, NumOfAcini, volAlvDuct, time, dt);
  }
  else
  {
    dserror("Material law is not a valid reduced dimensional lung acinus material.");
    exit(1);
  }
}


/*----------------------------------------------------------------------*
 | evaluate (public)                                       ismail 01/10 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::AcinusImpl<distype>::Evaluate(RedAcinus* ele, Teuchos::ParameterList& params,
    DRT::Discretization& discretization, std::vector<int>& lm,
    CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
    CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
    CORE::LINALG::SerialDenseVector& elevec1_epetra,
    CORE::LINALG::SerialDenseVector& elevec2_epetra,
    CORE::LINALG::SerialDenseVector& elevec3_epetra, Teuchos::RCP<MAT::Material> mat)
{
  const int elemVecdim = elevec1_epetra.length();
  std::vector<int>::iterator it_vcr;

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // Get control parameters for time integration
  // get time-step size
  const double dt = evaluation_data.dt;
  // get time
  const double time = evaluation_data.time;

  // Get control parameters for stabilization and higher-order elements (currently unused)
  // flag for higher order elements
  // bool higher_order_ele = ele->isHigherOrderElement(distype);

  // Get all general state vectors: flow, pressure,
  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");
  Teuchos::RCP<const Epetra_Vector> pn = discretization.GetState("pn");
  Teuchos::RCP<const Epetra_Vector> pnm = discretization.GetState("pnm");

  Teuchos::RCP<const Epetra_Vector> ial = discretization.GetState("intr_ac_link");

  if (pnp == Teuchos::null || pn == Teuchos::null || pnm == Teuchos::null)
    dserror("Cannot get state vectors 'pnp', 'pn', and/or 'pnm''");

  // Extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  DRT::UTILS::ExtractMyValues(*pnp, mypnp, lm);

  // Extract local values from the global vectors
  std::vector<double> mypn(lm.size());
  DRT::UTILS::ExtractMyValues(*pn, mypn, lm);

  // Extract local values from the global vectors
  std::vector<double> mypnm(lm.size());
  DRT::UTILS::ExtractMyValues(*pnm, mypnm, lm);

  // Extract local values from the global vectors
  std::vector<double> myial(lm.size());
  DRT::UTILS::ExtractMyValues(*ial, myial, lm);

  // Create objects for element arrays
  CORE::LINALG::SerialDenseVector epnp(elemVecdim);
  CORE::LINALG::SerialDenseVector epn(elemVecdim);
  CORE::LINALG::SerialDenseVector epnm(elemVecdim);
  for (int i = 0; i < elemVecdim; ++i)
  {
    // Split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
    epn(i) = mypn[i];
    epnm(i) = mypnm[i];
  }

  double e_acin_e_vnp;
  double e_acin_e_vn;

  for (int i = 0; i < elemVecdim; ++i)
  {
    // Split area and volumetric flow rate, insert into element arrays
    e_acin_e_vnp = (*evaluation_data.acinar_vnp)[ele->LID()];
    e_acin_e_vn = (*evaluation_data.acinar_vn)[ele->LID()];
  }

  // Get the volumetric flow rate from the previous time step
  DRT::REDAIRWAYS::ElemParams elem_params;
  elem_params.qout_np = (*evaluation_data.qout_np)[ele->LID()];
  elem_params.qout_n = (*evaluation_data.qout_n)[ele->LID()];
  elem_params.qout_nm = (*evaluation_data.qout_nm)[ele->LID()];
  elem_params.qin_np = (*evaluation_data.qin_np)[ele->LID()];
  elem_params.qin_n = (*evaluation_data.qin_n)[ele->LID()];
  elem_params.qin_nm = (*evaluation_data.qin_nm)[ele->LID()];

  elem_params.acin_vnp = e_acin_e_vnp;
  elem_params.acin_vn = e_acin_e_vn;

  elem_params.lungVolume_np = evaluation_data.lungVolume_np;
  elem_params.lungVolume_n = evaluation_data.lungVolume_n;
  elem_params.lungVolume_nm = evaluation_data.lungVolume_nm;

  // Call routine for calculating element matrix and right hand side
  Sysmat<distype>(ele, epnp, epn, epnm, elemat1_epetra, elevec1_epetra, mat, elem_params, time, dt);

  double Ao = 0.0;
  ele->getParams("Area", Ao);

  // Put zeros on second line of matrix and rhs in case of interacinar linker
  if (myial[1] > 0.0)
  {
    elemat1_epetra(1, 0) = 0.0;
    elemat1_epetra(1, 1) = 0.0;
    elevec1_epetra(1) = 0.0;
  }

  return 0;
}


/*----------------------------------------------------------------------*
 |  calculate element matrix and right hand side (private)  ismail 01/10|
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::Initial(RedAcinus* ele, Teuchos::ParameterList& params,
    DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<const MAT::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  std::vector<int> lmstride;
  Teuchos::RCP<std::vector<int>> lmowner = Teuchos::rcp(new std::vector<int>);
  ele->LocationVector(discretization, lm, *lmowner, lmstride);

  // Initialize the pressure vectors
  if (myrank == (*lmowner)[0])
  {
    int gid = lm[0];
    double val = 0.0;
    evaluation_data.p0np->ReplaceGlobalValues(1, &val, &gid);
    evaluation_data.p0n->ReplaceGlobalValues(1, &val, &gid);
    evaluation_data.p0nm->ReplaceGlobalValues(1, &val, &gid);
  }

  // Find the volume of an acinus element
  {
    int gid2 = ele->Id();
    double acin_vol = 0.0;
    ele->getParams("AcinusVolume", acin_vol);
    evaluation_data.acini_e_volume->ReplaceGlobalValues(1, &acin_vol, &gid2);
  }

  // Get the generation numbers
  for (int i = 0; i < 2; i++)
  {
    if (ele->Nodes()[i]->GetCondition("RedAirwayEvalLungVolCond"))
    {
      // find the acinus condition
      int gid = ele->Id();
      double val = 1.0;
      evaluation_data.acini_bc->ReplaceGlobalValues(1, &val, &gid);
    }
  }
  {
    int gid = ele->Id();
    int generation = -1;
    double val = double(generation);
    evaluation_data.generations->ReplaceGlobalValues(1, &val, &gid);
  }

  if (evaluation_data.solveScatra)
  {
    double A = 0.0;
    double V = 0.0;
    ele->getParams("AcinusVolume", V);
    ele->getParams("Area", A);
    int gid = lm[1];
    evaluation_data.junVolMix_Corrector->ReplaceGlobalValues(1, &A, &gid);

    for (int sci = 0; sci < iel; sci++)
    {
      int sgid = lm[sci];
      int esgid = ele->Id();
      // -------------------------------------------------------------
      //
      // -------------------------------------------------------------
      if (ele->Nodes()[sci]->GetCondition("RedAirwayScatraAirCond"))
      {
        double intSat = ele->Nodes()[sci]
                            ->GetCondition("RedAirwayScatraAirCond")
                            ->GetDouble("INITIAL_CONCENTRATION");
        int id = DRT::Problem::Instance()->Materials()->FirstIdByType(
            INPAR::MAT::m_0d_o2_air_saturation);
        // check if O2 properties material exists
        if (id == -1)
        {
          dserror("A material defining O2 properties in air could not be found");
          exit(1);
        }
        const MAT::PAR::Parameter* smat = DRT::Problem::Instance()->Materials()->ParameterById(id);
        const MAT::PAR::Air_0d_O2_saturation* actmat =
            static_cast<const MAT::PAR::Air_0d_O2_saturation*>(smat);

        // get atmospheric pressure
        double patm = actmat->atmospheric_p_;
        // get number of O2 moles per unit volume of O2
        double nO2perVO2 = actmat->nO2_per_VO2_;

        // calculate the PO2 at nodes
        double pO2 = intSat * patm;

        // calculate VO2
        double vO2 = V * (pO2 / patm);
        // evaluate initial concentration
        double intConc = nO2perVO2 * vO2 / V;

        evaluation_data.scatranp->ReplaceGlobalValues(1, &intConc, &sgid);
        evaluation_data.e1scatranp->ReplaceGlobalValues(1, &intConc, &esgid);
        evaluation_data.e2scatranp->ReplaceGlobalValues(1, &intConc, &esgid);
      }
      else
      {
        dserror("0D Acinus scatra must be predefined as \"air\" only");
        exit(1);
      }
    }
  }
}  // AcinusImpl::Initial


/*----------------------------------------------------------------------*
 |  Evaluate the values of the degrees of freedom           ismail 01/10|
 |  at terminal nodes.                                                  |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::EvaluateTerminalBC(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    CORE::LINALG::SerialDenseVector& rhs, Teuchos::RCP<MAT::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // Get total time
  const double time = evaluation_data.time;

  // The number of nodes
  const int numnode = lm.size();
  std::vector<int>::iterator it_vcr;

  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");

  if (pnp == Teuchos::null) dserror("Cannot get state vectors 'pnp'");

  // Extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  DRT::UTILS::ExtractMyValues(*pnp, mypnp, lm);

  // Create objects for element arrays
  CORE::LINALG::SerialDenseVector epnp(numnode);

  // Get all values at the last computed time step
  for (int i = 0; i < numnode; ++i)
  {
    // Split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
  }

  /**
   * Resolve the BCs
   **/
  for (int i = 0; i < ele->NumNode(); i++)
  {
    if (ele->Nodes()[i]->Owner() == myrank)
    {
      if (ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond") ||
          ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond") ||
          ele->Nodes()[i]->GetCondition("RedAcinusVentilatorCond"))
      {
        std::string Bc;
        double BCin = 0.0;
        if (ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond"))
        {
          DRT::Condition* condition = ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond");
          // Get the type of prescribed bc
          Bc = *(condition->Get<std::string>("boundarycond"));

          const std::vector<double>* vals = condition->Get<std::vector<double>>("val");
          const std::vector<int>* curve = condition->Get<std::vector<int>>("curve");
          const std::vector<int>* functions = condition->Get<std::vector<int>>("funct");

          // Read in the value of the applied BC
          // Get factor of first CURVE
          double curvefac = 1.0;
          if ((*curve)[0] >= 0)
          {
            curvefac = DRT::Problem::Instance()
                           ->FunctionById<DRT::UTILS::FunctionOfTime>((*curve)[0])
                           .Evaluate(time);
            BCin = (*vals)[0] * curvefac;
          }
          else
          {
            dserror("no boundary condition defined!");
            exit(1);
          }

          // Get factor of FUNCT
          int functnum = -1;
          if (functions)
            functnum = (*functions)[0];
          else
            functnum = -1;

          double functionfac = 0.0;
          if (functnum > 0)
          {
            functionfac = DRT::Problem::Instance()
                              ->FunctionById<DRT::UTILS::FunctionOfSpaceTime>(functnum - 1)
                              .Evaluate((ele->Nodes()[i])->X(), time, 0);
          }

          // Get factor of second CURVE
          int curve2num = -1;
          double curve2fac = 1.0;
          if (curve) curve2num = (*curve)[1];
          if (curve2num >= 0)
            curve2fac = DRT::Problem::Instance()
                            ->FunctionById<DRT::UTILS::FunctionOfTime>(curve2num)
                            .Evaluate(time);

          // Add first_CURVE + FUNCTION * second_CURVE
          BCin += functionfac * curve2fac;

          // Get the local id of the node to whom the bc is prescribed
          int local_id = discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
          if (local_id < 0)
          {
            dserror("node (%d) doesn't exist on proc(%d)", ele->Nodes()[i]->Id(),
                discretization.Comm().MyPID());
            exit(1);
          }
        }
        /**
         * For Art_redD_3D_CouplingCond bc
         **/
        else if (ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond"))
        {
          const DRT::Condition* condition =
              ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond");

          Teuchos::RCP<Teuchos::ParameterList> CoupledTo3DParams =
              params.get<Teuchos::RCP<Teuchos::ParameterList>>("coupling with 3D fluid params");
          // -----------------------------------------------------------------
          // If the parameter list is empty, then something is wrong!
          // -----------------------------------------------------------------
          if (CoupledTo3DParams.get() == NULL)
          {
            dserror(
                "Cannot prescribe a boundary condition from 3D to reduced D, if the parameters "
                "passed don't exist");
            exit(1);
          }

          // -----------------------------------------------------------------
          // Read in Condition type
          // -----------------------------------------------------------------
          //        Type = *(condition->Get<std::string>("CouplingType"));
          // -----------------------------------------------------------------
          // Read in coupling variable rescribed by the 3D simulation
          //
          //     In this case a map called map3D has the following form:
          //     +-----------------------------------------------------------+
          //     |           std::map< string               ,  double        >    |
          //     |     +------------------------------------------------+    |
          //     |     |  ID  | coupling variable name | variable value |    |
          //     |     +------------------------------------------------+    |
          //     |     |  1   |   flow1                |     0.12116    |    |
          //     |     +------+------------------------+----------------+    |
          //     |     |  2   |   pressure2            |    10.23400    |    |
          //     |     +------+------------------------+----------------+    |
          //     |     .  .   .   ....                 .     .......    .    |
          //     |     +------+------------------------+----------------+    |
          //     |     |  N   |   variableN            |    value(N)    |    |
          //     |     +------+------------------------+----------------+    |
          //     +-----------------------------------------------------------+
          // -----------------------------------------------------------------

          int ID = condition->GetInt("ConditionID");
          Teuchos::RCP<std::map<std::string, double>> map3D;
          map3D = CoupledTo3DParams->get<Teuchos::RCP<std::map<std::string, double>>>(
              "3D map of values");

          // find the applied boundary variable
          std::stringstream stringID;
          stringID << "_" << ID;
          for (std::map<std::string, double>::iterator itr = map3D->begin(); itr != map3D->end();
               itr++)
          {
            std::string VariableWithId = itr->first;
            size_t found;
            found = VariableWithId.rfind(stringID.str());
            if (found != std::string::npos)
            {
              Bc = std::string(VariableWithId, 0, found);
              BCin = itr->second;
              break;
            }
          }
        }
        /**
         * For RedAcinusVentilatorCond bc
         **/
        else if (ele->Nodes()[i]->GetCondition("RedAcinusVentilatorCond"))
        {
          DRT::Condition* condition = ele->Nodes()[i]->GetCondition("RedAcinusVentilatorCond");
          // Get the type of prescribed bc
          Bc = *(condition->Get<std::string>("phase1"));

          double period = condition->GetDouble("period");
          double period1 = condition->GetDouble("phase1_period");

          unsigned int phase_number = 0;

          if (fmod(time, period) > period1)
          {
            phase_number = 1;
            Bc = *(condition->Get<std::string>("phase2"));
          }

          const std::vector<int>* curve = condition->Get<std::vector<int>>("curve");
          double curvefac = 1.0;
          const std::vector<double>* vals = condition->Get<std::vector<double>>("val");

          // Read in the value of the applied BC
          if ((*curve)[phase_number] >= 0)
          {
            curvefac = DRT::Problem::Instance()
                           ->FunctionById<DRT::UTILS::FunctionOfTime>((*curve)[phase_number])
                           .Evaluate(time);
            BCin = (*vals)[phase_number] * curvefac;
          }
          else
          {
            dserror("no boundary condition defined!");
            exit(1);
          }

          // Get the local id of the node to whom the bc is prescribed
          int local_id = discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
          if (local_id < 0)
          {
            dserror("node (%d) doesn't exist on proc(%d)", ele->Nodes()[i]->Id(),
                discretization.Comm().MyPID());
            exit(1);
          }
        }
        else
        {
        }

        /**
         * For pressure or VolumeDependentPleuralPressure bc
         **/
        if (Bc == "pressure" || Bc == "VolumeDependentPleuralPressure")
        {
          if (Bc == "VolumeDependentPleuralPressure")
          {
            DRT::Condition* pplCond =
                ele->Nodes()[i]->GetCondition("RedAirwayVolDependentPleuralPressureCond");
            double Pp_np = 0.0;
            if (pplCond)
            {
              const std::vector<int>* curve = pplCond->Get<std::vector<int>>("curve");
              double curvefac = 1.0;
              const std::vector<double>* vals = pplCond->Get<std::vector<double>>("val");

              // Read in the value of the applied BC
              if ((*curve)[0] >= 0)
              {
                curvefac = DRT::Problem::Instance()
                               ->FunctionById<DRT::UTILS::FunctionOfTime>((*curve)[0])
                               .Evaluate(time);
              }

              // Get parameters for VolumeDependentPleuralPressure condition
              std::string ppl_Type = *(pplCond->Get<std::string>("TYPE"));
              double ap = pplCond->GetDouble("P_PLEURAL_0");
              double bp = pplCond->GetDouble("P_PLEURAL_LIN");
              double cp = pplCond->GetDouble("P_PLEURAL_NONLIN");
              double dp = pplCond->GetDouble("TAU");
              double RV = pplCond->GetDouble("RV");
              double TLC = pplCond->GetDouble("TLC");

              DRT::REDAIRWAYS::EvaluationData& evaluation_data =
                  DRT::REDAIRWAYS::EvaluationData::get();

              // Safety check: in case of polynomial TLC is not used
              if (((ppl_Type == "Linear_Polynomial") or (ppl_Type == "Nonlinear_Polynomial")) and
                  (TLC != 0.0))
              {
                dserror(
                    "TLC is not used for the following type of VolumeDependentPleuralPressure BC: "
                    "%s.\n Set TLC = 0.0",
                    ppl_Type.c_str());
              }

              // Safety check: in case of Ogden TLC, P_PLEURAL_0, and P_PLEURAL_LIN
              if ((ppl_Type == "Nonlinear_Ogden") and
                  ((TLC != 0.0) or (ap != 0.0) or (bp != 0.0) or (dp == 0.0)))
              {
                dserror(
                    "Parameters are not set correctly for Nonlinear_Ogden. Only P_PLEURAL_NONLIN, "
                    "TAU and RV are used. Set all others to zero. TAU is not allowed to be zero.");
              }

              if (ppl_Type == "Linear_Polynomial")
              {
                const double lungVolumenp = evaluation_data.lungVolume_n;
                Pp_np = ap + bp * (lungVolumenp - RV) + cp * pow((lungVolumenp - RV), dp);
              }
              else if (ppl_Type == "Linear_Exponential")
              {
                const double lungVolumenp = evaluation_data.lungVolume_n;
                const double TLCnp = (lungVolumenp - RV) / (TLC - RV);
                Pp_np = ap + bp * TLCnp + cp * exp(dp * TLCnp);
              }
              else if (ppl_Type == "Linear_Ogden")
              {
                const double lungVolumenp = evaluation_data.lungVolume_n;
                Pp_np = RV / lungVolumenp * cp / dp * (1 - pow(RV / lungVolumenp, dp));
              }
              else if (ppl_Type == "Nonlinear_Polynomial")
              {
                const double lungVolumenp = evaluation_data.lungVolume_np;
                Pp_np = ap + bp * (lungVolumenp - RV) + cp * pow((lungVolumenp - RV), dp);
              }
              else if (ppl_Type == "Nonlinear_Exponential")
              {
                const double lungVolumenp = evaluation_data.lungVolume_np;
                const double TLCnp = (lungVolumenp - RV) / (TLC - RV);
                Pp_np = ap + bp * TLCnp + cp * exp(dp * TLCnp);
              }
              else if (ppl_Type == "Nonlinear_Ogden")
              {
                const double lungVolumenp = evaluation_data.lungVolume_np;
                Pp_np = RV / lungVolumenp * cp / dp * (1 - pow(RV / lungVolumenp, dp));
              }
              else
              {
                dserror("Unknown volume pleural pressure type: %s", ppl_Type.c_str());
              }
              Pp_np *= curvefac * ((*vals)[0]);
            }
            else
            {
              dserror("No volume dependent pleural pressure condition was defined");
            }
            BCin += Pp_np;
          }

          DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

          // Set pressure at node i
          int gid;
          double val;

          gid = lm[i];
          val = BCin;
          evaluation_data.bcval->ReplaceGlobalValues(1, &val, &gid);

          gid = lm[i];
          val = 1;
          evaluation_data.dbctog->ReplaceGlobalValues(1, &val, &gid);
        }
        /**
         * For flow bc
         **/
        else if (Bc == "flow")
        {
          // ----------------------------------------------------------
          // Since a node might belong to multiple elements then the
          // flow might be added to the rhs multiple time.
          // To fix this the flow is divided by the number of elements
          // (which is the number of branches). Thus the sum of the
          // final added values is the actual prescribed flow.
          // ----------------------------------------------------------
          int numOfElems = (ele->Nodes()[i])->NumElement();
          BCin /= double(numOfElems);
          rhs(i) += -BCin + rhs(i);
        }
        else
        {
          dserror("prescribed [%s] is not defined for reduced acinuss", Bc.c_str());
          exit(1);
        }
      }
      /**
       * If the node is a terminal node, but no b.c is prescribed to it
       * then a zero output pressure is assumed
       **/
      else
      {
        if (ele->Nodes()[i]->NumElement() == 1)
        {
          // Get the local id of the node to whome the bc is prescribed
          int local_id = discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
          if (local_id < 0)
          {
            dserror("node (%d) doesn't exist on proc(%d)", ele->Nodes()[i],
                discretization.Comm().MyPID());
            exit(1);
          }

          DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

          // Set pressure=0.0 at node i
          int gid;
          double val;

          gid = lm[i];
          val = 0.0;
          evaluation_data.bcval->ReplaceGlobalValues(1, &val, &gid);

          gid = lm[i];
          val = 1;
          evaluation_data.dbctog->ReplaceGlobalValues(1, &val, &gid);
        }
      }  // END of if there is no BC but the node still is at the terminal

    }  // END of if node is available on this processor
  }    // End of node i has a condition
}


/*----------------------------------------------------------------------*
 |  Calculate flowrate at current iteration step and the    ismail 01/10|
 |  correponding acinus volume via dV = 0.5*(qnp+qn)*dt                 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::CalcFlowRates(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)

{
  const int elemVecdim = lm.size();

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // Get control parameters for time integration
  // Get time-step size
  const double dt = evaluation_data.dt;
  // Get time
  const double time = evaluation_data.time;

  // Get control parameters for stabilization and higher-order elements
  // flag for higher order elements
  //  bool higher_order_ele = ele->isHigherOrderElement(distype);

  // Get all general state vectors: flow, pressure,
  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");
  Teuchos::RCP<const Epetra_Vector> pn = discretization.GetState("pn");
  Teuchos::RCP<const Epetra_Vector> pnm = discretization.GetState("pnm");

  if (pnp == Teuchos::null || pn == Teuchos::null || pnm == Teuchos::null)
    dserror("Cannot get state vectors 'pnp', 'pn', and/or 'pnm''");

  // Extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  DRT::UTILS::ExtractMyValues(*pnp, mypnp, lm);

  // Extract local values from the global vectors
  std::vector<double> mypn(lm.size());
  DRT::UTILS::ExtractMyValues(*pn, mypn, lm);

  // Extract local values from the global vectors
  std::vector<double> mypnm(lm.size());
  DRT::UTILS::ExtractMyValues(*pnm, mypnm, lm);

  // Create objects for element arrays
  CORE::LINALG::SerialDenseVector epnp(elemVecdim);
  CORE::LINALG::SerialDenseVector epn(elemVecdim);
  CORE::LINALG::SerialDenseVector epnm(elemVecdim);
  for (int i = 0; i < elemVecdim; ++i)
  {
    // Split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
    epn(i) = mypn[i];
    epnm(i) = mypnm[i];
  }

  double e_acin_vnp = 0.0;
  double e_acin_vn = 0.0;

  for (int i = 0; i < elemVecdim; ++i)
  {
    // Split area and volumetric flow rate, insert into element arrays
    e_acin_vnp = (*evaluation_data.acinar_vnp)[ele->LID()];
    e_acin_vn = (*evaluation_data.acinar_vn)[ele->LID()];
  }

  // Get the volumetric flow rate from the previous time step
  DRT::REDAIRWAYS::ElemParams elem_params;
  elem_params.qout_np = (*evaluation_data.qout_np)[ele->LID()];
  elem_params.qout_n = (*evaluation_data.qout_n)[ele->LID()];
  elem_params.qout_nm = (*evaluation_data.qout_nm)[ele->LID()];
  elem_params.qin_np = (*evaluation_data.qin_np)[ele->LID()];
  elem_params.qin_n = (*evaluation_data.qin_n)[ele->LID()];
  elem_params.qin_nm = (*evaluation_data.qin_nm)[ele->LID()];

  elem_params.acin_vnp = e_acin_vnp;
  elem_params.acin_vn = e_acin_vn;

  CORE::LINALG::SerialDenseMatrix sysmat(elemVecdim, elemVecdim, true);
  CORE::LINALG::SerialDenseVector rhs(elemVecdim);

  // Call routine for calculating element matrix and right hand side
  Sysmat<distype>(ele, epnp, epn, epnm, sysmat, rhs, material, elem_params, time, dt);

  double qn = (*evaluation_data.qin_n)[ele->LID()];
  double qnp = -1.0 * (sysmat(0, 0) * epnp(0) + sysmat(0, 1) * epnp(1) - rhs(0));

  int gid = ele->Id();

  evaluation_data.qin_np->ReplaceGlobalValues(1, &qnp, &gid);
  evaluation_data.qout_np->ReplaceGlobalValues(1, &qnp, &gid);

  // Calculate the new volume of the acinus due to the incoming flow; 0.5*(qnp+qn)*dt
  {
    double acinus_volume = e_acin_vn;
    acinus_volume += 0.5 * (qnp + qn) * dt;
    evaluation_data.acinar_vnp->ReplaceGlobalValues(1, &acinus_volume, &gid);

    // Calculate correponding acinar strain
    double vo = 0.0;
    ele->getParams("AcinusVolume", vo);
    double avs_np = (acinus_volume - vo) / vo;
    evaluation_data.acinar_vnp_strain->ReplaceGlobalValues(1, &avs_np, &gid);
  }
}


/*----------------------------------------------------------------------*
 |  Calculate element volume                                ismail 07/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::CalcElemVolume(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)

{
  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // Get acinus size
  double evolnp = (*evaluation_data.elemVolumenp)[ele->LID()];

  // Get element global ID
  int gid = ele->Id();

  // Update elem
  evaluation_data.elemVolumenp->ReplaceGlobalValues(1, &evolnp, &gid);

  // calculate and update element radius
  double eRadiusnp = std::pow(evolnp * 0.75 * M_1_PI, 1.0 / 3.0);
  evaluation_data.elemRadiusnp->ReplaceGlobalValues(1, &eRadiusnp, &gid);
}

/*----------------------------------------------------------------------*
 |  Get the coupled the values on the coupling interface    ismail 07/10|
 |  of the 3D/reduced-D problem                                         |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::GetCoupledValues(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  // The number of nodes
  const int numnode = lm.size();
  std::vector<int>::iterator it_vcr;

  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");

  if (pnp == Teuchos::null) dserror("Cannot get state vectors 'pnp'");

  // extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  DRT::UTILS::ExtractMyValues(*pnp, mypnp, lm);

  // create objects for element arrays
  CORE::LINALG::SerialDenseVector epnp(numnode);

  // get all values at the last computed time step
  for (int i = 0; i < numnode; ++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
  }

  // ---------------------------------------------------------------------------------
  // Resolve the BCs
  // ---------------------------------------------------------------------------------
  for (int i = 0; i < ele->NumNode(); i++)
  {
    if (ele->Nodes()[i]->Owner() == myrank)
    {
      if (ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond"))
      {
        const DRT::Condition* condition = ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond");
        Teuchos::RCP<Teuchos::ParameterList> CoupledTo3DParams =
            params.get<Teuchos::RCP<Teuchos::ParameterList>>("coupling with 3D fluid params");
        // -----------------------------------------------------------------
        // If the parameter list is empty, then something is wrong!
        // -----------------------------------------------------------------
        if (CoupledTo3DParams.get() == NULL)
        {
          dserror(
              "Cannot prescribe a boundary condition from 3D to reduced D, if the parameters "
              "passed don't exist");
          exit(1);
        }


        // -----------------------------------------------------------------
        // Compute the variable solved by the reduced D simulation to be
        // passed to the 3D simulation
        //
        //     In this case a map called map1D has the following form:
        //     +-----------------------------------------------------------+
        //     |              std::map< string            ,  double        > >  |
        //     |     +------------------------------------------------+    |
        //     |     |  ID  | coupling variable name | variable value |    |
        //     |     +------------------------------------------------+    |
        //     |     |  1   |   flow1                |     xxxxxxx    |    |
        //     |     +------+------------------------+----------------+    |
        //     |     |  2   |   pressure2            |     xxxxxxx    |    |
        //     |     +------+------------------------+----------------+    |
        //     |     .  .   .   ....                 .     .......    .    |
        //     |     +------+------------------------+----------------+    |
        //     |     |  N   |   variable(N)          | trash value(N) |    |
        //     |     +------+------------------------+----------------+    |
        //     +-----------------------------------------------------------+
        // -----------------------------------------------------------------

        int ID = condition->GetInt("ConditionID");
        Teuchos::RCP<std::map<std::string, double>> map1D;
        map1D = CoupledTo3DParams->get<Teuchos::RCP<std::map<std::string, double>>>(
            "reducedD map of values");

        std::string returnedBC = *(condition->Get<std::string>("ReturnedVariable"));

        double BC3d = 0.0;
        if (returnedBC == "flow")
        {
          // MUST BE DONE
        }
        else if (returnedBC == "pressure")
        {
          BC3d = epnp(i);
        }
        else
        {
          std::string str = (*condition->Get<std::string>("ReturnedVariable"));
          dserror("%s, is an unimplimented type of coupling", str.c_str());
          exit(1);
        }
        std::stringstream returnedBCwithId;
        returnedBCwithId << returnedBC << "_" << ID;

        //        std::cout<<"Return ["<<returnedBC<<"] form 1D problem to 3D SURFACE of
        //        ID["<<ID<<"]: "<<BC3d<<std::endl;

        // -----------------------------------------------------------------
        // Check whether the coupling wrapper has already initialized this
        // map else wise we will have problems with parallelization, that's
        // because of the preassumption that the map is filled and sorted
        // Thus we can use parallel addition
        // -----------------------------------------------------------------

        std::map<std::string, double>::iterator itrMap1D;
        itrMap1D = map1D->find(returnedBCwithId.str());
        if (itrMap1D == map1D->end())
        {
          dserror("The 3D map for (1D - 3D coupling) has no variable (%s) for ID [%d]",
              returnedBC.c_str(), ID);
          exit(1);
        }

        // update the 1D map
        (*map1D)[returnedBCwithId.str()] = BC3d;
      }
    }  // END of if node is available on this processor
  }    // End of node i has a condition
}


/*----------------------------------------------------------------------*
 |  calculate the amount of fluid mixing inside a           ismail 02/13|
 |  junction                                                            |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::GetJunctionVolumeMix(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    CORE::LINALG::SerialDenseVector& volumeMix_np, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // get the element qout
  double q_out = (*evaluation_data.qout_np)[ele->LID()];

  // if transport is flowing into the acinus
  if (q_out >= 0.0)
  {
    ele->getParams("Area", volumeMix_np(1));
  }
  // els if transport is flowing out of the acinus
  else
  {
    ele->getParams("Area", volumeMix_np(0));
    ele->getParams("Area", volumeMix_np(1));
  }

  // extra treatment if an acinus is not connected to anything else
  for (int i = 0; i < iel; i++)
  {
    if (ele->Nodes()[i]->NumElement() == 1) ele->getParams("Area", volumeMix_np(i));
  }
}


/*----------------------------------------------------------------------*
 |  calculate the scalar transport                          ismail 02/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::SolveScatra(RedAcinus* ele, Teuchos::ParameterList& params,
    DRT::Discretization& discretization, CORE::LINALG::SerialDenseVector& scatranp,
    CORE::LINALG::SerialDenseVector& volumeMix_np, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  Teuchos::RCP<const Epetra_Vector> volumeMix = discretization.GetState("junctionVolumeInMix");

  double volumenp = (*evaluation_data.acinar_vnp)[ele->LID()];
  double volumen = (*evaluation_data.acinar_vn)[ele->LID()];

  // extract local values from the global vectors
  std::vector<double> myvolmix(lm.size());
  DRT::UTILS::ExtractMyValues(*volumeMix, myvolmix, lm);
  // get area
  double area = myvolmix[1];

  // get the elements Qin and Qout
  double q_out = (*evaluation_data.qout_np)[ele->LID()];
  double q_in = (*evaluation_data.qin_np)[ele->LID()];
  double e1s = (*evaluation_data.e1scatran)[ele->LID()];
  double e2s = (*evaluation_data.e2scatran)[ele->LID()];

  // get time step size
  // const double dt = evaluation_data.dt;

  // get time
  const double time = evaluation_data.time;

  //--------------------------------------------------------------------
  // get element length
  //--------------------------------------------------------------------

  // evaluate velocity at nodes (1) and (2)
  double vel1 = q_in / area;
  double vel2 = q_out / area;

  CORE::LINALG::Matrix<2, 1> velv;
  velv(0, 0) = vel1;
  velv(1, 0) = vel2;
  // get average velocity
  double vel = vel2;

  //--------------------------------------------------------------------
  // if vel>=0 then node(2) is analytically evaluated;
  //  ---> node(1) is either prescribed or comes from the junction
  // if vel< 0 then node(1) is analytically evaluated;
  //  ---> node(2) is either prescribed or comes from the junction
  //--------------------------------------------------------------------

  if (vel >= 0.0)
  {
    // extrapolate the analytical solution
    int gid = ele->Id();
    double scnp = 0.0;
    scnp = (e2s * volumen + e1s * (volumenp - volumen)) / (volumenp);
    evaluation_data.e2scatranp->ReplaceGlobalValues(1, &scnp, &gid);
  }
  else
  {
    // extrapolate the analytical solution
    double scnp = 0.0;
    int gid = ele->Id();
    scnp = (e2s * volumen + e2s * (volumenp - volumen)) / (volumenp);
    {
      evaluation_data.e2scatranp->ReplaceGlobalValues(1, &scnp, &gid);
      evaluation_data.e1scatranp->ReplaceGlobalValues(1, &scnp, &gid);
    }
  }

  for (int i = 0; i < 2; i++)
  {
    if (ele->Nodes()[i]->GetCondition("RedAirwayPrescribedScatraCond") &&
        myrank == ele->Nodes()[i]->Owner())
    {
      double scnp = 0.0;
      DRT::Condition* condition = ele->Nodes()[i]->GetCondition("RedAirwayPrescribedScatraCond");
      // Get the type of prescribed bc

      const std::vector<int>* curve = condition->Get<std::vector<int>>("curve");
      double curvefac = 1.0;
      const std::vector<double>* vals = condition->Get<std::vector<double>>("val");

      // -----------------------------------------------------------------
      // Read in the value of the applied BC
      // -----------------------------------------------------------------
      int curvenum = -1;
      if (curve) curvenum = (*curve)[0];
      if (curvenum >= 0)
        curvefac =
            DRT::Problem::Instance()->FunctionById<DRT::UTILS::FunctionOfTime>(curvenum).Evaluate(
                time);

      scnp = (*vals)[0] * curvefac;

      const std::vector<int>* functions = condition->Get<std::vector<int>>("funct");
      int functnum = -1;
      if (functions)
        functnum = (*functions)[0];
      else
        functnum = -1;

      double functionfac = 0.0;
      if (functnum > 0)
      {
        functionfac = DRT::Problem::Instance()
                          ->FunctionById<DRT::UTILS::FunctionOfSpaceTime>(functnum - 1)
                          .Evaluate((ele->Nodes()[i])->X(), time, 0);
      }
      scnp += functionfac;

      // ----------------------------------------------------
      // convert O2 saturation to O2 concentration
      // ----------------------------------------------------
      // get O2 properties in air
      int id =
          DRT::Problem::Instance()->Materials()->FirstIdByType(INPAR::MAT::m_0d_o2_air_saturation);
      // check if O2 properties material exists
      if (id == -1)
      {
        dserror("A material defining O2 properties in air could not be found");
        exit(1);
      }
      const MAT::PAR::Parameter* smat = DRT::Problem::Instance()->Materials()->ParameterById(id);
      const MAT::PAR::Air_0d_O2_saturation* actmat =
          static_cast<const MAT::PAR::Air_0d_O2_saturation*>(smat);

      // get atmospheric pressure
      double patm = actmat->atmospheric_p_;
      // get number of O2 moles per unit volume of O2
      double nO2perVO2 = actmat->nO2_per_VO2_;
      // calculate the PO2 at nodes
      double pO2 = scnp * patm;
      // calculate VO2
      double vO2 = volumenp * (pO2 / patm);
      // evaluate initial concentration
      scnp = nO2perVO2 * vO2 / volumenp;
      //------------
      if (i == 0)
      {
        int gid = ele->Id();
        double val = scnp;
        if (vel < 0.0) val = (*evaluation_data.e1scatranp)[ele->LID()];
        //        if (ele->Owner()==myrank)
        {
          evaluation_data.e1scatranp->ReplaceGlobalValues(1, &val, &gid);
        }
        scatranp(0) = val * area;
      }
      else
      {
        int gid = ele->Id();
        double val = scnp;
        if (vel >= 0.0) val = (*evaluation_data.e2scatranp)[ele->LID()];
        //        if (ele->Owner()==myrank)
        {
          evaluation_data.e2scatranp->ReplaceGlobalValues(1, &val, &gid);
        }
        scatranp(1) = val * area;
      }
    }
  }


  {
    scatranp(1) = (*evaluation_data.e2scatranp)[ele->LID()] * area;
  }
  if (vel < 0.0)
  {
    scatranp(0) = (*evaluation_data.e1scatranp)[ele->LID()] * area;
  }
}  // SolveScatra



/*----------------------------------------------------------------------*
 |  calculate the scalar transport                          ismail 02/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::SolveScatraBifurcations(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    CORE::LINALG::SerialDenseVector& scatranp, CORE::LINALG::SerialDenseVector& volumeMix_np,
    std::vector<int>& lm, Teuchos::RCP<MAT::Material> material)
{
  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  Teuchos::RCP<const Epetra_Vector> scatran = discretization.GetState("scatranp");
  Teuchos::RCP<const Epetra_Vector> volumeMix = discretization.GetState("junctionVolumeInMix");

  // extract local values from the global vectors
  std::vector<double> myvolmix(lm.size());
  DRT::UTILS::ExtractMyValues(*volumeMix, myvolmix, lm);
  // get area
  double area = myvolmix[1];

  // get the elements Qin and Qout
  double q_out = (*evaluation_data.qout_np)[ele->LID()];
  double q_in = (*evaluation_data.qin_np)[ele->LID()];

  // extract local values from the global vectors
  std::vector<double> myscatran(lm.size());
  DRT::UTILS::ExtractMyValues(*scatran, myscatran, lm);

  // evaluate velocity at nodes (1) and (2)
  double vel1 = q_in / area;
  double vel2 = q_out / area;

  CORE::LINALG::Matrix<2, 1> velv;
  velv(0, 0) = vel1;
  velv(1, 0) = vel2;
  // get average velocity
  double vel = 0.5 * (vel1 + vel2);

  //--------------------------------------------------------------------
  // if vel>=0 then node(2) is analytically evaluated;
  //  ---> node(1) is either prescribed or comes from the junction
  // if vel< 0 then node(1) is analytically evaluated;
  //  ---> node(2) is either prescribed or comes from the junction
  //--------------------------------------------------------------------
  if (vel >= 0.0)
  {
    // extrapolate the analytical solution
    double scnp = myscatran[0];
    int gid = ele->Id();
    evaluation_data.e1scatranp->ReplaceGlobalValues(1, &scnp, &gid);
  }
  else
  {
    // extrapolate the analytical solution
    double scnp = myscatran[1];
    int gid = ele->Id();
    evaluation_data.e2scatranp->ReplaceGlobalValues(1, &scnp, &gid);
  }
}  // SolveScatraBifurcations


/*----------------------------------------------------------------------*
 |  calculate the scalar transport                          ismail 02/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::UpdateScatra(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  Teuchos::RCP<const Epetra_Vector> dscatranp = discretization.GetState("dscatranp");
  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // get flowrate
  double qin = (*evaluation_data.qin_np)[ele->LID()];

  // extract local values from the global vectors
  std::vector<double> mydscatra(lm.size());
  DRT::UTILS::ExtractMyValues(*dscatranp, mydscatra, lm);

  //--------------------------------------------------------------------
  // if vel>=0 then node(2) is analytically evaluated;
  //  ---> node(1) is either prescribed or comes from the junction
  // if vel< 0 then node(1) is analytically evaluated;
  //  ---> node(2) is either prescribed or comes from the junction
  //--------------------------------------------------------------------
  if (qin < 0.0)
  {
    int gid = lm[1];
    double val = mydscatra[1];
    if (myrank == ele->Nodes()[1]->Owner())
    {
      evaluation_data.dscatranp->ReplaceGlobalValues(1, &val, &gid);
    }
  }
}  // UpdateScatra


template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::UpdateElem12Scatra(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  Teuchos::RCP<const Epetra_Vector> scatranp = discretization.GetState("scatranp");
  Teuchos::RCP<const Epetra_Vector> dscatranp = discretization.GetState("dscatranp");
  Teuchos::RCP<const Epetra_Vector> volumeMix = discretization.GetState("junctionVolumeInMix");

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // extract local values from the global vectors
  std::vector<double> myscatranp(lm.size());
  DRT::UTILS::ExtractMyValues(*scatranp, myscatranp, lm);

  // extract local values from the global vectors
  std::vector<double> mydscatranp(lm.size());
  DRT::UTILS::ExtractMyValues(*dscatranp, mydscatranp, lm);

  // extract local values from the global vectors
  std::vector<double> myvolmix(lm.size());
  DRT::UTILS::ExtractMyValues(*volumeMix, myvolmix, lm);

  // get flowrate
  double qin = (*evaluation_data.qin_np)[ele->LID()];
  // Get the average concentration

  // ---------------------------------------------------------------------
  // element scatra must be updated only at the capillary nodes.
  // ---------------------------------------------------------------------
  //  double e2s = (*e2scatranp)[ele->LID()] + mydscatranp[1]*myvolmix[1];
  double e2s = myscatranp[1];

  int gid = ele->Id();
  evaluation_data.e2scatranp->ReplaceGlobalValues(1, &e2s, &gid);
  if (qin < 0.0)
  {
    evaluation_data.e1scatranp->ReplaceGlobalValues(1, &e2s, &gid);
  }
}



/*----------------------------------------------------------------------*
 |  calculate PO2 from concentration                        ismail 06/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::EvalPO2FromScatra(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  const int myrank = discretization.Comm().MyPID();


  // get Po2 vector
  Teuchos::RCP<const Epetra_Vector> scatran = discretization.GetState("scatranp");

  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  // -------------------------------------------------------------------
  // extract scatra values
  // -------------------------------------------------------------------
  // extract local values from the global vectors
  std::vector<double> myscatran(lm.size());
  DRT::UTILS::ExtractMyValues(*scatran, myscatran, lm);

  // -------------------------------------------------------------------
  // find out if the material type is Air or Blood
  // -------------------------------------------------------------------
  std::string fluidType = "none";
  // if RedAirwayScatraAirCond then material type is air
  if (ele->Nodes()[0]->GetCondition("RedAirwayScatraAirCond") != NULL &&
      ele->Nodes()[1]->GetCondition("RedAirwayScatraAirCond") != NULL)
  {
    fluidType = "air";
  }
  else
  {
    dserror("A scalar transport element must be defined either as \"air\"");
    exit(1);
  }

  // define a empty pO2 vector
  double pO2 = 0.0;

  // -------------------------------------------------------------------
  // Get O2 properties in air
  // -------------------------------------------------------------------
  if (fluidType == "air")
  {
    // -----------------------------------------------------------------
    // Get O2 properties in air
    // -----------------------------------------------------------------

    int id =
        DRT::Problem::Instance()->Materials()->FirstIdByType(INPAR::MAT::m_0d_o2_air_saturation);
    // check if O2 properties material exists
    if (id == -1)
    {
      dserror("A material defining O2 properties in air could not be found");
      exit(1);
    }
    const MAT::PAR::Parameter* smat = DRT::Problem::Instance()->Materials()->ParameterById(id);
    const MAT::PAR::Air_0d_O2_saturation* actmat =
        static_cast<const MAT::PAR::Air_0d_O2_saturation*>(smat);

    // get atmospheric pressure
    double patm = actmat->atmospheric_p_;
    // get number of O2 moles per unit volume of O2
    double nO2perVO2 = actmat->nO2_per_VO2_;

    // -----------------------------------------------------------------
    // Calculate Vo2 in air
    // -----------------------------------------------------------------
    // get airway volume
    double vAir = (*evaluation_data.acinar_vnp)[ele->LID()];
    // calculate the VO2 at nodes
    double vO2 = (vAir * myscatran[lm.size() - 1]) / nO2perVO2;
    // calculate PO2 at nodes
    pO2 = patm * vO2 / vAir;
  }
  else
  {
    dserror("A scalar transport element must be defined either as \"air\" or \"blood\"");
    exit(1);
  }

  // -------------------------------------------------------------------
  // Set element pO2 to PO2 vector
  // -------------------------------------------------------------------
  int gid = lm[lm.size() - 1];
  double val = pO2;
  if (myrank == ele->Nodes()[lm.size() - 1]->Owner())
  {
    evaluation_data.po2->ReplaceGlobalValues(1, &val, &gid);
  }

}  // EvalPO2FromScatra


/*----------------------------------------------------------------------*
 |  calculate essential nodal values                        ismail 06/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AcinusImpl<distype>::EvalNodalEssentialValues(RedAcinus* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    CORE::LINALG::SerialDenseVector& nodal_surface, CORE::LINALG::SerialDenseVector& nodal_volume,
    CORE::LINALG::SerialDenseVector& nodal_avg_scatra, std::vector<int>& lm,
    Teuchos::RCP<MAT::Material> material)
{
  // Get all general state vectors: flow, pressure,
  DRT::REDAIRWAYS::EvaluationData& evaluation_data = DRT::REDAIRWAYS::EvaluationData::get();

  Teuchos::RCP<const Epetra_Vector> scatranp = discretization.GetState("scatranp");

  // Extract scatra values
  // Extract local values from the global vectors
  std::vector<double> myscatranp(lm.size());
  DRT::UTILS::ExtractMyValues(*scatranp, myscatranp, lm);

  // Find the volume of an acinus
  // Get the current acinar volume
  double volAcinus = (*evaluation_data.acinar_v)[ele->LID()];
  // Set nodal volume
  nodal_volume[1] = volAcinus;

  // Find the average scalar transport concentration
  // Set nodal flowrate
  nodal_avg_scatra[0] = myscatranp[1];
  nodal_avg_scatra[1] = myscatranp[1];

  // Find the total gas exchange surface inside an acinus
  // get the initial volume of an acinus
  double volAcinus0;
  ele->getParams("AcinusVolume", volAcinus0);
  // get the initial volume of an alveolar duct
  double volAlvDuct0;
  ele->getParams("AlveolarDuctVolume", volAlvDuct0);
  // find the  number of alveolar duct
  const double numOfAlvDucts = double(floor(volAcinus0 / volAlvDuct0));
  // define the number of alveoli per alveolar duct
  const double nAlveoliPerAlveolarDuct = 36.0;
  // define the number of alveoli per duct
  const double nAlveoliPerDuct = 4.0;
  // find the volume of one alveolus
  const double volAlveolus = volAcinus / numOfAlvDucts / nAlveoliPerAlveolarDuct;
  // find the surface of one alveolus
  const double surfAlveolus =
      (6.0 + 12.0 * sqrt(3.0)) * pow(volAlveolus / (8.0 * sqrt(2.0)), 2.0 / 3.0);
  // get the length of an edge of an alveolus
  const double al = pow(volAlveolus / (8.0 * sqrt(2.0)), 1.0 / 3.0) / 3.0;
  // find the surface of an alveolar duct
  const double surfAlveolarDuct =
      (nAlveoliPerAlveolarDuct - 2.0 * nAlveoliPerDuct) * surfAlveolus + 6.0 * (al * al);
  // find the surface of an acinus
  const double surfAcinus = surfAlveolarDuct * numOfAlvDucts;
  // set nodal surface area
  nodal_surface[1] = surfAcinus;
}
