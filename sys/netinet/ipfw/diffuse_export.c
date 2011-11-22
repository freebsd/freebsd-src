/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Functions to manage the export protocol.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_inet.h"
#ifndef INET
#error DIFFUSE requires INET.
#endif /* INET */
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#define	WITH_DIP_INFO 1
#include <netinet/ip_diffuse_export.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_private.h>

static VNET_DEFINE(uint32_t, ex_max_qsize);
#define	V_ex_max_qsize VNET(ex_max_qsize)

static uma_zone_t di_rec_zone;

#ifndef __FreeBSD__
DEFINE_SPINLOCK(di_er_mtx);
#else
static struct mtx di_er_mtx; /* Mutex guarding dynamic rules. */
#endif

#define	DI_ER_LOCK()		mtx_lock(&di_er_mtx)
#define	DI_ER_UNLOCK()		mtx_unlock(&di_er_mtx)
#define	DI_ER_LOCK_ASSERT()	mtx_assert(&di_er_mtx, MA_OWNED)
#define	DI_ER_LOCK_DESTROY()	mtx_destroy(&di_er_mtx)
#define	DI_ER_LOCK_INIT()	mtx_init(&di_er_mtx, \
    "DIFFUSE export record list", NULL, MTX_DEF)

/*
 * Length of the fixed size, per-packet header on outgoing flow rule template
 * based export packets.
 * The packet header consists of the following parts (in order):
 * - struct dip_header
 * - struct dip_set_header
 * - struct dip_templ_header
 * - A uint16_t ID field for each information element (IE)
 * - A uint16_t length field for variable length IEs (there are currently 4)
 */
#define	DIP_FIXED_HDR_LEN (sizeof(struct dip_header) + \
    sizeof(struct dip_set_header) + sizeof(struct dip_templ_header) + \
    (N_DEFAULT_FLOWRULE_TEMPLATE_ITEMS * sizeof(uint16_t)) + \
    4 * sizeof(uint16_t))

/* Size for one data set. */
static int def_data_size;

/* Offset into mhead where the data set header starts. */
static int def_data_shdr_offs;

/* Template for packet header. */
static struct mbuf *mhead;

#ifdef SYSCTL_NODE
SYSBEGIN(xxx)
SYSCTL_DECL(_net_inet_ip_diffuse);
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ex_max_qsize, CTLFLAG_RW,
    &VNET_NAME(ex_max_qsize), 0, "Max export record queue size");
SYSEND
#endif /* SYSCTL_NODE */

/* Compute static size of one data set according to template. */
static int
_get_data_size(void)
{
	int i, n, size;

	size = 0;

	for (i = 0; i < N_DEFAULT_FLOWRULE_TEMPLATE_ITEMS; i++) {
		n = dip_info[def_flowrule_template[i]].len;
		if (n > 0)
			size += n;
		else if (n == 0) {
			if (def_flowrule_template[i] == DIP_IE_ACTION ||
			    def_flowrule_template[i] == DIP_IE_EXPORT_NAME ||
			    def_flowrule_template[i] == DIP_IE_CLASSIFIER_NAME) {
				size += DI_MAX_NAME_STR_LEN;
			} else if (def_flowrule_template[i] == DIP_IE_ACTION_PARAMS) {
				size += DI_MAX_PARAM_STR_LEN;
			}
		}
		/* Do not count dynamic length fields here. */
	}

	return (size);
}

/* Compute dynamic size of record r. */
static int
get_data_size(struct di_export_rec *r)
{
	int i, l, slen;

	l = 0;

	for (i = 0; i < r->tcnt; i++) {
		slen = strlen(r->class_tags[i].cname);
		/* String, null, class. */
		l += slen + 1 + dip_info[DIP_IE_CLASS_LABEL].len;
	}
	l++; /* Field length byte. */

	return (def_data_size + l);
}

static inline uint32_t
tv_sub0_ms(struct timeval *num, struct timeval *sub)
{
	struct timeval rv;

	rv = tv_sub0(num, sub);

	return (tvtoms(&rv));
}

static void
remove_rec(struct di_export_rec *r)
{

	DI_ER_LOCK_ASSERT();

	TAILQ_REMOVE(&di_config.export_rec_list, r, next);
	uma_zfree(di_rec_zone, r);
	di_config.export_rec_count--;
}

