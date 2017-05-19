#include <timer.h>
#include <_malloc.h>
#include <net/interface.h>
#include <net/ether.h>
#include <net/arp.h>
#include <util/map.h>

#define ARP_TABLE		"net.arp.arptable"
#define ARP_TABLE_GC		"net.arp.arptable.gc"
#define ARP_ENTITY_MAX_COUNT	16
#define ARP_TIMEOUT		14400 * 1000000;	// 4 hours  (us)

#define GC_INTERVAL	(10 * 1000000)	// 10 secs

typedef struct _ARPEntity {
	uint64_t	mac;
	uint32_t	addr;
	uint64_t	timeout;

	bool		dynamic;
	int		next;
} ARPEntity;

typedef struct ARPTable {
	uint16_t	arp_entity_count;
	ARPEntity 	arp_entity[ARP_ENTITY_MAX_COUNT];
} ARPTable;

//TODO When load application in kernel, do set arp_table.
inline ARPTable* get_arp_table(NIC* nic) {
	//TODO Add cache
	static NIC* _nic = NULL;
	static ARPTable* arp_table = NULL;

	if(_nic == nic)
		return arp_table;

	int32_t arp_table_key = nic_config_key(nic, ARP_TABLE);
	if(arp_table_key <= 0) {
		arp_table_key = nic_config_alloc(nic, ARP_TABLE, sizeof(ARPTable));
		if(arp_table_key <= 0)
			return NULL;

		arp_table = nic_config_get(nic, arp_table_key);
		memset(arp_table, 0, sizeof(ARPTable));
	}

	_nic = nic;
	arp_table = nic_config_get(nic, arp_table_key);

	return arp_table;
}

// static bool arp_table_destroy(NIC* nic) {
// 	int32_t arp_table_key = nic_config_key(nic, ARP_TABLE);
// 	if(arp_table_key <= 0)
// 		return false;
//
// 	nic_config_free(nic, arp_table_key);
// 	return true;
// }

inline bool arp_table_update(ARPTable* arp_table, uint64_t mac, uint32_t addr, bool dynamic) {
	//TODO Check
	uint64_t current = timer_us();
	for(int i = 0; i < arp_table->arp_entity_count; i++) {
		// Update
		if(arp_table->arp_entity[i].addr == addr) {
			if(dynamic) {
			} else {
				arp_table->arp_entity[i].mac = mac;
				arp_table->arp_entity[i].timeout = current + ARP_TIMEOUT;
			}
			return true;
		}
	}

	//TODO add new entry
	return true;
}

inline bool arp_table_remove(ARPTable* arp_table, uint32_t addr) {
	return true;
}

uint64_t arp_get_mac(NIC* nic, uint32_t destination, uint32_t source) {
	Map* arp_table = nic_config_get(nic, ARP_TABLE);
	if(!arp_table) {
		arp_request(nic, destination, source);
		return 0xffffffffffff;
	}

	ARPEntity* entity = map_get(arp_table, (void*)(uintptr_t)destination);
	if(!entity) {
		arp_request(nic, destination, source);
		return 0xffffffffffff;
	}

	return entity->mac;
}

uint32_t arp_get_ip(NIC* nic, uint64_t mac) {
	ARPTable* arp_table = get_arp_table(nic);


	return 0;
}

void arp_set_reply_handler() {
}

