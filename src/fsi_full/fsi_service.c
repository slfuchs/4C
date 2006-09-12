/*!----------------------------------------------------------------------
\file
\brief service routines for fsi algorithms

<pre>
Maintainer: Steffen Genkinger
            genkinger@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/genkinger/
            0711 - 685-6127
</pre>

------------------------------------------------------------------------*/


/*!
\addtogroup FSI
*//*! @{ (documentation module open)*/


#ifdef D_FSI


#include "../headers/standardtypes.h"
#include "../solver/solver.h"
#include "fsi_prototypes.h"
#include "../fluid2/fluid2.h"
#include "../fluid3/fluid3.h"


/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | general problem data                                                 |
 | global variable GENPROB genprob is defined in global_control.c       |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;


/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of numfld FIELDs, defined in global_control.c                 |
 *----------------------------------------------------------------------*/
extern struct _FIELD      *field;


/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;


/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | pointer to allocate dynamic variables if needed                      |
 | dedfined in global_control.c                                         |
 | ALLDYNA               *alldyn;                                       |
 *----------------------------------------------------------------------*/
extern ALLDYNA      *alldyn;


/*!----------------------------------------------------------------------
\brief ranks and communicators

<pre>                                                         m.gee 8/00
This structure struct _PAR par; is defined in main_ccarat.c
and the type is in partition.h
</pre>

*----------------------------------------------------------------------*/
 extern struct _PAR   par;


/*!----------------------------------------------------------------------
\brief file pointers

<pre>                                                         m.gee 8/00
This structure struct _FILES allfiles is defined in input_control_global.c
and the type is in standardtypes.h
It holds all file pointers and some variables needed for the FRSYSTEM
</pre>
*----------------------------------------------------------------------*/
extern struct _FILES  allfiles;


/*!----------------------------------------------------------------------*
 |                                                       m.gee 02/02    |
 | number of load curves numcurve                                       |
 | vector of structures of curves                                       |
 | defined in input_curves.c                                            |
 | INT                   numcurve;                                      |
 | struct _CURVE      *curve;                                           |
 *----------------------------------------------------------------------*/
extern INT            numcurve;
extern struct _CURVE *curve;

static FSI_DYNAMIC  *fsidyn;               /* fluid dynamic variables   */
static FLUID_DYNAMIC *fdyn;





/*!---------------------------------------------------------------------
\brief calculate the grid velocity

<pre>                                                         genk 10/02

  u_grid = [d(n+1)-d(n)]/dt

  phase=1: ALE PHASE I
  phase=2: ALE PHASE II: update during the nonlinear iteration
           local lagrange part. impl.: use solution for u_grid
</pre>
\param *fdyn       FLUID_DYNAMIC       (i)
\param  dt         DOUBLE              (i)       time increment
\param  numdf      INT                 (i)       number of dofs
\param  phase      INT                 (i)       flag for ale-phase
\return void

------------------------------------------------------------------------*/
void fsi_alecp(
    FIELD              *fluidfield,
    INT                 fdisnum,
    FIELD              *alefield,
    INT                 adisnum,
    DOUBLE              dt,
    INT                 numdf,
    INT                 phase
    )

