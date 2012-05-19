/*
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)telldir.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/param.h>
#include <sys/queue.h>
#include <dirent.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "gen-private.h"
#include "telldir.h"

/*
 * The option SINGLEUSE may be defined to say that a telldir
 * cookie may be used only once before it is freed. This option
 * is used to avoid having memory usage grow without bound.
 */
#define SINGLEUSE

/*
 * return a pointer into a directory
 */
long
telldir(dirp)
	DIR *dirp;
{
	struct ddloc *lp;

	if ((lp = (struct ddloc *)malloc(sizeof(struct ddloc))) == NULL)
		return (-1);
	if (__isthreaded)
		_pthread_mutex_lock(&dirp->dd_lock);
	lp->loc_index = dirp->dd_td->td_loccnt++;
	lp->loc_seek = dirp->dd_seek;
	lp->loc_loc = dirp->dd_loc;
	LIST_INSERT_HEAD(&dirp->dd_td->td_locq, lp, loc_lqe);
	if (__isthreaded)
		_pthread_mutex_unlock(&dirp->dd_lock);
	return (lp->loc_index);
}

/*
 * seek to an entry in a directory.
 * Only values returned by "telldir" should be passed to seekdir.
 */
void
_seekdir(dirp, loc)
	DIR *dirp;
	long loc;
{
	struct ddloc *lp;
	struct dirent *dp;

	LIST_FOREACH(lp, &dirp->dd_td->td_locq, loc_lqe) {
		if (lp->loc_index == loc)
			break;
	}
	if (lp == NULL)
		return;
	if (lp->loc_loc == dirp->dd_loc && lp->loc_seek == dirp->dd_seek)
		goto found;
	(void) lseek(dirp->dd_fd, (off_t)lp->loc_seek, SEEK_SET);
	dirp->dd_seek = lp->loc_seek;
	dirp->dd_loc = 0;
	while (dirp->dd_loc < lp->loc_loc) {
		dp = _readdir_unlocked(dirp, 0);
		if (dp == NULL)
			break;
	}
found:
#ifdef SINGLEUSE
	LIST_REMOVE(lp, loc_lqe);
	free((caddr_t)lp);
#endif
}

/*
 * Reclaim memory for telldir cookies which weren't used.
 */
void
_reclaim_telldir(dirp)
	DIR *dirp;
{
	struct ddloc *lp;
	struct ddloc *templp;

	lp = LIST_FIRST(&dirp->dd_td->td_locq);
	while (lp != NULL) {
		templp = lp;
		lp = LIST_NEXT(lp, loc_lqe);
		free(templp);
	}
	LIST_INIT(&dirp->dd_td->td_locq);
}
