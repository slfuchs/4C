/*----------------------------------------------------------------------*/
/*!
\file fluid_impl.cpp

\brief Internal implementation of Fluid element

<pre>
Maintainer:

</pre>
*/
/*----------------------------------------------------------------------*/

#include "fluid_ele_calc.H"
#include "fluid_ele_parameter.H"

#include "../drt_inpar/inpar_turbulence.H"

#include "../drt_lib/drt_elementtype.H"

// turbulence model development
#include "../drt_fluid/fluid_turbulence_defines.H"


/*----------------------------------------------------------------------*
 |  compute turbulence parameters                       rasthofer 10/11 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::GetTurbulenceParams(
                               ParameterList&             turbmodelparams,
                               double&                    Cs_delta_sq,
                               int&                       nlayer,
                               double CsDeltaSq)
{
  if(fldpara_->TurbModAction() != INPAR::FLUID::no_model and nsd_ == 2)
    dserror("turbulence and 2D flow does not make any sense");

  // classical smagorinsky does only have constant parameter
  if (fldpara_->TurbModAction() == INPAR::FLUID::smagorinsky_with_van_Driest_damping)
  {
    // this will be the y-coordinate of a point in the element interior
    // we will determine the element layer in which he is contained to
    // be able to do the output of visceff etc.
    double center = 0.0;

    for(int inode=0;inode<nen_;inode++)
    {
      center += xyze_( 1, inode );
    }
    center/=nen_;

    // node coordinates of plane to the element layer
    RefCountPtr<vector<double> > planecoords
      =
      turbmodelparams.get<RefCountPtr<vector<double> > >("planecoords_");

    bool found = false;
    for (nlayer=0;nlayer<(int)(*planecoords).size()-1;)
    {
      if(center<(*planecoords)[nlayer+1])
      {
        found = true;
        break;
      }
      nlayer++;
    }
    if (found ==false)
    {
      dserror("could not determine element layer");
    }
  }
  // --------------------------------------------------
  // Smagorinsky model with dynamic Computation of Cs
  //else if (physical_turbulence_model == "Dynamic_Smagorinsky")
  else if (fldpara_->TurbModAction() == INPAR::FLUID::dynamic_smagorinsky)
  {
    //turb_mod_action_ = Fluid::dynamic_smagorinsky;

    // for homogeneous flow, use averaged quantities
    if (fldpara_->CsAveraged()==true){
    if (turbmodelparams.get<string>("HOMDIR","not_specified")
            !=
            "not_specified")
    {
      RCP<vector<double> > averaged_LijMij
        =
        turbmodelparams.get<RCP<vector<double> > >("averaged_LijMij_");
      RCP<vector<double> > averaged_MijMij
        =
        turbmodelparams.get<RCP<vector<double> > >("averaged_MijMij_");

      // get homogeneous direction
      string homdir = turbmodelparams.get<string>("HOMDIR","not_specified");

      // here, the layer is determined in order to get the correct
      // averaged value from the vector of averaged (M/L)ijMij
      double xcenter = 0.0;
      double ycenter = 0.0;
      double zcenter = 0.0;
      for(int inode=0;inode<nen_;inode++)
      {
        xcenter += xyze_( 0, inode );
        ycenter += xyze_( 1, inode );
        zcenter += xyze_( 2, inode );
      }
      xcenter/=nen_;
      ycenter/=nen_;
      zcenter/=nen_;

      if (homdir == "xy" or homdir == "xz" or homdir == "yz")
      {
        RCP<vector<double> > planecoords = turbmodelparams.get<RCP<vector<double> > >("planecoords_");
        // get center
        double center = 0.0;
        if (homdir == "xy")
          center = zcenter;
        else if (homdir == "xz")
          center = ycenter;
        else if (homdir == "yz")
          center = xcenter;

        bool found = false;
        for (nlayer=0;nlayer < static_cast<int>((*planecoords).size()-1);)
        {
          if(center<(*planecoords)[nlayer+1])
          {
            found = true;
            break;
          }
          nlayer++;
        }
        if (found ==false)
        {
          dserror("could not determine element layer");
        }
      }
      else if (homdir == "x" or homdir == "y" or homdir == "z")
      {
        RCP<vector<double> > dir1coords = turbmodelparams.get<RCP<vector<double> > >("dir1coords_");
        RCP<vector<double> > dir2coords = turbmodelparams.get<RCP<vector<double> > >("dir2coords_");
        // get center
        double dim1_center = 0.0;
        double dim2_center = 0.0;
        if (homdir == "x")
        {
          dim1_center = ycenter;
          dim2_center = zcenter;
        }
        else if (homdir == "y")
        {
          dim1_center = xcenter;
          dim2_center = zcenter;
        }
        else if (homdir == "z")
        {
          dim1_center = xcenter;
          dim2_center = ycenter;
        }

        int  n1layer;
        int  n2layer;
        bool dir1found = false;
        bool dir2found = false;
        for (n1layer=0;n1layer<(int)(*dir1coords).size()-1;)
        {
          if(dim1_center<(*dir1coords)[n1layer+1])
          {
            dir1found = true;
            break;
          }
          n1layer++;
        }
        if (dir1found ==false)
        {
          dserror("could not determine element layer");
        }
        for (n2layer=0;n2layer<(int)(*dir2coords).size()-1;)
        {
          if(dim2_center<(*dir2coords)[n2layer+1])
          {
            dir2found = true;
            break;
          }
          n2layer++;
        }
        if (dir2found ==false)
        {
          dserror("could not determine element layer");
        }

        const int numdir1layer = (int)(*dir2coords).size()-1;
        nlayer = numdir1layer * n2layer + n1layer;
      }
      else
        dserror("More than two homogeneous directions not supported!");

      // Cs_delta_sq is set by the averaged quantities
      Cs_delta_sq = 0.5 * (*averaged_LijMij)[nlayer]/(*averaged_MijMij)[nlayer] ;

      // clipping to get algorithm stable
      if (Cs_delta_sq<0)
      {
        Cs_delta_sq=0;
      }
    }
    }
    else
    {
      // when no averaging was done, we just keep the calculated (clipped) value
      Cs_delta_sq = CsDeltaSq;
    }
  }
  return;
} // FluidEleCalc::GetTurbulenceParams


/*----------------------------------------------------------------------*
 |  calculation of (all-scale) subgrid viscosity               vg 09/09 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::CalcSubgrVisc(
  const LINALG::Matrix<nsd_,nen_>&        evelaf,
  const double                            vol,
  double&                                 Cs,
  double&                                 Cs_delta_sq,
  double&                                 l_tau
  )
{
  // cast dimension to a double varibale -> pow()
  const double dim = double (nsd_);
  //
  // SMAGORINSKY MODEL
  // -----------------
  //                                   +-                                 -+ 1
  //                               2   |          / h \           / h \    | -
  //    visc          = dens * lmix  * | 2 * eps | u   |   * eps | u   |   | 2
  //        turbulent           |      |          \   / ij        \   / ij |
  //                            |      +-                                 -+
  //                            |
  //                            |      |                                   |
  //                            |      +-----------------------------------+
  //                            |           'resolved' rate of strain
  //                    mixing length
  // -> either provided by dynamic modeling procedure and stored in Cs_delta_sq
  // -> or computed based on fixed Smagorinsky constant Cs:
  //             Cs = 0.17   (Lilly --- Determined from filter
  //                          analysis of Kolmogorov spectrum of
  //                          isotropic turbulence)
  //             0.1 < Cs < 0.24 (depending on the flow)
  //

  // compute (all-scale) rate of strain
  double rateofstrain = -1.0e30;
  rateofstrain = GetStrainRate(evelaf);

  if (fldpara_->TurbModAction() == INPAR::FLUID::dynamic_smagorinsky)
  {
    // subgrid viscosity
    sgvisc_ = densaf_ * Cs_delta_sq * rateofstrain;

    // for evaluation of statistics: remember the 'real' Cs
    Cs = sqrt(Cs_delta_sq)/pow((vol),(1.0/3.0));
  }
  else
  {
    if (fldpara_->TurbModAction() == INPAR::FLUID::smagorinsky_with_van_Driest_damping)
    {
      // since the Smagorinsky constant is only valid if hk is in the inertial
      // subrange of turbulent flows, the mixing length is damped in the
      // viscous near wall region using the van Driest damping function
      /*
                                     /         /   y+ \ \
                   lmix = Cs * hk * | 1 - exp | - ---- | |
                                     \         \   A+ / /
      */
      // A+ is a constant parameter, y+ the distance from the wall in wall
     // units
      const double A_plus = 26.0;
      double y_plus;

      // the integration point coordinate is defined by the isometric approach
      /*
                  +-----
                   \
              x =   +      N (x) * x
                   /        j       j
                  +-----
                  node j
      */

      LINALG::Matrix<nsd_,1> centernodecoord;
      centernodecoord.Multiply(xyze_,funct_);

      if (centernodecoord(1,0)>0) y_plus=(1.0-centernodecoord(1,0))/l_tau;
      else                        y_plus=(1.0+centernodecoord(1,0))/l_tau;

      //   lmix *= (1.0-exp(-y_plus/A_plus));
      // multiply with van Driest damping function
      Cs *= (1.0-exp(-y_plus/A_plus));
    }

    // get characteristic element length for Smagorinsky model for 2D and 3D
    // 3D: hk = V^1/3
    // 2D: hk = A^1/2
    const double hk = pow(vol,(1.0/dim));

    // mixing length set proportional to grid witdh: lmix = Cs * hk
    double lmix = Cs * hk;

    Cs_delta_sq = lmix * lmix;

    // subgrid viscosity
    sgvisc_ = densaf_ * Cs_delta_sq * rateofstrain;
  }

  return;
}


