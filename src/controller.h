#ifndef __CONTROLLER_H
#define __CONTROLLER_H

/** The controller which sends and receives frames, and therefore initiates
 * jitter calculations */

#include <optional>
#include "system_services.h"

class controller
{
private:
	std::shared_ptr<system_services::provider> prov;

	/* The controller has two states: Master or slave. */
	bool is_master;

	/* A timer to determine if the master is alive */
	system_services::provider::timer_registration master_alive_timer;
	void master_alive_handler();

	/* A timer for sending the time signal if the controller is in master mode
	 * */
	std::optional<system_services::provider::timer_registration> time_signal_timer;
	void time_signal_sender();
	system_services::calendar_time last_pulse_sent_time;

	/* Receive ethernet frames */
	system_services::provider::frame_subscriber_registration frame_subscriber;
	void receive_frame (const ethernet_frame &frame);
	void receive_pulse (const ethernet_frame &frame);

	mac_addr_t lowest_mac_pulse_received;
	system_services::linear_time time_last_pulse_received;
	system_services::calendar_time last_pulse_received_time;

	/* Switch to master mode */
	void enable_master_mode();

	/* Switch to slave mode */
	void disable_master_mode();

	/* Statistics */
	/* Positive deviation means the local clock is behind the master's clock. */
	double deviations[100] = { 0 };

	/* Statistics on the deviation */
	/* Moving window average over the last 10 resp. 100 measurements */
	double mu_10 = 0;
	double mu_100 = 0;

	/* Maximum deviation (jitter, deviation from average deviation ;-)) from the
	 * corresponding moving window average over the last 10 resp. 100
	 * measurements */
	double delta_10_max = 0;
	double delta_100_max = 0;

	/* Moving window average deviation (jitter) from the corresponding moving
	 * window average over the last 10 resp. 100 measurements */
	double delta_10_bar = 0;
	double delta_100_bar = 0;

	void update_statistics (double new_deviation);

	/* Update the displayed values */
	void update_display();

public:
	controller (std::shared_ptr<system_services::provider> prov);
};

#endif /* __CONTROLLER_H */
