/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
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
"@(#) Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1983 Eric P. Allman.  All rights reserved.\n\
     Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)$Id: praliases.c,v 8.59.4.10 2000/07/18 05:41:39 gshapiro Exp $";
#endif /* ! lint */

/* $FreeBSD: src/contrib/sendmail/praliases/praliases.c,v 1.3.6.1 2000/08/27 17:31:22 gshapiro Exp $ */

#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef EX_OK
# undef EX_OK		/* unistd.h may have another use for this */
#endif /* EX_OK */
#include <sysexits.h>


#ifndef NOT_SENDMAIL
# define NOT_SENDMAIL
#endif /* ! NOT_SENDMAIL */
#include <sendmail/sendmail.h>
#include <sendmail/pathnames.h>
#include <libsmdb/smdb.h>

static void praliases __P((char *, int, char **));

uid_t	RealUid;
gid_t	RealGid;
char	*RealUserName;
uid_t	RunAsUid;
uid_t	RunAsGid;
char	*RunAsUserName;
int	Verbose = 2;
bool	DontInitGroups = FALSE;
uid_t	TrustedUid = 0;
BITMAP256 DontBlameSendmail;

extern void	syserr __P((const char *, ...));


int
main(argc, argv)
	int argc;
	char **argv;
{
	char *cfile;
	char *filename = NULL;
	FILE *cfp;
	int ch;
	char afilebuf[MAXLINE];
	char buf[MAXLINE];
	struct passwd *pw;
	static char rnamebuf[MAXNAME];
	extern char *optarg;
	extern int optind;


	clrbitmap(DontBlameSendmail);
	RunAsUid = RealUid = getuid();
	RunAsGid = RealGid = getgid();
	pw = getpwuid(RealUid);
	if (pw != NULL)
	{
		if (strlen(pw->pw_name) > MAXNAME - 1)
			pw->pw_name[MAXNAME] = 0;
		snprintf(rnamebuf, sizeof rnamebuf, "%s", pw->pw_name);
	}
	else
		(void) snprintf(rnamebuf, sizeof rnamebuf, "Unknown UID %d",
				(int) RealUid);
	RunAsUserName = RealUserName = rnamebuf;

	cfile = _PATH_SENDMAILCF;
	while ((ch = getopt(argc, argv, "C:f:")) != -1)
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
				      "usage: praliases [-C cffile] [-f aliasfile]\n");
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
		fprintf(stderr, "praliases: %s: %s\n",
			cfile, errstring(errno));
		exit(EX_NOINPUT);
	}

	while (fgets(buf, sizeof(buf), cfp) != NULL)
	{
		register char *b, *p;

		b = strchr(buf, '\n');
		if (b != NULL)
			*b = '\0';

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
			if (strlcpy(afilebuf, b, sizeof afilebuf) >=
			    sizeof afilebuf)
			{
				fprintf(stderr,
					"praliases: AliasFile filename too long: %.30s\n",
					b);
				(void) fclose(cfp);
				exit(EX_CONFIG);
			}
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
				{
					bool quoted = FALSE;

					for (; *p != '\0'; p++)
					{
						/*
						**  Don't break into a quoted
						**  string.
						*/

						if (*p == '"')
							quoted = !quoted;
						else if (*p == ',' && !quoted)
							break;
					}

					/* No more alias specs follow */
					if (*p == '\0')
					{
						/* chop trailing whitespace */
						while (isascii(*p) &&
						       isspace(*p) &&
						       p > b)
							p--;
						*p = '\0';
						p = NULL;
					}
				}

				if (p != NULL)
				{
					char *e = p - 1;

					/* chop trailing whitespace */
					while (isascii(*e) &&
					       isspace(*e) &&
					       e > b)
						e--;
					*++e = '\0';
					*p++ = '\0';
				}
				praliases(b, argc, argv);
			}

		  default:
			continue;
		}
	}
	(void) fclose(cfp);
	exit(EX_OK);
	/* NOTREACHED */
	return EX_OK;
}

