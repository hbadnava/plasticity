//shear BVP
//general headers
#include <fstream>
#include <sstream>
using namespace std;

#define feOrder   1
#define quadOrder 2 
#define meshRefineFactor 3
#define writeOutput true
#define linearSolverType PETScWrappers::SolverCG
#define totalNumIncrements 100
#define maxLinearSolverIterations 5000
#define relLinearSolverTolerance  1.0e-10
#define maxNonLinearIterations 20
#define absNonLinearTolerance 1.0e-18
#define relNonLinearTolerance 1.0e-10
#define stopOnConvergenceFailure false

//dealIIheaders
#include "../../../../src/materialModels/crystalPlasticity/fcc/crystalPlasticityFCC.cc"

//overload mesh() method to generate the required polycrystal geometry
template <int dim>
void crystalPlasticity<dim>::mesh(){
	//creating mesh
	this->pcout << "generating problem mesh\n";
	double spanX=1.0; //Span along x-axis
	double spanY=1.0; //Span along y-axis
	double spanZ=1.0; //Span along z-axis
	GridGenerator::hyper_rectangle (this->triangulation, Point<dim>(), Point<dim>(spanX,spanY,spanZ));
	this->triangulation.refine_global (meshRefineFactor);
} 

//Mark boundaries for applying Dirichlet BC's
template <int dim>
void crystalPlasticity<dim>::markBoundaries(){
	typename DoFHandler<dim>::active_cell_iterator 
		cell = this->dofHandler.begin_active(), 
		endc = this->dofHandler.end();

	//All boundaries are by marked with flag '0' by default. 
	//To pick specific boundaries, one needs to mark them 
	//with integer flags and use those flags in apply_dirichlet_conditons()
	for (;cell!=endc; ++cell){
		if (cell->is_locally_owned()){
			for (unsigned int f=0; f<GeometryInfo<dim>::faces_per_cell; ++f){
				if (cell->face(f)->at_boundary()){
					const Point<dim> face_center = cell->face(f)->center();
					//Find x3=0			  
					if (face_center[2] == 0.0){
						cell->face(f)->set_boundary_indicator (1); //bottom boundary
					}
					//Find x3=1.
					else if(face_center[2] == 1.0){
						cell->face(f)->set_boundary_indicator (2); //top boundary
					}
					//		  
					else if (face_center[1] == 0.0 || face_center[1] == 1.0 || face_center[0] == 0.0 || face_center[0] == 1.0){
						cell->face(f)->set_boundary_indicator (3); //left/right/front/back boundary
					}
				}
			}
		}
	}
}


//Class to set Dirichlet BC values 
template <int dim>
class BCFunction : public Function<dim>{
public:
	BCFunction(): Function<dim> (dim){}
	void vector_value (const Point<dim>   &p, Vector<double>   &values) const{
		Assert (values.size() == dim, ExcDimensionMismatch (values.size(), dim));    
		values[0]=0.0001; // displacement along X-Direction
	}
};

//Apply Dirchlet BCs for shear BVP
template <int dim>
void crystalPlasticity<dim>::applyDirichletBCs(){
	this->constraints.clear ();
	this->constraints.reinit (this->locally_relevant_dofs);
	DoFTools::make_hanging_node_constraints (this->dofHandler, this->constraints);
	std::vector<bool> mechanicsBoundary_Z1 (dim, true);
	std::vector<bool> mechanicsBoundary_Z2 (dim, true);
	std::vector<bool> mechanicsBoundary_Z3 (dim, true); mechanicsBoundary_Z3[0]=false;
	if (this->currentIteration==0) {
		VectorTools:: interpolate_boundary_values (this->dofHandler, 
			2, 
			BCFunction<dim>(), 
			this->constraints, 
			mechanicsBoundary_Z2);
	}
	else {
		VectorTools:: interpolate_boundary_values (this->dofHandler, 
			2, 
			ZeroFunction<dim>(dim), 
			this->constraints, 
			mechanicsBoundary_Z2);
	}
	VectorTools:: interpolate_boundary_values (this->dofHandler, 
		1, 
		ZeroFunction<dim>(dim), 
		this->constraints, 
		mechanicsBoundary_Z1);
	VectorTools:: interpolate_boundary_values (this->dofHandler, 
		3, 
		ZeroFunction<dim>(dim), 
		this->constraints, 
		mechanicsBoundary_Z3);
	this->constraints.close ();
}

//main
int main (int argc, char **argv)
{
	Utilities::System::MPI_InitFinalize mpi_initialization(argc, argv, 1);
	try
	{
		deallog.depth_console(0);
		crystalPlasticity<3> problem;

		FullMatrix<double> m_alpha,n_alpha; // Slip directions and Slip Normals
		const unsigned int n_slip_systems=12; //No. of slip systems 

		n_alpha.reinit(n_slip_systems,3);
		m_alpha.reinit(n_slip_systems,3);
		string line;
		ifstream myfile;

		// Reading slip Normals 
		myfile.open("slipNormals.txt");
		int i=0; int j=0;
		if (myfile.is_open()){
			for (unsigned int k=0; k<3*n_slip_systems; k++){	     
				getline (myfile,line,'\t');
				n_alpha[i][j] = atof(line.c_str()); 
				j++;
				if(j==3){
					i=i+1; j=0;
				}
			}
			myfile.close();
		}
		else cout << "Unable to open slipNormals file"; 
		myfile.clear() ;

		// Reading slip Normals 
		myfile.open("slipDirections.txt");
		i=0;
		j=0;
		if (myfile.is_open()){
			for (unsigned int k=0; k<3*n_slip_systems; k++){	     
				getline (myfile,line,'\t');
				m_alpha[i][j] = atof(line.c_str()); 
				j++;
				if(j==3){
					i=i+1; j=0;
				}
			}
			myfile.close();
		}
		else cout << "Unable to open slipDirections file"; 
		myfile.clear() ;

		problem.properties.n_slip_systems=n_slip_systems;
		//Latent Hardening Ratio
		problem.properties.q1=1.4;
		problem.properties.q2=1.0;
		//Slip Hardening Parameters
		problem.properties.a=2.25;
		problem.properties.h0=180;
		problem.properties.s_s=148;
		//Initial slip deformation resistance
		problem.properties.s0=16;
		//Elastic Parameters
		problem.properties.C11=170e3;
		problem.properties.C12=124e3;	
		problem.properties.C44=75e3;
		problem.properties.m_alpha=m_alpha;
		problem.properties.n_alpha=n_alpha;

		//reading materials atlas files
		unsigned int numPts[3]={10, 10, 6}; // No. of voxels in x,y and z directions
		double stencil[3]={1.0/(numPts[0]-1), 1.0/(numPts[1]-1), 1.0/(numPts[2]-1)}; // Dimensions of voxel
		problem.orientations.loadOrientations("grainID.txt",
			5,
			"orientations.txt",
			numPts,
			stencil);
		problem.orientations.loadOrientationVector("orientations.txt");
		problem.run ();
	}
	catch (std::exception &exc)
	{
		std::cerr << std::endl << std::endl
			<< "----------------------------------------------------"
			<< std::endl;
		std::cerr << "Exception on processing: " << std::endl
			<< exc.what() << std::endl
			<< "Aborting!" << std::endl
			<< "----------------------------------------------------"
			<< std::endl;
		return 1;
	}
	catch (...)
	{
		std::cerr << std::endl << std::endl
			<< "----------------------------------------------------"
			<< std::endl;
		std::cerr << "Unknown exception!" << std::endl
			<< "Aborting!" << std::endl
			<< "----------------------------------------------------"
			<< std::endl;
		return 1;
	}

	return 0;
}

