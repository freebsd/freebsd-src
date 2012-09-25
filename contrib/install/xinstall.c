/*	$NetBSD: xinstall.c,v 1.115 2011/09/06 18:50:32 joerg Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#else
#define HAVE_FUTIMES 1
#define HAVE_STRUCT_STAT_ST_FLAGS 1
#endif

#include <sys/cdefs.h>
#if defined(__COPYRIGHT) && !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1987, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#if defined(__RCSID) && !defined(lint)
#if 0
static char sccsid[] = "@(#)xinstall.c	8.1 (Berkeley) 7/21/93";
#else
__RCSID("$NetBSD: xinstall.c,v 1.115 2011/09/06 18:50:32 joerg Exp $");
#endif
#endif /* not lint */

#define __MKTEMP_OK__	/* All uses of mktemp have been checked */
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <vis.h>

#include <md5.h>
#include <rmd160.h>
#include <sha1.h>
#include <sha2.h>

#include "pathnames.h"
#include "mtree.h"

#define STRIP_ARGS_MAX 32
#define BACKUP_SUFFIX ".old"

static int	dobackup, dodir, dostrip, dolink, dopreserve, dorename, dounpriv;
static int	haveopt_f, haveopt_g, haveopt_m, haveopt_o;
static int	numberedbackup;
static int	mode = S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
static char	pathbuf[MAXPATHLEN];
static uid_t	uid = -1;
static gid_t	gid = -1;
static char	*group, *owner, *fflags, *tags;
static FILE	*metafp;
static char	*metafile;
static u_long	fileflags;
static char	*stripArgs;
static char	*afterinstallcmd;
static const char *suffix = BACKUP_SUFFIX;
static char	*destdir;

enum {
	DIGEST_NONE = 0,
	DIGEST_MD5,
	DIGEST_RMD160,
	DIGEST_SHA1,
	DIGEST_SHA256,
	DIGEST_SHA384,
	DIGEST_SHA512,
} digesttype = DIGEST_NONE;

static char	*digest;

#define LN_ABSOLUTE	0x01
#define LN_RELATIVE	0x02
#define LN_HARD		0x04
#define LN_SYMBOLIC	0x08
#define LN_MIXED	0x10

#define	DIRECTORY	0x01		/* Tell install it's a directory. */
#define	SETFLAGS	0x02		/* Tell install to set flags. */
#define	HASUID		0x04		/* Tell install the uid was given */
#define	HASGID		0x08		/* Tell install the gid was given */

static void	afterinstall(const char *, const char *, int);
static void	backup(const char *);
static char   *copy(int, char *, int, char *, off_t);
static int	do_link(char *, char *);
static void	do_symlink(char *, char *);
static void	install(char *, char *, u_int);
static void	install_dir(char *, u_int);
static void	makelink(char *, char *);
static void	metadata_log(const char *, const char *, struct timeval *,
	    const char *, const char *, off_t);
static int	parseid(char *, id_t *);
static void	strip(char *);
__dead static void	usage(void);
static char   *xbasename(char *);
static char   *xdirname(char *);

