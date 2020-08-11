#include <cstring>
#include <arpa/inet.h>
#include "protocol.h"

using namespace std;


ethernet_frame time_signal_pulse::to_frame() const
{
	ethernet_frame frame;
	memset (frame.dst, 0xff, sizeof(frame.dst));
	memset (frame.src, 0, sizeof(frame.dst));
	frame.ether_type = 0x88b6;

	*(uint16_t*) (frame.data + 0) = htons(0x0133);
	*(uint16_t*) (frame.data + 2) = htons(year);
	*(uint16_t*) (frame.data + 4) = htons(day);
	*(uint32_t*) (frame.data + 6) = htonl(second);
	*(uint32_t*) (frame.data + 10) = htonl(nanosecond);

	frame.data_size = 14;

	return frame;
}

optional<time_signal_pulse> time_signal_pulse::from_frame (const ethernet_frame &frame)
{
	if (frame.data_size < 14 || frame.ether_type != 0x88b6)
		return nullopt;

	if (ntohs(*(uint16_t*)  (frame.data + 0)) != 0x0133)
		return nullopt;

	time_signal_pulse pulse;

	memcpy (pulse.src, frame.src, sizeof(frame.src));
	pulse.year = ntohs (*(uint16_t*) (frame.data + 2));
	pulse.day = ntohs (*(uint16_t*) (frame.data + 4));
	pulse.second = ntohl (*(uint32_t*) (frame.data + 6));
	pulse.nanosecond = ntohl (*(uint32_t*) (frame.data + 10));

	return pulse;
}
