#include "../headers/standardtypes.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | general problem data                                                 |
 | global variable GENPROB genprob is defined in global_control.c       |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | structure allfiles, which holds all file pointers                    |
 | is defined in input_control_global.c
 *----------------------------------------------------------------------*/
extern struct _FILES  allfiles;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;

/*----------------------------------------------------------------------*
 | input of materials                                     m.gee 4/01    |
 *----------------------------------------------------------------------*/
void inp_material()
{
int  ierr, ierralloc;
int  i, j, ncm;
char *colpointer;
char buffer[50];
#ifdef DEBUG 
dstrc_enter("inp_material");
#endif
/*----------------------------------------------------------------------*/
mat = (MATERIAL*)CALLOC(genprob.nmat,sizeof(MATERIAL));
if (mat==NULL) dserror("Allocation of MATERIAL failed");
/*----------------------------------------------------------------------*/
frfind("--MATERIALS");
frread();
i=0;
while(strncmp(allfiles.actplace,"------",6)!=0)
{
   if (i==genprob.nmat) dserror("number of materials incorrect");

   frint("MAT",&(mat[i].Id),&ierr);

   frchk("MAT_fluid",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_fluid;
      mat[i].m.fluid = (FLUID*)CALLOC(1,sizeof(FLUID));
      if (mat[i].m.fluid==NULL) dserror("Alloocation of FLUID material failed");
      frdouble("VISCOSITY",&(mat[i].m.fluid->viscosity),&ierr);
      frdouble("DENS"  ,&(mat[i].m.fluid->density)  ,&ierr);
   }
   frchk("MAT_Struct_StVenantKirchhoff",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_stvenant;
      mat[i].m.stvenant = (STVENANT*)CALLOC(1,sizeof(STVENANT));
      if (mat[i].m.stvenant==NULL) dserror("Alloocation of STVENANT material failed");
      frdouble("YOUNG"  ,&(mat[i].m.stvenant->youngs)      ,&ierr);
      frdouble("NUE"    ,&(mat[i].m.stvenant->possionratio),&ierr);
      frdouble("DENS",&(mat[i].m.stvenant->density)     ,&ierr);
   }
   frchk("MAT_Struct_NeoHooke",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_neohooke;
      mat[i].m.neohooke = (NEO_HOOKE*)CALLOC(1,sizeof(NEO_HOOKE));
      if (mat[i].m.neohooke==NULL) dserror("Alloocation of NEO_HOOKE material failed");
      frdouble("YOUNG",&(mat[i].m.neohooke->youngs)        ,&ierr);
      frdouble("NUE"  ,&(mat[i].m.neohooke->possionratio)  ,&ierr);
      frdouble("DENSITY",&(mat[i].m.neohooke->density)     ,&ierr);
   }
   frchk("MAT_MisesPlastic",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_pl_mises;
      mat[i].m.pl_mises = (PL_MISES*)CALLOC(1,sizeof(PL_MISES));
      if (mat[i].m.pl_mises==NULL) dserror("Alloocation of MISES material failed");
      frdouble("YOUNG",&(mat[i].m.pl_mises->youngs)        ,&ierr);
      frdouble("NUE"  ,&(mat[i].m.pl_mises->possionratio)  ,&ierr);
      frdouble("ALFAT",&(mat[i].m.pl_mises->ALFAT)         ,&ierr);
      frdouble("Sigy" ,&(mat[i].m.pl_mises->Sigy)          ,&ierr);
      mat[i].m.pl_mises->Hard = 0.; 
      mat[i].m.pl_mises->GF   = 0.; 
      frdouble("Hard" ,&(mat[i].m.pl_mises->Hard)          ,&ierr);
      frdouble("GF"   ,&(mat[i].m.pl_mises->GF)            ,&ierr);
   }
   frchk("MAT_DP_Plastic",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_pl_dp;
      mat[i].m.pl_dp = (PL_DP*)CALLOC(1,sizeof(PL_DP));
      if (mat[i].m.pl_dp==NULL) dserror("Alloocation of Drucker Prager material failed");
      frdouble("YOUNG",&(mat[i].m.pl_dp->youngs)        ,&ierr);
      frdouble("NUE"  ,&(mat[i].m.pl_dp->possionratio)  ,&ierr);
      frdouble("ALFAT",&(mat[i].m.pl_dp->ALFAT)         ,&ierr);
      frdouble("Sigy" ,&(mat[i].m.pl_dp->Sigy)          ,&ierr);
      frdouble("Hard" ,&(mat[i].m.pl_dp->Hard)          ,&ierr);
      frdouble("PHI"  ,&(mat[i].m.pl_dp->PHI)           ,&ierr);
   }
   frchk("MAT_ConcretePlastic",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_pl_epc;
      mat[i].m.pl_epc = (PL_EPC*)calloc(1,sizeof(PL_EPC));
      if (mat[i].m.pl_epc==NULL) dserror("Allocation of elpl-concrete material failed");
      /* initialize */
      mat[i].m.pl_epc->gamma1 = 3.;
      mat[i].m.pl_epc->gamma2 = 6./5.;
      
      
      frdouble("DENS"    ,&(mat[i].m.pl_epc->dens        )        ,&ierr);
      /* concrete */
      frdouble("YOUNG"   ,&(mat[i].m.pl_epc->youngs      )        ,&ierr);
      frdouble("NUE"     ,&(mat[i].m.pl_epc->possionratio)        ,&ierr);
      frdouble("ALFAT"   ,&(mat[i].m.pl_epc->alfat       )        ,&ierr);
      frdouble("XSI"     ,&(mat[i].m.pl_epc->xsi         )        ,&ierr);
      frdouble("Sigy"    ,&(mat[i].m.pl_epc->sigy        )        ,&ierr);
      frread();
      frdouble("FTM"     ,&(mat[i].m.pl_epc->ftm         )        ,&ierr);
      frdouble("FCM"     ,&(mat[i].m.pl_epc->fcm         )        ,&ierr);
      frdouble("GT"      ,&(mat[i].m.pl_epc->gt          )        ,&ierr);
      frdouble("GC"      ,&(mat[i].m.pl_epc->gc          )        ,&ierr);
      frdouble("GAMMA1"  ,&(mat[i].m.pl_epc->gamma1      )        ,&ierr);
      if(mat[i].m.pl_epc->gamma1<1.)mat[i].m.pl_epc->gamma1=3.; 
      frdouble("GAMMA2"  ,&(mat[i].m.pl_epc->gamma2      )        ,&ierr);
     /* tension stiffening - next line in input file!*/
      frread();
      frint(   "NSTIFF"  ,&(mat[i].m.pl_epc->nstiff      )        ,&ierr);
      /* number of rebars - next line in input file! */
      frread();
      frint(   "MAXREB"   ,&(mat[i].m.pl_epc->maxreb     )        ,&ierr);
      /* allocate memory */
      ncm       = mat[i].m.pl_epc->maxreb;
      ierralloc = 0;
      if ((mat[i].m.pl_epc->rebar=(int*)calloc(ncm,sizeof(int)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_area  =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_ang   =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_so    =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_ds    =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_rgamma=(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_dens  =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_alfat =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_emod  =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_rebnue=(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_sigy  =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      if ((mat[i].m.pl_epc->reb_hard  =(double*)calloc(ncm,sizeof(double)))==NULL) ierralloc=1;
      
      if (ierralloc) dserror("Allocation of elpl-concrete material failed");
      /* rebar data - next line in input file! */
      if(ncm==0)
      {
        frread();
        frread();
        frread();
      }
      for(j=0;j<ncm;j++)
      {
        frread();
        frint(   "REBAR"   ,&(mat[i].m.pl_epc->rebar[j]     ),&ierr);
        frdouble("REBAREA" ,&(mat[i].m.pl_epc->reb_area[j]  ),&ierr);
        frdouble("REBANG"  ,&(mat[i].m.pl_epc->reb_ang[j]   ),&ierr);
        frdouble("REBSO"   ,&(mat[i].m.pl_epc->reb_so[j]    ),&ierr);
        frdouble("REBDS"   ,&(mat[i].m.pl_epc->reb_ds[j]    ),&ierr);
        frdouble("REBGAMMA",&(mat[i].m.pl_epc->reb_rgamma[j]),&ierr);
        frread();
        frdouble("REBDENS" ,&(mat[i].m.pl_epc->reb_dens[j]  ),&ierr);
        frdouble("REBALFAT",&(mat[i].m.pl_epc->reb_alfat[j] ),&ierr);
        frdouble("REBEMOD" ,&(mat[i].m.pl_epc->reb_emod[j]  ),&ierr);
        frdouble("REBNUE"  ,&(mat[i].m.pl_epc->reb_rebnue[j]),&ierr);
        frread();                               
        frdouble("REBSIGY" ,&(mat[i].m.pl_epc->reb_sigy[j]  ),&ierr);
        frdouble("REBHARD" ,&(mat[i].m.pl_epc->reb_hard[j]  ),&ierr);
      }
   }
   frchk("MAT_Porous_MisesPlastic",&ierr);
   if (ierr==1)
   {
      mat[i].mattyp = m_pl_por_mises;
      mat[i].m.pl_por_mises = (PL_POR_MISES*)CALLOC(1,sizeof(PL_POR_MISES));
      if (mat[i].m.pl_por_mises==NULL) dserror("Alloocation of MISES material failed");
      frdouble("YOUNG"   ,&(mat[i].m.pl_por_mises->youngs)        ,&ierr);
      frdouble("DP_YM"   ,&(mat[i].m.pl_por_mises->DP_YM )        ,&ierr);
      frdouble("NUE"     ,&(mat[i].m.pl_por_mises->possionratio)  ,&ierr);
      frdouble("ALFAT"   ,&(mat[i].m.pl_por_mises->ALFAT)         ,&ierr);
      frdouble("Sigy"    ,&(mat[i].m.pl_por_mises->Sigy)          ,&ierr);
      frdouble("DP_Sigy" ,&(mat[i].m.pl_por_mises->DP_Sigy)       ,&ierr);
      frdouble("Hard"    ,&(mat[i].m.pl_por_mises->Hard)          ,&ierr);
      frdouble("DP_Hard" ,&(mat[i].m.pl_por_mises->DP_Hard)       ,&ierr);
   }
   i++;
/*----------------------------------------------------------------------*/
   frread();
}
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of inp_material */
