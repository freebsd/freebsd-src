/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yp_dblookup.c,v 1.7 1996/04/27 17:50:18 wpaul Exp wpaul $
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <db.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <paths.h>
#include <rpcsvc/yp.h>
#include "yp_extern.h"

int ypdb_debug = 0;
int yp_errno = YP_TRUE;

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	512,		/* nelem */
	2048 * 512, 	/* cachesize */
	NULL,		/* hash */
	0,		/* lorder */
};

#ifdef DB_CACHE
#define MAXDBS 20
#define LASTDB (MAXDBS - 1)

struct dbent {
	DB *dbp;
	char *name;
	char *key;
	int size;
};

static struct dbent *dbs[MAXDBS];
static int numdbs = 0;

/*
 * Make sure all the DB entries are NULL to start with.
 */
void yp_init_dbs()
{
	register int i;

	for (i = 0; i < MAXDBS; i++);
		dbs[i] = NULL;
	return;
}

static inline void yp_flush(i)
	register int i;
{
	(void)(dbs[i]->dbp->close)(dbs[i]->dbp);
	dbs[i]->dbp = NULL;
	free(dbs[i]->name);
	dbs[i]->name = NULL;
	dbs[i]->key = NULL;
	dbs[i]->size = 0;
	free(dbs[i]);
	dbs[i] = NULL;
	numdbs--;
}

/*
 * Close all databases and erase all database names.
 * Don't free the memory allocated for each DB entry though: we
 * can just reuse it later.
 */
void yp_flush_all()
{
	register int i;

	for (i = 0; i < MAXDBS; i++) {
		if (dbs[i] != NULL && dbs[i]->dbp != NULL) {
			yp_flush(i);
		}
	}
}


/*
 * Add a DB handle and database name to the cache. We only maintain
 * fixed number of entries in the cache, so if we're asked to store
 * a new entry when all our slots are already filled, we have to kick
 * out the entry in the last slot to make room.
 */
static inline void yp_add_db(dbp, name, size)
	DB *dbp;
	char *name;
	int size;
{
	register int i;
	register struct dbent *tmp;
	static int count = 0;


	tmp = dbs[LASTDB];

	/* Rotate */
	for (i = LASTDB; i > 0; i--)
		dbs[i] = dbs[i - 1];

	dbs[0] = tmp;

	if (dbs[0]) {
		if (ypdb_debug)
			yp_error("table overflow -- releasing last slot");
		yp_flush(0);
	}

	/*
	 * Add the new entry. We allocate memory for the dbent
	 * structure if we need it. We shoudly only end up calling
	 * malloc(2) MAXDB times. Once all the slots are filled, we
	 * hold onto the memory and recycle it.
	 */
	if (dbs[0] == NULL) {
		count++;
		if (ypdb_debug)
			yp_error("allocating new DB member (%d)", count);
		dbs[0] = (struct dbent *)malloc(sizeof(struct dbent));
		bzero((char *)dbs[0], sizeof(struct dbent));
	}

	numdbs++;
	dbs[0]->dbp = dbp;
	dbs[0]->name = strdup(name);
	dbs[0]->size = size;
	return;
}

/*
 * Search the list for a database matching 'name.' If we find it,
 * move it to the head of the list and return its DB handle. If
 * not, just fail: yp_open_db_cache() will subsequently try to open
 * the database itself and call yp_add_db() to add it to the
 * list.
 *
 * The search works like this:
 *
 * - The caller specifies the name of a database to locate. We try to
 *   find an entry in our list with a matching name.
 *
 * - If the caller doesn't specify a key or size, we assume that the
 *   first entry that we encounter with a matching name is returned.
 *   This will result in matches regardless of the pointer index.
 *
 * - If the caller also specifies a key and length, we check name
 *   matches to see if their saved key indexes and lengths also match.
 *   This lets us return a DB handle that's already positioned at the
 *   correct location within a database.
 *
 * - Once we have a match, it gets migrated to the top of the list
 *   array so that it will be easier to find if another request for
 *   the same database comes in later.
 */
