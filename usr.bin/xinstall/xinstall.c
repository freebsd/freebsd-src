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
#if 0
static char sccsid[] = "From: @(#)xinstall.c	8.1 (Berkeley) 7/21/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>

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
#include <unistd.h>
#include <sysexits.h>
#include <utime.h>

#include "pathnames.h"

/* Bootstrap aid - this doesn't exist in most older releases */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)	/* from <sys/mman.h> */
#endif

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */
#define	NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)
#define	BACKUP_SUFFIX	".old"

struct passwd *pp;
struct group *gp;
gid_t gid;
uid_t uid;
int dobackup, docompare, dodir, dopreserve, dostrip, nommap, safecopy, verbose;
mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
char *suffix = BACKUP_SUFFIX;

void	copy __P((int, char *, int, char *, off_t));
int	compare __P((int, const char *, size_t, int, const char *, size_t));
int	create_newfile __P((char *, int, struct stat *));
int	create_tempfile __P((char *, char *, size_t));
void	install __P((char *, char *, u_long, u_int));
void	install_dir __P((char *));
u_long	numeric_id __P((char *, char *));
void	strip __P((char *));
int	trymmap __P((int));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat from_sb, to_sb;
	mode_t *set;
	u_long fset;
	int ch, no_target;
	u_int iflags;
	char *flags, *group, *owner, *to_name;

	iflags = 0;
	group = owner = NULL;
	while ((ch = getopt(argc, argv, "B:bCcdf:g:Mm:o:pSsv")) != -1)
		switch((char)ch) {
		case 'B':
			suffix = optarg;
			/* FALLTHROUGH */
		case 'b':
			dobackup = 1;
			break;
		case 'C':
			docompare = 1;
			break;
		case 'c':
			/* For backwards compatibility. */
			break;
		case 'd':
			dodir = 1;
			break;
		case 'f':
			flags = optarg;
			if (strtofflags(&flags, &fset, NULL))
				errx(EX_USAGE, "%s: invalid flag", flags);
			iflags |= SETFLAGS;
			break;
		case 'g':
			group = optarg;
			break;
		case 'M':
			nommap = 1;
			break;
		case 'm':
			if (!(set = setmode(optarg)))
				errx(EX_USAGE, "invalid file mode: %s",
				     optarg);
			mode = getmode(set, 0);
			free(set);
			break;
		case 'o':
			owner = optarg;
			break;
		case 'p':
			docompare = dopreserve = 1;
			break;
		case 'S':
			safecopy = 1;
			break;
		case 's':
			dostrip = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* some options make no sense when creating directories */
	if ((safecopy || dostrip) && dodir)
		usage();

	/*
	 * Older versions allowed -d -C combo.  Issue a warning
	 * for now, but turn this into an error before 4.5-RELEASE.
	 */
	if (docompare && dodir)
		warnx("the -d and -C options may not be specified together");

	/* must have at least two arguments, except when creating directories */
	if (argc < 2 && !dodir)
		usage();

	/* need to make a temp copy so we can compare stripped version */
	if (docompare && dostrip)
		safecopy = 1;

	/* get group and owner id's */
	if (group != NULL) {
		if ((gp = getgrnam(group)) != NULL)
			gid = gp->gr_gid;
		else
			gid = (uid_t)numeric_id(group, "group");
	} else
		gid = (gid_t)-1;

	if (owner != NULL) {
		if ((pp = getpwnam(owner)) != NULL)
			uid = pp->pw_uid;
		else
			uid = (uid_t)numeric_id(owner, "user");
	} else
		uid = (uid_t)-1;

	if (dodir) {
		for (; *argv != NULL; ++argv)
			install_dir(*argv);
		exit(EX_OK);
		/* NOTREACHED */
	}

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && S_ISDIR(to_sb.st_mode)) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, fset, iflags | DIRECTORY);
		exit(EX_OK);
		/* NOTREACHED */
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
	}
	install(*argv, to_name, fset, iflags);
	exit(EX_OK);
	/* NOTREACHED */
}

