//#include <dune/functions/gridfunctions/discreteglobalbasisfunction.hh>
#include <config.h>

#include <memory>
#include <stdexcept>
using std::shared_ptr;

#include <mpi.h>

#include "dune_includes"

#include <pfasst.hpp>
#include <pfasst/quadrature.hpp>


#include <pfasst/comm/mpi_p2p.hpp>
#include <pfasst/controller/two_level_pfasst.hpp>


#include "FE_sweeper.hpp"
#include "../../datatypes/dune_vec.hpp"
//#include "../../finite_element_stuff/spectral_transfer.hpp"
#include "spectral_transfer.hpp"

#include <vector>
//////////////////////////////////////////////////////////////////////////////////////
//
// Compiletimeparameter
//
//////////////////////////////////////////////////////////////////////////////////////

//const size_t DIM = 1;            //Räumliche Dimension des Rechengebiets ruth_dim

//const size_t BASIS_ORDER = 1;    //maximale Ordnung der Lagrange Basisfunktionen

//////////////////////////////////////////////////////////////////////////////////////


using encap_traits_t = pfasst::encap::dune_vec_encap_traits<double, double, 1>;
using pfasst::encap::DuneEncapsulation;
//using pfasst::encap::VectorEncapsulation;

using pfasst::quadrature::quadrature_factory;
using pfasst::quadrature::QuadratureType;
using pfasst::contrib::SpectralTransfer;
using pfasst::TwoLevelPfasst;
typedef pfasst::comm::MpiP2P CommunicatorType;

using pfasst::examples::heat_FE::Heat_FE;

typedef DuneEncapsulation<double, double, 1>                     EncapType;
//typedef VectorEncapsulation<double, double, 1>                     EncapType;


//typedef Heat_FE<pfasst::sweeper_traits<typename EncapType::traits>> SweeperType;
typedef Heat_FE<pfasst::examples::heat_FE::dune_sweeper_traits<encap_traits_t, BASE_ORDER, DIMENSION>> SweeperType;


typedef pfasst::transfer_traits<SweeperType, SweeperType, 1>       TransferTraits;
typedef SpectralTransfer<TransferTraits>                           TransferType;


namespace pfasst
{
  namespace examples
  {
    namespace heat_FE
    {
      void run_pfasst(const size_t nelements, const size_t basisorder, const size_t dim, const size_t& nnodes, const pfasst::quadrature::QuadratureType& quad_type,
                      const double& t_0, const double& dt, const double& t_end, const size_t& niter)
      {


        int my_rank, num_pro;
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank );
        MPI_Comm_size(MPI_COMM_WORLD, &num_pro );
        TwoLevelPfasst<TransferType, CommunicatorType> pfasst;
        pfasst.communicator() = std::make_shared<CommunicatorType>(MPI_COMM_WORLD);
        //pfasst.grid_builder(nelements);
	auto FinEl = make_shared<fe_manager>(nelements, 2);

        

        auto coarse = std::make_shared<SweeperType>(FinEl->get_basis(1), 1,  FinEl->get_grid());
        coarse->quadrature() = quadrature_factory<double>(nnodes, quad_type);
        auto fine = std::make_shared<SweeperType>(FinEl->get_basis(0), 0,  FinEl->get_grid());
        fine->quadrature() = quadrature_factory<double>(nnodes, quad_type);
        
        
        coarse->is_coarse=true;
        fine->is_coarse=false;

        auto transfer = std::make_shared<TransferType>();
	transfer->create(FinEl);
        
        fine->set_abs_residual_tol(1e-12);
        coarse->set_abs_residual_tol(1e-12);



	pfasst.add_sweeper(coarse, true);
	pfasst.add_sweeper(fine);
        
                pfasst.add_transfer(transfer);
        
        pfasst.set_options();



        pfasst.status()->time() = t_0;
        pfasst.status()->dt() = dt;
        pfasst.status()->t_end() = t_end;
        pfasst.status()->max_iterations() = niter;

        pfasst.setup();

        coarse->initial_state() = coarse->exact(pfasst.get_status()->get_time());
        fine->initial_state() = fine->exact(pfasst.get_status()->get_time());

        /*if(my_rank==0) {
        std::cout << my_rank << " test " << fine->exact(0)->data()[0] <<  std::endl;    
        for (int i=0; i< coarse->initial_state()->data().size(); i++){
          std::cout << i << " " << coarse->initial_state()->data()[i] <<  std::endl;
        }
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if(my_rank==1){
        std::cout << my_rank << " test  " << fine->exact(0)->data()[0] <<  std::endl; 
        for (int i=0; i< coarse->initial_state()->data().size(); i++){
          std::cout << i << " " << coarse->initial_state()->data()[i] <<  std::endl;
        }   
        }
        MPI_Barrier(MPI_COMM_WORLD);
        std::exit(0);*/
        
