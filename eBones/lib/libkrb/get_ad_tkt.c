/*
 * Copyright 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: get_ad_tkt.c,v 4.15 89/07/07 15:18:51 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

#include <krb.h>
#include <des.h>
#include <prot.h>
#include <strings.h>

#include <stdio.h>
#include <errno.h>

/* use the bsd time.h struct defs for PC too! */
#include <sys/time.h>
#include <sys/types.h>

extern int krb_debug;

struct timeval tt_local = { 0, 0 };

int swap_bytes;
unsigned long rep_err_code;

/*
 * get_ad_tkt obtains a new service ticket from Kerberos, using
 * the ticket-granting ticket which must be in the ticket file.
 * It is typically called by krb_mk_req() when the client side
 * of an application is creating authentication information to be
 * sent to the server side.
 *
 * get_ad_tkt takes four arguments: three pointers to strings which
 * contain the name, instance, and realm of the service for which the
 * ticket is to be obtained; and an integer indicating the desired
 * lifetime of the ticket.
 *
 * It returns an error status if the ticket couldn't be obtained,
 * or AD_OK if all went well.  The ticket is stored in the ticket
 * cache.
 *
 * The request sent to the Kerberos ticket-granting service looks
 * like this:
 *
 * pkt->dat
 *
 * TEXT			original contents of	authenticator+ticket
 *			pkt->dat		built in krb_mk_req call
 *
 * 4 bytes		time_ws			always 0 (?)
 * char			lifetime		lifetime argument passed
 * string		service			service name argument
 * string		sinstance		service instance arg.
 *
 * See "prot.h" for the reply packet layout and definitions of the
 * extraction macros like pkt_version(), pkt_msg_type(), etc.
 */

