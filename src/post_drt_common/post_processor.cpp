/*!
 \file post_processor.cpp

 \brief main routine of the main postprocessor filters

 <pre>
 Maintainer: Martin Kronbichler
 kronbichler@lnm.mw.tum.de
 http://www.lnm.mw.tum.de
 089 - 289-15235
 </pre>

 */

#include "post_drt_common.H"

#include "post_writer_base.H"
#include "post_single_field_writers.H"
#include "../post_gid/post_drt_gid.H"

#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_discret.H"



void runEnsightVtuFilter(PostProblem    &problem)
{
#if 0
    for (int i = 0; i<problem.num_discr(); ++i)
    {
        PostField* field = problem.get_discretization(i);
        StructureFilter writer(field, problem.outname());
        writer.WriteFiles();
    }
#endif

    // each problem type is different and writes different results
    switch (problem.Problemtype())
    {
    case prb_fsi:
    case prb_fsi_redmodels:
    case prb_fsi_lung:
    {
        std::string basename = problem.outname();
        PostField* structfield = problem.get_discretization(0);
        StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
        structwriter.WriteFiles();

        PostField* fluidfield = problem.get_discretization(1);
        FluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();

        //PostField* alefield = problem.get_discretization(2);
        //AleFilter alewriter(alefield, basename);
        //alewriter.WriteFiles();
#ifdef D_ARTNET
        // 1d artery
        if (problem.num_discr()== 4)
        {
          PostField* field = problem.get_discretization(2);
          StructureFilter writer(field, basename, problem.stresstype(), problem.straintype());
          writer.WriteFiles();
        }
#endif //D_ARTNET
        if (problem.num_discr() > 2 and problem.get_discretization(2)->name()=="xfluid")
        {
          std::string basename = problem.outname();
          PostField* fluidfield = problem.get_discretization(2);
          XFluidFilter xfluidwriter(fluidfield, basename);
          xfluidwriter.WriteFiles();
        }
        break;
    }
    case prb_gas_fsi:
    case prb_thermo_fsi:
    {
      std::string basename = problem.outname();
      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(1);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      int numdisc = problem.num_discr();

      for (int i=0; i<numdisc-3; ++i)
      {
        PostField* scatrafield = problem.get_discretization(3+i);
        ScaTraFilter scatrawriter(scatrafield, basename);
        scatrawriter.WriteFiles();
      }

      break;
    }
    case prb_biofilm_fsi:
    {
      std::string basename = problem.outname();
      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(1);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      int numdisc = problem.num_discr();

      for (int i=0; i<numdisc-4; ++i)
      {
        PostField* scatrafield = problem.get_discretization(3+i);
        ScaTraFilter scatrawriter(scatrafield, basename);
        scatrawriter.WriteFiles();
      }

      break;
    }
    case prb_structure:
    case prb_statmech:
    case prb_struct_ale:
    case prb_invana:
    {
        PostField* field = problem.get_discretization(0);
        StructureFilter writer(field, problem.outname(), problem.stresstype(), problem.straintype());
        writer.WriteFiles();
        break;
    }
    case prb_fluid:
    {
      if (problem.num_discr()== 2)
      {
        if (problem.get_discretization(1)->name()=="xfluid")
        {
          std::string basename = problem.outname();
          PostField* fluidfield = problem.get_discretization(1);
          XFluidFilter xfluidwriter(fluidfield, basename);
          xfluidwriter.WriteFiles();
        }
      }
      // don't add break here !!!!
    }
    case prb_fluid_redmodels:
    {
      if (problem.num_discr()== 2)
      {
        // 1d artery
#ifdef D_ARTNET
        std::string basename = problem.outname();
        PostField* field = problem.get_discretization(1);
        StructureFilter writer(field, basename, problem.stresstype(), problem.straintype());
        writer.WriteFiles();
#endif //D_ARTNET
        if (problem.get_discretization(1)->name()=="xfluid")
        {
          std::string basename = problem.outname();
          PostField* fluidfield = problem.get_discretization(1);
          XFluidFilter xfluidwriter(fluidfield, basename);
          xfluidwriter.WriteFiles();
        }
      }
      // don't add break here !!!!
    }
    case prb_fluid_ale:
    case prb_freesurf:
    {
        PostField* field = problem.get_discretization(0);
        FluidFilter writer(field, problem.outname());
        writer.WriteFiles();
        if (problem.num_discr()>1 and problem.get_discretization(1)->name()=="xfluid")
        {
          std::string basename = problem.outname();
          PostField* fluidfield = problem.get_discretization(1);
          XFluidFilter xfluidwriter(fluidfield, basename);
          xfluidwriter.WriteFiles();
        }
        break;
    }
    case prb_particle:
    {
      PostField* particlewallfield = problem.get_discretization(0);
      StructureFilter writer(particlewallfield, problem.outname(), problem.stresstype(), problem.straintype());
      writer.WriteFiles();

      PostField* particlefield = problem.get_discretization(1);
      ParticleFilter  particlewriter(particlefield, problem.outname());
      particlewriter.WriteFiles();

      break;
    }
    case prb_crack:
    {
      PostField* crackfield = problem.get_discretization(0);
      StructureFilter writer(crackfield, problem.outname(), problem.stresstype(), problem.straintype());
      writer.WriteFilesChangingGeom();
      break;
    }
    case prb_cavitation:
    {
      PostField* fluidfield = problem.get_discretization(0);
      FluidFilter fluidwriter(fluidfield, problem.outname());
      fluidwriter.WriteFiles();

      PostField* particlefield = problem.get_discretization(1);
      ParticleFilter  particlewriter(particlefield, problem.outname());
      particlewriter.WriteFiles();
      break;
    }
    case prb_level_set:
    {
      std::string basename = problem.outname();

      PostField* scatrafield = problem.get_discretization(0);
      ScaTraFilter scatrawriter(scatrafield, basename);
      scatrawriter.WriteFiles();

      // check if we have a particle field
      int numfield = problem.num_discr();
      if(numfield==2)
      {
        PostField* particlefield = problem.get_discretization(1);
        ParticleFilter  particlewriter(particlefield, basename);
        particlewriter.WriteFiles();
      }

      break;
    }
    case prb_redairways_tissue:
    {
        PostField* structfield = problem.get_discretization(0);
        StructureFilter structwriter(structfield, problem.outname(), problem.stresstype(), problem.straintype());
        structwriter.WriteFiles();

        PostField* fluidfield = problem.get_discretization(1);
        StructureFilter fluidwriter(fluidfield, problem.outname(), problem.stresstype(), problem.straintype());
        fluidwriter.WriteFiles();
        break;
    }
    case prb_fluid_fluid:
    {
      std::string basename = problem.outname();

      PostField* fluidfield = problem.get_discretization(0);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      PostField* embfluidfield = problem.get_discretization(1);
      FluidFilter embfluidwriter(embfluidfield, basename);
      embfluidwriter.WriteFiles();
      break;
    }
    case prb_fluid_fluid_ale:
    {
      std::string basename = problem.outname();

      PostField* fluidfield = problem.get_discretization(0);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      PostField* embfluidfield = problem.get_discretization(1);
      FluidFilter embfluidwriter(embfluidfield, basename);
      embfluidwriter.WriteFiles();

      break;
    }
    case prb_fluid_fluid_fsi:
    {
      std::string basename = problem.outname();
      PostField* fluidfield = problem.get_discretization(2);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      PostField* embfluidfield = problem.get_discretization(1);
      FluidFilter embfluidwriter(embfluidfield, basename);
      embfluidwriter.WriteFiles();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();
      break;
    }
    case prb_ale:
    {
        PostField* field = problem.get_discretization(0);
        AleFilter writer(field, problem.outname());
        writer.WriteFiles();
        break;
    }
    case prb_cardiac_monodomain:
    case prb_scatra:
    {
        std::string basename = problem.outname();
        // do we have a fluid discretization?
        int numfield = problem.num_discr();
        if(numfield==2)
        {
          PostField* fluidfield = problem.get_discretization(0);
          FluidFilter fluidwriter(fluidfield, basename);
          fluidwriter.WriteFiles();

          PostField* scatrafield = problem.get_discretization(1);
          ScaTraFilter scatrawriter(scatrafield, basename);
          scatrawriter.WriteFiles();
        }
        else if (numfield==1)
        {
          PostField* scatrafield = problem.get_discretization(0);
          ScaTraFilter scatrawriter(scatrafield, basename);
          scatrawriter.WriteFiles();
        }
        else
          dserror("number of fields does not match: got %d",numfield);

        break;
    }
    case prb_fsi_xfem:
    case prb_fsi_crack:
    {
      std::cout << "Output FSI-XFEM Problem" << std::endl;

        std::string basename = problem.outname();

        std::cout << "  Structural Field" << std::endl;
        PostField* structfield = problem.get_discretization(0);
        StructureFilter structwriter(structfield, problem.outname(), problem.stresstype(), problem.straintype());
        structwriter.WriteFiles();

        std::cout << "  Fluid Field" << std::endl;
        PostField* fluidfield = problem.get_discretization(1);
        FluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();


        std::cout << "  Interface Field" << std::endl;
        PostField* ifacefield = problem.get_discretization(2);
        InterfaceFilter ifacewriter(ifacefield, basename);
        ifacewriter.WriteFiles();

        break;
    }
    case prb_fpsi_xfem:
    {
      std::string basename = problem.outname();

      std::cout << "  Structural Field ( "<< problem.get_discretization(0)->name() << " )" << std::endl;
      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      std::cout << "  Porofluid Field ( "<< problem.get_discretization(1)->name() << " )" << std::endl;
      PostField* porofluidfield = problem.get_discretization(1);
      FluidFilter porofluidwriter(porofluidfield, basename);
      porofluidwriter.WriteFiles();

      std::cout << "  Ale Field ( "<< problem.get_discretization(2)->name() << " )" << std::endl;
      PostField* fluidfield = problem.get_discretization(2);
      AleFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      std::cout << "  Fluid Field ( "<< problem.get_discretization(3)->name() << " )" << std::endl;
      PostField* alefield = problem.get_discretization(3);
      FluidFilter alewriter(alefield, basename);
      alewriter.WriteFiles();

      std::cout << "  Interface Field ( "<< problem.get_discretization(4)->name() << " )" << std::endl;
      PostField* ifacefield = problem.get_discretization(4);
      InterfaceFilter ifacewriter(ifacefield, basename);
      ifacewriter.WriteFiles();

      break;
    }
    case prb_fluid_xfem:
    {
        std::cout << "Output FLUID-XFEM Problem" << std::endl;

        int numfield = problem.num_discr();
        if (numfield == 1 || numfield > 3)
          dserror("number of fields does not match: got %d",numfield);
        std::string basename = problem.outname();

        std::cout << "  Fluid Field" << std::endl;
        PostField* fluidfield = problem.get_discretization(0);
        FluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();

          std::cout << "  Interface Field" << std::endl;
          PostField* ifacefield = problem.get_discretization(numfield-1);
          InterfaceFilter ifacewriter(ifacefield, basename);
          ifacewriter.WriteFiles();
        break;
    }
    case prb_loma:
    {
        std::string basename = problem.outname();

        PostField* fluidfield = problem.get_discretization(0);
        FluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();

        PostField* scatrafield = problem.get_discretization(1);
        ScaTraFilter scatrawriter(scatrafield, basename);
        scatrawriter.WriteFiles();
        break;
    }
    case prb_two_phase_flow:
    case prb_fluid_xfem_ls:
    {
      std::string basename = problem.outname();

      PostField* fluidfield = problem.get_discretization(0);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      PostField* scatrafield = problem.get_discretization(1);
      ScaTraFilter scatrawriter(scatrafield, basename);
      scatrawriter.WriteFiles();

      // check if we have a particle field
      int numfield = problem.num_discr();
      if(numfield==3)
      {
        PostField* particlefield = problem.get_discretization(2);
        ParticleFilter particlewriter(particlefield, basename);
        particlewriter.WriteFiles();
      }
      break;
    }
    case prb_elch:
    {
      std::string basename = problem.outname();
      int numfield = problem.num_discr();
      if(numfield==3)
      {
        // Fluid, ScaTra and ALE fields are present
        PostField* fluidfield = problem.get_discretization(0);
        FluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();

        PostField* scatrafield = problem.get_discretization(1);
        ElchFilter elchwriter(scatrafield, basename);
        elchwriter.WriteFiles();

        PostField* alefield = problem.get_discretization(2);
        AleFilter alewriter(alefield, basename);
        alewriter.WriteFiles();
      }
      else if(numfield==2)
      {
        // Fluid and ScaTra fields are present
        PostField* fluidfield = problem.get_discretization(0);
        FluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();

        PostField* scatrafield = problem.get_discretization(1);
        ElchFilter elchwriter(scatrafield, basename);
        elchwriter.WriteFiles();
        break;
      }
      else if (numfield==1)
      {
        // only a ScaTra field is present
        PostField* scatrafield = problem.get_discretization(0);
        ElchFilter elchwriter(scatrafield, basename);
        elchwriter.WriteFiles();
      }
      else
        dserror("number of fields does not match: got %d",numfield);
      break;
    }
    case prb_combust:
    {
        std::string basename = problem.outname();

        PostField* fluidfield = problem.get_discretization(0);
        XFluidFilter fluidwriter(fluidfield, basename);
        fluidwriter.WriteFiles();

        PostField* scatrafield = problem.get_discretization(1);
        ScaTraFilter scatrawriter(scatrafield, basename);
        scatrawriter.WriteFiles();

        // check if we have a particle field
        int numfield = problem.num_discr();
        if(numfield==3)
        {
          PostField* particlefield = problem.get_discretization(2);
          ParticleFilter particlewriter(particlefield, basename);
          particlewriter.WriteFiles();
        }
        break;
    }
    case prb_art_net:
    {
      std::string basename = problem.outname();
      PostField* field = problem.get_discretization(0);
      //AnyFilter writer(field, problem.outname());
      StructureFilter writer(field, basename, problem.stresstype(), problem.straintype());
      writer.WriteFiles();

      break;
    }
    case prb_thermo:
    {
      PostField* field = problem.get_discretization(0);
      ThermoFilter writer(field, problem.outname(), problem.heatfluxtype(), problem.tempgradtype());
      writer.WriteFiles();
      break;
    }
    case prb_tsi:
    case prb_tfsi_aero:
    {
      std::cout << "Output TSI Problem" << std::endl;

      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* thermfield = problem.get_discretization(1);
      ThermoFilter thermwriter(thermfield, basename, problem.heatfluxtype(), problem.tempgradtype());
      thermwriter.WriteFiles();
      break;
    }
    case prb_red_airways:
    {
      std::string basename = problem.outname();
      PostField* field = problem.get_discretization(0);
      //AnyFilter writer(field, problem.outname());
      StructureFilter writer(field, basename, problem.stresstype(), problem.straintype());
      writer.WriteFiles();
      //      writer.WriteFiles();

      break;
    }
    case prb_poroelast:
    {
      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(1);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();
      break;
    }
    case prb_poroscatra:
    {
      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(1);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      PostField* scatrafield = problem.get_discretization(2);
      ScaTraFilter scatrawriter(scatrafield, basename);
      scatrawriter.WriteFiles();

      break;
    }
    case prb_fpsi:
    {
      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* porofluidfield = problem.get_discretization(1);
      FluidFilter porofluidwriter(porofluidfield, basename);
      porofluidwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(2);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      break;
    }
    case prb_immersed_fsi:
    {
      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(1);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      break;
    }
    case prb_fps3i:
    {
      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* porofluidfield = problem.get_discretization(1);
      FluidFilter porofluidwriter(porofluidfield, basename);
      porofluidwriter.WriteFiles();

      PostField* fluidfield = problem.get_discretization(2);
      FluidFilter fluidwriter(fluidfield, basename);
      fluidwriter.WriteFiles();

      /////////////

      int numdisc = problem.num_discr();

      for (int i=0; i<numdisc-4; ++i)
      {
        PostField* scatrafield = problem.get_discretization(4+i);
        ScaTraFilter scatrawriter(scatrafield, basename);
        scatrawriter.WriteFiles();
      }

      break;
    }
    case prb_ssi:
    {
      std::string basename = problem.outname();

      PostField* structfield = problem.get_discretization(0);
      StructureFilter structwriter(structfield, basename, problem.stresstype(), problem.straintype());
      structwriter.WriteFiles();

      PostField* scatrafield = problem.get_discretization(1);
      ScaTraFilter scatrawriter(scatrafield, basename);
      scatrawriter.WriteFiles();

      break;
    }
    case prb_fluid_topopt:
    {
      std::string basename = problem.outname();

      for (int i=0;i<problem.num_discr();i++)
      {
        std::string disname = problem.get_discretization(i)->discretization()->Name();

        if (disname.compare("fluid") == 0) // 0=true
        {
          PostField* fluidfield = problem.get_discretization(i);
          FluidFilter fluidwriter(fluidfield, basename);
          fluidwriter.WriteFiles();
        }
        else if (disname.compare("opti") == 0) // 0=true
        {
          PostField* optifield = problem.get_discretization(i);
          ScaTraFilter optiwriter(optifield, basename);
          optiwriter.WriteFiles();
        }
        else
          dserror("unknown discretization for postprocessing of topopt problem!");
      }

      break;
    }
    case prb_acou:
    {

      for (int i=0;i<problem.num_discr();i++)
      {
        std::string disname = problem.get_discretization(i)->discretization()->Name();
        if (disname.compare("acou") == 0) // 0=true
        {
          PostField* field = problem.get_discretization(i);
          AcouFilter writer(field, problem.outname());
          writer.WriteFiles();
        }
        else if(disname.compare("scatra") == 0)
        {
          PostField* field1 = problem.get_discretization(i);
          ScaTraFilter writer1(field1, problem.outname());
          writer1.WriteFiles();
        }
        else
          dserror("unknown discretization for postprocessing of acoustical problem!");
      }
      break;
    }
    case prb_uq:
    {
      for (int i=0;i<problem.num_discr();i++)
      {
        std::string disname = problem.get_discretization(i)->discretization()->Name();
        if( (disname.compare("structure") == 0) || (disname.compare("red_airway") == 0 ) ) // 0=true
        {
          PostField* field = problem.get_discretization(i);
          StructureFilter writer(field, problem.outname(), problem.stresstype(), problem.straintype());
          writer.WriteFiles();
        }
        else if(disname.compare("ale") == 0)
        {
          PostField* field = problem.get_discretization(i);
          AleFilter writer(field, problem.outname());
          writer.WriteFiles();
          break;
        }
        else
        {
           dserror("Unknown discretization type for problem type UQ");
        }
      } break;
    }
    case prb_none:
    {
      // Special problem type that contains one discretization and any number
      // of vectors. We just want to see whatever there is.
      PostField* field = problem.get_discretization(0);
      AnyFilter writer(field, problem.outname());
      writer.WriteFiles();
      break;
    }
    default:
        dserror("problem type %d not yet supported", problem.Problemtype());
        break;
    }
}



