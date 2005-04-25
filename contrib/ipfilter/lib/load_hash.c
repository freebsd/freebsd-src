/*	$NetBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: load_hash.c,v 1.11.2.2 2005/02/01 02:44:05 darrenr Exp
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include "ipf.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_htable.h"

static int hashfd = -1;


int load_hash(iphp, list, iocfunc)
iphtable_t *iphp;
iphtent_t *list;
ioctlfunc_t iocfunc;
{
	iplookupop_t op;
	iphtable_t iph;
	iphtent_t *a;
	size_t size;
	int n;

	if ((hashfd == -1) && ((opts & OPT_DONOTHING) == 0))
		hashfd = open(IPLOOKUP_NAME, O_RDWR);
	if ((hashfd == -1) && ((opts & OPT_DONOTHING) == 0))
		return -1;

	for (n = 0, a = list; a != NULL; a = a->ipe_next)
		n++;

	op.iplo_arg = 0;
	op.iplo_type = IPLT_HASH;
	op.iplo_unit = iphp->iph_unit;
	strncpy(op.iplo_name, iphp->iph_name, sizeof(op.iplo_name));
	if (*op.iplo_name == '\0')
		op.iplo_arg = IPHASH_ANON;
	op.iplo_size = sizeof(iph);
	op.iplo_struct = &iph;
	iph.iph_unit = iphp->iph_unit;
	iph.iph_type = iphp->iph_type;
	strncpy(iph.iph_name, iphp->iph_name, sizeof(iph.iph_name));
	iph.iph_flags = iphp->iph_flags;
	if (n <= 0)
		n = 1;
	if (iphp->iph_size == 0)
		size = n * 2 - 1;
	else
		size = iphp->iph_size;
	if ((list == NULL) && (size == 1)) {
		fprintf(stderr,
			"WARNING: empty hash table %s, recommend setting %s\n",
			iphp->iph_name, "size to match expected use");
	}
	iph.iph_size = size;
	iph.iph_seed = iphp->iph_seed;
	iph.iph_table = NULL;
	iph.iph_ref = 0;

	if ((opts & OPT_REMOVE) == 0) {
		if ((*iocfunc)(hashfd, SIOCLOOKUPADDTABLE, &op))
			if ((opts & OPT_DONOTHING) == 0) {
				perror("load_hash:SIOCLOOKUPADDTABLE");
				return -1;
			}
	}

	strncpy(op.iplo_name, iph.iph_name, sizeof(op.iplo_name));
	strncpy(iphp->iph_name, iph.iph_name, sizeof(op.iplo_name));

	if (opts & OPT_VERBOSE) {
		for (a = list; a != NULL; a = a->ipe_next) {
			a->ipe_addr.in4_addr = ntohl(a->ipe_addr.in4_addr);
			a->ipe_mask.in4_addr = ntohl(a->ipe_mask.in4_addr);
		}
		iph.iph_table = calloc(size, sizeof(*iph.iph_table));
		if (iph.iph_table == NULL) {
			perror("calloc(size, sizeof(*iph.iph_table))");
			return -1;
		}
		iph.iph_table[0] = list;
		printhash(&iph, bcopywrap, iph.iph_name, opts);
		free(iph.iph_table);

		for (a = list; a != NULL; a = a->ipe_next) {
			a->ipe_addr.in4_addr = htonl(a->ipe_addr.in4_addr);
			a->ipe_mask.in4_addr = htonl(a->ipe_mask.in4_addr);
		}
	}

	if (opts & OPT_DEBUG)
		printf("Hash %s:\n", iph.iph_name);

	for (a = list; a != NULL; a = a->ipe_next)
		load_hashnode(iphp->iph_unit, iph.iph_name, a, iocfunc);

	if ((opts & OPT_REMOVE) != 0) {
		if ((*iocfunc)(hashfd, SIOCLOOKUPDELTABLE, &op))
			if ((opts & OPT_DONOTHING) == 0) {
				perror("load_hash:SIOCLOOKUPDELTABLE");
				return -1;
			}
	}
	return 0;
}
