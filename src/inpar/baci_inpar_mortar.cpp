/*----------------------------------------------------------------------*/
/*! \file

\brief Input parameters for mortar coupling

\level 1

*/
/*----------------------------------------------------------------------*/



#include "baci_inpar_validparameters.H"
#include "baci_inpar_mortar.H"
#include "baci_lib_conditiondefinition.H"



void INPAR::MORTAR::SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list)
{
  using namespace DRT::INPUT;
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  /* parameters for mortar coupling */
  Teuchos::ParameterList& mortar = list->sublist("MORTAR COUPLING", false, "");

  setStringToIntegralParameter<int>("LM_SHAPEFCN", "Dual",
      "Type of employed set of shape functions",
      tuple<std::string>(
          "Dual", "dual", "Standard", "standard", "std", "PetrovGalerkin", "petrovgalerkin", "pg"),
      tuple<int>(shape_dual, shape_dual, shape_standard, shape_standard, shape_standard,
          shape_petrovgalerkin, shape_petrovgalerkin, shape_petrovgalerkin),
      &mortar);

  setStringToIntegralParameter<int>("SEARCH_ALGORITHM", "Binarytree", "Type of contact search",
      tuple<std::string>("BruteForce", "bruteforce", "BruteForceEleBased", "bruteforceelebased",
          "BinaryTree", "Binarytree", "binarytree"),
      tuple<int>(search_bfele, search_bfele, search_bfele, search_bfele, search_binarytree,
          search_binarytree, search_binarytree),
      &mortar);

  setStringToIntegralParameter<int>("BINARYTREE_UPDATETYPE", "BottomUp",
      "Type of binary tree update, which is either a bottom up or a top down approach.",
      tuple<std::string>("BottomUp", "TopDown"),
      tuple<int>(binarytree_bottom_up, binarytree_top_down), &mortar);

  DoubleParameter(
      "SEARCH_PARAM", 0.3, "Radius / Bounding volume inflation for contact search", &mortar);

  BoolParameter("SEARCH_USE_AUX_POS", "Yes",
      "If chosen auxiliary position is used for computing dops", &mortar);

  setStringToIntegralParameter<int>("LM_QUAD", "undefined",
      "Type of LM interpolation for quadratic FE",
      tuple<std::string>(
          "undefined", "quad", "quadratic", "pwlin", "piecewiselinear", "lin", "linear", "const"),
      tuple<int>(lagmult_undefined, lagmult_quad, lagmult_quad, lagmult_pwlin, lagmult_pwlin,
          lagmult_lin, lagmult_lin, lagmult_const),
      &mortar);

  BoolParameter("CROSSPOINTS", "No",
      "If chosen, multipliers are removed from crosspoints / edge nodes", &mortar);

  setStringToIntegralParameter<int>("LM_DUAL_CONSISTENT", "boundary",
      "For which elements should the dual basis be calculated on EXACTLY the same GPs as the "
      "contact terms",
      tuple<std::string>("none", "boundary", "all"),
      tuple<int>(consistent_none, consistent_boundary, consistent_all), &mortar);

  setStringToIntegralParameter<int>("MESH_RELOCATION", "Initial", "Type of mesh relocation",
      tuple<std::string>("Initial", "initial", "Every_Timestep", "every_timestep", "No", "no"),
      tuple<int>(relocation_initial, relocation_initial, relocation_timestep, relocation_timestep,
          relocation_none, relocation_none),
      &mortar);

  setStringToIntegralParameter<int>("ALGORITHM", "Mortar", "Type of meshtying/contact algorithm",
      tuple<std::string>("mortar", "Mortar", "nts", "NTS", "gpts", "GPTS", "lts", "LTS", "ltl",
          "LTL", "stl", "STL"),
      tuple<int>(algorithm_mortar, algorithm_mortar, algorithm_nts, algorithm_nts, algorithm_gpts,
          algorithm_gpts, algorithm_lts, algorithm_lts, algorithm_ltl, algorithm_ltl, algorithm_stl,
          algorithm_stl),
      &mortar);

  setStringToIntegralParameter<int>("INTTYPE", "Segments", "Type of numerical integration scheme",
      tuple<std::string>(
          "Segments", "segments", "Elements", "elements", "Elements_BS", "elements_BS"),
      tuple<int>(inttype_segments, inttype_segments, inttype_elements, inttype_elements,
          inttype_elements_BS, inttype_elements_BS),
      &mortar);

  IntParameter("NUMGP_PER_DIM", 0, "Number of employed integration points per dimension", &mortar);

  setStringToIntegralParameter<int>("TRIANGULATION", "Delaunay",
      "Type of triangulation for segment-based integration",
      tuple<std::string>("Delaunay", "delaunay", "Center", "center"),
      tuple<int>(triangulation_delaunay, triangulation_delaunay, triangulation_center,
          triangulation_center),
      &mortar);

  BoolParameter("RESTART_WITH_MESHTYING", "No",
      "Must be chosen if a non-meshtying simulation is to be restarted with meshtying", &mortar);

  BoolParameter("OUTPUT_INTERFACES", "No",
      "Write output for each mortar interface separately.\nThis is an additional feature, purely "
      "to enhance visualization. Currently, this is limited to solid meshtying and contact w/o "
      "friction.",
      &mortar);

  /*--------------------------------------------------------------------*/
  // parameters for parallel redistribution of mortar interfaces
  Teuchos::ParameterList& parallelRedist = mortar.sublist("PARALLEL REDISTRIBUTION", false,
      "Parameters to control parallel redistribution of mortar interfaces");

  BoolParameter("EXPLOIT_PROXIMITY", "Yes",
      "Exploit information on geometric proximity to split slave interface into close and "
      "non-close parts and redistribute them independently. [Contact only]",
      &parallelRedist);

  setStringToIntegralParameter<ExtendGhosting>("GHOSTING_STRATEGY", "redundant_master",
      "Type of interface ghosting and ghosting extension algorithm",
      tuple<std::string>("redundant_all", "redundant_master", "round_robin", "binning"),
      tuple<ExtendGhosting>(ExtendGhosting::redundant_all, ExtendGhosting::redundant_master,
          ExtendGhosting::roundrobin, ExtendGhosting::binning),
      &parallelRedist);

  DoubleParameter("IMBALANCE_TOL", 1.1,
      "Max. relative imbalance of subdomain size after redistribution", &parallelRedist);

  DoubleParameter("MAX_BALANCE_EVAL_TIME", 2.0,
      "Max-to-min ratio of contact evalation time per processor to triggger parallel "
      "redistribution",
      &parallelRedist);

  DoubleParameter("MAX_BALANCE_SLAVE_ELES", 0.5,
      "Max-to-min ratio of mortar slave elements per processor to triggger parallel "
      "redistribution",
      &parallelRedist);

  IntParameter("MIN_ELEPROC", 0,
      "Minimum no. of elements per processor for parallel redistribution", &parallelRedist);

  setStringToIntegralParameter<ParallelRedist>("PARALLEL_REDIST", "Static",
      "Type of redistribution algorithm",
      tuple<std::string>("None", "none", "No", "no", "Static", "static", "Dynamic", "dynamic"),
      tuple<ParallelRedist>(ParallelRedist::redist_none, ParallelRedist::redist_none,
          ParallelRedist::redist_none, ParallelRedist::redist_none, ParallelRedist::redist_static,
          ParallelRedist::redist_static, ParallelRedist::redist_dynamic,
          ParallelRedist::redist_dynamic),
      &parallelRedist);

  BoolParameter("PRINT_DISTRIBUTION", "Yes",
      "Print details of the parallel distribution, i.e. number of nodes/elements for each rank.",
      &parallelRedist);
}

