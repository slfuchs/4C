/*!----------------------------------------------------------------------
\file
\brief fluid3ml prototypes

<pre>
Maintainer: Volker Gravemeier
            vgravem@stanford.edu
            
            
</pre>

------------------------------------------------------------------------*/
/* RULE HOW TO ADD NEW FILES AND FUNCTIONS: 
   1.) THE FILENAMES ARE IN ALPHABETICAL ORDER !!!
   2.) FUNCTIONS ARE IN THE SAME ORDER LIKE IN THE FILE!!!
*/
/************************************************************************
 | f3_mlbubmat.c                                                          |
 ************************************************************************/
void f3_calbkvv(DOUBLE         **estif,   
		DOUBLE          *velint, 
		DOUBLE         **vderxy, 
		DOUBLE          *funct,  
		DOUBLE         **derxy,  
		DOUBLE          *vbubint,  
		DOUBLE         **vbubderxy,  
		DOUBLE           fac,    
		DOUBLE           visc,   
		INT              iel);
void f3_calbkvp(DOUBLE         **estif,   
		DOUBLE          *velint, 
		DOUBLE         **vderxy, 
		DOUBLE          *funct,  
		DOUBLE         **derxy,  
		DOUBLE         **pbubint,  
		DOUBLE        ***pbubderxy,  
		DOUBLE           fac,    
		DOUBLE           visc,   
		INT              iel);
void f3_calbkpv(DOUBLE         **estif,   
		DOUBLE          *funct,  
		DOUBLE         **vbubderxy,  
		DOUBLE           fac,    
		INT              iel);
void f3_calbkpp(DOUBLE         **estif,   
		DOUBLE          *funct,  
		DOUBLE        ***pbubderxy,  
		DOUBLE           fac,    
		INT              iel);
void f3_calbmvv(DOUBLE         **emass,  
	        DOUBLE          *funct, 
	        DOUBLE          *vbubint, 
	        DOUBLE           fac,   
	        INT              iel);
void f3_calbmvp(DOUBLE         **emass,  
	        DOUBLE          *funct, 
	        DOUBLE         **pbubint, 
	        DOUBLE           fac,   
	        INT              iel);
		
/************************************************************************
 | f3_mlele.c                                                          |
 ************************************************************************/
void f3_lsele(FLUID_DATA     *data,
              FLUID_DYN_ML   *mlvar, 
              FLUID_ML_SMESH *submesh, 
              FLUID_ML_SMESH *ssmesh, 
	      ELEMENT	     *ele,	       
              ARRAY	     *estif_global,   
              ARRAY	     *emass_global,   
	      ARRAY	     *etforce_global,	    
	      ARRAY	     *eiforce_global, 
	      ARRAY	     *edforce_global,	      
	      INT	     *hasdirich,      
              INT	     *hasext,
	      INT	      init);
void f3_smele(FLUID_DATA     *data,
              FLUID_DYN_ML   *mlvar, 
              FLUID_ML_SMESH *submesh, 
	      ELEMENT        *ele,             
              INT             init);
void f3_bubele(FLUID_DATA     *data, 
               FLUID_DYN_ML   *mlvar, 
               FLUID_ML_SMESH *submesh, 
	       ELEMENT        *ele);
void f3_dynsgv(FLUID_DATA     *data,
               FLUID_DYN_ML   *mlvar, 
               FLUID_ML_SMESH *submesh, 
               FLUID_ML_SMESH *ssmesh, 
	       ELEMENT	      *ele);
void f3_smintele(FLUID_DATA     *data, 
                 FLUID_ML_SMESH *submesh, 
	         ELEMENT        *ele,
	         DOUBLE         *smidiff,
	         DOUBLE         *smirhs);
void f3_ssele(FLUID_DATA      *data,
              FLUID_DYN_ML    *mlvar, 
              FLUID_ML_SMESH  *ssmesh, 
	      ELEMENT         *ele,
	      INT              init);
void f3_ssintele(FLUID_DATA     *data, 
                 FLUID_ML_SMESH *ssmesh, 
	         ELEMENT        *ele,
		 DOUBLE         *ssinbu);
		      
