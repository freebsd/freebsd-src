/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jeremy D. Lea.
 * 11 May 2002
 *
 * This is the version module. Based on pkg_version.pl by Bruce A. Mah.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include "version.h"
#include <err.h>
#include <fetch.h>
#include <signal.h>

FILE *IndexFile;
struct index_head Index = SLIST_HEAD_INITIALIZER(Index);

static int pkg_do(char *);
static void show_version(const char *, const char *, const char *);

/*
 * This is the traditional pkg_perform, except that the argument is _not_
 * a list of packages. It is the index file from the command line.
 *
 * We loop over the installed packages, matching them with the -s flag
 * if needed and calling pkg_do(). Before hand we set up a few things,
 * and after we tear them down...
 */
int
pkg_perform(char **indexarg)
{
    char tmp[PATH_MAX], **pkgs;
    struct index_entry *ie;
    int i, err_cnt = 0;

    /*
     * Try to find and open the INDEX. We only check IndexFile != NULL
     * later, if we actually need the INDEX.
     * XXX This should not be hard-coded to INDEX-5.
     */
    if (*indexarg == NULL)
	snprintf(tmp, PATH_MAX, "%s/INDEX-5", PORTS_DIR);
    else
	strlcpy(tmp, *indexarg, PATH_MAX);
    if (isURL(tmp))
	IndexFile = fetchGetURL(tmp, "");
    else
	IndexFile = fopen(tmp, "r");

    /* Get a list of all the installed packages */
    pkgs = matchinstalled(MATCH_ALL, NULL, &err_cnt);
    if (err_cnt != 0)
	errx(2, "Unable to find package database directory!");
    i = -1;
    while (pkgs[++i] != NULL) {
	if (MatchName == NULL || strstr(pkgs[i], MatchName))
	    err_cnt += pkg_do(pkgs[i]);
    }

    /* If we opened the INDEX in pkg_do(), clean up. */
    while (!SLIST_EMPTY(&Index)) {
	ie = SLIST_FIRST(&Index);
	SLIST_REMOVE_HEAD(&Index, next);
	if (ie->name != NULL)
	    free(ie->name);
	if (ie->origin != NULL)
	    free(ie->origin);
	free(ie);
    }
    if (IndexFile != NULL)
	fclose(IndexFile);

    return err_cnt;
}

/*
 * Traditional pkg_do(). We take the package name we are passed and
 * first slurp in the CONTENTS file, getting name and origin, then
 * we look for it's corresponding Makefile. If that fails we pull in
 * the INDEX, and check there.
 */
static int
pkg_do(char *pkg)
{
    char *ch, tmp[PATH_MAX], tmp2[PATH_MAX], *latest = NULL;
    Package plist;
    struct index_entry *ie;
    FILE *fp;
    size_t len;

    /* Suck in the contents list. */
    plist.head = plist.tail = NULL;
    plist.name = plist.origin = NULL;
    snprintf(tmp, PATH_MAX, "%s/%s/%s", LOG_DIR, pkg, CONTENTS_FNAME);
    fp = fopen(tmp, "r");
    if (!fp) {
	warnx("unable to open %s file", CONTENTS_FNAME);
	return 1;
    }
    read_plist(&plist, fp);
    fclose(fp);
    if (plist.name == NULL) {
    	warnx("%s does not appear to be a valid package!", pkg);
    	return 1;
    }

    /*
     * First we check if the installed package has an origin, and try
     * looking for it's Makefile. If we find the Makefile we get the
     * latest version from there. If we fail, we start looking in the
     * INDEX, first matching the origin and then the package name.
     */
    if (plist.origin != NULL) {
	snprintf(tmp, PATH_MAX, "%s/%s", PORTS_DIR, plist.origin);
	if (isdir(tmp) && chdir(tmp) != FAIL && isfile("Makefile")) {
	    if ((latest = vpipe("make -V PKGNAME", tmp)) == NULL)
		warnx("Failed to get PKGNAME from %s/Makefile!", tmp);
	    else
		show_version(plist.name, latest, "port");
	}
    }
    if (latest == NULL) {
	/* We only pull in the INDEX once, if needed. */
	if (SLIST_EMPTY(&Index)) {
	    if (!IndexFile)
		errx(2, "Unable to open INDEX in %s.", __func__);
	    while ((ch = fgetln(IndexFile, &len)) != NULL) {
		/*
		 * Don't use strlcpy() because fgetln() doesn't
		 * return a valid C string.
		 */
		strncpy(tmp, ch, MIN(len, PATH_MAX));
		tmp[PATH_MAX-1] = '\0';
		/* The INDEX has pkgname|portdir|... */
		if ((ch = strchr(tmp, '|')) != NULL)
		    ch[0] = '\0';
		if (ch != NULL && (ch = strchr(&ch[1], '|')) != NULL)
		    ch[0] = '\0';
		/* Look backwards for the last two dirs = origin */
		while (ch != NULL && *--ch != '/')
		    if (ch[0] == '\0')
			ch = NULL;
		while (ch != NULL && *--ch != '/')
		    if (ch[0] == '\0')
			ch = NULL;
		if (ch == NULL)
		    errx(2, "The INDEX does not appear to be valid!");
		if ((ie = malloc(sizeof(struct index_entry))) == NULL)
		    errx(2, "Unable to allocate memory in %s.", __func__);
		bzero(ie, sizeof(struct index_entry));
		ie->name = strdup(tmp);
		ie->origin = strdup(&ch[1]);
		/* Who really cares if we reverse the index... */
		SLIST_INSERT_HEAD(&Index, ie, next);
	    }
	}
	/* Now that we've slurped in the INDEX... */
	SLIST_FOREACH(ie, &Index, next) {
	    if (plist.origin != NULL) {
		if (strcmp(plist.origin, ie->origin) == 0)
		    latest = strdup(ie->name);
	    } else {
		strlcpy(tmp, ie->name, PATH_MAX);
		strlcpy(tmp2, plist.name, PATH_MAX);
		/* Chop off the versions and compare. */
		if ((ch = strrchr(tmp, '-')) == NULL)
		    errx(2, "The INDEX does not appear to be valid!");
		ch[0] = '\0';
		if ((ch = strrchr(tmp2, '-')) == NULL)
		    warnx("%s is not a valid package!", plist.name);
		else
		    ch[0] = '\0';
		if (strcmp(tmp2, tmp) == 0) {
		    if (latest != NULL) {
			/* Multiple matches */
			snprintf(tmp, PATH_MAX, "%s|%s", latest, ie->name);
			free(latest);
			latest = strdup(tmp);
		    } else
			latest = strdup(ie->name);
		}
	    }
	}
	if (latest == NULL)
	    show_version(plist.name, NULL, plist.origin);
	else
	    show_version(plist.name, latest, "index");
    }
    if (latest != NULL)
	free(latest);
    free_plist(&plist);
    return 0;
}

