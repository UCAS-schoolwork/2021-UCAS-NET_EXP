#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"
#include "list.h"

#include <stdlib.h>

static inline unsigned long rdtsc(void)
{
	unsigned long tickl,tickh;
	asm volatile   (
			"rdtsc\n\t"
			:"=a"(tickl),"=d"(tickh));
	return ((unsigned long )tickh<<32)|tickl;
}
#define MAXT 1000000
int utime[MAXT];
int cwnds[MAXT];
int idx=0;
#define MHZ 1990
void record_cwnd(struct tcp_sock *tsk)
{
	static unsigned long first = 0;
	if(idx==0){
		first = rdtsc();
		utime[idx] = 0;
		cwnds[idx++] = tsk->cwnd;
	}
	else if(idx<MAXT){
		unsigned long tick = rdtsc();
		utime[idx] = (tick-first)/MHZ;
		if(utime[idx]==utime[idx-1]) return;
		cwnds[idx++] = tsk->cwnd;
	}
}
void write_record()
{
	FILE *fp = fopen("cwnd.dat","w");
	for(int i=0;i<idx;i++){
		fprintf(fp,"%d %d\n",utime[i],cwnds[i]);
	}
	fclose(fp);
}
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	pthread_mutex_lock(&tsk->wait_send->lock);
	tsk->adv_wnd = cb->rwnd;
	tsk->snd_wnd = min(tsk->adv_wnd,tsk->cwnd*MSS);
	if(tcp_is_allow_to_send(tsk))
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

void tcp_ofo_buf_append(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	assert((cb->flags&TCP_FIN)==0);
	tcp_ofo_buf_entry_t *entry,*q, *new_entry;
	list_for_each_entry_safe(entry,q,&tsk->rcv_ofo_buf,list){
		if(less_or_equal_32b(cb->seq_end,entry->seq + entry->len)){
			if(greater_or_equal_32b(cb->seq,entry->seq))
				return;
			break;
		}
	}
	new_entry = malloc(sizeof(tcp_ofo_buf_entry_t));
	new_entry->seq = cb->seq;
	new_entry->len = cb->pl_len;
	new_entry->data = malloc(cb->pl_len);
	//fprintf(stdout,"OFO Append Seq:%u-%u",new_entry->seq,new_entry->seq+new_entry->len);
	memcpy(new_entry->data,cb->payload,new_entry->len);
	list_add_tail(&new_entry->list,entry->list.prev);
}
void tcp_check_ofo_buf(struct tcp_sock *tsk)
{
	tcp_ofo_buf_entry_t *entry,*q;
	list_for_each_entry_safe(entry,q,&tsk->rcv_ofo_buf,list){
		if(less_or_equal_32b(entry->seq+entry->len,tsk->rcv_nxt)){
			list_delete_entry(&entry->list);
			free(entry->data);
			free(entry);
		}
		else if(greater_or_equal_32b(tsk->rcv_nxt,entry->seq)){
			int len = (int)(entry->seq+entry->len-tsk->rcv_nxt);
			//fprintf(stdout,"OFO Read Seq:%u-%u",entry->seq+entry->len-len,entry->seq+entry->len);
			write_ring_buffer(tsk->rcv_buf,entry->data+entry->len-len,len);
			tsk->rcv_nxt += len;
			list_delete_entry(&entry->list);
			free(entry->data);
			free(entry);
		}
		else break;
	}
}
void tcp_ofo_buf_free(struct tcp_sock *tsk)
{
	tcp_ofo_buf_entry_t *entry,*q;
	list_for_each_entry_safe(entry,q,&tsk->rcv_ofo_buf,list){
		list_delete_entry(&entry->list);
		free(entry->data);
		free(entry);
	}
}

static inline void update_tsk(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if(less_or_equal_32b(cb->seq, tsk->rcv_nxt) && greater_than_32b(cb->seq_end, tsk->rcv_nxt))
		tsk->rcv_nxt = cb->seq_end;
}
static void handle_tcp_data(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	int len = (int)(cb->seq_end-tsk->rcv_nxt) - (cb->flags&(TCP_SYN|TCP_FIN)?1:0);
	if(!cb->pl_len || len<=0) {update_tsk(tsk,cb); return; }
	pthread_mutex_lock(&tsk->wait_recv->lock);
	if(ring_buffer_free(tsk->rcv_buf) < cb->pl_len)
		log(DEBUG,"RECV BUFFER FULL. DROPED.");
	else{
		if(less_or_equal_32b(cb->seq, tsk->rcv_nxt)){
			write_ring_buffer(tsk->rcv_buf,cb->payload+cb->pl_len-len,len);
			update_tsk(tsk,cb);
			tcp_check_ofo_buf(tsk);
			wake_with_lock(tsk->wait_recv);
			tsk->rcv_wnd = max(ring_buffer_free(tsk->rcv_buf),1);
		}
		else
			tcp_ofo_buf_append(tsk,cb);
	}
	pthread_mutex_unlock(&tsk->wait_recv->lock);
}

