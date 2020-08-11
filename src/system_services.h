#ifndef __SYSTEM_SERVICES_H
#define __SYSTEM_SERVICES_H

/** An abstraction of operating system specific services */

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <shared_mutex>
#include <iostream>
#include "protocol.h"

namespace system_services
{

struct linear_time
{
	uint64_t seconds;
	uint32_t nanoseconds;

	linear_time(uint64_t seconds, uint32_t nanoseconds)
		: seconds(seconds), nanoseconds(nanoseconds)
	{}

	linear_time()
		: seconds(0), nanoseconds(0)
	{}

	/* Add- and subtract linear times. Over- and underflow yields undefined
	 * behavior ;-)
	 * XXX: Adding- or subtracting linear time destroys leap seconds! Only do
	 * this if the used clock does not provide those ... */
	linear_time operator+(const linear_time &o) const
	{
		linear_time res = *this;
		res.nanoseconds += o.nanoseconds;
		res.seconds += o.seconds;

		if (res.nanoseconds >= 1000000000)
		{
			res.nanoseconds -= 1000000000;
			res.seconds += 1;
		}

		return res;
	}

	linear_time operator-(const linear_time &o) const
	{
		linear_time res = *this;

		if (res.nanoseconds >= o.nanoseconds)
		{
			res.nanoseconds -= o.nanoseconds;
		}
		else
		{
			res.nanoseconds = res.nanoseconds + 1000000000 - o.nanoseconds;
			res.seconds -= 1;
		}

		res.seconds -= o.seconds;

		return res;
	}

	operator double()
	{
		return (double) seconds + nanoseconds / 1000000000.;
	}
};

struct calendar_time
{
	uint16_t year = 0;

	/* 0 to 365 */
	uint16_t day_of_year = 0;
	uint32_t second_of_day = 0;
	uint32_t nanosecond = 0;
};


class provider : public std::enable_shared_from_this<provider>
{
protected:
	/* Prototypes */
	class timer;
	class frame_subscriber;

	/* Providing timers */
public:
	using timer_handler_t = std::function<void()>;
	using frame_subscriber_handler_t = std::function<void(const ethernet_frame&)>;

	class timer_registration
	{
	public:
		friend provider;

	private:
		std::weak_ptr<provider> prov;
		timer *token;

		timer_registration (std::weak_ptr<provider> prov, timer *token);

	public:
		timer_registration();
		timer_registration(const timer_registration &) = delete;
		timer_registration(timer_registration &&);
		timer_registration& operator=(timer_registration&&);

		~timer_registration();
		void unregister();
	};

protected:
	timer_registration create_timer_registration (timer *token);

	struct timer
	{
		/* Handler function */
		timer_handler_t handler;

		/* Timer period in ms */
		uint32_t period;

		/* Last time the handler was called */
		linear_time last_called;

		timer (timer_handler_t handler, uint32_t period);

		double get_remaining_time(const linear_time &now);
	};

	std::list<timer> timers;

	/* Low level add- and removal of timers (for internal use only) */
	timer *add_timer(timer_handler_t handler, uint32_t period);
	void remove_timer(timer *token);

	virtual void unregister_timer(timer *token) = 0;

	/* Receiving ethernet frames with ethertype 0x88b6 */
public:
	class frame_subscriber_registration
	{
	public:
		friend provider;

	private:
		std::weak_ptr<provider> prov;
		frame_subscriber *token;

		frame_subscriber_registration (std::weak_ptr<provider> prov, frame_subscriber *token);

	public:
		frame_subscriber_registration();
		frame_subscriber_registration(const frame_subscriber_registration &) = delete;
		frame_subscriber_registration(frame_subscriber_registration &&);
		frame_subscriber_registration& operator=(frame_subscriber_registration &&);

		~frame_subscriber_registration();
		void unregister();
	};

protected:
	frame_subscriber_registration create_frame_subscriber_registration (
			frame_subscriber *token);

	struct frame_subscriber
	{
		/* Handler function */
		frame_subscriber_handler_t handler;
		frame_subscriber (frame_subscriber_handler_t handler);
	};

	std::shared_mutex frame_subscribers_m;
	std::list<frame_subscriber> frame_subscribers;

	virtual void unregister_frame_subscriber(frame_subscriber *token);

	provider();

public:
	virtual ~provider();

	/** Platform independent printf ... */
	virtual void printf(const char *fmt, ...) = 0;
	virtual void flush() = 0;

	/** Retrieve the time of a monotonic clock */
	virtual linear_time get_monotonic_time() = 0;

	/** Retrieve UTC */
	virtual calendar_time get_utc() = 0;

	/** Register a timer. If the returned object is destroyed, the timer is
	 * automatically unregistered. However it can also be unregistered before by
	 * calling `unregister()` on the timer_registration object.
	 * @param handler The handler function
	 * @param period The timer's period in ms
	 * @returns A timer_registration object that refers to this particular timer
	 *		registration */
	virtual timer_registration register_timer(timer_handler_t handler, uint32_t period) = 0;

	/** Retrieve the mac address of of the chosen interface of this computer. */
	virtual const mac_addr_t& get_own_mac_address () = 0;

	/** Send an ethernet frame.
	 * @raises An implementation specific exception in case of failure. */
	virtual void send_frame(const ethernet_frame &frame) = 0;

	/** Add a subscriber to receive frames
	 * @param handler The frame handler function
	 * @returns A `frame_subscriber` object that refers to this particular
	 * 		subscription */
	virtual frame_subscriber_registration add_frame_subscriber(
			frame_subscriber_handler_t handler);
};

}

/* String ocnversion functions outside that namespace */
std::ostream& operator<<(std::ostream& o, const system_services::linear_time &lt);

#endif /* __SYSTEM_SERVICES_H */
