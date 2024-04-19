/*----------------------------------------------------------------------*/
/*! \file

\brief Solve FSI problems using a Dirichlet-Neumann partitioning approach
       with sliding ALE-structure interfaces



\level 1

*/
/*----------------------------------------------------------------------*/


#include "baci_fsi_dirichletneumannslideale.hpp"

#include "baci_adapter_str_fsiwrapper.hpp"
#include "baci_coupling_adapter.hpp"
#include "baci_coupling_adapter_mortar.hpp"
#include "baci_discretization_geometry_searchtree.hpp"
#include "baci_fsi_debugwriter.hpp"
#include "baci_fsi_utils.hpp"
#include "baci_global_data.hpp"
#include "baci_inpar_fsi.hpp"
#include "baci_mortar_interface.hpp"

#include <Teuchos_StandardParameterEntryValidators.hpp>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
FSI::DirichletNeumannSlideale::DirichletNeumannSlideale(const Epetra_Comm& comm)
    : DirichletNeumann(comm)
{
  // empty constructor
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::DirichletNeumannSlideale::Setup()
{
  // call setup of base class
  FSI::DirichletNeumann::Setup();

  const Teuchos::ParameterList& fsidyn = GLOBAL::Problem::Instance()->FSIDynamicParams();
  const Teuchos::ParameterList& fsipart = fsidyn.sublist("PARTITIONED SOLVER");
  SetKinematicCoupling(
      CORE::UTILS::IntegralValue<int>(fsipart, "COUPVARIABLE") == INPAR::FSI::CoupVarPart::disp);

  INPAR::FSI::SlideALEProj aletype = CORE::UTILS::IntegralValue<INPAR::FSI::SlideALEProj>(
      GLOBAL::Problem::Instance()->FSIDynamicParams(), "SLIDEALEPROJ");

  slideale_ = Teuchos::rcp(new FSI::UTILS::SlideAleUtils(StructureField()->Discretization(),
      MBFluidField()->Discretization(), StructureFluidCouplingMortar(), true, aletype));

  islave_ = Teuchos::rcp(new Epetra_Vector(*StructureFluidCouplingMortar().SlaveDofMap(), true));
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FSI::DirichletNeumannSlideale::Remeshing()
{
  // dispn and dispnp of structure, used for surface integral and velocity of the fluid in the
  // interface
  Teuchos::RCP<Epetra_Vector> idisptotal = StructureField()->ExtractInterfaceDispnp();

  slideale_->Remeshing(*StructureField(), MBFluidField()->Discretization(), idisptotal, islave_,
      StructureFluidCouplingMortar(), Comm());

  // Evaluate solid/fluid Mortar coupling
  slideale_->EvaluateMortar(
      StructureField()->ExtractInterfaceDispnp(), islave_, StructureFluidCouplingMortar());
  // Evaluate solid/ale Mortar coupling
  slideale_->EvaluateFluidMortar(idisptotal, islave_);

  Teuchos::RCP<Epetra_Vector> unew =
      slideale_->InterpolateFluid(MBFluidField()->ExtractInterfaceVelnp());
  MBFluidField()->ApplyInterfaceValues(islave_, unew);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::DirichletNeumannSlideale::FluidOp(
    Teuchos::RCP<Epetra_Vector> idispcurr, const FillType fillFlag)
{
  FSI::Partitioned::FluidOp(idispcurr, fillFlag);

  if (fillFlag == User)
  {
    FOUR_C_THROW("not implemented");
    // SD relaxation calculation
    return FluidToStruct(MBFluidField()->RelaxationSolve(StructToFluid(idispcurr), Dt()));
  }
  else
  {
    // normal fluid solve

    // the displacement -> velocity conversion at the interface
    const Teuchos::RCP<Epetra_Vector> ivel = InterfaceVelocity(idispcurr);

    // A rather simple hack. We need something better!
    const int itemax = MBFluidField()->Itemax();
    if (fillFlag == MF_Res and mfresitemax_ > 0) MBFluidField()->SetItemax(mfresitemax_ + 1);

    // new Epetra_Vector for aledisp in interface
    Teuchos::RCP<Epetra_Vector> iale =
        Teuchos::rcp(new Epetra_Vector(*(StructureFluidCouplingMortar().MasterDofMap()), true));

    Teuchos::RCP<Epetra_Vector> idispn = StructureField()->ExtractInterfaceDispn();

    iale->Update(1.0, *idispcurr, 0.0);

    // iale reduced by old displacement dispn and instead added the real last displacements
    iale->Update(1.0, *FTStemp_, -1.0, *idispn, 1.0);

    MBFluidField()->NonlinearSolve(StructToFluid(iale), StructToFluid(ivel));

    MBFluidField()->SetItemax(itemax);

    return FluidToStruct(MBFluidField()->ExtractInterfaceForces());
  }
}
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::DirichletNeumannSlideale::StructOp(
    Teuchos::RCP<Epetra_Vector> iforce, const FillType fillFlag)
{
  FSI::Partitioned::StructOp(iforce, fillFlag);

  if (fillFlag == User)
  {
    // SD relaxation calculation
    return StructureField()->RelaxationSolve(iforce);
  }
  else
  {
    // normal structure solve
    StructureField()->ApplyInterfaceForces(iforce);
    StructureField()->Solve();
    return StructureField()->ExtractInterfaceDispnp();
  }
}
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FSI::DirichletNeumannSlideale::InitialGuess()
{
  if (GetKinematicCoupling())
  {
    // real displacement of slave side at time step begin on master side --> for calcualtion of
    // FluidOp
    FTStemp_ = FluidToStruct(islave_);
    // predict displacement
    return StructureField()->PredictInterfaceDispnp();
  }
  else
  {
    const Teuchos::ParameterList& fsidyn = GLOBAL::Problem::Instance()->FSIDynamicParams();
    const Teuchos::ParameterList& fsipart = fsidyn.sublist("PARTITIONED SOLVER");
    if (CORE::UTILS::IntegralValue<int>(fsipart, "PREDICTOR") != 1)
    {
      FOUR_C_THROW(
          "unknown interface force predictor '%s'", fsipart.get<std::string>("PREDICTOR").c_str());
    }
    return InterfaceForce();
  }
}

FOUR_C_NAMESPACE_CLOSE