static void
praliases(filename, argc, argv)
	char *filename;
	int argc;
	char **argv;
{
	int result;
	char *colon;
	char *db_name;
	char *db_type;
	SMDB_DATABASE *database = NULL;
	SMDB_CURSOR *cursor = NULL;
	SMDB_DBENT db_key, db_value;
	SMDB_DBPARAMS params;
	SMDB_USER_INFO user_info;

	colon = strchr(filename, ':');
	if (colon == NULL)
	{
		db_name = filename;
		db_type = SMDB_TYPE_DEFAULT;
	}
	else
	{
		*colon = '\0';
		db_name = colon + 1;
		db_type = filename;
	}

	/* clean off arguments */
	for (;;)
	{
		while (isascii(*db_name) && isspace(*db_name))
			db_name++;
		if (*db_name != '-')
			break;
		while (*db_name != '\0' &&
		       !(isascii(*db_name) && isspace(*db_name)))
			db_name++;
	}

	if (*db_name == '\0' || (db_type != NULL && *db_type == '\0'))
	{
		if (colon != NULL)
			*colon = ':';
		fprintf(stderr,	"praliases: illegal alias specification: %s\n",
			filename);
		goto fatal;
	}

	memset(&params, '\0', sizeof params);
	params.smdbp_cache_size = 1024 * 1024;

	user_info.smdbu_id = RunAsUid;
	user_info.smdbu_group_id = RunAsGid;
	strlcpy(user_info.smdbu_name, RunAsUserName, SMDB_MAX_USER_NAME_LEN);

	result = smdb_open_database(&database, db_name, O_RDONLY, 0,
				    SFF_ROOTOK, db_type, &user_info, &params);
	if (result != SMDBE_OK)
	{
		fprintf(stderr, "praliases: %s: open: %s\n",
			db_name, errstring(result));
		goto fatal;
	}

	if (argc == 0)
	{
		memset(&db_key, '\0', sizeof db_key);
		memset(&db_value, '\0', sizeof db_value);

		result = database->smdb_cursor(database, &cursor, 0);
		if (result != SMDBE_OK)
		{
			fprintf(stderr, "praliases: %s: set cursor: %s\n",
				db_name, errstring(result));
			goto fatal;
		}

		while ((result = cursor->smdbc_get(cursor, &db_key, &db_value,
						   SMDB_CURSOR_GET_NEXT)) ==
						   SMDBE_OK)
		{
#if 0
			/* skip magic @:@ entry */
			if (db_key.data.size == 2 &&
			    db_key.data.data[0] == '@' &&
			    db_key.data.data[1] == '\0' &&
			    db_value.data.size == 2 &&
			    db_value.data.data[0] == '@' &&
			    db_value.data.data[1] == '\0')
				continue;
#endif /* 0 */

			printf("%.*s:%.*s\n",
			       (int) db_key.data.size,
			       (char *) db_key.data.data,
			       (int) db_value.data.size,
			       (char *) db_value.data.data);
		}

		if (result != SMDBE_OK && result != SMDBE_LAST_ENTRY)
		{
			fprintf(stderr,
				"praliases: %s: get value at cursor: %s\n",
				db_name, errstring(result));
			goto fatal;
		}
	}
	else for (; *argv != NULL; ++argv)
	{
		memset(&db_key, '\0', sizeof db_key);
		memset(&db_value, '\0', sizeof db_value);
		db_key.data.data = *argv;
		db_key.data.size = strlen(*argv) + 1;
		if (database->smdb_get(database, &db_key,
				       &db_value, 0) == SMDBE_OK)
		{
			printf("%.*s:%.*s\n",
			       (int) db_key.data.size,
			       (char *) db_key.data.data,
			       (int) db_value.data.size,
			       (char *) db_value.data.data);
		}
		else
			printf("%s: No such key\n", (char *) db_key.data.data);
	}

 fatal:
	if (cursor != NULL)
		(void) cursor->smdbc_close(cursor);
	if (database != NULL)
		(void) database->smdb_close(database);
	if (colon != NULL)
		*colon = ':';
	return;
}

/*VARARGS1*/
void
#ifdef __STDC__
message(const char *msg, ...)
#else /* __STDC__ */
message(msg, va_alist)
	const char *msg;
	va_dcl
#endif /* __STDC__ */
{
	const char *m;
	VA_LOCAL_DECL

	m = msg;
	if (isascii(m[0]) && isdigit(m[0]) &&
	    isascii(m[1]) && isdigit(m[1]) &&
	    isascii(m[2]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	(void) vfprintf(stderr, m, ap);
	VA_END;
	(void) fprintf(stderr, "\n");
}

/*VARARGS1*/
void
#ifdef __STDC__
syserr(const char *msg, ...)
#else /* __STDC__ */
syserr(msg, va_alist)
	const char *msg;
	va_dcl
#endif /* __STDC__ */
{
	const char *m;
	VA_LOCAL_DECL

	m = msg;
	if (isascii(m[0]) && isdigit(m[0]) &&
	    isascii(m[1]) && isdigit(m[1]) &&
	    isascii(m[2]) && isdigit(m[2]) && m[3] == ' ')
		m += 4;
	VA_START(msg);
	(void) vfprintf(stderr, m, ap);
	VA_END;
	(void) fprintf(stderr, "\n");
}
