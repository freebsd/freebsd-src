/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
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

/*
 * Scan the directory dirname calling select to make a list of selected
 * directory entries then sort using qsort and compare routine dcomp.
 * Returns the number of entries and a pointer to a list of pointers to
 * struct dirent (through namelist). Returns -1 if there were any errors.
 */

#include "namespace.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#ifdef	I_AM_SCANDIR_B
#include "block_abi.h"
#define	SELECT(x)	CALL_BLOCK(select, x)
#ifndef __BLOCKS__
void qsort_b(void *, size_t, size_t, void *);
#endif
#else
#define	SELECT(x)	select(x)
#endif

#ifdef I_AM_SCANDIR_B
typedef DECLARE_BLOCK(int, select_block, const struct dirent *);
typedef DECLARE_BLOCK(int, dcomp_block, const struct dirent **,
    const struct dirent **);
#else
static int scandir_thunk_cmp(const void *p1, const void *p2, void *thunk);
#endif

static int
#ifdef I_AM_SCANDIR_B
scandir_b_dirp(DIR *dirp, struct dirent ***namelist, select_block select,
    dcomp_block dcomp)
#else
scandir_dirp(DIR *dirp, struct dirent ***namelist,
    int (*select)(const struct dirent *), int (*dcomp)(const struct dirent **,
    const struct dirent **))
#endif
{
	struct dirent *d, *p, **names = NULL;
	size_t arraysz, numitems;

	numitems = 0;
	arraysz = 32;	/* initial estimate of the array size */
	names = (struct dirent **)malloc(arraysz * sizeof(struct dirent *));
	if (names == NULL)
		goto fail;

	while ((d = readdir(dirp)) != NULL) {
		if (select != NULL && !SELECT(d))
			continue;	/* just selected names */
		/*
		 * Make a minimum size copy of the data
		 */
		p = (struct dirent *)malloc(_GENERIC_DIRSIZ(d));
		if (p == NULL)
			goto fail;
		p->d_fileno = d->d_fileno;
		p->d_type = d->d_type;
		p->d_reclen = d->d_reclen;
		p->d_namlen = d->d_namlen;
		bcopy(d->d_name, p->d_name, p->d_namlen + 1);
		/*
		 * Check to make sure the array has space left and
		 * realloc the maximum size.
		 */
		if (numitems >= arraysz) {
			struct dirent **names2;

			names2 = reallocarray(names, arraysz,
			    2 * sizeof(struct dirent *));
			if (names2 == NULL) {
				free(p);
				goto fail;
			}
			names = names2;
			arraysz *= 2;
		}
		names[numitems++] = p;
	}
	closedir(dirp);
	if (numitems && dcomp != NULL)
#ifdef I_AM_SCANDIR_B
		qsort_b(names, numitems, sizeof(struct dirent *), (void*)dcomp);
#else
		qsort_r(names, numitems, sizeof(struct dirent *),
		    scandir_thunk_cmp, &dcomp);
#endif
	*namelist = names;
	return (numitems);

fail:
	while (numitems > 0)
		free(names[--numitems]);
	free(names);
	closedir(dirp);
	return (-1);
}

int
#ifdef I_AM_SCANDIR_B
scandir_b(const char *dirname, struct dirent ***namelist, select_block select,
    dcomp_block dcomp)
#else
scandir(const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *), int (*dcomp)(const struct dirent **,
    const struct dirent **))
#endif
{
	DIR *dirp;

	dirp = opendir(dirname);
	if (dirp == NULL)
		return (-1);
	return (
#ifdef I_AM_SCANDIR_B
	    scandir_b_dirp
#else
	    scandir_dirp
#endif
	    (dirp, namelist, select, dcomp));
}

#ifndef I_AM_SCANDIR_B
int
scandirat(int dirfd, const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *), int (*dcomp)(const struct dirent **,
    const struct dirent **))
{
	DIR *dirp;
	int fd;

	fd = _openat(dirfd, dirname, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd == -1)
		return (-1);
	dirp = fdopendir(fd);
	if (dirp == NULL) {
		_close(fd);
		return (-1);
	}
	return (scandir_dirp(dirp, namelist, select, dcomp));
}

/*
 * Alphabetic order comparison routine for those who want it.
 * POSIX 2008 requires that alphasort() uses strcoll().
 */
int
alphasort(const struct dirent **d1, const struct dirent **d2)
{

	return (strcoll((*d1)->d_name, (*d2)->d_name));
}

int
versionsort(const struct dirent **d1, const struct dirent **d2)
{

	return (strverscmp((*d1)->d_name, (*d2)->d_name));
}

static int
scandir_thunk_cmp(const void *p1, const void *p2, void *thunk)
{
	int (*dc)(const struct dirent **, const struct dirent **);

	dc = *(int (**)(const struct dirent **, const struct dirent **))thunk;
	return (dc((const struct dirent **)p1, (const struct dirent **)p2));
}
#endif