{

  INT     i,j;             /* some counters                               */
  INT     numnp_total;     /* total number of nodes                       */
  INT     numveldof;       /* number of velocity dofs                     */
  INT     numaf;           /* number of ALE field                         */
  INT     phipos;          /* index of free surface movement (height func)*/
  DOUBLE  dxyznm;          /* ale-displement at (n-1)                     */
  DOUBLE  dxyzn;           /* ale-displement at (n)                       */
  DOUBLE  dxyz;            /* ale-displement at (n+1)                     */
  DOUBLE  phi,phin;        /* heightfunction values                       */
  NODE   *actfnode;        /* actual fluid node                           */
  NODE   *actanode;        /* actual ale node                             */
  GNODE  *actfgnode;       /* actual fluid gnode                          */
  ARRAY_POSITION *fluid_ipos;
  ARRAY_POSITION *ale_ipos;


#ifdef DEBUG
  dstrc_enter("fsi_alecp");
#endif


  fdyn = alldyn[genprob.numff].fdyn;

  fluid_ipos = &(fluidfield->dis[fdisnum].ipos);
  ale_ipos   = &(alefield->dis[adisnum].ipos);

  numnp_total  = fluidfield->dis[fdisnum].numnp;
  numveldof    = numdf-1;
  phipos       = numdf-2;
  numaf        = genprob.numaf;

  /*======================================================================*
   * nodal solution history fluid field:                                  *
   * sol_increment[0][j] ... solution at time (n-1)                       *
   * sol_increment[1][j] ... solution at time (n)                         *
   * sol_increment[2][j] ... solution at time (n+g)                       *
   * sol_increment[3][j] ... solution at time (n+1)                       *
   * sol_increment[4][i] ... grid velocity time (n)      (n+1)            *
   * sol_increment[5][i] ... convective velocity at time (n)              *
   * sol_increment[6][i] ... convective velocity at time (n+1)            *
   *======================================================================*
   * nodal solution history ale field:                                    *
   * sol_mf[0][i]        ... displacements at (n)                         *
   * sol_mf[1][i]        ... displacements at (n+1)                       *
   *======================================================================*/

  switch (phase)
  {

    case 1: /* ALE-PHASE I: get grid velocity from mesh displacements */

      /* loop all nodes */
      for (i=0;i<numnp_total;i++)
      {
        actfnode  = &(fluidfield->dis[fdisnum].node[i]);
        actfgnode = actfnode->gnode;
        actanode  = actfgnode->mfcpnode[numaf];
        if (actanode==NULL) continue;
        for (j=0;j<numveldof;j++)
        {
          dxyzn  = actanode->sol_mf.a.da[ale_ipos->mf_dispn ][j];
          dxyz   = actanode->sol_mf.a.da[ale_ipos->mf_dispnp][j];
          switch (fdyn->iop)
          {
            case 4: /* BE grid velocity: 1st order accuracy */
              actfnode->sol_increment.a.da[fluid_ipos->gridv][j] = (dxyz-dxyzn)/dt;
              break;
            case 7: /* BDF2 grid velocity: 2nd order accuracy */
              dxyznm = actanode->sol_mf.a.da[ale_ipos->mf_dispnm][j];
              actfnode->sol_increment.a.da[fluid_ipos->gridv][j] =
                (1.5*dxyz-2.0*dxyzn+0.5*dxyznm)/dt;
              break;
            default: dserror("Time integration scheme unknown for FSI");
          }
        } /* end of loop over vel dofs */
      } /* end of loop over all nodes */
      break;


    case 2:
    case 6: /* ALE-PHASE II: update grid velocity at free surface (local lagrange) */

      /* loop all nodes */
      for (i=0;i<numnp_total;i++)
      {
        actfnode  = &(fluidfield->dis[fdisnum].node[i]);
        if (actfnode->xfs==NULL) continue;
        for (j=0;j<numveldof;j++)
          actfnode->sol_increment.a.da[fluid_ipos->gridv][j]
            = actfnode->sol_increment.a.da[fluid_ipos->velnp][j+numdf];

      } /* end of loop over nodes */
      break;


    case 3:
    case 5: /* ALE-PHASE II: update grid velocity at free surface
               (height function separat & implicit) */
      for (i=0;i<numnp_total;i++)
      {
        actfnode  = &(fluidfield->dis[fdisnum].node[i]);
        if (actfnode->xfs==NULL) continue;
        phi  = actfnode->xfs[phipos];
        phin = actfnode->sol_increment.a.da[fluid_ipos->veln][numdf];
        actfnode->sol_increment.a.da[fluid_ipos->gridv][phipos] = (phi-phin)/dt;
      }  /* end of loop over nodes */
      break;


    default:
      dserror("ale phase out of range!\n");
  }


#ifdef DEBUG
  dstrc_exit();
#endif

  return;
} /* end of fsi_alecp */





/*!---------------------------------------------------------------------
\brief  calculate ale-convective velocity

<pre>                                                         genk 10/02

   c(n+1) = u(n+1) - u_grid(n->n+1)
   c(n)   = u(n)   - u_grid(n->n+1)

   NOTE: local co-system
         u is given in the xyz* co-system
         u_grid is given in the XYZ co-system
         Thus we have to transform the u-vector from xyz* to XYZ

</pre>
\param *fdyn       FLUID_DYNAMIC     (i)
\param  numdf      INT               (i)     number of dofs
\param  pos1       INT               (i)     position in sol_incr
\param  pos2       INT               (i)     position in sol_incr
\param  pos3       INT               (i)     position in sol_incr
\return void

------------------------------------------------------------------------*/
void fsi_aleconv(
    FIELD              *fluidfield,
    INT                 disnum,
    INT                 numdf,
    INT                 pos1,
    INT                 pos2
    )

{

  INT    i,j;           /* some counters                                  */
  INT    numnp_total;   /* total number of nodes                          */
  INT    numc;          /* number of veldofs                              */
  NODE  *actfnode;      /* actual fluid node                              */
  ARRAY_POSITION *ipos;


#ifdef DEBUG
  dstrc_enter("fsi_aleconv");
#endif


  ipos       = &(fluidfield->dis[disnum].ipos);
  numnp_total  = fluidfield->dis[disnum].numnp;
  numc         = numdf-1;

  /*======================================================================*
   * nodal solution history fluid field:                                  *
   * sol_increment[0][j] ... solution at time (n-1)                       *
   * sol_increment[1][j] ... solution at time (n)                         *
   * sol_increment[2][j] ... solution at time (n+g)                       *
   * sol_increment[3][j] ... solution at time (n+1)                       *
   * sol_increment[4][i] ... grid velocity time (n)      (n+1)            *
   * sol_increment[5][i] ... convective velocity at time (n)              *
   * sol_increment[6][i] ... convective velocity at time (n+1)            *
   *   needed for steepest descent method only:                           *
   * sol_increment[7][i] ... fluid solution for Relaxation parameter      *
   *======================================================================*/


  /* loop all nodes */
  for (i=0;i<numnp_total;i++)
  {
    actfnode  = &(fluidfield->dis[disnum].node[i]);
    for (j=0;j<numc;j++)
    {
      actfnode->sol_increment.a.da[pos1][j]
        =   actfnode->sol_increment.a.da[pos2][j]
        - actfnode->sol_increment.a.da[ipos->gridv][j];
    }
  }

#if 0
  for (i=0;i<numnp_total;i++)
  {
    actfnode  = &(fluidfield->dis[disnum].node[i]);
    if (actfnode->xfs==NULL) continue;
    printf(" %12.10lf   %12.10lf\n", actfnode->sol_increment.a.da[pos1][0],
        actfnode->sol_increment.a.da[pos1][1]);
  }
#endif


#ifdef DEBUG
  dstrc_exit();
#endif

  return;
} /* end of fsi_aleconv */





