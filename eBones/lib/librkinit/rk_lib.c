/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/lib/librkinit/rk_lib.c,v $
 * $Author: pst $
 *
 * This file contains the non-rpc top-level rkinit library routines.
 * The routines in the rkinit library that should be called from clients
 * are exactly those defined in this file.
 *
 * The naming convetions used within the rkinit library are as follows:
 * Functions intended for general client use start with rkinit_
 * Functions intended for use only inside the library or server start with
 * rki_
 * Functions that do network communcation start with rki_rpc_
 * Static functions can be named in any fashion.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <krb.h>

#include <rkinit.h>
#include <rkinit_private.h>
#include <rkinit_err.h>

#ifdef __STDC__
char *rkinit_errmsg(char *string)
#else
char *rkinit_errmsg(string)
  char *string;
#endif /* __STDC__ */
{
    static char errmsg[BUFSIZ];

    if (string) {
	BCLEAR(errmsg);
	strncpy(errmsg, string, sizeof(errmsg) - 1);
    }

    return(errmsg);
}

#ifdef __STDC__
int rkinit(char *host, char *r_krealm, rkinit_info *info, int timeout)
#else
int rkinit(host, r_krealm, info, timeout)
  char *host;
  char *r_krealm;
  rkinit_info *info;
  int timeout;
#endif /* __STDC__ */
{
    int status = RKINIT_SUCCESS;
    int version = 0;
    char phost[MAXHOSTNAMELEN];
    jmp_buf timeout_env;
    void (*old_alrm)(int) = NULL;
    char origtktfilename[MAXPATHLEN]; /* original ticket file name */
    char tktfilename[MAXPATHLEN]; /* temporary client ticket file */

    BCLEAR(phost);
    BCLEAR(origtktfilename);
    BCLEAR(tktfilename);
    BCLEAR(timeout_env);

    init_rkin_err_tbl();

    if ((status = rki_setup_rpc(host)))
	return(status);	

    if (timeout)
	old_alrm = rki_setup_timer(timeout_env);
	
    /* The alarm handler longjmps us to here. */
    if ((status = setjmp(timeout_env)) == 0) {

	strcpy(origtktfilename, tkt_string());
	sprintf(tktfilename, "/tmp/tkt_rkinit.%d", getpid());
	krb_set_tkt_string(tktfilename);

	if ((status = rki_choose_version(&version)) == RKINIT_SUCCESS)
	    status = rki_get_tickets(version, host, r_krealm, info);
    }
    
    if (timeout)
	rki_restore_timer(old_alrm);

    dest_tkt();
    krb_set_tkt_string(origtktfilename);

    rki_cleanup_rpc();

    return(status);
}
