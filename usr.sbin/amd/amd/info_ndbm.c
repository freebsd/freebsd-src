/*
 * Copyright (c) 1989 Jan-Simon Pendry
 * Copyright (c) 1989 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *
 *	@(#)info_ndbm.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

/*
 * Get info from NDBM map
 */

#include "am.h"

#ifdef HAS_NDBM_MAPS

#include <ndbm.h>
#include <fcntl.h>
#include <sys/stat.h>

static int search_ndbm P((DBM *db, char *key, char **val));
static int search_ndbm(db, key, val)
DBM *db;
char *key;
char **val;
{
	datum k, v;
	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v = dbm_fetch(db, k);
	if (v.dptr) {
		*val = strdup(v.dptr);
		return 0;
	}
	return ENOENT;
}

int ndbm_search P((mnt_map *m, char *map, char *key, char **pval, time_t *tp));
int ndbm_search(m, map, key, pval, tp)
mnt_map *m;
char *map;
char *key;
char **pval;
time_t *tp;
{
	DBM *db;

	db = dbm_open(map, O_RDONLY, 0);
	if (db) {
		struct stat stb;
		int error;
		error = fstat(dbm_pagfno(db), &stb);
		if (!error && *tp < stb.st_mtime) {
			*tp = stb.st_mtime;
			error = -1;
		} else {
			error = search_ndbm(db, key, pval);
		}
		(void) dbm_close(db);
		return error;
	}

	return errno;
}

int ndbm_init P((char *map, time_t *tp));
int ndbm_init(map, tp)
char *map;
time_t *tp;
{
	DBM *db;

	db = dbm_open(map, O_RDONLY, 0);
	if (db) {
		struct stat stb;

		if (fstat(dbm_pagfno(db), &stb) < 0)
			*tp = clocktime();
		else
			*tp = stb.st_mtime;
		dbm_close(db);
		return 0;
	}

	return errno;
}

#endif /* HAS_NDBM_MAPS */
