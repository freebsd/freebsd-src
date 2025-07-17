/*	$OpenBSD: diffdir.c,v 1.45 2015/10/05 20:15:00 millert Exp $	*/

/*
 * Copyright (c) 2003, 2010 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/stat.h>
#include <sys/tree.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"

static int selectfile(const struct dirent *);
static void diffit(struct dirent *, char *, size_t, struct dirent *,
	char *, size_t, int);
static void print_only(const char *, size_t, const char *);

#define d_status	d_type		/* we need to store status for -l */

struct inode {
	dev_t dev;
	ino_t ino;
	RB_ENTRY(inode) entry;
};

static int
inodecmp(struct inode *a, struct inode *b)
{
	return (a->dev < b->dev ? -1 : a->dev > b->dev ? 1 :
	    a->ino < b->ino ? -1 : a->ino > b->ino ? 1 : 0);
}

RB_HEAD(inodetree, inode);
static struct inodetree v1 = RB_INITIALIZER(&v1);
static struct inodetree v2 = RB_INITIALIZER(&v2);
RB_GENERATE_STATIC(inodetree, inode, entry, inodecmp);

static int
vscandir(struct inodetree *tree, const char *path, struct dirent ***dirp,
    int (*selectf)(const struct dirent *),
    int (*comparf)(const struct dirent **, const struct dirent **))
{
	struct stat sb;
	struct inode *ino = NULL;
	int fd = -1, ret, serrno;

	if ((fd = open(path, O_DIRECTORY | O_RDONLY)) < 0 ||
	    (ino = calloc(1, sizeof(*ino))) == NULL ||
	    fstat(fd, &sb) != 0)
		goto fail;
	ino->dev = sb.st_dev;
	ino->ino = sb.st_ino;
	if (RB_FIND(inodetree, tree, ino)) {
		free(ino);
		close(fd);
		warnx("%s: Directory loop detected", path);
		*dirp = NULL;
		return (0);
	}
	if ((ret = fdscandir(fd, dirp, selectf, comparf)) < 0)
		goto fail;
	RB_INSERT(inodetree, tree, ino);
	close(fd);
	return (ret);
fail:
	serrno = errno;
	if (ino != NULL)
		free(ino);
	if (fd >= 0)
		close(fd);
	errno = serrno;
	return (-1);
}

/*
 * Diff directory traversal. Will be called recursively if -r was specified.
 */
void
diffdir(char *p1, char *p2, int flags)
{
	struct dirent *dent1, **dp1, **edp1, **dirp1 = NULL;
	struct dirent *dent2, **dp2, **edp2, **dirp2 = NULL;
	size_t dirlen1, dirlen2;
	char path1[PATH_MAX], path2[PATH_MAX];
	int pos;

	edp1 = edp2 = NULL;

	dirlen1 = strlcpy(path1, *p1 ? p1 : ".", sizeof(path1));
	if (dirlen1 >= sizeof(path1) - 1) {
		warnc(ENAMETOOLONG, "%s", p1);
		status |= 2;
		return;
	}
	while (dirlen1 > 1 && path1[dirlen1 - 1] == '/')
		path1[--dirlen1] = '\0';
	dirlen2 = strlcpy(path2, *p2 ? p2 : ".", sizeof(path2));
	if (dirlen2 >= sizeof(path2) - 1) {
		warnc(ENAMETOOLONG, "%s", p2);
		status |= 2;
		return;
	}
	while (dirlen2 > 1 && path2[dirlen2 - 1] == '/')
		path2[--dirlen2] = '\0';

	/*
	 * Get a list of entries in each directory, skipping "excluded" files
	 * and sorting alphabetically.
	 */
	pos = vscandir(&v1, path1, &dirp1, selectfile, alphasort);
	if (pos == -1) {
		if (errno == ENOENT && (Nflag || Pflag)) {
			pos = 0;
		} else {
			warn("%s", path1);
			goto closem;
		}
	}
	dp1 = dirp1;
	edp1 = dirp1 + pos;

	pos = vscandir(&v2, path2, &dirp2, selectfile, alphasort);
	if (pos == -1) {
		if (errno == ENOENT && Nflag) {
			pos = 0;
		} else {
			warn("%s", path2);
			goto closem;
		}
	}
	dp2 = dirp2;
	edp2 = dirp2 + pos;

	/*
	 * If we were given a starting point, find it.
	 */
	if (start != NULL) {
		while (dp1 != edp1 && strcmp((*dp1)->d_name, start) < 0)
			dp1++;
		while (dp2 != edp2 && strcmp((*dp2)->d_name, start) < 0)
			dp2++;
	}

	/*
	 * Append separator so children's names can be appended directly.
	 */
	if (path1[dirlen1 - 1] != '/') {
		path1[dirlen1++] = '/';
		path1[dirlen1] = '\0';
	}
	if (path2[dirlen2 - 1] != '/') {
		path2[dirlen2++] = '/';
		path2[dirlen2] = '\0';
	}

	/*
	 * Iterate through the two directory lists, diffing as we go.
	 */
	while (dp1 != edp1 || dp2 != edp2) {
		dent1 = dp1 != edp1 ? *dp1 : NULL;
		dent2 = dp2 != edp2 ? *dp2 : NULL;

		pos = dent1 == NULL ? 1 : dent2 == NULL ? -1 :
		    ignore_file_case ? strcasecmp(dent1->d_name, dent2->d_name) :
		    strcmp(dent1->d_name, dent2->d_name) ;
		if (pos == 0) {
			/* file exists in both dirs, diff it */
			diffit(dent1, path1, dirlen1, dent2, path2, dirlen2, flags);
			dp1++;
			dp2++;
		} else if (pos < 0) {
			/* file only in first dir, only diff if -N */
			if (Nflag) {
				diffit(dent1, path1, dirlen1, dent2, path2,
					dirlen2, flags);
			} else {
				print_only(path1, dirlen1, dent1->d_name);
				status |= 1;
			}
			dp1++;
		} else {
			/* file only in second dir, only diff if -N or -P */
			if (Nflag || Pflag)
				diffit(dent2, path1, dirlen1, dent1, path2,
					dirlen2, flags);
			else {
				print_only(path2, dirlen2, dent2->d_name);
				status |= 1;
			}
			dp2++;
		}
	}

closem:
	if (dirp1 != NULL) {
		for (dp1 = dirp1; dp1 < edp1; dp1++)
			free(*dp1);
		free(dirp1);
	}
	if (dirp2 != NULL) {
		for (dp2 = dirp2; dp2 < edp2; dp2++)
			free(*dp2);
		free(dirp2);
	}
}

