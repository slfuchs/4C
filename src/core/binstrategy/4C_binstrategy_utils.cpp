/*----------------------------------------------------------------------*/
/*! \file

\brief utils class for use of binning strategy

\level 2

*----------------------------------------------------------------------*/


#include "4C_binstrategy_utils.hpp"

#include "4C_comm_exporter.hpp"
#include "4C_io_pstream.hpp"
#include "4C_rebalance_binning_based.hpp"
#include "4C_rebalance_print.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Core::Binstrategy::Utils
{
  /*-----------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------*/
  void extend_discretization_ghosting(Teuchos::RCP<Core::FE::Discretization> discret,
      Teuchos::RCP<Epetra_Map> const& extendedelecolmap, bool assigndegreesoffreedom,
      bool initelements, bool doboundaryconditions)
  {
    // make sure that all procs are either filled or unfilled
    // oldmap in export_column_elements must be reset() on every proc or nowhere
    discret->check_filled_globally();

    // adapt layout to extended ghosting in discret
    // first export the elements according to the processor local element column maps
    discret->export_column_elements(*extendedelecolmap);

    // get the node ids of the elements that are to be ghosted
    // and create a proper node column map for their export
    std::set<int> nodes;
    for (int lid = 0; lid < extendedelecolmap->NumMyElements(); ++lid)
    {
      Core::Elements::Element* ele = discret->g_element(extendedelecolmap->GID(lid));
      const int* nodeids = ele->node_ids();
      for (int inode = 0; inode < ele->num_node(); ++inode) nodes.insert(nodeids[inode]);
    }

    std::vector<int> colnodes(nodes.begin(), nodes.end());
    Teuchos::RCP<Epetra_Map> nodecolmap = Teuchos::rcp(
        new Epetra_Map(-1, (int)colnodes.size(), colnodes.data(), 0, discret->get_comm()));

    // now ghost the nodes
    discret->export_column_nodes(*nodecolmap);

    // fillcomplete discret with extended ghosting
    discret->fill_complete(assigndegreesoffreedom, initelements, doboundaryconditions);

#ifdef FOUR_C_ENABLE_ASSERTIONS
    // print distribution after standard ghosting
    if (discret->get_comm().MyPID() == 0)
      std::cout << "parallel distribution with extended ghosting" << std::endl;
    Core::Rebalance::UTILS::print_parallel_distribution(*discret);
#endif

    return;
  }

  /*-----------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------*/
  void communicate_elements(Teuchos::RCP<Core::FE::Discretization>& discret,
      std::map<int, std::vector<Core::Elements::Element*>> const& toranktosendeles)
  {
    // build exporter
    Core::Communication::Exporter exporter(discret->get_comm());
    int const numproc = discret->get_comm().NumProc();

    // -----------------------------------------------------------------------
    // send
    // -----------------------------------------------------------------------
    // ---- pack data for sending -----
    std::map<int, std::vector<char>> sdata;
    std::vector<int> targetprocs(numproc, 0);
    std::map<int, std::vector<Core::Elements::Element*>>::const_iterator p;
    for (p = toranktosendeles.begin(); p != toranktosendeles.end(); ++p)
    {
      std::vector<Core::Elements::Element*>::const_iterator iter;
      for (iter = p->second.begin(); iter != p->second.end(); ++iter)
      {
        Core::Communication::PackBuffer data;
        (*iter)->pack(data);
        sdata[p->first].insert(sdata[p->first].end(), data().begin(), data().end());
      }
      targetprocs[p->first] = 1;
    }

    // ---- send ----
    const int length = sdata.size();
    std::vector<MPI_Request> request(length);
    int tag = 0;
    for (std::map<int, std::vector<char>>::const_iterator p = sdata.begin(); p != sdata.end(); ++p)
    {
      exporter.i_send(discret->get_comm().MyPID(), p->first, (p->second).data(),
          (int)(p->second).size(), 1234, request[tag]);
      ++tag;
    }
    if (tag != length) FOUR_C_THROW("Number of messages is mixed up");

    // -----------------------------------------------------------------------
    // receive
    // -----------------------------------------------------------------------
    // ---- prepare receiving procs -----
    std::vector<int> summedtargets(numproc, 0);
    discret->get_comm().SumAll(targetprocs.data(), summedtargets.data(), numproc);

    // ---- receive ----
    for (int rec = 0; rec < summedtargets[discret->get_comm().MyPID()]; ++rec)
    {
      std::vector<char> rdata;
      int length = 0;
      int tag = -1;
      int from = -1;
      exporter.receive_any(from, tag, rdata, length);
      if (tag != 1234)
        FOUR_C_THROW("Received on proc %i data with wrong tag from proc %i",
            discret->get_comm().MyPID(), from);

      // ---- unpack ----
      {
        // Put received nodes into discretization
        Communication::UnpackBuffer buffer(rdata);
        while (!buffer.at_end())
        {
          std::vector<char> data;
          Core::Communication::ParObject::extract_from_pack(buffer, data);
          Communication::UnpackBuffer data_buffer(data);
          // this Teuchos::rcp holds the memory of the node
          Teuchos::RCP<Core::Communication::ParObject> object =
              Teuchos::rcp(Core::Communication::factory(data_buffer), true);
          Teuchos::RCP<Core::Elements::Element> element =
              Teuchos::rcp_dynamic_cast<Core::Elements::Element>(object);
          if (element == Teuchos::null) FOUR_C_THROW("Received object is not a element");

          // safety check
          if (discret->have_global_element(element->id()) != true)
            FOUR_C_THROW(
                "%i is getting owner of element %i without having it ghosted before, "
                "this is not intended.",
                discret->get_comm().MyPID(), element->id());

          // delete already existing element (as it has wrong internal variables)
          discret->delete_element(element->id());
          // add node (ownership already adapted on sending proc)
          discret->add_element(element);
        }
      }
    }

    // wait for all communications to finish
    for (int i = 0; i < length; ++i) exporter.wait(request[i]);
    // safety, should be a no time operation if everything works fine before
    discret->get_comm().Barrier();
  }

  /*-----------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------*/
  void communicate_distribution_of_transferred_elements_to_bins(
      Teuchos::RCP<Core::FE::Discretization>& discret,
      std::map<int, std::vector<std::pair<int, std::vector<int>>>> const& toranktosendbinids,
      std::map<int, std::set<int>>& bintorowelemap)
  {
    // build exporter
    Core::Communication::Exporter exporter(discret->get_comm());
    int const numproc = discret->get_comm().NumProc();

    // -----------------------------------------------------------------------
    // send
    // -----------------------------------------------------------------------
    // ---- pack data for sending -----
    std::map<int, std::vector<char>> sdata;
    std::vector<int> targetprocs(numproc, 0);
    std::map<int, std::vector<std::pair<int, std::vector<int>>>>::const_iterator p;
    for (p = toranktosendbinids.begin(); p != toranktosendbinids.end(); ++p)
    {
      std::vector<std::pair<int, std::vector<int>>>::const_iterator iter;
      for (iter = p->second.begin(); iter != p->second.end(); ++iter)
      {
        Core::Communication::PackBuffer data;
        Core::Communication::ParObject::add_to_pack(data, *iter);
        sdata[p->first].insert(sdata[p->first].end(), data().begin(), data().end());
      }
      targetprocs[p->first] = 1;
    }

    // ---- send ----
    const int length = sdata.size();
    std::vector<MPI_Request> request(length);
    int tag = 0;
    for (std::map<int, std::vector<char>>::const_iterator p = sdata.begin(); p != sdata.end(); ++p)
    {
      exporter.i_send(discret->get_comm().MyPID(), p->first, (p->second).data(),
          (int)(p->second).size(), 1234, request[tag]);
      ++tag;
    }
    if (tag != length) FOUR_C_THROW("Number of messages is mixed up");

    // -----------------------------------------------------------------------
    // receive
    // -----------------------------------------------------------------------
    // ---- prepare receiving procs -----
    std::vector<int> summedtargets(numproc, 0);
    discret->get_comm().SumAll(targetprocs.data(), summedtargets.data(), numproc);

    // ---- receive ----
    for (int rec = 0; rec < summedtargets[discret->get_comm().MyPID()]; ++rec)
    {
      std::vector<char> rdata;
      int length = 0;
      int tag = -1;
      int from = -1;
      exporter.receive_any(from, tag, rdata, length);
      if (tag != 1234)
        FOUR_C_THROW("Received on proc %i data with wrong tag from proc %i",
            discret->get_comm().MyPID(), from);

      // ---- unpack ----
      {
        // Put received nodes into discretization
        Communication::UnpackBuffer buffer(rdata);
        while (!buffer.at_end())
        {
          std::pair<int, std::vector<int>> pair;
          Core::Communication::ParObject::extract_from_pack(buffer, pair);
          std::vector<int>::const_iterator j;
          for (j = pair.second.begin(); j != pair.second.end(); ++j)
            bintorowelemap[*j].insert(pair.first);
        }
      }
    }

    // wait for all communications to finish
    for (int i = 0; i < length; ++i) exporter.wait(request[i]);
    // safety, should be a no time operation if everything works fine before
    discret->get_comm().Barrier();
  }

  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  void get_current_node_pos(const Core::FE::Discretization& discret, Core::Nodes::Node const* node,
      Teuchos::RCP<const Epetra_Vector> const disnp, double* currpos)
  {
    if (disnp != Teuchos::null)
    {
      const int gid = discret.dof(node, 0);
      const int lid = disnp->Map().LID(gid);
      if (lid < 0)
        FOUR_C_THROW(
            "Your displacement is incomplete (need to be based on a column map"
            " as this function is also called from a loop over elements and "
            "each proc does (usually) not own all nodes of his row elements ");
      for (int dim = 0; dim < 3; ++dim)
      {
        currpos[dim] = node->x()[dim] + (*disnp)[lid + dim];
      }
    }
    else
    {
      for (int dim = 0; dim < 3; ++dim) currpos[dim] = node->x()[dim];
    }
  }
}  // namespace Core::Binstrategy::Utils

FOUR_C_NAMESPACE_CLOSE
