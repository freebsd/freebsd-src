/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2025 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions~
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <err.h>

#include <netinet/in.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpc/clnt_nl.h>

#include "genl.h"

struct nl_request_parsed {
	uint32_t	group;
	struct nlattr	*data;
};
static const struct snl_attr_parser rpcnl_attr_parser[] = {
#define	OUT(field)	offsetof(struct nl_request_parsed, field)
    { .type = RPCNL_REQUEST_GROUP, .off = OUT(group),
       .cb = snl_attr_get_uint32 },
    { .type = RPCNL_REQUEST_BODY, .off = OUT(data), .cb = snl_attr_get_nla },
#undef OUT
};
SNL_DECLARE_PARSER(request_parser, struct genlmsghdr, snl_f_p_empty,
    rpcnl_attr_parser);

void
parser_rpc(struct snl_state *ss __unused, struct nlmsghdr *hdr)
{
	struct nl_request_parsed req;
	struct genlmsghdr *ghdr = (struct genlmsghdr *)(hdr + 1);
	XDR xdrs;
	struct rpc_msg msg;
	struct opaque_auth *oa;
	int32_t *buf;

	if (!snl_parse_nlmsg(NULL, hdr, &request_parser, &req))
		errx(EXIT_FAILURE, "failed to parse RPC message");

	printf("RPC %s: group %8s[0x%2x] length %4u XDR length %4u\n",
	    ghdr->cmd == RPCNL_REQUEST ? "request" : "unknown",
	    group_name(req.group), req.group,
	    hdr->nlmsg_len, NLA_DATA_LEN(req.data));

	xdrmem_create(&xdrs, NLA_DATA(req.data), NLA_DATA_LEN(req.data),
	    XDR_DECODE);
	if ((buf = XDR_INLINE(&xdrs, 8 * BYTES_PER_XDR_UNIT)) == NULL) {
		printf("\trunt datagram\n");
		return;
	}

	msg.rm_xid = IXDR_GET_U_INT32(buf);
	msg.rm_direction = IXDR_GET_ENUM(buf, enum msg_type);
	msg.rm_call.cb_rpcvers = IXDR_GET_U_INT32(buf);
	msg.rm_call.cb_prog = IXDR_GET_U_INT32(buf);
	msg.rm_call.cb_vers = IXDR_GET_U_INT32(buf);
	msg.rm_call.cb_proc = IXDR_GET_U_INT32(buf);
	printf("    %5s: xid 0x%-8x program 0x%08xv%u procedure %u\n",
	    msg.rm_direction == CALL ? "CALL" : "REPLY", msg.rm_xid,
            msg.rm_call.cb_prog, msg.rm_call.cb_vers, msg.rm_call.cb_proc);

	oa = &msg.rm_call.cb_cred;
	oa->oa_flavor = IXDR_GET_ENUM(buf, enum_t);
	oa->oa_length = (u_int)IXDR_GET_U_INT32(buf);
	if (oa->oa_length) {
		printf("\tcb_cred auth flavor %u length %u\n",
		    oa->oa_flavor, oa->oa_length);
/*
 *	Excerpt from rpc_callmsg.c, if we want to parse cb_cred better.
		if (oa->oa_length > MAX_AUTH_BYTES) {
			return (FALSE);
		}
		if (oa->oa_base == NULL) {
			oa->oa_base = (caddr_t)
			    mem_alloc(oa->oa_length);
			if (oa->oa_base == NULL)
				return (FALSE);
		}
		buf = XDR_INLINE(&xdrs, RNDUP(oa->oa_length));
		if (buf == NULL) {
			if (xdr_opaque(&xdrs, oa->oa_base,
			    oa->oa_length) == FALSE) {
				return (FALSE);
			}
		} else {
			memmove(oa->oa_base, buf,
			    oa->oa_length);
		}
*/
	}
	oa = &msg.rm_call.cb_verf;
	buf = XDR_INLINE(&xdrs, 2 * BYTES_PER_XDR_UNIT);
	if (buf == NULL) {
		if (xdr_enum(&xdrs, &oa->oa_flavor) == FALSE ||
		    xdr_u_int(&xdrs, &oa->oa_length) == FALSE)
			return;
	} else {
		oa->oa_flavor = IXDR_GET_ENUM(buf, enum_t);
		oa->oa_length = (u_int)IXDR_GET_U_INT32(buf);
	}
	if (oa->oa_length) {
		printf("\tcb_verf auth flavor %u length %u\n",
		    oa->oa_flavor, oa->oa_length);
/*
 *	Excerpt from rpc_callmsg.c, if we want to parse cb_verf better.
		if (oa->oa_length > MAX_AUTH_BYTES) {
			return (FALSE);
		}
		if (oa->oa_base == NULL) {
			oa->oa_base = (caddr_t)
			    mem_alloc(oa->oa_length);
			if (oa->oa_base == NULL)
				return (FALSE);
		}
		buf = XDR_INLINE(&xdrs, RNDUP(oa->oa_length));
		if (buf == NULL) {
			if (xdr_opaque(&xdrs, oa->oa_base,
			    oa->oa_length) == FALSE) {
				return (FALSE);
			}
		} else {
			memmove(oa->oa_base, buf,
			    oa->oa_length);
		}
*/
	}
}