/*
 * Do the actual diff by calling either diffreg() or diffdir().
 */
static void
diffit(struct dirent *dp, char *path1, size_t plen1, struct dirent *dp2,
	char *path2, size_t plen2, int flags)
{
	flags |= D_HEADER;
	strlcpy(path1 + plen1, dp->d_name, PATH_MAX - plen1);

	/*
	 * If we are ignoring file case, use dent2s name here if both names are
	 * the same apart from case.
	 */
	if (ignore_file_case && strcasecmp(dp2->d_name, dp2->d_name) == 0)
		strlcpy(path2 + plen2, dp2->d_name, PATH_MAX - plen2);
	else
		strlcpy(path2 + plen2, dp->d_name, PATH_MAX - plen2);

	if (noderef) {
		if (lstat(path1, &stb1) != 0) {
			if (!(Nflag || Pflag) || errno != ENOENT) {
				warn("%s", path1);
				return;
			}
			flags |= D_EMPTY1;
			memset(&stb1, 0, sizeof(stb1));
		}

		if (lstat(path2, &stb2) != 0) {
			if (!Nflag || errno != ENOENT) {
				warn("%s", path2);
				return;
			}
			flags |= D_EMPTY2;
			memset(&stb2, 0, sizeof(stb2));
			stb2.st_mode = stb1.st_mode;
		}
		if (stb1.st_mode == 0)
			stb1.st_mode = stb2.st_mode;
		if (S_ISLNK(stb1.st_mode) || S_ISLNK(stb2.st_mode)) {
			if  (S_ISLNK(stb1.st_mode) && S_ISLNK(stb2.st_mode)) {
				char buf1[PATH_MAX];
				char buf2[PATH_MAX];
				ssize_t len1 = 0;
				ssize_t len2 = 0;

				len1 = readlink(path1, buf1, sizeof(buf1));
				len2 = readlink(path2, buf2, sizeof(buf2));

				if (len1 < 0 || len2 < 0) {
					perror("reading links");
					return;
				}
				buf1[len1] = '\0';
				buf2[len2] = '\0';

				if (len1 != len2 || strncmp(buf1, buf2, len1) != 0) {
					printf("Symbolic links %s and %s differ\n",
						path1, path2);
					status |= 1;
				}

				return;
			}

			printf("File %s is a %s while file %s is a %s\n",
				path1, S_ISLNK(stb1.st_mode) ? "symbolic link" :
					(S_ISDIR(stb1.st_mode) ? "directory" :
					(S_ISREG(stb1.st_mode) ? "file" : "error")),
				path2, S_ISLNK(stb2.st_mode) ? "symbolic link" :
					(S_ISDIR(stb2.st_mode) ? "directory" :
					(S_ISREG(stb2.st_mode) ? "file" : "error")));
			status |= 1;
			return;
		}
	} else {
		if (stat(path1, &stb1) != 0) {
			if (!(Nflag || Pflag) || errno != ENOENT) {
				warn("%s", path1);
				return;
			}
			flags |= D_EMPTY1;
			memset(&stb1, 0, sizeof(stb1));
		}

		if (stat(path2, &stb2) != 0) {
			if (!Nflag || errno != ENOENT) {
				warn("%s", path2);
				return;
			}
			flags |= D_EMPTY2;
			memset(&stb2, 0, sizeof(stb2));
			stb2.st_mode = stb1.st_mode;
		}
		if (stb1.st_mode == 0)
			stb1.st_mode = stb2.st_mode;
	}
	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		if (rflag)
			diffdir(path1, path2, flags);
		else
			printf("Common subdirectories: %s and %s\n",
			    path1, path2);
		return;
	}
	if (!S_ISREG(stb1.st_mode) && !S_ISDIR(stb1.st_mode))
		dp->d_status = D_SKIPPED1;
	else if (!S_ISREG(stb2.st_mode) && !S_ISDIR(stb2.st_mode))
		dp->d_status = D_SKIPPED2;
	else
		dp->d_status = diffreg(path1, path2, flags, 0);
	print_status(dp->d_status, path1, path2, "");
}

/*
 * Returns 1 if the directory entry should be included in the
 * diff, else 0.  Checks the excludes list.
 */
static int
selectfile(const struct dirent *dp)
{
	struct excludes *excl;

	if (dp->d_fileno == 0)
		return (0);

	/* always skip "." and ".." */
	if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' ||
	    (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
		return (0);

	/* check excludes list */
	for (excl = excludes_list; excl != NULL; excl = excl->next)
		if (fnmatch(excl->pattern, dp->d_name, FNM_PATHNAME) == 0)
			return (0);

	return (1);
}

void
print_only(const char *path, size_t dirlen, const char *entry)
{
	if (dirlen > 1)
		dirlen--;
	printf("Only in %.*s: %s\n", (int)dirlen, path, entry);
}
