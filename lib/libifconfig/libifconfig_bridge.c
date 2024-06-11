/*
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/ioctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_bridgevar.h>
#include <net/route.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "libifconfig.h"
#include "libifconfig_internal.h"

/* Internal structure used for allocations and frees */
struct _ifconfig_bridge_status {
	struct ifconfig_bridge_status inner;	/* wrapped bridge status */
	struct ifbropreq params;		/* operational parameters */
};

static int
ifconfig_bridge_ioctlwrap(ifconfig_handle_t *h, const char *name,
    unsigned long cmd, void *arg, size_t arglen, bool set)
{
	struct ifdrv ifd = { 0 };
	unsigned long req = set ? SIOCSDRVSPEC : SIOCGDRVSPEC;

	strlcpy(ifd.ifd_name, name, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = cmd;
	ifd.ifd_data = arg;
	ifd.ifd_len = arglen;

	return (ifconfig_ioctlwrap(h, AF_LOCAL, req, &ifd));
}

int
ifconfig_bridge_get_bridge_status(ifconfig_handle_t *h,
    const char *name, struct ifconfig_bridge_status **bridgep)
{
	struct ifbifconf members;
	struct ifbrparam cache_param;
	struct _ifconfig_bridge_status *bridge;
	char *buf;

	*bridgep = NULL;

	bridge = calloc(1, sizeof(struct _ifconfig_bridge_status));
	if (bridge == NULL) {
		h->error.errtype = OTHER;
		h->error.errcode = ENOMEM;
		return (-1);
	}
	bridge->inner.params = &bridge->params;

	if (ifconfig_bridge_ioctlwrap(h, name, BRDGGCACHE,
	    &cache_param, sizeof(cache_param), false) != 0) {
		free(bridge);
		return (-1);
	}
	bridge->inner.cache_size = cache_param.ifbrp_csize;

	if (ifconfig_bridge_ioctlwrap(h, name, BRDGGTO,
	    &cache_param, sizeof(cache_param), false) != 0) {
		free(bridge);
		return (-1);
	}
	bridge->inner.cache_lifetime = cache_param.ifbrp_ctime;

	if (ifconfig_bridge_ioctlwrap(h, name, BRDGPARAM,
	    &bridge->params, sizeof(bridge->params), false) != 0) {
		free(bridge);
		return (-1);
	}

	members.ifbic_buf = NULL;
	for (size_t len = 8192;
	    (buf = realloc(members.ifbic_buf, len)) != NULL;
	    len *= 2) {
		members.ifbic_buf = buf;
		members.ifbic_len = len;
		if (ifconfig_bridge_ioctlwrap(h, name, BRDGGIFS,
		    &members, sizeof(members), false) != 0) {
			free(buf);
			free(bridge);
			return (-1);
		}
		if ((members.ifbic_len + sizeof(*members.ifbic_req)) < len)
			break;
	}
	if (buf == NULL) {
		free(members.ifbic_buf);
		free(bridge);
		h->error.errtype = OTHER;
		h->error.errcode = ENOMEM;
		return (-1);
	}
	bridge->inner.members = members.ifbic_req;
	bridge->inner.members_count =
	    members.ifbic_len / sizeof(*members.ifbic_req);

	*bridgep = &bridge->inner;

	return (0);
}

void
ifconfig_bridge_free_bridge_status(struct ifconfig_bridge_status *bridge)
{
	if (bridge != NULL) {
		free(bridge->members);
		free(bridge);
	}
}