/*----------------------------------------------------------------------*
 |  calculation of fine-scale subgrid viscosity                vg 09/09 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::CalcFineScaleSubgrVisc(
  const LINALG::Matrix<nsd_,nen_>&        evelaf,
  const LINALG::Matrix<nsd_,nen_>&        fsevelaf,
  const double                            vol,
  double&                                 Cs
  )
{
  // cast dimension to a double varibale -> pow()
  const double dim = double (nsd_);

  //     // get characteristic element length for Smagorinsky model for 2D and 3D
  // 3D: hk = V^1/3
  // 2D: hk = A^1/2
  const double hk = pow(vol,(1.0/dim));

  if (fldpara_->Fssgv() == INPAR::FLUID::smagorinsky_all)
  {
    //
    // ALL-SCALE SMAGORINSKY MODEL
    // ---------------------------
    //                                      +-                                 -+ 1
    //                                  2   |          / h \           / h \    | -
    //    visc          = dens * (C_S*h)  * | 2 * eps | u   |   * eps | u   |   | 2
    //        turbulent                     |          \   / ij        \   / ij |
    //                                      +-                                 -+
    //                                      |                                   |
    //                                      +-----------------------------------+
    //                                            'resolved' rate of strain
    //

    // compute (all-scale) rate of strain
    double rateofstrain = -1.0e30;
    rateofstrain = GetStrainRate(evelaf);

    fssgvisc_ = densaf_ * Cs * Cs * hk * hk * rateofstrain;
  }
  else if (fldpara_->Fssgv() == INPAR::FLUID::smagorinsky_small)
  {
    //
    // FINE-SCALE SMAGORINSKY MODEL
    // ----------------------------
    //                                      +-                                 -+ 1
    //                                  2   |          /    \          /   \    | -
    //    visc          = dens * (C_S*h)  * | 2 * eps | fsu |   * eps | fsu |   | 2
    //        turbulent                     |          \   / ij        \   / ij |
    //                                      +-                                 -+
    //                                      |                                   |
    //                                      +-----------------------------------+
    //                                            'resolved' rate of strain
    //

    // fine-scale rate of strain
    double fsrateofstrain = -1.0e30;
    fsrateofstrain = GetStrainRate(fsevelaf);

    fssgvisc_ = densaf_ * Cs * Cs * hk * hk * fsrateofstrain;
  }

  return;
}


/*----------------------------------------------------------------------*
 |  compute multifractal subgrid scales parameters    rasthofer 04/2011 |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::PrepareMultifractalSubgrScales(
  LINALG::Matrix<nsd_,1>&           B_mfs,
  double &                          D_mfs,
  const LINALG::Matrix<nsd_,nen_>&  evelaf,
  const LINALG::Matrix<nsd_,nen_>&  fsevelaf,
  const double                      vol
)
{
    // set input parameters
    double Csgs = fldpara_->Csgs();
    double alpha = fldpara_->Alpha();

    // allocate vector for parameter N
    // N may depend on the direction
    vector<double> Nvel (3);

    // potential calculation of Re to determine N
    double Re_ele = -1.0;
    // characteristic element length
    double hk = 1.0e+10;
    double strainnorm = 0.0;
    // ratio of viscous scale to element length
    double scale_ratio = 0.0;

    // get norm
    const double vel_norm = velint_.Norm2();
    const double fsvel_norm = fsvelint_.Norm2();

    // do we have a fixed parameter N
    if (not fldpara_->CalcN())
    {
      for (int rr=1;rr<3;rr++)
        Nvel[rr] = fldpara_->N();
#ifdef DIR_N // direction dependent stuff, currently not used
    Nvel[0] = NUMX;
    Nvel[1] = NUMY;
    Nvel[2] = NUMZ;
#endif
    }
    else //no, so we calculate N from Re
    {
      // calculate characteristic element length
      // cf. stabilization parameters
      switch (fldpara_->RefLength()){
      case INPAR::FLUID::streamlength:
      {
        // a) streamlength due to Tezduyar et al. (1992)
        // normed velocity vector
        LINALG::Matrix<nsd_,1> velino(true);
        if (vel_norm>=1e-6) velino.Update(1.0/vel_norm,velint_);
        else
        {
          velino.Clear();
          velino(0,0) = 1.0;
        }
        LINALG::Matrix<nen_,1> tmp;
        tmp.MultiplyTN(derxy_,velino);
        const double val = tmp.Norm1();
        hk = 2.0/val;

        break;
      }
      case INPAR::FLUID::sphere_diameter:
      {
        // b) volume-equivalent diameter
        hk = pow((6.*vol/M_PI),(1.0/3.0))/sqrt(3.0);

        break;
      }
      case INPAR::FLUID::cube_edge:
      {
        // c) qubic element length
        hk = pow(vol,(1.0/nsd_));
        break;
      }
      case INPAR::FLUID::metric_tensor:
      {
        /*          +-           -+   +-           -+   +-           -+
                    |             |   |             |   |             |
                    |  dr    dr   |   |  ds    ds   |   |  dt    dt   |
              G   = |  --- * ---  | + |  --- * ---  | + |  --- * ---  |
               ij   |  dx    dx   |   |  dx    dx   |   |  dx    dx   |
                    |    i     j  |   |    i     j  |   |    i     j  |
                    +-           -+   +-           -+   +-           -+
        */
        LINALG::Matrix<3,3> G;

        for (int nn=0;nn<3;++nn)
        {
          for (int rr=0;rr<3;++rr)
          {
            G(nn,rr) = xji_(nn,0)*xji_(rr,0);
            for (int mm=1;mm<3;++mm)
            {
              G(nn,rr) += xji_(nn,mm)*xji_(rr,mm);
            }
          }
        }

        /*          +----
                     \
            G : G =   +   G   * G
            -   -    /     ij    ij
            -   -   +----
                     i,j
        */
        double normG = 0;
        for (int nn=0;nn<3;++nn)
        {
          for (int rr=0;rr<3;++rr)
          {
            normG+=G(nn,rr)*G(nn,rr);
          }
        }
        hk = pow(normG,-0.25);

        break;
      }
      case INPAR::FLUID::gradient_based:
      {
        LINALG::Matrix<3,1> normed_velgrad;

        for (int rr=0;rr<3;++rr)
        {
          normed_velgrad(rr)=sqrt(vderxy_(0,rr)*vderxy_(0,rr)
                                +
                                vderxy_(1,rr)*vderxy_(1,rr)
                                +
                                vderxy_(2,rr)*vderxy_(2,rr));
        }
        double norm=normed_velgrad.Norm2();

        // normed gradient
        if (norm>1e-6)
        {
          for (int rr=0;rr<3;++rr)
          {
            normed_velgrad(rr)/=norm;
          }
        }
        else
        {
          normed_velgrad(0) = 1.;
          for (int rr=1;rr<3;++rr)
          {
            normed_velgrad(rr)=0.0;
          }
        }

        // get length in this direction
        double val = 0.0;
        for (int rr=0;rr<nen_;++rr) /* loop element nodes */
        {
          val += abs( normed_velgrad(0)*derxy_(0,rr)
                      +normed_velgrad(1)*derxy_(1,rr)
                      +normed_velgrad(2)*derxy_(2,rr));
        } /* end of loop over element nodes */

        hk = 2.0/val;

        break;
      }
      default:
        dserror("Unknown length");
      }