/*!---------------------------------------------------------------------
\brief reduce stresses

<pre>                                                         genk 10/02

after stress calculation the stress results have to be copied from the
stress field of the element to the sol_mf structure in order to transfer
them to the structure as Neumann conditions.
At the moment the element results are averaged only by the corresponding
number of elements belonging to this node.

</pre>
\param *actfield      FIELD          (i)  actual field
\param  numdf         INT            (i)  number of dofs
\return void

------------------------------------------------------------------------*/
void fsi_fluidstress_result(
    FIELD           *actfield,
    INT              disnum,
    INT              numdf
    )

{

  INT         i,j,k,l;      /* simply some counters                       */
  INT         numnp_total;  /* total number of nodes                      */
  INT         numele;       /* number of elements at actual node          */
  INT         numnp;        /* number of nodes at actual element          */
  NODE       *actnode;      /* actual node                                */
  GNODE      *actgnode;     /* actual gnode                               */
  ELEMENT    *actele;       /* actual element                             */
  ARRAY_POSITION *ipos;

#ifdef DEBUG
  dstrc_enter("fsi_fluidstress_result");
#endif

  /*======================================================================*/
  /* nodal solution history fluid field:                                  *
   * sol_mf[0][j]        ... solution at time (n+1)                       *
   * sol_mf[1][j]        ... nodal stresses at FS-interface at time (n+1) *
   *======================================================================*/


  /* set some values */
  numnp_total  = actfield->dis[disnum].numnp;
  ipos = &(actfield->dis[disnum].ipos);


  for (i=0;i<numnp_total;i++) /* loop nodes */
  {
    actnode=&(actfield->dis[disnum].node[i]);
    actgnode = actnode->gnode;
    /* check if there is a struct node coupled to actnode */
    /* this approach does not work with a nonconforming discretization
       of the interface, thus it is replaced by the second one */
    /*
       if (actgnode->mfcpnode[genprob.numsf]==NULL) continue;
       */
    if (actgnode->fsicouple == NULL) continue;
    numele=actnode->numele;
    for (j=0; j<numele; j++) /* loop elements belonging to node */
    {
      actele=actnode->element[j];
      numnp=actele->numnp;
      for (k=0;k<numnp;k++)
        if(actele->node[k]==actnode) break;
#ifdef D_FLUID2
      if (numdf==3)
      {
        for(l=0;l<3;l++)
          actnode->sol_mf.a.da[ipos->mf_forcenp][l]+=actele->e.f2->stress_ND.a.da[k][l]/numele;
      }
#endif
#ifdef D_FLUID3
      if (numdf==4)
      {
        for(l=0;l<6;l++)
          actnode->sol_mf.a.da[ipos->mf_forcenp][l]+=actele->e.f3->stress_ND.a.da[k][l]/numele;
      }
#endif
    }
  } /* end of loop over nodes */


#ifdef DEBUG
  dstrc_exit();
#endif

  return;

} /* end of  fsi_fluidstress_result*/





/*!---------------------------------------------------------------------
\brief output of fsi-algorithm data to the screen

<pre>                                                         genk 10/02

</pre>

\param *fsidyn      FSI_DYNAMIC      (i)
\param  itnum       INT              (i)  actual number of iteration
\return void

------------------------------------------------------------------------*/
void fsi_algoout(
    INT                 itnum
    )