u_long
numeric_id(name, type)
	char *name, *type;
{
	u_long val;
	char *ep;

	/*
	 * XXX
	 * We know that uid_t's and gid_t's are unsigned longs.
	 */
	errno = 0;
	val = strtoul(name, &ep, 10);
	if (errno)
		err(EX_NOUSER, "%s", name);
	if (*ep != '\0')
		errx(EX_NOUSER, "unknown %s %s", type, name);
	return (val);
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
	struct stat from_sb, temp_sb, to_sb;
	struct utimbuf utb;
	int devnull, files_match, from_fd, serrno, target;
	int tempcopy, temp_fd, to_fd;
	char backup[MAXPATHLEN], *p, pathbuf[MAXPATHLEN], tempfile[MAXPATHLEN];

	files_match = 0;

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
		devnull = 1;
	}

	target = stat(to_name, &to_sb) == 0;

	/* Only install to regular files. */
	if (target && !S_ISREG(to_sb.st_mode)) {
		errno = EFTYPE;
		warn("%s", to_name);
		return;
	}

	/* Only copy safe if the target exists. */
	tempcopy = safecopy && target;

	if (!devnull && (from_fd = open(from_name, O_RDONLY, 0)) < 0)
		err(EX_OSERR, "%s", from_name);

	/* If we don't strip, we can compare first. */
	if (docompare && !dostrip && target) {
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);
		if (devnull)
			files_match = to_sb.st_size == 0;
		else
			files_match = !(compare(from_fd, from_name,
			    (size_t)from_sb.st_size, to_fd,
			    to_name, (size_t)to_sb.st_size));

		/* Close "to" file unless we match. */
		if (!files_match)
			(void)close(to_fd);
	}

	if (!files_match) {
		if (tempcopy) {
			to_fd = create_tempfile(to_name, tempfile,
			    sizeof(tempfile));
			if (to_fd < 0)
				err(EX_OSERR, "%s", tempfile);
		} else {
			if ((to_fd = create_newfile(to_name, target,
			    &to_sb)) < 0)
				err(EX_OSERR, "%s", to_name);
			if (verbose)
				(void)printf("install: %s -> %s\n",
				    from_name, to_name);
		}
		if (!devnull)
			copy(from_fd, from_name, to_fd,
			     tempcopy ? tempfile : to_name, from_sb.st_size);
	}

	if (dostrip) {
		strip(tempcopy ? tempfile : to_name);

		/*
		 * Re-open our fd on the target, in case we used a strip
		 * that does not work in-place -- like GNU binutils strip.
		 */
		close(to_fd);
		to_fd = open(tempcopy ? tempfile : to_name, O_RDONLY, 0);
		if (to_fd < 0)
			err(EX_OSERR, "stripping %s", to_name);
	}

	/*
	 * Compare the stripped temp file with the target.
	 */
	if (docompare && dostrip && target) {
		temp_fd = to_fd;

		/* Re-open to_fd using the real target name. */
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);

		if (fstat(temp_fd, &temp_sb)) {
			serrno = errno;
			(void)unlink(tempfile);
			errno = serrno;
			err(EX_OSERR, "%s", tempfile);
		}

		if (compare(temp_fd, tempfile, (size_t)temp_sb.st_size, to_fd,
			    to_name, (size_t)to_sb.st_size) == 0) {
			/*
			 * If target has more than one link we need to
			 * replace it in order to snap the extra links.
			 * Need to preserve target file times, though.
			 */
			if (to_sb.st_nlink != 1) {
				utb.actime = to_sb.st_atime;
				utb.modtime = to_sb.st_mtime;
				(void)utime(tempfile, &utb);
			} else {
				files_match = 1;
				(void)unlink(tempfile);
			}
			(void) close(temp_fd);
		}
	}

	/*
	 * Move the new file into place if doing a safe copy
	 * and the files are different (or just not compared).
	 */
	if (tempcopy && !files_match) {
		/* Try to turn off the immutable bits. */
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)chflags(to_name, to_sb.st_flags & ~NOCHANGEBITS);
		if (dobackup) {
			if (snprintf(backup, MAXPATHLEN, "%s%s", to_name,
			    suffix) != strlen(to_name) + strlen(suffix)) {
				unlink(tempfile);
				errx(EX_OSERR, "%s: backup filename too long",
				    to_name);
			}
			if (verbose)
				(void)printf("install: %s -> %s\n", to_name, backup);
			if (rename(to_name, backup) < 0) {
				serrno = errno;
				unlink(tempfile);
				errno = serrno;
				err(EX_OSERR, "rename: %s to %s", to_name,
				     backup);
			}
		}
		if (verbose)
			(void)printf("install: %s -> %s\n", from_name, to_name);
		if (rename(tempfile, to_name) < 0) {
			serrno = errno;
			unlink(tempfile);
			errno = serrno;
			err(EX_OSERR, "rename: %s to %s",
			    tempfile, to_name);
		}

		/* Re-open to_fd so we aren't hosed by the rename(2). */
		(void) close(to_fd);
		if ((to_fd = open(to_name, O_RDONLY, 0)) < 0)
			err(EX_OSERR, "%s", to_name);
	}

	/*
	 * Preserve the timestamp of the source file if necessary.
	 */
	if (dopreserve && !files_match && !devnull) {
		utb.actime = from_sb.st_atime;
		utb.modtime = from_sb.st_mtime;
		(void)utime(to_name, &utb);
	}

	if (fstat(to_fd, &to_sb) == -1) {
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_OSERR, "%s", to_name);
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if ((gid != (gid_t)-1 && gid != to_sb.st_gid) ||
	    (uid != (uid_t)-1 && uid != to_sb.st_uid) ||
	    (mode != to_sb.st_mode)) {
		/* Try to turn off the immutable bits. */
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)fchflags(to_fd, to_sb.st_flags & ~NOCHANGEBITS);
	}

	if ((gid != (gid_t)-1 && gid != to_sb.st_gid) ||
	    (uid != (uid_t)-1 && uid != to_sb.st_uid))
		if (fchown(to_fd, uid, gid) == -1) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR,"%s: chown/chgrp", to_name);
		}

	if (mode != to_sb.st_mode)
		if (fchmod(to_fd, mode)) {
			serrno = errno;
			(void)unlink(to_name);
			errno = serrno;
			err(EX_OSERR, "%s: chmod", to_name);
		}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump flag.
	 * NFS does not support flags.  Ignore EOPNOTSUPP flags if we're just
	 * trying to turn off UF_NODUMP.  If we're trying to set real flags,
	 * then warn if the the fs doesn't support it, otherwise fail.
	 */
	if (!devnull && fchflags(to_fd,
	    flags & SETFLAGS ? fset : from_sb.st_flags & ~UF_NODUMP)) {
		if (flags & SETFLAGS) {
			if (errno == EOPNOTSUPP)
				warn("%s: chflags", to_name);
			else {
				serrno = errno;
				(void)unlink(to_name);
				errno = serrno;
				err(EX_OSERR, "%s: chflags", to_name);
			}
		}
	}

	(void)close(to_fd);
	if (!devnull)
		(void)close(from_fd);
}

