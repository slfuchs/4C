/*======================================================================*/
/*!
\file
\brief headerfile for 3dim thermal element (SOLID3), containing 
structures and prototypes

<pre>
Maintainer: Moritz Frenzel
            frenzel@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/frenzel
            089-289-15240
</pre>
*/
/*======================================================================*/

#ifdef D_SOLID3

/*!
\addtogroup SOLID3
*//*! @{ (documentation module open)*/

/*----------------------------------------------------------------------*/
/* headers */
#include "../headers/standardtypes.h"

/*======================================================================*/
/* defines constants of SOLID3 */
#define NDIM_SOLID3      (3)    /* 3dim problem */

#ifndef MAXNOD_SOLID3
#ifdef MAXNOD
#define MAXNOD_SOLID3    (MAXNOD)
#else
#define MAXNOD_SOLID3    (27)   /* maximum of element nodes : max hex27 */
#endif
#endif

#ifndef MAXEDG_SOLID3
#define MAXEDG_SOLID3    (12)   /* maximal number element edges */
#endif

#ifndef MAXSID_SOLID3
#define MAXSID_SOLID3    (6)    /* maximal number element sides */
#endif

#ifndef MAXNE_SOLID3
#define MAXNE_SOLID3     (3)    /* maximal number of nodes on an edge */
#endif

#ifndef MAXNS_SOLID3
#define MAXNS_SOLID3     (9)    /* maximal number of nodes on a side */
#endif

#define DIMSID_SOLID3    (2)    /* dimension of element faces */

#define NUMDOF_SOLID3    (3)    /* number of structural DOFs at each node :
                                 * displacement ux uy uz */