/************************************************************************
 | f3_mlelesize.c                                                          |
 ************************************************************************/
void f3_smelesize(ELEMENT         *ele,    
		  FLUID_DATA	  *data,
		  FLUID_DYN_ML    *mlvar,
	          DOUBLE	  *funct,  
	          DOUBLE	 **deriv,  
	          DOUBLE	 **deriv2,		
	          DOUBLE	  *smfunct,  
	          DOUBLE	 **smderiv,  
	          DOUBLE	 **smderiv2,		
	          DOUBLE	 **derxy,  
		  DOUBLE	 **xjm,    
		  DOUBLE	 **evel,		 
		  DOUBLE	  *velint, 
	          DOUBLE	 **vderxy,  
	          DOUBLE	 **smxyze,  
	          DOUBLE	 **smxyzep,  
		  DOUBLE	 **wa1);
void f3_mlcalelesize(ELEMENT         *ele,
		     FLUID_DATA      *data,
	             DOUBLE          *funct,
	             DOUBLE         **deriv,
	             DOUBLE         **deriv2,	       
                     DOUBLE         **derxy,
		     DOUBLE         **xjm,
		     DOUBLE         **evel,	       
		     DOUBLE          *velint,
	             DOUBLE         **vderxy,  
		     DOUBLE         **wa1);
void f3_mlcalelesize2(ELEMENT         *ele,
                      DOUBLE	    *velint,	      
		      DOUBLE	   **vderxy, 
                      DOUBLE	   **derxy,	       
		      DOUBLE	     visc,
		      INT 	     iel,
		      INT 	     ntyp); 
		  
/************************************************************************
 | f3_mlgalmat.c                                                          |
 ************************************************************************/
void f3_calsmk(FLUID_DYN_ML    *mlvar, 
	       DOUBLE         **smestif,   
	       DOUBLE          *velint, 
	       DOUBLE         **vderxy, 
	       DOUBLE          *smfunct,  
	       DOUBLE         **smderxy,  
	       DOUBLE           fac,    
	       DOUBLE           visc,   
	       INT              smiel);
void f3_calsmm(DOUBLE         **smemass,   
	       DOUBLE          *smfunct,  
	       DOUBLE           fac,    
	       INT              smiel);
void f3_calsmkd(DOUBLE         **smiediff,   
        	DOUBLE         **smderxy,  
	        DOUBLE           fac,    
	        INT              smiel);
void f3_lscalkvv(DOUBLE	      **estif,
	       DOUBLE	       *velint,
	       DOUBLE	      **vderxy,
	       DOUBLE	       *funct,
	       DOUBLE	      **derxy,
	       DOUBLE		fac,
	       DOUBLE		visc,
	       INT		iel);
void f3_lscalkvp(DOUBLE         **estif,
	       DOUBLE	       *funct,
	       DOUBLE	      **derxy,
	       DOUBLE		fac,
	       INT		iel);
void f3_lscalmvv(DOUBLE         **estif,
	       DOUBLE	       *funct,
	       DOUBLE		fac,
	       INT		iel);
		
/************************************************************************
 | f3_mlint.c                                                          |
 ************************************************************************/
