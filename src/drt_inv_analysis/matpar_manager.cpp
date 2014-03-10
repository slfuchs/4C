/*----------------------------------------------------------------------*/
/*!
\file matpar_manager.H
\brief manage material parameters during optimization

<pre>
Maintainer: Sebastian Kehl
            kehl@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15249
</pre>

!*/

/*----------------------------------------------------------------------*/
/* headers */
#include "matpar_manager.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_io/io.H"
#include "../drt_io/io_pstream.H"

#include "../drt_lib/drt_element.H"

#include "../drt_mat/material.H"
#include "../drt_mat/matpar_bundle.H"

#include "../linalg/linalg_utils.H"

/*----------------------------------------------------------------------*/
/* constructor                                               keh 10/13  */
/*----------------------------------------------------------------------*/
STR::INVANA::MatParManager::MatParManager(Teuchos::RCP<DRT::Discretization> discret):
numparams_(0),
discret_(discret),
params_(Teuchos::null),
optparams_(Teuchos::null),
optparams_o_(Teuchos::null),
paramlayoutmap_(Teuchos::null)
{
  const Teuchos::ParameterList& statinvp = DRT::Problem::Instance()->StatInverseAnalysisParams();

  if (not discret_->Filled() || not discret_->HaveDofs())
      dserror("Discretisation is not complete or has no dofs!");

  // set up maps to link against materials, parameters and materials/parameters for the optimization
  SetupMatOptMap();

  params_ = Teuchos::rcp(new Epetra_MultiVector(*(discret_->ElementColMap()),numparams_,true));

  // want metaparametrization
  metaparams_ = DRT::INPUT::IntegralValue<bool>(statinvp, "METAPARAMS");
}

