/*----------------------------------------------------------------------*/
/*! \file

\brief Declaration of a base anisotropy extension to be used by anisotropic materials with
@Mat::Anisotropy

\level 3


*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_MAT_ANISOTROPY_EXTENSION_BASE_HPP
#define FOUR_C_MAT_ANISOTROPY_EXTENSION_BASE_HPP

#include "4C_config.hpp"

#include <Teuchos_RCPDecl.hpp>

#include <vector>

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace Core::Communication
{
  class PackBuffer;
  class UnpackBuffer;
}  // namespace Core::Communication
namespace Mat
{
  // forward declaration
  class Anisotropy;

  class BaseAnisotropyExtension
  {
    // Anisotropy is a friend to create back reference
    friend class Anisotropy;

   public:
    /// If element fibers are used, they are stored at the beginning of the list
    static constexpr int GPDEFAULT = 0;

    virtual ~BaseAnisotropyExtension() = default;

    ///@name Packing and Unpacking
    /// @{

    /*!
     * \brief Pack all data for parallel distribution and restart
     *
     * \param data
     */
    virtual void pack_anisotropy(Core::Communication::PackBuffer& data) const = 0;

    /*!
     * \brief Unpack all data from parallel distribution or restart
     */
    virtual void unpack_anisotropy(Core::Communication::UnpackBuffer& buffer) = 0;
    /// @}

    /*!
     * \brief This method will be called by Mat::Anisotropy if element and Gauss point fibers are
     * available
     */
    virtual void on_global_data_initialized() = 0;

   protected:
    /*!
     * \brief Returns the reference to the anisotropy
     *
     * \return Teuchos::RCP<Anisotropy>& Reference to the anisotropy
     */
    Teuchos::RCP<Anisotropy>& get_anisotropy() { return anisotropy_; }
    /*!
     * \brief Returns the reference to the anisotropy
     *
     * \return Teuchos::RCP<Anisotropy>& Reference to the anisotropy
     */
    const Teuchos::RCP<Anisotropy>& get_anisotropy() const { return anisotropy_; }

   private:
    /*!
     * \brief This method will be called by Mat::Anisotropy to notify that element information is
     * available.
     */
    virtual void on_global_element_data_initialized() = 0;


    /*!
     * \brief This method will be called by Mat::Anisotropy to notify that Gauss point information
     * is available.
     */
    virtual void on_global_gp_data_initialized() = 0;

    /// \name Private methods called by the friend class Mat::Anisotropy
    /// \{
    /*!
     * \brief Set the anisotropy. This method will only be used by Anisotropy itself to give the
     * extension access to all anisotropy information.
     *
     * \param anisotropy
     */
    void set_anisotropy(Anisotropy& anisotropy);

    /// Reference to Anisotropy
    Teuchos::RCP<Anisotropy> anisotropy_;
  };
}  // namespace Mat
FOUR_C_NAMESPACE_CLOSE

#endif