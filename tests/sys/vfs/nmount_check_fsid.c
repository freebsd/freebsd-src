/*-
 * Copyright (c) 2026 Gleb Popov <arrowd@FreeBSD.org>
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
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <mntopts.h>
#include <stdio.h>
#include <stdlib.h>

#include "freebsd_test_suite/macros.h"


static char *
fsid_to_str(fsid_t fsid)
{
	char* ret;
	asprintf(&ret, "FSID:%d:%d", fsid.val[0], fsid.val[1]);
	ATF_REQUIRE(ret != NULL);
	return ret;
}

ATF_TC_WITH_CLEANUP(mount_check_fsid_positive);
ATF_TC_HEAD(mount_check_fsid_positive, tc)
{
	atf_tc_set_md_var(tc, "descr", "Positive testcase for nmount() with check_fsid");
}
ATF_TC_BODY(mount_check_fsid_positive, tc)
{
	int mnt_fd = -1;
	char mnt[PATH_MAX];
	char *fsid;
	struct statfs statfs_buf;
	struct iovec *iov = NULL;
	int iovlen = 0;

	ATF_REQUIRE_INTEQ(0, mkdir("mnt", S_IRWXU));
	ATF_REQUIRE(realpath("mnt", mnt) != NULL);

	ATF_REQUIRE((mnt_fd = open(mnt, O_RDONLY)) >= 0);

	ATF_REQUIRE_INTEQ(0, fstatfs(mnt_fd, &statfs_buf));

	fsid = fsid_to_str(statfs_buf.f_fsid);

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "fspath", mnt, -1);
	build_iovec(&iov, &iovlen, "check_fsid", fsid, -1);

	ATF_REQUIRE_INTEQ(0, nmount(iov, iovlen, 0));

	free_iovec(&iov, &iovlen);
	free(fsid);
}
ATF_TC_CLEANUP(mount_check_fsid_positive, tc)
{
	unmount("mnt", MNT_FORCE);
}

ATF_TC_WITH_CLEANUP(mount_check_fsid_negative);
ATF_TC_HEAD(mount_check_fsid_negative, tc)
{
	atf_tc_set_md_var(tc, "descr", "Negative testcase for nmount() with check_fsid");
}
ATF_TC_BODY(mount_check_fsid_negative, tc)
{
	int mnt_fd = -1;
	char mnt[PATH_MAX];
	char *fsid;
	struct statfs statfs_buf;
	struct iovec *iov = NULL;
	int iovlen = 0;

	ATF_REQUIRE_INTEQ(0, mkdir("mnt", S_IRWXU));
	ATF_REQUIRE(realpath("mnt", mnt) != NULL);

	ATF_REQUIRE((mnt_fd = open("mnt", O_RDONLY)) >= 0);

	ATF_REQUIRE_INTEQ(0, fstatfs(mnt_fd, &statfs_buf));

	/* create invalid fsid */
	statfs_buf.f_fsid.val[0]++;
	statfs_buf.f_fsid.val[1]--;

	fsid = fsid_to_str(statfs_buf.f_fsid);

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "fspath", mnt, -1);
	build_iovec(&iov, &iovlen, "check_fsid", fsid, -1);

	ATF_REQUIRE_ERRNO(ENOENT, nmount(iov, iovlen, 0) < 0);

	free_iovec(&iov, &iovlen);
	free(fsid);
}
ATF_TC_CLEANUP(mount_check_fsid_negative, tc)
{
	unmount("mnt", MNT_FORCE);
}

ATF_TC_WITH_CLEANUP(update_check_fsid_positive);
ATF_TC_HEAD(update_check_fsid_positive, tc)
{
	atf_tc_set_md_var(tc, "descr", "Positive testcase for updating a mount with check_fsid");
}
ATF_TC_BODY(update_check_fsid_positive, tc)
{
	int mnt_fd = -1;
	char mnt[PATH_MAX];
	char *fsid;
	struct statfs statfs_buf;
	struct iovec *iov = NULL;
	int iovlen = 0;

	ATF_REQUIRE_INTEQ(0, mkdir("mnt", S_IRWXU));
	ATF_REQUIRE(realpath("mnt", mnt) != NULL);

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "fspath", mnt, -1);

	ATF_REQUIRE_INTEQ(0, nmount(iov, iovlen, 0));

	free_iovec(&iov, &iovlen);

	ATF_REQUIRE((mnt_fd = open(mnt, O_RDONLY)) >= 0);

	ATF_REQUIRE_INTEQ(0, fstatfs(mnt_fd, &statfs_buf));

	fsid = fsid_to_str(statfs_buf.f_fsid);

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "fspath", mnt, -1);
	build_iovec(&iov, &iovlen, "ro", NULL, -1);
	build_iovec(&iov, &iovlen, "update", NULL, -1);
	build_iovec(&iov, &iovlen, "check_fsid", fsid, -1);

	ATF_REQUIRE_INTEQ(0, nmount(iov, iovlen, 0));

	free_iovec(&iov, &iovlen);
	free(fsid);
}
ATF_TC_CLEANUP(update_check_fsid_positive, tc)
{
	unmount("mnt", MNT_FORCE);
}

ATF_TC_WITH_CLEANUP(update_check_fsid_negative);
ATF_TC_HEAD(update_check_fsid_negative, tc)
{
	atf_tc_set_md_var(tc, "descr", "Negative testcase for updating a mount with check_fsid");
}
ATF_TC_BODY(update_check_fsid_negative, tc)
{
	int mnt_fd = -1;
	char mnt[PATH_MAX];
	char *fsid;
	struct statfs statfs_buf;
	struct iovec *iov = NULL;
	int iovlen = 0;

	ATF_REQUIRE_INTEQ(0, mkdir("mnt", S_IRWXU));
	ATF_REQUIRE(realpath("mnt", mnt) != NULL);

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "fspath", mnt, -1);

	ATF_REQUIRE_INTEQ(0, nmount(iov, iovlen, 0));

	free_iovec(&iov, &iovlen);

	ATF_REQUIRE((mnt_fd = open(mnt, O_RDONLY)) >= 0);

	ATF_REQUIRE_INTEQ(0, fstatfs(mnt_fd, &statfs_buf));

	/* create invalid fsid */
	statfs_buf.f_fsid.val[0]++;
	statfs_buf.f_fsid.val[1]--;

	fsid = fsid_to_str(statfs_buf.f_fsid);

	build_iovec(&iov, &iovlen, "fstype", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "from", __DECONST(void *, "tmpfs"), -1);
	build_iovec(&iov, &iovlen, "fspath", mnt, -1);
	build_iovec(&iov, &iovlen, "ro", NULL, -1);
	build_iovec(&iov, &iovlen, "update", NULL, -1);
	build_iovec(&iov, &iovlen, "check_fsid", fsid, -1);

	ATF_REQUIRE_ERRNO(ENOENT, nmount(iov, iovlen, 0) < 0);

	free_iovec(&iov, &iovlen);
	free(fsid);
}
ATF_TC_CLEANUP(update_check_fsid_negative, tc)
{
	unmount("mnt", MNT_FORCE);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, mount_check_fsid_positive);
	ATF_TP_ADD_TC(tp, mount_check_fsid_negative);

	ATF_TP_ADD_TC(tp, update_check_fsid_positive);
	ATF_TP_ADD_TC(tp, update_check_fsid_negative);

	return (atf_no_error());
}
