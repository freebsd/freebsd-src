/*
 * Copyright (c) 2004-2005, 2007, 2010, 2012-2014
 *	Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// #include <config.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#include <fcntl.h>
#include <limits.h>
#ifdef HAVE_PSTAT_GETPROC
# include <sys/param.h>
# include <sys/pstat.h>
#else
# ifdef HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
# else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  ifdef HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  ifdef HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  ifdef HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
#endif

#ifndef OPEN_MAX
# define OPEN_MAX 256
#endif

#if defined(HAVE_FCNTL_CLOSEM) && !defined(HAVE_DIRFD)
# define closefrom	closefrom_fallback
#endif

static inline void
closefrom_close(int fd)
{
#ifdef __APPLE__
	/* Avoid potential libdispatch crash when we close its fds. */
	(void)fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
	(void)close(fd);
#endif
}

/*
 * Close all file descriptors greater than or equal to lowfd.
 * This is the expensive (fallback) method.
 */
void
closefrom_fallback(int lowfd)
{
	long fd, maxfd;

	/*
	 * Fall back on sysconf() or getdtablesize().  We avoid checking
	 * resource limits since it is possible to open a file descriptor
	 * and then drop the rlimit such that it is below the open fd.
	 */
#ifdef HAVE_SYSCONF
	maxfd = sysconf(_SC_OPEN_MAX);
#else
	maxfd = getdtablesize();
#endif /* HAVE_SYSCONF */
	if (maxfd < 0)
		maxfd = OPEN_MAX;

	for (fd = lowfd; fd < maxfd; fd++)
		closefrom_close(fd);
}

/*
 * Close all file descriptors greater than or equal to lowfd.
 * We try the fast way first, falling back on the slow method.
 */
#if defined(HAVE_FCNTL_CLOSEM)
void
closefrom(int lowfd)
{
	if (fcntl(lowfd, F_CLOSEM, 0) == -1)
		closefrom_fallback(lowfd);
}
#elif defined(HAVE_PSTAT_GETPROC)
void
closefrom(int lowfd)
{
	struct pst_status pstat;
	int fd;

	if (pstat_getproc(&pstat, sizeof(pstat), 0, getpid()) != -1) {
		for (fd = lowfd; fd <= pstat.pst_highestfd; fd++)
			(void)close(fd);
	} else {
		closefrom_fallback(lowfd);
	}
}
#elif defined(HAVE_DIRFD)
static int
closefrom_procfs(int lowfd)
{
	const char *path;
	DIR *dirp;
	struct dirent *dent;
	int *fd_array = NULL;
	int fd_array_used = 0;
	int fd_array_size = 0;
	int ret = 0;
	int i;

	/* Use /proc/self/fd (or /dev/fd on FreeBSD) if it exists. */
# if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__APPLE__)
	path = "/dev/fd";
# else
	path = "/proc/self/fd";
# endif
	dirp = opendir(path);
	if (dirp == NULL)
		return -1;

	while ((dent = readdir(dirp)) != NULL) {
		const char *errstr;
		int fd;

		fd = strtonum(dent->d_name, lowfd, INT_MAX, &errstr);
		if (errstr != NULL || fd == dirfd(dirp))
			continue;

		if (fd_array_used >= fd_array_size) {
			int *ptr;

			if (fd_array_size > 0)
				fd_array_size *= 2;
			else
				fd_array_size = 32;

			ptr = reallocarray(fd_array, fd_array_size, sizeof(int));
			if (ptr == NULL) {
				ret = -1;
				break;
			}
			fd_array = ptr;
		}

		fd_array[fd_array_used++] = fd;
	}

	for (i = 0; i < fd_array_used; i++)
		closefrom_close(fd_array[i]);

	free(fd_array);
	(void)closedir(dirp);

	return ret;
}

void
closefrom(int lowfd)
{
	if (closefrom_procfs(lowfd) == 0)
		return;

	closefrom_fallback(lowfd);
}
#endif /* HAVE_FCNTL_CLOSEM */