#define OUTPUT(c) ((PreventChars != NULL && !strchr(PreventChars, (c))) || \
			(LimitChars != NULL && strchr(LimitChars, (c))) || \
			(PreventChars == NULL && LimitChars == NULL))

/*
 * Do the work of comparing and outputing. Ugly, but well that's what
 * You get when you try to match perl output in C ;-).
 */
void
show_version(const char *installed, const char *latest, const char *source)
{
    char *ch, tmp[PATH_MAX];
    const char *ver;
    int cmp = 0;

    if (!installed || strlen(installed) == 0)
	return;
    strlcpy(tmp, installed, PATH_MAX);
    if (!Verbose) {
	if ((ch = strrchr(tmp, '-')) != NULL)
	    ch[0] = '\0';
    }
    if (latest == NULL) {
	if (source == NULL && OUTPUT('!')) {
	    printf("%-34s  !", tmp);
	    if (Verbose)
		printf("   Comparison failed");
	    printf("\n");
	} else if (source != NULL && OUTPUT('?')) {
	    printf("%-34s  ?", tmp);
	    if (Verbose)
		printf("   orphaned: %s", source);
	    printf("\n");
	}
    } else if (strchr(latest,'|') != NULL) {
	if (OUTPUT('*')) {
	    printf("%-34s  *", tmp);
	    if (Verbose) {
		strlcpy(tmp, latest, PATH_MAX);
		ch = strchr(tmp, '|');
		ch[0] = '\0';

		ver = version_of(tmp, NULL, NULL);
		printf("   multiple versions (index has %s", ver);
		do {
		    ver = version_of(&ch[1], NULL, NULL);
		    if ((ch = strchr(&ch[1], '|')) != NULL)
			    ch[0] = '\0';
		    printf(", %s", ver);
		} while (ch != NULL);
		printf(")");
	    }
	    printf("\n");
	}
    } else {
	cmp = version_cmp(installed, latest);
	ver = version_of(latest, NULL, NULL);
	if (cmp < 0 && OUTPUT('<')) {
	    printf("%-34s  <", tmp);
	    if (Verbose)
		printf("   needs updating (%s has %s)", source, ver);
	    printf("\n");
	} else if (cmp == 0 && OUTPUT('=')) {
	    printf("%-34s  =", tmp);
	    if (Verbose)
		printf("   up-to-date with %s", source);
	    printf("\n");
	} else if (cmp > 0 && OUTPUT('>')) {
	    printf("%-34s  >", tmp);
	    if (Verbose)
		printf("   succeeds %s (%s has %s)", source, source, ver);
	    printf("\n");
	}
    }
}

void
cleanup(int sig)
{
    if (sig)
	exit(1);
}
