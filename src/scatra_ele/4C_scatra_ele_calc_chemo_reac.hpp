/*----------------------------------------------------------------------*/
/*! \file
 \brief main file containing routines for calculation of scatra element with chemotactic AND
reactive scalars

\level 2

 *----------------------------------------------------------------------*/

#ifndef FOUR_C_SCATRA_ELE_CALC_CHEMO_REAC_HPP
#define FOUR_C_SCATRA_ELE_CALC_CHEMO_REAC_HPP

#include "4C_config.hpp"

#include "4C_mat_scatra_chemotaxis.hpp"
#include "4C_mat_scatra_reaction.hpp"
#include "4C_scatra_ele_calc.hpp"
#include "4C_scatra_ele_calc_advanced_reaction.hpp"
#include "4C_scatra_ele_calc_chemo.hpp"

FOUR_C_NAMESPACE_OPEN


namespace Discret
{
  namespace ELEMENTS
  {
    template <Core::FE::CellType distype, int probdim = Core::FE::dim<distype>>
    class ScaTraEleCalcChemoReac : public ScaTraEleCalcChemo<distype, probdim>,
                                   public ScaTraEleCalcAdvReac<distype, probdim>
    {
     private:
      //! private constructor, since we are a Singleton.
      ScaTraEleCalcChemoReac(
          const int numdofpernode, const int numscal, const std::string& disname);

      typedef ScaTraEleCalc<distype, probdim> my;
      typedef ScaTraEleCalcChemo<distype, probdim> chemo;
      typedef ScaTraEleCalcAdvReac<distype, probdim> advreac;

     public:
      //! Singleton access method
      static ScaTraEleCalcChemoReac<distype, probdim>* instance(
          const int numdofpernode, const int numscal, const std::string& disname);

     protected:
      //! get the material parameters
      void get_material_params(
          const Core::Elements::Element* ele,  //!< the element we are dealing with
          std::vector<double>& densn,          //!< density at t_(n)
          std::vector<double>& densnp,         //!< density at t_(n+1) or t_(n+alpha_F)
          std::vector<double>& densam,         //!< density at t_(n+alpha_M)
          double& visc,                        //!< fluid viscosity
          const int iquad = -1                 //!< id of current gauss point (default = -1)
          ) override;

    };  // end class ScaTraEleCalcChemoReac

  }  // namespace ELEMENTS

}  // namespace Discret

FOUR_C_NAMESPACE_CLOSE

#endif