// alternative length for comparison, currently not used
#ifdef HMIN
      double xmin = 0.0;
      double ymin = 0.0;
      double zmin = 0.0;
      double xmax = 0.0;
      double ymax = 0.0;
      double zmax = 0.0;
      for (int inen=0; inen<nen_; inen++)
      {
        if (inen == 0)
        {
          xmin = xyze_(0,inen);
          xmax = xyze_(0,inen);
          ymin = xyze_(1,inen);
          ymax = xyze_(1,inen);
          zmin = xyze_(2,inen);
          zmax = xyze_(2,inen);
        }
        else
        {
          if(xyze_(0,inen)<xmin)
            xmin = xyze_(0,inen);
          if(xyze_(0,inen)>xmax)
            xmax = xyze_(0,inen);
          if(xyze_(1,inen)<ymin)
            ymin = xyze_(1,inen);
          if(xyze_(1,inen)>ymax)
            ymax = xyze_(1,inen);
          if(xyze_(2,inen)<zmin)
            zmin = xyze_(2,inen);
          if(xyze_(2,inen)>zmax)
            zmax = xyze_(2,inen);
        }
      }
      if ((xmax-xmin) < (ymax-ymin))
      {
        if ((xmax-xmin) < (zmax-zmin))
           hk = xmax-xmin;
      }
      else
      {
        if ((ymax-ymin) < (zmax-zmin))
           hk = ymax-ymin;
        else
           hk = zmax-zmin;
      }
