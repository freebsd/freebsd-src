/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cp.c	5.26 (Berkeley) 10/27/91";
#endif /* not lint */

/*
 * cp copies source files to target files.
 * 
 * The global PATH_T structures "to" and "from" always contain paths to the
 * current source and target files, respectively.  Since cp does not change
 * directories, these paths can be either absolute or dot-realative.
 * 
 * The basic algorithm is to initialize "to" and "from", and then call the
 * recursive copy() function to do the actual work.  If "from" is a file,
 * copy copies the data.  If "from" is a directory, copy creates the
 * corresponding "to" directory, and calls itself recursively on all of
 * the entries in the "from" directory.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extern.h"

static void copy __P((void));
static void copy_dir __P((void));
static void copy_fifo __P((struct stat *, int));
static void copy_file __P((struct stat *, int));
static void copy_link __P((int));
static void copy_special __P((struct stat *, int));
static void setfile __P((struct stat *, int));
static void usage __P((void));

PATH_T from = { from.p_path, "" };
PATH_T to = { to.p_path, "" };

uid_t myuid;
int exit_val, myumask;
int iflag, pflag, orflag, rflag;
int (*statfcn)();
char *progname;

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	struct stat to_stat;
	register int c, r;
	int symfollow, lstat(), stat();
	char *old_to, *p;

	/*
	 * The utility cp(1) is used by mv(1) -- except for usage statements,
	 * print the "called as" program name.
	 */
	progname = (p = rindex(*argv,'/')) ? ++p : *argv;

	symfollow = 0;
	while ((c = getopt(argc, argv, "Rfhipr")) != EOF) {
	switch ((char)c) {
		case 'f':
			iflag = 0;
			break;
		case 'h':
			symfollow = 1;
			break;
		case 'i':
			iflag = isatty(fileno(stdin));
			break;
		case 'p':
			pflag = 1;
			break;
		case 'R':
			rflag = 1;
			break;
		case 'r':
			orflag = 1;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	if (rflag && orflag) {
		(void)fprintf(stderr,
		    "cp: the -R and -r options are mutually exclusive.\n");
		exit(1);
	}

	myuid = getuid();

	/* copy the umask for explicit mode setting */
	myumask = umask(0);
	(void)umask(myumask);

	/* consume last argument first. */
	if (!path_set(&to, argv[--argc]))
		exit(1);

	statfcn = symfollow || !rflag ? stat : lstat;

	/*
	 * Cp has two distinct cases:
	 *
	 * % cp [-rip] source target
	 * % cp [-rip] source1 ... directory
	 *
	 * In both cases, source can be either a file or a directory.
	 *
	 * In (1), the target becomes a copy of the source. That is, if the
	 * source is a file, the target will be a file, and likewise for
	 * directories.
	 *
	 * In (2), the real target is not directory, but "directory/source".
	 */

	r = stat(to.p_path, &to_stat);
	if (r == -1 && errno != ENOENT) {
		err("%s: %s", to.p_path, strerror(errno));
		exit(1);
	}
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		/*
		 * Case (1).  Target is not a directory.
		 */
		if (argc > 1) {
			usage();
			exit(1);
		}
		if (!path_set(&from, *argv))
			exit(1);
		copy();
	}
	else {
		/*
		 * Case (2).  Target is a directory.
		 */
		for (;; ++argv) {
			if (!path_set(&from, *argv))
				continue;
			if (!(old_to =
			    path_append(&to, path_basename(&from), -1)))
				continue;
			copy();
			if (!--argc)
				break;
			path_restore(&to, old_to);
		}
	}
	exit(exit_val);
}

