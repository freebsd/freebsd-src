/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb_locl.h"

RCSID("$Id: get_ad_tkt.c,v 1.22 1999/12/02 16:58:41 joda Exp $");

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
get_ad_tkt(char *service, char *sinstance, char *realm, int lifetime)
{
    static KTEXT_ST pkt_st;
    KTEXT pkt = & pkt_st;	/* Packet to KDC */
    static KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Returned packet */

    CREDENTIALS cr;
    char lrealm[REALM_SZ];
    u_int32_t time_ws = 0;
    int kerror;
    unsigned char *p;
    size_t rem;
    int tmp;

    /*
     * First check if we have a "real" TGT for the corresponding
     * realm, if we don't, use ordinary inter-realm authentication.
     */

    kerror = krb_get_cred(KRB_TICKET_GRANTING_TICKET, realm, realm, &cr);
    if (kerror == KSUCCESS) {
      strlcpy(lrealm, realm, REALM_SZ);
    } else
      kerror = krb_get_tf_realm(TKT_FILE, lrealm);
    
    if (kerror != KSUCCESS)
	return(kerror);

    /*
     * Look for the session key (and other stuff we don't need)
     * in the ticket file for krbtgt.realm@lrealm where "realm" 
     * is the service's realm (passed in "realm" argument) and 
     * lrealm is the realm of our initial ticket.  If we don't 
     * have this, we will try to get it.
     */
    
    if ((kerror = krb_get_cred(KRB_TICKET_GRANTING_TICKET,
			       realm, lrealm, &cr)) != KSUCCESS) {
	/*
	 * If realm == lrealm, we have no hope, so let's not even try.
	 */
	if ((strncmp(realm, lrealm, REALM_SZ)) == 0)
	    return(AD_NOTGT);
	else{
	    if ((kerror = 
		 get_ad_tkt(KRB_TICKET_GRANTING_TICKET,
			    realm, lrealm, lifetime)) != KSUCCESS) {
		if (kerror == KDC_PR_UNKNOWN)
		  return(AD_INTR_RLM_NOTGT);
		else
		  return(kerror);
	    }
	    if ((kerror = krb_get_cred(KRB_TICKET_GRANTING_TICKET,
				       realm, lrealm, &cr)) != KSUCCESS)
		return(kerror);
	}
    }
    
    /*
     * Make up a request packet to the "krbtgt.realm@lrealm".
     * Start by calling krb_mk_req() which puts ticket+authenticator
     * into "pkt".  Then tack other stuff on the end.
     */
    
    kerror = krb_mk_req(pkt,
			KRB_TICKET_GRANTING_TICKET,
			realm,lrealm,0L);

    if (kerror)
	return(AD_NOTGT);

    p = pkt->dat + pkt->length;
    rem = sizeof(pkt->dat) - pkt->length;

    tmp = krb_put_int(time_ws, p, rem, 4);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(lifetime, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_nir(service, sinstance, NULL, p, rem);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;
    
    pkt->length = p - pkt->dat;
    rpkt->length = 0;
    
    /* Send the request to the local ticket-granting server */
    if ((kerror = send_to_kdc(pkt, rpkt, realm))) return(kerror);

    /* check packet version of the returned packet */

    {
	KTEXT_ST cip;
	CREDENTIALS cred;
	struct timeval tv;

	kerror = kdc_reply_cipher(rpkt, &cip);
	if(kerror != KSUCCESS)
	    return kerror;
	
	encrypt_ktext(&cip, &cr.session, DES_DECRYPT);

	kerror = kdc_reply_cred(&cip, &cred);
	if(kerror != KSUCCESS)
	    return kerror;

	if (strcmp(cred.service, service) || strcmp(cred.instance, sinstance) ||
	    strcmp(cred.realm, realm))	/* not what we asked for */
	    return INTK_ERR;	/* we need a better code here XXX */
	
	krb_kdctimeofday(&tv);
	if (abs((int)(tv.tv_sec - cred.issue_date)) > CLOCK_SKEW) {
	    return RD_AP_TIME; /* XXX should probably be better code */
	}
	

	kerror = save_credentials(cred.service, cred.instance, cred.realm, 
				  cred.session, cred.lifetime, cred.kvno, 
				  &cred.ticket_st, tv.tv_sec);
	return kerror;
    }
}
