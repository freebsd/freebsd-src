/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Klara, Inc.
 */

#include <sys/capsicum.h>
#include <sys/filio.h>
#include <sys/inotify.h>
#include <sys/ioccom.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/un.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mntopts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static const char *
ev2name(int event)
{
	switch (event) {
	case IN_ACCESS:
		return ("IN_ACCESS");
	case IN_ATTRIB:
		return ("IN_ATTRIB");
	case IN_CLOSE_WRITE:
		return ("IN_CLOSE_WRITE");
	case IN_CLOSE_NOWRITE:
		return ("IN_CLOSE_NOWRITE");
	case IN_CREATE:
		return ("IN_CREATE");
	case IN_DELETE:
		return ("IN_DELETE");
	case IN_DELETE_SELF:
		return ("IN_DELETE_SELF");
	case IN_MODIFY:
		return ("IN_MODIFY");
	case IN_MOVE_SELF:
		return ("IN_MOVE_SELF");
	case IN_MOVED_FROM:
		return ("IN_MOVED_FROM");
	case IN_MOVED_TO:
		return ("IN_MOVED_TO");
	case IN_OPEN:
		return ("IN_OPEN");
	default:
		return (NULL);
	}
}

static void
close_checked(int fd)
{
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Make sure that no other events are pending, and close the inotify descriptor.
 */
static void
close_inotify(int fd)
{
	int n;

	ATF_REQUIRE(ioctl(fd, FIONREAD, &n) == 0);
	ATF_REQUIRE(n == 0);
	close_checked(fd);
}

static uint32_t
consume_event_cookie(int ifd, int wd, unsigned int event, unsigned int flags,
    const char *name)
{
	struct inotify_event *ev;
	size_t evsz, namelen;
	ssize_t n;
	uint32_t cookie;

	/* Only read one record. */
	namelen = name == NULL ? 0 : strlen(name);
	evsz = sizeof(*ev) + _IN_NAMESIZE(namelen);
	ev = malloc(evsz);
	ATF_REQUIRE(ev != NULL);

	n = read(ifd, ev, evsz);
	ATF_REQUIRE_MSG(n >= 0, "failed to read event %s", ev2name(event));
	ATF_REQUIRE((size_t)n >= sizeof(*ev));
	ATF_REQUIRE((size_t)n == sizeof(*ev) + ev->len);
	ATF_REQUIRE((size_t)n == evsz);

	ATF_REQUIRE_MSG((ev->mask & IN_ALL_EVENTS) == event,
	    "expected event %#x, got %#x", event, ev->mask);
	ATF_REQUIRE_MSG((ev->mask & _IN_ALL_RETFLAGS) == flags,
	    "expected flags %#x, got %#x", flags, ev->mask);
	ATF_REQUIRE_MSG(ev->wd == wd,
	    "expected wd %d, got %d", wd, ev->wd);
	ATF_REQUIRE_MSG(name == NULL || strcmp(name, ev->name) == 0,
	    "expected name '%s', got '%s'", name, ev->name);
	cookie = ev->cookie;
	if ((ev->mask & (IN_MOVED_FROM | IN_MOVED_TO)) == 0)
		ATF_REQUIRE(cookie == 0);
	free(ev);
	return (cookie);
}

/*
 * Read an event from the inotify file descriptor and check that it
 * matches the expected values.
 */
static void
consume_event(int ifd, int wd, unsigned int event, unsigned int flags,
    const char *name)
{
	(void)consume_event_cookie(ifd, wd, event, flags, name);
}

static int
inotify(int flags)
{
	int ifd;

	ifd = inotify_init1(flags);
	ATF_REQUIRE(ifd != -1);
	return (ifd);
}

static void
mount_nullfs(char *dir, char *src)
{
	struct iovec *iov;
	char errmsg[1024];
	int error, iovlen;

	iov = NULL;
	iovlen = 0;

	build_iovec(&iov, &iovlen, "fstype", "nullfs", (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", dir, (size_t)-1);
	build_iovec(&iov, &iovlen, "target", src, (size_t)-1);
	build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));

	errmsg[0] = '\0';
	error = nmount(iov, iovlen, 0);
	ATF_REQUIRE_MSG(error == 0,
	    "mount nullfs %s %s: %s", src, dir,
	    errmsg[0] == '\0' ? strerror(errno) : errmsg);

	free_iovec(&iov, &iovlen);
}

static void
mount_tmpfs(const char *dir)
{
	struct iovec *iov;
	char errmsg[1024];
	int error, iovlen;

	iov = NULL;
	iovlen = 0;

	build_iovec(&iov, &iovlen, "fstype", "tmpfs", (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", __DECONST(char *, dir),
	    (size_t)-1);
	build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));

	errmsg[0] = '\0';
	error = nmount(iov, iovlen, 0);
	ATF_REQUIRE_MSG(error == 0,
	    "mount tmpfs %s: %s", dir,
	    errmsg[0] == '\0' ? strerror(errno) : errmsg);

	free_iovec(&iov, &iovlen);
}