#define NUMDFGR_SOLID3   (9)    /* number of deformation gradient components
                                 * (deformation gradient is non-symmetric */
#define NUMSTR_SOLID3    (6)    /* number of strains and stresses :
				 * GL strain: E_XX,E_YY,E_ZZ,2E_XY,2E_YZ,2E_ZX
                                 * 2PK stress: S_XX,S_YY,S_ZZ,S_XY,S_YZ,S_ZX */

#define MAXDOF_SOLID3    (MAXNOD_SOLID3*NUMDOF_SOLID3)  /* maximal element
                                                         * DOFs */

#ifndef MAXGAUSS_SOLID3
#ifdef MAXGAUSS
#define MAXGAUSS_SOLID3  (MAXGAUSS)
#else
#define MAXGAUSS_SOLID3  (27)   /* maximum of total Gauss points in domain */
#endif
#endif

#ifndef GLINTC_SOLID3
#define GLINTC_SOLID3    (6)    /* line domain Gauss integration cases */
#endif

#ifndef GLMAXP_SOLID3
#define GLMAXP_SOLID3    (6)    /* line domain max. number of Gauss points */
#endif
#ifndef GTINTC_SOLID3
#define GTINTC_SOLID3    (3)    /* tetrahedron domain Gauss integration cases*/
#endif

#ifndef GTMAXP_SOLID3
#define GTMAXP_SOLID3    (5)    /* tet domain max. number of Gauss points */
#endif

#ifndef GSINTC_SOLID3
#define GSINTC_SOLID3    (5)    /* triangle domain Gauss integration cases */
#endif

#ifndef GSMAXP_SOLID3
#define GSMAXP_SOLID3    (6)    /* triangle max. number of Gauss points */
#endif

/*======================================================================*/
/* global declarations, variables etc */

/*----------------------------------------------------------------------*/
/*!
\brief Gauss points and weights. This is static data.

Most of the preprocessorily constants are defined on the top of the file.
A few constants are defined in headers/define_sizes.h.

\author mf
\date 10/06
*/
typedef struct _SO3_DATA
{
  /*--------------------------------------------------------------------*/
  /* Gauss coordinates and weights */
  /* hexahedron domain, sides and edges --> line [-1,+1] */
  DOUBLE ghlc[GLINTC_SOLID3][GLMAXP_SOLID3];  /* coordinates */
  DOUBLE ghlw[GLINTC_SOLID3][GLMAXP_SOLID3];  /* weights */
  /* tetrahedron domain [T.J.R. Hughes, "The FEM", Dover 2000] */
  DOUBLE gtdc[GTINTC_SOLID3][GTMAXP_SOLID3][NDIM_SOLID3];  /* coordinates 
                                                            * in r,s,t */
  DOUBLE gtdw[GTINTC_SOLID3][GTMAXP_SOLID3];  /* weights */
  /* tetrahedron sides */
  DOUBLE gtsc[GSINTC_SOLID3][GSMAXP_SOLID3][DIMSID_SOLID3];  /* coordinates 
                                                                in side */
  DOUBLE gtsw[GSINTC_SOLID3][GSMAXP_SOLID3];  /* weights */
  /* triangle edges --> line [0,+1] */
  DOUBLE gtlc[GLINTC_SOLID3][GLMAXP_SOLID3];  /* coordinates */
  DOUBLE gtlw[GLINTC_SOLID3][GLMAXP_SOLID3];  /* weights */
  /*--------------------------------------------------------------------*/
  /* numbering of element nodes, edges, sides in parameter space */
  /* parameter coordinates of nodes */
  DOUBLE nodhrst[MAXNOD_SOLID3][NDIM_SOLID3];  /* hexahedron */
  DOUBLE nodtrst[MAXNOD_SOLID3][NDIM_SOLID3];  /* tetrahedron */
  /* nodes on sides (surfaces) */
  INT nodsidh[MAXSID_SOLID3][MAXNS_SOLID3];  /* hexahedra */
  INT nodsidt[MAXSID_SOLID3][MAXNS_SOLID3];  /* tetrahedra */
  /* nodes on edges */
  INT nodedghl[MAXEDG_SOLID3][MAXNE_SOLID3];  /* linear hex8 */       
  INT nodedghq[MAXEDG_SOLID3][MAXNE_SOLID3];  /* quadratic hex20,27 */
  INT nodedgtl[MAXEDG_SOLID3][MAXNE_SOLID3];  /* linear tet4 */       
  INT nodedgtq[MAXEDG_SOLID3][MAXNE_SOLID3];  /* quadratic tet10 */
  /*--------------------------------------------------------------------*/
  /* anchor and span vectors for sides and edges in param. space */
  /* sides hex */
  DOUBLE ancsidh[MAXSID_SOLID3][NDIM_SOLID3];  /* anchors hex */
  DOUBLE redsidh[MAXSID_SOLID3][DIMSID_SOLID3][NDIM_SOLID3];  /* dim red 
                                                               * matrix */
  /* sides tet */
  DOUBLE ancsidt[MAXSID_SOLID3][NDIM_SOLID3];  /* anchors tet */
  DOUBLE redsidt[MAXSID_SOLID3][DIMSID_SOLID3][NDIM_SOLID3];  /* dim red 
                                                               * matrix */
  /* edges hex */
  DOUBLE ancedgh[MAXEDG_SOLID3][NDIM_SOLID3];  /* anchors hex */
  DOUBLE rededgh[MAXEDG_SOLID3][NDIM_SOLID3];  /* dimension reduction
                                                * matrix multiplied
                                                * on Jacobi matrix */
  /* edges tet */
  DOUBLE ancedgt[MAXEDG_SOLID3][NDIM_SOLID3];  /* anchors tet */
  DOUBLE rededgt[MAXEDG_SOLID3][NDIM_SOLID3];  /* dimension reduction
                                                * matrix multiplied
                                                * on Jacobi matrix */
} SO3_DATA;



/*----------------------------------------------------------------------*/
/*!
\brief All Gauss point coordinates, shape functions and their 
       parametric derivates evaluated
\author bborn
\date 12/06
*/
typedef struct _SO3_GPSHAPEDERIV
{
  DIS_TYP distyp;  /* discretisation */
  INT gpintc[NDIM_SOLID3];  /* Gauss integration case */
  INT gptot;  /* total number of Gauss points in domain */
  DOUBLE gpco[MAXGAUSS_SOLID3][NDIM_SOLID3];
  DOUBLE gpwg[MAXGAUSS_SOLID3];
  DOUBLE gpshape[MAXGAUSS_SOLID3][MAXNOD_SOLID3];
  DOUBLE gpderiv[MAXGAUSS_SOLID3][MAXNOD_SOLID3][NDIM_SOLID3];
} SO3_GPSHAPEDERIV;


/*----------------------------------------------------------------------*/
/*!
\brief The (mostly tensorial) variables of this datum type describe
       the geometric transformations between the 3 frames:
       parameter (r,s,t), material (X,Y,Z) and spatial (x,y,z).
       Moreover a few (mostly tensorial) variables defined in 
       these frames are contained.
       These data are handed down to the material routines.
\author bborn
\date 12/06
*/
typedef struct _SO3_GEODEFSTR
{
  /*--------------------------------------------------------------------*/
  /* geometry:
   *     These variables refer to the transformation between 
   *     parameter frame (r,s,t coordinates) and material frame
   *     (X,Y,Z coordinates)
   */
  /* (FE-) Jacobi matrix J (isoparametric)
   *         [ J_11  J_12  J_13 ]   [ X_{,r}  Y_{,r}  Z_{,r} ]
   *     J = [ J_21  J_22  J_23 ] = [ X_{,s}  Y_{,s}  Z_{,s} ]
   *         [ J_31  J_32  J_33 ]   [ X_{,t}  Y_{,t}  Z_{,t} ] */
  DOUBLE xjm[NDIM_SOLID3][NDIM_SOLID3];
  /* Jacobi determinant det(J) */
  DOUBLE xjdet;
  /* inverted Jacobi matrix J^{-1}*/
  DOUBLE xji[NDIM_SOLID3][NDIM_SOLID3];
  /* rotational component of  J = R . U  ie  R = J . U^{-1} */
  DOUBLE xrm[NDIM_SOLID3][NDIM_SOLID3];
  /*--------------------------------------------------------------------*/
  /* deformation: 
   *     These variables refer to the transformation between
   *     material frame (X,Y,Z coordinates) and spatial frame
   *     (x,y,z coordinates)
   */
  /* material deformation gradient (commonly denoted F)
   *         [ F_11  F_12  F_13 ]
   *     F = [ F_21  F_22  F_23 ]
   *         [ F_31  F_32  F_33 ] */
  DOUBLE defgrd[NDIM_SOLID3][NDIM_SOLID3];
  /* material deformation gradient in vectorial notion Fv
   *     Fv^T = [ F_11  F_22  F_33  F_12  F_21  F_23  F_32  F_31  F_13 ] */
  DOUBLE disgrdv[NUMDFGR_SOLID3];
  /*--------------------------------------------------------------------*/
  /* strain:
   *     Strains referred to different frames */
  /* Green-Lagrange strain tensor in vector notion Ev
   *     Ev^T = [ E_11  E_22  E_33  E_12  E_23  E_31 ] */
  DOUBLE stsglv[NUMSTR_SOLID3];
  /*--------------------------------------------------------------------*/
  /* stress:
   *     Stresses referred to different frames */
  /* 2nd Piola-Kirchhoff stress vector Sv
   *     Sv^T = [ S_11  S_22  S_33  S_12  S_23  S_31 ] */
  DOUBLE str2pkv[NUMSTR_SOLID3];
  /* 1st Piola-Kirchhoff stress vector Pv
   *     Pv^T = [ P_11  P_22  P_33  P_12  P_21  P_23  P_32  P_31  P_13 ] */
  DOUBLE str1pkv[NUMDFGR_SOLID3];
} SO3_GEODEFSTR;


/* /\*----------------------------------------------------------------------*\/ */
/* /\*! */
/* \brief Stress type */

/* \author bborn */
/* \date 12/06 */
/* *\/ */
/* typedef enum _SO3_STRESSTYPE */
/* { */
/*   so3_stress_none,  /\* none *\/ */
/*   so3_stress_lin,  /\* linear stress *\/ */
/*   so3_stress_pk2,  /\* 2nd Piola--Kirchhoff stress *\/ */
/*   so3_stress_pk1,  /\* 1st Piola--Kirchhoff stress *\/ */
/*   so3_stress_ct  /\* Cauchy (true) stress *\/ */
/* } SO3_STRESSTYPE; */


/* /\*----------------------------------------------------------------------*\/ */
/* /\*! */
/* \brief Strain measure */
/* \author bborn */
/* \date 12/06 */
/* *\/ */
/* typedef enum _SO3_STRNMEAS */
/* { */
/*   so3_strnmeas_none,  /\* none *\/ */
/*   so3_strnmeas_lin,  /\* linear strain *\/ */
/*   so3_strnmeas_gl,  /\* Green-Lagrange strain *\/ */
/*   so3_strnmeas_ea  /\* Euler-Almansi strain *\/ */
/* } SO3_STRNMEAS; */


/*----------------------------------------------------------------------*/
/*!
\brief Type of kinematics

\author mf
\date 10/06
*/
typedef enum _SO3_KINEMATICS
{
  so3_geo_lin,      /* geometrically linear */
  so3_total_lagr,   /* geometrically non-linear => total Lagrangian */
  so3_updated_lagr  /* geometrically non-linear => updated Lagrangian */
} SO3_KINEMATICS;


/*----------------------------------------------------------------------*/
/*!
\brief Type of stress output

\author mf
\date 10/06
*/
typedef enum _SO3_STRESSOUT
{
  so3_stress_none,
  so3_stress_gpxyz,  /* globally xyz-oriented at Gauss points */
  so3_stress_gp123,  /* in principal directions */
  so3_stress_gprst,  /* locally rs-oriented at Gauss points */
  so3_stress_gpxyz123,  /* all of the above */
  so3_stress_ndxyz,     /* the same for nodes*/
  so3_stress_nd123,
  so3_stress_ndrst,
  so3_stress_ndxyz123
} SO3_STRESSOUT;


/*----------------------------------------------------------------------*/
/*!
\brief Definition of SOLID3 type holding SOLID3 element properties

\author mf
\date 10/06
*/
/*----------------------------------------------------------------------*/
typedef struct _SOLID3
{
  SO3_KINEMATICS kintype;  /* type of kinematics */
  SO3_STRESSOUT stresstype;  /* output type of stress */

  /* number of Gauss points as obtained at read-in
   * hexahedra:
   *       nGP[0] : in r-direction : 1,2,3,4,5,6 : read-in
   *       nGP[1] : in s-direction : 1,2,3,4,5,6 : read-in/set
   *       nGP[2] : in t-direction : 1,2,3,4,5,6 : read-in/set
   * tetrahedra:
   *       nGP[0] : total number of GPs in domain: 1,4,5 : read-in
   *       nGP[1] : total number of GPs on sides: 1,3,4,6 : read-in/set
   *       nGP[2] : total number of GPs on edges: 1,2,3,4,5,6 : read-in/set */
  INT gpnum[NDIM_SOLID3];
  INT gpintc[NDIM_SOLID3];

  /* stress vector at Gauss points or nodes
   *    stress_gpxyz : at Gauss points in global XYZ-components
   *    stress_gprst : at Gauss points in parameter rst-compoonents
   *    stress_gp123 : at Gauss points modulus and angles vs XYZ-direct. */
  ARRAY4D stress_gpxyz;
  ARRAY4D stress_gprst;
  ARRAY4D stress_gp123;
  ARRAY4D stress_ndxyz;
  ARRAY4D stress_ndrst;
  ARRAY4D stress_nd123;

  /* thermo-structure-interaction */
#ifdef D_TSI
  TSI_COUPTYP tsi_couptyp;  /* TSI coupling type */
  ELEMENT *therm_ele;      /* pointer to conforming thermal2 element */
#endif

} SOLID3;






/*======================================================================*/
/* Declarations of functions in solid3 directory
 * Order: Firstly, alphabetically list file names, secondly, list
 *        functions in file according to appearance */


/*----------------------------------------------------------------------*/
/* file so3_bop.c */
void so3_bop(INT        enod,
             DOUBLE     deriv[MAXNOD_SOLID3][NDIM_SOLID3],
             DOUBLE     xji[NDIM_SOLID3][NDIM_SOLID3],
             DOUBLE     bop[NDIM_SOLID3][NUMDOF_SOLID3*MAXNOD_SOLID3]);

/*---------------------------------------------------------------------*/
/* file so3_cfg.c */
void so3_cfg_chkdef();
void so3_cfg_init(SO3_DATA *data);
void so3_cfg_noderst(ELEMENT *ele,
                     SO3_DATA *data,
                     INT inode,
                     DOUBLE *rst)

/*----------------------------------------------------------------------*/
/* file so3_inp.c */
void so3_inp(ELEMENT *ele);


/*----------------------------------------------------------------------*/
/* file so3_intg.c */
void so3_intg_eleinp(ELEMENT *actele,
                     INT *ierr);
void so3_intg_init(SO3_DATA *data);

/*----------------------------------------------------------------------*/
/* file so3_metr.c */
void so3_metr_jaco(ELEMENT *ele,
                   INT      enod,
                   DOUBLE   deriv[MAXNOD_SOLID3][NDIM_SOLID3],
                   INT      flag,
                   DOUBLE   xjm[NDIM_SOLID3][NDIM_SOLID3],
                   DOUBLE  *det,
                   DOUBLE   xji[NDIM_SOLID3][NDIM_SOLID3]);
void so3_metr_surf(ELEMENT *ele, 
                   INT      nelenod, 
                   DOUBLE   deriv[MAXNOD_SOLID3][NDIM_SOLID3], 
                   DOUBLE   sidredm[DIMSID_SOLID3][NDIM_SOLID3],
                   DOUBLE  *metr);
void so3_metr_line(ELEMENT *ele, 
                   INT      nelenod, 
                   DOUBLE   deriv[MAXNOD_SOLID3][NDIM_SOLID3], 
                   DOUBLE   linredv[NDIM_SOLID3],
                   DOUBLE  *metr);

/*----------------------------------------------------------------------*/
/* file so2_stiff.c */
void so3_stiff(ELEMENT *ele,
                   SO3_DATA *data,
                   MATERIAL *mat,
                   ARRAY *estif_global,
                   ARRAY *emass_global,
                   DOUBLE *force);
void so3_stiff_bcb(INT neledof,
                   DOUBLE bop[NUMSTRN_SOLID3][NUMDOF_SOLID3*MAXNOD_SOLID3],
                   DOUBLE cmat[NUMSTSS_SOLID3][NUMSTRN_SOLID3],
                   DOUBLE fac,
                   DOUBLE **tmat);
void so3_stiff_fint(INT neledof,
                    DOUBLE bop[NUMSTRN_SOLID3][NUMDOF_SOLID3*MAXNOD_SOLID3],
                    DOUBLE stress[NUMSTSS_SOLID3],
                    DOUBLE fac,
                    DOUBLE *intfor);

/*----------------------------------------------------------------------*/
/* file th3_load.c */
void so3_eleload(ELEMENT *ele,  /* actual element */
                   TH3_DATA *data,
                   INT imyrank,
                   DOUBLE *loadvec); /* global element load vector fext */
void so3_load_vol(ELEMENT *ele,
                  INT nelenod,
                  DOUBLE shape[MAXNOD_SOLID3],
                  DOUBLE fac,
                  DOUBLE eload[NUMDOF_SOLID3][MAXNOD_SOLID3]);
void so3_load_surf(ELEMENT *ele,
                   INT nelenod,
                   GSURF *gsurf,
                   DOUBLE shape[MAXNOD_SOLID3],
                   DOUBLE fac,
                   DOUBLE eload[NUMDOF_SOLID3][MAXNOD_SOLID3]);
void so3_load_line(ELEMENT *ele,
                   INT nelenod,
                   GLINE *gline,
                   DOUBLE shape[MAXNOD_SOLID3],
                   DOUBLE fac,
                   DOUBLE eload[NUMDOF_SOLID3][MAXNOD_SOLID3]);

/*----------------------------------------------------------------------*/
/* file so3_main.c */
void solid3(PARTITION *actpart,
	    INTRA *actintra,
	    ELEMENT *ele,
	    ARRAY *estif_global,
	    ARRAY *emass_global,
	    ARRAY *intforce_global,
	    CALC_ACTION *action,
	    CONTAINER *container);   /* contains variables defined 
				      * in container.h */

/*----------------------------------------------------------------------*/
/* file so3_mat.c */
void so3_mat_sel(ELEMENT *ele,
                 MATERIAL *mat,
                 DOUBLE bop[NUMSTRN_SOLID3][NUMDOF_SOLID3*MAXNOD_SOLID3],
                 INT ip,
                 DOUBLE stress[NUMSTSS_SOLID3],
                 DOUBLE cmat[NUMSTSS_SOLID3][NUMSTRN_SOLID3]);

/*----------------------------------------------------------------------*/
/* file so3_shape.c */
void so3_shape_deriv(DIS_TYP     typ,
                     DOUBLE      r,
                     DOUBLE      s,
                     DOUBLE      t,
                     INT         option,
                     DOUBLE      shape[MAXNOD_SOLID3],
                     DOUBLE      deriv[MAXNOD_SOLID3][NDIM_SOLID3]);

/*----------------------------------------------------------------------*/
#endif /*end of #ifdef D_SOLID3 */
/*! @} (documentation module close) */
