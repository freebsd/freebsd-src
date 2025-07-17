/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)dbm.c	8.6 (Berkeley) 11/7/95";
#endif /* LIBC_SCCS and not lint */

#include "db-int.h"

#include <sys/param.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "db-ndbm.h"
#include "db-dbm.h"
#include "hash.h"

/* If the two size fields of datum and DBMT are not equal, then
 * casting between structures will result in stack garbage being
 * transferred. Has been observed for DEC Alpha OSF, but will handle
 *  the general case.
 */

#define NEED_COPY

/*
 *
 * This package provides dbm and ndbm compatible interfaces to DB.
 * First are the DBM routines, which call the NDBM routines, and
 * the NDBM routines, which call the DB routines.
 */
static DBM *__cur_db;

static void no_open_db __P((void));

int
kdb2_dbminit(file)
	char *file;
{
	if (__cur_db != NULL)
		(void)kdb2_dbm_close(__cur_db);
	if ((__cur_db = kdb2_dbm_open(file, O_RDWR|O_BINARY, 0)) != NULL)
		return (0);
	if ((__cur_db = kdb2_dbm_open(file, O_RDONLY|O_BINARY, 0)) != NULL)
		return (0);
	return (-1);
}

datum
kdb2_fetch(key)
	datum key;
{
	datum item;

	if (__cur_db == NULL) {
		no_open_db();
		item.dptr = 0;
		item.dsize = 0;
		return (item);
	}
	return (kdb2_dbm_fetch(__cur_db, key));
}

datum
kdb2_firstkey()
{
	datum item;

	if (__cur_db == NULL) {
		no_open_db();
		item.dptr = 0;
		item.dsize = 0;
		return (item);
	}
	return (kdb2_dbm_firstkey(__cur_db));
}

datum
kdb2_nextkey(key)
	datum key;
{
	datum item;

	if (__cur_db == NULL) {
		no_open_db();
		item.dptr = 0;
		item.dsize = 0;
		return (item);
	}
	return (kdb2_dbm_nextkey(__cur_db));
}

int
kdb2_delete(key)
	datum key;
{
	if (__cur_db == NULL) {
		no_open_db();
		return (-1);
	}
	return (kdb2_dbm_delete(__cur_db, key));
}

int
kdb2_store(key, dat)
	datum key, dat;
{
	if (__cur_db == NULL) {
		no_open_db();
		return (-1);
	}
	return (kdb2_dbm_store(__cur_db, key, dat, DBM_REPLACE));
}

static void
no_open_db()
{
	(void)fprintf(stderr, "dbm: no open database.\n");
}

/*
 * Returns:
 * 	*DBM on success
 *	 NULL on failure
 */
DBM *
kdb2_dbm_open(file, flags, mode)
	const char *file;
	int flags, mode;
{
	HASHINFO info;
	char path[MAXPATHLEN];

	info.bsize = 4096;
	info.ffactor = 40;
	info.nelem = 1;
	info.cachesize = 0;
	info.hash = NULL;
	info.lorder = 0;
	(void)strncpy(path, file, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	(void)strncat(path, DBM_SUFFIX, sizeof(path) - 1 - strlen(path));
	return ((DBM *)__hash_open(path, flags, mode, &info, 0));
}

/*
 * Returns:
 *	Nothing.
 */
void
kdb2_dbm_close(db)
	DBM *db;
{
	(void)(db->close)(db);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
kdb2_dbm_fetch(db, key)
	DBM *db;
	datum key;
{
	datum retval;
	int status;

#ifdef NEED_COPY
	DBT k, r;

	k.data = key.dptr;
	k.size = key.dsize;
	status = (db->get)(db, &k, &r, 0);
	retval.dptr = r.data;
	retval.dsize = r.size;
#else
	status = (db->get)(db, (DBT *)&key, (DBT *)&retval, 0);
#endif
	if (status) {
		retval.dptr = NULL;
		retval.dsize = 0;
	}
	return (retval);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
kdb2_dbm_firstkey(db)
	DBM *db;
{
	int status;
	datum retkey;

#ifdef NEED_COPY
	DBT k, r;

	status = (db->seq)(db, &k, &r, R_FIRST);
	retkey.dptr = k.data;
	retkey.dsize = k.size;
#else
	datum retdata;

	status = (db->seq)(db, (DBT *)&retkey, (DBT *)&retdata, R_FIRST);
#endif
	if (status)
		retkey.dptr = NULL;
	return (retkey);
}

/*
 * Returns:
 *	DATUM on success
 *	NULL on failure
 */
datum
kdb2_dbm_nextkey(db)
	DBM *db;
{
	int status;
	datum retkey;

#ifdef NEED_COPY
	DBT k, r;

	status = (db->seq)(db, &k, &r, R_NEXT);
	retkey.dptr = k.data;
	retkey.dsize = k.size;
#else
	datum retdata;

	status = (db->seq)(db, (DBT *)&retkey, (DBT *)&retdata, R_NEXT);
#endif
	if (status)
		retkey.dptr = NULL;
	return (retkey);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 */
int
kdb2_dbm_delete(db, key)
	DBM *db;
	datum key;
{
	int status;

#ifdef NEED_COPY
	DBT k;

	k.data = key.dptr;
	k.size = key.dsize;
	status = (db->del)(db, &k, 0);
#else
	status = (db->del)(db, (DBT *)&key, 0);
#endif
	if (status)
		return (-1);
	else
		return (0);
}

/*
 * Returns:
 *	 0 on success
 *	<0 failure
 *	 1 if DBM_INSERT and entry exists
 */
int
kdb2_dbm_store(db, key, content, flags)
	DBM *db;
	datum key, content;
	int flags;
{
#ifdef NEED_COPY
	DBT k, c;

	k.data = key.dptr;
	k.size = key.dsize;
	c.data = content.dptr;
	c.size = content.dsize;
	return ((db->put)(db, &k, &c,
	    (flags == DBM_INSERT) ? R_NOOVERWRITE : 0));
#else
	return ((db->put)(db, (DBT *)&key, (DBT *)&content,
	    (flags == DBM_INSERT) ? R_NOOVERWRITE : 0));
#endif
}

int
kdb2_dbm_error(db)
	DBM *db;
{
	HTAB *hp;

	hp = (HTAB *)db->internal;
	return (hp->local_errno);
}

int
kdb2_dbm_clearerr(db)
	DBM *db;
{
	HTAB *hp;

	hp = (HTAB *)db->internal;
	hp->local_errno = 0;
	return (0);
}

int
kdb2_dbm_dirfno(db)
	DBM *db;
{
	return(((HTAB *)db->internal)->fp);
}
