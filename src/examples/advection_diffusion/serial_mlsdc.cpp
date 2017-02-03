/**
 * Advection-Diffusion with serial MLSDC.
 *
 * @ingroup AdvectionDiffusionFiles
 * @file examples/advection_diffusion/serial_mlsdc.cpp
 * @since v0.1.0
 */



#include <dune/functions/functionspacebases/pqknodalbasis.hh>
#include <dune/istl/matrixmarket.hh>
#include <dune/functions/common/utility.hh>
#include <dune/functions/functionspacebases/interpolate.hh>
#include <dune/grid/io/file/vtk/vtkwriter.hh>

//#include <cstdlib>
//#include <memory>

//#include <pfasst.hpp>
//#include <pfasst/controller/sdc.hpp>


#include "../../datatypes/dune_vec.hpp"

#include "../../finite_element_stuff/assemble.hpp"
#include "advection_diffusion_sweeper.hpp"




#include <memory>
using namespace std;

#include <pfasst.hpp>
#include <pfasst/controller/two_level_mlsdc.hpp>
//#include <pfasst/encap/vector.hpp>

using namespace pfasst::encap;

#include "../../finite_element_stuff/spectral_transfer.hpp"



namespace pfasst
{
  namespace examples
  {
    namespace advection_diffusion
    {
      /**
       * Advection/diffusion example using an encapsulated IMEX sweeper.
       *
       * This example uses a (serial) multi-level SDC sweeper.
       *
       * @ingroup AdvectionDiffusion
       */
      tuple<error_map, residual_map> run_serial_mlsdc(size_t nlevs,               //Anzahl der mlsdc-level 
                                                      size_t nsteps_in=4,         //Anzahl der Zeitschritte
                                                      double step_size_in=0.25,   //Groesse der Zeitschritte 
                                                      size_t num_iter_in=5,       //Anzahl der Iterationen
                                                      size_t nnodes_in=3,         //Anzahl zeiliche Quadraturpunkte
                                                      size_t ndofs_in=32)         //Anzahl raeumliche Freiheitsgrade
      {
        MLSDC<> mlsdc;

	nlevs               = config::get_value<size_t>("num_levs", nlevs);
        const size_t nsteps = config::get_value<size_t>("num_steps", nsteps_in);
        const double dt     = config::get_value<double>("step_size", step_size_in);
	

        const size_t niters = config::get_value<size_t>("num_iter", num_iter_in);
        const int    xrat   = 2;
        const int    trat   = 2;

        size_t nnodes = config::get_value<size_t>("num_nodes", nnodes_in);
        size_t nelements  = config::get_value<size_t>("spatial_dofs", ndofs_in);

        const double abs_res_tol = pfasst::config::get_value<double>("abs_res_tol", 1e-8);
        const double rel_res_tol = pfasst::config::get_value<double>("rel_res_tol", 0.0);

	const size_t base_order=1;
	

	
	
        /*
         * build space/time discretisation levels and add them to mlsdc
         * controller.  this loop adds the finest level first, and
         * subsequently refines in time (accoring to 'trat') and space
         * (according to 'xrat').
         */
	
	auto FinEl   = make_shared<fe_manager>(nelements, nlevs); //spatial_dofs
	std::shared_ptr<std::vector<MatrixType*>> vecvec = FinEl->get_transfer();
	
	
	if(base_order!=1){
	for (size_t l = 0; l < base_order; l++) {
	  

	  //auto quad     = quadrature::quadrature_factory(nnodes, quadrature::QuadratureType::GaussLobatto);// GaussRadau);// GaussLegendre); //GaussLobatto);	  
	  auto quad     = quadrature::quadrature_factory(nnodes, quadrature::QuadratureType::GaussRadau);
	  
	  auto ndofs = FinEl->get_ndofs(l); 

	  auto factory = make_shared<encap::Dune_VectorFactory<double>>(ndofs);

	  auto sweeper = make_shared<AdvectionDiffusionSweeper<>>(FinEl, l);

	  //std::exit(0);
	  std::cout << "erzeuge transfer nr " << l << std::endl;
	  auto transfer = make_shared<SpectralTransfer<>>(FinEl, l, base_order);

          //ML_LOG(INFO, "expected quadrature error: " << quad->expected_error() << " (" << nnodes << ")");


          sweeper->set_quadrature(quad);
          sweeper->set_factory(factory);
          sweeper->set_residual_tolerances(abs_res_tol, rel_res_tol);

          mlsdc.add_level(sweeper, transfer);

          //ndofs  = ndofs / xrat;
          //nnodes = (nnodes - 1) / trat + 1;

        }
	}
	  
	std::cout << "nleves " << nlevs << std::endl;
        for (size_t l = 0; l < nlevs; l++) {
	  

	  //auto quad     = quadrature::quadrature_factory(nnodes, quadrature::QuadratureType::GaussLobatto);// GaussRadau);// GaussLegendre); //GaussLobatto);	  
	  auto quad     = quadrature::quadrature_factory(nnodes, quadrature::QuadratureType::GaussRadau);
	  
	  auto ndofs = FinEl->get_ndofs(l); //das feinste zuerst!!!

	  auto factory = make_shared<encap::Dune_VectorFactory<double>>(ndofs);

	  auto sweeper = make_shared<AdvectionDiffusionSweeper<>>(FinEl, l);

	  //std::exit(0);
	  std::cout << "erzeuge transfer nr " << l << std::endl;
	  auto transfer = make_shared<SpectralTransfer<>>(FinEl, l);

          //ML_LOG(INFO, "expected quadrature error: " << quad->expected_error() << " (" << nnodes << ")");


          sweeper->set_quadrature(quad);
          sweeper->set_factory(factory);
          sweeper->set_residual_tolerances(abs_res_tol, rel_res_tol);

          mlsdc.add_level(sweeper, transfer);

          //ndofs  = ndofs / xrat;
          //nnodes = (nnodes - 1) / trat + 1;

        }



	 std::cout<< "nach schleife" << std::endl;
        /*
         * set up the mlsdc controller (which in turn calls 'setup' on the
         * sweepers that were added above).  this stage typically
         * preallocates various buffers that the sweepers need.
         */
        mlsdc.setup();

        /*
         * set initial conditions on each level
         */
        auto sweeper = mlsdc.get_finest<AdvectionDiffusionSweeper<>>();
        auto q0 = sweeper->get_start_state();
        sweeper->exact(q0, 0.0);
        //  sweeper->set_residual_tolerances(1e-5, 0.0);

        /*
         * run mlsdc!
         */
	double tend = 0.5; //nsteps*dt;
        mlsdc.set_duration(0.0, tend, dt, niters);
	
	

        mlsdc.run();

	
	//mlsdc->get_end_state()
	typedef encap::Dune_VectorEncapsulation<double, double> vector_type;
        vector_type exact(FinEl->get_ndofs(0)); //= encap::as_vector<double,double>(q0);
	
	sweeper->exact(exact, tend);	
	vector_type u=  encap::as_vector<double,double>(sweeper->get_end_state());
	vector_type u0(FinEl->get_ndofs(0));
	sweeper->exact(u0, 0);
	/*for (int i=0; i< u.size(); i++){
	  std::cout<< exact[i][0] << " " << u[i][0] << " " << u0[i][0] << std::endl;
	}*/
	std::cout<< exact[4][0] << " " << u[4][0] << " " << u0[4][0] << std::endl;
	double error = (exact.axpy(-1,u)).infinity_norm();
	std::cout << "**************************************** error  " << error << std::endl; 

	ofstream f;
	stringstream ss;
	ss << nelements;
	string s = "solution_normal_mlsdc/" + ss.str() + ".dat";
	f.open(s, ios::app | std::ios::out );
	f << nelements << " " << dt << " "<< error << endl;


	f.close();

	
	
 
        tuple<error_map, residual_map> rinfo;
        get<0>(rinfo) = mlsdc.get_finest<AdvectionDiffusionSweeper<>>()->get_errors();
        for (auto l = mlsdc.coarsest(); l <= mlsdc.finest(); ++l) {
          get<1>(rinfo).insert(pair<size_t, error_map>(l.level, l.current<AdvectionDiffusionSweeper<>>()->get_residuals()));
        }
        return rinfo;
      }
    }  // ::pfasst::examples::advection_diffusion
  }  // ::pfasst::examples
}  // ::pfasst

#ifndef PFASST_UNIT_TESTING
int main(int argc, char** argv)
{
  pfasst::init(argc, argv,
               pfasst::examples::advection_diffusion::AdvectionDiffusionSweeper<>::init_logs);
  pfasst::examples::advection_diffusion::run_serial_mlsdc(3);
}
#endif