static int
watch_file(int ifd, int events, char *path)
{
	int fd, wd;

	strncpy(path, "test.XXXXXX", PATH_MAX);
	fd = mkstemp(path);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);

	wd = inotify_add_watch(ifd, path, events);
	ATF_REQUIRE(wd != -1);

	return (wd);
}

static int
watch_dir(int ifd, int events, char *path)
{
	char *p;
	int wd;

	strlcpy(path, "test.XXXXXX", PATH_MAX);
	p = mkdtemp(path);
	ATF_REQUIRE(p == path);

	wd = inotify_add_watch(ifd, path, events);
	ATF_REQUIRE(wd != -1);

	return (wd);
}

/*
 * Verify that Capsicum restrictions are applied as expected.
 */
ATF_TC_WITHOUT_HEAD(inotify_capsicum);
ATF_TC_BODY(inotify_capsicum, tc)
{
	int error, dfd, ifd, wd;

	ifd = inotify(IN_NONBLOCK);
	ATF_REQUIRE(ifd != -1);

	dfd = open(".", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(dfd != -1);

	error = mkdirat(dfd, "testdir", 0755);
	ATF_REQUIRE(error == 0);

	error = cap_enter();
	ATF_REQUIRE(error == 0);

	/*
	 * Plain inotify_add_watch() is disallowed.
	 */
	wd = inotify_add_watch(ifd, ".", IN_DELETE_SELF);
	ATF_REQUIRE_ERRNO(ECAPMODE, wd == -1);
	wd = inotify_add_watch_at(ifd, dfd, "testdir", IN_DELETE_SELF);
	ATF_REQUIRE(wd >= 0);

	/*
	 * Generate a record and consume it.
	 */
	error = unlinkat(dfd, "testdir", AT_REMOVEDIR);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_DELETE_SELF, IN_ISDIR, NULL);
	consume_event(ifd, wd, 0, IN_IGNORED, NULL);

	close_checked(dfd);
	close_inotify(ifd);
}

/*
 * Make sure that duplicate, back-to-back events are coalesced.
 */
