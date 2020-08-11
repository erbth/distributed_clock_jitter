#include <cmath>
#include <cstring>
#include <cinttypes>
#include <arpa/inet.h>
#include <functional>
#include "controller.h"

using namespace std;

int cmp_mac_addrs (unsigned const char a[], unsigned const char b[])
{
	for (int i = 0; i < 6; i++)
	{
		if (a[i] < b[i])
			return -1;

		if (a[i] > b[i])
			return 1;
	}

	return 0;
}

uint16_t days_in_year (uint16_t year)
{
	if (year % 4 == 0)
	{
		if (year % 100 == 0)
		{
			if (year % 400)
				return 366;
		}
		else
		{
			return 366;
		}
	}

	return 365;
}

controller::controller (shared_ptr<system_services::provider> prov)
	:
		prov(prov),
		master_alive_timer(prov->register_timer (
					bind (&controller::master_alive_handler, this), 1500))
{
	frame_subscriber = prov->add_frame_subscriber (
			bind (&controller::receive_frame, this, placeholders::_1));

	/* Start in slave mode */
	is_master = false;
	memset (lowest_mac_pulse_received, 0xff, sizeof (lowest_mac_pulse_received));
	update_display();

	/* The first handler call will be in 1.5s, and will make us master if none
	 * has been discovered so far. */
}


void controller::master_alive_handler()
{
	if (is_master)
		return;

	/* If no master was discovered yet, make ourselves master. */
	if (cmp_mac_addrs (lowest_mac_pulse_received, (unsigned char*) "\xff\xff\xff\xff\xff\xff") == 0)
	{
		enable_master_mode();
		update_display ();
		return;
	}

	/* If the last signal we got from the choosen master is more than 1.5
	 * seconds in the past, question if it is still active. */
	if (prov->get_monotonic_time() - time_last_pulse_received >= 1.5)
	{
		memset (lowest_mac_pulse_received, 0xff, sizeof (lowest_mac_pulse_received));
		update_display ();
	}
}

/** Send a time signal pulse with the current time. */
void controller::time_signal_sender()
{
	last_pulse_sent_time = prov->get_utc();

	time_signal_pulse pulse;
	pulse.year       = last_pulse_sent_time.year;
	pulse.day        = last_pulse_sent_time.day_of_year;
	pulse.second     = last_pulse_sent_time.second_of_day;
	pulse.nanosecond = last_pulse_sent_time.nanosecond;

	prov->send_frame (pulse.to_frame());

	update_display();
}


void controller::receive_frame (const ethernet_frame &frame)
{
	if (frame.ether_type != 0x88b6)
		return;

	if (ntohs(*((uint16_t*) frame.data + 0)) != 0x0133)
		return;

	receive_pulse (frame);
}

void controller::receive_pulse (const ethernet_frame &frame)
{
	auto utc = prov->get_utc();

	auto o = time_signal_pulse::from_frame (frame);
	if (!o)
		return;

	auto pulse = *o;

	/* If this is a new master, remember it's address and switch to slave mode
	 * if currently in master mode. */
	if (cmp_mac_addrs (pulse.src, lowest_mac_pulse_received) < 0)
	{
		if (cmp_mac_addrs (pulse.src, prov->get_own_mac_address()) < 0)
		{
			disable_master_mode ();
			memcpy (lowest_mac_pulse_received, pulse.src, sizeof (pulse.src));
		}
	}

	/* Only use the time signal from the chosen master */
	if (cmp_mac_addrs (pulse.src, lowest_mac_pulse_received) == 0)
	{
		time_last_pulse_received = prov->get_monotonic_time();

		last_pulse_received_time.year          = pulse.year;
		last_pulse_received_time.day_of_year   = pulse.day;
		last_pulse_received_time.second_of_day = pulse.second;
		last_pulse_received_time.nanosecond    = pulse.nanosecond;

		int32_t diff_days = 0;

		/* Different years */
		if (utc.year < pulse.year)
		{
			for (uint16_t y = utc.year; y < pulse.year; y++)
				diff_days += days_in_year (y);
		}
		else if (utc.year > pulse.year)
		{
			for (uint16_t y = pulse.year; y < utc.year; y++)
				diff_days -= days_in_year(y);
		}

		/* Different days (start from 0) */
		diff_days += (int32_t) pulse.day - utc.day_of_year;

		/* Different seconds */
		double diff_seconds = (double) pulse.second - utc.second_of_day;

		/* Different nanoseconds */
		double diff_nanoseconds = ((double) pulse.nanosecond - utc.nanosecond) * 1e-9;

		/* Overall difference */
		double new_deviation = diff_days * 86400. + diff_seconds + diff_nanoseconds;

		update_statistics (new_deviation);
	}

	update_display ();
}


