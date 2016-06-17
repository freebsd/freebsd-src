/*
 * IPVS         Application module
 *
 * Version:     $Id: ip_vs_app.c,v 1.14 2001/11/23 14:34:10 wensong Exp $
 *
 * Authors:     Wensong Zhang <wensong@linuxvirtualserver.org>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Most code here is taken from ip_masq_app.c in kernel 2.2. The difference
 * is that ip_vs_app module handles the reverse direction (incoming requests
 * and outgoing responses). The ip_vs_app modules are only used for VS/NAT.
 *
 *		IP_MASQ_APP application masquerading module
 *
 * Author:	Juan Jose Ciarlante, <jjciarla@raiz.uncu.edu.ar>
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <net/protocol.h>
#include <asm/system.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>

#include <net/ip_vs.h>

#define IP_VS_APP_TAB_SIZE  16          /* must be power of 2 */

#define IP_VS_APP_HASH(proto, port) ((port^proto) & (IP_VS_APP_TAB_SIZE-1))
#define IP_VS_APP_TYPE(proto, port) (proto<<16 | port)
#define IP_VS_APP_PORT(type)        (type & 0xffff)
#define IP_VS_APP_PROTO(type)       ((type>>16) & 0x00ff)


EXPORT_SYMBOL(register_ip_vs_app);
EXPORT_SYMBOL(unregister_ip_vs_app);


/*
 *	will hold ipvs app. hashed list heads
 */
static struct list_head ip_vs_app_base[IP_VS_APP_TAB_SIZE];

/* lock for ip_vs_app table */
static rwlock_t __ip_vs_app_lock = RW_LOCK_UNLOCKED;


/*
 *	ip_vs_app registration routine
 *	port: host byte order.
 */
int register_ip_vs_app(struct ip_vs_app *vapp,
		       unsigned short proto, __u16 port)
{
	unsigned hash;

	if (!vapp) {
		IP_VS_ERR("register_ip_vs_app(): NULL arg\n");
		return -EINVAL;
	}

	MOD_INC_USE_COUNT;

	vapp->type = IP_VS_APP_TYPE(proto, port);
	hash = IP_VS_APP_HASH(proto, port);

	write_lock_bh(&__ip_vs_app_lock);
	list_add(&vapp->n_list, &ip_vs_app_base[hash]);
	write_unlock_bh(&__ip_vs_app_lock);

	return 0;
}


/*
 *	ip_vs_app unregistration routine.
 */
int unregister_ip_vs_app(struct ip_vs_app *vapp)
{
	if (!vapp) {
		IP_VS_ERR("unregister_ip_vs_app(): NULL arg\n");
		return -EINVAL;
	}

	write_lock_bh(&__ip_vs_app_lock);
	list_del(&vapp->n_list);
	write_unlock_bh(&__ip_vs_app_lock);

	MOD_DEC_USE_COUNT;

	return 0;
}


/*
 *	get ip_vs_app object by its proto and port (net byte order).
 */
static struct ip_vs_app * ip_vs_app_get(unsigned short proto, __u16 port)
{
	struct list_head *e;
	struct ip_vs_app *vapp;
	unsigned hash;
	unsigned type;

	port = ntohs(port);
	type = IP_VS_APP_TYPE(proto, port);
	hash = IP_VS_APP_HASH(proto, port);

	read_lock_bh(&__ip_vs_app_lock);

	list_for_each(e, &ip_vs_app_base[hash]) {
		vapp = list_entry(e, struct ip_vs_app, n_list);

		/*
		 * Test and MOD_INC_USE_COUNT atomically
		 */
		if (vapp->module && !try_inc_mod_count(vapp->module)) {
			/*
			 * This application module is just deleted
			 */
			continue;
		}
		if (type == vapp->type) {
			read_unlock_bh(&__ip_vs_app_lock);
			return vapp;
		}

		if (vapp->module)
			__MOD_DEC_USE_COUNT(vapp->module);
	}

	read_unlock_bh(&__ip_vs_app_lock);
	return NULL;
}


/*
 *	Bind ip_vs_conn to its ip_vs_app based on proto and dport,
 *	and call the ip_vs_app constructor.
 */
struct ip_vs_app * ip_vs_bind_app(struct ip_vs_conn *cp)
{
	struct ip_vs_app *vapp;

	/* no need to bind app if its forwarding method is not NAT */
	if (IP_VS_FWD_METHOD(cp) != IP_VS_CONN_F_MASQ)
		return NULL;

	if (cp->protocol != IPPROTO_TCP && cp->protocol != IPPROTO_UDP)
		return NULL;

	/*
	 *	don't allow binding if already bound
	 */
	if (cp->app != NULL) {
		IP_VS_ERR("ip_vs_bind_app(): "
			  "called for already bound object.\n");
		return cp->app;
	}

	vapp = ip_vs_app_get(cp->protocol, cp->vport);

	if (vapp != NULL) {
		cp->app = vapp;

		if (vapp->init_conn)
			vapp->init_conn(vapp, cp);
	}
	return vapp;
}


