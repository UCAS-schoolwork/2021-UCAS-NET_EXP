#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if(tsk->state==TCP_LISTEN) return 1;
	if(tsk->state==TCP_SYN_SENT || tsk->state == TCP_LAST_ACK) {
		if(cb->ack == tsk->snd_nxt) return 1;
		return 0;
	}
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	// recvd < seq_end && seq < exp_end
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) 
		return 1;
	log(ERROR, "received packet with invalid seq, drop it.");
	return 0;
	
}

static inline int update_tsk(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	tsk->rcv_nxt = cb->seq_end;
	tsk->snd_una = cb->ack;
	return 0;
}
// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	// TODO: check if the packet is valid.
	if(!tsk) return;
	struct tcphdr *thr = packet_to_tcp_hdr(packet);
	if(!is_tcp_seq_valid(tsk,cb)) return;
	update_tsk(tsk,cb);
	if(thr->flags & TCP_RST){
		tsk->state = TCP_CLOSED;
		tcp_unhash(tsk);
		tcp_bind_unhash(tsk);
	}
	else if(tsk->state == TCP_LISTEN && thr->flags & TCP_SYN){
		struct tcp_sock *csk = alloc_tcp_sock();
		csk->sk_sip = cb->daddr;
		csk->sk_sport = cb->dport;
		csk->sk_dip = cb->saddr;
		csk->sk_dport = cb->sport;
		csk->rcv_nxt = cb->seq_end;
		csk->parent = tsk;
		csk->state = TCP_SYN_RECV;
		pthread_mutex_lock(&tsk->wait_accept->lock);
		tcp_sock_listen_enqueue(csk);
		pthread_mutex_unlock(&tsk->wait_accept->lock);
		tcp_hash(csk);
		tcp_send_control_packet(csk,TCP_SYN|TCP_ACK);
	}
	else if(tsk->state == TCP_SYN_SENT && thr->flags & (TCP_ACK|TCP_SYN)){
		tsk->state = TCP_ESTABLISHED;
		tcp_send_control_packet(tsk,TCP_ACK);
		wake_up(tsk->wait_connect);
	}
	else if(tsk->state == TCP_SYN_RECV && thr->flags & TCP_ACK){
		struct tcp_sock *psk = tsk->parent;
		pthread_mutex_lock(&psk->wait_accept->lock);
		if(!tcp_sock_accept_queue_full(psk)){
			tsk->state = TCP_ESTABLISHED;
			tcp_sock_accept_enqueue(tsk);
			wake_with_lock(psk->wait_accept);
		}
		pthread_mutex_unlock(&psk->wait_accept->lock);
	}
	else if(tsk->state == TCP_ESTABLISHED && thr->flags & TCP_FIN){
		tsk->state = TCP_CLOSE_WAIT;
		tcp_send_control_packet(tsk,TCP_ACK);
		usleep(100000);
		tcp_sock_close(tsk);
	}
	else if(tsk->state == TCP_FIN_WAIT_1 && thr->flags & TCP_ACK){
		tsk->state = TCP_FIN_WAIT_2;
		if(thr->flags & TCP_FIN)
			tsk->state = TCP_TIME_WAIT;
	}
	else if(tsk->state == TCP_FIN_WAIT_2 && thr->flags & (TCP_FIN|TCP_ACK)){
		tsk->state = TCP_TIME_WAIT;
		tcp_send_control_packet(tsk,TCP_ACK);
		tcp_set_timewait_timer(tsk);
	}
	else if(tsk->state == TCP_LAST_ACK && thr->flags & (TCP_ACK)){
		tsk->state = TCP_CLOSED;
		tcp_unhash(tsk);
	}

	return;
}
