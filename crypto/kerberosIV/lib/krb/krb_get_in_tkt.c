/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

RCSID("$Id: krb_get_in_tkt.c,v 1.30 1999/12/02 16:58:42 joda Exp $");

/*
 * decrypt_tkt(): Given user, instance, realm, passwd, key_proc
 * and the cipher text sent from the KDC, decrypt the cipher text
 * using the key returned by key_proc.
 */

static int
decrypt_tkt(const char *user,
	    char *instance,
	    const char *realm,
	    const void *arg,
	    key_proc_t key_proc,
	    KTEXT *cip)
{
    des_cblock key;		/* Key for decrypting cipher */
    int ret;

    ret = key_proc(user, instance, realm, arg, &key);
    if (ret != 0)
	return ret;
    
    encrypt_ktext(*cip, &key, DES_DECRYPT);

    memset(&key, 0, sizeof(key));
    return 0;
}

/*
 * krb_get_in_tkt() gets a ticket for a given principal to use a given
 * service and stores the returned ticket and session key for future
 * use.
 *
 * The "user", "instance", and "realm" arguments give the identity of
 * the client who will use the ticket.  The "service" and "sinstance"
 * arguments give the identity of the server that the client wishes
 * to use.  (The realm of the server is the same as the Kerberos server
 * to whom the request is sent.)  The "life" argument indicates the
 * desired lifetime of the ticket; the "key_proc" argument is a pointer
 * to the routine used for getting the client's private key to decrypt
 * the reply from Kerberos.  The "decrypt_proc" argument is a pointer
 * to the routine used to decrypt the reply from Kerberos; and "arg"
 * is an argument to be passed on to the "key_proc" routine.
 *
 * If all goes well, krb_get_in_tkt() returns INTK_OK, otherwise it
 * returns an error code:  If an AUTH_MSG_ERR_REPLY packet is returned
 * by Kerberos, then the error code it contains is returned.  Other
 * error codes returned by this routine include INTK_PROT to indicate
 * wrong protocol version, INTK_BADPW to indicate bad password (if
 * decrypted ticket didn't make sense), INTK_ERR if the ticket was for
 * the wrong server or the ticket store couldn't be initialized.
 *
 * The format of the message sent to Kerberos is as follows:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * 1 byte		KRB_PROT_VERSION	protocol version number
 * 1 byte		AUTH_MSG_KDC_REQUEST |	message type
 *			HOST_BYTE_ORDER		local byte order in lsb
 * string		user			client's name
 * string		instance		client's instance
 * string		realm			client's realm
 * 4 bytes		tlocal.tv_sec		timestamp in seconds
 * 1 byte		life			desired lifetime
 * string		service			service's name
 * string		sinstance		service's instance
 */

int
krb_mk_as_req(const char *user,
	      const char *instance,
	      const char *realm, 
	      const char *service,
	      const char *sinstance,
	      int life,
	      KTEXT cip)
{
    KTEXT_ST pkt_st;
    KTEXT pkt = &pkt_st;	/* Packet to KDC */
    KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Reply from KDC */
    
    int kerror;
    struct timeval tv;

    /* BUILD REQUEST PACKET */

    unsigned char *p = pkt->dat;
    int tmp;
    size_t rem = sizeof(pkt->dat);
    
    tmp = krb_put_int(KRB_PROT_VERSION, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(AUTH_MSG_KDC_REQUEST, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_nir(user, instance, realm, p, rem);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    gettimeofday(&tv, NULL);
    tmp = krb_put_int(tv.tv_sec, p, rem, 4);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(life, p, rem, 1);
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

    /* SEND THE REQUEST AND RECEIVE THE RETURN PACKET */

    kerror = send_to_kdc(pkt, rpkt, realm);
    if(kerror) return kerror;
    kerror = kdc_reply_cipher(rpkt, cip);
    return kerror;
}

int
krb_decode_as_rep(const char *user,
		  char *instance,
		  const char *realm,
		  const char *service,
		  const char *sinstance, 
		  key_proc_t key_proc,
		  decrypt_proc_t decrypt_proc,
		  const void *arg,
		  KTEXT as_rep,
		  CREDENTIALS *cred)
{
    int kerror;
    time_t now;
    
    if (decrypt_proc == NULL)
        decrypt_tkt(user, instance, realm, arg, key_proc, &as_rep);
    else
        (*decrypt_proc)(user, instance, realm, arg, key_proc, &as_rep);

    kerror = kdc_reply_cred(as_rep, cred);
    if(kerror != KSUCCESS)
	return kerror;
	
    if (strcmp(cred->service, service) || 
	strcmp(cred->instance, sinstance) ||
	strcmp(cred->realm, realm))	/* not what we asked for */
	return INTK_ERR;	/* we need a better code here XXX */

    now = time(NULL);
    if(krb_get_config_bool("kdc_timesync"))
	krb_set_kdc_time_diff(cred->issue_date - now);
    else if (abs((int)(now - cred->issue_date)) > CLOCK_SKEW)
	return RD_AP_TIME; /* XXX should probably be better code */

    return 0;
}

int
krb_get_in_tkt(char *user, char *instance, char *realm, 
	       char *service, char *sinstance, int life,
	       key_proc_t key_proc, decrypt_proc_t decrypt_proc, void *arg)
{
    KTEXT_ST as_rep;
    CREDENTIALS cred;
    int ret;

    ret = krb_mk_as_req(user, instance, realm, 
			service, sinstance, life, &as_rep);
    if(ret)
	return ret;
    ret = krb_decode_as_rep(user, instance, realm, service, sinstance, 
			    key_proc, decrypt_proc, arg, &as_rep, &cred);
    if(ret)
	return ret;

    return tf_setup(&cred, user, instance);
}
