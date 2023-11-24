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


static int
test_something(struct ktest_test_context *ctx)
{
	KTEST_LOG(ctx, "I'm here, [%s]", __func__);

	pause("sleeping...", hz / 10);

	KTEST_LOG(ctx, "done");

	return (0);
}

static int
test_something_else(struct ktest_test_context *ctx)
{
	return (0);
}

static int
test_failed(struct ktest_test_context *ctx)
{
	return (EBUSY);
}

static int
test_failed2(struct ktest_test_context *ctx)
{
	KTEST_LOG(ctx, "failed because it always fails");
	return (EBUSY);
}

#include <sys/malloc.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>

struct test1_attrs {
	uint32_t	arg1;
	uint32_t	arg2;
	char		*text;
};

#define	_OUT(_field)	offsetof(struct test1_attrs, _field)
static const struct nlattr_parser nla_p_test1[] = {
	{ .type = 1, .off = _OUT(arg1), .cb = nlattr_get_uint32 },
	{ .type = 2, .off = _OUT(arg2), .cb = nlattr_get_uint32 },
	{ .type = 3, .off = _OUT(text), .cb = nlattr_get_string },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(test1_parser, nla_p_test1);

static int
test_with_params_parser(struct ktest_test_context *ctx, struct nlattr *nla)
{
	struct test1_attrs *attrs = npt_alloc(ctx->npt, sizeof(*attrs));

	ctx->arg = attrs;
	if (attrs != NULL)
		return (nl_parse_nested(nla, &test1_parser, ctx->npt, attrs));
	return (ENOMEM);
}

static int
test_with_params(struct ktest_test_context *ctx)
{
	struct test1_attrs *attrs = ctx->arg;

	if (attrs->text != NULL)
		KTEST_LOG(ctx, "Get '%s'", attrs->text);
	KTEST_LOG(ctx, "%u + %u = %u", attrs->arg1, attrs->arg2,
	    attrs->arg1 + attrs->arg2);
	return (0);
}

static const struct ktest_test_info tests[] = {
	{
		.name = "test_something",
		.desc = "example description",
		.func = &test_something,
	},
	{
		.name = "test_something_else",
		.desc = "example description 2",
		.func = &test_something_else,
	},
	{
		.name = "test_failed",
		.desc = "always failing test",
		.func = &test_failed,
	},
	{
		.name = "test_failed2",
		.desc = "always failing test",
		.func = &test_failed2,
	},
	{
		.name = "test_with_params",
		.desc = "test summing integers",
		.func = &test_with_params,
		.parse = &test_with_params_parser,
	},
};
KTEST_MODULE_DECLARE(ktest_example, tests);