/*----------------------------------------------------------------------*/
/* Get initial set of material parameters                    keh 10/13  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::InitParams()
{
  const std::map<int,Teuchos::RCP<MAT::PAR::Material> >& mats = *DRT::Problem::Instance()->Materials()->Map();

  std::map<int,std::vector<int> >::const_iterator it;
  for (it=paramap_.begin(); it!=paramap_.end(); it++)
  {
    Teuchos::RCP<MAT::PAR::Material> actmat = mats.at(it->first);
    switch(actmat->Parameter()->Type())
    {
      case INPAR::MAT::m_aaaneohooke:
      {
        std::vector<int>::const_iterator jt;
        for (jt = it->second.begin(); jt != it->second.end(); jt++)
        {
          if (metaparams_)
            (*optparams_)( parapos_.at(it->first).at(jt-it->second.begin()) )->PutScalar( sqrt(2*((actmat->Parameter()->GetParameter(*jt,0))-0.1)) );
          else
            (*optparams_)( parapos_.at(it->first).at(jt-it->second.begin()) )->PutScalar( (actmat->Parameter()->GetParameter(*jt,0)) );
        }
      }
      break;
      default:
        dserror("Materiall not provided by the Material Manager for Optimization");
      break;
    }
  }
}


/*----------------------------------------------------------------------*/
/* Setup map of material parameters to be optimized          keh 10/13  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::SetupMatOptMap()
{
  const Teuchos::ParameterList& statinvp = DRT::Problem::Instance()->StatInverseAnalysisParams();

  // the materials of the problem
  const std::map<int,Teuchos::RCP<MAT::PAR::Material> >& mats = *DRT::Problem::Instance()->Materials()->Map();

  // parameters to be optimized
  std::string word2;
  std::istringstream pstream(Teuchos::getNumericStringParameter(statinvp,"PARAMLIST"));
  int matid;
  int actmatid=0;
  char* pEnd;
  while (pstream >> word2)
  {
    matid = std::strtol(&word2[0],&pEnd,10);
    if (*pEnd=='\0') //if (matid != 0)
    {
      std::cout <<  "matid " << matid << std::endl;
      actmatid = matid;
      continue;
    }

    if (word2!="none" && actmatid!=0)
    {
      //check whether this material exists in the problem
      if ( mats.find(actmatid) == mats.end() )
        dserror("material %d not found in matset", actmatid);

      //check if this material has parameters to be optimized:
      std::map<std::string, int> optparams;
      mats.at(actmatid)->Parameter()->OptParams(&optparams);
      if ( optparams.find(word2) == optparams.end() )
        dserror("parameter %s is not prepared to be optimized for mat %s", word2.c_str(), mats.at(actmatid)->Name().c_str());

      paramap_[actmatid].push_back(optparams.at(word2));
      parapos_[actmatid].push_back(numparams_);
      numparams_ += 1;
    }
    else
      dserror("Give the parameters for the respective materials");
  }

  std::cout << "the number of parameters is: " << numparams_ << std::endl;

}

/*----------------------------------------------------------------------*/
/* bring current set of parameters to the material           keh 10/13  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::SetParams()
{
  const std::map<int,Teuchos::RCP<MAT::PAR::Material> >& mats = *DRT::Problem::Instance()->Materials()->Map();

  // get the actual set of elementwise material parameters from the derived classes
  Teuchos::RCP<Epetra_MultiVector> getparams;
  getparams = Teuchos::rcp(new Epetra_MultiVector(*(discret_->ElementColMap()),numparams_,true));
  FillParameters(getparams);


  if (metaparams_)
  {
    params_->PutScalar(0.1);
    params_->Multiply(0.5,*getparams,*getparams,1.0);
  }
  else
    params_->Scale(1.0,*getparams);


  //loop materials to be optimized
  std::map<int,std::vector<int> >::const_iterator curr;
  for (curr=paramap_.begin(); curr != paramap_.end(); curr++ )
  {
    Teuchos::RCP<MAT::PAR::Material> actmat = mats.at(curr->first);

    // loop the parameters to be optimized
    std::vector<int> actparams = paramap_.at(curr->first);
    std::vector<int>::const_iterator it;
    for ( it=actparams.begin(); it!=actparams.end(); it++)
    {
      actmat->Parameter()->SetParameter(*it,Teuchos::rcp((*params_)( parapos_.at(curr->first).at(it-actparams.begin()) ),false));
    }
  }//loop optimized materials
}

/*----------------------------------------------------------------------*/
/* update material parameters and keep them as "old"         keh 03/14  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::UpdateParams(Teuchos::RCP<Epetra_MultiVector> toadd)
{
  optparams_o_->Scale(1.0, *optparams_);
  optparams_->Update(1.0,*toadd,1.0);

  // bring updated parameters to the elements
  SetParams();
}

/*----------------------------------------------------------------------*/
/* replace material parameters AND DONT touch old ones       keh 03/14  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::ReplaceParams(Teuchos::RCP<Epetra_MultiVector> toreplace)
{
  optparams_->Update(1.0,*toreplace,0.0);

  // bring updated parameters to the elements
  SetParams();
}

/*----------------------------------------------------------------------*/
/* reset to last set of material parameters                  keh 03/14  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::ResetParams()
{
  optparams_->Scale(1.0, *optparams_o_);

  // bring updated parameters to the elements
  SetParams();
}

/*----------------------------------------------------------------------*/
/* evaluate gradient based on dual solution                  keh 10/13  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::Evaluate(double time, Teuchos::RCP<Epetra_MultiVector> dfint)
{

  // this temporarily hold the derivative of the residual wrt one physical parameter multiplied by the dual displacements
  Epetra_Vector dfintp(*(discret_->ElementColMap()),true);

  Teuchos::RCP<const Epetra_Vector> disdual = discret_->GetState("dual displacement");

  // the reason not to do this loop via a discretizations evaluate call is that if done as is,
  // all elements have to be looped only once and evaluation is done only in case when an
  // element really has materials with parameters to be optimized. And "Nachdifferenzieren"
  // with respect to the parameters to be optimized can be done without setting up the whole
  // gradient dR/dp_m and postmultiply it with dp_m\dp_o
  // with R: Residual forces; p_m: material params; p_o parametrization of p_m
  // TODO: Think of a more standard baci design way!
  for (int i=0; i<discret_->NumMyColElements(); i++)
  {
    DRT::Element* actele;
    actele = discret_->lColElement(i);
    int elematid = actele->Material()->Parameter()->Id();

    if (paramap_.find(elematid) == paramap_.end() )
      continue;

    // list to define routines at elementlevel
    Teuchos::ParameterList p;
    p.set("total time", time);

    std::vector<int> actparams = paramap_.at( elematid );
    std::vector<int>::const_iterator it;
    for ( it=actparams.begin(); it!=actparams.end(); it++)
    {
      dfintp.PutScalar(0.0);

      p.set("action", "calc_struct_internalforce");
      p.set("matparderiv", *it);

      //initialize element vectors
      int ndof = actele->NumNode()*3;
      Epetra_SerialDenseMatrix elematrix1(ndof,ndof,false);
      Epetra_SerialDenseMatrix elematrix2(ndof,ndof,false);
      Epetra_SerialDenseVector elevector1(ndof);
      Epetra_SerialDenseVector elevector2(ndof);
      Epetra_SerialDenseVector elevector3(ndof);

      DRT::Element::LocationArray la(discret_->NumDofSets());
      actele->LocationVector(*discret_,la,false);
      actele->Evaluate(p,*discret_,la,elematrix1,elematrix2,elevector1,elevector2,elevector3);

      // dont forget product rule in case of parametrized material parameters!
      if (metaparams_)
      {
        double val1 = (*(*params_)( parapos_.at(elematid).at(it-actparams.begin()) ))[actele->LID()];
        elevector1.Scale(sqrt(2*(val1-0.1)));
      }

      //reuse elevector2
      for (int l=0; l<(int)la[0].lm_.size(); l++)
      {
        int lid=disdual->Map().LID(la[0].lm_.at(l));
        if (lid==-1) dserror("not found on this processor");
        elevector2[l] = (*disdual)[lid];
      }
      double val2 = elevector2.Dot(elevector1);

      //Assemble the gradient wrt one physical parameter
      int success = dfintp.SumIntoGlobalValue(actele->Id(),0,val2);
      if (success!=0) dserror("gid %d is not on this processor", actele->LID());


      // contraction to (optimization) parameter space:
      ContractGradient(dfint,Teuchos::rcp(&dfintp,false),parapos_.at(elematid).at(it-actparams.begin()));

    }//loop this elements material parameters (only the ones to be optimized)

  }//loop elements
}

/*----------------------------------------------------------------------*/
/* evaluate gradient based on FD                             keh 01/14  */
/*----------------------------------------------------------------------*/
void STR::INVANA::MatParManager::EvaluateFD(Teuchos::RCP<Epetra_MultiVector> dfint)
{
  if (discret_->Comm().NumProc()>1) dserror("this does probably not run in parallel");

  Epetra_MultiVector paramsbak(*params_);

  Teuchos::RCP<const Epetra_Vector> disdual = discret_->GetState("dual displacement");

  for (int i=0; i<discret_->NumMyColElements(); i++)
  {
    DRT::Element* actele;
    actele = discret_->lColElement(i);
    int elematid = actele->Material()->Parameter()->Id();

    if (paramap_.find(elematid) == paramap_.end() )
      continue;

    // list to define routines at elementlevel
    Teuchos::ParameterList p;

    std::vector<int> actparams = paramap_.at( elematid );
    std::vector<int>::const_iterator it;
    for ( it=actparams.begin(); it!=actparams.end(); it++)
    {
      p.set("action", "calc_struct_internalforce");

      double pa=1.0e-6;
      double pb=1.0e-12;

      //initialize element vectors
      int ndof = actele->NumNode()*3;
      Epetra_SerialDenseMatrix elematrix1(ndof,ndof,false);
      Epetra_SerialDenseMatrix elematrix2(ndof,ndof,false);
      Epetra_SerialDenseVector elevector1(ndof);
      Epetra_SerialDenseVector elevector2(ndof);
      Epetra_SerialDenseVector elevector3(ndof);

      DRT::Element::LocationArray la(discret_->NumDofSets());
      actele->LocationVector(*discret_,la,false);

      double actp = (*(*params_)( parapos_.at(elematid).at(it-actparams.begin()) ))[actele->LID()];
      double perturb = actp + pb + actp*pa;

      for (int j=0; j<2; j++)
      {
        if (j==1)
        {
          params_->ReplaceGlobalValue(actele->LID(),parapos_.at(elematid).at(it-actparams.begin()),perturb);
          SetParams();
        }

        if (j==0)
          actele->Evaluate(p,*discret_,la,elematrix1,elematrix2,elevector1,elevector2,elevector3);
        else if(j==1)
          actele->Evaluate(p,*discret_,la,elematrix1,elematrix2,elevector3,elevector2,elevector2);

        //reset params
        if (j==1)
        {
          params_->Update(1.0,paramsbak,0.0);
          SetParams();
        }
      }

      //build fd approx
      elevector1.Scale(-1.0);
      elevector1 += elevector3;
      elevector1.Scale(1.0/(pb+actp*pa));

      // dont forget product rule in case of parametrized material parameters!
      if (metaparams_)
      {
        dserror("verify this first for the FD-version");
        double val1 = (*(*params_)( parapos_.at(elematid).at(it-actparams.begin()) ))[actele->LID()];
        elevector1.Scale(val1);
      }

      //reuse elevector2
      for (int l=0; l<(int)la[0].lm_.size(); l++)
      {
        int lid=disdual->Map().LID(la[0].lm_.at(l));
        if (lid==-1) dserror("not found on this processor");
        elevector2[l] = (*disdual)[lid];
      }
      double val2 = elevector2.Dot(elevector1);

      int success = dfint->SumIntoMyValue(actele->LID(),parapos_.at(elematid).at(it-actparams.begin()),val2);
      if (success!=0) dserror("gid %d is not on this processor", actele->LID());


    }//loop this elements material parameters (only the ones to be optimized)

  }//loop elements
}

