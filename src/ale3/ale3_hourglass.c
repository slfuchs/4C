/*!----------------------------------------------------------------------
\file
\brief contains the routine 'ale3_hourglass' which calculates the
hourglass stabilization matrix for a 3D ale element

<pre>
Maintainer: Malte Neumann
            neumann@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/neumann/
            0711 - 685-6121
</pre>

*----------------------------------------------------------------------*/
#ifdef D_ALE
#include "../headers/standardtypes.h"
#include "ale3.h"
/*----------------------------------------------------------------------*
 |                                                         mn 06/02     |
 | vector of material laws                                              |
 | defined in global_control.c
*----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;

/*!
\addtogroup Ale
*//*! @{ (documentation module open)*/

/*!----------------------------------------------------------------------
\brief calculates the additional stiffness matrix for hourglass stabilization

<pre>                                                              mn 06/02
This routine calcuates the additional stiffness matrix for hourglass
stabilization for a 3D element.

see also:
   (1) T. Belytschko and L.P. Bindeman:
       Assumed strain stabilization of the 8-node hexahedral element
       Comp. Meth. Appl. Mech. Eng.: 105 (1993) p. 225-260.
   (2) D.P. Flanagan and T. Belytschko:
       A uniform strain hexahedron and quadrilateral with orthogonal
       hourglass control
       Int. J. Num. Meth. Ing.: Vol. 17 (1981) p. 679-706.

</pre>
\param *ele  ELEMENT  (i)   the element
\param **s   DOUBLE   (i/o) (i) the one point quadrature matrix
                            (o) the complete, stabilized stiffness matrix
\param vol   DOUBLE   (i)   the volume of the element

\warning There is nothing special to this routine
\return void
\sa calling: ---; called by: ale3_static_ke

*----------------------------------------------------------------------*/
void ale3_hourglass(
    ELEMENT  *ele,
    DOUBLE  **s,
    DOUBLE    vol
    )
{

  MATERIAL      *actmat;

  INT            i,j,k,l;
  INT            l0=0,l1=0,l2=0,l3=0,l4=0,l5=0,l6=0,l7=0;

  DOUBLE         x[3][8];
  DOUBLE         xc[3][8];
  DOUBLE         b[3][8];

  DOUBLE         a[3][3];
  DOUBLE         ba[3];
  DOUBLE         r[3][3];

  DOUBLE         h[4][8] = {{1,1,-1,-1,-1,-1,1,1},{1,-1,-1,1,-1,1,1,-1},
                            {1,-1,1,-1,1,-1,1,-1},{-1,1,-1,1,1,-1,1,-1}};
  DOUBLE         lam[3][8] = {{-1,1,1,-1,-1,1,1,-1},
                              {-1,-1,1,1,-1,-1,1,1},
                              {-1,-1,-1,-1,1,1,1,1}};
  DOUBLE         gam[4][8];
  DOUBLE         lx[3];
  DOUBLE         hh[3][3];

  DOUBLE         gg00[8][8], gg11[8][8], gg22[8][8], gg33[8][8];
  DOUBLE         gg01[8][8], gg10[8][8], gg02[8][8], gg20[8][8];
  DOUBLE         gg12[8][8], gg21[8][8];

  DOUBLE         kstab[24][24];

  DOUBLE         c1,c2,c3;
  DOUBLE         dum;
  DOUBLE         ee,nu,mu;


#ifdef DEBUG
  dstrc_enter("ale3_hourglass");
#endif
  /* material data */
  actmat = &(mat[ele->mat-1]);
  ee = actmat->m.stvenant->youngs;
  nu = actmat->m.stvenant->possionratio;
  mu = ee / (2*(1+nu));


  /* Constants for the stabilization matrix accord. to (1) Table*/
  /* ADS */
#if 0
  c1 = 2.0/3.0;
  c2 = 2.0/9.0;
  c3 = -1.0/3.0;
#endif

  /*ASQBI */
  c1 = 1.0/(1.0 - nu);
  c2 = (1.0 + nu)/3;
  c3 = 1.0/(1.0 - nu);


  /* nodal coordinates */
  for(i=0; i<3; i++)
  {
    for(j=0; j<8; j++)
    {
      x[i][j] = ele->node[j]->x[i];
    }
  }


  /* corotational coordinate system: rotation tensor r[3][3] */
  /* accord. to (1) Appendix A, Table 9 */
  for (i=0; i<2; i++)
  {
    for (j=0; j<3; j++)
    {
      a[i][j] = 0,0;
      for (k=0; k<8; k++)
      {
        a[i][j] += lam[i][k]*x[j][k];
      }
    }
  }

  dum =(a[0][0]*a[1][0]+a[0][1]*a[1][1]+a[0][2]*a[1][2])/
    (a[0][0]*a[0][0]+a[0][1]*a[0][1]+a[0][2]*a[0][2]);

  a[1][0] = a[1][0] - dum * a[0][0];
  a[1][1] = a[1][1] - dum * a[0][1];
  a[1][2] = a[1][2] - dum * a[0][2];

  a[2][0] = a[0][1]*a[1][2] - a[0][2]*a[1][1];
  a[2][1] = a[0][2]*a[1][0] - a[0][0]*a[1][2];
  a[2][2] = a[0][0]*a[1][1] - a[0][1]*a[1][0];

  for(i = 0; i<3; i++)
  {
    ba[i] = sqrt(a[i][0]*a[i][0]+a[i][1]*a[i][1]+a[i][2]*a[i][2]);
    r[i][0] = a[i][0] / ba[i];
    r[i][1] = a[i][1] / ba[i];
    r[i][2] = a[i][2] / ba[i];
  }


  /* transforming nodal coordinates to the corotational system */
  for(i=0; i<8; i++)
  {
    for(j=0; j<3; j++)
    {
      xc[j][i] = r[j][0]*x[0][i]+r[j][1]*x[1][i]+r[j][2]*x[2][i];
    }
  }


  /* B-matrix b[3][8] accord. to (2) Appendix I, eqn (79) */
  for (i=0; i<3; i++)
  {
    j = (i+1)%3;
    k = (j+1)%3;
    for(l=0; l<8;l++)
    {
      switch(l)
      {
        case 0:
          l0=0;l1=1;l2=2;l3=3;l4=4;l5=5;l6=6;l7=7;
          break;
        case 1:
          l0=1;l1=2;l2=3;l3=0;l4=5;l5=6;l6=7;l7=4;
          break;
        case 2:
          l0=2;l1=3;l2=0;l3=1;l4=6;l5=7;l6=4;l7=5;
          break;
        case 3:
          l0=3;l1=0;l2=1;l3=2;l4=7;l5=4;l6=5;l7=6;
          break;
        case 4:
          l0=4;l1=7;l2=6;l3=5;l4=0;l5=3;l6=2;l7=1;
          break;
        case 5:
          l0=5;l1=4;l2=7;l3=6;l4=1;l5=0;l6=3;l7=2;
          break;
        case 6:
          l0=6;l1=5;l2=4;l3=7;l4=2;l5=1;l6=0;l7=3;
          break;
        case 7:
          l0=7;l1=6;l2=5;l3=4;l4=3;l5=2;l6=1;l7=0;
          break;
      }
      b[i][l0] =1/(12 * vol) * (
          xc[j][l1]*((xc[k][l5] - xc[k][l2])-(xc[k][l3] - xc[k][l4]))
          + xc[j][l2]* (xc[k][l1] - xc[k][l3])
          + xc[j][l3]*((xc[k][l2] - xc[k][l7])-(xc[k][l4] - xc[k][l1]))
          + xc[j][l4]*((xc[k][l7] - xc[k][l5])-(xc[k][l1] - xc[k][l3]))
          + xc[j][l5]* (xc[k][l4] - xc[k][l1])
          + xc[j][l7]* (xc[k][l3] - xc[k][l4]) );
    }
  }


  /* gamma vectors, accord. to (1) eqn (2.12b) */
  for(i=0; i<4; i++)
  {
    for(j=0; j<8; j++)
    {
      gam[i][j] = 0.125 * h[i][j];
      for(k=0; k<3; k++)
      {
        dum = h[i][0]*xc[k][0]+h[i][1]*xc[k][1]+h[i][2]*xc[k][2]+
          h[i][3]*xc[k][3]+h[i][4]*xc[k][4]+h[i][5]*xc[k][5]+
          h[i][6]*xc[k][6]+h[i][7]*xc[k][7];
        gam[i][j] -= 0.125 * dum * b[k][j];
      }
    }
  }


  /* lambda * x (auxiliary vector) */
  lx[0] = lam[0][0]*xc[0][0]+lam[0][1]*xc[0][1]+lam[0][2]*xc[0][2]+
    lam[0][3]*xc[0][3]+lam[0][4]*xc[0][4]+lam[0][5]*xc[0][5]+
    lam[0][6]*xc[0][6]+lam[0][7]*xc[0][7];
  lx[1] = lam[1][0]*xc[1][0]+lam[1][1]*xc[1][1]+lam[1][2]*xc[1][2]+
    lam[1][3]*xc[1][3]+lam[1][4]*xc[1][4]+lam[1][5]*xc[1][5]+
    lam[1][6]*xc[1][6]+lam[1][7]*xc[1][7];
  lx[2] = lam[2][0]*xc[2][0]+lam[2][1]*xc[2][1]+lam[2][2]*xc[2][2]+
    lam[2][3]*xc[2][3]+lam[2][4]*xc[2][4]+lam[2][5]*xc[2][5]+
    lam[2][6]*xc[2][6]+lam[2][7]*xc[2][7];


  /* H_ij, accord. to (1) eqns. (3.15d) and (3.15e) */
  hh[0][0] = 1.0/3.0 * (lx[1]*lx[2])/lx[0];
  hh[1][1] = 1.0/3.0 * (lx[2]*lx[0])/lx[1];
  hh[2][2] = 1.0/3.0 * (lx[0]*lx[1])/lx[2];
  hh[0][1] = 1.0/3.0 * lx[2];
  hh[1][0] = 1.0/3.0 * lx[2];
  hh[0][2] = 1.0/3.0 * lx[1];
  hh[2][0] = 1.0/3.0 * lx[1];
  hh[1][2] = 1.0/3.0 * lx[0];
  hh[2][1] = 1.0/3.0 * lx[0];


  /* stabalization matrix with respect to the corotational ccord. system. */
  /* rearranging orders of dofs, accord. to (1) eqns. (3.15a) to (3.15c) */
  for(i=0; i<8; i++)
  {
    for(j=0; j<8; j++)
    {
      gg00[i][j] = gam[0][i] * gam[0][j];
      gg11[i][j] = gam[1][i] * gam[1][j];
      gg22[i][j] = gam[2][i] * gam[2][j];
      gg33[i][j] = gam[3][i] * gam[3][j];
      gg01[i][j] = gam[0][i] * gam[1][j];
      gg10[i][j] = gam[1][i] * gam[0][j];
      gg02[i][j] = gam[0][i] * gam[2][j];
      gg20[i][j] = gam[2][i] * gam[0][j];
      gg12[i][j] = gam[1][i] * gam[2][j];
      gg21[i][j] = gam[2][i] * gam[1][j];

      /* kstab 00 */
      kstab[i*3][j*3]     = 2*mu* (hh[0][0]*(c1*(gg11[i][j] + gg22[i][j])
            + c2*gg33[i][j]) + 0.5 * (hh[1][1] + hh[2][2]) * gg00[i][j]);

      /* kstab 11 */
      kstab[i*3+1][j*3+1] = 2*mu* (hh[1][1]*(c1*(gg22[i][j] + gg00[i][j])
            + c2*gg33[i][j]) + 0.5 * (hh[2][2] + hh[0][0]) * gg11[i][j]);

      /* kstab 22 */
      kstab[i*3+2][j*3+2] = 2*mu* (hh[2][2]*(c1*(gg00[i][j] + gg11[i][j])
            + c2*gg33[i][j]) + 0.5 * (hh[0][0] + hh[1][1]) * gg22[i][j]);

      /* kstab 01 */
      kstab[i*3][j*3+1]   = 2*mu* (hh[0][1]*(c3*gg10[i][j]+0.5*gg01[i][j]));

      /* kstab 10 */
      kstab[i*3+1][j*3]   = 2*mu* (hh[1][0]*(c3*gg01[i][j]+0.5*gg10[i][j]));

      /* kstab 02 */
      kstab[i*3][j*3+2]   = 2*mu* (hh[0][2]*(c3*gg20[i][j]+0.5*gg02[i][j]));

      /* kstab 20 */
      kstab[i*3+2][j*3]   = 2*mu* (hh[2][0]*(c3*gg02[i][j]+0.5*gg20[i][j]));

      /* kstab 12 */
      kstab[i*3+1][j*3+2] = 2*mu* (hh[1][2]*(c3*gg21[i][j]+0.5*gg12[i][j]));

      /* kstab 21 */
      kstab[i*3+2][j*3+1] = 2*mu* (hh[2][1]*(c3*gg12[i][j]+0.5*gg21[i][j]));
    }
  }


  /* transforming kstab to the global coordinate system and */
  /* and adding to the one point quadrature matrix */
  for(i=0; i<8; i++)
  {
    for(j=0;j<8;j++)
    {
      for(k=0;k<3;k++)
      {
        for(l=0;l<3;l++)
        {
          s[i*3+k][j*3+l] = s[i*3+k][j*3+l]
            + (r[0][k]*kstab[i*3+0][j*3+0] + r[1][k]*kstab[i*3+1][j*3+0]
                + r[2][k]*kstab[i*3+2][j*3+0]) * r[0][l]
            + (r[0][k]*kstab[i*3+0][j*3+1] + r[1][k]*kstab[i*3+1][j*3+1]
                + r[2][k]*kstab[i*3+2][j*3+1]) * r[1][l]
            + (r[0][k]*kstab[i*3+0][j*3+2] + r[1][k]*kstab[i*3+1][j*3+2]
                + r[2][k]*kstab[i*3+2][j*3+2]) * r[2][l];
        }
      }
    }
  }

#ifdef DEBUG
  dstrc_exit();
#endif

  return;
} /* end of ale3_hourglass */


#endif
/*! @} (documentation module close)*/
