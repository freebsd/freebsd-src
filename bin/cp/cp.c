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
#include <fcntl.h>
#include <fts.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static char dot[] = ".";

#define END(buf) (buf + sizeof(buf))
PATH_T to = { .dir = -1, .end = to.path };
bool Nflag, fflag, iflag, lflag, nflag, pflag, sflag, vflag;
static bool Hflag, Lflag, Pflag, Rflag, rflag, Sflag;
volatile sig_atomic_t info;

enum op { FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE };

static int copy(char *[], enum op, int, struct stat *);
static void siginfo(int __unused);

enum {
	SORT_OPT = CHAR_MAX,
};

static const struct option long_opts[] =
{
	{ "archive",		no_argument,		NULL,	'a' },
	{ "force",		no_argument,		NULL,	'f' },
	{ "interactive",	no_argument,		NULL,	'i' },
	{ "dereference",	no_argument,		NULL,	'L' },
	{ "link",		no_argument,		NULL,	'l' },
	{ "no-clobber",		no_argument,		NULL,	'n' },
	{ "no-dereference",	no_argument,		NULL,	'P' },
	{ "recursive",		no_argument,		NULL,	'R' },
	{ "symbolic-link",	no_argument,		NULL,	's' },
	{ "verbose",		no_argument,		NULL,	'v' },
	{ "one-file-system",	no_argument,		NULL,	'x' },
	{ "sort",		no_argument,		NULL,	SORT_OPT },
	{ 0 }
};

