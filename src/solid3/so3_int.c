/*======================================================================*/
/*!
\file
\brief (tangent) stiffness matrix, 
       mass matrix,
       internal forces of SOLID3 element

<pre>
Maintainer: Moritz Frenzel
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089-289-15240
</pre>
*/


/*----------------------------------------------------------------------*/
#ifdef D_SOLID3

/*----------------------------------------------------------------------*/
/* headers */
#include "../headers/standardtypes.h"
#include "solid3.h"

/*!
\addtogroup SOLID3
*//*! @{ (documentation module open)*/


/*----------------------------------------------------------------------*/
/*!
\brief General problem data

\author mf
\date 10/06
*/
extern GENPROB genprob;



/*======================================================================*/
/*!
\brief Calculate linear stiffness matrix of SOLID3 element

The element stiffness matrix, ie the tangent operator, is determined
for the linear, 3dim elasticity

\param   *ele           ELEMENT     (i)  pointer to current element
\param   *data          SO3_DATA    (i)  common element data
\param   *estif_global  ARRAY       (o)  element stiffness matrix
\param   *emass_global  ARRAY       (o)  element mass matrix
\param   *force         DOUBLE      (o)  global vector for internal 
                                           forces (initialized!)
\return void

\author mf
\date 10/06
*/
void so3_int_fintstiffmass(ELEMENT *ele,
                           SO3_GPSHAPEDERIV *gpshade,
                           MATERIAL *mat,
                           ARRAY *estif_global,
                           ARRAY *emass_global,
                           DOUBLE *eforce_global)
{
  /* general variables/constants */
  INT nelenod;  /* numnp of this element */
  INT neledof;  /* total number of element DOFs */
  DOUBLE ex[MAXNOD_SOLID3][NDIM_SOLID3];  /* material coord. of element */
  DOUBLE edis[MAXNOD_SOLID3][NDIM_SOLID3];  /* cur. element displacements */

  /* integration */
  INT ngp;  /* total number of Gauss points in domain */
  INT igp;  /* current total index of Gauss point */
  DOUBLE gpcr;  /* r-coord current GP */
  DOUBLE gpcs;  /* s-coord current GP */
  DOUBLE gpct;  /* t-coord current GP */
  DOUBLE fac;  /* integration factors */

  /* quantities at Gauss point */
  DOUBLE shape[MAXNOD_SOLID3];  /* shape functions */
  DOUBLE deriv[MAXNOD_SOLID3][NDIM_SOLID3];   /* shape fct. derivatives */
  DOUBLE xjm[NDIM_SOLID3][NDIM_SOLID3];  /* Jacobian matrix */
  DOUBLE det;  /* Jacobi determinant */
  DOUBLE xji[NDIM_SOLID3][NDIM_SOLID3];  /* inverse Jacobian matrix */
  DOUBLE bop[NDIM_SOLID3][MAXDOF_SOLID3]; /* B-operator */
  DOUBLE cmat[NUMSTR_SOLID3][NUMSTR_SOLID3];  /* material matrix */
  DOUBLE stress[NUMSTR_SOLID3];  /* stress */

  /* convenience */
  DOUBLE estif[MAXDOF_SOLID3][MAXDOF_SOLID3];  /* element stiffness matrix */
  DOUBLE emass[MAXDOF_SOLID3][MAXDOF_SOLID3];  /* element stiffness matrix */
  DOUBLE eforce[MAXDOF_SOLID3];  /* element internal force vector */

  /*--------------------------------------------------------------------*/
  /* start */
#ifdef DEBUG
  dstrc_enter("so3_int_fintstiffmass");
#endif

  /*--------------------------------------------------------------------*/
  /* element matrix fields are reused for every element, thus
  /* have to be reinitialized to zero */
  amzero(estif_global);  /* element tangent matrix */
  estif = estif_global->a.da;
  amzero(emass_global); /* element mass matrix */
  emass = emass_global->a.da;

  /*--------------------------------------------------------------------*/
  /* element properties */
  nelenod = ele->numnp;
  neledof = NUMDOF_SOLID3 * nelenod;
  for (inod=0; inod<enod; inod++)
  {
    actnode = ele->node[inod];
    for (jdim=0; jdim<NDIM_SOLID3; jdim++)
    {
      ex[inod][jdim] = actnode->x[jdim];
      /* edis[inod][jdim] = actnode->sol[][jdim]; */
    }
  }
  ngp = gpshade->gptot;  /* total number of Gauss points in domain */

  /*--------------------------------------------------------------------*/
  /* integration loop */
  for (igp=0; igp<ngp; igp++)
  {
    gpcr = gpshade->gpco[igp][0];  /* r-coordinate */
    gpcs = gpshade->gpco[igp][1];  /* s-coordinate */
    gpct = gpshade->gpco[igp][2];  /* t-coordinate */
    fac = gpshade->gpwg[igp];  /* weight */
    /*------------------------------------------------------------------*/
    /* compute Jacobian matrix, its determinant and inverse */
    so3_metr_jaco(ele, nelenod, ex, gpshade->gpderiv[igp], 1, 
                     xjm, &det, xji);
    /*------------------------------------------------------------------*/
    /* integration (quadrature) factor */
    fac = fac * det;
    /*------------------------------------------------------------------*/
    /* deformation tensor and displacement gradient */
    so3_def_grad(enod, edis, deriv, xji, disgrdv, defgrd);
    /*------------------------------------------------------------------*/
    /* strain vector */
    if (ele->e.so3->kintype == so3_geo_lin)
    {
      so3_strain_lin(ele, disgrdv, strain);
    }
    else if (ele->e.so3->kintype == so3_total_lagr)
    {
      so3_strain_gl(ele, disgrdv, strain);
    }
    else
    {
      dserror("Cannot digest chosen type of spatial kinematic\n");
    }
    /*------------------------------------------------------------------*/
    /* calculate B-operator */
    so3_bop(nelenod, deriv, xji, bopl, bop);
    /*------------------------------------------------------------------*/
    /* call material law */
    so3_mat_sel(ele, mat, igp, strain, stress, cmat);
    /*------------------------------------------------------------------*/
    /* element internal force from integration of stresses */
    if (eforce_global)
    {
      so3_int_fintcont(neledof, bop, stress, fac, eforce);
    }
    /*------------------------------------------------------------------*/
    /* element stiffness matrix */
    if (estif_global)
    {
      /* geometrically linear kinematics (in space) */
      if (ele->e.so3->kintype == so3_geo_lin)
      {
        /* `elastic' stiffness */
        so3_int_stiffbcb(neledof, bop, cmat, fac, estif);
      }
      /* geometrically non-linear kinematics (in space) */ 
      else if (ele->e.so3->kintype == so3_total_lagr)
      {
        /* `elastic' and `initial-displacement' stiffness */
        so3_int_stiffbcb(neledof, bop, cmat, fac, estif);
        /* `geometric' stiffness */
        so3_int_stiffgeo(enod, bopn, stress, fac, estif);
      }
      /* catch unknown spatial kinematics */
      else
      {
        dserror("Cannot digest chosen type of spatial kinematic\n");
      }
    }
    /*------------------------------------------------------------------*/
    /* element mass matrix */
    if (emass_global)
    {
      so3_int_mass(enod, dens, gpshade->gpshape[igp], emass);
    }
  }

  /*--------------------------------------------------------------------*/
  /* local co-system */
  dsassert(ele->locsys == locsys_no,
           "locsys not implemented for this element!\n");

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
} /* end of so3_lin_stiff(...) */


