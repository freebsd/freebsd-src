#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <mntopts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *const linrdlnk[] = { "linrdlnk", NULL };
static const char *const nodup_linrdlnk[] = { "nodup", "linrdlnk", NULL };

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
create_directory(const char *path, char marker)
{
	int dirfd, fd;

	ATF_REQUIRE_EQ(0, mkdir(path, 0755));
	ATF_REQUIRE((dirfd = open(path, O_RDONLY | O_DIRECTORY)) >= 0);
	ATF_REQUIRE(
	    (fd = openat(dirfd, "marker", O_WRONLY | O_CREAT, 0644)) >= 0);
	ATF_REQUIRE_EQ(1, write(fd, &marker, 1));
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, close(dirfd));
}

static void
check_identity(const struct stat *expected, const struct stat *actual)
{
	ATF_CHECK_EQ(expected->st_dev, actual->st_dev);
	ATF_CHECK_EQ(expected->st_ino, actual->st_ino);
	ATF_CHECK_EQ(expected->st_mode & S_IFMT, actual->st_mode & S_IFMT);
}

static void
check_directory(int dirfd, const char *suffix)
{
	struct stat expected, actual;
	char path[64];
	int fd;

	fdpath(path, sizeof(path), dirfd, suffix);
	ATF_REQUIRE_EQ(0, fstat(dirfd, &expected));
	ATF_REQUIRE_MSG(stat(path, &actual) == 0, "stat(%s): %s", path,
	    strerror(errno));
	check_identity(&expected, &actual);
	ATF_REQUIRE_MSG((fd = open(path, O_RDONLY | O_DIRECTORY)) >= 0,
	    "open(%s): %s", path, strerror(errno));
	ATF_REQUIRE_EQ(0, fstat(fd, &actual));
	check_identity(&expected, &actual);
	ATF_REQUIRE_EQ(0, close(fd));
}

static void
check_marker(int dirfd, char expected)
{
	char path[64], actual;
	int fd;

	fdpath(path, sizeof(path), dirfd, "/marker");
	ATF_REQUIRE_MSG((fd = open(path, O_RDONLY)) >= 0, "open(%s): %s", path,
	    strerror(errno));
	ATF_REQUIRE_EQ(1, read(fd, &actual, 1));
	ATF_CHECK_EQ(expected, actual);
	ATF_REQUIRE_EQ(0, close(fd));
}

static void
check_bare(int fd, const char *target)
{
	struct stat sb;
	char path[64], expected[PATH_MAX], actual[PATH_MAX];
	ssize_t len;
	int copy, flags, actual_flags;

	fdpath(path, sizeof(path), fd, "");
	ATF_REQUIRE_EQ(0, lstat(path, &sb));
	ATF_CHECK(S_ISLNK(sb.st_mode));
	ATF_REQUIRE(realpath(target, expected) != NULL);
	ATF_REQUIRE((len = readlink(path, actual, sizeof(actual))) >= 0);
	ATF_REQUIRE_EQ(strlen(expected), (size_t)len);
	ATF_CHECK_EQ(0, memcmp(expected, actual, len));
	ATF_REQUIRE((copy = open(path, O_RDONLY)) >= 0);
	ATF_REQUIRE((flags = fcntl(fd, F_GETFL)) >= 0);
	ATF_REQUIRE_EQ(0, fcntl(copy, F_SETFL, flags ^ O_NONBLOCK));
	ATF_REQUIRE((actual_flags = fcntl(fd, F_GETFL)) >= 0);
	ATF_CHECK_EQ((flags ^ O_NONBLOCK) & O_NONBLOCK,
	    actual_flags & O_NONBLOCK);
	ATF_REQUIRE_EQ(0, fcntl(fd, F_SETFL, flags));
	ATF_REQUIRE_EQ(0, close(copy));
}

