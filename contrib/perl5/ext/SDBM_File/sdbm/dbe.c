#include <stdio.h>
#ifndef VMS
#include <sys/file.h>
#include <ndbm.h>
#else
#include "file.h"
#include "ndbm.h"
#endif
#include <ctype.h>

/***************************************************************************\
**                                                                         **
**   Function name: getopt()                                               **
**   Author:        Henry Spencer, UofT                                    **
**   Coding date:   84/04/28                                               **
**                                                                         **
**   Description:                                                          **
**                                                                         **
**   Parses argv[] for arguments.                                          **
**   Works with Whitesmith's C compiler.                                   **
**                                                                         **
**   Inputs   - The number of arguments                                    **
**            - The base address of the array of arguments                 **
**            - A string listing the valid options (':' indicates an       **
**              argument to the preceding option is required, a ';'        **
**              indicates an argument to the preceding option is optional) **
**                                                                         **
**   Outputs  - Returns the next option character,                         **
**              '?' for non '-' arguments                                  **
**              or ':' when there is no more arguments.                    **
**                                                                         **
**   Side Effects + The argument to an option is pointed to by 'optarg'    **
**                                                                         **
*****************************************************************************
**                                                                         **
**   REVISION HISTORY:                                                     **
**                                                                         **
**     DATE           NAME                        DESCRIPTION              **
**   YY/MM/DD  ------------------   ------------------------------------   **
**   88/10/20  Janick Bergeron      Returns '?' on unamed arguments        **
**                                  returns '!' on unknown options         **
**                                  and 'EOF' only when exhausted.         **
**   88/11/18  Janick Bergeron      Return ':' when no more arguments      **
**   89/08/11  Janick Bergeron      Optional optarg when ';' in optstring  **
**                                                                         **
\***************************************************************************/

char *optarg;			       /* Global argument pointer. */

#ifdef VMS
#define index  strchr
#endif

char
getopt(int argc, char **argv, char *optstring)
{
	register int c;
	register char *place;
	extern char *index();
	static int optind = 0;
	static char *scan = NULL;

	optarg = NULL;

	if (scan == NULL || *scan == '\0') {

		if (optind == 0)
			optind++;
		if (optind >= argc)
			return ':';

		optarg = place = argv[optind++];
		if (place[0] != '-' || place[1] == '\0')
			return '?';
		if (place[1] == '-' && place[2] == '\0')
			return '?';
		scan = place + 1;
	}

	c = *scan++;
	place = index(optstring, c);
	if (place == NULL || c == ':' || c == ';') {

		(void) fprintf(stderr, "%s: unknown option %c\n", argv[0], c);
		scan = NULL;
		return '!';
	}
	if (*++place == ':') {

		if (*scan != '\0') {

			optarg = scan;
			scan = NULL;

		}
		else {

			if (optind >= argc) {

				(void) fprintf(stderr, "%s: %c requires an argument\n",
					       argv[0], c);
				return '!';
			}
			optarg = argv[optind];
			optind++;
		}
	}
	else if (*place == ';') {

		if (*scan != '\0') {

			optarg = scan;
			scan = NULL;

		}
		else {

			if (optind >= argc || *argv[optind] == '-')
				optarg = NULL;
			else {
				optarg = argv[optind];
				optind++;
			}
		}
	}
	return c;
}


void
print_datum(datum db)
{
	int i;

	putchar('"');
	for (i = 0; i < db.dsize; i++) {
		if (isprint((unsigned char)db.dptr[i]))
			putchar(db.dptr[i]);
		else {
			putchar('\\');
			putchar('0' + ((db.dptr[i] >> 6) & 0x07));
			putchar('0' + ((db.dptr[i] >> 3) & 0x07));
			putchar('0' + (db.dptr[i] & 0x07));
		}
	}
	putchar('"');
}


datum
read_datum(char *s)
{
	datum db;
	char *p;
	int i;

	db.dsize = 0;
	db.dptr = (char *) malloc(strlen(s) * sizeof(char));
	if (!db.dptr)
	    oops("cannot get memory");

	for (p = db.dptr; *s != '\0'; p++, db.dsize++, s++) {
		if (*s == '\\') {
			if (*++s == 'n')
				*p = '\n';
			else if (*s == 'r')
				*p = '\r';
			else if (*s == 'f')
				*p = '\f';
			else if (*s == 't')
				*p = '\t';
			else if (isdigit((unsigned char)*s)
				 && isdigit((unsigned char)*(s + 1))
				 && isdigit((unsigned char)*(s + 2)))
			{
				i = (*s++ - '0') << 6;
				i |= (*s++ - '0') << 3;
				i |= *s - '0';
				*p = i;
			}
			else if (*s == '0')
				*p = '\0';
			else
				*p = *s;
		}
		else
			*p = *s;
	}

	return db;
}


char *
key2s(datum db)
{
	char *buf;
	char *p1, *p2;

	buf = (char *) malloc((db.dsize + 1) * sizeof(char));
	if (!buf)
	    oops("cannot get memory");
	for (p1 = buf, p2 = db.dptr; *p2 != '\0'; *p1++ = *p2++);
	*p1 = '\0';
	return buf;
}