/*
 *	Unbind cp from type object and call cp destructor (does not kfree()).
 */
int ip_vs_unbind_app(struct ip_vs_conn *cp)
{
	struct ip_vs_app *vapp = cp->app;

	if (cp->protocol != IPPROTO_TCP && cp->protocol != IPPROTO_UDP)
		return 0;

	if (vapp != NULL) {
		if (vapp->done_conn)
			vapp->done_conn(vapp, cp);
		cp->app = NULL;
		if (vapp->module)
			__MOD_DEC_USE_COUNT(vapp->module);
	}
	return (vapp != NULL);
}


/*
 *	Fixes th->seq based on ip_vs_seq info.
 */
static inline void vs_fix_seq(const struct ip_vs_seq *vseq, struct tcphdr *th)
{
	__u32 seq = ntohl(th->seq);

	/*
	 *	Adjust seq with delta-offset for all packets after
	 *	the most recent resized pkt seq and with previous_delta offset
	 *	for all packets	before most recent resized pkt seq.
	 */
	if (vseq->delta || vseq->previous_delta) {
		if(after(seq, vseq->init_seq)) {
			th->seq = htonl(seq + vseq->delta);
			IP_VS_DBG(9, "vs_fix_seq(): added delta (%d) to seq\n",
				  vseq->delta);
		} else {
			th->seq = htonl(seq + vseq->previous_delta);
			IP_VS_DBG(9, "vs_fix_seq(): added previous_delta "
				  "(%d) to seq\n", vseq->previous_delta);
		}
	}
}


/*
 *	Fixes th->ack_seq based on ip_vs_seq info.
 */
static inline void
vs_fix_ack_seq(const struct ip_vs_seq *vseq, struct tcphdr *th)
{
	__u32 ack_seq = ntohl(th->ack_seq);

	/*
	 * Adjust ack_seq with delta-offset for
	 * the packets AFTER most recent resized pkt has caused a shift
	 * for packets before most recent resized pkt, use previous_delta
	 */
	if (vseq->delta || vseq->previous_delta) {
		/* since ack_seq is the number of octet that is expected
		   to receive next, so compare it with init_seq+delta */
		if(after(ack_seq, vseq->init_seq+vseq->delta)) {
			th->ack_seq = htonl(ack_seq - vseq->delta);
			IP_VS_DBG(9, "vs_fix_ack_seq(): subtracted delta "
				  "(%d) from ack_seq\n", vseq->delta);

		} else {
			th->ack_seq = htonl(ack_seq - vseq->previous_delta);
			IP_VS_DBG(9, "vs_fix_ack_seq(): subtracted "
				  "previous_delta (%d) from ack_seq\n",
				  vseq->previous_delta);
		}
	}
}


/*
 *	Updates ip_vs_seq if pkt has been resized
 *	Assumes already checked proto==IPPROTO_TCP and diff!=0.
 */
static inline void vs_seq_update(struct ip_vs_conn *cp, struct ip_vs_seq *vseq,
				 unsigned flag, __u32 seq, int diff)
{
	/* spinlock is to keep updating cp->flags atomic */
	spin_lock(&cp->lock);
	if ( !(cp->flags & flag) || after(seq, vseq->init_seq)) {
		vseq->previous_delta = vseq->delta;
		vseq->delta += diff;
		vseq->init_seq = seq;
		cp->flags |= flag;
	}
	spin_unlock(&cp->lock);
}


/*
 *	Output pkt hook. Will call bound ip_vs_app specific function
 *	called by ip_vs_out(), assumes previously checked cp!=NULL
 *	returns (new - old) skb->len diff.
 */
int ip_vs_app_pkt_out(struct ip_vs_conn *cp, struct sk_buff *skb)
{
	struct ip_vs_app *vapp;
	int diff;
	struct iphdr *iph;
	struct tcphdr *th;
	__u32 seq;

	/*
	 *	check if application module is bound to
	 *	this ip_vs_conn.
	 */
	if ((vapp = cp->app) == NULL)
		return 0;

	iph = skb->nh.iph;
	th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);

	/*
	 *	Remember seq number in case this pkt gets resized
	 */
	seq = ntohl(th->seq);

	/*
	 *	Fix seq stuff if flagged as so.
	 */
	if (cp->protocol == IPPROTO_TCP) {
		if (cp->flags & IP_VS_CONN_F_OUT_SEQ)
			vs_fix_seq(&cp->out_seq, th);
		if (cp->flags & IP_VS_CONN_F_IN_SEQ)
			vs_fix_ack_seq(&cp->in_seq, th);
	}

	/*
	 *	Call private output hook function
	 */
	if (vapp->pkt_out == NULL)
		return 0;

	diff = vapp->pkt_out(vapp, cp, skb);

	/*
	 *	Update ip_vs seq stuff if len has changed.
	 */
	if (diff != 0 && cp->protocol == IPPROTO_TCP)
		vs_seq_update(cp, &cp->out_seq,
			      IP_VS_CONN_F_OUT_SEQ, seq, diff);

	return diff;
}


