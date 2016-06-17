/*
 * IPVS:        Least-Connection Scheduling module
 *
 * Version:     $Id: ip_vs_lc.c,v 1.8.2.1 2003/04/11 14:02:35 wensong Exp $
 *
 * Authors:     Wensong Zhang <wensong@linuxvirtualserver.org>
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Changes:
 *     Wensong Zhang            :     added the ip_vs_lc_update_svc
 *     Wensong Zhang            :     added any dest with weight=0 is quiesced
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <net/ip_vs.h>


static int ip_vs_lc_init_svc(struct ip_vs_service *svc)
{
	return 0;
}


static int ip_vs_lc_done_svc(struct ip_vs_service *svc)
{
	return 0;
}


static int ip_vs_lc_update_svc(struct ip_vs_service *svc)
{
	return 0;
}


static inline unsigned int
ip_vs_lc_dest_overhead(struct ip_vs_dest *dest)
{
	/*
	 * We think the overhead of processing active connections is 256
	 * times higher than that of inactive connections in average. (This
	 * 256 times might not be accurate, we will change it later) We
	 * use the following formula to estimate the overhead now:
	 *		  dest->activeconns*256 + dest->inactconns
	 */
	return (atomic_read(&dest->activeconns) << 8) +
		atomic_read(&dest->inactconns);
}


/*
 *	Least Connection scheduling
 */
static struct ip_vs_dest *
ip_vs_lc_schedule(struct ip_vs_service *svc, struct iphdr *iph)
{
	struct list_head *l, *e;
	struct ip_vs_dest *dest, *least;
	unsigned int loh, doh;

	IP_VS_DBG(6, "ip_vs_lc_schedule(): Scheduling...\n");

	/*
	 * Simply select the server with the least number of
	 *        (activeconns<<5) + inactconns
	 * Except whose weight is equal to zero.
	 * If the weight is equal to zero, it means that the server is
	 * quiesced, the existing connections to the server still get
	 * served, but no new connection is assigned to the server.
	 */

	l = &svc->destinations;
	for (e=l->next; e!=l; e=e->next) {
		least = list_entry (e, struct ip_vs_dest, n_list);
		if (atomic_read(&least->weight) > 0) {
			loh = ip_vs_lc_dest_overhead(least);
			goto nextstage;
		}
	}
	return NULL;

	/*
	 *    Find the destination with the least load.
	 */
  nextstage:
	for (e=e->next; e!=l; e=e->next) {
		dest = list_entry(e, struct ip_vs_dest, n_list);
		if (atomic_read(&dest->weight) == 0)
			continue;
		doh = ip_vs_lc_dest_overhead(dest);
		if (doh < loh) {
			least = dest;
			loh = doh;
		}
	}

	IP_VS_DBG(6, "LC: server %u.%u.%u.%u:%u activeconns %d inactconns %d\n",
		  NIPQUAD(least->addr), ntohs(least->port),
		  atomic_read(&least->activeconns),
		  atomic_read(&least->inactconns));

	return least;
}


static struct ip_vs_scheduler ip_vs_lc_scheduler = {
	{0},			/* n_list */
	"lc",			/* name */
	ATOMIC_INIT(0),		/* refcnt */
	THIS_MODULE,		/* this module */
	ip_vs_lc_init_svc,	/* service initializer */
	ip_vs_lc_done_svc,	/* service done */
	ip_vs_lc_update_svc,	/* service updater */
	ip_vs_lc_schedule,	/* select a server from the destination list */
};


static int __init ip_vs_lc_init(void)
{
	INIT_LIST_HEAD(&ip_vs_lc_scheduler.n_list);
	return register_ip_vs_scheduler(&ip_vs_lc_scheduler) ;
}

static void __exit ip_vs_lc_cleanup(void)
{
	unregister_ip_vs_scheduler(&ip_vs_lc_scheduler);
}

module_init(ip_vs_lc_init);
module_exit(ip_vs_lc_cleanup);
MODULE_LICENSE("GPL");
