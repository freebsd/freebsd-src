/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Alexander V. Chernikov
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <tests/ktest.h>
#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/netlink_message_writer.h>

#define KTEST_CALLER
#include <netlink/ktest_netlink_message_writer.h>

#ifdef INVARIANTS

struct test_nlbuf_attrs {
	uint32_t	size;
	uint32_t	expected_avail;
	int		waitok;
};

#define	_OUT(_field)	offsetof(struct test_nlbuf_attrs, _field)
static const struct nlattr_parser nla_p_nlbuf_w[] = {
	{ .type = 1, .off = _OUT(size), .cb = nlattr_get_uint32 },
	{ .type = 2, .off = _OUT(expected_avail), .cb = nlattr_get_uint32 },
	{ .type = 3, .off = _OUT(waitok), .cb = nlattr_get_uint32 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(nlbuf_w_parser, nla_p_nlbuf_w);

static int
test_nlbuf_parser(struct ktest_test_context *ctx, struct nlattr *nla)
{
	struct test_nlbuf_attrs *attrs = npt_alloc(ctx->npt, sizeof(*attrs));

	ctx->arg = attrs;
	if (attrs != NULL)
		return (nl_parse_nested(nla, &nlbuf_w_parser, ctx->npt, attrs));
	return (ENOMEM);
}

static int
test_nlbuf_writer_allocation(struct ktest_test_context *ctx)
{
	struct test_nlbuf_attrs *attrs = ctx->arg;
	struct nl_writer nw = {};
	u_int alloc_len;
	bool ret;

	ret = nlmsg_get_buf_wrapper(&nw, attrs->size, attrs->waitok);
	if (!ret)
		return (EINVAL);

	alloc_len = nw.buf->buflen;
	KTEST_LOG(ctx, "requested %u, allocated %d", attrs->size, alloc_len);

	/* Mark enomem to avoid reallocation */
	nw.enomem = true;

	if (nlmsg_reserve_data(&nw, alloc_len, void *) == NULL) {
		KTEST_LOG(ctx, "unable to get %d bytes from the writer", alloc_len);
		return (EINVAL);
	}

	nl_buf_free(nw.buf);

	if (alloc_len < attrs->expected_avail) {
		KTEST_LOG(ctx, "alloc_len %d, expected %u",
		    alloc_len, attrs->expected_avail);
		return (EINVAL);
	}

	return (0);
}
#endif

static const struct ktest_test_info tests[] = {
#ifdef INVARIANTS
	{
		.name = "test_nlbuf_writer_allocation",
		.desc = "test different buffer sizes in the netlink writer",
		.func = &test_nlbuf_writer_allocation,
		.parse = &test_nlbuf_parser,
	},
#endif
};
KTEST_MODULE_DECLARE(ktest_netlink_message_writer, tests);