void f3_smint(FLUID_DATA      *data,     
	      ELEMENT	      *ele,
	      FLUID_DYN_ML    *mlvar, 
	      FLUID_ML_SMESH  *submesh, 
              DOUBLE	     **smestif,   
	      DOUBLE	     **smemass,   
	      DOUBLE	     **smevfor, 
	      DOUBLE	     **smetfor, 
	      DOUBLE	     **smxyze, 
	      DOUBLE	     **smxyzep, 
	      DOUBLE	      *funct,	
	      DOUBLE	     **deriv,	
	      DOUBLE	     **deriv2,  
	      DOUBLE	     **xjm,	
	      DOUBLE	     **derxy,	
	      DOUBLE	     **derxy2,  
	      DOUBLE	      *smfunct,   
	      DOUBLE	     **smderiv,   
	      DOUBLE	     **smderiv2,  
	      DOUBLE	     **smxjm,	  
	      DOUBLE	     **smderxy,   
	      DOUBLE	     **smderxy2,  
	      DOUBLE	     **eveln,	
	      DOUBLE	     **evel,  
	      DOUBLE	      *epren,	
	      DOUBLE	      *epre,
	      DOUBLE	     **evbub,	
	      DOUBLE	     **epbub,	
	      DOUBLE	     **efbub,	
	      DOUBLE	     **evbubn,   
	      DOUBLE	     **epbubn,   
	      DOUBLE	     **efbubn,   
              DOUBLE	      *vbubint,    
              DOUBLE	     **vbubderxy,  
              DOUBLE	     **vbubderxy2, 
              DOUBLE	     **pbubint,    
              DOUBLE	    ***pbubderxy,  
              DOUBLE	    ***pbubderxy2, 
              DOUBLE	      *vbubintn,   
              DOUBLE	     **vbubderxyn, 
              DOUBLE	     **vbubderxy2n,
              DOUBLE	     **pbubintn,   
              DOUBLE	    ***pbubderxyn, 
              DOUBLE	    ***pbubderxy2n,
	      DOUBLE	      *velint,  
              DOUBLE	      *velintn,   
              DOUBLE	      *velintnt,  
              DOUBLE	      *velintnc,  
	      DOUBLE	     **vderxy,  
              DOUBLE	     **vderxyn,   
              DOUBLE	     **vderxync,  
              DOUBLE	     **vderxynv,  
	      DOUBLE	     **vderxy2, 
              DOUBLE	     **vderxy2n,  
              DOUBLE	     **vderxy2nv,  
              DOUBLE	      *pderxyn,   
              DOUBLE	      *smvelint,  
              DOUBLE	     **smvderxy,  
              DOUBLE	      *smpreint,  
              DOUBLE	     **smpderxy,  
              DOUBLE	      *smvelintn, 
              DOUBLE	     **smvderxyn, 
              DOUBLE	     **smvderxy2n, 
              DOUBLE	      *smpreintn,  
              DOUBLE	     **smpderxyn,  
              DOUBLE	     **smpderxy2n, 
              DOUBLE	      *smfint,	 
              DOUBLE	     **smfderxy,  
              DOUBLE	      *smfintn,    
              DOUBLE	     **smfderxyn,  
              DOUBLE	     **smfderxy2n,
	      DOUBLE	     **wa1,	
	      DOUBLE	     **wa2);
void f3_bubint(FLUID_DATA      *data,     
	       ELEMENT         *ele,     
	       FLUID_DYN_ML    *mlvar, 
	       FLUID_ML_SMESH  *submesh, 
               DOUBLE	      **estif,	
	       DOUBLE	      **emass,	
	       DOUBLE	       *eiforce, 
	       DOUBLE         **smxyze, 
	       DOUBLE         **smxyzep, 
	       DOUBLE          *funct,   
	       DOUBLE         **deriv,   
	       DOUBLE         **deriv2,   
	       DOUBLE         **xjm,     
	       DOUBLE         **derxy,   
	       DOUBLE          *smfunct,   
	       DOUBLE         **smderiv,   
	       DOUBLE         **smderiv2,   
	       DOUBLE         **smxjm,     
	       DOUBLE         **smderxy,   
	       DOUBLE         **evel,  
	       DOUBLE          *epre,
	       DOUBLE         **evbub,   
	       DOUBLE         **epbub,   
	       DOUBLE         **efbub,   
               DOUBLE          *vbubint,    
               DOUBLE         **vbubderxy,  
               DOUBLE         **pbubint,    
               DOUBLE        ***pbubderxy,  
	       DOUBLE	       *covint,  
	       DOUBLE          *velint,  
	       DOUBLE         **vderxy,  
               DOUBLE          *smvelint,  
               DOUBLE         **smvderxy,  
               DOUBLE          *smpreint,  
               DOUBLE         **smpderxy,  
               DOUBLE          *smfint,    
               DOUBLE         **smfderxy,  
	       DOUBLE         **wa1,     
	       DOUBLE         **wa2);