struct di_export_rec *
diffuse_export_add_rec(struct di_ft_entry *q, struct di_export *ex,
    int add_command)
{
	struct di_export_rec *r, *s;
	struct di_flow_class *c;
	int have_entry;

	r = NULL;
	have_entry = 0;

	/* We don't handle exporting v6 flow records yet. */
	if (q->id.addr_type == 6)
		return (NULL);

	DI_ER_LOCK();

	/* Update and export entry if we have one for this flow already. */
	TAILQ_FOREACH(s, &di_config.export_rec_list, next) {
		/*
		 * Only compare pointer for speed. If new flow with same 5-tuple
		 * we may add another record for same 5-tuple
		 */
		if (s->ft_rec == q) {
			have_entry = 1;
			r = s;
			break;
		}
	}

	if (!have_entry) {
		r = uma_zalloc(di_rec_zone, M_NOWAIT | M_ZERO);
		if (r == NULL) {
			DI_ER_UNLOCK();
			return (NULL);
		}
		strcpy(r->ename, ex->name);
		r->ft_rec = q;
	}

	getmicrotime(&r->time);
	r->id = q->id;
	r->fcnt = q->fcnt;
	r->ftype = q->ftype;
	r->mtype = add_command ? DIP_MSG_ADD : DIP_MSG_REMOVE;
	r->ttype = di_config.an_rule_removal;
	r->pcnt = q->pcnt;
	/*
	 * The flow byte count we send across the wire is in KBytes
	 * (see DIP_IE_KBYTE_CNT in <sys/netinet/ip_diffuse_export.h>).
	 */
	r->bcnt = q->bcnt / 1000;
	if (add_command) {
		r->tval = (uint16_t) (q->expire > time_uptime) ?
		    q->expire - time_uptime : 0;
		r->tval++; /* Make it slightly larger. */
	} else {
		r->tval = 0;
	}

	/* Class tags (only confirmed!). */
	r->tcnt = 0;
	SLIST_FOREACH(c, &q->flow_classes, next) {
		if (r->tcnt >= DI_MAX_CLASSES)
			break;

		if (c->confirm >= ex->conf.confirm) {
			strcpy(r->class_tags[r->tcnt].cname, c->cname);
			r->class_tags[r->tcnt].class = c->class;
			r->tcnt++;
		}
	}

	if (!have_entry) {
		if (r->ftype & DI_FLOW_TYPE_BIDIRECTIONAL ||
		    ex->conf.atype & DI_ACTION_TYPE_BIDIRECTIONAL)
			r->action_dir = DI_ACTION_TYPE_BIDIRECTIONAL;
		else
			r->action_dir = DI_ACTION_TYPE_UNIDIRECTIONAL;

		strcpy(r->action, ex->conf.action);
		strcpy(r->act_params, ex->conf.action_param);
		TAILQ_INSERT_TAIL(&di_config.export_rec_list, r, next);
		di_config.export_rec_count++;
		r->no_earlier.tv_sec = r->no_earlier.tv_usec = 0;
	}

	DI_ER_UNLOCK();

	return (r);
}

/* Limit total number of records. */
void
diffuse_export_prune_recs(void)
{
	struct di_export_rec *r;

	DI_ER_LOCK();

	if (V_ex_max_qsize > 65535)
		V_ex_max_qsize = 65535;

	if (V_ex_max_qsize < 0)
		V_ex_max_qsize = 0;

	while (di_config.export_rec_count > V_ex_max_qsize) {
		r = TAILQ_FIRST(&di_config.export_rec_list);
		remove_rec(r);
	}

	DI_ER_UNLOCK();
}

int
diffuse_export_remove_recs(char *ename)
{
	struct di_export_rec *r, *tmp;

	DI_ER_LOCK();

	TAILQ_FOREACH_SAFE(r, &di_config.export_rec_list, next, tmp) {
		if (ename == NULL || !strcmp(r->ename, ename))
			remove_rec(r);
	}

	DI_ER_UNLOCK();

	return (0);
}

/* Open and bind socket. */
struct socket *
diffuse_export_open(struct di_export_config *conf)
{
	struct socket *sock;
	struct thread *td;
	int ret;

	sock = NULL;
	td = curthread;

	if (mhead != NULL) {
		/* Open UDP socket. */
		ret = socreate(AF_INET, &sock, SOCK_DGRAM, IPPROTO_UDP,
		    td->td_ucred, td);
		if (ret)
			DID("socket create error %d", ret);
	}

	return (sock);
}

/*
 * Prepare the packet header for later use. Every packet contains one rule spec
 * template followed by data.
 */
