/*
 * fsmagic - magic based on filesystem info - directory, special files, etc.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include "file.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
/* Since major is a function on SVR4, we can't use `ifndef major'.  */
#ifdef MAJOR_IN_MKDEV
# include <sys/mkdev.h>
# define HAVE_MAJOR
#endif
#ifdef MAJOR_IN_SYSMACROS
# include <sys/sysmacros.h>
# define HAVE_MAJOR
#endif
#ifdef major			/* Might be defined in sys/types.h.  */
# define HAVE_MAJOR
#endif
  
#ifndef HAVE_MAJOR
# define major(dev)  (((dev) >> 8) & 0xff)
# define minor(dev)  ((dev) & 0xff)
#endif
#undef HAVE_MAJOR

#ifndef	lint
FILE_RCSID("@(#)$Id: fsmagic.c,v 1.33 2000/08/05 17:36:48 christos Exp $")
#endif	/* lint */

int
fsmagic(fn, sb)
	const char *fn;
	struct stat *sb;
{
	int ret = 0;

	/*
	 * Fstat is cheaper but fails for files you don't have read perms on.
	 * On 4.2BSD and similar systems, use lstat() to identify symlinks.
	 */
#ifdef	S_IFLNK
	if (!lflag)
		ret = lstat(fn, sb);
	else
#endif
	ret = stat(fn, sb);	/* don't merge into if; see "ret =" above */

	if (ret) {
		ckfprintf(stdout,
			/* Yes, I do mean stdout. */
			/* No \n, caller will provide. */
			"can't stat `%s' (%s).", fn, strerror(errno));
		return 1;
	}

	if (iflag) {
		if ((sb->st_mode & S_IFMT) != S_IFREG) {
			ckfputs("application/x-not-regular-file", stdout);
			return 1;
		}
	}
	else {
#ifdef S_ISUID
		if (sb->st_mode & S_ISUID) ckfputs("setuid ", stdout);
#endif
#ifdef S_ISGID
		if (sb->st_mode & S_ISGID) ckfputs("setgid ", stdout);
#endif
#ifdef S_ISVTX
		if (sb->st_mode & S_ISVTX) ckfputs("sticky ", stdout);
#endif
	}
	
	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		ckfputs("directory", stdout);
		return 1;
#ifdef S_IFCHR
	case S_IFCHR:
		/* 
		 * If -s has been specified, treat character special files
		 * like ordinary files.  Otherwise, just report that they
		 * are block special files and go on to the next file.
		 */
		if (sflag)
			break;
#ifdef HAVE_ST_RDEV
# ifdef dv_unit
		(void) printf("character special (%d/%d/%d)",
			major(sb->st_rdev),
			dv_unit(sb->st_rdev),
			dv_subunit(sb->st_rdev));
# else
		(void) printf("character special (%ld/%ld)",
			(long) major(sb->st_rdev), (long) minor(sb->st_rdev));
# endif
#else
		(void) printf("character special");
#endif
		return 1;
#endif
#ifdef S_IFBLK
	case S_IFBLK:
		/* 
		 * If -s has been specified, treat block special files
		 * like ordinary files.  Otherwise, just report that they
		 * are block special files and go on to the next file.
		 */
		if (sflag)
			break;
#ifdef HAVE_ST_RDEV
# ifdef dv_unit
		(void) printf("block special (%d/%d/%d)",
			major(sb->st_rdev),
			dv_unit(sb->st_rdev),
			dv_subunit(sb->st_rdev));
# else
		(void) printf("block special (%ld/%ld)",
			(long) major(sb->st_rdev), (long) minor(sb->st_rdev));
# endif
#else
		(void) printf("block special");
#endif
		return 1;
#endif
	/* TODO add code to handle V7 MUX and Blit MUX files */
#ifdef	S_IFIFO
	case S_IFIFO:
		ckfputs("fifo (named pipe)", stdout);
		return 1;
#endif
#ifdef	S_IFDOOR
	case S_IFDOOR:
		ckfputs("door", stdout);
		return 1;
#endif
#ifdef	S_IFLNK
	case S_IFLNK:
		{
			char buf[BUFSIZ+4];
			int nch;
			struct stat tstatbuf;

			if ((nch = readlink(fn, buf, BUFSIZ-1)) <= 0) {
				ckfprintf(stdout, "unreadable symlink (%s).", 
				      strerror(errno));
				return 1;
			}
			buf[nch] = '\0';	/* readlink(2) forgets this */

			/* If broken symlink, say so and quit early. */
			if (*buf == '/') {
			    if (stat(buf, &tstatbuf) < 0) {
				ckfprintf(stdout,
					"broken symbolic link to %s", buf);
				return 1;
			    }
			}
			else {
			    char *tmp;
			    char buf2[BUFSIZ+BUFSIZ+4];

			    if ((tmp = strrchr(fn,  '/')) == NULL) {
				tmp = buf; /* in current directory anyway */
			    }
			    else {
				strcpy (buf2, fn);  /* take directory part */
				buf2[tmp-fn+1] = '\0';
				strcat (buf2, buf); /* plus (relative) symlink */
				tmp = buf2;
			    }
			    if (stat(tmp, &tstatbuf) < 0) {
				ckfprintf(stdout,
					"broken symbolic link to %s", buf);
				return 1;
			    }
                        }

			/* Otherwise, handle it. */
			if (lflag) {
				process(buf, strlen(buf));
				return 1;
			} else { /* just print what it points to */
				ckfputs("symbolic link to ", stdout);
				ckfputs(buf, stdout);
			}
		}
		return 1;
#endif
#ifdef	S_IFSOCK
#ifndef __COHERENT__
	case S_IFSOCK:
		ckfputs("socket", stdout);
		return 1;
#endif
#endif
	case S_IFREG:
		break;
	default:
		error("invalid mode 0%o.\n", sb->st_mode);
		/*NOTREACHED*/
	}

	/*
	 * regular file, check next possibility
	 *
	 * If stat() tells us the file has zero length, report here that
	 * the file is empty, so we can skip all the work of opening and 
	 * reading the file.
	 * But if the -s option has been given, we skip this optimization,
	 * since on some systems, stat() reports zero size for raw disk
	 * partitions.  (If the block special device really has zero length,
	 * the fact that it is empty will be detected and reported correctly
	 * when we read the file.)
	 */
	if (!sflag && sb->st_size == 0) {
		ckfputs(iflag ? "application/x-empty" : "empty", stdout);
		return 1;
	}
	return 0;
}