/*
 *	Input pkt hook. Will call bound ip_vs_app specific function
 *	called by ip_fw_demasquerade(), assumes previously checked cp!=NULL.
 *	returns (new - old) skb->len diff.
 */
int ip_vs_app_pkt_in(struct ip_vs_conn *cp, struct sk_buff *skb)
{
	struct ip_vs_app *vapp;
	int diff;
	struct iphdr *iph;
	struct tcphdr *th;
	__u32 seq;

	/*
	 *	check if application module is bound to
	 *	this ip_vs_conn.
	 */
	if ((vapp = cp->app) == NULL)
		return 0;

	iph = skb->nh.iph;
	th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);

	/*
	 *	Remember seq number in case this pkt gets resized
	 */
	seq = ntohl(th->seq);

	/*
	 *	Fix seq stuff if flagged as so.
	 */
	if (cp->protocol == IPPROTO_TCP) {
		if (cp->flags & IP_VS_CONN_F_IN_SEQ)
			vs_fix_seq(&cp->in_seq, th);
		if (cp->flags & IP_VS_CONN_F_OUT_SEQ)
			vs_fix_ack_seq(&cp->out_seq, th);
	}

	/*
	 *	Call private input hook function
	 */
	if (vapp->pkt_in == NULL)
		return 0;

	diff = vapp->pkt_in(vapp, cp, skb);

	/*
	 *	Update ip_vs seq stuff if len has changed.
	 */
	if (diff != 0 && cp->protocol == IPPROTO_TCP)
		vs_seq_update(cp, &cp->in_seq,
			      IP_VS_CONN_F_IN_SEQ, seq, diff);

	return diff;
}


/*
 *	/proc/net/ip_vs_app entry function
 */
static int ip_vs_app_getinfo(char *buffer, char **start, off_t offset,
			     int length)
{
	off_t pos=0;
	int len=0;
	char temp[64];
	int idx;
	struct ip_vs_app *vapp;
	struct list_head *e;

	pos = 64;
	if (pos > offset) {
		len += sprintf(buffer+len, "%-63s\n",
			       "prot port    usecnt name");
	}

	read_lock_bh(&__ip_vs_app_lock);
	for (idx=0 ; idx < IP_VS_APP_TAB_SIZE; idx++) {
		list_for_each (e, &ip_vs_app_base[idx]) {
			vapp = list_entry(e, struct ip_vs_app, n_list);

			pos += 64;
			if (pos <= offset)
				continue;
			sprintf(temp, "%-3s  %-7u %-6d %-17s",
				ip_vs_proto_name(IP_VS_APP_PROTO(vapp->type)),
				IP_VS_APP_PORT(vapp->type),
				vapp->module?GET_USE_COUNT(vapp->module):0,
				vapp->name);
			len += sprintf(buffer+len, "%-63s\n", temp);
			if (pos >= offset+length)
				goto done;
		}
	}
  done:
	read_unlock_bh(&__ip_vs_app_lock);

	*start = buffer+len-(pos-offset);       /* Start of wanted data */
	len = pos-offset;
	if (len > length)
		len = length;
	if (len < 0)
		len = 0;
	return len;
}


/*
 *	Replace a segment of data with a new segment
 */
int ip_vs_skb_replace(struct sk_buff *skb, int pri,
		      char *o_buf, int o_len, char *n_buf, int n_len)
{
	struct iphdr *iph;
	int diff;
	int o_offset;
	int o_left;

	EnterFunction(9);

	diff = n_len - o_len;
	o_offset = o_buf - (char *)skb->data;
	/* The length of left data after o_buf+o_len in the skb data */
	o_left = skb->len - (o_offset + o_len);

	if (diff <= 0) {
		memmove(o_buf + n_len, o_buf + o_len, o_left);
		memcpy(o_buf, n_buf, n_len);
		skb_trim(skb, skb->len + diff);
	} else if (diff <= skb_tailroom(skb)) {
		skb_put(skb, diff);
		memmove(o_buf + n_len, o_buf + o_len, o_left);
		memcpy(o_buf, n_buf, n_len);
	} else {
		if (pskb_expand_head(skb, skb_headroom(skb), diff, pri))
			return -ENOMEM;
		skb_put(skb, diff);
		memmove(skb->data + o_offset + n_len,
			skb->data + o_offset + o_len, o_left);
		memcpy(skb->data + o_offset, n_buf, n_len);
	}

	/* must update the iph total length here */
	iph = skb->nh.iph;
	iph->tot_len = htons(skb->len);

	LeaveFunction(9);
	return 0;
}


int ip_vs_app_init(void)
{
	int idx;

	for (idx=0 ; idx < IP_VS_APP_TAB_SIZE; idx++) {
		INIT_LIST_HEAD(&ip_vs_app_base[idx]);
	}

	/* we will replace it with proc_net_ipvs_create() soon */
	proc_net_create("ip_vs_app", 0, ip_vs_app_getinfo);
	return 0;
}

void ip_vs_app_cleanup(void)
{
	proc_net_remove("ip_vs_app");
}