namespace
{
  std::string get_filter (int argc, char** argv)
  {
    Teuchos::CommandLineProcessor clp(false, false, false);
    std::string filter ("ensight");
    clp.setOption("filter",&filter,"filter to run [ensight, gid, vtu, vti]");
    // ignore warnings about unrecognized options.
    std::ostringstream warning;
    clp.parse(argc, argv, &warning);
    return filter;
  }
}


/*!
 \brief post-processor main routine

 Select the appropriate filter and run!

 \author kronbichler
 \date 03/14
 */
int main(
        int argc,
        char** argv)
{
  try
  {
    std::string filter = get_filter(argc, argv);
    Teuchos::CommandLineProcessor My_CLP;
    My_CLP.setDocString("Main BACI post-processor\n");

    PostProblem problem(My_CLP, argc, argv);

    if (filter == "ensight" || filter == "vtu" || filter == "vti")
      runEnsightVtuFilter(problem);
    else if (filter == "gid")
      PostGid::runGidFilter(problem);
    else
      dserror("Unknown filter %s given, supported filters: [ensight|vtu|vti|gid]", filter.c_str());

  } // try
  catch ( std::runtime_error & err )
  {
    char line[] = "=========================================================================\n";
    std::cout << "\n\n"
        << line
        << err.what()
        << "\n"
        << line
        << "\n" << std::endl;

    // proper cleanup
    DRT::Problem::Done();
#ifdef DSERROR_DUMP
    abort();
#endif

#ifdef PARALLEL
    MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
#else
    exit(1);
#endif
  } // catch

  // proper cleanup
  DRT::Problem::Done();

  return 0;
}

