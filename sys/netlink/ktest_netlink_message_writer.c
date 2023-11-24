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
#include <sys/mbuf.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_message_writer.h>

#define KTEST_CALLER
#include <netlink/ktest_netlink_message_writer.h>

#ifdef INVARIANTS

struct test_mbuf_attrs {
	uint32_t	size;
	uint32_t	expected_avail;
	uint32_t	expected_count;
	uint32_t	wtype;
	int		waitok;
};

#define	_OUT(_field)	offsetof(struct test_mbuf_attrs, _field)
static const struct nlattr_parser nla_p_mbuf_w[] = {
	{ .type = 1, .off = _OUT(size), .cb = nlattr_get_uint32 },
	{ .type = 2, .off = _OUT(expected_avail), .cb = nlattr_get_uint32 },
	{ .type = 3, .off = _OUT(expected_count), .cb = nlattr_get_uint32 },
	{ .type = 4, .off = _OUT(wtype), .cb = nlattr_get_uint32 },
	{ .type = 5, .off = _OUT(waitok), .cb = nlattr_get_uint32 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(mbuf_w_parser, nla_p_mbuf_w);

static int
test_mbuf_parser(struct ktest_test_context *ctx, struct nlattr *nla)
{
	struct test_mbuf_attrs *attrs = npt_alloc(ctx->npt, sizeof(*attrs));

	ctx->arg = attrs;
	if (attrs != NULL)
		return (nl_parse_nested(nla, &mbuf_w_parser, ctx->npt, attrs));
	return (ENOMEM);
}

static int
test_mbuf_writer_allocation(struct ktest_test_context *ctx)
{
	struct test_mbuf_attrs *attrs = ctx->arg;
	bool ret;
	struct nl_writer nw = {};

	ret = nlmsg_get_buf_type_wrapper(&nw, attrs->size, attrs->wtype, attrs->waitok);
	if (!ret)
		return (EINVAL);

	int alloc_len = nw.alloc_len;
	KTEST_LOG(ctx, "requested %u, allocated %d", attrs->size, alloc_len);

	/* Set cleanup callback */
	nw.writer_target = NS_WRITER_TARGET_SOCKET;
	nlmsg_set_callback_wrapper(&nw);

	/* Mark enomem to avoid reallocation */
	nw.enomem = true;

	if (nlmsg_reserve_data(&nw, alloc_len, void *) == NULL) {
		KTEST_LOG(ctx, "unable to get %d bytes from the writer", alloc_len);
		return (EINVAL);
	}

	/* Mark as empty to free the storage */
	nw.offset = 0;
	nlmsg_flush(&nw);

	if (alloc_len < attrs->expected_avail) {
		KTEST_LOG(ctx, "alloc_len %d, expected %u",
		    alloc_len, attrs->expected_avail);
		return (EINVAL);
	}

	return (0);
}

static int
test_mbuf_chain_allocation(struct ktest_test_context *ctx)
{
	struct test_mbuf_attrs *attrs = ctx->arg;
	int mflags = attrs->waitok ? M_WAITOK : M_NOWAIT;
	struct mbuf *chain = nl_get_mbuf_chain_wrapper(attrs->size, mflags);

	if (chain == NULL) {
		KTEST_LOG(ctx, "nl_get_mbuf_chain(%u) returned NULL", attrs->size);
		return (EINVAL);
	}

	/* Iterate and check number of mbufs and space */
	uint32_t allocated_count = 0, allocated_size = 0;
	for (struct mbuf *m = chain; m != NULL; m = m->m_next) {
		allocated_count++;
		allocated_size += M_SIZE(m);
	}
	m_freem(chain);

	if (attrs->expected_avail > allocated_size) {
		KTEST_LOG(ctx, "expected/allocated avail(bytes) %u/%u"
				" expected/allocated count %u/%u",
		    attrs->expected_avail, allocated_size,
		    attrs->expected_count, allocated_count);
		return (EINVAL);
	}

	if (attrs->expected_count > 0 && (attrs->expected_count != allocated_count)) {
		KTEST_LOG(ctx, "expected/allocated avail(bytes) %u/%u"
				" expected/allocated count %u/%u",
		    attrs->expected_avail, allocated_size,
		    attrs->expected_count, allocated_count);
		return (EINVAL);
	}

	return (0);
}
#endif

static const struct ktest_test_info tests[] = {
#ifdef INVARIANTS
	{
		.name = "test_mbuf_writer_allocation",
		.desc = "test different mbuf sizes in the mbuf writer",
		.func = &test_mbuf_writer_allocation,
		.parse = &test_mbuf_parser,
	},
	{
		.name = "test_mbuf_chain_allocation",
		.desc = "verify allocation different chain sizes",
		.func = &test_mbuf_chain_allocation,
		.parse = &test_mbuf_parser,
	},
#endif
};
KTEST_MODULE_DECLARE(ktest_netlink_message_writer, tests);
