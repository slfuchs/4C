/*----------------------------------------------------------------------*/
/*! \file
\brief Robinson's visco-plastic material

      The visco-plastic Robinson's material is only admissible in small strain
      regime. So we can use this material for geometrically linear and
      geometrically nonlinear analysis.
      In original implementation, on material level it is decided if calculation
      uses linear or Green-Lagrange strains
      --> In 4C a strain vector is passed to material, that can be linear or
      Green-Lagrange strain

      example input line:
      MAT 1 MAT_Struct_Robinson  KIND Arya_NarloyZ  YOUNG POLY 2 1.47e9 -7.05e5
        NUE 0.34  DENS 8.89e-3  THEXPANS 0.0  INITTEMP 293.15  HRDN_FACT 3.847e-12  HRDN_EXPO 4.0
        SHRTHRSHLD POLY 2 69.88e8 -0.067e8   RCVRY 6.083e-3  ACTV_ERGY 40000.0
        ACTV_TMPR 811.0  G0 0.04  M_EXPO 4.365  BETA POLY 3 0.8 0.0 0.533e-6
        H_FACT 1.67e16
      12.01.12 as first step only implement geometric non-linear case, i.e., total Lagrange

\level 2

*/
/*----------------------------------------------------------------------*/
#ifndef FOUR_C_MAT_ROBINSON_HPP
#define FOUR_C_MAT_ROBINSON_HPP


#include "4C_config.hpp"

#include "4C_comm_parobjectfactory.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_linalg_serialdensevector.hpp"
#include "4C_mat_so3_material.hpp"
#include "4C_material_parameter_base.hpp"

#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN


namespace MAT
{
  namespace PAR
  {
    /*----------------------------------------------------------------------*/
    //! material parameters for visco-plastic Robinson's material
    class Robinson : public CORE::MAT::PAR::Parameter
    {
     public:
      //! standard constructor
      Robinson(Teuchos::RCP<CORE::MAT::PAR::Material> matdata);

      //! @name material parameters
      //@{

      //! kind of Robinson material (slight
      //! differences:vague,butler,arya,arya_narloyz,arya_crmosteel)
      const std::string kind_;
      //! Young's modulus (temperature dependent --> polynomial expression)
      // 'E' [N/m^2]
      const std::vector<double> youngs_;
      //! Possion's ratio \f$ \nu \f$ [-]
      const double poissonratio_;
      //! mass density \f$ \rho [kg/m^3] \f$
      const double density_;
      //! linear coefficient of thermal expansion \f$ \alpha_T \f$ [1/K]
      const double thermexpans_;
      /// initial temperature (constant) \f$ \theta_0  \f$ [K]
      const double inittemp_;
      //! hardening factor 'A' (needed for flow law) [1/s]
      const double hrdn_fact_;
      //! hardening power 'n'  (exponent of F in the flow law) [-]
      const double hrdn_expo_;
      //! Bingam-Prager shear stress threshold \f$ \kappa^2 \f$
      //! 'K^2=K^2(K_0)' [N^2 / m^4]
      const std::vector<double> shrthrshld_;
      //! recovery factor 'R_0' [N/(s . m^2)]
      const double rcvry_;
      //! activation energy 'Q_0' for Arya_NARloy-Z [1/s]
      const double actv_ergy_;
      //! activation temperature 'T_0' [K]
      const double actv_tmpr_;
      //! 'G_0' (temperature independent, minimum value attainable by G )  [-]
      const double g0_;
      //! 'm'  [-]
      //! temperature independent, exponent in evolution law for back stress
      const double m_;
      //! '\f$\beta\f$' [-] (temperature independent)
      //! Arya_NarloyZ: \f$\beta = 0.533e-6 T^2 + 0.8\f$
      const std::vector<double> beta_;
      //! H
      //! Arya_NarloyZ: \f$H = 1.67e4 . (6.895)^(beta - 1) / (3 . K_0^2)\f$ [N^3/m^6]
      //! Arya_CrMoSteel: [N/m^2]
      const double h_;