int
main(int argc, char *argv[])
{
	struct stat	from_sb, to_sb;
	void		*set;
	u_int		iflags;
	int		ch, no_target;
	char		*p, *to_name;

	setprogname(argv[0]);

	iflags = 0;
	while ((ch = getopt(argc, argv, "a:cbB:dD:f:g:h:l:m:M:N:o:prsS:T:U"))
	    != -1)
		switch((char)ch) {
		case 'a':
			afterinstallcmd = strdup(optarg);
			if (afterinstallcmd == NULL)
				errx(1, "%s", strerror(ENOMEM));
			break;
		case 'B':
			suffix = optarg;
			numberedbackup = 0;
			{
				/* Check if given suffix really generates
				   different suffixes - catch e.g. ".%" */
				char suffix_expanded0[FILENAME_MAX],
				     suffix_expanded1[FILENAME_MAX];
				(void)snprintf(suffix_expanded0, FILENAME_MAX,
					       suffix, 0);
				(void)snprintf(suffix_expanded1, FILENAME_MAX,
					       suffix, 1);
				if (strcmp(suffix_expanded0, suffix_expanded1)
				    != 0)
					numberedbackup = 1;
			}
			/* fall through; -B implies -b */
			/*FALLTHROUGH*/
		case 'b':
			dobackup = 1;
			break;
		case 'c':
			/* ignored; was "docopy" which is now the default. */
			break;
		case 'd':
			dodir = 1;
			break;
		case 'D':
			destdir = optarg;
			break;
#if ! HAVE_NBTOOL_CONFIG_H
		case 'f':
			haveopt_f = 1;
			fflags = optarg;
			break;
#endif
		case 'g':
			haveopt_g = 1;
			group = optarg;
			break;
		case 'h':
			digest = optarg;
			break;
		case 'l':
			for (p = optarg; *p; p++)
				switch (*p) {
				case 's':
					dolink &= ~(LN_HARD|LN_MIXED);
					dolink |= LN_SYMBOLIC;
					break;
				case 'h':
					dolink &= ~(LN_SYMBOLIC|LN_MIXED);
					dolink |= LN_HARD;
					break;
				case 'm':
					dolink &= ~(LN_SYMBOLIC|LN_HARD);
					dolink |= LN_MIXED;
					break;
				case 'a':
					dolink &= ~LN_RELATIVE;
					dolink |= LN_ABSOLUTE;
					break;
				case 'r':
					dolink &= ~LN_ABSOLUTE;
					dolink |= LN_RELATIVE;
					break;
				default:
					errx(1, "%c: invalid link type", *p);
					/* NOTREACHED */
				}
			break;
		case 'm':
			haveopt_m = 1;
			if (!(set = setmode(optarg)))
				err(1, "Cannot set file mode `%s'", optarg);
			mode = getmode(set, 0);
			free(set);
			break;
		case 'M':
			metafile = optarg;
			break;
		case 'N':
			if (! setup_getid(optarg))
				errx(1,
			    "Unable to use user and group databases in `%s'",
				    optarg);
			break;
		case 'o':
			haveopt_o = 1;
			owner = optarg;
			break;
		case 'p':
			dopreserve = 1;
			break;
		case 'r':
			dorename = 1;
			break;
		case 'S':
			stripArgs = strdup(optarg);
			if (stripArgs == NULL)
				errx(1, "%s", strerror(ENOMEM));
			/* fall through; -S implies -s */
			/*FALLTHROUGH*/
		case 's':
			dostrip = 1;
			break;
		case 'T':
			tags = optarg;
			break;
		case 'U':
			dounpriv = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* strip and link options make no sense when creating directories */
	if ((dostrip || dolink) && dodir)
		usage();

	/* strip and flags make no sense with links */
	if ((dostrip || fflags) && dolink)
		usage();

	/* must have at least two arguments, except when creating directories */
	if (argc < 2 && !dodir)
		usage();

	if (digest) {
		if (0) {
		} else if (strcmp(digest, "none") == 0) {
			digesttype = DIGEST_NONE;
		} else if (strcmp(digest, "md5") == 0) {
			digesttype = DIGEST_MD5;
		} else if (strcmp(digest, "rmd160") == 0) {
			digesttype = DIGEST_RMD160;
		} else if (strcmp(digest, "sha1") == 0) {
			digesttype = DIGEST_SHA1;
		} else if (strcmp(digest, "sha256") == 0) {
			digesttype = DIGEST_SHA256;
		} else if (strcmp(digest, "sha384") == 0) {
			digesttype = DIGEST_SHA384;
		} else if (strcmp(digest, "sha512") == 0) {
			digesttype = DIGEST_SHA512;
		} else {
			warnx("unknown digest `%s'", digest);
			usage();
		}
	}

	/* get group and owner id's */
	if (group && !dounpriv) {
		if (gid_from_group(group, &gid) == -1) {
			id_t id;
			if (!parseid(group, &id))
				errx(1, "unknown group %s", group);
			gid = id;
		}
		iflags |= HASGID;
	}
	if (owner && !dounpriv) {
		if (uid_from_user(owner, &uid) == -1) {
			id_t id;
			if (!parseid(owner, &id))
				errx(1, "unknown user %s", owner);
			uid = id;
		}
		iflags |= HASUID;
	}

#if ! HAVE_NBTOOL_CONFIG_H
	if (fflags && !dounpriv) {
		if (string_to_flags(&fflags, &fileflags, NULL))
			errx(1, "%s: invalid flag", fflags);
		/* restore fflags since string_to_flags() changed it */
		fflags = flags_to_string(fileflags, "-");
		iflags |= SETFLAGS;
	}
#endif

	if (metafile) {
		if ((metafp = fopen(metafile, "a")) == NULL)
			warn("open %s", metafile);
	} else
		digesttype = DIGEST_NONE;

	if (dodir) {
		for (; *argv != NULL; ++argv)
			install_dir(*argv, iflags);
		exit (0);
	}

	no_target = stat(to_name = argv[argc - 1], &to_sb);
	if (!no_target && S_ISDIR(to_sb.st_mode)) {
		for (; *argv != to_name; ++argv)
			install(*argv, to_name, iflags | DIRECTORY);
		exit(0);
	}

	/* can't do file1 file2 directory/file */
	if (argc != 2) {
		errx(EXIT_FAILURE, "the last argument (%s) "
		    "must name an existing directory", argv[argc - 1]);
		/* NOTREACHED */
	}

	if (!no_target) {
		/* makelink() handles checks for links */
		if (!dolink) {
			if (stat(*argv, &from_sb))
				err(1, "%s: stat", *argv);
			if (!S_ISREG(to_sb.st_mode))
				errx(1, "%s: not a regular file", to_name);
			if (to_sb.st_dev == from_sb.st_dev &&
			    to_sb.st_ino == from_sb.st_ino)
				errx(1, "%s and %s are the same file", *argv,
				    to_name);
		}
		/*
		 * Unlink now... avoid ETXTBSY errors later.  Try and turn
		 * off the append/immutable bits -- if we fail, go ahead,
		 * it might work.
		 */
#if ! HAVE_NBTOOL_CONFIG_H
#define	NOCHANGEBITS	(UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND)
		if (to_sb.st_flags & NOCHANGEBITS)
			(void)chflags(to_name,
			    to_sb.st_flags & ~(NOCHANGEBITS));
#endif
		if (dobackup)
			backup(to_name);
		else if (!dorename)
			(void)unlink(to_name);
	}
	install(*argv, to_name, iflags);
	exit(0);
}

/*
 * parseid --
 *	parse uid or gid from arg into id, returning non-zero if successful
 */
static int
parseid(char *name, id_t *id)
{
	char	*ep;

	errno = 0;
	*id = (id_t)strtoul(name, &ep, 10);
	if (errno || *ep != '\0')
		return (0);
	return (1);
}

/*
 * do_link --
 *	make a hard link, obeying dorename if set
 *	return -1 on failure
 */
static int
do_link(char *from_name, char *to_name)
{
	char tmpl[MAXPATHLEN];
	int ret;

	if (dorename) {
		(void)snprintf(tmpl, sizeof(tmpl), "%s.inst.XXXXXX", to_name);
		/* This usage is safe. */
		if (mktemp(tmpl) == NULL)
			err(1, "%s: mktemp", tmpl);
		ret = link(from_name, tmpl);
		if (ret == 0) {
			ret = rename(tmpl, to_name);
			/* If rename has posix semantics, then the temporary
			 * file may still exist when from_name and to_name point
			 * to the same file, so unlink it unconditionally.
			 */
			(void)unlink(tmpl);
		}
		return (ret);
	} else
		return (link(from_name, to_name));
}

/*
 * do_symlink --
 *	make a symbolic link, obeying dorename if set
 *	exit on failure
 */
static void
do_symlink(char *from_name, char *to_name)
{
	char tmpl[MAXPATHLEN];

	if (dorename) {
		(void)snprintf(tmpl, sizeof(tmpl), "%s.inst.XXXXXX", to_name);
		/* This usage is safe. */
		if (mktemp(tmpl) == NULL)
			err(1, "%s: mktemp", tmpl);

		if (symlink(from_name, tmpl) == -1)
			err(1, "symlink %s -> %s", from_name, tmpl);
		if (rename(tmpl, to_name) == -1) {
			/* remove temporary link before exiting */
			(void)unlink(tmpl);
			err(1, "%s: rename", to_name);
		}
	} else {
		if (symlink(from_name, to_name) == -1)
			err(1, "symlink %s -> %s", from_name, to_name);
	}
}

/*
 * makelink --
 *	make a link from source to destination
 */
static void
makelink(char *from_name, char *to_name)
{
	char	src[MAXPATHLEN], dst[MAXPATHLEN], lnk[MAXPATHLEN];
	struct stat	to_sb;

	/* Try hard links first */
	if (dolink & (LN_HARD|LN_MIXED)) {
		if (do_link(from_name, to_name) == -1) {
			if ((dolink & LN_HARD) || errno != EXDEV)
				err(1, "link %s -> %s", from_name, to_name);
		} else {
			if (stat(to_name, &to_sb))
				err(1, "%s: stat", to_name);
			if (S_ISREG(to_sb.st_mode)) {
					/* XXX: hard links to anything
					 * other than plain files are not
					 * metalogged
					 */
				int omode;
				char *oowner, *ogroup, *offlags;
				char *dres;

					/* XXX: use underlying perms,
					 * unless overridden on command line.
					 */
				omode = mode;
				if (!haveopt_m)
					mode = (to_sb.st_mode & 0777);
				oowner = owner;
				if (!haveopt_o)
					owner = NULL;
				ogroup = group;
				if (!haveopt_g)
					group = NULL;
				offlags = fflags;
				if (!haveopt_f)
					fflags = NULL;
				switch (digesttype) {
				case DIGEST_MD5:
					dres = MD5File(from_name, NULL);
					break;
				case DIGEST_RMD160:
					dres = RMD160File(from_name, NULL);
					break;
				case DIGEST_SHA1:
					dres = SHA1File(from_name, NULL);
					break;
				case DIGEST_SHA256:
					dres = SHA256_File(from_name, NULL);
					break;
				case DIGEST_SHA384:
					dres = SHA384_File(from_name, NULL);
					break;
				case DIGEST_SHA512:
					dres = SHA512_File(from_name, NULL);
					break;
				default:
					dres = NULL;
				}
				metadata_log(to_name, "file", NULL, NULL,
				    dres, to_sb.st_size);
				free(dres);
				mode = omode;
				owner = oowner;
				group = ogroup;
				fflags = offlags;
			}
			return;
		}
	}

	/* Symbolic links */
	if (dolink & LN_ABSOLUTE) {
		/* Convert source path to absolute */
		if (realpath(from_name, src) == NULL)
			err(1, "%s: realpath", from_name);
		do_symlink(src, to_name);
			/* XXX: src may point outside of destdir */
		metadata_log(to_name, "link", NULL, src, NULL, 0);
		return;
	}

	if (dolink & LN_RELATIVE) {
		char *cp, *d, *s;

		/* Resolve pathnames */
		if (realpath(from_name, src) == NULL)
			err(1, "%s: realpath", from_name);

		/*
		 * The last component of to_name may be a symlink,
		 * so use realpath to resolve only the directory.
		 */
		cp = xdirname(to_name);
		if (realpath(cp, dst) == NULL)
			err(1, "%s: realpath", cp);
		/* .. and add the last component */
		if (strcmp(dst, "/") != 0) {
			if (strlcat(dst, "/", sizeof(dst)) > sizeof(dst))
				errx(1, "resolved pathname too long");
		}
		cp = xbasename(to_name);
		if (strlcat(dst, cp, sizeof(dst)) > sizeof(dst))
			errx(1, "resolved pathname too long");

		/* trim common path components */
		for (s = src, d = dst; *s == *d; s++, d++)
			continue;
		while (*s != '/')
			s--, d--;

		/* count the number of directories we need to backtrack */
		for (++d, lnk[0] = '\0'; *d; d++)
			if (*d == '/')
				(void)strlcat(lnk, "../", sizeof(lnk));

		(void)strlcat(lnk, ++s, sizeof(lnk));

		do_symlink(lnk, to_name);
			/* XXX: lnk may point outside of destdir */
		metadata_log(to_name, "link", NULL, lnk, NULL, 0);
		return;
	}

	/*
	 * If absolute or relative was not specified, 
	 * try the names the user provided
	 */
	do_symlink(from_name, to_name);
		/* XXX: from_name may point outside of destdir */
	metadata_log(to_name, "link", NULL, from_name, NULL, 0);
}

/*
 * install --
 *	build a path name and install the file
 */
static void
install(char *from_name, char *to_name, u_int flags)
{
	struct stat	from_sb;
	struct stat	to_sb;
	struct timeval	tv[2];
	off_t		size;
	int		devnull, from_fd, to_fd, serrno, tmpmode;
	char		*p, tmpl[MAXPATHLEN], *oto_name, *digestresult;

	size = -1;
	if (!dolink) {
			/* ensure that from_sb & tv are sane if !dolink */
		if (stat(from_name, &from_sb))
			err(1, "%s: stat", from_name);
		size = from_sb.st_size;
#if BSD4_4 && !HAVE_NBTOOL_CONFIG_H
		TIMESPEC_TO_TIMEVAL(&tv[0], &from_sb.st_atimespec);
		TIMESPEC_TO_TIMEVAL(&tv[1], &from_sb.st_mtimespec);
#else
		tv[0].tv_sec = from_sb.st_atime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = from_sb.st_mtime;
		tv[1].tv_usec = 0;
#endif
	}

	if (flags & DIRECTORY || strcmp(from_name, _PATH_DEVNULL) != 0) {
		devnull = 0;
		if (!dolink) {
			if (!S_ISREG(from_sb.st_mode))
				errx(1, "%s: not a regular file", from_name);
		}
		/* Build the target path. */
		if (flags & DIRECTORY) {
			(void)snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
			    to_name,
			    (p = strrchr(from_name, '/')) ? ++p : from_name);
			to_name = pathbuf;
		}
	} else {
		devnull = 1;
		size = 0;
#if HAVE_STRUCT_STAT_ST_FLAGS
		from_sb.st_flags = 0;	/* XXX */
#endif
	}

	/*
	 * Unlink now... avoid ETXTBSY errors later.  Try and turn
	 * off the append/immutable bits -- if we fail, go ahead,
	 * it might work.
	 */
#if ! HAVE_NBTOOL_CONFIG_H
	if (stat(to_name, &to_sb) == 0 &&
	    to_sb.st_flags & (NOCHANGEBITS))
		(void)chflags(to_name, to_sb.st_flags & ~(NOCHANGEBITS));
#endif
	if (dorename) {
		(void)snprintf(tmpl, sizeof(tmpl), "%s.inst.XXXXXX", to_name);
		oto_name = to_name;
		to_name = tmpl;
	} else {
		oto_name = NULL;	/* pacify gcc */
		if (dobackup)
			backup(to_name);
		else
			(void)unlink(to_name);
	}

	if (dolink) {
		makelink(from_name, dorename ? oto_name : to_name);
		return;
	}

	/* Create target. */
	if (dorename) {
		if ((to_fd = mkstemp(to_name)) == -1)
			err(1, "%s: mkstemp", to_name);
	} else {
		if ((to_fd = open(to_name,
		    O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0)
			err(1, "%s: open", to_name);
	}
	digestresult = NULL;
	if (!devnull) {
		if ((from_fd = open(from_name, O_RDONLY, 0)) < 0) {
			(void)unlink(to_name);
			err(1, "%s: open", from_name);
		}
		digestresult =
		    copy(from_fd, from_name, to_fd, to_name, from_sb.st_size);
		(void)close(from_fd);
	}

	if (dostrip) {
		strip(to_name);

		/*
		 * Re-open our fd on the target, in case we used a strip
		 *  that does not work in-place -- like gnu binutils strip.
		 */
		close(to_fd);
		if ((to_fd = open(to_name, O_RDONLY, S_IRUSR | S_IWUSR)) < 0)
			err(1, "stripping %s", to_name);

		/*
		 * Recalculate size and digestresult after stripping.
		 */
		if (fstat(to_fd, &to_sb) != 0)
			err(1, "%s: fstat", to_name);
		size = to_sb.st_size;
		digestresult =
		    copy(to_fd, to_name, -1, NULL, size);

	}

	if (afterinstallcmd != NULL) {
		afterinstall(afterinstallcmd, to_name, 1);

		/*
		 * Re-open our fd on the target, in case we used an
		 * after-install command that does not work in-place
		 */
		close(to_fd);
		if ((to_fd = open(to_name, O_RDONLY, S_IRUSR | S_IWUSR)) < 0)
			err(1, "running after install command on %s", to_name);
	}

	/*
	 * Set owner, group, mode for target; do the chown first,
	 * chown may lose the setuid bits.
	 */
	if (!dounpriv &&
	    (flags & (HASUID | HASGID)) && fchown(to_fd, uid, gid) == -1) {
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "%s: chown/chgrp: %s", to_name, strerror(serrno));
	}
	tmpmode = mode;
	if (dounpriv)
		tmpmode &= S_IRWXU|S_IRWXG|S_IRWXO;
	if (fchmod(to_fd, tmpmode) == -1) {
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "%s: chmod: %s", to_name, strerror(serrno));
	}

	/*
	 * Preserve the date of the source file.
	 */
	if (dopreserve) {
#if HAVE_FUTIMES
		if (futimes(to_fd, tv) == -1)
			warn("%s: futimes", to_name);
#else
		if (utimes(to_name, tv) == -1)
			warn("%s: utimes", to_name);
#endif
	}

	(void)close(to_fd);

	if (dorename) {
		if (rename(to_name, oto_name) == -1)
			err(1, "%s: rename", to_name);
		to_name = oto_name;
	}

	/*
	 * If provided a set of flags, set them, otherwise, preserve the
	 * flags, except for the dump flag.
	 */
#if ! HAVE_NBTOOL_CONFIG_H
	if (!dounpriv && chflags(to_name,
	    flags & SETFLAGS ? fileflags : from_sb.st_flags & ~UF_NODUMP) == -1)
	{
		if (errno != EOPNOTSUPP || (from_sb.st_flags & ~UF_NODUMP) != 0)
			warn("%s: chflags", to_name);
	}
#endif

	metadata_log(to_name, "file", tv, NULL, digestresult, size);
	free(digestresult);
}

