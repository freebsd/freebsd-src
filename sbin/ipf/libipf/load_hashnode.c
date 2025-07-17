
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include "ipf.h"
#include "netinet/ip_lookup.h"
#include "netinet/ip_htable.h"


int
load_hashnode(int unit, char *name, iphtent_t *node, int ttl,
	ioctlfunc_t iocfunc)
{
	iplookupop_t op;
	iphtent_t ipe;
	char *what;
	int err;

	if (pool_open() == -1)
		return (-1);

	op.iplo_type = IPLT_HASH;
	op.iplo_unit = unit;
	op.iplo_arg = 0;
	op.iplo_size = sizeof(ipe);
	op.iplo_struct = &ipe;
	strncpy(op.iplo_name, name, sizeof(op.iplo_name));

	bzero((char *)&ipe, sizeof(ipe));
	ipe.ipe_family = node->ipe_family;
	ipe.ipe_die = ttl;
	bcopy((char *)&node->ipe_addr, (char *)&ipe.ipe_addr,
	      sizeof(ipe.ipe_addr));
	bcopy((char *)&node->ipe_mask, (char *)&ipe.ipe_mask,
	      sizeof(ipe.ipe_mask));
	bcopy((char *)&node->ipe_group, (char *)&ipe.ipe_group,
	      sizeof(ipe.ipe_group));

	if ((opts & OPT_REMOVE) == 0) {
		what = "add";
		err = pool_ioctl(iocfunc, SIOCLOOKUPADDNODE, &op);
	} else {
		what = "delete";
		err = pool_ioctl(iocfunc, SIOCLOOKUPDELNODE, &op);
	}

	if (err != 0)
		if (!(opts & OPT_DONOTHING)) {
			char msg[255];
			char ipaddr[80], mask_msg[10], mask[8];

			inet_ntop(ipe.ipe_family,
				ipe.ipe_addr.vptr, ipaddr,
				sizeof(ipaddr));
#ifdef USE_INET6
			if (ipe.ipe_family == AF_INET) {
#endif
				inet_ntop(ipe.ipe_family,
					ipe.ipe_mask.vptr, mask,
					sizeof(mask));
				mask_msg[0]='/';
				mask_msg[1]='\0';
				strlcat(mask_msg, mask, sizeof(mask_msg));
#ifdef USE_INET6
			} else {
				mask_msg[0]='\0';
			}
#endif

			snprintf(msg, sizeof(msg), "%s node from lookup hash table(%s) node(%s%s)", what, name, ipaddr, mask_msg);
			return (ipf_perror_fd(pool_fd(), iocfunc, msg));
		}
	return (0);
}
