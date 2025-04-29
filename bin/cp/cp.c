/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * Cp copies source files to target files.
 *
 * The global PATH_T structure "to" always contains the path to the
 * current target file.  Since fts(3) does not change directories,
 * this path can be either absolute or dot-relative.
 *
 * The basic algorithm is to initialize "to" and use fts(3) to traverse
 * the file hierarchy rooted in the argument list.  A trivial case is the
 * case of 'cp file1 file2'.  The more interesting case is the case of
 * 'cp file1 file2 ... fileN dir' where the hierarchy is traversed and the
 * path (relative to the root of the traversal) is appended to dir (stored
 * in "to") to form the final target path.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define	STRIP_TRAILING_SLASH(p) {					\
	while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')	\
	*--(p).p_end = 0;						\
}

static char emptystring[] = "";

PATH_T to = { to.p_path, emptystring, "" };

int Nflag, fflag, iflag, lflag, nflag, pflag, sflag, vflag;
static int Hflag, Lflag, Pflag, Rflag, rflag;
volatile sig_atomic_t info;

enum op { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

static int copy(char *[], enum op, int, struct stat *);
static void siginfo(int __unused);

int
main(int argc, char *argv[])
{
	struct stat to_stat, tmp_stat;
	enum op type;
	int ch, fts_options, r, have_trailing_slash;
	char *target;

	fts_options = FTS_NOCHDIR | FTS_PHYSICAL;
	while ((ch = getopt(argc, argv, "HLPRafilNnprsvx")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'a':
			pflag = 1;
			Rflag = 1;
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'f':
			fflag = 1;
			iflag = nflag = 0;
			break;
		case 'i':
			iflag = 1;
			fflag = nflag = 0;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'n':
			nflag = 1;
			fflag = iflag = 0;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'x':
			fts_options |= FTS_XDEV;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	if (Rflag && rflag)
		errx(1, "the -R and -r options may not be specified together");
	if (lflag && sflag)
		errx(1, "the -l and -s options may not be specified together");
	if (rflag)
		Rflag = 1;
	if (Rflag) {
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else if (!Pflag) {
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL | FTS_COMFOLLOW;
	}
	(void)signal(SIGINFO, siginfo);

	/* Save the target base in "to". */
	target = argv[--argc];
	if (strlcpy(to.p_path, target, sizeof(to.p_path)) >= sizeof(to.p_path))
		errx(1, "%s: name too long", target);
	to.p_end = to.p_path + strlen(to.p_path);
	if (to.p_path == to.p_end) {
		*to.p_end++ = '.';
		*to.p_end = 0;
	}
	have_trailing_slash = (to.p_end[-1] == '/');
	if (have_trailing_slash)
		STRIP_TRAILING_SLASH(to);
	to.target_end = to.p_end;

	/* Set end of argument list for fts(3). */
	argv[argc] = NULL;

	/*
	 * Cp has two distinct cases:
	 *
	 * cp [-R] source target
	 * cp [-R] source1 ... sourceN directory
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
	if (r == -1 && errno != ENOENT)
		err(1, "%s", to.p_path);
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		/*
		 * Case (1).  Target is not a directory.
		 */
		if (argc > 1)
			errc(1, ENOTDIR, "%s", to.p_path);

		/*
		 * Need to detect the case:
		 *	cp -R dir foo
		 * Where dir is a directory and foo does not exist, where
		 * we want pathname concatenations turned on but not for
		 * the initial mkdir().
		 */
		if (r == -1) {
			if (Rflag && (Lflag || Hflag))
				stat(*argv, &tmp_stat);
			else
				lstat(*argv, &tmp_stat);

			if (S_ISDIR(tmp_stat.st_mode) && Rflag)
				type = DIR_TO_DNE;
			else
				type = FILE_TO_FILE;
		} else
			type = FILE_TO_FILE;

		if (have_trailing_slash && type == FILE_TO_FILE) {
			if (r == -1)
				errc(1, ENOENT, "%s", to.p_path);
			else
				errc(1, ENOTDIR, "%s", to.p_path);
		}
	} else {
		/*
		 * Case (2).  Target is a directory.
		 */
		type = FILE_TO_DIR;
	}

	/*
	 * For DIR_TO_DNE, we could provide copy() with the to_stat we've
	 * already allocated on the stack here that isn't being used for
	 * anything.  Not doing so, though, simplifies later logic a little bit
	 * as we need to skip checking root_stat on the first iteration and
	 * ensure that we set it with the first mkdir().
	 */
	exit (copy(argv, type, fts_options, (type == DIR_TO_DNE ? NULL :
	    &to_stat)));
}

static int
copy(char *argv[], enum op type, int fts_options, struct stat *root_stat)
{
	char rootname[NAME_MAX];
	struct stat created_root_stat, to_stat;
	FTS *ftsp;
	FTSENT *curr;
	int base = 0, dne, badcp, rval;
	size_t nlen;
	char *p, *recurse_path, *target_mid;
	mode_t mask, mode;

	/*
	 * Keep an inverted copy of the umask, for use in correcting
	 * permissions on created directories when not using -p.
	 */
	mask = ~umask(0777);
	umask(~mask);

	recurse_path = NULL;
	if ((ftsp = fts_open(argv, fts_options, NULL)) == NULL)
		err(1, "fts_open");
	for (badcp = rval = 0; (curr = fts_read(ftsp)) != NULL; badcp = 0) {
		switch (curr->fts_info) {
		case FTS_NS:
		case FTS_DNR:
		case FTS_ERR:
			warnc(curr->fts_errno, "%s", curr->fts_path);
			badcp = rval = 1;
			continue;
		case FTS_DC:			/* Warn, continue. */
			warnx("%s: directory causes a cycle", curr->fts_path);
			badcp = rval = 1;
			continue;
		default:
			;
		}

		/*
		 * Stash the root basename off for detecting recursion later.
		 *
		 * This will be essential if the root is a symlink and we're
		 * rolling with -L or -H.  The later bits will need this bit in
		 * particular.
		 */
		if (curr->fts_level == FTS_ROOTLEVEL) {
			strlcpy(rootname, curr->fts_name, sizeof(rootname));
		}

		/*
		 * If we are in case (2) or (3) above, we need to append the
		 * source name to the target name.
		 */
		if (type != FILE_TO_FILE) {
			/*
			 * Need to remember the roots of traversals to create
			 * correct pathnames.  If there's a directory being
			 * copied to a non-existent directory, e.g.
			 *	cp -R a/dir noexist
			 * the resulting path name should be noexist/foo, not
			 * noexist/dir/foo (where foo is a file in dir), which
			 * is the case where the target exists.
			 *
			 * Also, check for "..".  This is for correct path
			 * concatenation for paths ending in "..", e.g.
			 *	cp -R .. /tmp
			 * Paths ending in ".." are changed to ".".  This is
			 * tricky, but seems the easiest way to fix the problem.
			 *
			 * XXX
			 * Since the first level MUST be FTS_ROOTLEVEL, base
			 * is always initialized.
			 */
			if (curr->fts_level == FTS_ROOTLEVEL) {
				if (type != DIR_TO_DNE) {
					p = strrchr(curr->fts_path, '/');
					base = (p == NULL) ? 0 :
					    (int)(p - curr->fts_path + 1);

					if (strcmp(curr->fts_path + base, "..")
					    == 0)
						base += 1;
				} else
					base = curr->fts_pathlen;
			}

			p = &curr->fts_path[base];
			nlen = curr->fts_pathlen - base;
			target_mid = to.target_end;
			if (*p != '/' && target_mid[-1] != '/')
				*target_mid++ = '/';
			*target_mid = 0;
			if (target_mid - to.p_path + nlen >= PATH_MAX) {
				warnx("%s%s: name too long (not copied)",
				    to.p_path, p);
				badcp = rval = 1;
				continue;
			}
			(void)strncat(target_mid, p, nlen);
			to.p_end = target_mid + nlen;
			*to.p_end = 0;
			STRIP_TRAILING_SLASH(to);

			/*
			 * We're on the verge of recursing on ourselves.  Either
			 * we need to stop right here (we knowingly just created
			 * it), or we will in an immediate descendant.  Record
			 * the path of the immediate descendant to make our
			 * lives a little less complicated looking.
			 */
			if (curr->fts_info == FTS_D && root_stat != NULL &&
			    root_stat->st_dev == curr->fts_statp->st_dev &&
			    root_stat->st_ino == curr->fts_statp->st_ino) {
				assert(recurse_path == NULL);

				if (root_stat == &created_root_stat) {
					/*
					 * This directory didn't exist when we
					 * started, we created it as part of
					 * traversal.  Stop right here before we
					 * do something silly.
					 */
					fts_set(ftsp, curr, FTS_SKIP);
					continue;
				}

				if (asprintf(&recurse_path, "%s/%s", to.p_path,
				    rootname) == -1)
					err(1, "asprintf");
			}

			if (recurse_path != NULL &&
			    strcmp(to.p_path, recurse_path) == 0) {
				fts_set(ftsp, curr, FTS_SKIP);
				continue;
			}
		}

		if (curr->fts_info == FTS_DP) {
			/*
			 * We are nearly finished with this directory.  If we
			 * didn't actually copy it, or otherwise don't need to
			 * change its attributes, then we are done.
			 */
			if (!curr->fts_number)
				continue;
			/*
			 * If -p is in effect, set all the attributes.
			 * Otherwise, set the correct permissions, limited
			 * by the umask.  Optimise by avoiding a chmod()
			 * if possible (which is usually the case if we
			 * made the directory).  Note that mkdir() does not
			 * honour setuid, setgid and sticky bits, but we
			 * normally want to preserve them on directories.
			 */
			if (pflag) {
				if (setfile(curr->fts_statp, -1))
					rval = 1;
				if (preserve_dir_acls(curr->fts_statp,
				    curr->fts_accpath, to.p_path) != 0)
					rval = 1;
			} else {
				mode = curr->fts_statp->st_mode;
				if ((mode & (S_ISUID | S_ISGID | S_ISTXT)) ||
				    ((mode | S_IRWXU) & mask) != (mode & mask))
					if (chmod(to.p_path, mode & mask) !=
					    0) {
						warn("chmod: %s", to.p_path);
						rval = 1;
					}
			}
			continue;
		}

		/* Check if source and destination are identical. */
		if (stat(to.p_path, &to_stat) == 0 &&
		    to_stat.st_dev == curr->fts_statp->st_dev &&
		    to_stat.st_ino == curr->fts_statp->st_ino) {
			warnx("%s and %s are identical (not copied).",
			    to.p_path, curr->fts_path);
			badcp = rval = 1;
			if (S_ISDIR(curr->fts_statp->st_mode))
				(void)fts_set(ftsp, curr, FTS_SKIP);
			continue;
		}

		/* Not an error but need to remember it happened. */
		dne = lstat(to.p_path, &to_stat) != 0;

		switch (curr->fts_statp->st_mode & S_IFMT) {
		case S_IFLNK:
			if ((fts_options & FTS_LOGICAL) ||
			    ((fts_options & FTS_COMFOLLOW) &&
			    curr->fts_level == 0)) {
				/*
				 * We asked FTS to follow links but got
				 * here anyway, which means the target is
				 * nonexistent or inaccessible.  Let
				 * copy_file() deal with the error.
				 */
				if (copy_file(curr, dne))
					badcp = rval = 1;
			} else {
				/* Copy the link. */
				if (copy_link(curr, !dne))
					badcp = rval = 1;
			}
			break;
		case S_IFDIR:
			if (!Rflag) {
				warnx("%s is a directory (not copied).",
				    curr->fts_path);
				(void)fts_set(ftsp, curr, FTS_SKIP);
				badcp = rval = 1;
				break;
			}
			/*
			 * If the directory doesn't exist, create the new
			 * one with the from file mode plus owner RWX bits,
			 * modified by the umask.  Trade-off between being
			 * able to write the directory (if from directory is
			 * 555) and not causing a permissions race.  If the
			 * umask blocks owner writes, we fail.
			 */
			if (dne) {
				mode = curr->fts_statp->st_mode | S_IRWXU;
				if (mkdir(to.p_path, mode) != 0) {
					warn("%s", to.p_path);
					(void)fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					break;
				}
				/*
				 * First DNE with a NULL root_stat is the root
				 * path, so set root_stat.  We can't really
				 * tell in all cases if the target path is
				 * within the src path, so we just stat() the
				 * first directory we created and use that.
				 */
				if (root_stat == NULL &&
				    stat(to.p_path, &created_root_stat) != 0) {
					warn("%s", to.p_path);
					(void)fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					break;
				}
				if (root_stat == NULL)
					root_stat = &created_root_stat;
			} else if (!S_ISDIR(to_stat.st_mode)) {
				warnc(ENOTDIR, "%s", to.p_path);
				(void)fts_set(ftsp, curr, FTS_SKIP);
				badcp = rval = 1;
				break;
			}
			/*
			 * Arrange to correct directory attributes later
			 * (in the post-order phase) if this is a new
			 * directory, or if the -p flag is in effect.
			 */
			curr->fts_number = pflag || dne;
			break;
		case S_IFBLK:
		case S_IFCHR:
			if (Rflag && !sflag) {
				if (copy_special(curr->fts_statp, !dne))
					badcp = rval = 1;
			} else {
				if (copy_file(curr, dne))
					badcp = rval = 1;
			}
			break;
		case S_IFSOCK:
			warnx("%s is a socket (not copied).",
			    curr->fts_path);
			break;
		case S_IFIFO:
			if (Rflag && !sflag) {
				if (copy_fifo(curr->fts_statp, !dne))
					badcp = rval = 1;
			} else {
				if (copy_file(curr, dne))
					badcp = rval = 1;
			}
			break;
		default:
			if (copy_file(curr, dne))
				badcp = rval = 1;
			break;
		}
		if (vflag && !badcp)
			(void)printf("%s -> %s\n", curr->fts_path, to.p_path);
	}
	if (errno)
		err(1, "fts_read");
	fts_close(ftsp);
	free(recurse_path);
	return (rval);
}

static void
siginfo(int sig __unused)
{

	info = 1;
}