static void
prepare_header(void)
{
	struct dip_header *hdr;
	struct dip_set_header *shdr;
	struct dip_templ_header *thdr;
	int i, offs;
	char *buf;

	/*
	 * Ensures we will be able to shoehorn the entire fixed length header
	 * into a single mbuf.
	 */
	CTASSERT(MHLEN >= DIP_FIXED_HDR_LEN);

	offs = 0;

	mhead = m_gethdr(M_WAITOK, MT_DATA);
	mhead->m_next = NULL;
	buf = mtod(mhead, char *);
	hdr = (struct dip_header *)buf;
	hdr->version = htons((uint16_t)DIP_VERSION);
	hdr->msg_len = 0;
	hdr->seq_no = 0;
	hdr->time = 0;
	offs += sizeof(struct dip_header);

	shdr = (struct dip_set_header *)(buf + offs);
	shdr->set_id = htons((uint16_t)DIP_SET_ID_FLOWRULE_TPL);
	shdr->set_len = 0;
	offs += sizeof(struct dip_set_header);

	thdr = (struct dip_templ_header *)(buf + offs);
	thdr->templ_id = htons((uint16_t)DIP_SET_ID_DATA);
	thdr->flags = 0;
	offs += sizeof(struct dip_templ_header);

	for (i = 0; i < N_DEFAULT_FLOWRULE_TEMPLATE_ITEMS; i++) {
		*((uint16_t *)(buf + offs)) =
		    htons(dip_info[def_flowrule_template[i]].id);
		offs += sizeof(uint16_t);
		if (def_flowrule_template[i] == DIP_IE_ACTION ||
		    def_flowrule_template[i] == DIP_IE_EXPORT_NAME ||
		    def_flowrule_template[i] == DIP_IE_CLASSIFIER_NAME) {
			*((uint16_t *)(buf + offs)) =
			    htons((uint16_t)DI_MAX_NAME_STR_LEN);
			offs += sizeof(uint16_t);
		} else if (def_flowrule_template[i] == DIP_IE_ACTION_PARAMS) {
			*((uint16_t *)(buf + offs)) =
			    htons((uint16_t)DI_MAX_PARAM_STR_LEN);
			offs += sizeof(uint16_t);
		}
	}

	shdr->set_len = htons(offs - sizeof(struct dip_header));
	def_data_shdr_offs = offs;
	shdr = (struct dip_set_header *)(buf + offs);
	shdr->set_id = htons((uint16_t)DIP_SET_ID_DATA);
	shdr->set_len = htons(sizeof(struct dip_set_header));
	offs += sizeof(struct dip_set_header);

	hdr->msg_len = htons(offs);
	mhead->m_len = offs;

	m_fixhdr(mhead);
	mhead->m_pkthdr.rcvif = NULL;
}

/*
 * If r is not NULL we add data and possibly send if packet length reached.
 * If r is NULL we just send packet if we have an mbuf.
 */
static void
add_record(struct di_export *ex, struct di_export_rec *r,
    struct timeval *tv, int dyn_rsize)
{
	struct dip_header *hdr;
	struct dip_set_header *shdr;
	struct di_export_config *conf;
	struct mbuf *md;
	char *buf;
	int i, new_header, offs, slen;
	unsigned char *lfield;

	DI_ER_LOCK_ASSERT();

	conf = &ex->conf;
	new_header = offs = 0;

	if (ex->mh == NULL) {
		ex->mh = m_dup(mhead, MT_DATA);
		ex->mh->m_next = NULL;
		ex->mh->m_nextpkt = NULL;
		ex->mt = ex->mh;
		/* Make sure IPFW/DIFFUSE skips exporter packets. */
		ex->mh->m_flags |= M_SKIP_FIREWALL;
		new_header = 1;
	}

	/* Update stuff in packet header. */
	buf = mtod(ex->mh, char *);
	hdr = (struct dip_header *)buf;
	if (new_header)
		hdr->seq_no = htonl(ex->seq_no++);

	hdr->time = htonl(tv->tv_sec);
	hdr->msg_len = htons(ntohs(hdr->msg_len) + dyn_rsize);

	/* Update stuff in data set header. */
	shdr = (struct dip_set_header *)(buf + def_data_shdr_offs);
	shdr->set_len = htons(ntohs(shdr->set_len) + dyn_rsize);

	/* Find space for data, add new mbuf if required. */
	if (ex->mt == ex->mh || (MLEN - ex->mt->m_len) < dyn_rsize) {
		/* Add new mbuf to chain. */
		md = m_get(M_NOWAIT, MT_DATA);
		if (!md)
			return;
		md->m_next = NULL;
		ex->mt->m_next = md;
		ex->mt = md;
	}

	buf = mtod(ex->mt, char *);
	offs = ex->mt->m_len;

