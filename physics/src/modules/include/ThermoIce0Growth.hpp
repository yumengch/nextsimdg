/*!
 * @file ThermoIce0Growth.hpp
 *
 * @date Mar 17, 2022
 * @author Tim Spain <timothy.spain@nersc.no>
 */

#ifndef PHYSICS_SRC_MODULES_INCLUDE_THERMOICE0GROWTH_HPP_
#define PHYSICS_SRC_MODULES_INCLUDE_THERMOICE0GROWTH_HPP_

#include "include/IVerticalIceGrowth.hpp"

namespace Nextsim {

class ThermoIce0Growth : public IVerticalIceGrowth {
public:
    ThermoIce0Growth()
        : IVerticalIceGrowth()
    {
    }
    virtual ~ThermoIce0Growth() = default;

    void update(const TimePoint& tsInitialTime) override;

private:
    void calculateElement(size_t i);

    HField delta_hi;
    HField delta_hs_melt;
    HField delta_hi_topmelt;
    HField delta_hi_botmelt;
};

} /* namespace Nextsim */

#endif /* PHYSICS_SRC_MODULES_INCLUDE_THERMOICE0GROWTH_HPP_ */