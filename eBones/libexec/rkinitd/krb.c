/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/libexec/rkinitd/krb.c,v $
 * $Author: gibbs $
 *
 * This file contains all of the kerberos part of rkinitd.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <krb.h>
#include <des.h>

#include <rkinit.h>
#include <rkinit_private.h>
#include <rkinit_err.h>

#include "rkinitd.h"

#define FAILURE (!RKINIT_SUCCESS)

extern int errno;

static char errbuf[BUFSIZ];

typedef struct {
    jmp_buf env;
} rkinitd_intkt_info;


#if defined(_AIX) && defined(_IBMR2)

#include <sys/id.h>

/*
 * The RIOS has bizzarre ideas about changing uids around.  They are
 * such that the seteuid and setruid calls here fail.  For this reason
 * we are replacing the seteuid and setruid calls.
 * 
 * The bizzarre ideas are as follows:
 *
 * The effective ID may be changed only to the current real or
 * saved IDs.
 *
 * The saved uid may be set only if the real and effective
 * uids are being set to the same value.
 *
 * The real uid may be set only if the effective
 * uid is being set to the same value.
 */

#ifdef __STDC__
static int setruid(uid_t ruid)
#else
static int setruid(ruid)
  uid_t ruid;
#endif /* __STDC__ */
{
    uid_t euid;

    euid = geteuid();

    if (setuidx(ID_REAL | ID_EFFECTIVE, ruid) == -1)
	return (-1);
    
    return (setuidx(ID_EFFECTIVE, euid));
}


#ifdef __STDC__
static int seteuid(uid_t euid)
#else
static int seteuid(euid)
  uid_t euid;
#endif /* __STDC__ */
{
    uid_t ruid;

    ruid = getuid();

    if (setuidx(ID_SAVED | ID_REAL | ID_EFFECTIVE, euid) == -1)
	return (-1);
    
    return (setruid(ruid));
}


#ifdef __STDC__
static int setreuid(uid_t ruid, uid_t euid)
#else
static int setreuid(ruid, euid)
  uid_t ruid;
  uid_t euid;
#endif /* __STDC__ */
{
    if (seteuid(euid) == -1)
	return (-1);

    return (setruid(ruid));
}


#ifdef __STDC__
static int setuid(uid_t uid)
#else
static int setuid(uid)
  uid_t uid;
#endif /* __STDC__ */
{
    return (setreuid(uid, uid));
}

#endif /* RIOS */


#ifdef __STDC__
static void this_phost(char *host, int hostlen)
#else
static void this_phost(host, hostlen)
  char *host;
  int hostlen;
#endif /* __STDC__ */
{
    char this_host[MAXHOSTNAMELEN + 1];

    BCLEAR(this_host);
    
    if (gethostname(this_host, sizeof(this_host)) < 0) {
	sprintf(errbuf, "gethostname: %s", sys_errlist[errno]);
	rkinit_errmsg(errbuf);
	error();
	exit(1);
    }

    strncpy(host, krb_get_phost(this_host), hostlen - 1);
}

#ifdef __STDC__
static int decrypt_tkt(char *user, char *instance, char *realm, char *arg, 
		       int (*key_proc)(), KTEXT *cipp)
#else
static int decrypt_tkt(user, instance, realm, arg, key_proc, cipp)
  char *user;
  char *instance;
  char *realm;
  char *arg;
  int (*key_proc)();
  KTEXT *cipp;
#endif /* __STDC__ */
{
    MSG_DAT msg_data;		/* Message data containing decrypted data */
    KTEXT_ST auth;		/* Authenticator */
    AUTH_DAT auth_dat;		/* Authentication data */
    KTEXT cip = *cipp;
    MSG_DAT scip;
    int status = 0;
    des_cblock key;
    des_key_schedule sched;
    char phost[MAXHOSTNAMELEN + 1];
    struct sockaddr_in caddr;	/* client internet address */
    struct sockaddr_in saddr;	/* server internet address */

    rkinitd_intkt_info *rii = (rkinitd_intkt_info *)arg;

    u_char enc_data[MAX_KTXT_LEN];

    SBCLEAR(auth);
    SBCLEAR(auth_dat);
    SBCLEAR(scip);
    BCLEAR(enc_data);

    scip.app_data = enc_data;

    /* 
     * Exchange with the client our response from the KDC (ticket encrypted
     * in user's private key) for the same ticket encrypted in our
     * (not yet known) session key.
     */

    rpc_exchange_tkt(cip, &scip);

    /* 
     * Get the authenticator 
     */

    SBCLEAR(auth);

    rpc_getauth(&auth, &caddr, &saddr);

    /* 
     * Decode authenticator and extract session key.  The first zero
     * means we don't care what host this comes from.  This needs to
     * be done with euid of root so that /etc/srvtab can be read.
     */

    BCLEAR(phost);
    this_phost(phost, sizeof(phost));

    /* 
     * This function has to use longjmp to return to the caller
     * because the kerberos library routine that calls it doesn't
     * pay attention to the return value it gives.  That means that
     * if any of these routines failed, the error returned to the client
     * would be "password incorrect".
     */

    if ((status = krb_rd_req(&auth, KEY, phost, caddr.sin_addr.s_addr, 
			    &auth_dat, KEYFILE))) {
	sprintf(errbuf, "krb_rd_req: %s", krb_err_txt[status]);
	rkinit_errmsg(errbuf);
	longjmp(rii->env, status);
    }

    bcopy(auth_dat.session, key, sizeof(key));
    if (des_key_sched(&key, sched)) {
	sprintf(errbuf, "Error in des_key_sched");
	rkinit_errmsg(errbuf);
	longjmp(rii->env, RKINIT_DES);
    }

    /* Decrypt the data. */
    if ((status = 
	 krb_rd_priv((u_char *)scip.app_data, scip.app_length, 
		     sched, key, &caddr, &saddr, &msg_data)) == KSUCCESS) {
	cip->length = msg_data.app_length;
	bcopy(msg_data.app_data, cip->dat, msg_data.app_length);
	cip->dat[cip->length] = 0;
    } 
    else {
	sprintf(errbuf, "krb_rd_priv: %s", krb_err_txt[status]);
	rkinit_errmsg(errbuf);
	longjmp(rii->env, status);
    }
    
    return(status);
}

