/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)praliases.c	8.21 (Berkeley) 12/27/1998";
#endif /* not lint */

#if !defined(NDBM) && !defined(NEWDB)
  ERROR README:	You must define one of NDBM or NEWDB in order to compile
  ERROR README:	praliases.
#endif

#ifdef NDBM
# include <ndbm.h>
#endif
#ifndef NOT_SENDMAIL
# define NOT_SENDMAIL
#endif
#include <sendmail.h>
#include <pathnames.h>
#ifdef NEWDB
# include <db.h>
# ifndef DB_VERSION_MAJOR
#  define DB_VERSION_MAJOR 1
# endif
#endif

#if defined(IRIX64) || defined(IRIX5) || defined(IRIX6) || \
    defined(BSD4_4) || defined(__osf__) || defined(__GNU_LIBRARY__)
# ifndef HASSTRERROR
#  define HASSTRERROR	1	/* has strerror(3) */
# endif
#endif

#if !HASSTRERROR
extern char	*strerror __P((int));
#endif

static void praliases __P((char *, int, char **));
#ifdef NDBM
static void praliases_dbm __P((char *, int, char **));
#endif

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	char *cfile;
#if _FFR_GRAB_ALIASFILE_OPTION
	char *filename = NULL;
#else
	char *filename = "/etc/aliases";
#endif
	FILE *cfp;
	int ch;
	char afilebuf[MAXLINE];
	char buf[MAXLINE];

	cfile = _PATH_SENDMAILCF;
#if _FFR_GRAB_ALIASFILE_OPTION
	while ((ch = getopt(argc, argv, "C:f:")) != EOF)
#else
	while ((ch = getopt(argc, argv, "f:")) != EOF)
#endif
	{
		switch ((char)ch) {
		case 'C':
			cfile = optarg;
			break;
		case 'f':
			filename = optarg;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
#if _FFR_GRAB_ALIASFILE_OPTION
				"usage: praliases [-C cffile] [-f aliasfile]\n");
#else
				"usage: praliases [-f aliasfile]\n");
#endif
			exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if (filename != NULL)
	{
		praliases(filename, argc, argv);
		exit(EX_OK);
	}

	if ((cfp = fopen(cfile, "r")) == NULL)
	{
		fprintf(stderr, "praliases: ");
		perror(cfile);
		exit(EX_NOINPUT);
	}

	while (fgets(buf, sizeof(buf), cfp) != NULL)
	{
		register char *b, *p;

		b = buf;
		switch (*b++)
		{
		  case 'O':		/* option -- see if alias file */
			if (strncasecmp(b, " AliasFile", 10) == 0 &&
			    !(isascii(b[10]) && isalnum(b[10])))
			{
				/* new form -- find value */
				b = strchr(b, '=');
				if (b == NULL)
					continue;
				while (isascii(*++b) && isspace(*b))
					continue;
			}
			else if (*b++ != 'A')
			{
				/* something else boring */
				continue;
			}

			/* this is the A or AliasFile option -- save it */
			if (strlen(b) >= sizeof afilebuf)
			{
				fprintf(stderr,
					"AliasFile filename too long: %.30s...\n",
					b);
				(void) fclose(cfp);
				exit(EX_CONFIG);
			}
			strcpy(afilebuf, b);
			b = afilebuf;

			for (p = b; p != NULL; )
			{
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p == '\0')
					break;
				b = p;

				p = strpbrk(p, " ,/");
				/* find end of spec */
				if (p != NULL)
					p = strpbrk(p, ",\n");
				if (p != NULL)
					*p++ = '\0';

				praliases(b, argc, argv);
			}

		  default:
			continue;
		}
	}
	(void) fclose(cfp);
	exit(EX_OK);
}