int
main(int argc, char **argv)
{
	typedef enum {
		YOW, FETCH, STORE, DELETE, SCAN, REGEXP
	} commands;
	char opt;
	int flags;
	int giveusage = 0;
	int verbose = 0;
	commands what = YOW;
	char *comarg[3];
	int st_flag = DBM_INSERT;
	int argn;
	DBM *db;
	datum key;
	datum content;

	flags = O_RDWR;
	argn = 0;

	while ((opt = getopt(argc, argv, "acdfFm:rstvx")) != ':') {
		switch (opt) {
		case 'a':
			what = SCAN;
			break;
		case 'c':
			flags |= O_CREAT;
			break;
		case 'd':
			what = DELETE;
			break;
		case 'f':
			what = FETCH;
			break;
		case 'F':
			what = REGEXP;
			break;
		case 'm':
			flags &= ~(000007);
			if (strcmp(optarg, "r") == 0)
				flags |= O_RDONLY;
			else if (strcmp(optarg, "w") == 0)
				flags |= O_WRONLY;
			else if (strcmp(optarg, "rw") == 0)
				flags |= O_RDWR;
			else {
				fprintf(stderr, "Invalid mode: \"%s\"\n", optarg);
				giveusage = 1;
			}
			break;
		case 'r':
			st_flag = DBM_REPLACE;
			break;
		case 's':
			what = STORE;
			break;
		case 't':
			flags |= O_TRUNC;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'x':
			flags |= O_EXCL;
			break;
		case '!':
			giveusage = 1;
			break;
		case '?':
			if (argn < 3)
				comarg[argn++] = optarg;
			else {
				fprintf(stderr, "Too many arguments.\n");
				giveusage = 1;
			}
			break;
		}
	}

	if (giveusage || what == YOW || argn < 1) {
		fprintf(stderr, "Usage: %s databse [-m r|w|rw] [-crtx] -a|-d|-f|-F|-s [key [content]]\n", argv[0]);
		exit(-1);
	}

	if ((db = dbm_open(comarg[0], flags, 0777)) == NULL) {
		fprintf(stderr, "Error opening database \"%s\"\n", comarg[0]);
		exit(-1);
	}

	if (argn > 1)
		key = read_datum(comarg[1]);
	if (argn > 2)
		content = read_datum(comarg[2]);

	switch (what) {

	case SCAN:
		key = dbm_firstkey(db);
		if (dbm_error(db)) {
			fprintf(stderr, "Error when fetching first key\n");
			goto db_exit;
		}
		while (key.dptr != NULL) {
			content = dbm_fetch(db, key);
			if (dbm_error(db)) {
				fprintf(stderr, "Error when fetching ");
				print_datum(key);
				printf("\n");
				goto db_exit;
			}
			print_datum(key);
			printf(": ");
			print_datum(content);
			printf("\n");
			if (dbm_error(db)) {
				fprintf(stderr, "Error when fetching next key\n");
				goto db_exit;
			}
			key = dbm_nextkey(db);
		}
		break;

	case REGEXP:
		if (argn < 2) {
			fprintf(stderr, "Missing regular expression.\n");
			goto db_exit;
		}
		if (re_comp(comarg[1])) {
			fprintf(stderr, "Invalid regular expression\n");
			goto db_exit;
		}
		key = dbm_firstkey(db);
		if (dbm_error(db)) {
			fprintf(stderr, "Error when fetching first key\n");
			goto db_exit;
		}
		while (key.dptr != NULL) {
			if (re_exec(key2s(key))) {
				content = dbm_fetch(db, key);
				if (dbm_error(db)) {
					fprintf(stderr, "Error when fetching ");
					print_datum(key);
					printf("\n");
					goto db_exit;
				}
				print_datum(key);
				printf(": ");
				print_datum(content);
				printf("\n");
				if (dbm_error(db)) {
					fprintf(stderr, "Error when fetching next key\n");
					goto db_exit;
				}
			}
			key = dbm_nextkey(db);
		}
		break;

	case FETCH:
		if (argn < 2) {
			fprintf(stderr, "Missing fetch key.\n");
			goto db_exit;
		}
		content = dbm_fetch(db, key);
		if (dbm_error(db)) {
			fprintf(stderr, "Error when fetching ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		if (content.dptr == NULL) {
			fprintf(stderr, "Cannot find ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		print_datum(key);
		printf(": ");
		print_datum(content);
		printf("\n");
		break;

	case DELETE:
		if (argn < 2) {
			fprintf(stderr, "Missing delete key.\n");
			goto db_exit;
		}
		if (dbm_delete(db, key) || dbm_error(db)) {
			fprintf(stderr, "Error when deleting ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		if (verbose) {
			print_datum(key);
			printf(": DELETED\n");
		}
		break;

	case STORE:
		if (argn < 3) {
			fprintf(stderr, "Missing key and/or content.\n");
			goto db_exit;
		}
		if (dbm_store(db, key, content, st_flag) || dbm_error(db)) {
			fprintf(stderr, "Error when storing ");
			print_datum(key);
			printf("\n");
			goto db_exit;
		}
		if (verbose) {
			print_datum(key);
			printf(": ");
			print_datum(content);
			printf(" STORED\n");
		}
		break;
	}

db_exit:
	dbm_clearerr(db);
	dbm_close(db);
	if (dbm_error(db)) {
		fprintf(stderr, "Error closing database \"%s\"\n", comarg[0]);
		exit(-1);
	}
}
