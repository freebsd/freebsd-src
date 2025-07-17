/*-
 * Copyright 2018 Aniket Pandey
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/extattr.h>
#include <sys/ioctl.h>

#include <bsm/libbsm.h>
#include <bsm/auditd_lib.h>
#include <security/audit/audit_ioctl.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"

/*
 * Checks the presence of "auditregex" in auditpipe(4) after the
 * corresponding system call has been triggered.
 */
static bool
get_records(const char *auditregex, FILE *pipestream)
{
	uint8_t *buff;
	tokenstr_t token;
	ssize_t size = 1024;
	char membuff[size];
	char del[] = ",";
	int reclen, bytes = 0;
	FILE *memstream;

	/*
	 * Open a stream on 'membuff' (address to memory buffer) for storing
	 * the audit records in the default mode.'reclen' is the length of the
	 * available records from auditpipe which is passed to the functions
	 * au_fetch_tok(3) and au_print_flags_tok(3) for further use.
	 */
	ATF_REQUIRE((memstream = fmemopen(membuff, size, "w")) != NULL);
	ATF_REQUIRE((reclen = au_read_rec(pipestream, &buff)) != -1);

	/*
	 * Iterate through each BSM token, extracting the bits that are
	 * required to start processing the token sequences.
	 */
	while (bytes < reclen) {
		if (au_fetch_tok(&token, buff + bytes, reclen - bytes) == -1) {
			perror("au_read_rec");
			atf_tc_fail("Incomplete Audit Record");
		}

		/* Print the tokens as they are obtained, in the default form */
		au_print_flags_tok(memstream, &token, del, AU_OFLAG_NONE);
		fputc(',', memstream);
		bytes += token.len;
	}

	free(buff);
	ATF_REQUIRE_EQ(0, fclose(memstream));
	return (atf_utils_grep_string("%s", membuff, auditregex));
}

/*
 * Override the system-wide audit mask settings in /etc/security/audit_control
 * and set the auditpipe's maximum allowed queue length limit
 */
static void
set_preselect_mode(int filedesc, au_mask_t *fmask)
{
	int qlimit_max;
	int fmode = AUDITPIPE_PRESELECT_MODE_LOCAL;

	/* Set local preselection mode for auditing */
	if (ioctl(filedesc, AUDITPIPE_SET_PRESELECT_MODE, &fmode) < 0)
		atf_tc_fail("Preselection mode: %s", strerror(errno));

	/* Set local preselection flag corresponding to the audit_event */
	if (ioctl(filedesc, AUDITPIPE_SET_PRESELECT_FLAGS, fmask) < 0)
		atf_tc_fail("Preselection flag: %s", strerror(errno));

	/* Set local preselection flag for non-attributable audit_events */
	if (ioctl(filedesc, AUDITPIPE_SET_PRESELECT_NAFLAGS, fmask) < 0)
		atf_tc_fail("Preselection naflag: %s", strerror(errno));

	/* Query the maximum possible queue length limit for auditpipe */
	if (ioctl(filedesc, AUDITPIPE_GET_QLIMIT_MAX, &qlimit_max) < 0)
		atf_tc_fail("Query max-limit: %s", strerror(errno));

	/* Set the queue length limit as obtained from previous step */
	if (ioctl(filedesc, AUDITPIPE_SET_QLIMIT, &qlimit_max) < 0)
		atf_tc_fail("Set max-qlimit: %s", strerror(errno));

	/* This removes any outstanding record on the auditpipe */
	if (ioctl(filedesc, AUDITPIPE_FLUSH) < 0)
		atf_tc_fail("Auditpipe flush: %s", strerror(errno));
}

/*
 * Get the corresponding audit_mask for class-name "name" then set the
 * success and failure bits for fmask to be used as the ioctl argument
 */
static au_mask_t
get_audit_mask(const char *name)
{
	au_mask_t fmask;
	au_class_ent_t *class;

	ATF_REQUIRE((class = getauclassnam(name)) != NULL);
	fmask.am_success = class->ac_class;
	fmask.am_failure = class->ac_class;
	return (fmask);
}

/*
 * Loop until the auditpipe returns something, check if it is what
 * we want, else repeat the procedure until ppoll(2) times out.
 */
static void
check_auditpipe(struct pollfd fd[], const char *auditregex, FILE *pipestream)
{
	struct timespec currtime, endtime, timeout;

	/* Set the expire time for poll(2) while waiting for syscall audit */
	ATF_REQUIRE_EQ(0, clock_gettime(CLOCK_MONOTONIC, &endtime));
	/* Set limit to 30 seconds total and ~10s without an event. */
	endtime.tv_sec += 30;

	for (;;) {
		/* Update the time left for auditpipe to return any event */
		ATF_REQUIRE_EQ(0, clock_gettime(CLOCK_MONOTONIC, &currtime));
		timespecsub(&endtime, &currtime, &timeout);
		timeout.tv_sec = MIN(timeout.tv_sec, 9);
		if (timeout.tv_sec < 0) {
			atf_tc_fail("%s not found in auditpipe within the "
			    "time limit", auditregex);
		}

		switch (ppoll(fd, 1, &timeout, NULL)) {
		/* ppoll(2) returns, check if it's what we want */
		case 1:
			if (fd[0].revents & POLLIN) {
				if (get_records(auditregex, pipestream))
					return;
			} else {
				atf_tc_fail("Auditpipe returned an "
				"unknown event %#x", fd[0].revents);
			}
			break;

		/* poll(2) timed out */
		case 0:
			atf_tc_fail("%s not found in auditpipe within the "
					"time limit", auditregex);
			break;

		/* poll(2) standard error */
		case -1:
			atf_tc_fail("Poll: %s", strerror(errno));
			break;

		default:
			atf_tc_fail("Poll returned too many file descriptors");
		}
	}
}