#endif
#ifdef HMAX
      double xmin = 0.0;
      double ymin = 0.0;
      double zmin = 0.0;
      double xmax = 0.0;
      double ymax = 0.0;
      double zmax = 0.0;
      for (int inen=0; inen<nen_; inen++)
      {
        if (inen == 0)
        {
          xmin = xyze_(0,inen);
          xmax = xyze_(0,inen);
          ymin = xyze_(1,inen);
          ymax = xyze_(1,inen);
          zmin = xyze_(2,inen);
          zmax = xyze_(2,inen);
        }
        else
        {
          if(xyze_(0,inen)<xmin)
            xmin = xyze_(0,inen);
          if(xyze_(0,inen)>xmax)
            xmax = xyze_(0,inen);
          if(xyze_(1,inen)<ymin)
            ymin = xyze_(1,inen);
          if(xyze_(1,inen)>ymax)
            ymax = xyze_(1,inen);
          if(xyze_(2,inen)<zmin)
            zmin = xyze_(2,inen);
          if(xyze_(2,inen)>zmax)
            zmax = xyze_(2,inen);
        }
      }
      if ((xmax-xmin) > (ymax-ymin))
      {
        if ((xmax-xmin) > (zmax-zmin))
           hk = xmax-xmin;
      }
      else
      {
        if ((ymax-ymin) > (zmax-zmin))
           hk = ymax-ymin;
        else
           hk = zmax-zmin;
      }
#endif

      if (hk == 1.0e+10)
        dserror("Something went wrong!");

      switch (fldpara_->RefVel()){
      case INPAR::FLUID::resolved:
      {
        Re_ele = vel_norm * hk * densaf_ / visc_;
        break;
      }
      case INPAR::FLUID::fine_scale:
      {
        Re_ele = fsvel_norm * hk * densaf_ / visc_;
        break;
      }
      case INPAR::FLUID::strainrate:
      {
        //strainnorm = GetNormStrain(evelaf,derxy_,vderxy_);
        strainnorm = GetStrainRate(evelaf);
        strainnorm /= sqrt(2.0);
        Re_ele = strainnorm * hk * hk * densaf_ / visc_;
        break;
      }
      default:
        dserror("Unknown velocity!");
      }
      if (Re_ele < 0.0)
        dserror("Something went wrong!");

      // clip Re to prevent negative N
      if (Re_ele < 1.0)
         Re_ele = 1.0;

      //
      //   Delta
      //  ---------  ~ Re^(3/4)
      //  lambda_nu
      //
      scale_ratio = fldpara_->CNu() * pow(Re_ele,3.0/4.0);
      // scale_ratio < 1.0 leads to N < 0
      // therefore, we clip once more
      if (scale_ratio < 1.0)
        scale_ratio = 1.0;

      //         |   Delta     |
      //  N =log | ----------- |
      //        2|  lambda_nu  |
      double N_re = log(scale_ratio)/log(2.0);
      if (N_re < 0.0)
        dserror("Something went wrong when calculating N!");

      // store calculated N
      for (int i=0; i<nsd_; i++)
        Nvel[i] = N_re;
    }

#ifdef DIR_N
    vector<double> weights (3);
    weights[0] = WEIGHT_NX;
    weights[1] = WEIGHT_NY;
    weights[2] = WEIGHT_NZ;
    for (int i=0; i<nsd_; i++)
      Nvel[i] *= weights[i];
#endif

    // calculate near-wall correction
    if (fldpara_->NearWallLimit())
    {
      // not yet calculated, estimate norm of strain rate
      if (fldpara_->CalcN() or fldpara_->RefVel() != INPAR::FLUID::strainrate)
      {
        //strainnorm = GetNormStrain(evelaf,derxy_,vderxy_);
        strainnorm = GetStrainRate(evelaf);
        strainnorm /= sqrt(2.0);
      }

      // get Re from strain rate
      double Re_ele_str = strainnorm * hk * hk * densaf_ / visc_;
      if (Re_ele_str < 0.0)
        dserror("Something went wrong!");
      // ensure positive values
      if (Re_ele_str < 1.0)
         Re_ele_str = 1.0;

      // calculate corrected Csgs
      //           -3/16
      //  *(1 - (Re)   )
      //
      Csgs *= (1-pow(Re_ele_str,-3.0/16.0));
    }

    // call function to compute coefficient B
    CalcMultiFracSubgridVelCoef(Csgs,alpha,Nvel,B_mfs);

    // prepare calculation of subgrid-scalar coefficient for loma
    // required if further subgrid-scale terms of cross- and Reynolds-stress
    // type arising in the continuity equation should be included
    if (fldpara_->PhysicalType() == INPAR::FLUID::loma)
    {
      // set input parameters
      double Csgs_phi = fldpara_->CsgsPhi();

      // calculate prandtl number
      double Pr = visc_/diffus_;

      // allocate vector for parameter N
      double Nphi = 0.0;
      // ratio of dissipation scale to element length
      double scale_ratio_phi = 0.0;

      if (fldpara_->CalcN())
      {
        //
        //   Delta
        //  ---------  ~ Re^(3/4)*Pr^(1/2)
        //  lambda_diff
        //
        scale_ratio_phi = fldpara_->CDiff() * pow(Re_ele,3.0/4.0) * pow(Pr,1.0/2.0);
        // scale_ratio < 1.0 leads to N < 0
        // therefore, we clip again
        if (scale_ratio_phi < 1.0)
          scale_ratio_phi = 1.0;

        //         |   Delta     |
        //  N =log | ----------- |
        //        2|  lambda_nu  |
       Nphi = log(scale_ratio_phi)/log(2.0);
       if (Nphi < 0.0)
          dserror("Something went wrong when calculating N!");
      }
      else
       dserror("Multifractal subgrid-scales for loma with calculation of N, only!");

      // call function to compute coefficient D
      CalcMultiFracSubgridScaCoef(Csgs_phi,alpha,Pr,Nvel,Nphi,D_mfs);
    }
}