/*
 * copy --
 *	copy from one file to another, returning a digest.
 *
 *	If to_fd < 0, just calculate a digest, don't copy.
 */
static char *
copy(int from_fd, char *from_name, int to_fd, char *to_name, off_t size)
{
	ssize_t	nr, nw;
	int	serrno;
	u_char	*p;
	u_char	buf[MAXBSIZE];
	MD5_CTX		ctxMD5;
	RMD160_CTX	ctxRMD160;
	SHA1_CTX	ctxSHA1;
	SHA256_CTX	ctxSHA256;
	SHA384_CTX	ctxSHA384;
	SHA512_CTX	ctxSHA512;

	switch (digesttype) {
	case DIGEST_MD5:
		MD5Init(&ctxMD5);
		break;
	case DIGEST_RMD160:
		RMD160Init(&ctxRMD160);
		break;
	case DIGEST_SHA1:
		SHA1Init(&ctxSHA1);
		break;
	case DIGEST_SHA256:
		SHA256_Init(&ctxSHA256);
		break;
	case DIGEST_SHA384:
		SHA384_Init(&ctxSHA384);
		break;
	case DIGEST_SHA512:
		SHA512_Init(&ctxSHA512);
		break;
	case DIGEST_NONE:
		if (to_fd < 0)
			return NULL; /* no need to do anything */
	default:
		break;
	}
	/*
	 * There's no reason to do anything other than close the file
	 * now if it's empty, so let's not bother.
	 */
	if (size > 0) {

		/*
		 * Mmap and write if less than 8M (the limit is so we
		 * don't totally trash memory on big files).  This is
		 * really a minor hack, but it wins some CPU back.
		 */

		if (size <= 8 * 1048576) {
			if ((p = mmap(NULL, (size_t)size, PROT_READ,
			    MAP_FILE|MAP_SHARED, from_fd, (off_t)0))
			    == MAP_FAILED) {
				goto mmap_failed;
			}
#if defined(MADV_SEQUENTIAL) && !defined(__APPLE__)
			if (madvise(p, (size_t)size, MADV_SEQUENTIAL) == -1
			    && errno != EOPNOTSUPP)
				warnx("madvise: %s", strerror(errno));
#endif

			if (to_fd >= 0 && write(to_fd, p, size) != size) {
				serrno = errno;
				(void)unlink(to_name);
				errx(1, "%s: write: %s",
				    to_name, strerror(serrno));
			}
			switch (digesttype) {
			case DIGEST_MD5:
				MD5Update(&ctxMD5, p, size);
				break;
			case DIGEST_RMD160:
				RMD160Update(&ctxRMD160, p, size);
				break;
			case DIGEST_SHA1:
				SHA1Update(&ctxSHA1, p, size);
				break;
			case DIGEST_SHA256:
				SHA256_Update(&ctxSHA256, p, size);
				break;
			case DIGEST_SHA384:
				SHA384_Update(&ctxSHA384, p, size);
				break;
			case DIGEST_SHA512:
				SHA512_Update(&ctxSHA512, p, size);
				break;
			default:
				break;
			}
			(void)munmap(p, size);
		} else {
 mmap_failed:
			while ((nr = read(from_fd, buf, sizeof(buf))) > 0) {
				if (to_fd >= 0 &&
				    (nw = write(to_fd, buf, nr)) != nr) {
					serrno = errno;
					(void)unlink(to_name);
					errx(1, "%s: write: %s", to_name,
					    strerror(nw > 0 ? EIO : serrno));
				}
				switch (digesttype) {
				case DIGEST_MD5:
					MD5Update(&ctxMD5, buf, nr);
					break;
				case DIGEST_RMD160:
					RMD160Update(&ctxRMD160, buf, nr);
					break;
				case DIGEST_SHA1:
					SHA1Update(&ctxSHA1, buf, nr);
					break;
				case DIGEST_SHA256:
					SHA256_Update(&ctxSHA256, buf, nr);
					break;
				case DIGEST_SHA384:
					SHA384_Update(&ctxSHA384, buf, nr);
					break;
				case DIGEST_SHA512:
					SHA512_Update(&ctxSHA512, buf, nr);
					break;
				default:
					break;
				}
			}
			if (nr != 0) {
				serrno = errno;
				(void)unlink(to_name);
				errx(1, "%s: read: %s", from_name, strerror(serrno));
			}
		}
	}
	switch (digesttype) {
	case DIGEST_MD5:
		return MD5End(&ctxMD5, NULL);
	case DIGEST_RMD160:
		return RMD160End(&ctxRMD160, NULL);
	case DIGEST_SHA1:
		return SHA1End(&ctxSHA1, NULL);
	case DIGEST_SHA256:
		return SHA256_End(&ctxSHA256, NULL);
	case DIGEST_SHA384:
		return SHA384_End(&ctxSHA384, NULL);
	case DIGEST_SHA512:
		return SHA512_End(&ctxSHA512, NULL);
	default:
		return NULL;
	}
}

