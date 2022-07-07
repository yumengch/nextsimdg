/*!
 * @file ModelMetadata.hpp
 *
 * @date Jun 29, 2022
 * @author Tim Spain <timothy.spain@nersc.no>
 */

#ifndef MODELMETADATA_HPP
#define MODELMETADATA_HPP

#include "include/Time.hpp"

#include <string>

namespace Nextsim {

/*!
 * A class to hold the metadata pertaining to the model as a whole, both
 * constant and time varying values. Especially values required for data file
 * output.
 */
class ModelMetadata {
public:
    /*!
     * @brief Sets the initial or current model time
     *
     * @param time TimePoint instance encoding the current time.
     */
    inline void setTime(const TimePoint& time) { m_time = time; }
    /*!
     * @brief Increments the model time metadata value.
     *
     * @param step Duration of the time increment to add.
     */
    inline void incrementTime(const Duration& step) { m_time += step; }
    //! Returns the current model time.
    inline const TimePoint& time() const { return m_time; }

    //! Returns the string description of the model grid structure.
    const std::string& structureName() const;

private:
    TimePoint m_time;
};

} /* namespace Nextsim */

#endif /* MODELMETADATA_HPP */
