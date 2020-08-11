#include <cmath>
#include <cstring>
#include <cstdarg>
#include <time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "errno_exception.h"
#include "linux_system_services.h"

using namespace std;

namespace system_services
{

class linux_provider_pi : public linux_provider
{
public:
	linux_provider_pi(const string &if_name)
		: linux_provider(if_name)
	{}
};

shared_ptr<linux_provider> linux_provider::create(const string &if_name)
{
	return make_shared<linux_provider_pi>(if_name);
}

void linux_provider::unregister_timer(timer *token)
{
	remove_timer(token);
}

linux_provider::linux_provider(const std::string &if_name)
	: provider()
{
	frame_socket = socket(AF_PACKET, SOCK_DGRAM, htons(0x88b6));
	if (frame_socket < 0)
		throw errno_exception("socket(AF_PACKET, SOCK_DGRAM, 0x88b6", errno);

	struct ifreq req;
	strncpy (req.ifr_name, if_name.c_str(), IFNAMSIZ);
	req.ifr_name[IFNAMSIZ - 1] = '\0';

	int ret = ioctl (frame_socket, SIOCGIFINDEX, &req);
	if (ret < 0)
	{
		int err = errno;
		close (frame_socket);
		throw errno_exception("ioctl(SIOCGIFINDEX)", err);
	}

	if_index = req.ifr_ifindex;

	ret = ioctl (frame_socket, SIOCGIFHWADDR, &req);
	if (ret < 0)
	{
		int err = errno;
		close (frame_socket);
		throw errno_exception("ioctl(SIOCGIFHWADDR)", err);
	}

	memcpy (own_mac_address, req.ifr_hwaddr.sa_data, 6);
}

linux_provider::~linux_provider()
{
	close(frame_socket);
}

void linux_provider::printf(const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vprintf (fmt, ap);
	va_end(ap);
}

void linux_provider::flush()
{
	fflush (stdout);
}

linear_time linux_provider::get_monotonic_time()
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		throw errno_exception("clock_gettime", errno);

	return linear_time(ts.tv_sec, ts.tv_nsec);
}

calendar_time linux_provider::get_utc()
{
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
		throw errno_exception("clock_gettime", errno);

	struct tm gct;
	gmtime_r (&ts.tv_sec, &gct);

	calendar_time ct;
	ct.year          = gct.tm_year + 1900;
	ct.day_of_year   = gct.tm_yday;
	ct.second_of_day = (uint32_t) gct.tm_hour * 3600 +
		               (uint32_t) gct.tm_min * 60 +
					   (uint32_t) gct.tm_sec;
	ct.nanosecond    = ts.tv_nsec;

	return ct;
}

linux_provider::timer_registration linux_provider::register_timer(
		timer_handler_t handler, uint32_t period)
{
	auto tim = add_timer(handler, period);
	tim->last_called = get_monotonic_time();
	return create_timer_registration (tim);
}

const mac_addr_t& linux_provider::get_own_mac_address()
{
	return own_mac_address;
}

void linux_provider::send_frame(const ethernet_frame &frame)
{
	struct sockaddr_ll addr = {
		.sll_family = AF_PACKET,
		.sll_protocol = htons(frame.ether_type),
		.sll_ifindex = if_index,
		.sll_hatype = 0,
		.sll_pkttype = 0,
		.sll_halen = 6
	};

	memcpy (addr.sll_addr, frame.dst, 6);

	auto ret = sendto (frame_socket, frame.data, frame.data_size, 0,
			(const sockaddr*) &addr, sizeof(addr));

	if (ret < 0)
		throw errno_exception("sendto", errno);
}

void linux_provider::main_loop()
{
	int epfd = epoll_create1(EPOLL_CLOEXEC);
	if (epfd < 0)
		throw errno_exception("epoll_create", errno);

	try
	{
		/* Add the packet socket to the epoll instance */
		struct epoll_event tmp_event;
		tmp_event.events = EPOLLIN;
		tmp_event.data.fd = frame_socket;

		if (epoll_ctl (epfd, EPOLL_CTL_ADD, frame_socket, &tmp_event) < 0)
			throw errno_exception("epoll_ctl (add frame_socket)", errno);

		while (true)
		{
			double delay = 60;

			bool finished = false;
			while (!finished)
			{
				finished = true;

				for (auto &tim : timers)
				{
					auto now = get_monotonic_time();
					auto remaining = tim.get_remaining_time(now);

					if (remaining <= 0)
					{
						tim.last_called = now;
						tim.handler();

						/* Assuming that only one timer expires within a round.
						 * */
						finished = false;
						break;
					}

					delay = remaining < delay ? remaining : delay;
				}
			}

			/* Sleep at most `delay` time. */
			int ep_delay = floor (delay * 1000);
			if (ep_delay == 0)
				ep_delay = 1;

			struct epoll_event event;

			int num = epoll_wait (epfd, &event, 1, ep_delay);

			if (num < 0)
			{
				throw errno_exception ("epoll_wait", errno);
			}
			else if (num > 0)
			{
				if (event.data.fd == frame_socket)
				{
					ethernet_frame frame;

					struct sockaddr_ll addr;
					socklen_t len = sizeof (addr);

					auto cnt = recvfrom (frame_socket, frame.data, sizeof (frame.data),
							0, (sockaddr*) &addr, &len);

					if (cnt < 0)
						throw errno_exception("recvfrom", errno);

					frame.data_size = cnt;

					memset (frame.dst, 0, sizeof (frame.dst));
					memcpy (frame.src, addr.sll_addr, 6);
					frame.ether_type = ntohs(addr.sll_protocol);

					{
						shared_lock lk(frame_subscribers_m);

						for (auto &subs : frame_subscribers)
							subs.handler ((const ethernet_frame) frame);
					}
				}
			}
		}
	}
	catch(...)
	{
		close (epfd);
	}
}

}