/*
 * strip --
 *	use strip(1) to strip the target file
 */
static void
strip(char *to_name)
{
	static const char exec_failure[] = ": exec of strip failed: ";
	int	serrno, status;
	const char * volatile stripprog, *progname;
	char *cmd;

	if ((stripprog = getenv("STRIP")) == NULL || *stripprog == '\0') {
#ifdef TARGET_STRIP
		stripprog = TARGET_STRIP;
#else
		stripprog = _PATH_STRIP;
#endif
	}

	cmd = NULL;

	if (stripArgs) {
		/*
		 * Build up a command line and let /bin/sh
		 * parse the arguments.
		 */
		int ret = asprintf(&cmd, "%s %s %s", stripprog, stripArgs,
		    to_name);

		if (ret == -1 || cmd == NULL)
			err(1, "asprintf failed");
	}

	switch (vfork()) {
	case -1:
		serrno = errno;
		(void)unlink(to_name);
		errx(1, "vfork: %s", strerror(serrno));
		/*NOTREACHED*/
	case 0:

		if (stripArgs)
			execl(_PATH_BSHELL, "sh", "-c", cmd, NULL);
		else
			execlp(stripprog, "strip", to_name, NULL);

		progname = getprogname();
		write(STDERR_FILENO, progname, strlen(progname));
		write(STDERR_FILENO, exec_failure, strlen(exec_failure));
		write(STDERR_FILENO, stripprog, strlen(stripprog));
		write(STDERR_FILENO, "\n", 1);
		_exit(1);
		/*NOTREACHED*/
	default:
		if (wait(&status) == -1 || status)
			(void)unlink(to_name);
	}

	free(cmd);
}

