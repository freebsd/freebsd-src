/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	btreeop.c				12-Nov-98
 *
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"

const char *dbdefault = "btree";   	/* default database name */
const char *progname  = "btreeop";		/* command name */

static void	usage __P((void));
void	signal_setup __P((void));
void	onintr __P((int));
int	main __P((int, char **));
void	dbwrite __P((DBOP *));
void	dbkey __P((DBOP *, char *, int));
void	dbscan __P((DBOP *, char *, int));
void	dbdel __P((DBOP *, char *, int));
void	dbbysecondkey __P((DBOP *, int, char *, int));

#define F_KEY	0
#define F_DEL	1

static void
usage()
{
	fprintf(stderr, "%s\n",
		"usage: btreeop [-A][-C][-D[n] key][-K[n] key][-L[2]][-k prefix][dbname]");
	exit(1);
}

/*
 * Btreeop catch signal even if the parent ignore it.
 */
int	exitflag = 0;

void
onintr(signo)
int	signo;
{
	exitflag = 1;
}

void
signal_setup()
{
	signal(SIGHUP, onintr);
	signal(SIGINT, onintr);
	signal(SIGQUIT, onintr);
	signal(SIGTERM, onintr);
}

int
main(argc, argv)
int	argc;
char	*argv[];
{
	char	command = 'R';
	char	*key = NULL;
	int     mode = 0;
	const char *db_name;
	DBOP	*dbop;
	int	i, c;
	int	secondkey = 0;
	int	keylist = 0;
	char	*prefix = NULL;

	for (i = 1; i < argc && argv[i][0] == '-'; ++i) {
		switch (c = argv[i][1]) {
		case 'D':
		case 'K':
			if (argv[i][2] && isdigit(argv[i][2]))
				secondkey = atoi(&argv[i][2]);
			if (++i < argc)
				key = argv[i];
			else
				usage();
			/* FALLTHROUGH */
		case 'A':
		case 'C':
		case 'L':
			if (command != 'R')
				usage();
			command = c;
			if (command == 'L') {
				keylist = 1;
				if (argv[i][2] == '2')
					keylist = 2;
			}
			break;
		case 'k':
			if (++i < argc)
				prefix = argv[i];
			else
				usage();
			break;
		default:
			usage();
		}
	}
	db_name = (i < argc) ? argv[i] : dbdefault;
	switch (command) {
	case 'A':
	case 'D':
		mode = 2;
		break;
	case 'C':
		mode = 1;
		break;
	case 'K':
	case 'L':
	case 'R':
		mode = 0;
		break;
	}
	dbop = dbop_open(db_name, mode, 0644, DBOP_DUP);
	if (dbop == NULL) {
		switch (mode) {
		case 0:
		case 2:
			die1("cannot open '%s'.", db_name);
			break;
		case 1:
			die1("cannot create '%s'.", db_name);
			break;
		}
	}
	switch (command) {
	case 'A':			/* Append records */
	case 'C':			/* Create database */
		dbwrite(dbop);
		break;
	case 'D':			/* Delete records */
		dbdel(dbop, key, secondkey);
		break;
	case 'K':			/* Keyed (indexed) read */
		dbkey(dbop, key, secondkey);
		break;
	case 'R':			/* sequencial Read */
	case 'L':			/* primary key List */
		dbscan(dbop, prefix, keylist);
		break;
	}
	dbop_close(dbop);
	if (exitflag)
		exit(1);
	exit(0);
}
/*
 * dbwrite: write to database
 *
 *	i)	dbop		database
 */
