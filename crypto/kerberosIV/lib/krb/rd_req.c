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

RCSID("$Id: rd_req.c,v 1.27.2.2 2000/06/23 04:00:20 assar Exp $");

static struct timeval t_local = { 0, 0 };

/*
 * Keep the following information around for subsequent calls
 * to this routine by the same server using the same key.
 */

static des_key_schedule serv_key;	/* Key sched to decrypt ticket */
static des_cblock ky;              /* Initialization vector */
static int st_kvno;		/* version number for this key */
static char st_rlm[REALM_SZ];	/* server's realm */
static char st_nam[ANAME_SZ];	/* service name */
static char st_inst[INST_SZ];	/* server's instance */

/*
 * This file contains two functions.  krb_set_key() takes a DES
 * key or password string and returns a DES key (either the original
 * key, or the password converted into a DES key) and a key schedule
 * for it.
 *
 * krb_rd_req() reads an authentication request and returns information
 * about the identity of the requestor, or an indication that the
 * identity information was not authentic.
 */

/*
 * krb_set_key() takes as its first argument either a DES key or a
 * password string.  The "cvt" argument indicates how the first
 * argument "key" is to be interpreted: if "cvt" is null, "key" is
 * taken to be a DES key; if "cvt" is non-null, "key" is taken to
 * be a password string, and is converted into a DES key using
 * string_to_key().  In either case, the resulting key is returned
 * in the external static variable "ky".  A key schedule is
 * generated for "ky" and returned in the external static variable
 * "serv_key".
 *
 * This routine returns the return value of des_key_sched.
 *
 * krb_set_key() needs to be in the same .o file as krb_rd_req() so that
 * the key set by krb_set_key() is available in private storage for
 * krb_rd_req().
 */

int
krb_set_key(void *key, int cvt)
{
#ifdef NOENCRYPTION
    memset(ky, 0, sizeof(ky));
    return KSUCCESS;
#else /* Encrypt */
    if (cvt)
        des_string_to_key((char*)key, &ky);
    else
        memcpy((char*)ky, key, 8);
    return(des_key_sched(&ky, serv_key));
#endif /* NOENCRYPTION */
}


/*
 * krb_rd_req() takes an AUTH_MSG_APPL_REQUEST or
 * AUTH_MSG_APPL_REQUEST_MUTUAL message created by krb_mk_req(),
 * checks its integrity and returns a judgement as to the requestor's
 * identity.
 *
 * The "authent" argument is a pointer to the received message.
 * The "service" and "instance" arguments name the receiving server,
 * and are used to get the service's ticket to decrypt the ticket
 * in the message, and to compare against the server name inside the
 * ticket.  "from_addr" is the network address of the host from which
 * the message was received; this is checked against the network
 * address in the ticket.  If "from_addr" is zero, the check is not
 * performed.  "ad" is an AUTH_DAT structure which is
 * filled in with information about the sender's identity according
 * to the authenticator and ticket sent in the message.  Finally,
 * "fn" contains the name of the file containing the server's key.
 * (If "fn" is NULL, the server's key is assumed to have been set
 * by krb_set_key().  If "fn" is the null string ("") the default
 * file KEYFILE, defined in "krb.h", is used.)
 *
 * krb_rd_req() returns RD_AP_OK if the authentication information
 * was genuine, or one of the following error codes (defined in
 * "krb.h"):
 *
 *	RD_AP_VERSION		- wrong protocol version number
 *	RD_AP_MSG_TYPE		- wrong message type
 *	RD_AP_UNDEC		- couldn't decipher the message
 *	RD_AP_INCON		- inconsistencies found
 *	RD_AP_BADD		- wrong network address
 *	RD_AP_TIME		- client time (in authenticator)
 *				  too far off server time
 *	RD_AP_NYV		- Kerberos time (in ticket) too
 *				  far off server time
 *	RD_AP_EXP		- ticket expired
 *
 * For the message format, see krb_mk_req().
 *
 * Mutual authentication is not implemented.
 */

