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
 * Maxim Sobolev
 * 24 February 2001
 *
 * Routines used to query installed packages.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>
#include <fnmatch.h>
#include <fts.h>
#include <regex.h>

/*
 * Simple structure representing argv-like
 * NULL-terminated list.
 */
struct store {
    int currlen;
    int used;
    char **store;
};

static int rex_match(const char *, const char *);
struct store *storecreate(struct store *);
static int storeappend(struct store *, const char *);
static int fname_cmp(const FTSENT * const *, const FTSENT * const *);

/*
 * Function to query names of installed packages.
 * MatchType	- one of MATCH_ALL, MATCH_REGEX, MATCH_GLOB;
 * patterns	- NULL-terminated list of glob or regex patterns
 *		  (could be NULL for MATCH_ALL);
 * retval	- return value (could be NULL if you don't want/need
 *		  return value).
 * Returns NULL-terminated list with matching names.
 * Names in list returned are dynamically allocated and should
 * not be altered by the caller.
 */
char **
matchinstalled(match_t MatchType, char **patterns, int *retval)
{
    int i, errcode, len;
    char *matched;
    const char *paths[2] = {LOG_DIR, NULL};
    static struct store *store = NULL;
    FTS *ftsp;
    FTSENT *f;
    Boolean *lmatched;

    store = storecreate(store);
    if (store == NULL) {
	if (retval != NULL)
	    *retval = 1;
	return NULL;
    }

    if (retval != NULL)
	*retval = 0;

    if (!isdir(paths[0])) {
	if (retval != NULL)
	    *retval = 1;
	return NULL;
	/* Not reached */
    }

    /* Count number of patterns */
    if (patterns != NULL) {
	for (len = 0; patterns[len]; len++) {}
	lmatched = alloca(sizeof(*lmatched) * len);
	if (lmatched == NULL) {
	    warnx("%s(): alloca() failed", __func__);
	    if (retval != NULL)
		*retval = 1;
	    return NULL;
    	} 
    } else
	len = 0;
    
    for (i = 0; i < len; i++)
	lmatched[i] = FALSE;

    ftsp = fts_open((char * const *)(uintptr_t)paths, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT, fname_cmp);
    if (ftsp != NULL) {
	while ((f = fts_read(ftsp)) != NULL) {
	    if (f->fts_info == FTS_D && f->fts_level == 1) {
		fts_set(ftsp, f, FTS_SKIP);
		matched = NULL;
		errcode = 0;
		if (MatchType == MATCH_ALL)
		    matched = f->fts_name;
		else 
		    for (i = 0; patterns[i]; i++) {
			switch (MatchType) {
			case MATCH_REGEX:
			    errcode = rex_match(patterns[i], f->fts_name);
			    if (errcode == 1) {
				matched = f->fts_name;
				errcode = 0;
			    }
			    break;
			case MATCH_GLOB:
			    if (fnmatch(patterns[i], f->fts_name, 0) == 0) {
				matched = f->fts_name;
				lmatched[i] = TRUE;
			    }
			    break;
			default:
			    break;
			}
			if (matched != NULL || errcode != 0)
			    break;
		    }
		if (errcode == 0 && matched != NULL)
		    errcode = storeappend(store, matched);
		if (errcode != 0) {
		    if (retval != NULL)
			*retval = 1;
		    return NULL;
		    /* Not reached */
		}
	    }
	}
	fts_close(ftsp);
    }

    if (MatchType == MATCH_GLOB) {
	for (i = 0; i < len; i++)
	    if (lmatched[i] == FALSE)
		storeappend(store, patterns[i]);
    }

    if (store->used == 0)
	return NULL;
    else
	return store->store;
}

/*
 * Synopsis is similar to matchinstalled(), but use origin
 * as a key for matching packages.
 */
