/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *	dbio.c					14-Dec-97
 *
 */
#include <stdlib.h>
#include <fcntl.h>
#include "dbio.h"
#include "die.h"

DBT	key;					/* key of record	*/
DBT	dat;					/* data of record	*/
/*
 * db_open: open db database.
 *
 *	i)	dbname	database name
 *	i)	mode	0: read only, 1: write only, 2: read & write
 *	i)	perm	file permission
 *	i)	flags
 *			DBIO_DUP: allow duplicate records.
 *			DBIO_REMOVE: remove on closed.
 *	r)		descripter for db_xxx()
 *
 * db_open leaves database permission 0600. please chmod(2) to make public.
 */
DBIO	*
db_open(dbname, mode, perm, flags)
char	*dbname;
int	mode;
int	perm;
int	flags;
{
	DB	*db;
	int     rw;
	BTREEINFO info;
	DBIO	*dbio;

	/*
	 * setup argments.
	 */
	if (mode == 0)
		rw = O_RDONLY;
	else if (mode == 1)
		rw = O_RDWR|O_CREAT|O_TRUNC;
	else if (mode == 2)
		rw = O_RDWR;
	else
		die("db_open illegal mode.");
	info.flags = (flags & DBIO_DUP) ? R_DUP : 0;
	info.cachesize = 500000;
	info.maxkeypage = 0;
	info.minkeypage = 0;
	info.psize = 0;
	info.compare = NULL;
	info.prefix = NULL;
	info.lorder = LITTLE_ENDIAN;

	/*
	 * if unlink do job normally, those who already open tag file can use
	 * it until closing.
	 */
	if (mode == 1 && test("f", dbname))
		(void)unlink(dbname);
	db = dbopen(dbname, rw, 0600, DB_BTREE, &info);
	if (!db)
		die1("db_open failed (dbname = %s).", dbname);
	if (!(dbio = (DBIO *)malloc(sizeof(DBIO))))
		die("short of memory.");
	strcpy(dbio->dbname, dbname);
	dbio->db	= db;
	dbio->openflags	= flags;
	dbio->perm	= (mode == 1) ? perm : 0;
	dbio->lastkey	= (char *)0;
	dbio->lastdat	= (char *)0;

	return dbio;
}
/*
 * db_get: get data by a key.
 *
 *	i)	dbio	descripter
 *	i)	k	key
 *	r)		pointer to data
 */
char	*
db_get(dbio, k)
DBIO	*dbio;
char	*k;
{
	DB	*db = dbio->db;
	int	status;

	key.data = k;
	key.size = strlen(k)+1;

	status = (*db->get)(db, &key, &dat, 0);
	dbio->lastkey	= (char *)key.data;
	dbio->lastdat	= (char *)dat.data;
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
		die("db_get failed.");
	case RET_SPECIAL:
		return((char *)0);
	}
	return((char *)dat.data);
}
/*
 * db_put: put data by a key.
 *
 *	i)	dbio	descripter
 *	i)	k	key
 *	i)	d	data
 */
void
db_put(dbio, k, d)
DBIO	*dbio;
char	*k;
char	*d;
{
	DB	*db = dbio->db;
	int	status;

	if (strlen(k) > MAXKEYLEN)
		die("primary key too long.");
	key.data = k;
	key.size = strlen(k)+1;
	dat.data = d;
	dat.size = strlen(d)+1;

	status = (*db->put)(db, &key, &dat, 0);
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
	case RET_SPECIAL:
		die("db_put failed.");
	}
}
/*
 * db_del: delete record by a key.
 *
 *	i)	dbio	descripter
 *	i)	k	key
 */
void
db_del(dbio, k)
DBIO	*dbio;
char	*k;
{
	DB	*db = dbio->db;
	int	status;

	if (k) {
		key.data = k;
		key.size = strlen(k)+1;
		status = (*db->del)(db, &key, 0);
	} else
		status = (*db->del)(db, &key, R_CURSOR);
	if (status == RET_ERROR)
		die("db_del failed.");
}
/*
 * db_first: get first record. 
 * 
 *	i)	dbio	dbio descripter
 *	i)	k	key
 *			!=NULL: indexed read by key
 *			==NULL: sequential read
 *	i)	flags	following db_next call take over this.
 *			DBIO_KEY	read key part
 *			DBIO_PREFIX	prefix read
 *			DBIO_SKIPMETA	skip META record
 *					only valied when sequential read
 *	r)		data
 */
