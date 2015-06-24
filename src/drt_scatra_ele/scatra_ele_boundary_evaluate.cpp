/*!----------------------------------------------------------------------
\file scatra_ele_boundary_evaluate.cpp
\brief

Evaluate boundary conditions for scalar transport problems

<pre>
Maintainer: Andreas Ehrl
            ehrl@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15252
</pre>
 *----------------------------------------------------------------------*/
#include "../drt_mat/elchmat.H"

#include "scatra_ele_action.H"
#include "scatra_ele_boundary_calc.H"
#include "scatra_ele_boundary_factory.H"
#include "scatra_ele_parameter_elch.H"
#include "scatra_ele.H"


/*----------------------------------------------------------------------*
 |  evaluate the element (public)                             gjb 01/09 |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::TransportBoundary::Evaluate(
    Teuchos::ParameterList&   params,
    DRT::Discretization&      discretization,
    std::vector<int>&         lm,
    Epetra_SerialDenseMatrix& elemat1,
    Epetra_SerialDenseMatrix& elemat2,
    Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseVector& elevec2,
    Epetra_SerialDenseVector& elevec3)
{
  // we assume here, that numdofpernode is equal for every node within
  // the discretization and does not change during the computations
  const int numdofpernode = NumDofPerNode(*(Nodes()[0]));
  int numscal = numdofpernode;

  // perform additional operations specific to implementation type
  switch(ParentElement()->ImplType())
  {
  case INPAR::SCATRA::impltype_elch_diffcond:
  case INPAR::SCATRA::impltype_elch_diffcond_thermo:
  case INPAR::SCATRA::impltype_elch_electrode:
  case INPAR::SCATRA::impltype_elch_electrode_thermo:
  case INPAR::SCATRA::impltype_elch_NP:
  {
    // adapt number of transported scalars for electrochemistry problems
    numscal -= 1;

    // get the material of the first element
    // we assume here, that the material is equal for all elements in this discretization
    // get the parent element including its material
    Teuchos::RCP<MAT::Material> material = ParentElement()->Material();
    if (material->MaterialType() == INPAR::MAT::m_elchmat)
      numscal = static_cast<const MAT::ElchMat*>(material.get())->NumScal();

    break;
  }

  case INPAR::SCATRA::impltype_std:
  case INPAR::SCATRA::impltype_advreac:
  case INPAR::SCATRA::impltype_aniso:
  case INPAR::SCATRA::impltype_cardiac_monodomain:
  case INPAR::SCATRA::impltype_levelset:
  case INPAR::SCATRA::impltype_loma:
  case INPAR::SCATRA::impltype_poro:
  case INPAR::SCATRA::impltype_pororeac:
  case INPAR::SCATRA::impltype_thermo_elch_diffcond:
  case INPAR::SCATRA::impltype_thermo_elch_electrode:
    // do nothing in these cases
    break;

  default:
  {
    // other implementation types are invalid
    dserror("Invalid implementation type!");
    break;
  }
  }

  // all physics-related stuff is included in the implementation class that can
  // be used in principle inside any element (at the moment: only Transport
  // boundary element)
  // If this element has special features/ methods that do not fit in the
  // generalized implementation class, you have to do a switch here in order to
  // call element-specific routines
  return DRT::ELEMENTS::ScaTraBoundaryFactory::ProvideImpl(this,ParentElement()->ImplType(),numdofpernode,numscal)->Evaluate(
      this,
      params,
      discretization,
      lm,
      elemat1,
      elemat2,
      elevec1,
      elevec2,
      elevec3
      );
}


/*----------------------------------------------------------------------*
 | evaluate Neumann boundary condition on boundary element   fang 01/15 |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::TransportBoundary::EvaluateNeumann(
    Teuchos::ParameterList&   params,
    DRT::Discretization&      discretization,
    DRT::Condition&           condition,
    std::vector<int>&         lm,
    Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseMatrix* elemat1)
{
  // add Neumann boundary condition to parameter list
  params.set<DRT::Condition*>("condition",&condition);

  // evaluate boundary element
  return Evaluate(params,discretization,lm,*elemat1,*elemat1,elevec1,elevec1,elevec1);
}


/*----------------------------------------------------------------------*
 |  Get degrees of freedom used by this element                (public) |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::TransportBoundary::LocationVector(
    const Discretization&   dis,
    LocationArray&          la,
    bool                    doDirichlet,
    const std::string&      condstring,
    Teuchos::ParameterList& params
    ) const
{
  // check for the action parameter
  const SCATRA::BoundaryAction action = DRT::INPUT::get<SCATRA::BoundaryAction>(params,"action");
  switch (action)
  {
  case SCATRA::bd_calc_weak_Dirichlet:
    // special cases: the boundary element assembles also into
    // the inner dofs of its parent element
    // note: using these actions, the element will get the parent location vector
    //       as input in the respective evaluate routines
    ParentElement()->LocationVector(dis,la,doDirichlet);
    break;
  default:
    DRT::Element::LocationVector(dis,la,doDirichlet);
    break;
  }
  return;
}
