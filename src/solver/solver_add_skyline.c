/*!----------------------------------------------------------------------
\file
\brief 

<pre>
Maintainer: Malte Neumann
            neumann@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/neumann/
            0711 - 685-6121
</pre>

*----------------------------------------------------------------------*/

#include "../headers/standardtypes.h"
#include "../solver/solver.h"

/*----------------------------------------------------------------------*
 | global dense matrices for element routines             m.gee 9/01    |
 | (defined in global_calelm.c, so they are extern here)                |                
 *----------------------------------------------------------------------*/
extern struct _ARRAY estif_global;
extern struct _ARRAY emass_global;

/*----------------------------------------------------------------------*
  |  routine to assemble element array to global skyline-matrix          |
  |  in parallel and sequentiell,taking care of coupling conditions      |
  |                                                                      |
  |                                                                      |
  |                                                         m.gee 9/01   |
 *----------------------------------------------------------------------*/
void  add_skyline(
    struct _PARTITION     *actpart,
    struct _SOLVAR        *actsolv,
    struct _INTRA         *actintra,
    struct _ELEMENT       *actele,
    struct _SKYMATRIX     *sky1,
    struct _SKYMATRIX     *sky2)
{

  INT               i,j;                   /* some counter variables */
  INT               ii,jj;                 /* counter variables for system matrix */
  INT               nd;                    /* size of estif */
  INT               numeq_total;           /* total number of equations */
  INT               numeq;                 /* number of equations on this proc */
  INT               myrank;                /* my intra-proc number */
  INT               nprocs;                /* my intra- number of processes */
  DOUBLE          **estif;                  /* element matrix 1 to be added to system matrix */
  DOUBLE          **emass;                  /* element matrix 2 to be added to system matrix */
  DOUBLE           *A;                      /* the skyline matrix 1 */
  DOUBLE           *B;                      /* the skyline matrix 2 */
  INT              *maxa;

  INT               index;

#ifdef DEBUG 
  dstrc_enter("add_skyline");
#endif

  /* set some pointers and variables */
  myrank     = actintra->intra_rank;
  nprocs     = actintra->intra_nprocs;
  estif      = estif_global.a.da;
  emass      = emass_global.a.da;
  nd         = actele->nd;
  numeq_total= sky1->numeq_total;
  numeq      = sky1->numeq;
  A          = sky1->A.a.dv;
  maxa       = sky1->maxa.a.iv;
  if (sky2)
    B          = sky2->A.a.dv;
  else
    B          = NULL;

  /* loop over i (the element row) */
  for (i=0; i<nd; i++)
  {
    ii = actele->locm[i];

    /* loop over j (the element column) */
    for (j=0; j<nd; j++)
    {
      jj = actele->locm[j];
      index = actele->index[i][j];

      if(index >= 0)  /* normal dof */
      {
        A[index] += estif[i][j];
        if (B)
          B[index] += emass[i][j];
      }

      if(index == -1)  /* boundary condition dof */
        continue;

    } /* end loop over j */
  }/* end loop over i */

#ifdef DEBUG 
  dstrc_exit();
#endif
  return;
} /* end of add_skyline */



/*----------------------------------------------------------------------*
 |  make redundant skyline matrix on all procs               m.gee 01/02|
 *----------------------------------------------------------------------*/
void redundant_skyline(
                        PARTITION     *actpart,
                        SOLVAR        *actsolv,
                        INTRA         *actintra,
                        SKYMATRIX     *sky1,
                        SKYMATRIX     *sky2
                        )
{

#ifdef PARALLEL
INT      imyrank;
INT      inprocs;

ARRAY    recv_a;
DOUBLE  *recv;
#endif

#ifdef DEBUG 
dstrc_enter("redundant_skyline");
#endif
/*----------------------------------------------------------------------*/
/*  NOTE:
          In this routine, for a relatively short time the system matrix
          exists 2 times. This takes a lot of memory and may be a 
          bottle neck!
          In MPI2 there exists a flag for in-place-Allreduce:
          
          MPI_Allreduce(MPI_IN_PLACE,
                        ucchb->a.a.dv,
                        (ucchb->a.fdim)*(ucchb->a.sdim),
                        MPI_DOUBLE,
                        MPI_SUM,
                        actintra->MPI_INTRA_COMM);
          
          But there is no MPI2 in for HP, yet.
*/
/*----------------------------------------------------------------------*/
#ifdef PARALLEL 
/*----------------------------------------------------------------------*/
imyrank = actintra->intra_rank;
inprocs = actintra->intra_nprocs;
/*--- very comfortable: the only thing to do is to alreduce the ARRAY a */
/*                      (all coupling conditions are done then as well) */
/*--------------------------------------------------- allocate recvbuff */
recv = amdef("recv_a",&recv_a,sky1->A.fdim,sky1->A.sdim,"DV");
/*----------------------------------------------------------- Allreduce */  
MPI_Allreduce(sky1->A.a.dv,
              recv,
              (sky1->A.fdim)*(sky1->A.sdim),
              MPI_DOUBLE,
              MPI_SUM,
              actintra->MPI_INTRA_COMM);
/*----------------------------------------- copy reduced data back to a */
amcopy(&recv_a,&(sky1->A));
if (sky2)
{
/*----------------------------------------------------------- Allreduce */  
MPI_Allreduce(sky2->A.a.dv,
              recv,
              (sky2->A.fdim)*(sky2->A.sdim),
              MPI_DOUBLE,
              MPI_SUM,
              actintra->MPI_INTRA_COMM);
/*----------------------------------------- copy reduced data back to a */
amcopy(&recv_a,&(sky2->A));
}
/*----------------------------------------------------- delete recvbuff */
amdel(&recv_a);
#endif /*---------------------------------------------- end of PARALLEL */ 
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of redundant_skyline */