/* copy file or directory at "from" to "to". */
static void
copy()
{
	struct stat from_stat, to_stat;
	int dne, statval;

	statval = statfcn(from.p_path, &from_stat);
	if (statval == -1) {
		err("%s: %s", from.p_path, strerror(errno));
		return;
	}

	/* not an error, but need to remember it happened */
	if (stat(to.p_path, &to_stat) == -1)
		dne = 1;
	else {
		if (to_stat.st_dev == from_stat.st_dev &&
		    to_stat.st_ino == from_stat.st_ino) {
			(void)fprintf(stderr,
			    "%s: %s and %s are identical (not copied).\n",
			    progname, to.p_path, from.p_path);
			exit_val = 1;
			return;
		}
		dne = 0;
	}

	switch(from_stat.st_mode & S_IFMT) {
	case S_IFLNK:
		copy_link(!dne);
		return;
	case S_IFDIR:
		if (!rflag && !orflag) {
			(void)fprintf(stderr,
			    "%s: %s is a directory (not copied).\n",
			    progname, from.p_path);
			exit_val = 1;
			return;
		}
		if (dne) {
			/*
			 * If the directory doesn't exist, create the new
			 * one with the from file mode plus owner RWX bits,
			 * modified by the umask.  Trade-off between being
			 * able to write the directory (if from directory is
			 * 555) and not causing a permissions race.  If the
			 * umask blocks owner writes cp fails.
			 */
			if (mkdir(to.p_path, from_stat.st_mode|S_IRWXU) < 0) {
				err("%s: %s", to.p_path, strerror(errno));
				return;
			}
		}
		else if (!S_ISDIR(to_stat.st_mode)) {
			(void)fprintf(stderr, "%s: %s: not a directory.\n",
			    progname, to.p_path);
			return;
		}
		copy_dir();
		/*
		 * If not -p and directory didn't exist, set it to be the
		 * same as the from directory, umodified by the umask;
		 * arguably wrong, but it's been that way forever.
		 */
		if (pflag)
			setfile(&from_stat, 0);
		else if (dne)
			(void)chmod(to.p_path, from_stat.st_mode);
		return;
	case S_IFCHR:
	case S_IFBLK:
		if (rflag) {
			copy_special(&from_stat, !dne);
			return;
		}
		break;
	case S_IFIFO:
		if (rflag) {
			copy_fifo(&from_stat, !dne);
			return;
		}
		break;
	}
	copy_file(&from_stat, dne);
}

static void
copy_file(fs, dne)
	struct stat *fs;
	int dne;
{
	static char buf[MAXBSIZE];
	register int from_fd, to_fd, rcount, wcount;
	struct stat to_stat;
	char *p;

	if ((from_fd = open(from.p_path, O_RDONLY, 0)) == -1) {
		err("%s: %s", from.p_path, strerror(errno));
		return;
	}

	/*
	 * If the file exists and we're interactive, verify with the user.
	 * If the file DNE, set the mode to be the from file, minus setuid
	 * bits, modified by the umask; arguably wrong, but it makes copying
	 * executables work right and it's been that way forever.  (The
	 * other choice is 666 or'ed with the execute bits on the from file
	 * modified by the umask.)
	 */
	if (!dne) {
		if (iflag) {
			int checkch, ch;

			(void)fprintf(stderr, "overwrite %s? ", to.p_path);
			checkch = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (checkch != 'y') {
				(void)close(from_fd);
				return;
			}
		}
		to_fd = open(to.p_path, O_WRONLY|O_TRUNC, 0);
	} else
		to_fd = open(to.p_path, O_WRONLY|O_CREAT|O_TRUNC,
		    fs->st_mode & ~(S_ISUID|S_ISGID));

	if (to_fd == -1) {
		err("%s: %s", to.p_path, strerror(errno));
		(void)close(from_fd);
		return;
	}

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
	if (fs->st_size <= 8 * 1048576) {
		if ((p = mmap(NULL, fs->st_size, PROT_READ,
		    MAP_FILE, from_fd, (off_t)0)) == (char *)-1)
			err("%s: %s", from.p_path, strerror(errno));
		madvise((caddr_t) p, fs->st_size, MADV_SEQUENTIAL);
		if (write(to_fd, p, fs->st_size) != fs->st_size)
			err("%s: %s", to.p_path, strerror(errno));
		munmap((caddr_t) p, fs->st_size);
	} else {
		while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
			wcount = write(to_fd, buf, rcount);
			if (rcount != wcount || wcount == -1) {
				err("%s: %s", to.p_path, strerror(errno));
				break;
			}
		}
		if (rcount < 0)
			err("%s: %s", from.p_path, strerror(errno));
	}
	if (pflag)
		setfile(fs, to_fd);
	/*
	 * If the source was setuid or setgid, lose the bits unless the
	 * copy is owned by the same user and group.
	 */
	else if (fs->st_mode & (S_ISUID|S_ISGID) && fs->st_uid == myuid)
		if (fstat(to_fd, &to_stat))
			err("%s: %s", to.p_path, strerror(errno));
#define	RETAINBITS	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
		else if (fs->st_gid == to_stat.st_gid && fchmod(to_fd,
		    fs->st_mode & RETAINBITS & ~myumask))
			err("%s: %s", to.p_path, strerror(errno));
	(void)close(from_fd);
	if (close(to_fd))
		err("%s: %s", to.p_path, strerror(errno));
}