static void
check_open_offset(bool duplicate)
{
	char path[64], buf[2], byte;
	int fd, copy;

	ATF_REQUIRE((fd = open("file", O_RDWR | O_CREAT, 0644)) >= 0);
	ATF_REQUIRE_EQ(6, write(fd, "abcdef", 6));
	ATF_REQUIRE_EQ(0, lseek(fd, 0, SEEK_SET));
	ATF_REQUIRE_EQ(2, read(fd, buf, sizeof(buf)));
	ATF_REQUIRE_EQ(0, memcmp(buf, "ab", sizeof(buf)));
	fdpath(path, sizeof(path), fd, "");
	ATF_REQUIRE((copy = open(path, O_RDONLY)) >= 0);
	ATF_CHECK_EQ(duplicate ? 2 : 0, lseek(copy, 0, SEEK_CUR));
	ATF_REQUIRE_EQ(1, read(copy, &byte, 1));
	ATF_CHECK_EQ(duplicate ? 'c' : 'a', byte);
	ATF_CHECK_EQ(duplicate ? 3 : 2, lseek(fd, 0, SEEK_CUR));
	ATF_REQUIRE_EQ(0, close(copy));
	ATF_REQUIRE_EQ(0, close(fd));
}

static void
check_notdir(int fd)
{
	const char *suffixes[] = { "/child", "/" };
	char path[64];
	size_t i;

	for (i = 0; i < nitems(suffixes); i++) {
		fdpath(path, sizeof(path), fd, suffixes[i]);
		ATF_REQUIRE_ERRNO(ENOTDIR, open(path, O_RDONLY) == -1);
	}
}

