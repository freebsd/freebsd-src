#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

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

static int rex_match(char *, char *);
static void storeappend(struct store *, const char *);
static int fname_cmp(const FTSENT **, const FTSENT **);

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
    int i, matched, errcode;
    char *tmp;
    char *paths[2];
    static struct store *store = NULL;
    FTS *ftsp;
    FTSENT *f;

    if (store == NULL) {
	store = malloc(sizeof *store);
	store->currlen = 0;
	store->store = NULL;
    } else {
	if (store->store != NULL) {
	    /* Free previously allocated memory */
	    for (i = 0; store->store[i] != NULL; i++)
		free(store->store[i]);
	}
    }
    store->used = 0;

    if (retval != NULL)
	*retval = 0;

    tmp = getenv(PKG_DBDIR);
    if (!tmp)
	tmp = DEF_LOG_DIR;
    if (!isdir(tmp)) {
	if (retval != NULL)
	    *retval = 1;
	return NULL;
	/* Not reached */
    }

    paths[0] = tmp;
    paths[1] = NULL;
    ftsp = fts_open(paths, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT, fname_cmp);
    if (ftsp != NULL) {
	while ((f = fts_read(ftsp)) != NULL) {
	    if (f->fts_info == FTS_D && f->fts_level == 1) {
		fts_set(ftsp, f, FTS_SKIP);
		if (MatchType == MATCH_ALL) {
		    storeappend(store, f->fts_name);
		    continue;
		}
		for (i = 0; patterns[i]; i++) {
		    matched = 0;
		    switch (MatchType) {
		    case MATCH_REGEX:
			errcode = rex_match(patterns[i], f->fts_name);
			if (errcode == 1) {
			    storeappend(store, f->fts_name);
			    matched = 1;
			} else if (errcode == -1) {
			    if (retval != NULL)
				*retval = 1;
			    return NULL;
			    /* Not reached */
			}
			break;
		    case MATCH_GLOB:
			if (fnmatch(patterns[i], f->fts_name, 0) == 0) {
			    storeappend(store, f->fts_name);
			    matched = 1;
			}
			break;
		    default:
			break;
		    }
		    if (matched == 1)
			break;
		}
	    }
	}
	fts_close(ftsp);
    }

    if (store->used == 0)
	return NULL;
    else
	return store->store;
}

/*
 * Returns 1 if specified pkgname matches RE pattern.
 * Otherwise returns 0 if doesn't match or -1 if RE
 * engine reported an error (usually invalid syntax).
 */
static int
rex_match(char *pattern, char *pkgname)
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

static void
storeappend(struct store *store, const char *item)
{
    char **tmp;

    if (store->used + 2 > store->currlen) {
	tmp = store->store;
	store->currlen += 16;
	store->store = malloc(store->currlen * sizeof(*(store->store)));
	memcpy(store->store, tmp, store->used * sizeof(*(store->store)));
	free(tmp);
    }

    asprintf(&(store->store[store->used]), "%s", item);
    store->used++;
    store->store[store->used] = NULL;
}

static int
fname_cmp(const FTSENT **a, const FTSENT **b)
{
    return strcmp((*a)->fts_name, (*b)->fts_name);
}
