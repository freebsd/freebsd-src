/*-
 * Copyright (c) 2007-2009, Centre for Advanced Internet Architectures
 * Swinburne University of Technology, Melbourne, Australia
 * (CRICOS number 00111D).
 *
 * All rights reserved.
 *
 * SIFTR was first released in 2007 by James Healy and Lawrence Stewart whilst
 * working on the NewTCP research project at Swinburne University's Centre for
 * Advanced Internet Architectures, Melbourne, Australia, which was made
 * possible in part by a grant from the Cisco University Research Program Fund
 * at Community Foundation Silicon Valley. More details are available at:
 *   http://caia.swin.edu.au/urp/newtcp/
 *
 * Work on SIFTR v1.2.x was sponsored by the FreeBSD Foundation as part of
 * the "Enhancing the FreeBSD TCP Implementation" project 2008-2009.
 * More details are available at:
 *   http://www.freebsdfoundation.org/
 *   http://caia.swin.edu.au/freebsd/etcp09/
 *
 * Lawrence Stewart is currently the sole maintainer, and all contact regarding
 * SIFTR should be directed to him via email: lastewart@swin.edu.au
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors, the "Centre for Advanced Internet
 *    Architectures" and "Swinburne University of Technology" may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/******************************************************
 * Statistical Information For TCP Research (SIFTR)
 *
 * A FreeBSD kernel module that adds very basic intrumentation to the
 * TCP stack, allowing internal stats to be recorded to a log file
 * during experimentation.
 *
 * Initial release date: June 2007
 * Most recent update: July 2009
 ******************************************************/


#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/unistd.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sbuf.h>
#include <sys/alq.h>
#include <sys/proc.h>
#if (__FreeBSD_version >= 800044)
#include <sys/vimage.h>
#endif

#include <net/if.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp_var.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>

#if (__FreeBSD_version >= 800044)
#include <netinet/vinet.h>
#endif

#ifdef SIFTR_IPV6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>

#if (__FreeBSD_version >= 800044)
#include <netinet6/vinet6.h>
#endif

#endif /* SIFTR_IPV6 */

#include <machine/in_cksum.h>

#include "siftr_hash.h"


#define MODVERSION  "1.2.2"
#define HOOK 0
#define UNHOOK 1

#define SIFTR_EXPECTED_MAX_TCP_FLOWS 65536


#define SYS_NAME "FreeBSD"
#define PACKET_TAG_SIFTR 100
#define PACKET_COOKIE_SIFTR 21749576
#define SIFTR_LOG_FILE_MODE 0644

/*
 * log messages are less than MAX_LOG_MSG_LEN chars long, so divide
 * SIFTR_ALQ_BUFLEN by MAX_LOG_MSG_LEN to get the approximate upper bound
 * number of log messages that can be held in the ALQ buffer
 */
#define MAX_LOG_MSG_LEN 200
#define SIFTR_ALQ_BUFLEN 200000


#define SIFTR_DISABLE 0
#define SIFTR_ENABLE 1


/*
 * 1 byte for IP version
 * IPv4: src/dst IP (4+4) + src/dst port (2+2) = 12 bytes
 * IPv6: src/dst IP (16+16) + src/dst port (2+2) = 36 bytes
 */
#ifdef SIFTR_IPV6
#define FLOW_KEY_LEN 37
#else
#define FLOW_KEY_LEN 13
#endif

#ifdef SIFTR_IPV6
#define SIFTR_IPMODE 6
#else
#define SIFTR_IPMODE 4
#endif

/* useful macros */
#define CAST_PTR_INT(X) (*((int*)(X)))

#define TOTAL_TCP_PKTS \
( \
	siftr_num_inbound_tcp_pkts + \
	siftr_num_outbound_tcp_pkts \
)

#define TOTAL_SKIPPED_TCP_PKTS \
( \
	siftr_num_inbound_skipped_pkts_malloc + \
	siftr_num_inbound_skipped_pkts_mtx + \
	siftr_num_outbound_skipped_pkts_malloc + \
	siftr_num_outbound_skipped_pkts_mtx + \
	siftr_num_inbound_skipped_pkts_tcb + \
	siftr_num_outbound_skipped_pkts_tcb + \
	siftr_num_inbound_skipped_pkts_icb + \
	siftr_num_outbound_skipped_pkts_icb \
)

#define UPPER_SHORT(X)	(((X) & 0xFFFF0000) >> 16)
#define LOWER_SHORT(X)	((X) & 0x0000FFFF)

#define FIRST_OCTET(X)	(((X) & 0xFF000000) >> 24)
#define SECOND_OCTET(X)	(((X) & 0x00FF0000) >> 16)
#define THIRD_OCTET(X)	(((X) & 0x0000FF00) >> 8)
#define FOURTH_OCTET(X)	((X) & 0x000000FF)

MALLOC_DECLARE(M_SIFTR);
MALLOC_DEFINE(M_SIFTR, "siftr", "dynamic memory used by SIFTR");

MALLOC_DECLARE(M_SIFTR_PKTNODE);
MALLOC_DEFINE(M_SIFTR_PKTNODE, "siftr_pktnode", "SIFTR pkt_node struct");

MALLOC_DECLARE(M_SIFTR_HASHNODE);
MALLOC_DEFINE(M_SIFTR_HASHNODE, "siftr_hashnode", "SIFTR flow_hash_node struct");

/* Struct that will make up links in the pkt manager queue */
struct pkt_node {
	/* timestamp of pkt as noted in the pfil hook */
	struct timeval		tval;
	/* direction pkt is travelling; either PFIL_IN or PFIL_OUT */
	uint8_t			direction;
	/* IP version pkt_node relates to; either INP_IPV4 or INP_IPV6 */
	uint8_t			ipver;
	/* hash of the pkt which triggered the log message */
	uint32_t		hash;
	/* local/foreign IP address */
#ifdef SIFTR_IPV6
	uint32_t		ip_laddr[4];
	uint32_t		ip_faddr[4];
#else
	uint8_t			ip_laddr[4];
	uint8_t			ip_faddr[4];
#endif
	/* local TCP port */
	uint16_t		tcp_localport;
	/* foreign TCP port */
	uint16_t		tcp_foreignport;
	/* Congestion Window (bytes) */
	u_long			snd_cwnd;
	/* Sending Window (bytes) */
	u_long			snd_wnd;
	/* Receive Window (bytes) */
	u_long			rcv_wnd;
	/* Bandwidth Controlled Window (bytes) */
	u_long			snd_bwnd;
	/* Slow Start Threshold (bytes) */
	u_long			snd_ssthresh;
	/* Current state of the TCP FSM */
	int			conn_state;
	/* Max Segment Size (bytes) */
	u_int			max_seg_size;
	/*
	 * Smoothed RTT stored as found in the TCP control block
	 * in units of (TCP_RTT_SCALE*hz)
	 */
	int			smoothed_rtt;
	/* Is SACK enabled? */
	u_char			sack_enabled;
	/* Window scaling for snd window */
	u_char			snd_scale;
	/* Window scaling for recv window */
	u_char			rcv_scale;
	/* TCP control block flags */
	u_int			flags;
	/* Retransmit timeout length */
	int			rxt_length;
	/* Size of the TCP send buffer in bytes */
	u_int			snd_buf_hiwater;
	/* Current num bytes in the send socket buffer */
	u_int			snd_buf_cc;
	/* Size of the TCP receive buffer in bytes */
	u_int			rcv_buf_hiwater;
	/* Current num bytes in the receive socket buffer */
	u_int			rcv_buf_cc;
	/* Number of bytes inflight that we are waiting on ACKs for */
	u_int			sent_inflight_bytes;
	/* Link to next pkt_node in the list */
	STAILQ_ENTRY(pkt_node)	nodes;
};

/* Struct that will be stored in the TCP flow hash table */
struct flow_hash_node
{
  uint16_t counter;
  uint8_t key[FLOW_KEY_LEN];
  LIST_ENTRY(flow_hash_node) nodes;
};

