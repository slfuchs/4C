/*!
\file drt_utils_fem_shapefunctions.cpp
\brief

<pre>
Maintainer: Axel Gerstenberger
            gerstenberger@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>

*/
#ifdef CCADISCRET
#ifdef TRILINOS_PACKAGE

#include "drt_element.H"
#include "drt_discret.H"
#include "drt_utils.H"
#include "drt_dserror.H"



//
// shape functions
//
void DRT::Utils::shape_function_3D(
  Epetra_SerialDenseVector&                  funct,
  const double&                              r,
  const double&                              s,
  const double&                              t,
  const DRT::Element::DiscretizationType&    distype)
{
    const double Q18 = 1.0/8.0;
    const double Q12 = 1.0/2.0;

    switch (distype)
    {
    case DRT::Element::hex8:
    {
       const double rp=1.0+r;
       const double rm=1.0-r;
       const double sp=1.0+s;
       const double sm=1.0-s;
       const double tp=1.0+t;
       const double tm=1.0-t;

       funct[0]=Q18*rp*sm*tm;
       funct[1]=Q18*rp*sp*tm;
       funct[2]=Q18*rm*sp*tm;
       funct[3]=Q18*rm*sm*tm;
       funct[4]=Q18*rp*sm*tp;
       funct[5]=Q18*rp*sp*tp;
       funct[6]=Q18*rm*sp*tp;
       funct[7]=Q18*rm*sm*tp;
       break;
    }
    case DRT::Element::hex20:
    {
        dserror("shape functions for hex20 are not validated!!! \n");

        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double tp=1.0+t;
        const double tm=1.0-t;
        const double rrm=1.0-r*r;
        const double ssm=1.0-s*s;
        const double ttm=1.0-t*t;

        funct[0] =Q18*rp*sm*tm*(rp+sm+tm-5.0);
        funct[1] =Q18*rp*sp*tm*(rp+sp+tm-5.0);
        funct[2] =Q18*rm*sp*tm*(rm+sp+tm-5.0);
        funct[3] =Q18*rm*sm*tm*(rm+sm+tm-5.0);
        funct[4] =Q18*rp*sm*tp*(rp+sm+tp-5.0);
        funct[5] =Q18*rp*sp*tp*(rp+sp+tp-5.0);
        funct[6] =Q18*rm*sp*tp*(rm+sp+tp-5.0);
        funct[7] =Q18*rm*sm*tp*(rm+sm+tp-5.0);
        funct[8] =0.25*rp*ssm*tm;
        funct[9] =0.25*rrm*sp*tm;
        funct[10]=0.25*rm*ssm*tm;
        funct[11]=0.25*rrm*sm*tm;
        funct[12]=0.25*rp*ssm*tp;
        funct[13]=0.25*rrm*sp*tp;
        funct[14]=0.25*rm*ssm*tp;
        funct[15]=0.25*rrm*sm*tp;
        funct[16]=0.25*rp*sm*ttm;
        funct[17]=0.25*rp*sp*ttm;
        funct[18]=0.25*rm*sp*ttm;
        funct[19]=0.25*rm*sm*ttm;  /* analytisch gecheckt und fuer OK erklaert!!! */
        break;
    }
    case DRT::Element::hex27:
    {
        const double rm1=0.5*r*(r - 1.0);
        const double r00=(1.0 - r*r);
        const double rp1=0.5*r*(r + 1.0);
        const double sm1=0.5*s*(s - 1.0);
        const double s00=(1.0 - s*s);
        const double sp1=0.5*s*(s + 1.0);
        const double tm1=0.5*t*(t - 1.0);
        const double t00=(1.0 - t*t);
        const double tp1=0.5*t*(t + 1.0);

        funct[0] = rp1*sp1*tp1;
        funct[1] = sm1*rp1*tp1;
        funct[2] = rm1*sm1*tp1;
        funct[3] = rm1*sp1*tp1;
        funct[4] = tm1*rp1*sp1;
        funct[5] = sm1*tm1*rp1;
        funct[6] = rm1*sm1*tm1;
        funct[7] = rm1*tm1*sp1;
        funct[8] = s00*rp1*tp1;
        funct[9] = r00*sm1*tp1;
        funct[10] = s00*rm1*tp1;
        funct[11] = r00*sp1*tp1;
        funct[12] = t00*rp1*sp1;
        funct[13] = t00*sm1*rp1;
        funct[14] = t00*rm1*sm1;
        funct[15] = t00*rm1*sp1;
        funct[16] = s00*tm1*rp1;
        funct[17] = r00*sm1*tm1;
        funct[18] = s00*rm1*tm1;
        funct[19] = r00*tm1*sp1;
        funct[20] = r00*s00*tp1;
        funct[21] = s00*t00*rp1;
        funct[22] = r00*t00*sm1;
        funct[23] = s00*t00*rm1;
        funct[24] = r00*t00*sp1;
        funct[25] = r00*s00*tm1;
        funct[26] = r00*s00*t00;
        break;
    }
    case DRT::Element::tet4:
    {
        const double t1=1.0-r-s-t;
        const double t2=r;
        const double t3=s;
        const double t4=t;

        funct[0]= t1;
        funct[1]= t2;
        funct[2]= t3;
        funct[3]= t4;
        break;
    }
    case DRT::Element::tet10:
    {
        const double u=1.0-r-s-t;

        funct[0] =u*(2*u -1);
        funct[1] =r*(2*r -1);
        funct[2] =s*(2*s -1);
        funct[3] =t*(2*t -1);
        funct[4] =4*r*u;
        funct[5] =4*r*s;
        funct[6] =4*s*u;
        funct[7] =4*t*u;
        funct[8] =4*r*t;
        funct[9] =4*s*t;
        break;
    }
    case DRT::Element::weg6:
    {
      const double t3=1.0-r-s;

      funct[0]=Q12*r*(1-t);
      funct[1]=Q12*s*(1-t);
      funct[2]=Q12*t3*(1-t);
      funct[3]=Q12*r*(1+t);
      funct[4]=Q12*s*(1+t);
      funct[5]=Q12*t3*(1+t);
      break;
    }
    case DRT::Element::weg15:
    {
      dserror("shape functions for weg15 have not been checked\n");

      const double t1=r;
      const double t2=s;
      const double t3=1.0-r-s;
      const double p1=Q12*t*(t-1);
      const double p2=1.0-t*t;
      const double p3=Q12*t*(t+1);

      funct[0]=t1*(2*t1-1)*p1;
      funct[1]=t2*(2*t2-1)*p1;
      funct[2]=t3*(2*t3-1)*p1;
      funct[3]=t1*(2*t1-1)*p3;
      funct[4]=t2*(2*t2-1)*p3;
      funct[5]=t3*(2*t3-1)*p3;
      funct[6]=4*t1*t2*p1;
      funct[7]=4*t2*t3*p1;
      funct[8]=4*t1*t3*p1;
      funct[9]=4*t1*t2*p3;
      funct[10]=4*t2*t3*p3;
      funct[11]=4*t1*t3*p3;
      funct[12]=t1*(2*t1-1)*p2;
      funct[13]=t2*(2*t2-1)*p2;
      funct[14]=t3*(2*t3-1)*p2;
      break;
    }

    default:
        dserror("distyp unknown\n");
    } /* end switch(distype) */

    return;
}

