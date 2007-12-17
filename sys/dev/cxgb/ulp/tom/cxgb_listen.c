/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>


#include <dev/cxgb/cxgb_osdep.h>
#include <dev/cxgb/sys/mbufq.h>

#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>

#include <netinet/tcp_offload.h>
#include <net/route.h>

#include <dev/cxgb/t3cdev.h>
#include <dev/cxgb/common/cxgb_firmware_exports.h>
#include <dev/cxgb/common/cxgb_t3_cpl.h>
#include <dev/cxgb/common/cxgb_tcb.h>
#include <dev/cxgb/common/cxgb_ctl_defs.h>
#include <dev/cxgb/cxgb_l2t.h>
#include <dev/cxgb/cxgb_offload.h>
#include <dev/cxgb/ulp/toecore/cxgb_toedev.h>
#include <dev/cxgb/ulp/tom/cxgb_defs.h>
#include <dev/cxgb/ulp/tom/cxgb_tom.h>
#include <dev/cxgb/ulp/tom/cxgb_t3_ddp.h>
#include <dev/cxgb/ulp/tom/cxgb_toepcb.h>


static struct listen_info *listen_hash_add(struct tom_data *d, struct socket *so, unsigned int stid);
static int listen_hash_del(struct tom_data *d, struct socket *so);

/*
 * Process a CPL_CLOSE_LISTSRV_RPL message.  If the status is good we release
 * the STID.
 */
static int
do_close_server_rpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
	struct cpl_close_listserv_rpl *rpl = cplhdr(m);
	unsigned int stid = GET_TID(rpl);

	if (rpl->status != CPL_ERR_NONE)
		log(LOG_ERR, "Unexpected CLOSE_LISTSRV_RPL status %u for "
		       "STID %u\n", rpl->status, stid);
	else {
		struct listen_ctx *listen_ctx = (struct listen_ctx *)ctx;

		cxgb_free_stid(cdev, stid);
		free(listen_ctx, M_CXGB);
	}

	return (CPL_RET_BUF_DONE);
}

/*
 * Process a CPL_PASS_OPEN_RPL message.  Remove the socket from the listen hash
 * table and free the STID if there was any error, otherwise nothing to do.
 */
static int
do_pass_open_rpl(struct t3cdev *cdev, struct mbuf *m, void *ctx)
{
       	struct cpl_pass_open_rpl *rpl = cplhdr(m);

	if (rpl->status != CPL_ERR_NONE) {
		int stid = GET_TID(rpl);
		struct listen_ctx *listen_ctx = (struct listen_ctx *)ctx;
		struct tom_data *d = listen_ctx->tom_data;
		struct socket *lso = listen_ctx->lso;

#if VALIDATE_TID
		if (!lso)
			return (CPL_RET_UNKNOWN_TID | CPL_RET_BUF_DONE);
#endif
		/*
		 * Note: It is safe to unconditionally call listen_hash_del()
		 * at this point without risking unhashing a reincarnation of
		 * an already closed socket (i.e., there is no listen, close,
		 * listen, free the sock for the second listen while processing
		 * a message for the first race) because we are still holding
		 * a reference on the socket.  It is possible that the unhash
		 * will fail because the socket is already closed, but we can't
		 * unhash the wrong socket because it is impossible for the
		 * socket to which this message refers to have reincarnated.
		 */
		listen_hash_del(d, lso);
		cxgb_free_stid(cdev, stid);
#ifdef notyet
		/*
		 * XXX need to unreference the inpcb
		 * but we have no way of knowing that other TOMs aren't referencing it 
		 */
		sock_put(lso);
#endif
		free(listen_ctx, M_CXGB);
	}
	return CPL_RET_BUF_DONE;
}

void
t3_init_listen_cpl_handlers(void)
{
	t3tom_register_cpl_handler(CPL_PASS_OPEN_RPL, do_pass_open_rpl);
	t3tom_register_cpl_handler(CPL_CLOSE_LISTSRV_RPL, do_close_server_rpl);
}

static inline int
listen_hashfn(const struct socket *so)
{
	return ((unsigned long)so >> 10) & (LISTEN_INFO_HASH_SIZE - 1);
}

/*
 * Create and add a listen_info entry to the listen hash table.  This and the
 * listen hash table functions below cannot be called from softirqs.
 */
static struct listen_info *
listen_hash_add(struct tom_data *d, struct socket *so, unsigned int stid)
{
	struct listen_info *p;

	p = malloc(sizeof(*p), M_CXGB, M_NOWAIT|M_ZERO);
	if (p) {
		int bucket = listen_hashfn(so);

		p->so = so;	/* just a key, no need to take a reference */
		p->stid = stid;
		mtx_lock(&d->listen_lock);		
		p->next = d->listen_hash_tab[bucket];
		d->listen_hash_tab[bucket] = p;
		mtx_unlock(&d->listen_lock);
	}
	return p;
}

