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

struct test_nlbuf_refill_attrs {
	uint32_t	size;
	uint32_t	completed;
	uint32_t	unfinished;
	uint32_t	attr_len;
};

#define	_OUT(_field)	offsetof(struct test_nlbuf_refill_attrs, _field)
static const struct nlattr_parser nla_p_nlbuf_refill[] = {
	{ .type = 1, .off = _OUT(size), .cb = nlattr_get_uint32 },
	{ .type = 2, .off = _OUT(completed), .cb = nlattr_get_uint32 },
	{ .type = 3, .off = _OUT(unfinished), .cb = nlattr_get_uint32 },
	{ .type = 4, .off = _OUT(attr_len), .cb = nlattr_get_uint32 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(nlbuf_refill_parser, nla_p_nlbuf_refill);

static int
test_nlbuf_refill_parser(struct ktest_test_context *ctx, struct nlattr *nla)
{
	struct test_nlbuf_refill_attrs *attrs;

	attrs = npt_alloc(ctx->npt, sizeof(*attrs));
	ctx->arg = attrs;
	if (attrs != NULL)
		return (nl_parse_nested(nla, &nlbuf_refill_parser, ctx->npt,
		    attrs));
	return (ENOMEM);
}

static bool
test_nlbuf_discard(struct nl_writer *nw)
{
	nl_buf_free(nw->buf);
	nw->buf = NULL;
	return (true);
}

/*
 * Add an attribute that does not fit into the buffer to an unfinished
 * message, optionally preceded by a completed one.
 */
static int
test_nlbuf_writer_refill(struct ktest_test_context *ctx)
{
	struct test_nlbuf_refill_attrs *attrs = ctx->arg;
	struct nl_writer nw = {};
	void *data;
	u_int expected;
	int error = 0;

	if (attrs == NULL || attrs->unfinished < sizeof(struct nlmsghdr) ||
	    (attrs->completed != 0 &&
	    attrs->completed < sizeof(struct nlmsghdr)))
		return (EINVAL);
	if (!nlmsg_get_buf_wrapper(&nw, attrs->size, true))
		return (EINVAL);
	nw.cb = test_nlbuf_discard;
	data = malloc(attrs->attr_len, M_TEMP, M_WAITOK | M_ZERO);

	if (attrs->completed != 0) {
		if (!nlmsg_add(&nw, 0, 0, NLMSG_NOOP, 0, 0) ||
		    nlmsg_reserve_data(&nw, attrs->completed -
		    sizeof(struct nlmsghdr), void *) == NULL ||
		    !nlmsg_end(&nw)) {
			error = EINVAL;
			goto out;
		}
	}
	if (!nlmsg_add(&nw, 0, 0, NLMSG_NOOP, 0, 0) ||
	    nlmsg_reserve_data(&nw, attrs->unfinished -
	    sizeof(struct nlmsghdr), void *) == NULL ||
	    !nlattr_add(&nw, 1, attrs->attr_len, data)) {
		error = EINVAL;
		goto out;
	}

	expected = NETLINK_ALIGN(attrs->unfinished) +
	    NLA_ALIGN(attrs->attr_len + sizeof(struct nlattr));
	KTEST_LOG(ctx, "message of %u bytes in a buffer of %u, expected %u",
	    nw.buf->datalen, nw.buf->buflen, expected);
	if (nw.buf->datalen > nw.buf->buflen || nw.buf->datalen != expected)
		error = EINVAL;
out:
	if (nw.buf != NULL)
		nl_buf_free(nw.buf);
	free(data, M_TEMP);

	return (error);
}

static const struct ktest_test_info tests[] = {
	{
		.name = "test_nlbuf_writer_allocation",
		.desc = "test different buffer sizes in the netlink writer",
		.func = &test_nlbuf_writer_allocation,
		.parse = &test_nlbuf_parser,
	},
	{
		.name = "test_nlbuf_writer_refill",
		.desc = "test growing the buffer under an unfinished message",
		.func = &test_nlbuf_writer_refill,
		.parse = &test_nlbuf_refill_parser,
	},
};
KTEST_MODULE_DECLARE(ktest_netlink_message_writer, tests);