int
krb_rd_req(KTEXT authent,	/* The received message */
	   char *service,	/* Service name */
	   char *instance,	/* Service instance */
	   int32_t from_addr,	/* Net address of originating host */
	   AUTH_DAT *ad,	/* Structure to be filled in */
	   char *a_fn)		/* Filename to get keys from */
{
    static KTEXT_ST ticket;     /* Temp storage for ticket */
    static KTEXT tkt = &ticket;
    static KTEXT_ST req_id_st;  /* Temp storage for authenticator */
    KTEXT req_id = &req_id_st;

    char realm[REALM_SZ];	/* Realm of issuing kerberos */

    unsigned char skey[KKEY_SZ]; /* Session key from ticket */
    char sname[SNAME_SZ];	/* Service name from ticket */
    char iname[INST_SZ];	/* Instance name from ticket */
    char r_aname[ANAME_SZ];	/* Client name from authenticator */
    char r_inst[INST_SZ];	/* Client instance from authenticator */
    char r_realm[REALM_SZ];	/* Client realm from authenticator */
    u_int32_t r_time_sec;	/* Coarse time from authenticator */
    unsigned long delta_t;      /* Time in authenticator - local time */
    long tkt_age;		/* Age of ticket */
    static unsigned char s_kvno;/* Version number of the server's key
				 * Kerberos used to encrypt ticket */

    struct timeval tv;
    int status;

    int pvno;
    int type;
    int little_endian;

    const char *fn = a_fn;

    unsigned char *p;

    if (authent->length <= 0)
	return(RD_AP_MODIFIED);

    p = authent->dat;

    /* get msg version, type and byte order, and server key version */

    pvno = *p++;

    if(pvno != KRB_PROT_VERSION)
	return RD_AP_VERSION;
    
    type = *p++;
    
    little_endian = type & 1;
    type &= ~1;
    
    if(type != AUTH_MSG_APPL_REQUEST && type != AUTH_MSG_APPL_REQUEST_MUTUAL)
	return RD_AP_MSG_TYPE;

    s_kvno = *p++;

    p += krb_get_string(p, realm, sizeof(realm));

    /*
     * If "fn" is NULL, key info should already be set; don't
     * bother with ticket file.  Otherwise, check to see if we
     * already have key info for the given server and key version
     * (saved in the static st_* variables).  If not, go get it
     * from the ticket file.  If "fn" is the null string, use the
     * default ticket file.
     */
    if (fn && (strcmp(st_nam,service) || strcmp(st_inst,instance) ||
               strcmp(st_rlm,realm) || (st_kvno != s_kvno))) {
        if (*fn == 0) fn = (char *)KEYFILE;
        st_kvno = s_kvno;
        if (read_service_key(service, instance, realm, s_kvno,
			     fn, (char *)skey))
            return(RD_AP_UNDEC);
        if ((status = krb_set_key((char*)skey, 0)))
	    return(status);
        strlcpy (st_rlm, realm, REALM_SZ);
        strlcpy (st_nam, service, SNAME_SZ);
        strlcpy (st_inst, instance, INST_SZ);
    }

    tkt->length = *p++;

    req_id->length = *p++;

    if(tkt->length + (p - authent->dat) > authent->length)
	return RD_AP_MODIFIED;

    memcpy(tkt->dat, p, tkt->length);
    p += tkt->length;

    if (krb_ap_req_debug)
        krb_log("ticket->length: %d",tkt->length);

    /* Decrypt and take apart ticket */
    if (decomp_ticket(tkt, &ad->k_flags, ad->pname, ad->pinst, ad->prealm,
                      &ad->address, ad->session, &ad->life,
                      &ad->time_sec, sname, iname, &ky, serv_key))
        return RD_AP_UNDEC;
    
    if (krb_ap_req_debug) {
        krb_log("Ticket Contents.");
        krb_log(" Aname:   %s.%s",ad->pname, ad->prealm);
        krb_log(" Service: %s", krb_unparse_name_long(sname, iname, NULL));
    }

    /* Extract the authenticator */
    
    if(req_id->length + (p - authent->dat) > authent->length)
	return RD_AP_MODIFIED;

    memcpy(req_id->dat, p, req_id->length);
    p = req_id->dat;
    
#ifndef NOENCRYPTION
    /* And decrypt it with the session key from the ticket */
    if (krb_ap_req_debug) krb_log("About to decrypt authenticator");

    encrypt_ktext(req_id, &ad->session, DES_DECRYPT);

    if (krb_ap_req_debug) krb_log("Done.");
#endif /* NOENCRYPTION */

    /* cast req_id->length to int? */
#define check_ptr() if ((ptr - (char *) req_id->dat) > req_id->length) return(RD_AP_MODIFIED);

    p += krb_get_nir(p,
		     r_aname, sizeof(r_aname),
		     r_inst, sizeof(r_inst),
		     r_realm, sizeof(r_realm));

    p += krb_get_int(p, &ad->checksum, 4, little_endian);

    p++; /* time_5ms is not used */

    p += krb_get_int(p, &r_time_sec, 4, little_endian);

    /* Check for authenticity of the request */
    if (krb_ap_req_debug)
        krb_log("Principal: %s.%s@%s / %s.%s@%s",ad->pname,ad->pinst, ad->prealm, 
	      r_aname, r_inst, r_realm);
    if (strcmp(ad->pname, r_aname) != 0 ||
	strcmp(ad->pinst, r_inst) != 0 ||
	strcmp(ad->prealm, r_realm) != 0)
	return RD_AP_INCON;
    
    if (krb_ap_req_debug)
        krb_log("Address: %x %x", ad->address, from_addr);

    if (from_addr && (!krb_equiv(ad->address, from_addr)))
        return RD_AP_BADD;

    gettimeofday(&tv, NULL);
    delta_t = abs((int)(tv.tv_sec - r_time_sec));
    if (delta_t > CLOCK_SKEW) {
        if (krb_ap_req_debug)
            krb_log("Time out of range: %lu - %lu = %lu",
		    (unsigned long)t_local.tv_sec,
		    (unsigned long)r_time_sec,
		    (unsigned long)delta_t);
        return RD_AP_TIME;
    }

    /* Now check for expiration of ticket */

    tkt_age = tv.tv_sec - ad->time_sec;
    if (krb_ap_req_debug)
        krb_log("Time: %ld Issue Date: %lu Diff: %ld Life %x",
		(long)tv.tv_sec,
		(unsigned long)ad->time_sec,
		tkt_age,
		ad->life);
    
    if ((tkt_age < 0) && (-tkt_age > CLOCK_SKEW))
	return RD_AP_NYV;

    if (tv.tv_sec > krb_life_to_time(ad->time_sec, ad->life))
        return RD_AP_EXP;

    /* All seems OK */
    ad->reply.length = 0;

    return(RD_AP_OK);
}
