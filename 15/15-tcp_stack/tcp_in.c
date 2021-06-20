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
	pthread_mutex_lock(&tsk->wait_send->lock);
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_with_lock(tsk->wait_send);
	pthread_mutex_unlock(&tsk->wait_send->lock);
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

void handle_tcp_data(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if(!cb->pl_len) return;
	pthread_mutex_lock(&tsk->wait_recv->lock);
	if(ring_buffer_free(tsk->rcv_buf) < cb->pl_len)
		log(DEBUG,"RECV BUFFER FULL. DROPED.");
	else{
		write_ring_buffer(tsk->rcv_buf,cb->payload,cb->pl_len);
		wake_with_lock(tsk->wait_recv);
	}
	pthread_mutex_unlock(&tsk->wait_recv->lock);
}

static inline int update_tsk(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	tsk->rcv_nxt = cb->seq_end;
	if(cb->flags & TCP_ACK)
		tsk->snd_una = max(tsk->snd_una,cb->ack);
	tcp_update_window_safe(tsk,cb);
	if(tsk->state != TCP_LISTEN && less_than_32b(cb->seq,cb->seq_end))
		tcp_send_control_packet(tsk,TCP_ACK);

	return 0;
}

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if(tsk->state==TCP_LISTEN) return 1;
	if(tsk->state==TCP_SYN_SENT || tsk->state == TCP_LAST_ACK) {
		if(cb->ack == tsk->snd_nxt) return 1;
		log(ERROR,"SYN/FIN SEQ ERROR.");
		return 0;
	}
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	// TODO: HANDLE PKT OUT OF ORDER
	if(!less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)){
		log(ERROR, "Dup pkt.DROP.");
		return 0;
	}
	if (!less_than_32b(cb->seq, rcv_end)) {
		log(ERROR, "INVALID SEQ.");
		return 0;
	}
	if (!less_or_equal_32b(cb->seq_end, rcv_end)) {
		log(ERROR, "PKT TOO BIG. DROP.");
		return 0;
	}
	return 1;
}

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if(!tsk) return;
	if(!is_tcp_seq_valid(tsk,cb)) return;
	if(cb->flags & TCP_RST){
		tsk->state = TCP_CLOSED;
		tcp_unhash(tsk);
		tcp_bind_unhash(tsk);
		return;
	}
	update_tsk(tsk,cb);
	if(tsk->state == TCP_ESTABLISHED || tsk->state == TCP_SYN_RECV || tsk->state == TCP_FIN_WAIT_1 || tsk->state == TCP_FIN_WAIT_2)
		handle_tcp_data(tsk,cb);

	if(tsk->state == TCP_LISTEN && cb->flags & TCP_SYN){
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
	else if(tsk->state == TCP_SYN_SENT && (cb->flags & (TCP_ACK|TCP_SYN)) == (TCP_ACK|TCP_SYN)){
		tsk->state = TCP_ESTABLISHED;
		//tcp_send_control_packet(tsk,TCP_ACK);
		wake_up(tsk->wait_connect);
	}
	else if(tsk->state == TCP_SYN_RECV && cb->flags & TCP_ACK){
		struct tcp_sock *psk = tsk->parent;
		pthread_mutex_lock(&psk->wait_accept->lock);
		if(!tcp_sock_accept_queue_full(psk)){
			tsk->state = TCP_ESTABLISHED;
			tcp_sock_accept_enqueue(tsk);
			wake_with_lock(psk->wait_accept);
		}
		pthread_mutex_unlock(&psk->wait_accept->lock);
	}
	else if(tsk->state == TCP_ESTABLISHED && cb->flags & TCP_FIN){
		tsk->state = TCP_CLOSE_WAIT;
		//tcp_send_control_packet(tsk,TCP_ACK);
		wake_up(tsk->wait_recv);
	}
	else if(tsk->state == TCP_FIN_WAIT_1 && cb->flags & TCP_ACK){
		tsk->state = TCP_FIN_WAIT_2;
		if(cb->flags & TCP_FIN)
			goto JUMP;
	}
	else if(tsk->state == TCP_FIN_WAIT_2 && (cb->flags & (TCP_ACK|TCP_FIN)) == (TCP_ACK|TCP_FIN)){
JUMP:
		tsk->state = TCP_TIME_WAIT;
		//tcp_send_control_packet(tsk,TCP_ACK);
		tcp_set_timewait_timer(tsk);
	}
	else if(tsk->state == TCP_LAST_ACK && cb->flags & (TCP_ACK)){
		tsk->state = TCP_CLOSED;
		tcp_unhash(tsk);
	}
	
	return;
}
