#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/filio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *const linrdlnk[] = { "linrdlnk", NULL };
static const char *const nodup[] = { "nodup", NULL };
static const char *const nodup_linrdlnk[] = { "nodup", "linrdlnk", NULL };

static void
build_iovec(struct iovec **iov, int *iovlen, const char *name, void *val,
	    size_t len)
{
	int i;

	if (*iovlen < 0)
		return;
	i = *iovlen;
	*iov = realloc(*iov, sizeof **iov * (i + 2));
	if (*iov == NULL) {
		*iovlen = -1;
		return;
	}
	(*iov)[i].iov_base = strdup(name);
	(*iov)[i].iov_len = strlen(name) + 1;
	i++;
	(*iov)[i].iov_base = val;
	if (len == (size_t)-1) {
		if (val != NULL)
			len = strlen(val) + 1;
		else
			len = 0;
	}
	(*iov)[i].iov_len = (int)len;
	*iovlen = ++i;
}

static void
free_iovec(struct iovec **iov, int *iovlen)
{
	int i;

	for (i = 0; i < *iovlen; i += 2)
		free((*iov)[i].iov_base);
	free(*iov);
}

static void
mount_fdescfs(const char *const *opts)
{
	struct iovec *iov;
	char errmsg[256];
	int error, iovlen;

	ATF_REQUIRE_EQ(0, mkdir("mnt", 0755));
	iov = NULL;
	iovlen = 0;
	build_iovec(&iov, &iovlen, __DECONST(char *, "fstype"),
	    __DECONST(char *, "fdescfs"), (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "fspath"),
	    __DECONST(char *, "mnt"), (size_t)-1);
	for (; opts != NULL && *opts != NULL; opts++)
		build_iovec(&iov, &iovlen, __DECONST(char *, *opts), NULL,
		    (size_t)-1);
	build_iovec(&iov, &iovlen, __DECONST(char *, "errmsg"), errmsg,
	    sizeof(errmsg));
	errmsg[0] = '\0';
	error = nmount(iov, iovlen, 0);
	if (error != 0 && errno == ENODEV)
		atf_tc_skip("fdescfs is not available");
	ATF_REQUIRE_MSG(error == 0, "nmount: %s",
	    errmsg[0] != '\0' ? errmsg : strerror(errno));
	free_iovec(&iov, &iovlen);
}

static void
fdpath(char *path, size_t size, int fd, const char *suffix)
{
	int len;

	len = snprintf(path, size, "mnt/%d%s", fd, suffix);
	ATF_REQUIRE(len > 0 && (size_t)len < size);
}

