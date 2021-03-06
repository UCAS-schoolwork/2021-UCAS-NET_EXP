#include "base.h"
#include "ether.h"
#include "stp.h"
#include "utils.h"
#include "log.h"
#include "mac.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void handle_packet(iface_info_t *iface, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	stp_port_t *p = iface->port;
	pthread_mutex_lock(&p->stp->lock);
	
	if (memcmp(eh->ether_dhost, eth_stp_addr, sizeof(*eth_stp_addr))!=0) {
		// TODO: forward this packet, if this lab has merged with 05-switch.
		// fprintf(stdout, "TODO: received non-stp packet, forward it.\n");
		if(port_is_valid(p)){
			log(DEBUG, "port %02d has recv\n",p->port_id & 0xFF);
			//log(DEBUG, "the dst mac address is " ETHER_STRING ".\n", ETHER_FMT(eh->ether_dhost));
			struct ether_header *eh = (struct ether_header *)packet;
			iface_info_t* dest = lookup_port(eh->ether_dhost);
			if(dest != NULL && port_is_valid(dest->port)){
				log(DEBUG, "send from port %02d\n",dest->port->port_id & 0xFF);
				iface_send_packet(dest, packet, len);
			}	
			else {
				log(DEBUG, "broatcast.\n");
				broadcast_packet(iface, packet, len);
			}
			insert_mac_port(eh->ether_shost, iface);
		}
	}
	else {
		stp_port_handle_packet(p, packet, len);
	}
	pthread_mutex_unlock(&p->stp->lock);
	free(packet);
}

// run user stack, receive packet on each interface, and handle those packet
// like normal switch
void ustack_run()
{
	struct sockaddr_ll addr;
	socklen_t addr_len = sizeof(addr);
	char buf[ETH_FRAME_LEN];
	int len;

	while (1) {
		int ready = poll(instance->fds, instance->nifs, -1);
		if (ready < 0) {
			// interrupted by SIGTERM, wait until this program EXIT
			while (1) sleep(1);
		}
		else if (ready == 0)
			continue;

		for (int i = 0; i < instance->nifs; i++) {
			if (instance->fds[i].revents & POLLIN) {
				len = recvfrom(instance->fds[i].fd, buf, ETH_FRAME_LEN, 0, \
						(struct sockaddr*)&addr, &addr_len);
				if (len <= 0) {
					log(ERROR, "receive packet error: %s", strerror(errno));
				}
				else if (addr.sll_pkttype == PACKET_OUTGOING) {
					// XXX: Linux raw socket will capture both incoming and
					// outgoing packets, while we only care about the incoming ones.

					// log(DEBUG, "received packet which is sent from the "
					// 		"interface itself, drop it.");
				}
				else {
					iface_info_t *iface = fd_to_iface(instance->fds[i].fd);
					if (!iface) 
						continue;

					char *packet = malloc(len);
					if (!packet) {
						log(ERROR, "malloc failed when receiving packet.");
						continue;
					}
					memcpy(packet, buf, len);
					handle_packet(iface, packet, len);
				}
			}
		}
	}
}

int main(int argc, const char **argv)
{
	if (getuid() && geteuid()) {
		printf("Permission denied, should be superuser!\n");
		exit(1);
	}

	init_ustack();

	stp_init(&instance->iface_list);
	init_mac_port_table();
	ustack_run();

	return 0;
}
