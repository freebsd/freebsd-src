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

static int rex_match(const char *, const char *, int);
static int csh_match(const char *, const char *, int);
struct store *storecreate(struct store *);
static int storeappend(struct store *, const char *);
static int fname_cmp(const FTSENT * const *, const FTSENT * const *);

/*
 * Function to query names of installed packages.
 * MatchType	- one of MATCH_ALL, MATCH_EREGEX, MATCH_REGEX, MATCH_GLOB, MATCH_NGLOB;
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
    Boolean *lmatched = NULL;

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
			errcode = pattern_match(MatchType, patterns[i], f->fts_name);
			if (errcode == 1) {
			    matched = f->fts_name;
			    lmatched[i] = TRUE;
			    errcode = 0;
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

int
pattern_match(match_t MatchType, char *pattern, const char *pkgname)
{
    int errcode = 0;
    const char *fname = pkgname;
    char basefname[PATH_MAX];
    char condchar = '\0';
    char *condition;

    /* do we have an appended condition? */
    condition = strpbrk(pattern, "<>=");
    if (condition) {
	const char *ch;
	/* yes, isolate the pattern from the condition ... */
	if (condition > pattern && condition[-1] == '!')
	    condition--;
	condchar = *condition;
	*condition = '\0';
	/* ... and compare the name without version */
	ch = strrchr(fname, '-');
	if (ch && ch - fname < PATH_MAX) {
	    strlcpy(basefname, fname, ch - fname + 1);
	    fname = basefname;
	}
    }

    switch (MatchType) {
    case MATCH_EREGEX:
    case MATCH_REGEX:
	errcode = rex_match(pattern, fname, MatchType == MATCH_EREGEX ? 1 : 0);
	break;
    case MATCH_NGLOB:
    case MATCH_GLOB:
	errcode = (csh_match(pattern, fname, 0) == 0) ? 1 : 0;
	break;
    case MATCH_EXACT:
	errcode = (strcmp(pattern, fname) == 0) ? 1 : 0;
	break;
    case MATCH_ALL:
	errcode = 1;
	break;
    default:
	break;
    }

    /* loop over all appended conditions */
    while (condition) {
	/* restore the pattern */
	*condition = condchar;
	/* parse the condition (fun with bits) */
	if (errcode == 1) {
	    char *nextcondition;
	    /* compare version numbers */
	    int match = 0;
	    if (*++condition == '=') {
		match = 2;
		condition++;
	    }
	    switch(condchar) {
	    case '<':
		match |= 1;
		break;
	    case '>':
		match |= 4;
		break;
	    case '=':
		match |= 2;
		break;
	    case '!':
		match = 5;
		break;
	    }
	    /* isolate the version number from the next condition ... */
	    nextcondition = strpbrk(condition, "<>=!");
	    if (nextcondition) {
		condchar = *nextcondition;
		*nextcondition = '\0';
	    }
	    /* and compare the versions (version_cmp removes the filename for us) */
	    if ((match & (1 << (version_cmp(pkgname, condition) + 1))) == 0)
		errcode = 0;
	    condition = nextcondition;
	} else {
	    break;
	}
    }

    return errcode;
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
	    warnx("the package info for package '%s' is corrupt", installed[i]);
	    continue;
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
		if (csh_match(origin, cp, FNM_PATHNAME) == 0)
		    storeappend(store, installed[i]);
		break;
	    }
	}
	if (cmd != PLIST_ORIGIN && ( Verbose || 0 != strncmp("bsdpan-", installed[i], 7 ) ) )
	    warnx("package %s has no origin recorded", installed[i]);
	fclose(fp);
    }

    if (store->used == 0)
	return NULL;
    else
	return store->store;
}

/*
 * Small linked list to memoize results of isinstalledpkg().  A hash table
 * would be faster but for n ~= 1000 may be overkill.
 */
struct iip_memo {
	LIST_ENTRY(iip_memo) iip_link;
	char	*iip_name;
	int	 iip_result;
};
LIST_HEAD(, iip_memo) iip_memo = LIST_HEAD_INITIALIZER(iip_memo);