int
get_ad_tkt(service,sinstance,realm,lifetime)
    char    *service;
    char    *sinstance;
    char    *realm;
    int     lifetime;
{
    static KTEXT_ST pkt_st;
    KTEXT pkt = & pkt_st;	/* Packet to KDC */
    static KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Returned packet */
    static KTEXT_ST cip_st;
    KTEXT cip = &cip_st;	/* Returned Ciphertext */
    static KTEXT_ST tkt_st;
    KTEXT tkt = &tkt_st;	/* Current ticket */
    C_Block ses;                /* Session key for tkt */
    CREDENTIALS cr;
    int kvno;			/* Kvno for session key */
    char lrealm[REALM_SZ];
    C_Block key;		/* Key for decrypting cipher */
    Key_schedule key_s;
    long time_ws = 0;

    char s_name[SNAME_SZ];
    char s_instance[INST_SZ];
    int msg_byte_order;
    int kerror;
    char rlm[REALM_SZ];
    char *ptr;

    unsigned long kdc_time;   /* KDC time */

    if ((kerror = krb_get_tf_realm(TKT_FILE, lrealm)) != KSUCCESS)
	return(kerror);

    /* Create skeleton of packet to be sent */
    (void) gettimeofday(&tt_local,(struct timezone *) 0);

    pkt->length = 0;

    /*
     * Look for the session key (and other stuff we don't need)
     * in the ticket file for krbtgt.realm@lrealm where "realm"
     * is the service's realm (passed in "realm" argument) and
     * lrealm is the realm of our initial ticket.  If we don't
     * have this, we will try to get it.
     */

    if ((kerror = krb_get_cred("krbtgt",realm,lrealm,&cr)) != KSUCCESS) {
	/*
	 * If realm == lrealm, we have no hope, so let's not even try.
	 */
	if ((strncmp(realm, lrealm, REALM_SZ)) == 0)
	    return(AD_NOTGT);
	else{
	    if ((kerror =
		 get_ad_tkt("krbtgt",realm,lrealm,lifetime)) != KSUCCESS)
		return(kerror);
	    if ((kerror = krb_get_cred("krbtgt",realm,lrealm,&cr)) != KSUCCESS)
		return(kerror);
	}
    }

    /*
     * Make up a request packet to the "krbtgt.realm@lrealm".
     * Start by calling krb_mk_req() which puts ticket+authenticator
     * into "pkt".  Then tack other stuff on the end.
     */

    kerror = krb_mk_req(pkt,"krbtgt",realm,lrealm,0L);

    if (kerror)
	return(AD_NOTGT);

    /* timestamp */
    bcopy((char *) &time_ws,(char *) (pkt->dat+pkt->length),4);
    pkt->length += 4;
    *(pkt->dat+(pkt->length)++) = (char) lifetime;
    (void) strcpy((char *) (pkt->dat+pkt->length),service);
    pkt->length += 1 + strlen(service);
    (void) strcpy((char *)(pkt->dat+pkt->length),sinstance);
    pkt->length += 1 + strlen(sinstance);

    rpkt->length = 0;

    /* Send the request to the local ticket-granting server */
    if ((kerror = send_to_kdc(pkt, rpkt, realm))) return(kerror);

    /* check packet version of the returned packet */
    if (pkt_version(rpkt) != KRB_PROT_VERSION )
        return(INTK_PROT);

    /* Check byte order */
    msg_byte_order = pkt_msg_type(rpkt) & 1;
    swap_bytes = 0;
    if (msg_byte_order != HOST_BYTE_ORDER)
	swap_bytes++;

    switch (pkt_msg_type(rpkt) & ~1) {
    case AUTH_MSG_KDC_REPLY:
	break;
    case AUTH_MSG_ERR_REPLY:
	bcopy(pkt_err_code(rpkt), (char *) &rep_err_code, 4);
	if (swap_bytes)
	    swap_u_long(rep_err_code);
	return(rep_err_code);

    default:
	return(INTK_PROT);
    }

    /* Extract the ciphertext */
    cip->length = pkt_clen(rpkt);       /* let clen do the swap */

    bcopy((char *) pkt_cipher(rpkt),(char *) (cip->dat),cip->length);

#ifndef NOENCRYPTION
    key_sched((C_Block *)cr.session,key_s);
    pcbc_encrypt((C_Block *)cip->dat,(C_Block *)cip->dat,(long)cip->length,
	key_s,(C_Block *)cr.session,DECRYPT);
#endif
    /* Get rid of all traces of key */
    bzero((char *) cr.session, sizeof(key));
    bzero((char *) key_s, sizeof(key_s));

    ptr = (char *) cip->dat;

    bcopy(ptr,(char *)ses,8);
    ptr += 8;

    (void) strcpy(s_name,ptr);
    ptr += strlen(s_name) + 1;

    (void) strcpy(s_instance,ptr);
    ptr += strlen(s_instance) + 1;

    (void) strcpy(rlm,ptr);
    ptr += strlen(rlm) + 1;

    lifetime = (unsigned long) ptr[0];
    kvno = (unsigned long) ptr[1];
    tkt->length = (int) ptr[2];
    ptr += 3;
    bcopy(ptr,(char *)(tkt->dat),tkt->length);
    ptr += tkt->length;

    if (strcmp(s_name, service) || strcmp(s_instance, sinstance) ||
        strcmp(rlm, realm))	/* not what we asked for */
	return(INTK_ERR);	/* we need a better code here XXX */

    /* check KDC time stamp */
    bcopy(ptr,(char *)&kdc_time,4); /* Time (coarse) */
    if (swap_bytes) swap_u_long(kdc_time);

    ptr += 4;

    (void) gettimeofday(&tt_local,(struct timezone *) 0);
    if (abs((int)(tt_local.tv_sec - kdc_time)) > CLOCK_SKEW) {
        return(RD_AP_TIME);		/* XXX should probably be better
					   code */
    }

    if ((kerror = save_credentials(s_name,s_instance,rlm,ses,lifetime,
				  kvno,tkt,tt_local.tv_sec)))
	return(kerror);

    return(AD_OK);
}