/*
 * compare --
 *	compare two files; non-zero means files differ
 */
int
compare(int from_fd, const char *from_name, size_t from_len,
	int to_fd, const char *to_name, size_t to_len)
{
	char *p, *q;
	int rv;
	int done_compare;

	rv = 0;
	if (from_len != to_len)
		return 1;

	if (from_len <= 8 * 1024 * 1024) {
		done_compare = 0;
		if (trymmap(from_fd) && trymmap(to_fd)) {
			p = mmap(NULL, from_len, PROT_READ, MAP_SHARED, from_fd, (off_t)0);
			if (p == (char *)MAP_FAILED)
				goto out;
			q = mmap(NULL, from_len, PROT_READ, MAP_SHARED, to_fd, (off_t)0);
			if (q == (char *)MAP_FAILED) {
				munmap(p, from_len);
				goto out;
			}

			rv = memcmp(p, q, from_len);
			munmap(p, from_len);
			munmap(q, from_len);
			done_compare = 1;
		}
	out:
		if (!done_compare) {
			char buf1[MAXBSIZE];
			char buf2[MAXBSIZE];
			int n1, n2;

			rv = 0;
			lseek(from_fd, 0, SEEK_SET);
			lseek(to_fd, 0, SEEK_SET);
			while (rv == 0) {
				n1 = read(from_fd, buf1, sizeof(buf1));
				if (n1 == 0)
					break;		/* EOF */
				else if (n1 > 0) {
					n2 = read(to_fd, buf2, n1);
					if (n2 == n1)
						rv = memcmp(buf1, buf2, n1);
					else
						rv = 1;	/* out of sync */
				} else
					rv = 1;		/* read failure */
			}
			lseek(from_fd, 0, SEEK_SET);
			lseek(to_fd, 0, SEEK_SET);
		}
	} else
		rv = 1;	/* don't bother in this case */

	return rv;
}

/*
 * create_tempfile --
 *	create a temporary file based on path and open it
 */
int
create_tempfile(path, temp, tsize)
	char *path;
	char *temp;
	size_t tsize;
{
	char *p;

	(void)strncpy(temp, path, tsize);
	temp[tsize - 1] = '\0';
	if ((p = strrchr(temp, '/')) != NULL)
		p++;
	else
		p = temp;
	(void)strncpy(p, "INS@XXXX", &temp[tsize - 1] - p);
	temp[tsize - 1] = '\0';
	return (mkstemp(temp));
}

