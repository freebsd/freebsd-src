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

/*
 * This is where a cache would be implemented, if it were necessary.
 */

#include "kdb_locl.h"

RCSID("$Id: krb_cache.c,v 1.7 1998/06/09 19:25:14 joda Exp $");

#ifdef DEBUG
extern int debug;
extern long kerb_debug;
#endif
static int init = 0;

/*
 * initialization routine for cache 
 */

int
kerb_cache_init(void)
{
    init = 1;
    return (0);
}

/*
 * look up a principal in the cache returns number of principals found 
 */

int
kerb_cache_get_principal(char *serv, /* could have wild card */
			 char *inst, /* could have wild card */
			 Principal *principal,
			 unsigned int max) /* max number of name structs to return */
{
    int     found = 0;

    if (!init)
	kerb_cache_init();
#ifdef DEBUG
    if (kerb_debug & 2)
	fprintf(stderr, "cache_get_principal for %s %s max = %d\n",
	    serv, inst, max);
#endif /* DEBUG */
    
#ifdef DEBUG
    if (kerb_debug & 2) {
	if (found) {
	    fprintf(stderr, "cache get %s %s found %s %s sid = %d\n",
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
kerb_cache_put_principal(Principal *principal,
			 unsigned int max)
                     		/* max number of principal structs to
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
kerb_cache_get_dba(char *serv,	/* could have wild card */
		   char *inst,	/* could have wild card */
		   Dba *dba,
		   unsigned int max) /* max number of name structs to return */
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
	    fprintf(stderr, "cache get %s %s found %s %s sid = %d\n",
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
kerb_cache_put_dba(Dba *dba,
		   unsigned int max)
                     		/* max number of dba structs to insert */
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