	/* Fill in data. */
	/* XXX: Create function with a switch for all IEs. */
	memcpy((char *)(buf + offs), r->ename, DI_MAX_NAME_STR_LEN);
	offs += DI_MAX_NAME_STR_LEN;
	*((uint8_t *)(buf + offs)) = r->mtype;
	offs += sizeof(uint8_t);
	*((uint32_t *)(buf + offs)) = htonl(r->id.src_ip);
	offs += sizeof(uint32_t);
	*((uint32_t *)(buf + offs)) = htonl(r->id.dst_ip);
	offs += sizeof(uint32_t);
	*((uint16_t *)(buf + offs)) = htons(r->id.src_port);
	offs += sizeof(uint16_t);
	*((uint16_t *)(buf + offs)) = htons(r->id.dst_port);
	offs += sizeof(uint16_t);
	*((uint8_t *)(buf + offs)) = r->id.proto;
	offs += sizeof(uint8_t);
	*((uint32_t *)(buf + offs)) = htonl(r->pcnt);
	offs += sizeof(uint32_t);
	*((uint32_t *)(buf + offs)) = htonl(r->bcnt);
	offs += sizeof(uint32_t);

	lfield = buf + offs;
	offs++;
	*lfield = 1;
	/*
	 * tcnt will be <= DI_MAX_CLASSES which is set so that lfield will
	 * never overflow.
	 */
	for (i = 0; i < r->tcnt; i++) {
		slen = strlen(r->class_tags[i].cname);
		memcpy((char *)(buf + offs), r->class_tags[i].cname, slen + 1);
		offs += slen + 1;
		*((uint16_t *)(buf + offs)) = htons(r->class_tags[i].class);
		offs += sizeof(uint16_t);
		KASSERT((*lfield + slen + 1 + sizeof(uint16_t) <
		    (1 << sizeof(*lfield))), ("%s: lfield overflowed",
		    __func__));
		*lfield += slen + 1 + sizeof(uint16_t);
	}
	*((uint8_t *)(buf + offs)) = r->ttype;
	offs += sizeof(uint8_t);
	*((uint16_t *)(buf + offs)) = htons(r->tval);
	offs += sizeof(uint16_t);
	memcpy((char *)(buf + offs), r->action, DI_MAX_NAME_STR_LEN);
	offs += DI_MAX_NAME_STR_LEN;
	*((uint16_t *)(buf + offs)) = htons((uint16_t)r->action_dir);
	offs += sizeof(uint16_t);
	memcpy((char *)(buf + offs), r->act_params, DI_MAX_PARAM_STR_LEN);
	offs += DI_MAX_PARAM_STR_LEN;

	ex->mt->m_len = offs;

	/* Fix chain header, e.g. adjust chain length. */
	m_fixhdr(ex->mh);
	ex->mh->m_pkthdr.rcvif = NULL;
}

static inline int
queue_tx_pkt_if(struct di_export *ex, int dyn_rsize, struct timeval *tv,
    struct mbuf **tx_pkt_queue, int force)
{
	struct route sro;
	struct sockaddr_in *dst;
	unsigned long mtu;
	int ready_to_send;

	DI_ER_LOCK_ASSERT();

	ready_to_send = 0;
	mtu = DIP_DEFAULT_MTU;

	bzero(&sro, sizeof(sro));
	dst = (struct sockaddr_in *)&sro.ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr = ex->conf.ip;
	in_rtalloc_ign(&sro, 0, ex->sock->so_fibnum);

	if (sro.ro_rt != NULL) {
		if (sro.ro_rt->rt_rmx.rmx_mtu == 0)
			mtu = sro.ro_rt->rt_ifp->if_mtu;
		else
			mtu = min(sro.ro_rt->rt_rmx.rmx_mtu,
			    sro.ro_rt->rt_ifp->if_mtu);
		RTFREE(sro.ro_rt);
	}

	/*
	 * If the export packet currently being constructed (chain headed by
	 * ex->mh) would be overfilled by adding a record of size dyn_rsize,
	 * move the chain to the tx_pkt_queue and set the ex chain headers to
	 * NULL so that add_record() will start a new chain.
	 */
	if (force || (ex->mt->m_len + dyn_rsize) >=
	    (mtu - sizeof(struct ip) + sizeof(struct udphdr))) {
		/* Add to queue. */
		if (*tx_pkt_queue == NULL) {
			*tx_pkt_queue = ex->mh;
		} else {
			while ((*tx_pkt_queue)->m_nextpkt != NULL)
				tx_pkt_queue = &((*tx_pkt_queue)->m_nextpkt);

			(*tx_pkt_queue)->m_nextpkt = ex->mh;
		}

		ex->mh = NULL;
		ex->mt = NULL;
		ex->last_pkt_time = *tv;
		ready_to_send = 1;
	}

