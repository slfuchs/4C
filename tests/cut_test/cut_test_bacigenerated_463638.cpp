/*----------------------------------------------------------------------*/
/*! \file
\brief Test for the CUT Library

\level 1

*----------------------------------------------------------------------*/
// This test was automatically generated by CUT::OUTPUT::GmshElementCutTest(),
// as the cut crashed for this configuration!

#include "4C_cut_combintersection.hpp"
#include "4C_cut_levelsetintersection.hpp"
#include "4C_cut_meshintersection.hpp"
#include "4C_cut_options.hpp"
#include "4C_cut_side.hpp"
#include "4C_cut_tetmeshintersection.hpp"
#include "4C_cut_volumecell.hpp"
#include "4C_discretization_fem_general_utils_local_connectivity_matrices.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "cut_test_utils.hpp"

void test_bacigenerated_463638()
{
  CORE::GEO::CUT::MeshIntersection intersection;
  intersection.GetOptions().Init_for_Cuttests();  // use full cln
  std::vector<int> nids;

  int sidecount = 0;
  std::vector<double> lsvs(8);
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05555;
    tri3_xyze(1, 0) = -0.237013;
    tri3_xyze(2, 0) = -0.0252117;
    nids.push_back(34594);
    tri3_xyze(0, 1) = 1.05562;
    tri3_xyze(1, 1) = -0.247872;
    tri3_xyze(2, 1) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 2) = 1.05551;
    tri3_xyze(1, 2) = -0.242467;
    tri3_xyze(2, 2) = -0.0264472;
    nids.push_back(-5740);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05547;
    tri3_xyze(1, 0) = -0.24792;
    tri3_xyze(2, 0) = -0.0276825;
    nids.push_back(34596);
    tri3_xyze(0, 1) = 1.05562;
    tri3_xyze(1, 1) = -0.247872;
    tri3_xyze(2, 1) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 2) = 1.05011;
    tri3_xyze(1, 2) = -0.247927;
    tri3_xyze(2, 2) = -0.0260514;
    nids.push_back(-5619);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05562;
    tri3_xyze(1, 0) = -0.247872;
    tri3_xyze(2, 0) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 1) = 1.04475;
    tri3_xyze(1, 1) = -0.247933;
    tri3_xyze(2, 1) = -0.0244203;
    nids.push_back(33841);
    tri3_xyze(0, 2) = 1.05011;
    tri3_xyze(1, 2) = -0.247927;
    tri3_xyze(2, 2) = -0.0260514;
    nids.push_back(-5619);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05569;
    tri3_xyze(1, 0) = -0.236965;
    tri3_xyze(2, 0) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 1) = 1.04483;
    tri3_xyze(1, 1) = -0.237024;
    tri3_xyze(2, 1) = -0.021949;
    nids.push_back(33843);
    tri3_xyze(0, 2) = 1.0503;
    tri3_xyze(1, 2) = -0.242425;
    tri3_xyze(2, 2) = -0.0221541;
    nids.push_back(-5618);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05562;
    tri3_xyze(1, 0) = -0.226108;
    tri3_xyze(2, 0) = -0.0227405;
    nids.push_back(34625);
    tri3_xyze(0, 1) = 1.05569;
    tri3_xyze(1, 1) = -0.236965;
    tri3_xyze(2, 1) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 2) = 1.05558;
    tri3_xyze(1, 2) = -0.231561;
    tri3_xyze(2, 2) = -0.0239762;
    nids.push_back(-5741);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05562;
    tri3_xyze(1, 0) = -0.247872;
    tri3_xyze(2, 0) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 1) = 1.05577;
    tri3_xyze(1, 1) = -0.247824;
    tri3_xyze(2, 1) = -0.0223594;
    nids.push_back(34592);
    tri3_xyze(0, 2) = 1.05026;
    tri3_xyze(1, 2) = -0.247879;
    tri3_xyze(2, 2) = -0.0233897;
    nids.push_back(-5617);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05577;
    tri3_xyze(1, 0) = -0.247824;
    tri3_xyze(2, 0) = -0.0223594;
    nids.push_back(34592);
    tri3_xyze(0, 1) = 1.0449;
    tri3_xyze(1, 1) = -0.247885;
    tri3_xyze(2, 1) = -0.0217585;
    nids.push_back(33840);
    tri3_xyze(0, 2) = 1.05026;
    tri3_xyze(1, 2) = -0.247879;
    tri3_xyze(2, 2) = -0.0233897;
    nids.push_back(-5617);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05562;
    tri3_xyze(1, 0) = -0.247872;
    tri3_xyze(2, 0) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 1) = 1.05555;
    tri3_xyze(1, 1) = -0.237013;
    tri3_xyze(2, 1) = -0.0252117;
    nids.push_back(34594);
    tri3_xyze(0, 2) = 1.05566;
    tri3_xyze(1, 2) = -0.242419;
    tri3_xyze(2, 2) = -0.0237854;
    nids.push_back(-5739);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05555;
    tri3_xyze(1, 0) = -0.237013;
    tri3_xyze(2, 0) = -0.0252117;
    nids.push_back(34594);
    tri3_xyze(0, 1) = 1.05569;
    tri3_xyze(1, 1) = -0.236965;
    tri3_xyze(2, 1) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 2) = 1.05566;
    tri3_xyze(1, 2) = -0.242419;
    tri3_xyze(2, 2) = -0.0237854;
    nids.push_back(-5739);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05569;
    tri3_xyze(1, 0) = -0.236965;
    tri3_xyze(2, 0) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 1) = 1.05577;
    tri3_xyze(1, 1) = -0.247824;
    tri3_xyze(2, 1) = -0.0223594;
    nids.push_back(34592);
    tri3_xyze(0, 2) = 1.05566;
    tri3_xyze(1, 2) = -0.242419;
    tri3_xyze(2, 2) = -0.0237854;
    nids.push_back(-5739);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05562;
    tri3_xyze(1, 0) = -0.247872;
    tri3_xyze(2, 0) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 1) = 1.05547;
    tri3_xyze(1, 1) = -0.24792;
    tri3_xyze(2, 1) = -0.0276825;
    nids.push_back(34596);
    tri3_xyze(0, 2) = 1.05551;
    tri3_xyze(1, 2) = -0.242467;
    tri3_xyze(2, 2) = -0.0264472;
    nids.push_back(-5740);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.04483;
    tri3_xyze(1, 0) = -0.237024;
    tri3_xyze(2, 0) = -0.021949;
    nids.push_back(33843);
    tri3_xyze(0, 1) = 1.05569;
    tri3_xyze(1, 1) = -0.236965;
    tri3_xyze(2, 1) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 2) = 1.05023;
    tri3_xyze(1, 2) = -0.231565;
    tri3_xyze(2, 2) = -0.0223446;
    nids.push_back(-5620);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05569;
    tri3_xyze(1, 0) = -0.236965;
    tri3_xyze(2, 0) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 1) = 1.05562;
    tri3_xyze(1, 1) = -0.226108;
    tri3_xyze(2, 1) = -0.0227405;
    nids.push_back(34625);
    tri3_xyze(0, 2) = 1.05023;
    tri3_xyze(1, 2) = -0.231565;
    tri3_xyze(2, 2) = -0.0223446;
    nids.push_back(-5620);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.0449;
    tri3_xyze(1, 0) = -0.247885;
    tri3_xyze(2, 0) = -0.0217585;
    nids.push_back(33840);
    tri3_xyze(0, 1) = 1.05577;
    tri3_xyze(1, 1) = -0.247824;
    tri3_xyze(2, 1) = -0.0223594;
    nids.push_back(34592);
    tri3_xyze(0, 2) = 1.0503;
    tri3_xyze(1, 2) = -0.242425;
    tri3_xyze(2, 2) = -0.0221541;
    nids.push_back(-5618);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05577;
    tri3_xyze(1, 0) = -0.247824;
    tri3_xyze(2, 0) = -0.0223594;
    nids.push_back(34592);
    tri3_xyze(0, 1) = 1.05569;
    tri3_xyze(1, 1) = -0.236965;
    tri3_xyze(2, 1) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 2) = 1.0503;
    tri3_xyze(1, 2) = -0.242425;
    tri3_xyze(2, 2) = -0.0221541;
    nids.push_back(-5618);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.04475;
    tri3_xyze(1, 0) = -0.247933;
    tri3_xyze(2, 0) = -0.0244203;
    nids.push_back(33841);
    tri3_xyze(0, 1) = 1.05562;
    tri3_xyze(1, 1) = -0.247872;
    tri3_xyze(2, 1) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 2) = 1.05026;
    tri3_xyze(1, 2) = -0.247879;
    tri3_xyze(2, 2) = -0.0233897;
    nids.push_back(-5617);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05569;
    tri3_xyze(1, 0) = -0.236965;
    tri3_xyze(2, 0) = -0.0225497;
    nids.push_back(34595);
    tri3_xyze(0, 1) = 1.05555;
    tri3_xyze(1, 1) = -0.237013;
    tri3_xyze(2, 1) = -0.0252117;
    nids.push_back(34594);
    tri3_xyze(0, 2) = 1.05558;
    tri3_xyze(1, 2) = -0.231561;
    tri3_xyze(2, 2) = -0.0239762;
    nids.push_back(-5741);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05555;
    tri3_xyze(1, 0) = -0.237013;
    tri3_xyze(2, 0) = -0.0252117;
    nids.push_back(34594);
    tri3_xyze(0, 1) = 1.05548;
    tri3_xyze(1, 1) = -0.226156;
    tri3_xyze(2, 1) = -0.025403;
    nids.push_back(34624);
    tri3_xyze(0, 2) = 1.05558;
    tri3_xyze(1, 2) = -0.231561;
    tri3_xyze(2, 2) = -0.0239762;
    nids.push_back(-5741);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.05577;
    tri3_xyze(1, 0) = -0.247824;
    tri3_xyze(2, 0) = -0.0223594;
    nids.push_back(34592);
    tri3_xyze(0, 1) = 1.05562;
    tri3_xyze(1, 1) = -0.247872;
    tri3_xyze(2, 1) = -0.0250208;
    nids.push_back(34593);
    tri3_xyze(0, 2) = 1.05566;
    tri3_xyze(1, 2) = -0.242419;
    tri3_xyze(2, 2) = -0.0237854;
    nids.push_back(-5739);
    intersection.AddCutSide(++sidecount, nids, tri3_xyze, CORE::FE::CellType::tri3);
  }
  {
    CORE::LINALG::SerialDenseMatrix hex8_xyze(3, 8);

    nids.clear();
    hex8_xyze(0, 0) = 1.05556;
    hex8_xyze(1, 0) = -0.255556;
    hex8_xyze(2, 0) = -0.00555556;
    nids.push_back(1489582);
    hex8_xyze(0, 1) = 1.05556;
    hex8_xyze(1, 1) = -0.255556;
    hex8_xyze(2, 1) = -0.0166667;
    nids.push_back(1489583);
    hex8_xyze(0, 2) = 1.05556;
    hex8_xyze(1, 2) = -0.244444;
    hex8_xyze(2, 2) = -0.0166667;
    nids.push_back(1489586);
    hex8_xyze(0, 3) = 1.05556;
    hex8_xyze(1, 3) = -0.244444;
    hex8_xyze(2, 3) = -0.00555556;
    nids.push_back(1489585);
    hex8_xyze(0, 4) = 1.06667;
    hex8_xyze(1, 4) = -0.255556;
    hex8_xyze(2, 4) = -0.00555556;
    nids.push_back(1489591);
    hex8_xyze(0, 5) = 1.06667;
    hex8_xyze(1, 5) = -0.255556;
    hex8_xyze(2, 5) = -0.0166667;
    nids.push_back(1489592);
    hex8_xyze(0, 6) = 1.06667;
    hex8_xyze(1, 6) = -0.244444;
    hex8_xyze(2, 6) = -0.0166667;
    nids.push_back(1489595);
    hex8_xyze(0, 7) = 1.06667;
    hex8_xyze(1, 7) = -0.244444;
    hex8_xyze(2, 7) = -0.00555556;
    nids.push_back(1489594);

    intersection.AddElement(463424, nids, hex8_xyze, CORE::FE::CellType::hex8);
  }

  {
    CORE::LINALG::SerialDenseMatrix hex8_xyze(3, 8);

    nids.clear();
    hex8_xyze(0, 0) = 1.05556;
    hex8_xyze(1, 0) = -0.244444;
    hex8_xyze(2, 0) = -0.0166667;
    nids.push_back(1489586);
    hex8_xyze(0, 1) = 1.05556;
    hex8_xyze(1, 1) = -0.244444;
    hex8_xyze(2, 1) = -0.0277778;
    nids.push_back(1489809);
    hex8_xyze(0, 2) = 1.05556;
    hex8_xyze(1, 2) = -0.233333;
    hex8_xyze(2, 2) = -0.0277778;
    nids.push_back(1489812);
    hex8_xyze(0, 3) = 1.05556;
    hex8_xyze(1, 3) = -0.233333;
    hex8_xyze(2, 3) = -0.0166667;
    nids.push_back(1489589);
    hex8_xyze(0, 4) = 1.06667;
    hex8_xyze(1, 4) = -0.244444;
    hex8_xyze(2, 4) = -0.0166667;
    nids.push_back(1489595);
    hex8_xyze(0, 5) = 1.06667;
    hex8_xyze(1, 5) = -0.244444;
    hex8_xyze(2, 5) = -0.0277778;
    nids.push_back(1489818);
    hex8_xyze(0, 6) = 1.06667;
    hex8_xyze(1, 6) = -0.233333;
    hex8_xyze(2, 6) = -0.0277778;
    nids.push_back(1489821);
    hex8_xyze(0, 7) = 1.06667;
    hex8_xyze(1, 7) = -0.233333;
    hex8_xyze(2, 7) = -0.0166667;
    nids.push_back(1489598);

    intersection.AddElement(463641, nids, hex8_xyze, CORE::FE::CellType::hex8);
  }

  {
    CORE::LINALG::SerialDenseMatrix hex8_xyze(3, 8);

    nids.clear();
    hex8_xyze(0, 0) = 1.05556;
    hex8_xyze(1, 0) = -0.255556;
    hex8_xyze(2, 0) = -0.0166667;
    nids.push_back(1489583);
    hex8_xyze(0, 1) = 1.05556;
    hex8_xyze(1, 1) = -0.255556;
    hex8_xyze(2, 1) = -0.0277778;
    nids.push_back(1489806);
    hex8_xyze(0, 2) = 1.05556;
    hex8_xyze(1, 2) = -0.244444;
    hex8_xyze(2, 2) = -0.0277778;
    nids.push_back(1489809);
    hex8_xyze(0, 3) = 1.05556;
    hex8_xyze(1, 3) = -0.244444;
    hex8_xyze(2, 3) = -0.0166667;
    nids.push_back(1489586);
    hex8_xyze(0, 4) = 1.06667;
    hex8_xyze(1, 4) = -0.255556;
    hex8_xyze(2, 4) = -0.0166667;
    nids.push_back(1489592);
    hex8_xyze(0, 5) = 1.06667;
    hex8_xyze(1, 5) = -0.255556;
    hex8_xyze(2, 5) = -0.0277778;
    nids.push_back(1489815);
    hex8_xyze(0, 6) = 1.06667;
    hex8_xyze(1, 6) = -0.244444;
    hex8_xyze(2, 6) = -0.0277778;
    nids.push_back(1489818);
    hex8_xyze(0, 7) = 1.06667;
    hex8_xyze(1, 7) = -0.244444;
    hex8_xyze(2, 7) = -0.0166667;
    nids.push_back(1489595);

    intersection.AddElement(463638, nids, hex8_xyze, CORE::FE::CellType::hex8);
  }

  {
    CORE::LINALG::SerialDenseMatrix hex8_xyze(3, 8);

    nids.clear();
    hex8_xyze(0, 0) = 1.05556;
    hex8_xyze(1, 0) = -0.255556;
    hex8_xyze(2, 0) = -0.0277778;
    nids.push_back(1489806);
    hex8_xyze(0, 1) = 1.05556;
    hex8_xyze(1, 1) = -0.255556;
    hex8_xyze(2, 1) = -0.0388889;
    nids.push_back(1489807);
    hex8_xyze(0, 2) = 1.05556;
    hex8_xyze(1, 2) = -0.244444;
    hex8_xyze(2, 2) = -0.0388889;
    nids.push_back(1489810);
    hex8_xyze(0, 3) = 1.05556;
    hex8_xyze(1, 3) = -0.244444;
    hex8_xyze(2, 3) = -0.0277778;
    nids.push_back(1489809);
    hex8_xyze(0, 4) = 1.06667;
    hex8_xyze(1, 4) = -0.255556;
    hex8_xyze(2, 4) = -0.0277778;
    nids.push_back(1489815);
    hex8_xyze(0, 5) = 1.06667;
    hex8_xyze(1, 5) = -0.255556;
    hex8_xyze(2, 5) = -0.0388889;
    nids.push_back(1489816);
    hex8_xyze(0, 6) = 1.06667;
    hex8_xyze(1, 6) = -0.244444;
    hex8_xyze(2, 6) = -0.0388889;
    nids.push_back(1489819);
    hex8_xyze(0, 7) = 1.06667;
    hex8_xyze(1, 7) = -0.244444;
    hex8_xyze(2, 7) = -0.0277778;
    nids.push_back(1489818);

    intersection.AddElement(463639, nids, hex8_xyze, CORE::FE::CellType::hex8);
  }

  intersection.CutTest_Cut(
      true, INPAR::CUT::VCellGaussPts_DirectDivergence, INPAR::CUT::BCellGaussPts_Tessellation);
  intersection.Cut_Finalize(true, INPAR::CUT::VCellGaussPts_DirectDivergence,
      INPAR::CUT::BCellGaussPts_Tessellation, false, true);
}