char	*
db_first(dbio, k, flags)
DBIO	*dbio;
char	*k;
int	flags;
{
	DB	*db = dbio->db;
	int	status;

	if (flags & DBIO_PREFIX && !k)
		flags &= ~DBIO_PREFIX;
	if (flags & DBIO_SKIPMETA && k)
		flags &= ~DBIO_SKIPMETA;
	if (k) {
		if (strlen(k) > MAXKEYLEN)
			die("primary key too long.");
		strcpy(dbio->key, k);
		key.data = k;
		key.size = strlen(k);
		/*
		 * includes NULL character unless prefix read.
		 */
		if (!(flags & DBIO_PREFIX))
			key.size++;
		dbio->keylen = key.size;
		status = (*db->seq)(db, &key, &dat, R_CURSOR);
	} else {
		dbio->keylen = dbio->key[0] = 0;
		for (status = (*db->seq)(db, &key, &dat, R_FIRST);
			status == RET_SUCCESS	&&
			flags & DBIO_SKIPMETA	&&
			*((char *)dat.data) == ' ';
			status = (*db->seq)(db, &key, &dat, R_NEXT))
			;
	}
	dbio->lastkey	= (char *)key.data;
	dbio->lastdat	= (char *)dat.data;
	switch (status) {
	case RET_SUCCESS:
		break;
	case RET_ERROR:
		die("db_first failed.");
	case RET_SPECIAL:
		return ((char *)0);
	}
	dbio->ioflags = flags;
	if (flags & DBIO_PREFIX) {
		if (strncmp((char *)key.data, dbio->key, dbio->keylen))
			return (char *)0;
	} else if (dbio->keylen) {
		if (strcmp((char *)key.data, dbio->key))
			return (char *)0;
	}
	if (flags & DBIO_KEY) {
		strcpy(dbio->prev, (char *)key.data);
		return (char *)key.data;
	}
	return ((char *)dat.data);
}
/*
 * db_next: get next record. 
 * 
 *	i)	dbio	dbio descripter
 *	r)		data
 */
char	*
db_next(dbio)
DBIO	*dbio;
{
	DB	*db = dbio->db;
	int	flags = dbio->ioflags;
	int	status;

	while ((status = (*db->seq)(db, &key, &dat, R_NEXT)) == RET_SUCCESS) {
		if (flags & DBIO_SKIPMETA) {
			if (*((char *)dat.data) == ' ')
				continue;
		}
		if (flags & DBIO_KEY) {
			if (!strcmp(dbio->prev, (char *)key.data))
				continue;
			if (strlen((char *)key.data) > MAXKEYLEN)
				die("primary key too long.");
			strcpy(dbio->prev, (char *)key.data);
		}
		dbio->lastkey	= (char *)key.data;
		dbio->lastdat	= (char *)dat.data;
		if (flags & DBIO_PREFIX) {
			if (strncmp((char *)key.data, dbio->key, dbio->keylen))
				return (char *)0;
		} else if (dbio->keylen) {
			if (strcmp((char *)key.data, dbio->key))
				return (char *)0;
		}
		return (flags & DBIO_KEY) ? (char *)key.data : (char *)dat.data;
	}
	if (status == RET_ERROR)
		die("db_next failed.");
	return (char *)0;
}
/*
 * db_close: close db
 * 
 *	i)	dbio	dbio descripter
 */
void
db_close(dbio)
DBIO	*dbio;
{
	DB	*db = dbio->db;
	(void)db->close(db);
	if (dbio->openflags & DBIO_REMOVE)
		(void)unlink(dbio->dbname);
	else if (dbio->perm && chmod(dbio->dbname, dbio->perm) < 0)
		die("cannot change file mode.");
	(void)free(dbio);
}
