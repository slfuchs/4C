/*!
\file gmsh.cpp

\brief simple element print library for Gmsh (debuging only)

<pre>
Maintainer: Axel Gerstenberger
            gerstenberger@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>
*/

#include <string>
#include <blitz/array.h>

#include "gmsh.H"
#include "../drt_lib/drt_node.H"
#include "../drt_lib/drt_utils.H"

string GMSH::elementToGmshString(DRT::Element* ele)
{
    //const int nen = ele->NumNode();
    DRT::Node** nodes = ele->Nodes();
    
    stringstream pos_array_string;
    pos_array_string << "SH(";
    int nendebug = 8;
    for (int i = 0; i<nendebug;++i)
    {
        const DRT::Node* node = nodes[i];
        const double* x = node->X();
        pos_array_string << scientific << x[0] << ",";
        pos_array_string << scientific << x[1] << ",";
        pos_array_string << scientific << x[2];
        if (i < nendebug-1){
            pos_array_string << ",";
        }
    };
    pos_array_string << ")";
    // values
    pos_array_string << "{";
    for (int i = 0; i<nendebug;++i)
    {
        pos_array_string << scientific << 0.0;
        if (i < nendebug-1){
            pos_array_string << ",";
        }
    };
    pos_array_string << "};";
    return pos_array_string.str();
};

string GMSH::intCellToGmshString(DRT::Element* ele, const XFEM::Integrationcell cell)
{
    stringstream pos_array_string;
    pos_array_string << "SS(";
    
    const int nen = 4;
    const DRT::Element::DiscretizationType distype = ele->Shape();
    
    // coordinates
    for (int inen = 0; inen < nen;++inen)
    {
        // shape functions
        Epetra_SerialDenseVector funct(27);
        DRT::Utils::shape_function_3D(funct,cell.GetCoord()[inen][0],
                                            cell.GetCoord()[inen][1],
                                            cell.GetCoord()[inen][2],
                                            distype);
        
        //interpolate position to x-space
        vector<double> x_interpol(3);
        for (int isd=0;isd < 3;++isd ){
            x_interpol[isd] = 0.0;
        }
        for (int inenparent = 0; inenparent < ele->NumNode();++inenparent)
        {
            for (int isd=0;isd < 3;++isd ){
                x_interpol[isd] += ele->Nodes()[inenparent]->X()[isd] * funct(inenparent);
            }
        }
        // print position in x-space
        pos_array_string << scientific << x_interpol[0] << ",";
        pos_array_string << scientific << x_interpol[1] << ",";
        pos_array_string << scientific << x_interpol[2];
        if (inen < nen-1){
            pos_array_string << ",";
        }
    };
    pos_array_string << ")";
    // values
    pos_array_string << "{";
    for (int i = 0; i<nen;++i)
    {
        pos_array_string << scientific << 0.0;
        if (i < nen-1){
            pos_array_string << ",";
        }
    };
    pos_array_string << "};";
    return pos_array_string.str();
};