static inline DB *yp_find_db(name, key, size)
	char *name;
	char *key;
{
	register int i, j;
	register struct dbent *tmp;

	for (i = 0; i < numdbs; i++) {
		if (dbs[i]->name != NULL && !strcmp(dbs[i]->name, name)) {
			if (size) {
				if (size != dbs[i]->size ||
					strncmp(dbs[i]->key, key, size))
					continue;
			} else {
				if (dbs[i]->size) {
					continue;
				}
			}
			if (i > 0) {
				tmp = dbs[i];
				for (j = i; j > 0; j--)
					dbs[j] = dbs[j - 1];
				dbs[0] = tmp;
			}
			return(dbs[0]->dbp);
		}
	}
	return(NULL);
}

/*
 * Open a DB database and cache the handle for later use. We first
 * check the cache to see if the required database is already open.
 * If so, we fetch the handle from the cache. If not, we try to open
 * the database and save the handle in the cache for later use.
 */
DB *yp_open_db_cache(domain, map, key, size)
	const char *domain;
	const char *map;
	const char *key;
	const int size;
{
	DB *dbp = NULL;
	char buf[MAXPATHLEN + 2];

	snprintf(buf, sizeof(buf), "%s/%s", domain, map);

	if ((dbp = yp_find_db((char *)&buf, key, size)) != NULL) {
		return(dbp);
	} else {
		if ((dbp = yp_open_db(domain, map)) != NULL) 
			yp_add_db(dbp, (char *)&buf, size);
	}

	return (dbp);
}
#endif

/*
 * Open a DB database.
 */
DB *yp_open_db(domain, map)
	const char *domain;
	const char *map;
{
	DB *dbp = NULL;
	char buf[MAXPATHLEN + 2];

	yp_errno = YP_TRUE;

	if (map[0] == '.' || strchr(map, '/')) {
		yp_errno = YP_BADARGS;
		return (NULL);
	}

#ifdef DB_CACHE
	if (yp_validdomain(domain)) {
		yp_errno = YP_NODOM;
		return(NULL);
	}
#endif
	snprintf(buf, sizeof(buf), "%s/%s/%s", yp_dir, domain, map);

#ifdef DB_CACHE
again:
#endif
	dbp = dbopen(buf,O_RDONLY, PERM_SECURE, DB_HASH, NULL);

	if (dbp == NULL) {
		switch(errno) {
#ifdef DB_CACHE
		case ENFILE:
			/*
			 * We ran out of file descriptors. Nuke an
			 * open one and try again.
			 */
			yp_error("ran out of file descriptors");
			yp_flush(numdbs - 1);
			goto again;
			break;
#endif
		case ENOENT:
			yp_errno = YP_NOMAP;
			break;
		case EFTYPE:
			yp_errno = YP_BADDB;
			break;
		default:
			yp_errno = YP_YPERR;
			break;
		}
	}

	return (dbp);
}

/*
 * Database access routines.
 *
 * - yp_get_record(): retrieve an arbitrary key/data pair given one key
 *                 to match against.
 *
 * - yp_first_record(): retrieve first key/data base in a database.
 * 
 * - yp_next_record(): retrieve key/data pair that sequentially follows
 *                   the supplied key value in the database.
 */

