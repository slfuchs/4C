#include "../headers/standardtypes.h"
#include "../headers/solution.h"
#include "../shell8/shell8.h"
#include "../wall1/wall1.h"
#include "../brick1/brick1.h"
#include "../fluid3/fluid3.h"
/*----------------------------------------------------------------------*
 | enum _CALC_ACTION                                      m.gee 1/02    |
 | command passed from control routine to the element level             |
 | to tell element routines what to do                                  |
 | defined globally in global_calelm.c                                  |
 *----------------------------------------------------------------------*/
extern enum _CALC_ACTION calc_action[MAXFIELD];
/*----------------------------------------------------------------------*
 | global dense matrices for element routines             m.gee 7/01    |
 *----------------------------------------------------------------------*/
extern struct _ARRAY estif_global;
extern struct _ARRAY emass_global;
extern struct _ARRAY intforce_global;
/*----------------------------------------------------------------------*
 |  routine to call elements                             m.gee 3/02     |
 *----------------------------------------------------------------------*/
void calelm_dyn(FIELD        *actfield,     /* active field */        
                SOLVAR       *actsolv,      /* active SOLVAR */
                PARTITION    *actpart,      /* my partition of this field */
                INTRA        *actintra,     /* my intra-communicator */
                int           sysarray1,    /* number of first sparse system matrix */
                int           sysarray2,    /* number of secnd system matrix, if present, else -1 */
                double       *dvec,         /* global redundant vector passed to elements */
                double       *dirich,
                int           global_numeq, /* size of dvec */
                double       *dirichfacs,   /* factors for rhs-entries due to prescribed displacements */
                int           kstep,        /* time in increment step we are in */
                CALC_ACTION  *action)       /* calculation option passed to element routines */
