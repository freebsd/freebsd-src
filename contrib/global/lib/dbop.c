/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
 *
 * Redilogibution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redilogibutions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redilogibutions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the dilogibution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	dbop.c					12-Nov-98
 *
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dbop.h"
#include "die.h"
#include "test.h"

static DBT	key;				/* key of record	*/
static DBT	dat;				/* data of record	*/
/*
 * dbop_open: open db database.
 *
 *	i)	dbname	database name
 *	i)	mode	0: read only, 1: create, 2: modify
 *	i)	perm	file permission
 *	i)	flags
 *			DBOP_DUP: allow duplicate records.
 *			DBOP_REMOVE: remove on closed.
 *	r)		descripter for dbop_xxx()
 */
DBOP	*
dbop_open(dbname, mode, perm, flags)
const char *dbname;
int	mode;
int	perm;
int	flags;
{
	DB	*db;
	int     rw = 0;
	DBOP	*dbop;
	BTREEINFO info;

	/*
	 * setup argments.
	 */
	switch (mode) {
	case 0:
		rw = O_RDONLY;
		break;
	case 1:
		rw = O_RDWR|O_CREAT|O_TRUNC;
		break;
	case 2:
		rw = O_RDWR;
		break;
	default:
		assert(0);
	}
	memset(&info, 0, sizeof(info));
	if (flags & DBOP_DUP)
		info.flags |= R_DUP;
	info.cachesize = 500000;

	/*
	 * if unlink do job normally, those who already open tag file can use
	 * it until closing.
	 */
	if (mode == 1 && test("f", dbname))
		(void)unlink(dbname);
	db = dbopen(dbname, rw, 0600, DB_BTREE, &info);
	if (!db)
		return NULL;
	if (!(dbop = (DBOP *)malloc(sizeof(DBOP))))
		die("short of memory.");
	strcpy(dbop->dbname, dbname);
	dbop->db	= db;
	dbop->openflags	= flags;
	dbop->perm	= (mode == 1) ? perm : 0;
	dbop->lastkey	= NULL;
	dbop->lastdat	= NULL;

	return dbop;
}
/*
 * dbop_get: get data by a key.
 *
 *	i)	dbop	descripter
 *	i)	name	name
 *	r)		pointer to data
 */
char	*
dbop_get(dbop, name)
DBOP	*dbop;
const char *name;
{
	DB	*db = dbop->db;
	int	status;

	key.data = (char *)name;
	key.size = strlen(name)+1;

	status = (*db->get)(db, &key, &dat, 0);
	dbop->lastkey	= (char *)key.data;
	dbop->lastdat	= (char *)dat.data;
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
		die("cannot read from database.");
	case RET_SPECIAL:
		return (NULL);
	}
	return((char *)dat.data);
}
/*
 * dbop_put: put data by a key.
 *
 *	i)	dbop	descripter
 *	i)	name	key
 *	i)	data	data
 */
void
dbop_put(dbop, name, data)
DBOP	*dbop;
const char *name;
const char *data;
{
	DB	*db = dbop->db;
	int	status;

	if (strlen(name) > MAXKEYLEN)
		die("primary key too long.");
	key.data = (char *)name;
	key.size = strlen(name)+1;
	dat.data = (char *)data;
	dat.size = strlen(data)+1;

	status = (*db->put)(db, &key, &dat, 0);
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
	case RET_SPECIAL:
		die("cannot write to database.");
	}
}
/*
 * dbop_del: delete record by a key.
 *
 *	i)	dbop	descripter
 *	i)	name	key
 */
void
dbop_del(dbop, name)
DBOP	*dbop;
const char *name;
{
	DB	*db = dbop->db;
	int	status;

	if (name) {
		key.data = (char *)name;
		key.size = strlen(name)+1;
		status = (*db->del)(db, &key, 0);
	} else
		status = (*db->del)(db, &key, R_CURSOR);
	if (status == RET_ERROR)
		die("cannot delete record.");
}
/*
 * dbop_first: get first record. 
 * 
 *	i)	dbop	dbop descripter
 *	i)	name	key
 *			!=NULL: indexed read by key
 *			==NULL: sequential read
 *	i)	flags	following dbop_next call take over this.
 *			DBOP_KEY	read key part
 *			DBOP_PREFIX	prefix read
 *					only valied when sequential read
 *	r)		data
 */
