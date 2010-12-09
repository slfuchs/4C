/*!----------------------------------------------------------------------
\file statmech_output.cpp
\brief output methods for statistical mechanics

<pre>
Maintainer: Christian Cyron
            cyron@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15234
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <Teuchos_Time.hpp>

#include "statmech_manager.H"
#include "../drt_inpar/inpar_statmech.H"
#include "../drt_lib/drt_utils.H"
#include "../linalg/linalg_utils.H"
#include "../drt_lib/drt_element.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_condition_utils.H"
#include "../linalg/linalg_fixedsizematrix.H"
#include "../drt_fem_general/largerotations.H"


#ifdef D_BEAM3
#include "../drt_beam3/beam3.H"
#endif
#ifdef D_BEAM3II
#include "../drt_beam3ii/beam3ii.H"
#endif
#ifdef D_TRUSS3
#include "../drt_truss3/truss3.H"
#endif

#include "../drt_torsion3/torsion3.H"

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <math.h>

//namespace with utility functions for operations with large rotations used
using namespace LARGEROTATIONS;

//MEASURETIME activates measurement of computation time for certain parts of the code
//#define MEASURETIME


/*----------------------------------------------------------------------*
 | write special output for statistical mechanics (public)    cyron 09/08|
 *----------------------------------------------------------------------*/
void StatMechManager::Output(ParameterList& params, const int ndim,
                                     const double& time, const int& istep, const double& dt,
                                     const Epetra_Vector& dis, const Epetra_Vector& fint)
{
  /*in general simulations in statistical mechanics run over so many time steps that the amount of data stored in the error file
   * may exceed the capacity even of a server hard disk; thus, we rewind the error file in each time step so that the amount of data
   * does not increase after the first time step any longer*/
  bool printerr = params.get<bool> ("print to err", false);
  FILE* errfile = params.get<FILE*> ("err file", NULL);
  if (printerr)
    rewind(errfile);

  //the following variable makes sense in case of serial computing only; its use is not allowed for parallel computing!
  int num_dof = dis.GlobalLength();

  double starttime = statmechparams_.get<double>("STARTTIME",0.0);

  switch (Teuchos::getIntegralValue<INPAR::STATMECH::StatOutput>(statmechparams_, "SPECIAL_OUTPUT"))
  {
    case INPAR::STATMECH::statout_endtoendlog:
    {

      double endtoend = 0; //end to end length at a certain time step in single filament dynamics
      double DeltaR2 = 0;

      //as soon as system is equilibrated (after time STARTTIME) a new file for storing output is generated
      if ( (time > starttime && fabs(time-starttime)>dt/1e4) && (starttimeoutput_ == -1.0))
      {
        endtoendref_ = pow(pow((dis)[num_dof - 3] + 10 - (dis)[0], 2) + pow(
            (dis)[num_dof - 2] - (dis)[1], 2), 0.5);
        starttimeoutput_ = time;
        istart_ = istep;
      }
      if (time > starttimeoutput_ && starttimeoutput_ > -1.0)
      {
        endtoend = pow(pow((dis)[num_dof - 3] + 10 - (dis)[0], 2) + pow(
            (dis)[num_dof - 2] - (dis)[1], 2), 0.5);

        //applying in the following a well conditioned substraction formula according to Crisfield, Vol. 1, equ. (7.53)
        DeltaR2 = pow((endtoend * endtoend - endtoendref_ * endtoendref_) / (endtoend + endtoendref_), 2);

        //writing output: writing Delta(R^2) according to PhD thesis Hallatschek, eq. (4.60), where t=0 corresponds to starttimeoutput_
        if ((istep - istart_) % int(ceil(pow(10, floor(log10((time - starttimeoutput_) / (10* dt )))))) == 0)
        {

          //proc 0 write complete output into file, all other proc inactive
          if(!discret_.Comm().MyPID())
          {
            FILE* fp = NULL; //file pointer for statistical output file

            //name of output file
            std::ostringstream outputfilename;
            outputfilename << "EndToEnd" << outputfilenumber_ << ".dat";

            // open file and append new data line
            fp = fopen(outputfilename.str().c_str(), "a");

            //defining temporary stringstream variable
            std::stringstream filecontent;
            filecontent << scientific << setprecision(15) << time - starttimeoutput_ << "  " << DeltaR2 << endl;

            // move temporary stringstream to file and close it
            fprintf(fp, filecontent.str().c_str());
            fclose(fp);
          }
        }
      }

    }
    break;
    case INPAR::STATMECH::statout_endtoendconst:
    {
      /*in the following we assume that there is only a pulling force point Neumann condition of equal absolute
       *value on either filament length; we get the absolute value of the first one of these two conditions */
      double neumannforce;
      vector<DRT::Condition*> pointneumannconditions(0);
      discret_.GetCondition("PointNeumann", pointneumannconditions);
      if (pointneumannconditions.size() > 0)
      {
        const vector<double>* val = pointneumannconditions[0]->Get<vector<double> > ("val");
        neumannforce = fabs((*val)[0]);
      }
      else
        neumannforce = 0;

      double endtoend = 0.0; //end to end length at a certain time step in single filament dynamics

      //as soon as system is equilibrated (after time STARTTIME) a new file for storing output is generated
      if ((time > starttime && fabs(time-starttime)>dt/1e4) && (starttimeoutput_ == -1.0))
      {
        starttimeoutput_ = time;
        istart_ = istep;
      }

      if (time > starttimeoutput_ && starttimeoutput_ > -1.0)
      {
        //end to end vector
        LINALG::Matrix<3, 1> endtoendvector(true);
        for (int i = 0; i < ndim; i++)
        {
          endtoendvector(i) -= (discret_.gNode(0))->X()[i] + dis[i];
          endtoendvector(i) += (discret_.gNode(discret_.NumMyRowNodes() - 1))->X()[i] + dis[num_dof - discret_.NumDof(discret_.gNode(discret_.NumMyRowNodes() - 1)) + i];
        }

        endtoend = endtoendvector.Norm2();

        //writing output: current time and end to end distance are stored at each 100th time step
        if ((istep - istart_) % statmechparams_.get<int> ("OUTPUTINTERVALS", 1) == 0)
        {

          //proc 0 write complete output into file, all other proc inactive
          if(!discret_.Comm().MyPID())
          {
            FILE* fp = NULL; //file pointer for statistical output file

            //name of output file
            std::ostringstream outputfilename;
            outputfilename << "E2E_" << discret_.NumMyRowElements() << '_' << dt<< '_' << neumannforce << '_' << outputfilenumber_ << ".dat";

            // open file and append new data line
            fp = fopen(outputfilename.str().c_str(), "a");
            //defining temporary stringstream variable
            std::stringstream filecontent;
            filecontent << scientific << setprecision(15) << time << "  "
                        << endtoend << " " << fint[num_dof - discret_.NumDof(
                           discret_.gNode(discret_.NumMyRowNodes() - 1))] << endl;
            // move temporary stringstream to file and close it
            fprintf(fp, filecontent.str().c_str());
            fclose(fp);
          }
        }
      }
    }
    break;
    /*computing and writing into file data about correlation of orientation of different elements
     *as it is considered in the context of the persistence length*/
    case INPAR::STATMECH::statout_orientationcorrelation:
    {

      //we need displacements also of ghost nodes and hence export displacment vector to column map format
      Epetra_Vector discol(*(discret_.DofColMap()), true);
      LINALG::Export(dis, discol);

      vector<double> arclength(discret_.NumMyColNodes() - 1, 0);
      vector<double> cosdiffer(discret_.NumMyColNodes() - 1, 0);

      //after initilization time write output cosdiffer in every statmechparams_.get<int>("OUTPUTINTERVALS",1) timesteps,
      //when discret_.NumMyRowNodes()-1 = 0,cosdiffer is always equil to 1!!
      if ((time > starttime && fabs(time-starttime)>dt/1e4) && (istep% statmechparams_.get<int> ("OUTPUTINTERVALS", 1) == 0))
      {
        Epetra_SerialDenseMatrix coord;
        coord.Shape(discret_.NumMyColNodes(), ndim);

        for (int id=0; id<discret_.NumMyColNodes(); id++)
          for (int j=0; j<ndim; j++)
            coord(id, j) = (discret_.lColNode(id))->X()[j] + (discol)[(id) * (ndim - 1) * 3 + j];

        for (int id=0; id < discret_.NumMyColNodes() - 1; id++)
        {

          //calculate the deformed length of every element
          for (int j=0; j<ndim; j++)
          {
            arclength[id] += pow((coord(id + 1, j) - coord(id, j)), 2);
            cosdiffer[id] += (coord(id + 1, j) - coord(id, j)) * (coord(0 + 1, j) - coord(0, j));
          }

          //calculate the cosine difference referring to the first element
          //Dot product of the (id+1)th element with the 1st element and devided by the length of the (id+1)th element and the 1st element
          arclength[id] = pow(arclength[id], 0.5);
          cosdiffer[id] = cosdiffer[id] / (arclength[id] * arclength[0]);
        }

        //proc 0 write complete output into file, all other proc inactive
        if(!discret_.Comm().MyPID())
        {
          FILE* fp = NULL; //file pointer for statistical output file

          //name of output file
          std::ostringstream outputfilename;
          outputfilename.str("");
          outputfilename << "OrientationCorrelation" << outputfilenumber_ << ".dat";

          fp = fopen(outputfilename.str().c_str(), "a");
          std::stringstream filecontent;
          filecontent << istep;
          filecontent << scientific << setprecision(10);

          for (int id = 0; id < discret_.NumMyColNodes() - 1; id++)
            filecontent << " " << cosdiffer[id];

          filecontent << endl;
          fprintf(fp, filecontent.str().c_str());
          fclose(fp);
        }

      }

    }
    break;
    //the following output allows for anisotropic diffusion simulation of a quasi stiff polymer
    case INPAR::STATMECH::statout_anisotropic:
    {
      //output in every statmechparams_.get<int>("OUTPUTINTERVALS",1) timesteps
      if ((time > starttime && fabs(time-starttime)>dt/1e4) && (istep % statmechparams_.get<int> ("OUTPUTINTERVALS", 1) == 0))
      {


        //positions of first and last node in current time step (note: always 3D variables used; in case of 2D third compoenent just constantly zero)
        LINALG::Matrix<3, 1> beginnew;
        LINALG::Matrix<3, 1> endnew;

        beginnew.PutScalar(0);
        endnew.PutScalar(0);
        std::cout << "ndim: " << ndim << "\n";

        for (int i = 0; i < ndim; i++)
        {
          beginnew(i) = (discret_.gNode(0))->X()[i] + dis[i];
          endnew(i) = (discret_.gNode(discret_.NumMyRowNodes() - 1))->X()[i] + dis[num_dof - discret_.NumDof(discret_.gNode(discret_.NumMyRowNodes() - 1)) + i];
        }

        //unit direction vector for filament axis in last time step
        LINALG::Matrix<3, 1> axisold;
        axisold = endold_;
        axisold -= beginold_;
        axisold.Scale(1 / axisold.Norm2());

        //unit direction vector for filament axis in current time step
        LINALG::Matrix<3, 1> axisnew;
        axisnew = endnew;
        axisnew -= beginnew;
        axisnew.Scale(1 / axisnew.Norm2());

        //displacement of first and last node between last time step and current time step
        LINALG::Matrix<3, 1> dispbegin;
        LINALG::Matrix<3, 1> dispend;
        dispbegin = beginnew;
        dispbegin -= beginold_;
        dispend = endnew;
        dispend -= endold_;

        //displacement of middle point
        LINALG::Matrix<3, 1> dispmiddle;
        dispmiddle = dispbegin;
        dispmiddle += dispend;
        dispmiddle.Scale(0.5);
        sumdispmiddle_ += dispmiddle;

        //update sum of square displacement increments of middle point
        double incdispmiddle = dispmiddle.Norm2() * dispmiddle.Norm2();
        sumsquareincmid_ += incdispmiddle;

        //update sum of square displacement increments of middle point parallel to new filament axis (computed by scalar product)
        double disppar_square = pow(axisnew(0) * dispmiddle(0) + axisnew(1) * dispmiddle(1) + axisnew(2) * dispmiddle(2), 2);
        sumsquareincpar_ += disppar_square;

        //update sum of square displacement increments of middle point orthogonal to new filament axis (via crossproduct)
        LINALG::Matrix<3, 1> aux;
        aux(0) = dispmiddle(1) * axisnew(2) - dispmiddle(2) * axisnew(1);
        aux(1) = dispmiddle(2) * axisnew(0) - dispmiddle(0) * axisnew(2);
        aux(2) = dispmiddle(0) * axisnew(1) - dispmiddle(1) * axisnew(0);
        double disport_square = aux.Norm2() * aux.Norm2();
        sumsquareincort_ += disport_square;

        //total displacement of rotational angle (in 2D only)
        double incangle = 0;
        if (ndim == 2)
        {
          //angle of old axis relative to x-axis
          double phiold = acos(axisold(0) / axisold.Norm2());
          if (axisold(1) < 0)
            phiold *= -1;

          //angle of new axis relative to x-axis
          double phinew = acos(axisnew(0) / axisnew.Norm2());
          if (axisnew(1) < 0)
            phinew *= -1;

          //angle increment
          incangle = phinew - phiold;
          if (incangle > M_PI)
          {
            incangle -= 2*M_PI;
            incangle *= -1;
          }
          if (incangle < -M_PI)
          {
            incangle += 2*M_PI;
            incangle *= -1;
          }

          //update absolute rotational displacement compared to reference configuration
          sumsquareincrot_ += incangle * incangle;
          sumrotmiddle_ += incangle;
        }

        //proc 0 write complete output into file, all other proc inactive
        if(!discret_.Comm().MyPID())
        {
          FILE* fp = NULL; //file pointer for statistical output file

          //name of output file
          std::ostringstream outputfilename;
          outputfilename << "AnisotropicDiffusion" << outputfilenumber_ << ".dat";

          // open file and append new data line
          fp = fopen(outputfilename.str().c_str(), "a");

          //defining temporary stringstream variable
          std::stringstream filecontent;
          filecontent << scientific << setprecision(15) << dt << " "
              << sumsquareincmid_ << " " << sumsquareincpar_ << " "
              << sumsquareincort_ << " " << sumsquareincrot_ << " "
              << sumdispmiddle_.Norm2() * sumdispmiddle_.Norm2() << " "
              << sumrotmiddle_ * sumrotmiddle_ << endl;

          // move temporary stringstream to file and close it
          fprintf(fp, filecontent.str().c_str());
          fclose(fp);
        }

        //new positions in this time step become old positions in last time step
        beginold_ = beginnew;
        endold_ = endnew;
      }
    }
    break;
    case INPAR::STATMECH::statout_viscoelasticity:
    {
      //output in every statmechparams_.get<int>("OUTPUTINTERVALS",1) timesteps (or for the very last step)
      if (istep % statmechparams_.get<int> ("OUTPUTINTERVALS", 1) == 0 || istep==params.get<int>("nstep",5)-1 || fabs(time-starttime)<1e-8)
      {

#ifdef DEBUG
        if (forcesensor_ == null)
          dserror("forcesensor_ is NULL pointer; possible reason: dynamic crosslinkers not activated and forcesensor applicable in this case only");
#endif  // #ifdef DEBUG
        double f = 0;//mean value of force
        double d = 0;//Displacement
        int count = 0;

        for(int i=0; i<forcesensor_->MyLength(); i++)//changed
          if((*forcesensor_)[i]>0.9)
          {
            count++;
            f += fint[i];
            d = dis[i];
          }

        //f is the sum of all forces at the top on this processor; compute the sum fglob on all processors all together
        double fglob = 0;
        discret_.Comm().SumAll(&f,&fglob,1);



        if(!discret_.Comm().MyPID())
        {

          //pointer to file into which each processor writes the output related with the dof of which it is the row map owner
          FILE* fp = NULL;

          //content to be written into the output file
          std::stringstream filecontent;

          //name of file into which output is written
          std::ostringstream outputfilename;
          outputfilename << "ViscoElOutputProc.dat";

          fp = fopen(outputfilename.str().c_str(), "a");

          /*the output to be written consists of internal forces at exactly those degrees of freedom
           * marked in *forcesensor_ by a one entry*/

          filecontent << scientific << setprecision(10) << time;//changed

          //Putting time, displacement, meanforce  in Filestream
          filecontent << "   "<< d << "   " << fglob << "   " << discret_.NumGlobalElements() << endl; //changed
          //writing filecontent into output file and closing it
          fprintf(fp,filecontent.str().c_str());
          fclose(fp);
        }
      }

    }
    break;
    //writing data for generating a Gmsh video of the simulation
    case INPAR::STATMECH::statout_gmsh:
    {
      //output in every statmechparams_.get<int>("OUTPUTINTERVALS",1) timesteps
      if( istep % statmechparams_.get<int>("OUTPUTINTERVALS",1) == 0 )
      {
        std::ostringstream filename2;
        filename2 << "./DensityDensityCorrFunction_"<<std::setw(6) << setfill('0') << istep <<".dat";
        DDCorrOutput(dis, filename2, istep);
        /*construct unique filename for gmsh output with two indices: the first one marking the time step number
         * and the second one marking the newton iteration number, where numbers are written with zeros in the front
         * e.g. number one is written as 000001, number fourteen as 000014 and so on;*/

        // first index = time step index
        std::ostringstream filename;

        //creating complete file name dependent on step number with 6 digits and leading zeros
        if (istep<1000000)
        	filename << "./GmshOutput/network"<< std::setw(6) << setfill('0') << istep <<".pos";
        else
        	dserror("Gmsh output implemented for a maximum of 999999 steps");

        //calling method for writing Gmsh output
        GmshOutput(dis,filename,istep);
      }
    }
    break;
    case INPAR::STATMECH::statout_densitydensitycorr:
    {
      //output in every statmechparams_.get<int>("OUTPUTINTERVALS",1) timesteps
      if( istep % statmechparams_.get<int>("OUTPUTINTERVALS",1) == 0 )
      {

        std::ostringstream filename;
        filename << "./DensityDensityCorrFunction_"<<std::setw(6) << setfill('0') << istep <<".dat";
        DDCorrOutput(dis, filename, istep);
      }
    }
    break;
    case INPAR::STATMECH::statout_none:
    default:
    break;
  }

  return;
} // StatMechManager::Output()


