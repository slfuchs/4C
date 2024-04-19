/*----------------------------------------------------------------------*/
/*! \file
\brief Test for the CUT Library

\level 1

*----------------------------------------------------------------------*/

#include "cut_test_loader.hpp"

#include "baci_linalg_serialdensematrix.hpp"

void MeshLoader::GetCutNode(int nid, double x, double y, double z, double lsv)
{
  if (nid > -1)
  {
    std::vector<double>& values = cut_nodes_[nid];
    values.reserve(3);
    values.push_back(x);
    values.push_back(y);
    values.push_back(z);
    // values.push_back( lsv );
  }
}

void MeshLoader::GetNode(int nid, double x, double y, double z, double lsv)
{
  if (nid > -1)
  {
    std::vector<double>& values = nodes_[nid];
    values.reserve(3);
    values.push_back(x);
    values.push_back(y);
    values.push_back(z);
    // values.push_back( lsv );
  }
}

void MeshLoader::CreateSide(
    int sid, int nid1, int nid2, int nid3, int nid4, CORE::FE::CellType shape)
{
  switch (shape)
  {
    case CORE::FE::CellType::quad4:
    {
      CORE::LINALG::SerialDenseMatrix xyz(3, 4);
      Fill(cut_nodes_, nid1, &xyz(0, 0));
      Fill(cut_nodes_, nid2, &xyz(0, 1));
      Fill(cut_nodes_, nid3, &xyz(0, 2));
      Fill(cut_nodes_, nid4, &xyz(0, 3));

      std::vector<int> nids;
      nids.reserve(4);
      nids.push_back(nid1);
      nids.push_back(nid2);
      nids.push_back(nid3);
      nids.push_back(nid4);
      mesh_.AddCutSide(sid, nids, xyz, CORE::FE::CellType::quad4);

      break;
    }
    default:
      FOUR_C_THROW("unknown shape creating a side in mesh loader");
  }
}

void MeshLoader::CreateElement(int eid, int nid1, int nid2, int nid3, int nid4, int nid5, int nid6,
    int nid7, int nid8, CORE::FE::CellType shape)
{
  switch (shape)
  {
    case CORE::FE::CellType::hex8:
    {
      CORE::LINALG::SerialDenseMatrix xyz(3, 8);
      Fill(nodes_, nid1, &xyz(0, 0));
      Fill(nodes_, nid2, &xyz(0, 1));
      Fill(nodes_, nid3, &xyz(0, 2));
      Fill(nodes_, nid4, &xyz(0, 3));
      Fill(nodes_, nid5, &xyz(0, 4));
      Fill(nodes_, nid6, &xyz(0, 5));
      Fill(nodes_, nid7, &xyz(0, 6));
      Fill(nodes_, nid8, &xyz(0, 7));

      std::vector<int> nids;
      nids.reserve(8);
      nids.push_back(nid1);
      nids.push_back(nid2);
      nids.push_back(nid3);
      nids.push_back(nid4);
      nids.push_back(nid5);
      nids.push_back(nid6);
      nids.push_back(nid7);
      nids.push_back(nid8);
      mesh_.AddElement(eid, nids, xyz, CORE::FE::CellType::hex8);

      break;
    }
    default:
      FOUR_C_THROW("unknown shape creating an element in mesh loader");
  }
}

void MeshLoader::Fill(std::map<int, std::vector<double>>& nodes, int nid, double* values)
{
  if (nodes.find(nid) == nodes.end())
  {
    FOUR_C_THROW("node not defined in mesh loader");
  }
  std::vector<double>& v = nodes[nid];
  std::copy(v.begin(), v.end(), values);
}