/* various runtime stats variables */
static volatile uint32_t siftr_num_inbound_skipped_pkts_malloc = 0;
static volatile uint32_t siftr_num_inbound_skipped_pkts_mtx = 0;
static volatile uint32_t siftr_num_outbound_skipped_pkts_malloc = 0;
static volatile uint32_t siftr_num_outbound_skipped_pkts_mtx = 0;
static volatile uint32_t siftr_num_inbound_skipped_pkts_icb = 0;
static volatile uint32_t siftr_num_outbound_skipped_pkts_icb = 0;
static volatile uint32_t siftr_num_inbound_skipped_pkts_tcb = 0;
static volatile uint32_t siftr_num_outbound_skipped_pkts_tcb = 0;
static volatile uint32_t siftr_num_inbound_skipped_pkts_dejavu = 0;
static volatile uint32_t siftr_num_outbound_skipped_pkts_dejavu = 0;
static volatile uint32_t siftr_num_inbound_tcp_pkts = 0;
static volatile uint32_t siftr_num_outbound_tcp_pkts = 0;

static volatile uint32_t siftr_exit_pkt_manager_thread = 0;
static uint8_t siftr_enabled = 0;
static uint32_t siftr_pkts_per_log = 1;
static char siftr_logfile[PATH_MAX] = "/var/log/siftr.log\0";

/*
 * Controls whether we generate a hash for each packet that triggers
 * a SIFTR log message. Should eventually be made accessible via sysctl.
 */
static uint8_t siftr_generate_hashes = 1;

/*
 * pfil.h defines PFIL_IN as 1 and PFIL_OUT as 2,
 * which we use as an index into this array.
 */
static char direction[3] = {'\0', 'i','o'};


static char *log_writer_msg_buf;
STAILQ_HEAD(pkthead, pkt_node) pkt_queue = STAILQ_HEAD_INITIALIZER(pkt_queue);


static u_long siftr_hashmask;
LIST_HEAD(listhead, flow_hash_node) *counter_hash;

static int wait_for_pkt;

static struct alq *siftr_alq = NULL;
static struct mtx siftr_pkt_queue_mtx;

static struct mtx siftr_pkt_mgr_mtx;



static struct thread *siftr_pkt_manager_thr = NULL;
#if (__FreeBSD_version < 800000)
static struct proc *siftr_pkt_manager_proc = NULL;
#endif

#if (__FreeBSD_version >= 800044)
#define _siftrtcbinfo &V_tcbinfo
#else
#define _siftrtcbinfo &tcbinfo
#endif



static void
siftr_process_pkt(struct pkt_node * pkt_node)
{
	char siftr_log_msg[MAX_LOG_MSG_LEN];
	uint8_t found_match = 0;
	uint8_t key[FLOW_KEY_LEN];
	uint8_t key_offset = 1;
	struct flow_hash_node *hash_node = NULL;
	struct listhead *counter_list = NULL;
	
	/*
	 * Create the key that will be used to create a hash index
	 * into our hash table.
	 * Our key consists of ipversion,localip,localport,foreignip,foreignport
	 */
	key[0] = pkt_node->ipver;
	memcpy(	key + key_offset,
		(void *)(&(pkt_node->ip_laddr)),
		sizeof(pkt_node->ip_laddr)
	);
	key_offset += sizeof(pkt_node->ip_laddr);
	memcpy(	key + key_offset,
		(void *)(&(pkt_node->tcp_localport)),
		sizeof(pkt_node->tcp_localport)
	);
	key_offset += sizeof(pkt_node->tcp_localport);
	memcpy(	key + key_offset,
		(void *)(&(pkt_node->ip_faddr)),
		sizeof(pkt_node->ip_faddr)
	);
	key_offset += sizeof(pkt_node->ip_faddr);
	memcpy(	key + key_offset,
		(void *)(&(pkt_node->tcp_foreignport)),
		sizeof(pkt_node->tcp_foreignport)
	);
	
	counter_list = (counter_hash + 
			(hash32_buf(key, sizeof(key), 0) & siftr_hashmask));
	
	/*
	 * If the list is not empty i.e. the hash index has
	 * been used by another flow previously.
	 */
	if(LIST_FIRST(counter_list) != NULL) {
		/*
		 * Loop through the hash nodes in the list.
		 * There should normally only be 1 hash node in the list,
		 * except if there have been collisions at the hash index
		 * computed by hash32_buf()
		 */
		LIST_FOREACH(hash_node, counter_list, nodes) {
			/*
			 * Check if the key for the pkt we are currently
			 * processing is the same as the key stored in the
			 * hash node we are currently processing.
			 * If they are the same, then we've found the
			 * hash node that stores the counter for the flow
			 * the pkt belongs to
			 */
			if (memcmp(hash_node->key, key, sizeof(key)) == 0) {
				found_match = 1;
				break;
			}
		}
	}

	/* If this flow hash hasn't been seen before or we have a collision */
	if (hash_node == NULL || !found_match) {
		/* Create a new hash node to store the flow's counter */
		hash_node = malloc(	sizeof(struct flow_hash_node),
					M_SIFTR_HASHNODE,
					M_WAITOK
		);

		if (hash_node != NULL) {
			/* Initialise our new hash node list entry */
			hash_node->counter = 0;
			memcpy(hash_node->key, key, sizeof(key));
			LIST_INSERT_HEAD(counter_list, hash_node, nodes);
		}
		else {
			/* malloc failed */
			if (pkt_node->direction == PFIL_IN)
				siftr_num_inbound_skipped_pkts_malloc++;
			else
				siftr_num_outbound_skipped_pkts_malloc++;

			return;
		}
	}
	else if (siftr_pkts_per_log > 1) {
		/*
		 * Taking the remainder of the counter divided
		 * by the current value of siftr_pkts_per_log
		 * and storing that in counter provides a neat
		 * way to modulate the frequency of log
		 * messages being written to the log file
		 */
		hash_node->counter = (hash_node->counter + 1) %
						siftr_pkts_per_log;

		/*
		 * If we have not seen enough packets since the last time
		 * we wrote a log message for this connection, return
		 */
		if (hash_node->counter > 0)
			return;
	}

#ifdef SIFTR_IPV6
	pkt_node->ip_laddr[3] = ntohl(pkt_node->ip_laddr[3]);
	pkt_node->ip_faddr[3] = ntohl(pkt_node->ip_faddr[3]);

	if (pkt_node->ipver == INP_IPV6) { /* IPv6 packet */
		pkt_node->ip_laddr[0] = ntohl(pkt_node->ip_laddr[0]);
		pkt_node->ip_laddr[1] = ntohl(pkt_node->ip_laddr[1]);
		pkt_node->ip_laddr[2] = ntohl(pkt_node->ip_laddr[2]);
		pkt_node->ip_faddr[0] = ntohl(pkt_node->ip_faddr[0]);
		pkt_node->ip_faddr[1] = ntohl(pkt_node->ip_faddr[1]);
		pkt_node->ip_faddr[2] = ntohl(pkt_node->ip_faddr[2]);

		/* Construct an IPv6 log message. */
		sprintf(siftr_log_msg,
#if (__FreeBSD_version >= 700000)
			"%c,0x%08x,%zd.%06ld,%x:%x:%x:%x:%x:%x:%x:%x,%u,%x:%x:%x:%x:%x:%x:%x:%x,%u,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
#else
			"%c,0x%08x,%ld.%06ld,%x:%x:%x:%x:%x:%x:%x:%x,%u,%x:%x:%x:%x:%x:%x:%x:%x,%u,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
#endif
			direction[pkt_node->direction],
			pkt_node->hash,
			pkt_node->tval.tv_sec,
			pkt_node->tval.tv_usec,
			UPPER_SHORT(pkt_node->ip_laddr[0]),
			LOWER_SHORT(pkt_node->ip_laddr[0]),
			UPPER_SHORT(pkt_node->ip_laddr[1]),
			LOWER_SHORT(pkt_node->ip_laddr[1]),
			UPPER_SHORT(pkt_node->ip_laddr[2]),
			LOWER_SHORT(pkt_node->ip_laddr[2]),
			UPPER_SHORT(pkt_node->ip_laddr[3]),
			LOWER_SHORT(pkt_node->ip_laddr[3]),
			ntohs(pkt_node->tcp_localport),
			UPPER_SHORT(pkt_node->ip_faddr[0]),
			LOWER_SHORT(pkt_node->ip_faddr[0]),
			UPPER_SHORT(pkt_node->ip_faddr[1]),
			LOWER_SHORT(pkt_node->ip_faddr[1]),
			UPPER_SHORT(pkt_node->ip_faddr[2]),
			LOWER_SHORT(pkt_node->ip_faddr[2]),
			UPPER_SHORT(pkt_node->ip_faddr[3]),
			LOWER_SHORT(pkt_node->ip_faddr[3]),
			ntohs(pkt_node->tcp_foreignport),
			pkt_node->snd_ssthresh,
			pkt_node->snd_cwnd,
			pkt_node->snd_bwnd,
			pkt_node->snd_wnd,
			pkt_node->rcv_wnd,
			pkt_node->snd_scale,
			pkt_node->rcv_scale,
			pkt_node->conn_state,
			pkt_node->max_seg_size,
			pkt_node->smoothed_rtt,
			pkt_node->sack_enabled,
			pkt_node->flags,
			pkt_node->rxt_length,
			pkt_node->snd_buf_hiwater,
			pkt_node->snd_buf_cc,
			pkt_node->rcv_buf_hiwater,
			pkt_node->rcv_buf_cc,
			pkt_node->sent_inflight_bytes
		);
	} else { /* IPv4 packet */
		pkt_node->ip_laddr[0] = FIRST_OCTET(pkt_node->ip_laddr[3]);
		pkt_node->ip_laddr[1] = SECOND_OCTET(pkt_node->ip_laddr[3]);
		pkt_node->ip_laddr[2] = THIRD_OCTET(pkt_node->ip_laddr[3]);
		pkt_node->ip_laddr[3] = FOURTH_OCTET(pkt_node->ip_laddr[3]);
		pkt_node->ip_faddr[0] = FIRST_OCTET(pkt_node->ip_faddr[3]);
		pkt_node->ip_faddr[1] = SECOND_OCTET(pkt_node->ip_faddr[3]);
		pkt_node->ip_faddr[2] = THIRD_OCTET(pkt_node->ip_faddr[3]);
		pkt_node->ip_faddr[3] = FOURTH_OCTET(pkt_node->ip_faddr[3]);
#endif /* SIFTR_IPV6 */

		/* Construct an IPv4 log message. */
		sprintf(siftr_log_msg,
#if (__FreeBSD_version >= 700000)
			"%c,0x%08x,%zd.%06ld,%u.%u.%u.%u,%u,%u.%u.%u.%u,%u,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
#else
			"%c,0x%08x,%ld.%06ld,%u.%u.%u.%u,%u,%u.%u.%u.%u,%u,%ld,%ld,%ld,%ld,%ld,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%u,%u\n",
#endif
			direction[pkt_node->direction],
			pkt_node->hash,
			pkt_node->tval.tv_sec,
			pkt_node->tval.tv_usec,
			pkt_node->ip_laddr[0],
			pkt_node->ip_laddr[1],
			pkt_node->ip_laddr[2],
			pkt_node->ip_laddr[3],
			ntohs(pkt_node->tcp_localport),
			pkt_node->ip_faddr[0],
			pkt_node->ip_faddr[1],
			pkt_node->ip_faddr[2],
			pkt_node->ip_faddr[3],
			ntohs(pkt_node->tcp_foreignport),
			pkt_node->snd_ssthresh,
			pkt_node->snd_cwnd,
			pkt_node->snd_bwnd,
			pkt_node->snd_wnd,
			pkt_node->rcv_wnd,
			pkt_node->snd_scale,
			pkt_node->rcv_scale,
			pkt_node->conn_state,
			pkt_node->max_seg_size,
			pkt_node->smoothed_rtt,
			pkt_node->sack_enabled,
			pkt_node->flags,
			pkt_node->rxt_length,
			pkt_node->snd_buf_hiwater,
			pkt_node->snd_buf_cc,
			pkt_node->rcv_buf_hiwater,
			pkt_node->rcv_buf_cc,
			pkt_node->sent_inflight_bytes
		);
#ifdef SIFTR_IPV6
	}
#endif

	/*
	 * XXX: This could possibly be made more efficient by padding
	 * the log message to always be a fixed number of characters...
	 * We wouldn't need the call to strlen if we did this
	 */
	/* XXX: Should we use alq_getn/alq_post here to avoid the bcopy? */
	alq_writen(siftr_alq, siftr_log_msg, strlen(siftr_log_msg), ALQ_WAITOK);
}