static void handle_ack(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	pthread_mutex_lock(&retrans_timer_lock);
	if(cb->flags & TCP_ACK){
		if(less_than_32b(tsk->snd_una,cb->ack)){
			tsk->snd_una = cb->ack;
			if(tsk->dup_ack >=3){
				//fast recovery
				fprintf(stdout,"Partial ack: %d\n",tsk->dup_ack);
				if(greater_or_equal_32b(cb->ack,tsk->recovery_point)){
					fprintf(stdout,"Exit Fast Recovery: %d\n",tsk->dup_ack);
					tsk->dup_ack = 0; //exit fast recovery
				}
			}
			else if(tsk->cwnd<tsk->ssthresh){
				tsk->cwnd += 1;
				//record_cwnd(tsk);	
			}
			else {
				tsk->c_counter += 1;
				if(tsk->c_counter >= tsk->cwnd){
					tsk->cwnd += 1;
					tsk->c_counter = 0;
					//record_cwnd(tsk);	
				}
			}
			tcp_reset_retrans_timer(tsk);
			if(tcp_send_buf_clear(tsk,tsk->snd_una)){ //Be careful. Holds 2 locks;
				tcp_unset_retrans_timer(tsk);
			}
			else if(tsk->dup_ack >=3)
				tcp_send_buf_retrans(tsk); //fast recovery
		}
		else{
			tsk->dup_ack += 1;
			if(tsk->dup_ack == 3){
				// fast retrans
				tsk->ssthresh = max(tsk->cwnd/2,1);
				tsk->cwnd = tsk->ssthresh;
				//record_cwnd(tsk);	
				tsk->recovery_point = tsk->snd_nxt;
				fprintf(stdout,"Fast Retrans: %u\n",cb->ack);
				tcp_send_buf_retrans(tsk);
			}
		}
		
	}
	tcp_update_window(tsk,cb);
	pthread_mutex_unlock(&retrans_timer_lock);
	if(tsk->state != TCP_LISTEN && less_than_32b(cb->seq,cb->seq_end))
		tcp_send_control_packet(tsk,TCP_ACK);
}

// check whether the sequence number of the incoming packet is in the receiving window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if(tsk->state==TCP_LISTEN) return 1;
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if(tsk->state==TCP_SYN_SENT) {
		if(cb->ack == tsk->snd_nxt) return 1;
		log(ERROR,"SYN SEQ ERROR.");
		return 0;
	}
	if (!less_than_32b(cb->seq, rcv_end)) {
		log(ERROR, "SEQ OUT OF WINDOW.");
		return 0;
	}
	if(!less_or_equal_32b(cb->ack, tsk->snd_nxt)){
		log(ERROR, "ACK OUT OF SENT.");
		return 0;
	}
	if(!less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)||
			(tsk->rcv_nxt==cb->seq_end && less_than_32b(cb->seq,cb->seq_end))){
		log(ERROR, "Dup pkt.DROP.");
		handle_ack(tsk,cb);
		return 0;
	}
	if (!less_or_equal_32b(cb->seq_end, rcv_end)) {
		log(ERROR, "PKT TOO BIG. DROP.");
		return 0;
	}
	if(greater_than_32b(cb->seq, tsk->rcv_nxt) && cb->flags&(TCP_FIN|TCP_SYN)){
		log(ERROR, "SYN|FIN out of order.");
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
	if(tsk->state == TCP_LISTEN){
		if(cb->flags & TCP_SYN){
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
		return;
	}
	if(tsk->state == TCP_SYN_SENT){
		if((cb->flags & (TCP_ACK|TCP_SYN)) == (TCP_ACK|TCP_SYN)){
			tsk->state = TCP_ESTABLISHED;
			tsk->rcv_nxt = cb->seq_end;
			tsk->snd_una = cb->ack-1; //update after
			handle_ack(tsk,cb);
			//tcp_send_control_packet(tsk,TCP_ACK);
			wake_up(tsk->wait_connect);
		}
		return;
	}

	if(tsk->state == TCP_ESTABLISHED || tsk->state == TCP_SYN_RECV || tsk->state == TCP_FIN_WAIT_1 || tsk->state == TCP_FIN_WAIT_2)
		handle_tcp_data(tsk,cb);
	else 
		update_tsk(tsk,cb);
	handle_ack(tsk,cb);

	if(tsk->state == TCP_SYN_RECV && cb->flags & TCP_ACK && cb->ack == tsk->snd_nxt){
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
		while(greater_than_32b(tsk->snd_nxt,tsk->snd_una))
			sleep_on(tsk->wait_send);
		//tcp_send_control_packet(tsk,TCP_ACK);
		wake_up(tsk->wait_recv);
	}
	else if(tsk->state == TCP_FIN_WAIT_1 && cb->flags & TCP_ACK && cb->ack == tsk->snd_nxt){
		tsk->state = TCP_FIN_WAIT_2;
		if(cb->flags & TCP_FIN)
			goto JUMP;
	}
	else if(tsk->state == TCP_FIN_WAIT_2 && (cb->flags & (TCP_ACK|TCP_FIN)) == (TCP_ACK|TCP_FIN)){
JUMP:
		//write_record();
		tsk->state = TCP_TIME_WAIT;
		//tcp_send_control_packet(tsk,TCP_ACK);
		tcp_set_timewait_timer(tsk);
	}
	else if(tsk->state == TCP_LAST_ACK && cb->flags & (TCP_ACK && cb->ack == tsk->snd_nxt)){
		tsk->state = TCP_CLOSED;
		tcp_unhash(tsk);
	}
	
	return;
}
