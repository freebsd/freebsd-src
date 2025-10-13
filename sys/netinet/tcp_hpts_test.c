/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Netflix, Inc.
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
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#define	_WANT_INPCB
#include <netinet/in_pcb.h>
#include <netinet/tcp_seq.h>
#define	_WANT_TCPCB
#include <netinet/tcp_var.h>
#include <netinet/tcp_hpts.h>
#include <dev/tcp_log/tcp_log_dev.h>
#include <netinet/tcp_log_buf.h>

#define	KTEST_ERR(_ctx, _fmt, ...)	\
	KTEST_LOG_LEVEL(_ctx, LOG_ERR, _fmt, ## __VA_ARGS__)

#define KTEST_VERIFY(x) do { \
	if (!(x)) { \
		KTEST_ERR(ctx, "FAIL: %s", #x); \
		return (EINVAL); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s", #x); \
	} \
} while (0)

static int
test_hpts_init(struct ktest_test_context *ctx)
{
	/* TODO: Refactor HPTS code so that it may be tested. */
	KTEST_VERIFY(tcp_min_hptsi_time != 0);
	return (0);
}

static const struct ktest_test_info tests[] = {
	{
		.name = "test_hpts_init",
		.desc = "Tests HPTS initialization and cleanup",
		.func = &test_hpts_init,
	},
};

KTEST_MODULE_DECLARE(ktest_tcphpts, tests);
KTEST_MODULE_DEPEND(ktest_tcphpts, tcphpts);
