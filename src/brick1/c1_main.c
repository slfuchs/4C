/*!----------------------------------------------------------------------
\file
\brief contains the routine 'ale2', the main routine of the 2d ale element 

*----------------------------------------------------------------------*/
#include "../headers/standardtypes.h"
#include "brick1.h"
#include "brick1_prototypes.h"
/*----------------------------------------------------------------------*
 |                                                         al 06/02     |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;

/*! 
\addtogroup BRICK1 
*//*! @{ (documentation module open)*/

/*!----------------------------------------------------------------------
\brief  main control routine for the 3D Hex element

<pre>                                                              al 06/02 
This routine controles the calculation of the element stiffness, acts
according to the action.

</pre>
\param *actpart         PARTITION   (i)   my partition
\param *actintra        INTRA       (i)   my intra-communicator 
\param *ele             ELEMENT     (i)   my element
\param *estif_global    ARRAY       (i)   global stiffness matrix
\param *emass_global    ARRAY       (i)   global mass      matrix
\param *intforce_global ARRAY       (i)   global mass      matrix
\param *action          CALC_ACTION (i)   option passed to element
\param *container       CONTAINER   (i)   contains variables defined in container.h

\warning There is nothing special to this routine
\return void                                               
\sa calling: c1_cint(); called by: calelm(), cal_rhs()

*----------------------------------------------------------------------*/
void brick1(      PARTITION   *actpart,
                  INTRA       *actintra,
                  ELEMENT     *ele,
                  ARRAY       *estif_global,
                  ARRAY       *emass_global,
                  ARRAY       *intforce_global,
                  CALC_ACTION *action,
                  CONTAINER   *container)
{
/*----------------------------------------------------------------------*/
#ifdef D_BRICK1
/*----------------------------------------------------------------------*/
int  i, kstep, iloc;
C1_DATA      actdata;
MATERIAL    *actmat;

int          imyrank;
double      *intforce;
double       getval;

#ifdef DEBUG 
dstrc_enter("brick1");
#endif
/*----------------------------------------------------------------------*/
  kstep = container->kstep; /* time step */
/*----------------------------------------------------------------------*/
  if (intforce_global)
  {
    intforce = intforce_global->a.dv;
  }
/*------------------------------------------------- switch to do option */
switch (*action)
{
/*------------------------------------------- init the element routines */
case calc_struct_init:
   c1init(actpart, mat);
   c1_cint(NULL,NULL,NULL,NULL,NULL,1);
   c1_eleload(NULL,NULL,NULL,NULL,1);
break;/*----------------------------------------------------------------*/
/*----------------------------------- calculate linear stiffness matrix */
case calc_struct_linstiff:
   actmat = &(mat[ele->mat-1]);
   c1_cint(ele,&actdata,actmat,estif_global,NULL,0);
break;/*----------------------------------------------------------------*/
/*---------------------------------calculate nonlinear stiffness matrix */
case calc_struct_nlnstiff:
   actmat = &(mat[ele->mat-1]);
   c1_cint(ele,&actdata,actmat,estif_global,intforce,0);
break;/*----------------------------------------------------------------*/
/*-------------------------- calculate linear stiffness and mass matrix */
case calc_struct_linstiffmass:
break;/*----------------------------------------------------------------*/
/*----------------------- calculate nonlinear stiffness and mass matrix */
case calc_struct_nlnstiffmass:
break;/*----------------------------------------------------------------*/
/*-------------------------------- calculate stresses in a certain step */
case calc_struct_stress:
   actmat = &(mat[ele->mat-1]);
   c1_cint(ele,&actdata,actmat,estif_global,NULL,3);
break;/*----------------------------------------------------------------*/
/*------------------------------ calculate load vector of element loads */
case calc_struct_eleload:
   imyrank = actintra->intra_rank;
   if (imyrank==ele->proc) 
   {
      actmat = &(mat[ele->mat-1]);
      c1_eleload(ele,&actdata,actmat,intforce,0);
   }
break;/*----------------------------------------------------------------*/
/*--------------------------------------- update after incremental step */
case calc_struct_update_istep:
   actmat = &(mat[ele->mat-1]);
   if(actmat->mattyp == m_stvenant) break;
   if(actmat->mattyp == m_stvenpor) break;
   if(actmat->mattyp == m_neohooke) break;
   if(actmat->mattyp == m_fluid)    break;
   c1_cint(ele,&actdata,actmat,estif_global,intforce,2);
break;/*----------------------------------------------------------------*/
#ifdef D_OPTIM                   /* include optimization code to ccarat */
/*-------------- init the element integration routines for optimization */
case calc_struct_opt_init:
  c1_oint(NULL,NULL,NULL,NULL,1);
break;/*----------------------------------------------------------------*/
/*---------------------------------------------- evaluate strain energy */
case calc_struct_ste:
#ifdef PARALLEL 
   if(ele->proc!=actintra->intra_rank) break;
#endif
   actmat = &(mat[ele->mat-1]);
   c1_oint(ele,
           &actdata,
           actmat,
           &getval,
           2 /* flag for strain energy */
           );
   (*container).getvalue += getval;
break;/*----------------------------------------------------------------*/
/*------------------------------------------------------- evaluate mass */
case calc_struct_stm:
#ifdef PARALLEL 
   if(ele->proc!=actintra->intra_rank) break;
#endif
   actmat = &(mat[ele->mat-1]);
   c1_oint(ele,
           &actdata,
           actmat,
           &getval,
           3 /* flag for mass */
           );
   (*container).getvalue += getval;
break;/*----------------------------------------------------------------*/
/*-------------------------------- evaluate derivative of strain energy */
case calc_struct_dee:
#ifdef PARALLEL 
   if(ele->proc!=actintra->intra_rank) break;
#endif
   if(ele->optdata==NULL) break; /* element does not take part in opt. */
   if(ele->optdata[0]==0) break; /* position in variable vector        */
   iloc = ele->optdata[0];
   
   actmat = &(mat[ele->mat-1]);
   c1_oint(ele,
           &actdata,
           actmat,
           &getval,
           4 /* flag for derivative of strain energy */
           );
   (*container).getvector[iloc-1] += getval;
break;/*----------------------------------------------------------------*/
/*------------------------------ evaluate derivative of mass constraint */
case calc_struct_dmc:
#ifdef PARALLEL 
   if(ele->proc!=actintra->intra_rank) break;
#endif
   if(ele->optdata==NULL) break; /* element does not take part in opt. */
   if(ele->optdata[0]==0) break; /* position in variable vector        */
   iloc = ele->optdata[0];
   
   actmat = &(mat[ele->mat-1]);
   c1_oint(ele,
           &actdata,
           actmat,
           &getval,
           5 /* flag for volume */
           );
   (*container).getvector[iloc-1] += getval;
break;/*----------------------------------------------------------------*/
/*---------------------------------------------------- update densities */
case update_struct_odens:
   if(ele->optdata==NULL) break; /* element does not take part in opt. */
   if(ele->optdata[0]==0) break; /* position in variable vector        */
   iloc = ele->optdata[0];

   getval = (*container).getvector[iloc-1]   ;
   ele->e.c1->elewa->matdata[0]   =  getval;
break;/*----------------------------------------------------------------*/
#endif /* stop including optimization code to ccarat :*/
default:
   dserror("action unknown");
break;
}
#ifdef DEBUG 
dstrc_exit();
#endif
/*----------------------------------------------------------------------*/
#endif
/*----------------------------------------------------------------------*/
return; 
} /* end of brick1 */
/*! @} (documentation module close)*/