/*----------------------------------------------------------------------*
 | writing Gmsh data for current step                 public)cyron 01/09|
 *----------------------------------------------------------------------*/
void StatMechManager::GmshOutput(const Epetra_Vector& disrow, const std::ostringstream& filename, const int& step)
{
  /*the following method writes output data for Gmsh into file with name "filename"; all line elements are written;
   * the nodal displacements are handed over in the variable "dis"; note: in case of parallel computing only
   * processor 0 writes; it is assumed to have a fully overlapping column map and hence all the information about
   * all the nodal position; parallel output is now possible with the restriction that the nodes(processors) in question
   * are of the same machine*/
	double periodlength = statmechparams_.get<double>("PeriodLength", 0.0);

	GmshPrepareVisualization(disrow);

  //we need displacements also of ghost nodes and hence export displacment vector to column map format
  Epetra_Vector discol(*(discret_.DofColMap()), true);
  LINALG::Export(disrow, discol);

  // do output to file in c-style
  FILE* fp = NULL;

  //number of solid elements by which a round line is depicted
  const int nline = 8;

  // first processor starts by opening the file and writing the header, other processors have to wait
  if (discret_.Comm().MyPID() == 0)
  {
    //open file to write output data into
    fp = fopen(filename.str().c_str(), "w");
    // write output to temporary stringstream;
    std::stringstream gmshfileheader;
    /*the beginning of the stream is "View \"" to indicate Gmsh that the following data is in order to create an image and
     * this command is followed by the name of that view displayed during it's shown in the video; in the following example
     * this name is for the 100th time step: Step00100; then the data to be presented within this view is written within { ... };
     * in the following example this data consists of scalar lines defined by the coordinates of their end points*/
    gmshfileheader <<"General.BackgroundGradient = 0;\n";
    gmshfileheader <<"View.LineType = 1;\n";
    gmshfileheader <<"View.LineWidth = 1.4;\n";
    gmshfileheader <<"View.PointType = 1;\n";
    gmshfileheader <<"View.PointSize = 3;\n";
    gmshfileheader <<"General.ColorScheme = 1;\n";
    gmshfileheader <<"General.Color.Background = {255,255,255};\n";
    gmshfileheader <<"General.Color.Foreground = {255,255,255};\n";
    //gmshfileheader <<"General.Color.BackgroundGradient = {128,147,255};\n";
    gmshfileheader <<"General.Color.Foreground = {85,85,85};\n";
    gmshfileheader <<"General.Color.Text = {0,0,0};\n";
    gmshfileheader <<"General.Color.Axes = {0,0,0};\n";
    gmshfileheader <<"General.Color.SmallAxes = {0,0,0};\n";
    gmshfileheader <<"General.Color.AmbientLight = {25,25,25};\n";
    gmshfileheader <<"General.Color.DiffuseLight = {255,255,255};\n";
    gmshfileheader <<"General.Color.SpecularLight = {255,255,255};\n";
    gmshfileheader <<"View.ColormapAlpha = 1;\n";
    gmshfileheader <<"View.ColormapAlphaPower = 0;\n";
    gmshfileheader <<"View.ColormapBeta = 0;\n";
    gmshfileheader <<"View.ColormapBias = 0;\n";
    gmshfileheader <<"View.ColormapCurvature = 0;\n";
    gmshfileheader <<"View.ColormapInvert = 0;\n";
    gmshfileheader <<"View.ColormapNumber = 2;\n";
    gmshfileheader <<"View.ColormapRotation = 0;\n";
    gmshfileheader <<"View.ColormapSwap = 0;\n";
    gmshfileheader <<"View.ColorTable = {Black,Yellow,Blue,Orange,Red,Cyan,Purple,Brown,Green};\n";
    gmshfileheader << "View \" Step " << step << " \" {" << endl;

    //write content into file and close it
    fprintf(fp, gmshfileheader.str().c_str());
    fclose(fp);
  }

  // wait for all processors to arrive at this point
  discret_.Comm().Barrier();

  // loop over the participating processors each of which appends its part of the output to one output file
  for (int proc = 0; proc < discret_.Comm().NumProc(); proc++)
  {
    if (discret_.Comm().MyPID() == proc)
    {
      //open file again to append ("a") output data into
      fp = fopen(filename.str().c_str(), "a");
      // write output to temporary stringstream;
      std::stringstream gmshfilecontent;

      //looping through all elements on the processor
      for (int i=0; i<discret_.NumMyColElements(); ++i)
      {
        //getting pointer to current element
        DRT::Element* element = discret_.lColElement(i);

        //getting number of nodes of current element
        //if( element->NumNode() > 2)
        //dserror("Gmsh output for two noded elements only");

        //preparing variable storing coordinates of all these nodes
        LINALG::SerialDenseMatrix coord(3, element->NumNode());
        for (int id=0; id<3; id++)
          for (int jd=0; jd<element->NumNode(); jd++)
          {
            double referenceposition = ((element->Nodes())[jd])->X()[id];
            vector<int> dofnode = discret_.Dof((element->Nodes())[jd]);
            double displacement = discol[discret_.DofColMap()->LID(dofnode[id])];
            coord(id, jd) = referenceposition + displacement;
          }

        //declaring variable for color of elements
        double color;

        //apply different colors for elements representing filaments and those representing dynamics crosslinkers
        if (element->Id() < basisnodes_)
          color = 1.0;
        else
          color = 0.5;

        //if no periodic boundary conditions are to be applied, we just plot the current element
        if (periodlength == 0.0)
        {
          // check whether the kinked visualization is to be applied
          bool kinked = CheckForKinkedVisual(element->Id());

          const DRT::ElementType & eot = element->ElementType();
#ifdef D_BEAM3
#ifdef D_BEAM3II
          if (eot == DRT::ELEMENTS::Beam3Type::Instance() ||
              eot==DRT::ELEMENTS::Beam3iiType::Instance())
          {
            if (!kinked)
            {
              for (int j=0; j<element->NumNode() - 1; j++)
              {
                //define output coordinates
                LINALG::SerialDenseMatrix coordout(3,2);
                for(int m=0; m<3; m++)
                  for(int n=0; n<2; n++)
                    coordout(m,n)=coord(m,j+n);

                 GMSH_2_noded(nline,coordout,element,gmshfilecontent,color,false);

              }
            }
            else
              GmshKinkedVisual(coord, 0.875, element->Id(), gmshfilecontent);
          }
          else
#endif
#endif
#ifdef D_TRUSS3
          if (eot == DRT::ELEMENTS::Truss3Type::Instance())
          {
            if (!kinked)
            {
              for (int j=0; j<element->NumNode()-1; j++)
              {
                gmshfilecontent << "SL(" << scientific;
                gmshfilecontent << coord(0, j) << "," << coord(1, j) << ","<< coord(2, j) << ","
                                << coord(0, j + 1) << "," << coord(1,j + 1) << "," << coord(2, j + 1);
                gmshfilecontent << ")" << "{" << scientific << color << ","<< color << "};" << endl;
              }
            }
            else
              GmshKinkedVisual(coord, 0.875, element->Id(), gmshfilecontent);
          }
          else
#endif
#ifdef D_TORSION3
          if (eot == DRT::ELEMENTS::Torsion3Type::Instance())
          {
            double beadcolor = 0.75;
            for (int j=0; j<element->NumNode(); j++)
            {
              gmshfilecontent << "SP(" << scientific;
              gmshfilecontent << coord(0, j) << "," << coord(1, j) << ","<< coord(2, j);
              gmshfilecontent << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
            }
          }
          else
#endif
          {
          }
        }
        //in case of periodic boundary conditions we have to take care to plot correctly an element broken at some boundary plane
        else
          GmshOutputPeriodicBoundary(coord, color, gmshfilecontent,element->Id(),false);
      }
      //write content into file and close it (this way we make sure that the output is written serially)
      fprintf(fp, gmshfilecontent.str().c_str());
      fclose(fp);
    }
    discret_.Comm().Barrier();
  }
  // plot the periodic boundary box
  std::vector<double> center(3,periodlength/2);
  GmshOutputBox(0.0, &center, periodlength, &filename);
  // plot the shifted center
  std::vector<double> dummyshift(3,0.0);
  std::vector<int> dummyentries;
 	DDCorrShift(&center, &dummyshift, &dummyentries);
  GmshOutputBox(0.75, &center, 0.125, &filename);
  // plot crosslink molecule diffusion and (partial) bonding
  GmshOutputCrosslinkDiffusion(0.125, &filename, disrow);
  // finish data section of this view by closing curly brackets
  if (discret_.Comm().MyPID() == 0)
  {
    fp = fopen(filename.str().c_str(), "a");
    std::stringstream gmshfileend;

    gmshfileend << "SP(" << scientific;
    gmshfileend << center[0]<<","<<center[1]<<","<<center[2]<<"){" << scientific << 0.75 << ","<< 0.75 <<"};"<<endl;
    gmshfileend << "};" << endl;
    fprintf(fp, gmshfileend.str().c_str());
    fclose(fp);
  }

  // return simultaneously (not sure if really needed)
  discret_.Comm().Barrier();

  return;
} // StatMechManager::GmshOutput()


/*----------------------------------------------------------------------*
 | gmsh output data in case of periodic boundary conditions             |
 |                                                    public)cyron 02/10|
 *----------------------------------------------------------------------*/