void f3_lsint(FLUID_DATA      *data,     
	      ELEMENT	      *ele,	
	      FLUID_DYN_ML    *mlvar, 
              INT             *hasext,
              DOUBLE	     **estif,	
	      DOUBLE	     **emass,	
	      DOUBLE	      *eiforce, 
	      DOUBLE	      *etforce, 
	      DOUBLE	      *funct,	
	      DOUBLE	     **deriv,	
	      DOUBLE	     **deriv2,  
	      DOUBLE	     **xjm,	
	      DOUBLE	     **derxy,	
	      DOUBLE	     **derxy2,  
	      DOUBLE	     **evel,  
	      DOUBLE	     **eveln,  
	      DOUBLE          *epren,   
	      DOUBLE          *edeadn,   
	      DOUBLE          *edead,   
	      DOUBLE	      *velint,  
              DOUBLE          *velintn,   
	      DOUBLE	      *covint,  
	      DOUBLE	      *covintn,  
	      DOUBLE	     **vderxy,  
              DOUBLE         **vderxyn,  
	      DOUBLE	     **wa1,	
	      DOUBLE	     **wa2);
void f3_smint2(FLUID_DATA      *data,     
	       ELEMENT	       *ele,	
	       FLUID_ML_SMESH  *submesh, 
               DOUBLE	      **smediff,   
	       DOUBLE	       *smerhs,   
	       DOUBLE	      **smxyze, 
	       DOUBLE	       *smfunct,   
	       DOUBLE	      **smderiv,   
	       DOUBLE	      **smderiv2,  
	       DOUBLE	      **smxjm,	  
	       DOUBLE	      **smderxy,
	       DOUBLE	      **wa1);
void f3_ssint(FLUID_DATA      *data,     
	      ELEMENT	      *ele,	
	      FLUID_DYN_ML    *mlvar, 
	      FLUID_ML_SMESH  *ssmesh, 
              DOUBLE	     **ssestif,   
	      DOUBLE	      *ssenfor,   
	      DOUBLE	     **ssxyze, 
	      DOUBLE	     **ssxyzep, 
	      DOUBLE	      *funct,	
	      DOUBLE	     **deriv,	
	      DOUBLE	     **deriv2,  
	      DOUBLE	     **xjm,	
	      DOUBLE	     **derxy,	
	      DOUBLE	      *ssfunct,   
	      DOUBLE	     **ssderiv,   
	      DOUBLE	     **ssderiv2,  
	      DOUBLE	     **ssxjm,	  
	      DOUBLE	     **ssderxy,   
	      DOUBLE	     **evel,  
	      DOUBLE	      *velint,  
	      DOUBLE	     **vderxy,
	      DOUBLE	     **wa1);
void f3_inbu(FLUID_DATA      *data,     
	     ELEMENT	     *ele,   
	     FLUID_ML_SMESH  *ssmesh, 
             DOUBLE	     *ssinbu,   
             DOUBLE	     *ebub,   
	     DOUBLE	    **ssxyze, 
	     DOUBLE	     *ssfunct,   
	     DOUBLE	    **ssderiv,   
	     DOUBLE	    **ssderiv2,  
	     DOUBLE	    **ssxjm);
	     
/************************************************************************
 | f3_mlservice.c                                                            |
 ************************************************************************/
void f3_lsset(ELEMENT	      *ele,	
              DOUBLE	     **eveln,	 
	      DOUBLE	     **evel, 
	      DOUBLE	      *epren,
	      DOUBLE	      *epre,
	      DOUBLE	      *edeadn,
	      DOUBLE	      *edead,
	      INT	      *hasext);
void f3_smset(FLUID_ML_SMESH  *smesh, 
	      ELEMENT	      *ele,	
              INT 	      *smlme,	 
	      INT	      *smitope, 
	      DOUBLE	     **smxyze,
	      DOUBLE	     **smxyzep,
	      INT	       iele,
	      INT	       flag);
