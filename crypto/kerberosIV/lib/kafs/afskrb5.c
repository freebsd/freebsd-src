/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include "kafs_locl.h"

RCSID("$Id: afskrb5.c,v 1.7 1999/07/07 12:30:06 assar Exp $");

struct krb5_kafs_data {
    krb5_context context;
    krb5_ccache id;
    krb5_const_realm realm;
};

static int
get_cred(kafs_data *data, const char *name, const char *inst, 
	 const char *realm, CREDENTIALS *c)
{
    krb5_error_code ret;
    krb5_creds in_creds, *out_creds;
    struct krb5_kafs_data *d = data->data;

    memset(&in_creds, 0, sizeof(in_creds));
    ret = krb5_425_conv_principal(d->context, name, inst, realm, 
				  &in_creds.server);
    if(ret)
	return ret;
    ret = krb5_cc_get_principal(d->context, d->id, &in_creds.client);
    if(ret){
	krb5_free_principal(d->context, in_creds.server);
	return ret;
    }
    ret = krb5_get_credentials(d->context, 0, d->id, &in_creds, &out_creds);
    krb5_free_principal(d->context, in_creds.server);
    krb5_free_principal(d->context, in_creds.client);
    if(ret)
	return ret;
    ret = krb524_convert_creds_kdc(d->context, out_creds, c);
    krb5_free_creds(d->context, out_creds);
    return ret;
}

static krb5_error_code
afslog_uid_int(kafs_data *data, const char *cell, uid_t uid,
	       const char *homedir)
{
    krb5_error_code ret;
    CREDENTIALS c;
    krb5_realm lrealm; /* local realm */
    struct krb5_kafs_data *d = data->data;
    
    if (cell == 0 || cell[0] == 0)
	return _kafs_afslog_all_local_cells (data, uid, homedir);

    ret = krb5_get_default_realm(d->context, &lrealm);
    if(ret || (d->realm && strcmp(d->realm, lrealm) == 0)){
	free(lrealm);
	lrealm = NULL;
    }

    ret = _kafs_get_cred(data, cell, d->realm, lrealm, &c);
    if(lrealm)
	free(lrealm);
    
    if(ret == 0)
	ret = kafs_settoken(cell, uid, &c);
    return ret;
}

static char *
get_realm(kafs_data *data, const char *host)
{
    struct krb5_kafs_data *d = data->data;
    krb5_realm *realms;
    char *r;
    if(krb5_get_host_realm(d->context, host, &realms))
	return NULL;
    r = strdup(realms[0]);
    krb5_free_host_realm(d->context, realms);
    return r;
}

krb5_error_code
krb5_afslog_uid_home(krb5_context context,
		     krb5_ccache id,
		     const char *cell,
		     krb5_const_realm realm,
		     uid_t uid,
		     const char *homedir)
{
    kafs_data kd;
    struct krb5_kafs_data d;
    kd.afslog_uid = afslog_uid_int;
    kd.get_cred = get_cred;
    kd.get_realm = get_realm;
    kd.data = &d;
    d.context = context;
    d.id = id;
    d.realm = realm;
    return afslog_uid_int(&kd, cell, uid, homedir);
}

krb5_error_code
krb5_afslog_uid(krb5_context context,
		krb5_ccache id,
		const char *cell,
		krb5_const_realm realm,
		uid_t uid)
{
    return krb5_afslog_uid_home (context, id, cell, realm, uid, NULL);
}

krb5_error_code
krb5_afslog(krb5_context context,
	    krb5_ccache id, 
	    const char *cell,
	    krb5_const_realm realm)
{
    return krb5_afslog_uid (context, id, cell, realm, getuid());
}

krb5_error_code
krb5_afslog_home(krb5_context context,
		 krb5_ccache id, 
		 const char *cell,
		 krb5_const_realm realm,
		 const char *homedir)
{
    return krb5_afslog_uid_home (context, id, cell, realm, getuid(), homedir);
}

/*
 *
 */

krb5_error_code
krb5_realm_of_cell(const char *cell, char **realm)
{
    kafs_data kd;

    kd.get_realm = get_realm;
    return _kafs_realm_of_cell(&kd, cell, realm);
}
