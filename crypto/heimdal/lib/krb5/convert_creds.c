/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
RCSID("$Id: convert_creds.c,v 1.17 2001/05/14 06:14:45 assar Exp $");

static krb5_error_code
check_ticket_flags(TicketFlags f)
{
    return 0; /* maybe add some more tests here? */
}

/* include this here, to avoid dependencies on libkrb */

#define		MAX_KTXT_LEN	1250

#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40

struct ktext {
    unsigned int length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    u_int32_t mbz;		/* zero to catch runaway strings */
};

struct credentials {
    char    service[ANAME_SZ];	/* Service name */
    char    instance[INST_SZ];	/* Instance */
    char    realm[REALM_SZ];	/* Auth domain */
    des_cblock session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    struct ktext ticket_st;	/* The ticket itself */
    int32_t    issue_date;	/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};


#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */
#ifndef NEVERDATE
#define NEVERDATE ((time_t)0x7fffffffL)
#endif

static const int _tkt_lifetimes[TKTLIFENUMFIXED] = {
   38400,   41055,   43894,   46929,   50174,   53643,   57352,   61318,
   65558,   70091,   74937,   80119,   85658,   91581,   97914,  104684,
  111922,  119661,  127935,  136781,  146239,  156350,  167161,  178720,
  191077,  204289,  218415,  233517,  249664,  266926,  285383,  305116,
  326213,  348769,  372885,  398668,  426234,  455705,  487215,  520904,
  556921,  595430,  636601,  680618,  727680,  777995,  831789,  889303,
  950794, 1016537, 1086825, 1161973, 1242318, 1328218, 1420057, 1518247,
 1623226, 1735464, 1855462, 1983758, 2120925, 2267576, 2424367, 2592000
};

static int
_krb_time_to_life(time_t start, time_t end)
{
    int i;
    time_t life = end - start;

    if (life > MAXTKTLIFETIME || life <= 0) 
	return 0;
#if 0    
    if (krb_no_long_lifetimes) 
	return (life + 5*60 - 1)/(5*60);
#endif
    
    if (end >= NEVERDATE)
	return TKTLIFENOEXPIRE;
    if (life < _tkt_lifetimes[0]) 
	return (life + 5*60 - 1)/(5*60);
    for (i=0; i<TKTLIFENUMFIXED; i++)
	if (life <= _tkt_lifetimes[i])
	    return i + TKTLIFEMINFIXED;
    return 0;
    
}

/* Convert the v5 credentials in `in_cred' to v4-dito in `v4creds'.
 * This is done by sending them to the 524 function in the KDC.  If
 * `in_cred' doesn't contain a DES session key, then a new one is
 * gotten from the KDC and stored in the cred cache `ccache'.
 */

krb5_error_code
krb524_convert_creds_kdc(krb5_context context, 
			 krb5_ccache ccache,
			 krb5_creds *in_cred,
			 struct credentials *v4creds)
{
    krb5_error_code ret;
    krb5_data reply;
    krb5_storage *sp;
    int32_t tmp;
    krb5_data ticket;
    char realm[REALM_SZ];
    krb5_creds *v5_creds = in_cred;
    krb5_keytype keytype;

    keytype = v5_creds->session.keytype;

    if (keytype != ENCTYPE_DES_CBC_CRC) {
	/* MIT krb524d doesn't like nothing but des-cbc-crc tickets,
           so go get one */
	krb5_creds template;

	memset (&template, 0, sizeof(template));
	template.session.keytype = ENCTYPE_DES_CBC_CRC;
	ret = krb5_copy_principal (context, in_cred->client, &template.client);
	if (ret) {
	    krb5_free_creds_contents (context, &template);
	    return ret;
	}
	ret = krb5_copy_principal (context, in_cred->server, &template.server);
	if (ret) {
	    krb5_free_creds_contents (context, &template);
	    return ret;
	}

	ret = krb5_get_credentials (context, 0, ccache,
				    &template, &v5_creds);
	krb5_free_creds_contents (context, &template);
	if (ret)
	    return ret;
    }

    ret = check_ticket_flags(v5_creds->flags.b);
    if(ret)
	goto out2;

    {
	char **hostlist;
	int port;
	port = krb5_getportbyname (context, "krb524", "udp", 4444);
	
	ret = krb5_get_krbhst (context, krb5_princ_realm(context, 
							 v5_creds->server), 
			       &hostlist);
	if(ret)
	    goto out2;
	
	ret = krb5_sendto (context,
			   &v5_creds->ticket,
			   hostlist,
			   port,
			   &reply);
	if(ret == KRB5_KDC_UNREACH) {
	    port = krb5_getportbyname (context, "kerberos", "udp", 88);
	    ret = krb5_sendto (context,
			       &v5_creds->ticket,
			       hostlist,
			       port,
			       &reply);
	}
	krb5_free_krbhst (context, hostlist);
    }
    if (ret)
	goto out2;
    sp = krb5_storage_from_mem(reply.data, reply.length);
    if(sp == NULL) {
	ret = ENOMEM;
	krb5_set_error_string (context, "malloc: out of memory");
	goto out2;
    }
    krb5_ret_int32(sp, &tmp);
    ret = tmp;
    if(ret == 0) {
	memset(v4creds, 0, sizeof(*v4creds));
	ret = krb5_ret_int32(sp, &tmp);
	if(ret)
	    goto out;
	v4creds->kvno = tmp;
	ret = krb5_ret_data(sp, &ticket);
	if(ret)
	    goto out;
	v4creds->ticket_st.length = ticket.length;
	memcpy(v4creds->ticket_st.dat, ticket.data, ticket.length);
	krb5_data_free(&ticket);
	ret = krb5_524_conv_principal(context, 
				      v5_creds->server, 
				      v4creds->service, 
				      v4creds->instance, 
				      v4creds->realm);
	if(ret)
	    goto out;
	v4creds->issue_date = v5_creds->times.authtime;
	v4creds->lifetime = _krb_time_to_life(v4creds->issue_date,
					      v5_creds->times.endtime);
	ret = krb5_524_conv_principal(context, v5_creds->client, 
				      v4creds->pname, 
				      v4creds->pinst, 
				      realm);
	if(ret)
	    goto out;
	memcpy(v4creds->session, v5_creds->session.keyvalue.data, 8);
    }
out:
    krb5_storage_free(sp);
    krb5_data_free(&reply);
out2:
    if (v5_creds != in_cred)
	krb5_free_creds (context, v5_creds);
    return ret;
}