//
// first natural derivative of shape functions
//
void DRT::Utils::shape_function_3D_deriv1(
                     Epetra_SerialDenseMatrix&                  deriv1,
                     const double&                              r,
                     const double&                              s,
                     const double&                              t,
                     const DRT::Element::DiscretizationType&    distype)
{
    const double Q18 = 1.0/8.0;
    const double Q12 = 1.0/2.0;

    switch (distype)
    {
    case DRT::Element::hex8:
    {
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double tp=1.0+t;
        const double tm=1.0-t;

        deriv1(0,0)= Q18*sm*tm;
        deriv1(0,1)= Q18*sp*tm;
        deriv1(0,2)=-Q18*sp*tm;
        deriv1(0,3)=-Q18*sm*tm;
        deriv1(0,4)= Q18*sm*tp;
        deriv1(0,5)= Q18*sp*tp;
        deriv1(0,6)=-Q18*sp*tp;
        deriv1(0,7)=-Q18*sm*tp;

        deriv1(1,0)=-Q18*tm*rp;
        deriv1(1,1)= Q18*tm*rp;
        deriv1(1,2)= Q18*tm*rm;
        deriv1(1,3)=-Q18*tm*rm;
        deriv1(1,4)=-Q18*tp*rp;
        deriv1(1,5)= Q18*tp*rp;
        deriv1(1,6)= Q18*tp*rm;
        deriv1(1,7)=-Q18*tp*rm;

        deriv1(2,0)=-Q18*rp*sm;
        deriv1(2,1)=-Q18*rp*sp;
        deriv1(2,2)=-Q18*rm*sp;
        deriv1(2,3)=-Q18*rm*sm;
        deriv1(2,4)= Q18*rp*sm;
        deriv1(2,5)= Q18*rp*sp;
        deriv1(2,6)= Q18*rm*sp;
        deriv1(2,7)= Q18*rm*sm;
        break;
    }

    case DRT::Element::hex20:
    {
        dserror("shape functions for hex20 are not correct!!! Compare with f3f_calfunctderiv1.f \n");

/*--------------------------------------------------- form basic values */
//   rp=1.0+r;
//   rm=1.0-r;
//   sp=1.0+s;
//   sm=1.0-s;
//   tp=1.0+t;
//   tm=1.0-t;
//   rrm=1.0-r*r;
//   ssm=1.0-s*s;
//   ttm=1.0-t*t;
//
//
//      deriv1(0,0) = Q18*sm*tm*(2.0*rp+sm+tm-5.0);
//      deriv1(1,0) =-Q18*tm*rp*(2.0*sm+tm+rp-5.0);
//      deriv1(2,0) =-Q18*rp*sm*(2.0*tm+rp+sm-5.0);
//
//      deriv1(0,1) = Q18*sp*tm*(2.0*rp+sp+tm-5.0);
//      deriv1(1,1) = Q18*tm*rp*(2.0*sp+tm+rp-5.0);
//      deriv1(2,1) =-Q18*rp*sp*(2.0*tm+rp+sp-5.0);
//
//      deriv1(0,2) =-Q18*sp*tm*(2.0*rm+sp+tm-5.0);
//      deriv1(1,2) = Q18*tm*rm*(2.0*sp+tm+rm-5.0);
//      deriv1(2,2) =-Q18*rm*sp*(2.0*tm+rm+sp-5.0);
//
//      deriv1(0,3) =-Q18*sm*tm*(2.0*rm+sm+tm-5.0);
//      deriv1(1,3) =-Q18*tm*rm*(2.0*sm+tm+rm-5.0);
//      deriv1(2,3) =-Q18*rm*sm*(2.0*tm+rm+sm-5.0);
//
//      deriv1(0,4) = Q18*sm*tp*(2.0*rp+sm+tp-5.0);
//      deriv1(1,4) =-Q18*tp*rp*(2.0*sm+tp+rp-5.0);
//      deriv1(2,4) = Q18*rp*sm*(2.0*tp+rp+sm-5.0);
//
//      deriv1(0,5) = Q18*sp*tp*(2.0*rp+sp+tp-5.0);
//      deriv1(1,5) = Q18*tp*rp*(2.0*sp+tp+rp-5.0);
//      deriv1(2,5) = Q18*rp*sp*(2.0*tp+rp+sp-5.0);
//
//      deriv1(0,6) =-Q18*sp*tp*(2.0*rm+sp+tp-5.0);
//      deriv1(1,6) = Q18*tp*rm*(2.0*sp+tp+rm-5.0);
//      deriv1(2,6) = Q18*rm*sp*(2.0*tp+rm+sp-5.0);
//
//      deriv1(0,7) =-Q18*sm*tp*(2.0*rm+sm+tp-5.0);
//      deriv1(1,7) =-Q18*tp*rm*(2.0*sm+tp+rm-5.0);
//      deriv1(2,7) = Q18*rm*sm*(2.0*tp+rm+sm-5.0);
//
//      deriv1(0,8) = 0.25*ssm*tm;
//      deriv1(1,8) =-0.5*s*tm*rp;
//      deriv1(2,8) =-0.25*ssm*rp;
//
//      deriv1(0,9) =-0.5*r*sp*tm;
//      deriv1(1,9) = 0.25*rrm*tm;
//      deriv1(2,9) =-0.25*rrm*sp;
//
//      deriv1(0,10)=-deriv1(0,8);
//      deriv1(1,10)=-0.5*s*tm*rm;
//      deriv1(2,10)=-0.25*ssm*rm;
//
//      deriv1(0,11)=-0.5*r*sm*tm;
//      deriv1(1,11)=-deriv1(1,9);
//      deriv1(2,11)=-0.25*rrm*sm;
//
//      deriv1(0,12)= 0.25*ssm*tp;
//      deriv1(1,12)=-0.5*s*tp*rp;
//      deriv1(2,12)=-deriv1(2,8);
//
//      deriv1(0,13)=-0.5*r*sp*tp;
//      deriv1(1,13)= 0.25*rrm*tp;
//      deriv1(2,13)=-deriv1(2,8);
//
//      deriv1(0,14)=-deriv1(0,12);
//      deriv1(1,14)=-0.5*s*tp*rm;
//      deriv1(2,14)=-deriv1(2,10);
//
//      deriv1(0,15)=-0.5*r*sm*tp;
//      deriv1(1,15)=-deriv1(1,13);
//      deriv1(2,15)=-deriv1(2,11);
//
//      deriv1(0,16)= 0.25*sm*ttm;
//      deriv1(1,16)=-0.25*ttm*rp;
//      deriv1(2,16)=-0.5*t*rp*sm;
//
//      deriv1(0,17)= 0.25*sp*ttm;
//      deriv1(1,17)=-deriv1(1,16);
//      deriv1(2,17)=-0.5*t*rp*sp;
//
//      deriv1(0,18)=-deriv1(0,17);
//      deriv1(1,18)= 0.25*ttm*rm;
//      deriv1(2,18)=-0.5*t*rm*sp;
//
//      deriv1(0,19)=-deriv1(0,16);
//      deriv1(1,19)=-deriv1(1,18);
//      deriv1(2,19)=-0.5*t*rm*sm;
        break;
    }
    case DRT::Element::hex27:
    {
        const double rm1=0.5*r*(r - 1.0);
        const double r00=(1.0 - r*r);
        const double rp1=0.5*r*(r + 1.0);
        const double sm1=0.5*s*(s - 1.0);
        const double s00=(1.0 - s*s);
        const double sp1=0.5*s*(s + 1.0);
        const double tm1=0.5*t*(t - 1.0);
        const double t00=(1.0 - t*t);
        const double tp1=0.5*t*(t + 1.0);

        const double drm1 = r - 0.5;
        const double dr00 = -2.0 * r;
        const double drp1 = r + 0.5;
        const double dsm1 = s - 0.5;
        const double ds00 = -2.0 * s;
        const double dsp1 = s + 0.5;
        const double dtm1 = t - 0.5;
        const double dt00 = -2.0 * t;
        const double dtp1 = t + 0.5;

        deriv1(0,0) = sp1*tp1*drp1;
        deriv1(0,1) = sm1*tp1*drp1;
        deriv1(0,2) = sm1*tp1*drm1;
        deriv1(0,3) = sp1*tp1*drm1;
        deriv1(0,4) = tm1*sp1*drp1;
        deriv1(0,5) = sm1*tm1*drp1;
        deriv1(0,6) = sm1*tm1*drm1;
        deriv1(0,7) = tm1*sp1*drm1;
        deriv1(0,8) = s00*tp1*drp1;
        deriv1(0,9) = sm1*tp1*dr00;
        deriv1(0,10) = s00*tp1*drm1;
        deriv1(0,11) = sp1*tp1*dr00;
        deriv1(0,12) = t00*sp1*drp1;
        deriv1(0,13) = t00*sm1*drp1;
        deriv1(0,14) = t00*sm1*drm1;
        deriv1(0,15) = t00*sp1*drm1;
        deriv1(0,16) = s00*tm1*drp1;
        deriv1(0,17) = sm1*tm1*dr00;
        deriv1(0,18) = s00*tm1*drm1;
        deriv1(0,19) = tm1*sp1*dr00;
        deriv1(0,20) = s00*tp1*dr00;
        deriv1(0,21) = s00*t00*drp1;
        deriv1(0,22) = t00*sm1*dr00;
        deriv1(0,23) = s00*t00*drm1;
        deriv1(0,24) = t00*sp1*dr00;
        deriv1(0,25) = s00*tm1*dr00;
        deriv1(0,26) = s00*t00*dr00;

        deriv1(1,0) = rp1*tp1*dsp1;
        deriv1(1,1) = rp1*tp1*dsm1;
        deriv1(1,2) = rm1*tp1*dsm1;
        deriv1(1,3) = rm1*tp1*dsp1;
        deriv1(1,4) = tm1*rp1*dsp1;
        deriv1(1,5) = tm1*rp1*dsm1;
        deriv1(1,6) = rm1*tm1*dsm1;
        deriv1(1,7) = rm1*tm1*dsp1;
        deriv1(1,8) = rp1*tp1*ds00;
        deriv1(1,9) = r00*tp1*dsm1;
        deriv1(1,10) = rm1*tp1*ds00;
        deriv1(1,11) = r00*tp1*dsp1;
        deriv1(1,12) = t00*rp1*dsp1;
        deriv1(1,13) = t00*rp1*dsm1;
        deriv1(1,14) = t00*rm1*dsm1;
        deriv1(1,15) = t00*rm1*dsp1;
        deriv1(1,16) = tm1*rp1*ds00;
        deriv1(1,17) = r00*tm1*dsm1;
        deriv1(1,18) = rm1*tm1*ds00;
        deriv1(1,19) = r00*tm1*dsp1;
        deriv1(1,20) = r00*tp1*ds00;
        deriv1(1,21) = t00*rp1*ds00;
        deriv1(1,22) = r00*t00*dsm1;
        deriv1(1,23) = t00*rm1*ds00;
        deriv1(1,24) = r00*t00*dsp1;
        deriv1(1,25) = r00*tm1*ds00;
        deriv1(1,26) = r00*t00*ds00;

        deriv1(2,0) = rp1*sp1*dtp1;
        deriv1(2,1) = sm1*rp1*dtp1;
        deriv1(2,2) = rm1*sm1*dtp1;
        deriv1(2,3) = rm1*sp1*dtp1;
        deriv1(2,4) = rp1*sp1*dtm1;
        deriv1(2,5) = sm1*rp1*dtm1;
        deriv1(2,6) = rm1*sm1*dtm1;
        deriv1(2,7) = rm1*sp1*dtm1;
        deriv1(2,8) = s00*rp1*dtp1;
        deriv1(2,9) = r00*sm1*dtp1;
        deriv1(2,10) = s00*rm1*dtp1;
        deriv1(2,11) = r00*sp1*dtp1;
        deriv1(2,12) = rp1*sp1*dt00;
        deriv1(2,13) = sm1*rp1*dt00;
        deriv1(2,14) = rm1*sm1*dt00;
        deriv1(2,15) = rm1*sp1*dt00;
        deriv1(2,16) = s00*rp1*dtm1;
        deriv1(2,17) = r00*sm1*dtm1;
        deriv1(2,18) = s00*rm1*dtm1;
        deriv1(2,19) = r00*sp1*dtm1;
        deriv1(2,20) = r00*s00*dtp1;
        deriv1(2,21) = s00*rp1*dt00;
        deriv1(2,22) = r00*sm1*dt00;
        deriv1(2,23) = s00*rm1*dt00;
        deriv1(2,24) = r00*sp1*dt00;
        deriv1(2,25) = r00*s00*dtm1;
        deriv1(2,26) = r00*s00*dt00;
        break;
    }
    case DRT::Element::tet4:
    {
        deriv1(0,0)=-1.0;
        deriv1(0,1)= 1.0;
        deriv1(0,2)= 0.0;
        deriv1(0,3)= 0.0;

        deriv1(1,0)=-1.0;
        deriv1(1,1)= 0.0;
        deriv1(1,2)= 1.0;
        deriv1(1,3)= 0.0;

        deriv1(2,0)=-1.0;
        deriv1(2,1)= 0.0;
        deriv1(2,2)= 0.0;
        deriv1(2,3)= 1.0;
        break;
    }
    case DRT::Element::tet10:
    {
        const double u=1.0-r-s-t;

        deriv1(0,0) = -4*u+1;
        deriv1(1,0) = deriv1(0,0);
        deriv1(2,0) = deriv1(0,0);

        deriv1(0,1) = 4*r-1;
        deriv1(1,1) = 0;
        deriv1(2,1) = 0;

        deriv1(0,2) = 0;
        deriv1(1,2) = 4*s-1;
        deriv1(2,2) = 0;

        deriv1(0,3) = 0;
        deriv1(1,3) = 0;
        deriv1(2,3) = 4*t-1;

        deriv1(0,4) = 4*(u-r);
        deriv1(1,4) = -4*r;
        deriv1(2,4) = -4*r;

        deriv1(0,5) = 4*s;
        deriv1(1,5) = 4*r;
        deriv1(2,5) = 0;

        deriv1(0,6) = -4*s;
        deriv1(1,6) = 4*(u-s);
        deriv1(2,6) = -4*s;

        deriv1(0,7) = -4*t;
        deriv1(1,7) = -4*t;
        deriv1(2,7) = 4*(u-t);

        deriv1(0,8) = 4*t;
        deriv1(1,8) = 0;
        deriv1(2,8) = 4*r;

        deriv1(0,9) = 0;
        deriv1(1,9) = 4*t;
        deriv1(2,9) = 4*s;

        break;
    }
    case DRT::Element::weg6:
    {
      const double p1=Q12*(1-t);
      const double p2=Q12*(1+t);
      const double t3=1.0-r-s;

      deriv1(0,0)=p1;
      deriv1(0,1)=0;
      deriv1(0,2)=-p1;
      deriv1(0,3)=p2;
      deriv1(0,4)=0;
      deriv1(0,5)=-p2;

      deriv1(1,0)=0;
      deriv1(1,1)=p1;
      deriv1(1,2)=-p1;
      deriv1(1,3)=0;
      deriv1(1,4)=p2;
      deriv1(1,5)=-p1;

      deriv1(2,0)=-Q12*r;
      deriv1(2,1)=-Q12*s;
      deriv1(2,2)=-Q12*t3;
      deriv1(2,3)=Q12*r;
      deriv1(2,4)=Q12*s;
      deriv1(2,5)=Q12*t3;

      break;
    }
     case DRT::Element::weg15:
    {
      const double t1=r;
      const double t2=s;
      const double t3=1.0-r-s;
      const double p1=Q12*t*(t-1);
      const double p2=1.0-t*t;
      const double p3=Q12*t*(t+1);
      const double pd1=t-1.0;
      const double pd2=-2.0*t;
      const double pd3=t+1.0;

      deriv1(0,0)=(4.0*t1-1)*p1;
      deriv1(0,1)=0;
      deriv1(0,2)=(4.0*t1+4.0*t2-3.0)*p1;
      deriv1(0,3)=(4.0*t1-1)*p3;
      deriv1(0,4)=0;
      deriv1(0,5)=(4.0*t1+4.0*t2-3.0)*p3;
      deriv1(0,6)=4.0*t2*p1;
      deriv1(0,7)=-4.0*t2*p1;
      deriv1(0,8)=(4.0-8.0*t1-4.0*t2)*p1;
      deriv1(0,9)=4.0*t2*p3;
      deriv1(0,10)=-4.0*t2*p3;
      deriv1(0,11)=(4.0-8.0*t1-4.0*t2)*p2;
      deriv1(0,12)=(4.0*t1-1)*p2;
      deriv1(0,13)=0;
      deriv1(0,14)=(4.0*t1+4.0*t2-3.0)*p2;


      deriv1(1,0)=0;
      deriv1(1,1)=(4.0*t2-1)*p1;
      deriv1(1,2)=(4.0*t1+4.0*t2-3.0)*p1;
      deriv1(1,3)=0;
      deriv1(1,4)=(4.0*t2-1)*p3;
      deriv1(1,5)=(4.0*t1+4.0*t2-3.0)*p3;
      deriv1(1,6)=4.0*t1*p1;
      deriv1(1,7)=(4.0-4.0*t1-8.0*t2)*p1;
      deriv1(1,8)=-4.0*t1*p1;
      deriv1(1,9)=4.0*t1*p3;
      deriv1(1,10)=(4.0-4.0*t1-8.0*t2)*p3;
      deriv1(1,11)=-4.0*t1*p3;
      deriv1(1,12)=0;
      deriv1(1,13)=(4.0*t2-1)*p2;
      deriv1(1,14)=(4.0*t1+4.0*t2-3)*p2;


      deriv1(2,0)=t1*(2*t1-1)*pd1;
      deriv1(2,1)=t2*(2*t2-1)*pd1;
      deriv1(2,2)=t3*(2*t3-1)*pd1;
      deriv1(2,3)=t1*(2*t1-1)*pd3;
      deriv1(2,4)=t2*(2*t2-1)*pd3;
      deriv1(2,5)=t3*(2*t3-1)*pd3;
      deriv1(2,6)=4*t1*t2*pd1;
      deriv1(2,7)=4*t2*t3*pd1;
      deriv1(2,8)=4*t1*t3*pd1;
      deriv1(2,9)=4*t1*t2*pd3;
      deriv1(2,10)=4*t2*t3*pd3;
      deriv1(2,11)=4*t1*t3*pd3;
      deriv1(2,12)=t1*(2*t1-1)*pd2;
      deriv1(2,13)=t2*(2*t2-1)*pd2;
      deriv1(2,14)=t3*(2*t3-1)*pd2;

    }
    default:
        dserror("distyp unknown\n");
    } /* end switch(distype) */

    return;
}