int yp_get_record(domain,map,key,data,allow)
	const char *domain;
	const char *map;
	const DBT *key;
	DBT *data;
	int allow;
{
	DB *dbp;
	int rval = 0;

	if (ypdb_debug)
		yp_error("Looking up key [%.*s] in map [%s]",
			  key->size, key->data, map);

	/*
	 * Avoid passing back magic "YP_*" entries unless
	 * the caller specifically requested them by setting
	 * the 'allow' flag.
	 */
	if (!allow && !strncmp(key->data, "YP_", 3))
		return(YP_NOKEY);

#ifdef DB_CACHE
	if ((dbp = yp_open_db_cache(domain, map, NULL, 0)) == NULL) {
#else
	if ((dbp = yp_open_db(domain, map)) == NULL) {
#endif
		return(yp_errno);
	}

	if ((rval = (dbp->get)(dbp, key, data, 0)) != 0) {
#ifdef DB_CACHE
		dbs[0]->size = 0;
#else
		(void)(dbp->close)(dbp);
#endif
		if (rval == 1)
			return(YP_NOKEY);
		else
			return(YP_BADDB);
	}

	if (ypdb_debug)
		yp_error("Result of lookup: key: [%.*s] data: [%.*s]",
			 key->size, key->data, data->size, data->data);

#ifdef DB_CACHE
	if (dbs[0]->size) {
		dbs[0]->key = key->data;
		dbs[0]->size = key->size;
	}
#else
	(void)(dbp->close)(dbp);
#endif

	return(YP_TRUE);
}

int yp_first_record(dbp,key,data,allow)
	const DB *dbp;
	DBT *key;
	DBT *data;
	int allow;
{
	int rval;

	if (ypdb_debug)
		yp_error("Retrieving first key in map.");

	if ((rval = (dbp->seq)(dbp,key,data,R_FIRST)) != 0) {
#ifdef DB_CACHE
		dbs[0]->size = 0;
#endif
		if (rval == 1)
			return(YP_NOKEY);
		else 
			return(YP_BADDB);
	}

	/* Avoid passing back magic "YP_*" records. */
	while (!strncmp(key->data, "YP_", 3) && !allow) {
		if ((rval = (dbp->seq)(dbp,key,data,R_NEXT)) != 0) {
#ifdef DB_CACHE
			dbs[0]->size = 0;
#endif
			if (rval == 1)
				return(YP_NOKEY);
			else
				return(YP_BADDB);
		}
	}

	if (ypdb_debug)
		yp_error("Result of lookup: key: [%.*s] data: [%.*s]",
			 key->size, key->data, data->size, data->data);

#ifdef DB_CACHE
	if (dbs[0]->size) {
		dbs[0]->key = key->data;
		dbs[0]->size = key->size;
	}
#endif

	return(YP_TRUE);
}

int yp_next_record(dbp,key,data,all,allow)
	const DB *dbp;
	DBT *key;
	DBT *data;
	int all;
	int allow;
{
	static DBT lkey = { NULL, 0 };
	static DBT ldata = { NULL, 0 };
	int rval;

	if (key == NULL || key->data == NULL) {
		rval = yp_first_record(dbp,key,data,allow);
		if (rval == YP_NOKEY)
			return(YP_NOMORE);
		else
			return(rval);
	}

	if (ypdb_debug)
		yp_error("Retreiving next key, previous was: [%.*s]",
			  key->size, key->data);

	if (!all) {
#ifndef DB_CACHE
		if (key->size != lkey.size ||
			strncmp(key->data, lkey.data, key->size)) {
#else
		if (!dbs[0]->size) {
#endif
			(dbp->seq)(dbp,&lkey,&ldata,R_FIRST);
			while(strncmp((char *)key->data,lkey.data,
				(int)key->size) || key->size != lkey.size)
				(dbp->seq)(dbp,&lkey,&ldata,R_NEXT);
		}
	}

	if ((dbp->seq)(dbp,key,data,R_NEXT)) {
#ifdef DB_CACHE
		dbs[0]->size = 0;
#endif
		return(YP_NOMORE);
	}

	/* Avoid passing back magic "YP_*" records. */
	while (!strncmp(key->data, "YP_", 3) && !allow)
		if ((dbp->seq)(dbp,key,data,R_NEXT)) {
#ifdef DB_CACHE
			dbs[0]->size = 0;
#endif
			return(YP_NOMORE);
		}

	if (ypdb_debug)
		yp_error("Result of lookup: key: [%.*s] data: [%.*s]",
			 key->size, key->data, data->size, data->data);

#ifdef DB_CACHE
	if (dbs[0]->size) {
		dbs[0]->key = key->data;
		dbs[0]->size = key->size;
	}
#else
	lkey.data = key->data;
	lkey.size = key->size;
#endif

	return(YP_TRUE);
}