      //@}

      //! create material instance of matching type with my parameters
      Teuchos::RCP<CORE::MAT::Material> CreateMaterial() override;

    };  // class Robinson

  }  // namespace PAR


  class RobinsonType : public CORE::COMM::ParObjectType
  {
   public:
    std::string Name() const override { return "RobinsonType"; }

    static RobinsonType& Instance() { return instance_; };

    CORE::COMM::ParObject* Create(const std::vector<char>& data) override;

   private:
    static RobinsonType instance_;

  };  // RobinsonType


  /*----------------------------------------------------------------------*/
  //! wrapper for visco-plastic Robinson's material
  class Robinson : public So3Material
  {
   public:
    //! construct empty material object
    Robinson();

    //! construct the material object given material parameters
    explicit Robinson(MAT::PAR::Robinson* params);

    //! @name Packing and Unpacking

    //!  \brief return unique ParObject id
    //!
    //!  every class implementing ParObject needs a unique id defined at the
    //!  top of parobject.H (this file) and should return it in this method.
    int UniqueParObjectId() const override { return RobinsonType::Instance().UniqueParObjectId(); }

    //!  \brief Pack this class so it can be communicated
    //!
    //!  Resizes the vector data and stores all information of a class in it.
    //!  The first information to be stored in data has to be the
    //!  unique parobject id delivered by UniqueParObjectId() which will then
    //!  identify the exact class on the receiving processor.
    //!
    void Pack(CORE::COMM::PackBuffer& data  //!< (i/o): char vector to store class information
    ) const override;

    //!  \brief Unpack data from a char vector into this class
    //!
    //!  The vector data contains all information to rebuild the
    //!  exact copy of an instance of a class on a different processor.
    //!  The first entry in data has to be an integer which is the unique
    //!  parobject id defined at the top of this file and delivered by
    //!  UniqueParObjectId().
    //!
    void Unpack(const std::vector<char>&
            data  //!< (i) vector storing all data to be unpacked into this instance
        ) override;

    //@}

    //! material type
    CORE::Materials::MaterialType MaterialType() const override
    {
      return CORE::Materials::m_vp_robinson;
    }

    /// check if element kinematics and material kinematics are compatible
    void ValidKinematics(INPAR::STR::KinemType kinem) override
    {
      if (!(kinem == INPAR::STR::KinemType::linear))
        FOUR_C_THROW("element and material kinematics are not compatible");
    }

    //! return copy of this material object
    Teuchos::RCP<CORE::MAT::Material> Clone() const override
    {
      return Teuchos::rcp(new Robinson(*this));
    }

    //! initialise internal stress variables
    void Setup(const int numgp,  //!< number of Gauss points
        INPUT::LineDefinition* linedef) override;

    //! update internal stress variables
    void Update() override;

    void Evaluate(const CORE::LINALG::Matrix<3, 3>* defgrd,
        const CORE::LINALG::Matrix<6, 1>* glstrain, Teuchos::ParameterList& params,
        CORE::LINALG::Matrix<6, 1>* stress, CORE::LINALG::Matrix<6, 6>* cmat, int gp,
        int eleGID) override;

    //! computes Cauchy stress
    void Stress(const double p,                                   //!< volumetric stress tensor
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>& devstress,  //!< deviatoric stress tensor
        CORE::LINALG::Matrix<NUM_STRESS_3D, 1>& stress            //!< 2nd PK-stress
    );

    //! computes relative stress eta = stress - back stress
    void RelDevStress(
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>& devstress,  //!< deviatoric stress tensor
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&,            //!< back stress tensor
        CORE::LINALG::Matrix<NUM_STRESS_3D, 1>& eta               //!< relative stress
    );

    //! computes isotropic elasticity tensor in matrix notion for 3d
    void SetupCmat(double temp,                                   //!< current temperature
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>& cmat  //!< material tangent
    );

