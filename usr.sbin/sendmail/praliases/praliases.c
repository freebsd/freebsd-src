/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)praliases.c	8.5 (Berkeley) 5/28/97";
#endif /* not lint */

#include <ndbm.h>
#define NOT_SENDMAIL
#include <sendmail.h>
#ifdef NEWDB
#include <db.h>
#endif

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
#ifdef NDBM
	DBM *dbp;
	datum content, key;
#endif
	char *filename;
	int ch;
#ifdef NEWDB
	const DB *db;
	DBT newdbkey, newdbcontent;
	char buf[MAXNAME];
#endif

	filename = "/etc/aliases";
	while ((ch = getopt(argc, argv, "f:")) != -1)
		switch((char)ch) {
		case 'f':
			filename = optarg;
			break;
		case '?':
		default:
			(void)fprintf(stderr, "usage: praliases [-f file]\n");
			exit(EX_USAGE);
		}
	argc -= optind;
	argv += optind;

#ifdef NEWDB
	if (strlen(filename) + 4 >= sizeof buf)
	{
		fprintf(stderr, "Alias filename too long: %.30s...\n", filename);
		exit(EX_USAGE);
	}
	(void) strcpy(buf, filename);
	(void) strcat(buf, ".db");
	if (db = dbopen(buf, O_RDONLY, 0444 , DB_HASH, NULL)) {
		if (!argc) {
			while(!db->seq(db, &newdbkey, &newdbcontent, R_NEXT))
				printf("%.*s:%.*s\n",
					newdbkey.size, newdbkey.data,
					newdbcontent.size, newdbcontent.data);
		}
		else for (; *argv; ++argv) {
			newdbkey.data = *argv;
			newdbkey.size = strlen(*argv) + 1;
			if (!db->get(db, &newdbkey, &newdbcontent, 0))
				printf("%s:%.*s\n", newdbkey.data,
					newdbcontent.size, newdbcontent.data);
			else
				printf("%s: No such key\n",
					newdbkey.data);
		}
	}
#endif
#ifdef NDBM
#ifdef NEWDB
	else {
#endif /* NEWDB */
		if ((dbp = dbm_open(filename, O_RDONLY, 0)) == NULL) {
			(void)fprintf(stderr,
			    "praliases: %s: %s\n", filename, strerror(errno));
			exit(EX_OSFILE);
		}
		if (!argc)
			for (key = dbm_firstkey(dbp);
			    key.dptr != NULL; key = dbm_nextkey(dbp)) {
				content = dbm_fetch(dbp, key);
				(void)printf("%.*s:%.*s\n",
					key.dsize, key.dptr,
					content.dsize, content.dptr);
			}
		else for (; *argv; ++argv) {
			key.dptr = *argv;
			key.dsize = strlen(*argv) + 1;
			content = dbm_fetch(dbp, key);
			if (!content.dptr)
				(void)printf("%s: No such key\n", key.dptr);
			else
				(void)printf("%s:%.*s\n", key.dptr,
					content.dsize, content.dptr);
		}
#ifdef NEWDB
	}
#endif /* NEWDB */
#endif /* NDBM */
	exit(EX_OK);
}
