/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/lib/librkinit/rk_krb.c,v $
 * $Author: gibbs $
 *
 * This file contains the kerberos parts of the rkinit library.
 * See the comment at the top of rk_lib.c for a description of the naming
 * conventions used within the rkinit library.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <krb.h>
#include <des.h>

#include <signal.h>
#include <setjmp.h>

#ifdef POSIX
#include <termios.h>
#else
#include <sgtty.h>
#endif

#include <rkinit.h>
#include <rkinit_err.h>
#include <rkinit_private.h>

static jmp_buf env;
static void sig_restore();
static void push_signals();
static void pop_signals();

/* Information to be passed around within client get_in_tkt */
typedef struct {
    KTEXT scip;			/* Server KDC packet */
    char *username;
    char *host;
} rkinit_intkt_info;
    
static char errbuf[BUFSIZ];

/* The compiler complains if this is declared static. */
#ifdef __STDC__
int rki_key_proc(char *user, char *instance, char *realm, char *arg, 
		 des_cblock *key)
#else
int rki_key_proc(user, instance, realm, arg, key)
  char *user;
  char *instance;
  char *realm;
  char *arg;
  des_cblock *key;
#endif /* __STDC__ */

{
    rkinit_intkt_info *rii = (rkinit_intkt_info *)arg;
    char password[BUFSIZ];
    int ok = 0;
#ifdef POSIX
    struct termios ttyb;
#else
    struct sgttyb ttyb;		/* For turning off echo */
#endif

    SBCLEAR(ttyb);
    BCLEAR(password);

    /* 
     * If the username does not match the aname in the ticket, 
     * we will print that too.  Otherwise, we won't.
     */
    
    printf("Kerberos initialization (%s)", rii->host);
    if (strcmp(rii->username, user))
	printf(": tickets will be owned by %s", rii->username);
    
    printf("\nPassword for %s%s%s@%s: ", user,
	   (instance[0]) ? "." : "", instance, realm);

    fflush(stdout);

    push_signals();
    if (setjmp(env)) {
	ok = -1;
	goto lose;
    }
    
#ifndef POSIX
    ioctl(0, TIOCGETP, &ttyb);
    ttyb.sg_flags &= ~ECHO;
    ioctl(0, TIOCSETP, &ttyb);
#else
    (void) tcgetattr(0, &ttyb);
    ttyb.c_lflag &= ~ECHO;
    (void) tcsetattr(0, TCSAFLUSH, &ttyb);
#endif

    bzero(password, sizeof(password));
    if (read(0, password, sizeof(password)) == -1) {
	perror("read");
	ok = -1;
	goto lose;
    }

    if (password[strlen(password)-1] == '\n')
	password[strlen(password)-1] = 0;

     /* Generate the key from the password and destroy the password */

    des_string_to_key(password, key);

lose:
    BCLEAR(password);

#ifndef POSIX
    ttyb.sg_flags |= ECHO;
    ioctl(0, TIOCSETP, &ttyb);
#else
    ttyb.c_lflag |= ECHO;
    (void) tcsetattr(0, TCSAFLUSH, &ttyb);
#endif

    pop_signals();
    printf("\n");

    return(ok);
}

#ifdef __STDC__
static int rki_decrypt_tkt(char *user, char *instance, char *realm, 
			   char *arg, int (*key_proc)(), KTEXT *cipp)
#else
static int rki_decrypt_tkt(user, instance, realm, arg, key_proc, cipp)
  char *user;
  char *instance;
  char *realm;
  char *arg;
  int (*key_proc)();
  KTEXT *cipp;
#endif /* __STDC__ */
{
    KTEXT cip = *cipp;
    C_Block key;		/* Key for decrypting cipher */
    Key_schedule key_s;
    KTEXT scip = 0;		/* cipher from rkinit server */
    
    rkinit_intkt_info *rii = (rkinit_intkt_info *)arg;

    /* generate a key */
    {
	register int rc;
	rc = (*key_proc)(user, instance, realm, arg, key);
	if (rc)
	    return(rc);
    }

    des_key_sched(&key, key_s);

    /* Decrypt information from KDC */
    des_pcbc_encrypt((C_Block *)cip->dat,(C_Block *)cip->dat,
		     (long) cip->length, key_s, &key, 0);

    /* DescrYPT rkinit server's information from KDC */
    scip = rii->scip;
    des_pcbc_encrypt((C_Block *)scip->dat,(C_Block *)scip->dat,
		     (long) scip->length, key_s, &key, 0);

    /* Get rid of all traces of key */
    bzero((char *)key, sizeof(key));
    bzero((char *)key_s, sizeof(key_s));

    return(0);
}

