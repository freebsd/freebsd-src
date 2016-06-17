/*
 * IPVS:        Weighted Round-Robin Scheduling module
 *
 * Version:     $Id: ip_vs_wrr.c,v 1.11 2002/03/25 12:44:35 wensong Exp $
 *
 * Authors:     Wensong Zhang <wensong@linuxvirtualserver.org>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Changes:
 *     Wensong Zhang            :     changed the ip_vs_wrr_schedule to return dest
 *     Wensong Zhang            :     changed some comestics things for debugging
 *     Wensong Zhang            :     changed for the d-linked destination list
 *     Wensong Zhang            :     added the ip_vs_wrr_update_svc
 *     Julian Anastasov         :     fixed the bug of returning destination
 *                                    with weight 0 when all weights are zero
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <net/ip_vs.h>

/*
 * current destination pointer for weighted round-robin scheduling
 */
struct ip_vs_wrr_mark {
	struct list_head *cl;	/* current list head */
	int cw;			/* current weight */
	int mw;			/* maximum weight */
	int di;			/* decreasing interval */
};


/*
 *    Get the gcd of server weights
 */
static int gcd(int a, int b)
{
	int c;

	while ((c = a % b)) {
		a = b;
		b = c;
	}
	return b;
}

static int ip_vs_wrr_gcd_weight(struct ip_vs_service *svc)
{
	register struct list_head *l, *e;
	struct ip_vs_dest *dest;
	int weight;
	int g = 1;

	l = &svc->destinations;
	for (e=l->next; e!=l; e=e->next) {
		dest = list_entry(e, struct ip_vs_dest, n_list);
		weight = atomic_read(&dest->weight);
		if (weight > 0) {
			g = weight;
			break;
		}
	}
	if (e == l)
		return g;

	for (e=e->next; e!=l; e=e->next) {
		dest = list_entry(e, struct ip_vs_dest, n_list);
		weight = atomic_read(&dest->weight);
		if (weight > 0)
			g = gcd(weight, g);
	}

	return g;
}


/*
 *    Get the maximum weight of the service destinations.
 */
static int ip_vs_wrr_max_weight(struct ip_vs_service *svc)
{
	register struct list_head *l, *e;
	struct ip_vs_dest *dest;
	int weight = 0;

	l = &svc->destinations;
	for (e=l->next; e!=l; e=e->next) {
		dest = list_entry(e, struct ip_vs_dest, n_list);
		if (atomic_read(&dest->weight) > weight)
			weight = atomic_read(&dest->weight);
	}

	return weight;
}


static int ip_vs_wrr_init_svc(struct ip_vs_service *svc)
{
	struct ip_vs_wrr_mark *mark;

	/*
	 *    Allocate the mark variable for WRR scheduling
	 */
	mark = kmalloc(sizeof(struct ip_vs_wrr_mark), GFP_ATOMIC);
	if (mark == NULL) {
		IP_VS_ERR("ip_vs_wrr_init_svc(): no memory\n");
		return -ENOMEM;
	}
	mark->cl = &svc->destinations;
	mark->cw = 0;
	mark->mw = ip_vs_wrr_max_weight(svc);
	mark->di = ip_vs_wrr_gcd_weight(svc);
	svc->sched_data = mark;

	return 0;
}


static int ip_vs_wrr_done_svc(struct ip_vs_service *svc)
{
	/*
	 *    Release the mark variable
	 */
	kfree(svc->sched_data);

	return 0;
}


static int ip_vs_wrr_update_svc(struct ip_vs_service *svc)
{
	struct ip_vs_wrr_mark *mark = svc->sched_data;

	mark->cl = &svc->destinations;
	mark->mw = ip_vs_wrr_max_weight(svc);
	mark->di = ip_vs_wrr_gcd_weight(svc);
	return 0;
}


/*
 *    Weighted Round-Robin Scheduling
 */
static struct ip_vs_dest *
ip_vs_wrr_schedule(struct ip_vs_service *svc, struct iphdr *iph)
{
	struct ip_vs_dest *dest;
	struct ip_vs_wrr_mark *mark = svc->sched_data;

	IP_VS_DBG(6, "ip_vs_wrr_schedule(): Scheduling...\n");

	/*
	 * This loop will always terminate, because 0<mark->cw<max_weight,
	 * and at least one server has its weight equal to max_weight.
	 */
	write_lock(&svc->sched_lock);
	while (1) {
		if (mark->cl == &svc->destinations) {
			/* it is at the head of the destination list */

			if (mark->cl == mark->cl->next) {
				/* no dest entry */
				write_unlock(&svc->sched_lock);
				return NULL;
			}

			mark->cl = svc->destinations.next;
			mark->cw -= mark->di;
			if (mark->cw <= 0) {
				mark->cw = mark->mw;
				/*
				 * Still zero, which means no availabe servers.
				 */
				if (mark->cw == 0) {
					mark->cl = &svc->destinations;
					write_unlock(&svc->sched_lock);
					IP_VS_INFO("ip_vs_wrr_schedule(): "
						   "no available servers\n");
					return NULL;
				}
			}
		}
		else mark->cl = mark->cl->next;

		if (mark->cl != &svc->destinations) {
			/* not at the head of the list */
			dest = list_entry(mark->cl, struct ip_vs_dest, n_list);
			if (atomic_read(&dest->weight) >= mark->cw) {
				write_unlock(&svc->sched_lock);
				break;
			}
		}
	}

	IP_VS_DBG(6, "WRR: server %u.%u.%u.%u:%u "
		  "activeconns %d refcnt %d weight %d\n",
		  NIPQUAD(dest->addr), ntohs(dest->port),
		  atomic_read(&dest->activeconns),
		  atomic_read(&dest->refcnt),
		  atomic_read(&dest->weight));

	return	dest;
}


static struct ip_vs_scheduler ip_vs_wrr_scheduler = {
	{0},			/* n_list */
	"wrr",			/* name */
	ATOMIC_INIT(0),		/* refcnt */
	THIS_MODULE,		/* this module */
	ip_vs_wrr_init_svc,	/* service initializer */
	ip_vs_wrr_done_svc,	/* service done */
	ip_vs_wrr_update_svc,	/* service updater */
	ip_vs_wrr_schedule,	/* select a server from the destination list */
};

static int __init ip_vs_wrr_init(void)
{
	INIT_LIST_HEAD(&ip_vs_wrr_scheduler.n_list);
	return register_ip_vs_scheduler(&ip_vs_wrr_scheduler) ;
}

static void __exit ip_vs_wrr_cleanup(void)
{
	unregister_ip_vs_scheduler(&ip_vs_wrr_scheduler);
}

module_init(ip_vs_wrr_init);
module_exit(ip_vs_wrr_cleanup);
MODULE_LICENSE("GPL");
