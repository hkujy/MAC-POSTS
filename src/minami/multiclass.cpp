#include "limits.h"
#include "multiclass.h"

#include <algorithm>
#include <fstream>
#include <iostream>

/******************************************************************************************************************
*******************************************************************************************************************
												Link Models
*******************************************************************************************************************
******************************************************************************************************************/

MNM_Dlink_Multiclass::MNM_Dlink_Multiclass(TInt ID,
										TInt number_of_lane,
										TFlt length)
	: MNM_Dlink::MNM_Dlink(ID, number_of_lane, length, 0.0) // Note: m_ffs is not used in child class, so let it be 0.0
{
	m_N_in_car = NULL;
  	m_N_out_car = NULL;
  	m_N_in_truck = NULL;
  	m_N_out_truck = NULL;

  	install_cumulative_curve_multiclass();
}

MNM_Dlink_Multiclass::~MNM_Dlink_Multiclass()
{
	if (m_N_out_car != NULL) delete m_N_out_car;
  	if (m_N_in_car != NULL) delete m_N_in_car;
  	if (m_N_out_truck != NULL) delete m_N_out_truck;
  	if (m_N_in_truck != NULL) delete m_N_in_truck;
}

int MNM_Dlink_Multiclass::install_cumulative_curve_multiclass()
{
	m_N_in_car = new MNM_Cumulative_Curve();
  	m_N_out_car = new MNM_Cumulative_Curve();
  	m_N_in_truck = new MNM_Cumulative_Curve();
  	m_N_out_truck = new MNM_Cumulative_Curve();
  	m_N_in_car -> add_record(std::pair<TFlt, TFlt>(TFlt(0), TFlt(0)));
  	m_N_out_car -> add_record(std::pair<TFlt, TFlt>(TFlt(0), TFlt(0)));
  	m_N_in_truck -> add_record(std::pair<TFlt, TFlt>(TFlt(0), TFlt(0)));
  	m_N_out_truck -> add_record(std::pair<TFlt, TFlt>(TFlt(0), TFlt(0)));
  	return 0;
}

/*************************************************************************					
						Multiclass CTM Functions
			(currently only for car & truck two classes)
	(see: Z. (Sean) Qian et al./Trans. Res. Part B 99 (2017) 183-204)			
**************************************************************************/
MNM_Dlink_Ctm_Multiclass::MNM_Dlink_Ctm_Multiclass(TInt ID,
												   TInt number_of_lane,
												   TFlt length, // (m)
												   TFlt lane_hold_cap_car, // Jam density (veh/m)
												   TFlt lane_hold_cap_truck,
												   TFlt lane_flow_cap_car, // Max flux (veh/s)
												   TFlt lane_flow_cap_truck,
												   TFlt ffs_car, // Free-flow speed (m/s)
												   TFlt ffs_truck, 
												   TFlt unit_time, // (s)
												   TFlt veh_convert_factor, // 1 * truck = c * private cars 
												   							// when compute node demand
												   TFlt flow_scalar) // flow_scalar can be 2.0, 5.0, 10.0, etc.
	: MNM_Dlink_Multiclass::MNM_Dlink_Multiclass(ID, number_of_lane, length)
{
	// Jam density for private cars and trucks cannot be negative
	if ((lane_hold_cap_car < 0) || (lane_hold_cap_truck < 0)){
		printf("lane_hold_cap can't be negative, current link ID is %d\n", m_link_ID());
		exit(-1);
	}
	// Jam density for private cars cannot be too large
	if (lane_hold_cap_car > TFlt(300) / TFlt(1600)){
		// "lane_hold_cap is too large, set to 300 veh/mile
		lane_hold_cap_car = TFlt(300) / TFlt(1600);
	}
	// Jam density for trucks cannot be too large !!!NEED CHECK FOR THRESHOLD!!!
	if (lane_hold_cap_truck > TFlt(300) / TFlt(1600)){
		// "lane_hold_cap is too large, set to 300 veh/mile
		lane_hold_cap_truck = TFlt(300) / TFlt(1600);
	}

	// Maximum flux for private cars and trucks cannot be negative
	if ((lane_flow_cap_car < 0) || (lane_flow_cap_truck < 0)){
		printf("lane_flow_cap can't be less than zero, current link ID is %d\n", m_link_ID());
		exit(-1);
	}
	// Maximum flux for private cars cannot be too large
	if (lane_flow_cap_car > TFlt(3500) / TFlt(3600)){
		// lane_flow_cap is too large, set to 3500 veh/hour
		lane_flow_cap_car = TFlt(3500) / TFlt(3600);
	}
	// Maximum flux for trucks cannot be too large !!!NEED CHECK FOR THRESHOLD!!!
	if (lane_flow_cap_truck > TFlt(3500) / TFlt(3600)){
		// lane_flow_cap is too large, set to 3500 veh/hour
		lane_flow_cap_truck = TFlt(3500) / TFlt(3600);
	}

	if ((ffs_car < 0) || (ffs_truck < 0)){
		printf("free-flow speed can't be less than zero, current link ID is %d\n", m_link_ID());
		exit(-1);
	}

	if (veh_convert_factor < 1){
		printf("veh_convert_factor can't be less than 1, current link ID is %d\n", m_link_ID());
		exit(-1);
	}

	if (flow_scalar < 1){
		printf("flow_scalar can't be less than 1, current link ID is %d\n", m_link_ID());
		exit(-1);
	}

	if (unit_time <= 0){
		printf("unit_time should be positive, current link ID is %d\n", m_link_ID());
		exit(-1);
	}

	m_lane_flow_cap_car = lane_flow_cap_car;
	m_lane_flow_cap_truck = lane_flow_cap_truck;	
	m_lane_hold_cap_car = lane_hold_cap_car;
	m_lane_hold_cap_truck = lane_hold_cap_truck;
	m_ffs_car = ffs_car;
	m_ffs_truck = ffs_truck;
	m_veh_convert_factor = veh_convert_factor;
	m_flow_scalar = flow_scalar;

	m_cell_array = std::vector<Ctm_Cell_Multiclass*>();

	// Note m_ffs_car > m_ffs_truck, use ffs_car to define the standard cell length
	TFlt _std_cell_length = m_ffs_car * unit_time;
	m_num_cells = TInt(floor(m_length / _std_cell_length));
	if (m_num_cells == 0){
		m_num_cells = 1;
	}
	TFlt _last_cell_length = m_length - TFlt(m_num_cells - 1) * _std_cell_length;

	m_lane_critical_density_car = m_lane_flow_cap_car / m_ffs_car;
	m_lane_critical_density_truck = m_lane_flow_cap_truck / m_ffs_truck;

	if (m_lane_hold_cap_car <= m_lane_critical_density_car){
		printf("Wrong private car parameters, current link ID is %d\n", m_link_ID());
		exit(-1);
	}
	m_wave_speed_car = m_lane_flow_cap_car / (m_lane_hold_cap_car - m_lane_critical_density_car);

	if (m_lane_hold_cap_truck <= m_lane_critical_density_truck){
		printf("Wrong truck parameters, current link ID is %d\n", m_link_ID());
		exit(-1);
	}
	m_wave_speed_truck = m_lane_flow_cap_truck / (m_lane_hold_cap_truck - m_lane_critical_density_truck);

	// see the reference paper for definition
	// m_lane_rho_1_N > m_lane_critical_density_car and m_lane_critical_density_truck
	m_lane_rho_1_N = m_lane_hold_cap_car * (m_wave_speed_car / (m_ffs_truck + m_wave_speed_car));

	init_cell_array(unit_time, _std_cell_length, _last_cell_length);
}

MNM_Dlink_Ctm_Multiclass::~MNM_Dlink_Ctm_Multiclass()
{
	for (Ctm_Cell_Multiclass* _cell : m_cell_array){
		delete _cell;
	}
	m_cell_array.clear();
}

int MNM_Dlink_Ctm_Multiclass::init_cell_array(TFlt unit_time, 
											  TFlt std_cell_length, 
											  TFlt last_cell_length)
{
	// All previous cells
	Ctm_Cell_Multiclass *cell = NULL;
	for (int i = 0; i < m_num_cells - 1; ++i){
		cell = new Ctm_Cell_Multiclass(std_cell_length,
									   unit_time,
									   // Covert lane parameters to cell (link) parameters by multiplying # of lanes
									   TFlt(m_number_of_lane) * m_lane_hold_cap_car,
									   TFlt(m_number_of_lane) * m_lane_hold_cap_truck,
									   TFlt(m_number_of_lane) * m_lane_critical_density_car,
									   TFlt(m_number_of_lane) * m_lane_critical_density_truck,
									   TFlt(m_number_of_lane) * m_lane_rho_1_N,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_car,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_truck,
									   m_ffs_car,
									   m_ffs_truck,
									   m_wave_speed_car,
									   m_wave_speed_truck,
									   m_flow_scalar);
		if (cell == NULL) {
			printf("Fail to initialize some standard cell.\n");
			exit(-1);
		}
		m_cell_array.push_back(cell);
	}

	// The last cell
	// last cell must exist as long as link length > 0, see definition above
	if (m_length > 0.0) {
		cell = new Ctm_Cell_Multiclass(last_cell_length, // Note last cell length is longer but < 2X
									   unit_time,
									   TFlt(m_number_of_lane) * m_lane_hold_cap_car,
									   TFlt(m_number_of_lane) * m_lane_hold_cap_truck,
									   TFlt(m_number_of_lane) * m_lane_critical_density_car,
									   TFlt(m_number_of_lane) * m_lane_critical_density_truck,
									   TFlt(m_number_of_lane) * m_lane_rho_1_N,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_car,
									   TFlt(m_number_of_lane) * m_lane_flow_cap_truck,
									   m_ffs_car,
									   m_ffs_truck,
									   m_wave_speed_car,
									   m_wave_speed_truck,
									   m_flow_scalar);
		if (cell == NULL) {
			printf("Fail to initialize the last cell.\n");
			exit(-1);
		}
		m_cell_array.push_back(cell);
	}

	// compress the cell_array to reduce space
	m_cell_array.shrink_to_fit();

	return 0;
}

