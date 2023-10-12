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

#include <sys/param.h>
#include <sys/refcount.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/priv.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_message_parser.h>

#include <machine/stdarg.h>
#include <tests/ktest.h>

struct mtx ktest_mtx;
#define	KTEST_LOCK()		mtx_lock(&ktest_mtx)
#define	KTEST_UNLOCK()		mtx_unlock(&ktest_mtx)
#define	KTEST_LOCK_ASSERT()	mtx_assert(&ktest_mtx, MA_OWNED)

MTX_SYSINIT(ktest_mtx, &ktest_mtx, "ktest mutex", MTX_DEF);

struct ktest_module {
	struct ktest_module_info	*info;
	volatile u_int			refcount;
	TAILQ_ENTRY(ktest_module)	entries;
};
static TAILQ_HEAD(, ktest_module) module_list = TAILQ_HEAD_INITIALIZER(module_list);

struct nl_ktest_parsed {
	char		*mod_name;
	char		*test_name;
	struct nlattr	*test_meta;
};

#define	_IN(_field)	offsetof(struct genlmsghdr, _field)
#define	_OUT(_field)	offsetof(struct nl_ktest_parsed, _field)

static const struct nlattr_parser nla_p_get[] = {
	{ .type = KTEST_ATTR_MOD_NAME, .off = _OUT(mod_name), .cb = nlattr_get_string },
	{ .type = KTEST_ATTR_TEST_NAME, .off = _OUT(test_name), .cb = nlattr_get_string },
	{ .type = KTEST_ATTR_TEST_META, .off = _OUT(test_meta), .cb = nlattr_get_nla },
};
static const struct nlfield_parser nlf_p_get[] = {
};
NL_DECLARE_PARSER(ktest_parser, struct genlmsghdr, nlf_p_get, nla_p_get);
#undef _IN
#undef _OUT

static bool
create_reply(struct nl_writer *nw, struct nlmsghdr *hdr, int cmd)
{
	if (!nlmsg_reply(nw, hdr, sizeof(struct genlmsghdr)))
		return (false);

	struct genlmsghdr *ghdr_new = nlmsg_reserve_object(nw, struct genlmsghdr);
	ghdr_new->cmd = cmd;
	ghdr_new->version = 0;
	ghdr_new->reserved = 0;

	return (true);
}

static int
dump_mod_test(struct nlmsghdr *hdr, struct nl_pstate *npt,
    struct ktest_module *mod, const struct ktest_test_info *test_info)
{
	struct nl_writer *nw = npt->nw;

	if (!create_reply(nw, hdr, KTEST_CMD_NEWTEST))
		goto enomem;

	nlattr_add_string(nw, KTEST_ATTR_MOD_NAME, mod->info->name);
	nlattr_add_string(nw, KTEST_ATTR_TEST_NAME, test_info->name);
	nlattr_add_string(nw, KTEST_ATTR_TEST_DESCR, test_info->desc);

	if (nlmsg_end(nw))
		return (0);
enomem:
	nlmsg_abort(nw);
	return (ENOMEM);
}

static int
dump_mod_tests(struct nlmsghdr *hdr, struct nl_pstate *npt,
    struct ktest_module *mod, struct nl_ktest_parsed *attrs)
{
	for (int i = 0; i < mod->info->num_tests; i++) {
		const struct ktest_test_info *test_info = &mod->info->tests[i];
		if (attrs->test_name != NULL && strcmp(attrs->test_name, test_info->name))
			continue;
		int error = dump_mod_test(hdr, npt, mod, test_info);
		if (error != 0)
			return (error);
	}

	return (0);
}

static int
dump_tests(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nl_ktest_parsed attrs = { };
	struct ktest_module *mod;
	int error;

	error = nl_parse_nlmsg(hdr, &ktest_parser, npt, &attrs);
	if (error != 0)
		return (error);

	hdr->nlmsg_flags |= NLM_F_MULTI;

	KTEST_LOCK();
	TAILQ_FOREACH(mod, &module_list, entries) {
		if (attrs.mod_name && strcmp(attrs.mod_name, mod->info->name))
			continue;
		error = dump_mod_tests(hdr, npt, mod, &attrs);
		if (error != 0)
			break;
	}
	KTEST_UNLOCK();

	if (!nlmsg_end_dump(npt->nw, error, hdr)) {
		//NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
		return (ENOMEM);
	}

	return (error);
}