    //! \brief calculate visco-plastic strain rate governed by the evolution law
    void CalcBEViscousStrainRate(const double dt,  //!< (i) time step size
        double tempnp,                             //!< (i) current temperature
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            strain_p,  //!< (i) viscous strain \f$\varepsilon^v_n\f$ at t_n
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            strain_pn,  //!< (i) viscous strain \f$\varepsilon^v_{n+1}\f$ at t_n at t_{n+1}^<i>
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            devstress,  //!< (i) stress deviator \f$s_n\f$ at t_{n+1}^<i>
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            eta,  //!< (i) over stress/relative stress \f$\eta_{n+1}\f$ at t_{n+1}^<i>
        CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            strain_pres,  //!< (o) viscous strain residual \f$f_{res}^v\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kve,  //!< (o) \f$\dfrac{\partial f_{res}^v}{\partial \Delta\varepsilon}\f$
                  //!< tangent of viscous strain residual with respect to total strain inc eps
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kvv,  //!< (o) \f$\dfrac{\partial f_{res}^v}{\partial \Delta\varepsilon^v}\f$
                  //!<  tangent of viscous strain residual with respec to viscous strains iinc eps^v
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kva  //!< (o) \f$\dfrac{\partial f_{res}^v}{\partial \Delta\alpha}\f$
                 //!< tangent of viscous strain residual with respect to back stresses iinc al
    );

    //! \brief residual of BE-discretised back stress according to the flow rule
    //!        at Gauss point
    void CalcBEBackStressFlow(const double dt,  //!< (i) time step size
        const double tempnp,                    //!< (i) current temperature at t_{n+1}
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            strain_p,  //!< (i) viscous strain \f$\varepsilon_{n}\f$ at t_n^i
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            strain_pn,  //!< (i) viscous strain \f$\varepsilon_{n+1}\f$ at t_{n+1}^i
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            devstress,  //!< (i) deviatoric stress \f$s_{n+1}\f$ at t_{n+1}^i
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            backstress,  //!<  (i)back stress \f$\alpha_{n}\f$  at t_{n}^i,
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            backstress_n,  //!< (i) back stress \f$\alpha_{n+1}\f$ at t_{n+1}^i,
        CORE::LINALG::Matrix<NUM_STRESS_3D, 1>& backstress_res,  //!< (o) back stress residual
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kae,  //!< (o) \f$\dfrac{\partial f_{res}^{al}}{\partial \Delta\varepsilon}\f$
                  //!< tangent of back stress residual with respect to total strain inc eps
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kav,  //!< (o) \f$\dfrac{\partial f_{res}^{al}}{\partial \Delta\varepsilon^v}\f$
                  //!< tangent of back stress residual with respect to viscous strains iinc eps^v
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kaa  //!< (o) \f$\dfrac{\partial f_{res}^{al}}{\partial \Delta\alpha}\f$
                 //!< tangent of back stress residual with respect to back stresses iinc al
    );

