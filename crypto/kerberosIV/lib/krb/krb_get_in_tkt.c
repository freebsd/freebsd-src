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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$Id: krb_get_in_tkt.c,v 1.20 1997/04/01 08:18:34 joda Exp $");

/*
 * decrypt_tkt(): Given user, instance, realm, passwd, key_proc
 * and the cipher text sent from the KDC, decrypt the cipher text
 * using the key returned by key_proc.
 */

static int
decrypt_tkt(char *user, char *instance, char *realm,
	    void *arg, key_proc_t key_proc, KTEXT *cip)
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
krb_get_in_tkt(char *user, char *instance, char *realm, 
	       char *service, char *sinstance, int life,
	       key_proc_t key_proc, decrypt_proc_t decrypt_proc, void *arg)
{
    KTEXT_ST pkt_st;
    KTEXT pkt = &pkt_st;	/* Packet to KDC */
    KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Returned packet */

    int kerror;
    struct timeval tv;

    /* BUILD REQUEST PACKET */

    unsigned char *p = pkt->dat;
    
    p += krb_put_int(KRB_PROT_VERSION, p, 1);
    p += krb_put_int(AUTH_MSG_KDC_REQUEST, p, 1);

    p += krb_put_nir(user, instance, realm, p);

    gettimeofday(&tv, NULL);
    p += krb_put_int(tv.tv_sec, p, 4);
    p += krb_put_int(life, p, 1);

    p += krb_put_nir(service, sinstance, NULL, p);

    pkt->length = p - pkt->dat;

    rpkt->length = 0;

    /* SEND THE REQUEST AND RECEIVE THE RETURN PACKET */

    if ((kerror = send_to_kdc(pkt, rpkt, realm))) return(kerror);
    
    p = rpkt->dat;
    
    {
	CREDENTIALS cred;
	KTEXT_ST cip;
	KTEXT foo = &cip; /* braindamage */
	
	kerror = kdc_reply_cipher(rpkt, &cip);
	if(kerror != KSUCCESS)
	    return kerror;

	if (decrypt_proc == NULL)
	    decrypt_proc = decrypt_tkt;
	(*decrypt_proc)(user, instance, realm, arg, key_proc, &foo);

	kerror = kdc_reply_cred(&cip, &cred);
	if(kerror != KSUCCESS)
	    return kerror;
	
	if (strcmp(cred.service, service) || 
	    strcmp(cred.instance, sinstance) ||
	    strcmp(cred.realm, realm))	/* not what we asked for */
	    return INTK_ERR;	/* we need a better code here XXX */

	if (abs((int)(tv.tv_sec - cred.issue_date)) > CLOCK_SKEW) {
	    return RD_AP_TIME; /* XXX should probably be better code */
	}

	/* initialize ticket cache */

	return tf_setup(&cred, user, instance);
    }
}
