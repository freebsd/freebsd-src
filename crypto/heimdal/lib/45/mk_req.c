/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

/* implementation of krb_mk_req that uses 524 protocol */

#include "45_locl.h"

RCSID("$Id: mk_req.c,v 1.6 2000/04/11 00:49:35 assar Exp $");

static int lifetime = 255;

static void
build_request(KTEXT req,
	      const char *name, const char *inst, const char *realm, 
	      u_int32_t checksum)
{
    struct timeval tv;
    krb5_storage *sp;
    krb5_data data;
    sp = krb5_storage_emem();
    krb5_store_stringz(sp, name);
    krb5_store_stringz(sp, inst);
    krb5_store_stringz(sp, realm);
    krb5_store_int32(sp, checksum);
    gettimeofday(&tv, NULL);
    krb5_store_int8(sp, tv.tv_usec  / 5000);
    krb5_store_int32(sp, tv.tv_sec);
    krb5_storage_to_data(sp, &data);
    krb5_storage_free(sp);
    memcpy(req->dat, data.data, data.length);
    req->length = (data.length + 7) & ~7;
    krb5_data_free(&data);
}

#ifdef KRB_MK_REQ_CONST
int
krb_mk_req(KTEXT authent,
	   const char *service, const char *instance, const char *realm, 
	   int32_t checksum)
#else
int
krb_mk_req(KTEXT authent,
	   char *service, char *instance, char *realm, 
	   int32_t checksum)

#endif
{
    CREDENTIALS cr;
    KTEXT_ST req;
    krb5_storage *sp;
    int code;
    /* XXX get user realm */
    const char *myrealm = realm;
    krb5_data a;

    code = krb_get_cred(service, instance, realm, &cr);
    if(code || time(NULL) > krb_life_to_time(cr.issue_date, cr.lifetime)){
	code = get_ad_tkt((char *)service,
			  (char *)instance, (char *)realm, lifetime);
	if(code == KSUCCESS)
	    code = krb_get_cred(service, instance, realm, &cr);
    }

    if(code)
	return code;

    sp = krb5_storage_emem();

    krb5_store_int8(sp, KRB_PROT_VERSION);
    krb5_store_int8(sp, AUTH_MSG_APPL_REQUEST);
    
    krb5_store_int8(sp, cr.kvno);
    krb5_store_stringz(sp, realm);
    krb5_store_int8(sp, cr.ticket_st.length);

    build_request(&req, cr.pname, cr.pinst, myrealm, checksum);
    encrypt_ktext(&req, &cr.session, DES_ENCRYPT);

    krb5_store_int8(sp, req.length);

    sp->store(sp, cr.ticket_st.dat, cr.ticket_st.length);
    sp->store(sp, req.dat, req.length);
    krb5_storage_to_data(sp, &a);
    krb5_storage_free(sp);
    memcpy(authent->dat, a.data, a.length);
    authent->length = a.length;
    krb5_data_free(&a);

    memset(&cr, 0, sizeof(cr));
    memset(&req, 0, sizeof(req));

    return KSUCCESS;
}

/* 
 * krb_set_lifetime sets the default lifetime for additional tickets
 * obtained via krb_mk_req().
 * 
 * It returns the previous value of the default lifetime.
 */

int
krb_set_lifetime(int newval)
{
    int olife = lifetime;

    lifetime = newval;
    return(olife);
}
