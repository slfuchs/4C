#include "../headers/standardtypes.h"
/*!----------------------------------------------------------------------
\brief the tracing variable

<pre>                                                         m.gee 8/00
defined in pss_ds.c, declared in tracing.h                                                  
</pre>
*----------------------------------------------------------------------*/
#ifdef DEBUG
extern struct _TRACE         trace;
#endif
/*----------------------------------------------------------------------*
 | Initialize program service systems                     m.gee 8/00    |
 *----------------------------------------------------------------------*/
void ntaini(int argc, char *argv[])
{
/*---------------------------------------------------initialize tracing */
#ifdef DEBUG 
dsinit();
#endif
/*-------------------------------------------------------initialize I/O */
ntadev(argc,argv);
/*----------------------------------------- initialise CPU-time tracing */
ds_cputime_init();
/*------------------------------------------initialize free-field-input */
frinit();

/* because there is no dstrc_enter to this routine, the dstrc_exit has to
   be done 'by hand'
*/
#ifdef DEBUG 
trace.actroutine->dsroutcontrol=dsout;
trace.actroutine = trace.actroutine->prev;
trace.deepness--;
#endif
return;
} /* end of ntaini */