/*======================================================================*/
/*!
\brief Evaluate element nodal forces

The element nodal forces 'fie' are incremented by the contribution
of the current Gauss point

\param     neledof   INT      (i)    number of element nodes
\param     bop[][]   DOUBLE   (i)    B-operator for current GP
\param     stress[]  DOUBLE   (i)    stress for current GP
\param     fac       DOUBLE   (i)    Gauss quadrature factor mult. etc.
\param     intfor[]  DOUBLE   (io)   element internal force
\return void

\author mf
\date 03/06
*/
void so3_int_fintcont(INT neledof,
                      DOUBLE bop[NUMSTR_SOLID3][MAXDOF_SOLID3],
                      DOUBLE stress[NUMSTR_SOLID3],
                      DOUBLE fac,
                      DOUBLE intfor[MAXDOF_SOLID3])
{
  INT istr, idof;  /* counters */
  DOUBLE intforidof;  /* stress multiplied by 'fac' */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("so3_int_fintcont");
#endif

  /*--------------------------------------------------------------------*/
  /* f_int = B . Sv */
  for (idof=0; idof<neledof; idof++)
  {
    intforidof = 0.0;
    for (istr=0, istr<NUMSTR_SOLID3; istr++)
    {
      intforidof += bop[istr][idof]*stress[istr]*fac;
    }
    intfor[idof] += intforidof;
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end of so3_lin_fint(...) */



/*======================================================================*/
/*!
\brief Add so-called elastic (and initial-displacement) stiffness
       matrix at Gauss point to element stiffness matrix.
       This is done by the famous classic B^T C B operation.
       The B-operator carries only in a total Lagrangean setting the
       geometrically non-linear initial-displacement part.

\param   neledof    INT     (i)    number of element DOFs
\param   bop[][]    DOUBLE  (i)    B-operator
\param   cmat[][]   DOUBLE  (i)    constitutive matrix
\param   fac        DOUBLE  (i)    integration factor of current GP
\param   stif[][]   DOUBLE  (io)   element stiffness matrix increment of
                                     current Gauss point
\return  void

\author mf
\date 10/06
*/
void so3_int_stiffbcb(INT neledof,
                      DOUBLE bop[NUMSTR_SOLID3][MAXDOF_SOLID3],
                      DOUBLE cmat[NUMSTR_SOLID3][NUMSTR_SOLID3],
                      DOUBLE fac,
                      DOUBLE stif[MAXDOF_SOLID3][MAXDOF_SOLID3])
{
  INT istr, jstr, idof, jdof;  /* counters */
  DOUBLE bopcmatistr, tidofjdof;  /* intermediate sums */
  DOUBLE bopcmat[NUMSTR_SOLID3];  /* bopcmat_ki = bop_kj * cmat_ji */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("so3_int_stiffbcb");
#endif

  /*--------------------------------------------------------------------*/
  for (idof=0; idof<neledof; idof++)
  {
    /* bopcmat_ki = bop_kj * cmat_ji */
    for (istr=0; istr<NUMSTR_SOLID3; istr++)
    {
      bopcmatistr = 0.0;
      for (jstr=0; jstr<NUMSTR_SOLID3; jstr++)
      {
        bopcmatistr += bop[jstr][idof]*cmat[jstr][istr]*fac;
      }
      bopcmat[istr] = bopcmatistr;
    }
    /* tmat_kl = bopcmat_ki * bop_il */
    for (jdof=0; jdof<neledof; jdof++)
    {
      stidofjdof = 0.0;
      for (istr=0; istr<NUMSTR_SOLID3; istr++)
      {
        stidofjdof += bopcmat[istr]*bop[istr][jdof];
      }
      stif[idof][jdof] += stidofjdof;
    }
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
   dstrc_exit();
#endif
   return;
} /* end of so3_stiff_bcb(...) */



/*======================================================================*/
/*!
\brief Add so-called geometric stiffness matrix at Gauss point
       to element stiffness matrix

\param   enod       INT         (i)   number of element nodes 
\param   bopn[][]   DOUBLE      (i)   B-operator
\param   stress[]   DOUBLE      (i)   stress vector
\param   fac        DOUBLE      (i)   Gaussian integration factor
\param   estif[][]  DOUBLE      (io)  element stiffness matrix
\return  void

\author bborn
\date 12/06
*/
void so3_int_stiffgeo(INT enod,
                      DOUBLE bopn[MAXDOF_SOLID3][NUMDOF_SOLID3],
                      DOUBLE stress[NUMSTR_SOLID3],
                      DOUBLE fac,
                      DOUBLE estif[MAXDOF_SOLID3][MAXDOF_SOLID3])
{
  DOUBLE S11, S12, S13, S21, S22, S23, S31, S32, S33;  /* stress compo. */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("so3_int_stiffgeo");
#endif

  /*--------------------------------------------------------------------*/
  /* set auxiliar stress components multiplied by Gaussian factor */
  S11 = fac*stress[0];
  S22 = fac*stress[1];
  S33 = fac*stress[2];
  S12 = S21 = fac*stress[3];
  S23 = S32 = fac*stress[4];
  S31 = S13 = fac*stress[5];

  /*--------------------------------------------------------------------*/
  /* loop 1st direction of nodes */
  for (inod=0; inod<enod; inod++)
  {
    /* intermediate Bn-matrix entries */
    bopinod[0] = bopn[inod][0];  /* N_{,1}^k with k=inod */
    bopinod[1] = bopn[inod][1];  /* N_{,2}^k with k=inod */
    bopinod[2] = bopn[inod][2];  /* N_{,3}^k with k=inod */
    /* intermediate Sm . Blin */
    strbopinod[0] = S11*bopinod[0] + S12*bopinod[1] + S13*bopinod[2];
    strbopinod[1] = S21*bopinod[0] + S22*bopinod[1] + S23*bopinod[2];
    strbopinod[2] = S31*bopinod[0] + S32*bopinod[1] + S33*bopinod[2];
    /* loop 2nd direction of nodes */
    for (jnod=0; jnod<enod; jnod++)
    {
      /* Blin . Sm . Blin */
      bopstrbop = 0.0;
      for (idim=0; idim<NDIM_SOLID3; idim++)
      {
        bopstrbop += bopn[jnod][idim]*strbopinod[idim];
      }
      /* add contribution to element stiffness matrix */
      for (idof=inod*NDIM_SOLID3; idof<(inod+1)*NDIM_SOLID3; idof++)
      {
        for (jdof=jnod*NDIM_SOLID3; jdof<(jnod+1)*NDIM_SOLID3; jdof++)
        {
          estif[idof][jdof] += bopstrbop;
        }
      }
    }
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}




/*======================================================================*/
/*!
\brief element mass matrix contribution of current Gauss point


WHAT ABOUT _CONSISTENT_ AND _LUMPED_ MASS MARICES?
       ===> CURRENTLY, _LUMPING_ IS _NOT_ IMPLEMENTED!
       ===> GO TO YOUR KEYBOARD AND TAKE ON THE QUEST.


\param   nnod       INT     (i)  number of element nodes
\param   density    DOUBLE  (i)  density (indeed)
\param   shape[]    DOUBLE  (i)  shape functions at Gauss point
\param   fac        DOUBLE  (i)  Gaussian integration factor 
\param   emass[][]  DOUBLE  (io) element mass matrix
\return  void

\author bborn
\date 12/06
*/
void so3_int_mass(INT nnod,
                  DOUBLE density,
                  DOUBLE shape[MAXNOD_SOLID3],
                  DOUBLE fac,
                  DOUBLE emass[MAXDOF_SOLID3][MAXDOF_SOLID3])
{
  INT inod, jnod;  /* loop indices */
  DOUBLE shapeinod;  /* temporary (inod)th shape function */
  DOUBLE mascom[MAXNOD_SOLID3][MAXNOD_SOLID3];  /* compact mass matrix 
                                                 * containing only mass
                                                 * for one direction */
  INT idof, jdof;  /* loop indices for DOFs */
  
  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("so3_int_mass");
#endif

  /*--------------------------------------------------------------------*/
  /* compact mass matrix contribution at current Gauss point */
  /* m = N^T . rho * N */
  for (inod=0; inod<nnod; inod++)
  {
    shapeinod = fac * density * shape[inod];
    for (jnod=0; jnod<nnod; jnod++)
    {
      mascom[inod][jnod] = shapeinod * shape[jnod] 
    }
  }

  /*--------------------------------------------------------------------*/
  /* explode compact mass matrix to element mass matrix */
  /* consistent mass matrix:
   *            [ ... |         ...     ...    ...  | ... ]
   *        /   [ ~~~   ~~~~~~~~~~~   ~~~~~   ~~~~~   ~~~ ]
   *        |   [ ... | N^i*rho*N^j       0       0 | ... ]
   *  m^e = I   [ ... |         N^i*rho*N^j       0 | ... ] |J| dOmega
   *        |   [ ... | sym             N^i*rho*N^j | ... ]
   *        /   [ ~~~   ~~~~~~~~~~~   ~~~~~   ~~~~~   ~~~ ]
   *    Omega^e [ ... |         ...     ...     ... | ... ]
   */
  for (inod=0; inod<nnod; inod++)
  {
    for (idim=0; idim<NDIM_SOLID3; idim++)
    {
      idof = inod*NDIM_SOLID3 + idim;
      for (jnod=0; jnod<nnod; jnod++)
      {
        jdof = jnod*NDIM_SOLID3 + idim;
        emass[idof][jdof] += mascom[inod][jnod];
      }
    }
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}

/*======================================================================*/
#endif /* end of #ifdef D_SOLID3 */
/*! @} (documentation module close)*/