static void
siftr_pkt_manager_thread(void *arg)
{
	struct pkt_node *pkt_node, *pkt_node_temp;
	STAILQ_HEAD(pkthead, pkt_node) tmp_pkt_queue = STAILQ_HEAD_INITIALIZER(tmp_pkt_queue);
	uint8_t draining = 2;

	mtx_lock(&siftr_pkt_mgr_mtx);

	/* draining == 0 when queue has been flushed and it's safe to exit */
	while (draining) {
		/*
		 * Sleep until we are signalled to wake because thread has
		 * been told to exit or until 1 tick has passed
		 */
		msleep(&wait_for_pkt, &siftr_pkt_mgr_mtx, PWAIT, "pktwait", 1);

		/* Gain exclusive access to the pkt_node queue */
		mtx_lock(&siftr_pkt_queue_mtx);

		/*
		 * Move pkt_queue to tmp_pkt_queue, which leaves
		 * pkt_queue empty and ready to receive more pkt_nodes
		 */
		STAILQ_CONCAT(&tmp_pkt_queue, &pkt_queue);

		/*
		 * We've finished making changes to the list. Unlock it
		 * so the pfil hooks can continue queuing pkt_nodes
		 */
		mtx_unlock(&siftr_pkt_queue_mtx);

		/*
		 * We can't hold a mutex whilst calling siftr_process_pkt
		 * because ALQ might sleep waiting for buffer space.
		 */
		mtx_unlock(&siftr_pkt_mgr_mtx);

		/* Flush all pkt_nodes to the log file */
		STAILQ_FOREACH_SAFE(pkt_node,
				&tmp_pkt_queue,
				nodes,
				pkt_node_temp) {
			siftr_process_pkt(pkt_node);
			STAILQ_REMOVE_HEAD(&tmp_pkt_queue, nodes);
			free(pkt_node, M_SIFTR_PKTNODE);
		}

		KASSERT(STAILQ_EMPTY(&tmp_pkt_queue),
			("SIFTR tmp_pkt_queue not empty after flush")
		);

		mtx_lock(&siftr_pkt_mgr_mtx);

		/*
		 * If siftr_exit_pkt_manager_thread gets set during the window
		 * where we are draining the tmp_pkt_queue above, there might
		 * still be pkts in pkt_queue that need to be drained.
		 * Allow one further iteration to occur after
		 * siftr_exit_pkt_manager_thread has been set to ensure
		 * pkt_queue is completely empty before we kill the thread.
		 *
		 * siftr_exit_pkt_manager_thread is set only after the pfil
		 * hooks have been removed, so only 1 extra iteration
		 * is needed to drain the queue.
		 */
		if (siftr_exit_pkt_manager_thread)
			draining--;
	}

	mtx_unlock(&siftr_pkt_mgr_mtx);

#if (__FreeBSD_version >= 800000)
	/* calls wakeup on this thread's struct thread ptr */
	kthread_exit();
#else
#if (__FreeBSD_version < 700000)
	/* no wakeup given in 6.x so have to do it ourself */
	wakeup(siftr_pkt_manager_proc);
#endif
	/* calls wakeup on this thread's struct proc ptr on 7.x */
	kthread_exit(0);
#endif
}

