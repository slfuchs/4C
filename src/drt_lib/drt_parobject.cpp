/*!----------------------------------------------------------------------
\file drt_parobject.cpp
\brief

<pre>
Maintainer: Michael Gee
            gee@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15239
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "drt_parobject.H"



/*----------------------------------------------------------------------*
 |  ctor (public)                                            mwgee 11/06|
 *----------------------------------------------------------------------*/
DRT::ParObject::ParObject()
{
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       mwgee 11/06|
 *----------------------------------------------------------------------*/
DRT::ParObject::ParObject(const DRT::ParObject& old)
{
  return;
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            mwgee 11/06|
 *----------------------------------------------------------------------*/
DRT::ParObject::~ParObject()
{
  return;
}

/*----------------------------------------------------------------------*
 |      a vector<int> specialization                           (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::AddtoPack(vector<char>& data, const vector<int>& stuff)
{
  int numele = stuff.size();
  AddtoPack(data,numele);
  AddtoPack(data,&stuff[0],numele*sizeof(int));
  return;
}
/*----------------------------------------------------------------------*
 |      a vector<double> specialization                        (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::AddtoPack(vector<char>& data, const vector<double>& stuff)
{
  int numele = stuff.size();
  AddtoPack(data,numele);
  AddtoPack(data,&stuff[0],numele*sizeof(double));
  return;
}
/*----------------------------------------------------------------------*
 |        a vector<char> specialization                        (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::AddtoPack(vector<char>& data, const vector<char>& stuff)
{
  int numele = stuff.size();
  AddtoPack(data,numele);
  AddtoPack(data,&stuff[0],numele*sizeof(char));
  return;
}
/*----------------------------------------------------------------------*
 | a Epetra_SerialDenseMatrix specialization                   (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::AddtoPack(vector<char>& data, const Epetra_SerialDenseMatrix& stuff)
{
  int m = stuff.M();
  int n = stuff.N();
  AddtoPack(data,m);
  AddtoPack(data,n);
  double* A = stuff.A();
  AddtoPack(data,A,n*m*sizeof(double));
  return;
}
/*----------------------------------------------------------------------*
 | a Epetra_SerialDenseVector specialization                   (public) |
 |                                                     TK & MAF  05/08  |
 *----------------------------------------------------------------------*/
void DRT::ParObject::AddtoPack(vector<char>& data, const Epetra_SerialDenseVector& stuff)
{
  int m = stuff.Length();
  AddtoPack(data,m);
  double* A = stuff.Values();
  AddtoPack(data,A,m*sizeof(double));
  return;
}
/*----------------------------------------------------------------------*
 | a string specialization                                     (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::AddtoPack(vector<char>& data, const string& stuff)
{
  int numele = stuff.size();
  AddtoPack(data,numele);
  AddtoPack(data,&stuff[0],numele*sizeof(char));
  return;
}
/*----------------------------------------------------------------------*
 | a int vector specialization                                 (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::ExtractfromPack(int& position, const vector<char>& data, vector<int>& stuff)
{
  int dim = 0;
  ExtractfromPack(position,data,dim);
  stuff.resize(dim);
  int size = dim*sizeof(int);
  ExtractfromPack(position,data,&stuff[0],size);
  return;
}
/*----------------------------------------------------------------------*
 | a double vector specialization                                 (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::ExtractfromPack(int& position, const vector<char>& data, vector<double>& stuff)
{
  int dim = 0;
  ExtractfromPack(position,data,dim);
  stuff.resize(dim);
  int size = dim*sizeof(double);
  ExtractfromPack(position,data,&stuff[0],size);
  return;
}
/*----------------------------------------------------------------------*
 | a char vector specialization                                 (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::ExtractfromPack(int& position, const vector<char>& data, vector<char>& stuff)
{
  int dim = 0;
  ExtractfromPack(position,data,dim);
  stuff.resize(dim);
  int size = dim*sizeof(char);
  ExtractfromPack(position,data,&stuff[0],size);
  return;
}
/*----------------------------------------------------------------------*
 | a Epetra_SerialDenseMatrix specialization                   (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::ExtractfromPack(int& position, const vector<char>& data,
                                     Epetra_SerialDenseMatrix& stuff)
{
  int m = 0;
  ExtractfromPack(position,data,m);
  int n = 0;
  ExtractfromPack(position,data,n);
  stuff.Reshape(m,n);
  double* a = stuff.A();
  if(m*n)
    ExtractfromPack(position,data,a,n*m*sizeof(double));
  return;
}
/*----------------------------------------------------------------------*
 | a Epetra_SerialDenseVector specialization                   (public) |
 |                                                     TK & MAF  05/08  |
 *----------------------------------------------------------------------*/
void DRT::ParObject::ExtractfromPack(int& position, const vector<char>& data,
                                     Epetra_SerialDenseVector& stuff)
{
  int m = 0;
  ExtractfromPack(position,data,m);
  stuff.Resize(m);
  double* a = stuff.Values();
  if(m)
    ExtractfromPack(position,data,a,m*sizeof(double));
  return;
}


/*----------------------------------------------------------------------*
 | a string specialization                                     (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::ParObject::ExtractfromPack(int& position, const vector<char>& data, string& stuff)
{
  int dim = 0;
  ExtractfromPack(position,data,dim);
  stuff.resize(dim);
  int size = dim*sizeof(char);
  ExtractfromPack(position,data,&stuff[0],size);
  return;
}



#endif  // #ifdef CCADISCRET
