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
 *	$Id: yp_dblookup.c,v 1.3 1996/02/04 05:39:35 wpaul Exp $
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
#include <errno.h>
#include <paths.h>
#include "yp.h"
#include "yp_extern.h"

int ypdb_debug = 0;
int yp_errno = YP_TRUE;

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024, 	/* cachesize */
	NULL,		/* hash */
	0,		/* lorder */
};

/*
 * Open a DB database
 */
DB *yp_open_db(domain, map)
	const char *domain;
	const char *map;
{
	DB *dbp;
	char buf[1025];


	yp_errno = YP_TRUE;

	if (map[0] == '.' || strchr(map, '/')) {
		yp_errno = YP_BADARGS;
		return (NULL);
	}

	snprintf(buf, sizeof(buf), "%s/%s/%s", yp_dir, domain, map);

	dbp = dbopen(buf,O_RDONLY|O_EXCL, PERM_SECURE, DB_HASH, NULL);

	if (dbp == NULL) {
		switch(errno) {
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
	int rval;

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

	if ((dbp = yp_open_db(domain, map)) == NULL) {
		return(yp_errno);
	}

	if ((rval = (dbp->get)(dbp,key,data,0)) != 0) {
		(void)(dbp->close)(dbp);
		if (rval == 1)
			return(YP_NOKEY);
		else
			return(YP_BADDB);
	}

	(void)(dbp->close)(dbp);

	if (ypdb_debug)
		yp_error("Result of lookup: key: [%.*s] data: [%.*s]",
			 key->size, key->data, data->size, data->data);

	return(YP_TRUE);
}

int yp_first_record(dbp,key,data)
	const DB *dbp;
	DBT *key;
	DBT *data;
{
	int rval;

	if (ypdb_debug)
		yp_error("Retrieving first key in map.");

	if ((rval = (dbp->seq)(dbp,key,data,R_FIRST)) != 0) {
		if (rval == 1)
			return(YP_NOKEY);
		else 
			return(YP_BADDB);
	}

	/* Avoid passing back magic "YP_*" records. */
	while (!strncmp(key->data, "YP_", 3)) {
		if ((rval = (dbp->seq)(dbp,key,data,R_NEXT)) != 0) {
			if (rval == 1)
				return(YP_NOKEY);
			else
				return(YP_BADDB);
		}
	}

	if (ypdb_debug)
		yp_error("Result of lookup: key: [%.*s] data: [%.*s]",
			 key->size, key->data, data->size, data->data);

	return(YP_TRUE);
}

int yp_next_record(dbp,key,data,all)
	const DB *dbp;
	DBT *key;
	DBT *data;
	int all;
{
	DBT lkey, ldata;
	int rval;

	if (key == NULL || key->data == NULL) {
		rval = yp_first_record(dbp,key,data);
		if (rval == YP_NOKEY)
			return(YP_NOMORE);
		else
			return(rval);
	}

	if (ypdb_debug)
		yp_error("Retreiving next key, previous was: [%.*s]",
			  key->size, key->data);

	if (!all) {
		(dbp->seq)(dbp,&lkey,&ldata,R_FIRST);
		while(strncmp((char *)key->data,lkey.data,(int)key->size) ||
		      key->size != lkey.size)
			(dbp->seq)(dbp,&lkey,&ldata,R_NEXT);
	}

	if ((dbp->seq)(dbp,&lkey,&ldata,R_NEXT))
		return(YP_NOMORE);

	/* Avoid passing back magic "YP_*" records. */
	while (!strncmp(lkey.data, "YP_", 3))
		if ((dbp->seq)(dbp,&lkey,&ldata,R_NEXT))
			return(YP_NOMORE);

	if ((dbp->get)(dbp,&lkey,&ldata,0))
		return(YP_FALSE);

	*key = lkey;
	*data = ldata;

	if (ypdb_debug)
		yp_error("Result of lookup: key: [%.*s] data: [%.*s]",
			 key->size, key->data, data->size, data->data);

	return(YP_TRUE);
}
