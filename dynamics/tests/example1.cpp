#include <cassert>
#include <iostream>
#include <chrono>
#include <vector>

#include "dginitial.hpp"
#include "dglimiters.hpp"
#include "mesh.hpp"
#include "timemesh.hpp"
#include "dgvisu.hpp"
#include "dgtransport.hpp"
#include "stopwatch.hpp"
#include "testtools.hpp"

namespace Nextsim
{
  extern Timer GlobalTimer;
}


bool WRITE_VTK = false; //!< set to true for vtk output
double TOL = 1.e-10;    //!< tolerance for checking test results


//! Initially a smooth bump centered at (0.4,0.4)
//! This will be transported in a circle with (-y, x) for one complete revolution
class InitialPhi : virtual public Nextsim::InitialBase {
  
public:

  
  double smooth(double x) const // smooth transition from 0 to 1 on [0,1]
  {
    if (x<=0)
      return 0;
    if (x>=1.0)
      return 0;
    
    if (x<0.5)
      return 0.5*exp(-1./x)/exp(-2.0);
    else
      return 1.-0.5*exp(-1./(1.-x))/exp(-2.0);
  }
  

  double operator()(double x, double y) const
  {
    double r = sqrt(pow(x-0.4,2.0)+pow(y-0.4,2.0));
    if (r<0.1)
      return 1.0;
    if (r<0.3)
      return 1.0-smooth(5.0*(r-0.1));
    return 0.0;
  }
};


// Velocity
class InitialVX : virtual public Nextsim::InitialBase {
  
  double _time;

public:

  void settime(double t)
  {_time = t;}
  
  double operator()(double x, double y) const { return (0.5*M_PI*sin(0.5*_time)) * (y - 0.5); }
};
class InitialVY : virtual public Nextsim::InitialBase {
  
  double _time;

public:

  void settime(double t)
  {_time = t;}
  
  double operator()(double x, double y) const { return (0.5*M_PI*sin(0.5*_time)) * (0.5 - x); }
};

//////////////////////////////////////////////////


template<int DGdegree>
class Test
{

  size_t N; //!< size of mesh N x N
  

  Nextsim::Mesh mesh;   //!< space mesh
  Nextsim::TimeMesh timemesh; //!< time mesh
  
  //! Velocity vectors and density
  Nextsim::CellVector<DGdegree> vx, vy, phi, finalphi;

  //! Transport main class
  Nextsim::DGTransport<DGdegree> dgtransport;

  //! Velocity Field
  InitialVX VX;
  InitialVY VY;

  std::vector<double> values; //!< for storing numerical results (mass)
  std::vector<double> errors; //!< for storing error w.r.t. exact solution

  size_t writestep; //! write out n step in total (for debugging only)
  
 public:

  Test(size_t n) : N(n), dgtransport(vx,vy), writestep(40)
  {
    //! Set time stepping scheme. 2nd order for dg0 and dg1, 3rd order dG2
    if (DGdegree<2)
      dgtransport.settimesteppingscheme("rk2");
    else
      dgtransport.settimesteppingscheme("rk3");
  }

  Test() 
  {
    std::cout << "call Test(N). N is number of mesh elements per row" << std::endl;
  }

  void init()
  {
    //! Init Mesh
    mesh.BasicInit(N, N, 1.0 / N, 1.0 / N);

    //! Init Time Mesh
    double cfl = 0.2;
    double k   = cfl * std::min(mesh.hx, mesh.hy) / 1.0; // max-velocity is 1
    double tmax = 2.0*M_PI;
    int NT = (static_cast<int>((tmax / k + 1) /100 + 1) * 100); // No time steps dividable by 100
    timemesh.BasicInit(tmax, NT);
    
    //! Init Transport Scheme
    dgtransport.setmesh(mesh);
    dgtransport.settimemesh(timemesh);

    //! Init Vectors
    vx.resize_by_mesh(mesh);
    vy.resize_by_mesh(mesh);
    phi.resize_by_mesh(mesh);
  }
  
