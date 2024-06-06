/*----------------------------------------------------------------------*/
/*! \file

\brief Read node sections of dat files.

\level 0

*/
/*----------------------------------------------------------------------*/

#include "4C_io_nodereader.hpp"

#include "4C_discretization_fem_general_element_definition.hpp"
#include "4C_discretization_fem_general_fiber_node.hpp"
#include "4C_discretization_fem_general_immersed_node.hpp"
#include "4C_lib_discret.hpp"
#include "4C_nurbs_discret_control_point.hpp"

#include <istream>
#include <utility>

FOUR_C_NAMESPACE_OPEN

namespace
{
  std::vector<Teuchos::RCP<DRT::Discretization>> FindDisNode(
      const std::vector<CORE::IO::ElementReader>& element_readers, int global_node_id)
  {
    std::vector<Teuchos::RCP<DRT::Discretization>> list_of_discretizations;
    for (const auto& element_reader : element_readers)
      if (element_reader.HasNode(global_node_id))
        list_of_discretizations.emplace_back(element_reader.GetDis());

    return list_of_discretizations;
  }

}  // namespace


void CORE::IO::ReadNodes(const CORE::IO::DatFileReader& reader,
    const std::string& node_section_name, std::vector<ElementReader>& element_readers,
    int& max_node_id)
{
  // Check if there are any nodes to be read. If not, leave right away.
  const int numnodes = reader.excluded_section_length(node_section_name);
  const auto& comm = reader.Comm();

  if (numnodes == 0) return;
  const int myrank = comm->MyPID();

  // We will read the nodes block wise. We will use one block per processor
  // so the number of blocks is numproc
  // OR number of blocks is numnodes if less nodes than procs are read in
  // determine a rough blocksize
  int number_of_blocks = std::min(comm->NumProc(), numnodes);
  int blocksize = std::max(numnodes / number_of_blocks, 1);

  // an upper limit for blocksize
  const int maxblocksize = 200000;

  if (blocksize > maxblocksize)
  {
    // without an additional increase of number_of_blocks by 1 the last block size
    // could reach a maximum value of (2*maxblocksize)-1, potentially
    // violating the intended upper limit!
    number_of_blocks = 1 + numnodes / maxblocksize;
    blocksize = maxblocksize;
  }

  // open input file at the right position
  // note that stream is valid on proc 0 only!
  const std::string inputfile_name = reader.MyInputfileName();
  std::ifstream file;
  if (myrank == 0)
  {
    file.open(inputfile_name.c_str());
    file.seekg(reader.excluded_section_position(node_section_name));
  }
  std::string tmp;
  std::string tmp2;

  // note that the last block is special....
  int filecount = 0;
  for (int block = 0; block < number_of_blocks; ++block)
  {
    if (myrank == 0)
    {
      int block_counter = 0;
      for (; file; ++filecount)
      {
        file >> tmp;

        if (tmp == "NODE")
        {
          std::vector<double> coords(3, 0.0);
          int nodeid;
          // read in the node coordinates
          file >> nodeid >> tmp >> coords[0] >> coords[1] >> coords[2];

          nodeid--;
          max_node_id = std::max(max_node_id, nodeid) + 1;
          std::vector<Teuchos::RCP<DRT::Discretization>> dis = FindDisNode(element_readers, nodeid);

          for (const auto& di : dis)
          {
            // create node and add to discretization
            Teuchos::RCP<CORE::Nodes::Node> node =
                Teuchos::rcp(new CORE::Nodes::Node(nodeid, coords, myrank));
            di->AddNode(node);
          }

          ++block_counter;
          if (block != number_of_blocks - 1)  // last block takes all the rest
            if (block_counter == blocksize)   // block is full
            {
              ++filecount;
              break;
            }
        }
        // this is a specialized node for immersed problems
        else if (tmp == "INODE")
        {
          std::vector<double> coords(3, 0.0);
          int nodeid;
          // read in the node coordinates
          file >> nodeid >> tmp >> coords[0] >> coords[1] >> coords[2];

          nodeid--;
          max_node_id = std::max(max_node_id, nodeid) + 1;
          std::vector<Teuchos::RCP<DRT::Discretization>> diss =
              FindDisNode(element_readers, nodeid);

          for (const auto& dis : diss)
          {
            // create node and add to discretization
            Teuchos::RCP<CORE::Nodes::Node> node =
                Teuchos::rcp(new CORE::Nodes::ImmersedNode(nodeid, coords, myrank));
            dis->AddNode(node);
          }

          ++block_counter;
          if (block != number_of_blocks - 1)  // last block takes all the rest
            if (block_counter == blocksize)   // block is full
            {
              ++filecount;
              break;
            }
        }
        // this node is a Nurbs control point
        else if (tmp == "CP")
        {
          // read control points for isogeometric analysis (Nurbs)
          std::vector<double> coords(3, 0.0);
          double weight;

          int cpid;
          file >> cpid >> tmp >> coords[0] >> coords[1] >> coords[2] >> weight;
          cpid--;
          max_node_id = std::max(max_node_id, cpid) + 1;
          if (cpid != filecount)
            FOUR_C_THROW("Reading of control points failed: They must be numbered consecutive!!");
          if (tmp != "COORD") FOUR_C_THROW("failed to read control point %d", cpid);
          std::vector<Teuchos::RCP<DRT::Discretization>> diss = FindDisNode(element_readers, cpid);

          for (auto& dis : diss)
          {
            // create node/control point and add to discretization
            Teuchos::RCP<DRT::NURBS::ControlPoint> node =
                Teuchos::rcp(new DRT::NURBS::ControlPoint(cpid, coords, weight, myrank));
            dis->AddNode(node);
          }

          ++block_counter;
          if (block != number_of_blocks - 1)  // last block takes all the rest
            if (block_counter == blocksize)   // block is full
            {
              ++filecount;
              break;
            }
        }
        // this is a special node with additional fiber information
        else if (tmp == "FNODE")
        {
          enum class FiberType
          {
            Unknown,
            Angle,
            Fiber,
            CosyDirection
          };

          // read fiber node
          std::vector<double> coords(3, 0.0);
          std::map<CORE::Nodes::CoordinateSystemDirection, std::array<double, 3>> cosyDirections;
          std::vector<std::array<double, 3>> fibers;
          std::map<CORE::Nodes::AngleType, double> angles;

          int nodeid;
          // read in the node coordinates and fiber direction
          file >> nodeid >> tmp >> coords[0] >> coords[1] >> coords[2];
          nodeid--;
          max_node_id = std::max(max_node_id, nodeid) + 1;

          while (true)
          {
            // store current position of file reader
            std::ifstream::pos_type length = file.tellg();
            // try to read new fiber direction or coordinate system
            file >> tmp2;

            CORE::Nodes::CoordinateSystemDirection coordinateSystemDirection;
            CORE::Nodes::AngleType angleType;
            FiberType type = FiberType::Unknown;

            if (tmp2 == "FIBER" + std::to_string(1 + fibers.size()))
            {
              type = FiberType::Fiber;
            }
            else if (tmp2 == "CIR")
            {
              coordinateSystemDirection = CORE::Nodes::CoordinateSystemDirection::Circular;
              type = FiberType::CosyDirection;
            }
            else if (tmp2 == "TAN")
            {
              coordinateSystemDirection = CORE::Nodes::CoordinateSystemDirection::Tangential;
              type = FiberType::CosyDirection;
            }
            else if (tmp2 == "RAD")
            {
              coordinateSystemDirection = CORE::Nodes::CoordinateSystemDirection::Radial;
              type = FiberType::CosyDirection;
            }
            else if (tmp2 == "HELIX")
            {
              angleType = CORE::Nodes::AngleType::Helix;
              type = FiberType::Angle;
            }
            else if (tmp2 == "TRANS")
            {
              angleType = CORE::Nodes::AngleType::Transverse;
              type = FiberType::Angle;
            }
            else
            {
              // No more fiber information. Jump to last position.
              file.seekg(length);
              break;
            }

            // add fiber / angle to the map
            switch (type)
            {
              case FiberType::Unknown:
              {
                FOUR_C_THROW(
                    "Unknown fiber node attribute. Numbered fibers must be in order, i.e. "
                    "FIBER1, FIBER2, ...");
              }
              case FiberType::Angle:
              {
                file >> angles[angleType];
                break;
              }
              case FiberType::Fiber:
              {
                std::array<double, 3> fiber_components;
                file >> fiber_components[0] >> fiber_components[1] >> fiber_components[2];
                fibers.emplace_back(fiber_components);
                break;
              }
              case FiberType::CosyDirection:
              {
                file >> cosyDirections[coordinateSystemDirection][0] >>
                    cosyDirections[coordinateSystemDirection][1] >>
                    cosyDirections[coordinateSystemDirection][2];
                break;
              }
              default:
                FOUR_C_THROW("Unknown number of components");
            }
          }

          // add fiber information to node
          std::vector<Teuchos::RCP<DRT::Discretization>> discretizations =
              FindDisNode(element_readers, nodeid);
          for (auto& dis : discretizations)
          {
            auto node = Teuchos::rcp(
                new CORE::Nodes::FiberNode(nodeid, coords, cosyDirections, fibers, angles, myrank));
            dis->AddNode(node);
          }

          ++block_counter;
          if (block != number_of_blocks - 1)  // last block takes all the rest
          {
            if (block_counter == blocksize)  // block is full
            {
              ++filecount;
              break;
            }
          }
        }
        else if (tmp.find("--") == 0)
          break;
        else
          FOUR_C_THROW("unexpected word '%s'", tmp.c_str());
      }
    }
  }
}

FOUR_C_NAMESPACE_CLOSE