/*
 * Copyright (c) 1990, 1993 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 * Convert a file map into an ndbm map
 */

#ifndef lint
char copyright[] = "\
@(#)Copyright (c) 1990, 1993 Jan-Simon Pendry\n\
@(#)Copyright (c) 1990 Imperial College of Science, Technology & Medicine\n\
@(#)Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mk-amd-map.c	8.1 (Berkeley) 6/28/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include "am.h"
#include <stdlib.h>

#ifndef SIGINT
#include <signal.h>
#endif

#ifdef OS_HAS_NDBM
#define HAS_DATABASE
#include <ndbm.h>

#ifdef DBM_SUFFIX
#define USING_DB
#endif

#define create_database(name) dbm_open(name, O_RDWR|O_CREAT, 0644)

static void usage __P((void));

static int store_data(db, k, v)
voidp db;
char *k, *v;
{
	datum key, val;

	key.dptr = k; val.dptr = v;
	key.dsize = strlen(k) + 1;
	val.dsize = strlen(v) + 1;
	return dbm_store((DBM *) db, key, val, DBM_INSERT);
}

#endif /* OS_HAS_NDBM */

#ifdef HAS_DATABASE
#include <fcntl.h>
#include <ctype.h>

static int read_line(buf, size, fp)
char *buf;
int size;
FILE *fp;
{
	int done = 0;

	do {
		while (fgets(buf, size, fp)) {
			int len = strlen(buf);
			done += len;
			if (len > 1 && buf[len-2] == '\\' &&
					buf[len-1] == '\n') {
				int ch;
				buf += len - 2;
				size -= len - 2;
				*buf = '\n'; buf[1] = '\0';
				/*
				 * Skip leading white space on next line
				 */
				while ((ch = getc(fp)) != EOF &&
					isascii(ch) && isspace(ch))
						;
				(void) ungetc(ch, fp);
			} else {
				return done;
			}
		}
	} while (size > 0 && !feof(fp));

	return done;
}

/*
 * Read through a map
 */
static int read_file(fp, map, db)
FILE *fp;
char *map;
voidp db;
{
	char key_val[2048];
	int chuck = 0;
	int line_no = 0;
	int errs = 0;

	while (read_line(key_val, sizeof(key_val), fp)) {
		char *kp;
		char *cp;
		char *hash;
		int len = strlen(key_val);
		line_no++;

		/*
		 * Make sure we got the whole line
		 */
		if (key_val[len-1] != '\n') {
			fprintf(stderr, "line %d in \"%s\" is too long", line_no, map);
			chuck = 1;
		} else {
			key_val[len-1] = '\0';
		}

		/*
		 * Strip comments
		 */
		hash = strchr(key_val, '#');
		if (hash)
			*hash = '\0';

		/*
		 * Find start of key
		 */
		for (kp = key_val; *kp && isascii(*kp) && isspace(*kp); kp++)
			;

		/*
		 * Ignore blank lines
		 */
		if (!*kp)
			goto again;

		/*
		 * Find end of key
		 */
		for (cp = kp; *cp&&(!isascii(*cp)||!isspace(*cp)); cp++)
			;

		/*
		 * Check whether key matches, or whether
		 * the entry is a wildcard entry.
		 */
		if (*cp)
			*cp++ = '\0';
		while (*cp && isascii(*cp) && isspace(*cp))
			cp++;
		if (*kp == '+') {
			fprintf(stderr, "Can't interpolate %s\n", kp);
			errs++;
		} else if (*cp) {
			if (db) {
				if (store_data(db, kp, cp) < 0) {
					fprintf(stderr, "Could store %s -> %s\n", kp, cp);
					errs++;
				}
			} else {
				printf("%s\t%s\n", kp, cp);
			}
		} else {
			fprintf(stderr, "%s: line %d has no value field", map, line_no);
			errs++;
		}

again:
		/*
		 * If the last read didn't get a whole line then
		 * throw away the remainder before continuing...
		 */
		if (chuck) {
			while (fgets(key_val, sizeof(key_val), fp) &&
				!strchr(key_val, '\n'))
					;
			chuck = 0;
		}
	}
	return errs;
}

static int remove_file(f)
char *f;
{
	if (unlink(f) < 0 && errno != ENOENT)
		return -1;
	return 0;
}