  void run()
  {
    values.clear();

    size_t NITER=3;
    
    //! Iteration with successive mesh refinement
    for (size_t iter=0;iter<NITER;++iter)
      {
	Nextsim::GlobalTimer.reset();
	Nextsim::GlobalTimer.start("run");

	Nextsim::GlobalTimer.start("run -- init");
	init();
	
	// initial density
	Nextsim::L2ProjectInitial(mesh, phi, InitialPhi());

	if (WRITE_VTK)
	  {
	    Nextsim::GlobalTimer.start("run -- init -- vtk");
	    Nextsim::VTK::write_dg<DGdegree>("Results/dg",0,phi,mesh);
	    Nextsim::GlobalTimer.stop("run -- init -- vtk");
	  }
	
	//! Save initial solution for error control
	finalphi = phi;
	Nextsim::GlobalTimer.stop("run -- init");
	

	//! time loop
	Nextsim::GlobalTimer.start("run -- loop");
	for (size_t iter = 1; iter <= timemesh.N; ++iter)
	  {
	    Nextsim::GlobalTimer.start("run -- loop -- vel");
	    VX.settime(timemesh.k * iter);
	    VY.settime(timemesh.k * iter);
	    Nextsim::GlobalTimer.start("run -- loop -- vel -- l2");
	    Nextsim::L2ProjectInitial(mesh, vx, VX);
	    Nextsim::L2ProjectInitial(mesh, vy, VY);
	    Nextsim::GlobalTimer.stop("run -- loop -- vel -- l2");
	    dgtransport.reinitvelocity();
	    Nextsim::GlobalTimer.stop("run -- loop -- vel");
	    
	    dgtransport.step(phi); // performs one time step with the 2nd Order Heun scheme
	    if (WRITE_VTK)
	      if (iter % (timemesh.N/writestep)==0)
		{
		  Nextsim::GlobalTimer.start("run -- loop -- vtk");
		  Nextsim::VTK::write_dg<DGdegree>("Results/dg",iter/(timemesh.N/writestep),phi,mesh);
		  Nextsim::GlobalTimer.stop("run -- loop -- vtk");
		}
	    
	  }
	Nextsim::GlobalTimer.stop("run -- loop");


	Nextsim::GlobalTimer.start("run -- error");
	Nextsim::CellVector<DGdegree> errorphi = phi;
	errorphi += -finalphi;
	if (WRITE_VTK)
	  {
	    Nextsim::GlobalTimer.start("run -- error- vtk");
	    Nextsim::VTK::write_dg<DGdegree>("Results/error",N,errorphi,mesh);
	    Nextsim::GlobalTimer.stop("run -- error- vtk");
	  }
	errors.push_back(errorphi.norm() * sqrt(mesh.hx*mesh.hy));
	values.push_back(phi.col(0).sum() * mesh.hx * mesh.hy);
	Nextsim::GlobalTimer.stop("run -- error");
	

	Nextsim::GlobalTimer.stop("run");

	//	Nextsim::GlobalTimer.print(); 

	if (iter<NITER-1)
	  N*=2; //!< double the problem size
      }
  }

  void print_error(const std::string& message) const
  {
    std::cerr << "dG(" << DGdegree << ") FAILED: " << message << std::endl;
    for (size_t i=0;i<values.size();++i)
      std::cerr << values[i] << "\t" << errors[i] << std::endl;
  }

  void print_error(const std::array<double,3>& v,
		   const std::array<double,3>& e,
		   const std::string& message) const
  {
    std::cerr << "dG(" << DGdegree << ") FAILED: " << message << std::endl;

    assert(values.size()>=3);
    
    for (size_t i=0;i<3;++i)
      std::cerr << v[i] << " = " << values[i+values.size()-3] << "\t" << e[i] << " = " << errors[i+values.size()-3] << std::endl;
  }

  bool check_references(const std::array<double,3>& v, const std::array<double,3>& e) const
  {
    bool passed = true;
    
    for (size_t i=0;i<3;++i)
      {
	if (fabs(v[i]-values[i])>TOL)
	  {
	    print_error(v,e,"difference in mass");
	    passed = false;
	  }
	if (fabs(e[i]-errors[i])>TOL)
	  {
	    print_error(v,e,"difference in error");
	    passed = false;
	  }
      }

    return passed;
  }
  

  bool check() const
  {
    std::array<double,3> val_ref, err_ref;

    if (N==80)
      {
	if (DGdegree == 0)
	  {
	    val_ref = std::array<double,3>({0.1041620796203775,0.1196262461401992,0.1264044476119508});
	    err_ref = std::array<double,3>({0.2052188657233615,0.1544332132653233,0.1110687031463838});
	  }
	else if (DGdegree == 1)
	  {
	    val_ref = std::array<double,3>({0.1297379288134224,0.1290796184183312,0.1290992302048382});
	    err_ref = std::array<double,3>({0.07213309600190464,0.01905730153394329,0.004537411616573561});
	  }
	else if (DGdegree == 2)
	  {
	    val_ref = std::array<double,3>({0.1290846500028399,0.129099699867661,0.1290996998773258});
	    err_ref = std::array<double,3>({0.03042875264858447,0.005337828057657737,0.0005416593875619044});
	  }
	else abort();
      }
    else
      {
	print_error("reference values only for N=20,40,80");
	return false;
      }

    return check_references(val_ref,err_ref);
  }
  

};


//////////////////////////////////////////////////

int main()
{
  size_t N = 20;
  Test<0> test0(N);
  test0.run();
  if (!test0.check())
    std::cout << "dG(0) TEST FAILED" << std::endl;
  else
    std::cout << "dG(0) TEST PASSED" << std::endl;

  Test<1> test1(N);
  test1.run();
  if (!test1.check())
    std::cout << "dG(0) TEST FAILED" << std::endl;
  else
    std::cout << "dG(0) TEST PASSED" << std::endl;

  Test<2> test2(N);
  test2.run();
  if (!test2.check())
    std::cout << "dG(0) TEST FAILED" << std::endl;
  else
    std::cout << "dG(0) TEST PASSED" << std::endl;
}