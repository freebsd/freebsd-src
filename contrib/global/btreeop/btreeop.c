/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
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
 *	btreeop.c				6-Jul-97
 *
 */
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <db.h>
#include <fcntl.h>

char    *dbdefault = "btree";   	/* default database name */
char    *dbname;
char	buf[BUFSIZ+1];
char	out[BUFSIZ+1];

#ifndef __P
#if defined(__STDC__)
#define __P(protos)     protos
#else
#define __P(protos)     ()
#endif
#endif

void	die __P((char *));
static void	usage __P((void));
void	entab __P((char *));
void	detab __P((char *, char *));
void	main __P((int, char **));
void	dbwrite __P((DB *));
void	dbkey __P((DB *, char *));
void	dbscan __P((DB *, int));
void	dbdel __P((DB *, char *));
DB	*db;
char	*key;

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN      4321
#endif

void
die(s)
char	*s;
{
	errx(1, "%s", s);
}

static void
usage()
{
	fprintf(stderr, "%s\n%s\n",
		"usage: btreeop [-A][-C][-D key][-K key][-L][-b][-c cachesize]",
		"               [-l][-p psize][dbname]");
	exit(1);
}

#define TABPOS(i)	((i)%8 == 0)
/*
 * entab: convert spaces into tabs
 *
 *	io)	buf	string buffer
 */
void
entab(buf)
char	*buf;
{
	int	blanks = 0;
	int	pos, src, dst;
	char	c;

	pos = src = dst = 0;
	while ((c = buf[src++]) != 0) {
		if (c == ' ') {
			if (!TABPOS(++pos)) {
				blanks++;		/* count blanks */
				continue;
			}
			buf[dst++] = '\t';
		} else if (c == '\t') {
			while (!TABPOS(++pos))
				;
			buf[dst++] = '\t';
		} else {
			++pos;
			while (blanks--)
				buf[dst++] = ' ';
			buf[dst++] = c;
		}
		blanks = 0;
	}
	buf[dst] = 0;
}
/*
 * detab: convert tabs into spaces
 *
 *	i)	buf	string including tabs
 *	o)	out	output
 */
void
detab(buf, out)
char	*buf;
char	*out;
{
	int	src, dst;
	char	c;

	src = dst = 0;
	while ((c = buf[src++]) != 0) {
		if (c == '\t') {
			do {
				out[dst++] = ' ';
			} while (!TABPOS(dst));
		} else {
			out[dst++] = c;
		}
	}
	out[dst] = 0;
}