void MNM_Dlink_Ctm_Multiclass::print_info()
{
	printf("Total number of cell: \t%d\n Flow scalar: \t%.4f\n", int(m_num_cells), double(m_flow_scalar));

	printf("Car volume for each cell is:\n");
	for (int i = 0; i < m_num_cells - 1; ++i){
		printf("%d, ", int(m_cell_array[i] -> m_volume_car));
	}
	printf("%d\n", int(m_cell_array[m_num_cells - 1] -> m_volume_car));

	printf("Truck volume for each cell is:\n");
	for (int i = 0; i < m_num_cells - 1; ++i){
		printf("%d, ", int(m_cell_array[i] -> m_volume_truck));
	}
	printf("%d\n", int(m_cell_array[m_num_cells - 1] -> m_volume_truck));
}

int MNM_Dlink_Ctm_Multiclass::update_out_veh()
{
	TFlt _temp_out_flux_car, _supply_car, _demand_car;
	TFlt _temp_out_flux_truck, _supply_truck, _demand_truck;

	printf("Link %d to be moved: ", m_link_ID);
	// no update is needed if only one cell
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i){
			// car, veh_type = TInt(0)
			_demand_car = m_cell_array[i] -> get_perceived_demand(TInt(0));
			_supply_car = m_cell_array[i + 1] -> get_perceived_supply(TInt(0));
			_temp_out_flux_car = m_cell_array[i] -> m_space_fraction_car * MNM_Ults::min(_demand_car, _supply_car);
			m_cell_array[i] -> m_out_veh_car = MNM_Ults::round(_temp_out_flux_car * m_flow_scalar);
			// Edge case, when only few cars on the in this cell and there is room in the downstream cell for these cars, move them!!
			if ((m_cell_array[i] -> m_veh_queue_car.size() != 0) && (_demand_car < _supply_car) && (m_cell_array[i] -> m_out_veh_car == 0)){
				m_cell_array[i] -> m_out_veh_car = m_cell_array[i] -> m_veh_queue_car.size();
			}

			// truck, veh_type = TInt(1)
			_demand_truck = m_cell_array[i] -> get_perceived_demand(TInt(1));
			_supply_truck = m_cell_array[i + 1] -> get_perceived_supply(TInt(1));
			_temp_out_flux_truck = m_cell_array[i] -> m_space_fraction_truck * MNM_Ults::min(_demand_truck, _supply_truck);
			m_cell_array[i] -> m_out_veh_truck = MNM_Ults::round(_temp_out_flux_truck * m_flow_scalar);
			// Edge case, when only few trucks on the in this cell and there is room in the downstream cell for these trucks, move them!!
			if ((m_cell_array[i] -> m_veh_queue_truck.size() != 0) && (_demand_truck < _supply_truck) && (m_cell_array[i] -> m_out_veh_truck == 0)){
				m_cell_array[i] -> m_out_veh_truck = m_cell_array[i] -> m_veh_queue_truck.size();
			}

			// printf("(%.2f, %.2f, %.2f, %.2f, %d; %.2f, %.2f, %.2f, %.2f, %d) ", 
			// 						_demand_car, _supply_car, _temp_out_flux_car, m_cell_array[i] -> m_space_fraction_car, m_cell_array[i] -> m_out_veh_car,
			// 						_demand_truck, _supply_truck, _temp_out_flux_truck, m_cell_array[i] -> m_space_fraction_truck, m_cell_array[i] -> m_out_veh_truck);
			printf("(%d, %d) ",  m_cell_array[i] -> m_out_veh_car, m_cell_array[i] -> m_out_veh_truck);
		}
	}
	m_cell_array[m_num_cells - 1] -> m_out_veh_car = m_cell_array[m_num_cells - 1] -> m_veh_queue_car.size();
	m_cell_array[m_num_cells - 1] -> m_out_veh_truck = m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.size();
	printf("(%d, %d)\n", m_cell_array[m_num_cells - 1] -> m_out_veh_car, m_cell_array[m_num_cells - 1] -> m_out_veh_truck);
	return 0;
}

int MNM_Dlink_Ctm_Multiclass::evolve(TInt timestamp)
{
	printf("Link %d volume before: ", m_link_ID);
	/* update volume */
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i)
		{
			printf("(%d, %d) ", m_cell_array[i] -> m_veh_queue_car.size(), m_cell_array[i] -> m_veh_queue_truck.size());
		}
	}
	std::deque<MNM_Veh*>::iterator _veh_it;
	TInt _count_car = 0;
	TInt _count_truck = 0;
	// m_class: 0 - private car, 1 - truck
	for (_veh_it = m_finished_array.begin(); _veh_it != m_finished_array.end(); _veh_it++){
		MNM_Veh_Multiclass *_veh = dynamic_cast<MNM_Veh_Multiclass *>(*_veh_it);
		if (_veh -> m_class == 0) _count_car += 1;
		if (_veh -> m_class == 1) _count_truck += 1;
	}
	printf("(%d, %d; %d, %d)\n", m_cell_array[m_num_cells - 1] -> m_veh_queue_car.size(), 
		m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.size(), _count_car, _count_truck);

	update_out_veh();
	
	TInt _num_veh_tomove_car, _num_veh_tomove_truck;
	/* previous cells */
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i)
		{
			// Car
			_num_veh_tomove_car = m_cell_array[i] -> m_out_veh_car;
			move_veh_queue( &(m_cell_array[i] -> m_veh_queue_car),
							&(m_cell_array[i+1] -> m_veh_queue_car),
							_num_veh_tomove_car);
			// Truck
			_num_veh_tomove_truck = m_cell_array[i] -> m_out_veh_truck;
			move_veh_queue( &(m_cell_array[i] -> m_veh_queue_truck),
							&(m_cell_array[i+1] -> m_veh_queue_truck),
							_num_veh_tomove_truck);
		}
	}

	/* last cell */
	move_last_cell();

	printf("Link %d volume after: ", m_link_ID);
	/* update volume */
	if (m_num_cells > 1){
		for (int i = 0; i < m_num_cells - 1; ++i)
		{
			m_cell_array[i] -> m_volume_car = m_cell_array[i] -> m_veh_queue_car.size();
			m_cell_array[i] -> m_volume_truck = m_cell_array[i] -> m_veh_queue_truck.size();
			// Update perceived density of the i-th cell
			m_cell_array[i] -> update_perceived_density();
			printf("(%d, %d) ", m_cell_array[i] -> m_volume_car, m_cell_array[i] -> m_volume_truck);
		}
	}

	_count_car = 0;
	_count_truck = 0;
	// m_class: 0 - private car, 1 - truck
	for (_veh_it = m_finished_array.begin(); _veh_it != m_finished_array.end(); _veh_it++){
		MNM_Veh_Multiclass *_veh = dynamic_cast<MNM_Veh_Multiclass *>(*_veh_it);
		if (_veh -> m_class == 0) _count_car += 1;
		if (_veh -> m_class == 1) _count_truck += 1;
	}
	m_cell_array[m_num_cells - 1] -> m_volume_car = 
		m_cell_array[m_num_cells - 1] -> m_veh_queue_car.size() + _count_car;
	m_cell_array[m_num_cells - 1] -> m_volume_truck = 
		m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.size() + _count_truck;	
	m_cell_array[m_num_cells - 1] -> update_perceived_density();
	printf("(%d, %d; %d, %d)\n", m_cell_array[m_num_cells - 1] -> m_volume_car, 
		m_cell_array[m_num_cells - 1] -> m_volume_truck, _count_car, _count_truck);
	return 0;
}


int MNM_Dlink_Ctm_Multiclass::move_last_cell() 
{
	TInt _num_veh_tomove_car = m_cell_array[m_num_cells - 1] -> m_out_veh_car;
	TInt _num_veh_tomove_truck = m_cell_array[m_num_cells - 1] -> m_out_veh_truck;
	TFlt _pstar = TFlt(_num_veh_tomove_car)/TFlt(_num_veh_tomove_car + _num_veh_tomove_truck);
	MNM_Veh* _veh;
	TFlt _r;
	while ((_num_veh_tomove_car > 0) || (_num_veh_tomove_truck > 0)){
		_r = MNM_Ults::rand_flt();
		// probability = _pstar to move a car
		if (_r < _pstar){
			// still has car to move
			if (_num_veh_tomove_car > 0){
				_veh = m_cell_array[m_num_cells - 1] -> m_veh_queue_car.front();
				m_cell_array[m_num_cells - 1] -> m_veh_queue_car.pop_front();
				if (_veh -> has_next_link()){
					m_finished_array.push_back(_veh);
				}
				else {
					printf("Dlink_CTM_Multiclass::Some thing wrong!\n");
					exit(-1);
				}
				_num_veh_tomove_car--;
			}
			// no car to move, move a truck
			else {
				_veh = m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.front();
				m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.pop_front();
				if (_veh -> has_next_link()){
					m_finished_array.push_back(_veh);
				}
				else {
					printf("Dlink_CTM_Multiclass::Some thing wrong!\n");
					exit(-1);
				}
				_num_veh_tomove_truck--;
			}
		}
		// probability = 1 - _pstar to move a truck
		else {
			// still has truck to move
			if (_num_veh_tomove_truck > 0){
				_veh = m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.front();
				m_cell_array[m_num_cells - 1] -> m_veh_queue_truck.pop_front();
				if (_veh -> has_next_link()){
					m_finished_array.push_back(_veh);
				}
				else {
					printf("Dlink_CTM_Multiclass::Some thing wrong!\n");
					exit(-1);
				}
				_num_veh_tomove_truck--;
			}
			// no truck to move, move a car
			else {
				_veh = m_cell_array[m_num_cells - 1] -> m_veh_queue_car.front();
				m_cell_array[m_num_cells - 1] -> m_veh_queue_car.pop_front();
				if (_veh -> has_next_link()){
					m_finished_array.push_back(_veh);
				}
				else {
					printf("Dlink_CTM_Multiclass::Some thing wrong!\n");
					exit(-1);
				}
				_num_veh_tomove_car--;
			}
		}
	}
	return 0;
}

