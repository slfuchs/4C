void mlpcg_precond_create(DBCSR     *bdcsr, 
                          MLPCGVARS *mlpcgvars,
                          INTRA     *actintra);
void mlpcg_precond_init(DBCSR  *bdcsr,MLPCGVARS *mlpcgvars, INTRA *actintra);
void mlpcg_precond_restrictK(MLLEVEL  *actlev, MLLEVEL *nextlev, INTRA *actintra);
void mlpcg_precond_getdirs(void);
void mlpcg_precond_aggsetdofs(MLLEVEL *actlev,int numdf, INTRA *actintra);
void mlpcg_precond_agg(MLLEVEL *actlev,INTRA *actintra);
void mlpcg_precond_getneightoagg(int **neighblock, 
                                 int  *bpatch[],
                                 int   nbpatch,
                                 int **freeblock,
                                 int   nfreeblock,
                                 int   numeq,
                                 int  *update,
                                 int  *ia,
                                 int  *ja);
void mlpcg_precond_getfreenblocks(int  *actblock,
                                  int **freeblock,
                                  int   nfreeblock,
                                  int  *bpatch[],
                                  int  *nbpatch,
                                  int   numeq,
                                  int  *update,
                                  int  *ia,
                                  int  *ja);
void mlpcg_matvec(double       *y, 
                  DBCSR        *A,
                  double       *x,
                  double        fac,
                  int           init,
                  INTRA        *actintra);
void mlpcg_matvec_init(DBCSR       *bdcsr, 
                       INTRA       *actintra);
void mlpcg_matvec_uninit(DBCSR       *bdcsr);
void mlpcg_vecvec(double *scalar, double *x, double *y, const int dim, INTRA *actintra);
int mlpcg_getindex(int dof, int *update, int length);
int mlpcg_getowner(int dof, int owner[][2], int nproc);
void mlpcg_precond_P0(MLLEVEL  *actlev, INTRA *actintra);
void mlpcg_precond_P(MLLEVEL  *actlev, INTRA *actintra);
void mlpcg_precond_oneP_fish(AGG     *actagg,
                             double   aggblock[][500],
                             int      rindex[],
                             int      cindex[],
                             int     *nrow,
                             int     *ncol,
                             DBCSR   *actstiff,
                             INTRA   *actintra);
void mlpcg_smoothP(DBCSR *P, double block[][500], int *rindex, int *cindex,
                  int *nrow, int *ncol, DBCSR *actstiff, 
                  AGG *agg, int nagg, INTRA *actintra);
void mlpcg_precond_oneP0_vanek(AGG     *actagg,
                         double   aggblock[][500],
                         int      rindex[],
                         int      cindex[],
                         int     *nrow,
                         int     *ncol,
                         DBCSR   *actstiff);
void mlpcg_precond_oneP_vanek(AGG     *actagg,
                        double   aggblock[][500],
                        int      rindex[],
                        int      cindex[],
                        int     *nrow,
                        int     *ncol,
                        DBCSR   *actstiff,
                        MLLEVEL *prevlevel);
void mlpcg_csr_open(DBCSR*  csr,
                    int     firstdof,
                    int     lastdof,
                    int     numeq_total,
                    int     nnz_guess,
                    INTRA  *actintra);
void mlpcg_renumberdofs(int            myrank,
                        int            nproc,
                        FIELD         *actfield, 
                        PARTDISCRET   *actpdiscret, 
                        INTRA         *actintra,
                        DBCSR         *bdcsr);
void mlpcg_extractcollocal(DBCSR *P, int actcol, double *col, 
                           int *rcol, int *nrow);
void mlpcg_csr_open(DBCSR*  matrix,
                    int     firstdof,
                    int     lastdof,
                    int     numeq_total,
                    int     nnz_guess,
                    INTRA  *actintra);
void mlpcg_csr_close(DBCSR*   matrix);
void mlpcg_csr_destroy(DBCSR*   matrix);
void mlpcg_csr_setblock(DBCSR*   matrix,
                        double   block[][500],
                        int     *rindex,
                        int     *cindex,
                        int      nrow, 
                        int      ncol,
                        INTRA   *actintra);
void mlpcg_csr_addblock(DBCSR*   matrix,
                        double   block[][500],
                        int     *rindex,
                        int     *cindex,
                        int      nrow, 
                        int      ncol,
                        INTRA   *actintra);
void mlpcg_csr_addentry(DBCSR*   matrix,
                        double  val,
                        int     rindex,
                        int     cindex,
                        INTRA   *actintra);
void mlpcg_csr_addrow(DBCSR*   matrix,
                      int       rownum,
                      double   *row,
                      int     *cindex,
                      int      ncol,
                      INTRA   *actintra);