static void
check_traverse(int flags)
{
	struct stat expected, actual;
	char path[64];
	int fd;

	mount_fdescfs(linrdlnk);
	ATF_REQUIRE_EQ(0, mkdir("parent", 0755));
	create_directory("parent/target", 'a');
	ATF_REQUIRE((fd = open("parent/target", flags | O_DIRECTORY)) >= 0);
	fdpath(path, sizeof(path), fd, "/..");
	ATF_REQUIRE_EQ(0, stat("parent", &expected));
	ATF_REQUIRE_MSG(stat(path, &actual) == 0, "stat(%s): %s", path,
	    strerror(errno));
	check_identity(&expected, &actual);
	check_marker(fd, 'a');
	fdpath(path, sizeof(path), fd, "/child");
	ATF_REQUIRE_MSG(mkdir(path, 0755) == 0, "mkdir(%s): %s", path,
	    strerror(errno));
	ATF_REQUIRE_EQ(0, stat("parent/target/child", &expected));
	ATF_REQUIRE_EQ(0, stat(path, &actual));
	ATF_CHECK(S_ISDIR(actual.st_mode));
	check_identity(&expected, &actual);
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

FDESCFS_TC(traverse_dir, "Traverse a directory descriptor under linrdlnk");
ATF_TC_BODY(traverse_dir, tc)
{
	check_traverse(O_RDONLY);
}

FDESCFS_TC(traverse_path, "Traverse an O_PATH descriptor under linrdlnk");
ATF_TC_BODY(traverse_path, tc)
{
	check_traverse(O_PATH);
}

FDESCFS_TC(trailing_slash,
    "Trailing slashes resolve to the referenced directory");
ATF_TC_BODY(trailing_slash, tc)
{
	const int flags[] = { O_RDONLY, O_PATH };
	size_t i;
	int fd;

	mount_fdescfs(linrdlnk);
	create_directory("target", 'a');
	for (i = 0; i < nitems(flags); i++) {
		ATF_REQUIRE((fd = open("target", flags[i] | O_DIRECTORY)) >= 0);
		check_directory(fd, "/");
		ATF_REQUIRE_EQ(0, close(fd));
	}
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(last_component_dups,
    "Bare linrdlnk opens share the original offset");
ATF_TC_BODY(last_component_dups, tc)
{
	mount_fdescfs(linrdlnk);
	check_open_offset(true);
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(lookup_order,
    "Bare and traversing lookups do not affect each other");
ATF_TC_BODY(lookup_order, tc)
{
	int first, second;

	mount_fdescfs(linrdlnk);
	create_directory("target", 'a');
	ATF_REQUIRE((first = open("target", O_RDONLY | O_DIRECTORY)) >= 0);
	ATF_REQUIRE((second = open("target", O_RDONLY | O_DIRECTORY)) >= 0);
	check_bare(first, "target");
	check_marker(first, 'a');
	check_directory(first, "/");
	check_bare(first, "target");
	check_marker(second, 'a');
	check_directory(second, "/");
	check_bare(second, "target");
	check_marker(second, 'a');
	ATF_REQUIRE_EQ(0, close(second));
	ATF_REQUIRE_EQ(0, close(first));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(descriptor_reuse, "Reusing a descriptor selects its current target");
ATF_TC_BODY(descriptor_reuse, tc)
{
	int fd, replacement;

	mount_fdescfs(linrdlnk);
	create_directory("first", 'a');
	create_directory("second", 'b');
	ATF_REQUIRE((fd = open("first", O_RDONLY | O_DIRECTORY)) >= 0);
	check_bare(fd, "first");
	check_marker(fd, 'a');
	ATF_REQUIRE(
	    (replacement = open("second", O_RDONLY | O_DIRECTORY)) >= 0);
	ATF_REQUIRE_EQ(fd, dup2(replacement, fd));
	ATF_REQUIRE_EQ(0, close(replacement));
	check_marker(fd, 'b');
	check_directory(fd, "/");
	check_bare(fd, "second");
	ATF_REQUIRE((replacement = open("second/marker", O_RDONLY)) >= 0);
	ATF_REQUIRE_EQ(fd, dup2(replacement, fd));
	ATF_REQUIRE_EQ(0, close(replacement));
	check_notdir(fd);
	check_bare(fd, "second/marker");
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(traverse_nondir, "Files and pipes reject traversal");
ATF_TC_BODY(traverse_nondir, tc)
{
	int fd, pipes[2];

	mount_fdescfs(linrdlnk);
	ATF_REQUIRE((fd = open("file", O_RDONLY | O_CREAT, 0644)) >= 0);
	check_notdir(fd);
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, pipe(pipes));
	check_notdir(pipes[0]);
	ATF_REQUIRE_EQ(0, close(pipes[0]));
	ATF_REQUIRE_EQ(0, close(pipes[1]));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(plain_mount, "Plain mounts reject traversal");
ATF_TC_BODY(plain_mount, tc)
{
	int fd;

	mount_fdescfs(NULL);
	create_directory("target", 'a');
	ATF_REQUIRE((fd = open("target", O_RDONLY | O_DIRECTORY)) >= 0);
	check_notdir(fd);
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(nodup_linrdlnk_mount, "Linrdlnk preserves nodup behavior");
ATF_TC_BODY(nodup_linrdlnk_mount, tc)
{
	int fd;

	mount_fdescfs(nodup_linrdlnk);
	create_directory("target", 'a');
	ATF_REQUIRE((fd = open("target", O_RDONLY | O_DIRECTORY)) >= 0);
	check_marker(fd, 'a');
	check_directory(fd, "/");
	ATF_REQUIRE_EQ(0, close(fd));
	check_open_offset(false);
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

FDESCFS_TC(root_reference,
    "Traverse a descriptor referencing the fdescfs root");
ATF_TC_BODY(root_reference, tc)
{
	char suffix[32];
	int fd, len;

	mount_fdescfs(linrdlnk);
	ATF_REQUIRE((fd = open("mnt", O_RDONLY | O_DIRECTORY)) >= 0);
	check_directory(fd, "/");
	len = snprintf(suffix, sizeof(suffix), "/%d/", fd);
	ATF_REQUIRE(len > 0 && (size_t)len < sizeof(suffix));
	check_directory(fd, suffix);
	ATF_REQUIRE_EQ(0, close(fd));
	ATF_REQUIRE_EQ(0, unmount("mnt", 0));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, traverse_dir);
	ATF_TP_ADD_TC(tp, traverse_path);
	ATF_TP_ADD_TC(tp, trailing_slash);
	ATF_TP_ADD_TC(tp, last_component_dups);
	ATF_TP_ADD_TC(tp, lookup_order);
	ATF_TP_ADD_TC(tp, descriptor_reuse);
	ATF_TP_ADD_TC(tp, traverse_nondir);
	ATF_TP_ADD_TC(tp, plain_mount);
	ATF_TP_ADD_TC(tp, nodup_linrdlnk_mount);
	ATF_TP_ADD_TC(tp, root_reference);
	return (atf_no_error());
}