{


#ifdef DEBUG
  dstrc_enter("fsi_algoout");
#endif


  /* set fsi dynamic pointer */
  fsidyn = alldyn[3].fsidyn;

  printf("\n");

  switch (fsidyn->ifsi)
  {
    case fsi_basic_sequ_stagg:
      printf("BASIC SEQUENTIAL STAGGERED SCHEME\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E  STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep);
      printf("\n");
      break;

    case fsi_sequ_stagg_pred:
      printf("SEQUENTIAL STAGGERED SCHEME WITH PREDICTOR\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep);
      printf("\n");
      break;

    case fsi_iter_stagg_fixed_rel_param:
      printf("ITERATIVE STAGGERED SCHEME WITH FIXED RELAXATION PARAMETER\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;

    case fsi_iter_stagg_AITKEN_rel_param:
      printf("ITERATIVE STAGGERED SCHEME WITH RELAXATION PARAMETER VIA AITKEN ITERATION\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;

    case fsi_iter_stagg_steep_desc:
      printf("ITERATIVE STAGGERED SCHEME WITH RELAXATION PARAMETER VIA STEEPEST DESCENT METHOD\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;

    case fsi_iter_stagg_AITKEN_rel_force:
      printf("ITERATIVE STAGGERED SCHEME WITH FORCE RELAXATION VIA AITKEN ITERATION\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;

    case fsi_iter_stagg_steep_desc_force:
      printf("ITERATIVE STAGGERED SCHEME WITH FORCE RELAXATION PARAMETER VIA STEEPEST DESCENT METHOD\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;

    case fsi_iter_stagg_Newton_FD:
      printf("ITERATIVE STAGGERED SCHEME WITH NEWTON-METHOD - APPROXIMATION BY FINITE DIFFERENC\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;

    case fsi_iter_stagg_Newton_I:
      printf("ITERATIVE STAGGERED SCHEME WITH NEWTON-METHOD - APPROXIMATION BY IDENTITY MATRIX\n");
      printf("TIME: %10.3E/%10.3E   DT=%10.3E   STEP=" YELLOW_LIGHT "%4d" END_COLOR "/%4d   ITNUM=" YELLOW_LIGHT "%4d" END_COLOR "/%4d\n",
          fsidyn->time,fsidyn->maxtime,fsidyn->dt,fsidyn->step,fsidyn->nstep,
          itnum,fsidyn->itemax);
      printf("\n");
      break;


    default:
      dserror("algoout not implemented yet\n");
  }

#ifdef DEBUG
  dstrc_exit();
#endif
  return;
} /* end of  fsi_algoout*/





/*!---------------------------------------------------------------------
\brief output of fsi-algorithm data to the screen

<pre>                                                         genk 01/03

</pre>

\param *fsidyn        FSI_DYNAMIC    (i)
\param *actfield      FIELD          (i)  actual field
\param  INT           INT            (i)  flag
\return void

------------------------------------------------------------------------*/
void fsi_structpredictor(
    FIELD              *actfield,
    INT                 disnum,
    INT                 init
    )

{

  INT    i,j,k;                  /* some counters                         */
  INT    olddim;
  INT    actcurve;               /* actual time curve                     */
  INT    numnp_total;            /* total number of nodes                 */
  const INT    FDIM=11;          /* dimension of structural sol field     */
  DOUBLE T,dt;                   /* actual time, time increment           */
  DOUBLE **sol;                  /* nodal solution history                */
  DOUBLE **sol_mf;               /* nodal multifield solution history     */
  DOUBLE timefac[MAXTIMECURVE];  /* timefactors from time curve           */
  DOUBLE acttimefac, initval;
  GNODE  *actgnode;              /* actual GNODE                          */
  NODE   *actnode;               /* actual NODE                           */
  ARRAY_POSITION *ipos;

#ifdef DEBUG
  dstrc_enter("fsi_structpredictor");
#endif

  fsidyn = alldyn[3].fsidyn;
  ipos = &(actfield->dis[disnum].ipos);

  /*======================================================================*
   * nodal solution history structural field:                             *
   * sol[0][j]           ... total displacements at time (t)              *
   * sol[1][j]           ... velocities at time (t)                       *
   * sol[2][j]           ... accels at time (t)                           *
   * sol[3][j]           ... prescribed displacements at time (t-dt)      *
   * sol[4][j]           ... prescribed displacements at time (t)         *
   * sol[5][j]           ... place 4 - place 3                            *
   * sol[6][j]           ... the  velocities of prescribed dofs           *
   * sol[7][j]           ... the  accels of prescribed dofs               *
   * sol[8][j]           ... working space                                *
   * sol[9][j]           ... total displacements at time (t-dt)           *
   * sol[10][j]          ... velocities at time (t-dt)                    *
   * sol_mf[0][j]        ... latest struct-displacements                  *
   * sol_mf[1][j]        ... (relaxed) displ. of the last iteration step  *
   * sol_mf[2][j]        ... converged relaxed displ. at time (t-dt)      *
   * sol_mf[3][j]        ... actual dispi                                 *
   * sol_mf[4][j]        ... FSI coupl.-forces at the end of the timestep *
   * sol_mf[5][j]        ... FSI coupl.-forces at beginning of the timest *
   *======================================================================*/

  numnp_total  = actfield->dis[disnum].numnp;

  /* enlarge sol-array during initialisation phase */
  if (init==1)
  {
    for (i=0;i<numnp_total;i++)
    {
      actnode=&(actfield->dis[disnum].node[i]);
      if (FDIM >= actnode->sol.fdim)
      {
        amredef(&(actnode->sol),FDIM,actnode->sol.sdim,"DA");
        amzero(&(actnode->sol));
      } /* endif enlargement */
    }
    goto end;
  }
  else if (init==2)
  {
    for (i=0;i<numnp_total;i++)
    {
      actnode=&(actfield->dis[disnum].node[i]);
      olddim=actnode->sol.fdim;
      if (FDIM >= olddim)
      {
        amredef(&(actnode->sol),FDIM,actnode->sol.sdim,"DA");
        for (j=olddim;j<FDIM;j++)
          for (k=0;k<actnode->sol.sdim;k++)
            actnode->sol.a.da[j][k]=ZERO;
      } /* endif enlargement */
    }
    goto end;
  }


  T  = fsidyn->time;
  dt = fsidyn->dt;

  /* WHAT DO WE HAVE TO DO IF WE HAVE PRESCRIBED DISPLACEMENTS AT THE NODES???
     ask MICHAEL of this is correct! */


  /* get values from time curve */
  for (actcurve=0;actcurve<numcurve;actcurve++)
  {
    dyn_facfromcurve(actcurve,T,&timefac[actcurve]) ;
  } /* end loop over active timecurves */



  /* calculate predictor velocity */

  switch (fsidyn->ipre)
  {
    case 1: /* dp(n+1) = d(n) */
      for (i=0;i<numnp_total;i++)
      {
        actnode=&(actfield->dis[disnum].node[i]);
        actgnode=actnode->gnode;
        sol = actnode->sol.a.da;
        sol_mf = actnode->sol_mf.a.da;
        /*--------------------------------- check for dirichlet condition */
        if (actgnode->dirich==NULL)
        {
          for (j=0;j<actnode->numdf;j++)
            sol_mf[ipos->mf_dispnp][j] = sol[0][j];
        }
        else /* if there are dirichlet values at n+1 use them!!! */
        {
          for (j=0;j<actnode->numdf;j++)
          {
            if (actgnode->dirich->dirich_onoff.a.iv[j]==0)
            {
              sol_mf[ipos->mf_dispnp][j]= sol[0][j];
              continue;
            }
            actcurve = actgnode->dirich->curve.a.iv[j]-1;
            if (actcurve<0) acttimefac = ONE;
            else acttimefac = timefac[actcurve];
            initval  = actgnode->dirich->dirich_val.a.dv[j];
            sol_mf[ipos->mf_dispnp][j] = initval*acttimefac;
          }
        }
      }
      break;
    case 2: /* dp(n+1) = d(n) + dt * ( 3/2*vg(n)-1/2*vg(n-1) ) */
      for (i=0;i<numnp_total;i++)
      {
        actnode=&(actfield->dis[disnum].node[i]);
        actgnode=actnode->gnode;
        sol = actnode->sol.a.da;
        sol_mf = actnode->sol_mf.a.da;
        /*--------------------------------- check for dirichlet condition */
        if (actgnode->dirich==NULL)
        {
          for (j=0;j<actnode->numdf;j++)
            sol_mf[ipos->mf_dispnp][j]= sol[0][j] + dt/TWO*(THREE*sol[1][j] - sol[10][j]);
        }
        else /* if there are dirichlet values at n+1 use them!!! */
        {
          for (j=0;j<actnode->numdf;j++)
          {
            if (actgnode->dirich->dirich_onoff.a.iv[j]==0)
            {
              sol_mf[ipos->mf_dispnp][j]= sol[0][j] + dt/TWO*(THREE*sol[1][j]- sol[9][j]);
              continue;
            }
            actcurve = actgnode->dirich->curve.a.iv[j]-1;
            if (actcurve<0) acttimefac = ONE;
            else acttimefac = timefac[actcurve];
            initval  = actgnode->dirich->dirich_val.a.dv[j];
            sol_mf[ipos->mf_dispnp][j] = initval*acttimefac;
          }
        }
      }
      break;
    case 3: /* dp(n+1) = d(n) + dt * vg(n) */
      for (i=0;i<numnp_total;i++)
      {
        actnode=&(actfield->dis[disnum].node[i]);
        actgnode=actnode->gnode;
        sol = actnode->sol.a.da;
        sol_mf = actnode->sol_mf.a.da;
        /*--------------------------------- check for dirichlet condition */
        if (actgnode->dirich==NULL)
        {
          for (j=0;j<actnode->numdf;j++)
            sol_mf[ipos->mf_dispnp][j]= sol[0][j] + dt*sol[1][j];
        }
        else /* if there are dirichlet values at n+1 use them!!! */
        {
          for (j=0;j<actnode->numdf;j++)
          {
            if (actgnode->dirich->dirich_onoff.a.iv[j]==0)
            {
              sol_mf[ipos->mf_dispnp][j]= sol[0][j] + dt*sol[1][j];
              continue;
            }
            actcurve = actgnode->dirich->curve.a.iv[j]-1;
            if (actcurve<0) acttimefac = ONE;
            else acttimefac = timefac[actcurve];
            initval  = actgnode->dirich->dirich_val.a.dv[j];
            sol_mf[ipos->mf_dispnp][j] = initval*acttimefac;
          }
        }
      }
      break;
    case 4: /* dp(n+1) = d(n) + dt * vg(n) + 1/2* dt**2 * ag(n) */
      for (i=0;i<numnp_total;i++)
      {
        actnode=&(actfield->dis[disnum].node[i]);
        actgnode=actnode->gnode;
        sol = actnode->sol.a.da;
        sol_mf = actnode->sol_mf.a.da;
        /*--------------------------------- check for dirichlet condition */
        if (actgnode->dirich==NULL)
        {
          for (j=0;j<actnode->numdf;j++)
            sol_mf[ipos->mf_dispnp][j]= sol[0][j] + dt*(sol[1][j] + (dt/TWO)*sol[2][j]);
        }
        else /* if there are dirichlet values at n+1 use them!!! */
        {
          for (j=0;j<actnode->numdf;j++)
          {
            if (actgnode->dirich->dirich_onoff.a.iv[j]==0)
            {
              sol_mf[ipos->mf_dispnp][j]= sol[0][j] + dt*sol[1][j] + dt*dt/TWO*sol[2][j];
              continue;
            }
            actcurve = actgnode->dirich->curve.a.iv[j]-1;
            if (actcurve<0) acttimefac = ONE;
            else acttimefac = timefac[actcurve];
            initval  = actgnode->dirich->dirich_val.a.dv[j];
            sol_mf[ipos->mf_dispnp][j] = initval*acttimefac;

          }
        }
      }
      break;
    default:
      dserror("structural PREDICTOR unknown!\n");
  }

end:
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
} /* end of  fsi_structpredictor*/






/*!---------------------------------------------------------------------
\brief convergence check for FSI-iteration

<pre>                                                         genk 01/03

in this routine the convergence ratios for the FSI interation over the
fields is calculated.
There are two possibility to check the convergence:
(see dissertation of D.P. MOK chapter 6.2)

- scaled_2-norm_of_residual (inrmfsi=1):
   || g(i) || / sqrt(neq) <= TOL

-  2-norm_of_residual_of_1st_iter (inrmfsi=2):
   || g(i) || / || g(0) || <= TOL

where g(i) = d~(i+1) - d(i);
      neq ... number of structural inteface dofs

</pre>
\param *structfield   FIELD          (i)   structural field
\param *fsidyn        FSI_DYNAMIC    (i)
\return INT

------------------------------------------------------------------------*/
INT fsi_convcheck(
    FIELD              *structfield,
    INT                 disnum,
    INT                 itnum,
    DOUBLE             *resnorm
    )

{

  INT     i,j;           /* some counters                                 */
  INT     converged=0;   /* flag for convergence                          */
  INT     numnp_total;   /* total number of nodes                         */
  INT     numdf_total;   /* total number of dofs                          */
  INT     numdf,dof;     /* actual number of dofs, actual dof             */
  INT    *sid;           /* structural interface dofs                     */
  DOUBLE  fac;
  DOUBLE  gnorm=ZERO;
  DOUBLE  g;
  DOUBLE  grat=0.0;
  NODE   *actsnode;      /* actual struct node                            */
  static DOUBLE g0norm;  /* norm of first iteration                       */
  FILE           *out = allfiles.out_out;

  ARRAY_POSITION *ipos;

#ifdef DEBUG
  dstrc_enter("fsi_convcheck");
#endif

  fsidyn = alldyn[3].fsidyn;

#if 1
  if (itnum==0)
  {
    grat=ONE;
    goto check;
  }
#endif

  /*----------------------------------------------------- set some values */
  numnp_total = structfield->dis[disnum].numnp;
  sid         = fsidyn->sid.a.iv;
  numdf_total = fsidyn->sid.fdim;
  ipos = &(structfield->dis[disnum].ipos);

  /*---------------------- loop nodes and calculate norm at the interface */
  for (i=0;i<numnp_total;i++)
  {
    actsnode  = &(structfield->dis[disnum].node[i]);
    /*----------------------------------------- check for coupling nodes */
    numdf = actsnode->numdf;
    /*-------------------------------------------------------- loop dofs */
    for (j=0;j<numdf;j++)
    {
      dof = actsnode->dof[j];
      dsassert(dof<numdf_total,"dofnumber not valid!\n");
      if (sid[dof]==0) continue;
      g = (actsnode->sol_mf.a.da[ipos->mf_dispnp ][j] -
	   actsnode->sol_mf.a.da[ipos->mf_reldisp][j]);
      gnorm += g*g;
    } /* end of loop over dofs */
  } /* end of loop over nodes */

  /*-------------------------------------- determine the convegence ratio */
  gnorm = sqrt(gnorm);
  switch (fsidyn->inrmfsi)
  {
    case 1: /* scaled_2-norm_of_residual */
      fac  = sqrt((DOUBLE)fsidyn->numsid);
      grat = gnorm/fac;
      break;
    case 2: /* 2-norm_of_residual_of_1st_iter */
      if (itnum==1)
      {
        g0norm = gnorm;
        if (g0norm<EPS5) g0norm=ONE;
      }
      grat = gnorm/g0norm;
      break;
    default:
      dserror("parameter out of range: inrmfsi\n");
  } /* end switch (fsidyn->inrmfsi) */

  /*------------------------------ check for convergence over the fields */
check:
  *resnorm = grat;
  if (grat<fsidyn->convtol)
    converged=2;
  if (itnum==fsidyn->itemax)
    converged++;

  /*----------------------------------------------- output to the screen */
  if (par.myrank==0)
  {
    printf("CONVERGENCE CHECK FOR ITERATION OVER FIELDS (ITNUM = %4d/%4d):\n",
        itnum,fsidyn->itemax);
    switch (fsidyn->inrmfsi)
    {
      case 1:
        if (converged==0)
        {
          printf("|| g(i) || / sqrt(neq) = " RED_LIGHT "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        if (converged==1)
        {
          printf("|| g(i) || / sqrt(neq) = " RED "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS AFTER ITEMAX STEPS!\n");
          printf("                ***** CONTINUING ****\n\n");
        }
        if (converged>=2)
        {
          printf("|| g(i) || / sqrt(neq) = " GREEN_LIGHT "%10.3E" END_COLOR " < TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        break;
      case 2:
        if (converged==0)
        {
          printf("|| g(i) || / || g(0) || = " RED_LIGHT "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        if (converged==1)
        {
          printf("|| g(i) || / || g(0) || = " RED "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS AFTER ITEMAX STEPS!\n");
          printf("                ***** CONTINUING ****\n\n");
        }
        if (converged>=2)
        {
          printf("|| g(i) || / || g(0) || = " GREEN_LIGHT "%10.3E" END_COLOR " < TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        break;
      default:
        dserror("parameter out of range: inrmfsi\n");
    }/* end switch (fsidyn->inrmfsi) */

    fprintf(out," %10.3E |",grat);


  }/* end if (par.myrank==0)*/

#ifdef DEBUG
  dstrc_exit();
#endif
  return (converged);
} /* end of  fsi_convcheck*/



INT fsi_convcheck_force(
  FIELD              *structfield,
  INT                 sdisnum,
  FIELD              *fluidfield,
  INT                 fdisnum,
  INT                 itnum,
  INT                 numff
    )
{
  INT     i,j,k,m;           /* some counters                                 */
  INT     converged=0;   /* flag for convergence                          */
  INT     numnp_total;   /* total number of nodes                         */
  INT     numdf_total;   /* total number of dofs                          */
  INT     numdf,dof;     /* actual number of dofs, actual dof             */
  INT    *sid;           /* structural interface dofs                     */
  DOUBLE  fac;
  DOUBLE  gnorm=ZERO;
  DOUBLE  g;
  DOUBLE  grat=0.0;
  NODE   *actsnode;      /* actual struct node                            */
  static DOUBLE g0norm;  /* norm of first iteration                       */
  FILE           *out = allfiles.out_out;
  ARRAY_POSITION *fluid_ipos;

#ifdef DEBUG
  dstrc_enter("fsi_convcheck_force");
#endif

  fsidyn = alldyn[3].fsidyn;

  if (itnum==0)
  {
    grat=ONE;
    goto check;
  }

  /*----------------------------------------------------- set some values */
  numnp_total = structfield->dis[sdisnum].numnp;
  sid         = fsidyn->sid.a.iv;
  numdf_total = fsidyn->sid.fdim;

  fluid_ipos = &(fluidfield->dis[fdisnum].ipos);

  /*---------------------- loop nodes and calculate norm at the interface */
  for (i=0;i<numnp_total;i++)
  {
    actsnode  = &(structfield->dis[sdisnum].node[i]);
    /*----------------------------------------- check for coupling nodes */
    numdf = actsnode->numdf;
    /*-------------------------------------------------------- loop dofs */
#ifdef FSI_NONMATCH
    for (j=0;j<numdf;j++)
    {
      NODE   *actfnode;
      ELEMENT *actele;

      dof = actsnode->dof[j];
      dsassert(dof<numdf_total,"dofnumber not valid!\n");
      if (sid[dof]==0) continue;

      /* The forces are calculated by the fluid. */
      /*actfnode = actsnode->gnode->mfcpnode[numff];*/
      for (k=0;k<actsnode->numele;k++)
      {
        if (actsnode->element[k]->coupleptr==NULL) continue;

        actele=actsnode->element[k];
        /*In diesem Element sind jetzt sicher Fluid Knoten!*/
        for (m=0;m<actele->coupleptr->numnp;m++)
        {
          actfnode=actele->coupleptr->couplenode[m];
          /*actfnode = actsnode->gnode->mfcpnode[numff];*/
          g = (actfnode->sol_mf.a.da[fluid_ipos->mf_forcenp][j] -
               actfnode->sol_mf.a.da[fluid_ipos->mf_forcen ][j]);

          gnorm += g*g;
        }
      }
    } /* end of loop over dofs */
#else
    for (j=0;j<numdf;j++)
    {
      NODE   *actfnode;

      dof = actsnode->dof[j];
      dsassert(dof<numdf_total,"dofnumber not valid!\n");
      if (sid[dof]==0) continue;

      /* The forces are calculated by the fluid. */
      actfnode = actsnode->gnode->mfcpnode[numff];
      g = (actfnode->sol_mf.a.da[fluid_ipos->mf_forcenp][j] -
	   actfnode->sol_mf.a.da[fluid_ipos->mf_forcen ][j]);

      gnorm += g*g;
    } /* end of loop over dofs */
#endif
  } /* end of loop over nodes */

  /*-------------------------------------- determine the convegence ratio */
  gnorm = sqrt(gnorm);
  switch (fsidyn->inrmfsi)
  {
    case 1: /* scaled_2-norm_of_residual */
      fac  = sqrt((DOUBLE)fsidyn->numsid);
      grat = gnorm/fac;
      break;
    case 2: /* 2-norm_of_residual_of_1st_iter */
      if (itnum==1)
      {
        g0norm = gnorm;
        if (g0norm<EPS5) g0norm=ONE;
      }
      grat = gnorm/g0norm;
      break;
    default:
      dserror("parameter out of range: inrmfsi\n");
  } /* end switch (fsidyn->inrmfsi) */

  /*------------------------------ check for convergence over the fields */
check:
  if (grat<fsidyn->convtol)
    converged=2;
  if (itnum==fsidyn->itemax)
    converged++;

  /*----------------------------------------------- output to the screen */
  if (par.myrank==0)
  {
    printf("CONVERGENCE CHECK FOR ITERATION OVER FIELDS (ITNUM = %4d/%4d):\n",
        itnum,fsidyn->itemax);
    switch (fsidyn->inrmfsi)
    {
      case 1:
        if (converged==0)
        {
          printf("|| g(i) || / sqrt(neq) = " RED_LIGHT "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        if (converged==1)
        {
          printf("|| g(i) || / sqrt(neq) = " RED "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS AFTER ITEMAX STEPS!\n");
          printf("                ***** CONTINUING ****\n\n");
        }
        if (converged>=2)
        {
          printf("|| g(i) || / sqrt(neq) = " GREEN_LIGHT "%10.3E" END_COLOR " < TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        break;
      case 2:
        if (converged==0)
        {
          printf("|| g(i) || / || g(0) || = " RED_LIGHT "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        if (converged==1)
        {
          printf("|| g(i) || / || g(0) || = " RED "%10.3E" END_COLOR " >= TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("NO CONVERGENCE OF ITERATION OVER FIELDS AFTER ITEMAX STEPS!\n");
          printf("                ***** CONTINUING ****\n\n");
        }
        if (converged>=2)
        {
          printf("|| g(i) || / || g(0) || = " GREEN_LIGHT "%10.3E" END_COLOR " < TOL = %10.3E \n",
              grat,fsidyn->convtol);
          printf("CONVERGENCE OF ITERATION OVER FIELDS!\n\n");
        }
        break;
      default:
        dserror("parameter out of range: inrmfsi\n");
    }/* end switch (fsidyn->inrmfsi) */

    fprintf(out," %10.3E |",grat);


  }/* end if (par.myrank==0)*/

#ifdef DEBUG
  dstrc_exit();
#endif
  return (converged);
}





/*!---------------------------------------------------------------------
\brief initialisation of ale field

<pre>                                                        chfoe 11/03

in this routine the ale field is initialised. The solution history is
enlarged to numr entities at the sol_increment.

</pre>
\param *acttfield   FIELD       (i)  ale field
\param  numr        INT         (i)  number of sol_increment places needed
\return void

\sa   calling:
      called by: fsi_ale_nln(), fsi_ale_spring, fsi_ale_laplace,
                 fsi_ale_2step
------------------------------------------------------------------------*/
void fsi_init_ale(
    FIELD              *actfield,
    INT                 numr
    )

{

  INT              i;
  INT              numnp_total;
  INT              disnum;
  NODE            *actnode;


#ifdef DEBUG
  dstrc_enter("fsi_init_ale");
#endif

  for (disnum=0;disnum<actfield->ndis;disnum++)
  {
    numnp_total=actfield->dis[disnum].numnp;
    for (i=0;i<numnp_total;i++)
    {
      actnode=&(actfield->dis[disnum].node[i]);
      amredef(&(actnode->sol_increment),numr,actnode->numdf,"DA");
      amzero(&(actnode->sol_increment));
    }
  }


#ifdef DEBUG
  dstrc_exit();
#endif

  return;

} /* end of fsi_init_ale */


/*!---------------------------------------------------------------------
\brief init positions in sol_increment

<pre>                                                        chfoe 11/04

This routine inits the positions in sol_increment in the case of an ALE
problem.

</pre>
\return void

------------------------------------------------------------------------*/
void fluid_init_pos_ale(
    FIELD              *fluidfield,
    INT                 disnum
    )

{

  FSI_DYNAMIC    *fsidyn;
  ARRAY_POSITION *ipos;


#ifdef DEBUG
  dstrc_enter("fluid_init_pos_ale");
#endif

  ipos = &(fluidfield->dis[disnum].ipos);
  fdyn = alldyn[genprob.numff].fdyn;
  fsidyn = alldyn[genprob.numaf+1].fsidyn;


  switch (fdyn->iop)
  {
    case 4:     /* One step Theta */
      ipos->velnm  = 0;
      ipos->veln   = 1;
      ipos->hist   = 2;
      ipos->velnp  = 3;
      ipos->gridv  = 4;
      ipos->convn  = 5;
      ipos->convnp = 6;
      ipos->accn   = 7;
      ipos->accnm  = 8;
      if ((fsidyn->ifsi == fsi_iter_stagg_steep_desc) ||
          (fsidyn->ifsi == fsi_iter_stagg_steep_desc_force))
        ipos->relax = 9;
      ipos->stresspro = 9;
      ipos->pred   =-1;
      ipos->terr   =-1;
      break;


    case 7:     /* BDF2 */
      ipos->velnm  = 0;
      ipos->veln   = 1;
      ipos->hist   = 2;
      ipos->velnp  = 3;
      ipos->gridv  = 4;
      ipos->convn  = 5;
      ipos->convnp = 6;
      if ((fsidyn->ifsi == fsi_iter_stagg_steep_desc) ||
          (fsidyn->ifsi == fsi_iter_stagg_steep_desc_force))
        ipos->relax = 7;
      ipos->stresspro = 7;
      ipos->accn   =-1;
      ipos->accnm  =-1;
      ipos->pred   =-1;
      ipos->terr   =-1;
      break;


    default:
      dserror("desired time stepping scheme not (fully) implemented.");
      break;
  }

#ifdef DEBUG
  dstrc_exit();
#endif

  return;

}



#endif
/*! @} (documentation module close)*/