void mlpcg_csr_csrtocsc(DBCSR *matrix, INTRA *actintra);
void mlpcg_extractcolcsc(int     col,
                         int     numeq,
                         int    *update,
                         int    *ia,
                         int    *ja,
                         double *a,
                         double  col_out[],
                         int     rcol_out[],
                         int     *nrow);
void mlpcg_extractrowcsr(int     row,
                         int     numeq,
                         int    *update,
                         int    *ia,
                         int    *ja,
                         double *a,
                         double  row_out[],
                         int     rrow_out[],
                         int     *ncol);
void mlpcg_csr_extractsubblock_dense(DBCSR *from, double **A,
                                     int   *index, int nindex,
                                     INTRA *actintra);
void mlpcg_csr_setentry(DBCSR*   matrix,
                        double  val,
                        int     rindex,
                        int     cindex,
                        INTRA   *actintra);
void mlpcg_csr_setentry_overlap(DBCSR*   matrix,
                                double  val,
                                int     rindex,
                                int     cindex,
                                INTRA   *actintra);
void mlpcg_matvec_asm_overlap(double       *y, 
                              DBCSR        *A,
                              double       *x,
                              double        fac,
                              int           init,
                              INTRA        *actintra);
void mlpcg_csr_getdinv(double *Dinv, DBCSR *csr, int numeq);
void mlpcg_csr_extractsubblock(DBCSR *from, DBCSR *to,
                               int    rstart,
                               int    rend,
                               int    cstart,
                               int    cend,
                               INTRA *actintra);
void mlpcg_csr_localnumsf(DBCSR *matrix);
void mlpcg_csr_zero(DBCSR*  matrix,
                    INTRA  *actintra);
void mlpcg_extractcollocal_init(DBCSR    *matrix,
                                int      *sizes,
                                int    ***icol,
                                double ***dcol);
void mlpcg_extractcollocal_fast(DBCSR *matrix, int actcol, 
                                double *col,int *rcol, int *nrow,
                                int *sizes, int ***icol, double ***dcol);
void mlpcg_extractcollocal_uninit(DBCSR    *matrix,
                                  int      *sizes,
                                  int    ***icol,
                                  double ***dcol);
void mlpcg_precond_PtKP(DBCSR *P, DBCSR *incsr, DBCSR *outcsr, DBCSR *work,
                        AGG *agg, int nagg, INTRA *actintra);
void mlpcg_precond_presmo(double *z, double *r, DBCSR *csr, MLLEVEL *lev, INTRA *actintra, int level);
void mlpcg_precond_postsmo(double *z, double *r, DBCSR *csr, MLLEVEL *lev, INTRA *actintra, int level);
void mlpcg_precond_coarsesolv(double *z, double *r, MLLEVEL *lev, INTRA *actintra);
void mlpcg_precond_prolongz(double *zc, double *z, DBCSR *P, DBCSR *coarsecsr, 
                            INTRA *actintra);
void mlpcg_precond_restrictr(double *rc, double *r, DBCSR *P, DBCSR *coarsecsr, 
                            INTRA *actintra);
void mlpcg_precond_check_fcd(DBCSR *matrix, INTRA *actintra);
void mlpcg_precond_checkdirich(DBCSR *matrix, INTRA *actintra);
void mlpcg_precond_smoJacobi(double *z, double *r, DBCSR *csr, int nsweep, INTRA *actintra);
void mlpcg_precond_lapacksolve(double *z, double *r, DBCSR *csr, INTRA *actintra);
void mlpcg_precond_smo_ILUn(double *z, double *r, DBCSR *csr, int nsweep, INTRA *actintra);
void mlpcg_precond_smo_ILUn_overlap(double *z, double *r, DBCSR *csr, int nsweep, INTRA *actintra);
void mlpcg_precond_gramschmidt(double **P, double **R,const int nrow,const int ncol);
void mlpcg_precond_spoolessolve(double *z, double *r, DBCSR *csr, INTRA *actintra);
void mlpcg_precond_amgVW(int    level,
                         ARRAY *z_a,
                         ARRAY *r_a,
                         INTRA *actintra,
                         int   *gamma);
void mlpcg_printvec(int          iter,
                   double      *z, 
                   DBCSR*       csr,
                   DISCRET     *fielddis,
                   PARTDISCRET *partdis,
                   INTRA       *actintra);
void mlpcg_csr_overlap(DBCSR *csr, DBCSR *ocsr, DBCSR *ilu, int overlap, INTRA *actintra);
void mlpcg_csr_localnumsf_overlap(DBCSR *matrix);
void mlpcg_precond_P_fish(MLLEVEL  *actlev, INTRA *actintra);

