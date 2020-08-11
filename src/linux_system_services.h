#ifndef __LINUX_SYSTEM_SERVICES_H
#define __LINUX_SYSTEM_SERVICES_H

#include "system_services.h"

namespace system_services
{

class linux_provider : public provider
{
protected:
	void unregister_timer(timer *token) override;

	/* Sending- and receiving frames */
	std::string if_name;
	int if_index;
	int frame_socket = -1;
	mac_addr_t own_mac_address;

	linux_provider(const std::string &if_name);

public:
	static std::shared_ptr<linux_provider> create(const std::string &if_name);

	virtual ~linux_provider();

	void printf(const char *fmt, ...) override;
	void flush() override;

	linear_time get_monotonic_time() override;
	calendar_time get_utc() override;

	timer_registration register_timer(timer_handler_t handler, uint32_t period) override;

	const mac_addr_t& get_own_mac_address () override;

	void send_frame(const ethernet_frame &frame) override;

	void main_loop();
};

}

#endif /* __LINUX_SYSTEM_SERVICES_H */
