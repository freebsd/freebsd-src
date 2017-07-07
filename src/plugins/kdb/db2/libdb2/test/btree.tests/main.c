/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <fcntl.h>
#include "db-int.h"
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"

typedef struct cmd_table {
	char *cmd;
	int nargs;
	int rconv;
	void (*func) __P((DB *, char **));
	char *usage, *descrip;
} cmd_table;

int stopstop;
DB *globaldb;

#if 0
void append	__P((DB *, char **));
#endif
#ifdef STATISTICS
void bstat	__P((DB *, char **));
#endif
void cursor	__P((DB *, char **));
void delcur	__P((DB *, char **));
void delete	__P((DB *, char **));
#ifdef DEBUG
void dump	__P((DB *, char **));
#endif
void first	__P((DB *, char **));
void get	__P((DB *, char **));
void help	__P((DB *, char **));
void iafter	__P((DB *, char **));
void ibefore	__P((DB *, char **));
void icursor	__P((DB *, char **));
void insert	__P((DB *, char **));
void keydata	__P((DBT *, DBT *));
void last	__P((DB *, char **));
void list	__P((DB *, char **));
#if 0
void load	__P((DB *, char **));
#endif
#ifdef STATISTICS
void mstat	__P((DB *, char **));
#endif
void next	__P((DB *, char **));
int  parse	__P((char *, char **, int));
void previous	__P((DB *, char **));
#ifdef DEBUG
void show	__P((DB *, char **));
#endif
void rlist	__P((DB *, char **));
void rnext	__P((DB *, char **));
void rprev	__P((DB *, char **));
void usage	__P((void));
void user	__P((DB *));
void unlinkpg	__P((DB *, char **));

cmd_table commands[] = {
	"?",	0, 0, help, "help", NULL,
#if 0
	"a",	2, 1, append, "append key def", "append key with data def",
#endif
#ifdef STATISTICS
	"b",	0, 0, bstat, "bstat", "stat btree",
#endif
	"c",	1, 1, cursor,  "cursor word", "move cursor to word",
	"delc",	0, 0, delcur, "delcur", "delete key the cursor references",
	"dele",	1, 1, delete, "delete word", "delete word",
#ifdef DEBUG
	"d",	0, 0, dump, "dump", "dump database",
#endif
	"f",	0, 0, first, "first", "move cursor to first record",
	"g",	1, 1, get, "get key", "locate key",
	"h",	0, 0, help, "help", "print command summary",
	"ia",	2, 1, iafter, "iafter key data", "insert data after key",
	"ib",	2, 1, ibefore, "ibefore key data", "insert data before key",
	"ic",	2, 1, icursor, "icursor key data", "replace cursor",
	"in",	2, 1, insert, "insert key def", "insert key with data def",
	"la",	0, 0, last, "last", "move cursor to last record",
	"li",	1, 1, list, "list file", "list to a file",
#if 0
	"loa",	1, 0, load, "load file", NULL,
#endif
	"loc",	1, 1, get, "get key", NULL,
#ifdef STATISTICS
	"m",	0, 0, mstat, "mstat", "stat memory pool",
#endif
	"n",	0, 0, next, "next", "move cursor forward one record",
	"p",	0, 0, previous, "previous", "move cursor back one record",
	"q",	0, 0, NULL, "quit", "quit",
	"rli",	1, 1, rlist, "rlist file", "list to a file (recursive)",
	"rn",	0, 0, rnext, "rnext", "move cursor forward one record (recursive)",
	"rp",	0, 0, rprev, "rprev", "move cursor back one record (recursive)",
#ifdef DEBUG
	"sh",	1, 0, show, "show page", "dump a page",
#endif
	"u",	1, 0, unlinkpg, "unlink pgno|internal|leaf", "unlink a page",

	{ NULL },
};

