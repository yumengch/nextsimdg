/*!
 * @file benchmark_mehlmann_mevp.cpp
 * @date 24 July 2022
 * @author Thomas Richter <thomas.richter@ovgu.de>
 */

#include "Interpolations.hpp"
#include "ParametricMesh.hpp"
#include "ParametricTools.hpp"
#include "DGTransport.hpp"

#include "Tools.hpp"
#include "cgParametricMomentum.hpp"
#include "cgVector.hpp"
#include "dgInitial.hpp"
#include "dgLimit.hpp"
#include "dgVisu.hpp"
#include "mevp.hpp"
#include "stopwatch.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>

bool WRITE_VTK = true;

namespace Nextsim {
extern Timer GlobalTimer;
}

inline constexpr double SQR(double x) { return x * x; }

//////////////////////////////////////////////////// Benchmark testcase from [Mehlmann / Richter,
///...]
//! Description of the problem data, wind & ocean fields

namespace ReferenceScale {
constexpr double T = 2.0 * 24 * 60. * 60.; //!< Time horizon 2 days
constexpr double L = 512000.0; //!< Size of domain !!!
constexpr double vmax_ocean = 0.01; //!< Maximum velocity of ocean
double vmax_atm = 30.0 / exp(1.0); //!< Max. vel. of wind
}

class OceanX : public Nextsim::Interpolations::Function {
public:
    double operator()(double x, double y) const { return ReferenceScale::vmax_ocean; }
};
class OceanY : public Nextsim::Interpolations::Function {
public:
    double operator()(double x, double y) const { return ReferenceScale::vmax_ocean; }
};

class AtmX : public Nextsim::Interpolations::Function {
public:
    double operator()(double x, double y) const { return ReferenceScale::vmax_atm; }
};
class AtmY : public Nextsim::Interpolations::Function {
public:
    double operator()(double x, double y) const { return ReferenceScale::vmax_atm; }
};

class InitialH : public Nextsim::Interpolations::Function {
public:
    double operator()(double x, double y) const
    {
        if (x > 100.e3 && x < 200.e3 && y > 100.e3 && y < 200.e3) {
            return 0.01*(1.0 - exp(-0.2e-7 * (x - 150000.0) * (x - 150000.0)))
                * (1.0 - exp(-0.2e-7 * (y - 150000.0) * (y - 150000.0)));
        } else {
            return 0.;
        }
    }
};
class InitialA : public Nextsim::Interpolations::Function {
public:
    double operator()(double x, double y) const
    {
        if (x > 100.e3 && x < 200.e3 && y > 100.e3 && y < 200.e3) {
            return 0.1 * (1.0 - exp(-0.2e-7 * (x - 150000.0) * (x - 150000.0)))
                * (1.0 - exp(-0.2e-7 * (y - 150000.0) * (y - 150000.0)));
        } else {
            return 0.;
        }
    }
};
//////////////////////////////////////////////////

template <int CG, int DGadvection, int DGstress> void run_benchmark(const std::string meshfile)
{
    //! Define the spatial mesh
    Nextsim::ParametricMesh smesh;
    smesh.readmesh(meshfile);
    size_t NX = smesh.nx;

    //! Compose name of output directory and create it
    std::string resultsdir = "Freedrift_" + std::to_string(CG) + "_" + std::to_string(DGadvection)
        + "_" + std::to_string(DGstress) + "__" + std::to_string(NX);
    std::filesystem::create_directory(resultsdir);

    //! Main class to handle the momentum equation. This class also stores the CG velocity vector
    Nextsim::CGParametricMomentum<CG, DGstress> momentum(smesh);

    //! define the time mesh
    constexpr double dt_adv = 120.0; //!< Time step of advection problem
    constexpr size_t NT = ReferenceScale::T / dt_adv + 1.e-4; //!< Number of Advections steps

    //! MEVP parameters
    constexpr double alpha = 1500.0;
    constexpr double beta = 1500.0;
    constexpr size_t NT_evp = 100;

    //! Rheology-Parameters
    Nextsim::VPParameters VP;

    std::cout << "Time step size (advection) " << dt_adv << "\t" << NT << " time steps" << std::endl
              << "MEVP subcycling NTevp " << NT_evp << "\t alpha/beta " << alpha << " / " << beta
              << std::endl
              << "CG/DG/DGstress " << CG << "\t" << DGadvection << "\t" << DGstress << std::endl;

    //! VTK output
    constexpr double T_vtk = 60.0 * 60.0; // every hour
    constexpr size_t NT_vtk = T_vtk / dt_adv + 1.e-4;
    //! LOG message
    constexpr double T_log = 10.0 * 60.0; // every 30 minute
    constexpr size_t NT_log = T_log / dt_adv + 1.e-4;

    ////////////////////////////////////////////////// Forcing
    Nextsim::Interpolations::Function2CG(smesh, momentum.GetOceanx(), OceanX());
    Nextsim::Interpolations::Function2CG(smesh, momentum.GetOceany(), OceanY());
    Nextsim::Interpolations::Function2CG(smesh, momentum.GetAtmx(), AtmX());
    Nextsim::Interpolations::Function2CG(smesh, momentum.GetAtmy(), AtmY());

    ////////////////////////////////////////////////// Variables and Initial Values
    Nextsim::DGVector<DGadvection> H(smesh), A(smesh); //!< ice height and concentration
    Nextsim::Interpolations::Function2DG(smesh, H, InitialH());
    Nextsim::Interpolations::Function2DG(smesh, A, InitialA());

    ////////////////////////////////////////////////// i/o of initial condition
    Nextsim::GlobalTimer.start("time loop - i/o");
    if (1) // write initial?
    {
        //! Gauss-point limiting
        Nextsim::LimitMax(A, 1.0);
        Nextsim::LimitMin(A, 0.0);
        Nextsim::LimitMin(H, 0.0);
        if (WRITE_VTK) {
            Nextsim::VTK::write_cg_velocity(
                resultsdir + "/vel", 0, momentum.GetVx(), momentum.GetVy(), smesh);
            Nextsim::VTK::write_dg(resultsdir + "/A", 0, A, smesh);
            Nextsim::VTK::write_dg(resultsdir + "/H", 0, H, smesh);
            Nextsim::VTK::write_dg(resultsdir + "/Shear", 0,
                Nextsim::Tools::Shear(
                    smesh, momentum.GetE11(), momentum.GetE12(), momentum.GetE22()),
                smesh);
        }
    }
    Nextsim::GlobalTimer.stop("time loop - i/o");

    ////////////////////////////////////////////////// Initialize transport
    Nextsim::ParametricTransport<DGadvection> dgtransport(smesh);
    dgtransport.settimesteppingscheme("rk2");

    ////////////////////////////////////////////////// Main Loop
    Nextsim::GlobalTimer.start("time loop");
    for (size_t timestep = 1; timestep <= NT; ++timestep) {
        double time = dt_adv * timestep;
        double timeInMinutes = time / 60.0;
        double timeInHours = time / 60.0 / 60.0;
        double timeInDays = time / 60.0 / 60.0 / 24.;

        if (timestep % NT_log == 0)
            std::cout << "\rAdvection step " << timestep << "\t " << std::setprecision(2)
                      << std::fixed << std::setw(10) << std::right << time << "s\t" << std::setw(8)
                      << std::right << timeInMinutes << "m\t" << std::setw(6) << std::right
                      << timeInHours << "h\t" << std::setw(6) << std::right << timeInDays << "d\t\t"
                      << std::flush;

        //////////////////////////////////////////////////
        //! Advection step
        Nextsim::GlobalTimer.start("time loop - advection");

        // interpolates CG velocity to DG and reinits normal velocity
        dgtransport.prepareAdvection(momentum.GetVx(), momentum.GetVy());

        // performs the transport steps
        dgtransport.step(dt_adv, A);
        dgtransport.step(dt_adv, H);

        //! Gauss-point limiting
        Nextsim::LimitMax(A, 1.0);
        Nextsim::LimitMin(A, 0.0);
        Nextsim::LimitMin(H, 0.0);
        Nextsim::GlobalTimer.stop("time loop - advection");

        //////////////////////////////////////////////////
        Nextsim::GlobalTimer.start("time loop - mevp");
        momentum.prepareIteration(H, A);
        // MEVP subcycling
        for (size_t mevpstep = 0; mevpstep < NT_evp; ++mevpstep) {
            momentum.mEVPStep(VP, NT_evp, alpha, beta, dt_adv, H, A);
            // <- MPI
        }
        Nextsim::GlobalTimer.stop("time loop - mevp");

        //////////////////////////////////////////////////
        if (WRITE_VTK) // Output
            if ((timestep % NT_vtk == 0)) {
                std::cout << "VTK output at day " << time / 24. / 60. / 60. << std::endl;

                int printstep = timestep / NT_vtk + 1.e-4;
                Nextsim::GlobalTimer.start("time loop - i/o");
                Nextsim::VTK::write_cg_velocity(
                    resultsdir + "/vel", printstep, momentum.GetVx(), momentum.GetVy(), smesh);
                Nextsim::VTK::write_dg(resultsdir + "/A", printstep, A, smesh);
                Nextsim::VTK::write_dg(resultsdir + "/H", printstep, H, smesh);
                Nextsim::VTK::write_dg(resultsdir + "/Delta", printstep,
                    Nextsim::Tools::Delta(smesh, momentum.GetE11(), momentum.GetE12(),
                        momentum.GetE22(), VP.DeltaMin),
                    smesh);
                Nextsim::VTK::write_dg(resultsdir + "/Shear", printstep,
                    Nextsim::Tools::Shear(
                        smesh, momentum.GetE11(), momentum.GetE12(), momentum.GetE22()),
                    smesh);
                Nextsim::GlobalTimer.stop("time loop - i/o");
            }
    }
    Nextsim::GlobalTimer.stop("time loop");

    std::cout << std::endl;
    Nextsim::GlobalTimer.print();
}

int main()
{
    run_benchmark<2, 6, 8>("../ParametricMesh/distortedrectangle_128x128.smesh");
}