#ifdef __STDC__
int rki_get_tickets(int version, char *host, char *r_krealm, rkinit_info *info)
#else
int rki_get_tickets(version, host, r_krealm, info)
  int version;
  char *host;
  char *r_krealm;
  rkinit_info *info;
#endif /* __STDC__ */
{
    int status = RKINIT_SUCCESS;
    KTEXT_ST auth;
    char phost[MAXHOSTNAMELEN];
    KTEXT_ST scip;		/* server's KDC packet */
    des_cblock key;
    des_key_schedule sched;
    struct sockaddr_in caddr;
    struct sockaddr_in saddr;
    CREDENTIALS cred;
    MSG_DAT msg_data;
    u_char enc_data[MAX_KTXT_LEN];

    rkinit_intkt_info rii;

    SBCLEAR(auth);
    BCLEAR(phost);
    SBCLEAR(rii);
    SBCLEAR(scip);
    SBCLEAR(caddr);
    SBCLEAR(saddr);
    SBCLEAR(cred);
    SBCLEAR(msg_data);
    BCLEAR(enc_data);

    if ((status = rki_send_rkinit_info(version, info)) != RKINIT_SUCCESS)
	return(status);

    if ((status = rki_rpc_get_skdc(&scip)) != RKINIT_SUCCESS) 
	return(status);

    rii.scip = &scip;
    rii.host = host;
    rii.username = info->username;

    if ((status = krb_get_in_tkt(info->aname, info->inst, info->realm, 
				"krbtgt", info->realm, 1,
				rki_key_proc, rki_decrypt_tkt, (char *)&rii))) {
	strcpy(errbuf, krb_err_txt[status]);
	rkinit_errmsg(errbuf);
	return(RKINIT_KERBEROS);	    
    }

    /* Create an authenticator */
    strcpy(phost, krb_get_phost(host));    
    if ((status = krb_mk_req(&auth, KEY, phost, r_krealm, 0))) {
	sprintf(errbuf, "krb_mk_req: %s", krb_err_txt[status]);
	rkinit_errmsg(errbuf);
	return(RKINIT_KERBEROS);
    }

    /* Re-encrypt server KDC packet in session key */
    /* Get credentials from ticket file */
    if ((status = krb_get_cred(KEY, phost, r_krealm, &cred))) {
	sprintf(errbuf, "krb_get_cred: %s", krb_err_txt[status]);
	rkinit_errmsg(errbuf);
	return(RKINIT_KERBEROS);
    }

    /* Exctract the session key and make the schedule */
    bcopy(cred.session, key, sizeof(key));
    if ((status = des_key_sched(&key, sched))) {
	sprintf(errbuf, "des_key_sched: %s", krb_err_txt[status]);
	rkinit_errmsg(errbuf);
	return(RKINIT_DES);
    }

    /* Get client and server addresses */
    if ((status = rki_get_csaddr(&caddr, &saddr)) != RKINIT_SUCCESS)
	return(status);

    /* 
     * scip was passed to krb_get_in_tkt, where it was decrypted.
     * Now re-encrypt in the session key. 
     */

    msg_data.app_data = enc_data;
    if ((msg_data.app_length = 
	 krb_mk_priv(scip.dat, msg_data.app_data, scip.length, sched, key, 
		     &caddr, &saddr)) == -1) {
	sprintf(errbuf, "krb_mk_priv failed.");
	rkinit_errmsg(errbuf);
	return(RKINIT_KERBEROS);
    }

    /* Destroy tickets, which we no longer need */
    dest_tkt();

    if ((status = rki_rpc_send_ckdc(&msg_data)) != RKINIT_SUCCESS)
	return(status);

    if ((status = rki_rpc_sendauth(&auth)) != RKINIT_SUCCESS)
	return(status);

    if ((status = rki_rpc_get_status()))
	return(status);

    return(RKINIT_SUCCESS);
}


static void (*old_sigfunc[NSIG])(int);

static void push_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
        old_sigfunc[i] = signal(i,sig_restore);
}

static void pop_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
        signal(i,old_sigfunc[i]);
}

static void sig_restore(sig,code,scp)
    int sig,code;
    struct sigcontext *scp;
{
    longjmp(env,1);
}