/*----------------------------------------------------------------------*/
{
int               i;
ELEMENT          *actele;
SPARSE_TYP        sysarray1_typ;
SPARSE_TYP        sysarray2_typ;
ASSEMBLE_ACTION   assemble_action;

#ifdef DEBUG 
dstrc_enter("calelm_dyn");
#endif
/*----------------------------------------------------------------------*/
/*-------------- zero the parallel coupling exchange buffers if present */  
#ifdef PARALLEL 
/*------------------------ check the send & recv buffers from sysarray1 */
if (sysarray1 != -1)
{
   switch(actsolv->sysarray_typ[sysarray1])
   {
   case msr:
      if (actsolv->sysarray[sysarray1].msr->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].msr->couple_d_send);
      if (actsolv->sysarray[sysarray1].msr->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].msr->couple_d_recv);
   break;
   case parcsr:
      if (actsolv->sysarray[sysarray1].parcsr->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].parcsr->couple_d_send);
      if (actsolv->sysarray[sysarray1].parcsr->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].parcsr->couple_d_recv);
   break;
   case ucchb:
      if (actsolv->sysarray[sysarray1].ucchb->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].ucchb->couple_d_send);
      if (actsolv->sysarray[sysarray1].ucchb->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].ucchb->couple_d_recv);
   break;
   case dense:
      if (actsolv->sysarray[sysarray1].dense->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].dense->couple_d_send);
      if (actsolv->sysarray[sysarray1].dense->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].dense->couple_d_recv);
   break;
   case rc_ptr:
      if (actsolv->sysarray[sysarray1].rc_ptr->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].rc_ptr->couple_d_send);
      if (actsolv->sysarray[sysarray1].rc_ptr->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].rc_ptr->couple_d_recv);
   break;
   case ccf:
      if (actsolv->sysarray[sysarray1].ccf->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].ccf->couple_d_send);
      if (actsolv->sysarray[sysarray1].ccf->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].ccf->couple_d_recv);
   break;
   case skymatrix:
      if (actsolv->sysarray[sysarray1].sky->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].sky->couple_d_send);
      if (actsolv->sysarray[sysarray1].sky->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].sky->couple_d_recv);
   break;
   case spoolmatrix:
      if (actsolv->sysarray[sysarray1].spo->couple_d_send)
         amzero(actsolv->sysarray[sysarray1].spo->couple_d_send);
      if (actsolv->sysarray[sysarray1].spo->couple_d_recv)
         amzero(actsolv->sysarray[sysarray1].spo->couple_d_recv);
   break;
   default:
      dserror("Unknown typ of system matrix");
   break;
   }
}
/*------------------------ check the send & recv buffers from sysarray2 */
if (sysarray2 != -1)
{
   switch(actsolv->sysarray_typ[sysarray2])
   {
   case msr:
      if (actsolv->sysarray[sysarray2].msr->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].msr->couple_d_send);
      if (actsolv->sysarray[sysarray2].msr->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].msr->couple_d_send);
   break;
   case parcsr:
      if (actsolv->sysarray[sysarray2].parcsr->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].parcsr->couple_d_send);
      if (actsolv->sysarray[sysarray2].parcsr->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].parcsr->couple_d_send);
   break;
   case ucchb:
      if (actsolv->sysarray[sysarray2].ucchb->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].ucchb->couple_d_send);
      if (actsolv->sysarray[sysarray2].ucchb->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].ucchb->couple_d_send);
   break;
   case dense:
      if (actsolv->sysarray[sysarray2].dense->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].dense->couple_d_send);
      if (actsolv->sysarray[sysarray2].dense->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].dense->couple_d_send);
   break;
   case rc_ptr:
      if (actsolv->sysarray[sysarray2].rc_ptr->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].rc_ptr->couple_d_send);
      if (actsolv->sysarray[sysarray2].rc_ptr->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].rc_ptr->couple_d_recv);
   break;
   case ccf:
      if (actsolv->sysarray[sysarray2].ccf->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].ccf->couple_d_send);
      if (actsolv->sysarray[sysarray2].ccf->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].ccf->couple_d_recv);
   break;
   case skymatrix:
      if (actsolv->sysarray[sysarray2].sky->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].sky->couple_d_send);
      if (actsolv->sysarray[sysarray2].sky->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].sky->couple_d_recv);
   break;
   case spoolmatrix:
      if (actsolv->sysarray[sysarray2].spo->couple_d_send)
         amzero(actsolv->sysarray[sysarray2].spo->couple_d_send);
      if (actsolv->sysarray[sysarray2].spo->couple_d_recv)
         amzero(actsolv->sysarray[sysarray2].spo->couple_d_recv);
   break;
   default:
      dserror("Unknown typ of system matrix");
   break;
   }
}
#endif
/* =======================================================call elements */
/*---------------------------------------------- loop over all elements */
for (i=0; i<actpart->pdis[0].numele; i++)
{
   /*------------------------------------ set pointer to active element */
   actele = actpart->pdis[0].element[i];
   /* if present, init the element vectors intforce_global and dirich_global */
   if (dvec) amzero(&intforce_global);
   switch(actele->eltyp)/*======================= call element routines */
   {
   case el_shell8:
      shell8(actfield,actpart,actintra,actele,
             &estif_global,&emass_global,&intforce_global,
             kstep,0,NULL,action);
   break;
   case el_brick1:
      brick1(actpart,actintra,actele,
             &estif_global,&emass_global,
             action);
   break;
   case el_wall1:
      wall1(actpart,actintra,actele,
            &estif_global,&emass_global,&intforce_global,0,NULL,
            action);
   break;
   case el_fluid2: 
   break;
   case el_fluid3: 
   break;
   case el_ale3:
   break;
   case el_none:
      dserror("Typ of element unknown");
   break;
   default:
      dserror("Typ of element unknown");
   }/* end of calling elements */


   switch(*action)/*=== call assembly dependent on calculation-flag */
   {
   case calc_struct_linstiff     : assemble_action = assemble_one_matrix; break;
   case calc_struct_nlnstiff     : assemble_action = assemble_one_matrix; break;
   case calc_struct_nlnstiffmass : assemble_action = assemble_two_matrix; break;
   case calc_struct_internalforce: assemble_action = assemble_do_nothing; break;
   case calc_struct_eleload      : assemble_action = assemble_do_nothing; break;
   case calc_struct_stress       : assemble_action = assemble_do_nothing; break;
   case calc_struct_update_istep : assemble_action = assemble_do_nothing; break;
   default: dserror("Unknown type of assembly"); break;
   }
   /*--------------------------- assemble one or two system matrices */
   assemble(sysarray1,
            &estif_global,
            sysarray2,
            &emass_global,
            actpart,
            actsolv,
            actintra,
            actele,
            assemble_action);
   /*---------------------------- assemble the vector intforce_global */
   if (dvec)
   assemble_intforce(actele,dvec,global_numeq,&intforce_global);
   /*------ assemble the rhs vector of condensed dirichlet conditions */
   if (dirich)
   assemble_dirich_dyn(actele,dirich,global_numeq,&estif_global,&emass_global,
                       dirichfacs);
}/* end of loop over elements */
/*----------------------------------------------------------------------*/
/*                    in parallel coupled dofs have to be exchanged now */
/*             (if there are any inter-proc couplings, which is tested) */
/*----------------------------------------------------------------------*/
#ifdef PARALLEL 
switch(*action)
{
case calc_struct_linstiff      : assemble_action = assemble_one_exchange; break;
case calc_struct_nlnstiff      : assemble_action = assemble_one_exchange; break;
case calc_struct_nlnstiffmass  : assemble_action = assemble_two_exchange; break;
case calc_struct_internalforce : assemble_action = assemble_do_nothing; break;
case calc_struct_eleload       : assemble_action = assemble_do_nothing; break;
case calc_struct_stress        : assemble_action = assemble_do_nothing; break;
case calc_struct_update_istep  : assemble_action = assemble_do_nothing; break;
default: dserror("Unknown type of assembly"); break;
}
/*------------------------------ exchange coupled dofs, if there are any */
assemble(sysarray1,
         NULL,
         sysarray2,
         NULL,
         actpart,
         actsolv,
         actintra,
         actele,
         assemble_action);
#endif
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of calelm_dyn */