TFlt MNM_Dlink_Ctm_Multiclass::get_link_supply()
{
	TFlt _real_volume_both = ( TFlt(m_cell_array[0] -> m_volume_truck) * m_veh_convert_factor + 
							   TFlt(m_cell_array[0] -> m_volume_car) ) / m_flow_scalar;

	// m_cell_length can't be 0 according to implementation above
	TFlt _density = _real_volume_both / (m_cell_array[0] -> m_cell_length);
	double _tmp = std::min(double(m_cell_array[0] -> m_flow_cap_car), m_wave_speed_car * (m_cell_array[0] -> m_hold_cap_car - _density));

	return std::max(0.0, _tmp) * (m_cell_array[0] -> m_unit_time);
}

int MNM_Dlink_Ctm_Multiclass::clear_incoming_array()
{
	if (get_link_supply() * m_flow_scalar < m_incoming_array.size()){
		printf("Wrong incoming array size\n");
		exit(-1);
	}
	
	printf("Link incoming array size: %d\n", m_incoming_array.size());

	MNM_Veh_Multiclass* _veh;
	size_t _cur_size = m_incoming_array.size();
	for (size_t i = 0; i < _cur_size; ++i) {
		_veh = dynamic_cast<MNM_Veh_Multiclass *>(m_incoming_array.front());
		m_incoming_array.pop_front();
		if (_veh -> m_class == TInt(0)) {
			// printf("car\n");
			m_cell_array[0] -> m_veh_queue_car.push_back(_veh);
		}
		else {
			// printf("truck\n");
			m_cell_array[0] -> m_veh_queue_truck.push_back(_veh);
		}
	}
	m_cell_array[0] -> m_volume_car = m_cell_array[0] -> m_veh_queue_car.size();
	m_cell_array[0] -> m_volume_truck = m_cell_array[0] -> m_veh_queue_truck.size();
	m_cell_array[0] -> update_perceived_density();
	
	return 0;
}


TFlt MNM_Dlink_Ctm_Multiclass::get_link_flow()
{
	// For adaptive routing, need modidication for multiclass case

	// TInt _total_volume_car = 0;
	// TInt _total_volume_truck = 0;
	// for (int i = 0; i < m_num_cells; ++i){
	// 	_total_volume_car += m_cell_array[i] -> m_volume_car;
	// 	_total_volume_truck += m_cell_array[i] -> m_volume_truck;
	// }
	// return TFlt(_total_volume_car + _total_volume_truck) / m_flow_scalar;

	return 0;
}

TFlt MNM_Dlink_Ctm_Multiclass::get_link_tt()
{
	// For adaptive routing, need modidication for multiclass case

	// TFlt _cost, _spd;
	// // get the density in veh/mile
	// TFlt _rho = get_link_flow()/m_number_of_lane/m_length;
	// // get the jam density
	// TFlt _rhoj = m_lane_hold_cap;
	// // get the critical density
	// TFlt _rhok = m_lane_flow_cap/m_ffs;

	// if (_rho >= _rhoj){
	// 	_cost = MNM_Ults::max_link_cost();
	// }
	// else {
	// 	if (_rho <= _rhok){
	// 		_spd = m_ffs;
	// 	}
	// 	else {
	// 		_spd = MNM_Ults::max(0.001 * m_ffs, 
	// 				m_lane_flow_cap * (_rhoj - _rho) / (_rhoj - _rhok) / _rho);
	// 	}
	// 	_cost = m_length / _spd;
	// }
	// return _cost;

	return 0;
}

/*							Multiclass CTM Cells
**************************************************************************/
MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::Ctm_Cell_Multiclass(TFlt cell_length,
														TFlt unit_time,
														TFlt hold_cap_car,
														TFlt hold_cap_truck,
														TFlt critical_density_car,
														TFlt critical_density_truck, 
														TFlt rho_1_N,
														TFlt flow_cap_car,
														TFlt flow_cap_truck,
														TFlt ffs_car,
														TFlt ffs_truck,
														TFlt wave_speed_car,
														TFlt wave_speed_truck,
														TFlt flow_scalar)
{
	m_cell_length = cell_length;
	m_unit_time = unit_time;
	m_flow_scalar = flow_scalar;

	m_hold_cap_car = hold_cap_car; // Veh/m
	m_hold_cap_truck = hold_cap_truck; // Veh/m
	m_critical_density_car = critical_density_car; // Veh/m
	m_critical_density_truck = critical_density_truck; // Veh/m
	m_rho_1_N = rho_1_N; // Veh/m
	m_flow_cap_car = flow_cap_car; // Veh/s
	m_flow_cap_truck = flow_cap_truck; // Veh/s
	m_ffs_car = ffs_car;
	m_ffs_truck = ffs_truck;
	m_wave_speed_car = wave_speed_car;
	m_wave_speed_truck = wave_speed_truck;

	m_space_fraction_car = 0;
	m_space_fraction_truck = 0;

	m_volume_car = TInt(0);
	m_volume_truck = TInt(0);
	m_out_veh_car = TInt(0);
	m_out_veh_truck = TInt(0);
	m_veh_queue_car = std::deque<MNM_Veh*>();
	m_veh_queue_truck = std::deque<MNM_Veh*>();
}

MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::~Ctm_Cell_Multiclass()
{
	m_veh_queue_car.clear();
	m_veh_queue_truck.clear();
}

int MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::update_perceived_density()
{
	TFlt _real_volume_car = TFlt(m_volume_car) / m_flow_scalar;
	TFlt _real_volume_truck = TFlt(m_volume_truck) / m_flow_scalar;

	TFlt _density_car = _real_volume_car / m_cell_length;
	TFlt _density_truck = _real_volume_truck / m_cell_length;

	TFlt _space_fraction_car, _space_fraction_truck;
	
	// Free-flow traffic (free-flow for both car and truck classes)
	if (_density_car/m_critical_density_car + _density_truck/m_critical_density_truck <= 1) {
		_space_fraction_car = _density_car/m_critical_density_car;
		_space_fraction_truck = _density_truck/m_critical_density_truck;
		m_perceived_density_car = _density_car + m_critical_density_car * _space_fraction_truck;
		m_perceived_density_truck = _density_truck + m_critical_density_truck * _space_fraction_car;
	}
	// Semi-congested traffic (truck free-flow but car not)
	else if ((_density_truck / m_critical_density_truck < 1) && 
			 (_density_car / (1 - _density_truck/m_critical_density_truck) <= m_rho_1_N)) {
		_space_fraction_truck = _density_truck/m_critical_density_truck;
		_space_fraction_car = 1 - _space_fraction_truck;
		m_perceived_density_car = _density_car / _space_fraction_car;
		m_perceived_density_truck = m_critical_density_truck;
	}
	// Fully congested traffic (both car and truck not free-flow)
	// this case should satisfy: 1. m_perceived_density_car > m_rho_1_N
	// 							 2. m_perceived_density_truck > m_critical_density_truck
	else {
		// _density_truck (m_volume_truck) could still be 0, but _density_car can't
		if (m_volume_truck == 0) {
			m_perceived_density_car = _density_car;
			_space_fraction_car = 1;
			_space_fraction_truck = 0;
			// this case same speed (u) for both private cars and trucks
			TFlt _u = (m_hold_cap_car - _density_car) * m_wave_speed_car / _density_car;
			m_perceived_density_truck = (m_hold_cap_truck * m_wave_speed_truck) / (_u + m_wave_speed_truck);
		}
		else {
			TFlt _tmp_1 = m_hold_cap_car * m_wave_speed_car * _density_truck;
			TFlt _tmp_2 = m_hold_cap_truck * m_wave_speed_truck * _density_car;
			_space_fraction_car = ( _density_car * _density_truck * (m_wave_speed_car - m_wave_speed_truck) 
									 + _tmp_2 ) / ( _tmp_2 + _tmp_1 );
			_space_fraction_truck = ( _density_car * _density_truck * (m_wave_speed_truck - m_wave_speed_car)
									   + _tmp_1 ) / ( _tmp_2 + _tmp_1 );
			m_perceived_density_car = _density_car / _space_fraction_car;
			m_perceived_density_truck = _density_truck / _space_fraction_truck;
		}
	}
	m_space_fraction_car = _space_fraction_car;
	m_space_fraction_truck = _space_fraction_truck;
	return 0;
}

TFlt MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::get_perceived_demand(TInt veh_type)
{	
	// car
	if (veh_type == TInt(0)) {
		return std::min(m_flow_cap_car, TFlt(m_ffs_car * m_perceived_density_car)) * m_unit_time;
	}
	// truck
	else {
		return std::min(m_flow_cap_truck, TFlt(m_ffs_truck * m_perceived_density_truck)) * m_unit_time;
	}
}

TFlt MNM_Dlink_Ctm_Multiclass::Ctm_Cell_Multiclass::get_perceived_supply(TInt veh_type)
{
	TFlt _tmp;
	// car
	if (veh_type == TInt(0)) {
		_tmp = std::min(m_flow_cap_car, TFlt(m_wave_speed_car * (m_hold_cap_car - m_perceived_density_car)));
	}
	// truck
	else {
		_tmp = std::min(m_flow_cap_truck, TFlt(m_wave_speed_truck * (m_hold_cap_truck - m_perceived_density_truck)));
	}
	return std::max(TFlt(0.0), _tmp) * m_unit_time;
}

/**************************************************************************
							Multiclass Point-Queue Model
**************************************************************************/
MNM_Dlink_Pq_Multiclass::MNM_Dlink_Pq_Multiclass(TInt ID,
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
												TFlt flow_scalar)
  : MNM_Dlink_Multiclass::MNM_Dlink_Multiclass(ID, number_of_lane, length)
{
	m_lane_hold_cap = lane_hold_cap_car;
	m_lane_flow_cap = lane_flow_cap_car;
	m_flow_scalar = flow_scalar;
	m_hold_cap = m_lane_hold_cap * TFlt(number_of_lane) * m_length;
	m_max_stamp = MNM_Ults::round(m_length/(ffs_car * unit_time));
	printf("m_max_stamp = %d\n", m_max_stamp);
	m_veh_pool = std::unordered_map<MNM_Veh*, TInt>();
	m_volume_car = TInt(0);
	m_volume_truck = TInt(0);
	m_unit_time = unit_time;
	m_veh_convert_factor = veh_convert_factor;
}

MNM_Dlink_Pq_Multiclass::~MNM_Dlink_Pq_Multiclass()
{
	m_veh_pool.clear();
}

TFlt MNM_Dlink_Pq_Multiclass::get_link_supply()
{
	return m_lane_flow_cap * TFlt(m_number_of_lane) * m_unit_time;
}

int MNM_Dlink_Pq_Multiclass::clear_incoming_array() {
	MNM_Veh_Multiclass *_veh;
	TFlt _to_be_moved = get_link_supply() * m_flow_scalar;
	while (!m_incoming_array.empty()) {
		if ( _to_be_moved > 0){
			_veh = dynamic_cast<MNM_Veh_Multiclass *>(m_incoming_array.front());
			m_incoming_array.pop_front();
			m_veh_pool.insert({_veh, TInt(0)});
			if (_veh -> m_class == 0) {
				//printf("car\n");
				m_volume_car += 1;
				_to_be_moved -= 1;
			}
			else {
				//printf("truck\n");
				m_volume_truck += 1;
				_to_be_moved -= m_veh_convert_factor;
			}
		}
		else {
			break;
		}
	}
	// printf("car: %d, truck: %d\n", m_volume_car, m_volume_truck);
	return 0;
}

void MNM_Dlink_Pq_Multiclass::print_info()
{
	printf("Link Dynamic model: Poing Queue\n");
	printf("Total car volume in the link: %.4f\n", (float)(m_volume_car/m_flow_scalar));
	printf("Total truck volume in the link: %.4f\n", (float)(m_volume_truck/m_flow_scalar));
}

int MNM_Dlink_Pq_Multiclass::evolve(TInt timestamp)
{
	std::unordered_map<MNM_Veh*, TInt>::iterator _que_it = m_veh_pool.begin();
	MNM_Veh_Multiclass* _veh;
	TInt _num_car = 0, _num_truck = 0;
	while (_que_it != m_veh_pool.end()) {
		if (_que_it -> second >= m_max_stamp) {
			m_finished_array.push_back(_que_it -> first);
			_veh = dynamic_cast<MNM_Veh_Multiclass *>(m_finished_array.back());
			if (_veh -> m_class == 0) {
				_num_car += 1;
			}
			else {
				_num_truck += 1;
			}
			_que_it = m_veh_pool.erase(_que_it); //c++ 11
		}
		else {
			_que_it -> second += 1;
			_que_it ++;
		}
	}
	// printf("car: %d, truck: %d\n", _num_car, _num_truck);
	return 0;
}

TFlt MNM_Dlink_Pq_Multiclass::get_link_flow()
{
	// For adaptive routing, need modidication for multiclass case
	// return TFlt(m_volume_car + m_volume_truck) / m_flow_scalar;

	return 0;
}

TFlt MNM_Dlink_Pq_Multiclass::get_link_tt()
{
	// For adaptive routing, need modidication for multiclass case

	// TFlt _cost, _spd;
	// // get the density in veh/mile
	// TFlt _rho = get_link_flow()/m_number_of_lane/m_length;
	// // get the jam density
	// TFlt _rhoj = m_lane_hold_cap;
	// // get the critical density
	// TFlt _rhok = m_lane_flow_cap/m_ffs;

	// if (_rho >= _rhoj){
	// 	_cost = MNM_Ults::max_link_cost();
	// }
	// else {
	// 	if (_rho <= _rhok){
	// 		_spd = m_ffs;
	// 	}
	// 	else {
	// 		_spd = MNM_Ults::max(0.001 * m_ffs, 
	// 				m_lane_flow_cap * (_rhoj - _rho) / (_rhoj - _rhok) / _rho);
	// 	}
	// 	_cost = m_length / _spd;
	// }
	// return _cost;
  	
  	return 0;
}




/******************************************************************************************************************
*******************************************************************************************************************
												Node Models
*******************************************************************************************************************
******************************************************************************************************************/

/**************************************************************************
                              Origin node
**************************************************************************/
MNM_DMOND_Multiclass::MNM_DMOND_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor)
	: MNM_DMOND::MNM_DMOND(ID, flow_scalar)
{
	m_veh_convert_factor = veh_convert_factor;
}

MNM_DMOND_Multiclass::~MNM_DMOND_Multiclass()
{
	;
}

int MNM_DMOND_Multiclass::evolve(TInt timestamp)
{
  	MNM_Dlink *_link;
  	MNM_Veh_Multiclass *_veh;
  	MNM_Dlink_Pq_Multiclass *_next_link;

  	for (unsigned i = 0; i < m_out_link_array.size(); ++i){
    	_link = m_out_link_array[i];
   		m_out_volume[_link] = 0;
  	}

  	/* compute out flow */
  	std::deque<MNM_Veh*>::iterator _que_it = m_in_veh_queue.begin();
  	while (_que_it != m_in_veh_queue.end()) {
  		_veh = dynamic_cast<MNM_Veh_Multiclass *>(*_que_it);
    	_link = _veh -> get_next_link();
    	if (_veh -> m_class == 0){
    		m_out_volume[_link] += 1;
    	}
    	else {
    		_next_link = dynamic_cast<MNM_Dlink_Pq_Multiclass *>(_link);
    		m_out_volume[_link] += _next_link -> m_veh_convert_factor;
    	}
    	_que_it++;
  	}
  	for (unsigned i = 0; i < m_out_link_array.size(); ++i){
	    _link = m_out_link_array[i];
	    if ((_link -> get_link_supply() * m_flow_scalar) < TFlt(m_out_volume[_link])){
	      	m_out_volume[_link] = TInt(_link -> get_link_supply() * m_flow_scalar);
	    }
  	}

  	/* move vehicle */
  	TInt _moved_car, _moved_truck;
  	for (unsigned i = 0; i < m_out_link_array.size(); ++i){
	    _link = m_out_link_array[i];
	    _moved_car = 0;
	    _moved_truck = 0;	    
	    _que_it = m_in_veh_queue.begin();
	    while (_que_it != m_in_veh_queue.end()) {
	      	if (m_out_volume[_link] > 0){
		        _veh = dynamic_cast<MNM_Veh_Multiclass *>(*_que_it);
		        if (_veh -> get_next_link() == _link){
					_link -> m_incoming_array.push_back(_veh);
					_veh -> set_current_link(_link);
					if (_veh -> m_class == 0){
						m_out_volume[_link] -= 1;
						_moved_car += 1;
					}
					else {
						_next_link = dynamic_cast<MNM_Dlink_Pq_Multiclass *>(_link);
						m_out_volume[_link] -= _next_link -> m_veh_convert_factor;
						_moved_truck += 1;
					}
					_que_it = m_in_veh_queue.erase(_que_it); //c++ 11
		        }
		        else{
		        	_que_it++;
		        }
	      	}
	      	else{
	        	break; //break while loop
	      	}
	    }
	    // record cc for both classes
	    _next_link = dynamic_cast<MNM_Dlink_Pq_Multiclass *>(_link);
	    if (_next_link -> m_N_in_car != NULL) {
	      	_next_link -> m_N_in_car -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp + 1), TFlt(_moved_car)/m_flow_scalar));
	    }
	    if (_next_link -> m_N_in_truck != NULL) {
	      	_next_link -> m_N_in_truck -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp + 1), TFlt(_moved_truck)/m_flow_scalar));
	    }
	    // printf("car: %d, truck: %d\n", _moved_car, _moved_truck);
  	}
  	return 0;
}


/**************************************************************************
                              Destination node
**************************************************************************/
MNM_DMDND_Multiclass::MNM_DMDND_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor)
	: MNM_DMDND::MNM_DMDND(ID, flow_scalar)
{
	m_veh_convert_factor = veh_convert_factor;
}

MNM_DMDND_Multiclass::~MNM_DMDND_Multiclass()
{
	;
}