/*
 * afterinstall --
 *	run provided command on the target file or directory after it's been
 *	installed and stripped, but before permissions are set or it's renamed
 */
static void
afterinstall(const char *command, const char *to_name, int errunlink)
{
	int	serrno, status;
	char	*cmd;

	switch (vfork()) {
	case -1:
		serrno = errno;
		if (errunlink)
			(void)unlink(to_name);
		errx(1, "vfork: %s", strerror(serrno));
		/*NOTREACHED*/
	case 0:
		/*
		 * build up a command line and let /bin/sh
		 * parse the arguments
		 */
		cmd = (char*)malloc(sizeof(char)*
					  (2+strlen(command)+
					     strlen(to_name)));

		if (cmd == NULL)
			errx(1, "%s", strerror(ENOMEM));

		sprintf(cmd, "%s %s", command, to_name);

		execl(_PATH_BSHELL, "sh", "-c", cmd, NULL);

		warn("%s: exec of after install command", command);
		_exit(1);
		/*NOTREACHED*/
	default:
		if ((wait(&status) == -1 || status) && errunlink)
			(void)unlink(to_name);
	}
}

/*
 * backup --
 *	backup file "to_name" to to_name.suffix
 *	if suffix contains a "%", it's taken as a printf(3) pattern
 *	used for a numbered backup.
 */
