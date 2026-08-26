/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/resource.h>
#include <sys/select.h>

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <libcasper.h>

#include <atf-c.h>

#define	NCONNECTIONS	(FD_SETSIZE + 64)
#define	FD_HEADROOM	64

/* Test that file descriptors past FD_SETSIZE (1024) work. */
ATF_TC_WITHOUT_HEAD(many_connections);
ATF_TC_BODY(many_connections, tc)
{
	struct rlimit rl;
	cap_channel_t *chan;
	cap_channel_t **clones;
	size_t i;

	if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
		atf_tc_skip("getrlimit: %s", strerror(errno));
	if (rl.rlim_max < NCONNECTIONS + FD_HEADROOM)
		atf_tc_skip("RLIMIT_NOFILE hard cap %ju below required %d",
		    (uintmax_t)rl.rlim_max, NCONNECTIONS + FD_HEADROOM);
	rl.rlim_cur = rl.rlim_max;
	ATF_REQUIRE_MSG(setrlimit(RLIMIT_NOFILE, &rl) == 0,
	    "setrlimit: %s", strerror(errno));

	chan = cap_init();
	ATF_REQUIRE_MSG(chan != NULL, "cap_init failed: %s", strerror(errno));

	clones = calloc(NCONNECTIONS, sizeof(*clones));
	ATF_REQUIRE(clones != NULL);

	/*
	 * Every cap_clone(3) adds one more connection to the helper.
	 * After this loop the helper is watching more fds than an
	 * fd_set can hold.
	 */
	for (i = 0; i < NCONNECTIONS; i++) {
		clones[i] = cap_clone(chan);
		ATF_REQUIRE_MSG(clones[i] != NULL,
		    "cap_clone failed at %zu/%d: %s",
		    i, NCONNECTIONS, strerror(errno));
	}

	for (i = 0; i < NCONNECTIONS; i++)
		cap_close(clones[i]);
	free(clones);
	cap_close(chan);
}

#define	CHURN_CONNECTIONS	50
#define	CHURN_CLOSE_STEP	5

/* Test that gaps in the file descriptor list do not break casper. */
ATF_TC_WITHOUT_HEAD(connection_churn);
ATF_TC_BODY(connection_churn, tc)
{
	cap_channel_t *chan, *survivor, *extra;
	cap_channel_t *clones[CHURN_CONNECTIONS];
	size_t i, survivor_idx;

	chan = cap_init();
	ATF_REQUIRE_MSG(chan != NULL, "cap_init failed: %s", strerror(errno));

	for (i = 0; i < CHURN_CONNECTIONS; i++) {
		clones[i] = cap_clone(chan);
		ATF_REQUIRE_MSG(clones[i] != NULL,
		    "cap_clone failed at %zu: %s", i, strerror(errno));
	}

	/*
	 * Close every Nth clone.
	 */
	for (i = 0; i < CHURN_CONNECTIONS; i += CHURN_CLOSE_STEP) {
		cap_close(clones[i]);
		clones[i] = NULL;
	}

	/*
	 * Force a poll() cycle: the helper handles POLLIN on chan and
	 * POLLHUP on the closed clones in the same walk.
	 */
	extra = cap_clone(chan);
	ATF_REQUIRE_MSG(extra != NULL, "cap_clone after churn failed: %s",
	    strerror(errno));

	/* A surviving clone must still round-trip. */
	survivor_idx = 1;
	survivor = cap_clone(clones[survivor_idx]);
	ATF_REQUIRE_MSG(survivor != NULL,
	    "cap_clone on survivor failed: %s", strerror(errno));

	cap_close(survivor);
	cap_close(extra);
	for (i = 0; i < CHURN_CONNECTIONS; i++) {
		if (clones[i] != NULL)
			cap_close(clones[i]);
	}
	cap_close(chan);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, many_connections);
	ATF_TP_ADD_TC(tp, connection_churn);
	return (atf_no_error());
}
