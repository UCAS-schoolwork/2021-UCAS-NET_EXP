#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	//fprintf(stdout, "TODO: determine the direction of this packet.\n");
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	rt_entry_t *sentry = longest_prefix_match(ntohl(ihr->saddr));
	rt_entry_t *dentry = longest_prefix_match(ntohl(ihr->daddr));
	if(sentry&&sentry->iface==nat.internal_iface && dentry&&dentry->iface==nat.external_iface)
		return DIR_OUT;
	if(sentry&&sentry->iface==nat.external_iface && ntohl(ihr->daddr)==nat.external_iface->ip)
		return DIR_IN;
	return DIR_INVALID;
}

static inline int hash_nat(u32 rm_ip,u16 rm_port)
{
	return (rm_ip*33+rm_port) % HASH_NAT;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	//fprintf(stdout, "TODO: do translation for this packet.\n");
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	struct tcphdr *thr = packet_to_tcp_hdr(packet);
	u32 rm_ip = dir==DIR_IN ? ntohl(ihr->saddr) : ntohl(ihr->daddr);
	u16 rm_port = dir==DIR_IN ? ntohs(thr->sport) : ntohs(thr->dport);
	u32 trans_ip = dir==DIR_IN ? ntohl(ihr->daddr) : ntohl(ihr->saddr);
	u16 trans_port = dir==DIR_IN ? ntohs(thr->dport) : ntohs(thr->sport);
	pthread_mutex_lock(&nat.lock);
	int id = hash_nat(rm_ip,rm_port);
	int exist = 0, found = 0;
	struct nat_mapping *entry = NULL;
	if(dir==DIR_OUT){
		list_for_each_entry(entry,&nat.nat_mapping_list[id],list){
			if(entry->remote_ip == rm_ip && entry->remote_port == rm_port
				&& entry->internal_ip == trans_ip && entry->internal_port == trans_port){
					exist = 1;
					break;
			}
		}
		if(!exist){
			entry = malloc(sizeof(struct nat_mapping));
			entry->remote_ip = rm_ip, entry->remote_port = rm_port;
			entry->internal_ip = trans_ip, entry->internal_port = trans_port;
			entry->external_ip = nat.external_iface->ip;
			for(int i=NAT_PORT_MIN;i<NAT_PORT_MAX;i++){
				if(!nat.assigned_ports[i]){
					nat.assigned_ports[i] = 1;
					entry->external_port = i;
					found = 1;
					break;
				}
			}
			if(!found){ free(packet); free(entry); return; }
			list_add_tail(&entry->list,&nat.nat_mapping_list[id]);
		}
		ihr->saddr = htonl(entry->external_ip);
		thr->sport = htons(entry->external_port);
		entry->conn.internal_fin = thr->flags & TCP_FIN;
		entry->conn.internal_ack = thr->flags & TCP_ACK;
		entry->conn.internal_seq_end = tcp_seq_end(ihr,thr);
	}
	else{ //DIR_IN
		list_for_each_entry(entry,&nat.nat_mapping_list[id],list){
			if(entry->remote_ip == rm_ip && entry->remote_port == rm_port
				&& entry->external_ip == trans_ip && entry->external_port == trans_port){
					exist = 1;
					break;
			}
		}
		if(!exist){
			struct dnat_rule *rule = NULL;
			list_for_each_entry(rule,&nat.rules,list){
				if(rule->external_ip==trans_ip && rule->external_port==trans_port){
					entry = malloc(sizeof(struct nat_mapping));
					entry->remote_ip = rm_ip, entry->remote_port = rm_port;
					entry->external_ip = trans_ip, entry->external_port = trans_port;
					entry->internal_ip = rule->internal_ip;
					entry->internal_port = rule->internal_port;
					list_add_tail(&entry->list,&nat.nat_mapping_list[id]);
					found = 1;
					break;
				}
			}
			if(!found){ free(packet); free(entry); return; }
		}
		ihr->daddr = htonl(entry->internal_ip);
		thr->dport = htons(entry->internal_port);
		entry->conn.external_fin = thr->flags & TCP_FIN;
		entry->conn.external_ack = thr->flags & TCP_ACK;
		entry->conn.external_seq_end = tcp_seq_end(ihr,thr);
	}
	entry->update_time = time(NULL);
	pthread_mutex_unlock(&nat.lock);
	thr->checksum = tcp_checksum(ihr,thr);
	ihr->checksum = ip_checksum(ihr);
	ip_send_packet(packet,len);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		//fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		sleep(1);
		pthread_mutex_lock(&nat.lock);
		struct nat_mapping *entry, *q;
		for(int i=0;i<HASH_NAT;i++){
			list_for_each_entry_safe(entry,q,&nat.nat_mapping_list[i],list){
				if(is_flow_finished(&entry->conn)
						|| time(NULL)-entry->update_time >= TCP_ESTABLISHED_TIMEOUT){
					list_delete_entry(&entry->list);
					nat.assigned_ports[entry->external_port] = 0;
					free(entry);
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
	}
	return NULL;
}

int parse_config(const char *filename)
{
	//fprintf(stdout, "TODO: parse config file, including i-iface, e-iface (and dnat-rules if existing).\n");
	FILE *fd = fopen(filename,"r");
	if(!fd) return -1;
	#define MLINE 80
	char buf[MLINE];
	for(int i=0;i<2;i++){
		fgets(buf,MLINE,fd);
		char *face = strtok(buf," ");
		face = strtok(NULL,"\n");
		iface_info_t *iface = if_name_to_iface(face);
		if(!iface) {log(ERROR,"get face err"); exit(0);}
		if(i==0)
			nat.internal_iface = iface;
		else 
			nat.external_iface = iface;		
	}

	int t[10],c;
	while(1){
		while((c=fgetc(fd))!=':'&&c!=EOF);
		if(c==EOF) return 0;
		if(fscanf(fd," %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d",t,t+1,t+2,t+3,t+4,t+5,t+6,t+7,t+8,t+9)==10)
		{
			unsigned ip1 = (((((t[0]<<8)+t[1])<<8)+t[2])<<8)+t[3];
			unsigned port1 = t[4];
			unsigned ip2 = (((((t[5]<<8)+t[6])<<8)+t[7])<<8)+t[8];
			unsigned port2 = t[9];
			struct dnat_rule * rule = malloc(sizeof(struct dnat_rule));
			rule->external_ip = ip1;
			rule->internal_ip = ip2;
			rule->external_port = (u16)port1;
			rule->internal_port = (u16)port2;
			list_add_tail(&rule->list ,&nat.rules);
		}
	}

	return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	parse_config(config_file);
	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit()
{
	//fprintf(stdout, "TODO: release all resources allocated.\n");
}
