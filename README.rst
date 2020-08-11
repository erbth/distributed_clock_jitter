A software system to measure clock jitter of multiple computers in a network
===

Principle of the protocol:
---

From all nodes that have the service started the node with minimum mac address
sends a time signal once a second, which carries the current time at that node.
All other nodes receive those time signals and can calculate their local clocks'
deviation at the very point in time when the signal arrives. Of course latencies
across the network are contained in this deviation. However this first version
of the protocol / system shall be simple and give overall results with, say 10ms
precision. Therefore a typical twisted-pair ethernet along with an OS that
handles packets within not-too-long timebounds should suffice.

The transport layer of the protocol is chosen to be Ethernet II to better reach
realtime bounds, and because I always wanted to try that. It uses the ethertype
0x88b6.

If a node does not receive time signals within 1 second, it starts to send time
signals on its own. However as soon as it receives a time signal from a node
with lower mac address (interpret the mac address as unsigned little endian
integer) it shall stop sending again.

Protocol
---

Time signal messages (excluding preamble, SFD and trailer):

+-----+-----+--------+--------+----------------------+--------------------+----------------------+-----------------------------+
| dst | src | 0x88b6 | 0x0133 | 2 byte unsigned year | 2 byte day in year | 4 byte second of day | 4 byte nanosecond of second |
+-----+-----+--------+--------+----------------------+--------------------+----------------------+-----------------------------+

Length: 28 byte (+ 4 byte FCS)
