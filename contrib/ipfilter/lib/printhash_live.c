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


iphtable_t *printhash_live(hp, fd, name, opts)
iphtable_t *hp;
int fd;
char *name;
int opts;
{
	iphtent_t entry, *top, *node;
	ipflookupiter_t iter;
	int printed, last;
	ipfobj_t obj;

	if ((name != NULL) && strncmp(name, hp->iph_name, FR_GROUPLEN))
		return hp->iph_next;

	printhashdata(hp, opts);

	if ((hp->iph_flags & IPHASH_DELETE) != 0)
		PRINTF("# ");

	if ((opts & OPT_DEBUG) == 0)
		PRINTF("\t{");

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_LOOKUPITER;
	obj.ipfo_ptr = &iter;
	obj.ipfo_size = sizeof(iter);

	iter.ili_data = &entry;
	iter.ili_type = IPLT_HASH;
	iter.ili_otype = IPFLOOKUPITER_NODE;
	iter.ili_ival = IPFGENITER_LOOKUP;
	iter.ili_unit = hp->iph_unit;
	strncpy(iter.ili_name, hp->iph_name, FR_GROUPLEN);

	last = 0;
	top = NULL;
	printed = 0;

	while (!last && (ioctl(fd, SIOCLOOKUPITER, &obj) == 0)) {
		if (entry.ipe_next == NULL)
			last = 1;
		entry.ipe_next = top;
		top = malloc(sizeof(*top));
		if (top == NULL)
			break;
		bcopy(&entry, top, sizeof(entry));
	}

	while (top != NULL) {
		node = top;
		(void) printhashnode(hp, node, bcopywrap, opts);
		top = node->ipe_next;
		free(node);
		printed++;
	}

	if (printed == 0)
		putchar(';');

	if ((opts & OPT_DEBUG) == 0)
		PRINTF(" };\n");
	return hp->iph_next;
}