	return (ready_to_send);
}

int
diffuse_export_send(struct di_export *ex)
{
	static int waiting = 0; /* Number of records waiting to be sent. */
	struct thread *td;
	struct di_export_rec *r, *tmp;
	struct di_export_config *conf;
	struct timeval tv;
	/* Copies for sending outside lock. */
	struct socket *sock;
	struct sockaddr_in sin;
	struct mbuf *next_tx_pkt, *tx_pkt_queue;
	int cnt, dyn_rsize, ret;

	td = curthread;
	sock = NULL;
	tx_pkt_queue = NULL;
	conf = &ex->conf;
	cnt = waiting; /* Number of records processed */

	DI_ER_LOCK();

	if (di_config.export_rec_count == 0 ||
	    waiting + di_config.export_rec_count < conf->min_batch) {
		DI_ER_UNLOCK();
		return (0);
	}

	getmicrotime(&tv);

	/* Export the records that are over max delay, if max_delay set. */
	if (conf->max_delay > 0) {
		TAILQ_FOREACH_SAFE(r, &di_config.export_rec_list, next, tmp) {
			if (tv_sub0_ms(&tv, &r->time) >= conf->max_delay &&
			    tv_sub0_ms(&tv, &r->no_earlier) > 0) {
				dyn_rsize = get_data_size(r);
				if (queue_tx_pkt_if(ex, dyn_rsize, &tv,
				    &tx_pkt_queue, 0) == 1)
					waiting = 0;
				else
					waiting++;

				add_record(ex, r, &tv, dyn_rsize);
				remove_rec(r);
				cnt++;
			}
		}
	}

	/* Export up to max_batch or if max_batch is not set export the rest. */
	if ((conf->max_batch == 0 || cnt < conf->max_batch) &&
	    di_config.export_rec_count > 0) {
		TAILQ_FOREACH_SAFE(r, &di_config.export_rec_list, next, tmp) {
			if (tv_sub0_ms(&tv, &r->no_earlier) > 0) {
				dyn_rsize = get_data_size(r);
				if (queue_tx_pkt_if(ex, dyn_rsize, &tv,
				    &tx_pkt_queue, 0) == 1)
					waiting = 0;
				else
					waiting++;

				add_record(ex, r, &tv, dyn_rsize);
				remove_rec(r);
				cnt++;
			}
			if ((conf->max_batch > 0 && cnt >= conf->max_batch) ||
			    di_config.export_rec_count == 0)
				break;
		}
	}

	DI_ER_UNLOCK();

	if (waiting > 0 && waiting >= conf->min_batch) {
		/* Force send of incomplete packet. */
		queue_tx_pkt_if(ex, 0, &tv, &tx_pkt_queue, 1);
		waiting = 0;
	}

	if (tx_pkt_queue != NULL) {
		sock = ex->sock;
		/* Set target. */
		memset(&sin, 0, sizeof(sin));
#ifndef __linux__
		sin.sin_len = sizeof(sin);
#endif
		sin.sin_family = AF_INET;
		sin.sin_port = htons(conf->port);
		sin.sin_addr = conf->ip;
	}

	DID("exported %d rules", cnt);

	while (tx_pkt_queue != NULL) {
		ret = sosend(sock, (struct sockaddr *)&sin, NULL, tx_pkt_queue, NULL,
		    MSG_DONTWAIT, td);
		DID("send packet %d\n", ret);
		next_tx_pkt = tx_pkt_queue->m_nextpkt;
		m_freem(tx_pkt_queue);
		tx_pkt_queue = next_tx_pkt;
	}

	return (0);
}

/* Close socket. */
void
diffuse_export_close(struct socket *sock)
{

	soclose(sock);
}

void
diffuse_export_init(void)
{

	V_ex_max_qsize = 256;

	di_rec_zone = uma_zcreate("DIFFUSE rule recs",
	    sizeof(struct di_export_rec), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	DI_ER_LOCK_INIT();

	/* Determine size of one entry. */
	def_data_size = _get_data_size();

	/* Prepare packet header. */
	prepare_header();
}

void
diffuse_export_uninit(void)
{

	/* Free packet header. */
	if (mhead)
		m_freem(mhead);

	uma_zdestroy(di_rec_zone);
	DI_ER_LOCK_DESTROY();
}
