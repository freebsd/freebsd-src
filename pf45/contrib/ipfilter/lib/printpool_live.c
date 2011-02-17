/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include <sys/ioctl.h>
#include "ipf.h"
#include "netinet/ipl.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf


ip_pool_t *printpool_live(pool, fd, name, opts)
ip_pool_t *pool;
int fd;
char *name;
int opts;
{
	ip_pool_node_t entry, *top, *node;
	ipflookupiter_t iter;
	int printed, last;
	ipfobj_t obj;

	if ((name != NULL) && strncmp(name, pool->ipo_name, FR_GROUPLEN))
		return pool->ipo_next;

	printpooldata(pool, opts);

	if ((pool->ipo_flags & IPOOL_DELETE) != 0)
		PRINTF("# ");
	if ((opts & OPT_DEBUG) == 0)
		PRINTF("\t{");

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_LOOKUPITER;
	obj.ipfo_ptr = &iter;
	obj.ipfo_size = sizeof(iter);

	iter.ili_data = &entry;
	iter.ili_type = IPLT_POOL;
	iter.ili_otype = IPFLOOKUPITER_NODE;
	iter.ili_ival = IPFGENITER_LOOKUP;
	iter.ili_unit = pool->ipo_unit;
	strncpy(iter.ili_name, pool->ipo_name, FR_GROUPLEN);

	last = 0;
	top = NULL;
	printed = 0;

	while (!last && (ioctl(fd, SIOCLOOKUPITER, &obj) == 0)) {
		if (entry.ipn_next == NULL)
			last = 1;
		node = malloc(sizeof(*top));
		if (node == NULL)
			break;
		bcopy(&entry, node, sizeof(entry));
		node->ipn_next = top;
		top = node;
	}

	while (top != NULL) {
		node = top;
		(void) printpoolnode(node, opts);
		if ((opts & OPT_DEBUG) == 0)
			putchar(';');
		top = node->ipn_next;
		free(node);
		printed++;
	}

	if (printed == 0)
		putchar(';');

	if ((opts & OPT_DEBUG) == 0)
		PRINTF(" };\n");

	if (ioctl(fd, SIOCIPFDELTOK, &iter.ili_key) != 0)
		perror("SIOCIPFDELTOK");

	return pool->ipo_next;
}