static uint32_t
hash_pkt(struct mbuf *m, uint32_t offset)
{
	register uint32_t hash = 0;

	while ((m != NULL) && (offset > m->m_len)) {
		/*
		 * The IP packet payload does not start in this mbuf, so
		 * need to figure out which mbuf it starts in and what offset
		 * into the mbuf's data region the payload starts at.
		 */
		offset -= m->m_len;
		m = m->m_next;
	}

	while (m != NULL) {
		/* Ensure there is data in the mbuf */
		if ((m->m_len - offset) > 0) {
			hash = hash32_buf(	m->m_data + offset,
						m->m_len - offset,
						hash
			);
                }

		m = m->m_next;
		offset = 0;
        }

	return hash;
}

/*
 * pfil hook that is called for each IPv4 packet making its way through the
 * stack in either direction.
 * The pfil subsystem holds a non-sleepable mutex somewhere when
 * calling our hook function, so we can't sleep at all.
 * It's very important to use the M_NOWAIT flag with all function calls
 * that support it so that they won't sleep, otherwise you get a panic
 */
static int
siftr_chkpkt(	void *arg,
		struct mbuf **m,
		struct ifnet *ifp,
		int dir,
		struct inpcb *inp
)
{
	register struct pkt_node *pkt_node = NULL;
	register struct ip *ip = NULL;
	register struct tcphdr *th = NULL;
	register struct tcpcb *tp = NULL;
	register unsigned int ip_hl = 0;
	register uint8_t inp_locally_locked = 0;

	/*
	 * I don't think we need m_pullup here because both
	 * ip_input and ip_output seem to do the heavy lifting
	 */
	/* *m = m_pullup(*m, sizeof(struct ip));
	if (*m == NULL)
		goto ret; */

	/* Cram the mbuf into an ip packet struct */
	ip = mtod(*m, struct ip *);

	/* Only continue processing if the packet is TCP */
	if(ip->ip_p != IPPROTO_TCP)
		goto ret;
	
	/*
	 * If a kernel subsystem reinjects packets into the stack, our pfil
	 * hook will be called multiple times for the same packet.
	 * Make sure we only process unique packets.
	 */
	if (m_tag_locate(*m, PACKET_COOKIE_SIFTR, PACKET_TAG_SIFTR, NULL)
	    != NULL) {

		if(dir == PFIL_IN)
			siftr_num_inbound_skipped_pkts_dejavu++;
		else
			siftr_num_outbound_skipped_pkts_dejavu++;

		goto ret;
	}
	else {
		struct m_tag *tag = m_tag_alloc( PACKET_COOKIE_SIFTR,
						 PACKET_TAG_SIFTR,
						 0,
						 M_NOWAIT
		);
		if (tag == NULL) {
			if(dir == PFIL_IN)
				siftr_num_inbound_skipped_pkts_malloc++;
			else
				siftr_num_outbound_skipped_pkts_malloc++;

			goto ret;
		}

		m_tag_prepend(*m, tag);
	}

	if(dir == PFIL_IN)
		siftr_num_inbound_tcp_pkts++;
	else
		siftr_num_outbound_tcp_pkts++;

	/*
	 * Create a tcphdr struct starting at the correct offset
	 * in the IP packet. ip->ip_hl gives the ip header length
	 * in 4-byte words, so multiply it to get the size in bytes
	 */
	ip_hl = (ip->ip_hl << 2);
	th = (struct tcphdr *)((caddr_t)ip + ip_hl);

	/*
	 * If the pfil hooks don't provide a pointer to the
	 * IP control block, we need to find it ourselves and lock it
	 */
	if (!inp) {
		/* Find the corresponding inpcb for this pkt */

		/* We need the tcbinfo lock */
#if (__FreeBSD_version >= 700000)
		INP_INFO_UNLOCK_ASSERT(_siftrtcbinfo);
#endif
		INP_INFO_RLOCK(_siftrtcbinfo);

		if (dir == PFIL_IN)
			inp = in_pcblookup_hash(_siftrtcbinfo,
						ip->ip_src,
						th->th_sport,
						ip->ip_dst,
						th->th_dport,
						0,
						(*m)->m_pkthdr.rcvif
			);
		else
			inp = in_pcblookup_hash(_siftrtcbinfo,
						ip->ip_dst,
						th->th_dport,
						ip->ip_src,
						th->th_sport,
						0,
						(*m)->m_pkthdr.rcvif
			);

		/* If we can't find the IP control block, bail */
		if (!inp) {
			if(dir == PFIL_IN)
				siftr_num_inbound_skipped_pkts_icb++;
			else
				siftr_num_outbound_skipped_pkts_icb++;

			INP_INFO_RUNLOCK(_siftrtcbinfo);

			goto ret;
		}

		/* Acquire the inpcb lock */
		INP_UNLOCK_ASSERT(inp);
#if (__FreeBSD_version >= 701000)
		INP_RLOCK(inp);
#else
		INP_LOCK(inp);
#endif
		INP_INFO_RUNLOCK(_siftrtcbinfo);

		inp_locally_locked = 1;
	}

	INP_LOCK_ASSERT(inp);

	pkt_node = malloc(sizeof(struct pkt_node), M_SIFTR_PKTNODE, M_NOWAIT | M_ZERO);
	
	if (pkt_node == NULL) {

		if(dir == PFIL_IN)
			siftr_num_inbound_skipped_pkts_malloc++;
		else
			siftr_num_outbound_skipped_pkts_malloc++;

		goto inp_unlock;
	}

	/* Find the TCP control block that corresponds with this packet */
	tp = intotcpcb(inp);

	/*
	 * If we can't find the TCP control block (happens occasionaly for a
	 * packet sent during the shutdown phase of a TCP connection),
	 * or we're in the timewait state, bail
	 */
#if (INP_TIMEWAIT == 0x8)
	if (!tp || (inp->inp_vflag & INP_TIMEWAIT)) {
#else
	if (!tp || (inp->inp_flags & INP_TIMEWAIT)) {
#endif
		if(dir == PFIL_IN)
			siftr_num_inbound_skipped_pkts_tcb++;
		else
			siftr_num_outbound_skipped_pkts_tcb++;

		free(pkt_node, M_SIFTR_PKTNODE);
		goto inp_unlock;
	}

	/* Fill in pkt_node data */
#ifdef SIFTR_IPV6
	pkt_node->ip_laddr[3] = inp->inp_laddr.s_addr;
	pkt_node->ip_faddr[3] = inp->inp_faddr.s_addr;
#else
	*((uint32_t *)pkt_node->ip_laddr) = inp->inp_laddr.s_addr;
	*((uint32_t *)pkt_node->ip_faddr) = inp->inp_faddr.s_addr;
#endif
	pkt_node->ipver = INP_IPV4;
	pkt_node->tcp_localport = inp->inp_lport;
	pkt_node->tcp_foreignport = inp->inp_fport;
	pkt_node->snd_cwnd = tp->snd_cwnd;
	pkt_node->snd_wnd = tp->snd_wnd;
	pkt_node->rcv_wnd = tp->rcv_wnd;
	pkt_node->snd_bwnd = tp->snd_bwnd;
	pkt_node->snd_ssthresh = tp->snd_ssthresh;
	pkt_node->snd_scale = tp->snd_scale;
	pkt_node->rcv_scale = tp->rcv_scale;
	pkt_node->conn_state = tp->t_state;
	pkt_node->max_seg_size = tp->t_maxseg;
	pkt_node->smoothed_rtt = tp->t_srtt;
#if (__FreeBSD_version >= 700000)
	pkt_node->sack_enabled = tp->t_flags & TF_SACK_PERMIT;
#else
	pkt_node->sack_enabled = tp->sack_enable;
#endif
	pkt_node->flags = tp->t_flags;
	pkt_node->rxt_length = tp->t_rxtcur;
	pkt_node->snd_buf_hiwater = inp->inp_socket->so_snd.sb_hiwat;
	pkt_node->snd_buf_cc = inp->inp_socket->so_snd.sb_cc;
	pkt_node->rcv_buf_hiwater = inp->inp_socket->so_rcv.sb_hiwat;
	pkt_node->rcv_buf_cc = inp->inp_socket->so_rcv.sb_cc;
	pkt_node->sent_inflight_bytes = tp->snd_max - tp->snd_una;

	/* We've finished accessing the tcb so release the lock */
	if (inp_locally_locked)
#if (__FreeBSD_version >= 701000)
		INP_RUNLOCK(inp);
#else
		INP_UNLOCK(inp);
#endif

	pkt_node->direction = dir;

	/*
	 * Significantly more accurate than using getmicrotime(), but slower!
	 * Gives true microsecond resolution at the expense of a hit to
	 * maximum pps throughput processing when SIFTR is loaded and enabled.
	 */
	microtime(&(pkt_node->tval));

	if (siftr_generate_hashes) {

		if ((*m)->m_pkthdr.csum_flags & CSUM_TCP) {
			/*
			 * For outbound packets, the TCP checksum isn't
			 * calculated yet. This is a problem for our packet
			 * hashing as the receiver will calc a different hash
			 * to ours if we don't include the correct TCP checksum
			 * in the bytes being hashed. To work around this
			 * problem, we manually calc the TCP checksum here in
			 * software. We unset the CSUM_TCP flag so the lower
			 * layers don't recalc it.
			 */
			(*m)->m_pkthdr.csum_flags &= ~CSUM_TCP;
	
			/*
			 * Calculate the TCP checksum in software and assign
			 * to correct TCP header field, which will follow the
			 * packet mbuf down the stack. The trick here is that
			 * tcp_output() sets th->th_sum to the checksum of the
			 * pseudo header for us already. Because of the nature
			 * of the checksumming algorithm, we can sum over the
			 * entire IP payload (i.e. TCP header and data), which
			 * will include the already calculated pseduo header
			 * checksum, thus giving us the complete TCP checksum.
			 *
			 * To put it in simple terms, if checksum(1,2,3,4)=10,
			 * then checksum(1,2,3,4,5) == checksum(10,5).
			 * This property is what allows us to "cheat" and
			 * checksum only the IP payload which has the TCP
			 * th_sum field populated with the pseudo header's
			 * checksum, and not need to futz around checksumming
			 * pseudo header bytes and TCP header/data in one hit.
			 * Refer to RFC 1071 for more info.
			 *
			 * NB: in_cksum_skip(struct mbuf *m, int len, int skip)
			 * in_cksum_skip 2nd argument is NOT the number of
			 * bytes to read from the mbuf at "skip" bytes offset
			 * from the start of the mbuf (very counter intuitive!).
			 * The number of bytes to read is calculated internally
			 * by the function as len-skip i.e. to sum over the IP
			 * payload (TCP header + data) bytes, it is INCORRECT
			 * to call the function like this:
			 * in_cksum_skip(at, ip->ip_len - offset, offset)
			 * Rather, it should be called like this:
			 * in_cksum_skip(at, ip->ip_len, offset)
			 * which means read "ip->ip_len - offset" bytes from
			 * the mbuf cluster "at" at offset "offset" bytes from
			 * the beginning of the "at" mbuf's data pointer.
			 */
			th->th_sum  = in_cksum_skip(*m, ip->ip_len, ip_hl);
		}
		/*
		printf("th->th_sum: 0x%04x\n\n", th->th_sum);
		nanotime(&start);
		*/

		/*
		 * XXX: Having to calculate the checksum in software and then
		 * hash over all bytes is really inefficient. Would be nice to
		 * find a way to create the hash and checksum in the same pass
		 * over the bytes.
		 */
		pkt_node->hash = hash_pkt(*m, ip_hl);
		
		/*
		nanotime(&end);
		timespecsub(&end, &start);
		pkt_node->hash = in_addword(th->th_sum, th->th_sum);
		printf("dir: %c\tpkt_node->hash: 0x%08x\thashtime: %us %uns\n\n", direction[dir], pkt_node->hash, (unsigned int)end.tv_sec, (unsigned int)end.tv_nsec);
		printf("at: 0x%08x\n", (unsigned int)at);
		printf("dir: %c\tip->ip_len: %d\thash: 0x%04x\tfast hash: 0x%04x\tip->ip_src.s_addr: 0x%08x\tip->ip_dst.s_addr: 0x%08x\n\n", direction[dir], ip->ip_len, pkt_node->hash, th->th_sum + th->th_sum, ip->ip_src.s_addr, ip->ip_dst.s_addr);
		*/
	}

	mtx_lock(&siftr_pkt_queue_mtx);
	STAILQ_INSERT_TAIL(&pkt_queue, pkt_node, nodes);
	mtx_unlock(&siftr_pkt_queue_mtx);
	goto ret;

inp_unlock:
	if (inp_locally_locked)
#if (__FreeBSD_version >= 701000)
		INP_RUNLOCK(inp);
#else
		INP_UNLOCK(inp);
#endif

ret:
	/* Returning 0 ensures pfil will not discard the pkt */
	return 0;
}





#ifdef SIFTR_IPV6
static int
siftr_chkpkt6(	void *arg,
		struct mbuf **m,
		struct ifnet *ifp,
		int dir,
		struct inpcb *inp
)
{
	register struct pkt_node *pkt_node = NULL;
	register struct ip6_hdr *ip6 = NULL;
	register struct tcphdr *th = NULL;
	register struct tcpcb *tp = NULL;
	register unsigned int ip6_hl = 0;
	register uint8_t inp_locally_locked = 0;
	
	/*
	 * I don't think we need m_pullup here because both
	 * ip_input and ip_output seem to do the heavy lifting
	 */
	/* *m = m_pullup(*m, sizeof(struct ip));
	if (*m == NULL)
		goto ret; */

	/* Cram the mbuf into an ip6 packet struct */
	ip6 = mtod(*m, struct ip6_hdr *);

	/*
	 * Only continue processing if the packet is TCP
	 * XXX: We should follow the next header fields
	 * as shown on Pg 6 RFC 2460, but right now we'll
	 * only check pkts that have no extension headers
	 */
	if (ip6->ip6_nxt != IPPROTO_TCP)
		goto ret6;

	/*
	 * If a kernel subsystem reinjects packets into the stack, our pfil
	 * hook will be called multiple times for the same packet.
	 * Make sure we only process unique packets.
	 */
	if (m_tag_locate(*m, PACKET_COOKIE_SIFTR, PACKET_TAG_SIFTR, NULL)
	    != NULL) {

		if(dir == PFIL_IN)
			siftr_num_inbound_skipped_pkts_dejavu++;
		else
			siftr_num_outbound_skipped_pkts_dejavu++;

		goto ret6;
	}
	else {
		struct m_tag *tag = m_tag_alloc( PACKET_COOKIE_SIFTR,
						 PACKET_TAG_SIFTR,
						 0,
						 M_NOWAIT
		);
		if (tag == NULL) {
			if(dir == PFIL_IN)
				siftr_num_inbound_skipped_pkts_malloc++;
			else
				siftr_num_outbound_skipped_pkts_malloc++;

			goto ret6;
		}

		m_tag_prepend(*m, tag);
	}

	if (dir == PFIL_IN)
		siftr_num_inbound_tcp_pkts++;
	else
		siftr_num_outbound_tcp_pkts++;

	ip6_hl = sizeof(struct ip6_hdr);

	/*
	 * Create a tcphdr struct starting at the correct offset
	 * in the ipv6 packet. ip->ip_hl gives the ip header length
	 * in 4-byte words, so multiply it to get the size in bytes
	 */
	th = (struct tcphdr *)((caddr_t)ip6 + ip6_hl);

	/*
	 * For inbound packets, the pfil hooks don't provide a pointer to the
	 * IP control block, so we need to find it ourselves and lock it
	 */
	if (!inp) {
		/* Find the the corresponding inpcb for this pkt */

		/* We need the tcbinfo lock */
#if (__FreeBSD_version >= 700000)
		INP_INFO_UNLOCK_ASSERT(_siftrtcbinfo);
#endif
		INP_INFO_RLOCK(_siftrtcbinfo);

		if (dir == PFIL_IN)
			inp = in6_pcblookup_hash(_siftrtcbinfo,
						&ip6->ip6_src,
						th->th_sport,
						&ip6->ip6_dst,
						th->th_dport,
						0,
						(*m)->m_pkthdr.rcvif
			);
		else
			inp = in6_pcblookup_hash(_siftrtcbinfo,
						&ip6->ip6_dst,
						th->th_dport,
						&ip6->ip6_src,
						th->th_sport,
						0,
						(*m)->m_pkthdr.rcvif
			);

		/* If we can't find the IP control block, bail */
		if (!inp) {
			if(dir == PFIL_IN)
				siftr_num_inbound_skipped_pkts_icb++;
			else
				siftr_num_outbound_skipped_pkts_icb++;

			INP_INFO_RUNLOCK(_siftrtcbinfo);
			goto ret6;
		}

		/* Acquire the inpcb lock */
#if (__FreeBSD_version >= 701000)
		INP_RLOCK(inp);
#else
		INP_LOCK(inp);
#endif
		INP_INFO_RUNLOCK(_siftrtcbinfo);
		inp_locally_locked = 1;
	}

	pkt_node = malloc(sizeof(struct pkt_node), M_SIFTR_PKTNODE, M_NOWAIT | M_ZERO);
	
	if (pkt_node == NULL) {

		if(dir == PFIL_IN)
			siftr_num_inbound_skipped_pkts_malloc++;
		else
			siftr_num_outbound_skipped_pkts_malloc++;

		goto inp_unlock6;
	}

	/* Find the TCP control block that corresponds with this packet */
	tp = intotcpcb(inp);

	/*
	 * If we can't find the TCP control block (happens occasionaly for a
	 * packet sent during the shutdown phase of a TCP connection),
	 * or we're in the timewait state, bail
	 */
#if (INP_TIMEWAIT == 0x8)
	if (!tp || (inp->inp_vflag & INP_TIMEWAIT)) {
#else
	if (!tp || (inp->inp_flags & INP_TIMEWAIT)) {
#endif
		if(dir == PFIL_IN)
			siftr_num_inbound_skipped_pkts_tcb++;
		else
			siftr_num_outbound_skipped_pkts_tcb++;

		free(pkt_node, M_SIFTR_PKTNODE);
		goto inp_unlock6;
	}

	/* Fill in pkt_node data */
	pkt_node->ip_laddr[0] = inp->in6p_laddr.s6_addr32[0];
	pkt_node->ip_laddr[1] = inp->in6p_laddr.s6_addr32[1];
	pkt_node->ip_laddr[2] = inp->in6p_laddr.s6_addr32[2];
	pkt_node->ip_laddr[3] = inp->in6p_laddr.s6_addr32[3];
	pkt_node->ip_faddr[0] = inp->in6p_faddr.s6_addr32[0];
	pkt_node->ip_faddr[1] = inp->in6p_faddr.s6_addr32[1];
	pkt_node->ip_faddr[2] = inp->in6p_faddr.s6_addr32[2];
	pkt_node->ip_faddr[3] = inp->in6p_faddr.s6_addr32[3];
	pkt_node->ipver = INP_IPV6;
	pkt_node->tcp_localport = inp->inp_lport;
	pkt_node->tcp_foreignport = inp->inp_fport;
	pkt_node->snd_cwnd = tp->snd_cwnd;
	pkt_node->snd_wnd = tp->snd_wnd;
	pkt_node->rcv_wnd = tp->rcv_wnd;
	pkt_node->snd_bwnd = tp->snd_bwnd;
	pkt_node->snd_ssthresh = tp->snd_ssthresh;
	pkt_node->snd_scale = tp->snd_scale;
	pkt_node->rcv_scale = tp->rcv_scale;
	pkt_node->conn_state = tp->t_state;
	pkt_node->max_seg_size = tp->t_maxseg;
	pkt_node->smoothed_rtt = tp->t_srtt;
#if (__FreeBSD_version >= 700000)
	pkt_node->sack_enabled = tp->t_flags & TF_SACK_PERMIT;
#else
	pkt_node->sack_enabled = tp->sack_enable;
#endif
	pkt_node->flags = tp->t_flags;
	pkt_node->rxt_length = tp->t_rxtcur;
	pkt_node->snd_buf_hiwater = inp->inp_socket->so_snd.sb_hiwat;
	pkt_node->snd_buf_cc = inp->inp_socket->so_snd.sb_cc;
	pkt_node->rcv_buf_hiwater = inp->inp_socket->so_rcv.sb_hiwat;
	pkt_node->rcv_buf_cc = inp->inp_socket->so_rcv.sb_cc;
	pkt_node->sent_inflight_bytes = tp->snd_max - tp->snd_una;

	/* We've finished accessing the tcb so release the lock */
	if (inp_locally_locked)
#if (__FreeBSD_version >= 701000)
		INP_RUNLOCK(inp);
#else
		INP_UNLOCK(inp);
#endif

	pkt_node->direction = dir;

	/*
	 * Significantly more accurate than using getmicrotime(), but slower!
	 * Gives true microsecond resolution at the expense of a hit to
	 * maximum pps throughput processing when SIFTR is loaded and enabled.
	 */
	microtime(&(pkt_node->tval));

	/* XXX: Figure out how to do hash calcs for IPv6 */

	mtx_lock(&siftr_pkt_queue_mtx);
	STAILQ_INSERT_TAIL(&pkt_queue, pkt_node, nodes);
	mtx_unlock(&siftr_pkt_queue_mtx);
	goto ret6;

inp_unlock6:
	if (inp_locally_locked)
#if (__FreeBSD_version >= 701000)
		INP_RUNLOCK(inp);
#else
		INP_UNLOCK(inp);
#endif

ret6:
	/* Returning 0 ensures pfil will not discard the pkt */
	return 0;
}
#endif /* #ifdef SIFTR_IPV6 */


static int
siftr_pfil(int action)
{
	struct pfil_head *pfh_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
#ifdef SIFTR_IPV6
	struct pfil_head *pfh_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
#endif

	if(action == HOOK) {
		pfil_add_hook(	siftr_chkpkt,
				NULL, PFIL_IN | PFIL_OUT | PFIL_WAITOK,
				pfh_inet
		);
#ifdef SIFTR_IPV6
		pfil_add_hook(	siftr_chkpkt6,
				NULL, PFIL_IN | PFIL_OUT | PFIL_WAITOK,
				pfh_inet6
		);
#endif
	}
	else if(action == UNHOOK) {
		pfil_remove_hook(siftr_chkpkt,
				NULL, PFIL_IN | PFIL_OUT | PFIL_WAITOK,
				pfh_inet
		);
#ifdef SIFTR_IPV6
		pfil_remove_hook(siftr_chkpkt6,
				NULL, PFIL_IN | PFIL_OUT | PFIL_WAITOK,
				pfh_inet6
		);
#endif
	}

	return 0;
}


static int
siftr_sysctl_logfile_name_handler(SYSCTL_HANDLER_ARGS)
{
	struct alq *new_alq;

	if (!req->newptr)
		goto skip;

	/* If old filename and new filename are different */
	if (strncmp(siftr_logfile, (char *)req->newptr, PATH_MAX)) {

		int error = alq_open(	&new_alq,
					req->newptr,
					curthread->td_ucred,
					SIFTR_LOG_FILE_MODE,
					SIFTR_ALQ_BUFLEN,
					0
		);

		/* Bail if unable to create new alq */
		if (error)
			return 1;

		/*
		 * If disabled, siftr_alq == NULL so we simply close
		 * the alq as we've proved it can be opened.
		 * If enabled, close the existing alq and switch the old
		 * for the new
		 */
		if (siftr_alq == NULL)
			alq_close(new_alq);
		else {
			alq_close(siftr_alq);
			siftr_alq = new_alq;
		}
	}

skip:
	return sysctl_handle_string(oidp, arg1, arg2, req);
}

static int
siftr_manage_ops(uint8_t action)
{
	int i, key_index, ret, error = 0;
	struct timeval tval;
	uint8_t *key;
	uint8_t ipver;
	uint16_t lport, fport;
	struct flow_hash_node *counter, *tmp_counter;
	struct sbuf *s = NULL;

#ifdef SIFTR_IPV6
	uint32_t laddr[4];
	uint32_t faddr[4];
#else
	uint8_t laddr[4];
	uint8_t faddr[4];
#endif

	/* Init an autosizing sbuf that initially holds 200 chars */
	if ((s = sbuf_new(NULL, NULL, 200, SBUF_AUTOEXTEND)) == NULL)
		return -1;

	if (action == SIFTR_ENABLE) {

		/*
		 * Create our alq
		 * XXX: We should abort if alq_open fails!
		 */
		alq_open(	&siftr_alq,
				siftr_logfile,
				curthread->td_ucred,
				SIFTR_LOG_FILE_MODE,
				SIFTR_ALQ_BUFLEN,
				0
		);

		STAILQ_INIT(&pkt_queue);

		siftr_exit_pkt_manager_thread = 0;

#if (__FreeBSD_version >= 800000)
		ret = kthread_add(	&siftr_pkt_manager_thread,
					NULL,
					NULL,
					&siftr_pkt_manager_thr,
					RFNOWAIT,
					0,
					"siftr_pkt_manager_thr"
		);
#else
		ret = kthread_create(	&siftr_pkt_manager_thread,
					NULL,
					&siftr_pkt_manager_proc,
					RFNOWAIT,
					0,
					"siftr_pkt_manager_thr"
		);
		siftr_pkt_manager_thr = FIRST_THREAD_IN_PROC(siftr_pkt_manager_proc);
#endif

		siftr_pfil(HOOK);

		microtime(&tval);

		sbuf_printf(s,
#if (__FreeBSD_version >= 700000)
			"enable_time_secs=%zd\tenable_time_usecs=%06ld\tsiftrver=%s\thz=%u\ttcp_rtt_scale=%u\tsysname=%s\tsysver=%u\tipmode=%u\n",
#else
			"enable_time_secs=%ld\tenable_time_usecs=%06ld\tsiftrver=%s\thz=%u\ttcp_rtt_scale=%u\tsysname=%s\tsysver=%u\tipmode=%u\n",
#endif
			tval.tv_sec,
			tval.tv_usec,
			MODVERSION,
			hz,
			TCP_RTT_SCALE,
			SYS_NAME,
			__FreeBSD_version,
			SIFTR_IPMODE
		);

		sbuf_finish(s);
		alq_writen(siftr_alq, sbuf_data(s), sbuf_len(s), ALQ_WAITOK);
	}
	else if (action == SIFTR_DISABLE && siftr_pkt_manager_thr != NULL) {

		/*
		 * Remove the pfil hook functions. All threads currently in
		 * the hook functions are allowed to exit before siftr_pfil()
		 * returns.
		 */
		siftr_pfil(UNHOOK);

		/* This will block until the pkt manager thread unlocks it */
		mtx_lock(&siftr_pkt_mgr_mtx);

		/* Tell the pkt manager thread that it should exit now */
		siftr_exit_pkt_manager_thread = 1;

		/*
		 * Wake the pkt_manager thread so it realises that
		 * siftr_exit_pkt_manager_thread == 1 and exits gracefully
		 * The wakeup won't be delivered until we unlock
		 * siftr_pkt_mgr_mtx so this isn't racy
		 */
		wakeup(&wait_for_pkt);

		/* Wait for the pkt_manager thread to exit */
#if (__FreeBSD_version >= 800000)
		msleep(	siftr_pkt_manager_thr,
			&siftr_pkt_mgr_mtx,
			PWAIT,
			"thrwait",
			0
		);
#else
		msleep(	siftr_pkt_manager_proc,
			&siftr_pkt_mgr_mtx,
			PWAIT,
			"thrwait",
			0
		);
		siftr_pkt_manager_proc = NULL;
#endif

		siftr_pkt_manager_thr = NULL;
		mtx_unlock(&siftr_pkt_mgr_mtx);

		microtime(&tval);
	
		sbuf_printf(s,
#if (__FreeBSD_version >= 700000)
			"disable_time_secs=%zd\tdisable_time_usecs=%06ld\tnum_inbound_tcp_pkts=%u\tnum_outbound_tcp_pkts=%u\ttotal_tcp_pkts=%u\tnum_inbound_skipped_pkts_malloc=%u\tnum_outbound_skipped_pkts_malloc=%u\tnum_inbound_skipped_pkts_mtx=%u\tnum_outbound_skipped_pkts_mtx=%u\tnum_inbound_skipped_pkts_tcb=%u\tnum_outbound_skipped_pkts_tcb=%u\tnum_inbound_skipped_pkts_icb=%u\tnum_outbound_skipped_pkts_icb=%u\ttotal_skipped_tcp_pkts=%u\tflow_list=",
#else
			"disable_time_secs=%ld\tdisable_time_usecs=%06ld\tnum_inbound_tcp_pkts=%u\tnum_outbound_tcp_pkts=%u\ttotal_tcp_pkts=%u\tnum_inbound_skipped_pkts_malloc=%u\tnum_outbound_skipped_pkts_malloc=%u\tnum_inbound_skipped_pkts_mtx=%u\tnum_outbound_skipped_pkts_mtx=%u\tnum_inbound_skipped_pkts_tcb=%u\tnum_outbound_skipped_pkts_tcb=%u\tnum_inbound_skipped_pkts_icb=%u\tnum_outbound_skipped_pkts_icb=%u\ttotal_skipped_tcp_pkts=%u\tflow_list=",
#endif
			tval.tv_sec,
			tval.tv_usec,
			siftr_num_inbound_tcp_pkts,
			siftr_num_outbound_tcp_pkts,
			TOTAL_TCP_PKTS,
			siftr_num_inbound_skipped_pkts_malloc,
			siftr_num_outbound_skipped_pkts_malloc,
			siftr_num_inbound_skipped_pkts_mtx,
			siftr_num_outbound_skipped_pkts_mtx,
			siftr_num_inbound_skipped_pkts_tcb,
			siftr_num_outbound_skipped_pkts_tcb,
			siftr_num_inbound_skipped_pkts_icb,
			siftr_num_outbound_skipped_pkts_icb,
			TOTAL_SKIPPED_TCP_PKTS
		);
	
		/*
		 * Iterate over the flow hash, printing a summary of each
		 * flow seen and freeing any malloc'd memory.
		 * The hash consists of an array of LISTs (man 3 queue)
		 */
		for(i = 0; i < siftr_hashmask; i++) {
			LIST_FOREACH_SAFE(	counter,
						counter_hash+i,
						nodes,
						tmp_counter) {
				key = (counter->key);
				key_index = 1;

				ipver = key[0];

				memcpy(laddr, key + key_index, sizeof(laddr));
				key_index += sizeof(laddr);
				
				memcpy(	&lport,
					key + key_index,
					sizeof(lport)
				);
				key_index += sizeof(lport);
				
				memcpy(faddr, key + key_index, sizeof(faddr));
				key_index += sizeof(faddr);
				
				memcpy(	&fport,
					key + key_index,
					sizeof(fport)
				);
	
#ifdef SIFTR_IPV6
				laddr[3] = ntohl(laddr[3]);
				faddr[3] = ntohl(faddr[3]);

				if (ipver == INP_IPV6) {
					laddr[0] = ntohl(laddr[0]);
					laddr[1] = ntohl(laddr[1]);
					laddr[2] = ntohl(laddr[2]);
					faddr[0] = ntohl(faddr[0]);
					faddr[1] = ntohl(faddr[1]);
					faddr[2] = ntohl(faddr[2]);

					sbuf_printf(s,
						"%x:%x:%x:%x:%x:%x:%x:%x;%u-%x:%x:%x:%x:%x:%x:%x:%x;%u,",
						UPPER_SHORT(laddr[0]),
						LOWER_SHORT(laddr[0]),
						UPPER_SHORT(laddr[1]),
						LOWER_SHORT(laddr[1]),
						UPPER_SHORT(laddr[2]),
						LOWER_SHORT(laddr[2]),
						UPPER_SHORT(laddr[3]),
						LOWER_SHORT(laddr[3]),
						ntohs(lport),
						UPPER_SHORT(faddr[0]),
						LOWER_SHORT(faddr[0]),
						UPPER_SHORT(faddr[1]),
						LOWER_SHORT(faddr[1]),
						UPPER_SHORT(faddr[2]),
						LOWER_SHORT(faddr[2]),
						UPPER_SHORT(faddr[3]),
						LOWER_SHORT(faddr[3]),
						ntohs(fport)
					);
				} else {
					laddr[0] = FIRST_OCTET(laddr[3]);
					laddr[1] = SECOND_OCTET(laddr[3]);
					laddr[2] = THIRD_OCTET(laddr[3]);
					laddr[3] = FOURTH_OCTET(laddr[3]);
					faddr[0] = FIRST_OCTET(faddr[3]);
					faddr[1] = SECOND_OCTET(faddr[3]);
					faddr[2] = THIRD_OCTET(faddr[3]);
					faddr[3] = FOURTH_OCTET(faddr[3]);
#endif
					sbuf_printf(s,
						"%u.%u.%u.%u;%u-%u.%u.%u.%u;%u,",
						laddr[0],
						laddr[1],
						laddr[2],
						laddr[3],
						ntohs(lport),
						faddr[0],
						faddr[1],
						faddr[2],
						faddr[3],
						ntohs(fport)
					);
#ifdef SIFTR_IPV6
				}
#endif
	
				free(counter, M_SIFTR_HASHNODE);
			}
	
			LIST_INIT(counter_hash+i);
		}

		sbuf_printf(s, "\n");
		sbuf_finish(s);
		alq_writen(siftr_alq, sbuf_data(s), sbuf_len(s), ALQ_WAITOK);
		alq_close(siftr_alq);
		siftr_alq = NULL;
	}

	sbuf_delete(s);

	/* Temporary debugging */
	/*printf("sizeof(struct pkt_node)=%lu,sizeof(struct m_tag)=%lu,siftr_num_inbound_skipped_pkts_dejavu=%u,siftr_num_outbound_skipped_pkts_dejavu=%u\n", sizeof(struct pkt_node), sizeof(struct m_tag), siftr_num_inbound_skipped_pkts_dejavu, siftr_num_outbound_skipped_pkts_dejavu);*/

	/*
	 * XXX: Should be using ret to check if any functions fail
	 * and set error appropriately
	 */

	return error;
}

static int
siftr_sysctl_enabled_handler(SYSCTL_HANDLER_ARGS)
{
	if (!req->newptr)
		goto skip;
	
	/* If the value passed in isn't 0 or 1, return an error */
	if (CAST_PTR_INT(req->newptr) != 0 && CAST_PTR_INT(req->newptr) != 1)
		return 1;
	
	/* If we are changing state (0 to 1 or 1 to 0) */
	if (CAST_PTR_INT(req->newptr) != siftr_enabled )
		if(siftr_manage_ops(CAST_PTR_INT(req->newptr))) {
			siftr_manage_ops(SIFTR_DISABLE);
			return 1;
		}

skip:
	return sysctl_handle_int(oidp, arg1, arg2, req);
}



static int
siftr_sysctl_pkts_per_log_handler(SYSCTL_HANDLER_ARGS)
{
	if (!req->newptr)
		goto skip;

	/* Bogus value, return an error */
	if (CAST_PTR_INT(req->newptr) <= 0)
		return 1;

skip:
	return sysctl_handle_int(oidp, arg1, arg2, req);
}

static void
siftr_shutdown_handler(void *arg)
{
	siftr_manage_ops(SIFTR_DISABLE);
}

/*
 * Module is being unloaded or machine is shutting down. Take care of cleanup.
 */
static int
deinit_siftr(void)
{
	/* Cleanup */
	siftr_manage_ops(SIFTR_DISABLE);
	hashdestroy(counter_hash, M_SIFTR, siftr_hashmask);
	mtx_destroy(&siftr_pkt_queue_mtx);
	mtx_destroy(&siftr_pkt_mgr_mtx);
	free(log_writer_msg_buf, M_SIFTR);

	return 0;
}

/*
 * Module has just been loaded into the kernel.
 */
static int
init_siftr(void)
{
	EVENTHANDLER_REGISTER(	shutdown_pre_sync,
				siftr_shutdown_handler,
				NULL,
				SHUTDOWN_PRI_FIRST
	);
	
	/* Initialise our flow counter hash table */
	counter_hash = hashinit(SIFTR_EXPECTED_MAX_TCP_FLOWS,
				M_SIFTR,
				&siftr_hashmask
	);

	/*
	 * Create a buffer to hold log messages
	 * before they get written to disk
	 */
	log_writer_msg_buf = malloc(SIFTR_ALQ_BUFLEN, M_SIFTR, M_WAITOK|M_ZERO);

	mtx_init(&siftr_pkt_queue_mtx, "siftr_pkt_queue_mtx", NULL, MTX_DEF);
	mtx_init(&siftr_pkt_mgr_mtx, "siftr_pkt_mgr_mtx", NULL, MTX_DEF);

	/* Print message to the user's current terminal */
	uprintf("\nStatistical Information For TCP Research (SIFTR) %s\n          http://caia.swin.edu.au/urp/newtcp\n\n", MODVERSION);

	return 0;
}

/*
 * This is the function that is called to load and unload the module.
 * When the module is loaded, this function is called once with
 * "what" == MOD_LOAD
 * When the module is unloaded, this function is called twice with
 * "what" = MOD_QUIESCE first, followed by "what" = MOD_UNLOAD second
 * When the system is shut down e.g. CTRL-ALT-DEL or using the shutdown command,
 * this function is called once with "what" = MOD_SHUTDOWN
 * When the system is shut down, the handler isn't called until the very end
 * of the shutdown sequence i.e. after the disks have been synced.
 */
static int siftr_load_handler(module_t mod, int what, void *arg)
{
	switch(what) {
		case MOD_LOAD:
			return init_siftr();
			break;
		
		case MOD_QUIESCE:
		case MOD_SHUTDOWN:
			return deinit_siftr();
			break;
		
		case MOD_UNLOAD:
			return 0;
			break;
		
		default:
			return EINVAL;
			break;
	}
}

/* Basic module data */
static moduledata_t siftr_mod =
{
	"siftr",
	siftr_load_handler, /* Execution entry point for the module */
	NULL
};

/*
 * Param 1: name of the kernel module
 * Param 2: moduledata_t struct containing info about the kernel module
 *          and the execution entry point for the module
 * Param 3: From sysinit_sub_id enumeration in /usr/include/sys/kernel.h
 *          Defines the module initialisation order
 * Param 4: From sysinit_elem_order enumeration in /usr/include/sys/kernel.h
 *          Defines the initialisation order of this kld relative to others
 *          within the same subsystem as defined by param 3
 */
DECLARE_MODULE(siftr, siftr_mod, SI_SUB_SMP, SI_ORDER_ANY);
MODULE_VERSION(siftr, 1);
MODULE_DEPEND(siftr, alq, 1, 1, 1);


/* Declare the static net.inet.siftr sysctl tree and populate it */

SYSCTL_DECL(_net_inet_siftr);

SYSCTL_NODE(	_net_inet,
		OID_AUTO,
		siftr,
		CTLFLAG_RW,
		NULL,
		"siftr related settings"
);

SYSCTL_OID(	_net_inet_siftr,
		OID_AUTO,
		enabled,
		CTLTYPE_UINT|CTLFLAG_RW,
		&siftr_enabled,
		0,
		&siftr_sysctl_enabled_handler,
		"IU",
		"switch siftr module operations on/off"
);

SYSCTL_OID(	_net_inet_siftr,
		OID_AUTO,
		ppl,
		CTLTYPE_UINT|CTLFLAG_RW,
		&siftr_pkts_per_log,
		1,
		&siftr_sysctl_pkts_per_log_handler,
		"IU",
		"number of packets between generating a log message"
);

SYSCTL_PROC(	_net_inet_siftr,
		OID_AUTO,
		logfile,
		CTLTYPE_STRING|CTLFLAG_RW,
		&siftr_logfile,
		sizeof(siftr_logfile),
		&siftr_sysctl_logfile_name_handler,
		"A",
		"file to save siftr log messages to"
);
