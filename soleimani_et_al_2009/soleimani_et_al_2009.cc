/*
 * Program to reproduce the results found in
 * Soleimani_et_al_2009 "Modelling of biological
 * in unsaturated porous media"
 *
 * It solves the coupled system in 1D and 2D:
 * -- Moisture movement (Darcy's equation)
 * -- Diffusion-Advection of organic matter
*/

#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/quadrature_selector.h>
#include <deal.II/base/function.h>
#include <deal.II/base/logstream.h>
#include <deal.II/base/thread_management.h> 
#include <deal.II/base/multithread_info.h>  
#include <deal.II/base/tensor_function.h>
#include <deal.II/base/parameter_handler.h>


#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/dynamic_sparsity_pattern.h>
//#include <deal.II/lac/compressed_sparsity_pattern.h>

#include <deal.II/lac/sparse_matrix.h>   
#include <deal.II/lac/solver_bicgstab.h> 
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/solver_gmres.h>
#include <deal.II/lac/precondition.h>

#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_refinement.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>
#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/grid_in.h>

#include <deal.II/dofs/dof_handler.h> 
//#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/error_estimator.h>
#include <deal.II/numerics/solution_transfer.h>

#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream> 
#include <vector>
#include <string>
#include <algorithm>
#include <time.h>
#include <map>

#include <DataTools.h>
#include "Parameters.h"

class Hydraulic_Properties {
public:

	Hydraulic_Properties (
			std::string type_of_hydraulic_properties_,
			double moisture_content_saturation_,
			double moisture_content_residual_,
			double hydraulic_conductivity_saturated_,
			double van_genuchten_alpha_,
			double van_genuchten_n_);

	double get_specific_moisture_capacity(
			double pressure_head_);
	double get_hydraulic_conductivity(
			double pressure_head_,
			double biomass_concentration_,
			double biomass_dry_density_,
			std::string relative_permeability_model_);
	double get_effective_total_saturation(
			double pressure_head_);
	double get_actual_total_saturation(
			double pressure_head_);
	double get_effective_biomass_saturation(
			double biomass_concentration_,
			double biomass_dry_density_);
	double get_actual_biomass_saturation(
			double biomass_concentration_,
			double biomass_dry_density_);
	double get_effective_free_saturation(
			double pressure_head_,
			double biomass_concentration_,
			double biomass_dry_density_);
	double get_moisture_content_total(
			double pressure_head_);
	double get_moisture_content_free(
			double pressure_head_,
			double biomass_concentration_,
			double biomass_dry_density_);
private:
	std::string type_of_hydraulic_properties;
	double moisture_content_saturation;
	double moisture_content_residual;
	double hydraulic_conductivity_saturated;

	double van_genuchten_alpha;
	double van_genuchten_n;
	double van_genuchten_m;
};

Hydraulic_Properties::Hydraulic_Properties(
		std::string type_of_hydraulic_properties_,
		double moisture_content_saturation_,
		double moisture_content_residual_,
		double hydraulic_conductivity_saturated_,
		double van_genuchten_alpha_,
		double van_genuchten_n_)
{
	type_of_hydraulic_properties=type_of_hydraulic_properties_;
	moisture_content_saturation=moisture_content_saturation_;
	moisture_content_residual=moisture_content_residual_;
	hydraulic_conductivity_saturated=hydraulic_conductivity_saturated_;

	van_genuchten_alpha=van_genuchten_alpha_;
	van_genuchten_n=van_genuchten_n_;
	van_genuchten_m=1.-1./van_genuchten_n_;
}

double Hydraulic_Properties::get_specific_moisture_capacity(
		double pressure_head)
{

	if (type_of_hydraulic_properties.compare("haverkamp_et_al_1977")==0)
	{
		double alpha=1.611E6;
		double beta =3.96;

		return(-1.*alpha*(moisture_content_saturation-moisture_content_residual)
				*beta*pressure_head*pow(fabs(pressure_head),beta-2)
		/pow(alpha+pow(fabs(pressure_head),beta),2));
	}
	else if (type_of_hydraulic_properties.compare("van_genuchten_1980")==0)
	{
		if (pressure_head>=0.)
			pressure_head=-0.01;

		return (-1.*van_genuchten_alpha*van_genuchten_m*van_genuchten_n*
				(moisture_content_saturation-moisture_content_residual)*
				pow(van_genuchten_alpha*fabs(pressure_head),van_genuchten_n-1.)*
				pow(1.+pow(van_genuchten_alpha*fabs(pressure_head),van_genuchten_n),
						-1.*van_genuchten_m-1.)*
						pressure_head/fabs(pressure_head));
	}
	else
	{
		std::cout << "Equations for \"" << type_of_hydraulic_properties
				<< "\" are not implemented. Error.\n";
		throw -1;
	}
}

double Hydraulic_Properties::get_effective_total_saturation(
		double pressure_head)
{
	if (type_of_hydraulic_properties.compare("van_genuchten_1980")==0)
	{
		double effective_total_saturation=-1000.;
		if (pressure_head>=0.)
			effective_total_saturation=
					1.;
		else
			effective_total_saturation=
					1./pow(1.+pow(van_genuchten_alpha*fabs(pressure_head),van_genuchten_n),van_genuchten_m);
		return (effective_total_saturation);
	}
	else
	{
		std::cout << "Equations for \"" << type_of_hydraulic_properties
				<< "\" are not implemented. Error.\n";
		throw -1;
	}
}

double Hydraulic_Properties::get_actual_total_saturation(
		double pressure_head)
{
	return ((moisture_content_residual/moisture_content_saturation)+
			(1.-moisture_content_residual/moisture_content_saturation)*
			get_effective_total_saturation(pressure_head));
}

double Hydraulic_Properties::get_effective_biomass_saturation(
		double biomass_concentration,
		double biomass_dry_density)
{
	double actual_biomass_saturation=
			biomass_concentration/biomass_dry_density;

	double effective_biomass_saturation=
			actual_biomass_saturation/(1.-(moisture_content_residual/moisture_content_saturation));

	if (effective_biomass_saturation>1.)
		effective_biomass_saturation=1.;

	return (effective_biomass_saturation);
}

double Hydraulic_Properties::get_actual_biomass_saturation(
		double biomass_concentration,
		double biomass_dry_density)
{
	return (get_effective_biomass_saturation(biomass_concentration,biomass_dry_density)*
			(1.-(moisture_content_residual/moisture_content_saturation)));
}

double Hydraulic_Properties::get_effective_free_saturation(
		double pressure_head,
		double biomass_concentration,
		double biomass_dry_density)
{
	double effective_free_saturation=
			get_effective_total_saturation(pressure_head)-
			get_effective_biomass_saturation(biomass_concentration,biomass_dry_density);

	if (effective_free_saturation<=0.)
		effective_free_saturation=0.;

	return (effective_free_saturation);
}

double Hydraulic_Properties::get_hydraulic_conductivity(
		double pressure_head,
		double biomass_concentration, // mg_biomass/cm3_void
		double biomass_dry_density,
		std::string relative_permeability_model)
{
	if (type_of_hydraulic_properties.compare("haverkamp_et_al_1977")==0)
	{
		double gamma=4.74;
		double A=1.175E6;

		return (hydraulic_conductivity_saturated*A/(A+pow(fabs(pressure_head),gamma)));
	}
	else if (type_of_hydraulic_properties.compare("van_genuchten_1980")==0)
	{
		double effective_total_saturation=
				get_effective_total_saturation(pressure_head);

		double effective_biomass_saturation=
				get_effective_biomass_saturation(biomass_concentration,biomass_dry_density);

		if (effective_biomass_saturation>effective_total_saturation)
			effective_total_saturation=effective_biomass_saturation;

		double relative_permeability=0.;
		if (relative_permeability_model.compare("soleimani")==0)
		{
			relative_permeability=
					pow(effective_total_saturation,0.5)*
					pow(pow(1.-pow(effective_biomass_saturation,1./van_genuchten_m),van_genuchten_m)-
							pow(1.-pow(effective_total_saturation,1./van_genuchten_m),van_genuchten_m),2.);
		}
		else if (relative_permeability_model.compare("clement")==0)
		{
			if (biomass_concentration/biomass_dry_density<1.)//cm3_biomass/cm3_void
				relative_permeability=
						pow(1.-biomass_concentration/biomass_dry_density,19/6);
			else
				relative_permeability=0.;

		}
		else if (relative_permeability_model.compare("okubo_and_matsumoto")==0)
		{
			if (biomass_concentration/biomass_dry_density<1.)//cm3_biomass/cm3_void
				relative_permeability=
						pow(1.-biomass_concentration/biomass_dry_density,2);
			else
				relative_permeability=0.;

		}
		else if (relative_permeability_model.compare("vandevivere")==0)
		{
			/*
			 * By Philippe Vandevivere,"Bacterial clogging of porous media:
			 * a new modelling approach", 1995
			 * */
			if (biomass_concentration/biomass_dry_density<1.)//cm3_biomass/cm3_void
			{
				double plug_hydraulic_conductivity=0.00025;
				double critical_biovolume_fraction=0.1;
				//double critical_porosity=0.9;
				//double relative_porosity=1.-biomass_concentration/biomass_dry_density;
				double biovolume_fraction=biomass_concentration/biomass_dry_density;
				double phi=exp(-0.5*pow(biovolume_fraction/critical_biovolume_fraction,2));

				relative_permeability
				=phi*pow(1-biovolume_fraction,2.)
				+
				(1.-phi)*plug_hydraulic_conductivity/(plug_hydraulic_conductivity+
						biovolume_fraction*(1.-plug_hydraulic_conductivity));
			}
			else
				relative_permeability=0.;
		}
		else
		{
			std::cout << "Relative permeability model not implemented: "
					<< relative_permeability_model << ".\n"
					<< "Available models are: soleimani, clement, okubo_and_matsumoto";
			throw -1;
		}

		return(hydraulic_conductivity_saturated*relative_permeability);
	}
	else
	{
		std::cout << "Equations for \"" << type_of_hydraulic_properties
				<< "\" are not implemented. Error.\n";
		throw -1;
	}
}

double Hydraulic_Properties::get_moisture_content_total(
		double pressure_head)
{
	return((moisture_content_saturation-moisture_content_residual)*
			get_effective_total_saturation(pressure_head)
			+moisture_content_residual);
}

double Hydraulic_Properties::get_moisture_content_free(double pressure_head,
						       double biomass_concentration,
						       double biomass_dry_density)
{
  return((moisture_content_saturation-moisture_content_residual)*
	 get_effective_free_saturation(pressure_head,biomass_concentration,biomass_dry_density)
	 +moisture_content_residual);
}

namespace TRL
{
  using namespace dealii;
#include <InitialValue.h>

  template<int dim>
  class BottomPressure:public Function<dim>
  {
  public:
    BottomPressure():Function<dim>(){}
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
  };

  template <int dim>
  double BottomPressure<dim>::value (const Point<dim>   &p,
				     const unsigned int) const
  {
    double required_velocity=7.834E-3;//cm/s
    double return_value=
      1.+
      100.*(required_velocity/0.05+1.);
    
    // double x=p[0];
    // if ((x<-30. && x>-35.) || (x<-65. && x>-70.))
    //   {
    // 	return_value-=3.;
    //   }
      
    return return_value;
  }

  
  template <int dim>
  class Heat_Pipe
  {
  public:
    Heat_Pipe(int argc, char *argv[]);
    ~Heat_Pipe();
    void run();

  private:
    void read_grid();
    void refine_grid(bool refine);
    void setup_system();
    void initial_condition();
    void assemble_system_flow();
    void assemble_system_transport();
    void solve_system_flow();
    void solve_system_transport();
    void output_results() const;

    void calculate_mass_balance_ratio();
    void hydraulic_properties(double pressure_head,
			      double &specific_moisture_capacity,
			      double &hydraulic_conductivity,
			      double &actual_total_moisture_content,
			      double &actual_free_water_moisture_content,
			      std::string hydraulic_properties,
			      double biomass_concentration=0,
			      bool stop_bacterial_growth=false);

    Triangulation<dim> triangulation;
    DoFHandler<dim>    dof_handler;
    FE_Q<dim>          fe;
    ConstraintMatrix   hanging_node_constraints;
    SparsityPattern    sparsity_pattern;

    // Richards' equation variables
    SparseMatrix<double> system_matrix_flow;
    SparseMatrix<double> mass_matrix_richards;
    SparseMatrix<double> laplace_matrix_new_richards;
    SparseMatrix<double> laplace_matrix_old_richards;
    Vector<double>       system_rhs_flow;
    Vector<double>       solution_flow_new_iteration;
    Vector<double>       solution_flow_old_iteration;
    Vector<double>       old_solution_flow;
    // Substrate variables
    SparseMatrix<double> system_matrix_transport;
    SparseMatrix<double> mass_matrix_transport_new;
    SparseMatrix<double> mass_matrix_transport_old;
    SparseMatrix<double> laplace_matrix_new_transport;
    SparseMatrix<double> laplace_matrix_old_transport;
    Vector<double>       system_rhs_transport;
    Vector<double>       solution_transport;
    Vector<double>       old_solution_transport;
    
    unsigned int timestep_number_max;
    unsigned int timestep_number;
    unsigned int refinement_level;
    double       time;
    double       time_step;
    double       time_max;
    double       theta_richards;
    double       theta_transport;
    std::string  mesh_filename;
    bool use_mesh_file;
    bool solve_flow;

    double milestone_time;
    double time_for_dry_conditions;
    double time_for_saturated_conditions;
    bool transient_drying;
    bool transient_saturation;
    bool transient_transport;
    bool activate_transport;
    bool test_transport;
    bool coupled_transport;
    
    Vector<double> old_nodal_biomass_concentration;
    Vector<double> new_nodal_biomass_concentration;
    Vector<double> old_nodal_biomass_fraction;
    Vector<double> new_nodal_biomass_fraction;    
    Vector<double> old_nodal_total_moisture_content;
    Vector<double> new_nodal_total_moisture_content;
    Vector<double> old_nodal_free_moisture_content;
    Vector<double> new_nodal_free_moisture_content;
    Vector<double> old_nodal_hydraulic_conductivity;
    Vector<double> new_nodal_hydraulic_conductivity;
    Vector<double> old_nodal_specific_moisture_capacity;
    Vector<double> new_nodal_specific_moisture_capacity;
    Vector<double> boundary_ids;
    std::vector<std::vector<double> > average_hydraulic_conductivity_vector;
    Parameters::AllParameters<dim>  parameters;

    unsigned int figure_count;
    bool         redefine_time_step;
    double flow_at_top;
    double flow_at_bottom;
    double nutrient_flow_at_top;
    double nutrient_flow_at_bottom;
    double nutrients_in_domain_previous;
    double nutrients_in_domain_current;
    double cumulative_flow_at_top;
    double cumulative_flow_at_bottom;
    std::map<std::vector<double>,unsigned int> repeated_points;
  };

  template<int dim>
  Heat_Pipe<dim>::Heat_Pipe(int argc, char *argv[])
    :
    dof_handler(triangulation),
    fe(1)
  {
    std::cout << "Program run with the following arguments:\n";
    if (argc!=2)
      {
	for (int i=0; i<argc; i++)
	  std::cout << "arg " << i << " : " << argv[i] << "\n";
	std::cout << "Error, wrong number of arguments passed to the program.\n"
		  << "Expected input: 'program name' 'input parameter file'\n";

      }
    else
      {
	std::cout << "Program name        : " << argv[0] << "\n";
	std::cout << "Input parameter file: " << argv[1] << "\n\n";
      }

    std::string input_filename = argv[1];
    std::cout << "parameter file: " << input_filename << "\n";
    std::ifstream inFile;
    inFile.open(input_filename.c_str());
    
    ParameterHandler prm;
    Parameters::AllParameters<dim>::declare_parameters (prm);
    prm.parse_input(inFile,input_filename);
    parameters.parse_parameters (prm);

    theta_richards      = parameters.theta_richards;
    theta_transport     = parameters.theta_transport;
    timestep_number_max = parameters.timestep_number_max;
    time_step           = parameters.time_step;
    time_max            = time_step*timestep_number_max;
    refinement_level    = parameters.refinement_level;
    use_mesh_file       = parameters.use_mesh_file;
    mesh_filename       = parameters.mesh_filename;
    test_transport      = parameters.test_function_transport;
    coupled_transport   = parameters.coupled_transport;
    
    timestep_number=0;
    time=0;
    solve_flow                   =true;
    milestone_time               =0;
    time_for_dry_conditions      =0;
    time_for_saturated_conditions=0;
    activate_transport           =false;
    figure_count                 =0;
    redefine_time_step           =false;
    
    flow_at_top=0.;
    flow_at_bottom=0.;
    nutrient_flow_at_top=0.;
    nutrient_flow_at_bottom=0.;
    nutrients_in_domain_previous=0.;
    nutrients_in_domain_current=0.;
    cumulative_flow_at_top=0.;
    cumulative_flow_at_bottom=0.;

    if (parameters.initial_state.compare("default")==0 ||
	parameters.initial_state.compare("final")==0)
      {
	transient_drying=true;
	transient_saturation=false;
	transient_transport=false;
      }
    else if (parameters.initial_state.compare("dry")==0)
      {
	transient_drying=false;
	transient_saturation=true;
	transient_transport=false;
      }
    else if (parameters.initial_state.compare("saturated")==0)
      {
	transient_drying=false;
	transient_saturation=false;
	transient_transport=true;
      }
    else if (parameters.initial_state.compare("no_drying")==0)
      {
	transient_drying=false;
	transient_saturation=true;
	transient_transport=false;
      }
    else
      {
	std::cout << "Wrong initial state specified in input file."
		  << "\"" << parameters.initial_state << "\" is not a valid "
		  << "parameter.";
	throw 1;
      }

    std::cout << "Solving problem with : "
	      << "\n\ttheta pressure     : " << theta_richards
	      << "\n\ttheta transport    : " << theta_transport
	      << "\n\ttimestep_number_max: " << timestep_number_max
	      << "\n\ttime_step          : " << time_step
	      << "\n\ttime_max           : " << time_max
	      << "\n\trefinement_level   : " << refinement_level
	      << "\n\tuse_mesh_file      : " << use_mesh_file
	      << "\n\tmesh_filename      : " << mesh_filename
	      << "\n\tcells              : " << triangulation.n_active_cells()
	      << "\n\tInitial State      : " << parameters.initial_state
	      << "\n\tTransport output frequency: " << parameters.output_frequency_transport
	      << "\n\n";
  }
  
  template<int dim>
  Heat_Pipe<dim>::~Heat_Pipe ()
  {
    dof_handler.clear ();
  }

  template <int dim>
  void Heat_Pipe<dim>::calculate_mass_balance_ratio()
  {
    new_nodal_biomass_concentration.reinit (dof_handler.n_dofs());
    new_nodal_biomass_fraction.reinit      (dof_handler.n_dofs());
    new_nodal_hydraulic_conductivity.reinit(dof_handler.n_dofs());
    new_nodal_total_moisture_content.reinit(dof_handler.n_dofs());
    new_nodal_free_moisture_content.reinit (dof_handler.n_dofs());
    new_nodal_specific_moisture_capacity.reinit(dof_handler.n_dofs());
    
    QGauss<dim>quadrature_formula(2);
    FEValues<dim>fe_values(fe, quadrature_formula,
			   update_values|update_gradients|
			   update_quadrature_points|
			   update_JxW_values);
    const unsigned int dofs_per_cell=fe.dofs_per_cell;
    std::vector<unsigned int> local_dof_indices(fe.dofs_per_cell);
    
    Vector<double> cell_biomass_concentration(dofs_per_cell);
    Vector<double> cell_biomass_fraction     (dofs_per_cell);
    Vector<double> cell_hydraulic_conductivity(dofs_per_cell);
    Vector<double> cell_total_moisture_content(dofs_per_cell);
    Vector<double> cell_free_moisture_content(dofs_per_cell);
    Vector<double> cell_moisture_capacity(dofs_per_cell);

    Vector<double> old_biomass_concentration;
    
    Vector<double> old_transport_values;
    Vector<double> new_transport_values;
    Vector<double> new_pressure_values_old_iteration;
    Vector<double> old_pressure_values;
    
    typename DoFHandler<dim>::active_cell_iterator
      cell = dof_handler.begin_active(),
      endc = dof_handler.end();
    for (; cell!=endc; ++cell)
      {
	fe_values.reinit (cell);
	cell_biomass_concentration=0.;
	cell_biomass_fraction=0.;
	cell_hydraulic_conductivity=0.;
	cell_total_moisture_content=0.;
	cell_free_moisture_content=0.;
	cell_moisture_capacity=0.;

	old_biomass_concentration
	  .reinit(cell->get_fe().dofs_per_cell);
	new_pressure_values_old_iteration
	  .reinit(cell->get_fe().dofs_per_cell);
	old_pressure_values
	  .reinit(cell->get_fe().dofs_per_cell);
	old_transport_values
	  .reinit(cell->get_fe().dofs_per_cell);
	new_transport_values
	  .reinit(cell->get_fe().dofs_per_cell);

	cell->get_dof_values(solution_flow_old_iteration,new_pressure_values_old_iteration);
	cell->get_dof_values(old_solution_flow          ,old_pressure_values);
	cell->get_dof_values(old_solution_transport     ,old_transport_values);
	cell->get_dof_values(solution_transport         ,new_transport_values);
	cell->get_dof_values(old_nodal_biomass_concentration,old_biomass_concentration);


	double saturated_hydraulic_conductivity=parameters.saturated_hydraulic_conductivity;
	//double radius=0.;
	double factor=1.;
	/*
	 * Hydraulic conductivity variation 1
	 */
	// if (dim==2)
	//   radius=sqrt(pow(cell->center()[0]+50.,2.)+pow(cell->center()[1]+50.,2.));
	// if (dim==1)
	//   radius=sqrt(pow(cell->center()[0]+50.,2.));
	// if (radius<=30.)
	//   {
	//     factor=0.9*(1.-pow(cos(radius*M_PI/60.),2.))+0.1;
	//   }
	/*
	 * Hydraulic conductivity variation 1 -- ramp function
	 */
	// double delta=10.;
	// if (dim==2)
	//   {
	//     if (cell->center()[1]>=-80.)
	//       if (cell->center()[0]<-65.)
	// 	{
	// 	  if (cell->center()[1]>=-70.)
	// 	    factor=0.1;
	// 	  else
	// 	    factor=((0.1-1.0)/delta)*(delta+cell->center()[1]+70.)+1.;
	// 	}
	//       else if (cell->center()[0]<-35.)
	// 	{
	// 	  if (cell->center()[1]>=-70.)
	// 	    factor=0.5;
	// 	  else
	// 	    factor=((0.5-1.0)/delta)*(delta+cell->center()[1]+70.)+1.;
	// 	}
	//   }

	saturated_hydraulic_conductivity*=factor;
	Hydraulic_Properties hydraulic_properties(parameters.hydraulic_properties,
						  parameters.moisture_content_saturation,
						  parameters.moisture_content_residual,
						  saturated_hydraulic_conductivity,
						  //parameters.saturated_hydraulic_conductivity,
						  parameters.van_genuchten_alpha,
						  parameters.van_genuchten_n);
	for (unsigned int i=0; i<GeometryInfo<dim>::vertices_per_cell; i++)
	  {
	    Point<dim> vertex=cell->vertex(i);
	    std::vector<double> point_wrapper;
	    for (unsigned int j=0; j<dim; j++)
	      point_wrapper.push_back(vertex[j]);
	    std::map<std::vector<double>,unsigned int>::iterator it;
	    it=repeated_points.find(point_wrapper);
	    if (it==repeated_points.end())//the point is not in the map
	      {
		std::cout << "error, point " << std::endl;
		for (unsigned int p=0; p<dim; p++)
		  {
		    std::cout << point_wrapper[p] << " ";
		  }
		std::cout << " not in the map" << std::endl;
		throw -1;
	      }
	    
	    if (transient_drying==false)
	      {
		double old_substrate=0;
		if (old_transport_values[i]>0)
		  old_substrate=old_transport_values[i];
		double effective_saturation_free=
		  hydraulic_properties
		  .get_effective_free_saturation(old_pressure_values[i],
						 old_biomass_concentration[i],
						 parameters.biomass_dry_density);
		
		cell_biomass_concentration(i)+=(1./it->second)*
		  old_biomass_concentration(i)*
		  exp((parameters.yield_coefficient*parameters.maximum_substrate_use_rate*
		       effective_saturation_free*old_substrate/
		       (effective_saturation_free*old_substrate+parameters.half_velocity_constant/1000.)
		       -parameters.decay_rate)*time_step);
		
		cell_biomass_fraction(i)+=(1./it->second)*
		  cell_biomass_concentration(i)/
		  parameters.biomass_dry_density;
	      }

	    cell_hydraulic_conductivity(i)+=(1./it->second)*
	      hydraulic_properties
	      .get_hydraulic_conductivity(new_pressure_values_old_iteration[i],
					  cell_biomass_concentration[i],
					  parameters.biomass_dry_density,
					  parameters.relative_permeability_model);
	    
	    cell_total_moisture_content(i)+=(1./it->second)*
	      hydraulic_properties.get_moisture_content_total(new_pressure_values_old_iteration[i]);
	    
	    cell_free_moisture_content(i)+=(1./it->second)*
	      hydraulic_properties.get_moisture_content_free(new_pressure_values_old_iteration[i],
							     cell_biomass_concentration[i],
							     parameters.biomass_dry_density);
	    
	    cell_moisture_capacity(i)+=(1./it->second)*
	      hydraulic_properties.get_specific_moisture_capacity(new_pressure_values_old_iteration[i]);
	  }
	    
	cell->get_dof_indices (local_dof_indices);
	for (unsigned int i=0; i<dofs_per_cell; ++i)
	  {
	    new_nodal_biomass_concentration(local_dof_indices[i])+=cell_biomass_concentration(i);
	    new_nodal_biomass_fraction(local_dof_indices[i])+=cell_biomass_fraction(i);
	    new_nodal_hydraulic_conductivity(local_dof_indices[i])+=cell_hydraulic_conductivity(i);
	    new_nodal_total_moisture_content(local_dof_indices[i])+=cell_total_moisture_content(i);
	    new_nodal_free_moisture_content(local_dof_indices[i])+=cell_free_moisture_content(i);
	    new_nodal_specific_moisture_capacity(local_dof_indices[i])+=cell_moisture_capacity(i);
	  }
      }
  }

  template <int dim>
  void Heat_Pipe<dim>::read_grid()
  {
    if(use_mesh_file)
      {
	GridIn<dim> grid_in;
	grid_in.attach_triangulation(triangulation);
	std::ifstream input_file(mesh_filename);
	grid_in.read_msh(input_file);
	
	//triangulation.refine_global(4);
      }
    else
      {
	GridGenerator::hyper_cube(triangulation,-1.*parameters.domain_size/*(cm)*/,0);
	triangulation.refine_global(refinement_level);	
      }
    /*
     * Count repeated dofs
     */    
    {
      QGauss<dim>quadrature_formula(2);
      FEValues<dim>fe_values(fe, quadrature_formula,
			     update_values|update_gradients|
			     update_quadrature_points|
			     update_JxW_values);
      //const unsigned int dofs_per_cell=fe.dofs_per_cell;
      boundary_ids.reinit(triangulation.n_active_cells());
      unsigned int vector_index=0;
      typename Triangulation<dim>::active_cell_iterator
	cell = triangulation.begin_active(),
	endc = triangulation.end();
      for (; cell!=endc; ++cell)
	{
	  fe_values.reinit (cell);
	  for (unsigned int i=0; i<GeometryInfo<dim>::vertices_per_cell; i++)
	    {
	      Point<dim> vertex=cell->vertex(i);
	      std::vector<double> point_wrapper;
	      for (unsigned int j=0; j<dim; j++)
		point_wrapper.push_back(vertex[j]);
	      std::map<std::vector<double>,unsigned int>::iterator it;
	      it=repeated_points.find(point_wrapper);
	      if (it!=repeated_points.end())//the point is already in the map
		it->second++;
	      else // the point is not yet in the map
		repeated_points[point_wrapper]=1;
	    }
	    
	  for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
	    {
	      if (cell->face(face)->at_boundary() && cell->face(face)->boundary_id()!=0)
		{
		  if (use_mesh_file)
		    {
		      boundary_ids[vector_index]=cell->face(face)->boundary_id();
		    }
		  else
		    {
		      if (fabs(cell->face(face)->center()[dim-1]-0.)<1E-4)//top
			{
			  cell->face(face)->set_boundary_id(1);
			  boundary_ids[vector_index]=1;
			}
		      else if (fabs(cell->face(face)->center()[dim-1]+parameters.domain_size)<1E-4)//bottom
			{
			  cell->face(face)->set_boundary_id(2);
			  boundary_ids[vector_index]=2;
			}
		    }
		}
	    }
	  vector_index++;
	}
	
      // for (std::map<std::vector<double>,unsigned int>::iterator it=repeated_points.begin();
      //      it!=repeated_points.end(); ++it)
      //   {
      //     std::cout << "\t" << it->first[0] << "\t" << it->first[1] << "\t" << it->second << "\n";
      //   }
    }
  }

  template <int dim>
  void Heat_Pipe<dim>::refine_grid (bool refine)
  {
    Vector<float> estimated_error_per_cell(triangulation.n_active_cells());
    //Vector<float> estimated_error_per_cell_2(triangulation.n_active_cells());

    if (refine)
      KellyErrorEstimator<dim>::estimate(dof_handler,
					 QGauss<dim-1>(2),
					 typename FunctionMap<dim>::type(),
					 solution_transport,
					 estimated_error_per_cell);
    else
      KellyErrorEstimator<dim>::estimate(dof_handler,
					 QGauss<dim-1>(2),
					 typename FunctionMap<dim>::type(),
					 solution_flow_new_iteration,
					 estimated_error_per_cell);
    
    // KellyErrorEstimator<dim>::estimate(dof_handler,
    // 				       QGauss<dim-1>(2),
    // 				       typename FunctionMap<dim>::type(),
    // 				       solution_flow_new_iteration,
    // 				       estimated_error_per_cell_2);
    
    //unsigned int old_active_cells=triangulation.n_active_cells();
    // for (unsigned int i=0; i<triangulation.n_active_cells(); i++)
    //   {
    // 	// if (i<0.15*triangulation.n_active_cells())
    // 	//   estimated_error_per_cell[i]
    // 	//     =estimated_error_per_cell[i];
    // 	// if (i>=0.15*triangulation.n_active_cells() &&
    // 	//     i<0.30*triangulation.n_active_cells())
    // 	//   estimated_error_per_cell[i]
    // 	//     =estimated_error_per_cell_2[i];
    // 	// else if (i>=0.30*triangulation.n_active_cells())
    // 	estimated_error_per_cell[i]
    // 	  =std::max(estimated_error_per_cell[i],estimated_error_per_cell_2[i]);
    //   }

   
    GridRefinement::refine_and_coarsen_fixed_fraction(triangulation,
						      estimated_error_per_cell,
						      0.49, 0.50,20000);
    // else
    //   GridRefinement::refine_and_coarsen_fixed_fraction(triangulation,
    // 							estimated_error_per_cell,
    // 							0.0, 0.50,500000);
    
    std::vector<Vector<double> > transfer_in;
    transfer_in.push_back(old_solution_flow);
    transfer_in.push_back(solution_flow_new_iteration);
    transfer_in.push_back(solution_flow_old_iteration);
    transfer_in.push_back(old_solution_transport);
    transfer_in.push_back(solution_transport);
    transfer_in.push_back(old_nodal_biomass_concentration);
    transfer_in.push_back(new_nodal_biomass_concentration);
    transfer_in.push_back(old_nodal_biomass_fraction);
    transfer_in.push_back(new_nodal_biomass_fraction);
    transfer_in.push_back(old_nodal_free_moisture_content);
    transfer_in.push_back(new_nodal_free_moisture_content);
    transfer_in.push_back(old_nodal_total_moisture_content);
    transfer_in.push_back(new_nodal_total_moisture_content);
    transfer_in.push_back(old_nodal_hydraulic_conductivity);
    transfer_in.push_back(new_nodal_hydraulic_conductivity);
    transfer_in.push_back(old_nodal_specific_moisture_capacity);
    transfer_in.push_back(new_nodal_specific_moisture_capacity);
    
    SolutionTransfer<dim> solution_transfer(dof_handler);
    
    triangulation
      .prepare_coarsening_and_refinement();
    solution_transfer
      .prepare_for_coarsening_and_refinement(transfer_in);
    triangulation
      .execute_coarsening_and_refinement();
    //dof_handler.distribute_dofs(fe);
    setup_system();
    
    std::vector<Vector<double> > transfer_out(transfer_in.size());
    for (unsigned int i=0; i<transfer_in.size(); i++)
      transfer_out[i].reinit(dof_handler.n_dofs());
    
    solution_transfer.interpolate(transfer_in,transfer_out);
    old_solution_flow                   =transfer_out[0];
    solution_flow_new_iteration         =transfer_out[1];
    solution_flow_old_iteration         =transfer_out[2];
    old_solution_transport              =transfer_out[3];
    solution_transport                  =transfer_out[4];
    old_nodal_biomass_concentration     =transfer_out[5];
    new_nodal_biomass_concentration     =transfer_out[6];
    old_nodal_biomass_fraction          =transfer_out[7];
    new_nodal_biomass_fraction          =transfer_out[8];
    old_nodal_free_moisture_content     =transfer_out[9];
    new_nodal_free_moisture_content     =transfer_out[10];
    old_nodal_total_moisture_content    =transfer_out[11];
    new_nodal_total_moisture_content    =transfer_out[12];
    old_nodal_hydraulic_conductivity    =transfer_out[13];
    new_nodal_hydraulic_conductivity    =transfer_out[14];
    old_nodal_specific_moisture_capacity=transfer_out[15];
    new_nodal_specific_moisture_capacity=transfer_out[16];

    //std::cout << "\tOld cell #: " << old_active_cells
    //<< "\tNew cell #: " << triangulation.n_active_cells() << "\n";
    {
      QGauss<dim>quadrature_formula(2);
      FEValues<dim>fe_values(fe, quadrature_formula,
			     update_values|update_gradients|
			     update_quadrature_points|
			     update_JxW_values);
      //const unsigned int dofs_per_cell=fe.dofs_per_cell;
      boundary_ids.reinit(triangulation.n_active_cells());
      unsigned int vector_index=0;
      typename Triangulation<dim>::active_cell_iterator
	cell = triangulation.begin_active(),
	endc = triangulation.end();
      repeated_points.clear();
      for (; cell!=endc; ++cell)
	{
	  fe_values.reinit (cell);
	  for (unsigned int i=0; i<GeometryInfo<dim>::vertices_per_cell; i++)
	    {
	      Point<dim> vertex=cell->vertex(i);
	      std::vector<double> point_wrapper;
	      for (unsigned int j=0; j<dim; j++)
		point_wrapper.push_back(vertex[j]);
	      std::map<std::vector<double>,unsigned int>::iterator it;
	      it=repeated_points.find(point_wrapper);
	      if (it!=repeated_points.end())//the point is already in the map
		it->second++;
	      else // the point is not yet in the map
		repeated_points[point_wrapper]=1;
	    }
	  
	  for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
	    {
	      if (cell->face(face)->at_boundary() && cell->face(face)->boundary_id()!=0)
		{
		  if (use_mesh_file)
		    {
		      boundary_ids[vector_index]=cell->face(face)->boundary_id();
		    }
		  else
		    {
		      if (fabs(cell->face(face)->center()[dim-1]-0.)<1E-4)//top
			{
			  cell->face(face)->set_boundary_id(1);
			  boundary_ids[vector_index]=1;
			}
		      else if (fabs(cell->face(face)->center()[dim-1]+parameters.domain_size)<1E-4)//bottom
			{
			  cell->face(face)->set_boundary_id(2);
			  boundary_ids[vector_index]=2;
			}
		    }
		}
	    }
	  vector_index++;
	}
    }
  }
  
  template <int dim>
  void Heat_Pipe<dim>::setup_system()
  {
    dof_handler.distribute_dofs(fe);
    hanging_node_constraints.clear();
    DoFTools::make_hanging_node_constraints(dof_handler,
					    hanging_node_constraints);
    hanging_node_constraints.close();

    DynamicSparsityPattern csp(dof_handler.n_dofs(),
			       dof_handler.n_dofs());

    DoFTools::make_sparsity_pattern(dof_handler,csp);

    hanging_node_constraints.condense(csp);
    //SparsityPattern sparsity_pattern;
    sparsity_pattern.copy_from(csp);

    solution_flow_new_iteration.reinit(dof_handler.n_dofs());
    solution_flow_old_iteration.reinit(dof_handler.n_dofs());
    old_solution_flow.reinit(dof_handler.n_dofs());

    solution_transport.reinit(dof_handler.n_dofs());
    old_solution_transport.reinit(dof_handler.n_dofs());

    old_nodal_biomass_concentration.reinit(dof_handler.n_dofs());
    new_nodal_biomass_concentration.reinit(dof_handler.n_dofs());
    old_nodal_biomass_fraction.reinit(dof_handler.n_dofs());
    new_nodal_biomass_fraction.reinit(dof_handler.n_dofs());
    old_nodal_total_moisture_content.reinit(dof_handler.n_dofs());
    new_nodal_total_moisture_content.reinit(dof_handler.n_dofs());
    old_nodal_free_moisture_content.reinit(dof_handler.n_dofs());
    new_nodal_free_moisture_content.reinit(dof_handler.n_dofs());
    old_nodal_hydraulic_conductivity.reinit(dof_handler.n_dofs());
    new_nodal_hydraulic_conductivity.reinit(dof_handler.n_dofs());
    old_nodal_specific_moisture_capacity.reinit(dof_handler.n_dofs());
    new_nodal_specific_moisture_capacity.reinit(dof_handler.n_dofs());
  }

  template <int dim>
  void Heat_Pipe<dim>::assemble_system_transport()
  {
    system_rhs_transport.reinit        (dof_handler.n_dofs());
    system_matrix_transport.reinit     (sparsity_pattern);
    mass_matrix_transport_new.reinit   (sparsity_pattern);
    mass_matrix_transport_old.reinit   (sparsity_pattern);
    laplace_matrix_new_transport.reinit(sparsity_pattern);
    laplace_matrix_old_transport.reinit(sparsity_pattern);

    QGauss<dim>   quadrature_formula(2);
    QGauss<dim-1> face_quadrature_formula(2);
    FEValues<dim> fe_values(fe,quadrature_formula,
			    update_values | update_gradients |
			    update_JxW_values);
    FEFaceValues<dim> fe_face_values(fe, face_quadrature_formula,
				     update_values|update_gradients|
				     update_normal_vectors|
				     update_quadrature_points|update_JxW_values);
    
    const unsigned int dofs_per_cell  =fe.dofs_per_cell;
    const unsigned int n_q_points     =quadrature_formula.size();
    const unsigned int n_face_q_points=face_quadrature_formula.size();

    FullMatrix<double> cell_mass_matrix_new   (dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_mass_matrix_old   (dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_laplace_matrix_new(dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_laplace_matrix_old(dofs_per_cell,dofs_per_cell);
    Vector<double>     cell_rhs(dofs_per_cell);

    std::vector<unsigned int> local_dof_indices (fe.dofs_per_cell);
    Vector<double> old_substrate_values;
    Vector<double> new_substrate_values;
    Vector<double> old_pressure_values;
    Vector<double> new_pressure_values;
    Vector<double> old_biomass_concentration_values;
    Vector<double> new_biomass_concentration_values;
    Vector<double> old_free_moisture_content_values;
    Vector<double> new_free_moisture_content_values;
    Vector<double> old_hydraulic_conductivity_values;
    Vector<double> new_hydraulic_conductivity_values;
    
    Vector<double> old_moisture_capacity_values;
    Vector<double> new_moisture_capacity_values;
    
    double face_boundary_indicator;
    nutrient_flow_at_bottom=0.;
    nutrient_flow_at_top=0.;
    nutrients_in_domain_current=0.;
    
    typename DoFHandler<dim>::active_cell_iterator
      cell = dof_handler.begin_active(),
      endc = dof_handler.end();
    for (; cell!=endc; ++cell)
      {
    	fe_values.reinit (cell);
    	cell_mass_matrix_new=0;
    	cell_mass_matrix_old=0;
    	cell_laplace_matrix_new=0;
    	cell_laplace_matrix_old=0;
	cell_rhs=0;

	old_substrate_values.reinit(cell->get_fe().dofs_per_cell);
	new_substrate_values.reinit(cell->get_fe().dofs_per_cell);
	old_pressure_values.reinit(cell->get_fe().dofs_per_cell);
    	new_pressure_values.reinit(cell->get_fe().dofs_per_cell);
	old_biomass_concentration_values.reinit(cell->get_fe().dofs_per_cell);
	new_biomass_concentration_values.reinit(cell->get_fe().dofs_per_cell);
	old_free_moisture_content_values.reinit(cell->get_fe().dofs_per_cell);
	new_free_moisture_content_values.reinit(cell->get_fe().dofs_per_cell);
	old_hydraulic_conductivity_values.reinit(cell->get_fe().dofs_per_cell);
	new_hydraulic_conductivity_values.reinit(cell->get_fe().dofs_per_cell);
	
    	cell->get_dof_values(old_solution_transport,old_substrate_values);
	cell->get_dof_values(    solution_transport,new_substrate_values);
    	cell->get_dof_values(old_solution_flow              ,old_pressure_values);
    	cell->get_dof_values(    solution_flow_old_iteration,new_pressure_values);
	cell->get_dof_values(old_nodal_biomass_concentration,old_biomass_concentration_values);
	cell->get_dof_values(new_nodal_biomass_concentration,new_biomass_concentration_values);
	cell->get_dof_values(old_nodal_free_moisture_content,old_free_moisture_content_values);
	cell->get_dof_values(new_nodal_free_moisture_content,new_free_moisture_content_values);
	cell->get_dof_values(old_nodal_hydraulic_conductivity,old_hydraulic_conductivity_values);
	cell->get_dof_values(new_nodal_hydraulic_conductivity,new_hydraulic_conductivity_values);
	/*
	 * Calculate local velocities, diffusivities
	 */
	Tensor<1,dim> new_velocity;
	Tensor<1,dim> old_velocity;
	double dV=0;
	for (unsigned int q_point=0; q_point<n_q_points; ++q_point)
	  {
	    for (unsigned int k=0; k<dofs_per_cell; ++k)
	      {
		new_velocity-=
		  new_hydraulic_conductivity_values[k]*
		  (new_pressure_values(k)+
		   cell->vertex(k)[dim-1])*
		  fe_values.shape_grad(k,q_point)*
		  fe_values.JxW(q_point);
		old_velocity-=
		  old_hydraulic_conductivity_values[k]*
		  (old_pressure_values(k)+
		   cell->vertex(k)[dim-1])*
		  fe_values.shape_grad(k,q_point)*
		  fe_values.JxW(q_point);
		dV+=
		  fe_values.shape_value(k,q_point)*
		  fe_values.JxW(q_point);
		nutrients_in_domain_current+=//mg/cm3_soil
		  new_free_moisture_content_values[k]*
		  new_substrate_values[k]*
		  fe_values.shape_value(k,q_point)*
		  fe_values.JxW(q_point);
	      }
	  }
	new_velocity/=dV;
	old_velocity/=dV;
	if (new_velocity.norm()<1E-6)
	  {
	    new_velocity=0.;
	    old_velocity=0.;
	  }
	if (new_velocity.norm()>=1E-6 && old_velocity.norm()<1E-6)
	  {
	    old_velocity=new_velocity;
	  }
	if (numbers::is_nan(new_velocity.norm()) || numbers::is_nan(old_velocity.norm()))
	  {
	    std::cout << "error in velocities calulation\n";
	    throw -1;
	  }
	if (!numbers::is_finite(new_velocity.norm()) || !numbers::is_finite(old_velocity.norm()))
	  {
	    std::cout << "error in velocities calulation\n";
	    throw -1;
	  }
	
	double new_diffusion_value=
	  parameters.dispersivity_longitudinal*new_velocity.norm()+
	  parameters.effective_diffusion_coefficient;
	double old_diffusion_value=
	  parameters.dispersivity_longitudinal*old_velocity.norm()+
	  parameters.effective_diffusion_coefficient;

	double Peclet=0.;
	double beta=0.;
	double tau=0.;
	if (new_velocity.norm()>=1E-6 && new_diffusion_value>1.E-10 && old_diffusion_value>1.E-10)
	  {
	    Peclet=
	      0.5*cell->diameter()*(0.5*new_velocity.norm()+0.5*old_velocity.norm())/
	      (0.5*new_diffusion_value+0.5*old_diffusion_value);
	    beta=
	      (1./tanh(Peclet)-1./Peclet);
	    tau=	  
	      0.5*beta*cell->diameter()/(0.5*new_velocity.norm()+0.5*old_velocity.norm());

	    //std::cout << "\t" << Peclet << "\t" << beta << "\t" << tau << "\n";
	  }
	
	if (Peclet<0 || beta<0 || tau<0)
	  {
	    std::cout << "error in Peclet number calulation\n";
	    throw -1;
	  }
	if (numbers::is_nan(Peclet) || numbers::is_nan(beta) || numbers::is_nan(tau))
	  {
	    std::cout << "error in Peclet number calulation\n";
	    throw -1;
	  }
	if (!numbers::is_finite(Peclet) || !numbers::is_finite(beta) || !numbers::is_finite(tau))
	  {
	    std::cout << "error in Peclet number calulation\n";
	    throw -1;
	  }
	
	for (unsigned int q_point=0; q_point<n_q_points; ++q_point)
	  {
	    for (unsigned int k=0; k<dofs_per_cell; ++k)
	      {
		double new_sink_factor=0;
		double old_sink_factor=0;
		// if (test_transport==true)
		//   {
		//     new_sink_factor=0;
		//     old_sink_factor=0;
		//   }
		// else if (parameters.homogeneous_decay_rate==true)
		//   {
		//     new_sink_factor=parameters.first_order_decay_factor;
		//     old_sink_factor=parameters.first_order_decay_factor;
		//   }
		// else
		//   {
		//     /* *
		//      * Some of the variables for the transport equation defined in the input file
		//      * are provided in [mg_substrate/L_total_water]. They need to be transformed
		//      * to [mg_substrate/cm3_total_water] to be consistent with the primary variable
		//      * in the transport equation and to [mg_biomass/cm3_total_water] in case of the
		//      * biomass concentration variable defined in the program as
		//      * [mg_biomass/cm3_total_water]. This is done in this way:
		//      *
		//      * half_velocity_constant[mg_substrate/L_total_water]
		//      * =half_velocity_constant[mg_substrate/L_total_water]*
		//      * total_water_volume_ratio [L_total_water/1000 cm3_total_water]
		//      * =(1./1000.)*half_velocity_constant[mg_substrate/cm3_total_water]
		//      * */
		//     new_sink_factor=
		//       -1.*parameters.porosity*
		//       new_biomass_concentration_values[k]*parameters.maximum_substrate_use_rate
		//       /(old_substrate_values[k]+parameters.half_velocity_constant/1000.);
		//     old_sink_factor=
		//       -1.*parameters.porosity*
		//       old_biomass_concentration_values[k]*parameters.maximum_substrate_use_rate
		//       /(old_substrate_values[k]+parameters.half_velocity_constant/1000.);
		//   }
				
		for (unsigned int i=0; i<dofs_per_cell; ++i)
		  {
		    for (unsigned int j=0; j<dofs_per_cell; ++j)
		      {
			/*i=test function, j=concentration IMPORTANT!!*/
			cell_mass_matrix_new(i,j)+=
			  (
			   fe_values.shape_value(i,q_point)
			   +
			   tau*
			   new_velocity*
			   fe_values.shape_grad(i,q_point)
			   )*
			  fe_values.shape_value(j,q_point)*
			  new_free_moisture_content_values[k]*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point);
			
			cell_mass_matrix_old(i,j)+=
			  (
			   fe_values.shape_value(i,q_point)
			   +
			   tau*
			   old_velocity*
			   fe_values.shape_grad(i,q_point)
			   )*
			  fe_values.shape_value(j,q_point)*
			  old_free_moisture_content_values[k]*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point);
			
			cell_laplace_matrix_new(i,j)+=
			  /*Diffusive term*/
			  fe_values.shape_grad(i,q_point)*
			  fe_values.shape_grad(j,q_point)*
			  new_diffusion_value*
			  new_free_moisture_content_values[k]*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point)
			  +
			  /*Convective term*/
			  (
			   fe_values.shape_value(i,q_point)
			   +
			   tau*
			   new_velocity*			   
			   fe_values.shape_grad(i,q_point)
			   )*
			  fe_values.shape_grad(j,q_point)*
			  new_velocity*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point)
			  /*Reaction term*/
			  -
			  (
			   fe_values.shape_value(i,q_point)
			   +
			   tau*
			   new_velocity*			   
			   fe_values.shape_grad(i,q_point)
			   )*
			  fe_values.shape_value(j,q_point)*
			  new_sink_factor*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point);

			cell_laplace_matrix_old(i,j)+=
			  /*Diffusive term*/
			  old_diffusion_value*
			  old_free_moisture_content_values[k]*
			  fe_values.shape_value(k,q_point)*
			  fe_values.shape_grad(i,q_point)*
			  fe_values.shape_grad(j,q_point)*
			  fe_values.JxW(q_point)
			  +
			  /*Convective term*/
			  (
			   fe_values.shape_value(i,q_point)
			   +
			   tau*
			   old_velocity*			   
			   fe_values.shape_grad(i,q_point)
			   )*
			  fe_values.shape_grad(j,q_point)*
			  old_velocity*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point)
			  /*Reaction term*/
			  -
			  (
			   fe_values.shape_value(i,q_point)
			   +
			   tau*
			   old_velocity*			   
			   fe_values.shape_grad(i,q_point)
			   )*
			  fe_values.shape_value(j,q_point)*
			  old_sink_factor*
			  fe_values.shape_value(k,q_point)*
			  fe_values.JxW(q_point);
		      }
		  }
	      }
	  }
	
	for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
	  {
	    if (cell->face(face)->at_boundary())
	      {
		fe_face_values.reinit(cell,face);
		face_boundary_indicator=cell->face(face)->boundary_id();
		//inlet
		if ((parameters.transport_fixed_at_top==false) &&
		    ((face_boundary_indicator==1 && parameters.transport_mass_entry_point.compare("top")==0) ||
		     (face_boundary_indicator==2 && parameters.transport_mass_entry_point.compare("bottom")==0)))
		  {
		    for (unsigned int q_face_point=0; q_face_point<n_face_q_points; ++q_face_point)
		      {
			for (unsigned int k=0; k<dofs_per_cell; ++k)
			  {
			    double concentration_at_boundary=//mg_substrate/cm3_total_water
			      parameters.transport_top_fixed_value/1000.;
			    
			    for (unsigned int i=0; i<dofs_per_cell; ++i)
			      {
				for (unsigned int j=0; j<dofs_per_cell; ++j)
				  {/*i=test function, j=concentration IMPORTANT!!*/
				    cell_laplace_matrix_new(i,j)-=
				      (
				       fe_face_values.shape_value(i,q_face_point)
				       +
				       tau*
				       new_velocity*			   
				       fe_face_values.shape_grad(i,q_face_point)
				       )*
				      fe_face_values.shape_value(j,q_face_point)*
				      new_velocity*
				      fe_face_values.normal_vector(q_face_point)*
				      fe_face_values.shape_value(k,q_face_point)*
				      fe_face_values.JxW(q_face_point);

				    cell_laplace_matrix_old(i,j)-=
				      (
				       fe_face_values.shape_value(i,q_face_point)
				       +
				       tau*
				       old_velocity*			   
				       fe_face_values.shape_grad(i,q_face_point)
				       )*
				      fe_face_values.shape_value(j,q_face_point)*
				      old_velocity*
				      fe_face_values.normal_vector(q_face_point)*
				      fe_face_values.shape_value(k,q_face_point)*
				      fe_face_values.JxW(q_face_point);
				  }
				cell_rhs(i)-=
				  (
				   fe_face_values.shape_value(i,q_face_point)
				   +
				   tau*
				   new_velocity*			   
				   fe_face_values.shape_grad(i,q_face_point)
				   )*
				  time_step*
				  (theta_transport)*
				  concentration_at_boundary*
				  new_velocity*
				  fe_face_values.normal_vector(q_face_point)*
				  fe_face_values.shape_value(k,q_face_point)*
				  fe_face_values.JxW(q_face_point)
				  +
				  (
				   fe_face_values.shape_value(i,q_face_point)
				   +
				   tau*
				   new_velocity*			   
				   fe_face_values.shape_grad(i,q_face_point)
				   )*
				  time_step*
				  (1.-theta_transport)*
				  concentration_at_boundary*
				  old_velocity*
				  fe_face_values.normal_vector(q_face_point)*
				  fe_face_values.shape_value(k,q_face_point)*
				  fe_face_values.JxW(q_face_point);
			      }
			  }
		      }
		  }

		//outlet
		if (false /*(parameters.transport_fixed_at_top==false) &&
		    ((face_boundary_indicator==2 && parameters.transport_mass_entry_point.compare("top")==0) ||
		    (face_boundary_indicator==1 && parameters.transport_mass_entry_point.compare("bottom")==0))*/)
		  {
		    for (unsigned int q_face_point=0; q_face_point<n_face_q_points; ++q_face_point)
		      {
			for (unsigned int k=0; k<dofs_per_cell; ++k)
			  {			
			    double new_velocity=
			      fe_face_values.normal_vector(q_face_point)*
			      new_hydraulic_conductivity_values[k]*
			      (new_pressure_values(k)+
			       cell->vertex(k)[dim-1])*
			      fe_face_values.shape_grad(k,q_face_point);
		
			    double old_velocity=
			      fe_face_values.normal_vector(q_face_point)*
			      old_hydraulic_conductivity_values[k]*
			      (old_pressure_values(k)+
			       cell->vertex(k)[dim-1])*
			      fe_face_values.shape_grad(k,q_face_point);
		    
			    for (unsigned int i=0; i<dofs_per_cell; ++i)
			      {
				for (unsigned int j=0; j<dofs_per_cell; ++j)
				  {
				    cell_laplace_matrix_new(i,j)+=
				      new_velocity*
				      fe_face_values.shape_value(i,q_face_point)*
				      fe_face_values.shape_value(j,q_face_point)*
				      fe_face_values.JxW(q_face_point)
				      // +
				      // tau*
				      // advection_field*
				      // /*new_discharge_darcy_velocity*/
				      // fe_face_values.shape_grad(i,q_face_point)*
				      // new_discharge_darcy_velocity*
				      // fe_face_values.shape_value(j,q_face_point)*
				      // fe_face_values.JxW(q_face_point)
				      ;

				    cell_laplace_matrix_old(i,j)+=
				      old_velocity*
				      fe_face_values.shape_value(i,q_face_point)*
				      fe_face_values.shape_value(j,q_face_point)*
				      fe_face_values.JxW(q_face_point)
				      // +
				      // tau*
				      // advection_field*
				      // /*old_discharge_darcy_velocity*/
				      // fe_face_values.shape_grad(i,q_face_point)*
				      // old_discharge_darcy_velocity*
				      // fe_face_values.shape_value(j,q_face_point)*
				      // fe_face_values.JxW(q_face_point)
				      ;
				  }
				// cell_rhs(i)-=
				//   time_step*
				//   (theta_transport)*
				//   concentration_at_boundary*
				//   // flow_direction*
				//   // new_discharge_darcy_velocity*
				//   new_velocity*
				//   fe_face_values.shape_value(i,q_face_point)*
				//   fe_face_values.JxW(q_face_point)
				//   // +
				//   // time_step*
				//   // (theta_transport)*
				//   // tau*
				//   // advection_field*
				//   // /*new_discharge_darcy_velocity*/
				//   // fe_face_values.shape_grad(i,q_face_point)*
				//   // concentration_at_boundary*
				//   // new_discharge_darcy_velocity*
				//   // fe_face_values.JxW(q_face_point)
				//   +
				//   time_step*
				//   (1.-theta_transport)*
				//   concentration_at_boundary*
				//   // flow_direction*
				//   // old_discharge_darcy_velocity*
				//   old_velocity*
				//   fe_face_values.shape_value(i,q_face_point)*
				//   fe_face_values.JxW(q_face_point)
				//   // +
				//   // time_step*
				//   // (1.-theta_transport)*
				//   // tau*
				//   // advection_field*
				//   // /*old_discharge_darcy_velocity*/
				//   // fe_face_values.shape_grad(i,q_face_point)*
				//   // concentration_at_boundary*
				//   // old_discharge_darcy_velocity*
				//   // fe_face_values.JxW(q_face_point)
				//   ;
			      }
			  }
		      }
		  }
		/*
		 * Calculate nutrient flow through the boundary
		 */
		{
		  double flow=0.;
		  for (unsigned int q_face_point=0; q_face_point<n_face_q_points; ++q_face_point)
		    {
		      for (unsigned int i=0; i<dofs_per_cell; ++i)
		        {
			  for (unsigned int k=0; k<dofs_per_cell; ++k)
			    {
			      flow+=
				-(theta_transport)*
				(
				 fe_face_values.shape_value(i,q_face_point)
				 +
				 tau*
				 new_velocity*			   
				 fe_face_values.shape_grad(i,q_face_point)
				 )*
				new_diffusion_value*
				new_substrate_values[k]*
				new_free_moisture_content_values[k]*
				fe_face_values.shape_grad(k,q_face_point)*
				fe_face_values.normal_vector(q_face_point)*
				fe_face_values.JxW(q_face_point)
				+
				(theta_transport)*
				(
				 fe_face_values.shape_value(i,q_face_point)
				 +
				 tau*
				 new_velocity*			   
				 fe_face_values.shape_grad(i,q_face_point)
				 )*
				//fe_face_values.shape_value(i,q_face_point)*
				new_substrate_values[k]*
				new_velocity*
				fe_face_values.shape_value(k,q_face_point)*
				fe_face_values.normal_vector(q_face_point)*
				fe_face_values.JxW(q_face_point)
				-
				(1.-theta_transport)*
				(
				 fe_face_values.shape_value(i,q_face_point)
				 +
				 tau*
				 old_velocity*			   
				 fe_face_values.shape_grad(i,q_face_point)
				 )*
				old_diffusion_value*
				old_free_moisture_content_values[k]*
				old_substrate_values[k]*
				fe_face_values.normal_vector(q_face_point)*
				fe_face_values.shape_grad(k,q_face_point)*
				fe_face_values.JxW(q_face_point)
				+
				(1.-theta_transport)*
				(
				 fe_face_values.shape_value(i,q_face_point)
				 +
				 tau*
				 old_velocity*			   
				 fe_face_values.shape_grad(i,q_face_point)
				 )*
				//fe_face_values.shape_value(i,q_face_point)*
				old_substrate_values[k]*
				old_velocity*
				fe_face_values.shape_value(k,q_face_point)*
				fe_face_values.normal_vector(q_face_point)*
				fe_face_values.JxW(q_face_point);
			    }
			}
		    }

		  if (face_boundary_indicator==2)
		    {
		      nutrient_flow_at_bottom+=flow;
		    }
		  else if (face_boundary_indicator==1)
		    {
		      nutrient_flow_at_top+=flow;
		    }
		}
		
	      }
	  }

    	cell->get_dof_indices(local_dof_indices);

    	for (unsigned int i=0; i<dofs_per_cell; ++i)
	  {
	    for (unsigned int j=0; j<dofs_per_cell; ++j)
	      {
		laplace_matrix_new_transport
		  .add(local_dof_indices[i],local_dof_indices[j],cell_laplace_matrix_new(i,j));
		laplace_matrix_old_transport
		  .add(local_dof_indices[i],local_dof_indices[j],cell_laplace_matrix_old(i,j));
		mass_matrix_transport_new
		  .add(local_dof_indices[i],local_dof_indices[j],cell_mass_matrix_new(i,j));
		mass_matrix_transport_old
		  .add(local_dof_indices[i],local_dof_indices[j],cell_mass_matrix_old(i,j));
	      }
	    system_rhs_transport(local_dof_indices[i])+=cell_rhs(i);
	  }
      }

    // std::cout << std::fixed << std::setprecision(5)
    // 	      << "\ttime step: " << time_step << " s\n"
    // 	      << "\tflow of nutrients at bottom: " << nutrient_flow_at_bottom << " mg/s\t"
    // 	      << "\tflow of nutrients at top: "    << nutrient_flow_at_top << " mg/s\n"
    // 	      << "\tnutrients in domain: "         << nutrients_in_domain_current << " mg\n"
    // 	      << "\tcumulative flow of nutrients at bottom: " << cumulative_flow_at_bottom << " mg\t"
    // 	      << "\tcumulative flow of nutrients at top: " << cumulative_flow_at_top << " mg\n";
    
    Vector<double> tmp(solution_transport.size ());

    mass_matrix_transport_old.vmult   ( tmp,old_solution_transport);
    system_rhs_transport.add          ( 1.0,tmp);
    laplace_matrix_old_transport.vmult( tmp,old_solution_transport);
    system_rhs_transport.add          (-(1-theta_transport)*time_step,tmp);

    system_matrix_transport.copy_from (mass_matrix_transport_new);
    system_matrix_transport.add       (theta_transport*time_step,laplace_matrix_new_transport);

    hanging_node_constraints.condense(system_matrix_transport);
    hanging_node_constraints.condense(system_rhs_transport);
    /* *
     * The boundary condition for the transport equation defined in the input file
     * is provided in [mg_substrate/L_free_water]. It needs to be transformed to
     * [mg_substrate/cm3_soil] to be consistent with the primary
     * variable in the transport equation. This is done in this way:
     *
     * boundary_condition[mg_substrate/L_free_water]
     * =boundary_condition[mg_substrate/L_free_water]*
     * moisture_content_free_water_at_boundary[cm3_free_water/cm3_soil]*
     * free_water_volume_ratio[L_free_water/1000 cm3_total_water]
     * =(1./1000.)*boundary_condition[mg_substrate/cm3_soil]
     * */
    //    if (parameters.transport_fixed_at_top==true)
    //    {
    //    	double boundary_pressure_at_top=
    //    			VectorTools::point_value(dof_handler,
    //    					solution_flow_old_iteration,
    //						Point<dim>(0.));
    ////    	double boundary_moisture_content_total_water=
    ////    			hydraulic_properties
    ////				.get_moisture_content_total(boundary_pressure_at_top);
    //
    //    	double boundary_moisture_content_total_water=
    //    			hydraulic_properties
    //				.get_moisture_content_free(
    //						boundary_pressure_at_top,
    //						biomass_concentration,
    //						parameters.biomass_dry_density);
    //
    //    	double boundary_condition_substrate=0.;
    //    	if (test_transport==true)
    //    		boundary_condition_substrate=
    //    				parameters.transport_top_fixed_value*(1./1000.);
    //    	else
    //    		boundary_condition_substrate=
    //    				boundary_moisture_content_total_water*
    //					parameters.transport_top_fixed_value*(1./1000.);
    //
    //    	std::map<unsigned int,double> boundary_values;
    //    	boundary_values.clear();
    //    	VectorTools::interpolate_boundary_values (dof_handler,
    //    			1,
    //				ConstantFunction<dim>(boundary_condition_substrate),
    //				boundary_values);
    //    	MatrixTools::apply_boundary_values (boundary_values,
    //    			system_matrix_transport,
    //				solution_transport,
    //				system_rhs_transport);
    //    }
  }

  template <int dim>
  void Heat_Pipe<dim>::assemble_system_flow()
  {
    system_rhs_flow.reinit         (dof_handler.n_dofs());
    system_matrix_flow.reinit      (sparsity_pattern);
    mass_matrix_richards.reinit        (sparsity_pattern);
    laplace_matrix_new_richards.reinit (sparsity_pattern);
    laplace_matrix_old_richards.reinit (sparsity_pattern);

    std::string quadrature_option;
    unsigned int order=0;
    if (parameters.lumped_matrix==false)
      {
    	quadrature_option="gauss";
    	order=2;
      }
    if (parameters.lumped_matrix==true)
      quadrature_option="trapez";

    QuadratureSelector<dim> quadrature_formula(quadrature_option,order);
    //QGauss<dim>   quadrature_formula(1);
    QGauss<dim-1>     face_quadrature_formula(1);
    FEValues<dim>fe_values(fe, quadrature_formula,
			   update_values|update_gradients|
			   update_quadrature_points|
			   update_JxW_values);
    FEFaceValues<dim> fe_face_values(fe, face_quadrature_formula,
				     update_values|update_gradients|
				     update_normal_vectors|
				     update_quadrature_points|update_JxW_values);
    const unsigned int dofs_per_cell  =fe.dofs_per_cell;
    const unsigned int n_face_q_points=face_quadrature_formula.size();
    const unsigned int n_q_points     =quadrature_formula.size();
    
    FullMatrix<double> cell_mass_matrix       (dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_laplace_matrix_new(dofs_per_cell,dofs_per_cell);
    FullMatrix<double> cell_laplace_matrix_old(dofs_per_cell,dofs_per_cell);
    Vector<double>     cell_rhs               (dofs_per_cell);

    std::vector<unsigned int> local_dof_indices(fe.dofs_per_cell);
    Vector<double> old_pressure_values;
    Vector<double> new_pressure_values;
    Vector<double> old_hydraulic_conductivity_values;
    Vector<double> new_hydraulic_conductivity_values;
    Vector<double> old_total_moisture_content_values;
    Vector<double> new_total_moisture_content_values;
    Vector<double> old_moisture_capacity_values;
    Vector<double> new_moisture_capacity_values;
    
    Hydraulic_Properties hydraulic_properties(parameters.hydraulic_properties,
					      parameters.moisture_content_saturation,
					      parameters.moisture_content_residual,
					      parameters.saturated_hydraulic_conductivity,
					      parameters.van_genuchten_alpha,
					      parameters.van_genuchten_n);
    double face_boundary_indicator;
    flow_at_top=0.;
    flow_at_bottom=0.;
    unsigned int cell_integer_index=0;
    typename DoFHandler<dim>::active_cell_iterator
      cell = dof_handler.begin_active(),
      endc = dof_handler.end();
    for (; cell!=endc; ++cell)
      {
	fe_values.reinit (cell);
	cell_mass_matrix       =0;
	cell_laplace_matrix_new=0;
	cell_laplace_matrix_old=0;
	cell_rhs               =0;

	old_pressure_values.reinit(cell->get_fe().dofs_per_cell);
	new_pressure_values.reinit(cell->get_fe().dofs_per_cell);
	old_hydraulic_conductivity_values.reinit(cell->get_fe().dofs_per_cell);
	new_hydraulic_conductivity_values.reinit(cell->get_fe().dofs_per_cell);
	old_total_moisture_content_values.reinit(cell->get_fe().dofs_per_cell);
	new_total_moisture_content_values.reinit(cell->get_fe().dofs_per_cell);
	old_moisture_capacity_values.reinit(cell->get_fe().dofs_per_cell);
	new_moisture_capacity_values.reinit(cell->get_fe().dofs_per_cell);
	
	cell->get_dof_values(old_solution_flow,old_pressure_values);
	cell->get_dof_values(solution_flow_old_iteration,new_pressure_values);
	cell->get_dof_values(old_nodal_hydraulic_conductivity,old_hydraulic_conductivity_values);
	cell->get_dof_values(new_nodal_hydraulic_conductivity,new_hydraulic_conductivity_values);
	cell->get_dof_values(old_nodal_total_moisture_content,old_total_moisture_content_values);
	cell->get_dof_values(new_nodal_total_moisture_content,new_total_moisture_content_values);
	cell->get_dof_values(old_nodal_specific_moisture_capacity,old_moisture_capacity_values);
	cell->get_dof_values(new_nodal_specific_moisture_capacity,new_moisture_capacity_values);
	
	for (unsigned int q_point=0; q_point<n_q_points; ++q_point)
	  {
	    for (unsigned int k=0; k<dofs_per_cell; k++)
	      {
		for (unsigned int i=0; i<dofs_per_cell; ++i)
		  {
		    for (unsigned int j=0; j<dofs_per_cell; ++j)
		      {
			if (parameters.moisture_transport_equation.compare("head")==0)
			  {
			    cell_mass_matrix(i,j)+=
			      (theta_richards)*
			      new_moisture_capacity_values[k]*
			      fe_values.shape_value(k,q_point)*
			      fe_values.shape_value(i,q_point)*
			      fe_values.shape_value(j,q_point)*
			      fe_values.JxW(q_point)
			      +
			      (1-theta_richards)*
			      old_moisture_capacity_values[k]*
			      fe_values.shape_value(k,q_point)*
			      fe_values.shape_value(i,q_point)*
			      fe_values.shape_value(j,q_point)*
			      fe_values.JxW(q_point);
			  }
			else if (parameters.moisture_transport_equation.compare("mixed")==0)
			  {
			    cell_mass_matrix(i,j)+=
			      new_moisture_capacity_values[k]*
			      fe_values.shape_value(k,q_point)*
			      fe_values.shape_value(j,q_point)*
			      fe_values.shape_value(i,q_point)*
			      fe_values.JxW(q_point);
			  }
			else
			  {
			    std::cout << "Moisture transport equation \""
				      << parameters.moisture_transport_equation
				      << "\" is not implemented. Error.\n";
			    throw -1;
			  }
			cell_laplace_matrix_new(i,j)+=(new_hydraulic_conductivity_values[k]*
						       fe_values.shape_value(k,q_point)*
						       fe_values.shape_grad(j,q_point)*
						       fe_values.shape_grad(i,q_point)*
						       fe_values.JxW(q_point));

			cell_laplace_matrix_old(i,j)+=(old_hydraulic_conductivity_values[k]*
						       fe_values.shape_value(k,q_point)*
						       fe_values.shape_grad(j,q_point)*
						       fe_values.shape_grad(i,q_point)*
						       fe_values.JxW(q_point));
		      }
		    cell_rhs(i)-=
		      time_step*
		      (theta_richards)*
		      new_hydraulic_conductivity_values[k]*
		      fe_values.shape_value(k,q_point)*
		      fe_values.shape_grad(i,q_point)[dim-1]*
		      fe_values.JxW(q_point)
		      +
		      time_step*
		      (1.-theta_richards)*
		      old_hydraulic_conductivity_values[k]*
		      fe_values.shape_value(k,q_point)*
		      fe_values.shape_grad(i,q_point)[dim-1]*
		      fe_values.JxW(q_point);

		    if (parameters.moisture_transport_equation.compare("mixed")==0)
		      {
			cell_rhs(i)-=(new_total_moisture_content_values[k]-
				      old_total_moisture_content_values[k])*
			  fe_values.shape_value(k,q_point)*
			  fe_values.shape_value(i,q_point)*
			  fe_values.JxW(q_point);
		      }
		  }
	      }
	  }
	
	for (unsigned int face=0; face<GeometryInfo<dim>::faces_per_cell; ++face)
	  {
	    if (cell->face(face)->at_boundary())
	      {
		fe_face_values.reinit (cell,face);
		face_boundary_indicator=cell->face(face)->boundary_id();
		
		if ((face_boundary_indicator==1)&&//top
		    (parameters.richards_fixed_at_top==false))//second kind b.c.
		  {
		    double flow=0.0;
		    if (transient_drying==false)
		      flow=parameters.richards_top_flow_value;
		    
		    for (unsigned int q_face_point=0; q_face_point<n_face_q_points; ++q_face_point)
		      for (unsigned int k=0; k<dofs_per_cell; ++k)
			for (unsigned int i=0; i<dofs_per_cell; ++i)
			  cell_rhs(i)-=
			    time_step*
			    (theta_richards)*flow*
			    fe_face_values.shape_value(k,q_face_point)*
			    fe_face_values.shape_value(i,q_face_point)*
			    fe_face_values.JxW(q_face_point)
			    +
			    time_step*
			    (1.-theta_richards)*flow*
			    fe_face_values.shape_value(k,q_face_point)*
			    fe_face_values.shape_value(i,q_face_point)*
			    fe_face_values.JxW(q_face_point);
		  }
		//-------------------------------
		// No boundary condition for the bottom?
		//-------------------------------
		/*
		 * Estimate the flow passing throw the top and bottom boundaries
		 */
		if (face_boundary_indicator==1 || //top OR bottom
		    face_boundary_indicator==2)
		  {
		    double flow=0.;
		    for (unsigned int q_face_point=0; q_face_point<n_face_q_points; ++q_face_point)
		      for (unsigned int k=0; k<dofs_per_cell; ++k)
			{
			  for (unsigned int i=0; i<dofs_per_cell; ++i)
			    {
			      for (unsigned int j=0; j<dofs_per_cell; ++j)
				{
				  flow-=
				    (theta_richards)*
				    new_hydraulic_conductivity_values[k]*
				    fe_face_values.shape_value(k,q_face_point)*
				    fe_face_values.normal_vector(q_face_point)*
				    fe_face_values.shape_grad(j,q_face_point)*
				    (
				     new_pressure_values(j)
				     +
				     cell->vertex(j)[dim-1]
				     )*
				    fe_face_values.shape_value(i,q_face_point)*
				    fe_face_values.JxW(q_face_point)
				    +
				    (1.-theta_richards)*
				    old_hydraulic_conductivity_values[k]*
				    fe_face_values.shape_value(k,q_face_point)*
				    fe_face_values.normal_vector(q_face_point)*
				    fe_face_values.shape_grad(j,q_face_point)*
				    (
				     old_pressure_values(j)
				     +
				     cell->vertex(j)[dim-1]
				     )*
				    fe_face_values.shape_value(i,q_face_point)*
				    fe_face_values.JxW(q_face_point);
				}
			    }
			}
		    if (face_boundary_indicator==1)//top
		      flow_at_top+=flow;
		    if (face_boundary_indicator==2)//bottom
		      flow_at_bottom+=flow;
		  }
	      }
	  }
	
	cell_integer_index++;
	cell->get_dof_indices (local_dof_indices);
	for (unsigned int i=0; i<dofs_per_cell; ++i)
	  {
	    for (unsigned int j=0; j<dofs_per_cell; ++j)
	      {
		laplace_matrix_new_richards
		  .add(local_dof_indices[i],local_dof_indices[j],cell_laplace_matrix_new(i,j));
		laplace_matrix_old_richards
		  .add(local_dof_indices[i],local_dof_indices[j],cell_laplace_matrix_old(i,j));
		mass_matrix_richards
		  .add(local_dof_indices[i],local_dof_indices[j],cell_mass_matrix(i,j));
	      }
	    system_rhs_flow(local_dof_indices[i])+=cell_rhs(i);
	  }
      }

    // std::cout << std::scientific << std::setprecision(2)
    // 	      << "\tflow at top: " << flow_at_top << " cm3/s"
    // 	      << "\tflow at bottom: " << flow_at_bottom << " cm3/s\n";
    
    Vector<double> tmp(solution_flow_new_iteration.size ());
    if (parameters.moisture_transport_equation.compare("head")==0)
      mass_matrix_richards.vmult(tmp,old_solution_flow);
    else if (parameters.moisture_transport_equation.compare("mixed")==0)
      mass_matrix_richards.vmult(tmp,solution_flow_old_iteration);
    else
      {
	std::cout << "Moisture transport equation \""
		  << parameters.moisture_transport_equation
		  << "\" is not implemented. Error.\n";
	throw -1;
      }

    system_rhs_flow.add          ( 1.0,tmp);
    laplace_matrix_old_richards.vmult( tmp,old_solution_flow);
    system_rhs_flow.add          (-(1-theta_richards)*time_step,tmp);

    system_matrix_flow.copy_from(mass_matrix_richards);
    system_matrix_flow.add      (theta_richards*time_step, laplace_matrix_new_richards);

    hanging_node_constraints.condense(system_matrix_flow);
    hanging_node_constraints.condense(system_rhs_flow);

    std::map<unsigned int,double> boundary_values;
    if (parameters.richards_fixed_at_bottom==true)
      {
	double boundary_condition_bottom_fixed_pressure=
	  parameters.domain_size+parameters.richards_top_fixed_value;
	/*
	 * Based on Paris thesis, p80.
	 * The peristaltic pump was supposed to apply a constant flow rate
	 * during the experiment but from measured flow rates, it seems like
	 * this pump failed to apply a constant flow rate and it applied a
	 * constant head instead.
	 *
	 * The nutrient solution was delivered at an initial flow rate of
	 * 2.5 ml/min. The nutrient feeding rate was set at 16 min per day
	 * using a timer. This flow rate was selected to provide a total
	 * flow of 40ml for each sand fraction.
	 */
	//double intpart=0.;
	//double fractpart=std::modf((time-milestone_time)/(24.*3600.),&intpart);
	//if (transient_transport==true && fractpart>=0. && fractpart<16.*60./(24.*3600.))
	boundary_condition_bottom_fixed_pressure=
	  parameters.richards_bottom_fixed_value;
	VectorTools::interpolate_boundary_values(dof_handler,
						 2,
						 ConstantFunction<dim>(boundary_condition_bottom_fixed_pressure),
						 boundary_values);
	MatrixTools::apply_boundary_values (boundary_values,
					    system_matrix_flow,
					    solution_flow_new_iteration,
					    system_rhs_flow);
      }
    if (parameters.richards_fixed_at_top==true && transient_drying==false)
      {
	double boundary_condition_top_fixed_pressure=
	  parameters.richards_top_fixed_value;

	boundary_values.clear();
	VectorTools::interpolate_boundary_values(dof_handler,
						 1,
						 ConstantFunction<dim>(boundary_condition_top_fixed_pressure),
						 boundary_values);
	MatrixTools::apply_boundary_values(boundary_values,
					   system_matrix_flow,
					   solution_flow_new_iteration,
					   system_rhs_flow);
      }
  }

  template <int dim>
  void Heat_Pipe<dim>::solve_system_flow ()
  {
    SolverControl solver_control(1000*solution_flow_new_iteration.size(),
				 1e-8*system_rhs_flow.l2_norm ());
    SolverCG<> cg(solver_control);
    PreconditionSSOR<> preconditioner;
    preconditioner.initialize (system_matrix_flow, 1.2);
    cg.solve(system_matrix_flow,solution_flow_new_iteration,
	     system_rhs_flow,preconditioner);
    hanging_node_constraints.distribute(solution_flow_new_iteration);
  }

  template <int dim>
  void Heat_Pipe<dim>::solve_system_transport ()
  {
    SolverControl solver_control_transport(1000*solution_transport.size(),
					   1e-8*system_rhs_transport.l2_norm());
    SolverBicgstab<> bicgstab_transport(solver_control_transport);
    PreconditionJacobi<> preconditioner_transport;
    preconditioner_transport
      .initialize(system_matrix_transport,1.0);
    bicgstab_transport
      .solve(system_matrix_transport,solution_transport,
	     system_rhs_transport,preconditioner_transport);
    hanging_node_constraints.distribute(solution_transport);
  }

  template <int dim>
  void Heat_Pipe<dim>::output_results () const
  {
    DataOut<dim> data_out;

    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution_flow_new_iteration,"pressure(cm_total_water)");
    data_out.add_data_vector(solution_transport,"substrate(mg_substrate_per_cm3_total_water)");
    
    data_out.add_data_vector(new_nodal_biomass_fraction,"biomass(cm3_biomass_per_cm3_void)");
    data_out.add_data_vector(new_nodal_free_moisture_content,"free_water(cm3_free_water_per_cm3_soi)");
    data_out.add_data_vector(new_nodal_total_moisture_content,"total_water(cm3_total_water_per_cm3_soil)");
    data_out.add_data_vector(new_nodal_hydraulic_conductivity,"hydraulic_conductivity(cm_per_s)");
    data_out.add_data_vector(new_nodal_specific_moisture_capacity,"specific_moisture_capacity(cm3_total_water_per_(cm3_soil)(cm_total_water))");
    data_out.add_data_vector(boundary_ids,"boundary_ids");
    data_out.build_patches();

    std::stringstream tsn;
    tsn << timestep_number;
    //	  std::stringstream ts;
    //	  ts << time_step;
    std::stringstream t;
    //t << std::setprecision(1) << std::setw(6) << std::setfill('0') << std::fixed << ((time-milestone_time)/3600);
    if (transient_drying==true)
      t << (int)(10*(time-milestone_time)/1.);
    else if (transient_saturation==true)
      t << (int)(10*(time-milestone_time)/1.);
    else
      t << std::setw(10) << std::setfill('0') << std::fixed << timestep_number;//(int)(time-milestone_time);

    std::stringstream d;
    d << dim;
    std::stringstream c;
    c << triangulation.n_active_cells();

    std::string output_file_format=parameters.output_file_format;

    std::string lm;
    if (parameters.lumped_matrix==true)
      lm="lumped_";

    std::string time_period;
    if (transient_drying==true)
      time_period="drying";
    else if (transient_saturation==true)
      time_period="saturating";
    else if (transient_transport==true)
      time_period="transporting";

    if (transient_transport==true && parameters.homogeneous_decay_rate==true)
      time_period+="_decaying";

    std::string filename;
    if (test_transport==false)
      {
	filename = parameters.output_directory + "/solution_"
	  + parameters.moisture_transport_equation + "_" + lm
	  + d.str() + "d_"
	  //			  + "tsn_" + tsn.str()
	  //			  + "_c_" + c.str() + "_"
	  + time_period + "_t_" + t.str()
	  + output_file_format;
      }
    else
      {
	filename = "solution_"
	  + d.str() + "d_"
	  + "tsn_" + tsn.str()
	  + output_file_format;
      }
    std::ofstream output (filename.c_str());
    if (output_file_format.compare(".gp")==0)
      data_out.write_gnuplot (output);
    else if (output_file_format.compare(".vtu")==0)
      data_out.write_vtu (output);
    else
      {
	std::cout << "Error in output function. Output file format "
		  << "not implemented.\n Current output file format is: "
		  << output_file_format << "\n" << "Options are: "
		  << ".gp, .vtu"
		  << std::endl;
      }
  }

  template <int dim>
  void Heat_Pipe<dim>::initial_condition()
  {
    /* *
     * Initial condition flow: Head [cm_total_water]
     * */
    if (parameters.initial_state.compare("default")==0 ||
	parameters.initial_state.compare("no_drying")==0)
      {
	VectorTools::project(dof_handler,
			     hanging_node_constraints,
			     QGauss<dim>(3),
			     ConstantFunction<dim>(parameters.initial_condition_homogeneous_flow),
			     old_solution_flow);
	solution_flow_new_iteration=
	  old_solution_flow;
	solution_flow_old_iteration=
	  old_solution_flow;
	
	Hydraulic_Properties hydraulic_properties(parameters.hydraulic_properties,
						  parameters.moisture_content_saturation,
						  parameters.moisture_content_residual,
						  parameters.saturated_hydraulic_conductivity,
						  parameters.van_genuchten_alpha,
						  parameters.van_genuchten_n);
	VectorTools::project (dof_handler,
			      hanging_node_constraints,
			      QGauss<dim>(3),
			      ConstantFunction<dim>(parameters//mg_substrate/cm3_water
						    .initial_condition_homogeneous_transport/1000.),
			      old_solution_transport);
	solution_transport=old_solution_transport;
	
	old_nodal_biomass_concentration=0.;// mg_biomass/cm3_soil
	// define the rest of the nodal vectors
	calculate_mass_balance_ratio();
	old_nodal_biomass_fraction=new_nodal_biomass_fraction;
	old_nodal_total_moisture_content=new_nodal_total_moisture_content;
	old_nodal_free_moisture_content=new_nodal_free_moisture_content;
	old_nodal_hydraulic_conductivity=new_nodal_hydraulic_conductivity;
	old_nodal_specific_moisture_capacity=new_nodal_specific_moisture_capacity;
      }
    else if (parameters.initial_state.compare("dry")==0)
      {
	{
	  std::ifstream file("state_dry_pressure.ph");
	  if (!file.is_open())
	    throw 2;

	  old_solution_flow.block_read(file);
	  solution_flow_new_iteration=
	    old_solution_flow;
	  solution_flow_old_iteration=
	    old_solution_flow;
	  
	  file.close();
	  if (file.is_open())
	    throw 3;
	}
	{
	  std::ifstream file("state_dry_substrate.ph");
	  if (!file.is_open())
	    throw 2;

	  old_solution_transport.block_read(file);
	  solution_transport=old_solution_transport;

	  file.close();
	  if (file.is_open())
	    throw 3;

	}
	{
	  std::ifstream file("state_dry_bacteria.ph");
	  if (!file.is_open())
	    throw 2;

	  old_nodal_biomass_concentration.block_read(file);
	  new_nodal_biomass_concentration=old_nodal_biomass_concentration;
	  
	  file.close();
	  if (file.is_open())
	    throw 3;
	}
      }
    else if (parameters.initial_state.compare("saturated")==0)
      {
	{
	  std::ifstream file("state_saturated_pressure.ph");
	  if (!file.is_open())
	    throw 2;
	  
	  old_solution_flow.block_read(file);
	  solution_flow_new_iteration=
	    old_solution_flow;
	  solution_flow_old_iteration=
	    old_solution_flow;

	  file.close();
	  if (file.is_open())
	    throw 3;
	}
	{
	  std::ifstream file("state_saturated_substrate.ph");
	  if (!file.is_open())
	    throw 2;

	  old_solution_transport.block_read(file);
	  solution_transport=old_solution_transport;

	  file.close();
	  if (file.is_open())
	    throw 3;

	}
	{
	  std::ifstream file("state_saturated_bacteria.ph");
	  if (!file.is_open())
	    throw 2;

	  old_nodal_biomass_concentration.block_read(file);
	  new_nodal_biomass_concentration=old_nodal_biomass_concentration;
	  
	  file.close();
	  if (file.is_open())
	    throw 3;
	}
      }
    else if (parameters.initial_state.compare("final")==0)
      {
	{
	  std::ifstream file("state_final_pressure.ph");
	  if (!file.is_open())
	    throw 2;

	  old_solution_flow.block_read(file);
	  solution_flow_new_iteration=
	    old_solution_flow;
	  solution_flow_old_iteration=
	    old_solution_flow;

	  file.close();
	  if (file.is_open())
	    throw 3;
	}
	{
	  std::ifstream file("state_final_substrate.ph");
	  if (!file.is_open())
	    throw 2;

	  old_solution_transport.block_read(file);
	  solution_transport=old_solution_transport;

	  file.close();
	  if (file.is_open())
	    throw 3;

	}
	{
	  std::ifstream file("state_final_bacteria.ph");
	  if (!file.is_open())
	    throw 2;

	  old_nodal_biomass_concentration.block_read(file);
	  new_nodal_biomass_concentration=old_nodal_biomass_concentration;
	  
	  file.close();
	  if (file.is_open())
	    throw 3;
	}
      }
    else
      {
	std::cout << "Wrong initial state specified in input file."
		  << "\"" << parameters.initial_state << "\" is not a valid "
		  << "parameter.";
	throw 1;
      }
    /* *
     * Velocities
     * */
    //estimate_flow_velocities();
  }
    
  template <int dim>
  void Heat_Pipe<dim>::run()
  {
    read_grid();
    setup_system();
    initial_condition();
    
    for (timestep_number=1;
	 timestep_number<parameters.timestep_number_max;
	 ++timestep_number)
      {
	if (transient_transport==true)
	  refine_grid(true);
	else
	  refine_grid(false);
    	double relative_error_flow=1000;
    	double relative_error_transport=0;
	double old_norm_flow=0.;
	double new_norm_flow=0.;
	double old_norm_transport=0.;
	double new_norm_transport=0.;
    	unsigned int iteration=0;
    	unsigned int step=0;
    	bool remain_in_loop=true;
	do
    	  {
    	    if (transient_transport==true && iteration==40)
    	      {
    	    	time_step=time_step/2.;
    	    	relative_error_flow=1000;
    	    	relative_error_transport=0;
    	    	iteration=0;
		// if (time_step<1)
		//   {
		//     time_step=1;
		//     refine_grid(true);
		//   }
    	      }
    	    /* *
    	     * ASSEMBLE systems
    	     * */
	    calculate_mass_balance_ratio();
    	    if (solve_flow && test_transport==false)
    	      assemble_system_flow();
    	    if ((transient_transport==true || test_transport==true) && coupled_transport==true)
    	      assemble_system_transport();
    	    /* *
    	     * SOLVE systems
    	     * */
    	    if(solve_flow && test_transport==false)
    	      {
    		solve_system_flow();
		old_norm_flow=
		  solution_flow_old_iteration.norm_sqr();
		new_norm_flow=
		  solution_flow_new_iteration.norm_sqr();
    		relative_error_flow=//(%)
		  fabs(1.-old_norm_flow/new_norm_flow);
		solution_flow_old_iteration=
    		  solution_flow_new_iteration;
    	      }
    	    if ((transient_transport==true || test_transport==true) && coupled_transport==true)
    	      {
		solve_system_transport();
		new_norm_transport=
		  solution_transport.norm_sqr();
    		relative_error_transport=
		  100.*fabs(1.-fabs(old_norm_transport/new_norm_transport));
		// 100.*fabs((1.-fabs((nutrient_flow_at_bottom+nutrient_flow_at_top)*time_step
		// 		     /fabs(nutrients_in_domain_current-nutrients_in_domain_previous))));
		old_norm_transport=
		  new_norm_transport;
	      }
    	    /* *
    	     * Evaluate the condition to remain in the loop, also
    	     * if the change in total moisture is negative (drying
    	     * soil) AND is less than 1E-6, then override the mass
    	     * balance error and activate the transport equation
    	     * */
    	    if (test_transport==false)
    	      {
    		if (relative_error_flow<1.E-8 &&
    		    relative_error_transport<=1E-3 && //[%]
    		    iteration!=0)
    		  remain_in_loop=false;
    	      }
    	    else
    	      {
    		if (relative_error_transport<1.5E-7)
    		  remain_in_loop=false;
    	      }
    	    /* *
    	     * Output some information about the current iteration.
    	     * */
	    if (false/*transient_transport==true*/)
	      {
		std::cout.setf(std::ios::scientific,std::ios::floatfield);
		std::cout << "\tit: "      << std::fixed << iteration
			  << "\tcell #s: " << triangulation.n_active_cells() << "\n"
			  << std::setprecision(5)
			  << "\ttime step: " << time_step << " s\n"
			  << "\tflow of nutrients at bottom: " << nutrient_flow_at_bottom << " mg/s\n"
			  << "\tflow of nutrients at top: "    << nutrient_flow_at_top << " mg/s\n"
			  << "\tnutrients in domain: "
			  << fabs(nutrients_in_domain_current-nutrients_in_domain_previous) << " mg\n"
			  << "\tcumulative flow of nutrients at bottom: " << cumulative_flow_at_bottom << " mg\n"
			  << "\tcumulative flow of nutrients at top: " << cumulative_flow_at_top << " mg\n"
			  << "\tcumulative nutrients in domain: " << nutrients_in_domain_previous << " mg\n"
			  << "\tmass error: "
			  << relative_error_transport << "%\n\n" ;
	      }
	    /* *
    	     * update the iteration
    	     * */
    	    iteration++;
    	    step++;
    	  }
    	while (remain_in_loop);

    	time=time+time_step;
	cumulative_flow_at_top+=//mg
	  nutrient_flow_at_top*time_step;
	cumulative_flow_at_bottom+=//mg
	  nutrient_flow_at_bottom*time_step;
	nutrients_in_domain_previous=
	  nutrients_in_domain_current;
	/* *
    	 * Choose in which time period are we and note the time
    	 * */
    	if (test_transport==false)
    	  {
	    double relative_error_drying=0.;
	    if (transient_drying==true)
	      {
		double relative_tolerance_drying=3.1E-4;
		double pressure_at_top=0.;
		if (dim==1)
		  pressure_at_top=VectorTools::point_value(dof_handler,
							   solution_flow_new_iteration,
							   Point<dim>(0.));
		else if (dim==2)
		  pressure_at_top=VectorTools::point_value(dof_handler,
							   solution_flow_new_iteration,
							   Point<dim>(0.,-10.0));
		relative_error_drying=
		  pressure_at_top/
		  (parameters.richards_bottom_fixed_value-parameters.domain_size);
	      
		/* *
		 * Begin SATURATION -- Dry conditions have being reached
		 * */
		if (fabs(1.-relative_error_drying)<relative_tolerance_drying)
		  {
		    figure_count=0;
		    transient_drying=false;
		    transient_saturation=true;
		    redefine_time_step=true;
		    
		    time_for_dry_conditions=time;
		    milestone_time=time;
		    std::cout << "\tDry conditions reached at: "
			      << time_for_dry_conditions/3600 << " h\n"
			      << "\ttimestep_number: " << timestep_number << "\n"
			      << "\ttime_step: " << time_step << " s\n"
			      << "\tnumerical pressure at top: " << pressure_at_top << " m\n"
			      << "\texpected pressure at top: "
			      << parameters.richards_bottom_fixed_value-parameters.domain_size << " m\n";
		    
		    if (parameters.richards_fixed_at_top==true)
		      std::cout << "\tFixing top pressure at: "
				<< parameters.richards_top_fixed_value << " cm\n";
		    else
		      std::cout << "\tActivating moisture flow: "
				<< parameters.richards_top_flow_value << " cm/s\n";
		  }
	      }
    	    /* *
    	     * Begin TRANSPORT -- Saturated conditions have being reached
    	     *
	     * Initial condition biomass: Concentration [mg_biomass/cm3_total_water]
	     *
	     * The initial condition for biomass defined in the input file is given in
	     * mg_biomass/L_total_water. It needs to be transformed to mg_biomass/cm3_total_water.
	     * This is done in this way:
	     *
	     * B[mg_biomass/L_total_water]=
	     * B[mg_biomass/L_total_water]*
	     * water_volume_ratio[L_total_water/1000 cm3_total_water]
	     * =[mg_biomass/L_total_water]*[L_total_water/1000 cm3_total_water]
	     * =(1/1000)*[mg_biomass/cm3_total_water]
	     * */	    
	    double relative_error_saturation=0.;
	    double absolute_error_saturation=0.;
	    if (transient_saturation==true && coupled_transport==true)
	      {
		double relative_tolerance_saturation=2E-2;
		double absolute_tolerance_saturation=3E-6;
		if (parameters.richards_fixed_at_top==false)
		  {
		    // relative_error_saturation=
		    //   fabs(moisture_flow_at_outlet_current/time_step)/
		    //   parameters.richards_top_flow_value;
		  }
		else
		  {
		    relative_error_saturation=
		      fabs(1.-fabs(flow_at_top/flow_at_bottom));
		    absolute_error_saturation=
		      fabs(flow_at_top+flow_at_bottom);
		  }
		if (relative_error_saturation<relative_tolerance_saturation||
		    absolute_error_saturation<absolute_tolerance_saturation)
		  {
		    std::cout << "\t\t: " << transient_saturation
			      << "\t" << relative_error_saturation
			      << "\t" << absolute_error_saturation << "\n";
		
		    transient_saturation=false;
		    transient_transport=true;
		    redefine_time_step=true;

		    for (unsigned int i=0; i<dof_handler.n_dofs(); i++)
		      {
			new_nodal_biomass_concentration[i]=//mg_biomass/cm3_total_water
			  (1./1000.)*parameters.initial_condition_homogeneous_bacteria;
			new_nodal_biomass_fraction[i]=
			  new_nodal_biomass_concentration[i]/
			  parameters.biomass_dry_density;
		      }
		
		    figure_count=0;
		    time_for_saturated_conditions=time-milestone_time;
		    milestone_time=time;

		    std::cout << "\tSaturated conditions reached at: "
			      << time_for_saturated_conditions/3600 << " h\n"
			      << "\ttimestep_number: " << timestep_number << "\n"
			      << "\ttime_step: "       << time_step       << " s\n";
		    std::cout << "\tActivating nutrient flow: "
			      << std::scientific << parameters.transport_top_fixed_value
			      << " mg_substrate/m3_soil\n";
		    if (parameters.homogeneous_decay_rate==true)
		      std::cout << "Activating decay rate: "
				<< std::scientific << parameters.first_order_decay_factor << " 1/s\n";
		  }
	      }
    	    /* *
    	     * Output info
    	     * */
    	    if ((test_transport==false) && ((timestep_number%1==0 && transient_drying==true) ||
    					    (timestep_number%1==0 && transient_saturation==true) ||
    					    (timestep_number%1==0 && transient_transport==true) ||
    					    (timestep_number==parameters.timestep_number_max-1)))
    	      {
		double effective_hydraulic_conductivity=0.;
		{
    		  std::vector<double> average_hydraulic_conductivity_vector_row;
    		  for (unsigned int i=0; i<new_nodal_hydraulic_conductivity.size(); i++)
    		    {
    		      effective_hydraulic_conductivity+=
    			1./
    			(new_nodal_hydraulic_conductivity[i]*new_nodal_hydraulic_conductivity.size());
    		    }
    		  average_hydraulic_conductivity_vector_row.push_back(timestep_number);
    		  average_hydraulic_conductivity_vector_row.push_back((time-milestone_time)/3600);
		  average_hydraulic_conductivity_vector_row.push_back(1./effective_hydraulic_conductivity);
    		  average_hydraulic_conductivity_vector.push_back(average_hydraulic_conductivity_vector_row);
    		}
		if (parameters.output_data_in_terminal==true)
    		  {
    		    std::cout.setf(std::ios::fixed,std::ios::floatfield);
    		    std::setprecision(10);
    		    std::cout << std::fixed
    			      << "tsn: "    << std::setw(6)  << timestep_number
    			      << "  time: " << std::setw(9) << std::setprecision(5)
			      << (time-milestone_time)/3600 << " h";
		    
    		    if (transient_drying==true)
    		      std::cout << "\tdrying";
    		    else if (transient_saturation)
    		      std::cout << "\tsaturation";
    		    else
    		      std::cout << "\ttransport";
		    
    		    if (transient_drying==true)
    		      std::cout << "\tRError: " << std::scientific << std::setprecision(2)
				<< relative_error_drying;
    		    else if (transient_saturation==true)
    		      std::cout << "\tRError: " << std::scientific << std::setprecision(2)
				<< relative_error_saturation
				<< "\tAError: " << std::scientific << std::setprecision(2)
				<< absolute_error_saturation;

    		    std::cout <<  std::fixed
    			      << "  ts: "    << std::fixed << std::setprecision(2) << std::setw(5)
			      << time_step
    			      << "  k_eff: " << std::scientific << std::setprecision(2)
			      << 1./effective_hydraulic_conductivity
			      << "\tcell #s: " << std::fixed << std::setprecision(2)
			      << triangulation.n_active_cells() << "\n"
			      << "\tflow of water at bottom    : " << std::setw(7) << std::fixed
			      << std::setprecision(4)
			      << flow_at_bottom << " cm3/s"
			      << "\tflow of water at top    : " << std::setw(7) << std::fixed
			      << std::setprecision(4)
			      << flow_at_top << " cm3/s\n"
			      << "\tflow of nutrients at bottom: " << std::setw(7) << std::fixed
			      << std::setprecision(4)
			      << nutrient_flow_at_bottom << "  mg/s"
			      << "\tflow of nutrients at top: "    << std::setw(7) << std::fixed
			      << std::setprecision(4)
			      << nutrient_flow_at_top << "  mg/s" << "\n"
			      << "\tcumulative flow of nutrients at bottom: " << std::fixed << std::setprecision(3)
			      << cumulative_flow_at_bottom << " mg\n"
			      << "\tcumulative flow of nutrients at top: " << std::fixed << std::setprecision(3)
			      << cumulative_flow_at_top << " mg\n"
			      << "\tcumulative nutrients in domain: " << std::fixed << std::setprecision(3)
			      << nutrients_in_domain_previous << " mg\n"
			      << std::endl;
    		  }
    	      }
	  }
    	else
    	  {
    	    std::cout << "Time step " << timestep_number << "\tts: " << time_step << "\n";
    	  }
    	/* *
    	 * OUTPUT solution files
    	 * */
    	double output_frequency_drying=1;
    	double output_frequency_saturation=1;
    	double output_frequency_transport=parameters.output_frequency_transport;
	if ((transient_drying==true && time-milestone_time>=figure_count*output_frequency_drying) ||
	    (transient_saturation==true && time-milestone_time>=figure_count*output_frequency_saturation) ||
	    (transient_transport==true && output_frequency_transport!=0 &&
	     time-milestone_time>=figure_count*output_frequency_transport) ||
	    (timestep_number==parameters.timestep_number_max-1))
    	  {
    	    output_results();
    	    figure_count++;
    	  }
    	/* *
    	 * Update timestep
    	 * */
    	if (test_transport==false)
    	  {
    	    if (redefine_time_step==true)
    	      {
    		time_step=1;
    		redefine_time_step=false;
    	      }
    	    else if (step<15)
    	      {
    		if (transient_drying==true || transient_transport==true)
    		  time_step=time_step*2;
    		else
    		  time_step=time_step*2;
    	      }
	    
    	    if (time_step<1)
    	      time_step=1;

    	    if (time_step>1 && transient_drying==true)
    	      time_step=1;
    	    else if (time_step>1 && transient_saturation==true)
    	      time_step=1;
    	    else if (time_step>60 && transient_transport==true)
    	      time_step=60;
    	  }
    	/* *
    	 * Update solutions
    	 * */
    	old_solution_flow=
    	  solution_flow_new_iteration;
    	old_solution_transport=
    	  solution_transport;
    	old_nodal_total_moisture_content=
    	  new_nodal_total_moisture_content;
	old_nodal_free_moisture_content=
    	  new_nodal_free_moisture_content;
	old_nodal_biomass_concentration=//mg_biomass/m3
	  new_nodal_biomass_concentration;
	old_nodal_hydraulic_conductivity=
	  new_nodal_hydraulic_conductivity;
	old_nodal_specific_moisture_capacity=
	  new_nodal_specific_moisture_capacity;
	old_nodal_biomass_fraction=
	  new_nodal_biomass_fraction;
      }
    std::stringstream filename;
    filename << "average_hydraulic_conductivity_sf_"
	     << parameters.relative_permeability_model << "_"
	     << parameters.sand_fraction << "_"
	     << parameters.yield_coefficient << "_"
	     << parameters.maximum_substrate_use_rate << "_"
	     << parameters.half_velocity_constant << ".txt";

    std::ofstream output_file(filename.str());
    DataTools data_tools;
    
    data_tools.print_data(output_file,
			  average_hydraulic_conductivity_vector);
    output_results();
    std::cout << "\t Job Done!!"
	      << std::endl;
  }
}

int main (int argc, char *argv[])
{
  try
    {
      using namespace TRL;
      using namespace dealii;
      {

	clock_t t1,t2;

	t1=clock();
	deallog.depth_console (0);
	Heat_Pipe<2> laplace_problem(argc,argv);
	laplace_problem.run();
	t2=clock();

	float time_diff
	  =((float)t2-(float)t1);

	std::cout << time_diff << std::endl;
	std::cout << time_diff/CLOCKS_PER_SEC << " seconds"<< std::endl;
      }
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
