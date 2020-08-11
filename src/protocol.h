#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <optional>
#include <cstdint>

using mac_addr_t = unsigned char[6];

/** An Ethernet II (IEEE 802.3) frame along with metadata */
class ethernet_frame
{
public:
	mac_addr_t dst;
	mac_addr_t src;
	uint16_t ether_type;
	unsigned char data[46];
	size_t data_size = 0;
};

class time_signal_pulse
{
public:
	mac_addr_t src {};
	uint16_t year {};
	uint16_t day {};
	uint32_t second {};
	uint32_t nanosecond {};

	/** Serialize the attributes into an ethernet frame. The source address is
	 * left as 00:00:00:00:00:00.
	 * @returns The serialized ethernet_frame */
	ethernet_frame to_frame() const;

	/** De-serialize a time signal pulse captured from 'the wire'
	 * @returns A time_signal_pulse or nullopt if deserializing failed. */
	static std::optional<time_signal_pulse> from_frame(const ethernet_frame &frame);
};

#endif /* __PROTOCOL_H */