void StatMechManager::GmshOutputPeriodicBoundary(const LINALG::SerialDenseMatrix& coord, const double& color, std::stringstream& gmshfilecontent, int eleid, bool ignoreeleid)
{
  //number of solid elements by which a round line is depicted
  const int nline = 8;

  //number of spatial dimensions
  const int ndim = 3;
  // get Element Type of the first Element to determine the graphics output
  DRT::Element* element = discret_.gElement(eleid);

  bool dotline = false;
  bool kinked = false;

  if (ignoreeleid)
    dotline = true;
  else
  {
    // draw colored lines between two nodes of a beam3 or truss3 element (meant for filaments/crosslinks/springs)
    const DRT::ElementType & eot = element->ElementType();
#ifdef D_BEAM3
    if (element->ElementType().Name() == "Beam3Type")
      dotline = eot == DRT::ELEMENTS::Beam3Type::Instance();
#endif
#ifdef D_BEAM3II
    if(element->ElementType().Name()=="Beam3iiType")
    dotline = eot==DRT::ELEMENTS::Beam3iiType::Instance();
#endif
#ifdef D_TRUSS3
    if (element->ElementType().Name() == "Truss3Type")
      dotline = dotline or eot == DRT::ELEMENTS::Truss3Type::Instance();
#endif
#ifdef D_TORSION3
    // draw spheres at node positions ("beads" of the bead spring model)
    if (eot == DRT::ELEMENTS::Torsion3Type::Instance())
    {
      double beadcolor = 0.75;
      for (int i=0; i<element->NumNode(); i++)
      {
        //writing element by nodal coordinates as a sphere
        gmshfilecontent << "SP(" << scientific;
        gmshfilecontent << coord(0,i) << "," << coord(1, i) << "," << coord(2,i);
        gmshfilecontent << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
      }
    }
#endif
    /* determine whether crosslink connects two filaments or occupies two binding spots on the same filament;
     * variable triggers different visualizations.*/
    kinked = CheckForKinkedVisual(element->Id());
  }

  if (dotline)
  {
    /*detect and save in vector "cut", at which boundaries the element is broken due to periodic boundary conditions;
     * the entries of cut have the following meaning: 0: element not broken in respective coordinate direction, 1:
     * element broken in respective coordinate direction (node 0 close to zero boundary and node 1 close to boundary
     * at PeriodLength);  2: element broken in respective coordinate direction (node 1 close to zero boundary and node
     * 0 close to boundary at PeriodLength);*/
    LINALG::SerialDenseMatrix cut;
    if (ignoreeleid)
      cut = LINALG::SerialDenseMatrix(3, 1, true);
    else
      cut = LINALG::SerialDenseMatrix(3, (int)element->NumNode()-1, true);

    /* "coord" currently holds the shifted set of coordinates.
     * In order to determine the correct vector "dir" of the visualization at the boundaries,
     * a copy of "coord" with adjustments in the proper places is introduced*/
    LINALG::SerialDenseMatrix unshift = coord;

    for (int i=0; i<cut.N(); i++)
    {
      for (int dof=0; dof<ndim; dof++)
      {
        if (fabs(coord(dof,i+1)-statmechparams_.get<double>("PeriodLength",0.0)-coord(dof,i)) < fabs(coord(dof,i+1) - coord(dof,i)))
        {
          cut(dof, i) = 1;
          unshift(dof, i + 1) -= statmechparams_.get<double>("PeriodLength",0.0);
        }
        if (fabs(coord(dof, i+1)+statmechparams_.get<double>("PeriodLength",0.0) - coord(dof,i)) < fabs(coord(dof,i+1)-coord(dof,i)))
        {
          cut(dof,i) = 2;
          unshift(dof,i+1) += statmechparams_.get<double>("PeriodLength",0.0);
        }
      }
    }

    // write special output for broken elements
    for (int i=0; i<cut.N(); i++)
    {
      if (cut(0, i) + cut(1, i) + cut(2, i) > 0)
      {
        //compute direction vector between first(i-th) and second(i+1-th) node of element (normed):
        LINALG::Matrix<3, 1> dir;
        double ldir = 0.0;
        for (int dof = 0; dof < ndim; dof++)
        {
          dir(dof) = unshift(dof, i + 1) - unshift(dof, i);
          ldir += dir(dof) * dir(dof);
        }
        for (int dof = 0; dof < ndim; dof++)
          dir(dof) /= ldir;

        //from node 0 to nearest boundary where element is broken you get by vector X + lambda0*dir
        double lambda0 = dir.Norm2();
        for (int dof = 0; dof < ndim; dof++)
        {
          if (cut(dof, i) == 1)
          {
            if (fabs(-coord(dof, i) / dir(dof)) < fabs(lambda0))
              lambda0 = -coord(dof, i) / dir(dof);
          }
          else if (cut(dof, i) == 2)
          {
            if (fabs((statmechparams_.get<double> ("PeriodLength", 0.0) - coord(dof, i)) / dir(dof)) < fabs(lambda0))
              lambda0 = (statmechparams_.get<double> ("PeriodLength", 0.0) - coord(dof, i)) / dir(dof);
          }
        }

        //from node 1 to nearest boundary where element is broken you get by vector X + lambda1*dir
        double lambda1 = dir.Norm2();
        for (int dof = 0; dof < ndim; dof++)
        {
          if (cut(dof, i) == 2)
          {
            if (fabs(-coord(dof, i + 1) / dir(dof)) < fabs(lambda1))
              lambda1 = -coord(dof, i + 1) / dir(dof);
          }
          else if (cut(dof, i) == 1)
          {
            if (fabs((statmechparams_.get<double> ("PeriodLength", 0.0) - coord(dof, i + 1)) / dir(dof)) < fabs(lambda1))
              lambda1 = (statmechparams_.get<double> ("PeriodLength", 0.0) - coord(dof, i + 1)) / dir(dof);
          }
        }
        //define output coordinates for broken elements, first segment
        LINALG::SerialDenseMatrix coordout=coord;
        for(int j=0 ;j<coordout.M(); j++)
          coordout(j,i+1) = coord(j,i) + lambda0*dir(j);
        if(!ignoreeleid)
          GMSH_2_noded(nline,coordout,element,gmshfilecontent,color,false);
        else
          GMSH_2_noded(nline,coordout,element,gmshfilecontent,color,true);

        //define output coordinates for broken elements, second segment
        for(int j=0; j<coordout.M(); j++)
        {
          coordout(j,i) = coord(j,i+1);
          coordout(j,i+1) = coord(j,i+1)+lambda1*dir(j);
        }
        if(!ignoreeleid)
          GMSH_2_noded(nline,coordout,element,gmshfilecontent,color,false);
        else
          GMSH_2_noded(nline,coordout,element,gmshfilecontent,color,true);

      }
      else // output for continuous elements
      {
        if (!kinked)
        {
          /*if(element->Id()>basisnodes_ && eleid!=0)
          {
            double l = sqrt((coord(0,1)-coord(0,0))*(coord(0,1)-coord(0,0)) +
                            (coord(1,1)-coord(1,0))*(coord(1,1)-coord(1,0)) +
                            (coord(2,1)-coord(2,0))*(coord(2,1)-coord(2,0)));
            if(l>1.05*(statmechparams_.get<double>("R_LINK",0.0)+statmechparams_.get<double>("DeltaR_LINK",0.0)) ||
              (l<0.95*(statmechparams_.get<double>("R_LINK",0.0)-statmechparams_.get<double>("DeltaR_LINK",0.0))))
              cout<<"Proc "<<discret_.Comm().MyPID()<<": GID="<<eleid<<", element->Id()="<<element->Id()<<"Nodes"<<element->NodeIds()[0]<<","<<element->NodeIds()[1]<<", l="<<l<<endl;
          }*/
          if(!ignoreeleid)
            GMSH_2_noded(nline,coord,element,gmshfilecontent,color,false);
          else
            GMSH_2_noded(nline,coord,element,gmshfilecontent,color,true);

        }
        else
          GmshKinkedVisual(coord, 0.875, element->Id(), gmshfilecontent);
      }
    }
  }
  return;
} // StatMechManager::GmshOutputPeriodicBoundary()

/*----------------------------------------------------------------------*
 | plot the periodic boundary box                  (public) mueller 7/10|
 *----------------------------------------------------------------------*/
void StatMechManager::GmshOutputBox(double boundarycolor, std::vector<double>* boxcenter, double length, const std::ostringstream *filename)
{
  double periodlength = statmechparams_.get<double> ("PeriodLength", 0.0);
  // plot the periodic box in case of periodic boundary conditions (first processor)
  if (periodlength > 0 && discret_.Comm().MyPID() == 0)
  {
    FILE *fp = fopen(filename->str().c_str(), "a");
    std::stringstream gmshfilefooter;
    // get current period length

    double xmin = (*boxcenter)[0]-length/2.0;
    double xmax = (*boxcenter)[0]+length/2.0;
    double ymin = (*boxcenter)[1]-length/2.0;
    double ymax = (*boxcenter)[1]+length/2.0;
    double zmin = (*boxcenter)[2]-length/2.0;
    double zmax = (*boxcenter)[2]+length/2.0;

    // define boundary lines
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmin << "," << ymin << "," << zmin << "," << xmax << ","<< ymin << "," << zmin;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 2
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmax << "," << ymin << "," << zmin << "," << xmax << "," << ymax << "," << zmin;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 3
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmax << "," << ymax << "," << zmin << "," << xmax << "," << ymax << "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 4
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmax << "," << ymax << "," << zmax << "," << xmin << "," << ymax << "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 5
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmin << "," << ymax << "," << zmax << "," << xmin << "," << ymin << "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 6
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmin << "," << ymin << "," << zmax << "," << xmin << ","<< ymin << "," << zmin;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 7
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmin << "," << ymin << "," << zmin << "," << xmin << ","<< ymax << "," << zmin;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 8
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmin << "," << ymax << "," << zmin << "," << xmax << "," << ymax << "," << zmin;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 9
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmin << "," << ymax << "," << zmin << "," << xmin << "," << ymax << "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 10
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmax << "," << ymin << "," << zmin << "," << xmax << "," << ymin << "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 11
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmax << "," << ymin << "," << zmax << "," << xmax << "," << ymax<< "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;
    // line 12
    gmshfilefooter << "SL(" << scientific;
    gmshfilefooter << xmax << "," << ymin << "," << zmax << "," << xmin << "," << ymin<< "," << zmax;
    gmshfilefooter << ")" << "{" << scientific << boundarycolor << ","<< boundarycolor << "};" << endl;

    fprintf(fp, gmshfilefooter.str().c_str());
    fclose(fp);
  }
  // wait for Proc 0 to catch up to the others
  discret_.Comm().Barrier();
}// StatMechManager::GmshOutputBoundaryBox

/*----------------------------------------------------------------------*
 | Gmsh output for crosslink molecule diffusion    (public) mueller 7/10|
 *----------------------------------------------------------------------*/
void StatMechManager::GmshOutputCrosslinkDiffusion(double color, const std::ostringstream *filename, const Epetra_Vector& disrow)
{
  // export row displacement to column map format
  Epetra_Vector discol(*(discret_.DofColMap()), true);
  LINALG::Export(disrow, discol);

  if (discret_.Comm().MyPID() == 0)
  {
    FILE *fp = fopen(filename->str().c_str(), "a");
    /*/ visualization of crosslink molecule positions by spheres on Proc 0
    std::stringstream gmshfilecross;
    for(int i=0; i<visualizepositions_->MyLength(); i++)
    {
      if((*numbond_)[i]<0.1)
      {
        double beadcolor = 5*color;
        //writing element by nodal coordinates as a sphere
        gmshfilecross << "SP(" << scientific;
        gmshfilecross<< (*visualizepositions_)[0][i]<< "," << (*visualizepositions_)[1][i] << "," << (*visualizepositions_)[2][i];
        gmshfilecross << ")" << "{" << scientific << beadcolor << "," << beadcolor << "};" << endl;
      }
    }
    fprintf(fp,gmshfilecross.str().c_str());
    fclose(fp);*/

    //special visualization for crosslink molecules with one/two bond(s); going through the Procs

    fp = fopen(filename->str().c_str(), "a");
    std::stringstream gmshfilebonds;

    // first, just update positions: redundant information on all procs
    for (int i=0; i<numbond_->MyLength(); i++)
    {
      switch ((int) (*numbond_)[i])
      {
        // crosslink molecule with one bond
        case 1:
        {
          // determine position of nodeGID entry
          int occupied = 0;
          for (int j=0; j<crosslinkerbond_->NumVectors(); j++)
            if ((int) (*crosslinkerbond_)[j][i] != -1)
            {
              occupied = j;
              break;
            }
          int nodeGID = (int) (*crosslinkerbond_)[occupied][i];

          DRT::Node *node = discret_.lColNode(discret_.NodeColMap()->LID(nodeGID));
          LINALG::SerialDenseMatrix coord(3, 2, true);
          for (int j=0; j<coord.M(); j++)
          {
            int dofgid = discret_.Dof(node)[j];
            coord(j, 0) = node->X()[j] + discol[dofgid];
            coord(j, 1) = (*visualizepositions_)[j][i];
            //length += (coord(j,1)-coord(j,0))*(coord(j,1)-coord(j,0));
          }

          double beadcolor = 2*color; //blue
          // in case of periodic boundary conditions
          if (statmechparams_.get<double> ("PeriodLength", 0.0) > 0.0)
          {
            // get arbitrary element (we just need it to properly visualize)
            DRT::Element* tmpelement=discret_.lRowElement(0);
            GmshOutputPeriodicBoundary(coord, 2*color, gmshfilebonds, tmpelement->Id(), true);
            // visualization of "real" crosslink molecule positions
            //beadcolor = 0.0; //black
            //gmshfilebonds << "SP(" << scientific;
            //gmshfilebonds << (*crosslinkerpositions_)[0][i] << ","<< (*crosslinkerpositions_)[1][i] << ","<< (*crosslinkerpositions_)[2][i];
            //gmshfilebonds << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
          }
          else
          {
            gmshfilebonds << "SL(" << scientific;
            gmshfilebonds << coord(0, 0) << "," << coord(1, 0) << ","<< coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)<< "," << coord(2, 1);
            gmshfilebonds << ")" << "{" << scientific << 2*color << ","<< 2*color << "};" << endl;
            gmshfilebonds << "SP(" << scientific;
            gmshfilebonds << coord(0, 1) << "," << coord(1, 1) << ","<< coord(2, 1);
            gmshfilebonds << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
          }
        }
          break;
          // crosslinker element: crosslink molecule (representation) position (Proc 0 only)
        case 2:
        {
          // actual crosslinker element connecting two filaments (self-binding kinked crosslinkers are visualized in GmshKinkedVisual())
          if((*searchforneighbours_)[i] > 0.9)
          {
            if((*crosslinkonsamefilament_)[i] < 0.1)
            {
              double beadcolor = 4* color ; //red
              gmshfilebonds << "SP(" << scientific;
              gmshfilebonds << (*visualizepositions_)[0][i] << ","<< (*visualizepositions_)[1][i] << ","<< (*visualizepositions_)[2][i];
              gmshfilebonds << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
            }
          }
          else  // passive crosslink molecule
          {
            // determine position of nodeGID entry
            int occupied = 0;
            for (int j=0; j<crosslinkerbond_->NumVectors(); j++)
              if ((int) (*crosslinkerbond_)[j][i] != -1)
              {
                occupied = j;
                break;
              }
            int nodeGID = (int) (*crosslinkerbond_)[occupied][i];

            DRT::Node *node = discret_.lColNode(discret_.NodeColMap()->LID(nodeGID));
            LINALG::SerialDenseMatrix coord(3, 2, true);
            for (int j=0; j<coord.M(); j++)
            {
              int dofgid = discret_.Dof(node)[j];
              coord(j, 0) = node->X()[j] + discol[dofgid];
              coord(j, 1) = (*visualizepositions_)[j][i];
            }

            double beadcolor = 3*color;
            // in case of periodic boundary conditions
            if (statmechparams_.get<double> ("PeriodLength", 0.0) > 0.0)
            {
              // get arbitrary element (we just need it to properly visualize)
              DRT::Element* tmpelement=discret_.lRowElement(0);
              GmshOutputPeriodicBoundary(coord, 3*color, gmshfilebonds, tmpelement->Id(), true);
              // visualization of "real" crosslink molecule positions
              //beadcolor = 0.0; //black
              //gmshfilebonds << "SP(" << scientific;
              //gmshfilebonds << (*crosslinkerpositions_)[0][i] << ","<< (*crosslinkerpositions_)[1][i] << ","<< (*crosslinkerpositions_)[2][i];
              //gmshfilebonds << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
            }
            else
            {
              gmshfilebonds << "SL(" << scientific;
              gmshfilebonds << coord(0, 0) << "," << coord(1, 0) << ","<< coord(2, 0) << "," << coord(0, 1) << "," << coord(1, 1)<< "," << coord(2, 1);
              gmshfilebonds << ")" << "{" << scientific << 3*color << ","<< 3*color << "};" << endl;
              gmshfilebonds << "SP(" << scientific;
              gmshfilebonds << coord(0, 1) << "," << coord(1, 1) << ","<< coord(2, 1);
              gmshfilebonds << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
            }
          }
        }
          break;
        default:
          continue;
      }
    }
    fprintf(fp, gmshfilebonds.str().c_str());
    fclose(fp);
  }

  discret_.Comm().Barrier();
}// GmshOutputCrosslinkDiffusion

