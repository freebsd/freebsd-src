/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * These tests exercise the KERN_PROC_* sysctls.
 */

/*
 * Loop through all valid PIDs and try to fetch info for each one.
 */
static void
sysctl_kern_proc_all(int cmd)
{
	int mib[4], pid_max;
	void *buf;
	size_t sz;

	sz = sizeof(pid_max);
	ATF_REQUIRE(sysctlbyname("kern.pid_max", &pid_max, &sz, NULL, 0) == 0);

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = cmd;
	for (int i = 1; i <= pid_max; i++) {
		mib[3] = i;

		if (sysctl(mib, 4, NULL, &sz, NULL, 0) == 0) {
			buf = malloc(sz);
			ATF_REQUIRE(buf != NULL);
			(void)sysctl(mib, 4, buf, &sz, NULL, 0);
			free(buf);
		}
	}

	mib[3] = -1;
	ATF_REQUIRE_ERRNO(ESRCH, sysctl(mib, 4, NULL, &sz, NULL, 0) != 0);
}

/*
 * Validate behaviour of the KERN_PROC_CWD sysctl.
 */
ATF_TC_WITHOUT_HEAD(sysctl_kern_proc_cwd);
ATF_TC_BODY(sysctl_kern_proc_cwd, tc)
{
	struct kinfo_file kfile;
	char cwd[PATH_MAX];
	int cmd, mib[4];
	size_t sz;
	pid_t child;
	int status;

	cmd = KERN_PROC_CWD;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = cmd;
	mib[3] = getpid();

	/* Try querying the kernel for the output buffer size. */
	sz = 0;
	ATF_REQUIRE(sysctl(mib, 4, NULL, &sz, NULL, 0) == 0);
	ATF_REQUIRE(sz <= sizeof(kfile));

	sz = sizeof(kfile);
	memset(&kfile, 0, sz);
	ATF_REQUIRE(sysctl(mib, 4, &kfile, &sz, NULL, 0) == 0);
	ATF_REQUIRE(sz <= sizeof(kfile));
	ATF_REQUIRE(sz == (u_int)kfile.kf_structsize);

	/* Make sure that we get the same result from getcwd(2). */
	ATF_REQUIRE(getcwd(cwd, sizeof(cwd)) == cwd);
	ATF_REQUIRE(strcmp(cwd, kfile.kf_path) == 0);

	/* Spot-check some of the kinfo fields. */
	ATF_REQUIRE(kfile.kf_type == KF_TYPE_VNODE);
	ATF_REQUIRE(kfile.kf_fd == KF_FD_TYPE_CWD);
	ATF_REQUIRE(S_ISDIR(kfile.kf_un.kf_file.kf_file_mode));
	ATF_REQUIRE((kfile.kf_status & KF_ATTR_VALID) != 0);

	/*
	 * Verify that a child process can get our CWD info, and that it
	 * matches the info we got above.
	 */
	child = fork();
	ATF_REQUIRE(child != -1);
	if (child == 0) {
		struct kinfo_file pkfile;

		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_CWD;
		mib[3] = getppid();

		sz = sizeof(pkfile);
		memset(&pkfile, 0, sz);
		if (sysctl(mib, 4, &pkfile, &sz, NULL, 0) != 0)
			_exit(1);
		if (memcmp(&kfile, &pkfile, sizeof(kfile)) != 0)
			_exit(2);
		_exit(0);
	}
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);

	/*
	 * Truncate the output buffer ever so slightly and make sure that we get
	 * an error.
	 */
	sz--;
	ATF_REQUIRE_ERRNO(ENOMEM, sysctl(mib, 4, &kfile, &sz, NULL, 0) != 0);

	sysctl_kern_proc_all(cmd);
}

/*
 * Validate behaviour of the KERN_PROC_FILEDESC sysctl.
 */
ATF_TC_WITHOUT_HEAD(sysctl_kern_proc_filedesc);
ATF_TC_BODY(sysctl_kern_proc_filedesc, tc)
{
	int cmd, fd, mib[4];
	struct kinfo_file *kfile;
	char *buf, tmp[16];
	size_t sz, sz1;

	cmd = KERN_PROC_FILEDESC;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = cmd;
	mib[3] = getpid();

	sz = 0;
	ATF_REQUIRE(sysctl(mib, 4, NULL, &sz, NULL, 0) == 0);
	ATF_REQUIRE(sz >= __offsetof(struct kinfo_file, kf_structsize) +
	    sizeof(kfile->kf_structsize));

	buf = malloc(sz);
	ATF_REQUIRE(buf != NULL);

	ATF_REQUIRE(sysctl(mib, 4, buf, &sz, NULL, 0) == 0);

	/* Walk over the list of returned files. */
	for (sz1 = 0; sz1 < sz; sz1 += kfile->kf_structsize) {
		kfile = (void *)(buf + sz1);

		ATF_REQUIRE((unsigned int)kfile->kf_structsize <= sz);
		ATF_REQUIRE((unsigned int)kfile->kf_structsize + sz1 <= sz);

		ATF_REQUIRE((kfile->kf_status & KF_ATTR_VALID) != 0);
	}
	/* We shouldn't have any trailing bytes. */
	ATF_REQUIRE(sz1 == sz);

	/*
	 * Open a file.  This increases the size of the output buffer, so an
	 * attempt to re-fetch the records without increasing the buffer size
	 * should fail with ENOMEM.
	 */
	snprintf(tmp, sizeof(tmp), "tmp.XXXXXX");
	fd = mkstemp(tmp);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_ERRNO(ENOMEM, sysctl(mib, 4, buf, &sz, NULL, 0) != 0);

	ATF_REQUIRE(unlink(tmp) == 0);
	ATF_REQUIRE(close(fd) == 0);

	free(buf);

	sysctl_kern_proc_all(cmd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sysctl_kern_proc_cwd);
	ATF_TP_ADD_TC(tp, sysctl_kern_proc_filedesc);

	return (atf_no_error());
}