void f3_bubset(FLUID_DYN_ML    *mlvar,  
               FLUID_ML_SMESH  *submesh, 
	       ELEMENT	       *ele,	
               INT	       *smlme,
	       DOUBLE         **evbub,
	       DOUBLE         **epbub,
	       DOUBLE         **efbub,
	       INT	        flag);
void f3_ssset(FLUID_ML_SMESH  *ssmesh, 
	      ELEMENT	      *ele,	
              INT 	      *sslme,	 
	      INT	      *ssitope, 
	      DOUBLE	     **ssxyze,
	      DOUBLE          *ebub,
	      INT	       iele);
void f3_smcopy(DOUBLE  **smrhs,
               ELEMENT  *ele,
               INT       numeq,
               INT       numrhs);      
void f3_smcopy2(ELEMENT  *ele,
                INT       numeq,
                INT       numrhs);      
void f3_smprei(DOUBLE  *smpreint,     
               DOUBLE **pbubint,    
	       DOUBLE  *epre,     
	       INT      iel); 
void f3_smpder(DOUBLE  **smpderxy,     
               DOUBLE ***pbubderxy,    
	       DOUBLE   *epre,     
	       INT       iel); 
void f3_smpder2(DOUBLE  **smpderxy2,    
                DOUBLE ***pbubderxy2,    
	        DOUBLE   *epre,      
	        INT       iel);
void f3_mlpermestif(DOUBLE         **estif,
		    DOUBLE	 **emass,
		    DOUBLE	 **tmp,
		    INT		   iel); 
void f3_mljaco(DOUBLE     *funct,
               DOUBLE    **deriv,
               DOUBLE    **xjm,
               DOUBLE     *det,
               ELEMENT    *ele,
               INT         iel);
void f3_mljaco3(DOUBLE    **xyze,
                DOUBLE	 *funct,    
                DOUBLE	**deriv,   
                DOUBLE	**xjm,     
                DOUBLE	 *det,  	
                INT	  iel,	
                ELEMENT	 *ele);
void f3_mlgcoor2(DOUBLE     *funct,      
                 DOUBLE    **xyze,      
	         INT         iel,      
	         DOUBLE     *gcoor);
void f3_mlgder2(ELEMENT     *ele,
	        DOUBLE	 **xjm, 	   
	        DOUBLE	 **xder2,
	        DOUBLE	 **derxy,
	        DOUBLE	 **derxy2,
                DOUBLE	 **deriv2,
	        INT	   iel);
void f3_mlcogder2(DOUBLE     **xyze,     
	          DOUBLE     **xjm,      
	          DOUBLE     **xder2,  
	          DOUBLE     **derxy,  
	          DOUBLE     **derxy2, 
                  DOUBLE     **deriv2, 
	          INT          iel);
	       		 
/************************************************************************
 | f3_mlstabmat.c                                                            |
 ************************************************************************/
void f3_calstabsmk(FLUID_DYN_ML    *mlvar, 
		   DOUBLE         **smestif,  
		   DOUBLE          *velint, 
		   DOUBLE         **vderxy, 
		   DOUBLE          *smfunct,  
		   DOUBLE         **smderxy,  
		   DOUBLE         **smderxy2, 
		   DOUBLE           fac,    
		   DOUBLE           visc,   
		   INT              smiel,    
                   INT              ihoelsm);
void f3_calstabsmm(FLUID_DYN_ML    *mlvar, 
		   DOUBLE         **smemass,  
		   DOUBLE          *velint, 
		   DOUBLE         **vderxy, 
		   DOUBLE          *smfunct,  
		   DOUBLE         **smderxy,  
		   DOUBLE         **smderxy2, 
		   DOUBLE           fac,    
		   DOUBLE           visc,   
		   INT              smiel,    
                   INT              ihoelsm);
void f3_lscalstabkvv(ELEMENT         *ele,
		   DOUBLE	  **estif,
		   DOUBLE	   *velint,
		   DOUBLE	  **vderxy,
		   DOUBLE	   *funct,
		   DOUBLE	  **derxy,
		   DOUBLE	  **derxy2,
		   DOUBLE	    fac,
		   DOUBLE	    visc,
		   INT  	    iel,	
                   INT  	    ihoel);