/*----------------------------------------------------------------------*
 | Special Gmsh output for crosslinkers occupying two binding spots on  |
 | the same filament                              (public) mueller 07/10|
 *----------------------------------------------------------------------*/
void StatMechManager::GmshKinkedVisual(const LINALG::SerialDenseMatrix& coord, const double& color, int eleid, std::stringstream& gmshfilecontent)
{
  /* We need a third point in order to visualize the crosslinker.
   * It marks the location of the kink.
   */
  std::vector<double> thirdpoint(3,0.0);
  // get the element
  DRT::Element* element = discret_.gElement(eleid);

  // calculate tangent
  double ltan = 0.0;
  for (int j=0; j<coord.M(); j++)
    ltan += (coord(j, coord.N() - 1) - coord(j, 0)) * (coord(j, coord.N() - 1) - coord(j, 0));
  ltan = sqrt(ltan);

  std::vector<double> t(3, 0.0);
  for (int j=0; j<coord.M(); j++)
    t.at(j) = ((coord(j, coord.N() - 1) - coord(j, 0)) / ltan);

  // calculate normal via cross product: [0 0 1]x[tx ty tz]
  std::vector<double> n(3, 0.0);
  n.at(0) = -t.at(1);
  n.at(1) = t.at(0);
  // norm it since the cross product does not keep the length
  double lnorm = 0.0;
  for (int j=0; j<(int)n.size(); j++)
    lnorm += n.at(j) * n.at(j);
  lnorm = sqrt(lnorm);
  for (int j=0; j<(int) n.size(); j++)
    n.at(j) /= lnorm;

  // by modulo operation involving the node IDs
  double alpha = fmod((double) (element->Nodes()[element->NumNode() - 1]->Id() + element->Nodes()[0]->Id()), 2*M_PI);

  // rotate the normal by alpha
  LINALG::SerialDenseMatrix RotMat(3, 3);
  // build the matrix of rotation
  for (int j=0; j<3; j++)
    RotMat(j, j) = cos(alpha) + t.at(j) * t.at(j) * (1 - cos(alpha));
  RotMat(0, 1) = t.at(0) * t.at(1) * (1 - cos(alpha)) - t.at(2) * sin(alpha);
  RotMat(0, 2) = t.at(0) * t.at(2) * (1 - cos(alpha)) + t.at(1) * sin(alpha);
  RotMat(1, 0) = t.at(1) * t.at(0) * (1 - cos(alpha)) + t.at(2) * sin(alpha);
  RotMat(1, 2) = t.at(1) * t.at(2) * (1 - cos(alpha)) - t.at(0) * sin(alpha);
  RotMat(2, 0) = t.at(2) * t.at(0) * (1 - cos(alpha)) - t.at(1) * sin(alpha);
  RotMat(2, 1) = t.at(2) * t.at(1) * (1 - cos(alpha)) + t.at(0) * sin(alpha);

  // rotation
  std::vector<double> nrot(3, 0.0);
  for (int j=0; j<3; j++)
    for (int k=0; k<3; k++)
      nrot.at(j) += RotMat(j, k) * n.at(k);

  // calculation of the third point lying in the direction of the rotated normal
  // height of the third point above the filament
  double h = 0.33 * (statmechparams_.get<double> ("R_LINK", 0.0) + statmechparams_.get<double> ("DeltaR_LINK", 0.0));
  for (int j=0; j<3; j++)
    thirdpoint.at(j) = (coord(j, 0) + coord(j, element->NumNode() - 1)) / 2.0 + h * nrot.at(j);

  gmshfilecontent << "SL(" << scientific << coord(0,0) << ","<< coord(1,0) << "," << coord(2,0) << ","
                  << thirdpoint.at(0) << ","<< thirdpoint.at(1) << "," << thirdpoint.at(2) << ")"
                  << "{" << scientific<< color << "," << color << "};" << endl;
  gmshfilecontent << "SL(" << scientific << thirdpoint.at(0) << "," << thirdpoint.at(1)<< "," << thirdpoint.at(2) << ","
                  << coord(0, 1) << "," << coord(1,1) << "," << coord(2,1) << ")"
                  << "{" << scientific<< color << "," << color << "};" << endl;
  gmshfilecontent << "SP(" << scientific
                  << thirdpoint.at(0) << "," << thirdpoint.at(1) << ","<< thirdpoint.at(2)
                  << ")" << "{" << scientific << color << ","<< color << "};" << endl;

  /*/ cout block
   cout<<"coord  = \n"<<coord<<endl;
   cout<<"ltan   = "<<ltan<<endl;
   cout<<"t      = [ "<<t.at(0)<<" "<<t.at(1)<<" "<<t.at(2)<<" ]"<<endl;
   cout<<"lnorm  = "<<lnorm<<endl;
   cout<<"n      = [ "<<n.at(0)<<" "<<n.at(1)<<" "<<n.at(2)<<" ]"<<endl;
   cout<<"alpha  = "<<alpha<<endl;
   cout<<"RotMat =\n"<<RotMat<<endl;
   cout<<"nrot   = [ "<<nrot.at(0)<<" "<<nrot.at(1)<<" "<<nrot.at(2)<<" ]"<<endl;
   cout<<"thirdp = [ "<<thirdpoint.at(0)<<" "<<thirdpoint.at(1)<<" "<<thirdpoint.at(2)<<" ]\n\n\n"<<endl;*/
  return;
}// StatMechManager::GmshKinkedVisual

/*----------------------------------------------------------------------*
 | prepare visualization vector for Gmsh Output  (private) mueller 08/10|
 *----------------------------------------------------------------------*/
void StatMechManager::GmshPrepareVisualization(const Epetra_Vector& dis)
{
  double ronebond = statmechparams_.get<double> ("R_LINK", 0.0) / 2.0;

  // column map displacement and displacement increment

  Epetra_Vector discol(*(discret_.DofColMap()), true);
  LINALG::Export(dis, discol);

  if (discret_.Comm().MyPID() == 0)
  {
    for (int i=0; i<numbond_->MyLength(); i++)
    {
      switch((int)(*numbond_)[i])
      {
        // diffusion
        case 0:
        {
          for (int j=0; j<visualizepositions_->NumVectors(); j++)
            (*visualizepositions_)[j][i] = (*crosslinkerpositions_)[j][i];
        }
        break;
        // one bonded crosslink molecule
        case 1:
        {
          // determine position of nodeGID entry
          int occupied = -1;
          for (int j=0; j<crosslinkerbond_->NumVectors(); j++)
            if ((*crosslinkerbond_)[j][i] > -0.9)
            {
              occupied = j;
              break;
            }

          int nodeLID = discret_.NodeColMap()->LID((int)(*crosslinkerbond_)[occupied][i]);
          int currfilament = (int)(*filamentnumber_)[nodeLID];
          const DRT::Node *node0 = discret_.lColNode(nodeLID);
          // choose a second (neighbour) node
          const DRT::Node *node1 = NULL;
          if(nodeLID < basisnodes_-1)
            if((*filamentnumber_)[nodeLID+1]==currfilament)
              node1 = discret_.lColNode(nodeLID+1);
            else
              node1 = discret_.lColNode(nodeLID-1);
          if(nodeLID == basisnodes_-1)
            if((*filamentnumber_)[nodeLID-1]==currfilament)
              node1 = discret_.lColNode(nodeLID-1);

          //calculate unit tangent
          LINALG::Matrix<3,1> nodepos0;
          LINALG::Matrix<3,1> tangent;

          for (int j=0; j<3; j++)
          {
            int dofgid0 = discret_.Dof(node0)[j];
            int dofgid1 = discret_.Dof(node1)[j];
            nodepos0(j,0) = node0->X()[j] + discol[discret_.DofColMap()->LID(dofgid0)];
            double nodeposj1 = node1->X()[j] + discol[discret_.DofColMap()->LID(dofgid1)];
            tangent(j) = nodeposj1 - nodepos0(j,0);
          }
          tangent.Scale(1 / tangent.Norm2());

          // calculate normal via cross product: [0 0 1]x[tx ty tz]
          LINALG::Matrix<3, 1> normal;
          normal(0) = -tangent(1);
          normal(1) = tangent(0);
          // norm it since the cross product does not keep the length
          normal.Scale(1 / normal.Norm2());

          // obtain angle
          // random angle
          // by modulo operation involving the crosslink molecule number
          double alpha = fmod((double) i, 2*M_PI);

          // rotate the normal by alpha
          LINALG::Matrix<3, 3> RotMat;
          // build the matrix of rotation
          for (int j=0; j<3; j++)
            RotMat(j, j) = cos(alpha) + tangent(j) * tangent(j) * (1 - cos(alpha));
          RotMat(0, 1) = tangent(0) * tangent(1) * (1 - cos(alpha)) - tangent(2) * sin(alpha);
          RotMat(0, 2) = tangent(0) * tangent(2) * (1 - cos(alpha)) + tangent(1) * sin(alpha);
          RotMat(1, 0) = tangent(1) * tangent(0) * (1 - cos(alpha)) + tangent(2) * sin(alpha);
          RotMat(1, 2) = tangent(1) * tangent(2) * (1 - cos(alpha)) - tangent(0) * sin(alpha);
          RotMat(2, 0) = tangent(2) * tangent(0) * (1 - cos(alpha)) - tangent(1) * sin(alpha);
          RotMat(2, 1) = tangent(2) * tangent(1) * (1 - cos(alpha)) + tangent(0) * sin(alpha);

          // rotation
          LINALG::Matrix<3, 1> rotnormal;
          rotnormal.Multiply(RotMat, normal);

          // calculation of the visualized point lying in the direction of the rotated normal
          for (int j=0; j<visualizepositions_->NumVectors(); j++)
            (*visualizepositions_)[j][i] = nodepos0(j,0) + ronebond*rotnormal(j,0);
        }
        break;
        case 2:
        {
          // actual crosslinker element (not kinked)
          if((*searchforneighbours_)[i]>0.9)
          {
            // loop over filament node components
            for (int j=0; j<visualizepositions_->NumVectors(); j++)
            {
              std::vector<double> dofnodepositions(crosslinkerbond_->NumVectors(), 0.0);
              // loop over filament node GIDs
              for (int k=0; k<crosslinkerbond_->NumVectors(); k++)
              {
                int nodeGID = (int) (*crosslinkerbond_)[k][i];
                DRT::Node *node = discret_.lColNode(discret_.NodeColMap()->LID(nodeGID));
                int dofgid = discret_.Dof(node)[j];
                dofnodepositions.at(k) = node->X()[j] + discol[dofgid];
              }
              /* Check if the crosslinker element is broken/discontinuous; if so, reposition the second nodal value.
               * It does not matter which value is shifted as long the shift occurs in a consistent way.*/
              if (statmechparams_.get<double> ("PeriodLength", 0.0) > 0.0)
              {
                (*visualizepositions_)[j][i] = dofnodepositions.at(0);
                for (int k=0; k<1; k++)
                {
                  // shift position if it is found to be outside the boundary box
                  if (fabs(dofnodepositions.at(k+1) - statmechparams_.get<double> ("PeriodLength", 0.0) - dofnodepositions.at(k))< fabs(dofnodepositions.at(k+1) - dofnodepositions.at(k)))
                    dofnodepositions.at(k+1) -= statmechparams_.get<double> ("PeriodLength", 0.0);
                  if (fabs(dofnodepositions.at(k+1) + statmechparams_.get<double> ("PeriodLength", 0.0) - dofnodepositions.at(k))< fabs(dofnodepositions.at(k+1) - dofnodepositions.at(k)))
                    dofnodepositions.at(k+1) += statmechparams_.get<double> ("PeriodLength", 0.0);

                  (*visualizepositions_)[j][i] += dofnodepositions.at(k+1);
                }
                // new crosslink molecule position
                (*visualizepositions_)[j][i] /= 2.0;
              }
              else
                (*visualizepositions_)[j][i] /= 2.0;
            }
          }
          else  // passive crosslink molecule
          {
            // determine position of nodeGID entry
            int occupied = -1;
            for (int j=0; j<crosslinkerbond_->NumVectors(); j++)
              if ((*crosslinkerbond_)[j][i] > -0.9)
              {
                occupied = j;
                break;
              }

            int nodeLID = discret_.NodeColMap()->LID((int)(*crosslinkerbond_)[occupied][i]);
            int currfilament = (int)(*filamentnumber_)[nodeLID];
            const DRT::Node *node0 = discret_.lColNode(nodeLID);
            // choose a second (neighbour) node
            const DRT::Node *node1 = NULL;
            if(nodeLID < basisnodes_-1)
              if((*filamentnumber_)[nodeLID+1]==currfilament)
                node1 = discret_.lColNode(nodeLID+1);
              else
                node1 = discret_.lColNode(nodeLID-1);
            if(nodeLID == basisnodes_-1)
              if((*filamentnumber_)[nodeLID-1]==currfilament)
                node1 = discret_.lColNode(nodeLID-1);

            //calculate unit tangent
            LINALG::Matrix<3,1> nodepos0;
            LINALG::Matrix<3,1> tangent;

            for (int j=0; j<3; j++)
            {
              int dofgid0 = discret_.Dof(node0)[j];
              int dofgid1 = discret_.Dof(node1)[j];
              nodepos0(j,0) = node0->X()[j] + discol[discret_.DofColMap()->LID(dofgid0)];
              double nodeposj1 = node1->X()[j] + discol[discret_.DofColMap()->LID(dofgid1)];
              tangent(j) = nodeposj1 - nodepos0(j,0);
            }
            tangent.Scale(1 / tangent.Norm2());

            // calculate normal via cross product: [0 0 1]x[tx ty tz]
            LINALG::Matrix<3, 1> normal;
            normal(0) = -tangent(1);
            normal(1) = tangent(0);
            // norm it since the cross product does not keep the length
            normal.Scale(1 / normal.Norm2());

            // obtain angle
            // random angle
            // by modulo operation involving the crosslink molecule number
            double alpha = fmod((double) i, 2*M_PI);

            // rotate the normal by alpha
            LINALG::Matrix<3, 3> RotMat;
            // build the matrix of rotation
            for (int j=0; j<3; j++)
              RotMat(j, j) = cos(alpha) + tangent(j) * tangent(j) * (1 - cos(alpha));
            RotMat(0, 1) = tangent(0) * tangent(1) * (1 - cos(alpha)) - tangent(2) * sin(alpha);
            RotMat(0, 2) = tangent(0) * tangent(2) * (1 - cos(alpha)) + tangent(1) * sin(alpha);
            RotMat(1, 0) = tangent(1) * tangent(0) * (1 - cos(alpha)) + tangent(2) * sin(alpha);
            RotMat(1, 2) = tangent(1) * tangent(2) * (1 - cos(alpha)) - tangent(0) * sin(alpha);
            RotMat(2, 0) = tangent(2) * tangent(0) * (1 - cos(alpha)) - tangent(1) * sin(alpha);
            RotMat(2, 1) = tangent(2) * tangent(1) * (1 - cos(alpha)) + tangent(0) * sin(alpha);

            // rotation
            LINALG::Matrix<3, 1> rotnormal;
            rotnormal.Multiply(RotMat, normal);

            // calculation of the visualized point lying in the direction of the rotated normal
            for (int j=0; j<visualizepositions_->NumVectors(); j++)
              (*visualizepositions_)[j][i] = nodepos0(j,0) + ronebond*rotnormal(j,0);
          }
        }
        break;
      }
    }
    if (statmechparams_.get<double>("PeriodLength", 0.0) > 0.0)
      CrosslinkerPeriodicBoundaryShift(*visualizepositions_);
  }
  else
  {
    visualizepositions_->PutScalar(0.0);
  }
  // synchronize results
  Epetra_Export crosslinkexporter(*crosslinkermap_, *transfermap_);
  Epetra_Import crosslinkimporter(*crosslinkermap_, *transfermap_);
  Epetra_MultiVector visualizepositionstrans(*transfermap_, 3, true);
  visualizepositionstrans.Export(*visualizepositions_, crosslinkexporter, Add);
  visualizepositions_->Import(visualizepositionstrans, crosslinkimporter, Insert);
  // debug couts
  //cout<<*visualizepositions_<<endl;
}//GmshPrepareVisualization