static void
copy_dir()
{
	struct stat from_stat;
	struct dirent *dp, **dir_list;
	register int dir_cnt, i;
	char *old_from, *old_to;

	dir_cnt = scandir(from.p_path, &dir_list, NULL, NULL);
	if (dir_cnt == -1) {
		(void)fprintf(stderr, "%s: can't read directory %s.\n",
		    progname, from.p_path);
		exit_val = 1;
	}

	/*
	 * Instead of handling directory entries in the order they appear
	 * on disk, do non-directory files before directory files.
	 * There are two reasons to do directories last.  The first is
	 * efficiency.  Files tend to be in the same cylinder group as
	 * their parent, whereas directories tend not to be.  Copying files
	 * all at once reduces seeking.  Second, deeply nested tree's
	 * could use up all the file descriptors if we didn't close one
	 * directory before recursivly starting on the next.
	 */
	/* copy files */
	for (i = 0; i < dir_cnt; ++i) {
		dp = dir_list[i];
		if (dp->d_namlen <= 2 && dp->d_name[0] == '.'
		    && (dp->d_name[1] == NULL || dp->d_name[1] == '.'))
			goto done;
		if (!(old_from =
		    path_append(&from, dp->d_name, (int)dp->d_namlen)))
			goto done;

		if (statfcn(from.p_path, &from_stat) < 0) {
			err("%s: %s", dp->d_name, strerror(errno));
			path_restore(&from, old_from);
			goto done;
		}
		if (S_ISDIR(from_stat.st_mode)) {
			path_restore(&from, old_from);
			continue;
		}
		if (old_to = path_append(&to, dp->d_name, (int)dp->d_namlen)) {
			copy();
			path_restore(&to, old_to);
		}
		path_restore(&from, old_from);
done:		dir_list[i] = NULL;
		free(dp);
	}

	/* copy directories */
	for (i = 0; i < dir_cnt; ++i) {
		dp = dir_list[i];
		if (!dp)
			continue;
		if (!(old_from =
		    path_append(&from, dp->d_name, (int)dp->d_namlen))) {
			free(dp);
			continue;
		}
		if (!(old_to =
		    path_append(&to, dp->d_name, (int)dp->d_namlen))) {
			free(dp);
			path_restore(&from, old_from);
			continue;
		}
		copy();
		free(dp);
		path_restore(&from, old_from);
		path_restore(&to, old_to);
	}
	free(dir_list);
}

static void
copy_link(exists)
	int exists;
{
	int len;
	char link[MAXPATHLEN];

	if ((len = readlink(from.p_path, link, sizeof(link))) == -1) {
		err("readlink: %s: %s", from.p_path, strerror(errno));
		return;
	}
	link[len] = '\0';
	if (exists && unlink(to.p_path)) {
		err("unlink: %s: %s", to.p_path, strerror(errno));
		return;
	}
	if (symlink(link, to.p_path)) {
		err("symlink: %s: %s", link, strerror(errno));
		return;
	}
}

static void
copy_fifo(from_stat, exists)
	struct stat *from_stat;
	int exists;
{
	if (exists && unlink(to.p_path)) {
		err("unlink: %s: %s", to.p_path, strerror(errno));
		return;
	}
	if (mkfifo(to.p_path, from_stat->st_mode)) {
		err("mkfifo: %s: %s", to.p_path, strerror(errno));
		return;
	}
	if (pflag)
		setfile(from_stat, 0);
}

static void
copy_special(from_stat, exists)
	struct stat *from_stat;
	int exists;
{
	if (exists && unlink(to.p_path)) {
		err("unlink: %s: %s", to.p_path, strerror(errno));
		return;
	}
	if (mknod(to.p_path, from_stat->st_mode,  from_stat->st_rdev)) {
		err("mknod: %s: %s", to.p_path, strerror(errno));
		return;
	}
	if (pflag)
		setfile(from_stat, 0);
}

static void
setfile(fs, fd)
	register struct stat *fs;
	int fd;
{
	static struct timeval tv[2];

	fs->st_mode &= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;

	tv[0].tv_sec = fs->st_atime;
	tv[1].tv_sec = fs->st_mtime;
	if (utimes(to.p_path, tv))
		err("utimes: %s: %s", to.p_path, strerror(errno));
	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	if (fd ? fchown(fd, fs->st_uid, fs->st_gid) :
	    chown(to.p_path, fs->st_uid, fs->st_gid)) {
		if (errno != EPERM)
			err("chown: %s: %s", to.p_path, strerror(errno));
		fs->st_mode &= ~(S_ISUID|S_ISGID);
	}
	if (fd ? fchmod(fd, fs->st_mode) : chmod(to.p_path, fs->st_mode))
		err("chown: %s: %s", to.p_path, strerror(errno));
}

static void
usage()
{
	(void)fprintf(stderr,
"usage: cp [-Rfhip] src target;\n       cp [-Rfhip] src1 ... srcN directory\n");
	exit(1);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit_val = 1;
}