#include <errno.h>
void
main(argc, argv)
int	argc;
char	*argv[];
{
	char	command = 'R';
	char	*key = NULL;
	DB	*db;
	BTREEINFO info;
	int	c;
	int	flags = 0;
	extern	char *optarg;
	extern	int optind;

	info.flags = R_DUP;		/* allow duplicate entries */
	info.cachesize = 500000;
	info.maxkeypage = 0;
	info.minkeypage = 0;
	info.psize = 0;
	info.compare = NULL;
	info.prefix = NULL;
	info.lorder = LITTLE_ENDIAN;

	while ((c = getopt(argc, argv, "ACD:K:Lbc:lp:")) != -1) {
		switch (c) {
		case 'K':
		case 'D':
			key = optarg;
		case 'A':
		case 'C':
		case 'L':
			if (command != 'R')
				usage();
			command = c;
			break;
		case 'b':
			info.lorder = BIG_ENDIAN;
			break;
		case 'c':
			info.cachesize = atoi(optarg);
			break;
		case 'l':
			info.lorder = LITTLE_ENDIAN;
			break;
		case 'p':
			info.psize = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	dbname = (optind < argc) ? argv[optind] : dbdefault;
	switch (command) {
	case 'A':
	case 'D':
		flags = O_RDWR|O_CREAT;
		break;
	case 'C':
		flags = O_RDWR|O_CREAT|O_TRUNC;
		break;
	case 'K':
	case 'L':
	case 'R':
		flags = O_RDONLY;
		break;
	}
	db = dbopen(dbname, flags, 0644, DB_BTREE, &info);
	if (db == NULL) {
		die("dbopen failed.");
	}
	switch (command) {
	case 'A':			/* Append records */
	case 'C':			/* Create database */
		dbwrite(db);
		break;
	case 'D':			/* Delete records */
		dbdel(db, key);
		break;
	case 'K':			/* Keyed (indexed) read */
		dbkey(db, key);
		break;
	case 'R':			/* sequencial Read */
	case 'L':			/* key's List */
		dbscan(db, (command == 'L') ? 1 : 0);
		break;
	}
	if (db->close(db)) {
		die("db->close failed.");
	}
	exit(0);
}
/*
 * dbwrite: write to database
 *
 *	i)	db
 */
void
dbwrite(db)
DB	*db;
{
	DBT     key, dat;
	int	status;
#define IDENTLEN 80
	char	keybuf[IDENTLEN+1];
	char	*c;

	/*
	 * Input file format:
	 * +------------------
	 * |Key		Data\n
	 * |Key		Data\n
	 * 	.
	 * 	.
	 * - Key and Data are separated by blank('\t' or ' '). 
	 * - Key cannot include blank.
	 * - Data can include blank.
	 * - Null Data not allowed.
	 *
	 * META record:
	 * You can write meta record by making key start with a ' '.
	 * You can read this record only by indexed read ('-K' option).
	 * +------------------
	 * | __.VERSION 2
	 */
	while (fgets(buf, BUFSIZ, stdin)) {
		if (buf[strlen(buf)-1] == '\n')		/* chop(buf) */
			buf[strlen(buf)-1] = 0;
		else
			while (fgetc(stdin) != '\n')
				;
		c = buf;
		if (*c == ' ') {			/* META record */
			if (*++c == ' ')
				die("illegal format.");
		}
		for (; *c && !isspace(*c); c++)		/* skip key part */
			;
		if (*c == 0)
			die("data part not found.");
		if (c - buf > IDENTLEN)
			die("key too long.");
		strncpy(keybuf, buf, c - buf);		/* make key string */
		keybuf[c - buf] = 0;
		for (; *c && isspace(*c); c++)		/* skip blanks */
			;
		if (*c == 0)
			die("data part is null.");
		entab(buf);
		key.data = keybuf;
		key.size = strlen(keybuf)+1;
		dat.data = buf;
		dat.size = strlen(buf)+1;

		status = (db->put)(db, &key, &dat, 0);
		switch (status) {
		case RET_SUCCESS:
			break;
		case RET_ERROR:
		case RET_SPECIAL:
			die("db->put: failed.");
		}
	}
}

/*
 * dbkey: Keyed search
 *
 *	i)	db
 *	i)	skey	
 */
void
dbkey(db, skey)
DB	*db;
char	*skey;
{
	DBT	dat, key;
	int	status;

	key.data = skey;
	key.size = strlen(skey)+1;

	for (status = (*db->seq)(db, &key, &dat, R_CURSOR);
		status == RET_SUCCESS && !strcmp(key.data, skey);
		status = (*db->seq)(db, &key, &dat, R_NEXT)) {
		detab((char *)dat.data, out);
		(void)fprintf(stdout, "%s\n", out);
	}
	if (status == RET_ERROR)
		die("db->seq failed.");
}

/*
 * dbscan: Scan all records
 *
 *	i)	db
 *	i)	keylist
 */
void
dbscan(db, keylist)
DB	*db;
int	keylist;
{
	DBT	dat, key;
	int	status;
	char	prev[IDENTLEN+1];

	prev[0] = 0;
	for (status = (*db->seq)(db, &key, &dat, R_FIRST);
		status == RET_SUCCESS;
		status = (*db->seq)(db, &key, &dat, R_NEXT)) {
		/* skip META record */
		if (*(char *)key.data == ' ')
			continue;
		if (keylist) {
			if (!strcmp(prev, (char *)key.data))
				continue;
			strcpy(prev, (char *)key.data);
			(void)fprintf(stdout, "%s\n", (char *)key.data);
			continue;
		}
		detab((char *)dat.data, out);
		(void)fprintf(stdout, "%s\n", out);
	}
	if (status == RET_ERROR)
		die("db->seq failed.");
}

/*
 * dbdel: Delete records
 *
 *	i)	db
 *	i)	skey	key
 */
void
dbdel(db, skey)
DB	*db;
char	*skey;
{
	DBT	key;
	int	status;

	key.data = skey;
	key.size = strlen(skey)+1;

	status = (*db->del)(db, &key, 0);
	if (status == RET_ERROR)
		die("db->del failed.");
}
