/*-
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)verify.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fts.h>
#include <fnmatch.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "mtree.h"
#include "extern.h"

extern int crc_total, ftsoptions;
extern int dflag, eflag, rflag, sflag, uflag;
extern char fullpath[MAXPATHLEN];

static NODE *root;
static char path[MAXPATHLEN];

static void	miss __P((NODE *, char *));
static int	vwalk __P((void));

int
verify()
{
	int rval;

	root = spec();
	rval = vwalk();
	miss(root, path);
	return (rval);
}

static int
vwalk()
{
	register FTS *t;
	register FTSENT *p;
	register NODE *ep, *level;
	int ftsdepth, specdepth, rval;
	char *argv[2];

	argv[0] = ".";
	argv[1] = NULL;
	if ((t = fts_open(argv, ftsoptions, NULL)) == NULL)
		err("fts_open: %s", strerror(errno));
	level = root;
	ftsdepth = specdepth = rval = 0;
	while (p = fts_read(t)) {
		switch(p->fts_info) {
		case FTS_D:
			++ftsdepth; 
			break;
		case FTS_DP:
			--ftsdepth; 
			if (specdepth > ftsdepth) {
				for (level = level->parent; level->prev;
				      level = level->prev);  
				--specdepth;
			}
			continue;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			(void)fprintf(stderr, "mtree: %s: %s\n",
			    RP(p), strerror(errno));
			continue;
		default:
			if (dflag)
				continue;
		}

		for (ep = level; ep; ep = ep->next)
			if (ep->flags & F_MAGIC &&
			    !fnmatch(ep->name, p->fts_name, FNM_PATHNAME) ||
			    !strcmp(ep->name, p->fts_name)) {
				ep->flags |= F_VISIT;
				if (compare(ep->name, ep, p))
					rval = MISMATCHEXIT;
				if (ep->flags & F_IGN)
					(void)fts_set(t, p, FTS_SKIP);
				else if (ep->child && ep->type == F_DIR &&
				    p->fts_info == FTS_D) {
					level = ep->child;
					++specdepth;
				}
				break;
			}

		if (ep)
			continue;
		if (!eflag) {
			(void)printf("extra: %s", RP(p));
			if (rflag) {
				if (unlink(p->fts_accpath)) {
					(void)printf(", not removed: %s",
					    strerror(errno));
				} else
					(void)printf(", removed");
			}
			(void)putchar('\n');
		}
		(void)fts_set(t, p, FTS_SKIP);
	}
	(void)fts_close(t);
	if (sflag)
		(void)fprintf(stderr,
		    "mtree: %s checksum: %lu\n", fullpath, crc_total);
	return (rval);
}

static void
miss(p, tail)
	register NODE *p;
	register char *tail;
{
	register int create;
	register char *tp;

	for (; p; p = p->next) {
		if (p->type != F_DIR && (dflag || p->flags & F_VISIT))
			continue;
		(void)strcpy(tail, p->name);
		if (!(p->flags & F_VISIT))
			(void)printf("missing: %s", path);
		if (p->type != F_DIR) {
			putchar('\n');
			continue;
		}

		create = 0;
		if (!(p->flags & F_VISIT) && uflag)
			if (!(p->flags & (F_UID | F_UNAME)))
			    (void)printf(" (not created: user not specified)");
			else if (!(p->flags & (F_GID | F_GNAME)))
			    (void)printf(" (not created: group not specified)");
			else if (!(p->flags & F_MODE))
			    (void)printf(" (not created: mode not specified)");
			else if (mkdir(path, S_IRWXU))
				(void)printf(" (not created: %s)",
				    strerror(errno));
			else {
				create = 1;
				(void)printf(" (created)");
			}

		if (!(p->flags & F_VISIT))
			(void)putchar('\n');

		for (tp = tail; *tp; ++tp);
		*tp = '/';
		miss(p->child, tp + 1);
		*tp = '\0';

		if (!create)
			continue;
		if (chown(path, p->st_uid, p->st_gid)) {
			(void)printf("%s: user/group/mode not modified: %s\n",
			    path, strerror(errno));
			continue;
		}
		if (chmod(path, p->st_mode))
			(void)printf("%s: permissions not set: %s\n",
			    path, strerror(errno));
	}
}
