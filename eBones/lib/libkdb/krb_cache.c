/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This is where a cache would be implemented, if it were necessary.
 *
 *	from: krb_cache.c,v 4.5 89/01/24 18:12:34 jon Exp $
 *	$Id: krb_cache.c,v 1.3 1995/07/18 16:37:12 mark Exp $
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$Id: krb_cache.c,v 1.3 1995/07/18 16:37:12 mark Exp $";
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
kerb_cache_get_principal(char *serv, char *inst, Principal *principal,
    unsigned int max)
{
    int     found = 0;

    if (!init)
	kerb_cache_init();
#ifdef DEBUG
    if (kerb_debug & 2)
	fprintf(stderr, "cache_get_principal for %s %s max = %d\n",
	    serv, inst, max);
#endif DEBUG

#ifdef DEBUG
    if (kerb_debug & 2) {
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
kerb_cache_put_principal(Principal *principal, unsigned int max)
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
kerb_cache_get_dba(char *serv, char *inst, Dba *dba, unsigned int max)
{
    int     found = 0;

    if (!init)
	kerb_cache_init();

#ifdef DEBUG
    if (kerb_debug & 2)
	fprintf(stderr, "cache_get_dba for %s %s max = %d\n",
	    serv, inst, max);
#endif

#ifdef DEBUG
    if (kerb_debug & 2) {
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
kerb_cache_put_dba(Dba *dba, unsigned int max)
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