void f3_lscalstabkvp(ELEMENT         *ele,
		   DOUBLE	  **estif,
		   DOUBLE	   *velint,
		   DOUBLE	  **vderxy,
		   DOUBLE	   *funct,
		   DOUBLE	  **derxy,
		   DOUBLE	  **derxy2,
		   DOUBLE	    fac,
		   DOUBLE	    visc,
		   INT  	    iel,
		   INT  	    ihoel);
void f3_lscalstabmvv(ELEMENT         *ele,
		   DOUBLE	  **emass,
		   DOUBLE	   *velint,
		   DOUBLE	  **vderxy,
    		   DOUBLE	   *funct,
		   DOUBLE	  **derxy,
		   DOUBLE	  **derxy2,
		   DOUBLE	    fac,
		   DOUBLE	    visc,
		   INT  	    iel,
		   INT  	    ihoel);
void f3_lscalstabkpv(DOUBLE	  **estif,
		   DOUBLE	   *velint,
		   DOUBLE	  **vderxy,
		   DOUBLE	   *funct,
		   DOUBLE	  **derxy,
		   DOUBLE	  **derxy2,
		   DOUBLE	    fac,
		   DOUBLE	    visc,
		   INT  	    iel,
		   INT  	    ihoel);
void f3_lscalstabkpp(DOUBLE	  **estif,
		   DOUBLE	  **derxy,
		   DOUBLE	    fac,
		   INT  	    iel);
void f3_lscalstabmpv(DOUBLE	  **emass,
		   DOUBLE	   *funct,
		   DOUBLE	  **derxy,
		   DOUBLE	    fac,
		   INT  	    iel);
		   
/************************************************************************
 | f3_mlstabpar.c                                                            |
 ************************************************************************/
void f3_smstabpar(ELEMENT         *ele,
		  FLUID_DYN_ML    *mlvar,
		  DOUBLE	  *velint,  
		  DOUBLE	   visc,    
		  INT		   iel,     
		  INT		   ntyp);
void f3_smsgvisc(ELEMENT         *ele,
                 FLUID_DYN_ML    *mlvar,
		 DOUBLE 	 *velint,  
		 DOUBLE 	**vderxy,  
		 DOUBLE 	  visc,    
		 INT		  iel,     
		 INT		  ntyp);
void f3_mlcalstabpar(ELEMENT         *ele,
		     DOUBLE	   *velint,  
		     DOUBLE	    visc,    
		     INT  	    iel,     
		     INT  	    ntyp,    
		     INT  	    iflag);
void f3_calsgvisc(ELEMENT         *ele,
		  DOUBLE          *velint,  
		  DOUBLE         **vderxy,  
		  DOUBLE           visc,    
		  INT              iel,     
		  INT              ntyp);
		 
/************************************************************************
 | f3_mlsubmesh.c                                                            |
 ************************************************************************/
void f3_pdsubmesh(FLUID_ML_SMESH *smesh,
                  INT             xele,
                  INT             yele,
                  INT             zele,
                  INT             order,  
		  INT             flag);
void f3_elesubmesh(ELEMENT        *ele,
                   FLUID_ML_SMESH *smesh,
		   INT             flag);
		   
/************************************************************************
 | f3_mltimerhs.c                                                       |
 ************************************************************************/
void f3_calsmft(FLUID_DYN_ML	*mlvar, 
		DOUBLE         **smetfor,  
		DOUBLE  	*velintn, 
		DOUBLE  	*velintnt, 
		DOUBLE  	*velintnc, 
		DOUBLE         **vderxyn, 
		DOUBLE         **vderxync, 
		DOUBLE         **vderxynv, 
		DOUBLE         **vderxy2n, 
		DOUBLE          *pderxyn, 
		DOUBLE  	*smfunct,  
		DOUBLE         **smderxy,  
		DOUBLE  	 fac,	 
		DOUBLE  	 visc,   
		INT		 smiel,    
		INT		 iel,    
                INT		 ihoel);