/*
 * create_newfile --
 *	create a new file, overwriting an existing one if necessary
 */
int
create_newfile(path, target, sbp)
	char *path;
	int target;
	struct stat *sbp;
{
	char backup[MAXPATHLEN];

	if (target) {
		/*
		 * Unlink now... avoid ETXTBSY errors later.  Try to turn
		 * off the append/immutable bits -- if we fail, go ahead,
		 * it might work.
		 */
		if (sbp->st_flags & NOCHANGEBITS)
			(void)chflags(path, sbp->st_flags & ~NOCHANGEBITS);

		if (dobackup) {
			if (snprintf(backup, MAXPATHLEN, "%s%s",
			    path, suffix) != strlen(path) + strlen(suffix))
				errx(EX_OSERR, "%s: backup filename too long",
				    path);
			(void)snprintf(backup, MAXPATHLEN, "%s%s",
			    path, suffix);
			if (verbose)
				(void)printf("install: %s -> %s\n",
				    path, backup);
			if (rename(path, backup) < 0)
				err(EX_OSERR, "rename: %s to %s", path, backup);
		} else
			(void)unlink(path);
	}

	return (open(path, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR));
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
	int done_copy;

	/* Rewind file descriptors. */
	if (lseek(from_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", from_name);
	if (lseek(to_fd, (off_t)0, SEEK_SET) == (off_t)-1)
		err(EX_OSERR, "lseek: %s", to_name);

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
	done_copy = 0;
	if (size <= 8 * 1048576 && trymmap(from_fd) &&
	    (p = mmap(NULL, (size_t)size, PROT_READ, MAP_SHARED,
		    from_fd, (off_t)0)) != (char *)MAP_FAILED) {
		if ((nw = write(to_fd, p, size)) != size) {
			serrno = errno;
			(void)unlink(to_name);
			errno = nw > 0 ? EIO : serrno;
			err(EX_OSERR, "%s", to_name);
		}
		done_copy = 1;
	}
	if (!done_copy) {
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

	switch (fork()) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errno = serrno;
		err(EX_TEMPFAIL, "fork");
	case 0:
		execlp("strip", "strip", to_name, (char *)NULL);
		err(EX_OSERR, "exec(strip)");
	default:
		if (wait(&status) == -1 || status) {
			(void)unlink(to_name);
			exit(EX_SOFTWARE);
			/* NOTREACHED */
		}
	}
}

/*
 * install_dir --
 *	build directory heirarchy
 */
void
install_dir(path)
	char *path;
{
	register char *p;
	struct stat sb;
	int ch;

	for (p = path;; ++p)
		if (!*p || (p != path && *p  == '/')) {
			ch = *p;
			*p = '\0';
			if (stat(path, &sb)) {
				if (errno != ENOENT || mkdir(path, 0755) < 0) {
					err(EX_OSERR, "mkdir %s", path);
					/* NOTREACHED */
				} else if (verbose)
					(void)printf("install: mkdir %s\n",
						     path);
			} else if (!S_ISDIR(sb.st_mode))
				errx(EX_OSERR, "%s exists but is not a directory", path);
			if (!(*p = ch))
				break;
 		}

	if ((gid != (gid_t)-1 || uid != (uid_t)-1) && chown(path, uid, gid))
		warn("chown %u:%u %s", uid, gid, path);
	if (chmod(path, mode))
		warn("chmod %o %s", mode, path);
}

/*
 * usage --
 *	print a usage message and die
 */
void
usage()
{
	(void)fprintf(stderr, "\
usage: install [-bCcpSsv] [-B suffix] [-f flags] [-g group] [-m mode]\n\
               [-o owner] file1 file2\n\
       install [-bCcpSsv] [-B suffix] [-f flags] [-g group] [-m mode]\n\
               [-o owner] file1 ... fileN directory\n\
       install -d [-v] [-g group] [-m mode] [-o owner] directory ...\n");
	exit(EX_USAGE);
	/* NOTREACHED */
}

/*
 * trymmap --
 *	return true (1) if mmap should be tried, false (0) if not.
 */
int
trymmap(fd)
	int fd;
{
/*
 * The ifdef is for bootstrapping - f_fstypename doesn't exist in
 * pre-Lite2-merge systems.
 */
#ifdef MFSNAMELEN
	struct statfs stfs;

	if (nommap || fstatfs(fd, &stfs) != 0)
		return (0);
	if (strcmp(stfs.f_fstypename, "ufs") == 0 ||
	    strcmp(stfs.f_fstypename, "cd9660") == 0)
		return (1);
#endif
	return (0);
}