char **
matchbyorigin(const char *origin, int *retval)
{
    char **installed;
    int i;
    static struct store *store = NULL;

    store = storecreate(store);
    if (store == NULL) {
	if (retval != NULL)
	    *retval = 1;
	return NULL;
    }

    if (retval != NULL)
	*retval = 0;

    installed = matchinstalled(MATCH_ALL, NULL, retval);
    if (installed == NULL)
	return NULL;

    for (i = 0; installed[i] != NULL; i++) {
	FILE *fp;
	char *cp, tmp[PATH_MAX];
	int cmd;

	snprintf(tmp, PATH_MAX, "%s/%s", LOG_DIR, installed[i]);
	/*
	 * SPECIAL CASE: ignore empty dirs, since we can can see them
	 * during port installation.
	 */
	if (isemptydir(tmp))
	    continue;
	snprintf(tmp, PATH_MAX, "%s/%s", tmp, CONTENTS_FNAME);
	fp = fopen(tmp, "r");
	if (fp == NULL) {
	    warn("%s", tmp);
	    if (retval != NULL)
		*retval = 1;
	    return NULL;
	}

	cmd = -1;
	while (fgets(tmp, sizeof(tmp), fp)) {
	    int len = strlen(tmp);

	    while (len && isspace(tmp[len - 1]))
		tmp[--len] = '\0';
	    if (!len)
		continue;
	    cp = tmp;
	    if (tmp[0] != CMD_CHAR)
		continue;
	    cmd = plist_cmd(tmp + 1, &cp);
	    if (cmd == PLIST_ORIGIN) {
		if (strcmp(origin, cp) == 0)
		    storeappend(store, installed[i]);
		break;
	    }
	}
	if (cmd != PLIST_ORIGIN)
	    warnx("package %s has no origin recorded", installed[i]);
	fclose(fp);
    }

    if (store->used == 0)
	return NULL;
    else
	return store->store;
}

/*
 * Return TRUE if the specified package is installed,
 * or FALSE otherwise.
 */
int
isinstalledpkg(const char *name)
{
    char buf[FILENAME_MAX];

    snprintf(buf, sizeof(buf), "%s/%s", LOG_DIR, name);
    if (!isdir(buf) || access(buf, R_OK) == FAIL)
	return FALSE;

    snprintf(buf, sizeof(buf), "%s/%s", buf, CONTENTS_FNAME);
    if (!isfile(buf) || access(buf, R_OK) == FAIL)
	return FALSE;

    return TRUE;
}

/*
 * Returns 1 if specified pkgname matches RE pattern.
 * Otherwise returns 0 if doesn't match or -1 if RE
 * engine reported an error (usually invalid syntax).
 */
static int
rex_match(const char *pattern, const char *pkgname)
{
    char errbuf[128];
    int errcode;
    int retval;
    regex_t rex;

    retval = 0;

    errcode = regcomp(&rex, pattern, REG_BASIC | REG_NOSUB);
    if (errcode == 0)
	errcode = regexec(&rex, pkgname, 0, NULL, 0);

    if (errcode == 0) {
	retval = 1;
    } else if (errcode != REG_NOMATCH) {
	regerror(errcode, &rex, errbuf, sizeof(errbuf));
	warnx("%s: %s", pattern, errbuf);
	retval = -1;
    }

    regfree(&rex);

    return retval;
}

/*
 * Create an empty store, optionally deallocating
 * any previously allocated space if store != NULL.
 */
struct store *
storecreate(struct store *store)
{
    int i;

    if (store == NULL) {
	store = malloc(sizeof *store);
	if (store == NULL) {
	    warnx("%s(): malloc() failed", __func__);
	    return NULL;
	}
	store->currlen = 0;
	store->store = NULL;
    } else if (store->store != NULL) {
	    /* Free previously allocated memory */
	    for (i = 0; store->store[i] != NULL; i++)
		free(store->store[i]);
	    store->store[0] = NULL;
    }
    store->used = 0;

    return store;
}

/*
 * Append specified element to the provided store.
 */
static int
storeappend(struct store *store, const char *item)
{
    if (store->used + 2 > store->currlen) {
	store->currlen += 16;
	store->store = reallocf(store->store,
				store->currlen * sizeof(*(store->store)));
	if (store->store == NULL) {
	    store->currlen = 0;
	    warnx("%s(): reallocf() failed", __func__);
	    return 1;
	}
    }

    asprintf(&(store->store[store->used]), "%s", item);
    if (store->store[store->used] == NULL) {
	warnx("%s(): malloc() failed", __func__);
	return 1;
    }
    store->used++;
    store->store[store->used] = NULL;

    return 0;
}

static int
fname_cmp(const FTSENT * const *a, const FTSENT * const *b)
{
    return strcmp((*a)->fts_name, (*b)->fts_name);
}
