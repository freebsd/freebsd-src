#include <config.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kod_management.h"
#include "log.h"
#include "sntp-opts.h"
#include "ntp_stdlib.h"
//#define DEBUG

int kod_init = 0, kod_db_cnt = 0;
const char *kod_db_file;
struct kod_entry **kod_db;	/* array of pointers to kod_entry */


/*
 * Search for a KOD entry
 */
int
search_entry (
		char *hostname,
		struct kod_entry **dst
	     )
{
	register int a, b, resc = 0;

	for (a = 0; a < kod_db_cnt; a++)
		if (!strcmp(kod_db[a]->hostname, hostname))
			resc++;

	if (!resc) {
		*dst = NULL;
		return 0;
	}

	*dst = emalloc(resc * sizeof(**dst));

	b = 0;
	for (a = 0; a < kod_db_cnt; a++)
		if (!strcmp(kod_db[a]->hostname, hostname)) {
			(*dst)[b] = *kod_db[a];
			b++;
		}

	return resc;
}


void
add_entry(
	char *hostname,
	char *type	/* 4 bytes not \0 terminated */
	)
{
	int n;
	struct kod_entry *pke;

	pke = emalloc(sizeof(*pke));
	pke->timestamp = time(NULL);
	memcpy(pke->type, type, 4);
	pke->type[sizeof(pke->type) - 1] = '\0';
	strncpy(pke->hostname, hostname,
		sizeof(pke->hostname));
	pke->hostname[sizeof(pke->hostname) - 1] = '\0';

	/*
	 * insert in address ("hostname") order to find duplicates
	 */
	for (n = 0; n < kod_db_cnt; n++)
		if (strcmp(kod_db[n]->hostname, pke->hostname) >= 0)
			break;

	if (n < kod_db_cnt &&
	    0 == strcmp(kod_db[n]->hostname, pke->hostname)) {
		kod_db[n]->timestamp = pke->timestamp;
		free(pke);
		return;
	}

	kod_db_cnt++;
	kod_db = erealloc(kod_db, kod_db_cnt * sizeof(kod_db[0]));
	if (n != kod_db_cnt - 1)
		memmove(&kod_db[n + 1], &kod_db[n],
			sizeof(kod_db[0]) * ((kod_db_cnt - 1) - n));
	kod_db[n] = pke;
}


void
delete_entry(
	char *hostname,
	char *type
	)
{
	register int a;

	for (a = 0; a < kod_db_cnt; a++)
		if (!strcmp(kod_db[a]->hostname, hostname)
		    && !strcmp(kod_db[a]->type, type))
			break;

	if (a == kod_db_cnt)
		return;

	free(kod_db[a]);
	kod_db_cnt--;

	if (a < kod_db_cnt)
		memmove(&kod_db[a], &kod_db[a + 1],
			(kod_db_cnt - a) * sizeof(kod_db[0]));
}


void
write_kod_db(void)
{
	FILE *db_s;
	char *pch;
	int dirmode;
	register int a;

	db_s = fopen(kod_db_file, "w");

	/*
	 * If opening fails, blindly attempt to create each directory
	 * in the path first, then retry the open.
	 */
	if (NULL == db_s && strlen(kod_db_file)) {
		dirmode = S_IRUSR | S_IWUSR | S_IXUSR
			| S_IRGRP | S_IXGRP
			| S_IROTH | S_IXOTH;
		pch = strchr(kod_db_file + 1, DIR_SEP);
		while (NULL != pch) {
			*pch = '\0';
			mkdir(kod_db_file, dirmode);
			*pch = DIR_SEP;
			pch = strchr(pch + 1, DIR_SEP);
		}
		db_s = fopen(kod_db_file, "w");
	}

	if (NULL == db_s) {
		msyslog(LOG_WARNING, "Can't open KOD db file %s for writing!",
			kod_db_file);

		return;
	}

	for (a = 0; a < kod_db_cnt; a++) {
		fprintf(db_s, "%16.16llx %s %s\n", (unsigned long long)
			kod_db[a]->timestamp, kod_db[a]->type,
			kod_db[a]->hostname);
	}

	fflush(db_s);
	fclose(db_s);
}


void
kod_init_kod_db(
	const char *db_file
	)
{
	/*
	 * Max. of 254 characters for hostname, 10 for timestamp, 4 for
	 * kisscode, 2 for spaces, 1 for \n, and 1 for \0
	 */
	char fbuf[254+10+4+2+1+1];
	FILE *db_s;
	int a, b, sepc, len;
	unsigned long long ull;
	char *str_ptr;
	char error = 0;

	atexit(write_kod_db);

#ifdef DEBUG
	printf("Initializing KOD DB...\n");
#endif

	kod_db_file = estrdup(db_file);


	db_s = fopen(db_file, "r");

	if (NULL == db_s) {
		msyslog(LOG_WARNING, "kod_init_kod_db(): Cannot open KoD db file %s",
			db_file);

		return;
	}

	if (ENABLED_OPT(NORMALVERBOSE))
		printf("Starting to read KoD file %s...\n", db_file);
	/* First let's see how many entries there are and check for right syntax */

	while (!feof(db_s) && NULL != fgets(fbuf, sizeof(fbuf), db_s)) {

		/* ignore blank lines */
		if ('\n' == fbuf[0])
			continue;

		sepc = 0;
		len = strlen(fbuf);
		for (a = 0; a < len; a++) {
			if (' ' == fbuf[a])
				sepc++;

			if ('\n' == fbuf[a]) {
				if (sepc != 2) {
					if (strcmp(db_file, "/dev/null"))
						msyslog(LOG_DEBUG,
							"Syntax error in KoD db file %s in line %i (missing space)",
							db_file,
							kod_db_cnt + 1);
					fclose(db_s);
					return;
				}
				sepc = 0;
				kod_db_cnt++;
			}
		}
	}

	if (0 == kod_db_cnt) {
#ifdef DEBUG
		printf("KoD DB %s empty.\n", db_file);
#endif
		fclose(db_s);
		return;
	}

#ifdef DEBUG
	printf("KoD DB %s contains %d entries, reading...\n", db_file, kod_db_cnt);
#endif

	rewind(db_s);

	kod_db = emalloc(sizeof(kod_db[0]) * kod_db_cnt);

	/* Read contents of file */
	for (b = 0; 
	     !feof(db_s) && !ferror(db_s) && b < kod_db_cnt;
	     b++) {

		str_ptr = fgets(fbuf, sizeof(fbuf), db_s);
		if (NULL == str_ptr) {
			error = 1;
			break;
		}

		/* ignore blank lines */
		if ('\n' == fbuf[0]) {
			b--;
			continue;
		}

		kod_db[b] = emalloc(sizeof(*kod_db[b]));

		if (3 != sscanf(fbuf, "%llx %4s %254s", &ull,
		    kod_db[b]->type, kod_db[b]->hostname)) {

			free(kod_db[b]);
			kod_db[b] = NULL;
			error = 1;
			break;
		}

		kod_db[b]->timestamp = (time_t)ull;
	}

	if (ferror(db_s) || error) {
		kod_db_cnt = b;
		msyslog(LOG_WARNING, "An error occured while parsing the KoD db file %s",
			db_file);
		fclose(db_s);

		return;
	}

	fclose(db_s);
#ifdef DEBUG
	for (a = 0; a < kod_db_cnt; a++)
		printf("KoD entry %d: %s at %llx type %s\n", a,
		       kod_db[a]->hostname,
		       (unsigned long long)kod_db[a]->timestamp,
		       kod_db[a]->type);
#endif
}
