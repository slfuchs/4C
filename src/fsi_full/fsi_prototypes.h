/*!----------------------------------------------------------------------
\file
\brief fsi_prototypes

<pre>
Maintainer: Steffen Genkinger
            genkinger@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/genkinger/
            0711 - 685-6127
</pre>

------------------------------------------------------------------------*/
/*!
\addtogroup FSI
*//*! @{ (documentation module open)*/

/* RULE HOW TO ADD NEW FILES AND FUNCTIONS:
   1.) THE FILENAMES ARE IN ALPHABETICAL ORDER !!!
   2.) FUNCTIONS ARE IN THE SAME ORDER LIKE IN THE FILE!!!
*/
/************************************************************************
 | fsi_aitken.c                                                         |
 ************************************************************************/
void fsi_aitken(
                 FIELD          *structfield,
		 INT             itnum,
                 INT             init
	       );

/************************************************************************
 | fsi_ale.c                                                            |
 ************************************************************************/
void fsi_ale(
               FIELD            *actfield,
               INT               mctrl
	    );

/************************************************************************
 | fsi_ale_2step.c                                                            |
 ************************************************************************/
void fsi_ale_2step(
                    FIELD            *actfield,
                    INT               mctrl
	           );

/************************************************************************
 | fsi_ale_laplace.c                                                            |
 ************************************************************************/
void fsi_ale_laplace(
                     FIELD            *actfield,
                     INT               mctrl
	            );

/************************************************************************
 | fsi_ale_LAS.c                                                            |
 ************************************************************************/
void fsi_ale_LAS(
                  FIELD            *actfield,
                  INT               mctrl
	       );

/************************************************************************
 | fsi_ale_lin.c                                                            |
 ************************************************************************/
void fsi_ale_lin(
                  FIELD            *actfield,
                  INT               mctrl
	       );

/************************************************************************
 | fsi_ale_nln.c                                                            |
 ************************************************************************/
void fsi_ale_nln(
                   FIELD            *actfield,
                   INT               mctrl
	         );

/************************************************************************
 | fsi_ale_spring.c                                                            |
 ************************************************************************/
void fsi_ale_spring(
                    FIELD            *actfield,
                    INT               mctrl
	           );

/************************************************************************
 | fsi_coupling.c                                                       |
 ************************************************************************/
void fsi_createfsicoup(void);
void fsi_initcoupling(
                          FIELD       *structfield,
                          FIELD       *fluidfield,
		          FIELD       *alefield
		      );
void fsi_struct_intdofs(
                          FIELD       *structfield
		       );

/************************************************************************
 | fsi_dyn.c                                                            |
 ************************************************************************/
void dyn_fsi(INT mctrl);

/************************************************************************
 | fsi_energy.c                                                          |
 ************************************************************************/
void fsi_dyneint(
                       FIELD          *structfield,
		       INT             init
		);
void fsi_energycheck( void );

/************************************************************************
 | fsi_fluid.c                                                          |
 ************************************************************************/
void fsi_fluid(
		       FIELD          *actfield,
		       INT             mctrl
	      );
/************************************************************************
 | fsi_gradient.c                                                       |
 ************************************************************************/
void fsi_gradient(
                  FIELD          *alefield,
                  FIELD          *structfield,
                  FIELD          *fluidfield,
		  INT             numfa,
		  INT             numff,
 		  INT             numfs
	         );
/************************************************************************
 | fsi_mortar.c                                                         |
 ************************************************************************/
#ifdef D_MORTAR
void fsi_initcoupling_intfaces( 
                  FIELD          *masterfield,
                  FIELD          *slavefield, 
                  INTERFACES     *int_faces
		  );
void fsi_init_interfaces(
                  FIELD          *masterfield, 
                  FIELD          *slavefield,
                  INTERFACES     *int_faces
                  );
void fsi_mortar_coeff(
                  FSI_DYNAMIC    *fsidyn, 
                  INTERFACES     *int_faces
                  );
void fsi_detect_intersection(
                  DOUBLE         lambdr1_2, 
                  DOUBLE         lambdr2_1, 
                  DOUBLE         lambdr2_2, 
                  DOUBLE         nr1_2, 
                  DOUBLE         nr2_1, 
                  DOUBLE         nr2_2, 
                  DOUBLE         *b1, 
                  DOUBLE         *b2, 
                  INT            *intersection
                  );
void fsi_calc_disp4ale(
                  FSI_DYNAMIC    *fsidyn, 
                  INTERFACES     *int_faces
                  );
void fsi_calc_intforces(
                  INTERFACES     *int_faces
                  );
void f2_fsiload(  
                  ELEMENT        *ele
                  );
void fsi_put_coupforc2struct(
                  FIELD          *masterfield, 
                  INTERFACES     *int_faces
                  );
void fsiserv_rhs_point_neum(
                  DOUBLE         *rhs, 
                  INT            dimrhs, 
                  PARTITION      *actpart
                  );     
#endif
/************************************************************************
 | fsi_relax_intdisp.c                                                  |
 ************************************************************************/
void fsi_relax_intdisp(
                            FIELD          *structfield
		      );

/************************************************************************
 | fsi_service.c                                                        |
 ************************************************************************/
void fsi_alecp(
		             FIELD           *fluidfield,
                             DOUBLE           dt,
		             INT              numdf,
		             INT              phase
	       ) ;
void fsi_aleconv(
		             FIELD  *fluidfield,
		             INT     numdf,
                             INT     pos1,
		             INT     pos2
                );
void fsi_copysol(
                              FIELD           *actfield,
                              INT              from,
		              INT              to,
		              INT              flag
		  );
void fsi_fluidstress_result(
                              FIELD           *actfield,
                              INT              numdf
			   );
void fsi_algoout(
			      INT               itnum
	        );
void fsi_structpredictor(
                              FIELD            *actfield,
                              INT               init
		         );
INT fsi_convcheck(            FIELD            *structfield,
			      INT               itnum
		 );
void fsi_init_ale(FIELD *actfield,INT numr);
void fluid_init_pos_ale(void);

/************************************************************************
 | fsi_struct.c                                                         |
 ************************************************************************/
void fsi_struct(
		   FIELD             *actfield,
		   INT                mctrl,
		   INT                fsiitnum
	       );

/*! @} (documentation module close)*/
