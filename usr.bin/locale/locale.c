/*-
 * Copyright (c) 2002 Alexey Zelkin <phantom@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <dirent.h>
#include <locale.h>
#include <rune.h>		/* for _PATH_LOCALE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>

/* Local prototypes */
void	usage(void);
void	get_locales_list(void);
void	list_locales(void);
void	display_locale_status(int);

/* Global variables */
static StringList *locales = NULL;

int	all_locales = 0;
int	verbose = 0;
int	env_used = 0;

struct _lcinfo {
	char	*name;
	int	id;
} lcinfo [] = {
	"LANG",		LC_ALL,
	"LC_ALL",	LC_ALL,
	"LC_CTYPE",	LC_CTYPE,
	"LC_COLLATE",	LC_COLLATE,
	"LC_TIME",	LC_TIME,
	"LC_NUMERIC",	LC_NUMERIC,
	"LC_MONETARY",	LC_MONETARY,
	"LC_MESSAGES",	LC_MESSAGES
};
#define NLCINFO (sizeof(lcinfo)/sizeof(lcinfo[0]))

int
main(int argc, char *argv[])
{
	char	ch;

	while ((ch = getopt(argc, argv, "av")) != -1)
		switch (ch) {
		case 'a':
			all_locales = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (all_locales) {
		list_locales();
		exit(0);
	}

	display_locale_status(verbose);
	return (0);
}

void
usage(void)
{
	printf("\nUsage: locale [ -a ] [ -v ]\n");
	exit(1);
}

void
list_locales(void)
{
	int i;

	get_locales_list();
	for (i = 0; i < locales->sl_cur; i++) {
		printf("%s\n", locales->sl_str[i]);
	}
}

void
probe_var(char *varname, int id)
{
	char *value, *res1, *res2;

	value = getenv(varname);
	if (value == NULL) {
		printf("%s = (null)\n", varname);
		return;
	}

	if (sl_find(locales, value) == NULL)
		res1 = "invalid";
	else
		res1 = "valid";

	if (strcmp(value, setlocale(id, value)) != 0)
		res2 = "invalid";
	else
		res2 = "valid";

	printf("%s = \"%s\"\t\t(name is %s, content is %s)\n",
			varname, value, res1, res2);
}

void
display_locale_status(int verbose)
{
	int i;

	setlocale(LC_ALL, "");

	if (verbose) {
		get_locales_list();
		if (env_used)
			printf("WARNING: PATH_LOCALE environment variable is set!");
	}

	printf("Current status of locale settings:\n\n");
	for (i = 1; i < NLCINFO; i++) {    /* start with 1 to avoid 'LANG' */
		printf("%s = \"%s\"\n",
			lcinfo[i].name,
			setlocale(lcinfo[i].id, ""));
	}

	/*
	 * If verbose flag is set, add environment information
	 */
	if (verbose) {
		printf("\nCurrent environment variables (and status):\n\n");
		for (i = 0; i < NLCINFO; i++) 
			probe_var(lcinfo[i].name, lcinfo[i].id);
	}
}

static char *
get_locale_path(void)
{
	char *localedir;

	localedir = getenv("PATH_LOCALE");
	if (localedir == NULL)
		localedir = _PATH_LOCALE;
	else
		env_used = 1;
	return (localedir);
}

static int
scmp(const void *s1, const void *s2)
{
	return strcmp(*(const char **)s1, *(const char **)s2);
}

void
get_locales_list()
{
	DIR *dirp;
	struct dirent *dp;
	char *dirname;

	if (locales != NULL)
		return;

	locales = sl_init();
	if (locales == NULL)
		err(1, "could not allocate memory");

	dirname = get_locale_path();
	dirp = opendir(dirname);
	if (dirp == NULL)
		err(1, "could not open directory '%s'", dirname);

	while ((dp = readdir(dirp)) != NULL) {
		if (*dp->d_name == '.')
			continue;
		sl_add(locales, strdup(dp->d_name));
	}
	closedir(dirp);

	qsort(locales->sl_str, locales->sl_cur, sizeof(char *), scmp);
}