ATF_TC_WITHOUT_HEAD(inotify_coalesce);
ATF_TC_BODY(inotify_coalesce, tc)
{
	char file[PATH_MAX], path[PATH_MAX];
	int fd, fd1, ifd, n, wd;

	ifd = inotify(IN_NONBLOCK);

	/* Create a directory and watch it. */
	wd = watch_dir(ifd, IN_OPEN, path);
	/* Create a file in the directory and open it. */
	snprintf(file, sizeof(file), "%s/file", path);
	fd = open(file, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	fd = open(file, O_RDWR);
	ATF_REQUIRE(fd != -1);
	fd1 = open(file, O_RDONLY);
	ATF_REQUIRE(fd1 != -1);
	close_checked(fd1);
	close_checked(fd);

	consume_event(ifd, wd, IN_OPEN, 0, "file");
	ATF_REQUIRE(ioctl(ifd, FIONREAD, &n) == 0);
	ATF_REQUIRE(n == 0);

	close_inotify(ifd);
}

/*
 * Check handling of IN_MASK_CREATE.
 */
ATF_TC_WITHOUT_HEAD(inotify_mask_create);
ATF_TC_BODY(inotify_mask_create, tc)
{
	char path[PATH_MAX];
	int ifd, wd, wd1;

	ifd = inotify(IN_NONBLOCK);

	/* Create a directory and watch it. */
	wd = watch_dir(ifd, IN_CREATE, path);
	/* Updating the watch with IN_MASK_CREATE should result in an error. */
	wd1 = inotify_add_watch(ifd, path, IN_MODIFY | IN_MASK_CREATE);
	ATF_REQUIRE_ERRNO(EEXIST, wd1 == -1);
	/* It's an error to specify IN_MASK_ADD with IN_MASK_CREATE. */
	wd1 = inotify_add_watch(ifd, path, IN_MODIFY | IN_MASK_ADD |
	    IN_MASK_CREATE);
	ATF_REQUIRE_ERRNO(EINVAL, wd1 == -1);
	/* Updating the watch without IN_MASK_CREATE should work. */
	wd1 = inotify_add_watch(ifd, path, IN_MODIFY);
	ATF_REQUIRE(wd1 != -1);
	ATF_REQUIRE_EQ(wd, wd1);

	close_inotify(ifd);
}

/*
 * Make sure that inotify cooperates with nullfs: if a lower vnode is the
 * subject of an event, the upper vnode should be notified, and if the upper
 * vnode is the subject of an event, the lower vnode should be notified.
 */
ATF_TC_WITH_CLEANUP(inotify_nullfs);
ATF_TC_HEAD(inotify_nullfs, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(inotify_nullfs, tc)
{
	char path[PATH_MAX], *p;
	int dfd, error, fd, ifd, mask, wd;

	mask = IN_CREATE | IN_OPEN;

	ifd = inotify(IN_NONBLOCK);

	strlcpy(path, "./test.XXXXXX", sizeof(path));
	p = mkdtemp(path);
	ATF_REQUIRE(p == path);

	error = mkdir("./mnt", 0755);
	ATF_REQUIRE(error == 0);

	/* Mount the testdir onto ./mnt. */
	mount_nullfs("./mnt", path);

	wd = inotify_add_watch(ifd, "./mnt", mask);
	ATF_REQUIRE(wd != -1);

	/* Create a file in the lower directory and open it. */
	dfd = open(path, O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(dfd != -1);
	fd = openat(dfd, "file", O_RDWR | O_CREAT, 0644);
	close_checked(fd);
	close_checked(dfd);

	/* We should see events via the nullfs mount. */
	consume_event(ifd, wd, IN_OPEN, IN_ISDIR, NULL);
	consume_event(ifd, wd, IN_CREATE, 0, "file");
	consume_event(ifd, wd, IN_OPEN, 0, "file");

	error = inotify_rm_watch(ifd, wd);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, 0, IN_IGNORED, NULL);

	/* Watch the lower directory. */
	wd = inotify_add_watch(ifd, path, mask);
	ATF_REQUIRE(wd != -1);
	/* ... and create a file in the upper directory and open it. */
	dfd = open("./mnt", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(dfd != -1);
	fd = openat(dfd, "file2", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	close_checked(dfd);

	/* We should see events via the lower directory. */
	consume_event(ifd, wd, IN_OPEN, IN_ISDIR, NULL);
	consume_event(ifd, wd, IN_CREATE, 0, "file2");
	consume_event(ifd, wd, IN_OPEN, 0, "file2");

	close_inotify(ifd);
}
ATF_TC_CLEANUP(inotify_nullfs, tc)
{
	int error;

	error = unmount("./mnt", 0);
	if (error != 0) {
		perror("unmount");
		exit(1);
	}
}

/*
 * Make sure that exceeding max_events pending events results in an overflow
 * event.
 */
ATF_TC_WITHOUT_HEAD(inotify_queue_overflow);
ATF_TC_BODY(inotify_queue_overflow, tc)
{
	char path[PATH_MAX];
	size_t size;
	int error, dfd, ifd, max, wd;

	size = sizeof(max);
	error = sysctlbyname("vfs.inotify.max_queued_events", &max, &size, NULL,
	    0);
	ATF_REQUIRE(error == 0);

	ifd = inotify(IN_NONBLOCK);

	/* Create a directory and watch it for file creation events. */
	wd = watch_dir(ifd, IN_CREATE, path);
	dfd = open(path, O_DIRECTORY);
	ATF_REQUIRE(dfd != -1);
	/* Generate max+1 file creation events. */
	for (int i = 0; i < max + 1; i++) {
		char name[NAME_MAX];
		int fd;

		(void)snprintf(name, sizeof(name), "file%d", i);
		fd = openat(dfd, name, O_CREAT | O_RDWR, 0644);
		ATF_REQUIRE(fd != -1);
		close_checked(fd);
	}

	/*
	 * Read our events.  We should see files 0..max-1 and then an overflow
	 * event.
	 */
	for (int i = 0; i < max; i++) {
		char name[NAME_MAX];

		(void)snprintf(name, sizeof(name), "file%d", i);
		consume_event(ifd, wd, IN_CREATE, 0, name);
	}

	/* Look for an overflow event. */
	consume_event(ifd, -1, 0, IN_Q_OVERFLOW, NULL);

	close_checked(dfd);
	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_access_file);
ATF_TC_BODY(inotify_event_access_file, tc)
{
	char path[PATH_MAX], buf[16];
	off_t nb;
	ssize_t n;
	int error, fd, fd1, ifd, s[2], wd;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_file(ifd, IN_ACCESS, path);

	fd = open(path, O_RDWR);
	n = write(fd, "test", 4);
	ATF_REQUIRE(n == 4);

	/* A simple read(2) should generate an access. */
	ATF_REQUIRE(lseek(fd, 0, SEEK_SET) == 0);
	n = read(fd, buf, sizeof(buf));
	ATF_REQUIRE(n == 4);
	ATF_REQUIRE(memcmp(buf, "test", 4) == 0);
	consume_event(ifd, wd, IN_ACCESS, 0, NULL);

	/* copy_file_range(2) should as well. */
	ATF_REQUIRE(lseek(fd, 0, SEEK_SET) == 0);
	fd1 = open("sink", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd1 != -1);
	n = copy_file_range(fd, NULL, fd1, NULL, 4, 0);
	ATF_REQUIRE(n == 4);
	close_checked(fd1);
	consume_event(ifd, wd, IN_ACCESS, 0, NULL);

	/* As should sendfile(2). */
	error = socketpair(AF_UNIX, SOCK_STREAM, 0, s);
	ATF_REQUIRE(error == 0);
	error = sendfile(fd, s[0], 0, 4, NULL, &nb, 0);
	ATF_REQUIRE(error == 0);
	ATF_REQUIRE(nb == 4);
	consume_event(ifd, wd, IN_ACCESS, 0, NULL);
	close_checked(s[0]);
	close_checked(s[1]);

	close_checked(fd);

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_access_dir);
ATF_TC_BODY(inotify_event_access_dir, tc)
{
	char root[PATH_MAX], path[PATH_MAX];
	struct dirent *ent;
	DIR *dir;
	int error, ifd, wd;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_dir(ifd, IN_ACCESS, root);
	snprintf(path, sizeof(path), "%s/dir", root);
	error = mkdir(path, 0755);
	ATF_REQUIRE(error == 0);

	/* Read an entry and generate an access. */
	dir = opendir(path);
	ATF_REQUIRE(dir != NULL);
	ent = readdir(dir);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE(strcmp(ent->d_name, ".") == 0 ||
	    strcmp(ent->d_name, "..") == 0);
	ATF_REQUIRE(closedir(dir) == 0);
	consume_event(ifd, wd, IN_ACCESS, IN_ISDIR, "dir");

	/*
	 * Reading the watched directory should generate an access event.
	 * This is contrary to Linux's inotify man page, which states that
	 * IN_ACCESS is only generated for accesses to objects in a watched
	 * directory.
	 */
	dir = opendir(root);
	ATF_REQUIRE(dir != NULL);
	ent = readdir(dir);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE(strcmp(ent->d_name, ".") == 0 ||
	    strcmp(ent->d_name, "..") == 0);
	ATF_REQUIRE(closedir(dir) == 0);
	consume_event(ifd, wd, IN_ACCESS, IN_ISDIR, NULL);

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_attrib);
ATF_TC_BODY(inotify_event_attrib, tc)
{
	char path[PATH_MAX];
	int error, ifd, fd, wd;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_file(ifd, IN_ATTRIB, path);

	fd = open(path, O_RDWR);
	ATF_REQUIRE(fd != -1);
	error = fchmod(fd, 0600);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_ATTRIB, 0, NULL);

	error = fchown(fd, getuid(), getgid());
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_ATTRIB, 0, NULL);

	close_checked(fd);
	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_close_nowrite);
ATF_TC_BODY(inotify_event_close_nowrite, tc)
{
	char file[PATH_MAX], file1[PATH_MAX], dir[PATH_MAX];
	int ifd, fd, wd1, wd2;

	ifd = inotify(IN_NONBLOCK);

	wd1 = watch_dir(ifd, IN_CLOSE_NOWRITE, dir);
	wd2 = watch_file(ifd, IN_CLOSE_NOWRITE | IN_CLOSE_WRITE, file);

	fd = open(dir, O_DIRECTORY);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd1, IN_CLOSE_NOWRITE, IN_ISDIR, NULL);

	fd = open(file, O_RDONLY);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd2, IN_CLOSE_NOWRITE, 0, NULL);

	snprintf(file1, sizeof(file1), "%s/file", dir);
	fd = open(file1, O_RDONLY | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd1, IN_CLOSE_NOWRITE, 0, "file");

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_close_write);
ATF_TC_BODY(inotify_event_close_write, tc)
{
	char path[PATH_MAX];
	int ifd, fd, wd;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_file(ifd, IN_CLOSE_NOWRITE | IN_CLOSE_WRITE, path);

	fd = open(path, O_RDWR);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd, IN_CLOSE_WRITE, 0, NULL);

	close_inotify(ifd);
}

