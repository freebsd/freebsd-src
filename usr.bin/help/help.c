/*
 * Author:	J. Mallett <jmallett@FreeBSD.org>
 * Date:	May 22, 2002
 * Program:	help
 * Description:
 * 	Displays help from files in the format used by SCCS.
 *
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Base path to the help files.
 */
#define	_PATH_LIBHELP	"/usr/lib/help"

/*
 * The file we check if all else fails.
 */
#define	_PATH_DEFAULT	_PATH_LIBHELP "/default"

int help(const char *);

int
main(int argc, char *argv[])
{
	char *key;
	int i, rv;

	rv = 0;

	if (argc == 1) {
		size_t len;

		(void)printf("Enter the message number or SCCS command name: ");
		if ((key = fgetln(stdin, &len)) == NULL) {
			err(1, NULL);
		}
		key[len - 1] = '\0';
		return help(key);
	}
	argc--;
	argv++;

	for (i = 0; i < argc; i++) {
		/*
		 * If no error occurred this time, rv becomes 1.
		 */
		if (help(argv[i]) == 0) {
			rv = 1;
		}
	}

	/*
	 * Return 0 if at least one help() worked.  Return 1 else.
	 */
	return rv ? 0 : 1;
}

/*
 * Function:	help
 * Returns:	0 if no error occurrs, 1 otherwise.
 * Arguments:	key -- The key we are looking up help for.
 * Description:
 * 	Looks up the help for a given key.
 */
int
help(const char *key)
{
	FILE *helpfile;
	char path[PATH_MAX];
	char *keybase, *p;
	const char *keyname, *keynumber;
	int helping, found;
	size_t len, numlen;

	found = helping = 0;

	keyname = key;
	keybase = strdup(keyname);
	if (keybase == NULL) {
		err(1, NULL);
	}
	p = keybase;
	while (!isnumber(*p) && *p != '\0') {
		++key;
		++p;
	}
	keynumber = key;
	key = keyname;
	*p = '\0';
	/*
	 * Try the default help file if we have a numeric key.
	 * Or else, use the non-numeric part of the key.
	 */
	if (strlen(keybase) == 0) {
		strlcpy(path, _PATH_DEFAULT, sizeof(path));
	} else {
		snprintf(path, sizeof(path), _PATH_LIBHELP "/%s", keybase);
	}
	free(keybase);
	numlen = strlen(keynumber);
	if (!numlen) {
		goto fail;
	}

	helpfile = fopen(path, "r");
	if (helpfile == NULL) {
		goto fail;
	}
	while (!feof(helpfile) && (p = fgetln(helpfile, &len)) != NULL) {
		switch (*p) {
		case '*':
			continue;
		case '-':
			if (len < numlen + 1) {
				continue;
			}
			if (strncmp(++p, keynumber, numlen) == 0) {
				found = 1;
				helping = 1;
				printf("\n%s:\n", key);
			} else {
				helping = 0;
			}
			continue;
		default:
			if (helping) {
				p[len - 1] = '\0';
				printf("%s\n", p);
			}
			continue;
		}
	}
	fclose(helpfile);
	if (found) {
		return 0;
	}
fail:
	printf("Key '%s' not found.\n", key);
	return 1;
}
