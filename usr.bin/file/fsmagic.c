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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#ifndef	major			/* if `major' not defined in types.h, */
#include <sys/sysmacros.h>	/* try this one. */
#endif
#ifndef	major	/* still not defined? give up, manual intervention needed */
		/* If cc tries to compile this, read and act on it. */
		/* On most systems cpp will discard it automatically */
		Congratulations, you have found a portability bug.
		Please grep /usr/include/sys and edit the above #include
		to point at the file that defines the "major" macro.
#endif	/*major*/

#include "file.h"

#ifndef	lint
static char *moduleid =
	"@(#)$Id: fsmagic.c,v 1.2 1995/05/30 06:30:03 rgrimes Exp $";
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

	if (sb->st_mode & S_ISUID) ckfputs("setuid ", stdout);
	if (sb->st_mode & S_ISGID) ckfputs("setgid ", stdout);
	if (sb->st_mode & S_ISVTX) ckfputs("sticky ", stdout);

	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		ckfputs("directory", stdout);
		return 1;
	case S_IFCHR:
		(void) printf("character special (%d/%d)",
			major(sb->st_rdev), minor(sb->st_rdev));
		return 1;
	case S_IFBLK:
		(void) printf("block special (%d/%d)",
			major(sb->st_rdev), minor(sb->st_rdev));
		return 1;
	/* TODO add code to handle V7 MUX and Blit MUX files */
#ifdef	S_IFIFO
	case S_IFIFO:
		ckfputs("fifo (named pipe)", stdout);
		return 1;
#endif
#ifdef	S_IFLNK
	case S_IFLNK:
		{
			char buf[BUFSIZ+4];
			register int nch;
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
	 */
	if (sb->st_size == 0) {
		ckfputs("empty", stdout);
		return 1;
	}
	return 0;
}

