/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "From: @(#)xinstall.c	8.1 (Berkeley) 7/21/93";*/
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

/*-
 * Todo:
 * o for -C, compare original files except in -s case.
 * o for -C, don't change anything if nothing needs be changed.  In
 *   particular, don't toggle the immutable flags just to allow null
 *   attribute changes and don't clear the dump flag.  (I think inode
 *   ctimes are not updated for null attribute changes, but this is a
 *   bug.)
 * o independent of -C, if a copy must be made, then copy to a tmpfile,
 *   set all attributes except the immutable flags, then rename, then
 *   set the immutable flags.  It's annoying that the immutable flags
 *   defeat the atomicicity of rename - it seems that there must be
 * o a window where the target is not immutable.
 */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <utime.h>

#include "pathnames.h"

struct passwd *pp;
struct group *gp;
int debug, docompare, docopy, dopreserve, dostrip;
int mode = S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
char *group, *owner, pathbuf[MAXPATHLEN];
char pathbuf2[MAXPATHLEN];

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */
#define	NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)

void	copy __P((int, char *, int, char *, off_t));
int	compare __P((int, const char *, int, const char *, 
		     const struct stat *, const struct stat *));
void	install __P((char *, char *, u_long, u_int));
u_long	string_to_flags __P((char **, u_long *, u_long *));
void	strip __P((char *));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat from_sb, to_sb;
	mode_t *set;
	u_long fset;
	u_int iflags;
	int ch, no_target;
	char *flags, *to_name;

	iflags = 0;
	while ((ch = getopt(argc, argv, "Ccdf:g:m:o:ps")) != EOF)
		switch((char)ch) {
		case 'C':
			docompare = docopy = 1;
			break;
		case 'c':
			docopy = 1;
			break;
		case 'd':
			debug++;
			break;
		case 'f':
			flags = optarg;
			if (string_to_flags(&flags, &fset, NULL))
				errx(EX_USAGE, "%s: invalid flag", flags);
			iflags |= SETFLAGS;
			break;
		case 'g':
			group = optarg;
			break;
		case 'm':
			if (!(set = setmode(optarg)))
				errx(EX_USAGE, "invalid file mode: %s",
				     optarg);
			mode = getmode(set, 0);
			break;
		case 'o':
			owner = optarg;
			break;
		case 'p':
			docompare = docopy = dopreserve = 1;
			break;
		case 's':
			dostrip = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();

	/* get group and owner id's */
	if (group && !(gp = getgrnam(group)))
		errx(EX_NOUSER, "unknown group %s", group);
	if (owner && !(pp = getpwnam(owner)))
		errx(EX_NOUSER, "unknown user %s", owner);

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && (to_sb.st_mode & S_IFMT) == S_IFDIR) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, fset, iflags | DIRECTORY);
		exit(0);
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2)
		usage();

	if (!no_target) {
		if (stat(*argv, &from_sb))
			err(EX_OSERR, "%s", *argv);
		if (!S_ISREG(to_sb.st_mode)) {
			errno = EFTYPE;
			err(EX_OSERR, "%s", to_name);
		}
		if (to_sb.st_dev == from_sb.st_dev &&
		    to_sb.st_ino == from_sb.st_ino)
			errx(EX_USAGE, 
			    "%s and %s are the same file", *argv, to_name);
/*
 * XXX - It's not at all clear why this code was here, since it completely
 * duplicates code install().  The version in install() handles the -C flag
 * correctly, so we'll just disable this for now.
 */
#if 0
		/*
		 * Unlink now... avoid ETXTBSY errors later.  Try and turn
		 * off the append/immutable bits -- if we fail, go ahead,
		 * it might work.
		 */
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)chflags(to_name,
			    to_sb.st_flags & ~(NOCHANGEBITS));
		(void)unlink(to_name);
#endif
	}
	install(*argv, to_name, fset, iflags);
	exit(0);
}

/*
 * install --
 *	build a path name and install the file
 */
