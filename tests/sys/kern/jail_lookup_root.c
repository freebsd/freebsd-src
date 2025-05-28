/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <jail.h>
#include <mntopts.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

static void
mkdir_checked(const char *dir, mode_t mode)
{
	int error;

	error = mkdir(dir, mode);
	ATF_REQUIRE_MSG(error == 0 || errno == EEXIST,
	    "mkdir %s: %s", dir, strerror(errno));
}

static void __unused
mount_nullfs(const char *dir, const char *target)
{
	struct iovec *iov;
	char errmsg[1024];
	int error, iovlen;

	iov = NULL;
	iovlen = 0;

	build_iovec(&iov, &iovlen, __DECONST(char *, "fstype"),
	    __DECONST(char *, "nullfs"), (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "fspath"),
	    __DECONST(char *, target), (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "from"),
	    __DECONST(char *, dir), (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "errmsg"),
	    errmsg, sizeof(errmsg));

	errmsg[0] = '\0';
	error = nmount(iov, iovlen, 0);
	ATF_REQUIRE_MSG(error == 0, "nmount: %s",
	    errmsg[0] != '\0' ? errmsg : strerror(errno));

	free_iovec(&iov, &iovlen);
}

ATF_TC_WITH_CLEANUP(jail_root);
ATF_TC_HEAD(jail_root, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_root, tc)
{
	int error, fd, jid;

	mkdir_checked("./root", 0755);
	mkdir_checked("./root/a", 0755);
	mkdir_checked("./root/b", 0755);
	mkdir_checked("./root/a/c", 0755);

	jid = jail_setv(JAIL_CREATE | JAIL_ATTACH,
	    "name", "nullfs_jail_root_test",
	    "allow.mount", "true",
	    "allow.mount.nullfs", "true",
	    "enforce_statfs", "1",
	    "path", "./root",
	    "persist", NULL,
	    NULL);
	ATF_REQUIRE_MSG(jid >= 0, "jail_setv: %s", jail_errmsg);

	mount_nullfs("/a", "/b");

	error = chdir("/b/c");
	ATF_REQUIRE(error == 0);

	error = rename("/a/c", "/c");
	ATF_REQUIRE(error == 0);

	/* Descending to the jail root should be ok. */
	error = chdir("..");
	ATF_REQUIRE(error == 0);

	/* Going beyond the root will trigger an error. */
	error = chdir("..");
	ATF_REQUIRE_ERRNO(ENOENT, error != 0);
	fd = open("..", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE_ERRNO(ENOENT, fd < 0);
}
ATF_TC_CLEANUP(jail_root, tc)
{
	struct statfs fs;
	fsid_t fsid;
	int error, jid;

	error = statfs("./root/b", &fs);
	if (error != 0)
		err(1, "statfs ./b");
	fsid = fs.f_fsid;
	error = statfs("./root", &fs);
	if (error != 0)
		err(1, "statfs ./root");
	if (fsid.val[0] != fs.f_fsid.val[0] ||
	    fsid.val[1] != fs.f_fsid.val[1]) {
		error = unmount("./root/b", 0);
		if (error != 0)
			err(1, "unmount ./root/b");
	}

	jid = jail_getid("nullfs_jail_root_test");
	if (jid >= 0) {
		error = jail_remove(jid);
		if (error != 0)
			err(1, "jail_remove");
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, jail_root);
	return (atf_no_error());
}
