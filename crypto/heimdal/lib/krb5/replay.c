/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
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
#include <vis.h>

RCSID("$Id: replay.c,v 1.9 2001/07/03 19:33:13 assar Exp $");

struct krb5_rcache_data {
    char *name;
};

krb5_error_code
krb5_rc_resolve(krb5_context context,
		krb5_rcache id,
		const char *name)
{
    id->name = strdup(name);
    if(id->name == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return KRB5_RC_MALLOC;
    }
    return 0;
}

krb5_error_code
krb5_rc_resolve_type(krb5_context context,
		     krb5_rcache *id,
		     const char *type)
{
    if(strcmp(type, "FILE")) {
	krb5_set_error_string (context, "replay cache type %s not supported",
			       type);
	return KRB5_RC_TYPE_NOTFOUND;
    }
    *id = calloc(1, sizeof(**id));
    if(*id == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return KRB5_RC_MALLOC;
    }
    return 0;
}

krb5_error_code
krb5_rc_resolve_full(krb5_context context,
		     krb5_rcache *id,
		     const char *string_name)
{
    krb5_error_code ret;
    if(strncmp(string_name, "FILE:", 5)) {
	krb5_set_error_string (context, "replay cache type %s not supported",
			       string_name);
	return KRB5_RC_TYPE_NOTFOUND;
    }
    ret = krb5_rc_resolve_type(context, id, "FILE");
    if(ret)
	return ret;
    ret = krb5_rc_resolve(context, *id, string_name + 5);
    return ret;
}

const char *
krb5_rc_default_name(krb5_context context)
{
    return "FILE:/var/run/default_rcache";
}

const char *
krb5_rc_default_type(krb5_context context)
{
    return "FILE";
}

krb5_error_code
krb5_rc_default(krb5_context context,
		krb5_rcache *id)
{
    return krb5_rc_resolve_full(context, id, krb5_rc_default_name(context));
}

struct rc_entry{
    time_t stamp;
    unsigned char data[16];
};

krb5_error_code
krb5_rc_initialize(krb5_context context,
		   krb5_rcache id,
		   krb5_deltat auth_lifespan)
{
    FILE *f = fopen(id->name, "w");
    struct rc_entry tmp;
    int ret;

    if(f == NULL) {
	ret = errno;
	krb5_set_error_string (context, "open(%s): %s", id->name,
			       strerror(ret));
	return ret;
    }
    tmp.stamp = auth_lifespan;
    fwrite(&tmp, 1, sizeof(tmp), f);
    fclose(f);
    return 0;
}

krb5_error_code
krb5_rc_recover(krb5_context context,
		krb5_rcache id)
{
    return 0;
}

krb5_error_code
krb5_rc_destroy(krb5_context context,
		krb5_rcache id)
{
    int ret;

    if(remove(id->name) < 0) {
	ret = errno;
	krb5_set_error_string (context, "remove(%s): %s", id->name,
			       strerror(ret));
	return ret;
    }
    return krb5_rc_close(context, id);
}

krb5_error_code
krb5_rc_close(krb5_context context,
	      krb5_rcache id)
{
    free(id->name);
    free(id);
    return 0;
}

static void
checksum_authenticator(Authenticator *auth, void *data)
{
    MD5_CTX md5;
    int i;

    MD5_Init (&md5);
    MD5_Update (&md5, auth->crealm, strlen(auth->crealm));
    for(i = 0; i < auth->cname.name_string.len; i++)
	MD5_Update(&md5, auth->cname.name_string.val[i], 
		   strlen(auth->cname.name_string.val[i]));
    MD5_Update (&md5, &auth->ctime, sizeof(auth->ctime));
    MD5_Update (&md5, &auth->cusec, sizeof(auth->cusec));
    MD5_Final (data, &md5);
}

krb5_error_code
krb5_rc_store(krb5_context context,
	      krb5_rcache id,
	      krb5_donot_replay *rep)
{
    struct rc_entry ent, tmp;
    time_t t;
    FILE *f;
    int ret;

    ent.stamp = time(NULL);
    checksum_authenticator(rep, ent.data);
    f = fopen(id->name, "r");
    if(f == NULL) {
	ret = errno;
	krb5_set_error_string (context, "open(%s): %s", id->name,
			       strerror(ret));
	return ret;
    }
    fread(&tmp, sizeof(ent), 1, f);
    t = ent.stamp - tmp.stamp;
    while(fread(&tmp, sizeof(ent), 1, f)){
	if(tmp.stamp < t)
	    continue;
	if(memcmp(tmp.data, ent.data, sizeof(ent.data)) == 0){
	    fclose(f);
	    krb5_clear_error_string (context);
	    return KRB5_RC_REPLAY;
	}
    }
    if(ferror(f)){
	ret = errno;
	fclose(f);
	krb5_set_error_string (context, "%s: %s", id->name, strerror(ret));
	return ret;
    }
    fclose(f);
    f = fopen(id->name, "a");
    if(f == NULL) {
	krb5_set_error_string (context, "open(%s): %s", id->name,
			       strerror(errno));
	return KRB5_RC_IO_UNKNOWN;
    }
    fwrite(&ent, 1, sizeof(ent), f);
    fclose(f);
    return 0;
}

krb5_error_code
krb5_rc_expunge(krb5_context context,
		krb5_rcache id)
{
    return 0;
}

krb5_error_code
krb5_rc_get_lifespan(krb5_context context,
		     krb5_rcache id,
		     krb5_deltat *auth_lifespan)
{
    FILE *f = fopen(id->name, "r");
    int r;
    struct rc_entry ent;
    r = fread(&ent, sizeof(ent), 1, f);
    fclose(f);
    if(r){
	*auth_lifespan = ent.stamp;
	return 0;
    }
    krb5_clear_error_string (context);
    return KRB5_RC_IO_UNKNOWN;
}

const char*
krb5_rc_get_name(krb5_context context,
		 krb5_rcache id)
{
    return id->name;
}
		 
const char*
krb5_rc_get_type(krb5_context context,
		 krb5_rcache id)
{
    return "FILE";
}
		 
krb5_error_code
krb5_get_server_rcache(krb5_context context, 
		       const krb5_data *piece, 
		       krb5_rcache *id)
{
    krb5_rcache rcache;
    krb5_error_code ret;

    char *tmp = malloc(4 * piece->length + 1);
    char *name;

    if(tmp == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    strvisx(tmp, piece->data, piece->length, VIS_WHITE | VIS_OCTAL);
#ifdef HAVE_GETEUID
    asprintf(&name, "FILE:rc_%s_%u", tmp, (unsigned)geteuid());
#else
    asprintf(&name, "FILE:rc_%s", tmp);
#endif
    free(tmp);
    if(name == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }

    ret = krb5_rc_resolve_full(context, &rcache, name);
    free(name);
    if(ret)
	return ret;
    *id = rcache;
    return ret;
}