static int
run_test(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nl_ktest_parsed attrs = { };
	struct ktest_module *mod;
	int error;

	error = nl_parse_nlmsg(hdr, &ktest_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.mod_name == NULL) {
		nlmsg_report_err_msg(npt, "KTEST_ATTR_MOD_NAME not set");
		return (EINVAL);
	}

	if (attrs.test_name == NULL) {
		nlmsg_report_err_msg(npt, "KTEST_ATTR_TEST_NAME not set");
		return (EINVAL);
	}

	const struct ktest_test_info *test = NULL;

	KTEST_LOCK();
	TAILQ_FOREACH(mod, &module_list, entries) {
		if (strcmp(attrs.mod_name, mod->info->name))
			continue;

		const struct ktest_module_info *info = mod->info;

		for (int i = 0; i < info->num_tests; i++) {
			const struct ktest_test_info *test_info = &info->tests[i];

			if (!strcmp(attrs.test_name, test_info->name)) {
				test = test_info;
				break;
			}
		}
		break;
	}
	if (test != NULL)
		refcount_acquire(&mod->refcount);
	KTEST_UNLOCK();

	if (test == NULL)
		return (ESRCH);

	/* Run the test */
	struct ktest_test_context ctx = {
		.npt = npt,
		.hdr = hdr,
		.buf = npt_alloc(npt, KTEST_MAX_BUF),
		.bufsize = KTEST_MAX_BUF,
	};

	if (ctx.buf == NULL) {
		//NL_LOG(LOG_DEBUG, "unable to allocate temporary buffer");
		return (ENOMEM);
	}

	if (test->parse != NULL && attrs.test_meta != NULL) {
		error = test->parse(&ctx, attrs.test_meta);
		if (error != 0)
			return (error);
	}

	hdr->nlmsg_flags |= NLM_F_MULTI;

	KTEST_LOG_LEVEL(&ctx, LOG_INFO, "start running %s", test->name);
	error = test->func(&ctx);
	KTEST_LOG_LEVEL(&ctx, LOG_INFO, "end running %s", test->name);

	refcount_release(&mod->refcount);

	if (!nlmsg_end_dump(npt->nw, error, hdr)) {
		//NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
		return (ENOMEM);
	}

	return (error);
}


/* USER API */
static void
register_test_module(struct ktest_module_info *info)
{
	struct ktest_module *mod = malloc(sizeof(*mod), M_TEMP, M_WAITOK | M_ZERO);

	mod->info = info;
	info->module_ptr = mod;
	KTEST_LOCK();
	TAILQ_INSERT_TAIL(&module_list, mod, entries);
	KTEST_UNLOCK();
}

static void
unregister_test_module(struct ktest_module_info *info)
{
	struct ktest_module *mod = info->module_ptr;

	info->module_ptr = NULL;

	KTEST_LOCK();
	TAILQ_REMOVE(&module_list, mod, entries);
	KTEST_UNLOCK();

	free(mod, M_TEMP);
}

static bool
can_unregister(struct ktest_module_info *info)
{
	struct ktest_module *mod = info->module_ptr;

	return (refcount_load(&mod->refcount) == 0);
}

int
ktest_default_modevent(module_t mod, int type, void *arg)
{
	struct ktest_module_info *info = (struct ktest_module_info *)arg;
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		register_test_module(info);
		break;
	case MOD_UNLOAD:
		if (!can_unregister(info))
			return (EBUSY);
		unregister_test_module(info);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

bool
ktest_start_msg(struct ktest_test_context *ctx)
{
	return (create_reply(ctx->npt->nw, ctx->hdr, KTEST_CMD_NEWMESSAGE));
}

void
ktest_add_msg_meta(struct ktest_test_context *ctx, const char *func,
    const char *fname, int line)
{
	struct nl_writer *nw = ctx->npt->nw;
	struct timespec ts;

	nanouptime(&ts);
	nlattr_add(nw, KTEST_MSG_ATTR_TS, sizeof(ts), &ts);

	nlattr_add_string(nw, KTEST_MSG_ATTR_FUNC, func);
	nlattr_add_string(nw, KTEST_MSG_ATTR_FILE, fname);
	nlattr_add_u32(nw, KTEST_MSG_ATTR_LINE, line);
}

void
ktest_add_msg_text(struct ktest_test_context *ctx, int msg_level,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(ctx->buf, ctx->bufsize, fmt, ap);
	va_end(ap);

	nlattr_add_u8(ctx->npt->nw, KTEST_MSG_ATTR_LEVEL, msg_level);
	nlattr_add_string(ctx->npt->nw, KTEST_MSG_ATTR_TEXT, ctx->buf);
}

void
ktest_end_msg(struct ktest_test_context *ctx)
{
	nlmsg_end(ctx->npt->nw);
}

/* Module glue */

static const struct nlhdr_parser *all_parsers[] = { &ktest_parser };

static const struct genl_cmd ktest_cmds[] = {
	{
		.cmd_num = KTEST_CMD_LIST,
		.cmd_name = "KTEST_CMD_LIST",
		.cmd_cb = dump_tests,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
	},
	{
		.cmd_num = KTEST_CMD_RUN,
		.cmd_name = "KTEST_CMD_RUN",
		.cmd_cb = run_test,
		.cmd_flags = GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		.cmd_priv = PRIV_KLD_LOAD,
	},
};

static void
ktest_nl_register(void)
{
	bool ret __diagused;
	int family_id __diagused;

	NL_VERIFY_PARSERS(all_parsers);
	family_id = genl_register_family(KTEST_FAMILY_NAME, 0, 1, KTEST_CMD_MAX);
	MPASS(family_id != 0);

	ret = genl_register_cmds(KTEST_FAMILY_NAME, ktest_cmds, NL_ARRAY_LEN(ktest_cmds));
	MPASS(ret);
}

static void
ktest_nl_unregister(void)
{
	MPASS(TAILQ_EMPTY(&module_list));

	genl_unregister_family(KTEST_FAMILY_NAME);
}

static int
ktest_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		ktest_nl_register();
		break;
	case MOD_UNLOAD:
		ktest_nl_unregister();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t ktestmod = {
        "ktest",
        ktest_modevent,
        0
};

DECLARE_MODULE(ktestmod, ktestmod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(ktestmod, 1);
MODULE_DEPEND(ktestmod, netlink, 1, 1, 1);

