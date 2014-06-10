/*!----------------------------------------------------------------------
\file matlist.cpp

<pre>
Maintainer: Andreas Ehrl
            ehrl@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089-289-15252
</pre>
*----------------------------------------------------------------------*/


#include <vector>
#include "matlist.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_mat/matpar_bundle.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::PAR::MatList::MatList(
  Teuchos::RCP<MAT::PAR::Material> matdata
  )
: Parameter(matdata),
  nummat_(matdata->GetInt("NUMMAT")),
  matids_(matdata->Get<std::vector<int> >("MATIDS")),
  local_((matdata->GetInt("LOCAL")))
{
  // check if sizes fit
  if (nummat_ != (int)matids_->size())
    dserror("number of materials %d does not fit to size of material vector %d", nummat_, matids_->size());

  if (not local_)
  {
    // make sure the referenced materials in material list have quick access parameters
    std::vector<int>::const_iterator m;
    for (m=matids_->begin(); m!=matids_->end(); ++m)
    {
      const int matid = *m;
      Teuchos::RCP<MAT::Material> mat = MAT::Material::Factory(matid);
      mat_.insert(std::pair<int,Teuchos::RCP<MAT::Material> >(matid,mat));
    }
  }
}

Teuchos::RCP<MAT::Material> MAT::PAR::MatList::CreateMaterial()
{
  return Teuchos::rcp(new MAT::MatList(this));
}


MAT::MatListType MAT::MatListType::instance_;


DRT::ParObject* MAT::MatListType::Create( const std::vector<char> & data )
{
  MAT::MatList* matlist = new MAT::MatList();
  matlist->Unpack(data);
  return matlist;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::MatList::MatList()
  : params_(NULL)
{
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::MatList::MatList(MAT::PAR::MatList* params)
  : params_(params)
{
  // setup of material map
  if (params_->local_)
  {
    SetupMatMap();
  }
  // else: material Teuchos::rcps live inside MAT::PAR::MatList
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::MatList::SetupMatMap()
{
  // safety first
  mat_.clear();
  if (not mat_.empty()) dserror("What's going wrong here?");

  // make sure the referenced materials in material list have quick access parameters

  // here's the recursive creation of materials
  std::vector<int>::const_iterator m;
  for (m=params_->MatIds()->begin(); m!=params_->MatIds()->end(); ++m)
  {
    const int matid = *m;
    Teuchos::RCP<MAT::Material> mat = MAT::Material::Factory(matid);
    if (mat == Teuchos::null) dserror("Failed to allocate this material");
    mat_.insert(std::pair<int,Teuchos::RCP<MAT::Material> >(matid,mat));
  }
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::MatList::Clear()
{
  params_ = NULL;
  mat_.clear();
  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::MatList::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm( data );
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // matid
  int matid = -1;
  if (params_ != NULL) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data,matid);

  if (params_->local_)
  {
    // loop map of associated local materials
    if (params_ != NULL)
    {
      //std::map<int, Teuchos::RCP<MAT::Material> >::const_iterator m;
      std::vector<int>::const_iterator m;
      for (m=params_->MatIds()->begin(); m!=params_->MatIds()->end(); m++)
      {
        (mat_.find(*m))->second->Pack(data);
      }
    }
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::MatList::Unpack(const std::vector<char>& data)
{
  // make sure we have a pristine material
  Clear();

  std::vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");

  // matid and recover params_
  int matid(-1);
  ExtractfromPack(position,data,matid);
  params_ = NULL;
  if (DRT::Problem::Instance()->Materials() != Teuchos::null)
    if (DRT::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = DRT::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat = DRT::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::MatList*>(mat);
      else
        dserror("Type of parameter material %d does not fit to calling type %d", mat->Type(), MaterialType());
    }

  if (params_ != NULL) // params_ are not accessible in postprocessing mode
  {
    // make sure the referenced materials in material list have quick access parameters
    std::vector<int>::const_iterator m;
    for (m=params_->MatIds()->begin(); m!=params_->MatIds()->end(); m++)
    {
      const int actmatid = *m;
      Teuchos::RCP<MAT::Material> mat = MAT::Material::Factory(actmatid);
      if (mat == Teuchos::null) dserror("Failed to allocate this material");
      mat_.insert(std::pair<int,Teuchos::RCP<MAT::Material> >(actmatid,mat));
    }

    if (params_->local_)
    {
      // loop map of associated local materials
      for (m=params_->MatIds()->begin(); m!=params_->MatIds()->end(); m++)
      {
        std::vector<char> pbtest;
        ExtractfromPack(position,data,pbtest);
        (mat_.find(*m))->second->Unpack(pbtest);
      }
    }
    // in the postprocessing mode, we do not unpack everything we have packed
    // -> position check cannot be done in this case
    if (position != data.size())
      dserror("Mismatch in size of data %d <-> %d",data.size(),position);
  } // if (params_ != NULL)
}