/* Verify that various operations in a directory generate IN_CREATE events. */
ATF_TC_WITHOUT_HEAD(inotify_event_create);
ATF_TC_BODY(inotify_event_create, tc)
{
	struct sockaddr_un sun;
	char path[PATH_MAX], path1[PATH_MAX], root[PATH_MAX];
	ssize_t n;
	int error, ifd, ifd1, fd, s, wd, wd1;
	char b;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_dir(ifd, IN_CREATE, root);

	/* Regular file. */
	snprintf(path, sizeof(path), "%s/file", root);
	fd = open(path, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	/*
	 * Make sure we get an event triggered by the fd used to create the
	 * file.
	 */
	ifd1 = inotify(IN_NONBLOCK);
	wd1 = inotify_add_watch(ifd1, root, IN_MODIFY);
	b = 42;
	n = write(fd, &b, sizeof(b));
	ATF_REQUIRE(n == sizeof(b));
	close_checked(fd);
	consume_event(ifd, wd, IN_CREATE, 0, "file");
	consume_event(ifd1, wd1, IN_MODIFY, 0, "file");
	close_inotify(ifd1);

	/* Hard link. */
	snprintf(path1, sizeof(path1), "%s/link", root);
	error = link(path, path1);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_CREATE, 0, "link");

	/* Directory. */
	snprintf(path, sizeof(path), "%s/dir", root);
	error = mkdir(path, 0755);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_CREATE, IN_ISDIR, "dir");

	/* Symbolic link. */
	snprintf(path1, sizeof(path1), "%s/symlink", root);
	error = symlink(path, path1);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_CREATE, 0, "symlink");

	/* FIFO. */
	snprintf(path, sizeof(path), "%s/fifo", root);
	error = mkfifo(path, 0644);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_CREATE, 0, "fifo");

	/* Binding a socket. */
	s = socket(AF_UNIX, SOCK_STREAM, 0);
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof(sun);
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s/socket", root);
	error = bind(s, (struct sockaddr *)&sun, sizeof(sun));
	ATF_REQUIRE(error == 0);
	close_checked(s);
	consume_event(ifd, wd, IN_CREATE, 0, "socket");

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_delete);
ATF_TC_BODY(inotify_event_delete, tc)
{
	char root[PATH_MAX], path[PATH_MAX], file[PATH_MAX];
	int error, fd, ifd, wd, wd2;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_dir(ifd, IN_DELETE | IN_DELETE_SELF, root);

	snprintf(path, sizeof(path), "%s/file", root);
	fd = open(path, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	error = unlink(path);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_DELETE, 0, "file");
	close_checked(fd);

	/*
	 * Make sure that renaming over a file generates a delete event when and
	 * only when that file is watched.
	 */
	fd = open(path, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	wd2 = inotify_add_watch(ifd, path, IN_DELETE | IN_DELETE_SELF);
	ATF_REQUIRE(wd2 != -1);
	snprintf(file, sizeof(file), "%s/file2", root);
	fd = open(file, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	error = rename(file, path);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd2, IN_DELETE_SELF, 0, NULL);
	consume_event(ifd, wd2, 0, IN_IGNORED, NULL);

	error = unlink(path);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_DELETE, 0, "file");
	error = rmdir(root);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd, IN_DELETE_SELF, IN_ISDIR, NULL);
	consume_event(ifd, wd, 0, IN_IGNORED, NULL);

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_move);
ATF_TC_BODY(inotify_event_move, tc)
{
	char dir1[PATH_MAX], dir2[PATH_MAX], path1[PATH_MAX], path2[PATH_MAX];
	char path3[PATH_MAX];
	int error, ifd, fd, wd1, wd2, wd3;
	uint32_t cookie1, cookie2;

	ifd = inotify(IN_NONBLOCK);

	wd1 = watch_dir(ifd, IN_MOVE | IN_MOVE_SELF, dir1);
	wd2 = watch_dir(ifd, IN_MOVE | IN_MOVE_SELF, dir2);

	snprintf(path1, sizeof(path1), "%s/file", dir1);
	fd = open(path1, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	snprintf(path2, sizeof(path2), "%s/file2", dir2);
	error = rename(path1, path2);
	ATF_REQUIRE(error == 0);
	cookie1 = consume_event_cookie(ifd, wd1, IN_MOVED_FROM, 0, "file");
	cookie2 = consume_event_cookie(ifd, wd2, IN_MOVED_TO, 0, "file2");
	ATF_REQUIRE_MSG(cookie1 == cookie2,
	    "expected cookie %u, got %u", cookie1, cookie2);

	snprintf(path2, sizeof(path2), "%s/dir", dir2);
	error = rename(dir1, path2);
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd1, IN_MOVE_SELF, IN_ISDIR, NULL);
	consume_event(ifd, wd2, IN_MOVED_TO, IN_ISDIR, "dir");

	wd3 = watch_file(ifd, IN_MOVE_SELF, path3);
	error = rename(path3, "foo");
	ATF_REQUIRE(error == 0);
	consume_event(ifd, wd3, IN_MOVE_SELF, 0, NULL);

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_move_dir);
ATF_TC_BODY(inotify_event_move_dir, tc)
{
	char dir[PATH_MAX], subdir1[PATH_MAX], subdir2[PATH_MAX];
	uint32_t cookie1, cookie2;
	int error, ifd, wd1, wd2;

	ifd = inotify(IN_NONBLOCK);

	wd1 = watch_dir(ifd, IN_MOVE, dir);
	snprintf(subdir1, sizeof(subdir1), "%s/subdir", dir);
	error = mkdir(subdir1, 0755);
	ATF_REQUIRE(error == 0);
	wd2 = inotify_add_watch(ifd, subdir1, IN_MOVE);
	ATF_REQUIRE(wd2 != -1);

	snprintf(subdir2, sizeof(subdir2), "%s/newsubdir", dir);
	error = rename(subdir1, subdir2);
	ATF_REQUIRE(error == 0);

	cookie1 = consume_event_cookie(ifd, wd1, IN_MOVED_FROM, IN_ISDIR,
	    "subdir");
	cookie2 = consume_event_cookie(ifd, wd1, IN_MOVED_TO, IN_ISDIR,
	    "newsubdir");
	ATF_REQUIRE_MSG(cookie1 == cookie2,
	    "expected cookie %u, got %u", cookie1, cookie2);

	close_inotify(ifd);
}

