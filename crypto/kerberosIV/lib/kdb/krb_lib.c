/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "kdb_locl.h"

RCSID("$Id: krb_lib.c,v 1.13 1998/11/22 09:41:43 assar Exp $");

#ifdef DEBUG
extern int debug;
extern char *progname;
long    kerb_debug;
#endif

static int init = 0;

/*
 * initialization routine for data base 
 */

int
kerb_init(void)
{
#ifdef DEBUG
    if (!init) {
	char *dbg = getenv("KERB_DBG");
	if (dbg)
	    sscanf(dbg, "%d", &kerb_debug);
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
kerb_fini(void)
{
    kerb_db_fini();
}


int
kerb_delete_principal(char *name, char *inst)
{
    int ret;
    
    if (!init)
	kerb_init();

    ret = kerb_db_delete_principal(name, inst);
#ifdef CACHE
    if(ret == 0){
	kerb_cache_delete_principal(name, inst);
    }
#endif
    return ret;
}


/*
 * look up a principal in the cache or data base returns number of
 * principals found 
 */

int
kerb_get_principal(char *name,	/* could have wild card */
		   char *inst,	/* could have wild card */
		   Principal *principal,
		   unsigned int max, /* max number of name structs to return */
		   int *more)	/* more tuples than room for */
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
    memset(principal, 0, max * sizeof(Principal));

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
    if (found > 0) {
	kerb_cache_put_principal(principal, found);
    }
#endif
    return (found);
}

/* principals */
int
kerb_put_principal(Principal *principal,
		   unsigned int n)                         
                   		/* number of principal structs to write */
{
    /* set mod date */
    principal->mod_date = time((time_t *)0);
    /* and mod date string */

    strftime(principal->mod_date_txt, 
	     sizeof(principal->mod_date_txt), 
	     "%Y-%m-%d", k_localtime(&principal->mod_date));
    strftime(principal->exp_date_txt, 
	     sizeof(principal->exp_date_txt), 
	     "%Y-%m-%d", k_localtime(&principal->exp_date));
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
kerb_get_dba(char *name,	/* could have wild card */
	     char *inst,	/* could have wild card */
	     Dba *dba,
	     unsigned int max,	/* max number of name structs to return */
	     int *more)		/* more tuples than room for */
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
    memset(dba, 0, max * sizeof(Dba));

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
