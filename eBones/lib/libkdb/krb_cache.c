/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This is where a cache would be implemented, if it were necessary.
 *
 *	from: krb_cache.c,v 4.5 89/01/24 18:12:34 jon Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <stdio.h>
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
extern long kerb_debug;
#endif
static  init = 0;

/*
 * initialization routine for cache
 */

int
kerb_cache_init()
{
    init = 1;
    return (0);
}

/*
 * look up a principal in the cache returns number of principals found
 */

int
kerb_cache_get_principal(serv, inst, principal, max)
    char   *serv;		/* could have wild card */
    char   *inst;		/* could have wild card */
    Principal *principal;
    unsigned int max;		/* max number of name structs to return */

{
    int     found = 0;

    if (!init)
	kerb_cache_init();
#ifdef DEBUG
    if (kerb_debug & 2) {
	fprintf(stderr, "cache_get_principal for %s %s max = %d\n",
	    serv, inst, max);
	if (found) {
	    fprintf(stderr, "cache get %s %s found %s %s\n",
		serv, inst, principal->name, principal->instance);
	} else {
	    fprintf(stderr, "cache %s %s not found\n", serv,
		inst);
	}
    }
#endif
    return (found);
}

/*
 * insert/replace a principal in the cache returns number of principals
 * inserted
 */

int
kerb_cache_put_principal(principal, max)
    Principal *principal;
    unsigned int max;		/* max number of principal structs to
				 * insert */

{
    u_long  i;
    int     count = 0;

    if (!init)
	kerb_cache_init();

#ifdef DEBUG
    if (kerb_debug & 2) {
	fprintf(stderr, "kerb_cache_put_principal  max = %d",
	    max);
    }
#endif

    for (i = 0; i < max; i++) {
#ifdef DEBUG
	if (kerb_debug & 2)
	    fprintf(stderr, "\n %s %s",
		    principal->name, principal->instance);
#endif
	/* DO IT */
	count++;
	principal++;
    }
    return count;
}

/*
 * look up a dba in the cache returns number of dbas found
 */

int
kerb_cache_get_dba(serv, inst, dba, max)
    char   *serv;		/* could have wild card */
    char   *inst;		/* could have wild card */
    Dba    *dba;
    unsigned int max;		/* max number of name structs to return */

{
    int     found = 0;

    if (!init)
	kerb_cache_init();

#ifdef DEBUG
    if (kerb_debug & 2) {
	fprintf(stderr, "cache_get_dba for %s %s max = %d\n",
	    serv, inst, max);
	if (found) {
	    fprintf(stderr, "cache get %s %s found %s %s\n",
		serv, inst, dba->name, dba->instance);
	} else {
	    fprintf(stderr, "cache %s %s not found\n", serv, inst);
	}
    }
#endif
    return (found);
}

/*
 * insert/replace a dba in the cache returns number of dbas inserted
 */

int
kerb_cache_put_dba(dba, max)
    Dba    *dba;
    unsigned int max;		/* max number of dba structs to insert */

{
    u_long  i;
    int     count = 0;

    if (!init)
	kerb_cache_init();
#ifdef DEBUG
    if (kerb_debug & 2) {
	fprintf(stderr, "kerb_cache_put_dba  max = %d", max);
    }
#endif
    for (i = 0; i < max; i++) {
#ifdef DEBUG
	if (kerb_debug & 2)
	    fprintf(stderr, "\n %s %s",
		    dba->name, dba->instance);
#endif
	/* DO IT */
	count++;
	dba++;
    }
    return count;
}

