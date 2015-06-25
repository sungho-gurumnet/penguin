#include <time.h>
#include <net/ether.h>
#include <net/ip.h>
#include <net/udp.h>
#include <util/map.h>

#define UDP_PORTS	".pn.udp.ports"
#define UDP_NEXT_PORT	".pn.udp.next_port"

uint16_t udp_port_alloc(NetworkInterface* ni) {
	Map* ports = ni_config_get(ni, UDP_PORTS);
	if(!ports) {
		ports = map_create(64, map_uint64_hash, map_uint64_equals, ni->pool);
		ni_config_put(ni, UDP_PORTS, ports);
	}
	
	uint16_t port = (uint16_t)(uintptr_t)ni_config_get(ni, UDP_NEXT_PORT);
	if(port < 49152)
		port = 49152;
	
	while(map_contains(ports, (void*)(uintptr_t)port)) {
		if(++port < 49152)
			port = 49152;
	}	
	
	map_put(ports, (void*)(uintptr_t)port, (void*)(uintptr_t)port);
	ni_config_put(ni, UDP_NEXT_PORT, (void*)(uintptr_t)(port + 1));
	
	return port;
}

void udp_port_free(NetworkInterface* ni, uint16_t port) {
	Map* ports = ni_config_get(ni, UDP_PORTS);
	if(!ports)
		return;
	
	map_remove(ports, (void*)(uintptr_t)port);
}

void udp_pack(Packet* packet, uint16_t udp_body_len) {
	Ether* ether = (Ether*)(packet->buffer + packet->start);
	IP* ip = (IP*)ether->payload;
	UDP* udp = (UDP*)ip->body;
	
	uint16_t udp_len = UDP_LEN + udp_body_len;
	udp->length = endian16(udp_len);
	udp->checksum = 0;	// TODO: Calc checksum
	
	ip_pack(packet, udp_len);
}