static void
backup(const char *to_name)
{
	char	bname[FILENAME_MAX];
	
	if (numberedbackup) {
		/* Do numbered backup */
		int cnt;
		char suffix_expanded[FILENAME_MAX];
		
		cnt=0;
		do {
			(void)snprintf(suffix_expanded, FILENAME_MAX, suffix,
			    cnt);
			(void)snprintf(bname, FILENAME_MAX, "%s%s", to_name,
			    suffix_expanded);
			cnt++;
		} while (access(bname, F_OK) == 0); 
	} else {
		/* Do simple backup */
		(void)snprintf(bname, FILENAME_MAX, "%s%s", to_name, suffix);
	}
	
	(void)rename(to_name, bname);
}

/*
 * install_dir --
 *	build directory hierarchy
 */
static void
install_dir(char *path, u_int flags)
{
	char		*p;
	struct stat	sb;
	int		ch;

	for (p = path;; ++p)
		if (!*p || (p != path && *p  == '/')) {
			ch = *p;
			*p = '\0';
			if (mkdir(path, 0777) < 0) {
				/*
				 * Can't create; path exists or no perms.
				 * stat() path to determine what's there now.
				 */
				int sverrno;
				sverrno = errno;
				if (stat(path, &sb) < 0) {
					/* Not there; use mkdir()s error */
					errno = sverrno;
					err(1, "%s: mkdir", path);
				}
				if (!S_ISDIR(sb.st_mode)) {
					errx(1,
					    "%s exists but is not a directory",
					    path);
				}
			}
			if (!(*p = ch))
				break;
		}

	if (afterinstallcmd != NULL)
		afterinstall(afterinstallcmd, path, 0);

	if (!dounpriv && (
	    ((flags & (HASUID | HASGID)) && chown(path, uid, gid) == -1)
	    || chmod(path, mode) == -1 )) {
		warn("%s: chown/chmod", path);
	}
	metadata_log(path, "dir", NULL, NULL, NULL, 0);
}