#ifdef __STDC__
static int validate_user(char *aname, char *inst, char *realm, 
			 char *username, char *errmsg)
#else
static int validate_user(aname, inst, realm, username, errmsg)
  char *aname;
  char *inst;
  char *realm;
  char *username;
  char *errmsg;
#endif /* __STDC__ */
{
    struct passwd *pwnam;	/* For access_check and uid */
    AUTH_DAT auth_dat;
    int kstatus = KSUCCESS;

    SBCLEAR(auth_dat);

    if ((pwnam = getpwnam(username)) == NULL) {
	sprintf(errmsg, "%s does not exist on the remote host.", username);
	return(FAILURE);
    }

    strcpy(auth_dat.pname, aname);
    strcpy(auth_dat.pinst, inst);
    strcpy(auth_dat.prealm, realm);

    if (seteuid(pwnam->pw_uid) < 0) {
	sprintf(errmsg, "Failure setting euid to %d: %s\n", pwnam->pw_uid, 
		sys_errlist[errno]);
	strcpy(errbuf, errmsg);
	error();
	return(FAILURE);
    }
    kstatus = kuserok(&auth_dat, username);
    if (seteuid(0) < 0) {
	sprintf(errmsg, "Failure setting euid to 0: %s\n", 
		sys_errlist[errno]);
	strcpy(errbuf, errmsg);
	error();
	return(FAILURE);
    }
    
    if (kstatus != KSUCCESS) {
	sprintf(errmsg, "%s has not allowed you to log in with", username);
	if (strlen(auth_dat.pinst))
	    sprintf(errmsg, "%s %s.%s", errmsg, auth_dat.pname, 
		    auth_dat.pinst);
	else
	    sprintf(errmsg, "%s %s", errmsg, auth_dat.pname);
	sprintf(errmsg, "%s@%s tickets.", errmsg, auth_dat.prealm);
	return(FAILURE);
    }
    
    /* 
     * Set real uid to owner of ticket file.  The library takes care
     * of making the appropriate change. 
     */
    if (setruid(pwnam->pw_uid) < 0) {
	sprintf(errmsg,	"Failure setting ruid to %d: %s\n", pwnam->pw_uid,
		sys_errlist[errno]);
	strcpy(errbuf, errmsg);
	error();
	return(FAILURE);
    }
	
    return(RKINIT_SUCCESS);
}

#ifdef __STDC__
int get_tickets(int version)
#else
int get_tickets(version)
  int version;
#endif /* __STDC__ */
{
    rkinit_info info;
    AUTH_DAT auth_dat;

    int status;
    char errmsg[BUFSIZ];	/* error message for client */

    rkinitd_intkt_info rii;

    SBCLEAR(info);
    SBCLEAR(auth_dat);
    BCLEAR(errmsg);
    SBCLEAR(rii);

    rpc_get_rkinit_info(&info);

    /* 
     * The validate_user routine makes sure that the principal in question
     * is allowed to log in as username, and if so, does a setuid(localuid).
     * If there is an access violation or an error in setting the uid,
     * an error is returned and the string errmsg is initialized with 
     * an error message that will be sent back to the client.
     */
    if ((status = validate_user(info.aname, info.inst, info.realm, 
				info.username, errmsg)) != RKINIT_SUCCESS) {
	rpc_send_error(errmsg);
	exit(0);
    }
    else
	rpc_send_success();

    /* 
     * If the name of a ticket file was specified, set it; otherwise, 
     * just use the default. 
     */
    if (strlen(info.tktfilename))
	krb_set_tkt_string(info.tktfilename);
    
    /* 
     * Call internal kerberos library routine so that we can supply
     * our own ticket decryption routine.
     */

    /* 
     * We need a setjmp here because krb_get_in_tkt ignores the
     * return value of decrypt_tkt.  Thus if we want any of its
     * return values to reach the client, we have to jump out of 
     * the routine.
     */

    if (setjmp(rii.env) == 0) {
	if ((status = krb_get_in_tkt(info.aname, info.inst, info.realm, 
				    info.sname, info.sinst, info.lifetime,
				    NULL, decrypt_tkt, (char *)&rii))) {
	    strcpy(errmsg, krb_err_txt[status]);
	    rpc_send_error(errmsg);
	}
	else
	    rpc_send_success();
    }
    else
	rpc_send_error(errbuf);
    
    return(RKINIT_SUCCESS);
}