//
// Second natural derivative of shape functions
//
void DRT::Utils::shape_function_3D_deriv2(
                     Epetra_SerialDenseMatrix&                  deriv2,
                     const double&                              r,
                     const double&                              s,
                     const double&                              t,
                     const DRT::Element::DiscretizationType&    distype)
{
    const double Q18 = 1.0/8.0;
    const double Q12 = 1.0/2.0;

    const int drdr = 0;
    const int dsds = 1;
    const int dtdt = 2;
    const int drds = 3;
    const int drdt = 4;
    const int dsdt = 5;

    switch (distype)
    {
    case DRT::Element::hex8:
    {

        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double tp=1.0+t;
        const double tm=1.0-t;

      deriv2(drdr,0) =  0.0;
      deriv2(dsds,0) =  0.0;
      deriv2(dtdt,0) =  0.0;
      deriv2(drds,0) = -Q18*tm;
      deriv2(drdt,0) = -Q18*sm;
      deriv2(dsdt,0) =  Q18*rp;

      deriv2(drdr,1) =  0.0;
      deriv2(dsds,1) =  0.0;
      deriv2(dtdt,1) =  0.0;
      deriv2(drds,1) = -deriv2(drds,0);
      deriv2(drdt,1) = -Q18*sp;
      deriv2(dsdt,1) = -deriv2(dsdt,0);

      deriv2(drdr,2) =  0.0;
      deriv2(dsds,2) =  0.0;
      deriv2(dtdt,2) =  0.0;
      deriv2(drds,2) =  deriv2(drds,0);
      deriv2(drdt,2) = -deriv2(drdt,1);
      deriv2(dsdt,2) = -Q18*rm;

      deriv2(drdr,3) =  0.0;
      deriv2(dsds,3) =  0.0;
      deriv2(dtdt,3) =  0.0;
      deriv2(drds,3) = -deriv2(drds,0);
      deriv2(drdt,3) = -deriv2(drdt,0);
      deriv2(dsdt,3) = -deriv2(dsdt,2);

      deriv2(drdr,4) =  0.0;
      deriv2(dsds,4) =  0.0;
      deriv2(dtdt,4) =  0.0;
      deriv2(drds,4) = -Q18*tp;
      deriv2(drdt,4) = -deriv2(drdt,0);
      deriv2(dsdt,4) = -deriv2(dsdt,0);

      deriv2(drdr,5) =  0.0;
      deriv2(dsds,5) =  0.0;
      deriv2(dtdt,5) =  0.0;
      deriv2(drds,5) = -deriv2(drds,4);
      deriv2(drdt,5) = -deriv2(drdt,1);
      deriv2(dsdt,5) =  deriv2(dsdt,0);

      deriv2(drdr,6) =  0.0;
      deriv2(dsds,6) =  0.0;
      deriv2(dtdt,6) =  0.0;
      deriv2(drds,6) =  deriv2(drds,4);
      deriv2(drdt,6) =  deriv2(drdt,1);
      deriv2(dsdt,6) = -deriv2(dsdt,2);

      deriv2(drdr,7) =  0.0;
      deriv2(dsds,7) =  0.0;
      deriv2(dtdt,7) =  0.0;
      deriv2(drds,7) = -deriv2(drds,4);
      deriv2(drdt,7) =  deriv2(drdt,0);
      deriv2(dsdt,7) =  deriv2(dsdt,2);

    break;
    }
    case DRT::Element::hex20:
    {
        dserror("shape functions for hex20 are not correct!!! Compare with f3f_calfunctderiv1.f \n");

//   rp=1.0+r;
//   rm=1.0-r;
//   sp=1.0+s;
//   sm=1.0-s;
//   tp=1.0+t;
//   tm=1.0-t;
//   rrm=1.0-r*r;
//   ssm=1.0-s*s;
//   ttm=1.0-t*t;

//      deriv2(drdr,0) = 0.25*sm*tm;
//      deriv2(dsds,0) = 0.25*tm*rp;
//      deriv2(dtdt,0) = 0.25*rp*sm;
//      deriv2(drds,0) =-Q18*(tm*(2*rp+sm+tm-5.0+sm*tm));
//      deriv2(drdt,0) =-Q18*(sm*(2*rp+sm+tm-5.0+sm*tm));
//      deriv2(dsdt,0) = Q18*(rp*(2*sm+tm+rp-5.0+tm*rp));
//
//      deriv2(drdr,1) = 0.25*sp*tm;
//      deriv2(dsds,1) = deriv2(dtdt,1);
//      deriv2(dtdt,1) = 0.25*rp*sp;
//      deriv2(drds,1) =-Q18*(tm*(2*rp+sp+tm-5.0+sp*tm));
//      deriv2(drdt,1) =-Q18*(sp*(2*rp+sp+tm-5.0+sp*tm));
//      deriv2(dsdt,1) =-Q18*(rp*(2*sp+tm+rp-5.0+tm*rp));
//
//      deriv2(drdr,2) =-deriv2(dsds,2);
//      deriv2(dsds,2) = 0.25*tm*rm;
//      deriv2(dtdt,2) = 0.25*rm*sp;
//      deriv2(drds,2) =-Q18*(tm*(2*rm+sp+tm-5.0+sp*tm));
//      deriv2(drdt,2) = Q18*(sp*(2*rm+sp+tm-5.0+sp*tm));
//      deriv2(dsdt,2) =-Q18*(rm*(2*sp+tm+rm-5.0+tm*rm));
//
//      deriv2(drdr,3) =-deriv2(dsds,1);
//      deriv2(dsds,3) = deriv2(dtdt,3);
//      deriv2(dtdt,3) = 0.25*rm*sm;
//      deriv2(drds,3) =-Q18*(tm*(2*rm+sm+tm-5.0+sm*tm));
//      deriv2(drdt,3) = Q18*(sm*(2*rm+sm+tm-5.0+sm*tm));
//      deriv2(dsdt,3) = Q18*(rm*(2*sm+tm+rm-5.0+tm*rm));
//
//      deriv2(drdr,4) = 0.25*sm*tp;
//      deriv2(dsds,4) = 0.25*tp*rp;
//      deriv2(dtdt,4) = deriv2(drds,1);
//      deriv2(drds,4) =-Q18*(tp*(2*rp+sm+tp-5.0+sm*tp));
//      deriv2(drdt,4) = Q18*(sm*(2*rp+sm+tp-5.0+sm*tp));
//      deriv2(dsdt,4) =-Q18*(rp*(2*sm+tp+rp-5.0+tp*rp));
//
//      deriv2(drdr,5) = 0.25*sp*tp;
//      deriv2(dsds,5) = deriv2(dtdt,5);
//      deriv2(dtdt,5) = deriv2(drds,2);
//      deriv2(drds,5) =-Q18*(tp*(2*rp+sp+tp-5.0+sp*tp));
//      deriv2(drdt,5) = Q18*(sp*(2*rp+sp+tp-5.0+sp*tp));
//      deriv2(dsdt,5) = Q18*(rp*(2*sp+tp+rp-5.0+tp*rp));
//
//      deriv2(drdr,6) =-deriv2(dsds,6);
//      deriv2(dsds,6) = 0.25*tp*rm;
//      deriv2(dtdt,6) = deriv2(drds,3);
//      deriv2(drds,6) =-Q18*(tp*(2*rm+sp+tp-5.0+sp*tp));
//      deriv2(drdt,6) =-Q18*(sp*(2*rm+sp+tp-5.0+sp*tp));
//      deriv2(dsdt,6) = Q18*(rm*(2*sp+tp+rm-5.0+tp*rm));
//
//      deriv2(drdr,7) =-deriv2(dsds,5);
//      deriv2(dsds,7) = deriv2(dtdt,7);
//      deriv2(dtdt,7) = deriv2(drds,4);
//      deriv2(drds,7) =-Q18*(tp*(2*rm+sm+tp-5.0+sm*tp));
//      deriv2(drdt,7) =-Q18*(sm*(2*rm+sm+tp-5.0+sm*tp));
//      deriv2(dsdt,7) =-Q18*(rm*(2*sm+tp+rm-5.0+tp*rm));
//
//      deriv2(drdr,8) = 0.0;
//      deriv2(dsds,8) = -0.5*tm*rp;
//      deriv2(dtdt,8) = 0.0;
//      deriv2(drds,8) =-0.5*s*tm;
//      deriv2(drdt,8) =-0.25*ssm;
//      deriv2(dsdt,8) = 0.5*s*rp;
//
//      deriv2(drdr,9)=-0.5*sp*tm;
//      deriv2(dsds,9)= 0.0;
//      deriv2(dtdt,9)= 0.0;
//      deriv2(drds,9)=-0.5*r*tm;
//      deriv2(drdt,9)= 0.5*r*sp;
//      deriv2(dsdt,9)=-0.25*rrm ;
//
//      deriv2(drdr,10)= 0.0;
//      deriv2(dsds,10)= -0.5*tm*rm;
//      deriv2(dtdt,10)= 0.0;
//      deriv2(drds,10)= 0.5*s*tm;
//      deriv2(drdt,10)=-deriv2(drdt,8);
//      deriv2(dsdt,10)= 0.5*s*rm;
//
//      deriv2(drdr,11)=-0.5*sm*tm;
//      deriv2(dsds,11)= 0.0;
//      deriv2(dtdt,11)= 0.0;
//      deriv2(drds,11)= 0.5*r*tm;
//      deriv2(drdt,11)= 0.5*r*sm;
//      deriv2(dsdt,11)=-deriv2(dsdt,9);
//
//      deriv2(drdr,12)= 0.0;
//      deriv2(dsds,12)= -0.5*tp*rp;
//      deriv2(dtdt,12)= 0.0;
//      deriv2(drds,12)=-0.5*s*tp;
//      deriv2(drdt,12)=-deriv2(drdt,8);
//      deriv2(dsdt,12)=-deriv2(dsdt,8);
//
//      deriv2(drdr,13)=-0.5*sp*tp;
//      deriv2(dsds,13)= 0.0;
//      deriv2(dtdt,13)= 0.0;
//      deriv2(drds,13)=-0.5*r*tp;
//      deriv2(drdt,13)=-deriv2(drdt,9);
//      deriv2(dsdt,13)=-deriv2(dsdt,9);
//
//      deriv2(drdr,14)= 0.0;
//      deriv2(dsds,14)= -0.5*tp*rm;
//      deriv2(dtdt,14)= 0.0;
//      deriv2(drds,14)= 0.5*s*tp;
//      deriv2(drdt,14)= deriv2(drdt,8);
//      deriv2(dsdt,14)=-deriv2(dsdt,10);
//
//      deriv2(drdr,15)=-0.5*sm*tp;
//      deriv2(dsds,15)= 0.0;
//      deriv2(dtdt,15)= 0.0;
//      deriv2(drds,15)= 0.5*r*tp;
//      deriv2(drdt,15)=-deriv2(drdt,11);
//      deriv2(dsdt,15)= deriv2(dsdt,9);
//
//      deriv2(drdr,16)= 0.0;
//      deriv2(dsds,16)= 0.0;
//      deriv2(dtdt,16)= 0.0;
//      deriv2(drds,16)=-0.25*ttm;
//      deriv2(drdt,16)=-0.5*t*sm;
//      deriv2(dsdt,16)= 0.5*t*rp;
//
//      deriv2(drdr,17)= 0.0;
//      deriv2(dsds,17)= 0.0;
//      deriv2(dtdt,17)= 0.0;
//      deriv2(drds,17)= 0.25*ttm;
//      deriv2(drdt,17)=-0.5*t*sp;
//      deriv2(dsdt,17)=-deriv2(dsdt,16);
//
//      deriv2(drdr,18)= 0.0;
//      deriv2(dsds,18)= 0.0;
//      deriv2(dtdt,18)= 0.0;
//      deriv2(drds,18)= deriv2(drds,16);
//      deriv2(drdt,18)= 0.5*t*sp;
//      deriv2(dsdt,18)= 0.5*t*rm;
//
//      deriv2(drdr,19)= 0.0;
//      deriv2(dsds,19)= 0.0;
//      deriv2(dtdt,19)= 0.0;
//      deriv2(drds,19)= deriv2(drds,17);
//      deriv2(drdt,19)= 0.5*t*sm;
//      deriv2(dsdt,19)=-deriv2(dsdt,18);

        break;
    }
    case DRT::Element::hex27:
    {
        const double rm1=0.5*r*(r - 1.0);
        const double r00=(1.0 - r*r);
        const double rp1=0.5*r*(r + 1.0);
        const double sm1=0.5*s*(s - 1.0);
        const double s00=(1.0 - s*s);
        const double sp1=0.5*s*(s + 1.0);
        const double tm1=0.5*t*(t - 1.0);
        const double t00=(1.0 - t*t);
        const double tp1=0.5*t*(t + 1.0);

        const double drm1 = r - 0.5;
        const double dr00 = -2.0 * r;
        const double drp1 = r + 0.5;
        const double dsm1 = s - 0.5;
        const double ds00 = -2.0 * s;
        const double dsp1 = s + 0.5;
        const double dtm1 = t - 0.5;
        const double dt00 = -2.0 * t;
        const double dtp1 = t + 0.5;


        deriv2(drdr,0) = sp1*tp1;
        deriv2(drdr,1) = sm1*tp1;
        deriv2(drdr,2) = sm1*tp1;
        deriv2(drdr,3) = sp1*tp1;
        deriv2(drdr,4) = tm1*sp1;
        deriv2(drdr,5) = sm1*tm1;
        deriv2(drdr,6) = sm1*tm1;
        deriv2(drdr,7) = tm1*sp1;
        deriv2(drdr,8) = s00*tp1;
        deriv2(drdr,9) = -2*sm1*tp1;
        deriv2(drdr,10) = s00*tp1;
        deriv2(drdr,11) = -2*sp1*tp1;
        deriv2(drdr,12) = t00*sp1;
        deriv2(drdr,13) = t00*sm1;
        deriv2(drdr,14) = t00*sm1;
        deriv2(drdr,15) = t00*sp1;
        deriv2(drdr,16) = s00*tm1;
        deriv2(drdr,17) = -2*sm1*tm1;
        deriv2(drdr,18) = s00*tm1;
        deriv2(drdr,19) = -2*tm1*sp1;
        deriv2(drdr,20) = -2*s00*tp1;
        deriv2(drdr,21) = s00*t00;
        deriv2(drdr,22) = -2*t00*sm1;
        deriv2(drdr,23) = s00*t00;
        deriv2(drdr,24) = -2*t00*sp1;
        deriv2(drdr,25) = -2*s00*tm1;
        deriv2(drdr,26) = -2*s00*t00;

        deriv2(dsds,0) = rp1*tp1;
        deriv2(dsds,1) = rp1*tp1;
        deriv2(dsds,2) = rm1*tp1;
        deriv2(dsds,3) = rm1*tp1;
        deriv2(dsds,4) = tm1*rp1;
        deriv2(dsds,5) = tm1*rp1;
        deriv2(dsds,6) = rm1*tm1;
        deriv2(dsds,7) = rm1*tm1;
        deriv2(dsds,8) = -2*rp1*tp1;
        deriv2(dsds,9) = r00*tp1;
        deriv2(dsds,10) = -2*rm1*tp1;
        deriv2(dsds,11) = r00*tp1;
        deriv2(dsds,12) = t00*rp1;
        deriv2(dsds,13) = t00*rp1;
        deriv2(dsds,14) = t00*rm1;
        deriv2(dsds,15) = t00*rm1;
        deriv2(dsds,16) = -2*tm1*rp1;
        deriv2(dsds,17) = r00*tm1;
        deriv2(dsds,18) = -2*rm1*tm1;
        deriv2(dsds,19) = r00*tm1;
        deriv2(dsds,20) = -2*r00*tp1;
        deriv2(dsds,21) = -2*t00*rp1;
        deriv2(dsds,22) = r00*t00;
        deriv2(dsds,23) = -2*t00*rm1;
        deriv2(dsds,24) = r00*t00;
        deriv2(dsds,25) = -2*r00*tm1;
        deriv2(dsds,26) = -2*r00*t00;

        deriv2(dtdt,0) = rp1*sp1;
        deriv2(dtdt,1) = sm1*rp1;
        deriv2(dtdt,2) = rm1*sm1;
        deriv2(dtdt,3) = rm1*sp1;
        deriv2(dtdt,4) = rp1*sp1;
        deriv2(dtdt,5) = sm1*rp1;
        deriv2(dtdt,6) = rm1*sm1;
        deriv2(dtdt,7) = rm1*sp1;
        deriv2(dtdt,8) = s00*rp1;
        deriv2(dtdt,9) = r00*sm1;
        deriv2(dtdt,10) = s00*rm1;
        deriv2(dtdt,11) = r00*sp1;
        deriv2(dtdt,12) = -2*rp1*sp1;
        deriv2(dtdt,13) = -2*sm1*rp1;
        deriv2(dtdt,14) = -2*rm1*sm1;
        deriv2(dtdt,15) = -2*rm1*sp1;
        deriv2(dtdt,16) = s00*rp1;
        deriv2(dtdt,17) = r00*sm1;
        deriv2(dtdt,18) = s00*rm1;
        deriv2(dtdt,19) = r00*sp1;
        deriv2(dtdt,20) = r00*s00;
        deriv2(dtdt,21) = -2*s00*rp1;
        deriv2(dtdt,22) = -2*r00*sm1;
        deriv2(dtdt,23) = -2*s00*rm1;
        deriv2(dtdt,24) = -2*r00*sp1;
        deriv2(dtdt,25) = r00*s00;
        deriv2(dtdt,26) = -2*r00*s00;

        deriv2(drds,0) = tp1*drp1*dsp1;
        deriv2(drds,1) = tp1*dsm1*drp1;
        deriv2(drds,2) = tp1*drm1*dsm1;
        deriv2(drds,3) = tp1*drm1*dsp1;
        deriv2(drds,4) = tm1*drp1*dsp1;
        deriv2(drds,5) = tm1*dsm1*drp1;
        deriv2(drds,6) = tm1*drm1*dsm1;
        deriv2(drds,7) = tm1*drm1*dsp1;
        deriv2(drds,8) = tp1*ds00*drp1;
        deriv2(drds,9) = tp1*dr00*dsm1;
        deriv2(drds,10) = tp1*ds00*drm1;
        deriv2(drds,11) = tp1*dr00*dsp1;
        deriv2(drds,12) = t00*drp1*dsp1;
        deriv2(drds,13) = t00*dsm1*drp1;
        deriv2(drds,14) = t00*drm1*dsm1;
        deriv2(drds,15) = t00*drm1*dsp1;
        deriv2(drds,16) = tm1*ds00*drp1;
        deriv2(drds,17) = tm1*dr00*dsm1;
        deriv2(drds,18) = tm1*ds00*drm1;
        deriv2(drds,19) = tm1*dr00*dsp1;
        deriv2(drds,20) = 4*r*s*tp1;
        deriv2(drds,21) = t00*ds00*drp1;
        deriv2(drds,22) = t00*dr00*dsm1;
        deriv2(drds,23) = t00*ds00*drm1;
        deriv2(drds,24) = t00*dr00*dsp1;
        deriv2(drds,25) = 4*r*s*tm1;
        deriv2(drds,26) = 4*r*s*t00;

        deriv2(drdt,0) = sp1*drp1*dtp1;
        deriv2(drdt,1) = sm1*drp1*dtp1;
        deriv2(drdt,2) = sm1*drm1*dtp1;
        deriv2(drdt,3) = sp1*drm1*dtp1;
        deriv2(drdt,4) = sp1*dtm1*drp1;
        deriv2(drdt,5) = sm1*dtm1*drp1;
        deriv2(drdt,6) = sm1*drm1*dtm1;
        deriv2(drdt,7) = sp1*drm1*dtm1;
        deriv2(drdt,8) = s00*drp1*dtp1;
        deriv2(drdt,9) = sm1*dr00*dtp1;
        deriv2(drdt,10) = s00*drm1*dtp1;
        deriv2(drdt,11) = sp1*dr00*dtp1;
        deriv2(drdt,12) = sp1*dt00*drp1;
        deriv2(drdt,13) = sm1*dt00*drp1;
        deriv2(drdt,14) = sm1*dt00*drm1;
        deriv2(drdt,15) = sp1*dt00*drm1;
        deriv2(drdt,16) = s00*dtm1*drp1;
        deriv2(drdt,17) = sm1*dr00*dtm1;
        deriv2(drdt,18) = s00*drm1*dtm1;
        deriv2(drdt,19) = sp1*dr00*dtm1;
        deriv2(drdt,20) = s00*dr00*dtp1;
        deriv2(drdt,21) = s00*dt00*drp1;
        deriv2(drdt,22) = 4*r*t*sm1;
        deriv2(drdt,23) = s00*dt00*drm1;
        deriv2(drdt,24) = 4*r*t*sp1;
        deriv2(drdt,25) = s00*dr00*dtm1;
        deriv2(drdt,26) = 4*r*t*s00;

        deriv2(dsdt,0) = rp1*dsp1*dtp1;
        deriv2(dsdt,1) = rp1*dsm1*dtp1;
        deriv2(dsdt,2) = rm1*dsm1*dtp1;
        deriv2(dsdt,3) = rm1*dsp1*dtp1;
        deriv2(dsdt,4) = rp1*dtm1*dsp1;
        deriv2(dsdt,5) = rp1*dsm1*dtm1;
        deriv2(dsdt,6) = rm1*dsm1*dtm1;
        deriv2(dsdt,7) = rm1*dtm1*dsp1;
        deriv2(dsdt,8) = rp1*ds00*dtp1;
        deriv2(dsdt,9) = r00*dsm1*dtp1;
        deriv2(dsdt,10) = rm1*ds00*dtp1;
        deriv2(dsdt,11) = r00*dsp1*dtp1;
        deriv2(dsdt,12) = rp1*dt00*dsp1;
        deriv2(dsdt,13) = rp1*dt00*dsm1;
        deriv2(dsdt,14) = rm1*dt00*dsm1;
        deriv2(dsdt,15) = rm1*dt00*dsp1;
        deriv2(dsdt,16) = rp1*ds00*dtm1;
        deriv2(dsdt,17) = r00*dsm1*dtm1;
        deriv2(dsdt,18) = rm1*ds00*dtm1;
        deriv2(dsdt,19) = r00*dtm1*dsp1;
        deriv2(dsdt,20) = r00*ds00*dtp1;
        deriv2(dsdt,21) = 4*s*t*rp1;
        deriv2(dsdt,22) = r00*dt00*dsm1;
        deriv2(dsdt,23) = 4*s*t*rm1;
        deriv2(dsdt,24) = r00*dt00*dsp1;
        deriv2(dsdt,25) = r00*ds00*dtm1;
        deriv2(dsdt,26) = 4*s*t*r00;
        break;
    }
    case DRT::Element::tet10:
    {
        deriv2(drdr,0) =  4.0;
        deriv2(dsds,0) =  4.0;
        deriv2(dtdt,0) =  4.0;
        deriv2(drds,0) =  4.0;
        deriv2(drdt,0) =  4.0;
        deriv2(dsdt,0) =  4.0;

        deriv2(drdr,1) =  4.0;
        deriv2(dsds,1) =  0.0;
        deriv2(dtdt,1) =  0.0;
        deriv2(drds,1) =  0.0;
        deriv2(drdt,1) =  0.0;
        deriv2(dsdt,1) =  0.0;

        deriv2(drdr,2) =  0.0;
        deriv2(dsds,2) =  4.0;
        deriv2(dtdt,2) =  0.0;
        deriv2(drds,2) =  0.0;
        deriv2(drdt,2) =  0.0;
        deriv2(dsdt,2) =  0.0;

        deriv2(drdr,3) =  0.0;
        deriv2(dsds,3) =  0.0;
        deriv2(dtdt,3) =  4.0;
        deriv2(drds,3) =  0.0;
        deriv2(drdt,3) =  0.0;
        deriv2(dsdt,3) =  0.0;

        deriv2(drdr,4) = -8.0;
        deriv2(dsds,4) =  0.0;
        deriv2(dtdt,4) =  0.0;
        deriv2(drds,4) = -4.0;
        deriv2(drdt,4) = -4.0;
        deriv2(dsdt,4) =  0.0;

        deriv2(drdr,5) =  0.0;
        deriv2(dsds,5) =  0.0;
        deriv2(dtdt,5) =  0.0;
        deriv2(drds,5) =  4.0;
        deriv2(drdt,5) =  0.0;
        deriv2(dsdt,5) =  0.0;

        deriv2(drdr,6) =  0.0;
        deriv2(dsds,6) = -8.0;
        deriv2(dtdt,6) =  0.0;
        deriv2(drds,6) = -4.0;
        deriv2(drdt,6) =  0.0;
        deriv2(dsdt,6) = -4.0;

        deriv2(drdr,7) =  0.0;
        deriv2(dsds,7) =  0.0;
        deriv2(dtdt,7) = -8.0;
        deriv2(drds,7) =  0.0;
        deriv2(drdt,7) = -4.0;
        deriv2(dsdt,7) = -4.0;

        deriv2(drdr,8) =  0.0;
        deriv2(dsds,8) =  0.0;
        deriv2(dtdt,8) =  0.0;
        deriv2(drds,8) =  0.0;
        deriv2(drdt,8) =  4.0;
        deriv2(dsdt,8) =  0.0;

        deriv2(drdr,9) =  0.0;
        deriv2(dsds,9) =  0.0;
        deriv2(dtdt,9) =  0.0;
        deriv2(drds,9) =  0.0;
        deriv2(drdt,9) =  0.0;
        deriv2(dsdt,9) =  4.0;

        break;
    }
    case DRT::Element::weg15:
    {

      const double t1=r;
      const double t2=s;
      const double t3=1.0-r-s;
      const double p1=Q12*t*(t-1);
      const double p2=1.0-t*t;
      const double p3=Q12*t*(t+1);
      const double pd1=2.0*t-1;
      const double pd2=-2.0*t;
      const double pd3=2.0*t+1;
      const double pdd1=2.0;
      const double pdd2=-2.0;
      const double pdd3=2.0;

      deriv2(0,0)=4.0*p1;
      deriv2(0,1)=0;
      deriv2(0,2)=4.0*p1;
      deriv2(0,3)=4.0*p3;
      deriv2(0,4)=0;
      deriv2(0,5)=4.0*p3;
      deriv2(0,6)=0;
      deriv2(0,7)=0;
      deriv2(0,8)=-8.0*p1;
      deriv2(0,9)=0;
      deriv2(0,10)=0;
      deriv2(0,11)=-8.0*p1;
      deriv2(0,12)=4.0*p2;
      deriv2(0,13)=0;
      deriv2(0,14)=4.0*p2;

      deriv2(1,0)=0;
      deriv2(1,1)=4.0*p1;
      deriv2(1,2)=4.0*p1;
      deriv2(1,3)=0;
      deriv2(1,4)=4.0*p3;
      deriv2(1,5)=4.0*p3;
      deriv2(1,6)=0;
      deriv2(1,7)=-8.0*p1;
      deriv2(1,8)=0;
      deriv2(1,9)=0;
      deriv2(1,10)=-8.0*p3;
      deriv2(1,11)=0;
      deriv2(1,12)=0;
      deriv2(1,13)=4.0*p2;
      deriv2(1,14)=4.0*p2;

      deriv2(2,0)=t1*(2*t1-1)*pdd1;
      deriv2(2,1)=t2*(2*t2-1)*pdd1;
      deriv2(2,2)=t3*(2*t3-1)*pdd1;
      deriv2(2,3)=t1*(2*t1-1)*pdd3;
      deriv2(2,4)=t2*(2*t2-1)*pdd3;
      deriv2(2,5)=t3*(2*t3-1)*pdd3;
      deriv2(2,6)=4*t1*t2*pdd1;
      deriv2(2,7)=4*t2*t3*pdd1;
      deriv2(2,8)=4*t1*t3*pdd1;
      deriv2(2,9)=4*t1*t2*pdd3;
      deriv2(2,10)=4*t2*t3*pdd3;
      deriv2(2,11)=4*t1*t3*pdd3;
      deriv2(2,12)=t1*(2*t1-1)*pdd2;
      deriv2(2,13)=t2*(2*t2-1)*pdd2;
      deriv2(2,14)=t3*(2*t3-1)*pdd2;

      deriv2(3,0)=0;
      deriv2(3,1)=0;
      deriv2(3,2)=4.0*p1;
      deriv2(3,3)=0;
      deriv2(3,4)=0;
      deriv2(3,5)=4.0*p3;
      deriv2(3,6)=4.0*p1;
      deriv2(3,7)=-4.0*p1;
      deriv2(3,8)=-4.0*p1;
      deriv2(3,9)=4.0*p3;
      deriv2(3,10)=-4.0*p3;
      deriv2(3,11)=-4.0*p3;
      deriv2(3,12)=0;
      deriv2(3,13)=0;
      deriv2(3,14)=4.0*p2;

      deriv2(4,0)=0;
      deriv2(4,1)=(4.0*t2-1)*pd1;
      deriv2(4,2)=(4.0*t1+4.0*t2-3.0)*pd1;
      deriv2(4,3)=0;
      deriv2(4,4)=(4.0*t2-1)*pd3;
      deriv2(4,5)=(4.0*t1+4.0*t2-3.0)*pd3;
      deriv2(4,6)=4.0*t1*pd1;
      deriv2(4,7)=(4.0-4.0*t1-8.0*t2)*pd1;
      deriv2(4,8)=-4.0*t1*pd1;
      deriv2(4,9)=4.0*t1*pd3;
      deriv2(4,10)=(4.0-4.0*t1-8.0*t2)*pd3;
      deriv2(4,11)=-4.0*t1*pd3;
      deriv2(4,12)=0;
      deriv2(4,13)=(4.0*t2-1)*pd2;
      deriv2(4,14)=(4.0*t1+4.0*t2-3)*pd2;

      deriv2(5,0)=(4.0*t1-1)*pd1;
      deriv2(5,1)=0;
      deriv2(5,2)=(4.0*t1+4.0*t2-3.0)*pd1;
      deriv2(5,3)=(4.0*t1-1)*pd3;
      deriv2(5,4)=0;
      deriv2(5,5)=(4.0*t1+4.0*t2-3.0)*pd3;
      deriv2(5,6)=4.0*t2*pd1;
      deriv2(5,7)=-4.0*t2*pd1;
      deriv2(5,8)=(4.0-8.0*t1-4.0*t2)*pd1;
      deriv2(5,9)=4.0*t2*pd3;
      deriv2(5,10)=-4.0*t2*pd3;
      deriv2(5,11)=(4.0-8.0*t1-4.0*t2)*pd2;
      deriv2(5,12)=(4.0*t1-1)*pd2;
      deriv2(5,13)=0;
      deriv2(5,14)=(4.0*t1+4.0*t2-3.0)*pd2;

    break;
    }
    default:
        dserror("distyp unknown\n");
    } /* end switch(distype) */

    return;
}