ATF_TC_WITHOUT_HEAD(inotify_event_open);
ATF_TC_BODY(inotify_event_open, tc)
{
	char root[PATH_MAX], path[PATH_MAX];
	int error, ifd, fd, wd;

	ifd = inotify(IN_NONBLOCK);

	wd = watch_dir(ifd, IN_OPEN, root);

	snprintf(path, sizeof(path), "%s/file", root);
	fd = open(path, O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd, IN_OPEN, 0, "file");

	fd = open(path, O_PATH);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd, IN_OPEN, 0, "file");

	fd = open(root, O_DIRECTORY);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd, IN_OPEN, IN_ISDIR, NULL);

	snprintf(path, sizeof(path), "%s/fifo", root);
	error = mkfifo(path, 0644);
	ATF_REQUIRE(error == 0);
	fd = open(path, O_RDWR);
	ATF_REQUIRE(fd != -1);
	close_checked(fd);
	consume_event(ifd, wd, IN_OPEN, 0, "fifo");

	close_inotify(ifd);
}

ATF_TC_WITH_CLEANUP(inotify_event_unmount);
ATF_TC_HEAD(inotify_event_unmount, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(inotify_event_unmount, tc)
{
	int error, fd, ifd, wd;

	ifd = inotify(IN_NONBLOCK);

	error = mkdir("./root", 0755);
	ATF_REQUIRE(error == 0);

	mount_tmpfs("./root");

	error = mkdir("./root/dir", 0755);
	ATF_REQUIRE(error == 0);
	wd = inotify_add_watch(ifd, "./root/dir", IN_OPEN);
	ATF_REQUIRE(wd >= 0);

	fd = open("./root/dir", O_RDONLY | O_DIRECTORY);
	ATF_REQUIRE(fd != -1);
	consume_event(ifd, wd, IN_OPEN, IN_ISDIR, NULL);
	close_checked(fd);

	/* A regular unmount should fail, as inotify holds a vnode reference. */
	error = unmount("./root", 0);
	ATF_REQUIRE_ERRNO(EBUSY, error == -1);
	error = unmount("./root", MNT_FORCE);
	ATF_REQUIRE_MSG(error == 0,
	    "unmounting ./root failed: %s", strerror(errno));

	consume_event(ifd, wd, 0, IN_UNMOUNT, NULL);
	consume_event(ifd, wd, 0, IN_IGNORED, NULL);

	close_inotify(ifd);
}
ATF_TC_CLEANUP(inotify_event_unmount, tc)
{
	(void)unmount("./root", MNT_FORCE);
}

ATF_TP_ADD_TCS(tp)
{
	/* Tests for the inotify syscalls. */
	ATF_TP_ADD_TC(tp, inotify_capsicum);
	ATF_TP_ADD_TC(tp, inotify_coalesce);
	ATF_TP_ADD_TC(tp, inotify_mask_create);
	ATF_TP_ADD_TC(tp, inotify_nullfs);
	ATF_TP_ADD_TC(tp, inotify_queue_overflow);
	/* Tests for the various inotify event types. */
	ATF_TP_ADD_TC(tp, inotify_event_access_file);
	ATF_TP_ADD_TC(tp, inotify_event_access_dir);
	ATF_TP_ADD_TC(tp, inotify_event_attrib);
	ATF_TP_ADD_TC(tp, inotify_event_close_nowrite);
	ATF_TP_ADD_TC(tp, inotify_event_close_write);
	ATF_TP_ADD_TC(tp, inotify_event_create);
	ATF_TP_ADD_TC(tp, inotify_event_delete);
	ATF_TP_ADD_TC(tp, inotify_event_move);
	ATF_TP_ADD_TC(tp, inotify_event_move_dir);
	ATF_TP_ADD_TC(tp, inotify_event_open);
	ATF_TP_ADD_TC(tp, inotify_event_unmount);
	return (atf_no_error());
}
