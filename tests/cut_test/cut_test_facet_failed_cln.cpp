// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_cut_combintersection.hpp"
#include "4C_cut_levelsetintersection.hpp"
#include "4C_cut_meshintersection.hpp"
#include "4C_cut_options.hpp"
#include "4C_cut_side.hpp"
#include "4C_cut_tetmeshintersection.hpp"
#include "4C_cut_volumecell.hpp"
#include "4C_fem_general_utils_local_connectivity_matrices.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "cut_test_utils.hpp"

void test_generated_1901()
{
  Cut::MeshIntersection intersection;
  intersection.get_options().init_for_cuttests();  // use full cln
  std::vector<int> nids;

  int sidecount = 0;
  std::vector<double> lsvs(8);
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = -0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-1);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 1) = -0.05;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-1);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.05;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 1) = -0.0482963;
    tri3_xyze(1, 1) = -0.012941;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2541);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-1);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = -0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2541);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = -0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-1);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = -0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = -0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-5);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = -0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-5);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-5);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = -0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-5);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = -0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = -0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = -0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-6);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = -0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = -0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = -0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-6);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = -0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = -0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = -0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-6);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = -0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = -0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = -0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-6);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.05;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-7);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-7);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 1) = -0.0482963;
    tri3_xyze(1, 1) = 0.012941;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-7);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0433013;
    tri3_xyze(1, 0) = -0.025;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2581);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = -0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = -0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-2);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = -0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = -0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = -0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-2);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = -0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2542);
    tri3_xyze(0, 1) = -0.0482963;
    tri3_xyze(1, 1) = -0.012941;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2541);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = -0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-2);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = 0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 1) = -0.05;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-7);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = 1.38494e-13;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 1) = -0.05;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-8);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.05;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 1) = -0.0482963;
    tri3_xyze(1, 1) = 0.012941;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-8);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = 0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-8);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = 1.38494e-13;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = 0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-8);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-9);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-9);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-9);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2546);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-9);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = -0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = -0.0298809;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2553);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = -0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-10);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = -0.0298809;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2553);
    tri3_xyze(0, 1) = -0.115451;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = -0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-10);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.115451;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = -0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-10);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = -0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = -0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-10);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = -0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 1) = -0.0999834;
    tri3_xyze(1, 1) = -0.0577254;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2587);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = -0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-11);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = -0.0577254;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2587);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = -0.0298809;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2553);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = -0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-11);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = -0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2541);
    tri3_xyze(0, 1) = -0.0433013;
    tri3_xyze(1, 1) = -0.025;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2581);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = -0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-2);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = -0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2541);
    tri3_xyze(0, 1) = -0.05;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-3);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.05;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2545);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = 1.38494e-13;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-3);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = 1.38494e-13;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = -0.0154125;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2577);
    tri3_xyze(0, 2) = -0.0538414;
    tri3_xyze(1, 2) = -0.00708835;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-3);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = -0.0298809;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2553);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = -0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = -0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-11);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = -0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2549);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = -0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = -0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-11);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 1) = -0.115451;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-12);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.115451;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = 0.0298809;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-12);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = 0.0298809;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-12);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2551);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-12);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.135665;
    tri3_xyze(1, 0) = -0.0363514;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2557);
    tri3_xyze(0, 1) = -0.140451;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 2) = -0.125771;
    tri3_xyze(1, 2) = -0.0165581;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-13);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.140451;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 1) = -0.115451;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 2) = -0.125771;
    tri3_xyze(1, 2) = -0.0165581;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-13);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.115451;
    tri3_xyze(1, 0) = -2.24087e-13;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 1) = -0.140451;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 2) = -0.125771;
    tri3_xyze(1, 2) = 0.0165581;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-15);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.140451;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 1) = -0.135665;
    tri3_xyze(1, 1) = 0.0363514;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 2) = -0.125771;
    tri3_xyze(1, 2) = 0.0165581;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-15);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.135665;
    tri3_xyze(1, 0) = 0.0363514;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = 0.0298809;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 2) = -0.125771;
    tri3_xyze(1, 2) = 0.0165581;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-15);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = 0.0298809;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 1) = -0.115451;
    tri3_xyze(1, 1) = -2.24087e-13;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2555);
    tri3_xyze(0, 2) = -0.125771;
    tri3_xyze(1, 2) = 0.0165581;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-15);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.144889;
    tri3_xyze(1, 0) = -0.0388229;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2561);
    tri3_xyze(0, 1) = -0.15;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = -0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-16);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.15;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 1) = -0.140451;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = -0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-16);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.140451;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 1) = -0.135665;
    tri3_xyze(1, 1) = -0.0363514;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2557);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = -0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-16);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.140451;
    tri3_xyze(1, 0) = -1.38494e-13;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 1) = -0.15;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-18);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.15;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 1) = -0.144889;
    tri3_xyze(1, 1) = 0.0388229;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-18);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.144889;
    tri3_xyze(1, 0) = 0.0388229;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 1) = -0.135665;
    tri3_xyze(1, 1) = 0.0363514;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-18);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.135665;
    tri3_xyze(1, 0) = 0.0363514;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 1) = -0.140451;
    tri3_xyze(1, 1) = -1.38494e-13;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2559);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-18);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.140451;
    tri3_xyze(1, 0) = 1.38494e-13;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2567);
    tri3_xyze(0, 1) = -0.15;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = -0.0187936;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-19);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.15;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 1) = -0.144889;
    tri3_xyze(1, 1) = -0.0388229;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2561);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = -0.0187936;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-19);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.15;
    tri3_xyze(1, 0) = 0;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 1) = -0.140451;
    tri3_xyze(1, 1) = 1.38494e-13;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2567);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-21);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.135665;
    tri3_xyze(1, 0) = 0.0363514;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2813);
    tri3_xyze(0, 1) = -0.144889;
    tri3_xyze(1, 1) = 0.0388229;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-21);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.144889;
    tri3_xyze(1, 0) = 0.0388229;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 1) = -0.15;
    tri3_xyze(1, 1) = 0;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2563);
    tri3_xyze(0, 2) = -0.142751;
    tri3_xyze(1, 2) = 0.0187936;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-21);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = -0.0218829;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2573);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = 2.24087e-13;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = -0.012941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-25);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = 2.24087e-13;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 1) = -0.115451;
    tri3_xyze(1, 1) = 2.24087e-13;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2571);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = -0.012941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-25);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.115451;
    tri3_xyze(1, 0) = 2.24087e-13;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2571);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = 2.24087e-13;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-27);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = 2.24087e-13;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-27);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = 0.0298809;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2815);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-27);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = 0.0298809;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2815);
    tri3_xyze(0, 1) = -0.115451;
    tri3_xyze(1, 1) = 2.24087e-13;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2571);
    tri3_xyze(0, 2) = -0.0982963;
    tri3_xyze(1, 2) = 0.012941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-27);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = -0.0154125;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2577);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = 1.38494e-13;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-28);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = 1.38494e-13;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = 2.24087e-13;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-28);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = 2.24087e-13;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = -0.0218829;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2573);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = -0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-28);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0845492;
    tri3_xyze(1, 0) = 2.24087e-13;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 1) = -0.0595492;
    tri3_xyze(1, 1) = 1.38494e-13;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-30);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0595492;
    tri3_xyze(1, 0) = 1.38494e-13;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2579);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-30);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-30);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 1) = -0.0845492;
    tri3_xyze(1, 1) = 2.24087e-13;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2575);
    tri3_xyze(0, 2) = -0.0708216;
    tri3_xyze(1, 2) = 0.00932385;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-30);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = -0.0421076;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2602);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = -0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = -0.0330594;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-31);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = -0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 1) = -0.0433013;
    tri3_xyze(1, 1) = -0.025;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2581);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = -0.0330594;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-31);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = -0.0421076;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2602);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = -0.0597853;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2605);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = -0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-33);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0597853;
    tri3_xyze(1, 0) = -0.0597853;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2605);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = -0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = -0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-33);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = -0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = -0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = -0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-33);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = -0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2582);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = -0.0421076;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2602);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = -0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-33);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = -0.0577254;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2587);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = -0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = -0.0603553;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-34);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = -0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2585);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = -0.0597853;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2605);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = -0.0603553;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-34);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = 0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-141);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-141);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 1) = -0.0433013;
    tri3_xyze(1, 1) = 0.025;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-141);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0433013;
    tri3_xyze(1, 0) = 0.025;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 1) = -0.0482963;
    tri3_xyze(1, 1) = 0.012941;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-141);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 1) = -0.0482963;
    tri3_xyze(1, 1) = 0.012941;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-142);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0482963;
    tri3_xyze(1, 0) = 0.012941;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2801);
    tri3_xyze(0, 1) = -0.0433013;
    tri3_xyze(1, 1) = 0.025;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-142);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0433013;
    tri3_xyze(1, 0) = 0.025;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-142);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 2) = -0.0501722;
    tri3_xyze(1, 2) = 0.020782;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-142);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-143);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-143);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-143);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2802);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-143);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = 0.0298809;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-144);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = 0.0298809;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 1) = -0.0999834;
    tri3_xyze(1, 1) = 0.0577254;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-144);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = 0.0577254;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-144);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2805);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-144);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = 0.0298809;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 1) = -0.135665;
    tri3_xyze(1, 1) = 0.0363514;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 2) = -0.1172;
    tri3_xyze(1, 2) = 0.0485458;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-145);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.135665;
    tri3_xyze(1, 0) = 0.0363514;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 1) = -0.121634;
    tri3_xyze(1, 1) = 0.0702254;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2829);
    tri3_xyze(0, 2) = -0.1172;
    tri3_xyze(1, 2) = 0.0485458;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-145);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.121634;
    tri3_xyze(1, 0) = 0.0702254;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2829);
    tri3_xyze(0, 1) = -0.0999834;
    tri3_xyze(1, 1) = 0.0577254;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 2) = -0.1172;
    tri3_xyze(1, 2) = 0.0485458;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-145);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = 0.0577254;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = 0.0298809;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2807);
    tri3_xyze(0, 2) = -0.1172;
    tri3_xyze(1, 2) = 0.0485458;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-145);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.135665;
    tri3_xyze(1, 0) = 0.0363514;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 1) = -0.144889;
    tri3_xyze(1, 1) = 0.0388229;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 2) = -0.133023;
    tri3_xyze(1, 2) = 0.0550999;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-146);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.144889;
    tri3_xyze(1, 0) = 0.0388229;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 1) = -0.129904;
    tri3_xyze(1, 1) = 0.075;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2831);
    tri3_xyze(0, 2) = -0.133023;
    tri3_xyze(1, 2) = 0.0550999;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-146);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.121634;
    tri3_xyze(1, 0) = 0.0702254;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2829);
    tri3_xyze(0, 1) = -0.135665;
    tri3_xyze(1, 1) = 0.0363514;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2809);
    tri3_xyze(0, 2) = -0.133023;
    tri3_xyze(1, 2) = 0.0550999;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-146);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.144889;
    tri3_xyze(1, 0) = 0.0388229;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 1) = -0.135665;
    tri3_xyze(1, 1) = 0.0363514;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2813);
    tri3_xyze(0, 2) = -0.133023;
    tri3_xyze(1, 2) = 0.0550999;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-147);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.129904;
    tri3_xyze(1, 0) = 0.075;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2831);
    tri3_xyze(0, 1) = -0.144889;
    tri3_xyze(1, 1) = 0.0388229;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2811);
    tri3_xyze(0, 2) = -0.133023;
    tri3_xyze(1, 2) = 0.0550999;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-147);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.111517;
    tri3_xyze(1, 0) = 0.0298809;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2815);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-149);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-149);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 1) = -0.0999834;
    tri3_xyze(1, 1) = 0.0577254;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2835);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-149);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = 0.0577254;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2835);
    tri3_xyze(0, 1) = -0.111517;
    tri3_xyze(1, 1) = 0.0298809;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2815);
    tri3_xyze(0, 2) = -0.0915976;
    tri3_xyze(1, 2) = 0.037941;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-149);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816682;
    tri3_xyze(1, 0) = 0.0218829;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 1) = -0.0575201;
    tri3_xyze(1, 1) = 0.0154125;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-150);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0575201;
    tri3_xyze(1, 0) = 0.0154125;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2819);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-150);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-150);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 1) = -0.0816682;
    tri3_xyze(1, 1) = 0.0218829;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2817);
    tri3_xyze(0, 2) = -0.0659953;
    tri3_xyze(1, 2) = 0.0273361;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-150);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0433013;
    tri3_xyze(1, 0) = 0.025;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-151);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = 0.0421076;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-151);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 1) = -0.0353553;
    tri3_xyze(1, 1) = 0.0353553;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-151);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0353553;
    tri3_xyze(1, 0) = 0.0353553;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 1) = -0.0433013;
    tri3_xyze(1, 1) = 0.025;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-151);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 1) = -0.0433013;
    tri3_xyze(1, 1) = 0.025;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-152);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0433013;
    tri3_xyze(1, 0) = 0.025;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2821);
    tri3_xyze(0, 1) = -0.0353553;
    tri3_xyze(1, 1) = 0.0353553;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-152);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0353553;
    tri3_xyze(1, 0) = 0.0353553;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = 0.0421076;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2859);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-152);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2859);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 2) = -0.0430838;
    tri3_xyze(1, 2) = 0.0330594;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-152);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-153);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = 0.0597853;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-153);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0597853;
    tri3_xyze(1, 0) = 0.0597853;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = 0.0421076;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-153);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2822);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-153);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 1) = -0.0999834;
    tri3_xyze(1, 1) = 0.0577254;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = 0.0603553;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-154);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = 0.0577254;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 1) = -0.0816361;
    tri3_xyze(1, 1) = 0.0816361;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = 0.0603553;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-154);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816361;
    tri3_xyze(1, 0) = 0.0816361;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = 0.0597853;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = 0.0603553;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-154);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0597853;
    tri3_xyze(1, 0) = 0.0597853;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2825);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = 0.0603553;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-154);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = 0.0577254;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 1) = -0.121634;
    tri3_xyze(1, 1) = 0.0702254;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2829);
    tri3_xyze(0, 2) = -0.100642;
    tri3_xyze(1, 2) = 0.0772252;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-155);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.121634;
    tri3_xyze(1, 0) = 0.0702254;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2829);
    tri3_xyze(0, 1) = -0.0993137;
    tri3_xyze(1, 1) = 0.0993137;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 2) = -0.100642;
    tri3_xyze(1, 2) = 0.0772252;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-155);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0993137;
    tri3_xyze(1, 0) = 0.0993137;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 1) = -0.0816361;
    tri3_xyze(1, 1) = 0.0816361;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 2) = -0.100642;
    tri3_xyze(1, 2) = 0.0772252;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-155);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816361;
    tri3_xyze(1, 0) = 0.0816361;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 1) = -0.0999834;
    tri3_xyze(1, 1) = 0.0577254;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2827);
    tri3_xyze(0, 2) = -0.100642;
    tri3_xyze(1, 2) = 0.0772252;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-155);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.106066;
    tri3_xyze(1, 0) = 0.106066;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2851);
    tri3_xyze(0, 1) = -0.0993137;
    tri3_xyze(1, 1) = 0.0993137;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 2) = -0.114229;
    tri3_xyze(1, 2) = 0.0876513;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-156);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0993137;
    tri3_xyze(1, 0) = 0.0993137;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 1) = -0.121634;
    tri3_xyze(1, 1) = 0.0702254;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2829);
    tri3_xyze(0, 2) = -0.114229;
    tri3_xyze(1, 2) = 0.0876513;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-156);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0999834;
    tri3_xyze(1, 0) = 0.0577254;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2835);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = 0.0603553;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-159);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = 0.0597853;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2857);
    tri3_xyze(0, 2) = -0.0786566;
    tri3_xyze(1, 2) = 0.0603553;
    tri3_xyze(2, 2) = 0.847553;
    nids.push_back(-159);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0732217;
    tri3_xyze(1, 0) = 0.0422746;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 1) = -0.0515711;
    tri3_xyze(1, 1) = 0.0297746;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-160);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0515711;
    tri3_xyze(1, 0) = 0.0297746;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2839);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = 0.0421076;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2859);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-160);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2859);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = 0.0597853;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2857);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-160);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0597853;
    tri3_xyze(1, 0) = 0.0597853;
    tri3_xyze(2, 0) = 0.847553;
    nids.push_back(2857);
    tri3_xyze(0, 1) = -0.0732217;
    tri3_xyze(1, 1) = 0.0422746;
    tri3_xyze(2, 1) = 0.847553;
    nids.push_back(2837);
    tri3_xyze(0, 2) = -0.0566714;
    tri3_xyze(1, 2) = 0.0434855;
    tri3_xyze(2, 2) = 0.838471;
    nids.push_back(-160);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0353553;
    tri3_xyze(1, 0) = 0.0353553;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = 0.0421076;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-161);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 1) = -0.0297746;
    tri3_xyze(1, 1) = 0.0515711;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2862);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-161);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0297746;
    tri3_xyze(1, 0) = 0.0515711;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2862);
    tri3_xyze(0, 1) = -0.025;
    tri3_xyze(1, 1) = 0.0433013;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-161);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.025;
    tri3_xyze(1, 0) = 0.0433013;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 1) = -0.0353553;
    tri3_xyze(1, 1) = 0.0353553;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-161);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2859);
    tri3_xyze(0, 1) = -0.0353553;
    tri3_xyze(1, 1) = 0.0353553;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-162);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0353553;
    tri3_xyze(1, 0) = 0.0353553;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2841);
    tri3_xyze(0, 1) = -0.025;
    tri3_xyze(1, 1) = 0.0433013;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-162);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.025;
    tri3_xyze(1, 0) = 0.0433013;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 1) = -0.0297746;
    tri3_xyze(1, 1) = 0.0515711;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2879);
    tri3_xyze(0, 2) = -0.0330594;
    tri3_xyze(1, 2) = 0.0430838;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-162);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0421076;
    tri3_xyze(1, 0) = 0.0421076;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = 0.0597853;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 2) = -0.0434855;
    tri3_xyze(1, 2) = 0.0566714;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-163);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0597853;
    tri3_xyze(1, 0) = 0.0597853;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 1) = -0.0422746;
    tri3_xyze(1, 1) = 0.0732217;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2865);
    tri3_xyze(0, 2) = -0.0434855;
    tri3_xyze(1, 2) = 0.0566714;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-163);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0297746;
    tri3_xyze(1, 0) = 0.0515711;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2862);
    tri3_xyze(0, 1) = -0.0421076;
    tri3_xyze(1, 1) = 0.0421076;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2842);
    tri3_xyze(0, 2) = -0.0434855;
    tri3_xyze(1, 2) = 0.0566714;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-163);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0597853;
    tri3_xyze(1, 0) = 0.0597853;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 1) = -0.0816361;
    tri3_xyze(1, 1) = 0.0816361;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 2) = -0.0603553;
    tri3_xyze(1, 2) = 0.0786566;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-164);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816361;
    tri3_xyze(1, 0) = 0.0816361;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 1) = -0.0577254;
    tri3_xyze(1, 1) = 0.0999834;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 2) = -0.0603553;
    tri3_xyze(1, 2) = 0.0786566;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-164);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0577254;
    tri3_xyze(1, 0) = 0.0999834;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 1) = -0.0422746;
    tri3_xyze(1, 1) = 0.0732217;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2865);
    tri3_xyze(0, 2) = -0.0603553;
    tri3_xyze(1, 2) = 0.0786566;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-164);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0422746;
    tri3_xyze(1, 0) = 0.0732217;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2865);
    tri3_xyze(0, 1) = -0.0597853;
    tri3_xyze(1, 1) = 0.0597853;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2845);
    tri3_xyze(0, 2) = -0.0603553;
    tri3_xyze(1, 2) = 0.0786566;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-164);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0816361;
    tri3_xyze(1, 0) = 0.0816361;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 1) = -0.0993137;
    tri3_xyze(1, 1) = 0.0993137;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 2) = -0.0772252;
    tri3_xyze(1, 2) = 0.100642;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-165);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0993137;
    tri3_xyze(1, 0) = 0.0993137;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 1) = -0.0702254;
    tri3_xyze(1, 1) = 0.121634;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2869);
    tri3_xyze(0, 2) = -0.0772252;
    tri3_xyze(1, 2) = 0.100642;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-165);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0702254;
    tri3_xyze(1, 0) = 0.121634;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2869);
    tri3_xyze(0, 1) = -0.0577254;
    tri3_xyze(1, 1) = 0.0999834;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 2) = -0.0772252;
    tri3_xyze(1, 2) = 0.100642;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-165);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0577254;
    tri3_xyze(1, 0) = 0.0999834;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 1) = -0.0816361;
    tri3_xyze(1, 1) = 0.0816361;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2847);
    tri3_xyze(0, 2) = -0.0772252;
    tri3_xyze(1, 2) = 0.100642;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-165);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0993137;
    tri3_xyze(1, 0) = 0.0993137;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 1) = -0.106066;
    tri3_xyze(1, 1) = 0.106066;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2851);
    tri3_xyze(0, 2) = -0.0876513;
    tri3_xyze(1, 2) = 0.114229;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-166);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0702254;
    tri3_xyze(1, 0) = 0.121634;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2869);
    tri3_xyze(0, 1) = -0.0993137;
    tri3_xyze(1, 1) = 0.0993137;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2849);
    tri3_xyze(0, 2) = -0.0876513;
    tri3_xyze(1, 2) = 0.114229;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-166);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.025;
    tri3_xyze(1, 0) = 0.0433013;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 1) = -0.0297746;
    tri3_xyze(1, 1) = 0.0515711;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2862);
    tri3_xyze(0, 2) = -0.020782;
    tri3_xyze(1, 2) = 0.0501722;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-171);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0154125;
    tri3_xyze(1, 0) = 0.0575201;
    tri3_xyze(2, 0) = 0.770611;
    nids.push_back(2882);
    tri3_xyze(0, 1) = -0.012941;
    tri3_xyze(1, 1) = 0.0482963;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 2) = -0.020782;
    tri3_xyze(1, 2) = 0.0501722;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-171);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.012941;
    tri3_xyze(1, 0) = 0.0482963;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 1) = -0.025;
    tri3_xyze(1, 1) = 0.0433013;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 2) = -0.020782;
    tri3_xyze(1, 2) = 0.0501722;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-171);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0297746;
    tri3_xyze(1, 0) = 0.0515711;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2879);
    tri3_xyze(0, 1) = -0.025;
    tri3_xyze(1, 1) = 0.0433013;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 2) = -0.020782;
    tri3_xyze(1, 2) = 0.0501722;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-172);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.025;
    tri3_xyze(1, 0) = 0.0433013;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2861);
    tri3_xyze(0, 1) = -0.012941;
    tri3_xyze(1, 1) = 0.0482963;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 2) = -0.020782;
    tri3_xyze(1, 2) = 0.0501722;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-172);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.012941;
    tri3_xyze(1, 0) = 0.0482963;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 1) = -0.0154125;
    tri3_xyze(1, 1) = 0.0575201;
    tri3_xyze(2, 1) = 0.829389;
    nids.push_back(2899);
    tri3_xyze(0, 2) = -0.020782;
    tri3_xyze(1, 2) = 0.0501722;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-172);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0422746;
    tri3_xyze(1, 0) = 0.0732217;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2865);
    tri3_xyze(0, 1) = -0.0577254;
    tri3_xyze(1, 1) = 0.0999834;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 2) = -0.037941;
    tri3_xyze(1, 2) = 0.0915976;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-174);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0577254;
    tri3_xyze(1, 0) = 0.0999834;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 1) = -0.0298809;
    tri3_xyze(1, 1) = 0.111517;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2887);
    tri3_xyze(0, 2) = -0.037941;
    tri3_xyze(1, 2) = 0.0915976;
    tri3_xyze(2, 2) = 0.752447;
    nids.push_back(-174);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0577254;
    tri3_xyze(1, 0) = 0.0999834;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 1) = -0.0702254;
    tri3_xyze(1, 1) = 0.121634;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2869);
    tri3_xyze(0, 2) = -0.0485458;
    tri3_xyze(1, 2) = 0.1172;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-175);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0298809;
    tri3_xyze(1, 0) = 0.111517;
    tri3_xyze(2, 0) = 0.752447;
    nids.push_back(2887);
    tri3_xyze(0, 1) = -0.0577254;
    tri3_xyze(1, 1) = 0.0999834;
    tri3_xyze(2, 1) = 0.752447;
    nids.push_back(2867);
    tri3_xyze(0, 2) = -0.0485458;
    tri3_xyze(1, 2) = 0.1172;
    tri3_xyze(2, 2) = 0.761529;
    nids.push_back(-175);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.012941;
    tri3_xyze(1, 0) = 0.0482963;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 1) = -0.0154125;
    tri3_xyze(1, 1) = 0.0575201;
    tri3_xyze(2, 1) = 0.770611;
    nids.push_back(2882);
    tri3_xyze(0, 2) = -0.00708835;
    tri3_xyze(1, 2) = 0.0538414;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-181);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = 1.49999e-12;
    tri3_xyze(1, 0) = 0.05;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2901);
    tri3_xyze(0, 1) = -0.012941;
    tri3_xyze(1, 1) = 0.0482963;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 2) = -0.00708835;
    tri3_xyze(1, 2) = 0.0538414;
    tri3_xyze(2, 2) = 0.785305;
    nids.push_back(-181);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.0154125;
    tri3_xyze(1, 0) = 0.0575201;
    tri3_xyze(2, 0) = 0.829389;
    nids.push_back(2899);
    tri3_xyze(0, 1) = -0.012941;
    tri3_xyze(1, 1) = 0.0482963;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 2) = -0.00708835;
    tri3_xyze(1, 2) = 0.0538414;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-182);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }
  {
    Core::LinAlg::SerialDenseMatrix tri3_xyze(3, 3);

    nids.clear();
    tri3_xyze(0, 0) = -0.012941;
    tri3_xyze(1, 0) = 0.0482963;
    tri3_xyze(2, 0) = 0.8;
    nids.push_back(2881);
    tri3_xyze(0, 1) = 1.49999e-12;
    tri3_xyze(1, 1) = 0.05;
    tri3_xyze(2, 1) = 0.8;
    nids.push_back(2901);
    tri3_xyze(0, 2) = -0.00708835;
    tri3_xyze(1, 2) = 0.0538414;
    tri3_xyze(2, 2) = 0.814695;
    nids.push_back(-182);
    intersection.add_cut_side(++sidecount, nids, tri3_xyze, Core::FE::CellType::tri3);
  }



  {
    Core::LinAlg::SerialDenseMatrix hex8_xyze(3, 8);

    nids.clear();
    hex8_xyze(0, 0) = -0.05;
    hex8_xyze(1, 0) = 0;
    hex8_xyze(2, 0) = 0.75;
    nids.push_back(1886);
    hex8_xyze(0, 1) = -0.05;
    hex8_xyze(1, 1) = 0.05;
    hex8_xyze(2, 1) = 0.75;
    nids.push_back(1887);
    hex8_xyze(0, 2) = -0.1;
    hex8_xyze(1, 2) = 0.05;
    hex8_xyze(2, 2) = 0.75;
    nids.push_back(1898);
    hex8_xyze(0, 3) = -0.1;
    hex8_xyze(1, 3) = 0;
    hex8_xyze(2, 3) = 0.75;
    nids.push_back(1897);
    hex8_xyze(0, 4) = -0.05;
    hex8_xyze(1, 4) = 0;
    hex8_xyze(2, 4) = 0.8;
    nids.push_back(2007);
    hex8_xyze(0, 5) = -0.05;
    hex8_xyze(1, 5) = 0.05;
    hex8_xyze(2, 5) = 0.8;
    nids.push_back(2008);
    hex8_xyze(0, 6) = -0.1;
    hex8_xyze(1, 6) = 0.05;
    hex8_xyze(2, 6) = 0.8;
    nids.push_back(2019);
    hex8_xyze(0, 7) = -0.1;
    hex8_xyze(1, 7) = 0;
    hex8_xyze(2, 7) = 0.8;
    nids.push_back(2018);

    intersection.add_element(1901, nids, hex8_xyze, Core::FE::CellType::hex8);
  }



  intersection.cut_test_cut(true, Cut::VCellGaussPts_DirectDivergence);
}