//
// shape functions 2D
//
void DRT::Utils::shape_function_2D(
                     Epetra_SerialDenseVector&                  funct,
                     const double&                              r,
                     const double&                              s,
                     const DRT::Element::DiscretizationType&    distype)
{
    switch (distype)
    {
    case DRT::Element::quad4:
    {
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;

        funct[0]=0.25*rm*sm;
        funct[1]=0.25*rp*sm;
        funct[2]=0.25*rp*sp;
        funct[3]=0.25*rm*sp;
        break;
    }
    case DRT::Element::quad8:
    {
        dserror("adjust numbering\n");
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double r2=1.0-r*r;
        const double s2=1.0-s*s;

        funct[0]=0.25*rp*sp-0.5*(funct[4]+funct[7]);
        funct[1]=0.25*rm*sp-0.5*(funct[4]+funct[5]);
        funct[2]=0.25*rm*sm-0.5*(funct[5]+funct[6]);
        funct[3]=0.25*rp*sm-0.5*(funct[6]+funct[7]);
        funct[4]=0.5*r2*sp;
        funct[5]=0.5*rm*s2;
        funct[6]=0.5*r2*sm;
        funct[7]=0.5*rp*s2;
        break;
    }
    case DRT::Element::quad9:
    {
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double r2=1.0-r*r;
        const double s2=1.0-s*s;
        const double rh=0.5*r;
        const double sh=0.5*s;
        const double rs=rh*sh;


        funct[0]= rs*rm*sm;
        funct[1]=-rs*rp*sm;
        funct[2]= rs*rp*sp;
        funct[3]=-rs*rm*sp;
        funct[4]=-sh*sm*r2;
        funct[5]= rh*rp*s2;
        funct[6]= sh*sp*r2;
        funct[7]=-rh*rm*s2;
        funct[8]= r2*s2;
        break;
    }
    case DRT::Element::tri3:
    {
        const double t1 = 1.0 - r - s;
        const double t2 = r;
        const double t3 = s;
        funct[0] = t1;
        funct[1] = t2;
        funct[2] = t3;
        break;
    }
    case DRT::Element::tri6:
    {
        const double t1 = 1.0-r-s;
        const double t2 = r;
        const double t3 = s;

        funct[0] = t1*(2.0*t1 - 1.0);
        funct[1] = t2*(2.0*t2 - 1.0);
        funct[2] = t3*(2.0*t3 - 1.0);
        funct[3] = 4.0*t2*t1;
        funct[4] = 4.0*t2*t3;
        funct[5] = 4.0*t3*t1;
        break;
    }
    default:
        dserror("distype unknown\n");
    } /* end switch(distype) */

    return;
}