void
install(from_name, to_name, fset, flags)
	char *from_name, *to_name;
	u_long fset;
	u_int flags;
{
	struct stat from_sb, to_sb;
	int devnull, from_fd, to_fd, serrno;
	char *p, *old_to_name = 0;

	if (debug >= 2 && !docompare)
		fprintf(stderr, "install: invoked without -C for %s to %s\n",
			from_name, to_name);

	/* If try to install NULL file to a directory, fails. */
	if (flags & DIRECTORY || strcmp(from_name, _PATH_DEVNULL)) {
		if (stat(from_name, &from_sb))
			err(EX_OSERR, "%s", from_name);
		if (!S_ISREG(from_sb.st_mode)) {
			errno = EFTYPE;
			err(EX_OSERR, "%s", from_name);
		}
		/* Build the target path. */
		if (flags & DIRECTORY) {
			(void)snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
			    to_name,
			    (p = strrchr(from_name, '/')) ? ++p : from_name);
			to_name = pathbuf;
		}
		devnull = 0;
	} else {
		from_sb.st_flags = 0;	/* XXX */
		devnull = 1;
	}

	if (docompare) {
		old_to_name = to_name;
		/*
		 * Make a new temporary file in the same file system
		 * (actually, in in the same directory) as the target so
		 * that the temporary file can be renamed to the target.
		 */
		snprintf(pathbuf2, sizeof pathbuf2, "%s", to_name);
		p = strrchr(pathbuf2, '/');
		p = (p == NULL ? pathbuf2 : p + 1);
		snprintf(p, &pathbuf2[sizeof pathbuf2] - p, "INS@XXXX");
		to_fd = mkstemp(pathbuf2);
		if (to_fd < 0)
			/* XXX should fall back to not comparing. */
			err(EX_OSERR, "mkstemp: %s for %s", pathbuf2, to_name);
		to_name = pathbuf2;
	} else {
		/*
		 * Unlink now... avoid errors later.  Try to turn off the
		 * append/immutable bits -- if we fail, go ahead, it might
		 * work.
		 */
		if (stat(to_name, &to_sb) == 0 && to_sb.st_flags & NOCHANGEBITS)
			(void)chflags(to_name, to_sb.st_flags & ~NOCHANGEBITS);
		unlink(to_name);

		/* Create target. */
		to_fd = open(to_name,
			     O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
		if (to_fd < 0)
			err(EX_OSERR, "%s", to_name);
	}

	if (!devnull) {
		if ((from_fd = open(from_name, O_RDONLY, 0)) < 0) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR, "%s", from_name);
		}
		copy(from_fd, from_name, to_fd, to_name, from_sb.st_size);
		(void)close(from_fd);
	}

	if (dostrip)
		strip(to_name);

	/*
	 * Unfortunately, because we strip the installed file and not the
	 * original one, it is impossible to do the comparison without
	 * first laboriously copying things over and then comparing.
	 * It may be possible to better optimize the !dostrip case, however.
	 * For further study.
	 */
	if (docompare) {
		struct stat old_sb, new_sb, timestamp_sb;
		int old_fd;
		struct utimbuf utb;

		old_fd = open(old_to_name, O_RDONLY, 0);
		if (old_fd < 0 && errno == ENOENT)
			goto different;
		if (old_fd < 0)
			err(EX_OSERR, "%s", old_to_name);
		fstat(old_fd, &old_sb);
		if (old_sb.st_flags & NOCHANGEBITS)
			(void)fchflags(old_fd, old_sb.st_flags & ~NOCHANGEBITS);
		fstat(to_fd, &new_sb);
		if (compare(old_fd, old_to_name, to_fd, to_name, &old_sb,
			    &new_sb)) {
different:
			if (debug != 0)
				fprintf(stderr,
					"install: renaming for %s: %s to %s\n",
					from_name, to_name, old_to_name);
			if (dopreserve && stat(from_name, &timestamp_sb) == 0) {
				utb.actime = from_sb.st_atime;
				utb.modtime = from_sb.st_mtime;
				(void)utime(to_name, &utb);
			}
moveit:
			if (rename(to_name, old_to_name) < 0) {
				serrno = errno;
				unlink(to_name);
				unlink(old_to_name);
				errno = serrno;
				err(EX_OSERR, "rename: %s to %s", to_name,
				    old_to_name);
			}
			close(old_fd);
		} else {
			if (old_sb.st_nlink != 1) {
				/*
				 * Replace the target, although it hasn't
				 * changed, to snap the extra links.  But
				 * preserve the target file times.
				 */
				if (fstat(old_fd, &timestamp_sb) == 0) {
					utb.actime = timestamp_sb.st_atime;
					utb.modtime = timestamp_sb.st_mtime;
					(void)utime(to_name, &utb);
				}
				goto moveit;
			}
			if (unlink(to_name) < 0)
				err(EX_OSERR, "unlink: %s", to_name);
			close(to_fd);
			to_fd = old_fd;
		}
		to_name = old_to_name;
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if ((group || owner) &&
	    fchown(to_fd, owner ? pp->pw_uid : -1, group ? gp->gr_gid : -1)) {
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_OSERR,"%s: chown/chgrp", to_name);
	}
	if (fchmod(to_fd, mode)) {
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_OSERR, "%s: chmod", to_name);
	}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump flag.
	 */
	if (fchflags(to_fd,
	    flags & SETFLAGS ? fset : from_sb.st_flags & ~UF_NODUMP)) {
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_OSERR, "%s: chflags", to_name);
	}

	(void)close(to_fd);
	if (!docopy && !devnull && unlink(from_name))
		err(EX_OSERR, "%s", from_name);
}

