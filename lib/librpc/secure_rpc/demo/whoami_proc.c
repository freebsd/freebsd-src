#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)whoami_proc.c	2.3 89/07/11 4.0 RPCSRC";
#endif
/*
 * whoami_proc.c: secure identity verifier and reporter: server proc
 */
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <pwd.h>
#include "whoami.h"

extern char *strcpy();

/*
 * Report on the server's notion of the client's identity.
 */
remote_identity *
whoami_iask_1(nullarg, rqstp)
    void           *nullarg;
	struct svc_req *rqstp;
{
static remote_identity   whoisthem;
static char              username[MAXNETNAMELEN+1];
static char              realname[MAXNETNAMELEN+1]; /* really gecos field */
static int               grouplist[NGROUPS];
    char                 publickey[HEXKEYBYTES+1];

    struct authdes_cred *des_cred;
    struct passwd *pwdent;

    switch (rqstp->rq_cred.oa_flavor)
    {
    case AUTH_DES:
        whoisthem.remote_username = username;
        whoisthem.remote_realname = realname;
        whoisthem.gids.gids_val = grouplist;
        des_cred = (struct authdes_cred *) rqstp->rq_clntcred;
        /*
         * Check to see if the netname being used is in the public key
         * database (if not, reject this (potential) imposter).
         */
        if (! getpublickey(des_cred->adc_fullname.name, publickey))
        {
            svcerr_weakauth(rqstp->rq_xprt);
            return(NULL);
        }
        /*
         * Get the info that the client wants.
         */
        if (! netname2user(des_cred->adc_fullname.name, &whoisthem.uid,
                &whoisthem.gid, &whoisthem.gids.gids_len,
                whoisthem.gids.gids_val))
        {                           /* netname not found */
            whoisthem.authenticated = FALSE;
            strcpy(whoisthem.remote_username, "nobody");
            strcpy(whoisthem.remote_realname, "INTERLOPER!");
            whoisthem.uid = -2;
            whoisthem.gid = -2;
            whoisthem.gids.gids_len = 0;
            return(&whoisthem);
        }
        /* else we found the netname */
        whoisthem.authenticated = TRUE;
        pwdent = getpwuid(whoisthem.uid);
        strcpy(whoisthem.remote_username, pwdent->pw_name);
        strcpy(whoisthem.remote_realname, pwdent->pw_gecos);
        return(&whoisthem);
        break;
    case AUTH_UNIX:
    case AUTH_NULL:
    default:
        svcerr_weakauth(rqstp->rq_xprt);
        return(NULL);
    }
}

/*
 * Return server's netname. AUTH_NONE is valid.
 * This routine allows this server to be started under any uid,
 * and the client can ask us our netname for use in authdes_create().
 */
name *
whoami_whoru_1(nullarg, rqstp)
    void           *nullarg;
	struct svc_req *rqstp;
{
static name              whoru;
static char              servername[MAXNETNAMELEN+1];

    whoru = servername;
    getnetname(servername);

    return(&whoru);
}