static void
praliases(filename, argc, argv)
	char *filename;
	int  argc;
	char **argv;
{
#ifdef NEWDB
	DB *db;
	DBT newdbkey, newdbcontent;
	char buf[MAXNAME];
#endif
	char *class;

	class = strchr(filename, ':');
	if (class != NULL)
	{
		if (strncasecmp(filename, "dbm:", 4) == 0)
		{
#ifdef NDBM
			praliases_dbm(class + 1, argc, argv);
			return;
#else
			fprintf(stderr, "class dbm not available\n");
			exit(EX_DATAERR);
#endif
		}
		filename = class + 1;
	}
#ifdef NEWDB
	if (strlen(filename) + 4 >= sizeof buf)
	{
		fprintf(stderr, "Alias filename too long: %.30s...\n", filename);
		exit(EX_USAGE);
	}
	(void) strcpy(buf, filename);
	(void) strcat(buf, ".db");
# if DB_VERSION_MAJOR < 2
	db = dbopen(buf, O_RDONLY, 0444, DB_HASH, NULL);
# else
	db = NULL;
	errno = db_open(buf, DB_HASH, DB_RDONLY, 0444, NULL, NULL, &db);
# endif
	if (db != NULL)
	{
		if (!argc)
		{
# if DB_VERSION_MAJOR > 1
			DBC *dbc;
# endif
			bzero(&newdbkey, sizeof newdbkey);
			bzero(&newdbcontent, sizeof newdbcontent);

# if DB_VERSION_MAJOR < 2
			while(!db->seq(db, &newdbkey, &newdbcontent, R_NEXT))
# else
#  if DB_VERSION_MAJOR > 2 || DB_VERSION_MINOR >=6
			if ((errno = db->cursor(db, NULL, &dbc, 0)) == 0)
#  else
			if ((errno = db->cursor(db, NULL, &dbc)) == 0)
#  endif
			{
				while ((errno = dbc->c_get(dbc, &newdbkey,
							   &newdbcontent,
							   DB_NEXT)) == 0)
# endif
				printf("%.*s:%.*s\n",
					(int) newdbkey.size,
					(char *) newdbkey.data,
					(int) newdbcontent.size,
					(char *) newdbcontent.data);
# if DB_VERSION_MAJOR > 1
				(void) dbc->c_close(dbc);
			}
			else
			{
				fprintf(stderr,
					"praliases: %s: Could not set cursor: %s\n",
					buf, strerror(errno));
				errno = db->close(db, 0);
				exit(EX_DATAERR);
			}
# endif
		}
		else for (; *argv; ++argv)
		{
			bzero(&newdbkey, sizeof newdbkey);
			bzero(&newdbcontent, sizeof newdbcontent);
			newdbkey.data = *argv;
			newdbkey.size = strlen(*argv) + 1;
# if DB_VERSION_MAJOR < 2
			if (!db->get(db, &newdbkey, &newdbcontent, 0))
# else
			if ((errno = db->get(db, NULL, &newdbkey,
					     &newdbcontent, 0)) == 0)
# endif
				printf("%s:%.*s\n", (char *) newdbkey.data,
					(int) newdbcontent.size,
					(char *) newdbcontent.data);
			else
				printf("%s: No such key\n",
					(char *) newdbkey.data);
		}
# if DB_VERSION_MAJOR < 2
		(void)db->close(db);
# else
		errno = db->close(db, 0);
# endif
	}
	else
	{
#endif
#ifdef NDBM
		praliases_dbm(filename, argc, argv);
#endif
#ifdef NEWDB
	}
#endif
}

#ifdef NDBM
static void
praliases_dbm(filename, argc, argv)
	char *filename;
	int  argc;
	char **argv;
{
	DBM *dbp;
	datum content, key;

	if ((dbp = dbm_open(filename, O_RDONLY, 0)) == NULL)
	{
		(void)fprintf(stderr,
		    "praliases: %s: %s\n", filename, strerror(errno));
		exit(EX_OSFILE);
	}
	if (!argc)
	{
		for (key = dbm_firstkey(dbp);
		    key.dptr != NULL; key = dbm_nextkey(dbp))
		{
			content = dbm_fetch(dbp, key);
			(void)printf("%.*s:%.*s\n",
				(int) key.dsize, key.dptr,
				(int) content.dsize, content.dptr);
		}
	}
	else
	{
		for (; *argv; ++argv)
		{
			/*
			**  Use the sendmail adaptive algorithm of trying
			**  the key first without, then if needed with,
			**  the terminating NULL byte.
			*/
			key.dptr = *argv;
			key.dsize = strlen(*argv);
			content = dbm_fetch(dbp, key);
			if (content.dptr == NULL)
			{
				key.dsize++;
				content = dbm_fetch(dbp, key);
			}
			if (content.dptr != NULL)
				(void)printf("%s:%.*s\n", key.dptr,
					(int) content.dsize, content.dptr);
			else
				(void)printf("%s: No such key\n", key.dptr);
		}
	}
	dbm_close(dbp);
}
#endif

#if !HASSTRERROR

char *
strerror(eno)
	int eno;
{
	extern int sys_nerr;
	extern char *sys_errlist[];
	static char ebuf[60];

	if (eno >= 0 && eno < sys_nerr)
		return sys_errlist[eno];
	(void) sprintf(ebuf, "Error %d", eno);
	return ebuf;
}

#endif /* !HASSTRERROR */
