#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static struct list_head timer_list;
pthread_mutex_t timer_lock;
static struct list_head retrans_timer_list;
pthread_mutex_t retrans_timer_lock;
#define TCP_RETRANS_TIME TCP_RETRANS_INTERVAL

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *entry,*q;
	list_for_each_entry_safe(entry,q,&timer_list,list){
		if(entry->enable){
			if((entry->timeout -= TCP_TIMER_SCAN_INTERVAL) < 0){
				struct tcp_sock *tsk = timewait_to_tcp_sock(entry);
				list_delete_entry(&entry->list);
				tsk->state = TCP_CLOSED;
				tcp_unhash(tsk);
				tcp_bind_unhash(tsk);
			}
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	tsk->timewait.enable = 1;
	tsk->timewait.type = 0;
	tsk->timewait.timeout = 2*TCP_MSL;
	pthread_mutex_lock(&timer_lock);
	list_add_tail(&tsk->timewait.list, &timer_list);
	pthread_mutex_unlock(&timer_lock);
}
void tcp_scan_retrans_timer_list()
{
	struct tcp_timer *entry,*q;
	list_for_each_entry_safe(entry,q,&retrans_timer_list,list){
		if(entry->enable){
			if((entry->timeout -= TCP_TIMER_SCAN_INTERVAL) < 0){
				struct tcp_sock *tsk = retranstimer_to_tcp_sock(entry);
				tsk->ssthresh = tsk->cwnd/2 > 1 ? tsk->cwnd/2 : 2;
				tsk->cwnd = 1;
				//record_cwnd(tsk);
				tsk->dup_ack = 0;
				//tsk->recovery_point = tsk->snd_nxt;
				int times = tcp_send_buf_retrans_all(tsk);
				fprintf(stdout,"Timeout Retrans: %d\n",times);
				printf("%u %u %u %u\n",tsk->snd_nxt,tsk->snd_una,tsk->snd_wnd,tsk->dup_ack*MSS);
				if(!times){
					tcp_send_control_packet(tsk,TCP_RST);
					list_delete_entry(&entry->list);
					tsk->state = TCP_CLOSED;
					tcp_unhash(tsk);
					tcp_bind_unhash(tsk);
				}
				else{
					entry->timeout = TCP_RETRANS_TIME * (1<<times);
				}
			}
		}
	}
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	pthread_mutex_init(&timer_lock,NULL);
	init_list_head(&retrans_timer_list);
	pthread_mutex_init(&retrans_timer_lock,NULL);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		pthread_mutex_lock(&timer_lock);
		tcp_scan_timer_list();
		pthread_mutex_unlock(&timer_lock);
		pthread_mutex_lock(&retrans_timer_lock);
		tcp_scan_retrans_timer_list();
		pthread_mutex_unlock(&retrans_timer_lock);
	}
	return NULL;
}
// hold lock
void tcp_reset_retrans_timer(struct tcp_sock *tsk)
{
	//fprintf(stdout,"reset! ack:%u\n",tsk->snd_una);
	/*
	if(!tsk->retrans_timer.enable){
		tsk->retrans_timer.enable = 1;
		tsk->retrans_timer.type = 0;
		list_add_tail(&tsk->retrans_timer.list, &retrans_timer_list);
	}
	*/
	tsk->retrans_timer.timeout = TCP_RETRANS_TIME;
}

void tcp_set_retrans_timer(struct tcp_sock *tsk)
{
	if(!tsk->retrans_timer.enable){
		tsk->retrans_timer.enable = 1;
		tsk->retrans_timer.type = 0;
		pthread_mutex_lock(&retrans_timer_lock);
		list_add_tail(&tsk->retrans_timer.list, &retrans_timer_list);
		pthread_mutex_unlock(&retrans_timer_lock);
		tsk->retrans_timer.timeout = TCP_RETRANS_TIME;
	}
}
void tcp_unset_retrans_timer(struct tcp_sock *tsk)
{
	tsk->retrans_timer.enable = 0;
	if(!list_empty(&tsk->retrans_timer.list))
		list_delete_entry(&tsk->retrans_timer.list);
}


