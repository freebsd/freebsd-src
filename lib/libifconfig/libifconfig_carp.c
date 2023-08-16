/*
 * Copyright (c) 1983, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <net/if.h>
#include <netinet/ip_carp.h>
#include <netinet/ip_carp_nl.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>
#include <netlink/netlink_snl_route.h>

#include <string.h>
#include <strings.h>

#include "libifconfig.h"
#include "libifconfig_internal.h"

#include <stdio.h>

#define	_OUT(_field)	offsetof(struct ifconfig_carp, _field)
static struct snl_attr_parser ap_carp_get[] = {
	{ .type = CARP_NL_VHID, .off = _OUT(carpr_vhid), .cb = snl_attr_get_uint32 },
	{ .type = CARP_NL_STATE, .off = _OUT(carpr_state), .cb = snl_attr_get_uint32 },
	{ .type = CARP_NL_ADVBASE, .off = _OUT(carpr_advbase), .cb = snl_attr_get_int32 },
	{ .type = CARP_NL_ADVSKEW, .off = _OUT(carpr_advskew), .cb = snl_attr_get_int32 },
	{ .type = CARP_NL_KEY, .off = _OUT(carpr_key), .cb = snl_attr_copy_string, .arg_u32 = CARP_KEY_LEN },
	{ .type = CARP_NL_ADDR, .off = _OUT(carpr_addr), .cb = snl_attr_get_in_addr },
	{ .type = CARP_NL_ADDR6, .off = _OUT(carpr_addr6), .cb = snl_attr_get_in6_addr },
};
#undef _OUT

SNL_DECLARE_GENL_PARSER(carp_get_parser, ap_carp_get);

static int
_ifconfig_carp_get(ifconfig_handle_t *h, const char *name,
    struct ifconfig_carp *carp, size_t ncarp, uint32_t vhid)
{
	struct snl_state ss = {};
	struct snl_errmsg_data e = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	size_t i = 0;
	uint32_t seq_id;
	int family_id;

	ifconfig_error_clear(h);

	bzero(carp, sizeof(*carp) * ncarp);

	if (! snl_init(&ss, NETLINK_GENERIC)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	snl_init_writer(&ss, &nw);

	family_id = snl_get_genl_family(&ss, CARP_NL_FAMILY_NAME);
	if (family_id == 0) {
		ifconfig_error(h, NETLINK, EPROTONOSUPPORT);
		goto out;
	}

	hdr = snl_create_genl_msg_request(&nw, family_id, CARP_NL_CMD_GET);
	hdr->nlmsg_flags |= NLM_F_DUMP;

	snl_add_msg_attr_string(&nw, CARP_NL_IFNAME, name);

	if (vhid != 0)
		snl_add_msg_attr_u32(&nw, CARP_NL_VHID, vhid);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL) {
		ifconfig_error(h, NETLINK, ENOMEM);
		goto out;
	}
	seq_id = hdr->nlmsg_seq;
	if (! snl_send_message(&ss, hdr)) {
		ifconfig_error(h, NETLINK, EIO);
		goto out;
	}

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		if (e.error != 0) {
			ifconfig_error(h, NETLINK, e.error);
			break;
		}

		if (i >= ncarp) {
			ifconfig_error(h, NETLINK, E2BIG);
			break;
		}

		memset(&carp[i], 0, sizeof(carp[0]));
		if (! snl_parse_nlmsg(&ss, hdr, &carp_get_parser, &carp[i]))
			continue;

		i++;
		carp[0].carpr_count = i;

		if (i > ncarp) {
			ifconfig_error(h, NETLINK, E2BIG);
			break;
		}
	}

out:
	snl_free(&ss);

	return (h->error.errcode ? -1 : 0);
}

int
ifconfig_carp_set_info(ifconfig_handle_t *h, const char *name,
    const struct ifconfig_carp *carpr)
{
	struct snl_state ss = {};
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	int family_id;
	uint32_t seq_id;

	ifconfig_error_clear(h);

	if (! snl_init(&ss, NETLINK_GENERIC)) {
		ifconfig_error(h, NETLINK, ENOTSUP);
		return (-1);
	}

	snl_init_writer(&ss, &nw);

	family_id = snl_get_genl_family(&ss, CARP_NL_FAMILY_NAME);
	if (family_id == 0) {
		ifconfig_error(h, NETLINK, EPROTONOSUPPORT);
		return (-1);
	}
	hdr = snl_create_genl_msg_request(&nw, family_id, CARP_NL_CMD_SET);

	snl_add_msg_attr_u32(&nw, CARP_NL_VHID, carpr->carpr_vhid);
	snl_add_msg_attr_u32(&nw, CARP_NL_STATE, carpr->carpr_state);
	snl_add_msg_attr_s32(&nw, CARP_NL_ADVBASE, carpr->carpr_advbase);
	snl_add_msg_attr_s32(&nw, CARP_NL_ADVSKEW, carpr->carpr_advskew);
	snl_add_msg_attr_string(&nw, CARP_NL_IFNAME, name);
	snl_add_msg_attr(&nw, CARP_NL_ADDR, sizeof(carpr->carpr_addr),
	    &carpr->carpr_addr);
	snl_add_msg_attr(&nw, CARP_NL_ADDR6, sizeof(carpr->carpr_addr6),
	    &carpr->carpr_addr6);
	snl_add_msg_attr_string(&nw, CARP_NL_KEY, carpr->carpr_key);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL) {
		ifconfig_error(h, NETLINK, ENOMEM);
		goto out;
	}

	seq_id = hdr->nlmsg_seq;
	if (! snl_send_message(&ss, hdr)) {
		ifconfig_error(h, NETLINK, EIO);
		goto out;
	}

	struct snl_errmsg_data e = { };
	if (! snl_read_reply_code(&ss, seq_id, &e))
		ifconfig_error(h, NETLINK, e.error);

out:
	snl_free(&ss);

	return (h->error.errcode ? -1 : 0);
}

int
ifconfig_carp_get_vhid(ifconfig_handle_t *h, const char *name,
    struct ifconfig_carp *carp, uint32_t vhid)
{
	return (_ifconfig_carp_get(h, name, carp, 1, vhid));
}

int
ifconfig_carp_get_info(ifconfig_handle_t *h, const char *name,
    struct ifconfig_carp *carp, size_t ncarp)
{
	return (_ifconfig_carp_get(h, name, carp, ncarp, 0));
}