/*----------------------------------------------------------------------*
 | wedge output for two-noded beams                        cyron   11/10|
 *----------------------------------------------------------------------*/
void StatMechManager::GMSH_2_noded(const int& n,
                                  const Epetra_SerialDenseMatrix& coord,
                                  DRT::Element* thisele,
                                  std::stringstream& gmshfilecontent,
                                  const double color,
                                  bool ignoreeleid)
{
  //if this element is a line element capable of providing its radius get that radius
  double radius = 0;

  const DRT::ElementType & eot = thisele->ElementType();
#ifdef D_BEAM3II
#ifdef D_BEAM3
  if(eot == DRT::ELEMENTS::Beam3Type::Instance())
    radius = sqrt(sqrt(4 * ((dynamic_cast<DRT::ELEMENTS::Beam3*>(thisele))->Izz()) / M_PI));
  else if(eot == DRT::ELEMENTS::Beam3iiType::Instance())
    radius = sqrt(sqrt(4 * ((dynamic_cast<DRT::ELEMENTS::Beam3ii*>(thisele))->Izz()) / M_PI));
  else
    dserror("thisele is not a line element providing its radius.");
#endif
#endif

  //line elements are plotted by a factor PlotFactorThick thicker than they are actually to allow for better visibility in gmsh pictures
  radius *= statmechparams_.get<double>("PlotFactorThick", 1.0);

  //solid lines plotted if PlotFactorThick != 0; otherwise output utilizes gmsh line visualization (lines width with fixed number of pixels, not a physical diameter)
  if(radius > 0.0)
  {

    // some local variables
    LINALG::Matrix<3,6> prism;
    LINALG::Matrix<3,1> axis;
    LINALG::Matrix<3,1> radiusvec1;
    LINALG::Matrix<3,1> radiusvec2;
    LINALG::Matrix<3,1> auxvec;
    LINALG::Matrix<3,1> theta;
    LINALG::Matrix<3,3> R;


    // compute three dimensional angle theta
    for (int j=0;j<3;++j)
      axis(j) = coord(j,1) - coord(j,0);
    double norm_axis = axis.Norm2();
    for (int j=0;j<3;++j)
      theta(j) = axis(j) / norm_axis * 2 * M_PI / n;

    // Compute rotation matirx R from rotation angle theta
    angletotriad(theta,R);

    // Now the first prism will be computed via two radiusvectors, that point from each of
    // the nodes to two points on the beam surface. Further prisms will be computed via a
    // for-loop, where the second node of the previous prism is used as the first node of the
    // next prism, whereas the central points (=nodes) stay  identic for each prism. The
    // second node will be computed by a rotation matrix and a radiusvector.

    // compute radius vector for first surface node of first prims
    for (int j=0;j<3;++j) auxvec(j) = coord(j,0) + norm_axis;

    // radiusvector for point on surface
    radiusvec1(0) = auxvec(1)*axis(2) - auxvec(2)*axis(1);
    radiusvec1(1) = auxvec(2)*axis(0) - auxvec(0)*axis(2);
    radiusvec1(2) = auxvec(0)*axis(1) - auxvec(1)*axis(0);

    // initialize all prism points to nodes
    for (int j=0;j<3;++j)
    {
      prism(j,0) = coord(j,0);
      prism(j,1) = coord(j,0);
      prism(j,2) = coord(j,0);
      prism(j,3) = coord(j,1);
      prism(j,4) = coord(j,1);
      prism(j,5) = coord(j,1);
    }

    // get first point on surface for node1 and node2
    for (int j=0;j<3;++j)
    {
      prism(j,1) += radiusvec1(j) / radiusvec1.Norm2() * radius;
      prism(j,4) += radiusvec1(j) / radiusvec1.Norm2() * radius;
    }

    // compute radiusvec2 by rotating radiusvec1 with rotation matrix R
    radiusvec2.Multiply(R,radiusvec1);

    // get second point on surface for node1 and node2
    for(int j=0;j<3;j++)
    {
      prism(j,2) += radiusvec2(j) / radiusvec2.Norm2() * radius;
      prism(j,5) += radiusvec2(j) / radiusvec2.Norm2() * radius;
    }

    // now first prism is built -> put coordinates into filecontent-stream
    // Syntax for gmsh input file
    // SI(x,y--,z,  x+.5,y,z,    x,y+.5,z,   x,y,z+.5, x+.5,y,z+.5, x,y+.5,z+.5){1,2,3,3,2,1};
    // SI( coordinates of the six corners ){colors}
    gmshfilecontent << "SI("<<scientific;
    gmshfilecontent << prism(0,0) << "," << prism(1,0) << "," << prism(2,0) << ",";
    gmshfilecontent << prism(0,1) << "," << prism(1,1) << "," << prism(2,1) << ",";
    gmshfilecontent << prism(0,2) << "," << prism(1,2) << "," << prism(2,2) << ",";
    gmshfilecontent << prism(0,3) << "," << prism(1,3) << "," << prism(2,3) << ",";
    gmshfilecontent << prism(0,4) << "," << prism(1,4) << "," << prism(2,4) << ",";
    gmshfilecontent << prism(0,5) << "," << prism(1,5) << "," << prism(2,5);
    gmshfilecontent << "){" << scientific;
    gmshfilecontent << color << "," << color << "," << color << "," << color << "," << color << "," << color << "};" << endl;

    // now the other prisms will be computed
    for (int sector=0;sector<n-1;++sector)
    {
      // initialize for next prism
      // some nodes of last prims can be taken also for the new next prism
      for (int j=0;j<3;++j)
      {
        prism(j,1)=prism(j,2);
        prism(j,4)=prism(j,5);
        prism(j,2)=prism(j,0);
        prism(j,5)=prism(j,3);
      }

      // old radiusvec2 is now radiusvec1; radiusvec2 is set to zero
      for (int j=0;j<3;++j)
      {
        radiusvec1(j) = radiusvec2(j);
        radiusvec2(j) = 0.0;
      }

      // compute radiusvec2 by rotating radiusvec1 with rotation matrix R
      radiusvec2.Multiply(R,radiusvec1);


      // get second point on surface for node1 and node2
      for (int j=0;j<3;++j)
      {
        prism(j,2) += radiusvec2(j) / radiusvec2.Norm2() * radius;
        prism(j,5) += radiusvec2(j) / radiusvec2.Norm2() * radius;
      }

      // put coordinates into filecontent-stream
      // Syntax for gmsh input file
      // SI(x,y--,z,  x+.5,y,z,    x,y+.5,z,   x,y,z+.5, x+.5,y,z+.5, x,y+.5,z+.5){1,2,3,3,2,1};
      // SI( coordinates of the six corners ){colors}
      gmshfilecontent << "SI("<<scientific;
      gmshfilecontent << prism(0,0) << "," << prism(1,0) << "," << prism(2,0) << ",";
      gmshfilecontent << prism(0,1) << "," << prism(1,1) << "," << prism(2,1) << ",";
      gmshfilecontent << prism(0,2) << "," << prism(1,2) << "," << prism(2,2) << ",";
      gmshfilecontent << prism(0,3) << "," << prism(1,3) << "," << prism(2,3) << ",";
      gmshfilecontent << prism(0,4) << "," << prism(1,4) << "," << prism(2,4) << ",";
      gmshfilecontent << prism(0,5) << "," << prism(1,5) << "," << prism(2,5);
      gmshfilecontent << "){" << scientific;
      gmshfilecontent << color << "," << color << "," << color << "," << color << "," << color << "," << color << "};" << endl;
    }
  }
  //no thickness >0 specified; plot elements as line segements without physical volume
  else
  {
    gmshfilecontent << "SL(" << scientific;
    gmshfilecontent << coord(0,0) << "," << coord(1,0) << "," << coord(2,0) << ","
                    << coord(0,1) << "," << coord(1,1) << "," << coord(2,1);
    gmshfilecontent << ")" << "{" << scientific << color << ","<< color << "};" << endl;
    /*/ crosslink molecules are marked with an additional small ball if they are plotted as volumeless lines
    if(ignoreeleid)
    {
      double beadcolor = color;
      gmshfilecontent << "SP(" << scientific;
      gmshfilecontent << coord(0,1) << "," << coord(1,1) << ","<< coord(2,1);
      gmshfilecontent << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
    }*/
  }
  // crosslink molecules are marked with an additional small ball if they are plotted as volumeless lines
  if(ignoreeleid)
  {
    double beadcolor = color;
    gmshfilecontent << "SP(" << scientific;
    gmshfilecontent << coord(0,1) << "," << coord(1,1) << ","<< coord(2,1);
    gmshfilecontent << ")" << "{" << scientific << beadcolor << ","<< beadcolor << "};" << endl;
  }
  return;
}//GMSH_2_noded


/*----------------------------------------------------------------------*
 | initialize special output for statistical mechanics(public)cyron 12/08|
 *----------------------------------------------------------------------*/