bool arp_process(NIC* nic, Packet* packet) {
	Ether* ether = (Ether*)(packet->buffer + packet->start);
	if(endian16(ether->type) != ETHER_TYPE_ARP)
		return false;

	ARP* arp = (ARP*)ether->payload;

	uint32_t addr = endian32(arp->tpa);

	if(!interface_get(nic, addr))
		return false;

// 	if(!nic_ip_get(nic, addr)) //Drop?
// 		return false;

	ARPTable* arp_table = get_arp_table(nic); //Drop?
	if(!arp_table)
		return false;

	// GC
// 	uint64_t gc_time = (uintptr_t)nic_config_get(packet->nic, ARP_TABLE_GC);
// 	if(gc_time == 0 && !nic_config_contains(packet->nic, ARP_TABLE_GC)) {
// 		gc_time = current + GC_INTERVAL;
// 		if(!nic_config_put(packet->nic, ARP_TABLE_GC, (void*)(uintptr_t)gc_time))
// 			return false;
// 	}

// 	if(gc_time < current) {
// 		MapIterator iter;
// 		map_iterator_init(&iter, arp_table);
// 		while(map_iterator_has_next(&iter)) {
// 			MapEntry* entry = map_iterator_next(&iter);
// 			if(((ARPEntity*)entry->data)->timeout < current) {
// 				__free(entry->data, packet->nic->pool);
// 				map_iterator_remove(&iter);
// 			}
// 		}
//
// 		gc_time = current + GC_INTERVAL;
// 		if(!nic_config_put(packet->nic, ARP_TABLE_GC, (void*)(uintptr_t)gc_time))
// 			return false;
// 	}

	switch(endian16(arp->operation)) {
		case 1:	// Request
			ether->dmac = ether->smac;
			ether->smac = endian48(nic->mac);
			arp->operation = endian16(2);
			arp->tha = arp->sha;
			arp->tpa = arp->spa;
			arp->sha = ether->smac;
			arp->spa = endian32(addr);

			nic_tx(nic, packet);

			//TODO map put

			return true;
		case 2: // Reply
			;
			uint64_t smac = endian48(arp->sha);
			uint32_t sip = endian32(arp->spa);
			//TODO map_put
// 			entity->mac = smac;
// 			entity->timeout = current + ARP_TIMEOUT;
			nic_free(packet);
			return true;
	}

	return false;
}

bool arp_request(NIC* nic, uint32_t destination, uint32_t source) {
	if(!interface_get(nic, source))
		return false;

	Packet* packet = nic_alloc(nic, sizeof(Ether) + sizeof(ARP));
	if(!packet)
		return false;

	Ether* ether = (Ether*)(packet->buffer + packet->start);
	ether->dmac = endian48(0xffffffffffff);
	ether->smac = endian48(nic->mac);
	ether->type = endian16(ETHER_TYPE_ARP);

	ARP* arp = (ARP*)ether->payload;
	arp->htype = endian16(1);
	arp->ptype = endian16(0x0800);
	arp->hlen = endian8(6);
	arp->plen = endian8(4);
	arp->operation = endian16(1);
	arp->sha = endian48(nic->mac);
	arp->spa = endian32(source);
	arp->tha = endian48(0);
	arp->tpa = endian32(destination);

	packet->end = packet->start + sizeof(Ether) + sizeof(ARP);

	return nic_tx(nic, packet);
}

bool arp_announce(NIC* nic, uint32_t source) {
	if(!interface_get(nic, source))
		return false;

	Packet* packet = nic_alloc(nic, sizeof(Ether) + sizeof(ARP));
	if(!packet)
		return false;

	Ether* ether = (Ether*)(packet->buffer + packet->start);
	ether->dmac = endian48(0xffffffffffff);
	ether->smac = endian48(nic->mac);
	ether->type = endian16(ETHER_TYPE_ARP);

	ARP* arp = (ARP*)ether->payload;
	arp->htype = endian16(1);
	arp->ptype = endian16(0x0800);
	arp->hlen = endian8(6);
	arp->plen = endian8(4);
	arp->operation = endian16(2);
	arp->sha = endian48(nic->mac);
	arp->spa = endian32(source);
	arp->tha = endian48(0);
	arp->tpa = endian32(source);

	packet->end = packet->start + sizeof(Ether) + sizeof(ARP);

	return nic_tx(nic, packet);
}

void arp_pack(Packet* packet) {
	Ether* ether = (Ether*)(packet->buffer + packet->start);
	ARP* arp = (ARP*)ether->payload;

	packet->end = ((void*)arp + sizeof(ARP)) - (void*)packet->buffer;
}