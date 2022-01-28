/*----------------------------   tools.hpp     ---------------------------*/
#ifndef __tools_HPP
#define __tools_HPP
/*----------------------------   tools.hpp     ---------------------------*/

#include "dgvector.hpp"

namespace Nextsim {

/*!
   * This namespace collects the auxiliary routines 
   */
namespace Tools {

    inline constexpr double SQR(double x)
    {
        return x * x;
    }





    template <int DGstress, int DGtracer>
    void Delta(const Mesh& mesh,
        const CellVector<DGstress>& E11, const CellVector<DGstress>& E12, const CellVector<DGstress>& E22,
        const double DeltaMin, CellVector<DGtracer>& DELTA)
    {

#pragma omp parallel for
        for (size_t i = 0; i < mesh.n; ++i) {

            DELTA(i,0) = sqrt(
                SQR(DeltaMin)
                + 1.25 * (SQR(E11(i, 0)) + SQR(E22(i, 0)))
                + 1.50 * E11(i, 0) * E22(i, 0)
                + SQR(E12(i, 0)));;
        }
     }


    template <int DGstress, int DGtracer>
    void Shear(const Mesh& mesh,
        const CellVector<DGstress>& E11, const CellVector<DGstress>& E12, const CellVector<DGstress>& E22,
        const double DeltaMin, CellVector<DGtracer>& SHEAR)
    {

#pragma omp parallel for
        for (size_t i = 0; i < mesh.n; ++i) {
            SHEAR(i, 0) = sqrt((SQR(DeltaMin) + SQR(E11(i, 0) - E22(i, 0)) + 4.0 * SQR(E12(i, 0))));
        }

     }



}

}

/*----------------------------   tools.hpp     ---------------------------*/
/* end of #ifndef __tools_HPP */
#endif
/*----------------------------   tools.hpp     ---------------------------*/