void StatMechManager::InitOutput(const int ndim, const double& dt)
{
  //initializing special output for statistical mechanics by looking for a suitable name of the outputfile and setting up an empty file with this name

  switch (Teuchos::getIntegralValue<INPAR::STATMECH::StatOutput>(statmechparams_, "SPECIAL_OUTPUT"))
  {
    case INPAR::STATMECH::statout_endtoendlog:
    {

      //output is written on proc 0 only
      if(!discret_.Comm().MyPID())
      {

        FILE* fp = NULL; //file pointer for statistical output file

        //defining name of output file
        std::ostringstream outputfilename;

        outputfilenumber_ = 0;

        //file pointer for operating with numbering file
        FILE* fpnumbering = NULL;
        std::ostringstream numberingfilename;

        //look for a numbering file where number of already existing output files is stored:
        numberingfilename.str("NumberOfRealizationsLog");
        fpnumbering = fopen(numberingfilename.str().c_str(), "r");

        //if there is no such numbering file: look for a not yet existing output file name (numbering upwards)
        if (fpnumbering == NULL)
        {
          do
          {
            outputfilenumber_++;
            outputfilename.str("");
            outputfilename << "EndToEnd" << outputfilenumber_ << ".dat";
            fp = fopen(outputfilename.str().c_str(), "r");
          }
          while (fp != NULL);

          //set up new file with name "outputfilename" without writing anything into this file
          fp = fopen(outputfilename.str().c_str(), "w");
          fclose(fp);
        }
        //if there already exists a numbering file
        else
        {
          fclose(fpnumbering);

          //read the number of the next realization out of the file into the variable testnumber
          std::fstream f(numberingfilename.str().c_str());
          while (f)
          {
            std::string tok;
            f >> tok;
            if (tok == "Next")
            {
              f >> tok;
              if (tok == "Number:")
                f >> outputfilenumber_;
            }
          } //while(f)

          //defining outputfilename by means of new testnumber
          outputfilename.str("");
          outputfilename << "EndToEnd" << outputfilenumber_ << ".dat";
        }

        //increasing the number in the numbering file by one
        fpnumbering = fopen(numberingfilename.str().c_str(), "w");
        std::stringstream filecontent;
        filecontent << "Next Number: " << (outputfilenumber_ + 1);
        fprintf(fpnumbering, filecontent.str().c_str());
        fclose(fpnumbering);
      }

    }
      break;
    case INPAR::STATMECH::statout_endtoendconst:
    {
      //output is written on proc 0 only
      if(!discret_.Comm().MyPID())
      {

        FILE* fp = NULL; //file pointer for statistical output file

        //defining name of output file
        std::ostringstream outputfilename;
        outputfilename.str("");
        outputfilenumber_ = 0;

        //file pointer for operating with numbering file
        FILE* fpnumbering = NULL;
        std::ostringstream numberingfilename;

        //look for a numbering file where number of already existing output files is stored:
        numberingfilename.str("NumberOfRealizationsConst");
        fpnumbering = fopen(numberingfilename.str().c_str(), "r");

        /*in the following we assume that there is only a pulling force point Neumann condition of equal absolute
         *value on either filament length; we get the absolute value of the first one of these two conditions */
        double neumannforce;
        vector<DRT::Condition*> pointneumannconditions(0);
        discret_.GetCondition("PointNeumann", pointneumannconditions);
        if (pointneumannconditions.size() > 0)
        {
          const vector<double>* val = pointneumannconditions[0]->Get<vector<double> > ("val");
          neumannforce = fabs((*val)[0]);
        }
        else
          neumannforce = 0;

        //if there is no such numbering file: look for a not yet existing output file name (numbering upwards)
        if (fpnumbering == NULL)
        {
          do
          {
            //defining name of output file
            outputfilenumber_++;
            outputfilename.str("");
            outputfilename << "E2E_" << discret_.NumMyRowElements() << '_' << dt<< '_' << neumannforce << '_' << outputfilenumber_ << ".dat";
            fp = fopen(outputfilename.str().c_str(), "r");
          }
          while (fp != NULL);

          //set up new file with name "outputfilename" without writing anything into this file
          fp = fopen(outputfilename.str().c_str(), "w");
          fclose(fp);
        }
        //if there already exists a numbering file
        else
        {
          fclose(fpnumbering);

          //read the number of the next realization out of the file into the variable testnumber
          std::fstream f(numberingfilename.str().c_str());
          while (f)
          {
            std::string tok;
            f >> tok;
            if (tok == "Next")
            {
              f >> tok;
              if (tok == "Number:")
                f >> outputfilenumber_;
            }
          } //while(f)

          //defining outputfilename by means of new testnumber
          outputfilename.str("");
          outputfilename << "E2E_" << discret_.NumMyRowElements() << '_' << dt << '_' << neumannforce << '_' << outputfilenumber_ << ".dat";

          //set up new file with name "outputfilename" without writing anything into this file
          fp = fopen(outputfilename.str().c_str(), "w");
          fclose(fp);
        }

        //increasing the number in the numbering file by one
        fpnumbering = fopen(numberingfilename.str().c_str(), "w");
        std::stringstream filecontent;
        filecontent << "Next Number: " << (outputfilenumber_ + 1);
        //write fileconent into file!
        fprintf(fpnumbering, filecontent.str().c_str());
        //close file
        fclose(fpnumbering);
      }

    }
    break;
    /*computing and writing into file data about correlation of orientation of different elements
     *as it is considered in the context of the persistence length*/
    case INPAR::STATMECH::statout_orientationcorrelation:
    {
      //output is written on proc 0 only
      if(!discret_.Comm().MyPID())
      {

        FILE* fp = NULL; //file pointer for statistical output file

        //defining name of output file
        std::ostringstream outputfilename;

        outputfilenumber_ = 0;

        //file pointer for operating with numbering file
        FILE* fpnumbering = NULL;
        std::ostringstream numberingfilename;

        //look for a numbering file where number of already existing output files is stored:
        numberingfilename.str("NumberOfRealizationsOrientCorr");
        fpnumbering = fopen(numberingfilename.str().c_str(), "r");

        //if there is no such numbering file: look for a not yet existing output file name (numbering upwards)
        if (fpnumbering == NULL)
        {
          do
          {
            outputfilenumber_++;
            outputfilename.str("");
            outputfilename << "OrientationCorrelation" << outputfilenumber_<< ".dat";
            fp = fopen(outputfilename.str().c_str(), "r");
          }
          while (fp != NULL);

          //set up new file with name "outputfilename" without writing anything into this file
          fp = fopen(outputfilename.str().c_str(), "w");
          fclose(fp);
        }
        //if there already exists a numbering file
        else
        {
          fclose(fpnumbering);

          //read the number of the next realization out of the file into the variable testnumber
          std::fstream f(numberingfilename.str().c_str());
          while (f)
          {
            std::string tok;
            f >> tok;
            if (tok == "Next")
            {
              f >> tok;
              if (tok == "Number:")
                f >> outputfilenumber_;
            }
          } //while(f)

          //defining outputfilename by means of new testnumber
          outputfilename.str("");
          outputfilename << "OrientationCorrelation" << outputfilenumber_<< ".dat";
        }

        //set up new file with name "outputfilename" without writing anything into this file
        fp = fopen(outputfilename.str().c_str(), "w");
        fclose(fp);

        //increasing the number in the numbering file by one
        fpnumbering = fopen(numberingfilename.str().c_str(), "w");
        std::stringstream filecontent;
        filecontent << "Next Number: " << (outputfilenumber_ + 1);
        //write fileconent into file!
        fprintf(fpnumbering, filecontent.str().c_str());
        //close file
        fclose(fpnumbering);
      }
    }
    break;
    //simulating diffusion coefficient for anisotropic friction
    case INPAR::STATMECH::statout_anisotropic:
    {
      //output is written on proc 0 only
      if(!discret_.Comm().MyPID())
      {

        FILE* fp = NULL; //file pointer for statistical output file

        //defining name of output file
        std::ostringstream outputfilename;

        outputfilenumber_ = 0;

        //file pointer for operating with numbering file
        FILE* fpnumbering = NULL;
        std::ostringstream numberingfilename;

        //look for a numbering file where number of already existing output files is stored:
        numberingfilename.str("NumberOfRealizationsAniso");
        fpnumbering = fopen(numberingfilename.str().c_str(), "r");

        //if there is no such numbering file: look for a not yet existing output file name (numbering upwards)
        if (fpnumbering == NULL)
        {
          do
          {
            outputfilenumber_++;
            outputfilename.str("");
            outputfilename << "AnisotropicDiffusion" << outputfilenumber_
                << ".dat";
            fp = fopen(outputfilename.str().c_str(), "r");
          }
          while (fp != NULL);

          //set up new file with name "outputfilename" without writing anything into this file
          fp = fopen(outputfilename.str().c_str(), "w");
          fclose(fp);
        }
        //if there already exists a numbering file
        else
        {
          fclose(fpnumbering);

          //read the number of the next realization out of the file into the variable testnumber
          std::fstream f(numberingfilename.str().c_str());
          while (f)
          {
            std::string tok;
            f >> tok;
            if (tok == "Next")
            {
              f >> tok;
              if (tok == "Number:")
                f >> outputfilenumber_;
            }
          } //while(f)

          //defining outputfilename by means of new testnumber
          outputfilename.str("");
          outputfilename << "AnisotropicDiffusion" << outputfilenumber_ << ".dat";
        }

        //set up new file with name "outputfilename" without writing anything into this file
        fp = fopen(outputfilename.str().c_str(), "w");
        fclose(fp);

        //increasing the number in the numbering file by one
        fpnumbering = fopen(numberingfilename.str().c_str(), "w");
        std::stringstream filecontent;
        filecontent << "Next Number: " << (outputfilenumber_ + 1);
        //write fileconent into file!
        fprintf(fpnumbering, filecontent.str().c_str());
        //close file
        fclose(fpnumbering);

        //initializing variables for positions of first and last node at the beginning
        beginold_.PutScalar(0);
        endold_.PutScalar(0);
        for (int i=0; i<ndim; i++)
        {
          beginold_(i) = (discret_.gNode(0))->X()[i];
          endold_(i) = (discret_.gNode(discret_.NumMyRowNodes() - 1))->X()[i];
        }

        for (int i=0; i<3; i++)
          sumdispmiddle_(i, 0) = 0.0;

        sumsquareincpar_ = 0.0;
        sumsquareincort_ = 0.0;
        sumrotmiddle_ = 0.0;
        sumsquareincmid_ = 0.0;
        sumsquareincrot_ = 0.0;
      }
    }
    break;
    case INPAR::STATMECH::statout_viscoelasticity:
    {
      if(!discret_.Comm().MyPID())
      {
        //pointer to file into which each processor writes the output related with the dof of which it is the row map owner
        FILE* fp = NULL;

        //content to be written into the output file
        std::stringstream filecontent;

        //defining name of output file related to processor Id
        std::ostringstream outputfilename;
        outputfilename.str("");
        outputfilename << "ViscoElOutputProc.dat";

        fp = fopen(outputfilename.str().c_str(), "w");

        //filecontent << "Output for measurement of viscoelastic properties written by processor "<< discret_.Comm().MyPID() << endl;

        // move temporary stringstream to file and close it
        fprintf(fp, filecontent.str().c_str());
        fclose(fp);
      }
    }
      break;
    case INPAR::STATMECH::statout_none:
    default:
      break;
  }

  return;
} // StatMechManager::InitOutput()

/*----------------------------------------------------------------------*
 | output for density-density-correlation-function(public) mueller 07/10|
 *----------------------------------------------------------------------*/
void StatMechManager::DDCorrOutput(const Epetra_Vector& disrow, const std::ostringstream& filename, const int& istep)
{
	// Output: crosslinkers per bin, spherical coordinates (sorted into bins as well)

	/*for(int i=0; i<discret_.Comm().NumProc(); i++)
	{
		if(i==discret_.Comm().MyPID())
			cout<<"Proc "<<discret_.Comm().MyPID()<<": Pos1"<<endl;
		discret_.Comm().Barrier();
	}*/
  int numbins = statmechparams_.get<int>("HISTOGRAMBINS", 1);
	double periodlength = statmechparams_.get<double>("PeriodLength", 0.0);
	// storage vector for shifted crosslinker LIDs(crosslinkermap)
	std::vector<double> boxcenter(3,0.0);
	std::vector<double> centershift(3,0.0);
	std::vector<int> crosslinkerentries;

	// determine the new center point of the periodic volume
	DDCorrShift(&boxcenter, &centershift, &crosslinkerentries);

	//calculcate the distance of crosslinker elements to all crosslinker elements (crosslinkermap)
	Epetra_Vector crosslinksperbinrow(*ddcorrrowmap_, true);
  // number of overall crosslink molecules
	int ncrosslink = statmechparams_.get<int>("N_crosslink", 0);
  // number of overall independent combinations
  int numcombinations = (ncrosslink*ncrosslink-ncrosslink)/2;
  // combinations on each processor
  int combinationsperproc = (int)floor((double)numcombinations/(double)discret_.Comm().NumProc());
  int remainder = numcombinations%combinationsperproc;
  int combicount = 0;

  // loop over crosslinkermap_ (column map, same for all procs: maps all crosslink molecules)
  // obtain crosslinker-crosslinker distances and sort them into histogram bins
  for(int mypid=0; mypid<discret_.Comm().NumProc(); mypid++)
  {
    bool quitloop = false;
    if(mypid==discret_.Comm().MyPID())
    {
      bool continueloop = false;
      int appendix = 0;
      if(mypid==discret_.Comm().NumProc()-1)
        appendix = remainder;

      for(int i=0; i<crosslinkermap_->NumMyElements(); i++)
      {
        for(int j=0; j<crosslinkermap_->NumMyElements(); j++)
        {
          // start adding crosslink from here
          if(i==(*startindex_)[2*mypid] && j==(*startindex_)[2*mypid+1])
            continueloop = true;

          // only entries above main diagonal and within limits of designated number of crosslink molecules per processor
          if(j>i && continueloop)
          	if(combicount<combinationsperproc+appendix)
						{
							combicount++;
							// only do calculate deltaxij if the following if both indices i and j stand for crosslinker elements
							if((*crosslinkerbond_)[0][i]>-0.9 && (*crosslinkerbond_)[1][i]>-0.9 && (*crosslinkerbond_)[0][j]>-0.9 && (*crosslinkerbond_)[1][j]>-0.9)
							{
								double deltaxij = 0.0;
								std::vector<int> indices(2,0);
								LINALG::Matrix<3,2> currpositions;

								indices[0] = i;
								indices[1] = j;

								// shift it according to new boundary box
								for(int m=0; m<(int)currpositions.M(); m++)
								{
									for(int n=0; n<(int)currpositions.N(); n++)
									{
										currpositions(m,n) = (*crosslinkerpositions_)[m][indices[n]];
										if (currpositions(m,n) > periodlength+centershift[m])
											currpositions(m,n) -= periodlength;
										if (currpositions(m,n) < 0.0+centershift[m])
											currpositions(m,n) += periodlength;
									}
									deltaxij += (currpositions(m,1)-currpositions(m,0)) * (currpositions(m,1)-currpositions(m,0));
								}
								deltaxij = sqrt(deltaxij);

								// calculate the actual bin to which the current distance belongs and increment the count for that bin (currbin=LID)
								int currbin = (int)floor(deltaxij/(periodlength*sqrt(3.0))*numbins);
								// in case the distance is exactly periodlength*sqrt(3)
								if(currbin==numbins)
									currbin--;
								crosslinksperbinrow[currbin] += 1.0;
							}
						}
						else
						{
							quitloop = true;
							break;
						}
        }
        if(quitloop)
          break;
      }
      if(quitloop)
        break;
    }
  }

  // calculation of filament element orientation in sperical coordinates, sorted into histogram
  Epetra_Vector phibinsrow(*ddcorrrowmap_, true);
  Epetra_Vector thetabinsrow(*ddcorrrowmap_, true);
  Epetra_Vector costhetabinsrow(*ddcorrrowmap_, true);

  for(int i=0; i<discret_.NumMyRowElements(); i++)
  {
  	DRT::Element* element = discret_.lRowElement(i);
  	// consider filament elements only
  	if(element->Id()<basisnodes_)
  	{
  		int gid0 = element->Nodes()[0]->Id();
  		int gid1 = element->Nodes()[1]->Id();
      int lid0 = discret_.NodeRowMap()->LID(gid0);
      int lid1 = discret_.NodeRowMap()->LID(gid1);
      DRT::Node* node0 = discret_.lRowNode(lid0);
      DRT::Node* node1 = discret_.lRowNode(lid1);

      // calculate directional vector between nodes
      LINALG::Matrix<3, 1> dirvec;
      for(int dof=0; dof<3; dof++)
      {
        int dofgid0 = discret_.Dof(node0)[dof];
        int dofgid1 = discret_.Dof(node1)[dof];
        double poscomponent0 = node0->X()[dof]+disrow[discret_.DofRowMap()->LID(dofgid0)];
        double poscomponent1 = node1->X()[dof]+disrow[discret_.DofRowMap()->LID(dofgid1)];
        // check for periodic boundary shift and correct accordingly
        if (fabs(poscomponent1 - periodlength - poscomponent0) < fabs(poscomponent1 - poscomponent0))
          poscomponent1 -= periodlength;
        else if (fabs(poscomponent1 + periodlength - poscomponent0) < fabs(poscomponent1 - poscomponent0))
          poscomponent1 += periodlength;

        dirvec(dof) = poscomponent1-poscomponent0;
      }
      // normed directional vector
      dirvec.Scale(1/dirvec.Norm2());

      // transform into spherical coordinates (phi E [-pi;pi], theta E [0; pi]) and sort into appropriate bin
      double phi = atan2(dirvec(1),dirvec(0)) + M_PI;
      double theta = acos(dirvec(2));

      int phibin = (int)floor(phi/(2*M_PI)*numbins);
      int thetabin = (int)floor(theta/M_PI*numbins);
      int costhetabin = (int)floor((cos(theta)+1.0)/2.0*numbins);
      if(phibin == numbins)
      	phibin--;
      if(thetabin == numbins)
      	thetabin--;
      if(costhetabin == numbins)
      	costhetabin--;
      if(phibin<0 || thetabin<0)
      	dserror("bin smaller zero");
      phibinsrow[phibin] += 1.0;
      thetabinsrow[thetabin] += 1.0;
      costhetabinsrow[costhetabin] += 1.0;
  	}
  }
  // Export
  Epetra_Vector crosslinksperbincol(*ddcorrcolmap_, true);
  Epetra_Vector phibinscol(*ddcorrcolmap_, true);
  Epetra_Vector thetabinscol(*ddcorrcolmap_, true);
  Epetra_Vector costhetabinscol(*ddcorrcolmap_, true);
  Epetra_Import importer(*ddcorrcolmap_, *ddcorrrowmap_);
  crosslinksperbincol.Import(crosslinksperbinrow, importer, Insert);
  phibinscol.Import(phibinsrow, importer, Insert);
  thetabinscol.Import(thetabinsrow, importer, Insert);
  costhetabinscol.Import(thetabinsrow, importer, Insert);

  // Add the processor-specific data up
  std::vector<int> crosslinksperbin(numbins, 0);
  std::vector<int> phibins(numbins, 0);
  std::vector<int> thetabins(numbins, 0);
  std::vector<int> costhetabins(numbins, 0);
  //int total = 0;
  for(int i=0; i<numbins; i++)
  {
    for(int pid=0; pid<discret_.Comm().NumProc(); pid++)
    {
      crosslinksperbin[i] += (int)crosslinksperbincol[pid*numbins+i];
      phibins[i] += (int)phibinscol[pid*numbins+i];
      thetabins[i] += (int)thetabinscol[pid*numbins+i];
      costhetabins[i] += (int)costhetabinscol[pid*numbins+i];
    }
  }

  // write data to file
  if(discret_.Comm().MyPID()==0)
  {
    FILE* fp = NULL;
    fp = fopen(filename.str().c_str(), "w");
    std::stringstream histogram;

    for(int i=0; i<numbins; i++)
      histogram<<i+1<<"    "<<crosslinksperbin[i]<<"    "<<phibins[i]<<"    "<<thetabins[i]<<"    "<<costhetabins[i]<<endl;
    //write content into file and close it
    fprintf(fp, histogram.str().c_str());
    fclose(fp);
  }
  // Determine current network structure
  DDCorrCurrentStructure(disrow, &centershift, &crosslinkerentries, istep, filename);
}////StatMechManager::DDCorrOutput()

