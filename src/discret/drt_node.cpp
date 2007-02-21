/*!----------------------------------------------------------------------
\file node.cpp
\brief A virtual class for a node

<pre>
Maintainer: Michael Gee
            gee@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15239
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET
#ifdef TRILINOS_PACKAGE

#include "drt_node.H"
#include "drt_dserror.H"



/*----------------------------------------------------------------------*
 |  ctor (public)                                            mwgee 11/06|
 *----------------------------------------------------------------------*/
DRT::Node::Node(int id, const double* coords, const int owner) :
ParObject(),
id_(id),
owner_(owner),
dofset_(),
dentitytype_(on_none),
dentityid_(-1)
{
  for (int i=0; i<3; ++i) x_[i] = coords[i];
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       mwgee 11/06|
 *----------------------------------------------------------------------*/
DRT::Node::Node(const DRT::Node& old) :
ParObject(old),
id_(old.id_),
owner_(old.owner_),
dofset_(old.dofset_),
element_(old.element_),
dentitytype_(old.dentitytype_),
dentityid_(old.dentityid_)
{
  for (int i=0; i<3; ++i) x_[i] = old.x_[i];
  
  // we want a true deep copy of the condition_
  map<string,RefCountPtr<Condition> >::const_iterator fool;
  for (fool=old.condition_.begin(); fool!=old.condition_.end(); ++fool)
    SetCondition(fool->first,rcp(new DRT::Condition(*(fool->second))));

  return;
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            mwgee 11/06|
 *----------------------------------------------------------------------*/
DRT::Node::~Node()
{
  return;
}


/*----------------------------------------------------------------------*
 |  Deep copy this instance of Node and return pointer to it (public)   |
 |                                                            gee 11/06 |
 *----------------------------------------------------------------------*/
DRT::Node* DRT::Node::Clone() const
{
  DRT::Node* newnode = new DRT::Node(*this);
  return newnode;
}

/*----------------------------------------------------------------------*
 |  << operator                                              mwgee 11/06|
 *----------------------------------------------------------------------*/
ostream& operator << (ostream& os, const DRT::Node& node)
{
  node.Print(os); 
  return os;
}


/*----------------------------------------------------------------------*
 |  print this element (public)                              mwgee 11/06|
 *----------------------------------------------------------------------*/
void DRT::Node::Print(ostream& os) const
{
  // Print id and coordinates
  os << "Node " << setw(12) << Id()
     << " Owner " << setw(4) << Owner()
     << " Coords " 
     << setw(12) << X()[0] << " " 
     << setw(12) << X()[1] << " " 
     << setw(12) << X()[2] << " ";
  // print dofs if there are any
  if (Dof().NumDof())
  {
    os << Dof();
  }
  
  // Print design entity if there is any
  if (dentitytype_ != on_none)
  {
    if      (dentitytype_==on_dnode) os << "on DNODE " << dentityid_ << " ";
    else if (dentitytype_==on_dline) os << "on DLINE " << dentityid_ << " ";
    else if (dentitytype_==on_dsurface) os << "on DSURF " << dentityid_ << " ";
    else if (on_dsurface==on_dvolume) os << "on DVOL " << dentityid_ << " ";
    else dserror("Unknown type of design entity");
  }
  // Print conditions if there are any
  int numcond = condition_.size();
  if (numcond)
  {
    os << endl << numcond << " Conditions:\n";
    map<string,RefCountPtr<Condition> >::const_iterator curr;
    for (curr=condition_.begin(); curr != condition_.end(); ++curr)
    {
      os << curr->first << " ";
      os << *(curr->second) << endl;
    }
  }
  return;
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::Node::Pack(vector<char>& data) const
{
  data.resize(0);
  
  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add id
  int id = Id();
  AddtoPack(data,id);
  // add owner
  int owner = Owner();
  AddtoPack(data,owner);
  // x_
  AddtoPack(data,x_,3*sizeof(double));
  // dofset
  vector<char> dofsetpack(0);
  dofset_.Pack(dofsetpack);
  AddVectortoPack(data,dofsetpack);
  // dentitytype_
  AddtoPack(data,dentitytype_);
  // dentityid_
  AddtoPack(data,dentityid_);
  
  return;
}

#if 0
/*----------------------------------------------------------------------*
 |  Pack data from this element into vector of length size     (public) |
 |                                                            gee 11/06 |
 *----------------------------------------------------------------------*/
const char* DRT::Node::Pack(int& size) const
{
  const int sizeint    = sizeof(int);
  const int sizedouble = sizeof(double);
  //const int sizechar   = sizeof(char);

  // get data and size of dofset_
  int dofsetsize=0;
  const char* dofsetpack = dofset_.Pack(dofsetsize);

#if 0 // conditions are no longer communicated
  // get the size of all conditions
  int condsize=0;
  map<string,RefCountPtr<Condition> >::const_iterator curr;
  for (curr=condition_.begin(); curr != condition_.end(); ++curr)
  {
    condsize += SizeString(curr->first);
    int tmp=0;
    const char* condpack = curr->second->Pack(tmp);
    condsize += tmp;
    delete [] condpack;
  }
#endif

  size = 
  sizeint                +   // holds size itself
  sizeint                +   // type of this instance of ParObject, see top of ParObject.H
  sizeint                +   // holds id
  sizeint                +   // owner_
  sizedouble*3           +   // holds x_
  dofsetsize             +   // dofset_
  sizeof(OnDesignEntity) +   // dentitytype_
  sizeint                +   // dentityid_
#if 0
  sizeint                +   // no. objects in condition_
  condsize               +   // condition_
#endif
  0;                         // continue to add data here...

  char* data = new char[size];

  // pack stuff into vector
  int position = 0;

  // add size
  AddtoPack(position,data,size);
  // ParObject type
  int type = UniqueParObjectId();
  AddtoPack(position,data,type);
  // add id_
  int id = Id();
  AddtoPack(position,data,id);
  // add owner_
  int owner = Owner();
  AddtoPack(position,data,owner);
  // add x_
  AddtoPack(position,data,x_,3*sizedouble);
  // dofset_
  AddtoPack(position,data,dofsetpack,dofsetsize);
  delete [] dofsetpack;
  // dentitytype_
  AddtoPack(position,data,dentitytype_);
  // dentityid_
  AddtoPack(position,data,dentityid_);
#if 0
  // condition_
  int num = condition_.size(); // no. of objects
  AddtoPack(position,data,num);
  for (curr=condition_.begin(); curr != condition_.end(); ++curr)
  {
    AddStringtoPack(position,data,curr->first);
    int tmp=0;
    const char* condpack = curr->second->Pack(tmp);
    AddtoPack(position,data,condpack,tmp);
    delete [] condpack;
  }
#endif  
  // continue to add stuff here
  
  if (position != size)
    dserror("Mismatch in size of data %d <-> %d",size,position);

  return data;
}
#endif

/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::Node::Unpack(const vector<char>& data)
{
  int position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // id_
  ExtractfromPack(position,data,id_);
  // owner_
  ExtractfromPack(position,data,owner_);
  // x_
  ExtractfromPack(position,data,x_,3*sizeof(double));
  // dofset_
  vector<char> dofpack(0);
  ExtractVectorfromPack(position,data,dofpack);
  dofset_.Unpack(dofpack);
  // dentitytype_
  ExtractfromPack(position,data,dentitytype_);
  // dentityid_
  ExtractfromPack(position,data,dentityid_);
  
  if (position != (int)data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
} 

#if 0
/*----------------------------------------------------------------------*
 |  Unpack data into this element                              (public) |
 |                                                            gee 11/06 |
 *----------------------------------------------------------------------*/
bool DRT::Node::Unpack(const char* data)
{
  //const int sizeint    = sizeof(int);
  const int sizedouble = sizeof(double);
  //const int sizechar   = sizeof(char);

  int position = 0;
  
  // extract size
  int size = 0;
  ExtractfromPack(position,data,size);
  // ParObject instance type
  int type=0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("Wrong instance type in data");
  // extract id_
  ExtractfromPack(position,data,id_);
  // extract owner_
  ExtractfromPack(position,data,owner_);
  // extract x_
  ExtractfromPack(position,data,x_,3*sizedouble);  
  // dofset_
  dofset_.Unpack(&data[position]);
  int dofsetsize = dofset_.SizePack(&data[position]);
  position += dofsetsize;
  // dentitytype_
  ExtractfromPack(position,data,dentitytype_);
  // dentityid_
  ExtractfromPack(position,data,dentityid_);
#if 0
  // condition_
  int num=0;
  ExtractfromPack(position,data,num);
  for (int i=0; i<num; ++i)
  {
    string name;
    ExtractStringfromPack(position,data,name);
    RefCountPtr<Condition> cond = rcp(new Condition());
    cond->Unpack(&data[position]);
    int condsize = cond->SizePack(&data[position]);
    position += condsize;
    SetCondition(name,cond);
  }
#endif

  if (position != size)
    dserror("Mismatch in size of data %d <-> %d",size,position);
  return true;
}
#endif

/*----------------------------------------------------------------------*
 |  Get a condition of a certain name                          (public) |
 |                                                            gee 12/06 |
 *----------------------------------------------------------------------*/
void DRT::Node::GetCondition(const string& name,vector<DRT::Condition*>& out)
{
  const int num = condition_.count(name);
  out.resize(num);
  multimap<string,RefCountPtr<Condition> >::iterator startit = 
                                         condition_.lower_bound(name);
  multimap<string,RefCountPtr<Condition> >::iterator endit = 
                                         condition_.upper_bound(name);
  int count=0;
  multimap<string,RefCountPtr<Condition> >::iterator curr;
  for (curr=startit; curr!=endit; ++curr)
    out[count++] = curr->second.get();
  if (count != num) dserror("Mismatch in number of conditions found");
  return;
}

/*----------------------------------------------------------------------*
 |  Get a condition of a certain name                          (public) |
 |                                                            gee 12/06 |
 *----------------------------------------------------------------------*/
DRT::Condition* DRT::Node::GetCondition(const string& name)
{
  multimap<string,RefCountPtr<Condition> >::iterator curr = 
                                         condition_.find(name);
  if (curr==condition_.end()) return NULL;
  curr = condition_.lower_bound(name);
  return curr->second.get();
}










#endif  // #ifdef TRILINOS_PACKAGE
#endif  // #ifdef CCADISCRET
