/*
 * Copyright (c) 1992 Eric P. Allman.
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)makemap.c	8.6 (Berkeley) 11/22/93";
#endif /* not lint */

#include <stdio.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/file.h>
#include <ctype.h>
#include <string.h>
#include "useful.h"
#include "conf.h"

#ifdef NDBM
#include <ndbm.h>
#endif

#ifdef NEWDB
#include <db.h>
#endif

enum type { T_DBM, T_BTREE, T_HASH, T_ERR, T_UNKNOWN };

union dbent
{
#ifdef NDBM
	datum	dbm;
#endif
#ifdef NEWDB
	DBT	db;
#endif
	struct
	{
		char	*data;
		int	size;
	} xx;
};

#define BUFSIZE		1024

main(argc, argv)
	int argc;
	char **argv;
{
	char *progname;
	bool inclnull = FALSE;
	bool notrunc = FALSE;
	bool allowreplace = FALSE;
	bool verbose = FALSE;
	bool foldcase = TRUE;
	int exitstat;
	int opt;
	char *typename;
	char *mapname;
	char *ext;
	int lineno;
	int st;
	int mode;
	enum type type;
	union
	{
#ifdef NDBM
		DBM	*dbm;
#endif
#ifdef NEWDB
		DB	*db;
#endif
		void	*dbx;
	} dbp;
	union dbent key, val;
	char ibuf[BUFSIZE];
	char fbuf[MAXNAME];
	extern char *optarg;
	extern int optind;

	progname = argv[0];

	while ((opt = getopt(argc, argv, "Nforv")) != EOF)
	{
		switch (opt)
		{
		  case 'N':
			inclnull = TRUE;
			break;

		  case 'f':
			foldcase = FALSE;
			break;

		  case 'o':
			notrunc = TRUE;
			break;

		  case 'r':
			allowreplace = TRUE;
			break;

		  case 'v':
			verbose = TRUE;
			break;

		  default:
			type = T_ERR;
			break;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 2)
		type = T_ERR;
	else
	{
		typename = argv[0];
		mapname = argv[1];
		ext = NULL;

		if (strcmp(typename, "dbm") == 0)
		{
			type = T_DBM;
		}
		else if (strcmp(typename, "btree") == 0)
		{
			type = T_BTREE;
			ext = ".db";
		}
		else if (strcmp(typename, "hash") == 0)
		{
			type = T_HASH;
			ext = ".db";
		}
		else
			type = T_UNKNOWN;
	}

	switch (type)
	{
	  case T_ERR:
		fprintf(stderr, "Usage: %s [-N] [-o] [-v] type mapname\n", progname);
		exit(EX_USAGE);

	  case T_UNKNOWN:
		fprintf(stderr, "%s: Unknown database type %s\n",
			progname, typename);
		exit(EX_USAGE);

#ifndef NDBM
	  case T_DBM:
#endif
#ifndef NEWDB
	  case T_BTREE:
	  case T_HASH:
#endif
		fprintf(stderr, "%s: Type %s not supported in this version\n",
			progname, typename);
		exit(EX_UNAVAILABLE);
	}

	/*
	**  Adjust file names.
	*/

	if (ext != NULL)
	{
		int el, fl;

		el = strlen(ext);
		fl = strlen(mapname);
		if (fl < el || strcmp(&mapname[fl - el], ext) != 0)
		{
			strcpy(fbuf, mapname);
			strcat(fbuf, ext);
			mapname = fbuf;
		}
	}

	/*
	**  Create the database.
	*/

	mode = O_RDWR;
	if (!notrunc)
		mode |= O_CREAT|O_TRUNC;
	switch (type)
	{
#ifdef NDBM
	  case T_DBM:
		dbp.dbm = dbm_open(mapname, mode, 0644);
		break;
#endif

#ifdef NEWDB
	  case T_HASH:
		dbp.db = dbopen(mapname, mode, 0644, DB_HASH, NULL);
		break;

	  case T_BTREE:
		dbp.db = dbopen(mapname, mode, 0644, DB_BTREE, NULL);
		break;
#endif

	  default:
		fprintf(stderr, "%s: internal error: type %d\n", progname, type);
		exit(EX_SOFTWARE);
	}

	if (dbp.dbx == NULL)
	{
		fprintf(stderr, "%s: cannot create type %s map %s\n",
			progname, typename, mapname);
		exit(EX_CANTCREAT);
	}

	/*
	**  Copy the data
	*/

	lineno = 0;
	exitstat = EX_OK;
	while (fgets(ibuf, sizeof ibuf, stdin) != NULL)
	{
		register char *p;

		lineno++;

		/*
		**  Parse the line.
		*/

		p = strchr(ibuf, '\n');
		if (p != NULL)
			*p = '\0';
		else if (!feof(stdin))
		{
			fprintf(stderr, "%s: %s: line %d: line too long (%d bytes max)\n",
				progname, mapname, lineno, sizeof ibuf);
			continue;
		}
			
		if (ibuf[0] == '\0' || ibuf[0] == '#')
			continue;
		if (isspace(ibuf[0]))
		{
			fprintf(stderr, "%s: %s: line %d: syntax error (leading space)\n",
				progname, mapname, lineno);
			continue;
		}
		key.xx.data = ibuf;
		for (p = ibuf; *p != '\0' && !isspace(*p); p++)
		{
			if (foldcase && isupper(*p))
				*p = tolower(*p);
		}
		key.xx.size = p - key.xx.data;
		if (inclnull)
			key.xx.size++;
		if (*p != '\0')
			*p++ = '\0';
		while (isspace(*p))
			p++;
		if (*p == '\0')
		{
			fprintf(stderr, "%s: %s: line %d: no RHS for LHS %s\n",
				progname, mapname, lineno, key.xx.data);
			continue;
		}
		val.xx.data = p;
		val.xx.size = strlen(p);
		if (inclnull)
			val.xx.size++;

		/*
		**  Do the database insert.
		*/

		if (verbose)
		{
			printf("key=`%s', val=`%s'\n", key.xx.data, val.xx.data);
		}

		switch (type)
		{
#ifdef NDBM
		  case T_DBM:
			st = dbm_store(dbp.dbm, key.dbm, val.dbm,
					allowreplace ? DBM_REPLACE : DBM_INSERT);
			break;
#endif

#ifdef NEWDB
		  case T_BTREE:
		  case T_HASH:
			st = (*dbp.db->put)(dbp.db, &key.db, &val.db,
					allowreplace ? 0 : R_NOOVERWRITE);
			break;
#endif
		}

		if (st < 0)
		{
			fprintf(stderr, "%s: %s: line %d: key %s: put error\n",
				progname, mapname, lineno, key.xx.data);
			perror(mapname);
			exitstat = EX_IOERR;
		}
		else if (st > 0)
		{
			fprintf(stderr, "%s: %s: line %d: key %s: duplicate key\n",
				progname, mapname, lineno, key.xx.data);
		}
	}

	/*
	**  Now close the database.
	*/

	switch (type)
	{
#ifdef NDBM
	  case T_DBM:
		dbm_close(dbp.dbm);
		break;
#endif

#ifdef NEWDB
	  case T_HASH:
	  case T_BTREE:
		if ((*dbp.db->close)(dbp.db) < 0)
		{
			fprintf(stderr, "%s: %s: error on close\n",
				progname, mapname);
			perror(mapname);
			exitstat = EX_IOERR;
		}
#endif
	}

	exit (exitstat);
}
