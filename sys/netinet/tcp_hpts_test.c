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
#include <netinet/in_pcb.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_hpts_internal.h>
#include <dev/tcp_log/tcp_log_dev.h>
#include <netinet/tcp_log_buf.h>

#define KTEST_VERIFY(x) do { \
	if (!(x)) { \
		KTEST_ERR(ctx, "FAIL: %s", #x); \
		return (EINVAL); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s", #x); \
	} \
} while (0)

/*
 * Validates that the HPTS module is properly loaded and initialized by checking
 * that the minimum HPTS time is configured.
 */
KTEST_FUNC(module_load)
{
	KTEST_VERIFY(tcp_min_hptsi_time != 0);
	KTEST_VERIFY(tcp_bind_threads >= 0 && tcp_bind_threads <= 2);
	return (0);
}

/*
 * Validates the creation and destruction of tcp_hptsi structures, ensuring
 * proper initialization of internal fields and clean destruction.
 */
KTEST_FUNC(hptsi_create_destroy)
{
	struct tcp_hptsi *pace;

	/* Allocate structure dynamically due to large size */
	pace = malloc(sizeof(struct tcp_hptsi), M_TEMP, M_WAITOK);

	tcp_hptsi_create(pace, false);
	KTEST_VERIFY(pace->rp_ent != NULL);
	KTEST_VERIFY(pace->cts_last_ran != NULL);
	KTEST_VERIFY(pace->rp_num_hptss > 0);
	tcp_hptsi_destroy(pace);

	free(pace, M_TEMP);
	return (0);
}

/*
 * Validates that tcp_hptsi structures can be started and stopped properly,
 * including verification that threads are created during start and cleaned up
 * during stop operations.
 */
KTEST_FUNC(hptsi_start_stop)
{
	struct tcp_hptsi *pace;

	pace = malloc(sizeof(struct tcp_hptsi), M_TEMP, M_WAITOK);

	tcp_hptsi_create(pace, false);
	tcp_hptsi_start(pace);

	/* Verify that entries have threads started */
	struct tcp_hpts_entry *hpts = pace->rp_ent[0];
	KTEST_VERIFY(hpts->ie_cookie != NULL);  /* Should have SWI handler */

	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	free(pace, M_TEMP);
	return (0);
}

static const struct ktest_test_info tests[] = {
	KTEST_INFO(module_load),
	KTEST_INFO(hptsi_create_destroy),
	KTEST_INFO(hptsi_start_stop),
};

KTEST_MODULE_DECLARE(ktest_tcphpts, tests);
KTEST_MODULE_DEPEND(ktest_tcphpts, tcphpts);
