#include <algorithm>
#include "system_services.h"

using namespace std;

namespace system_services
{

provider::timer_registration::timer_registration()
	: token(nullptr)
{
}

provider::timer_registration::timer_registration(
		weak_ptr<provider> prov, timer *token)
	: prov(prov), token(token)
{
}

provider::timer_registration::timer_registration(timer_registration &&o)
	: prov(move(o.prov)), token(o.token)
{
	o.token = nullptr;
}

provider::timer_registration& provider::timer_registration::operator=(timer_registration&& o)
{
	unregister();

	prov = move(o.prov);
	token = o.token;
	o.token = nullptr;

	return *this;
}

provider::timer_registration::~timer_registration()
{
	unregister();
}

void provider::timer_registration::unregister()
{
	if (!token)
		return;

	try
	{
		auto s = prov.lock();
		s->unregister_timer (token);

		/* Prevent that the timer is unregistered more than once (possibly
		 * unregistering another timer, which got the same, reused token) */
		token = nullptr;
	}
	catch (bad_weak_ptr&)
	{
	}
}

provider::timer_registration provider::create_timer_registration (timer *token)
{
	return timer_registration (shared_from_this(), token);
}


provider::timer::timer (timer_handler_t handler, uint32_t period)
	: handler(handler), period(period)
{
}

double provider::timer::get_remaining_time(const linear_time &now)
{
	return period / 1000. - (now - last_called);
}

provider::timer *provider::add_timer(timer_handler_t handler, uint32_t period)
{
	timers.push_back(timer (handler, period));
	return &timers.back();
}

void provider::remove_timer(timer *token)
{
	auto i = find_if(
			timers.begin(),
			timers.end(),
			[token](auto &tim) { return &tim == token; });

	if (i != timers.end())
		timers.erase(i);
}


provider::frame_subscriber_registration::frame_subscriber_registration()
	: token(nullptr)
{
}

provider::frame_subscriber_registration::frame_subscriber_registration(
		weak_ptr<provider> prov, frame_subscriber *token)
	: prov(prov), token(token)
{
}

provider::frame_subscriber_registration::frame_subscriber_registration(
		frame_subscriber_registration &&o)
	: prov(move(o.prov)), token(o.token)
{
	o.token = nullptr;
}

provider::frame_subscriber_registration& provider::frame_subscriber_registration::operator=(
		frame_subscriber_registration &&o)
{
	unregister();

	prov = move(o.prov);
	token = o.token;
	o.token = nullptr;

	return *this;
}

provider::frame_subscriber_registration::~frame_subscriber_registration()
{
	unregister();
}

void provider::frame_subscriber_registration::unregister()
{
	if (!token)
		return;

	try
	{
		auto s = prov.lock();
		s->unregister_frame_subscriber (token);

		/* Prevent that the frame subscriber is unregistered more than once
		 * (possibly unregistering another frame subscriber, which got the same,
		 * reused token) */
		token = nullptr;
	}
	catch (bad_weak_ptr&)
	{
	}
}

provider::frame_subscriber_registration provider::create_frame_subscriber_registration (
		frame_subscriber *token)
{
	return frame_subscriber_registration (shared_from_this(), token);
}


provider::frame_subscriber::frame_subscriber (frame_subscriber_handler_t handler)
	: handler(handler)
{
}

void provider::unregister_frame_subscriber(frame_subscriber *token)
{
	unique_lock lk(frame_subscribers_m);

	auto i = find_if(
			frame_subscribers.begin(),
			frame_subscribers.end(),
			[token](auto &subs) { return &subs == token; });

	if (i != frame_subscribers.end())
		frame_subscribers.erase(i);
}


provider::provider()
{
}

provider::~provider()
{
}


provider::frame_subscriber_registration provider::add_frame_subscriber(
		frame_subscriber_handler_t handler)
{
	unique_lock lk(frame_subscribers_m);

	frame_subscribers.push_back (frame_subscriber(handler));
	return create_frame_subscriber_registration(&frame_subscribers.back());
}

}


std::ostream& operator<<(std::ostream& o, const system_services::linear_time &lt)
{
	o << lt.seconds << '.';
	o.width(9);
	o.fill('0');
	o << lt.nanoseconds;
	return o;
}
