/*!----------------------------------------------------------------------
\file
\brief contains the routine 'ale2_jaco' which calculates the Jacobian
matrix for a 2d ale element and the routine 'ale2_min_jaco' which searches
for the smalles Jacobian determinant of a quad4-element as well as
'ale2_el_area'

<pre>
Maintainer: Christiane Foerster
            foerster@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/foerster/
            0711 - 685-6127
</pre>

*----------------------------------------------------------------------*/
#ifdef D_ALE
#include "../headers/standardtypes.h"
#include "ale2.h"

/*!
\addtogroup Ale
*//*! @{ (documentation module open)*/

/*!----------------------------------------------------------------------
\brief  calculate the Jacobian matrix

<pre>                                                              mn 06/02
This routine calculates the Jacobian matrix  at a point r,s for
a 2D ale element.

</pre>
\param *deriv   DOUBLE     (i)   derivatives of the shape functions
\param **xjm    DOUBLE     (o)   the Jacobian matrix
\param *det     DOUBLE     (i)   determinant of the Jacobian matrix
\param *ele     ELEMENT    (i)   the element
\param iel      INT        (i)   number of nodes of the element

\warning There is nothing special to this routine
\return void
\sa caling: ---; called by: ale2_static_ke()

*----------------------------------------------------------------------*/
void ale2_jaco(DOUBLE    **deriv,
               DOUBLE    **xjm,
               DOUBLE     *det,
               DOUBLE    **xyz,
               INT         iel)
{
/*----------------------------------------------------------------------*/
INT k;
#ifdef DEBUG
dstrc_enter("ale2_jaco");
#endif
/*---------------------------------- determine jacobian at point r,s ---*/
   xjm[0][0] = 0.0 ;
   xjm[0][1] = 0.0 ;
   xjm[1][0] = 0.0 ;
   xjm[1][1] = 0.0 ;

   for (k=0; k<iel; k++)
   {
     xjm[0][0] += deriv[0][k] * xyz[k][0] ;
     xjm[0][1] += deriv[0][k] * xyz[k][1] ;
     xjm[1][0] += deriv[1][k] * xyz[k][0] ;
     xjm[1][1] += deriv[1][k] * xyz[k][1] ;
   }
/*------------------------------------------ determinant of jacobian ---*/
     *det = xjm[0][0]* xjm[1][1] - xjm[1][0]* xjm[0][1];

      if (*det<0.0) dserror("NEGATIVE JACOBIAN DETERMINANT");
/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif
return;
} /* end of ale2_jaco */

/*!----------------------------------------------------------------------
\brief  routine looks for the minimal Jacobian determinant

<pre>                                                              ck 01/03
This routine searches for the minimal Jacobian determinant of a 4 noded
quad element by evaluating the Jacobian at all nodal points and looking
for the smallest. The Jacobian of linear triangles is constant and hence
minimal everywhere. The search for the minimal Jacobian determinant of
and higher order elements are not implemented.

</pre>
\param   distyp    enum _DIS_TYP (i)   distyp of actual element
\param **xyz       DOUBLE        (i)   elemental coordinates
\param  *min_detF  DOUBLE        (o)   minimal Jacobian determinant

\warning There is nothing special to this routine.

\return void
\sa calling: ---;
             called by: ale2_statik_ke(), ale2_statik_ke_test2()

*----------------------------------------------------------------------*/
void ale2_min_jaco(enum _DIS_TYP distyp, DOUBLE **xyz, DOUBLE *min_detF)
{
DOUBLE           detF[4];          /* Jacobian determinant at nodes */

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_enter("ale2_min_jaco");
#endif
switch (distyp)
{
   case quad4:
      /*--------------------- evaluate Jacobian determinant at nodes ---*/
      detF[0] = 0.25 * ( (xyz[0][0]-xyz[1][0]) * (xyz[0][1]-xyz[3][1])
                       - (xyz[0][1]-xyz[1][1]) * (xyz[0][0]-xyz[3][0]) );
      detF[1] = 0.25 * ( (xyz[0][0]-xyz[1][0]) * (xyz[1][1]-xyz[2][1])
                       - (xyz[0][1]-xyz[1][1]) * (xyz[1][0]-xyz[2][0]) );
      detF[2] = 0.25 * ( (xyz[3][0]-xyz[2][0]) * (xyz[1][1]-xyz[2][1])
                       - (xyz[3][1]-xyz[2][1]) * (xyz[1][0]-xyz[2][0]) );
      detF[3] = 0.25 * ( (xyz[3][0]-xyz[2][0]) * (xyz[0][1]-xyz[3][1])
                       - (xyz[3][1]-xyz[2][1]) * (xyz[0][0]-xyz[3][0]) );

      /*------------------------------------------------- check sign ---*/
      if (detF[0] <= 0.0) dserror("Negative JACOBIAN ");
      if (detF[1] <= 0.0) dserror("Negative JACOBIAN ");
      if (detF[2] <= 0.0) dserror("Negative JACOBIAN ");
      if (detF[3] <= 0.0) dserror("Negative JACOBIAN ");
      /*-------------------------------------- look for the smallest ---*/
      *min_detF = ( detF[0]  < detF[1]) ?  detF[0]  : detF[1];
      *min_detF = (*min_detF < detF[2]) ? *min_detF : detF[2];
      *min_detF = (*min_detF < detF[3]) ? *min_detF : detF[3];
      /*----------------------------------------------------------------*/
      break;
   case tri3:
      *min_detF = (-xyz[0][0]+xyz[1][0]) * (-xyz[0][1]+xyz[2][1])
                - (-xyz[0][0]+xyz[2][0]) * (-xyz[0][1]+xyz[1][1]);
      if (*min_detF <= 0.0) dserror("Negative JACOBIAN ");
      break;
   default:
      dserror("minimal Jacobian determinant for this distyp not implemented");
      break;
}
#ifdef DEBUG
dstrc_exit();
#endif
/*----------------------------------------------------------------------*/
return;
}


#endif
/*! @} (documentation module close)*/