void
dbwrite(dbop)
DBOP	*dbop;
{
	char	*p;
	char	keybuf[MAXKEYLEN+1];
	char	*c;

	signal_setup();
	/*
	 * Input file format:
	 * +--------------------------------------------------
	 * |Primary-key	secondary-key-1 secondary-key-2 Data\n
	 * |Primary-key	secondary-key-1 secondary-key-2 Data\n
	 * 	.
	 * 	.
	 * - Keys and Data are separated by blank('\t' or ' '). 
	 * - Keys cannot include blank.
	 * - Data can include blank.
	 * - Null record not allowed.
	 * - Secondary-key is assumed as a part of data by db(3).
	 *
	 * META record:
	 * You can write meta record by making key start with a ' '.
	 * You can read this record only by indexed read ('-K' option).
	 * +------------------
	 * | __.VERSION 2
	 */
	while ((p = mgets(stdin, NULL, 0)) != NULL) {
		if (exitflag)
			break;
		c = p;
		if (*c == ' ') {			/* META record */
			if (*++c == ' ')
				die("key cannot include blanks.");
		}
		for (; *c && !isspace(*c); c++)		/* skip key part */
			;
		if (*c == 0)
			die("data part not found.");
		if (c - p > MAXKEYLEN)
			die("primary key too long.");
		strncpy(keybuf, p, c - p);		/* make key string */
		keybuf[c - p] = 0;
		for (; *c && isspace(*c); c++)		/* skip blanks */
			;
		if (*c == 0)
			die("data part is null.");
		entab(p);
		dbop_put(dbop, keybuf, p);
	}
}

/*
 * dbkey: Keyed search
 *
 *	i)	dbop		database
 *	i)	skey		key for search
 *	i)	secondkey	0: primary key, >0: secondary key
 */
void
dbkey(dbop, skey, secondkey)
DBOP	*dbop;
char	*skey;
int	secondkey;
{
	char	*p;

	if (!secondkey) {
		for (p = dbop_first(dbop, skey, 0); p; p = dbop_next(dbop))
			detab(stdout, p);
		return;
	}
	dbbysecondkey(dbop, F_KEY, skey, secondkey);
}

/*
 * dbscan: Scan records
 *
 *	i)	dbop		database
 *	i)	prefix		prefix of primary key
 *	i)	keylist		0: data, 1: key, 2: key and data
 */
void
dbscan(dbop, prefix, keylist)
DBOP	*dbop;
char	*prefix;
int	keylist;
{
	char	*p;
	int	flags = 0;

	if (prefix)	
		flags |= DBOP_PREFIX;
	if (keylist)
		flags |= DBOP_KEY;

	for (p = dbop_first(dbop, prefix, flags); p; p = dbop_next(dbop)) {
		if (keylist == 2)
			fprintf(stdout, "%s %s\n", p, dbop->lastdat);
		else
			detab(stdout, p);
	}
}

/*
 * dbdel: Delete records
 *
 *	i)	dbop		database
 *	i)	skey		key for search
 *	i)	secondkey	0: primary key, >0: secondary key
 */
void
dbdel(dbop, skey, secondkey)
DBOP	*dbop;
char	*skey;
int	secondkey;
{
	signal_setup();
	if (!secondkey) {
		dbop_del(dbop, skey);
		return;
	}
	dbbysecondkey(dbop, F_DEL, skey, secondkey);
}
/*
 * dbbysecondkey: proc by second key
 *
 *	i)	dbop	database
 *	i)	func	F_KEY, F_DEL
 *	i)	skey
 *	i)	secondkey
 */
void
dbbysecondkey(dbop, func, skey, secondkey)
DBOP	*dbop;
int	func;
char	*skey;
int	secondkey;
{
	char	*c, *p;
	int	i;

	/* trim skey */
	for (c = skey; *c && isspace(*c); c++)
		;
	skey = c;
	for (c = skey+strlen(skey)-1; *c && isspace(*c); c--)
		*c = 0;

	for (p = dbop_first(dbop, NULL, 0); p; p = dbop_next(dbop)) {
		if (exitflag)
			break;
		c = p;
		/* reach to specified key */
		for (i = secondkey; i; i--) {
			for (; *c && !isspace(*c); c++)
				;
			if (*c == 0)
				die("specified key not found.");
			for (; *c && isspace(*c); c++)
				;
			if (*c == 0)
				die("specified key not found.");
		}
		i = strlen(skey);
		if (!strncmp(c, skey, i) && (*(c+i) == 0 || isspace(*(c+i)))) {
			switch (func) {
			case F_KEY:
				detab(stdout, p);
				break;
			case F_DEL:
				dbop_del(dbop, NULL);
				break;
			}
		}
		if (exitflag)
			break;
	}
}
