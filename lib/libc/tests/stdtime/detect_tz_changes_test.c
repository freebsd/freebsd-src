/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "tzdir.h"

#include <atf-c.h>

struct tzcase {
	const char *tzfn;
	const char *expect;
};

static const struct tzcase tzcases[] = {
	/*
	 * A handful of time zones and the expected result of
	 * strftime("%z (%Z)", tm) when that time zone is active
	 * and tm represents a date in the summer of 2025.
	 */
	{ "America/Vancouver",	"-0700 (PDT)"	},
	{ "America/New_York",	"-0400 (EDT)"	},
	{ "Europe/London",	"+0100 (BST)"	},
	{ "Europe/Paris",	"+0200 (CEST)"	},
	{ "Asia/Kolkata",	"+0530 (IST)"	},
	{ "Asia/Tokyo",		"+0900 (JST)"	},
	{ "Australia/Canberra",	"+1000 (AEST)"	},
	{ "UTC",		"+0000 (UTC)"	},
	{ 0 },
};
static const struct tzcase utc = { "UTC", "+0000 (UTC)" };
static const struct tzcase invalid = { "invalid", "+0000 (-00)" };
static const time_t then = 1751328000; /* 2025-07-01 00:00:00 UTC */

static bool debugging;

static void
debug(const char *fmt, ...)
{
	va_list ap;

	if (debugging) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fputc('\n', stderr);
	}
}

static void
change_tz(const char *tzn)
{
	static const char *zfn = TZDIR;
	static const char *tfn = "root" TZDEFAULT ".tmp";
	static const char *dfn = "root" TZDEFAULT;
	ssize_t clen;
	int zfd, sfd, dfd;

	ATF_REQUIRE((zfd = open(zfn, O_DIRECTORY | O_SEARCH)) >= 0);
	ATF_REQUIRE((sfd = openat(zfd, tzn, O_RDONLY)) >= 0);
	ATF_REQUIRE((dfd = open(tfn, O_CREAT | O_TRUNC | O_WRONLY, 0644)) >= 0);
	do {
		clen = copy_file_range(sfd, NULL, dfd, NULL, SSIZE_MAX, 0);
		ATF_REQUIRE_MSG(clen != -1, "failed to copy %s/%s: %m",
		    zfn, tzn);
	} while (clen > 0);
	ATF_CHECK_EQ(0, close(dfd));
	ATF_CHECK_EQ(0, close(sfd));
	ATF_CHECK_EQ(0, close(zfd));
	ATF_REQUIRE_EQ(0, rename(tfn, dfn));
	debug("time zone %s installed", tzn);
}

static void
test_tz(const char *expect)
{
	char buf[128];
	struct tm *tm;
	size_t len;

	ATF_REQUIRE((tm = localtime(&then)) != NULL);
	len = strftime(buf, sizeof(buf), "%z (%Z)", tm);
	ATF_REQUIRE(len > 0);
	ATF_CHECK_STREQ(expect, buf);
}

