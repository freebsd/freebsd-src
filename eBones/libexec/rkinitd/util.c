/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/libexec/rkinitd/util.c,v $
 * $Author: gibbs $
 *
 * This file contains general rkinit server utilities.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <rkinit.h>
#include <rkinit_err.h>
#include <rkinit_private.h>

#include "rkinitd.h"

static char errbuf[BUFSIZ];

void rpc_exchange_version_info();
void error();

#ifdef __STDC__
int choose_version(int *version)
#else
int choose_version(version)
  int *version;
#endif /* __STDC__ */
{
    int c_lversion;		/* lowest version number client supports */
    int c_hversion;		/* highest version number client supports */
    int status = RKINIT_SUCCESS;
    
    rpc_exchange_version_info(&c_lversion, &c_hversion,
				  RKINIT_LVERSION, RKINIT_HVERSION);
    
    *version = min(RKINIT_HVERSION, c_hversion);
    if (*version < max(RKINIT_LVERSION, c_lversion)) {
	sprintf(errbuf, 
		"Can't run version %d client against version %d server.",
		c_hversion, RKINIT_HVERSION);
	rkinit_errmsg(errbuf);
	return(RKINIT_VERSION);
    }

    return(status);
}
