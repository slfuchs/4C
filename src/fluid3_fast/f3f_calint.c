/*-----------------------------------------------------------------------*/
/*!
\file
\brief Brief description.

  Very detailed description.

<pre>
Maintainer: Malte Neumann
            neumann@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/neumann/
            0711 - 685-6121
</pre>

 */
/*-----------------------------------------------------------------------*/

/*!
\addtogroup Fluid3_fast
*//*! @{ (documentation module open)*/


#ifdef D_FLUID3_F

#include "../headers/standardtypes.h"
#include "../fluid3/fluid3_prototypes.h"
#include "../fluid3_fast/f3f_prototypes.h"
#include "../fluid3/fluid3.h"

/*----------------------------------------------------------------------*
  |                                                       m.gee 06/01  |
  | pointer to allocate dynamic variables if needed                    |
  | dedfined in global_control.c                                       |
  | ALLDYNA               *alldyn;                                     |
 *----------------------------------------------------------------------*/
extern ALLDYNA      *alldyn;

/*----------------------------------------------------------------------*
  |                                                       m.gee 06/01  |
  | general problem data                                               |
  | global variable GENPROB genprob is defined in global_control.c     |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;

/*----------------------------------------------------------------------*
  |                                                       m.gee 06/01  |
  | vector of material laws                                            |
  | defined in global_control.c                                        |
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;




/*-----------------------------------------------------------------------*/
/*!
  \brief integration loop for one fast fluid3 element

  In this routine the element stiffness matrix, iteration-RHS and
  time-RHS for one fluid3 element is calculated

  \param ele[LOOPL]      ELEMENT  (i) the set of elements
  \param elecord         DOUBLE   (i) vector containing nodal coordinates
  \param tau            *DOUBLE   (i) stabilisation parameter
  \param hasext         *INT      (i) flag if there are ext forces
  \param estif          *DOUBLE   (i) element stiffness matrix
  \param emass          *DOUBLE   (i) element mass matrix
  \param etforce        *DOUBLE   (i) element time force
  \param eiforce        *DOUBLE   (i) element iteration force
  \param funct          *DOUBLE   (i) shape functions
  \param deriv          *DOUBLE   (i) deriv. of shape funcs
  \param deriv2         *DOUBLE   (i) 2nd deriv. of sh. funcs
  \param xjm            *DOUBLE   (i) jacobian matrix
  \param derxy          *DOUBLE   (i) global derivatives
  \param derxy2         *DOUBLE   (i) 2nd global derivatives
  \param eveln          *DOUBLE   (i) vels at time n
  \param evelng         *DOUBLE   (i) vels at time n+g
  \param epren          *DOUBLE   (i) pres at time n
  \param edeadn         *DOUBLE   (i) dead load at n
  \param edeadng        *DOUBLE   (i) dead load at n+g
  \param velint         *DOUBLE   (i) vel at int point
  \param vel2int        *DOUBLE   (i) vel at int point
  \param covint         *DOUBLE   (i) conv. vel at int point
  \param vderxy         *DOUBLE   (i) global vel. derivatives
  \param vderxy2        *DOUBLE   (i) 2nd global vel. deriv.
  \param wa1            *DOUBLE   (i) working array
  \param wa2            *DOUBLE   (i) working array
  \param sizevec[6]      INT      (i) some sizes

  \return void

  \author mn
  \date   10/04
 */
