/*
 * $Source: /home/ncvs/src/eBones/lib/libkdb/krb_lib.c,v $
 * $Author: markm $
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <strings.h>
#include <des.h>
#include <krb.h>
#include <krb_db.h>

#ifdef DEBUG
extern int debug;
extern char *progname;
long    kerb_debug;
#endif

static  init = 0;

/*
 * initialization routine for data base
 */

int
kerb_init()
{
#ifdef DEBUG
    if (!init) {
	char *dbg = getenv("KERB_DBG");
	if (dbg)
	    sscanf(dbg, "%ld", &kerb_debug);
	init = 1;
    }
#endif
    kerb_db_init();

#ifdef CACHE
    kerb_cache_init();
#endif

    /* successful init, return 0, else errcode */
    return (0);
}

/*
 * finalization routine for database -- NOTE: MUST be called by any
 * program using kerb_init.  ALSO will have to be modified to finalize
 * caches, if they're ever really implemented.
 */

void
kerb_fini()
{
    kerb_db_fini();
}

/*
 * look up a principal in the cache or data base returns number of
 * principals found
 */

int
kerb_get_principal(name, inst, principal, max, more)
    char   *name;		/* could have wild card */
    char   *inst;		/* could have wild card */
    Principal *principal;
    unsigned int max;		/* max number of name structs to return */
    int    *more;		/* more tuples than room for */

{
    int     found = 0;
#ifdef CACHE
    static int wild = 0;
#endif
    if (!init)
	kerb_init();

#ifdef DEBUG
    if (kerb_debug & 1)
	fprintf(stderr, "\n%s: kerb_get_principal for %s %s max = %d\n",
	    progname, name, inst, max);
#endif

    /*
     * if this is a request including a wild card, have to go to db
     * since the cache may not be exhaustive.
     */

    /* clear the principal area */
    bzero((char *) principal, max * sizeof(Principal));

#ifdef CACHE
    /*
     * so check to see if the name contains a wildcard "*" or "?", not
     * preceeded by a backslash.
     */
    wild = 0;
    if (index(name, '*') || index(name, '?') ||
	index(inst, '*') || index(inst, '?'))
	wild = 1;

    if (!wild) {
	/* try the cache first */
	found = kerb_cache_get_principal(name, inst, principal, max, more);
	if (found)
	    return (found);
    }
#endif
    /* If we didn't try cache, or it wasn't there, try db */
    found = kerb_db_get_principal(name, inst, principal, max, more);
    /* try to insert principal(s) into cache if it was found */
#ifdef CACHE
    if (found) {
	kerb_cache_put_principal(principal, found);
    }
#endif
    return (found);
}

/* principals */
int
kerb_put_principal(principal, n)
    Principal *principal;
    unsigned int n;		/* number of principal structs to write */
{
    long time();
    struct tm *tp, *localtime();

    /* set mod date */
    principal->mod_date = time((long *)0);
    /* and mod date string */

    tp = localtime(&principal->mod_date);
    (void) sprintf(principal->mod_date_txt, "%4d-%2d-%2d",
		   tp->tm_year > 1900 ? tp->tm_year : tp->tm_year + 1900,
		   tp->tm_mon + 1, tp->tm_mday); /* January is 0, not 1 */
#ifdef DEBUG
    if (kerb_debug & 1) {
	int i;
	fprintf(stderr, "\nkerb_put_principal...");
	for (i = 0; i < n; i++) {
	    krb_print_principal(&principal[i]);
	}
    }
#endif
    /* write database */
    if (kerb_db_put_principal(principal, n) < 0) {
#ifdef DEBUG
	if (kerb_debug & 1)
	    fprintf(stderr, "\n%s: kerb_db_put_principal err", progname);
	/* watch out for cache */
#endif
	return -1;
    }
#ifdef CACHE
    /* write cache */
    if (!kerb_cache_put_principal(principal, n)) {
#ifdef DEBUG
	if (kerb_debug & 1)
	    fprintf(stderr, "\n%s: kerb_cache_put_principal err", progname);
#endif
	return -1;
    }
#endif
    return 0;
}

int
kerb_get_dba(name, inst, dba, max, more)
    char   *name;		/* could have wild card */
    char   *inst;		/* could have wild card */
    Dba    *dba;
    unsigned int max;		/* max number of name structs to return */
    int    *more;		/* more tuples than room for */

{
    int     found = 0;
#ifdef CACHE
    static int wild = 0;
#endif
    if (!init)
	kerb_init();

#ifdef DEBUG
    if (kerb_debug & 1)
	fprintf(stderr, "\n%s: kerb_get_dba for %s %s max = %d\n",
	    progname, name, inst, max);
#endif
    /*
     * if this is a request including a wild card, have to go to db
     * since the cache may not be exhaustive.
     */

    /* clear the dba area */
    bzero((char *) dba, max * sizeof(Dba));

#ifdef CACHE
    /*
     * so check to see if the name contains a wildcard "*" or "?", not
     * preceeded by a backslash.
     */

    wild = 0;
    if (index(name, '*') || index(name, '?') ||
	index(inst, '*') || index(inst, '?'))
	wild = 1;

    if (!wild) {
	/* try the cache first */
	found = kerb_cache_get_dba(name, inst, dba, max, more);
	if (found)
	    return (found);
    }
#endif
    /* If we didn't try cache, or it wasn't there, try db */
    found = kerb_db_get_dba(name, inst, dba, max, more);
#ifdef CACHE
    /* try to insert dba(s) into cache if it was found */
    if (found) {
	kerb_cache_put_dba(dba, found);
    }
#endif
    return (found);
}