/*
 * metadata_log --
 *	if metafp is not NULL, output mtree(8) full path name and settings to
 *	metafp, to allow permissions to be set correctly by other tools,
 *	or to allow integrity checks to be performed.
 */
static void
metadata_log(const char *path, const char *type, struct timeval *tv,
	const char *slink, const char *digestresult, off_t size)
{
	static const char	extra[] = { ' ', '\t', '\n', '\\', '#', '\0' };
	const char	*p;
	char		*buf;
	size_t		destlen;
	struct flock	metalog_lock;

	if (!metafp)	
		return;
	buf = (char *)malloc(4 * strlen(path) + 1);	/* buf for strsvis(3) */
	if (buf == NULL) {
		warnx("%s", strerror(ENOMEM));
		return;
	}
							/* lock log file */
	metalog_lock.l_start = 0;
	metalog_lock.l_len = 0;
	metalog_lock.l_whence = SEEK_SET;
	metalog_lock.l_type = F_WRLCK;
	if (fcntl(fileno(metafp), F_SETLKW, &metalog_lock) == -1) {
		warn("can't lock %s", metafile);
		free(buf);
		return;
	}

	p = path;					/* remove destdir */
	if (destdir) {
		destlen = strlen(destdir);
		if (strncmp(p, destdir, destlen) == 0 &&
		    (p[destlen] == '/' || p[destlen] == '\0'))
			p += destlen;
	}
	while (*p && *p == '/')				/* remove leading /s */
		p++;
	strsvis(buf, p, VIS_CSTYLE, extra);		/* encode name */
	p = buf;
							/* print details */
	fprintf(metafp, ".%s%s type=%s", *p ? "/" : "", p, type);
	if (owner)
		fprintf(metafp, " uname=%s", owner);
	if (group)
		fprintf(metafp, " gname=%s", group);
	fprintf(metafp, " mode=%#o", mode);
	if (slink) {
		strsvis(buf, slink, VIS_CSTYLE, extra);	/* encode link */
		fprintf(metafp, " link=%s", buf);
	}
	if (*type == 'f') /* type=file */
		fprintf(metafp, " size=%lld", (long long)size);
	if (tv != NULL && dopreserve)
		fprintf(metafp, " time=%lld.%ld",
			(long long)tv[1].tv_sec, (long)tv[1].tv_usec);
	if (digestresult && digest)
		fprintf(metafp, " %s=%s", digest, digestresult);
	if (fflags)
		fprintf(metafp, " flags=%s", fflags);
	if (tags)
		fprintf(metafp, " tags=%s", tags);
	fputc('\n', metafp);
	fflush(metafp);					/* flush output */
							/* unlock log file */
	metalog_lock.l_type = F_UNLCK;
	if (fcntl(fileno(metafp), F_SETLKW, &metalog_lock) == -1) {
		warn("can't unlock %s", metafile);
	}
	free(buf);
}