ATF_TC(tz_default);
ATF_TC_HEAD(tz_default, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test default zone");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(tz_default, tc)
{
	/* prepare chroot with no /etc/localtime */
	ATF_REQUIRE_EQ(0, mkdir("root", 0755));
	ATF_REQUIRE_EQ(0, mkdir("root/etc", 0755));
	/* enter chroot */
	ATF_REQUIRE_EQ(0, chroot("root"));
	ATF_REQUIRE_EQ(0, chdir("/"));
	/* check timezone */
	unsetenv("TZ");
	test_tz("+0000 (UTC)");
}

ATF_TC(tz_invalid_file);
ATF_TC_HEAD(tz_invalid_file, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test invalid zone file");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(tz_invalid_file, tc)
{
	static const char *dfn = "root/etc/localtime";
	int fd;

	/* prepare chroot with bogus /etc/localtime */
	ATF_REQUIRE_EQ(0, mkdir("root", 0755));
	ATF_REQUIRE_EQ(0, mkdir("root/etc", 0755));
	ATF_REQUIRE((fd = open(dfn, O_RDWR | O_CREAT, 0644)) >= 0);
	ATF_REQUIRE_EQ(8, write(fd, "invalid\n", 8));
	ATF_REQUIRE_EQ(0, close(fd));
	/* enter chroot */
	ATF_REQUIRE_EQ(0, chroot("root"));
	ATF_REQUIRE_EQ(0, chdir("/"));
	/* check timezone */
	unsetenv("TZ");
	test_tz(invalid.expect);
}

ATF_TC(thin_jail);
ATF_TC_HEAD(thin_jail, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test typical thin jail scenario");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(thin_jail, tc)
{
	const struct tzcase *tzcase = tzcases;

	/* prepare chroot */
	ATF_REQUIRE_EQ(0, mkdir("root", 0755));
	ATF_REQUIRE_EQ(0, mkdir("root/etc", 0755));
	change_tz(tzcase->tzfn);
	/* enter chroot */
	ATF_REQUIRE_EQ(0, chroot("root"));
	ATF_REQUIRE_EQ(0, chdir("/"));
	/* check timezone */
	unsetenv("TZ");
	test_tz(tzcase->expect);
}

#ifdef DETECT_TZ_CHANGES
/*
 * Test time zone change detection.
 *
 * The parent creates a chroot containing only /etc/localtime, initially
 * set to UTC.  It then forks a child which enters the chroot, repeatedly
 * checks the current time zone, and prints it to stdout if it changes
 * (including once on startup).  Meanwhile, the parent waits for output
 * from the child.  Every time it receives a line of text from the child,
 * it checks that it is as expected, then changes /etc/localtime within
 * the chroot to the next case in the list.  Once it reaches the end of
 * the list, it closes a pipe to notify the child, which terminates.
 *
 * Note that ATF and / or Kyua may have set the timezone before the test
 * case starts (even unintentionally).  Therefore, we start the test only
 * after we've received and discarded the first report from the child,
 * which should come almost immediately on startup.
 */
static const char *tz_change_interval_sym = "__tz_change_interval";
static int *tz_change_interval_p;
static const int tz_change_interval = 3;
static int tz_change_timeout = 90;

ATF_TC(detect_tz_changes);
ATF_TC_HEAD(detect_tz_changes, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test timezone change detection");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "600");
}
ATF_TC_BODY(detect_tz_changes, tc)
{
	char obuf[1024] = "";
	char ebuf[1024] = "";
	struct pollfd fds[3];
	int opd[2], epd[2], spd[2];
	time_t changed, now;
	const struct tzcase *tzcase = NULL;
	struct tm *tm;
	size_t olen = 0, elen = 0;
	ssize_t rlen;
	long curoff = LONG_MIN;
	pid_t pid;
	int nfds, status;

	/* speed up the test if possible */
	tz_change_interval_p = dlsym(RTLD_SELF, tz_change_interval_sym);
	if (tz_change_interval_p != NULL &&
	    *tz_change_interval_p > tz_change_interval) {
		debug("reducing detection interval from %d to %d",
		    *tz_change_interval_p, tz_change_interval);
		*tz_change_interval_p = tz_change_interval;
		tz_change_timeout = tz_change_interval * 3;
	}
	/* prepare chroot */
	ATF_REQUIRE_EQ(0, mkdir("root", 0755));
	ATF_REQUIRE_EQ(0, mkdir("root/etc", 0755));
	change_tz("UTC");
	time(&changed);
	/* output, error, sync pipes */
	if (pipe(opd) != 0 || pipe(epd) != 0 || pipe(spd) != 0)
		atf_tc_fail("failed to pipe");
	/* fork child */
	if ((pid = fork()) < 0)
		atf_tc_fail("failed to fork");
	if (pid == 0) {
		/* child */
		dup2(opd[1], STDOUT_FILENO);
		close(opd[0]);
		close(opd[1]);
		dup2(epd[1], STDERR_FILENO);
		close(epd[0]);
		close(epd[1]);
		close(spd[0]);
		unsetenv("TZ");
		ATF_REQUIRE_EQ(0, chroot("root"));
		ATF_REQUIRE_EQ(0, chdir("/"));
		fds[0].fd = spd[1];
		fds[0].events = POLLIN;
		for (;;) {
			ATF_REQUIRE(poll(fds, 1, 100) >= 0);
			if (fds[0].revents & POLLHUP) {
				/* parent closed sync pipe */
				_exit(0);
			}
			ATF_REQUIRE((tm = localtime(&then)) != NULL);
			if (tm->tm_gmtoff == curoff)
				continue;
			olen = strftime(obuf, sizeof(obuf), "%z (%Z)", tm);
			ATF_REQUIRE(olen > 0);
			fprintf(stdout, "%s\n", obuf);
			fflush(stdout);
			curoff = tm->tm_gmtoff;
		}
		_exit(2);
	}
	/* parent */
	close(opd[1]);
	close(epd[1]);
	close(spd[1]);
	/* receive output until child terminates */
	fds[0].fd = opd[0];
	fds[0].events = POLLIN;
	fds[1].fd = epd[0];
	fds[1].events = POLLIN;
	fds[2].fd = spd[0];
	fds[2].events = POLLIN;
	nfds = 3;
	for (;;) {
		ATF_REQUIRE(poll(fds, 3, 1000) >= 0);
		time(&now);
		if (fds[0].revents & POLLIN && olen < sizeof(obuf)) {
			rlen = read(opd[0], obuf + olen, sizeof(obuf) - olen);
			ATF_REQUIRE(rlen >= 0);
			olen += rlen;
		}
		if (olen > 0) {
			ATF_REQUIRE_EQ('\n', obuf[olen - 1]);
			obuf[--olen] = '\0';
			/* tzcase will be NULL at first */
			if (tzcase != NULL) {
				debug("%s", obuf);
				ATF_REQUIRE_STREQ(tzcase->expect, obuf);
				debug("change to %s detected after %d s",
				    tzcase->tzfn, (int)(now - changed));
				if (tz_change_interval_p != NULL) {
					ATF_CHECK((int)(now - changed) >=
					    *tz_change_interval_p - 1);
					ATF_CHECK((int)(now - changed) <=
					    *tz_change_interval_p + 1);
				}
			}
			olen = 0;
			/* first / next test case */
			if (tzcase == NULL)
				tzcase = tzcases;
			else
				tzcase++;
			if (tzcase->tzfn == NULL) {
				/* test is over */
				break;
			}
			change_tz(tzcase->tzfn);
			changed = now;
		}
		if (fds[1].revents & POLLIN && elen < sizeof(ebuf)) {
			rlen = read(epd[0], ebuf + elen, sizeof(ebuf) - elen);
			ATF_REQUIRE(rlen >= 0);
			elen += rlen;
		}
		if (elen > 0) {
			ATF_REQUIRE_EQ(elen, fwrite(ebuf, 1, elen, stderr));
			elen = 0;
		}
		if (nfds > 2 && fds[2].revents & POLLHUP) {
			/* child closed sync pipe */
			break;
		}
		/*
		 * The timeout for this test case is set to 10 minutes,
		 * because it can take that long to run with the default
		 * 61-second interval.  However, each individual tzcase
		 * entry should not take much longer than the detection
		 * interval to test, so we can detect a problem long
		 * before Kyua terminates us.
		 */
		if ((now - changed) > tz_change_timeout) {
			close(spd[0]);
			if (tz_change_interval_p == NULL &&
			    tzcase == tzcases) {
				/*
				 * The most likely explanation in this
				 * case is that libc was built without
				 * time zone change detection.
				 */
				atf_tc_skip("time zone change detection "
				    "does not appear to be enabled");
			}
			atf_tc_fail("timed out waiting for change to %s "
			    "to be detected", tzcase->tzfn);
		}
	}
	close(opd[0]);
	close(epd[0]);
	close(spd[0]); /* this will wake up and terminate the child */
	if (olen > 0)
		ATF_REQUIRE_EQ(olen, fwrite(obuf, 1, olen, stdout));
	if (elen > 0)
		ATF_REQUIRE_EQ(elen, fwrite(ebuf, 1, elen, stderr));
	ATF_REQUIRE_EQ(pid, waitpid(pid, &status, 0));
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(0, WEXITSTATUS(status));
}
#endif /* DETECT_TZ_CHANGES */

static void
test_tz_env(const char *tzval, const char *expect)
{
	setenv("TZ", tzval, 1);
	test_tz(expect);
}

static void
tz_env_common(void)
{
	char path[MAXPATHLEN];
	const struct tzcase *tzcase = tzcases;
	int len;

	/* relative path */
	for (tzcase = tzcases; tzcase->tzfn != NULL; tzcase++)
		test_tz_env(tzcase->tzfn, tzcase->expect);
	/* absolute path */
	for (tzcase = tzcases; tzcase->tzfn != NULL; tzcase++) {
		len = snprintf(path, sizeof(path), "%s/%s", TZDIR, tzcase->tzfn);
		ATF_REQUIRE(len > 0 && (size_t)len < sizeof(path));
		test_tz_env(path, tzcase->expect);
	}
	/* absolute path with additional slashes */
	for (tzcase = tzcases; tzcase->tzfn != NULL; tzcase++) {
		len = snprintf(path, sizeof(path), "%s/////%s", TZDIR, tzcase->tzfn);
		ATF_REQUIRE(len > 0 && (size_t)len < sizeof(path));
		test_tz_env(path, tzcase->expect);
	}
}

ATF_TC(tz_env);
ATF_TC_HEAD(tz_env, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test TZ environment variable");
}
ATF_TC_BODY(tz_env, tc)
{
	tz_env_common();
	/* escape from TZDIR is permitted when not setugid */
	test_tz_env("../zoneinfo/UTC", utc.expect);
}


ATF_TC(tz_invalid_env);
ATF_TC_HEAD(tz_invalid_env, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test invalid TZ value");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(tz_invalid_env, tc)
{
	test_tz_env("invalid", invalid.expect);
	test_tz_env(":invalid", invalid.expect);
}

ATF_TC(setugid);
ATF_TC_HEAD(setugid, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test setugid process");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(setugid, tc)
{
	const struct tzcase *tzcase = tzcases;

	/* prepare chroot */
	ATF_REQUIRE_EQ(0, mkdir("root", 0755));
	ATF_REQUIRE_EQ(0, mkdir("root/etc", 0755));
	change_tz(tzcase->tzfn);
	/* enter chroot */
	ATF_REQUIRE_EQ(0, chroot("root"));
	ATF_REQUIRE_EQ(0, chdir("/"));
	/* become setugid */
	ATF_REQUIRE_EQ(0, seteuid(UID_NOBODY));
	ATF_REQUIRE(issetugid());
	/* check timezone */
	unsetenv("TZ");
	test_tz(tzcases->expect);
}

ATF_TC(tz_env_setugid);
ATF_TC_HEAD(tz_env_setugid, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test TZ environment variable "
		"in setugid process");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(tz_env_setugid, tc)
{
	ATF_REQUIRE_EQ(0, seteuid(UID_NOBODY));
	ATF_REQUIRE(issetugid());
	tz_env_common();
	/* escape from TZDIR is not permitted when setugid */
	test_tz_env("../zoneinfo/UTC", invalid.expect);
}

ATF_TP_ADD_TCS(tp)
{
	debugging = !getenv("__RUNNING_INSIDE_ATF_RUN") &&
	    isatty(STDERR_FILENO);
	ATF_TP_ADD_TC(tp, tz_default);
	ATF_TP_ADD_TC(tp, tz_invalid_file);
	ATF_TP_ADD_TC(tp, thin_jail);
#ifdef DETECT_TZ_CHANGES
	ATF_TP_ADD_TC(tp, detect_tz_changes);
#endif /* DETECT_TZ_CHANGES */
	ATF_TP_ADD_TC(tp, tz_env);
	ATF_TP_ADD_TC(tp, tz_invalid_env);
	ATF_TP_ADD_TC(tp, setugid);
	ATF_TP_ADD_TC(tp, tz_env_setugid);
	return (atf_no_error());
}
