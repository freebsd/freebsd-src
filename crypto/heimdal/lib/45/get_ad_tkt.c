/*
 * Copyright (c) 1997, 1999 Kungliga Tekniska Högskolan
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

#include "45_locl.h"

RCSID("$Id: get_ad_tkt.c,v 1.3 1999/12/02 17:05:01 joda Exp $");

/* get an additional version 4 ticket via the 524 protocol */

#ifndef NEVERDATE
#define NEVERDATE ((unsigned long)0x7fffffffL)
#endif

int
get_ad_tkt(char *service, char *sinstance, char *realm, int lifetime)
{
    krb5_error_code ret;
    int code;
    krb5_context context;
    krb5_ccache id;
    krb5_creds in_creds, *out_creds;
    CREDENTIALS cred;
    time_t now;
    char pname[ANAME_SZ], pinst[INST_SZ], prealm[REALM_SZ];
    
    ret = krb5_init_context(&context);
    if(ret)
	return KFAILURE;
    ret = krb5_cc_default(context, &id);
    if(ret){
	krb5_free_context(context);
	return KFAILURE;
    }
    memset(&in_creds, 0, sizeof(in_creds));
    now = time(NULL);
    in_creds.times.endtime = krb_life_to_time(time(NULL), lifetime);
    if(in_creds.times.endtime == NEVERDATE)
	in_creds.times.endtime = 0;
    ret = krb5_cc_get_principal(context, id, &in_creds.client);
    if(ret){
	krb5_cc_close(context, id);
	krb5_free_context(context);
	return KFAILURE;
    }
    ret = krb5_524_conv_principal(context, in_creds.client, 
				  pname, pinst, prealm);
    if(ret){
	krb5_free_principal(context, in_creds.client);
	krb5_cc_close(context, id);
	krb5_free_context(context);
	return KFAILURE;
    }
    ret = krb5_425_conv_principal(context, service, sinstance, realm, 
				  &in_creds.server);
    if(ret){
	krb5_free_principal(context, in_creds.client);
	krb5_cc_close(context, id);
	krb5_free_context(context);
	return KFAILURE;
    }
    ret = krb5_get_credentials(context, 
			       0, 
			       id,
			       &in_creds,
			       &out_creds);
    krb5_free_principal(context, in_creds.client);
    krb5_free_principal(context, in_creds.server);
    if(ret){
	krb5_cc_close(context, id);
	krb5_free_context(context);
	return KFAILURE;
    }
    ret = krb524_convert_creds_kdc(context, id, out_creds, &cred);
    krb5_cc_close(context, id);
    krb5_free_context(context);
    krb5_free_creds(context, out_creds);
    if(ret)
	return KFAILURE;
    code = save_credentials(cred.service, cred.instance, cred.realm, 
			    cred.session, cred.lifetime, cred.kvno, 
			    &cred.ticket_st, now);
    if(code == NO_TKT_FIL)
	code = tf_setup(&cred, pname, pinst);
    memset(&cred.session, 0, sizeof(cred.session));
    return code;
}
