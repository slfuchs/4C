/*----------------------------------------------------------------------*/
/*!
\file drt_init_control.cpp

<pre>
Maintainer: Ulrich Kuettler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/
/*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <string>

#include "drt_init_control.H"
#include "standardtypes_cpp.H"


/*!----------------------------------------------------------------------
\brief file pointers

<pre>                                                         m.gee 8/00
This structure struct _FILES allfiles is defined in input_control_global.c
and the type is in standardtypes.h
It holds all file pointers and some variables needed for the FRSYSTEM
</pre>
*----------------------------------------------------------------------*/
extern struct _FILES  allfiles;


/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | general problem data                                                 |
 | struct _GENPROB       genprob; defined in global_control.c           |
 *----------------------------------------------------------------------*/
extern struct _GENPROB  genprob;


/*----------------------------------------------------------------------*/
/*
  Setup of input and output files. No actual read is performed
  here.
 */
/*----------------------------------------------------------------------*/
void ntaini_ccadiscret(int argc, char** argv, MPI_Comm mpi_local_comm)
{
  int myrank = 0;

#ifdef PARALLEL
  MPI_Comm_rank(mpi_local_comm, &myrank);
#endif

  if (argc <= 1)
  {
    if (myrank==0)
    {
      printf("You forgot to give the input and output file names!\n");
      printf("Try again!\n");
    }
#ifdef PARALLEL
    MPI_Finalize();
#endif
    exit(1);
  }
  else if (argc <= 2)
  {
    if (myrank==0)
    {
      printf("You forgot to give the output file name!\n");
      printf("Try again!\n");
    }
#ifdef PARALLEL
    MPI_Finalize();
#endif
    exit(1);
  }

  allfiles.outputfile_kenner = argv[2];
  if (strlen(argv[2])>=100)
  {
    if (myrank==0)
    {
      printf("Your outputfile kenner is too long!\n");
      fflush(stdout);
    }
#ifdef PARALLEL
    MPI_Finalize();
#endif
    exit(1);
  }

  allfiles.inputfile_name = argv[1];
  // set error file names
  sprintf(allfiles.outputfile_name, "%s%d.err",
          allfiles.outputfile_kenner, myrank);
  // REMARK:
  // error files are opened by OpenErrorFile()
  // called in ntainp_ccadiscret()

  // inform user
  if (myrank==0)
  {
    printf("input is read from         %s\n", allfiles.inputfile_name);
    printf("errors are reported to     %s\n", allfiles.outputfile_name);
  }

  /*-------------------------------------------------- check for restart */
  genprob.restart = 0;
  if (argc > 3)
  {
    std::string restart(argv[3]);
    if (restart.substr( 0, 8 )=="restart=")
    {
      int r = atoi( restart.substr( 8, std::string::npos ).c_str() );
      genprob.restart = r;
    }
  }
}

#endif