int
main(int argc, char *argv[])
{
	struct stat to_stat, tmp_stat;
	enum op type;
	int ch, fts_options, r;
	char *sep, *target;
	bool have_trailing_slash = false;

	fts_options = FTS_NOCHDIR | FTS_PHYSICAL;
	while ((ch = getopt_long(argc, argv, "+HLPRafilNnprsvx", long_opts,
	    NULL)) != -1)
		switch (ch) {
		case 'H':
			Hflag = true;
			Lflag = Pflag = false;
			break;
		case 'L':
			Lflag = true;
			Hflag = Pflag = false;
			break;
		case 'P':
			Pflag = true;
			Hflag = Lflag = false;
			break;
		case 'R':
			Rflag = true;
			break;
		case 'a':
			pflag = true;
			Rflag = true;
			Pflag = true;
			Hflag = Lflag = false;
			break;
		case 'f':
			fflag = true;
			iflag = nflag = false;
			break;
		case 'i':
			iflag = true;
			fflag = nflag = false;
			break;
		case 'l':
			lflag = true;
			break;
		case 'N':
			Nflag = true;
			break;
		case 'n':
			nflag = true;
			fflag = iflag = false;
			break;
		case 'p':
			pflag = true;
			break;
		case 'r':
			rflag = Lflag = true;
			Hflag = Pflag = false;
			break;
		case 's':
			sflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		case 'x':
			fts_options |= FTS_XDEV;
			break;
		case SORT_OPT:
			Sflag = true;
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
		Rflag = true;
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
	if (*target == '\0') {
		target = dot;
	} else if ((sep = strrchr(target, '/')) != NULL && sep[1] == '\0') {
		have_trailing_slash = true;
		while (sep > target && *sep == '/')
			sep--;
		sep[1] = '\0';
	}
	/*
	 * Copy target into to.base, leaving room for a possible separator
	 * which will be appended later in the non-FILE_TO_FILE cases.
	 */
	if (strlcpy(to.base, target, sizeof(to.base) - 1) >=
	    sizeof(to.base) - 1)
		errc(1, ENAMETOOLONG, "%s", target);

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
	r = stat(to.base, &to_stat);
	if (r == -1 && errno != ENOENT)
		err(1, "%s", target);
	if (r == -1 || !S_ISDIR(to_stat.st_mode)) {
		/*
		 * Case (1).  Target is not a directory.
		 */
		if (argc > 1)
			errc(1, ENOTDIR, "%s", target);

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
				errc(1, ENOENT, "%s", target);
			else
				errc(1, ENOTDIR, "%s", target);
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
ftscmp(const FTSENT * const *a, const FTSENT * const *b)
{
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

static int
copy(char *argv[], enum op type, int fts_options, struct stat *root_stat)
{
	char rootname[NAME_MAX];
	struct stat created_root_stat, to_stat, *curr_stat;
	FTS *ftsp;
	FTSENT *curr;
	char *recpath = NULL, *sep;
	int atflags, dne, badcp, len, level, rval;
	mode_t mask, mode;
	bool beneath = Rflag && type != FILE_TO_FILE;

	/*
	 * Keep an inverted copy of the umask, for use in correcting
	 * permissions on created directories when not using -p.
	 */
	mask = ~umask(0777);
	umask(~mask);

	if (type == FILE_TO_FILE) {
		to.dir = AT_FDCWD;
		to.end = to.path + strlcpy(to.path, to.base, sizeof(to.path));
		to.base[0] = '\0';
	} else if (type == FILE_TO_DIR) {
		to.dir = open(to.base, O_DIRECTORY | O_SEARCH);
		if (to.dir < 0)
			err(1, "%s", to.base);
		/*
		 * We have previously made sure there is room for this.
		 */
		if (strcmp(to.base, "/") != 0) {
			sep = strchr(to.base, '\0');
			sep[0] = '/';
			sep[1] = '\0';
		}
	} else {
		/*
		 * We will create the destination directory imminently.
		 */
		to.dir = -1;
	}

	level = FTS_ROOTLEVEL;
	if ((ftsp = fts_open(argv, fts_options, Sflag ? ftscmp : NULL)) == NULL)
		err(1, "fts_open");
	for (badcp = rval = 0;
	     (curr = fts_read(ftsp)) != NULL;
	     badcp = 0, *to.end = '\0') {
		curr_stat = curr->fts_statp;
		switch (curr->fts_info) {
		case FTS_NS:
		case FTS_DNR:
		case FTS_ERR:
			if (level > curr->fts_level) {
				/* leaving a directory; remove its name from to.path */
				if (type == DIR_TO_DNE &&
				    curr->fts_level == FTS_ROOTLEVEL) {
					/* this is actually our created root */
				} else {
					while (to.end > to.path && *to.end != '/')
						to.end--;
					assert(strcmp(to.end + (*to.end == '/'),
					    curr->fts_name) == 0);
					*to.end = '\0';
				}
				level--;
			}
			warnc(curr->fts_errno, "%s", curr->fts_path);
			badcp = rval = 1;
			continue;
		case FTS_DC:			/* Warn, continue. */
			warnx("%s: directory causes a cycle", curr->fts_path);
			badcp = rval = 1;
			continue;
		case FTS_D:
			/*
			 * Stash the root basename off for detecting
			 * recursion later.
			 *
			 * This will be essential if the root is a symlink
			 * and we're rolling with -L or -H.  The later
			 * bits will need this bit in particular.
			 */
			if (curr->fts_level == FTS_ROOTLEVEL) {
				strlcpy(rootname, curr->fts_name,
				    sizeof(rootname));
			}
			/* we must have a destination! */
			if (type == DIR_TO_DNE &&
			    curr->fts_level == FTS_ROOTLEVEL) {
				assert(to.dir < 0);
				assert(root_stat == NULL);
				mode = curr_stat->st_mode | S_IRWXU;
				/*
				 * Will our umask prevent us from entering
				 * the directory after we create it?
				 */
				if (~mask & S_IRWXU)
					umask(~mask & ~S_IRWXU);
				if (mkdir(to.base, mode) != 0) {
					warn("%s", to.base);
					fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					if (~mask & S_IRWXU)
						umask(~mask);
					continue;
				}
				to.dir = open(to.base, O_DIRECTORY | O_SEARCH);
				if (to.dir < 0) {
					warn("%s", to.base);
					(void)rmdir(to.base);
					fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					if (~mask & S_IRWXU)
						umask(~mask);
					continue;
				}
				if (fstat(to.dir, &created_root_stat) != 0) {
					warn("%s", to.base);
					(void)close(to.dir);
					(void)rmdir(to.base);
					fts_set(ftsp, curr, FTS_SKIP);
					to.dir = -1;
					badcp = rval = 1;
					if (~mask & S_IRWXU)
						umask(~mask);
					continue;
				}
				if (~mask & S_IRWXU)
					umask(~mask);
				root_stat = &created_root_stat;
				curr->fts_number = 1;
				/*
				 * We have previously made sure there is
				 * room for this.
				 */
				sep = strchr(to.base, '\0');
				sep[0] = '/';
				sep[1] = '\0';
			} else if (strcmp(curr->fts_name, "/") == 0) {
				/* special case when source is the root directory */
			} else {
				/* entering a directory; append its name to to.path */
				len = snprintf(to.end, END(to.path) - to.end, "%s%s",
				    to.end > to.path ? "/" : "", curr->fts_name);
				if (to.end + len >= END(to.path)) {
					*to.end = '\0';
					warnc(ENAMETOOLONG, "%s%s%s%s", to.base,
					    to.path, to.end > to.path ? "/" : "",
					    curr->fts_name);
					fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					continue;
				}
				to.end += len;
			}
			level++;
			/*
			 * We're on the verge of recursing on ourselves.
			 * Either we need to stop right here (we knowingly
			 * just created it), or we will in an immediate
			 * descendant.  Record the path of the immediate
			 * descendant to make our lives a little less
			 * complicated looking.
			 */
			if (type != FILE_TO_FILE &&
			    root_stat->st_dev == curr_stat->st_dev &&
			    root_stat->st_ino == curr_stat->st_ino) {
				assert(recpath == NULL);
				if (root_stat == &created_root_stat) {
					/*
					 * This directory didn't exist
					 * when we started, we created it
					 * as part of traversal.  Stop
					 * right here before we do
					 * something silly.
					 */
					fts_set(ftsp, curr, FTS_SKIP);
					continue;
				}
				if (asprintf(&recpath, "%s/%s", to.path,
				    rootname) < 0) {
					warnc(ENOMEM, NULL);
					fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					continue;
				}
			}
			if (recpath != NULL &&
			    strcmp(recpath, to.path) == 0) {
				fts_set(ftsp, curr, FTS_SKIP);
				continue;
			}
			break;
		case FTS_DP:
			/*
			 * We are nearly finished with this directory.  If we
			 * didn't actually copy it, or otherwise don't need to
			 * change its attributes, then we are done.
			 *
			 * If -p is in effect, set all the attributes.
			 * Otherwise, set the correct permissions, limited
			 * by the umask.  Optimise by avoiding a chmod()
			 * if possible (which is usually the case if we
			 * made the directory).  Note that mkdir() does not
			 * honour setuid, setgid and sticky bits, but we
			 * normally want to preserve them on directories.
			 */
			if (curr->fts_number && pflag) {
				int fd = *to.path ? -1 : to.dir;
				if (setfile(curr_stat, fd, true))
					rval = 1;
				if (preserve_dir_acls(curr->fts_accpath,
				    to.path) != 0)
					rval = 1;
			} else if (curr->fts_number) {
				const char *path = *to.path ? to.path : dot;
				mode = curr_stat->st_mode;
				if (fchmodat(to.dir, path, mode & mask, 0) != 0) {
					warn("chmod: %s%s", to.base, to.path);
					rval = 1;
				}
			}
			if (level > curr->fts_level) {
				/* leaving a directory; remove its name from to.path */
				if (type == DIR_TO_DNE &&
				    curr->fts_level == FTS_ROOTLEVEL) {
					/* this is actually our created root */
				} else if (strcmp(curr->fts_name, "/") == 0) {
					/* special case when source is the root directory */
				} else {
					while (to.end > to.path && *to.end != '/')
						to.end--;
					assert(strcmp(to.end + (*to.end == '/'),
					    curr->fts_name) == 0);
					*to.end = '\0';
				}
				level--;
			}
			continue;
		default:
			/* something else: append its name to to.path */
			if (type == FILE_TO_FILE)
				break;
			len = snprintf(to.end, END(to.path) - to.end, "%s%s",
			    to.end > to.path ? "/" : "", curr->fts_name);
			if (to.end + len >= END(to.path)) {
				*to.end = '\0';
				warnc(ENAMETOOLONG, "%s%s%s%s", to.base,
				    to.path, to.end > to.path ? "/" : "",
				    curr->fts_name);
				badcp = rval = 1;
				continue;
			}
			/* intentionally do not update to.end */
			break;
		}

		/* Not an error but need to remember it happened. */
		if (to.path[0] == '\0') {
			/*
			 * This can happen in three cases:
			 * - The source path is the root directory.
			 * - DIR_TO_DNE; we created the directory and
			 *   populated root_stat earlier.
			 * - FILE_TO_DIR if a source has a trailing slash;
			 *   the caller populated root_stat.
			 */
			dne = false;
			to_stat = *root_stat;
		} else {
			atflags = beneath ? AT_RESOLVE_BENEATH : 0;
			if (curr->fts_info == FTS_D || curr->fts_info == FTS_SL)
				atflags |= AT_SYMLINK_NOFOLLOW;
			dne = fstatat(to.dir, to.path, &to_stat, atflags) != 0;
		}

		/* Check if source and destination are identical. */
		if (!dne &&
		    to_stat.st_dev == curr_stat->st_dev &&
		    to_stat.st_ino == curr_stat->st_ino) {
			warnx("%s%s and %s are identical (not copied).",
			    to.base, to.path, curr->fts_path);
			badcp = rval = 1;
			if (S_ISDIR(curr_stat->st_mode))
				fts_set(ftsp, curr, FTS_SKIP);
			continue;
		}

		switch (curr_stat->st_mode & S_IFMT) {
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
				if (copy_file(curr, dne, beneath))
					badcp = rval = 1;
			} else {
				/* Copy the link. */
				if (copy_link(curr, dne, beneath))
					badcp = rval = 1;
			}
			break;
		case S_IFDIR:
			if (!Rflag) {
				warnx("%s is a directory (not copied).",
				    curr->fts_path);
				fts_set(ftsp, curr, FTS_SKIP);
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
				mode = curr_stat->st_mode | S_IRWXU;
				/*
				 * Will our umask prevent us from entering
				 * the directory after we create it?
				 */
				if (~mask & S_IRWXU)
					umask(~mask & ~S_IRWXU);
				if (mkdirat(to.dir, to.path, mode) != 0) {
					warn("%s%s", to.base, to.path);
					fts_set(ftsp, curr, FTS_SKIP);
					badcp = rval = 1;
					if (~mask & S_IRWXU)
						umask(~mask);
					break;
				}
				if (~mask & S_IRWXU)
					umask(~mask);
			} else if (!S_ISDIR(to_stat.st_mode)) {
				warnc(ENOTDIR, "%s%s", to.base, to.path);
				fts_set(ftsp, curr, FTS_SKIP);
				badcp = rval = 1;
				break;
			}
			/*
			 * Arrange to correct directory attributes later
			 * (in the post-order phase) if this is a new
			 * directory, or if the -p flag is in effect.
			 * Note that fts_number may already be set if this
			 * is the newly created destination directory.
			 */
			curr->fts_number |= pflag || dne;
			break;
		case S_IFBLK:
		case S_IFCHR:
			if (Rflag && !sflag) {
				if (copy_special(curr_stat, dne, beneath))
					badcp = rval = 1;
			} else {
				if (copy_file(curr, dne, beneath))
					badcp = rval = 1;
			}
			break;
		case S_IFSOCK:
			warnx("%s is a socket (not copied).",
			    curr->fts_path);
			break;
		case S_IFIFO:
			if (Rflag && !sflag) {
				if (copy_fifo(curr_stat, dne, beneath))
					badcp = rval = 1;
			} else {
				if (copy_file(curr, dne, beneath))
					badcp = rval = 1;
			}
			break;
		default:
			if (copy_file(curr, dne, beneath))
				badcp = rval = 1;
			break;
		}
		if (vflag && !badcp)
			(void)printf("%s -> %s%s\n", curr->fts_path, to.base, to.path);
	}
	assert(level == FTS_ROOTLEVEL);
	if (errno)
		err(1, "fts_read");
	(void)fts_close(ftsp);
	if (to.dir != AT_FDCWD && to.dir >= 0)
		(void)close(to.dir);
	free(recpath);
	return (rval);
}

static void
siginfo(int sig __unused)
{

	info = 1;
}