void controller::enable_master_mode()
{
	if (is_master)
		return;

	is_master = true;
	last_pulse_sent_time = system_services::calendar_time();
	time_signal_timer = prov->register_timer (
				bind (&controller::time_signal_sender, this), 1000);
}

void controller::disable_master_mode()
{
	if (!is_master)
		return;

	is_master = false;
	time_signal_timer = nullopt;

	memset (lowest_mac_pulse_received, 0xff, sizeof (lowest_mac_pulse_received));
	time_last_pulse_received = prov->get_monotonic_time();
	last_pulse_received_time = system_services::calendar_time();
}

void controller::update_statistics (double new_deviation)
{
	/* Update stored deviations */
	for (int i = 99; i > 0; i--)
	{
		deviations[i] = deviations[i - 1];
	}

	deviations[0] = new_deviation;

	/* Compute mu */
	mu_10 = mu_100 = 0;

	for (int i = 0; i < 10; i++)
		mu_10 += deviations[i];

	for (int i = 0; i < 100; i++)
		mu_100 += deviations[i];

	mu_10 /= 10;
	mu_100 /= 100;

	/* Compute delta */
	delta_10_max = delta_100_max = delta_10_bar = delta_100_bar = 0;

	for (int i = 0; i < 10; i++)
	{
		auto delta = fabs(deviations[i] - mu_10);
		delta_10_max = delta_10_max > delta ? delta_10_max : delta;
		delta_10_bar += delta;
	}

	delta_10_bar /= 10;

	for (int i = 0; i < 100; i++)
	{
		auto delta = fabs(deviations[i] - mu_100);
		delta_100_max = delta_100_max > delta ? delta_100_max : delta;
		delta_100_bar += delta;
	}

	delta_100_bar /= 100;
}

void controller::update_display()
{
	if (is_master)
	{
		prov->printf ("\033[2K\033[1F\033[2K\033[1F\033[2K\033[1F\033[0K"
				"m - %" PRIu16 ":%" PRIu16 ":%" PRIu32 ":%" PRIu32 "\n\n\n",
				last_pulse_sent_time.year,
				last_pulse_sent_time.day_of_year,
				last_pulse_sent_time.second_of_day,
				last_pulse_sent_time.nanosecond);

		prov->flush();
	}
	else
	{
		prov->printf ("\033[2K\033[1F\033[2K\033[1F\033[2K\033[1F\033[0K"
				"s [%02x:%02x:%02x:%02x:%02x:%02x] - "
				"%" PRIu16 ":%" PRIu16 ":%" PRIu32 ":%" PRIu32 "\n",
				(int) lowest_mac_pulse_received[0], (int) lowest_mac_pulse_received[1],
				(int) lowest_mac_pulse_received[2], (int) lowest_mac_pulse_received[3],
				(int) lowest_mac_pulse_received[4], (int) lowest_mac_pulse_received[5],
				last_pulse_received_time.year,
				last_pulse_received_time.day_of_year,
				last_pulse_received_time.second_of_day,
				last_pulse_received_time.nanosecond);

		prov->printf ("  current deviation: %es, mu_10 = %es, mu_100 = %es,\n",
				deviations[0], mu_10, mu_100);

		prov->printf ("  delta_10_max = %es, delta_100_max = %es,\n"
				"  delta_10_bar = %es, delta_100_bar = %es",
				delta_10_max, delta_100_max, delta_10_bar, delta_100_bar);

		prov->flush();
	}
}