        pfasst.run();
        pfasst.post_run();

        
                MPI_Barrier(MPI_COMM_WORLD);

        
                if(my_rank==0) {
        auto anfang    = fine->exact(0)->data();
        auto naeherung = fine->get_end_state()->data();
        auto exact     = fine->exact(t_end)->data();
        for (int i=0; i< fine->get_end_state()->data().size(); i++){
          std::cout << anfang[i] << " " << naeherung[i] << "   " << exact[i] << " "  <<  std::endl;
        }

        std::cout << "******************************************* " << std::endl;
        std::cout << " " << std::endl;
        std::cout << " " << std::endl;

        fine->get_end_state()->scaled_add(-1.0, fine->exact(t_end));
        std::cout << "Fehler: "  << fine->get_end_state()->norm0() << " " << std::endl;


std::cout << "******************************************* " << std::endl;
}

        MPI_Barrier(MPI_COMM_WORLD);
        
                if(my_rank==1) {
        auto anfang    = fine->exact(0)->data();
        auto naeherung = fine->get_end_state()->data();
        auto exact     = fine->exact(t_end)->data();
        for (int i=0; i< fine->get_end_state()->data().size(); i++){
          std::cout << anfang[i] << " " << naeherung[i] << "   " << exact[i] << " "  <<  std::endl;
        }

        std::cout << "******************************************* " << std::endl;
        std::cout << " " << std::endl;
        std::cout << " " << std::endl;

        fine->get_end_state()->scaled_add(-1.0, fine->exact(t_end));
        std::cout << "Fehler: "  << fine->get_end_state()->norm0() << " " << std::endl;


std::cout << "******************************************* " << std::endl;
}

        /*for (int i=0; i<num_pro; i++){
          if(my_rank==i){
            ofstream ff;
            stringstream sss;
            sss << nelements << "_iter";
            string st = "solution_pfasst/" + sss.str() + ".dat";
            ff.open(st, ios::app | std::ios::out );
            auto iter = pfasst._it_per_step;
            for (const auto &line : iter) {
              ff << my_rank << " " << dt <<"     " << line << std::endl;
            }

            ff.close();

          }
          MPI_Barrier(MPI_COMM_WORLD);

        }*/






      }
    }  // ::pfasst::examples::heat_FE
  } // ::pfasst::examples
}  // ::pfasst


int main(int argc, char** argv)
{
  using pfasst::config::get_value;
  using pfasst::quadrature::QuadratureType;
  //Dune::MPIHelper::instance(argc, argv); 
    
//Dune::FakeMPIHelper
  MPI_Init(&argc, &argv);
        int my_rank, num_pro;
        MPI_Comm_rank(MPI_COMM_WORLD, &my_rank );
        MPI_Comm_size(MPI_COMM_WORLD, &num_pro );

         Dune::FakeMPIHelper::instance(argc, argv);
        std::vector<int> vec(100);

  for(int i = 0; i< 100; i++){
      vec[i]=i*20*3*8/3;
  }  
  
          if(my_rank==0) {
       for(int i = 0; i< 100; i++){
      //std::cout << i << " "<< vec[i] <<std::endl;
  }  
        }
        MPI_Barrier(MPI_COMM_WORLD);
        if(my_rank==1){
       for(int i = 0; i< 100; i++){
      //std::cout << i << " "<< vec[i] <<std::endl;
  }          
        }
  
  
  pfasst::init(argc, argv, SweeperType::init_opts);
  pfasst::Status<double>::create_mpi_datatype();


  const size_t nelements = get_value<size_t>("num_elements", 4); //Anzahl der Elemente pro Dimension
  const size_t nnodes = get_value<size_t>("num_nodes", 3);
  const QuadratureType quad_type = QuadratureType::GaussRadau;
  const double t_0 = 0.0;
  const double dt = get_value<double>("dt", 0.1);
  double t_end = get_value<double>("tend", 0.2);
  size_t nsteps = get_value<size_t>("num_steps", 0);
  if (t_end == -1 && nsteps == 0) {
    ML_CLOG(ERROR, "USER", "Either t_end or num_steps must be specified.");
    throw std::runtime_error("either t_end or num_steps must be specified");
  } else if (t_end != -1 && nsteps != 0) {
    if (!pfasst::almost_equal(t_0 + nsteps * dt, t_end)) {
      ML_CLOG(ERROR, "USER", "t_0 + nsteps * dt != t_end ("
      << t_0 << " + " << nsteps << " * " << dt << " = " << (t_0 + nsteps * dt)
      << " != " << t_end << ")");
      throw std::runtime_error("t_0 + nsteps * dt != t_end");
    }
  } else if (nsteps != 0) {
    t_end = t_0 + dt * nsteps;
  }
  const size_t niter = get_value<size_t>("num_iters", 10);

  pfasst::examples::heat_FE::run_pfasst(nelements, BASE_ORDER, DIMENSION, nnodes, quad_type, t_0, dt, t_end, niter);

  pfasst::Status<double>::free_mpi_datatype();

  MPI_Finalize();

  return 0;
}