/*-----------------------------------------------------------------------*/
void f3fcalint(
    ELEMENT         *ele[LOOPL],
    DOUBLE          *elecord,
    DOUBLE          *tau,
    INT             *hasext,
    DOUBLE          *estif,
    DOUBLE          *emass,
    DOUBLE          *etforce,
    DOUBLE          *eiforce,
    DOUBLE          *funct,
    DOUBLE          *deriv,
    DOUBLE          *deriv2,
    DOUBLE          *xjm,
    DOUBLE          *derxy,
    DOUBLE          *derxy2,
    DOUBLE          *eveln,
    DOUBLE          *evelng,
    DOUBLE          *epren,
    DOUBLE          *edeadn,
    DOUBLE          *edeadng,
    DOUBLE          *velint,
    DOUBLE          *vel2int,
    DOUBLE          *covint,
    DOUBLE          *vderxy,
    DOUBLE          *pderxy,
    DOUBLE          *vderxy2,
    DOUBLE          *wa1,
    DOUBLE          *wa2,
    INT              sizevec[6]
    )
{
  INT      l;
  INT      intc;             /* "integration case" for tet for further infos
                                see f3_inpele.c and f3_intg.c */
  INT      nir,nis,nit;      /* number of integration nodes in r,s,t direction */
  INT      actmat;           /* material number of the element */
  INT      icode=2;          /* flag for eveluation of shape functions */
  INT      lr, ls, lt;       /* counter for integration */
  DOUBLE   fac[LOOPL];
  DOUBLE   facr=0.0,facs=0.0,fact=0.0;   /* integration weights */
  DOUBLE   e1,e2,e3;         /* natural coordinates of integr. point */
  DIS_TYP  typ;              /* element type */

  STAB_PAR_GLS    *gls;      /* pointer to GLS stabilisation parameters */
  FLUID_DYNAMIC   *fdyn;
  FLUID_DATA      *data;

  /*Fortran variables - passed to fortran subroutines as parameters.*/
  INT      flagvec[7];
  DOUBLE   paravec[2];

  INT      inttyp;
  DOUBLE   det[LOOPL];
  DOUBLE   facsl,facsr;
  DOUBLE   facsll[LOOPL];
  DOUBLE   preint[LOOPL];
  DOUBLE   ths,thp;


#ifdef DEBUG
  dstrc_enter("f3fcalint");
#endif

  /* initialisation */
  fdyn   = alldyn[genprob.numff].fdyn;
  data   = fdyn->data;
  gls    = ele[0]->e.f3->stabi.gls;
  actmat = ele[0]->mat-1;
  typ    = ele[0]->distyp;

  /* check for proper stabilisation mode */
  dsassert(ele[0]->e.f3->stab_type == stab_gls,
      "routine with no or wrong stabilisation called");



  flagvec[0] = gls->icont;
  flagvec[1] = gls->iadvec;
  flagvec[2] = gls->ivisc;
  flagvec[3] = fdyn->nir;
  flagvec[4] = fdyn->iprerhs;
  flagvec[5] = 0;               /* ihoel */
  flagvec[6] = 0;               /* isale */

  paravec[0] = 1.0;
  paravec[1] = mat[actmat].m.fluid->viscosity;


  /* get integraton data and check if elements are "higher order" */
  intc = 0;
  nir  = 0;
  nis  = 0;
  nit  = 0;
  switch (typ)
  {
    case hex8: case hex20: case hex27:  /* hex - element */
      icode   = 3;
      flagvec[5]   = 1;
      /* initialise integration */
      nir = ele[0]->e.f3->nGP[0];
      nis = ele[0]->e.f3->nGP[1];
      nit = ele[0]->e.f3->nGP[2];
      intc= 0;
      break;

    case tet10: /* tet - element */
      icode   = 3;
      flagvec[5]   = 1;
      /* do NOT break at this point!!! */

    case tet4:    /* initialise integration */
      nir  = ele[0]->e.f3->nGP[0];
      nis  = 1;
      nit  = 1;
      intc = ele[0]->e.f3->nGP[1];
      break;

    default:
      dserror("typ unknown!");
  } /* end switch (typ) */


  if (flagvec[5]!=0 && flagvec[2]!=0)
    switch (flagvec[2]) /* choose stabilisation type --> paravec[0] */
    {
      case 1: /* GLS- */
        paravec[0] = ONE;
        break;
      case 2: /* GLS+ */
        paravec[0] = -ONE;
        break;
      default:
        dserror("viscous stabilisation parameter unknown: IVISC");
    } /* end switch (ele->e.f3->ivisc) */


  /*----------------------------------------------------------------------*
    |               start loop over integration points                   |
   *----------------------------------------------------------------------*/
  for (lr=0;lr<nir;lr++)
  {
    for (ls=0;ls<nis;ls++)
    {
      for (lt=0;lt<nit;lt++)
      {
#ifdef PERF
    perf_begin(51);
#endif
        /* get values of  shape functions and their derivatives */
        switch(typ)
        {
          case hex8: case hex20: case hex27:   /* hex - element */
            e1   = data->qxg[lr][nir-1];
            e2   = data->qxg[ls][nis-1];
            e3   = data->qxg[lt][nit-1];
            facr = data->qwgt[lr][nir-1];
            facs = data->qwgt[ls][nis-1];
            fact = data->qwgt[lt][nit-1];

            if (typ==hex8)
              inttyp=8;
            else if(typ==hex20)
              inttyp=20;
            else if(typ==hex27)
              inttyp=27;

            f3fhex(funct, deriv, deriv2, &e1, &e2, &e3, &inttyp, &icode, sizevec);

            break;
          case tet4: case tet10:  /* tet - element */
            e1   = data->txgr[lr][intc];
            facr = data->twgt[lr][intc];
            e2   = data->txgs[lr][intc];
            facs = ONE;
            e3   = data->txgt[lr][intc];
            fact = ONE;

            if (typ==tet4)
              inttyp=4;
            else if(typ==tet10)
              inttyp=10;

            f3ftet(funct, deriv, deriv2, &e1, &e2, &e3, &inttyp, &icode, sizevec);

            break;
          default:
            dserror("typ unknown!");
        } /* end switch (typ) */
#ifdef PERF
    perf_end(51);
#endif


#ifdef PERF
    perf_begin(52);
#endif
        /* compute Jacobian matrix */
        f3fjaco(funct, deriv, xjm, det, elecord, sizevec);

        for(l=0;l<sizevec[4];l++)
          fac[l] = facr * facs * fact * det[l];
#ifdef PERF
    perf_end(52);
#endif


#ifdef PERF
    perf_begin(53);
#endif
        /* compute global derivates */
        f3fgder(derxy, deriv, xjm, wa1, det, sizevec);
#ifdef PERF
    perf_end(53);
#endif

#ifdef PERF
    perf_begin(54);
#endif
        /* compute second global derivative */
        if (flagvec[5]!=0)
          /*f3fgder2(elecord, xjm, wa1, wa2, derxy, derxy2, deriv2, sizevec);*/
          f3fgder2loop(elecord, xjm, wa1, wa2, derxy, derxy2, deriv2, sizevec);
#ifdef PERF
    perf_end(54);
#endif

#ifdef PERF
    perf_begin(55);
#endif
        /* get velocities (n+g,i) at integraton point */
        f3fveli(velint, funct, evelng, sizevec);
#ifdef PERF
    perf_end(55);
#endif

#ifdef PERF
    perf_begin(56);
#endif
        /* get velocity (n+g,i) derivatives at integration point */
        f3fvder(vderxy, derxy, evelng, sizevec);
#ifdef PERF
    perf_end(56);
#endif

#ifdef PERF
    perf_begin(57);
#endif
        /* compute stabilisation parameter during ntegration &aloopl*/
        if (gls->iduring!=0)
          f3fcalelesize2(ele, velint, wa1, tau, paravec[1], inttyp, sizevec);
#ifdef PERF
    perf_end(57);
#endif



        /*----------------------------------------------------------------------*
          |         compute "Standard Galerkin" matrices                       |
          | NOTE:                                                              |
          |  Standard Galerkin matrices are all stored in one matrix "estif"   |
          |  Standard Galerkin mass matrix is stored in "emass"                |
         *----------------------------------------------------------------------*/
#ifdef PERF
    perf_begin(58);
#endif

        /* compute matrix Mvv */
        if (fdyn->nis==0)
          f3fcalgalm( emass, funct, fac, sizevec);

        /* compute matrix Kvv, Kvp and Kpv*/
        f3fcalgalk(estif, velint, NULL, vderxy, funct, derxy, fac,
            paravec, flagvec, sizevec);

#ifdef PERF
    perf_end(58);
#endif



        /*----------------------------------------------------------------------*
          |         compute Stabilisation matrices                             |
          | NOTE:                                                              |
          |  Stabilisation matrices are all stored in one matrix "estif"       |
          |  Stabilisation mass matrices are all stored in one matrix "emass"  |
         *----------------------------------------------------------------------*/
#ifdef PERF
    perf_begin(59);
#endif

        /* stabilisation for matrix Kvv Kvp Kpv Kpp */
        f3fcalstabk(estif,velint,velint,NULL,vderxy,funct,derxy,derxy2,fac,
            tau, paravec, flagvec, sizevec);


        if (fdyn->nis==0)
        {
          /* stabilisation for matrix Mvv Mpv */
          f3fcalstabm(emass, velint, funct, derxy, derxy2, fac,
                  tau, paravec, flagvec, sizevec);
        }

#ifdef PERF
    perf_end(59);
#endif




        /*----------------------------------------------------------------------*
          |         compute "Iteration" Force Vectors                          |
          |      (for Newton iteration and for fixed-point iteration)          |
         *----------------------------------------------------------------------*/
#ifdef PERF
    perf_begin(60);
#endif
        if (fdyn->nii!=0)
        {
          /* get convective velocities (n+1,i) at integration point */
          f3fcovi( vderxy, velint, covint, sizevec);

          /* calculate galerkin part of "Iter-RHS" (vel dofs) */
          for(l=0;l<sizevec[4];l++)
            facsll[l] = fac[l] * fdyn->thsl * fdyn->sigma;


          /* calculate galerkin and stabilisation of "Iter-RHS" */
          f3fcalif(eiforce,covint,velint,funct,derxy,derxy2,
              facsll, tau, paravec, flagvec, sizevec);

        } /* endif (fdyn->nii!=0) */
#ifdef PERF
    perf_end(60);
#endif



        /*----------------------------------------------------------------------*
          |       compute "external" Force Vector                              |
          |   (at the moment there are no external forces implemented)         |
          |  but there can be due to self-weight /magnetism / etc. (b)         |
          |  dead load may vary over time, but stays constant over             |
          |  the whole domain --> no interpolation with shape funcs            |
          |  parts changing during the nonlinear iteration are added to        |
          |  Iteration Force Vector                                            |
         *----------------------------------------------------------------------*/
#ifdef PERF
    perf_begin(61);
#endif
        if (hasext[0]!=0)
        {
          /* compute stabilisation part of external RHS (vel dofs) at (n+1)*/
          ths=fdyn->thsl;
          thp=fdyn->thpl;

          f3fcalstabexf(eiforce,derxy,derxy2,edeadng,velint,fac,&ths,&thp,
              tau, paravec, flagvec, sizevec);

        } /* endif (*hasext!=0) */
#ifdef PERF
    perf_end(61);
#endif



        /*----------------------------------------------------------------------*
          |         compute "Time" Force Vectors                               |
         *----------------------------------------------------------------------*/
#ifdef PERF
    perf_begin(62);
#endif
        if (fdyn->nif!=0)
        {
          /* get pressure (n) at integration point */
          f3fprei(preint, funct, epren, sizevec);

          /* get pressure derivatives (n) at integration point */
          f3fpder(pderxy, derxy, epren, sizevec);

          /* get velocities (n) at integration point */
          f3fveli( velint, funct, eveln, sizevec);

          /* get velocitie derivatives (n) at integration point */
          f3fvder(vderxy, derxy, eveln, sizevec);

          /* get 2nd velocities derivatives (n) at integration point */
          if (flagvec[5]!=0)
            f3fvder2(vderxy2, derxy2, eveln, sizevec);


          /* get convective velocities (n) at integration point */
          f3fcovi( vderxy, velint, covint, sizevec);


          ths=fdyn->thsr;
          thp=fdyn->thpr;

          /* calculate galerkin and stabilization  part of "Time-RHS" */
          f3fcaltf(etforce,velint,velint,covint,funct,derxy,
              derxy2,vderxy,vderxy2,pderxy,preint,fac,&ths,&thp,
              tau,paravec, flagvec, sizevec);

#ifdef PERF
    perf_end(62);
#endif



          /*-------------------------------------------------------------*
            | compute "external" Force Vector                           |
            | (at the moment there are no external forces implemented)  |
            |  but there can be due to self-weight /magnetism / etc. (b)|
            |  dead load may vary over time, but stays constant over    |
            |  the whole domain --> no interpolation with shape funcs   |
            |  parts staying constant during nonlinear iteration are    |
            |  add to Time Force Vector                                 |
           *-------------------------------------------------------------*/
#ifdef PERF
    perf_begin(63);
#endif

          if (hasext[0]!=0)
          {
            /* compute galerkin part of external RHS (vel dofs) at (n) and (n+1) */
            facsl = fdyn->thsl;
            facsr = fdyn->thsr;

            f3fcalgalexf(etforce, funct, edeadn, edeadng, fac, &facsl, &facsr,
                sizevec);

            /* compute stabilisation part of external RHS at (n) */
            ths=fdyn->thsr;
            thp=fdyn->thpr;

            f3fcalstabexf(etforce,derxy,derxy2,edeadn,velint,fac,&ths,&thp,tau,
                paravec, flagvec, sizevec);

          } /* endif (*hasext!=0) */
#ifdef PERF
    perf_end(63);
#endif
        } /* endif (fdyn->nif!=0)   */

      }
    }
  } /* end of loop over integration points */


#ifdef DEBUG
  dstrc_exit();
#endif

  return;
} /* end of f3_calint */



#endif /* ifdef D_FLUID3_F */

/*! @} (documentation module close)*/