#if 0
/*
 * Given a pointer to a listening socket return its server TID by consulting
 * the socket->stid map.  Returns -1 if the socket is not in the map.
 */
static int
listen_hash_find(struct tom_data *d, struct socket *so)
{
	int stid = -1, bucket = listen_hashfn(so);
	struct listen_info *p;

	spin_lock(&d->listen_lock);
	for (p = d->listen_hash_tab[bucket]; p; p = p->next)
		if (p->sk == sk) {
			stid = p->stid;
			break;
		}
	spin_unlock(&d->listen_lock);
	return stid;
}
#endif

/*
 * Delete the listen_info structure for a listening socket.  Returns the server
 * TID for the socket if it is present in the socket->stid map, or -1.
 */
static int
listen_hash_del(struct tom_data *d, struct socket *so)
{
	int bucket, stid = -1;
	struct listen_info *p, **prev;

	bucket = listen_hashfn(so);
	prev  = &d->listen_hash_tab[bucket];

	mtx_lock(&d->listen_lock);
	for (p = *prev; p; prev = &p->next, p = p->next)
		if (p->so == so) {
			stid = p->stid;
			*prev = p->next;
			free(p, M_CXGB);
			break;
		}
	mtx_unlock(&d->listen_lock);
	
	return (stid);
}

/*
 * Start a listening server by sending a passive open request to HW.
 */
void
t3_listen_start(struct toedev *dev, struct socket *so, struct t3cdev *cdev)
{
	int stid;
	struct mbuf *m;
	struct cpl_pass_open_req *req;
	struct tom_data *d = TOM_DATA(dev);
	struct inpcb *inp = sotoinpcb(so);
	struct listen_ctx *ctx;

	if (!TOM_TUNABLE(dev, activated))
		return;

	printf("start listen\n");
	
	ctx = malloc(sizeof(*ctx), M_CXGB, M_NOWAIT);

	if (!ctx)
		return;

	ctx->tom_data = d;
	ctx->lso = so;
	ctx->ulp_mode = 0; /* DDP if the default */
	LIST_INIT(&ctx->synq_head);
	
	stid = cxgb_alloc_stid(d->cdev, d->client, ctx);
	if (stid < 0)
		goto free_ctx;

#ifdef notyet
	/*
	 * XXX need to mark inpcb as referenced
	 */
	sock_hold(sk);
#endif
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		goto free_stid;
	m->m_pkthdr.len = m->m_len = sizeof(*req);
	
	if (!listen_hash_add(d, so, stid))
		goto free_all;

	req = mtod(m, struct cpl_pass_open_req *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_PASS_OPEN_REQ, stid));
	req->local_port = inp->inp_lport; 
	memcpy(&req->local_ip, &inp->inp_laddr, 4);
	req->peer_port = 0;
	req->peer_ip = 0;
	req->peer_netmask = 0;
	req->opt0h = htonl(F_DELACK | F_TCAM_BYPASS);
	req->opt0l = htonl(V_RCV_BUFSIZ(16));
	req->opt1 = htonl(V_CONN_POLICY(CPL_CONN_POLICY_ASK));

	m_set_priority(m, CPL_PRIORITY_LISTEN); 
	cxgb_ofld_send(cdev, m);
	return;

free_all:
	m_free(m);
free_stid:
	cxgb_free_stid(cdev, stid);
#if 0	
	sock_put(sk);
#endif	
free_ctx:
	free(ctx, M_CXGB);
}

/*
 * Stop a listening server by sending a close_listsvr request to HW.
 * The server TID is freed when we get the reply.
 */
void
t3_listen_stop(struct toedev *dev, struct socket *so, struct t3cdev *cdev)
{
	struct mbuf *m;
	struct cpl_close_listserv_req *req;
	struct listen_ctx *lctx;
	int stid = listen_hash_del(TOM_DATA(dev), so);
	
	if (stid < 0)
		return;

	lctx = cxgb_get_lctx(cdev, stid);
	/*
	 * Do this early so embryonic connections are marked as being aborted
	 * while the stid is still open.  This ensures pass_establish messages
	 * that arrive while we are closing the server will be able to locate
	 * the listening socket.
	 */
	t3_reset_synq(lctx);

	/* Send the close ASAP to stop further passive opens */
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		/*
		 * XXX allocate from lowmem cache
		 */
	}
	m->m_pkthdr.len = m->m_len = sizeof(*req);

	req = mtod(m, struct cpl_close_listserv_req *);
	req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_CLOSE_LISTSRV_REQ, stid));
	req->cpu_idx = 0;
	m_set_priority(m, CPL_PRIORITY_LISTEN);
	cxgb_ofld_send(cdev, m);

	t3_disconnect_acceptq(so);
}
