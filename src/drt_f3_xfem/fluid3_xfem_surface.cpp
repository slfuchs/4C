/*!----------------------------------------------------------------------
\file fluid3_surface.cpp
\brief

<pre>
Maintainer: Axel Gerstenberger
            gerstenberger@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>

*----------------------------------------------------------------------*/
#ifdef D_FLUID3_XFEM
#ifdef CCADISCRET
#ifdef TRILINOS_PACKAGE

#include "fluid3_xfem.H"
#include "../drt_lib/linalg_utils.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_dserror.H"



/*----------------------------------------------------------------------*
 |  ctor (public)                                            mwgee 01/07|
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
DRT::Elements::XFluid3Surface::XFluid3Surface(int id,
                                              int owner,
                                              int nnode,
                                              const int* nodeids,
                                              DRT::Node** nodes,
                                              DRT::Elements::XFluid3* parent,
                                              const int lsurface) :
DRT::Element(id,element_xfluid3surface,owner),
parent_(parent),
lsurface_(lsurface)
{
  lines_.resize(0);
  lineptrs_.resize(0);	
  SetNodeIds(nnode,nodeids);
  BuildNodalPointers(nodes);
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       mwgee 01/07|
 *----------------------------------------------------------------------*/
DRT::Elements::XFluid3Surface::XFluid3Surface(const DRT::Elements::XFluid3Surface& old) :
DRT::Element(old),
parent_(old.parent_),
lsurface_(old.lsurface_),
lines_(old.lines_),
lineptrs_(old.lineptrs_)
{
  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance return pointer to it               (public) |
 |                                                            gee 01/07 |
 *----------------------------------------------------------------------*/
DRT::Element* DRT::Elements::XFluid3Surface::Clone() const
{
  DRT::Elements::XFluid3Surface* newelement = new DRT::Elements::XFluid3Surface(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 |                                                             (public) |
 |                                                          u.kue 03/07 |
 *----------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::Elements::XFluid3Surface::Shape() const
{
  switch (NumNode())
  {
		case 3: return tri3;
  		case 4: return quad4;
  		case 6: return tri6;
  		case 8: return quad8;
  		case 9: return quad9;
  		default:
    		dserror("unexpected number of nodes %d", NumNode());
  }
  return dis_none;
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::Elements::XFluid3Surface::Pack(vector<char>& data) const
{
  data.resize(0);
  dserror("this Fluid3Surface element does not support communication");

  return;
}

/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                            gee 02/07 |
 *----------------------------------------------------------------------*/
void DRT::Elements::XFluid3Surface::Unpack(const vector<char>& data)
{
  dserror("this XFluid3Surface element does not support communication");
  return;
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            mwgee 01/07|
 *----------------------------------------------------------------------*/
DRT::Elements::XFluid3Surface::~XFluid3Surface()
{
  return;
}


/*----------------------------------------------------------------------*
 |  print this element (public)                              mwgee 01/07|
 *----------------------------------------------------------------------*/
void DRT::Elements::XFluid3Surface::Print(ostream& os) const
{
  os << "XFluid3Surface ";
  Element::Print(os);
  return;
}


/*----------------------------------------------------------------------*
 |  get vector of lines (public)                             gammi 04/07|
 *----------------------------------------------------------------------*/
DRT::Element** DRT::Elements::XFluid3Surface::Lines()
{
    const DiscretizationType distype = Shape();
	const int nline   = NumLine();
  	lines_.resize(nline);
  	lineptrs_.resize(nline);

    switch (distype)
    {
    case tri3:
        CreateLinesTri(3, 2);
        break;
    case tri6:
        CreateLinesTri(3, 3);
        break;
    case quad4:
        CreateLinesQuad(4, 2);
        break;
    case quad8:
        CreateLinesQuad(4, 3);
        break;
    case quad9:
        CreateLinesQuad(4, 3);
        break;
    default:
        dserror("distype not supported");
    }

  	return (DRT::Element**)(&(lineptrs_[0]));
}



void DRT::Elements::XFluid3Surface::CreateLinesTri(const int& nline,
                                                   const int& nnode)
{
    for(int iline=0;iline<nline;iline++)
    {
        int nodeids[nnode];
        DRT::Node* nodes[nnode];
        
        for (int inode=0;inode<2;inode++)
        {
             nodeids[inode] = NodeIds()[tri6_lines_[iline][inode]];
             nodes[inode]   = Nodes()[tri6_lines_[iline][inode]];
        }
        lines_[iline] = rcp(new DRT::Elements::XFluid3Line(iline,Owner(),nnode,nodeids,nodes,this,NULL,iline));
        lineptrs_[iline] = lines_[iline].get();
    }
}        

void DRT::Elements::XFluid3Surface::CreateLinesQuad(const int& nline,
                                                    const int& nnode)
{
    for(int iline=0;iline<nline;iline++)
    {
        int nodeids[nnode];
        DRT::Node* nodes[nnode];
        
        for (int inode=0;inode<2;inode++)
        {
             nodeids[inode] = NodeIds()[quad9_lines_[iline][inode]];
             nodes[inode]   = Nodes()[quad9_lines_[iline][inode]];
        }
        lines_[iline] = rcp(new DRT::Elements::XFluid3Line(iline,Owner(),nnode,nodeids,nodes,this,NULL,iline));
        lineptrs_[iline] = lines_[iline].get();
    }
}    

#endif  // #ifdef TRILINOS_PACKAGE
#endif  // #ifdef CCADISCRET
#endif // #ifdef D_FLUID3
