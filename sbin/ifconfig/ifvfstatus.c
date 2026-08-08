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
#include <sys/nv.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ifconfig.h"

static void
vf_group_begin(bool *printed, const char *name)
{
	if (!*printed) {
		printf("\t\t\t%s:", name);
		*printed = true;
	}
}

static void
vf_group_end(bool printed)
{
	if (printed)
		putchar('\n');
}

void
vf_status(if_ctx *ctx)
{
	const nvlist_t * const *vfs;
	const nvlist_t *vf;
	struct ifreq ifr;
	const void *mac;
	nvlist_t *status;
	const char *mode, *state;
	size_t maclen, num_vfs;
	uint64_t speed;
	void *buf, *newbuf;
	u_int buflen, nextlen;
	bool printed;

	buflen = 16 * 1024;
	buf = malloc(buflen);
	if (buf == NULL)
		err(1, "malloc");
	for (;;) {
		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ctx->ifname, sizeof(ifr.ifr_name));
		ifr.ifr_vf_status_nv.buffer = buf;
		ifr.ifr_vf_status_nv.buf_length = buflen;
		if (ioctl_ctx(ctx, SIOCGIFVFSTATUS, &ifr) == 0)
			break;
		if (errno == EINVAL || errno == ENOTTY || errno == ENXIO ||
		    errno == ENOTSUP || errno == EOPNOTSUPP) {
			free(buf);
			return;
		}
		if (errno != EFBIG ||
		    buflen == IFR_VF_STATUS_NV_MAXBUFSIZE ||
		    ifr.ifr_vf_status_nv.length >
		    IFR_VF_STATUS_NV_MAXBUFSIZE) {
			free(buf);
			warn("SIOCGIFVFSTATUS");
			return;
		}
		if (ifr.ifr_vf_status_nv.length > buflen)
			nextlen = ifr.ifr_vf_status_nv.length;
		else if (buflen > IFR_VF_STATUS_NV_MAXBUFSIZE / 2)
			nextlen = IFR_VF_STATUS_NV_MAXBUFSIZE;
		else
			nextlen = buflen * 2;
		newbuf = realloc(buf, nextlen);
		if (newbuf == NULL) {
			free(buf);
			err(1, "realloc");
		}
		buf = newbuf;
		buflen = nextlen;
	}

	status = nvlist_unpack(buf, ifr.ifr_vf_status_nv.length, 0);
	free(buf);
	if (status == NULL ||
	    !nvlist_exists_number(status, IFVF_STATUS_VERSION_KEY) ||
	    nvlist_get_number(status, IFVF_STATUS_VERSION_KEY) !=
	    IFVF_STATUS_VERSION ||
	    !nvlist_exists_nvlist_array(status, IFVF_STATUS_VFS)) {
		warnx("SIOCGIFVFSTATUS returned an unsupported format");
		nvlist_destroy(status);
		return;
	}

	if (nvlist_exists_string(status, IFVF_STATUS_PF_LINK_STATE)) {
		state = nvlist_get_string(status, IFVF_STATUS_PF_LINK_STATE);
		printf("\tVF-visible PF link: state=%s", state);
		if (nvlist_exists_number(status, IFVF_STATUS_PF_LINK_SPEED)) {
			speed = nvlist_get_number(status,
			    IFVF_STATUS_PF_LINK_SPEED);
			if (speed % IF_Mbps(1) == 0)
				printf(" speed=%juMbps", (uintmax_t)(speed /
				    IF_Mbps(1)));
			else
				printf(" speed=%jubps", (uintmax_t)speed);
		}
		putchar('\n');
	}

	vfs = nvlist_get_nvlist_array(status, IFVF_STATUS_VFS, &num_vfs);
	printf("\tvirtual functions: %zu\n", num_vfs);
	for (size_t i = 0; i < num_vfs; i++) {
		vf = vfs[i];
		printf("\t\tvf %3ju:\n",
		    (uintmax_t)(nvlist_exists_number(vf, IFVF_STATUS_INDEX) ?
		    nvlist_get_number(vf, IFVF_STATUS_INDEX) : i));

		printed = false;
		if (nvlist_exists_binary(vf, IFVF_STATUS_MAC)) {
			mac = nvlist_get_binary(vf, IFVF_STATUS_MAC, &maclen);
			if (maclen == ETHER_ADDR_LEN) {
				vf_group_begin(&printed, "identity");
				printf(" mac=%s", ether_ntoa(mac));
			}
		}
		vf_group_end(printed);

		printed = false;
		if (nvlist_exists_bool(vf, IFVF_STATUS_CONFIGURED)) {
			vf_group_begin(&printed, "state");
			printf(" configured=%s",
			    nvlist_get_bool(vf, IFVF_STATUS_CONFIGURED) ?
			    "yes" : "no");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_INITIALIZED)) {
			vf_group_begin(&printed, "state");
			printf(" initialized=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_INITIALIZED) ? "yes" : "no");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_TRAFFIC_ENABLED)) {
			vf_group_begin(&printed, "state");
			printf(" traffic=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_TRAFFIC_ENABLED) ? "enabled" : "disabled");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_MDD_BLOCKED)) {
			vf_group_begin(&printed, "state");
			printf(" mdd-blocked=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_MDD_BLOCKED) ? "yes" : "no");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_QUARANTINED)) {
			vf_group_begin(&printed, "state");
			printf(" quarantined=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_QUARANTINED) ? "yes" : "no");
		}
		vf_group_end(printed);

		printed = false;
		if (nvlist_exists_number(vf, IFVF_STATUS_NUM_QUEUES)) {
			vf_group_begin(&printed, "resources");
			printf(" queue-pairs=%ju",
			    (uintmax_t)nvlist_get_number(vf,
			    IFVF_STATUS_NUM_QUEUES));
		}
		vf_group_end(printed);

		printed = false;
		if (nvlist_exists_string(vf, IFVF_STATUS_VLAN_MODE)) {
			mode = nvlist_get_string(vf, IFVF_STATUS_VLAN_MODE);
			vf_group_begin(&printed, "vlan");
			printf(" mode=%s", mode);
			if (strcmp(mode, IFVF_VLAN_MODE_ACCESS) == 0 &&
			    nvlist_exists_number(vf, IFVF_STATUS_VLAN))
				printf(" vid=%ju", (uintmax_t)nvlist_get_number(vf,
				    IFVF_STATUS_VLAN));
		}
		if (nvlist_exists_number(vf, IFVF_STATUS_VLAN_COUNT)) {
			vf_group_begin(&printed, "vlan");
			printf(" filters=%ju", (uintmax_t)nvlist_get_number(vf,
			    IFVF_STATUS_VLAN_COUNT));
		}
		if (nvlist_exists_number(vf, IFVF_STATUS_VLAN_LIMIT)) {
			vf_group_begin(&printed, "vlan");
			printf(" limit=%ju", (uintmax_t)nvlist_get_number(vf,
			    IFVF_STATUS_VLAN_LIMIT));
		}
		vf_group_end(printed);

		printed = false;
		if (nvlist_exists_bool(vf, IFVF_STATUS_ALLOW_SET_MAC)) {
			vf_group_begin(&printed, "policy");
			printf(" set-mac=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_ALLOW_SET_MAC) ? "allowed" : "denied");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_ALLOW_SET_VLAN)) {
			vf_group_begin(&printed, "policy");
			printf(" set-vlan=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_ALLOW_SET_VLAN) ? "allowed" : "denied");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_MAC_ANTI_SPOOF)) {
			vf_group_begin(&printed, "policy");
			printf(" anti-spoof=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_MAC_ANTI_SPOOF) ? "on" : "off");
		}
		if (nvlist_exists_bool(vf, IFVF_STATUS_ALLOW_PROMISC)) {
			vf_group_begin(&printed, "policy");
			printf(" promisc=%s", nvlist_get_bool(vf,
			    IFVF_STATUS_ALLOW_PROMISC) ? "allowed" : "denied");
		}
		if (nvlist_exists_string(vf, IFVF_STATUS_LINK_STATE_POLICY)) {
			vf_group_begin(&printed, "policy");
			printf(" link-state=%s", nvlist_get_string(vf,
			    IFVF_STATUS_LINK_STATE_POLICY));
		}
		vf_group_end(printed);

		printed = false;
		if (nvlist_exists_string(vf, IFVF_STATUS_API_VERSION)) {
			vf_group_begin(&printed, "protocol");
			printf(" api=%s", nvlist_get_string(vf,
			    IFVF_STATUS_API_VERSION));
		}
		vf_group_end(printed);

	}
	nvlist_destroy(status);
}
