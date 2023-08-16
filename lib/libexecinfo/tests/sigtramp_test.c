/*-
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	BT_FUNCTIONS		10

void	handler(int);

__noinline void
handler(int signum __unused)
{
	void *addresses[BT_FUNCTIONS];
	char **symbols;
	size_t n, i, match;

	n = backtrace(addresses, nitems(addresses));
	ATF_REQUIRE(n > 1);
	symbols = backtrace_symbols(addresses, n);
	ATF_REQUIRE(symbols != NULL);

	match = -1;
	for (i = 0; i < n; i++) {
		printf("%zu: %p, %s\n", i, addresses[i], symbols[i]);
		if (strstr(symbols[i], "<main+") != NULL)
			match = i;
	}
	ATF_REQUIRE(match > 0);
	printf("match at %zu, symbols %zu\n", match, n);

}

ATF_TC_WITHOUT_HEAD(test_backtrace_sigtramp);
ATF_TC_BODY(test_backtrace_sigtramp, tc)
{
	struct sigaction act;
	pid_t child;
	int status;

	memset(&act, 0, sizeof(act));
	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR1, &act, NULL);

	child = fork();
	ATF_REQUIRE(child != -1);

	if (child == 0) {
		kill(getppid(), SIGUSR1);
		_exit(0);
	} else
		wait(&status);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, test_backtrace_sigtramp);

	return (atf_no_error());
}
