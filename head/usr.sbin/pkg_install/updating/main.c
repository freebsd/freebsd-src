/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <beat@chruetertee.ch> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.          Beat Gätzi
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include <fetch.h>
#include <limits.h>
#include <sysexits.h>
#include <getopt.h>

#include "lib.h"
#include "pathnames.h"

typedef struct installedport {
	struct installedport *next;				/* List of installed ports. */
	char name[LINE_MAX];					/* Name of the installed port. */
} INSTALLEDPORT;

int usage(void);

static char opts[] = "d:f:h";
static struct option longopts[] = {
	{ "date",	required_argument,	NULL,		'd' },
	{ "file",	required_argument,	NULL,		'f' },
	{ "help",	no_argument,		NULL,		'h' },
	{ NULL,		0,			NULL,		0 },
};

/*
 * Parse /usr/port/UPDATING for corresponding entries. If no argument is
 * passed to pkg_updating all entries for all installed ports are displayed.
 * If a list of portnames is passed to pkg_updating only entries for the
 * given portnames are displayed. Use the -d option to define that only newer
 * entries as this date are shown.
 */
int
main(int argc, char *argv[])
{
	/* Keyword for searching portname in UPDATING. */
	const char *affects = "AFFECTS";
	/* Indicate a date -> end of a entry. Will fail on 2100-01-01... */
	const char *end = "20";
	/* Keyword for searching origin portname of installed port. */
	const char *origin = "@comment ORIGIN:";
	const char *pkgdbpath = LOG_DIR;		/* Location of pkgdb */
	const char *updatingfile = UPDATING;	/* Location of UPDATING */

	char *date = NULL; 						/* Passed -d argument */
	char *dateline = NULL;					/* Saved date of an entry */
	/* Tmp lines for parsing file */
	char *tmpline1 = NULL;
	char *tmpline2 = NULL;

	char originline[LINE_MAX];				/* Line of +CONTENTS */
	/* Temporary variable to create path to +CONTENTS for installed ports. */
	char tmp_file[MAXPATHLEN];
	char updatingline[LINE_MAX];			/* Line of UPDATING */

	int ch;									/* Char used by getopt */
	int found = 0;							/* Found an entry */
	int linelength;							/* Length of parsed line */
	int maxcharperline = LINE_MAX;			/* Max chars per line */
	int dflag = 0;							/* -d option set */
	/* If pflag = 0 UPDATING will be checked for all installed ports. */
	int pflag = 0;

	size_t n;								/* Offset to create path */

	struct dirent *pkgdbdir;				/* pkgdb directory */
	struct stat attribute;					/* attribute of pkgdb element */

	/* Needed nodes for linked list with installed ports. */
	INSTALLEDPORT *head = (INSTALLEDPORT *) NULL;
	INSTALLEDPORT *curr = (INSTALLEDPORT *) NULL;

	DIR *dir;
	FILE *fd;

	while ((ch = getopt_long(argc, argv, opts, longopts, NULL)) != -1) {
		switch (ch) {
			case 'd':
				dflag = 1;
				date = optarg;
				break;
			case 'f':
				updatingfile = optarg;
				break;
			case 'h':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Check if passed date has a correct format. */
	if (dflag == 1) {
		linelength = strlen(date);
		if (linelength != 8)
			exit(EX_DATAERR);
		if (strspn(date, "0123456789") != 8) {
			fprintf(stderr, "unknown date format: %s\n", date);
			exit(EX_DATAERR);
		}
	}

	/* Save the list of passed portnames. */
	if (argc != 0) {
		pflag = 1;
		while (*argv) {
			if ((curr = (INSTALLEDPORT *)
				malloc(sizeof(INSTALLEDPORT))) == NULL)
				(void)exit(EXIT_FAILURE);
			strlcpy(curr->name, *argv, strlen(*argv) + 1);
			curr->next = head;
			head = curr;
			(void)*argv++;
		}
	}

	/*
	 * UPDATING will be parsed for all installed ports
	 * if no portname is passed.
	 */
	if (pflag == 0) {
		/* Open /var/db/pkg and search for all installed ports. */
		if ((dir = opendir(pkgdbpath)) != NULL) {
			while ((pkgdbdir = readdir(dir)) != NULL) {
				if (strcmp(pkgdbdir->d_name, ".") != 0 && 
					strcmp(pkgdbdir->d_name, "..") != 0) {

					/* Create path to +CONTENTS file for each installed port */
					n = strlcpy(tmp_file, pkgdbpath, strlen(pkgdbpath)+1);
					n = strlcpy(tmp_file + n, "/", sizeof(tmp_file) - n);
					n = strlcat(tmp_file + n, pkgdbdir->d_name,
						sizeof(tmp_file) - n);
					if (stat(tmp_file, &attribute) == -1) {
						fprintf(stderr, "can't open %s: %s\n",
							tmp_file, strerror(errno));
						return EXIT_FAILURE;
					}
					if (attribute.st_mode & S_IFREG)
						continue;
					(void)strlcat(tmp_file + n, "/",
						sizeof(tmp_file) - n);
					(void)strlcat(tmp_file + n, CONTENTS_FNAME,
						sizeof(tmp_file) - n);

					/* Open +CONTENT file */
					fd = fopen(tmp_file, "r");
					if (fd == NULL) {
						fprintf(stderr, "warning: can't open %s: %s\n",
						tmp_file, strerror(errno));
						continue;
					}

					/*
					 * Parses +CONTENT for ORIGIN line and
					 * put element into linked list.
					 */
					while (fgets(originline, maxcharperline, fd) != NULL) {
						tmpline1 = strstr(originline, origin);
						if (tmpline1 != NULL) {
							/* Tmp variable to store port name. */
							char *pname;
							pname = strrchr(originline, (int)':');
							pname++;
							if ((curr = (INSTALLEDPORT *)
								malloc(sizeof(INSTALLEDPORT))) == NULL)
								(void)exit(EXIT_FAILURE);
							if (pname[strlen(pname) - 1] == '\n')
								pname[strlen(pname) - 1] = '\0';
							strlcpy (curr->name, pname, strlen(pname)+1);
							curr->next = head;
							head = curr;
						}
					}
					
					if (ferror(fd)) {
						fprintf(stderr, "error reading input\n");
						exit(EX_IOERR);
					}

					(void)fclose(fd);
				}
			}
			closedir(dir);
		} 
	}

	/* Fetch UPDATING file if needed and open file */
	if (isURL(updatingfile)) {
		if ((fd = fetchGetURL(updatingfile, "")) == NULL) {
			fprintf(stderr, "Error: Unable to get %s: %s\n",
				updatingfile, fetchLastErrString);
			exit(EX_UNAVAILABLE);
		}
	}
	else {
		fd = fopen(updatingfile, "r");
	}
	if (fd == NULL) {
		fprintf(stderr, "can't open %s: %s\n",
			updatingfile, strerror(errno));
		exit(EX_UNAVAILABLE);
	}

	/* Parse opened UPDATING file. */
	while (fgets(updatingline, maxcharperline, fd) != NULL) {
		/* No entry is found so far */
		if (found == 0) {
			/* Search for AFFECTS line to parse the portname. */
			tmpline1 = strstr(updatingline, affects);

			if (tmpline1 != NULL) {
				curr = head; 
				while (curr != NULL) {
					tmpline2 = strstr(updatingline, curr->name);
					if (tmpline2 != NULL)
						break;
					curr = curr->next;
				}
				if (tmpline2 != NULL) {
					/* If -d is set, check if entry is newer than the date. */
					if ((dflag == 1) && (strncmp(dateline, date, 8) < 0))
						continue;
					printf("%s", dateline);
					printf("%s", updatingline);
					found = 1;
				}
			}
		}
		/* Search for the end of an entry, if not found print the line. */
		else {
			tmpline1 = strstr(updatingline, end);
			if (tmpline1 == NULL)
				printf("%s", updatingline);
			else {
				linelength = strlen(updatingline);
				if (linelength == 10)
					found = 0;
				else
					printf("%s", updatingline);
			}
		}
		/* Save the actual line, it could be a date. */
		dateline = strdup(updatingline);
	}

	if (ferror(fd)) {
		fprintf(stderr, "error reading input\n");
		exit(EX_IOERR);
	}
	(void)fclose(fd);

	exit(EX_OK);
}

int
usage(void)
{
	fprintf(stderr,
		"usage: pkg_updating [-h] [-d YYYYMMDD] [-f file] [portname ...]\n");
	exit(EX_USAGE);
}

void
cleanup(int sig)
{
	if (sig)
		exit(1);
}
