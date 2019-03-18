/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ifconfig.h"

void
vf_status(if_ctx *ctx)
{
	struct ifvfstatus_entry *entries;
	struct ifvfstatus ifvfs;

	memset(&ifvfs, 0, sizeof(ifvfs));
	strlcpy(ifvfs.ifvfs_name, ctx->ifname, sizeof(ifvfs.ifvfs_name));
	if (ioctl_ctx(ctx, SIOCGIFVFSTATUS, &ifvfs) < 0)
		return;
	if (ifvfs.ifvfs_count == 0)
		return;

	entries = calloc(ifvfs.ifvfs_count, sizeof(*entries));
	if (entries == NULL)
		err(1, "calloc");
	ifvfs.ifvfs_list = entries;
	if (ioctl_ctx(ctx, SIOCGIFVFSTATUS, &ifvfs) < 0) {
		free(entries);
		warn("SIOCGIFVFSTATUS");
		return;
	}

	printf("\tvirtual functions: %d\n", ifvfs.ifvfs_count);
	for (int i = 0; i < ifvfs.ifvfs_count; i++) {
		printf("\t\tvf %3d: mac %s", i,
		    ether_ntoa((const struct ether_addr *)entries[i].mac_addr));
		if (entries[i].vlan > -1)
			printf(" vlan %d", entries[i].vlan);
		if (entries[i].active)
			printf(" active");
		putchar('\n');
	}

	free(entries);
}