/*------------------------------------------------------------------------------*
 | Selects raster point with the smallest average distance to all crosslinker   |
 | elements, makes it the new center of the boundary box and shifts crosslinker |
 | positions.                                                                   |
 |                                                        (public) mueller 11/10|
 *------------------------------------------------------------------------------*/
void StatMechManager::DDCorrShift(std::vector<double>* boxcenter, std::vector<double>* centershift, std::vector<int>* crosslinkerentries)
{
	int numrasterpoints = statmechparams_.get<int>("NUMRASTERPOINTS", 3);
	double periodlength = statmechparams_.get<double>("PeriodLength", 0.0);
	// smallest average distance among all the average distances between the rasterpoints and all crosslinker elements
	// (init with 2*pl,so that it is definitely overwritten by the first "real" value)
	double smallestdistance = 2*periodlength;

	//store crosslinker element position within crosslinkerbond to crosslinkerentries
	for(int i=0; i<crosslinkerbond_->MyLength(); i++)
		if((*crosslinkerbond_)[0][i]>-0.9 && (*crosslinkerbond_)[1][i]>-0.9)
			crosslinkerentries->push_back(i);

	int numcrossele = (int)crosslinkerentries->size();

	// determine the new center of the box
	if(numcrossele>0)
	{
		for(int i=0; i<numrasterpoints; i++)
			for(int j=0; j<numrasterpoints; j++)
				for(int k=0; k<numrasterpoints; k++)
				{
					double averagedistance = 0.0;
					std::vector<double> currentrasterpoint(3,0.0);
					std::vector<double> currentcentershift(3,0.0);

					// calculate current raster point
					currentrasterpoint[0] = i*periodlength/(numrasterpoints-1);
					currentrasterpoint[1] = j*periodlength/(numrasterpoints-1);
					currentrasterpoint[2] = k*periodlength/(numrasterpoints-1);

					//cout<<"currentrasterpoint:   "<<currentrasterpoint[0]<<", "<<currentrasterpoint[1]<<", "<<currentrasterpoint[2]<<endl;

					// calculate the center shift (difference vector between regular center and new center of the boundary box)
					for(int l=0; l<(int)currentrasterpoint.size(); l++)
						currentcentershift[l] = currentrasterpoint[l]-periodlength/2.0;

					// calculate average distance of crosslinker elements to raster point
					for(int l=0; l<numcrossele; l++)
					{
						// get the crosslinker position in question and shift it according to new boundary box center
						std::vector<double> currcrosslinkerpos(3,0.0);
						double distance=0.0;
						for(int m=0; m<(int)currcrosslinkerpos.size(); m++)
						{
							currcrosslinkerpos[m] = (*crosslinkerpositions_)[m][(*crosslinkerentries)[l]];
							if (currcrosslinkerpos[m] > periodlength+currentcentershift[m])
								currcrosslinkerpos[m] -= periodlength;
							if (currcrosslinkerpos[m] < 0.0+currentcentershift[m])
								currcrosslinkerpos[m] += periodlength;

							distance += (currcrosslinkerpos[m] - currentrasterpoint[m]) * (currcrosslinkerpos[m] - currentrasterpoint[m]);
						}
						averagedistance += sqrt(distance);
					}
					averagedistance /= (double)crosslinkerentries->size();

					if(averagedistance<smallestdistance)
					{
						smallestdistance = averagedistance;
						for(int m=0; m<3; m++)
						{
							(*boxcenter)[m] = currentrasterpoint[m];
							(*centershift)[m] = currentcentershift[m];
						}
					}
				}
	}
	else
		for(int m=0; m<3; m++)
		{
			(*boxcenter)[m] = periodlength/2.0;
			(*centershift)[m] = 0.0;
		}
	//if(!discret_.Comm().MyPID())
		//cout<<"Box Center(2): "<<(*boxcenter)[0]<<", "<<(*boxcenter)[1]<<", "<<(*boxcenter)[2]<<endl;
	return;
}//StatMechManager::DDCorrShift()

/*------------------------------------------------------------------------------*
 | Determine current network structure and output network type as single        |
 | characteristic number. Also, output filament orientations.                   |
 |                                                        (public) mueller 11/10|
 *------------------------------------------------------------------------------*/
