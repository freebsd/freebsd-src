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

#ifndef	SYS_TESTS_KTEST_H_
#define SYS_TESTS_KTEST_H_

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/syslog.h>

struct nlattr;
struct nl_pstate;
struct nlmsghdr;

struct ktest_test_context {
	void			*arg;
	struct nl_pstate	*npt;
	struct nlmsghdr		*hdr;
	char			*buf;
	size_t			bufsize;
};

typedef int (*ktest_run_t)(struct ktest_test_context *ctx);
typedef int (*ktest_parse_t)(struct ktest_test_context *ctx, struct nlattr *container);

struct ktest_test_info {
	const char	*name;
	const char	*desc;
	ktest_run_t	func;
	ktest_parse_t	parse;
};

struct ktest_module_info {
	const char			*name;
	const struct ktest_test_info	*tests;
	int				num_tests;
	void				*module_ptr;
};

int ktest_default_modevent(module_t mod, int type, void *arg);

bool ktest_start_msg(struct ktest_test_context *ctx);
void ktest_add_msg_meta(struct ktest_test_context *ctx, const char *func,
    const char *fname, int line);
void ktest_add_msg_text(struct ktest_test_context *ctx, int msg_level,
    const char *fmt, ...);
void ktest_end_msg(struct ktest_test_context *ctx);

#define	KTEST_LOG_LEVEL(_ctx, _l, _fmt, ...) {				\
	if (ktest_start_msg(_ctx)) {					\
		ktest_add_msg_meta(_ctx, __func__, __FILE__, __LINE__);	\
		ktest_add_msg_text(_ctx, _l, _fmt, ## __VA_ARGS__);	\
		ktest_end_msg(_ctx);					\
	}								\
}

#define	KTEST_LOG(_ctx, _fmt, ...)					\
	KTEST_LOG_LEVEL(_ctx, LOG_DEBUG, _fmt, ## __VA_ARGS__)

#define KTEST_MAX_BUF	512

#define	KTEST_MODULE_DECLARE(_n, _t) 					\
static struct ktest_module_info _module_info = {			\
	.name = #_n,							\
	.tests = _t,							\
	.num_tests = nitems(_t),					\
};									\
									\
static moduledata_t _module_data = {					\
        #_n,								\
        ktest_default_modevent,						\
        &_module_info,							\
};									\
									\
DECLARE_MODULE(ktest_##_n, _module_data, SI_SUB_PSEUDO, SI_ORDER_ANY);	\
MODULE_VERSION(ktest_##_n, 1);						\
MODULE_DEPEND(ktest_##_n, ktestmod, 1, 1, 1);				\
MODULE_DEPEND(ktest_##_n, netlink, 1, 1, 1);				\

#endif /* _KERNEL */

/* genetlink definitions */
#define KTEST_FAMILY_NAME	"ktest"

/* commands */
enum {
	KTEST_CMD_UNSPEC	= 0,
	KTEST_CMD_LIST		= 1,
	KTEST_CMD_RUN		= 2,
	KTEST_CMD_NEWTEST	= 3,
	KTEST_CMD_NEWMESSAGE	= 4,
	__KTEST_CMD_MAX,
};
#define	KTEST_CMD_MAX	(__KTEST_CMD_MAX - 1)

enum ktest_attr_type_t {
	KTEST_ATTR_UNSPEC,
	KTEST_ATTR_MOD_NAME	= 1,	/* string: test module name */
	KTEST_ATTR_TEST_NAME	= 2,	/* string: test name */
	KTEST_ATTR_TEST_DESCR	= 3,	/* string: test description */
	KTEST_ATTR_TEST_META	= 4,	/* nested: container with test-specific metadata */
};

enum ktest_msg_attr_type_t {
	KTEST_MSG_ATTR_UNSPEC,
	KTEST_MSG_ATTR_TS	= 1,	/* struct timespec */
	KTEST_MSG_ATTR_FUNC	= 2,	/* string: function name */
	KTEST_MSG_ATTR_FILE	= 3,	/* string: file name */
	KTEST_MSG_ATTR_LINE	= 4,	/* u32: line in the file */
	KTEST_MSG_ATTR_TEXT	= 5,	/* string: actual message data */
	KTEST_MSG_ATTR_LEVEL	= 6,	/* u8: syslog loglevel */
	KTEST_MSG_ATTR_META	= 7,	/* nested: message metadata */
};

#endif