/*-------------------------------------------------------------------------------*
 |calculation parameter for multifractal subgrid scale modeling  rasthofer 03/11 |
 *-------------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::CalcMultiFracSubgridVelCoef(
  const double            Csgs,
  const double            alpha,
  const vector<double>    Nvel,
  LINALG::Matrix<nsd_,1>& B_mfs
  )
{
  //
  //          |       1              |
  //  kappa = | -------------------- |
  //          |  1 - alpha ^ (-4/3)  |
  //
  double kappa = 1.0/(1.0-pow(alpha,-4.0/3.0));

  //                                                       1
  //                                  |                   |2
  //  B = Csgs * kappa * 2 ^ (-2*N/3) * | 2 ^ (4*N/3) - 1 |
  //                                  |                   |
  //
  for (int dim=0; dim<nsd_; dim++)
  {
    B_mfs(dim,0) = Csgs *sqrt(kappa) * pow(2.0,-2.0*Nvel[dim]/3.0) * sqrt((pow(2.0,4.0*Nvel[dim]/3.0)-1));
  }

#ifdef CONST_B // overwrite all, just for testing
  for (int dim=0; dim<nsd_; dim++)
  {
    B_mfs(dim,0) = B_CONST;
  }
#endif

  return;
}


/*-------------------------------------------------------------------------------*
 |calculation parameter for multifractal subgrid scale modeling  rasthofer 02/12 |
 |subgrid-scale scalar for loma                                                  |
 *-------------------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::CalcMultiFracSubgridScaCoef(
  const double            Csgs,
  const double            alpha,
  const double            Pr,
  const vector<double>    Nvel,
  double                  Nphi,
  double &                D_mfs
  )
{
  // here, we have to distinguish tree different cases:
  // Pr ~ 1 : fluid and scalar field have the nearly the same cutoff (usual case)
  //          k^(-5/3) scaling -> gamma = 4/3
  // Pr >> 1: (i)  cutoff in the inertial-convective range (Nvel>0, tricky!)
  //               k^(-5/3) scaling in the inertial-convective range
  //               k^(-1) scaling in the viscous-convective range
  //          (ii) cutoff in the viscous-convective range (fluid field fully resolved, easier)
  //               k^(-1) scaling -> gamma = 2
  // rare:
  // Pr << 1: scatra field could be fully resolved, not necessary
  //          k^(-5/3) scaling -> gamma = 4/3
  // Remark: case 2.(i) not implemented, yet

  double gamma = 0.0;
  if (Pr < 2.0) // Pr <= 1, i.e., case 1 and 3
    gamma = 4.0/3.0;
  else if (Pr > 2.0 and Nvel[0]<1.0) // Pr >> 1, i.e., case 2 (ii)
    gamma = 2.0;
  else if (Pr > 2.0 and Nvel[0]<Nphi)
    dserror("Inertial-convective and viscous-convective range?");
  else
    dserror("Could not determine D!");

  //
  //   Phi    |       1                |
  //  kappa = | ---------------------- |
  //          |  1 - alpha ^ (-gamma)  |
  //
  double kappa_phi = 1.0/(1.0-pow(alpha,-gamma));

  //                                                             1
  //       Phi    Phi                       |                   |2
  //  D = Csgs * kappa * 2 ^ (-gamma*N/2) * | 2 ^ (gamma*N) - 1 |
  //                                        |                   |
  //
  D_mfs = Csgs *sqrt(kappa_phi) * pow(2.0,-gamma*Nphi/2.0) * sqrt((pow(2.0,gamma*Nphi)-1));

  return;
}


template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::FineScaleSubGridViscosityTerm(
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          fssgviscfac)
{
  if (nsd_ == 2)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
      /* fine-scale subgrid-viscosity term on right hand side */
      /*
                          /                          \
                         |       /    \         / \   |
         - mu_art(fsu) * |  eps | Dfsu | , eps | v |  |
                         |       \    /         \ /   |
                          \                          /
      */
      velforce(0, vi) -= fssgviscfac*(2.0*derxy_(0, vi)*fsvderxy_(0, 0)
                                     +    derxy_(1, vi)*fsvderxy_(0, 1)
                                     +    derxy_(1, vi)*fsvderxy_(1, 0)) ;
      velforce(1, vi) -= fssgviscfac*(    derxy_(0, vi)*fsvderxy_(0, 1)
                                     +    derxy_(0, vi)*fsvderxy_(1, 0)
                                     +2.0*derxy_(1, vi)*fsvderxy_(1, 1)) ;
    }
  }
  else if(nsd_ == 3)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
      /* fine-scale subgrid-viscosity term on right hand side */
      /*
                            /                          \
                           |       /    \         / \   |
           - mu_art(fsu) * |  eps | Dfsu | , eps | v |  |
                           |       \    /         \ /   |
                            \                          /
      */
      velforce(0, vi) -= fssgviscfac*(2.0*derxy_(0, vi)*fsvderxy_(0, 0)
                                     +    derxy_(1, vi)*fsvderxy_(0, 1)
                                     +    derxy_(1, vi)*fsvderxy_(1, 0)
                                     +    derxy_(2, vi)*fsvderxy_(0, 2)
                                     +    derxy_(2, vi)*fsvderxy_(2, 0)) ;
      velforce(1, vi) -= fssgviscfac*(    derxy_(0, vi)*fsvderxy_(0, 1)
                                     +    derxy_(0, vi)*fsvderxy_(1, 0)
                                     +2.0*derxy_(1, vi)*fsvderxy_(1, 1)
                                     +    derxy_(2, vi)*fsvderxy_(1, 2)
                                     +    derxy_(2, vi)*fsvderxy_(2, 1)) ;
      velforce(2, vi) -= fssgviscfac*(    derxy_(0, vi)*fsvderxy_(0, 2)
                                     +    derxy_(0, vi)*fsvderxy_(2, 0)
                                     +    derxy_(1, vi)*fsvderxy_(1, 2)
                                     +    derxy_(1, vi)*fsvderxy_(2, 1)
                                     +2.0*derxy_(2, vi)*fsvderxy_(2, 2)) ;
    }
  }
  else dserror("fine-scale subgrid viscosity not implemented for 1-D problems!");

  return;
}