//
// shape functions and natural deriv1atives
//
void DRT::Utils::shape_function_2D_deriv1(
                     Epetra_SerialDenseMatrix&                  deriv1,
                     const double&                              r,
                     const double&                              s,
                     const DRT::Element::DiscretizationType&    distype)
{
    switch (distype)
    {
    case DRT::Element::quad4:
    {
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;

        deriv1(0,0)=-0.25*sm;
        deriv1(1,0)=-0.25*rm;

        deriv1(0,1)= 0.25*sm;
        deriv1(1,1)=-0.25*rp;

        deriv1(0,2)= 0.25*sp;
        deriv1(1,2)= 0.25*rp;

        deriv1(0,3)=-0.25*sp;
        deriv1(1,3)= 0.25*rm;
        break;
    }
    case DRT::Element::quad8:
    {
        dserror("adjust numbering\n");
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double r2=1.0-r*r;
        const double s2=1.0-s*s;

        deriv1(0,0)= 0.25*sp;
        deriv1(1,0)= 0.25*rp;

        deriv1(0,1)=-0.25*sp;
        deriv1(1,1)= 0.25*rm;

        deriv1(0,2)=-0.25*sm;
        deriv1(1,2)=-0.25*rm;

        deriv1(0,3)= 0.25*sm;
        deriv1(1,3)=-0.25*rp;

        deriv1(0,4)=-1.0*r*sp;
        deriv1(1,4)= 0.5*r2;

        deriv1(0,5)=-0.5*  s2;
        deriv1(1,5)=-1.0*rm*s;

        deriv1(0,6)=-1.0*r*sm;
        deriv1(1,6)=-0.5*r2;

        deriv1(0,7)= 0.5*s2;
        deriv1(1,7)=-1.0*rp*s;

        deriv1(0,0)-= 0.5*(deriv1(0,4)+deriv1(0,7));
        deriv1(1,0)-= 0.5*(deriv1(1,4)+deriv1(1,7));

        for(int i=1;i<4;i++)
        {
            const int ii=i+3;
            deriv1(0,i) -= 0.5*(deriv1(0,ii)+deriv1(0,ii+1));
            deriv1(1,i) -= 0.5*(deriv1(1,ii)+deriv1(1,ii+1));
        }
        break;
    }
    case DRT::Element::quad9:
    {
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double r2=1.0-r*r;
        const double s2=1.0-s*s;
        const double rh=0.5*r;
        const double sh=0.5*s;
        const double rhp=r+0.5;
        const double rhm=r-0.5;
        const double shp=s+0.5;
        const double shm=s-0.5;

        deriv1(0,0)=-rhm*sh*sm;
        deriv1(1,0)=-shm*rh*rm;

        deriv1(0,1)=-rhp*sh*sm;
        deriv1(1,1)= shm*rh*rp;

        deriv1(0,2)= rhp*sh*sp;
        deriv1(1,2)= shp*rh*rp;

        deriv1(0,3)= rhm*sh*sp;
        deriv1(1,3)=-shp*rh*rm;

        deriv1(0,4)= 2.0*r*sh*sm;
        deriv1(1,4)= shm*r2;

        deriv1(0,5)= rhp*s2;
        deriv1(1,5)=-2.0*s*rh*rp;

        deriv1(0,6)=-2.0*r*sh*sp;
        deriv1(1,6)= shp*r2;

        deriv1(0,7)= rhm*s2;
        deriv1(1,7)= 2.0*s*rh*rm;

        deriv1(0,8)=-2.0*r*s2;
        deriv1(1,8)=-2.0*s*r2;
        break;
    }
    case DRT::Element::tri3:
    {
        deriv1(0,0)=-1.0;
        deriv1(1,0)=-1.0;

        deriv1(0,1)= 1.0;
        deriv1(1,1)= 0.0;

        deriv1(0,2)= 0.0;
        deriv1(1,2)= 1.0;
        break;
    }
    case DRT::Element::tri6:
    {
        deriv1(0,0)= -3.0 + 4.0*(r + s);
        deriv1(1,0)= -3.0 + 4.0*(r + s);

        deriv1(0,1)= 4.0*r - 1.0;
        deriv1(1,1)= 0.0;

        deriv1(0,2)= 0.0;
        deriv1(1,2)= 4.0*s - 1.0;

        deriv1(0,3)= 4.0*(1.0 - 2.0*r - s);
        deriv1(1,3)=-4.0*r;

        deriv1(0,4)= 4.0*s;
        deriv1(1,4)= 4.0*r;

        deriv1(0,5)=-4.0*s;
        deriv1(1,5)= 4.0*(1.0 - r - 2.0*s);
        break;
    }
    default:
      dserror("distype unknown\n");
    } /* end switch(distype) */
    return;
}