/*
 * xbasename --
 *	libc basename(3) that returns a pointer to a static buffer
 *	instead of overwriting that passed-in string.
 */
static char *
xbasename(char *path)
{
	static char tmp[MAXPATHLEN];

	(void)strlcpy(tmp, path, sizeof(tmp));
	return (basename(tmp));
}

/*
 * xdirname --
 *	libc dirname(3) that returns a pointer to a static buffer
 *	instead of overwriting that passed-in string.
 */
static char *
xdirname(char *path)
{
	static char tmp[MAXPATHLEN];

	(void)strlcpy(tmp, path, sizeof(tmp));
	return (dirname(tmp));
}

/*
 * usage --
 *	print a usage message and die
 */
static void
usage(void)
{
	const char *prog;

	prog = getprogname();

	(void)fprintf(stderr,
"usage: %s [-Ubcprs] [-M log] [-D dest] [-T tags] [-B suffix]\n"
"           [-a aftercmd] [-f flags] [-m mode] [-N dbdir] [-o owner] [-g group] \n"
"           [-l linkflags] [-h hash] [-S stripflags] file1 file2\n"
"       %s [-Ubcprs] [-M log] [-D dest] [-T tags] [-B suffix]\n"
"           [-a aftercmd] [-f flags] [-m mode] [-N dbdir] [-o owner] [-g group]\n"
"           [-l linkflags] [-h hash] [-S stripflags] file1 ... fileN directory\n"
"       %s -d [-Up] [-M log] [-D dest] [-T tags] [-a aftercmd] [-m mode]\n"
"           [-N dbdir] [-o owner] [-g group] directory ...\n",
	    prog, prog, prog);
	exit(1);
}