//----------------------------------------------------------------------
// Basic scale-similarity                                rasthofer 01/11
//----------------------------------------------------------------------
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::ScaleSimSubGridStressTermPrefiltering(
//    const int &                             eid,
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          rhsfac,
    const double &                          Cl)
{
  if (nsd_ == 3)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
          /* subgrid-stress term on right hand side */
          /*
                        /                                \
                       |             ^     ^   ^          |
                       | nabla o ( (u*u) - u * u ) ,  v   |
                       |                                  |
                        \                                /
          */
       for (int nn=0; nn<nsd_; nn++)
       {
#if 1
         // convective form: div u_hat = 0 assumed
         velforce(nn, vi) -= Cl * rhsfac * densaf_ * funct_(vi)
                             * (reystresshatdiv_(nn,0)
                             - (velinthat_(0,0) * velhatderxy_(nn,0)
                               +velinthat_(1,0) * velhatderxy_(nn,1)
                               +velinthat_(2,0) * velhatderxy_(nn,2)
                               + velinthat_(nn,0) * velhatdiv_));
         if (fldpara_->IsConservative())
         {
           velforce(nn, vi) += Cl * rhsfac * densaf_ * funct_(vi) * velinthat_(nn,0) * velhatdiv_;
         }
#else
         velforce(nn, vi) -= Cl * rhsfac * densaf_ * funct_(vi)
                             * (reystresshatdiv_(nn,0) - velhativelhatjdiv_(nn,0));
#endif
       }
    }
// // with partial integration of subfilter-stress term, boundary integral is assumed included in Neumann BC
//    for (int vi=0; vi<nen_; ++vi)
//    {
//              // subgrid-stress term on right hand side //
//              //
                /*
                              /                             \
                             |     ^     ^   ^               |
                             | ( (u*u) - u * u ) , grad(v)   |
                             |                               |
                              \                             /
                */
//      for (int nn=0; nn<nsd_; nn++)
//      {
//        velforce(nn,vi) += Cl * rhsfac * densaf_
//                         * (derxy_(0, vi)* (reystressinthat_(nn,0) - velinthat_(nn,0) * velinthat_(0,0))
//                         +  derxy_(1, vi)* (reystressinthat_(nn,1) - velinthat_(nn,0) * velinthat_(1,0))
//                         +  derxy_(2, vi)* (reystressinthat_(nn,2) - velinthat_(nn,0) * velinthat_(2,0)));
//      }
//    }
//
  }
  else
    dserror("Scale similarity model for 3D-problems only!");

  return;
}


//----------------------------------------------------------------------
// Cross-stress terms: scale-similarity                  rasthofer 03/11
//----------------------------------------------------------------------
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::ScaleSimSubGridStressTermCross(
//    const int &                             eid,
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          rhsfac,
    const double &                          Cl)
{
  if (nsd_ == 3)
  {
    // with partial integration of subfilter-stress term, boundary integral is assumed included in Neumann BC
    for (int vi=0; vi<nen_; ++vi)
    {
              /* cross-stress term on right hand side */
              /*
                            /                               \
                           |        ^   ^                    |
                           | ( du * u - u * du ) ,  eps(v)   |
                           |                                 |
                            \                               /
              */

        velforce(0,vi) += 0.5 *Cl * rhsfac * densaf_
                        * ((2.0*derxy_(0, vi)*(fsvelint_(0,0)*velinthat_(0,0)+velinthat_(0,0)*fsvelint_(0,0))
                           +    derxy_(1, vi)*(fsvelint_(1,0)*velinthat_(0,0)+velinthat_(1,0)*fsvelint_(0,0))
                           +    derxy_(1, vi)*(fsvelint_(0,0)*velinthat_(1,0)+velinthat_(0,0)*fsvelint_(1,0))
                           +    derxy_(2, vi)*(fsvelint_(0,0)*velinthat_(2,0)+velinthat_(0,0)*fsvelint_(2,0))
                           +    derxy_(2, vi)*(fsvelint_(2,0)*velinthat_(0,0)+velinthat_(2,0)*fsvelint_(0,0))));
        velforce(1,vi) += 0.5 *Cl * rhsfac * densaf_
                        * ((    derxy_(0, vi)*(fsvelint_(0,0)*velinthat_(1,0)+velinthat_(0,0)*fsvelint_(1,0))
                           +    derxy_(0, vi)*(fsvelint_(1,0)*velinthat_(0,0)+velinthat_(1,0)*fsvelint_(0,0))
                           +2.0*derxy_(1, vi)*(fsvelint_(1,0)*velinthat_(1,0)+velinthat_(1,0)*fsvelint_(1,0))
                           +    derxy_(2, vi)*(fsvelint_(1,0)*velinthat_(2,0)+velinthat_(1,0)*fsvelint_(2,0))
                           +    derxy_(2, vi)*(fsvelint_(2,0)*velinthat_(1,0)+velinthat_(2,0)*fsvelint_(1,0))));
        velforce(2,vi) += 0.5 *Cl * rhsfac * densaf_
                        * ((    derxy_(0, vi)*(fsvelint_(0,0)*velinthat_(2,0)+velinthat_(0,0)*fsvelint_(2,0))
                           +    derxy_(0, vi)*(fsvelint_(2,0)*velinthat_(0,0)+velinthat_(2,0)*fsvelint_(0,0))
                           +    derxy_(1, vi)*(fsvelint_(1,0)*velinthat_(2,0)+velinthat_(1,0)*fsvelint_(2,0))
                           +    derxy_(1, vi)*(fsvelint_(2,0)*velinthat_(1,0)+velinthat_(2,0)*fsvelint_(1,0))
                           +2.0*derxy_(2, vi)*(fsvelint_(2,0)*velinthat_(2,0)+velinthat_(2,0)*fsvelint_(2,0))));
      }
    }
  else
    dserror("Scale similarity model for 3D-problems only!");

  return;
}