///
/// shape functions and natural deriv1atives
///
/// The second index indicates the node number
/// the first index indicates the derivative direction
void DRT::Utils::shape_function_2D_deriv2(
                     Epetra_SerialDenseMatrix&                  deriv2,
                     const double&                              r,
                     const double&                              s,
                     const DRT::Element::DiscretizationType&    distype)
{
    const int drdr = 0;
    const int dsds = 1;
    const int drds = 2;

    switch (distype)
    {
    case DRT::Element::quad4:
    {
        deriv2(drdr,0) =  0.0;
        deriv2(dsds,0) =  0.0;
        deriv2(drds,0) =  0.25;

        deriv2(drdr,1) =  0.0;
        deriv2(dsds,1) =  0.0;
        deriv2(drds,1) = -0.25;

        deriv2(drdr,2) =  0.0;
        deriv2(dsds,2) =  0.0;
        deriv2(drds,2) =  0.25;

        deriv2(drdr,3) =  0.0;
        deriv2(dsds,3) =  0.0;
        deriv2(drds,3) = -0.25;
        break;
    }
    case DRT::Element::quad9:
    {
        const double rp=1.0+r;
        const double rm=1.0-r;
        const double sp=1.0+s;
        const double sm=1.0-s;
        const double r2=1.0-r*r;
        const double s2=1.0-s*s;
        const double rh=0.5*r;
        const double sh=0.5*s;
        const double rhp=r+0.5;
        const double rhm=r-0.5;
        const double shp=s+0.5;
        const double shm=s-0.5;

        deriv2(drdr,0) =-sh*sm;
        deriv2(dsds,0) =-rh*rm;
        deriv2(drds,0) = shm*rhm;

        deriv2(drdr,1) =-sh*sm;
        deriv2(dsds,1) = rh*rp;
        deriv2(drds,1) = shm*rhp;

        deriv2(drdr,2) = sh*sp;
        deriv2(dsds,2) = rh*rp;
        deriv2(drds,2) = shp*rhp;

        deriv2(drdr,3) = sh*sp;
        deriv2(dsds,3) =-rh*rm;
        deriv2(drds,3) = shp*rhm;

        deriv2(drdr,4) = 2.0*sh*sm;
        deriv2(dsds,4) = r2;
        deriv2(drds,4) =-2.0*r*shm;

        deriv2(drdr,5) = s2;
        deriv2(dsds,5) =-2.0*rh*rp;
        deriv2(drds,5) =-2.0*s*rhp;

        deriv2(drdr,6) =-2.0*sh*sp;
        deriv2(dsds,6) = r2;
        deriv2(drds,6) =-2.0*r*shp;

        deriv2(drdr,7) = s2;
        deriv2(dsds,7) = 2.0*rh*rm;
        deriv2(drds,7) =-2.0*s*rhm;

        deriv2(drdr,8) =-2.0*s2;
        deriv2(dsds,8) =-2.0*r2;
        deriv2(drds,8) = 2.0*s*2.0*r;
        break;
    }
    case DRT::Element::tri6:
    {
        deriv2(drdr,0) = 4.0;
        deriv2(dsds,0) = 4.0;
        deriv2(drds,0) = 4.0;

        deriv2(drdr,1) = 4.0;
        deriv2(dsds,1) = 0.0;
        deriv2(drds,1) = 0.0;

        deriv2(drdr,2) = 0.0;
        deriv2(dsds,2) = 4.0;
        deriv2(drds,2) = 0.0;

        deriv2(drdr,3) =-8.0;
        deriv2(dsds,3) = 0.0;
        deriv2(drds,3) =-4.0;

        deriv2(drdr,4) = 0.0;
        deriv2(dsds,4) = 0.0;
        deriv2(drds,4) = 4.0;

        deriv2(drdr,5) = 0.0;
        deriv2(dsds,5) =-8.0;
        deriv2(drds,5) =-4.0;
        break;
    }
    default:
        dserror("distype unknown\n");
    } /* end switch(distype) */

    return;
}