int MNM_DMDND_Multiclass::evolve(TInt timestamp)
{
  	MNM_Dlink *_link;
  	MNM_Dlink_Pq_Multiclass *_from_link;
  	MNM_Veh_Multiclass *_veh;
  	size_t _size;
  	TInt _moved_car, _moved_truck;
  	for (size_t i = 0; i < m_in_link_array.size(); ++i){
  		_moved_car = 0;
	    _moved_truck = 0;
	    _link = m_in_link_array[i];
	    _size = _link -> m_finished_array.size();
	    for (size_t j = 0; j < _size; ++j){
			_veh = dynamic_cast<MNM_Veh_Multiclass *>(_link -> m_finished_array.front());
			if (_veh -> get_next_link() != NULL){
				printf("Something wrong in DMDND evolve\n");
				exit(-1);
			}
			m_out_veh_queue.push_back(_veh);
			_veh -> set_current_link(NULL);
			if (_veh -> m_class == 0){
				_moved_car += 1;
			}
			else {
				_moved_truck += 1;
			}
			_link -> m_finished_array.pop_front();
	    }
	    // record cc for both classes
	    _from_link = dynamic_cast<MNM_Dlink_Pq_Multiclass *>(_link);
	    if (_from_link -> m_N_out_car != NULL) {
	      	_from_link -> m_N_out_car -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp + 1), TFlt(_moved_car)/m_flow_scalar));
	    }
	    if (_from_link -> m_N_out_truck != NULL) {
	      	_from_link -> m_N_out_truck -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp + 1), TFlt(_moved_truck)/m_flow_scalar));
	    }
  	}
  	return 0;
}


/**************************************************************************
                   				In-out node
**************************************************************************/
MNM_Dnode_Inout_Multiclass::MNM_Dnode_Inout_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor)
	: MNM_Dnode::MNM_Dnode(ID, flow_scalar)
{
	m_demand = NULL;
	m_supply = NULL;
	m_veh_flow = NULL;
	m_veh_moved_car = NULL;
	m_veh_moved_truck = NULL;
	m_veh_convert_factor = veh_convert_factor;
}

MNM_Dnode_Inout_Multiclass::~MNM_Dnode_Inout_Multiclass()
{
  	if (m_demand != NULL) free(m_demand);
  	if (m_supply != NULL) free(m_supply);
  	if (m_veh_flow != NULL) free(m_veh_flow);
  	if (m_veh_moved_car != NULL) free(m_veh_moved_car);
  	if (m_veh_moved_truck != NULL) free(m_veh_moved_truck);
}

int MNM_Dnode_Inout_Multiclass::prepare_loading()
{
	TInt _num_in = m_in_link_array.size();
	TInt _num_out = m_out_link_array.size();
	m_demand = (TFlt*) malloc(sizeof(TFlt) * _num_in * _num_out); // real-world vehicles
	memset(m_demand, 0x0, sizeof(TFlt) * _num_in * _num_out);
	m_supply = (TFlt*) malloc(sizeof(TFlt) * _num_out); // real-world vehicles
	memset(m_supply, 0x0, sizeof(TFlt) * _num_out);
	m_veh_flow = (TFlt*) malloc(sizeof(TFlt) * _num_in * _num_out); // real-world vehicles
	memset(m_veh_flow, 0x0, sizeof(TFlt) * _num_in * _num_out);
	m_veh_moved_car = (TFlt*) malloc(sizeof(TFlt) * _num_in * _num_out); // simulation vehicles = real-world vehicles * flow scalar
	memset(m_veh_moved_car, 0x0, sizeof(TFlt) * _num_in * _num_out);
	m_veh_moved_truck = (TFlt*) malloc(sizeof(TFlt) * _num_in * _num_out); // simulation vehicles = real-world vehicles * flow scalar
	memset(m_veh_moved_truck, 0x0, sizeof(TFlt) * _num_in * _num_out);
	return 0;
}

int MNM_Dnode_Inout_Multiclass::prepare_supplyANDdemand()
{
	size_t _offset = m_out_link_array.size();
	TFlt _equiv_count;
	std::deque<MNM_Veh*>::iterator _veh_it;
	MNM_Dlink *_in_link, *_out_link;

	/* calculate demand */
	for (size_t i = 0; i < m_in_link_array.size(); ++i){
		_in_link = m_in_link_array[i];
		for (_veh_it = _in_link -> m_finished_array.begin(); _veh_it != _in_link -> m_finished_array.end(); _veh_it++){
			if (std::find(m_out_link_array.begin(), m_out_link_array.end(), (*_veh_it) -> get_next_link()) == m_out_link_array.end()){
				printf("Vehicle in the wrong node, no exit!\n");
        		printf("Vehicle is on link %d, node %d, next link ID is: %d\n", _in_link -> m_link_ID(), m_node_ID(), 
        			   (*_veh_it) -> get_next_link() -> m_link_ID());
        		exit(-1);
			}
		}
		for (size_t j = 0; j < m_out_link_array.size(); ++j){
			_out_link = m_out_link_array[j];
			_equiv_count = 0;
			for (_veh_it = _in_link -> m_finished_array.begin(); _veh_it != _in_link -> m_finished_array.end(); _veh_it++){
        		MNM_Veh_Multiclass *_veh = dynamic_cast<MNM_Veh_Multiclass *>(*_veh_it);
        		if (_veh -> get_next_link() == _out_link) {
        			if (_veh -> m_class == 0) {
        				// private car
        				_equiv_count += 1;
        			}
        			else { 
        				// truck
        				_equiv_count += m_veh_convert_factor;
        			}
        		}
      		}
      		m_demand[_offset * i + j] = _equiv_count / m_flow_scalar;
		}
	}

	/* calculated supply */
  	for (size_t j = 0; j < m_out_link_array.size(); ++j){
	    m_supply[j] = m_out_link_array[j] -> get_link_supply();
	    // printf("Link %d, supply is %.4f\n", _out_link -> m_link_ID, m_supply[j]);
  	}

  	return 0;
}

// int MNM_Dnode_Inout_Multiclass::flow_to_vehicle()
// {
// 	size_t _offset = m_out_link_array.size();
// 	MNM_Dlink *_out_link;
//  TFlt _to_move;
// 	size_t _rand_idx;
// 	for (size_t j = 0; j < m_out_link_array.size(); ++j){
// 		_to_move = 0;
// 		_out_link = m_out_link_array[j];
// 		for (size_t i = 0; i < m_in_link_array.size(); ++i){
// 			m_veh_tomove[i * _offset + j] = m_veh_flow[i * _offset + j] * m_flow_scalar;
//			_to_move += m_veh_tomove[i * _offset + j];
// 		}
// 	}
// 	return 0;
// }

int MNM_Dnode_Inout_Multiclass::record_cumulative_curve(TInt timestamp)
{
  	TInt _temp_sum_car, _temp_sum_truck;
  	MNM_Dlink_Multiclass *_in_link, *_out_link;
  	size_t _offset = m_out_link_array.size();

  	for (size_t j = 0; j < m_out_link_array.size(); ++j){
    	_temp_sum_car = 0;
    	_temp_sum_truck = 0;
    	_out_link = dynamic_cast<MNM_Dlink_Multiclass *>(m_out_link_array[j]);
    	for (size_t i = 0; i < m_in_link_array.size(); ++i) {
    		_in_link = dynamic_cast<MNM_Dlink_Multiclass *>(m_in_link_array[i]);
       		_temp_sum_car += m_veh_moved_car[i * _offset + j];
      		_temp_sum_truck += m_veh_moved_truck[i * _offset + j];
    	}
    	if (_out_link -> m_N_out_car != NULL) {
      		_out_link -> m_N_in_car -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp+1), TFlt(_temp_sum_car)/m_flow_scalar));
    	}
    	if (_out_link -> m_N_out_truck != NULL) {
      		_out_link -> m_N_in_truck -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp+1), TFlt(_temp_sum_truck)/m_flow_scalar));
    	}
  	}

  	for (size_t i = 0; i < m_in_link_array.size(); ++i){
    	_temp_sum_car = 0;
    	_temp_sum_truck = 0;
    	_in_link = dynamic_cast<MNM_Dlink_Multiclass *>(m_in_link_array[i]);
    	for (size_t j = 0; j < m_out_link_array.size(); ++j) {
      		_out_link = dynamic_cast<MNM_Dlink_Multiclass *>(m_out_link_array[j]);
      		_temp_sum_car += m_veh_moved_car[i * _offset + j];
      		_temp_sum_truck += m_veh_moved_truck[i * _offset + j];
    	}
    	if (_in_link -> m_N_in_car != NULL) {
      		_in_link -> m_N_out_car -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp+1), TFlt(_temp_sum_car)/m_flow_scalar));
    	}
    	if (_in_link -> m_N_in_truck != NULL) {
      		_in_link -> m_N_out_truck -> add_increment(std::pair<TFlt, TFlt>(TFlt(timestamp+1), TFlt(_temp_sum_truck)/m_flow_scalar));
    	}
  	}

  	return 0;
}