main(argc, argv)
int argc;
char *argv[];
{
	FILE *mapf;
	char *map;
	int rc = 0;
	DBM *mapd;
	static char maptmp[] = "dbmXXXXXX";
	char maptpag[16];
	char *mappag;
#ifndef USING_DB
	char maptdir[16];
	char *mapdir;
#endif
	int len;
	char *sl;
	int printit = 0;
	int usageflg = 0;
	int ch;

	while ((ch = getopt(argc, argv, "p")) !=  -1)
	switch (ch) {
	case 'p':
		printit = 1;
		break;
	default:
		usageflg++;
		break;
	}

	if (usageflg || optind != (argc - 1)) {
		usage();
	}

	map = argv[optind];
	sl = strrchr(map, '/');
	if (sl) {
		*sl = '\0';
		if (chdir(map) < 0) {
			fputs("Can't chdir to ", stderr);
			perror(map);
			exit(1);
		}
		map = sl + 1;
	}

	if (!printit) {
		len = strlen(map);
#ifdef USING_DB
		mappag = (char *) malloc(len + 5);
		if (!mappag) {
			perror("mk-amd-map: malloc");
			exit(1);
		}
		mktemp(maptmp);
		sprintf(maptpag, "%s%s", maptmp, DBM_SUFFIX);
		if (remove_file(maptpag) < 0) {
			fprintf(stderr, "Can't remove existing temporary file");
			perror(maptpag);
			exit(1);
		}
#else
		mappag = (char *) malloc(len + 5);
		mapdir = (char *) malloc(len + 5);
		if (!mappag || !mapdir) {
			perror("mk-amd-map: malloc");
			exit(1);
		}
		mktemp(maptmp);
		sprintf(maptpag, "%s.pag", maptmp);
		sprintf(maptdir, "%s.dir", maptmp);
		if (remove_file(maptpag) < 0 || remove_file(maptdir) < 0) {
			fprintf(stderr, "Can't remove existing temporary files; %s and", maptpag);
			perror(maptdir);
			exit(1);
		}
#endif
	}

	mapf =  fopen(map, "r");
	if (mapf && !printit)
		mapd = create_database(maptmp);
	else
		mapd = 0;

#ifndef DEBUG
	signal(SIGINT, SIG_IGN);
#endif

	if (mapd || printit) {
		int error = read_file(mapf, map, mapd);
		if (mapd)
			dbm_close(mapd);
		(void) fclose(mapf);
		if (printit) {
			if (error) {
				fprintf(stderr, "Error creating ndbm map for %s\n", map);
				rc = 1;
			}
		} else {
			if (error) {
				fprintf(stderr, "Error reading source file  %s\n", map);
				rc = 1;
			} else {
#ifdef USING_DB
				sprintf(mappag, "%s%s", map, DBM_SUFFIX);
				if (rename(maptpag, mappag) < 0) {
					fprintf(stderr, "Couldn't rename %s to ", maptpag);
					perror(mappag);
					/* Throw away the temporary map */
					unlink(maptpag);
					rc = 1;
				}
#else
				sprintf(mappag, "%s.pag", map);
				sprintf(mapdir, "%s.dir", map);
				if (rename(maptpag, mappag) < 0) {
					fprintf(stderr, "Couldn't rename %s to ", maptpag);
					perror(mappag);
					/* Throw away the temporary map */
					unlink(maptpag);
					unlink(maptdir);
					rc = 1;
				} else if (rename(maptdir, mapdir) < 0) {
					fprintf(stderr, "Couldn't rename %s to ", maptdir);
					perror(mapdir);
					/* Put the .pag file back */
					rename(mappag, maptpag);
					/* Throw away remaining part of original map */
					unlink(mapdir);
					fprintf(stderr,
						"WARNING: existing map \"%s.{dir,pag}\" destroyed\n",
						map);
					rc = 1;
				}
#endif
			}
		}
	} else {
		fprintf(stderr, "Can't open \"%s.{dir,pag}\" for ", map);
		perror("writing");
		rc = 1;
	}
	exit(rc);
}

static void
usage()
{
	fprintf(stderr, "usage: mk-amd-map [-p] file-map\n");
	exit(1);
}

#else
main()
{
	fputs("mk-amd-map: This system does not support hashed database files\n", stderr);
	exit(1);
}
#endif /* HAS_DATABASE */
