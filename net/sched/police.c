/*
 * net/sched/police.c	Input police filter.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

#define L2T(p,L)   ((p)->R_tab->data[(L)>>(p)->R_tab->rate.cell_log])
#define L2T_P(p,L) ((p)->P_tab->data[(L)>>(p)->P_tab->rate.cell_log])

static u32 idx_gen;
static struct tcf_police *tcf_police_ht[16];
/* Policer hash table lock */
static rwlock_t police_lock = RW_LOCK_UNLOCKED;

/* Each policer is serialized by its individual spinlock */

static __inline__ unsigned tcf_police_hash(u32 index)
{
	return index&0xF;
}

static __inline__ struct tcf_police * tcf_police_lookup(u32 index)
{
	struct tcf_police *p;

	read_lock(&police_lock);
	for (p = tcf_police_ht[tcf_police_hash(index)]; p; p = p->next) {
		if (p->index == index)
			break;
	}
	read_unlock(&police_lock);
	return p;
}

static __inline__ u32 tcf_police_new_index(void)
{
	do {
		if (++idx_gen == 0)
			idx_gen = 1;
	} while (tcf_police_lookup(idx_gen));

	return idx_gen;
}


void tcf_police_destroy(struct tcf_police *p)
{
	unsigned h = tcf_police_hash(p->index);
	struct tcf_police **p1p;
	
	for (p1p = &tcf_police_ht[h]; *p1p; p1p = &(*p1p)->next) {
		if (*p1p == p) {
			write_lock_bh(&police_lock);
			*p1p = p->next;
			write_unlock_bh(&police_lock);
#ifdef CONFIG_NET_ESTIMATOR
			qdisc_kill_estimator(&p->stats);
#endif
			if (p->R_tab)
				qdisc_put_rtab(p->R_tab);
			if (p->P_tab)
				qdisc_put_rtab(p->P_tab);
			kfree(p);
			return;
		}
	}
	BUG_TRAP(0);
}

struct tcf_police * tcf_police_locate(struct rtattr *rta, struct rtattr *est)
{
	unsigned h;
	struct tcf_police *p;
	struct rtattr *tb[TCA_POLICE_MAX];
	struct tc_police *parm;

	if (rtattr_parse(tb, TCA_POLICE_MAX, RTA_DATA(rta), RTA_PAYLOAD(rta)) < 0)
		return NULL;

	if (tb[TCA_POLICE_TBF-1] == NULL)
		return NULL;

	parm = RTA_DATA(tb[TCA_POLICE_TBF-1]);

	if (parm->index && (p = tcf_police_lookup(parm->index)) != NULL) {
		p->refcnt++;
		return p;
	}

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return NULL;

	memset(p, 0, sizeof(*p));
	p->refcnt = 1;
	spin_lock_init(&p->lock);
	p->stats.lock = &p->lock;
	if (parm->rate.rate) {
		if ((p->R_tab = qdisc_get_rtab(&parm->rate, tb[TCA_POLICE_RATE-1])) == NULL)
			goto failure;
		if (parm->peakrate.rate &&
		    (p->P_tab = qdisc_get_rtab(&parm->peakrate, tb[TCA_POLICE_PEAKRATE-1])) == NULL)
			goto failure;
	}
	if (tb[TCA_POLICE_RESULT-1])
		p->result = *(int*)RTA_DATA(tb[TCA_POLICE_RESULT-1]);
#ifdef CONFIG_NET_ESTIMATOR
	if (tb[TCA_POLICE_AVRATE-1])
		p->ewma_rate = *(u32*)RTA_DATA(tb[TCA_POLICE_AVRATE-1]);
#endif
	p->toks = p->burst = parm->burst;
	p->mtu = parm->mtu;
	if (p->mtu == 0) {
		p->mtu = ~0;
		if (p->R_tab)
			p->mtu = 255<<p->R_tab->rate.cell_log;
	}
	if (p->P_tab)
		p->ptoks = L2T_P(p, p->mtu);
	PSCHED_GET_TIME(p->t_c);
	p->index = parm->index ? : tcf_police_new_index();
	p->action = parm->action;
#ifdef CONFIG_NET_ESTIMATOR
	if (est)
		qdisc_new_estimator(&p->stats, est);
#endif
	h = tcf_police_hash(p->index);
	write_lock_bh(&police_lock);
	p->next = tcf_police_ht[h];
	tcf_police_ht[h] = p;
	write_unlock_bh(&police_lock);
	return p;

failure:
	if (p->R_tab)
		qdisc_put_rtab(p->R_tab);
	kfree(p);
	return NULL;
}

int tcf_police(struct sk_buff *skb, struct tcf_police *p)
{
	psched_time_t now;
	long toks;
	long ptoks = 0;

	spin_lock(&p->lock);

	p->stats.bytes += skb->len;
	p->stats.packets++;

#ifdef CONFIG_NET_ESTIMATOR
	if (p->ewma_rate && p->stats.bps >= p->ewma_rate) {
		p->stats.overlimits++;
		spin_unlock(&p->lock);
		return p->action;
	}
#endif

	if (skb->len <= p->mtu) {
		if (p->R_tab == NULL) {
			spin_unlock(&p->lock);
			return p->result;
		}

		PSCHED_GET_TIME(now);

		toks = PSCHED_TDIFF_SAFE(now, p->t_c, p->burst, 0);

		if (p->P_tab) {
			ptoks = toks + p->ptoks;
			if (ptoks > (long)L2T_P(p, p->mtu))
				ptoks = (long)L2T_P(p, p->mtu);
			ptoks -= L2T_P(p, skb->len);
		}
		toks += p->toks;
		if (toks > (long)p->burst)
			toks = p->burst;
		toks -= L2T(p, skb->len);

		if ((toks|ptoks) >= 0) {
			p->t_c = now;
			p->toks = toks;
			p->ptoks = ptoks;
			spin_unlock(&p->lock);
			return p->result;
		}
	}

	p->stats.overlimits++;
	spin_unlock(&p->lock);
	return p->action;
}

int tcf_police_dump(struct sk_buff *skb, struct tcf_police *p)
{
	unsigned char	 *b = skb->tail;
	struct tc_police opt;

	opt.index = p->index;
	opt.action = p->action;
	opt.mtu = p->mtu;
	opt.burst = p->burst;
	if (p->R_tab)
		opt.rate = p->R_tab->rate;
	else
		memset(&opt.rate, 0, sizeof(opt.rate));
	if (p->P_tab)
		opt.peakrate = p->P_tab->rate;
	else
		memset(&opt.peakrate, 0, sizeof(opt.peakrate));
	RTA_PUT(skb, TCA_POLICE_TBF, sizeof(opt), &opt);
	if (p->result)
		RTA_PUT(skb, TCA_POLICE_RESULT, sizeof(int), &p->result);
#ifdef CONFIG_NET_ESTIMATOR
	if (p->ewma_rate)
		RTA_PUT(skb, TCA_POLICE_AVRATE, 4, &p->ewma_rate);
#endif
	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}
