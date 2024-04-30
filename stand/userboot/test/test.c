/*-
 * Copyright (c) 2011 Google, Inc.
 * Copyright (c) 2023-2024 Juniper Networks, Inc.
 * All rights reserved.
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

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <userboot.h>

char **vars;

char *host_base = NULL;
struct termios term, oldterm;
char *image;
size_t image_size;

uint64_t regs[16];
uint64_t pc;
int *disk_fd;
int disk_index = -1;

void test_exit(void *arg, int v);

/*
 * Console i/o
 */

void
test_putc(void *arg, int ch)
{
	char c = ch;

	write(1, &c, 1);
}

int
test_getc(void *arg)
{
	char c;

	if (read(0, &c, 1) == 1)
		return c;
	return -1;
}

int
test_poll(void *arg)
{
	int n;

	if (ioctl(0, FIONREAD, &n) >= 0)
		return (n > 0);
	return (0);
}

/*
 * Host filesystem i/o
 */

struct test_file {
	int tf_isdir;
	size_t tf_size;
	struct stat tf_stat;
	union {
		int fd;
		DIR *dir;
	} tf_u;
};

static int
test_open_internal(void *arg, const char *filename, void **h_return,
    struct test_file *tf, int depth)
{
	char path[PATH_MAX];
	char linkpath[PATH_MAX];
	char *component, *cp, *linkptr;
	ssize_t slen;
	int comp_fd, dir_fd, error;
	char c;
	bool openbase;

	if (depth++ >= MAXSYMLINKS)
		return (ELOOP);

	openbase = false;
	error = EINVAL;
	if (tf == NULL) {
		tf = calloc(1, sizeof(struct test_file));
		if (tf == NULL)
			return (error);
		openbase = true;
	} else if (tf->tf_isdir) {
		if (filename[0] == '/') {
			closedir(tf->tf_u.dir);
			openbase = true;
		}
	} else
		return (error);

	if (openbase) {
		dir_fd = open(host_base, O_RDONLY);
		if (dir_fd < 0)
			goto out;

		tf->tf_isdir = 1;
		tf->tf_u.dir = fdopendir(dir_fd);

		if (fstat(dir_fd, &tf->tf_stat) < 0) {
			error = errno;
			goto out;
		}
		tf->tf_size = tf->tf_stat.st_size;
	}

	strlcpy(path, filename, sizeof(path));
	cp = path;
	while (*cp) {
		/*
		 * The test file should be a directory at this point.
		 * If it is not, then the caller provided an invalid filename.
		 */
		if (!tf->tf_isdir)
			goto out;

		/* Trim leading slashes */
		while (*cp == '/')
			cp++;

		/* If we reached the end, we are done */
		if (*cp == '\0')
			break;

		/* Get the file descriptor for the directory */
		dir_fd = dirfd(tf->tf_u.dir);

		/* Get the next component path */
		component = cp;
		while ((c = *cp) != '\0' && c != '/')
			cp++;
		if (c == '/')
			*cp++ = '\0';

		/* Get status of the component */
		if (fstatat(dir_fd, component, &tf->tf_stat,
		    AT_SYMLINK_NOFOLLOW) < 0) {
			error = errno;
			goto out;
		}
		tf->tf_size = tf->tf_stat.st_size;

		/*
		 * Check that the path component is a directory, regular file,
		 * or a symlink.
		 */
		if (!S_ISDIR(tf->tf_stat.st_mode) &&
		    !S_ISREG(tf->tf_stat.st_mode) &&
		    !S_ISLNK(tf->tf_stat.st_mode))
			goto out;

		/* For anything that is not a symlink, open it */
		if (!S_ISLNK(tf->tf_stat.st_mode)) {
			comp_fd = openat(dir_fd, component, O_RDONLY);
			if (comp_fd < 0)
				goto out;
		}

		if (S_ISDIR(tf->tf_stat.st_mode)) {
			/* Directory */

			/* close the parent directory */
			closedir(tf->tf_u.dir);

			/* Open the directory from the component descriptor */
			tf->tf_isdir = 1;
			tf->tf_u.dir = fdopendir(comp_fd);
			if (!tf->tf_u.dir)
				goto out;
		} else if (S_ISREG(tf->tf_stat.st_mode)) {
			/* Regular file */

			/* close the parent directory */
			closedir(tf->tf_u.dir);

			/* Stash the component descriptor */
			tf->tf_isdir = 0;
			tf->tf_u.fd = comp_fd;
		} else if (S_ISLNK(tf->tf_stat.st_mode)) {
			/* Symlink */

			/* Read what the symlink points to */
			slen = readlinkat(dir_fd, component, linkpath,
			    sizeof(linkpath));
			if (slen < 0)
				goto out;
			/* NUL-terminate the string */
			linkpath[(size_t)slen] = '\0';

			/* Open the thing that the symlink points to */
			error = test_open_internal(arg, linkpath, NULL,
			    tf, depth);
			if (error != 0)
				goto out;
		}
	}

