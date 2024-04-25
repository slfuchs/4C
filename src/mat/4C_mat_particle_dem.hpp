/*---------------------------------------------------------------------------*/
/*! \file
\brief particle material for DEM

\level 3


*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | definitions                                                sfuchs 07/2018 |
 *---------------------------------------------------------------------------*/
#ifndef FOUR_C_MAT_PARTICLE_DEM_HPP
#define FOUR_C_MAT_PARTICLE_DEM_HPP

/*---------------------------------------------------------------------------*
 | headers                                                    sfuchs 07/2018 |
 *---------------------------------------------------------------------------*/
#include "4C_config.hpp"

#include "4C_mat_particle_base.hpp"

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | class definitions                                          sfuchs 07/2018 |
 *---------------------------------------------------------------------------*/
namespace MAT
{
  namespace PAR
  {
    class ParticleMaterialDEM : public ParticleMaterialBase
    {
     public:
      //! constructor
      ParticleMaterialDEM(Teuchos::RCP<MAT::PAR::Material> matdata);

      //! create material instance of matching type with parameters
      Teuchos::RCP<MAT::Material> CreateMaterial() override;
    };

  }  // namespace PAR

  class ParticleMaterialDEMType : public CORE::COMM::ParObjectType
  {
   public:
    std::string Name() const override { return "ParticleMaterialDEMType"; };

    static ParticleMaterialDEMType& Instance() { return instance_; };

    CORE::COMM::ParObject* Create(const std::vector<char>& data) override;

   private:
    static ParticleMaterialDEMType instance_;
  };

  class ParticleMaterialDEM : public Material
  {
   public:
    //! constructor (empty material object)
    ParticleMaterialDEM();

    //! constructor (with given material parameters)
    explicit ParticleMaterialDEM(MAT::PAR::ParticleMaterialDEM* params);

    //! @name Packing and Unpacking

    //@{

    /*!
      \brief Return unique ParObject id

      every class implementing ParObject needs a unique id defined at the
      top of parobject.H (this file) and should return it in this method.
    */
    int UniqueParObjectId() const override
    {
      return ParticleMaterialDEMType::Instance().UniqueParObjectId();
    }

    /*!
      \brief Pack this class so it can be communicated

      Resizes the vector data and stores all information of a class in it.
      The first information to be stored in data has to be the
      unique parobject id delivered by UniqueParObjectId() which will then
      identify the exact class on the receiving processor.

      \param data (in/out): char vector to store class information
    */
    void Pack(CORE::COMM::PackBuffer& data) const override;

    /*!
      \brief Unpack data from a char vector into this class

      The vector data contains all information to rebuild the
      exact copy of an instance of a class on a different processor.
      The first entry in data has to be an integer which is the unique
      parobject id defined at the top of this file and delivered by
      UniqueParObjectId().

      \param data (in) : vector storing all data to be unpacked into this
      instance.
    */
    void Unpack(const std::vector<char>& data) override;

    //@}

    //! material type
    INPAR::MAT::MaterialType MaterialType() const override { return INPAR::MAT::m_particle_dem; }

    //! return copy of this material object
    Teuchos::RCP<Material> Clone() const override
    {
      return Teuchos::rcp(new ParticleMaterialDEM(*this));
    }

    //! return quick accessible material parameter data
    MAT::PAR::Parameter* Parameter() const override { return params_; }

    //@}

   private:
    //! my material parameters
    MAT::PAR::ParticleMaterialDEM* params_;
  };

}  // namespace MAT

/*---------------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif
