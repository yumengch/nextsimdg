/*!
 * @file dgTransport.cpp
 * @date 1 Mar 2022
 * @author Thomas Richter <thomas.richter@ovgu.de>
 */

#include "dgTransport.hpp"
#include "dgTimeStepping.hpp"
#include "stopwatch.hpp"

namespace Nextsim {

extern Timer GlobalTimer;

template <int DGcell, int DGedge>
void DGTransport<DGcell, DGedge>::setmesh(const Mesh& _mesh)
{
    mesh = _mesh; // copy mesh

    tmp1.resize_by_mesh(mesh); // resize tmp-vectors for time stepping
    tmp2.resize_by_mesh(mesh);
    tmp3.resize_by_mesh(mesh);

    // resizes the velocity-edge-vectors
    velx_edgeY.resize_by_mesh(mesh, EdgeType::Y);
    vely_edgeX.resize_by_mesh(mesh, EdgeType::X);
}

template <int DGcell, int DGedge>
void DGTransport<DGcell, DGedge>::reinitvelocity()
{
    // average the velocity to the edges
    average_to_edges_Y(mesh, velx_edgeY, velx);
    average_to_edges_X(mesh, vely_edgeX, vely);
}

template <int DGcell, int DGedge>
void DGTransport<DGcell, DGedge>::step_rk1(const double dt, CellVector<DGcell>& phi)
{
    transportoperator<DGcell, DGedge>(mesh, dt, velx, vely, velx_edgeY, vely_edgeX, phi, tmp1);

    phi += tmp1;
}

template <int DGcell, int DGedge>
void DGTransport<DGcell, DGedge>::step_rk2(const double dt, CellVector<DGcell>& phi)
{
    transportoperator<DGcell, DGedge>(mesh, dt, velx, vely, velx_edgeY, vely_edgeX, phi,
        tmp1); // tmp1 = k * F(u)

    phi += tmp1; // phi = phi + k * F(u)     (i.e.: implicit Euler)

    transportoperator<DGcell, DGedge>(mesh, dt, velx, vely, velx_edgeY, vely_edgeX, phi,
        tmp2); // tmp1 = k * F( u + k * F(u) )

    phi += 0.5 * (tmp2 - tmp1);
}

template <int DGcell, int DGedge>
void DGTransport<DGcell, DGedge>::step_rk3(const double dt, CellVector<DGcell>& phi)
{
    transportoperator<DGcell, DGedge>(mesh, dt, velx, vely, velx_edgeY, vely_edgeX, phi,
        tmp1); // tmp1 = k * F(u)  // K1 in Heun(3)

    phi += 1. / 3. * tmp1; // phi = phi + k/3 * F(u)   (i.e.: implicit Euler)
    transportoperator<DGcell, DGedge>(mesh, dt, velx, vely, velx_edgeY, vely_edgeX, phi,
        tmp2); // k * F(k1) // K2 in Heun(3)
    phi -= 1. / 3. * tmp1; // phi = phi + k/3 * F(u)   (i.e.: implicit Euler)

    phi += 2. / 3. * tmp2;
    transportoperator<DGcell, DGedge>(mesh, dt, velx, vely, velx_edgeY, vely_edgeX, phi,
        tmp3); // k * F(k2) // K3 in Heun(3)
    phi -= 2. / 3. * tmp2;

    phi += 0.25 * tmp1 + 0.75 * tmp3;
}

template <int DGcell, int DGedge>
void DGTransport<DGcell, DGedge>::step(const double dt, CellVector<DGcell>& phi)
{
    GlobalTimer.start("-- --> step");
    if (timesteppingscheme == "rk1")
        step_rk1(dt, phi);
    else if (timesteppingscheme == "rk2")
        step_rk2(dt, phi);
    else if (timesteppingscheme == "rk3")
        step_rk3(dt, phi);
    else {
        std::cerr << "Time stepping scheme '" << timesteppingscheme << "' not known!" << std::endl;
        abort();
    }
    GlobalTimer.stop("-- --> step");
}

template class DGTransport<1, 1>;
template class DGTransport<3, 2>;
template class DGTransport<6, 3>;

} /* namespace Nextsim */