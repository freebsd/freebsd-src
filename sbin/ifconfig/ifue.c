/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Li-Wen Hsu <lwhsu@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>

#include "ifconfig.h"

#define	UE_DRIVER_NAME	"ue"

static void
ue_status(if_ctx *ctx)
{
	char mibname[32];	/* "net.ue.<unit>.%parent" */
	char parent[IFNAMSIZ];
	char *drivername;
	size_t parentlen;
	u_int unit;
	char c;

	/* Use the orig name so it keeps working after interface renaming. */
	if (ifconfig_get_orig_name(lifh, ctx->ifname, &drivername) != 0)
		return;
	if (sscanf(drivername, UE_DRIVER_NAME "%u%c", &unit, &c) != 1)
		goto out;

	if (snprintf(mibname, sizeof(mibname),
	    "net." UE_DRIVER_NAME ".%u.%%parent", unit) >= (int)sizeof(mibname))
		goto out;

	parentlen = sizeof(parent);
	if (sysctlbyname(mibname, parent, &parentlen, NULL, 0) != 0)
		goto out;

	printf("\tparent interface: %s\n", parent);
out:
	free(drivername);
}

static struct afswtch af_ue = {
	.af_name	= "af_ue",
	.af_af		= AF_UNSPEC,
	.af_other_status = ue_status,
};

static __constructor void
ue_ctor(void)
{
	af_register(&af_ue);
}