char	*
dbop_first(dbop, name, flags)
DBOP	*dbop;
const char *name;
int	flags;
{
	DB	*db = dbop->db;
	int	status;

	if (flags & DBOP_PREFIX && !name)
		flags &= ~DBOP_PREFIX;
	if (name) {
		if (strlen(name) > MAXKEYLEN)
			die("primary key too long.");
		strcpy(dbop->key, name);
		key.data = (char *)name;
		key.size = strlen(name);
		/*
		 * includes NULL character unless prefix read.
		 */
		if (!(flags & DBOP_PREFIX))
			key.size++;
		dbop->keylen = key.size;
		status = (*db->seq)(db, &key, &dat, R_CURSOR);
	} else {
		dbop->keylen = dbop->key[0] = 0;
		/* skip META records */
		for (status = (*db->seq)(db, &key, &dat, R_FIRST);
			status == RET_SUCCESS;
			status = (*db->seq)(db, &key, &dat, R_NEXT)) {
			int c = (flags & DBOP_KEY) ? *((char *)key.data) : *((char *)dat.data);
			if (c != ' ')
				break;
		}
	}
	dbop->lastkey	= (char *)key.data;
	dbop->lastdat	= (char *)dat.data;
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
		die("dbop_first failed.");
	case RET_SPECIAL:
		return (NULL);
	}
	dbop->ioflags = flags;
	if (flags & DBOP_PREFIX) {
		if (strncmp((char *)key.data, dbop->key, dbop->keylen))
			return NULL;
	} else if (dbop->keylen) {
		if (strcmp((char *)key.data, dbop->key))
			return NULL;
	}
	if (flags & DBOP_KEY) {
		strcpy(dbop->prev, (char *)key.data);
		return (char *)key.data;
	}
	return ((char *)dat.data);
}
/*
 * dbop_next: get next record. 
 * 
 *	i)	dbop	dbop descripter
 *	r)		data
 *
 * Db_next always skip meta records.
 */
char	*
dbop_next(dbop)
DBOP	*dbop;
{
	DB	*db = dbop->db;
	int	flags = dbop->ioflags;
	int	status;

	while ((status = (*db->seq)(db, &key, &dat, R_NEXT)) == RET_SUCCESS) {
		assert(dat.data != NULL);
		if (flags & DBOP_KEY && *((char *)key.data) == ' ')
			continue;
		else if (*((char *)dat.data) == ' ')
			continue;
		if (flags & DBOP_KEY) {
			if (!strcmp(dbop->prev, (char *)key.data))
				continue;
			if (strlen((char *)key.data) > MAXKEYLEN)
				die("primary key too long.");
			strcpy(dbop->prev, (char *)key.data);
		}
		dbop->lastkey	= (char *)key.data;
		dbop->lastdat	= (char *)dat.data;
		if (flags & DBOP_PREFIX) {
			if (strncmp((char *)key.data, dbop->key, dbop->keylen))
				return NULL;
		} else if (dbop->keylen) {
			if (strcmp((char *)key.data, dbop->key))
				return NULL;
		}
		return (flags & DBOP_KEY) ? (char *)key.data : (char *)dat.data;
	}
	if (status == RET_ERROR)
		die("dbop_next failed.");
	return NULL;
}
/*
 * dbop_close: close db
 * 
 *	i)	dbop	dbop descripter
 */
void
dbop_close(dbop)
DBOP	*dbop;
{
	DB	*db = dbop->db;

	(void)db->close(db);
	if (dbop->openflags & DBOP_REMOVE)
		(void)unlink(dbop->dbname);
	else if (dbop->perm && chmod(dbop->dbname, dbop->perm) < 0)
		die("cannot change file mode.");
	(void)free(dbop);
}