int MNM_Dnode_Inout_Multiclass::move_vehicle()
{
	MNM_Dlink *_in_link, *_out_link;
	size_t _offset = m_out_link_array.size();
	TFlt _to_move;
	TFlt _equiv_num;
	TFlt _r;

	for (size_t j = 0; j < m_out_link_array.size(); ++j){
		_out_link = m_out_link_array[j];
		for (size_t i = 0; i < m_in_link_array.size(); ++i){
			_in_link = m_in_link_array[i];
			_to_move = m_veh_flow[i * _offset + j] * m_flow_scalar;
			printf("from %d to %d: %.4f\n", _in_link -> m_link_ID, _out_link -> m_link_ID, _to_move);
			auto _veh_it = _in_link -> m_finished_array.begin();

			// if (_to_move > 0){
			// 	auto _veh_it2 = _in_link -> m_finished_array.begin();
			// 	while (_veh_it2 != _in_link -> m_finished_array.end()){
			// 		MNM_Veh_Multiclass *_veh2 = dynamic_cast<MNM_Veh_Multiclass *>(*_veh_it2);
			// 		printf("%d ", _veh2 -> m_class);
			// 		_veh_it2++;
			// 	}
			// 	printf("\n");
			// }

			while (_veh_it != _in_link -> m_finished_array.end()){
				if (_to_move > 0){
					MNM_Veh_Multiclass *_veh = dynamic_cast<MNM_Veh_Multiclass *>(*_veh_it);
					if (_veh -> get_next_link() == _out_link){
						// printf("%d ", _veh -> m_class);
						if (_veh -> m_class == 0) {
							// private car
							_equiv_num = 1;
						}
						else { 
							// truck
							_equiv_num = m_veh_convert_factor;
						}
						if (_to_move < _equiv_num) {
							// randomly decide to move or not in this case 
							// base on the probability = _to_move/_equiv_num < 1
							// _r = MNM_Ults::rand_flt()
							// 				if (_r <= _to_move/_equiv_num){
							// 	_out_link -> m_incoming_array.push_back(_veh);
							// 	_veh -> set_current_link(_out_link);
							// 	if (_veh -> m_class == 0){
							// 		m_veh_moved_car[i * _offset + j] += 1;
							// 	}
							// 	else {
							// 		m_veh_moved_truck[i * _offset + j] += 1;
							// 	}
							// 	_veh_it = _in_link -> m_finished_array.erase(_veh_it);
							// }
						}
						else {
							_out_link -> m_incoming_array.push_back(_veh);
							_veh -> set_current_link(_out_link);
							if (_veh -> m_class == 0){
								m_veh_moved_car[i * _offset + j] += 1;
							}
							else {
								m_veh_moved_truck[i * _offset + j] += 1;
							}
							_veh_it = _in_link -> m_finished_array.erase(_veh_it);
						}
						_to_move -= _equiv_num;
					}
					else {
						_veh_it++;
					}
				}
				else {
					break;
				}
			}
			// printf("\n");
			if (_to_move > 0.001){
		        printf("Something wrong during the vehicle moving, remaining to move %.16f\n", (float)_to_move);
		        // printf("The finished veh queue is now size %d\n", (int)_in_link->m_finished_array.size());
		        // printf("But it is heading to %d\n", (int)_in_link->m_finished_array.front() -> get_next_link() -> m_link_ID);
		        exit(-1);
		    }
		}
		random_shuffle(_out_link -> m_incoming_array.begin(), _out_link -> m_incoming_array.end());
	}

	return 0;
}

int MNM_Dnode_Inout_Multiclass::add_out_link(MNM_Dlink* out_link)
{
  	m_out_link_array.push_back(out_link);
  	return 0;
}

int MNM_Dnode_Inout_Multiclass::add_in_link(MNM_Dlink *in_link)
{
  	m_in_link_array.push_back(in_link);
  	return 0;
}

int MNM_Dnode_Inout_Multiclass::evolve(TInt timestamp)
{
	// printf("Inout node evolve\n");
	// printf("1\n");
	prepare_supplyANDdemand();
	// printf("2\n"); 
	compute_flow();
	// printf("3\n");
	// flow_to_vehicle();
	// printf("4\n");
	move_vehicle();
	// printf("5\n");
	record_cumulative_curve(timestamp);
	// printf("6\n");
	return 0;
}

/*                          FWJ node
**************************************************************************/
MNM_Dnode_FWJ_Multiclass::MNM_Dnode_FWJ_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor)
  : MNM_Dnode_Inout_Multiclass::MNM_Dnode_Inout_Multiclass(ID, flow_scalar, veh_convert_factor)
{
}

MNM_Dnode_FWJ_Multiclass::~MNM_Dnode_FWJ_Multiclass()
{

}

int MNM_Dnode_FWJ_Multiclass::compute_flow()
{
	size_t _offset = m_out_link_array.size();
	TFlt _sum_in_flow, _portion;
	for (size_t j = 0; j < m_out_link_array.size(); ++j){
		_sum_in_flow = TFlt(0);
		for (size_t i = 0; i < m_in_link_array.size(); ++i){
	  		_sum_in_flow += m_demand[i * _offset + j];
		}
		for (size_t i = 0; i < m_in_link_array.size(); ++i){
	  		_portion = MNM_Ults::divide(m_demand[i * _offset + j], _sum_in_flow);
		  	m_veh_flow[i * _offset + j] = MNM_Ults::min(m_demand[i * _offset + j], _portion * m_supply[j]);
		}
	}

	return 0;
}

/*               General Road Junction node
**************************************************************************/
MNM_Dnode_GRJ_Multiclass::MNM_Dnode_GRJ_Multiclass(TInt ID, TFlt flow_scalar, TFlt veh_convert_factor)
  : MNM_Dnode_Inout_Multiclass::MNM_Dnode_Inout_Multiclass(ID, flow_scalar, veh_convert_factor)
{
	m_d_a = NULL;
	m_C_a = NULL;
}

MNM_Dnode_GRJ_Multiclass::~MNM_Dnode_GRJ_Multiclass()
{
	if (m_d_a != NULL) free(m_d_a);
	if (m_C_a != NULL) free(m_C_a);
}

int MNM_Dnode_GRJ_Multiclass::prepare_loading()
{
	MNM_Dnode_Inout_Multiclass::prepare_loading();
	TInt _num_in = m_in_link_array.size();
	m_d_a = (TFlt*) malloc(sizeof(TFlt) * _num_in);
	memset(m_d_a, 0x0, sizeof(TFlt) * _num_in);
	m_C_a = (TFlt*) malloc(sizeof(TFlt) * _num_in);
	memset(m_C_a, 0x0, sizeof(TFlt) * _num_in);
	return 0;
}

int MNM_Dnode_GRJ_Multiclass::compute_flow()
{
	return 0;
}




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass OD
*******************************************************************************************************************
******************************************************************************************************************/

/**************************************************************************
                          		Origin
**************************************************************************/
MNM_Origin_Multiclass::MNM_Origin_Multiclass(TInt ID, 
											TInt max_interval, 
											TFlt flow_scalar, 
											TInt frequency)
	: MNM_Origin::MNM_Origin(ID, max_interval, flow_scalar, frequency)
{
	m_demand_car = std::unordered_map<MNM_Destination_Multiclass*, TFlt*>();
	m_demand_truck = std::unordered_map<MNM_Destination_Multiclass*, TFlt*>();
}

MNM_Origin_Multiclass::~MNM_Origin_Multiclass()
{
	for (auto _demand_it : m_demand_car) {
		free(_demand_it.second);
	}
	m_demand_car.clear();

	for (auto _demand_it : m_demand_truck) {
		free(_demand_it.second);
	}
	m_demand_truck.clear();
}

int MNM_Origin_Multiclass::add_dest_demand_multiclass(MNM_Destination_Multiclass *dest, 
													TFlt* demand_car, 
													TFlt* demand_truck)
{
  	TFlt* _demand_car = (TFlt*) malloc(sizeof(TFlt) * m_max_assign_interval);
  	for (int i = 0; i < m_max_assign_interval; ++i) {
  		_demand_car[i] =  TFlt(demand_car[i]);
  	}
  	m_demand_car.insert({dest, _demand_car});

  	TFlt* _demand_truck = (TFlt*) malloc(sizeof(TFlt) * m_max_assign_interval);
  	for (int i = 0; i < m_max_assign_interval; ++i) {
  		_demand_truck[i] =  TFlt(demand_truck[i]);
  	}
  	m_demand_truck.insert({dest, _demand_truck});
  	
  	return 0;
}

int MNM_Origin_Multiclass::release(MNM_Veh_Factory* veh_factory, TInt current_interval)
{
  	if ((m_current_assign_interval < m_max_assign_interval) && (current_interval % m_frequency == 0)){
    	TInt _veh_to_release;
    	MNM_Veh_Multiclass *_veh;
    	MNM_Veh_Factory_Multiclass *_vfactory = dynamic_cast<MNM_Veh_Factory_Multiclass *>(veh_factory);
    	// release all car
	    for (auto _demand_it = m_demand_car.begin(); _demand_it != m_demand_car.end(); _demand_it++) {
	    	_veh_to_release = TInt(MNM_Ults::round((_demand_it -> second)[m_current_assign_interval] * m_flow_scalar));
	      	for (int i = 0; i < _veh_to_release; ++i) {
		        _veh = _vfactory -> make_veh_multiclass(current_interval, MNM_TYPE_ADAPTIVE, TInt(0));
		        _veh -> set_destination(_demand_it -> first);
		        _veh -> set_origin(this);
		        m_origin_node -> m_in_veh_queue.push_back(_veh);
	      	}
	    }
	    // release all truck
	    for (auto _demand_it = m_demand_truck.begin(); _demand_it != m_demand_truck.end(); _demand_it++) {
	    	_veh_to_release = TInt(MNM_Ults::round((_demand_it -> second)[m_current_assign_interval] * m_flow_scalar));
	      	for (int i = 0; i < _veh_to_release; ++i) {
		        _veh = _vfactory -> make_veh_multiclass(current_interval, MNM_TYPE_ADAPTIVE, TInt(1));
		        _veh -> set_destination(_demand_it -> first);
		        _veh -> set_origin(this);
		        m_origin_node -> m_in_veh_queue.push_back(_veh);
	      	}
	    }
	    m_current_assign_interval++;
  	}
  	random_shuffle(m_origin_node -> m_in_veh_queue.begin(), m_origin_node -> m_in_veh_queue.end());
  	return 0;
}

