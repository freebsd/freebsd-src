/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * This work was supported by Innovate UK project 105694, "Digital Security by
 * Design (DSbD) Technology Platform Prototype".
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/module.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <atf-c.h>

/*
 * Tests 0001-0999 are copied from OpenBSD's regress/sbin/pfctl.
 * Tests 1001-1999 are ours (FreeBSD's own).
 *
 * pf: Run pfctl -nv on pfNNNN.in and check that the output matches pfNNNN.ok.
 *     Copied from OpenBSD.  Main differences are some things not working
 *     in FreeBSD:
 *         * The action 'match'
 *         * The command 'set reassemble'
 *         * The 'from'/'to' options together with 'route-to'
 *         * The option 'scrub' (it is an action in FreeBSD)
 *         * Accepting undefined routing tables in actions (??: see pf0093.in)
 *         * The 'route' option
 *         * The 'set queue def' option
 * selfpf: Feed pfctl output through pfctl again and verify it stays the same.
 *         Copied from OpenBSD.
 */

static bool
check_pf_module_available(void)
{
	int modid;
	struct module_stat stat;

	if ((modid = modfind("pf")) < 0) {
		warn("pf module not found");
		return false;
	}
	stat.version = sizeof(struct module_stat);
	if (modstat(modid, &stat) < 0) {
		warn("can't stat pf module id %d", modid);
		return false;
	}
	return (true);
}

extern char **environ;

static struct sbuf *
read_fd(int fd, size_t sizehint)
{
	struct sbuf *sb;
	ssize_t count;
	char buffer[MAXBSIZE];

	sb = sbuf_new(NULL, NULL, sizehint, SBUF_AUTOEXTEND);
	errno = 0;
	while ((count = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
		sbuf_bcat(sb, buffer, count);
	}
	ATF_REQUIRE_ERRNO(0, count == 0 && "Should have reached EOF");
	sbuf_finish(sb); /* Ensure NULL-termination */
	return (sb);
}

static struct sbuf *
read_file(const char *filename)
{
	struct stat s;
	struct sbuf *result;
	int fd;

	errno = 0;
	ATF_REQUIRE_EQ_MSG(stat(filename, &s), 0, "cannot stat %s", filename);
	fd = open(filename, O_RDONLY);
	ATF_REQUIRE_ERRNO(0, fd > 0);
	result = read_fd(fd, s.st_size);
	ATF_REQUIRE_ERRNO(0, close(fd) == 0);
	return (result);
}

static void
run_pfctl_test(const char *input_path, const char *expected_path,
    const atf_tc_t *tc)
{
	int status;
	pid_t pid;
	int pipefds[2];
	char input_files_path[PATH_MAX];
	struct sbuf *expected_output;
	struct sbuf *real_output;
	posix_spawn_file_actions_t action;

	if (!check_pf_module_available())
		atf_tc_skip("pf(4) is not loaded");

	/* The test inputs need to be able to use relative includes. */
	snprintf(input_files_path, sizeof(input_files_path), "%s/files",
	    atf_tc_get_config_var(tc, "srcdir"));
	ATF_REQUIRE_ERRNO(0, chdir(input_files_path) == 0);

	ATF_REQUIRE_ERRNO(0, pipe(pipefds) == 0);
	expected_output = read_file(expected_path);

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, STDIN_FILENO);
	posix_spawn_file_actions_addclose(&action, pipefds[1]);
	posix_spawn_file_actions_adddup2(&action, pipefds[0], STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&action, pipefds[0], STDERR_FILENO);

	const char *argv[] = { "pfctl", "-o", "none", "-nvf", input_path,
		NULL };
	printf("Running %s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3],
	    argv[4]);
	status = posix_spawnp(
	    &pid, "pfctl", &action, NULL, __DECONST(char **, argv), environ);
	ATF_REQUIRE_EQ_MSG(
	    status, 0, "posix_spawn failed: %s", strerror(errno));
	posix_spawn_file_actions_destroy(&action);
	close(pipefds[0]);

	real_output = read_fd(pipefds[1], 0);
	printf("---\n%s---\n", sbuf_data(real_output));
	ATF_REQUIRE_EQ(waitpid(pid, &status, 0), pid);
	ATF_REQUIRE_MSG(WIFEXITED(status),
	    "pfctl returned non-zero! Output:\n %s", sbuf_data(real_output));

	ATF_CHECK_STREQ(sbuf_data(expected_output), sbuf_data(real_output));
	sbuf_delete(expected_output);
	sbuf_delete(real_output);
	close(pipefds[1]);
}

static void
do_pf_test(const char *number, const atf_tc_t *tc)
{
	char *input_path;
	char *expected_path;
	asprintf(&input_path, "%s/files/pf%s.in",
	    atf_tc_get_config_var(tc, "srcdir"), number);
	asprintf(&expected_path, "%s/files/pf%s.ok",
	    atf_tc_get_config_var(tc, "srcdir"), number);
	run_pfctl_test(input_path, expected_path, tc);
	free(input_path);
	free(expected_path);
}

static void
do_selfpf_test(const char *number, const atf_tc_t *tc)
{
	char *expected_path;
	asprintf(&expected_path, "%s/files/pf%s.ok",
	    atf_tc_get_config_var(tc, "srcdir"), number);
	run_pfctl_test(expected_path, expected_path, tc);
	free(expected_path);
}

#define PFCTL_TEST(number, descr)				\
	ATF_TC(pf##number);					\
	ATF_TC_HEAD(pf##number, tc)				\
	{							\
		atf_tc_set_md_var(tc, "descr", descr);		\
	}							\
	ATF_TC_BODY(pf##number, tc)				\
	{							\
		do_pf_test(#number, tc);			\
	}							\
	ATF_TC(selfpf##number);					\
	ATF_TC_HEAD(selfpf##number, tc)				\
	{							\
		atf_tc_set_md_var(tc, "descr", "Self " descr);	\
	}							\
	ATF_TC_BODY(selfpf##number, tc)				\
	{							\
		do_selfpf_test(#number, tc);			\
	}
#include "pfctl_test_list.inc"
#undef PFCTL_TEST

ATF_TP_ADD_TCS(tp)
{
#define PFCTL_TEST(number, descr)		\
	ATF_TP_ADD_TC(tp, pf##number);		\
	ATF_TP_ADD_TC(tp, selfpf##number);
#include "pfctl_test_list.inc"
#undef PFCTL_TEST

	return atf_no_error();
}