//----------------------------------------------------------------------
// Reynolds-stress term: scale-similarity                rasthofer 03/11
//----------------------------------------------------------------------
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::ScaleSimSubGridStressTermReynolds(
//    const int &                             eid,
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          rhsfac,
    const double &                          Cl)
{
  if (nsd_ == 3)
  {
    // with partial integration of subfilter-stress term, boundary integral is assumed included in Neumann BC
    for (int vi=0; vi<nen_; ++vi)
    {
              /* subgrid-stress term on right hand side */
              /*
                            /                      \
                           |                        |
                           | ( du * du ) , eps(v)   |
                           |                        |
                            \                      /
              */

        velforce(0,vi) += 0.5 *Cl * rhsfac * densaf_
                        * ((2.0*derxy_(0, vi)*(fsvelint_(0,0)*fsvelint_(0,0))
                           +    derxy_(1, vi)*(fsvelint_(1,0)*fsvelint_(0,0))
                           +    derxy_(1, vi)*(fsvelint_(0,0)*fsvelint_(1,0))
                           +    derxy_(2, vi)*(fsvelint_(0,0)*fsvelint_(2,0))
                           +    derxy_(2, vi)*(fsvelint_(2,0)*fsvelint_(0,0))));
        velforce(1,vi) += 0.5 *Cl * rhsfac * densaf_
                        * ((    derxy_(0, vi)*(fsvelint_(0,0)*fsvelint_(1,0))
                           +    derxy_(0, vi)*(fsvelint_(1,0)*fsvelint_(0,0))
                           +2.0*derxy_(1, vi)*(fsvelint_(1,0)*fsvelint_(1,0))
                           +    derxy_(2, vi)*(fsvelint_(1,0)*fsvelint_(2,0))
                           +    derxy_(2, vi)*(fsvelint_(2,0)*fsvelint_(1,0))));
        velforce(2,vi) += 0.5 *Cl * rhsfac * densaf_
                        * ((    derxy_(0, vi)*(fsvelint_(0,0)*fsvelint_(2,0))
                           +    derxy_(0, vi)*(fsvelint_(2,0)*fsvelint_(0,0))
                           +    derxy_(1, vi)*(fsvelint_(1,0)*fsvelint_(2,0))
                           +    derxy_(1, vi)*(fsvelint_(2,0)*fsvelint_(1,0))
                           +2.0*derxy_(2, vi)*(fsvelint_(2,0)*fsvelint_(2,0))));

    }
  }
  else
    dserror("Scale similarity model for 3D-problems only!");

  return;
}


//----------------------------------------------------------------------
// Cross-stress terms: multifractal subgrid-scales       rasthofer 06/11
//----------------------------------------------------------------------
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::MultfracSubGridScalesCross(
    LINALG::Matrix<nen_*nsd_,nen_*nsd_> &   estif_u,
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          timefacfac,
    const double &                          rhsfac)
{
  //--------------------------------------------------------------------
  // rhs contribution
  //--------------------------------------------------------------------
  if (nsd_ == 3)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
      /* cross-stress term on right hand side */
      /*
               /                                      \
              |                                        |
              | ( du o nabla u - u o nabla du ) ,  v   |
              |                                        |
               \                                      /
      */
      velforce(0,vi) -= rhsfac * densaf_ * funct_(vi,0)
                      * (velint_(0,0) * mffsvderxy_(0,0)
                        +velint_(1,0) * mffsvderxy_(0,1)
                        +velint_(2,0) * mffsvderxy_(0,2)
                        +mffsvelint_(0,0) * vderxy_(0,0)
                        +mffsvelint_(1,0) * vderxy_(0,1)
                        +mffsvelint_(2,0) * vderxy_(0,2));
      velforce(1,vi) -= rhsfac * densaf_ * funct_(vi,0)
                      * (velint_(0,0) * mffsvderxy_(1,0)
                        +velint_(1,0) * mffsvderxy_(1,1)
                        +velint_(2,0) * mffsvderxy_(1,2)
                        +mffsvelint_(0,0) * vderxy_(1,0)
                        +mffsvelint_(1,0) * vderxy_(1,1)
                        +mffsvelint_(2,0) * vderxy_(1,2));
      velforce(2,vi) -= rhsfac * densaf_ * funct_(vi,0)
                      * (velint_(0,0) * mffsvderxy_(2,0)
                        +velint_(1,0) * mffsvderxy_(2,1)
                        +velint_(2,0) * mffsvderxy_(2,2)
                        +mffsvelint_(0,0) * vderxy_(2,0)
                        +mffsvelint_(1,0) * vderxy_(2,1)
                        +mffsvelint_(2,0) * vderxy_(2,2));

      /* cross-stress term on right hand side */
      /* additional terms conservative form */
      /*
               /                                         \
              |                                           |
              | ( du (nabla o u) - u (nabla o du ) ,  v   |
              |                                           |
               \                                         /
      */
      if (fldpara_->IsConservative())
      {
        velforce(0,vi) -= rhsfac * densaf_ * funct_(vi,0)
                        * (mffsvelint_(0,0) * vdiv_
                          +velint_(0,0) * mffsvdiv_);
        velforce(1,vi) -= rhsfac * densaf_ * funct_(vi,0)
                        * (mffsvelint_(1,0) * vdiv_
                          +velint_(1,0) * mffsvdiv_);
        velforce(2,vi) -= rhsfac * densaf_ * funct_(vi,0)
                        * (mffsvelint_(2,0) * vdiv_
                          +velint_(2,0) * mffsvdiv_);
      }
    }
  }
  else
    dserror("Scale similarity model for 3D-problems only!");

  //--------------------------------------------------------------------
  // lhs contribution
  //--------------------------------------------------------------------
  // linearized as far as possible due to the filter

  LINALG::Matrix<nen_,1> mfconv_c(true);
  mfconv_c.MultiplyTN(derxy_,mffsvelint_);
  // turn left-hand-side contribution on
  double beta = fldpara_->Beta();

  // convective part
  for (int ui=0; ui<nen_; ui++)
  {
    for (int idim=0; idim<nsd_; idim++)
    {
      int fui = ui * nsd_ + idim;
      for (int vi=0; vi<nen_; vi++)
      {
        for (int jdim=0; jdim<nsd_; jdim++)
        {
          int fvi = vi * nsd_ + jdim;
          /*
                    /                             \
                   |  /                 \          |
                   | |   rho*Du  o nabla | du , v  |
                   |  \                 /          |
                    \                             /
          */
          estif_u(fvi,fui) += beta * timefacfac * densaf_ * funct_(vi)
                            * funct_(ui) * mffsvderxy_(jdim,idim);
          /*
                    /                             \
                   |  /                 \          |
                   | |   rho*du  o nabla | Du , v  |
                   |  \                 /          |
                    \                             /
          */
          if (jdim == idim)
          {
            estif_u(fvi,fui) += beta * timefacfac * densaf_ * funct_(vi)
                              * mfconv_c(ui);
          }

          // additional terms conservative part
          if (fldpara_->IsConservative())
          {
            /*
                   /                                     \
                   |      /               \       \      |
                   |  du | rho*nabla o Du  | , v   |     |
                   |      \               /       /      |
                   \                                     /
            */
            estif_u(fvi,fui) += beta * timefacfac * densaf_ * funct_(vi)
                              * mffsvelint_(jdim) * derxy_(idim, ui);
              /*
                    /                                     \
                    |      /               \       \      |
                    |  Du | rho*nabla o du  | , v   |     |
                    |      \               /       /      |
                    \                                     /
              */
            if (jdim == idim)
            {
              estif_u(fvi,fui) += beta * timefacfac * densaf_
                                * funct_(vi) * funct_(ui) * mffsvdiv_;
            }
          }
        }
      }
    }
  }

  return;
}


