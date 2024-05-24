/*---------------------------------------------------------------------*/
/*! \file

\brief Functionality to read a mesh from a dat file

\level 0

*/
/*---------------------------------------------------------------------*/

#ifndef FOUR_C_IO_MESHREADER_HPP
#define FOUR_C_IO_MESHREADER_HPP

#include "4C_config.hpp"

#include "4C_io_domainreader.hpp"
#include "4C_io_elementreader.hpp"
#include "4C_io_geometry_type.hpp"
#include "4C_io_inputreader.hpp"

#include <Epetra_CrsGraph.h>

FOUR_C_NAMESPACE_OPEN

namespace IO
{
  /*!
    \brief helper class to read a mesh

    This is an interface for handling node, element and domain readers
    and to set up a discretization from a dat file which is fill_complete().
   */
  class MeshReader
  {
   public:
    /**
     * Additional parameters that governt the reading process.
     */
    struct MeshReaderParameters
    {
      /**
       * How to partition then mesh among processes.
       */
      Teuchos::ParameterList mesh_paritioning_parameters;

      /**
       * Geometric search parameters for certain partitiong methods.
       */
      Teuchos::ParameterList geometric_search_parameters;

      /**
       * General verbosity settings and I/O parameters.
       */
      Teuchos::ParameterList io_parameters;
    };

    /**
     * Construct a mesh reader. Read nodes from the given @p reader under section
     * @p node_section_name.
     */
    MeshReader(INPUT::DatFileReader& reader, std::string node_section_name,
        MeshReaderParameters parameters = {});

    /// add an element reader for each discretization
    /*!
      Each discretization needs its own ElementReader. These readers
      have to be registered at the MeshReader.

      \param er (i) a reader of one discretization that uses (a fraction of) our nodes
     */
    void AddElementReader(const ElementReader& er) { element_readers_.emplace_back(er); }

    /*!
     * \brief Adds the selected reader to this meshreader
     *
     * This is a version without specific elementtypes. It just calls the full
     * version with a dummy set.
     *
     * \param dis            [in] This discretization will be passed on
     * \param reader         [in] This reader will be passed on
     * \param sectionname    [in] This will be passed on element/domain readers only (not used for
     *                            file reader)
     * \param geometrysource [in] selects which reader will be created
     * \param geofilepath    [in] path to the file for the file reader (not used for the others)
     */
    void AddAdvancedReader(Teuchos::RCP<DRT::Discretization> dis,
        const INPUT::DatFileReader& reader, const std::string& sectionname,
        const IO::GeometryType geometrysource, const std::string* geofilepath);

    /// do the actual reading
    /*!
      This method contains the whole machinery. The reading consists of
      three steps:

      - Reading and distributing elements using each registered
        ElementReader. This includes creating the connectivity graph,
        building the node row and column maps.

      - Reading and distributing all nodes. Each node gets assigned to
        its discretization.

      - Finalizing the discretizations.

      Actually most of the work gets done by the ElementReader. The
      reading of both elements and nodes happens in blocks on processor
      0. After each block read the discretizations are redistributed.

     */
    void ReadAndPartition();

   private:
    /*!
    \brief Read pre-generated mesh from dat-file and generate related FillCompleted()
    discretizations

    \param[in/out] max_node_id Maximum node id in a given discretization. To be used as global
                               offset to start node numbering (based on already existing nodes)
    */
    void read_mesh_from_dat_file(int& max_node_id);

    /*!
    \brief Rebalance discretizations built in read_mesh_from_dat_file()
    */
    void rebalance();

    /*!
    \brief Create inline mesh

    Ask the DomainReader to process input data and create a box shaped mesh at runtime without
    having to read or process nodes and elements from the input file.

    \param[in/out] max_node_id Maximum node id in a given discretization. To be used as global
                               offset to start node numbering (based on already existing nodes)
    */
    void create_inline_mesh(int& max_node_id);

    /// my comm
    Teuchos::RCP<Epetra_Comm> comm_;

    //! graphs of each discretization
    std::vector<Teuchos::RCP<const Epetra_CrsGraph>> graph_;

    /// my element readers
    std::vector<ElementReader> element_readers_;

    /// my domain readers
    std::vector<DomainReader> domain_readers_;

    /// Input file contents
    INPUT::DatFileReader& reader_;

    /// The name of the section under which we will read the nodes.
    std::string node_section_name_;

    /// Additional paramters for reading meshes.
    MeshReaderParameters parameters_;
  };
}  // namespace IO

FOUR_C_NAMESPACE_CLOSE

#endif