/*
 * Wrapper functions around static "check_auditpipe"
 */
static void
check_audit_startup(struct pollfd fd[], const char *auditrgx, FILE *pipestream){
	check_auditpipe(fd, auditrgx, pipestream);
}

void
check_audit(struct pollfd fd[], const char *auditrgx, FILE *pipestream) {
	check_auditpipe(fd, auditrgx, pipestream);

	/* Teardown: /dev/auditpipe's instance opened for this test-suite */
	ATF_REQUIRE_EQ(0, fclose(pipestream));
}

void
skip_if_extattr_not_supported(const char *path)
{
	ssize_t result;

	/*
	 * Some file systems (e.g. tmpfs) do not support extattr, so we need
	 * skip tests that use extattrs. To detect this we can check whether
	 * the extattr_list_file returns EOPNOTSUPP.
	 */
	result = extattr_list_file(path, EXTATTR_NAMESPACE_USER, NULL, 0);
	if (result == -1 && errno == EOPNOTSUPP) {
		atf_tc_skip("File system does not support extattrs.");
	}
}

static bool
is_auditd_running(void)
{
	int trigger;
	int err;

	/*
	 * AUDIT_TRIGGER_INITIALIZE is a no-op message on FreeBSD and can
	 * therefore be used to check whether auditd has already been started.
	 * This is significantly cheaper than running `service auditd onestatus`
	 * for each test case. It is also slightly less racy since it will only
	 * return true once auditd() has opened the trigger file rather than
	 * just when the pidfile has been created.
	 */
	trigger = AUDIT_TRIGGER_INITIALIZE;
	err = auditon(A_SENDTRIGGER, &trigger, sizeof(trigger));
	if (err == 0) {
		fprintf(stderr, "auditd(8) is running.\n");
		return (true);
	} else {
		/*
		 * A_SENDTRIGGER returns ENODEV if auditd isn't listening,
		 * all other error codes indicate a fatal error.
		 */
		ATF_REQUIRE_MSG(errno == ENODEV,
		    "Unexpected error from auditon(2): %s", strerror(errno));
		return (false);
	}

}

FILE *
setup(struct pollfd fd[], const char *name)
{
	au_mask_t fmask, nomask;
	FILE *pipestream;
	fmask = get_audit_mask(name);
	nomask = get_audit_mask("no");

	ATF_REQUIRE((fd[0].fd = open("/dev/auditpipe", O_RDONLY)) != -1);
	ATF_REQUIRE((pipestream = fdopen(fd[0].fd, "r")) != NULL);
	fd[0].events = POLLIN;

	/*
	 * Disable stream buffering for read operations from /dev/auditpipe.
	 * Otherwise it is possible that fread(3), called via au_read_rec(3),
	 * can store buffered data in user-space unbeknown to ppoll(2), which
	 * as a result, reports that /dev/auditpipe is empty.
	 */
	ATF_REQUIRE_EQ(0, setvbuf(pipestream, NULL, _IONBF, 0));

	/* Set local preselection audit_class as "no" for audit startup */
	set_preselect_mode(fd[0].fd, &nomask);
	if (!is_auditd_running()) {
		fprintf(stderr, "Running audit_quick_start() for testing... ");
		/*
		 * Previously, this test started auditd using
		 * `service auditd onestart`. However, there is a race condition
		 * there since service can return before auditd(8) has
		 * fully started (once the daemon parent process has forked)
		 * and this can cause check_audit_startup() to fail sometimes.
		 *
		 * In the CheriBSD CI this caused the first test executed by
		 * kyua (administrative:acct_failure) to fail every time, but
		 * subsequent ones would almost always succeed.
		 *
		 * To avoid this problem (and as a nice side-effect this speeds
		 * up the test quite a bit), we register this process as a
		 * "fake" auditd(8) using the audit_quick_start() function from
		 * libauditd.
		 */
		atf_utils_create_file("started_fake_auditd", "yes\n");
		ATF_REQUIRE(atf_utils_file_exists("started_fake_auditd"));
		ATF_REQUIRE_EQ_MSG(0, audit_quick_start(),
		    "Failed to start fake auditd: %m");
		fprintf(stderr, "done.\n");
		/* audit_quick_start() should log an audit start event. */
		check_audit_startup(fd, "audit startup", pipestream);
		/*
		 * If we exit cleanly shutdown audit_quick_start(), if not
		 * cleanup() will take care of it.
		 * This is not required, but makes it easier to run individual
		 * tests outside of kyua.
		 */
		atexit(cleanup);
	}

	/* Set local preselection parameters specific to "name" audit_class */
	set_preselect_mode(fd[0].fd, &fmask);
	return (pipestream);
}

void
cleanup(void)
{
	if (atf_utils_file_exists("started_fake_auditd")) {
		fprintf(stderr, "Running audit_quick_stop()... ");
		if (audit_quick_stop() != 0) {
			fprintf(stderr, "Failed to stop fake auditd: %m\n");
			abort();
		}
		fprintf(stderr, "done.\n");
		unlink("started_fake_auditd");
	}
}
