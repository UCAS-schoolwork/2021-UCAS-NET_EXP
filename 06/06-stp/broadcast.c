#include "base.h"
#include <stdio.h>
#include "stp.h"
#include "log.h"

// XXX ifaces are stored in instace->iface_list
extern ustack_t *instance;

extern void iface_send_packet(iface_info_t *iface, const char *packet, int len);

void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	// TODO: broadcast packet 
	// fprintf(stdout, "TODO: broadcast packet.\n");
	iface_info_t *iface_n = NULL;
	list_for_each_entry(iface_n, &instance->iface_list, list) {
		if (iface_n->fd != iface->fd && port_is_valid(iface_n->port)) {
			log(DEBUG, "bsend from port %02d\n",iface_n->port->port_id & 0xFF);
			iface_send_packet(iface_n, packet, len);
		}
			
	}
}