int recno;					/* use record numbers */
char *dict = "words";				/* default dictionary */
char *progname;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	int omode;
	DB *db;
	BTREEINFO b;

	progname = *argv;

	omode = O_RDONLY;
	b.flags = 0;
	b.cachesize = 0;
	b.maxkeypage = 0;
	b.minkeypage = 0;
	b.psize = 0;
	b.compare = NULL;
	b.prefix = NULL;
	b.lorder = 0;

	while ((c = getopt(argc, argv, "bc:di:lp:ruw")) != -1) {
		switch (c) {
		case 'b':
			b.lorder = DB_BIG_ENDIAN;
			break;
		case 'c':
			b.cachesize = atoi(optarg);
			break;
		case 'd':
			b.flags |= R_DUP;
			break;
		case 'i':
			dict = optarg;
			break;
		case 'l':
			b.lorder = DB_LITTLE_ENDIAN;
			break;
		case 'p':
			b.psize = atoi(optarg);
			break;
		case 'r':
			recno = 1;
			break;
		case 'u':
			b.flags = 0;
			break;
		case 'w':
			omode = O_RDWR;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (recno)
		db = dbopen(*argv == NULL ? NULL : *argv, omode|O_BINARY,
		    0, DB_RECNO, NULL);
	else
		db = dbopen(*argv == NULL ? NULL : *argv, O_CREAT|omode|O_BINARY,
		    0600, DB_BTREE, &b);

	if (db == NULL) {
		(void)fprintf(stderr, "dbopen: %s\n", strerror(errno));
		exit(1);
	}
	globaldb = db;
	user(db);
	exit(0);
	/* NOTREACHED */
}

void
user(db)
	DB *db;
{
	FILE *ifp;
	int argc, i, last;
	char *lbuf, *argv[4], buf[512];

	if ((ifp = fopen("/dev/tty", "r")) == NULL) {
		(void)fprintf(stderr,
		    "/dev/tty: %s\n", strerror(errno));
		exit(1);
	}
	for (last = 0;;) {
		(void)printf("> ");
		(void)fflush(stdout);
		if ((lbuf = fgets(&buf[0], 512, ifp)) == NULL) {
			(void)printf("\n");
			if (ferror(ifp) && errno == EINTR) {
				clearerr(ifp);
				continue;
			}
			break;
		}
		if (lbuf[0] == '\n') {
			i = last;
			goto uselast;
		}
		lbuf[strlen(lbuf) - 1] = '\0';

		if (lbuf[0] == 'q')
			break;

		argc = parse(lbuf, &argv[0], 3);
		if (argc == 0)
			continue;

		for (i = 0; commands[i].cmd != NULL; i++)
			if (strncmp(commands[i].cmd, argv[0],
			    strlen(commands[i].cmd)) == 0)
				break;

		if (commands[i].cmd == NULL) {
			(void)fprintf(stderr,
			    "%s: command unknown ('help' for help)\n", lbuf);
			continue;
		}

		if (commands[i].nargs != argc - 1) {
			(void)fprintf(stderr, "usage: %s\n", commands[i].usage);
			continue;
		}

		if (recno && commands[i].rconv) {
			static recno_t nlong;
			nlong = atoi(argv[1]);
			argv[1] = (char *)&nlong;
		}
uselast:	last = i;
		(*commands[i].func)(db, argv);
	}
	if ((db->sync)(db, 0) == RET_ERROR)
		perror("dbsync");
	else if ((db->close)(db) == RET_ERROR)
		perror("dbclose");
}

int
parse(lbuf, argv, maxargc)
	char *lbuf, **argv;
	int maxargc;
{
	int argc = 0;
	char *c;

	c = lbuf;
	while (isspace(*c))
		c++;
	while (*c != '\0' && argc < maxargc) {
		*argv++ = c;
		argc++;
		while (!isspace(*c) && *c != '\0') {
			c++;
		}
		while (isspace(*c))
			*c++ = '\0';
	}
	return (argc);
}

#if 0
void
append(db, argv)
	DB *db;
	char **argv;
{
	DBT key, data;
	int status;

	if (!recno) {
		(void)fprintf(stderr,
		    "append only available for recno db's.\n");
		return;
	}
	key.data = argv[1];
	key.size = sizeof(recno_t);
	data.data = argv[2];
	data.size = strlen(data.data);
	status = (db->put)(db, &key, &data, R_APPEND);
	switch (status) {
	case RET_ERROR:
		perror("append/put");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}
#endif

void
cursor(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;
	status = (*db->seq)(db, &key, &data, R_CURSOR);
	switch (status) {
	case RET_ERROR:
		perror("cursor/seq");
		break;
	case RET_SPECIAL:
		(void)printf("key not found\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
delcur(db, argv)
	DB *db;
	char **argv;
{
	int status;

	status = (*db->del)(db, NULL, R_CURSOR);

	if (status == RET_ERROR)
		perror("delcur/del");
}

void
delete(db, argv)
	DB *db;
	char **argv;
{
	DBT key;
	int status;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;

	status = (*db->del)(db, &key, 0);
	switch (status) {
	case RET_ERROR:
		perror("delete/del");
		break;
	case RET_SPECIAL:
		(void)printf("key not found\n");
		break;
	case RET_SUCCESS:
		break;
	}
}

#ifdef DEBUG
void
dump(db, argv)
	DB *db;
	char **argv;
{
	__bt_dump(db);
}
#endif

void
first(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_FIRST);

	switch (status) {
	case RET_ERROR:
		perror("first/seq");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
get(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;

	status = (*db->get)(db, &key, &data, 0);

	switch (status) {
	case RET_ERROR:
		perror("get/get");
		break;
	case RET_SPECIAL:
		(void)printf("key not found\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
help(db, argv)
	DB *db;
	char **argv;
{
	int i;

	for (i = 0; commands[i].cmd; i++)
		if (commands[i].descrip)
			(void)printf("%s: %s\n",
			    commands[i].usage, commands[i].descrip);
}

void
iafter(db, argv)
	DB *db;
	char **argv;
{
	DBT key, data;
	int status;

	if (!recno) {
		(void)fprintf(stderr,
		    "iafter only available for recno db's.\n");
		return;
	}
	key.data = argv[1];
	key.size = sizeof(recno_t);
	data.data = argv[2];
	data.size = strlen(data.data);
	status = (db->put)(db, &key, &data, R_IAFTER);
	switch (status) {
	case RET_ERROR:
		perror("iafter/put");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
ibefore(db, argv)
	DB *db;
	char **argv;
{
	DBT key, data;
	int status;

	if (!recno) {
		(void)fprintf(stderr,
		    "ibefore only available for recno db's.\n");
		return;
	}
	key.data = argv[1];
	key.size = sizeof(recno_t);
	data.data = argv[2];
	data.size = strlen(data.data);
	status = (db->put)(db, &key, &data, R_IBEFORE);
	switch (status) {
	case RET_ERROR:
		perror("ibefore/put");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
icursor(db, argv)
	DB *db;
	char **argv;
{
	int status;
	DBT data, key;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;
	data.data = argv[2];
	data.size = strlen(argv[2]) + 1;

	status = (*db->put)(db, &key, &data, R_CURSOR);
	switch (status) {
	case RET_ERROR:
		perror("icursor/put");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
insert(db, argv)
	DB *db;
	char **argv;
{
	int status;
	DBT data, key;

	key.data = argv[1];
	if (recno)
		key.size = sizeof(recno_t);
	else
		key.size = strlen(argv[1]) + 1;
	data.data = argv[2];
	data.size = strlen(argv[2]) + 1;

	status = (*db->put)(db, &key, &data, R_NOOVERWRITE);
	switch (status) {
	case RET_ERROR:
		perror("insert/put");
		break;
	case RET_SPECIAL:
		(void)printf("%s (duplicate key)\n", argv[1]);
		break;
	case RET_SUCCESS:
		break;
	}
}

void
last(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_LAST);

	switch (status) {
	case RET_ERROR:
		perror("last/seq");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
list(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	FILE *fp;
	int status;

	if ((fp = fopen(argv[1], "w")) == NULL) {
		(void)fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return;
	}
	status = (*db->seq)(db, &key, &data, R_FIRST);
	while (status == RET_SUCCESS) {
		(void)fprintf(fp, "%.*s\n", (int)key.size, key.data);
		status = (*db->seq)(db, &key, &data, R_NEXT);
	}
	(void)fclose(fp);
	if (status == RET_ERROR)
		perror("list/seq");
}

void
rlist(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	FILE *fp;
	int status;

	if ((fp = fopen(argv[1], "w")) == NULL) {
		(void)fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return;
	}
	status = (*db->seq)(db, &key, &data, R_FIRST);
	while (status == RET_SUCCESS) {
		(void)fprintf(fp, "%.*s\n", (int)key.size, key.data);
		status = (*db->seq)(db, &key, &data, R_RNEXT);
	}
	(void)fclose(fp);
	if (status == RET_ERROR)
		perror("list/seq");
}

#if 0
DB *BUGdb;
void
load(db, argv)
	DB *db;
	char **argv;
{
	register char *p, *t;
	FILE *fp;
	DBT data, key;
	recno_t cnt;
	size_t len;
	int status;
	char *lp, buf[16 * 1024];

	BUGdb = db;
	if ((fp = fopen(argv[1], "r")) == NULL) {
		(void)fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
		return;
	}
	(void)printf("loading %s...\n", argv[1]);

	for (cnt = 1; (lp = fgetline(fp, &len)) != NULL; ++cnt) {
		if (recno) {
			key.data = &cnt;
			key.size = sizeof(recno_t);
			data.data = lp;
			data.size = len + 1;
		} else {
			key.data = lp;
			key.size = len + 1;
			for (p = lp + len - 1, t = buf; p >= lp; *t++ = *p--);
			*t = '\0';
			data.data = buf;
			data.size = len + 1;
		}

		status = (*db->put)(db, &key, &data, R_NOOVERWRITE);
		switch (status) {
		case RET_ERROR:
			perror("load/put");
			exit(1);
		case RET_SPECIAL:
			if (recno)
				(void)fprintf(stderr,
				    "duplicate: %ld {%s}\n", cnt, data.data);
			else
				(void)fprintf(stderr,
				    "duplicate: %ld {%s}\n", cnt, key.data);
			exit(1);
		case RET_SUCCESS:
			break;
		}
	}
	(void)fclose(fp);
}
#endif

void
next(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_NEXT);

	switch (status) {
	case RET_ERROR:
		perror("next/seq");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
previous(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_PREV);

	switch (status) {
	case RET_ERROR:
		perror("previous/seq");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
rnext(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_RNEXT);

	switch (status) {
	case RET_ERROR:
		perror("rnext/seq");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

void
rprev(db, argv)
	DB *db;
	char **argv;
{
	DBT data, key;
	int status;

	status = (*db->seq)(db, &key, &data, R_RPREV);

	switch (status) {
	case RET_ERROR:
		perror("rprev/seq");
		break;
	case RET_SPECIAL:
		(void)printf("no more keys\n");
		break;
	case RET_SUCCESS:
		keydata(&key, &data);
		break;
	}
}

#ifdef DEBUG
void
show(db, argv)
	DB *db;
	char **argv;
{
	BTREE *t;
	PAGE *h;
	db_pgno_t pg;

	pg = atoi(argv[1]);
	t = db->internal;
	if ((h = mpool_get(t->bt_mp, pg, MPOOL_IGNOREPIN)) == NULL) {
		(void)printf("getpage of %ld failed\n", pg);
		return;
	}
	if (pg == 0)
		__bt_dmpage(h);
	else
		__bt_dpage(db, h);
}
#endif

#ifdef STATISTICS
void
bstat(db, argv)
	DB *db;
	char **argv;
{
	(void)printf("BTREE\n");
	__bt_stat(db);
}

void
mstat(db, argv)
	DB *db;
	char **argv;
{
	(void)printf("MPOOL\n");
	mpool_stat(((BTREE *)db->internal)->bt_mp);
}
#endif

void
keydata(key, data)
	DBT *key, *data;
{
	if (!recno && key->size > 0)
		(void)printf("%.*s/", (int)key->size, key->data);
	if (data->size > 0)
		(void)printf("%.*s", (int)data->size, data->data);
	(void)printf("\n");
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-bdluw] [-c cache] [-i file] [-p page] [file]\n",
	    progname);
	exit (1);
}

/* Find a candidate page to unlink. */
static PAGE *
candidatepg(BTREE *t, char *arg)
{
	PAGE *h = NULL;
	db_pgno_t pg;
	u_int32_t sflags;

	if (arg[0] == 'i')
		sflags = P_BINTERNAL | P_RINTERNAL;
	if (arg[0] == 'l')
		sflags = P_BLEAF | P_RLEAF;
	for (pg = P_ROOT; pg < t->bt_mp->npages;
	     mpool_put(t->bt_mp, h, 0), pg++) {
		if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
			return h;
		/* Look for a nonempty page of the correct
		 * type that has both left and right siblings. */
		if (h->prevpg == P_INVALID || h->nextpg == P_INVALID)
			continue;
		if ((h->flags & sflags) && NEXTINDEX(h) != 0)
			break;
	}
	if (pg == t->bt_mp->npages)
		h = NULL;
	return h;
}

void
unlinkpg(DB *db, char **argv)
{
	BTREE *t = db->internal;
	PAGE *h = NULL;
	db_pgno_t pg;

	pg = atoi(argv[1]);
	if (pg == 0)
		h = candidatepg(t, argv[1]);
	else
		h = mpool_get(t->bt_mp, pg, 0);

	if (h == NULL) {
		fprintf(stderr, "unable to find appropriate page to unlink\n");
		return;
	}
	printf("chain %d <- %d -> %d\n", h->prevpg, h->pgno, h->nextpg);
	if (__bt_relink(t, h) != 0) {
		perror("unlinkpg");
		goto cleanup;
	}
	h->prevpg = P_INVALID;
	h->nextpg = P_INVALID;
cleanup:
	mpool_put(t->bt_mp, h, MPOOL_DIRTY);
}