    //! Reduce (statically condense) system in eps,eps^v,al to purely eps
    /*!
    The linearised stress and internal residuals are

          [ sig   ]         [ sig    ]^i
      Lin [ res^v ]       = [ res^v  ]
          [ res^al]_{n+1}   [ res^al ]_{n+1}

                               [ kee  kev  kea ]^i  [ iinc eps   ]^i
                            +  [ kve  kvv  kva ]    [ iinc eps^v ]
                               [ kae  kav  kaa ]    [ iinc al    ]_{n+1}

                            [ sig ]
                          = [  0  ]  on every element (e)
                            [  0  ]  and at each Gauss point gp

    with - total strain increment/residual strains  iinc eps   -->  straininc
         - viscous strain increment                 iinc eps^v -->  strain_pn
         - back stress increment                    iinc al    -->  backstress
         - material tangent                         kee        -->  cmat

         - kee = dsigma / d eps = cmat,  kev = dsigma / d eps^v, kea = dsigma / d alpha
         - kve = dres^v / d eps, kvv = dres^v / d eps^v, kva = dres^v / d alpha,
         - kae = dres^al / d eps, kav = dres^al / d eps^v, kaa = dres^al / d alpha,

    Due to the fact that the internal residuals (the BE-discretised evolution
    laws of the viscous strain and the back stress) are C^{-1}-continuous
    across element boundaries. We can statically condense this system.
    The iterative increments inc eps^v and inc al are expressed in inc eps.
    We achieve

      [ iinc eps^v ]   [ kvv  kva ]^{-1} (   [ res^v  ]   [ kve ]                )
      [            ] = [          ]      ( - [        ] - [     ] . [ iinc eps ] )
      [ iinc al    ]   [ kav  kaa ]      (   [ res^al ]   [ kae ]                )

    thus

                                         [ kvv  kva ]^{-1} [ res^v  ]^i
      sig_red^i = sig^i - [ kev  kea ]^i [          ]      [        ]
                                         [ kav  kaa ]      [ res^al ]

    and
                                         [ kvv  kva ]^{-1} [ kve ]^i
      kee_red^i = kee^i - [ kev  kea ]^i [          ]      [     ]
                                         [ kav  kaa ]      [ kae ]

      ==> condensed system:

      Lin sig = kee_red^i . iinc eps + sig_red^i

    */
    void CalculateCondensedSystem(
        CORE::LINALG::Matrix<NUM_STRESS_3D, 1>& stress,  //!< (6x1) (io) stress vector \f$\sigma\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            cmat,  //!< cmat == kee (6x6) (io) material stiffness matrix, constitutive tensor
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kev,  //!< (6x6) (i) \f$\dfrac{\partial \sigma}{\partial \varepsilon^v}\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kea,  //!< (6x6) (i) \f$\dfrac{\partial \sigma}{\partial \alpha}\f$
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            strain_pres,  //!< (6x1) (i) viscous strain residual
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kve,  //!< (6x6) (i) \f$\dfrac{\partial f_{res}^{v}}{\partial \varepsilon}\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kvv,  //!< (6x6) (i) \f$\dfrac{\partial f_{res}^{v}}{\partial \varepsilon^v}\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kva,  //!< (6x6) (i) \f$\dfrac{\partial f_{res}^{v}}{\partial \alpha}\f$
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1>&
            backstress_res,  //!< (6x1) (i) backstress residual
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kae,  //!< (6x6) (i) \f$\dfrac{\partial f_{res}^{\alpha}}{\partial \varepsilon}\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kav,  //!< (6x6) (i) \f$\dfrac{\partial f_{res}^{\alpha}}{\partial \varepsilon^v}\f$
        CORE::LINALG::Matrix<NUM_STRESS_3D, NUM_STRESS_3D>&
            kaa,  //!< (6x6) (i) \f$\dfrac{\partial f_{res}^{\alpha}}{\partial \alpha}\f$
        CORE::LINALG::Matrix<(2 * NUM_STRESS_3D), 1>&
            kvarva,  //!< (12x1) (o) condensed matrix of residual
        CORE::LINALG::Matrix<(2 * NUM_STRESS_3D), NUM_STRESS_3D>&
            kvakvae  //!< (12x6) (o) condensed matrix of tangent
    );

    //! \brief iterative update of material internal variables
    //!
    //! material internal variables (viscous strain and back stress) are updated by
    //! their iterative increments.
    //! Their iterative increments are expressed in terms of the iterative increment
    //! of the total strain.
    //! Here the reduction matrices (kvarvam,kvakvae) stored at previous call of
    //! CalculateCondensedSystem() care used.
    //!
    //! strainplcurr_ = strainpllast_ + Delta strain_p (o)
    //! backstresscurr_ = backstresslast_ + Delta backstress (o)
    void IterativeUpdateOfInternalVariables(const int numgp,    //!< total number of Gauss points
        const CORE::LINALG::Matrix<NUM_STRESS_3D, 1> straininc  //!< (i) increment of total strain
    );

    //! return density
    double Density() const override { return params_->density_; }

    //! check if history variables are already initialised
    bool Initialized() const { return (isinit_ and (strainplcurr_ != Teuchos::null)); }