/*
 * compare --
 *	compare two files; non-zero means files differ
 */
int
compare(int from_fd, const char *from_name, int to_fd, const char *to_name,
	const struct stat *from_sb, const struct stat *to_sb)
{
	char *p, *q;
	int rv;
	size_t tsize;

	if (from_sb->st_size != to_sb->st_size)
		return 1;

	tsize = (size_t)from_sb->st_size;

	if (tsize <= 8 * 1024 * 1024) {
		p = mmap(NULL, tsize, PROT_READ, 0, from_fd, (off_t)0);
		if ((long)p == -1)
			err(EX_OSERR, "mmap %s", from_name);
		q = mmap(NULL, tsize, PROT_READ, 0, to_fd, (off_t)0);
		if ((long)q == -1)
			err(EX_OSERR, "mmap %s", to_name);

		rv = memcmp(p, q, tsize);
		munmap(p, tsize);
		munmap(q, tsize);
	} else {
		rv = 1;		/* don't bother in this case */
	}
	return rv;
}

/*
 * copy --
 *	copy from one file to another
 */
void
copy(from_fd, from_name, to_fd, to_name, size)
	register int from_fd, to_fd;
	char *from_name, *to_name;
	off_t size;
{
	register int nr, nw;
	int serrno;
	char *p, buf[MAXBSIZE];

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
	if (size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)size, PROT_READ,
		    0, from_fd, (off_t)0)) == (char *)-1)
			err(EX_OSERR, "mmap %s", from_name);
		if (write(to_fd, p, size) != size)
			err(EX_OSERR, "%s", to_name);
	} else {
		while ((nr = read(from_fd, buf, sizeof(buf))) > 0)
			if ((nw = write(to_fd, buf, nr)) != nr) {
				serrno = errno;
				(void)unlink(to_name);
				errno = nw > 0 ? EIO : serrno;
				err(EX_OSERR, "%s", to_name);
			}
		if (nr != 0) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR, "%s", from_name);
		}
	}
}

/*
 * strip --
 *	use strip(1) to strip the target file
 */
void
strip(to_name)
	char *to_name;
{
	int serrno, status;

	switch (vfork()) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_TEMPFAIL, "fork");
	case 0:
		execl(_PATH_STRIP, "strip", to_name, NULL);
		err(EX_OSERR, "exec(" _PATH_STRIP ")");
	default:
		if (wait(&status) == -1 || status)
			(void)unlink(to_name);
	}
}

/*
 * usage --
 *	print a usage message and die
 */
void
usage()
{
	(void)fprintf(stderr,
"usage: install [-Ccdps] [-f flags] [-g group] [-m mode] [-o owner] file1 file2;\n\tor file1 ... fileN directory\n");
	exit(1);
}