//----------------------------------------------------------------------
// Reynolds-stress terms: multifractal subgrid-scales    rasthofer 06/11
//----------------------------------------------------------------------
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::MultfracSubGridScalesReynolds(
    LINALG::Matrix<nen_*nsd_,nen_*nsd_> &   estif_u,
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          timefacfac,
    const double &                          rhsfac)
{
  //--------------------------------------------------------------------
  // rhs contribution
  //--------------------------------------------------------------------
  if (nsd_ == 3)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
      /* reynolds-stress term on right hand side */
      /*
               /                       \
              |                         |
              | ( du o nabla du) ,  v   |
              |                         |
               \                       /
      */
      velforce(0,vi) -= rhsfac * densaf_ * funct_(vi,0)
                      * (mffsvelint_(0,0) * mffsvderxy_(0,0)
                        +mffsvelint_(1,0) * mffsvderxy_(0,1)
                        +mffsvelint_(2,0) * mffsvderxy_(0,2));
      velforce(1,vi) -= rhsfac * densaf_ * funct_(vi,0)
                      * (mffsvelint_(0,0) * mffsvderxy_(1,0)
                        +mffsvelint_(1,0) * mffsvderxy_(1,1)
                        +mffsvelint_(2,0) * mffsvderxy_(1,2));
      velforce(2,vi) -= rhsfac * densaf_ * funct_(vi,0)
                      * (mffsvelint_(0,0) * mffsvderxy_(2,0)
                        +mffsvelint_(1,0) * mffsvderxy_(2,1)
                        +mffsvelint_(2,0) * mffsvderxy_(2,2));

      /* reynolds-stress term on right hand side */
      /* additional terms conservative form */
      /*
               /                       \
              |                         |
              |   du (nabla o du),  v   |
              |                         |
               \                       /
      */
      if (fldpara_->IsConservative())
      {
        velforce(0,vi) -= rhsfac * densaf_ * funct_(vi,0)
                        * (mffsvelint_(0,0) * mffsvdiv_);
        velforce(1,vi) -= rhsfac * densaf_ * funct_(vi,0)
                        * (mffsvelint_(1,0) * mffsvdiv_);
        velforce(2,vi) -= rhsfac * densaf_ * funct_(vi,0)
                        * (mffsvelint_(2,0) * mffsvdiv_);
      }
    }
  }
  else
    dserror("Scale similarity model for 3D-problems only!");

  //--------------------------------------------------------------------
  // lhs contribution
  //--------------------------------------------------------------------
  // no contribution, due to necessary linearization of filter

  return;
}


template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::FluidEleCalc<distype>::FineScaleSimilaritySubGridViscosityTerm(
    LINALG::Matrix<nsd_,nen_> &             velforce,
    const double &                          fssgviscfac)
{
//  LINALG::Matrix<nsd_,nen_> velforceold (true);
  if (nsd_ == 2)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
      /* fine-scale subgrid-viscosity term on right hand side */
      /*
                          /                          \
                         |       /    \         / \   |
         - mu_art(fsu) * |  eps | Dfsu | , eps | v |  |
                         |       \    /         \ /   |
                          \                          /
      */
      velforce(0, vi) -= fssgviscfac*(2.0*derxy_(0, vi)*fsvderxy_(0, 0)
                                     +    derxy_(1, vi)*fsvderxy_(0, 1)
                                     +    derxy_(1, vi)*fsvderxy_(1, 0)) ;
      velforce(1, vi) -= fssgviscfac*(    derxy_(0, vi)*fsvderxy_(0, 1)
                                     +    derxy_(0, vi)*fsvderxy_(1, 0)
                                     +2.0*derxy_(1, vi)*fsvderxy_(1, 1)) ;
    }
  }
  else if(nsd_ == 3)
  {
    for (int vi=0; vi<nen_; ++vi)
    {
      /* fine-scale subgrid-viscosity term on right hand side */
      /*
                            /                          \
                           |       /    \         / \   |
           - mu_art(fsu) * |  eps | Dfsu | , eps | v |  |
                           |       \    /         \ /   |
                            \                          /
      */
      velforce(0, vi) -= fssgviscfac*(2.0*derxy_(0, vi)*fsvderxy_(0, 0)
                                     +    derxy_(1, vi)*fsvderxy_(0, 1)
                                     +    derxy_(1, vi)*fsvderxy_(1, 0)
                                     +    derxy_(2, vi)*fsvderxy_(0, 2)
                                     +    derxy_(2, vi)*fsvderxy_(2, 0)) ;
      velforce(1, vi) -= fssgviscfac*(    derxy_(0, vi)*fsvderxy_(0, 1)
                                     +    derxy_(0, vi)*fsvderxy_(1, 0)
                                     +2.0*derxy_(1, vi)*fsvderxy_(1, 1)
                                     +    derxy_(2, vi)*fsvderxy_(1, 2)
                                     +    derxy_(2, vi)*fsvderxy_(2, 1)) ;
      velforce(2, vi) -= fssgviscfac*(    derxy_(0, vi)*fsvderxy_(0, 2)
                                     +    derxy_(0, vi)*fsvderxy_(2, 0)
                                     +    derxy_(1, vi)*fsvderxy_(1, 2)
                                     +    derxy_(1, vi)*fsvderxy_(2, 1)
                                     +2.0*derxy_(2, vi)*fsvderxy_(2, 2)) ;
    }
  }
  else dserror("fine-scale subgrid viscosity not implemented for 1-D problems!");

  //std::cout << "rhs  " << velforceold << std::endl;
  return;
}

// Ursula is responsible for this comment!
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::hex8>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::hex20>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::hex27>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::tet4>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::tet10>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::wedge6>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::pyramid5>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::quad4>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::quad8>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::quad9>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::tri3>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::tri6>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::nurbs9>;
template class DRT::ELEMENTS::FluidEleCalc<DRT::Element::nurbs27>;