void f3_calstabsmft(FLUID_DYN_ML    *mlvar, 
		    DOUBLE         **smetfor,  
		    DOUBLE  	    *velintn, 
		    DOUBLE  	    *velintnt, 
		    DOUBLE  	    *velintnc, 
		    DOUBLE         **vderxyn, 
		    DOUBLE         **vderxync, 
		    DOUBLE         **vderxynv, 
		    DOUBLE         **vderxy2n, 
		    DOUBLE          *pderxyn, 
	 	    DOUBLE  	    *smfunct,  
		    DOUBLE         **smderxy,  
		    DOUBLE         **smderxy2, 
		    DOUBLE  	     fac,	 
		    DOUBLE  	     visc,   
		    INT		     smiel,    
		    INT		     iel,    
                    INT		     ihoelsm,
                    INT		     ihoel);
		    
/************************************************************************
 | f3_mlvmmrhs.c                                                        |
 ************************************************************************/
void f3_calsmfv(FLUID_DYN_ML	*mlvar, 
		DOUBLE         **smevfor,  
		DOUBLE  	*velint, 
		DOUBLE         **vderxy, 
		DOUBLE  	*smfunct,  
		DOUBLE  	*funct,  
		DOUBLE         **derxy,  
		DOUBLE         **derxy2, 
		DOUBLE  	 fac,	 
		DOUBLE  	 visc,   
		INT		 smiel,    
		INT		 iel,    
                INT		 ihoel);
void f3_calstabsmfv(FLUID_DYN_ML    *mlvar, 
		    DOUBLE         **smevfor,  
		    DOUBLE  	    *velint, 
		    DOUBLE         **vderxy, 
		    DOUBLE  	    *smfunct,  
		    DOUBLE         **smderxy,  
		    DOUBLE         **smderxy2, 
		    DOUBLE  	    *funct,  
		    DOUBLE         **derxy,  
		    DOUBLE         **derxy2, 
		    DOUBLE  	     fac,	 
		    DOUBLE  	     visc,   
		    INT		     smiel,    
		    INT		     iel,    
                    INT		     ihoelsm,
                    INT		     ihoel);
void f3_calbfv(FLUID_DYN_ML    *mlvar,
	       DOUBLE          *eiforce,   
	       DOUBLE          *velint, 
	       DOUBLE         **vderxy, 
	       DOUBLE          *funct,  
	       DOUBLE         **derxy,  
	       DOUBLE          *smfint,  
	       DOUBLE         **smfderxy,  
	       DOUBLE           fac,    
	       DOUBLE           visc,   
	       INT              iel);
void f3_calbfp(DOUBLE          *eiforce,   
	       DOUBLE          *funct,  
	       DOUBLE         **smfderxy,  
	       DOUBLE           fac,    
	       INT              iel);
void f3_lscalgalexfv(DOUBLE          *eforce,     
		   DOUBLE          *funct,       
                   DOUBLE          *edeadn,
		   DOUBLE          *edeadng,
		   DOUBLE           fac,      
		   INT              iel); 
void f3_lscalgalifv(DOUBLE          *eforce,    
		  DOUBLE          *covint,
		  DOUBLE          *velint,  
		  DOUBLE         **vderxy,  
		  DOUBLE          *funct,
		  DOUBLE           fac,
		  INT              iel);  
void f3_lscalgaltfv(DOUBLE          *eforce,    
		  DOUBLE          *velint,
		  DOUBLE          *vel2int,
		  DOUBLE          *covint,
		  DOUBLE          *funct,
		  DOUBLE         **derxy,
		  DOUBLE         **vderxy,
		  DOUBLE           preint,
		  DOUBLE           visc,
		  DOUBLE           fac,
		  INT              iel);  
void f3_lscalgaltfp(DOUBLE	  *eforce,    
		  DOUBLE	  *funct,
		  DOUBLE	 **vderxy,
		  DOUBLE	   fac,
		  INT		   iel); 
	       
