/*
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
 *
 * This software was written by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <mntopts.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	DIR_GID		54320	/* group owning the test directory */
#define	TEST_UID	54321
#define	TEST_GID	54322

#define	TESTDIR		"mnt/dir"
#define	TESTPATH	TESTDIR "/node"

enum nodetype {
	NODE_FILE,
	NODE_FIFO,
};

/*
 * Mount tmpfs and make a world-writeable directory.
 */
static void
mount_tmpfs(void)
{
	struct iovec *iov;
	char errmsg[256];
	int error, iovlen;

	iov = NULL;
	iovlen = 0;

	ATF_REQUIRE(mkdir("mnt", 0755) == 0);
	build_iovec(&iov, &iovlen, "fstype", __DECONST(char *, "tmpfs"),
	    (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", __DECONST(char *, "mnt"),
	    (size_t)-1);
	build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));
	errmsg[0] = '\0';
	error = nmount(iov, iovlen, 0);
	ATF_REQUIRE_MSG(error == 0, "nmount: %s",
	    errmsg[0] != '\0' ? errmsg : strerror(errno));
	free_iovec(&iov, &iovlen);

	ATF_REQUIRE(mkdir("mnt/dir", 0777) == 0);
	ATF_REQUIRE(chown("mnt/dir", 0, DIR_GID) == 0);
	ATF_REQUIRE(chmod("mnt/dir", 0777) == 0);
}

static void
unmount_tmpfs(void)
{
	int error;

	error = unmount("mnt", MNT_FORCE);
	if (error != 0)
		warn("unmount");
}

/*
 * Try to create a setgid file, and return true if the setgid bit was applied. 
 */
static bool
create_setgid(enum nodetype type, uid_t uid, gid_t gid, const gid_t *groups,
    int ngroups)
{
	struct stat sb;
	pid_t pid;
	int fd, status;

	pid = fork();
	ATF_REQUIRE(pid != -1);
	if (pid == 0) {
		if (setgroups(ngroups, groups) != 0 ||
		    setgid(gid) != 0 ||
		    setuid(uid) != 0)
			_exit(errno);
		(void)umask(0);
		switch (type) {
		case NODE_FILE:
			fd = open("mnt/dir/a", O_WRONLY | O_CREAT | O_EXCL,
			    02755);
			if (fd < 0)
				_exit(errno);
			(void)close(fd);
			break;
		case NODE_FIFO:
			if (mkfifo("mnt/dir/a", 02644) != 0)
				_exit(errno);
			break;
		}
		_exit(0);
	}
	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE_MSG(WIFEXITED(status), "status %#x", status);
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0, "child failed: %d",
	    WEXITSTATUS(status));

	ATF_REQUIRE(stat("mnt/dir/a", &sb) == 0);
	ATF_REQUIRE_EQ(uid, sb.st_uid);
	ATF_REQUIRE_EQ(DIR_GID, sb.st_gid);
	return ((sb.st_mode & S_ISGID) != 0);
}

ATF_TC_WITH_CLEANUP(nonmember_file);
ATF_TC_HEAD(nonmember_file, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(nonmember_file, tc)
{
	const gid_t groups[] = { TEST_GID };

	mount_tmpfs();
	ATF_REQUIRE_MSG(!create_setgid(NODE_FILE, TEST_UID, TEST_GID, groups,
	    nitems(groups)), "S_ISGID was not cleared");
}
ATF_TC_CLEANUP(nonmember_file, tc)
{
	unmount_tmpfs();
}

ATF_TC_WITH_CLEANUP(nonmember_fifo);
ATF_TC_HEAD(nonmember_fifo, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(nonmember_fifo, tc)
{
	const gid_t groups[] = { TEST_GID };

	mount_tmpfs();
	ATF_REQUIRE_MSG(!create_setgid(NODE_FIFO, TEST_UID, TEST_GID, groups,
	    nitems(groups)), "S_ISGID was not cleared");
}
ATF_TC_CLEANUP(nonmember_fifo, tc)
{
	unmount_tmpfs();
}

ATF_TC_WITH_CLEANUP(member_file);
ATF_TC_HEAD(member_file, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(member_file, tc)
{
	const gid_t groups[] = { DIR_GID };

	mount_tmpfs();
	ATF_REQUIRE_MSG(create_setgid(NODE_FILE, TEST_UID, DIR_GID, groups,
	    nitems(groups)), "S_ISGID was cleared");
}
ATF_TC_CLEANUP(member_file, tc)
{
	unmount_tmpfs();
}

ATF_TC_WITH_CLEANUP(supplementary_member_file);
ATF_TC_HEAD(supplementary_member_file, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(supplementary_member_file, tc)
{
	const gid_t groups[] = { DIR_GID };

	mount_tmpfs();
	ATF_REQUIRE_MSG(create_setgid(NODE_FILE, TEST_UID, TEST_GID, groups,
	    nitems(groups)), "S_ISGID was cleared");
}
ATF_TC_CLEANUP(supplementary_member_file, tc)
{
	unmount_tmpfs();
}

ATF_TC_WITH_CLEANUP(privileged_file);
ATF_TC_HEAD(privileged_file, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(privileged_file, tc)
{
	const gid_t groups[] = { TEST_GID };

	mount_tmpfs();
	ATF_REQUIRE_MSG(create_setgid(NODE_FILE, 0, TEST_GID, groups,
	    nitems(groups)), "S_ISGID was cleared");
}
ATF_TC_CLEANUP(privileged_file, tc)
{
	unmount_tmpfs();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, nonmember_file);
	ATF_TP_ADD_TC(tp, nonmember_fifo);
	ATF_TP_ADD_TC(tp, member_file);
	ATF_TP_ADD_TC(tp, supplementary_member_file);
	ATF_TP_ADD_TC(tp, privileged_file);
	return (atf_no_error());
}