int MNM_Origin_Multiclass::release_one_interval(TInt current_interval, 
												MNM_Veh_Factory* veh_factory, 
												TInt assign_interval, 
												TFlt adaptive_ratio)
{
	if (assign_interval < 0) return 0;
	TInt _veh_to_release;
	MNM_Veh_Multiclass *_veh;
	MNM_Veh_Factory_Multiclass *_vfactory = dynamic_cast<MNM_Veh_Factory_Multiclass *>(veh_factory);
	// release all car
	for (auto _demand_it = m_demand_car.begin(); _demand_it != m_demand_car.end(); _demand_it++) {
		_veh_to_release = TInt(MNM_Ults::round((_demand_it -> second)[assign_interval] * m_flow_scalar));
		for (int i = 0; i < _veh_to_release; ++i) {
			if (adaptive_ratio == TFlt(0)){
				_veh = _vfactory -> make_veh_multiclass(current_interval, MNM_TYPE_STATIC, TInt(0));
			}
			else if (adaptive_ratio == TFlt(1)){
				_veh = _vfactory -> make_veh_multiclass(current_interval, MNM_TYPE_ADAPTIVE, TInt(0));
			}
			else{
				printf("HyBrid assign, not implemented!\n");
				exit(-1);
			}
			_veh -> set_destination(_demand_it -> first);
			_veh -> set_origin(this);
			m_origin_node -> m_in_veh_queue.push_back(_veh);
		}
	}
	// release all truck
	for (auto _demand_it = m_demand_truck.begin(); _demand_it != m_demand_truck.end(); _demand_it++) {
		_veh_to_release = TInt(MNM_Ults::round((_demand_it -> second)[assign_interval] * m_flow_scalar));
		for (int i = 0; i < _veh_to_release; ++i) {
			if (adaptive_ratio == TFlt(0)){
				_veh = _vfactory -> make_veh_multiclass(current_interval, MNM_TYPE_STATIC, TInt(1));
			}
			else if (adaptive_ratio == TFlt(1)){
				_veh = _vfactory -> make_veh_multiclass(current_interval, MNM_TYPE_ADAPTIVE, TInt(1));
			}
			else{
				printf("HyBrid assign, not implemented!\n");
				exit(-1);
			}
			_veh -> set_destination(_demand_it -> first);
			_veh -> set_origin(this);
			m_origin_node -> m_in_veh_queue.push_back(_veh);
		}
	}
	random_shuffle(m_origin_node -> m_in_veh_queue.begin(), m_origin_node -> m_in_veh_queue.end());
 	return 0;
}

/**************************************************************************
                          		Destination
**************************************************************************/
MNM_Destination_Multiclass::MNM_Destination_Multiclass(TInt ID)
	: MNM_Destination::MNM_Destination(ID)
{
	;
}


MNM_Destination_Multiclass::~MNM_Destination_Multiclass()
{
	;
}




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass Vehicle
*******************************************************************************************************************
******************************************************************************************************************/
MNM_Veh_Multiclass::MNM_Veh_Multiclass(TInt ID, TInt vehicle_class, TInt start_time)
	: MNM_Veh::MNM_Veh(ID, start_time)
{
	m_class = vehicle_class;
}

MNM_Veh_Multiclass::~MNM_Veh_Multiclass()
{
	;
}



/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass Factory
*******************************************************************************************************************
******************************************************************************************************************/

/**************************************************************************
                          Vehicle Factory
**************************************************************************/
MNM_Veh_Factory_Multiclass::MNM_Veh_Factory_Multiclass()
	: MNM_Veh_Factory::MNM_Veh_Factory()
{
	;
}

MNM_Veh_Factory_Multiclass::~MNM_Veh_Factory_Multiclass()
{
	;
}

MNM_Veh_Multiclass* MNM_Veh_Factory_Multiclass::make_veh_multiclass(TInt timestamp, 
														 			Vehicle_type veh_type,
														 			TInt vehicle_cls)
{
	// printf("A vehicle is produce at time %d, ID is %d\n", (int)timestamp, (int)m_num_veh + 1);
	MNM_Veh_Multiclass *_veh = new MNM_Veh_Multiclass(m_num_veh + 1, vehicle_cls, timestamp);
	_veh -> m_type = veh_type;
	m_veh_map.insert({m_num_veh + 1, _veh});
	m_num_veh += 1;
	return _veh;
}

/**************************************************************************
                          Node factory
**************************************************************************/
MNM_Node_Factory_Multiclass::MNM_Node_Factory_Multiclass()
	: MNM_Node_Factory::MNM_Node_Factory()
{
	;
}

MNM_Node_Factory_Multiclass::~MNM_Node_Factory_Multiclass()
{
	;
}

MNM_Dnode *MNM_Node_Factory_Multiclass::make_node_multiclass(TInt ID, 
												  			DNode_type_multiclass node_type, 
												  			TFlt flow_scalar,
												  			TFlt veh_convert_factor)
{
	MNM_Dnode *_node;
	switch (node_type){
    	case MNM_TYPE_FWJ_MULTICLASS:
			_node = new MNM_Dnode_FWJ_Multiclass(ID, flow_scalar, veh_convert_factor);
			break;
    	case MNM_TYPE_ORIGIN_MULTICLASS:
			_node = new MNM_DMOND_Multiclass(ID, flow_scalar, veh_convert_factor);
			break;
    	case MNM_TYPE_DEST_MULTICLASS:
			_node = new MNM_DMDND_Multiclass(ID, flow_scalar, veh_convert_factor);
			break;
    	default:
			printf("Wrong node type.\n");
			exit(-1);
	}
	m_node_map.insert({ID, _node});
	return _node;
}

/**************************************************************************
                          Link factory
**************************************************************************/
MNM_Link_Factory_Multiclass::MNM_Link_Factory_Multiclass()
	: MNM_Link_Factory::MNM_Link_Factory()
{
	;
}

MNM_Link_Factory_Multiclass::~MNM_Link_Factory_Multiclass()
{
	;
}

MNM_Dlink *MNM_Link_Factory_Multiclass::make_link_multiclass(TInt ID,
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
															TFlt flow_scalar)
{
	MNM_Dlink *_link;
	switch (link_type){
    	case MNM_TYPE_CTM_MULTICLASS:
			_link = new MNM_Dlink_Ctm_Multiclass(ID,
												number_of_lane,
												length,
												lane_hold_cap_car,
												lane_hold_cap_truck,
												lane_flow_cap_car,
												lane_flow_cap_truck,
												ffs_car,
												ffs_truck,
												unit_time,
												veh_convert_factor,
												flow_scalar);
			break;
    	case MNM_TYPE_PQ_MULTICLASS:
			_link = new MNM_Dlink_Pq_Multiclass(ID,
												number_of_lane,
												length,
												lane_hold_cap_car,
												lane_hold_cap_truck,
												lane_flow_cap_car,
												lane_flow_cap_truck,
												ffs_car,
												ffs_truck,
												unit_time,
												veh_convert_factor,
												flow_scalar);
			break;
    	default:
			printf("Wrong link type.\n");
			exit(-1);
	}
	m_link_map.insert({ID, _link});
	return _link;
}

/**************************************************************************
                          OD factory
**************************************************************************/
MNM_OD_Factory_Multiclass::MNM_OD_Factory_Multiclass()
	: MNM_OD_Factory::MNM_OD_Factory()
{
	;
}

MNM_OD_Factory_Multiclass::~MNM_OD_Factory_Multiclass()
{
	;
}

MNM_Destination_Multiclass *MNM_OD_Factory_Multiclass::make_destination(TInt ID)
{
	MNM_Destination_Multiclass *_dest;
	_dest = new MNM_Destination_Multiclass(ID);
	m_destination_map.insert({ID, _dest});
	return _dest;
}

MNM_Origin_Multiclass *MNM_OD_Factory_Multiclass::make_origin(TInt ID, 
												TInt max_interval, 
												TFlt flow_scalar, 
												TInt frequency)
{
	MNM_Origin_Multiclass *_origin;
	_origin = new MNM_Origin_Multiclass(ID, max_interval, flow_scalar, frequency);
	m_origin_map.insert({ID, _origin});
	return _origin;
}




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass IO Functions
*******************************************************************************************************************
******************************************************************************************************************/
int MNM_IO_Multiclass::build_node_factory_multiclass(std::string file_folder, 
											MNM_ConfReader *conf_reader, 
											MNM_Node_Factory *node_factory)
{
	/* find file */
	std::string _node_file_name = file_folder + "/MNM_input_node";
	std::ifstream _node_file;
	_node_file.open(_node_file_name, std::ios::in);

	/* read config */
	TInt _num_of_node = conf_reader -> get_int("num_of_node");
	TFlt _flow_scalar = conf_reader -> get_float("flow_scalar");

	/* read file */
	std::string _line;
	std::vector<std::string> _words;
	TInt _node_ID;
	std::string _type;
	TFlt _veh_convert_factor;

	MNM_Node_Factory_Multiclass* _node_factory = dynamic_cast<MNM_Node_Factory_Multiclass *>(node_factory);

	if (_node_file.is_open())
	{
		std::getline(_node_file,_line); //skip the first line
		for (int i = 0; i < _num_of_node; ++i){
			std::getline(_node_file,_line);
			_words = split(_line, ' ');
			if (_words.size() == 3) {
				_node_ID = TInt(std::stoi(_words[0]));
				_type = trim(_words[1]);
				_veh_convert_factor = TFlt(std::stod(_words[2]));
				if (_type == "FWJ"){
					_node_factory -> make_node_multiclass(_node_ID, 
														MNM_TYPE_FWJ_MULTICLASS, 
														_flow_scalar,
														_veh_convert_factor);
					continue;
				}				
				if (_type =="DMOND"){
					_node_factory -> make_node_multiclass(_node_ID, 
														MNM_TYPE_ORIGIN_MULTICLASS, 
														_flow_scalar,
														_veh_convert_factor);
					continue;
				}
				if (_type =="DMDND"){
					_node_factory -> make_node_multiclass(_node_ID, 
														MNM_TYPE_DEST_MULTICLASS, 
														_flow_scalar,
														_veh_convert_factor);
					continue;
				}
				printf("Wrong node type, %s\n", _type.c_str());
				exit(-1);
			}
			else {
				printf("MNM_IO_Multiclass::build_node_factory_Multiclass: Wrong length of line.\n");
				exit(-1);
			}
		}
		_node_file.close();
	}
	return 0;
}