/*
 * 
 * Return 1 if the specified package is installed,
 * 0 if not, and -1 if an error occured.
 */
int
isinstalledpkg(const char *name)
{
    int result;
    char *buf, *buf2;
    struct iip_memo *memo;

    LIST_FOREACH(memo, &iip_memo, iip_link) {
	if (strcmp(memo->iip_name, name) == 0)
	    return memo->iip_result;
    }
    
    buf2 = NULL;
    asprintf(&buf, "%s/%s", LOG_DIR, name);
    if (buf == NULL)
	goto errout;
    if (!isdir(buf) || access(buf, R_OK) == FAIL) {
	result = 0;
    } else {
	asprintf(&buf2, "%s/%s", buf, CONTENTS_FNAME);
	if (buf2 == NULL)
	    goto errout;

	if (!isfile(buf2) || access(buf2, R_OK) == FAIL)
	    result = -1;
	else
	    result = 1;
    }

    free(buf);
    buf = strdup(name);
    if (buf == NULL)
	goto errout;
    free(buf2);
    buf2 = NULL;

    memo = malloc(sizeof *memo);
    if (memo == NULL)
	goto errout;
    memo->iip_name = buf;
    memo->iip_result = result;
    LIST_INSERT_HEAD(&iip_memo, memo, iip_link);
    return result;

errout:
    if (buf != NULL)
	free(buf);
    if (buf2 != NULL)
	free(buf2);
    return -1;
}

/*
 * Returns 1 if specified pkgname matches RE pattern.
 * Otherwise returns 0 if doesn't match or -1 if RE
 * engine reported an error (usually invalid syntax).
 */
static int
rex_match(const char *pattern, const char *pkgname, int extended)
{
    char errbuf[128];
    int errcode;
    int retval;
    regex_t rex;

    retval = 0;

    errcode = regcomp(&rex, pattern, (extended ? REG_EXTENDED : REG_BASIC) | REG_NOSUB);
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
 * Match string by a csh-style glob pattern. Returns 0 on
 * match and FNM_NOMATCH otherwise, to be compatible with
 * fnmatch(3).
 */
static int
csh_match(const char *pattern, const char *string, int flags)
{
    int ret = FNM_NOMATCH;


    const char *nextchoice = pattern;
    const char *current = NULL;

    int prefixlen = -1;
    int currentlen = 0;

    int level = 0;

    do {
	const char *pos = nextchoice;
	const char *postfix = NULL;

	Boolean quoted = FALSE;

	nextchoice = NULL;

	do {
	    const char *eb;
	    if (!*pos) {
		postfix = pos;
	    } else if (quoted) {
		quoted = FALSE;
	    } else {
		switch (*pos) {
		case '{':
		    ++level;
		    if (level == 1) {
			current = pos+1;
			prefixlen = pos-pattern;
		    }
		    break;
		case ',':
		    if (level == 1 && !nextchoice) {
			nextchoice = pos+1;
			currentlen = pos-current;
		    }
		    break;
		case '}':
		    if (level == 1) {
			postfix = pos+1;
			if (!nextchoice)
			    currentlen = pos-current;
		    }
		    level--;
		    break;
		case '[':
		    eb = pos+1;
		    if (*eb == '!' || *eb == '^')
			eb++;
		    if (*eb == ']')
			eb++;
		    while(*eb && *eb != ']')
			eb++;
		    if (*eb)
			pos=eb;
		    break;
		case '\\':
		    quoted = TRUE;
		    break;
		default:
		    ;
		}
	    }
	    pos++;
	} while (!postfix);

	if (current) {
	    char buf[FILENAME_MAX];
	    snprintf(buf, sizeof(buf), "%.*s%.*s%s", prefixlen, pattern, currentlen, current, postfix);
	    ret = csh_match(buf, string, flags);
	    if (ret) {
		current = nextchoice;
		level = 1;
	    } else
		current = NULL;
	} else
	    ret = fnmatch(pattern, string, flags);
    } while (current);

    return ret;
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