void StatMechManager::DDCorrCurrentStructure(const Epetra_Vector& disrow,
																						 std::vector<double>* centershift,
																						 std::vector<int>* crosslinkerentries,
																						 const int& istep,
																						 const std::ostringstream& filename,
																						 bool filorientoutput)
{
  // get column map displacements
  Epetra_Vector discol(*(discret_.DofColMap()), true);
  LINALG::Export(disrow, discol);

  // calculations done by Proc 0 only
  if(discret_.Comm().MyPID()==0)
  {
		// number indicating structure type
		int structurenumber = 0;
		// number of crosslinker elements
		int numcrossele = (int)crosslinkerentries->size();
		double periodlength = statmechparams_.get<double>("PeriodLength", 0.0);
		double rlink = statmechparams_.get<double>("R_LINK", 1.0);
		// number of nested intervals
		int maxexponent = (int)ceil(log(periodlength/rlink)/log(2.0));
		// vector for test volumes (V[0]-sphere, V[1]-cylinder, V[2]-layer/homogenous network)
		std::vector<double> volumes(3,9e99);
		// characteristic lengths of "volumes"
		std::vector<double> characlength(3,9e99);

		if(numcrossele>0)
		{
/// calculate center of gravity with respect to shiftedpositions for bound crosslinkers
			LINALG::Matrix<3,1> cog;
			std::vector<LINALG::Matrix<3,1> > shiftedpositions;
			cog.Clear();
			// shift positions according to new center point (raster point)
			for(int i=0; i<numcrossele; i++)
			{
				LINALG::Matrix<3,1> currposition;
				for(int j=0; j<(int)currposition.M(); j++)
				{
					currposition(j) = (*crosslinkerpositions_)[j][(*crosslinkerentries)[i]];
					if (currposition(j) > periodlength+(*centershift)[j])
						currposition(j) -= periodlength;
					if (currposition(j) < 0.0+(*centershift)[j])
						currposition(j) += periodlength;

					cog(j) += currposition(j);
				}
				shiftedpositions.push_back(currposition);
			}
			if(numcrossele != 0)
				cog.Scale(1/(double)numcrossele);

	/// calculate normed vectors and output filament element orientations
			// normed vectors for structural analysis (projections of base vectors e1, e2, e3 onto structure)
			std::vector<LINALG::Matrix<3,1> > normedvectors;
			for(int i=0; i<3; i++)
			{
				LINALG::Matrix<3,1> normedi;
				normedi.Clear();
				normedvectors.push_back(normedi);
			}

	/// determine normed vectors as well as output filament element vectors
			std::ostringstream orientfilename;
			orientfilename << "./FilamentOrientations_"<<std::setw(6) << setfill('0') << istep <<".dat";
			FilamentOrientations(discol, &normedvectors, orientfilename, filorientoutput);

			// Scale normed vectors to unit length
			cout<<"\nnormed vectors:"<<endl;
			for(int i=0; i<(int)normedvectors.size(); i++)
			{
				normedvectors[i].Scale(1/normedvectors[i].Norm2());
				cout<<"v_"<<i<<": ";
				for(int j=0; j<(int)normedvectors[i].M() ; j++)
					cout<<normedvectors[i](j)<<" ";
				cout<<endl;
			}

	/// determine network structure
			// threshold fraction of crosslinkers
			double pthresh = 0.9;

			// calculate smallest possible test volumes that fulfill the requirement /numcrossele >= pthresh
			for(int i=0; i<(int)volumes.size(); i++)
			{
				switch(i)
				{
					// shperical volume
					case 0:
					{
						bool leaveloop = false;
						// initial search radius
						double radius = periodlength/2.0;
						// fraction of crosslinks within test volume
						double pr = 0.0;
						// tolerance
						double tol = 0.02;
						int exponent = 1;

						// loop as long as pr has not yet reached pthresh
						while(!leaveloop)
						{
							int rcount = 0;
							// loop over crosslinker elements
							for(int j=0; j<numcrossele; j++)
							{
								// get distance of crosslinker element to center of gravity
								LINALG::Matrix<3,1> dist = shiftedpositions[j];
								dist -= cog;
								if(dist.Norm2()<=radius)
									rcount++;
							}
							pr = double(rcount)/double(numcrossele);

							exponent++;
							// new radius
							if((pr<pthresh-tol || pr>pthresh+tol) && exponent<=maxexponent)
							{
								// determine "growth direction"
								double sign;
								if(pr<pthresh)
									sign = 1.0;
								else
									sign = -1.0;
								radius += sign*periodlength/pow(2.0,(double)exponent);
							}
							else
								leaveloop = true;
						}
						// store characteristic length and test sphere volume
						characlength[0] = radius;
						volumes[0] = 4/3 * M_PI * pow(radius, 3.0);
					}
					break;
					// cylindrical volume
					case 1:
					{
						// cylinder
						bool leaveloop = false;
						double radius = periodlength/2.0;
						double cyllength = 0.0;
						double pr = 0.0;
						double tol = 0.02;
						int exponent = 1;

						// get the intersections of normed[0] with the cube faces
						std::vector<LINALG::Matrix<3,1> > intersections;
						// cube face boundaries of jk-surface of cubical volume
						LINALG::Matrix<3,2> surfaceboundaries;
						for(int j=0; j<(int)surfaceboundaries.M(); j++)
						{
							surfaceboundaries(j,0) = (*centershift)[j];
							surfaceboundaries(j,1) = (*centershift)[j]+periodlength;
						}
						cout<<surfaceboundaries<<endl;
						cout<<"cog: "<<cog(0)<<" "<<cog(1)<<" "<<cog(2)<<" "<<endl;
						cout<<"v:   "<<(normedvectors[0])(0)<<" "<<(normedvectors[0])(1)<<" "<<(normedvectors[0])(2)<<endl;
						for(int j=0; j<(int)surfaceboundaries.N(); j++)
							for(int k=0; k<3; k++)
								for(int l=0; l<3; l++)
									if(l>k)
										for(int m=0; m<3; m++)
											if(m!=k && m!=l)
											{
												LINALG::Matrix<3,1> currentintersection;
												// known intersection component
												currentintersection(m) = surfaceboundaries(m,j);
												cout<<currentintersection(m)<<" "<<cog(m)<<" "<<(normedvectors[0])(m)<<endl;
												// get line parameter
												double lambdaline = (currentintersection(m)-cog(m))/(normedvectors[0])(m);
												currentintersection(k) = cog(k)+lambdaline*(normedvectors[0])(k);
												currentintersection(l) = cog(l)+lambdaline*(normedvectors[0])(l);
												cout<<"with lambda= "<<lambdaline<<": "<<currentintersection(k)<<" "<<currentintersection(l)<<" "<<currentintersection(m)<<endl;
												// check if intersection lies on volume boundary
												if(currentintersection(k)<=surfaceboundaries(k,1) && currentintersection(k)>=surfaceboundaries(k,0) &&
													 currentintersection(l)<=surfaceboundaries(l,1) && currentintersection(l)>=surfaceboundaries(l,0))
													intersections.push_back(currentintersection);
											}
						cout<<"intersections.size() = "<<intersections.size()<<endl;
						LINALG::Matrix<3,1> difference = intersections[1];
						difference -= intersections[0];
						cyllength = difference.Norm2();

						// consider only normedvector[0] as the other normed directions are parallel or antiparallel
						while(!leaveloop)
						{
							int rcount = 0;
							// loop over crosslinker elements
							for(int j=0; j<numcrossele; j++)
							{
								// get distance of crosslinker element to normed1 through center of gravity
								// intersection line-plane
								LINALG::Matrix<3,1> crosstocog = shiftedpositions[j];
								crosstocog -= cog;

								double numerator = crosstocog.Dot(normedvectors[0]);
								double denominator = (normedvectors[0]).Dot(normedvectors[0]);
								double lambda = numerator/denominator;
								// intersection and distance of crosslinker to intersection
								LINALG::Matrix<3,1> distance = normedvectors[0];
								distance.Scale(lambda);
								distance += cog;
								distance -= shiftedpositions[j];
								if(distance.Norm2()<=radius)
									rcount++;
							}
							pr = double(rcount)/double(numcrossele);

							exponent++;
							// new radius
							if((pr<pthresh-tol || pr>pthresh+tol) && exponent<=maxexponent)
							{
								// determine "growth direction"
								double sign;
								if(pr<pthresh)
									sign = 1.0;
								else
									sign = -1.0;
								radius += sign*periodlength/pow(2.0,(double)exponent);
							}
							else
								leaveloop = true;
						}
						characlength[1] = radius;
						volumes[1] = M_PI*radius*radius*cyllength;
					}
					break;
					// cuboid layer volume
					case 2:
					{
						bool leaveloop = false;
						double thickness = periodlength/2.0;
						double pr = 0.0;
						double tol = 0.02;
						int exponent = 1;

						//determine the two normed vectors with the largest inter-vector angle (two vectors are (anti)parallel, the third is perpendicular)
						double alpha = -1.0;
						int dir1=-1, dir2=-1;
						for(int j=0; j<3; j++)
							for(int k=0; k<3; k++)
								if(k>j && (normedvectors[j]).Dot(normedvectors[k])>alpha)
								{
									alpha = acos(normedvectors[j].Dot(normedvectors[k]));
									dir1=j;
									dir2=k;
								}
						cout<<"\n\nalpha_"<<dir1<<dir2<<" = "<<alpha<<endl;
						// if two directions exist
						if(alpha > 1e-8)
						{
							// cross product n_1 x n_2, plane normal
							LINALG::Matrix<3,1> normal;
							normal(0) = normedvectors[dir1](1)*normedvectors[dir2](2) - normedvectors[dir1](2)*normedvectors[dir2](1);
							normal(1) = normedvectors[dir1](2)*normedvectors[dir2](0) - normedvectors[dir1](0)*normedvectors[dir2](2);
							normal(2) = normedvectors[dir1](0)*normedvectors[dir2](1) - normedvectors[dir1](1)*normedvectors[dir2](0);
							cout<<"n_mn: "<<normal(0)<<" "<<normal(1)<<" "<<normal(2)<<endl;
							while(!leaveloop)
							{
								int rcount = 0;
								for(int j=0; j<numcrossele; j++)
								{
									// given, that cog E plane with normal vector "normal"
									// constant in Hessian normal form
									double d = normal.Dot(cog);
									// distance of crosslinker element to plane
									double pn = normal.Dot(shiftedpositions[j]);
									double disttoplane = fabs(pn-d);

									if(disttoplane <= thickness)
										rcount++;
								}
								pr = double(rcount)/double(numcrossele);

								exponent++;

								if((pr<pthresh-tol || pr>pthresh+tol) && exponent<=maxexponent)
								{
									double sign;
									if(pr<pthresh)
										sign = 1.0;
									else
										sign = -1.0;
									thickness += sign*periodlength/pow(2.0,(double)exponent);
								}
								else
									leaveloop = true;
							}

							// calculation of the volume
							// cube face boundaries of jk-surface of cubical volume
							LINALG::Matrix<3,2> surfaceboundaries;
							for(int j=0; j<(int)surfaceboundaries.M(); j++)
							{
								surfaceboundaries(j,0) = (*centershift)[j];
								surfaceboundaries(j,1) = (*centershift)[j]+periodlength;
							}

							// coordinates of intersection points
							int counter = 0;
							std::vector<LINALG::Matrix<3,1> > interseccoords;
							for(int m=0; m<(int)surfaceboundaries.N(); m++) // (two) planes perpendicular to l-direction
								for(int n=0; n<(int)surfaceboundaries.N(); n++) // (two) boundary cube edges for component k
								{
									cout<<"("<<m<<","<<n<<"):"<<endl;
									for(int j=0; j<3; j++) // spatial component j
										for(int k=0; k<3; k++) // spatial component k
											if(k>j)// above diagonal
												for(int l=0; l<3; l++) // spatial component l
													if(l!=j && l!=k)
													{
														counter++;
														// starting from intersection line of (layer plane) x (boundary volume plane kl): n_xX+n_yY = c,
														// calculate the value of the remaining component j and check if point lies on the cubic surface
														LINALG::Matrix<3,1> coords;
														coords(l) = surfaceboundaries(l,m);
														coords(k) = surfaceboundaries(k,n);
														coords(j) = ((cog(l)-coords(l))*normal(l)+(cog(k)-coords(k))*normal(k))/normal(j) + cog(j);
														cout<<"coords("<<j<<","<<k<<","<<l<<"): "<<coords(j)<<" "<<coords(k)<<" "<<coords(l)<<endl;
														if (fabs(coords(j))<=fabs(surfaceboundaries(j,m)) || fabs(coords(j)-surfaceboundaries(j,m))<1e-8)
														{
															cout<<"  ";
															for(int n=0; n<(int)coords.M(); n++)
																cout<<coords(n)<<" ";
															cout<<endl;
															interseccoords.push_back(coords);
														}
													}
								}
							// calculate volume according to number of intersection points
							cout<<"interseccoords.size() = "<<interseccoords.size()<<endl;
							switch((int)interseccoords.size())
							{
								// triangle
								case 3:
								{
									// area of the triangle via cosine rule
									LINALG::Matrix<3,1> c = interseccoords[0];
									c -= interseccoords[1];
									LINALG::Matrix<3,1> a = interseccoords[1];
									a -= interseccoords[2];
									LINALG::Matrix<3,1> b = interseccoords[0];
									b -= interseccoords[2];
									double cl = c.Norm2();
									double al = a.Norm2();
									double bl = b.Norm2();
									double alpha = acos((cl*cl+bl*bl-al*al)/(2.0*bl*cl));
									double h = bl * sin(alpha);

									// volume of layer (factor 0.5 missing, since "real" thickness is thickness*2.0)
									volumes[2] = cl*h*thickness;
								}
								break;
								// square/rectangle/trapezoid
								case 4:
								{
									// edges
									LINALG::Matrix<3,1> a = interseccoords[0];
									a -= interseccoords[1];
									LINALG::Matrix<3,1> c = interseccoords[2];
									c -= interseccoords[3];
									LINALG::Matrix<3,1> d = interseccoords[3];
									d -= interseccoords[0];
									// diagonal
									LINALG::Matrix<3,1> f = interseccoords[1];
									f -= interseccoords[3];
									double al = a.Norm2();
									double cl = c.Norm2();
									double dl = d.Norm2();
									double fl = f.Norm2();
									double alpha = acos((al*al+dl*dl-fl*fl)/(2.0*al*dl));
									double h = dl * sin(alpha);

									volumes[2] = (al+cl)/2.0 * h * thickness;
								}
								break;
								// hexahedron
								case 6:
								{
									double hexvolume = 0.0;
									for(int j=0; j<6; j++)
									{
										// get edge of j-th triangle wihtin hexagon
										LINALG::Matrix<3,1> c = interseccoords[j];
										LINALG::Matrix<3,1> a;
										LINALG::Matrix<3,1> b = interseccoords[j];
										b -= cog;
										if(j==5)
										{
											c -= interseccoords[0];
											a = interseccoords[0];
											a -= interseccoords[5];

										}
										else
										{
											c -= interseccoords[j+1];
											a = interseccoords[j+1];
											a -= cog;
										}
										double al = a.Norm2();
										double bl = b.Norm2();
										double cl = c.Norm2();
										double alpha = acos((cl*cl+bl*bl-al*al)/(2.0*bl*cl));
										double h = bl * sin(alpha);

										hexvolume += cl*h*thickness;
									}
									volumes[2] = hexvolume;
								}
								break;
							}
						}
						characlength[2] = 2.0*thickness;
					}
					break;
				}
			}
		}
		// smallest volume
		double minimalvol = 9e99;
		int minimum = 0;
		for(int j=0; j<(int)volumes.size(); j++)
			if(volumes[j]<minimalvol)
			{
				minimalvol = volumes[j];
				structurenumber = j;
				minimum = j;
			}
		if(structurenumber==0 && characlength[0]>=periodlength/2.0)
			structurenumber = 3;

		cout<<"\nDDCorr Volumes: "<<endl;
		for(int j=0; j<(int)volumes.size(); j++)
		{
			cout<<scientific<<"V("<<j<<"): "<<volumes[j]<<", l_c("<<j<<"): "<<characlength[j]<<endl;
		}

/// cout and return network structure
  	// write to output files
  	// append structure number to DDCorr output
		FILE* fp = NULL;
		fp = fopen(filename.str().c_str(), "a");
		std::stringstream structuretype;
		structuretype<<structurenumber<<"    "<<characlength[minimum]<<"    "<<0.0<<"    "<<0.0<<"    "<<0.0<<endl;
		fprintf(fp, structuretype.str().c_str());
		fclose(fp);

  	switch(structurenumber)
  	{
  		// cluster
  		case 0:
  			cout<<"\nNetwork structure: Cluster\n"<<endl;
  		break;
  		// bundle
  		case 1:
  			cout<<"\nNetwork structure: Bundle\n"<<endl;
  		break;
  		// layer
  		case 2:
  			cout<<"\nNetwork structure: Layer\n"<<endl;
  		break;
  		// layer
  		case 3:
  			cout<<"\nNetwork structure: Homogeneous network\n"<<endl;
  		break;
  	}
  }
}

/*----------------------------------------------------------------------*
 | filament orientations and output               (public) mueller 07/10|
 *----------------------------------------------------------------------*/
void StatMechManager::FilamentOrientations(const Epetra_Vector& discol, std::vector<LINALG::Matrix<3,1> >* normedvectors, const std::ostringstream& filename, bool fileoutput)
{
  /* Output of filament element orientations (Proc 0 only):
   * format: filamentnumber    d_x  d_y  d_z
   */

  if(discret_.Comm().MyPID()==0)
  {
  	double periodlength = statmechparams_.get<double>("PeriodLength", 0.0);

    FILE* fp = NULL;
    if(fileoutput)
    	fp = fopen(filename.str().c_str(), "w");
    std::stringstream fileleorientation;

  	// get filament number conditions
		vector<DRT::Condition*> filaments(0);
		discret_.GetCondition("FilamentNumber", filaments);

		for(int fil=0; fil<(int)filaments.size(); fil++)
		{
			// get next filament
			DRT::Condition* currfilament = filaments[fil];
			for(int node=1; node<(int)currfilament->Nodes()->size(); node++)
			{
				// obtain column map LIDs
				int gid0 = currfilament->Nodes()->at(node-1);
				int gid1 = currfilament->Nodes()->at(node);
				int nodelid0 = discret_.NodeColMap()->LID(gid0);
				int nodelid1 = discret_.NodeColMap()->LID(gid1);
				DRT::Node* node0 = discret_.lColNode(nodelid0);
				DRT::Node* node1 = discret_.lColNode(nodelid1);

				// calculate directional vector between nodes
				LINALG::Matrix<3, 1> dirvec;
				for(int dof=0; dof<3; dof++)
				{
					int dofgid0 = discret_.Dof(node0)[dof];
					int dofgid1 = discret_.Dof(node1)[dof];
					double poscomponent0 = node0->X()[dof]+discol[discret_.DofColMap()->LID(dofgid0)];
					double poscomponent1 = node1->X()[dof]+discol[discret_.DofColMap()->LID(dofgid1)];
					// check for periodic boundary shift and correct accordingly
					if (fabs(poscomponent1 - periodlength - poscomponent0) < fabs(poscomponent1 - poscomponent0))
						poscomponent1 -= periodlength;
					else if (fabs(poscomponent1 + periodlength - poscomponent0) < fabs(poscomponent1 - poscomponent0))
						poscomponent1 += periodlength;

					dirvec(dof) = poscomponent1-poscomponent0;
				}
				// normed vector
				dirvec.Scale(1/dirvec.Norm2());

				// add element directional vectors up
				for(int i=0; i<(int)normedvectors->size(); i++)
				{
					// base vector
					LINALG::Matrix<3,1> ei;
					LINALG::Matrix<3,1> vi = dirvec;

					ei.Clear();
					ei(i) = 1.0;

					if(acos(vi.Dot(ei))>M_PI/2.0)
						vi.Scale(-1.0);
					(*normedvectors)[i] += vi;
				}

				// write normed directional vector to stream
				fileleorientation<<fil<<"    "<<std::setprecision(12)<<dirvec(0)<<" "<<dirvec(1)<<" "<<dirvec(2)<<endl;
			}
		}
		if(fileoutput)
		{
			fprintf(fp, fileleorientation.str().c_str());
			fclose(fp);
		}
  }
}// StatMechManager:FilamentOrientations()

/*----------------------------------------------------------------------*
 | check the binding mode of a crosslinker        (public) mueller 07/10|
 *----------------------------------------------------------------------*/
bool StatMechManager::CheckForKinkedVisual(int eleid)
{
  bool kinked = true;
  // if element is a crosslinker
  if(eleid>basisnodes_)
  {
    DRT::Element *element = discret_.gElement(eleid);
    int lid = discret_.NodeColMap()->LID(element->NodeIds()[0]);
    for(int i=0; i<element->NumNode(); i++)
      if((*filamentnumber_)[lid]!=(*filamentnumber_)[element->NodeIds()[i]])
        kinked = false;
    return kinked;
  }
  else
    kinked = false;
  return kinked;
}//StatMechManager::CheckForKinkedVisual

#endif  // #ifdef CCADISCRET