	/* Completed the entire path and have a good file/directory */
	if (h_return != NULL)
		*h_return = tf;
	return (0);

out:
	/* Failure of some sort, clean up */
	if (tf->tf_isdir)
		closedir(tf->tf_u.dir);
	else
		close(tf->tf_u.fd);
	free(tf);
	return (error);
}

int
test_open(void *arg, const char *filename, void **h_return)
{
	if (host_base == NULL)
		return (ENOENT);

	return (test_open_internal(arg, filename, h_return, NULL, 0));
}

int
test_close(void *arg, void *h)
{
	struct test_file *tf = h;

	if (tf->tf_isdir)
		closedir(tf->tf_u.dir);
	else
		close(tf->tf_u.fd);
	free(tf);

	return (0);
}

int
test_isdir(void *arg, void *h)
{
	struct test_file *tf = h;

	return (tf->tf_isdir);
}

int
test_read(void *arg, void *h, void *dst, size_t size, size_t *resid_return)
{
	struct test_file *tf = h;
	ssize_t sz;

	if (tf->tf_isdir)
		return (EINVAL);
	sz = read(tf->tf_u.fd, dst, size);
	if (sz < 0)
		return (EINVAL);
	*resid_return = size - sz;
	return (0);
}

int
test_readdir(void *arg, void *h, uint32_t *fileno_return, uint8_t *type_return,
    size_t *namelen_return, char *name)
{
	struct test_file *tf = h;
	struct dirent *dp;

	if (!tf->tf_isdir)
		return (EINVAL);

	dp = readdir(tf->tf_u.dir);
	if (!dp)
		return (ENOENT);

	/*
	 * Note: d_namlen is in the range 0..255 and therefore less
	 * than PATH_MAX so we don't need to test before copying.
	 */
	*fileno_return = dp->d_fileno;
	*type_return = dp->d_type;
	*namelen_return = dp->d_namlen;
	memcpy(name, dp->d_name, dp->d_namlen);
	name[dp->d_namlen] = 0;

	return (0);
}

int
test_seek(void *arg, void *h, uint64_t offset, int whence)
{
	struct test_file *tf = h;

	if (tf->tf_isdir)
		return (EINVAL);
	if (lseek(tf->tf_u.fd, offset, whence) < 0)
		return (errno);
	return (0);
}

int
test_stat(void *arg, void *h, struct stat *stp)
{
	struct test_file *tf = h;

	if (!stp)
		return (-1);
	memset(stp, 0, sizeof(struct stat));
	stp->st_mode = tf->tf_stat.st_mode;
	stp->st_uid = tf->tf_stat.st_uid;
	stp->st_gid = tf->tf_stat.st_gid;
	stp->st_size = tf->tf_stat.st_size;
	stp->st_ino = tf->tf_stat.st_ino;
	stp->st_dev = tf->tf_stat.st_dev;
	stp->st_mtime = tf->tf_stat.st_mtime;
	return (0);
}

/*
 * Disk image i/o
 */

int
test_diskread(void *arg, int unit, uint64_t offset, void *dst, size_t size,
    size_t *resid_return)
{
	ssize_t n;

	if (unit > disk_index || disk_fd[unit] == -1)
		return (EIO);
	n = pread(disk_fd[unit], dst, size, offset);
	if (n == 0) {
		printf("%s: end of disk (%ju)\n", __func__, (intmax_t)offset);
		return (EIO);
	}

	if (n < 0)
		return (errno);
	*resid_return = size - n;
	return (0);
}

int
test_diskwrite(void *arg, int unit, uint64_t offset, void *src, size_t size,
    size_t *resid_return)
{
	ssize_t n;

	if (unit > disk_index || disk_fd[unit] == -1)
		return (EIO);
	n = pwrite(disk_fd[unit], src, size, offset);
	if (n < 0)
		return (errno);
	*resid_return = size - n;
	return (0);
}