int MNM_IO_Multiclass::build_link_factory_multiclass(std::string file_folder, 
										MNM_ConfReader *conf_reader, 
										MNM_Link_Factory *link_factory, 
										std::string file_name)
{
	/* find file */
	std::string _link_file_name = file_folder + "/" + file_name;
	std::ifstream _link_file;
	_link_file.open(_link_file_name, std::ios::in);

	/* read config */
	TInt _num_of_link = conf_reader -> get_int("num_of_link");
	TFlt _flow_scalar = conf_reader -> get_float("flow_scalar");
	TFlt _unit_time = conf_reader -> get_float("unit_time");

	/* read file */
	std::string _line;
	std::vector<std::string> _words;
	TInt _link_ID;
	TFlt _lane_hold_cap_car;
	TFlt _lane_flow_cap_car;
	TInt _number_of_lane;
	TFlt _length;
	TFlt _ffs_car;
	std::string _type;
	// new in multiclass vehicle case
	TFlt _lane_hold_cap_truck;
	TFlt _lane_flow_cap_truck;
	TFlt _ffs_truck;
	TFlt _veh_convert_factor;

	MNM_Link_Factory_Multiclass* _link_factory = dynamic_cast<MNM_Link_Factory_Multiclass *>(link_factory);

	if (_link_file.is_open())
	{
		// printf("Start build link factory.\n");
		std::getline(_link_file,_line); //skip the first line
		for (int i = 0; i < _num_of_link; ++i){
			std::getline(_link_file,_line);
			_words = split(_line, ' ');
			if (_words.size() == 11) {
				_link_ID = TInt(std::stoi(_words[0]));
				_type = trim(_words[1]);
				_length = TFlt(std::stod(_words[2]));
				_ffs_car = TFlt(std::stod(_words[3]));
				_lane_flow_cap_car = TFlt(std::stod(_words[4]));
				_lane_hold_cap_car = TFlt(std::stod(_words[5]));
				_number_of_lane = TInt(std::stoi(_words[6]));
				// new in multiclass vehicle case
				_ffs_truck = TFlt(std::stod(_words[7]));
				_lane_flow_cap_truck = TFlt(std::stod(_words[8]));
				_lane_hold_cap_truck = TFlt(std::stod(_words[9]));
				_veh_convert_factor = TFlt(std::stod(_words[10]));

				/* unit conversion */
				_length = _length * TFlt(1600);
				_ffs_car = _ffs_car * TFlt(1600) / TFlt(3600);
				_lane_flow_cap_car = _lane_flow_cap_car / TFlt(3600);
				_lane_hold_cap_car = _lane_hold_cap_car / TFlt(1600);
				_ffs_truck = _ffs_truck * TFlt(1600) / TFlt(3600);
				_lane_flow_cap_truck = _lane_flow_cap_truck / TFlt(3600);
				_lane_hold_cap_truck = _lane_hold_cap_truck / TFlt(1600);

				/* build */
				if (_type == "PQ"){
					_link_factory -> make_link_multiclass(_link_ID,
														MNM_TYPE_PQ_MULTICLASS,
														_number_of_lane,
														_length,
														_lane_hold_cap_car,
														_lane_hold_cap_truck,
														_lane_flow_cap_car,
														_lane_flow_cap_truck,
														_ffs_car,
														_ffs_truck,
														_unit_time,
														_veh_convert_factor,
														_flow_scalar);					
					continue;
				}
				if (_type =="CTM"){
					_link_factory -> make_link_multiclass(_link_ID,
														MNM_TYPE_CTM_MULTICLASS,
														_number_of_lane,
														_length,
														_lane_hold_cap_car,
														_lane_hold_cap_truck,
														_lane_flow_cap_car,
														_lane_flow_cap_truck,
														_ffs_car,
														_ffs_truck,
														_unit_time,
														_veh_convert_factor,
														_flow_scalar);
					continue;
				}
				printf("Wrong link type, %s\n", _type.c_str());
				exit(-1);
			}
			else{
				printf("MNM_IO::build_link_factory::Wrong length of line.\n");
				exit(-1);
			}
		}
		_link_file.close();
	}
	return 0;
}


int MNM_IO_Multiclass::build_demand_multiclass(std::string file_folder, 
 											MNM_ConfReader *conf_reader, 
 											MNM_OD_Factory *od_factory)
{
	/* find file */
	std::string _demand_file_name = file_folder + "/MNM_input_demand";
	std::ifstream _demand_file;
	_demand_file.open(_demand_file_name, std::ios::in);

	/* read config */
	TInt _max_interval = conf_reader -> get_int("max_interval"); 
	TInt _num_OD = conf_reader -> get_int("OD_pair");

	/* build */
	TInt _O_ID, _D_ID;
	MNM_Origin_Multiclass *_origin;
	MNM_Destination_Multiclass *_dest;
	std::string _line;
	std::vector<std::string> _words;
	if (_demand_file.is_open())
	{
		// printf("Start build demand profile.\n");
		TFlt *_demand_vector_car = (TFlt*) malloc(sizeof(TFlt) * _max_interval);
		TFlt *_demand_vector_truck = (TFlt*) malloc(sizeof(TFlt) * _max_interval);
		memset(_demand_vector_car, 0x0, sizeof(TFlt) * _max_interval);
		memset(_demand_vector_truck, 0x0, sizeof(TFlt) * _max_interval);
		std::getline(_demand_file,_line); //skip the first line
		for (int i = 0; i < _num_OD; ++i){
			std::getline(_demand_file,_line);
			// std::cout << "Processing: " << _line << "\n";
			_words = split(_line, ' ');
			if (TInt(_words.size()) == (_max_interval * 2 + 2)) {
				_O_ID = TInt(std::stoi(_words[0]));
				_D_ID = TInt(std::stoi(_words[1]));
				for (int j = 0; j < _max_interval; ++j) {
					_demand_vector_car[j] = TFlt(std::stod(_words[j + 2]));
					_demand_vector_truck[j] = TFlt(std::stod(_words[j + _max_interval + 2]));
				}
				_origin = dynamic_cast<MNM_Origin_Multiclass *>(od_factory -> get_origin(_O_ID));
				_dest = dynamic_cast<MNM_Destination_Multiclass *>(od_factory -> get_destination(_D_ID));
				_origin -> add_dest_demand_multiclass(_dest, _demand_vector_car, _demand_vector_truck);
			}
			else{
				printf("Something wrong in build_demand!\n");
				free(_demand_vector_car);
				free(_demand_vector_truck);
				exit(-1);
			}
		}
		free(_demand_vector_car);
		free(_demand_vector_truck);
		_demand_file.close();
	}
	return 0;
}




/******************************************************************************************************************
*******************************************************************************************************************
												Multiclass DTA
*******************************************************************************************************************
******************************************************************************************************************/
MNM_Dta_Multiclass::MNM_Dta_Multiclass(std::string file_folder)
	: MNM_Dta::MNM_Dta(file_folder)
{
	// Re-run the multiclass version of initialize();
	initialize();
}

MNM_Dta_Multiclass::~MNM_Dta_Multiclass()
{
	;
}

int MNM_Dta_Multiclass::initialize()
{
	m_veh_factory = new MNM_Veh_Factory_Multiclass();
	m_node_factory = new MNM_Node_Factory_Multiclass();
	m_link_factory = new MNM_Link_Factory_Multiclass();
	m_od_factory = new MNM_OD_Factory_Multiclass();
	m_config = new MNM_ConfReader(m_file_folder + "/config.conf", "DTA");
	m_unit_time = m_config -> get_int("unit_time");
	m_flow_scalar = m_config -> get_int("flow_scalar");
	m_assign_freq = m_config -> get_int("assign_frq");
	m_start_assign_interval = m_config -> get_int("start_assign_interval");
	m_total_assign_inter = m_config ->  get_int("max_interval");

	return 0;
}

int MNM_Dta_Multiclass::build_from_files()
{
	MNM_IO_Multiclass::build_node_factory_multiclass(m_file_folder, m_config, m_node_factory);
	MNM_IO_Multiclass::build_link_factory_multiclass(m_file_folder, m_config, m_link_factory);
	MNM_IO_Multiclass::build_od_factory_multiclass(m_file_folder, m_config, m_od_factory, m_node_factory);
	m_graph = MNM_IO_Multiclass::build_graph(m_file_folder, m_config);
	MNM_IO_Multiclass::build_demand_multiclass(m_file_folder, m_config, m_od_factory);
	build_workzone();
	set_statistics();
	set_routing();
	return 0;
}