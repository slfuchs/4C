/*----------------------------------------------------------------------*/
/*!
\file ad_fld_fluid_ale.cpp

\brief

<pre>
Maintainer: Ulrich Kuettler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/
/*----------------------------------------------------------------------*/
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_inpar/drt_validparameters.H"
#include <Teuchos_StandardParameterEntryValidators.hpp>

#include "ad_fld_fluid_ale.H"
#include "ad_ale_fluid.H"

#include "../drt_fluid/fluid_utils_mapextractor.H"
#include "adapter_coupling.H"
#include "../drt_inpar/inpar_fsi.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
ADAPTER::FluidAle::FluidAle(const Teuchos::ParameterList& prbdyn,
                            std::string condname)
{
  Teuchos::RCP<ADAPTER::FluidBaseAlgorithm> fluid = Teuchos::rcp(new ADAPTER::FluidBaseAlgorithm(prbdyn,DRT::Problem::Instance()->FluidDynamicParams(),"fluid",true,false));
  fluid_ = fluid->FluidField();
  Teuchos::RCP<ADAPTER::AleNewBaseAlgorithm> ale = Teuchos::rcp(new ADAPTER::AleNewBaseAlgorithm(prbdyn,DRT::Problem::Instance()->GetDis("ale")));
  ale_ = Teuchos::rcp_dynamic_cast<ADAPTER::AleFluidWrapper>(ale->AleField());

//  if (ale_ == Teuchos::null)
//    dserror("Failed to cast to problem-specific ALE-wrapper");

  const int ndim = DRT::Problem::Instance()->NDim();

  // the fluid-ale coupling always matches
  const Epetra_Map* fluidnodemap = FluidField()->Discretization()->NodeRowMap();
  const Epetra_Map* alenodemap   = AleField()->Discretization()->NodeRowMap();

  coupfa_ = Teuchos::rcp(new Coupling());
  coupfa_->SetupCoupling(*FluidField()->Discretization(),
                         *AleField()->Discretization(),
                         *fluidnodemap,
                         *alenodemap,
                         ndim);

  //initializing the fluid is done later as for xfluids the first cut is done there (coupfa_ cannot be build anymore!!!)
  FluidField()->Init();
  fluid->SetInitialFlowField(DRT::Problem::Instance()->FluidDynamicParams()); //call from base algorithm

  icoupfa_ = Teuchos::rcp(new Coupling());
  icoupfa_->SetupConditionCoupling(*FluidField()->Discretization(),
                                    FluidField()->Interface()->FSICondMap(),
                                   *AleField()->Discretization(),
                                    AleField()->Interface()->FSICondMap(),
                                   condname,
                                   ndim);

  fscoupfa_ = Teuchos::rcp(new Coupling());
  fscoupfa_->SetupConditionCoupling(*FluidField()->Discretization(),
                                     FluidField()->Interface()->FSCondMap(),
                                    *AleField()->Discretization(),
                                     AleField()->Interface()->FSCondMap(),
                                    "FREESURFCoupling",
                                    ndim);

  aucoupfa_ = Teuchos::rcp(new Coupling());
  aucoupfa_->SetupConditionCoupling(*FluidField()->Discretization(),
                                     FluidField()->Interface()->AUCondMap(),
                                    *AleField()->Discretization(),
                                     AleField()->Interface()->AUCondMap(),
                                    "ALEUPDATECoupling",
                                    ndim);

  FluidField()->SetMeshMap(coupfa_->MasterDofMap());

  // the ale matrix might be build just once
  AleField()->CreateSystemMatrix();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Discretization> ADAPTER::FluidAle::Discretization()
{
  return FluidField()->Discretization();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidAle::PrepareTimeStep()
{
  FluidField()->PrepareTimeStep();
  AleField()->PrepareTimeStep();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidAle::Update()
{
  FluidField()->Update();
  AleField()->Update();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidAle::Output()
{
  FluidField()->StatisticsAndOutput();
  AleField()->Output();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double ADAPTER::FluidAle::ReadRestart(int step)
{
  FluidField()->ReadRestart(step);
  AleField()->ReadRestart(step);
  return FluidField()->Time();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidAle::NonlinearSolve(Teuchos::RCP<Epetra_Vector> idisp,
                                       Teuchos::RCP<Epetra_Vector> ivel)
{

  const Teuchos::ParameterList& fsidyn = DRT::Problem::Instance()->FSIDynamicParams();
  if (idisp!=Teuchos::null)
  {
    // if we have values at the interface we need to apply them
    AleField()->ApplyInterfaceDisplacements(FluidToAle(idisp));
    if (DRT::INPUT::IntegralValue<int>(fsidyn,"COUPALGO") != fsi_pseudo_structureale)
    {
      FluidField()->ApplyInterfaceVelocities(ivel);
    }
  }

  // Update the ale update part
  if (FluidField()->Interface()->AUCondRelevant())
  {
    Teuchos::RCP<const Epetra_Vector> dispnp = FluidField()->Dispnp();
    Teuchos::RCP<Epetra_Vector> audispnp = FluidField()->Interface()->ExtractAUCondVector(dispnp);
    AleField()->ApplyAleUpdateDisplacements(aucoupfa_->MasterToSlave(audispnp));
  }

  // Update the free-surface part
  if (FluidField()->Interface()->FSCondRelevant())
  {
    Teuchos::RCP<const Epetra_Vector> dispnp = FluidField()->Dispnp();
    Teuchos::RCP<Epetra_Vector> fsdispnp = FluidField()->Interface()->ExtractFSCondVector(dispnp);
    AleField()->ApplyFreeSurfaceDisplacements(fscoupfa_->MasterToSlave(fsdispnp));
  }

  // Note: We do not look for moving ale boundaries (outside the coupling
  // interface) on the fluid side. Thus if you prescribe time variable ale
  // Dirichlet conditions the according fluid Dirichlet conditions will not
  // notice.

  AleField()->Solve();
  Teuchos::RCP<Epetra_Vector> fluiddisp = AleToFluidField(AleField()->Dispnp());
  FluidField()->ApplyMeshDisplacement(fluiddisp);

  // no computation of fluid velocities in case only structure and ALE are to compute
  if (DRT::INPUT::IntegralValue<int>(fsidyn,"COUPALGO") != fsi_pseudo_structureale)
  {
    FluidField()->Solve();
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidAle::ApplyInterfaceValues(Teuchos::RCP<Epetra_Vector> idisp,
                                       Teuchos::RCP<Epetra_Vector> ivel)
{

  const Teuchos::ParameterList& fsidyn = DRT::Problem::Instance()->FSIDynamicParams();
  if (idisp!=Teuchos::null)
  {
    // if we have values at the interface we need to apply them
    AleField()->ApplyInterfaceDisplacements(FluidToAle(idisp));
    if (DRT::INPUT::IntegralValue<int>(fsidyn,"COUPALGO") != fsi_pseudo_structureale)
    {
      FluidField()->ApplyInterfaceVelocities(ivel);
    }
  }

  if (FluidField()->Interface()->FSCondRelevant())
  {
    Teuchos::RCP<const Epetra_Vector> dispnp = FluidField()->Dispnp();
    Teuchos::RCP<Epetra_Vector> fsdispnp = FluidField()->Interface()->ExtractFSCondVector(dispnp);
    AleField()->ApplyFreeSurfaceDisplacements(fscoupfa_->MasterToSlave(fsdispnp));
  }

  Teuchos::RCP<Epetra_Vector> fluiddisp = AleToFluidField(AleField()->Dispnp());
  FluidField()->ApplyMeshDisplacement(fluiddisp);

}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::RelaxationSolve(Teuchos::RCP<Epetra_Vector> idisp,
                                                               double dt)
{
  // Here we have a mesh position independent of the
  // given trial vector, but still the grid velocity depends on the
  // trial vector only.

  // grid velocity
  AleField()->ApplyInterfaceDisplacements(FluidToAle(idisp));

  AleField()->Solve();
  Teuchos::RCP<Epetra_Vector> fluiddisp = AleToFluidField(AleField()->Dispnp());
  fluiddisp->Scale(1./dt);

  FluidField()->ApplyMeshVelocity(fluiddisp);

  // grid position is done inside RelaxationSolve

  // the displacement -> velocity conversion at the interface
  idisp->Scale(1./dt);

  return FluidField()->RelaxationSolve(idisp);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::ExtractInterfaceForces()
{
  return FluidField()->ExtractInterfaceForces();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::ExtractInterfaceVelnp()
{
  return FluidField()->ExtractInterfaceVelnp();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::ExtractInterfaceVeln()
{
  return FluidField()->ExtractInterfaceVeln();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::IntegrateInterfaceShape()
{
  return FluidField()->IntegrateInterfaceShape();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::ResultTest> ADAPTER::FluidAle::CreateFieldTest()
{
  return FluidField()->CreateFieldTest();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::AleToFluidField(Teuchos::RCP<Epetra_Vector> iv) const
{
  return coupfa_->SlaveToMaster(iv);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::AleToFluidField(Teuchos::RCP<const Epetra_Vector> iv) const
{
  return coupfa_->SlaveToMaster(iv);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::FluidToAle(Teuchos::RCP<Epetra_Vector> iv) const
{
  return icoupfa_->MasterToSlave(iv);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidAle::FluidToAle(Teuchos::RCP<const Epetra_Vector> iv) const
{
  return icoupfa_->MasterToSlave(iv);
}