//
// shape functions and natural deriv1atives
//
void DRT::Utils::shape_function_1D(
                     Epetra_SerialDenseVector&                  funct,
                     const double&                              r,
                     const DRT::Element::DiscretizationType&    distype)
{
    switch (distype)
    {
    case DRT::Element::line2:
    {
        funct[0] = 0.5*(1.0 - r);
        funct[1] = 0.5*(1.0 + r);
        break;
    }
    case DRT::Element::line3:
    {
        funct[0] = -0.5*r*(1.0 - r);
        funct[1] =  0.5*r*(1.0 + r);
        funct[2] =  1 - r*r;
        break;
    }
    default:
        dserror("distype unknown\n");
    } /* end switch(distype) */

    return;

}

//
// shape functions and natural deriv1atives
//
void DRT::Utils::shape_function_1D_deriv1(
                     Epetra_SerialDenseMatrix&                  deriv1,
                     const double&                              r,
                     const DRT::Element::DiscretizationType&    distype)
{
    switch (distype)
    {
    case DRT::Element::line2:
    {
        deriv1(0,0)= -0.5;
        deriv1(1,0)=  0.5;
        break;
    }
    case DRT::Element::line3:
    {
        deriv1(0,0)= r - 0.5;
        deriv1(1,0)= r + 0.5;
        deriv1(2,0)= -2.0*r;
        break;
    }
    default:
        dserror("distype unknown\n");
  } /* end switch(distype) */

  return;

}

//
// shape functions and natural derivatives
//
void DRT::Utils::shape_function_1D_deriv2(
                     Epetra_SerialDenseMatrix&                  deriv2,
                     const double&                              r,
                     const DRT::Element::DiscretizationType&    distype)
{
    switch (distype)
    {
    case DRT::Element::line2:
    {
        deriv2(0,0)= 0.0;
        deriv2(1,0)= 0.0;
        break;
    }
    case DRT::Element::line3:
    {
        deriv2(0,0)=  1.0;
        deriv2(1,0)=  1.0;
        deriv2(2,0)= -2.0;
        break;
    }
    default:
        dserror("distype unknown\n");
  } /* end switch(distype) */

  return;

}




#endif  // #ifdef TRILINOS_PACKAGE
#endif  // #ifdef CCADISCRET