/*----------------------------------------------------------------------*/
/* return vector index of an elements material parameter     keh 10/13  */
/*----------------------------------------------------------------------*/
int STR::INVANA::MatParManager::GetParameterLocation(int eleid, std::string name)
{
  int loc = -1;

  if (!discret_->HaveGlobalElement(eleid))
    dserror("provide only ids of elements on this processor");

  const std::map<int,Teuchos::RCP<MAT::PAR::Material> >& mats = *DRT::Problem::Instance()->Materials()->Map();

  DRT::Element* actele = discret_->gElement(eleid);
  int matid = actele->Material()->Parameter()->Id();

  std::map<std::string, int> optparams;
  mats.at(matid)->Parameter()->OptParams(&optparams);
  if ( optparams.find(name) == optparams.end() )
    dserror("parameter %s is not prepared to be optimized for mat %s", name.c_str(), mats.at(matid)->Name().c_str());

  if (paramap_.find(matid) == paramap_.end())
    dserror("Material with matid %d is not given for optimization in datfile", matid);
  else
  {
    //this is the vector index
    std::vector<int> actparams = paramap_.at(matid);
    std::vector<int>::const_iterator it;
    for ( it=actparams.begin(); it!=actparams.end(); it++)
    {
      if (actparams.at(it-actparams.begin()) == optparams.at(name))
        loc = parapos_.at(matid).at(it-actparams.begin());
    }
  }

return loc;

}