void INPAR::MORTAR::SetValidConditions(
    std::vector<Teuchos::RCP<DRT::INPUT::ConditionDefinition>>& condlist)
{
  using namespace DRT::INPUT;
  /*--------------------------------------------------------------------*/
  // mortar contact

  std::vector<Teuchos::RCP<ConditionComponent>> contactcomponents;

  contactcomponents.push_back(Teuchos::rcp(new IntConditionComponent("Interface ID")));
  contactcomponents.push_back(Teuchos::rcp(new StringConditionComponent("Side", "Master",
      Teuchos::tuple<std::string>("Master", "Slave", "Selfcontact"),
      Teuchos::tuple<std::string>("Master", "Slave", "Selfcontact"))));
  contactcomponents.push_back(Teuchos::rcp(new StringConditionComponent("Initialization",
      "Inactive", Teuchos::tuple<std::string>("Inactive", "Active"),
      Teuchos::tuple<std::string>("Inactive", "Active"), true)));

  contactcomponents.push_back(
      Teuchos::rcp(new SeparatorConditionComponent("FrCoeffOrBound", true)));
  contactcomponents.push_back(Teuchos::rcp(new RealConditionComponent("FrCoeffOrBound")));

  contactcomponents.push_back(Teuchos::rcp(new SeparatorConditionComponent("AdhesionBound", true)));
  contactcomponents.push_back(Teuchos::rcp(new RealConditionComponent("AdhesionBound")));

  contactcomponents.push_back(
      Teuchos::rcp(new StringConditionComponent("Application", "Solidcontact",
          Teuchos::tuple<std::string>("Solidcontact", "Beamtosolidcontact", "Beamtosolidmeshtying"),
          Teuchos::tuple<std::string>("Solidcontact", "Beamtosolidcontact", "Beamtosolidmeshtying"),
          true)));

  // optional DBC handling
  contactcomponents.push_back(Teuchos::rcp(new StringConditionComponent("dbc_handling", "DoNothing",
      Teuchos::tuple<std::string>("DoNothing", "RemoveDBCSlaveNodes"),
      Teuchos::tuple<int>(static_cast<int>(DBCHandling::do_nothing),
          static_cast<int>(DBCHandling::remove_dbc_nodes_from_slave_side)),
      true)));

  // optional two half pass approach
  contactcomponents.push_back(Teuchos::rcp(new SeparatorConditionComponent("TwoHalfPass", true)));
  contactcomponents.push_back(Teuchos::rcp(new RealConditionComponent("TwoHalfPass")));

  // optional reference configuration check for non-smooth self contact surfaces
  contactcomponents.push_back(Teuchos::rcp(
      new SeparatorConditionComponent("RefConfCheckNonSmoothSelfContactSurface", true)));
  contactcomponents.push_back(
      Teuchos::rcp(new RealConditionComponent("RefConfCheckNonSmoothSelfContactSurface")));

  contactcomponents.push_back(
      Teuchos::rcp(new SeparatorConditionComponent("ConstitutiveLawID", true)));
  contactcomponents.push_back(
      Teuchos::rcp(new IntConditionComponent("ConstitutiveLawID", false, true, true)));

  Teuchos::RCP<ConditionDefinition> linecontact =
      Teuchos::rcp(new ConditionDefinition("DESIGN LINE MORTAR CONTACT CONDITIONS 2D", "Contact",
          "Line Contact Coupling", DRT::Condition::Contact, true, DRT::Condition::Line));
  Teuchos::RCP<ConditionDefinition> surfcontact =
      Teuchos::rcp(new ConditionDefinition("DESIGN SURF MORTAR CONTACT CONDITIONS 3D", "Contact",
          "Surface Contact Coupling", DRT::Condition::Contact, true, DRT::Condition::Surface));

  for (unsigned i = 0; i < contactcomponents.size(); ++i)
  {
    linecontact->AddComponent(contactcomponents[i]);
    surfcontact->AddComponent(contactcomponents[i]);
  }

  condlist.push_back(linecontact);
  condlist.push_back(surfcontact);

  /*--------------------------------------------------------------------*/
  // mortar coupling (for ALL kinds of interface problems except contact)

  std::vector<Teuchos::RCP<ConditionComponent>> mortarcomponents;

  mortarcomponents.push_back(Teuchos::rcp(new IntConditionComponent("Interface ID")));
  mortarcomponents.push_back(Teuchos::rcp(
      new StringConditionComponent("Side", "Master", Teuchos::tuple<std::string>("Master", "Slave"),
          Teuchos::tuple<std::string>("Master", "Slave"))));
  mortarcomponents.push_back(Teuchos::rcp(new StringConditionComponent("Initialization", "Inactive",
      Teuchos::tuple<std::string>("Inactive", "Active"),
      Teuchos::tuple<std::string>("Inactive", "Active"), true)));

  Teuchos::RCP<ConditionDefinition> linemortar =
      Teuchos::rcp(new ConditionDefinition("DESIGN LINE MORTAR COUPLING CONDITIONS 2D", "Mortar",
          "Line Mortar Coupling", DRT::Condition::Mortar, true, DRT::Condition::Line));
  Teuchos::RCP<ConditionDefinition> surfmortar =
      Teuchos::rcp(new ConditionDefinition("DESIGN SURF MORTAR COUPLING CONDITIONS 3D", "Mortar",
          "Surface Mortar Coupling", DRT::Condition::Mortar, true, DRT::Condition::Surface));

  for (unsigned i = 0; i < mortarcomponents.size(); ++i)
  {
    linemortar->AddComponent(mortarcomponents[i]);
    surfmortar->AddComponent(mortarcomponents[i]);
  }

  condlist.push_back(linemortar);
  condlist.push_back(surfmortar);

  /*--------------------------------------------------------------------*/
  // mortar coupling symmetry condition

  std::vector<Teuchos::RCP<ConditionComponent>> mrtrsymcomponents;
  mrtrsymcomponents.push_back(Teuchos::rcp(new SeparatorConditionComponent("ONOFF")));
  mrtrsymcomponents.push_back(Teuchos::rcp(new IntVectorConditionComponent("onoff", 3)));

  Teuchos::RCP<ConditionDefinition> linemrtrsym =
      Teuchos::rcp(new ConditionDefinition("DESIGN LINE MORTAR SYMMETRY CONDITIONS 3D", "mrtrsym",
          "Symmetry plane normal for 3D contact", DRT::Condition::LineMrtrSym, true,
          DRT::Condition::Line));

  Teuchos::RCP<ConditionDefinition> pointmrtrsym =
      Teuchos::rcp(new ConditionDefinition("DESIGN POINT MORTAR SYMMETRY CONDITIONS 2D/3D",
          "mrtrsym", "Symmetry plane normal for 2D/3D contact", DRT::Condition::PointMrtrSym, true,
          DRT::Condition::Point));

  for (unsigned i = 0; i < mrtrsymcomponents.size(); ++i)
  {
    linemrtrsym->AddComponent(mrtrsymcomponents[i]);
    pointmrtrsym->AddComponent(mrtrsymcomponents[i]);
  }

  condlist.push_back(linemrtrsym);
  condlist.push_back(pointmrtrsym);

  /*--------------------------------------------------------------------*/
  // mortar edge/corner condition

  Teuchos::RCP<ConditionDefinition> edgemrtr =
      Teuchos::rcp(new ConditionDefinition("DESIGN LINE MORTAR EDGE CONDITIONS 3D", "mrtredge",
          "Geometrical edge for 3D contact", DRT::Condition::EdgeMrtr, true, DRT::Condition::Line));

  Teuchos::RCP<ConditionDefinition> cornermrtr =
      Teuchos::rcp(new ConditionDefinition("DESIGN POINT MORTAR CORNER CONDITIONS 2D/3D",
          "mrtrcorner", "Geometrical corner for 2D/3D contact", DRT::Condition::CornerMrtr, true,
          DRT::Condition::Point));

  condlist.push_back(edgemrtr);
  condlist.push_back(cornermrtr);


  {
    /*--------------------------------------------------------------------*/
    // mortar coupling (for ALL kinds of interface problems except contact)
    std::vector<Teuchos::RCP<ConditionComponent>> mortarcomponents;

    mortarcomponents.push_back(Teuchos::rcp(new IntConditionComponent("Interface ID")));
    mortarcomponents.push_back(Teuchos::rcp(new StringConditionComponent("Side", "Master",
        Teuchos::tuple<std::string>("Master", "Slave"),
        Teuchos::tuple<std::string>("Master", "Slave"))));
    mortarcomponents.push_back(Teuchos::rcp(new StringConditionComponent("Initialization",
        "Inactive", Teuchos::tuple<std::string>("Inactive", "Active"),
        Teuchos::tuple<std::string>("Inactive", "Active"), true)));

    Teuchos::RCP<ConditionDefinition> linemortar = Teuchos::rcp(
        new ConditionDefinition("DESIGN LINE MORTAR MULTI-COUPLING CONDITIONS 2D", "MortarMulti",
            "Line Mortar Multi-Coupling", DRT::Condition::MortarMulti, true, DRT::Condition::Line));
    Teuchos::RCP<ConditionDefinition> surfmortar =
        Teuchos::rcp(new ConditionDefinition("DESIGN SURF MORTAR MULTI-COUPLING CONDITIONS 3D",
            "MortarMulti", "Surface Mortar Multi-Coupling", DRT::Condition::MortarMulti, true,
            DRT::Condition::Surface));

    for (unsigned i = 0; i < mortarcomponents.size(); ++i)
    {
      linemortar->AddComponent(mortarcomponents[i]);
      surfmortar->AddComponent(mortarcomponents[i]);
    }

    condlist.push_back(linemortar);
    condlist.push_back(surfmortar);
  }
}