static void
check_cap_rights(const char *const *opts)
{
	cap_rights_t expected, actual;
	char path[64];
	int copy, fd;

	mount_fdescfs(opts);
	fd = open("file", O_RDONLY | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	cap_rights_init(&expected, CAP_READ, CAP_FSTAT);
	ATF_REQUIRE_EQ(0, cap_rights_limit(fd, &expected));
	fdpath(path, sizeof(path), fd, "");
	copy = open(path, O_RDONLY);
	ATF_REQUIRE(copy >= 0);
	ATF_REQUIRE_EQ(0, cap_rights_get(copy, &actual));
	ATF_CHECK(cap_rights_contains(&actual, &expected));
	ATF_CHECK(cap_rights_contains(&expected, &actual));
	ATF_REQUIRE_EQ(0, close(copy));
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

static void
check_resolve_beneath(const char *const *opts, int oflags)
{
	char path[64];
	int copy, dirfd, fd;

	mount_fdescfs(opts);
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	dirfd = open("dir", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(dirfd >= 0);
	ATF_REQUIRE_EQ(0, fcntl(dirfd, F_SETFD, FD_RESOLVE_BENEATH));
	fdpath(path, sizeof(path), dirfd, "");
	copy = open(path, oflags);
	ATF_REQUIRE(copy >= 0);
	fd = openat(copy, ".", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_CHECK_ERRNO(ENOTCAPABLE,
	    openat(copy, "..", O_RDONLY | O_DIRECTORY) == -1);
	ATF_REQUIRE_EQ(0, close(copy));
	ATF_REQUIRE_EQ(0, close(dirfd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

static void
check_ioctl_caps(const char *const *opts)
{
	cap_ioctl_t cmds[] = { FIOCLEX };
	char path[64];
	int copy, fd;

	mount_fdescfs(opts);
	fd = open("file", O_RDONLY | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ(0, cap_ioctls_limit(fd, cmds, nitems(cmds)));
	fdpath(path, sizeof(path), fd, "");
	copy = open(path, O_RDONLY);
	ATF_REQUIRE(copy >= 0);
	ATF_REQUIRE_EQ(0, ioctl(copy, FIOCLEX, 0));
	ATF_CHECK_ERRNO(ENOTCAPABLE, ioctl(copy, FIONCLEX, 0) == -1);
	ATF_REQUIRE_EQ(0, close(copy));
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

#define FDESCFS_TC(name, description)                          \
	ATF_TC_WITH_CLEANUP(name);                             \
	ATF_TC_HEAD(name, tc)                                  \
	{                                                      \
		atf_tc_set_md_var(tc, "descr", description);   \
		atf_tc_set_md_var(tc, "require.user", "root"); \
		atf_tc_set_md_var(tc, "timeout", "30");        \
	}                                                      \
	ATF_TC_CLEANUP(name, tc)                               \
	{                                                      \
		(void)unmount("mnt", 0);                       \
	}

FDESCFS_TC(cap_rights, "fdescfs preserves capability rights");
ATF_TC_BODY(cap_rights, tc)
{
	check_cap_rights(NULL);
}

FDESCFS_TC(nodup_cap_rights, "nodup mounts preserve capability rights");
ATF_TC_BODY(nodup_cap_rights, tc)
{
	check_cap_rights(nodup);
}

FDESCFS_TC(resolve_beneath, "fdescfs preserves the FD_RESOLVE_BENEATH flag");
ATF_TC_BODY(resolve_beneath, tc)
{
	check_resolve_beneath(NULL, O_RDONLY);
}

FDESCFS_TC(nodup_resolve_beneath,
    "nodup mounts preserve the FD_RESOLVE_BENEATH flag");
ATF_TC_BODY(nodup_resolve_beneath, tc)
{
	check_resolve_beneath(nodup, O_RDONLY | O_DIRECTORY);
}

FDESCFS_TC(ioctl_caps, "fdescfs preserves ioctl capability restrictions");
ATF_TC_BODY(ioctl_caps, tc)
{
	check_ioctl_caps(NULL);
}

FDESCFS_TC(nodup_ioctl_caps,
    "nodup mounts preserve ioctl capability restrictions");
ATF_TC_BODY(nodup_ioctl_caps, tc)
{
	check_ioctl_caps(nodup);
}

FDESCFS_TC(nodup_ioctl_cap_intersection,
    "nodup mounts intersect ioctl caps from source fd and dirfd");
ATF_TC_BODY(nodup_ioctl_cap_intersection, tc)
{
	cap_ioctl_t fd_cmds[] = { FIOCLEX };
	cap_ioctl_t both_cmds[] = { FIOCLEX, FIONCLEX };
	cap_ioctl_t fionclex_cmds[] = { FIONCLEX };
	char fd_path[16];
	int copy, fd, len, mntfd;

	mount_fdescfs(nodup);
	fd = open("file", O_RDONLY | O_CREAT, 0644);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE_EQ(0, cap_ioctls_limit(fd, fd_cmds, nitems(fd_cmds)));
	len = snprintf(fd_path, sizeof(fd_path), "%d", fd);
	ATF_REQUIRE(len > 0 && (size_t)len < sizeof(fd_path));

	/*
	 * Non-empty intersection: loop removes FIONCLEX, leaving {FIOCLEX}.
	 */
	mntfd = open("mnt", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(mntfd >= 0);
	ATF_REQUIRE_EQ(0, cap_ioctls_limit(mntfd, both_cmds, nitems(both_cmds)));
	copy = openat(mntfd, fd_path, O_RDONLY);
	ATF_REQUIRE(copy >= 0);
	ATF_REQUIRE_EQ(0, ioctl(copy, FIOCLEX, 0));
	ATF_CHECK_ERRNO(ENOTCAPABLE, ioctl(copy, FIONCLEX, 0) == -1);
	ATF_REQUIRE_EQ(0, close(copy));
	ATF_REQUIRE_EQ(0, close(mntfd));

	/*
	 * Empty intersection: loop removes FIONCLEX from {FIONCLEX} since fd
	 * only allows {FIOCLEX}, leaving an empty list.
	 */
	mntfd = open("mnt", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(mntfd >= 0);
	ATF_REQUIRE_EQ(0,
	    cap_ioctls_limit(mntfd, fionclex_cmds, nitems(fionclex_cmds)));
	copy = openat(mntfd, fd_path, O_RDONLY);
	ATF_REQUIRE(copy >= 0);
	ATF_CHECK_ERRNO(ENOTCAPABLE, ioctl(copy, FIOCLEX, 0) == -1);
	ATF_CHECK_ERRNO(ENOTCAPABLE, ioctl(copy, FIONCLEX, 0) == -1);
	ATF_REQUIRE_EQ(0, close(copy));
	ATF_REQUIRE_EQ(0, close(mntfd));

	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_rights);
	ATF_TP_ADD_TC(tp, nodup_cap_rights);
	ATF_TP_ADD_TC(tp, resolve_beneath);
	ATF_TP_ADD_TC(tp, nodup_resolve_beneath);
	ATF_TP_ADD_TC(tp, ioctl_caps);
	ATF_TP_ADD_TC(tp, nodup_ioctl_caps);
	ATF_TP_ADD_TC(tp, nodup_ioctl_cap_intersection);
	return (atf_no_error());
}
