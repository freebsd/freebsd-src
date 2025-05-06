/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/poll.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

#define MAILX	"mailx"
#define BODY	"hello\n"
#define BODYLEN	(sizeof(BODY) - 1)

/*
 * When interactive, mailx(1) should print a message on receipt of SIGINT,
 * then exit cleanly on receipt of a second.
 *
 * When not interactive, mailx(1) should terminate on receipt of SIGINT.
 */
static void
mailx_sigint(bool interactive)
{
	char obuf[1024] = "";
	char ebuf[1024] = "";
	struct pollfd fds[2];
	int ipd[2], opd[2], epd[2], spd[2];
	time_t start, now;
	size_t olen = 0, elen = 0;
	ssize_t rlen;
	pid_t pid;
	int kc, status;

	/* input, output, error, sync pipes */
	if (pipe(ipd) != 0 || pipe(opd) != 0 || pipe(epd) != 0 ||
	    pipe(spd) != 0 || fcntl(spd[1], F_SETFD, FD_CLOEXEC) != 0)
		atf_tc_fail("failed to pipe");
	/* fork child */
	if ((pid = fork()) < 0)
		atf_tc_fail("failed to fork");
	if (pid == 0) {
		/* child */
		dup2(ipd[0], STDIN_FILENO);
		close(ipd[0]);
		close(ipd[1]);
		dup2(opd[1], STDOUT_FILENO);
		close(opd[0]);
		close(opd[1]);
		dup2(epd[1], STDERR_FILENO);
		close(epd[0]);
		close(epd[1]);
		close(spd[0]);
		/* force dead.letter to go to cwd */
		setenv("HOME", ".", 1);
		/* exec mailx */
		execlp(MAILX,
		    MAILX,
		    interactive ? "-Is" : "-s",
		    "test",
		    "test@example.com",
		    NULL);
		_exit(2);
	}
	/* parent */
	close(ipd[0]);
	close(opd[1]);
	close(epd[1]);
	close(spd[1]);
	/* block until child execs or exits */
	(void)read(spd[0], &spd[1], sizeof(spd[1]));
	/* send one line of input */
	ATF_REQUIRE_INTEQ(BODYLEN, write(ipd[1], BODY, BODYLEN));
	/* give it a chance to process */
	poll(NULL, 0, 2000);
	/* send first SIGINT */
	ATF_CHECK_INTEQ(0, kill(pid, SIGINT));
	kc = 1;
	/* receive output until child terminates */
	fds[0].fd = opd[0];
	fds[0].events = POLLIN;
	fds[1].fd = epd[0];
	fds[1].events = POLLIN;
	time(&start);
	for (;;) {
		ATF_REQUIRE(poll(fds, 2, 1000) >= 0);
		if (fds[0].revents == POLLIN && olen < sizeof(obuf)) {
			rlen = read(opd[0], obuf + olen, sizeof(obuf) - olen - 1);
			ATF_REQUIRE(rlen >= 0);
			olen += rlen;
		}
		if (fds[1].revents == POLLIN && elen < sizeof(ebuf)) {
			rlen = read(epd[0], ebuf + elen, sizeof(ebuf) - elen - 1);
			ATF_REQUIRE(rlen >= 0);
			elen += rlen;
		}
		time(&now);
		if (now - start > 1 && elen > 0 && kc == 1) {
			ATF_CHECK_INTEQ(0, kill(pid, SIGINT));
			kc++;
		}
		if (now - start > 15 && kc > 0) {
			(void)kill(pid, SIGKILL);
			kc = -1;
		}
		if (waitpid(pid, &status, WNOHANG) == pid)
			break;
	}
	close(ipd[1]);
	close(opd[0]);
	close(epd[0]);
	close(spd[0]);
	if (interactive) {
		ATF_CHECK(WIFEXITED(status));
		if (WIFEXITED(status))
			ATF_CHECK_INTEQ(1, WEXITSTATUS(status));
		ATF_CHECK_INTEQ(2, kc);
		ATF_CHECK_STREQ("", obuf);
		ATF_CHECK_MATCH("Interrupt -- one more to kill letter", ebuf);
		atf_utils_compare_file("dead.letter", BODY);
	} else {
		ATF_CHECK(WIFSIGNALED(status));
		if (WIFSIGNALED(status))
			ATF_CHECK_INTEQ(SIGINT, WTERMSIG(status));
		ATF_CHECK_INTEQ(1, kc);
		ATF_CHECK_STREQ("", obuf);
		ATF_CHECK_STREQ("", ebuf);
		ATF_CHECK_INTEQ(-1, access("dead.letter", F_OK));
	}
}


ATF_TC_WITHOUT_HEAD(mail_sigint_interactive);
ATF_TC_BODY(mail_sigint_interactive, tc)
{
	mailx_sigint(true);
}

ATF_TC_WITHOUT_HEAD(mail_sigint_noninteractive);
ATF_TC_BODY(mail_sigint_noninteractive, tc)
{
	mailx_sigint(false);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mail_sigint_interactive);
	ATF_TP_ADD_TC(tp, mail_sigint_noninteractive);
	return (atf_no_error());
}