    //! return quick accessible material parameter data
    CORE::MAT::PAR::Parameter* Parameter() const override { return params_; }

    //! flag plastic step was called
    bool plastic_step;

    //! @name temperature specific methods
    //@{

    //! calculate temperature dependent material parameter and return value
    double GetMatParameterAtTempnp(
        const std::vector<double>* paramvector,  //!< (i) given parameter is a vector
        const double& tempnp                     //!< (i) current temperature
    );

    //! calculate temperature dependent material parameter
    double GetMatParameterAtTempnp(const double paramconst,  //!< (i) given parameter is a constant
        const double& tempnp                                 //!< (i) current temperature
    );

    //! Initial temperature \f$ \theta_0 \f$
    double InitTemp() const { return params_->inittemp_; }

    //@}

    //! @name specific methods for TSI and plastic material
    //@{

    //! material call to determine stress and constitutive tensor ctemp
    void Evaluate(
        const CORE::LINALG::Matrix<1, 1>& Ntemp,  //!< scalar-valued temperature of curr. element
        CORE::LINALG::Matrix<6, 1>& ctemp,        //!< temperature dependent material tangent
        CORE::LINALG::Matrix<6, 1>& stresstemp    //!< stress term dependent on temperature
    );

    //@}

   private:
    //! my material parameters
    MAT::PAR::Robinson* params_;

    //! indicator if #Initialize routine has been called
    bool isinit_;

    //! robinson's material requires the following internal variables:
    //! - visco-plastic strain vector (at t_n, t_n+1^i)
    //! - back stress vector (at t_n, t_n+1^i)
    //! - scaled residual --> for condensation of the system
    //! - scaled tangent --> for condensation of the system
    //!
    //! visco-plastic strain vector Ev^{gp} at t_{n} for every Gauss point gp
    //!    Ev^{gp,T} = [ E_11  E_22  E_33  2*E_12  2*E_23  2*E_31 ]^{gp} */
    //!< \f${\varepsilon}^p_{n}\f$
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<NUM_STRESS_3D, 1>>> strainpllast_;
    //! current visco-plastic strain vector Ev^{gp} at t_{n+1} for every Gauss point gp
    //!    Ev^{gp,T} = [ E_11  E_22  E_33  2*E_12  2*E_23  2*E_31 ]^{gp} */
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<NUM_STRESS_3D, 1>>>
        strainplcurr_;  //!< \f${\varepsilon}^p_{n+1}\f$
    //! old back stress vector Alpha^{gp} at t_n for every Gauss point gp
    //!    Alpha^{gp,T} = [ A_11  A_22  A_33  A_12  A_23  A_31 ]^{gp}
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<NUM_STRESS_3D, 1>>>
        backstresslast_;  //!< \f${\alpha}_{n}\f$
    //! current back stress vector Alpha^{gp} at t_{n+1} for every Gauss point gp
    //!< \f${\alpha}_{n+1}\f$
    //!    Alpha^{gp,T} = [ A_11  A_22  A_33  A_12  A_23  A_31 ]^{gp} */
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<NUM_STRESS_3D, 1>>>
        backstresscurr_;  //!< \f${\alpha}_{n+1}\f$
    //! update vector for MIV iterative increments
    //!          [ kvv  kva ]^{-1}   [ res^v  ]
    //! kvarva = [          ]      . [        ]
    //!          [ kav  kaa ]      . [ res^al ]
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<(2 * NUM_STRESS_3D), 1>>> kvarva_;
    //! update matrix for MIV iterative increments
    //!              [ kvv  kva ]^{-1}   [ kve ]
    //!    kvakvae = [          ]      . [     ]
    //!              [ kav  kaa ]      . [ kae ]
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<(2 * NUM_STRESS_3D), NUM_STRESS_3D>>> kvakvae_;
    //! strain at last evaluation
    std::vector<CORE::LINALG::Matrix<6, 1>> strain_last_;

  };  // class Robinson : public CORE::MAT::Material
}  // namespace MAT


/*----------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif