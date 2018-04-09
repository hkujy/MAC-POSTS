#ifndef MULTICLASS_H
#define MULTICLASS_H

#include "dlink.h"
#include "dnode.h"
#include "vehicle.h"
#include "dta.h"

class MNM_Destination_Multiclass;


/******************************************************************************************************************
*******************************************************************************************************************
												Link Models
*******************************************************************************************************************
******************************************************************************************************************/

class MNM_Dlink_Multiclass : public MNM_Dlink
{
public:
	MNM_Dlink_Multiclass(TInt ID,
						TInt number_of_lane,
						TFlt length);
	~MNM_Dlink_Multiclass();

	// use this one instead of the one in Dlink class
	int install_cumulative_curve_multiclass();

	// Two seperate N-curves for private cars and trucks
	MNM_Cumulative_Curve *m_N_in_car;
  	MNM_Cumulative_Curve *m_N_out_car;
  	MNM_Cumulative_Curve *m_N_in_truck;
  	MNM_Cumulative_Curve *m_N_out_truck;
};


/**************************************************************************
							Multiclass CTM
			(currently only for car & truck two classes)
	(see: Z. (Sean) Qian et al./Trans. Res. Part B 99 (2017) 183-204)
**************************************************************************/
class MNM_Dlink_Ctm_Multiclass : public MNM_Dlink_Multiclass
{
public:
	MNM_Dlink_Ctm_Multiclass(TInt ID,
							 TInt number_of_lane,
							 TFlt length,
							 TFlt lane_hold_cap_car,
							 TFlt lane_hold_cap_truck,
							 TFlt lane_flow_cap_car,
							 TFlt lane_flow_cap_truck,
							 TFlt ffs_car,
							 TFlt ffs_truck,
							 TFlt unit_time,
							 TFlt veh_convert_factor,
							 TFlt flow_scalar);
	~MNM_Dlink_Ctm_Multiclass();
	int virtual evolve(TInt timestamp) override;
	TFlt virtual get_link_supply() override;
	int virtual clear_incoming_array() override;
	void virtual print_info() override;
	TFlt virtual get_link_flow() override;
	TFlt virtual get_link_tt() override;

	class Ctm_Cell_Multiclass;
	int init_cell_array(TFlt unit_time, TFlt std_cell_length, TFlt last_cell_length);
	int update_out_veh();
	int move_last_cell();

	TInt m_num_cells;
	TFlt m_ffs_car;
	TFlt m_ffs_truck;
	TFlt m_lane_hold_cap_car;
	TFlt m_lane_hold_cap_truck;
	TFlt m_lane_critical_density_car;
	TFlt m_lane_critical_density_truck;
	TFlt m_lane_rho_1_N;
	TFlt m_lane_flow_cap_car;
	TFlt m_lane_flow_cap_truck;
	TFlt m_veh_convert_factor;
	TFlt m_flow_scalar;
	TFlt m_wave_speed_car;
	TFlt m_wave_speed_truck;
	std::vector<Ctm_Cell_Multiclass*> m_cell_array;
};

class MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass
{
public:
	Ctm_Cell_Multiclass(TFlt cell_length,
						TFlt unit_time,
						TFlt hold_cap_car,
						TFlt hold_cap_truck, 
						TFlt critical_density_car,
						TFlt critical_density_truck,
						TFlt m_rho_1_N,
						TFlt flow_cap_car,
						TFlt flow_cap_truck,
						TFlt ffs_car,
						TFlt ffs_truck,
						TFlt wave_speed_car,
						TFlt wave_speed_truck,
						TFlt flow_scalar);
	~Ctm_Cell_Multiclass();
	TFlt get_perceived_demand(TInt veh_type);
	TFlt get_perceived_supply(TInt veh_type);
	int update_perceived_density();

	TFlt m_cell_length;
	TFlt m_unit_time;
	TFlt m_flow_scalar;
	
	TFlt m_hold_cap_car;
	TFlt m_hold_cap_truck;
	TFlt m_critical_density_car;
	TFlt m_critical_density_truck;
	TFlt m_rho_1_N;
	TFlt m_flow_cap_car;
	TFlt m_flow_cap_truck;
	TFlt m_ffs_car;
	TFlt m_ffs_truck;
	TFlt m_wave_speed_car;
	TFlt m_wave_speed_truck;

	TInt m_volume_car;
	TInt m_volume_truck;
	TFlt m_space_fraction_car;
	TFlt m_space_fraction_truck;
	TFlt m_perceived_density_car;
	TFlt m_perceived_density_truck;
	TInt m_out_veh_car;
	TInt m_out_veh_truck;
	std::deque<MNM_Veh*> m_veh_queue_car;
	std::deque<MNM_Veh*> m_veh_queue_truck;
};

/**************************************************************************
							Multiclass Point-Queue Model
**************************************************************************/
class MNM_Dlink_Pq_Multiclass : public MNM_Dlink_Multiclass
{
public:
	MNM_Dlink_Pq_Multiclass(TInt ID,
							TInt number_of_lane,
							TFlt length,
							TFlt lane_hold_cap_car,
							TFlt lane_hold_cap_truck,
							TFlt lane_flow_cap_car,
							TFlt lane_flow_cap_truck,
							TFlt ffs_car,
							TFlt ffs_truck,
							TFlt unit_time,
							TFlt veh_convert_factor,
							TFlt flow_scalar);
	~MNM_Dlink_Pq_Multiclass();
	int virtual evolve(TInt timestamp) override;
	TFlt virtual get_link_supply() override;
	int virtual clear_incoming_array() override;
	void virtual print_info() override;
	TFlt virtual get_link_flow() override;
	TFlt virtual get_link_tt() override;

	std::unordered_map<MNM_Veh*, TInt> m_veh_pool;
	TInt m_volume_car; //vehicle number, without the flow scalar
	TInt m_volume_truck; //vehicle number, without the flow scalar
	TFlt m_lane_hold_cap;
	TFlt m_lane_flow_cap;
	TFlt m_flow_scalar;
	TFlt m_hold_cap;
	TInt m_max_stamp;
	TFlt m_unit_time;
	TFlt m_veh_convert_factor;
};




/******************************************************************************************************************
*******************************************************************************************************************
												Node Models
*******************************************************************************************************************
******************************************************************************************************************/

/**************************************************************************
                              Origin node
**************************************************************************/
class MNM_DMOND_Multiclass : public MNM_DMOND
{
public:
	MNM_DMOND_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor);
	~MNM_DMOND_Multiclass();
	int virtual evolve(TInt timestamp) override;
	TFlt m_veh_convert_factor;
};

/**************************************************************************
                              Destination node
**************************************************************************/
class MNM_DMDND_Multiclass : public MNM_DMDND
{
public:
	MNM_DMDND_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor);
	~MNM_DMDND_Multiclass();
	int virtual evolve(TInt timestamp) override;
	TFlt m_veh_convert_factor;
};

/**************************************************************************
                              In-Out node
**************************************************************************/
class MNM_Dnode_Inout_Multiclass : public MNM_Dnode
{
public:
	MNM_Dnode_Inout_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor);
	~MNM_Dnode_Inout_Multiclass();
	int virtual evolve(TInt timestamp) override;
	int virtual prepare_loading() override;
	int virtual add_out_link(MNM_Dlink* out_link) override;
	int virtual add_in_link(MNM_Dlink* in_link) override;
protected:
	int prepare_supplyANDdemand();
	int virtual compute_flow(){return 0;};
	// int flow_to_vehicle();
	int move_vehicle();
	int record_cumulative_curve(TInt timestamp);
	TFlt *m_demand; //2d
	TFlt *m_supply; //1d
	TFlt *m_veh_flow; //2d
	TFlt *m_veh_moved_car; //2d
	TFlt *m_veh_moved_truck; //2d
	TFlt m_veh_convert_factor;
};

/*                           FWJ node
**************************************************************************/
class MNM_Dnode_FWJ_Multiclass : public MNM_Dnode_Inout_Multiclass
{
public:
	MNM_Dnode_FWJ_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor);
	~MNM_Dnode_FWJ_Multiclass();
	int virtual compute_flow() override;
};

/*                  General Road Junction node
**************************************************************************/
class MNM_Dnode_GRJ_Multiclass : public MNM_Dnode_Inout_Multiclass
{
public:
	MNM_Dnode_GRJ_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor);
	~MNM_Dnode_GRJ_Multiclass();
	int virtual compute_flow() override;
	int virtual prepare_loading() override;
private:
	std::vector<std::vector<MNM_Dlink*>> m_pow;
	TFlt get_theta();
	int prepare_outflux();
	TFlt *m_d_a; //1d array
	TFlt *m_C_a; //1d array
	template<typename T> std::vector<std::vector<T> > powerSet(const std::vector<T>& set);
	std::vector<int> getOnLocations(int a);
};




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass OD
*******************************************************************************************************************
******************************************************************************************************************/
class MNM_Origin_Multiclass : public MNM_Origin
{
public:
	MNM_Origin_Multiclass(TInt ID, TInt max_interval, TFlt flow_scalar, TInt frequency);
	~MNM_Origin_Multiclass();
	int virtual release(MNM_Veh_Factory* veh_factory, TInt current_interval) override;
	int virtual release_one_interval(TInt current_interval, 
									MNM_Veh_Factory* veh_factory, 
									TInt assign_interval, 
									TFlt adaptive_ratio) override;
	// use this one instead of add_dest_demand in the base class
	int add_dest_demand_multiclass(MNM_Destination_Multiclass *dest, 
								TFlt* demand_car, 
								TFlt* demand_truck);
	// two new unordered_map for both classes
	std::unordered_map<MNM_Destination_Multiclass*, TFlt*> m_demand_car;
	std::unordered_map<MNM_Destination_Multiclass*, TFlt*> m_demand_truck;
};

class MNM_Destination_Multiclass : public MNM_Destination
{
public:
	MNM_Destination_Multiclass(TInt ID);
	~MNM_Destination_Multiclass();
};





/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass Vehicle
*******************************************************************************************************************
******************************************************************************************************************/
class MNM_Veh_Multiclass : public MNM_Veh
{
public:
	MNM_Veh_Multiclass(TInt ID, TInt vehicle_class, TInt start_time);
	~MNM_Veh_Multiclass();

	TInt m_class;
};




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass Factory
*******************************************************************************************************************
******************************************************************************************************************/
class MNM_Veh_Factory_Multiclass : public MNM_Veh_Factory
{
public:
	MNM_Veh_Factory_Multiclass();
	~MNM_Veh_Factory_Multiclass();

	// use this one instead of make_veh in the base class
	MNM_Veh_Multiclass* make_veh_multiclass(TInt timestamp, Vehicle_type veh_type, TInt vehicle_cls);
};

class MNM_Node_Factory_Multiclass : public MNM_Node_Factory
{
public:
	MNM_Node_Factory_Multiclass();
	~MNM_Node_Factory_Multiclass();

	// use this one instead of make_node in the base class
	MNM_Dnode *make_node_multiclass(TInt ID, DNode_type_multiclass node_type, TFlt flow_scalar, TFlt veh_convert_factor);
};

class MNM_Link_Factory_Multiclass : public MNM_Link_Factory
{
public:
	MNM_Link_Factory_Multiclass();
	~MNM_Link_Factory_Multiclass();

	// use this one instead of make_link in the base class
	MNM_Dlink *make_link_multiclass(TInt ID,
									DLink_type_multiclass link_type,
									TInt number_of_lane,
									TFlt length,
									TFlt lane_hold_cap_car,
									TFlt lane_hold_cap_truck,
									TFlt lane_flow_cap_car,
									TFlt lane_flow_cap_truck,
									TFlt ffs_car,
									TFlt ffs_truck,
									TFlt unit_time,
									TFlt veh_convert_factor,
									TFlt flow_scalar);
};

class MNM_OD_Factory_Multiclass : public MNM_OD_Factory
{
public:
	MNM_OD_Factory_Multiclass();
	~MNM_OD_Factory_Multiclass();
	virtual MNM_Destination_Multiclass* make_destination(TInt ID) override;
	virtual MNM_Origin_Multiclass* make_origin(TInt ID, 
											TInt max_interval, 
											TFlt flow_scalar, 
											TInt frequency) override;
};




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass IO Functions
*******************************************************************************************************************
******************************************************************************************************************/
class MNM_IO_Multiclass : public MNM_IO
{
public:
	static int build_node_factory_multiclass(std::string file_folder, 
											MNM_ConfReader *conf_reader, 
											MNM_Node_Factory *node_factory);
 	static int build_link_factory_multiclass(std::string file_folder, 
 											MNM_ConfReader *conf_reader, 
 											MNM_Link_Factory *link_factory, 
 											std::string file_name = "MNM_input_link");
 	static int build_od_factory_multiclass(std::string file_folder, 
 											MNM_ConfReader *conf_reader, 
 											MNM_OD_Factory *od_factory, 
 											MNM_Node_Factory *node_factory) {
 		return build_od_factory(file_folder, conf_reader, od_factory, node_factory);
 	};
 	static int build_demand_multiclass(std::string file_folder, 
 									MNM_ConfReader *conf_reader, 
 									MNM_OD_Factory *od_factory);
};




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass DTA
*******************************************************************************************************************
******************************************************************************************************************/
class MNM_Dta_Multiclass : public MNM_Dta
{
public:
	MNM_Dta_Multiclass(std::string file_folder);
	~MNM_Dta_Multiclass();
	int virtual initialize() override;
	int virtual build_from_files() override;
}; 




#endif              