int
test_diskioctl(void *arg, int unit, u_long cmd, void *data)
{
	struct stat sb;

	if (unit > disk_index || disk_fd[unit] == -1)
		return (EBADF);
	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = 512;
		break;
	case DIOCGMEDIASIZE:
		if (fstat(disk_fd[unit], &sb) == 0)
			*(off_t *)data = sb.st_size;
		else
			return (ENOTTY);
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Guest virtual machine i/o
 *
 * Note: guest addresses are kernel virtual
 */

int
test_copyin(void *arg, const void *from, uint64_t to, size_t size)
{

	to &= 0x7fffffff;
	if (to > image_size)
		return (EFAULT);
	if (to + size > image_size)
		size = image_size - to;
	memcpy(&image[to], from, size);
	return(0);
}

int
test_copyout(void *arg, uint64_t from, void *to, size_t size)
{

	from &= 0x7fffffff;
	if (from > image_size)
		return (EFAULT);
	if (from + size > image_size)
		size = image_size - from;
	memcpy(to, &image[from], size);
	return(0);
}

void
test_setreg(void *arg, int r, uint64_t v)
{

	if (r < 0 || r >= 16)
		return;
	regs[r] = v;
}

void
test_setmsr(void *arg, int r, uint64_t v)
{
}

void
test_setcr(void *arg, int r, uint64_t v)
{
}

void
test_setgdt(void *arg, uint64_t v, size_t sz)
{
}

void
test_exec(void *arg, uint64_t pc)
{
	printf("Execute at 0x%"PRIx64"\n", pc);
	test_exit(arg, 0);
}

/*
 * Misc
 */

void
test_delay(void *arg, int usec)
{

	usleep(usec);
}

void
test_exit(void *arg, int v)
{

	tcsetattr(0, TCSAFLUSH, &oldterm);
	exit(v);
}

void
test_getmem(void *arg, uint64_t *lowmem, uint64_t *highmem)
{

        *lowmem = 128*1024*1024;
        *highmem = 0;
}

char *
test_getenv(void *arg, int idx)
{
	static char *myvars[] = {
		"USERBOOT=1"
	};
	static const int num_myvars = nitems(myvars);

	if (idx < num_myvars)
		return (myvars[idx]);
	else
		return (vars[idx - num_myvars]);
}

struct loader_callbacks cb = {
	.putc = test_putc,
	.getc = test_getc,
	.poll = test_poll,

	.open = test_open,
	.close = test_close,
	.isdir = test_isdir,
	.read = test_read,
	.readdir = test_readdir,
	.seek = test_seek,
	.stat = test_stat,

	.diskread = test_diskread,
	.diskwrite = test_diskwrite,
	.diskioctl = test_diskioctl,

	.copyin = test_copyin,
	.copyout = test_copyout,
	.setreg = test_setreg,
	.setmsr = test_setmsr,
	.setcr = test_setcr,
        .setgdt = test_setgdt,
	.exec = test_exec,

	.delay = test_delay,
	.exit = test_exit,
        .getmem = test_getmem,

	.getenv = test_getenv,
};

void
usage()
{

	printf("usage: [-b <userboot shared object>] [-d <disk image path>] [-h <host filesystem path>\n");
	exit(1);
}

int
main(int argc, char** argv, char ** environment)
{
	void *h;
	void (*func)(struct loader_callbacks *, void *, int, int) __dead2;
	int opt;
	const char *userboot_obj = "/boot/userboot.so";
	int oflag = O_RDONLY;

	vars = environment;

	while ((opt = getopt(argc, argv, "wb:d:h:")) != -1) {
		switch (opt) {
		case 'b':
			userboot_obj = optarg;
			break;

		case 'd':
			disk_index++;
			disk_fd = reallocarray(disk_fd, disk_index + 1,
			    sizeof (int));
			disk_fd[disk_index] = open(optarg, oflag);
			if (disk_fd[disk_index] < 0)
				err(1, "Can't open disk image '%s'", optarg);
			break;

		case 'h':
			host_base = optarg;
			break;

		case 'w':
			oflag = O_RDWR;
			break;

		case '?':
			usage();
		}
	}

	h = dlopen(userboot_obj, RTLD_LOCAL);
	if (!h) {
		printf("%s\n", dlerror());
		return (1);
	}
	func = dlsym(h, "loader_main");
	if (!func) {
		printf("%s\n", dlerror());
		return (1);
	}

	image_size = 128*1024*1024;
	image = malloc(image_size);

	tcgetattr(0, &term);
	oldterm = term;
	term.c_iflag &= ~(ICRNL);
	term.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(0, TCSAFLUSH, &term);

	func(&cb, NULL, USERBOOT_VERSION_3, disk_index + 1);
}
