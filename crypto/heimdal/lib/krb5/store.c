/*
 * Copyright (c) 1997-2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: store.c,v 1.34 2000/04/11 00:46:09 assar Exp $");

void
krb5_storage_set_flags(krb5_storage *sp, krb5_flags flags)
{
    sp->flags |= flags;
}

void
krb5_storage_clear_flags(krb5_storage *sp, krb5_flags flags)
{
    sp->flags &= ~flags;
}

krb5_boolean
krb5_storage_is_flags(krb5_storage *sp, krb5_flags flags)
{
    return (sp->flags & flags) == flags;
}

ssize_t
_krb5_put_int(void *buffer, unsigned long value, size_t size)
{
    unsigned char *p = buffer;
    int i;
    for (i = size - 1; i >= 0; i--) {
	p[i] = value & 0xff;
	value >>= 8;
    }
    return size;
}

ssize_t
_krb5_get_int(void *buffer, unsigned long *value, size_t size)
{
    unsigned char *p = buffer;
    unsigned long v = 0;
    int i;
    for (i = 0; i < size; i++)
	v = (v << 8) + p[i];
    *value = v;
    return size;
}

krb5_error_code
krb5_storage_free(krb5_storage *sp)
{
    if(sp->free)
	(*sp->free)(sp);
    free(sp->data);
    free(sp);
    return 0;
}

krb5_error_code
krb5_storage_to_data(krb5_storage *sp, krb5_data *data)
{
    off_t pos;
    size_t size;
    krb5_error_code ret;

    pos = sp->seek(sp, 0, SEEK_CUR);
    size = (size_t)sp->seek(sp, 0, SEEK_END);
    ret = krb5_data_alloc (data, size);
    if (ret) {
	sp->seek(sp, pos, SEEK_SET);
	return ret;
    }
    if (size) {
	sp->seek(sp, 0, SEEK_SET);
	sp->fetch(sp, data->data, data->length);
	sp->seek(sp, pos, SEEK_SET);
    }
    return 0;
}

static krb5_error_code
krb5_store_int(krb5_storage *sp,
	       int32_t value,
	       size_t len)
{
    int ret;
    unsigned char v[4];

    _krb5_put_int(v, value, len);
    ret = sp->store(sp, v, len);
    if (ret != len)
	return (ret<0)?errno:KRB5_CC_END;
    return 0;
}

krb5_error_code
krb5_store_int32(krb5_storage *sp,
		 int32_t value)
{
    if(krb5_storage_is_flags(sp, KRB5_STORAGE_HOST_BYTEORDER))
	value = htonl(value);
    return krb5_store_int(sp, value, 4);
}

static krb5_error_code
krb5_ret_int(krb5_storage *sp,
	     int32_t *value,
	     size_t len)
{
    int ret;
    unsigned char v[4];
    unsigned long w;
    ret = sp->fetch(sp, v, len);
    if(ret != len)
	return (ret<0)?errno:KRB5_CC_END;
    _krb5_get_int(v, &w, len);
    *value = w;
    return 0;
}

krb5_error_code
krb5_ret_int32(krb5_storage *sp,
	       int32_t *value)
{
    krb5_error_code ret = krb5_ret_int(sp, value, 4);
    if(ret)
	return ret;
    if(krb5_storage_is_flags(sp, KRB5_STORAGE_HOST_BYTEORDER))
	*value = ntohl(*value);
    return 0;
}

krb5_error_code
krb5_store_int16(krb5_storage *sp,
		 int16_t value)
{
    if(krb5_storage_is_flags(sp, KRB5_STORAGE_HOST_BYTEORDER))
	value = htons(value);
    return krb5_store_int(sp, value, 2);
}

krb5_error_code
krb5_ret_int16(krb5_storage *sp,
	       int16_t *value)
{
    int32_t v;
    int ret;
    ret = krb5_ret_int(sp, &v, 2);
    if(ret)
	return ret;
    *value = v;
    if(krb5_storage_is_flags(sp, KRB5_STORAGE_HOST_BYTEORDER))
	*value = ntohs(*value);
    return 0;
}

krb5_error_code
krb5_store_int8(krb5_storage *sp,
		int8_t value)
{
    int ret;

    ret = sp->store(sp, &value, sizeof(value));
    if (ret != sizeof(value))
	return (ret<0)?errno:KRB5_CC_END;
    return 0;
}

krb5_error_code
krb5_ret_int8(krb5_storage *sp,
	      int8_t *value)
{
    int ret;

    ret = sp->fetch(sp, value, sizeof(*value));
    if (ret != sizeof(*value))
	return (ret<0)?errno:KRB5_CC_END;
    return 0;
}

krb5_error_code
krb5_store_data(krb5_storage *sp,
		krb5_data data)
{
    int ret;
    ret = krb5_store_int32(sp, data.length);
    if(ret < 0)
	return ret;
    ret = sp->store(sp, data.data, data.length);
    if(ret != data.length){
	if(ret < 0)
	    return errno;
	return KRB5_CC_END;
    }
    return 0;
}

krb5_error_code
krb5_ret_data(krb5_storage *sp,
	      krb5_data *data)
{
    int ret;
    int32_t size;

    ret = krb5_ret_int32(sp, &size);
    if(ret)
	return ret;
    ret = krb5_data_alloc (data, size);
    if (ret)
	return ret;
    if (size) {
	ret = sp->fetch(sp, data->data, size);
	if(ret != size)
	    return (ret < 0)? errno : KRB5_CC_END;
    }
    return 0;
}

krb5_error_code
krb5_store_string(krb5_storage *sp, const char *s)
{
    krb5_data data;
    data.length = strlen(s);
    data.data = (void*)s;
    return krb5_store_data(sp, data);
}

krb5_error_code
krb5_ret_string(krb5_storage *sp,
		char **string)
{
    int ret;
    krb5_data data;
    ret = krb5_ret_data(sp, &data);
    if(ret)
	return ret;
    *string = realloc(data.data, data.length + 1);
    if(*string == NULL){
	free(data.data);
	return ENOMEM;
    }
    (*string)[data.length] = 0;
    return 0;
}

krb5_error_code
krb5_store_stringz(krb5_storage *sp, const char *s)
{
    size_t len = strlen(s) + 1;
    ssize_t ret;

    ret = sp->store(sp, s, len);
    if(ret != len) {
	if(ret < 0)
	    return ret;
	else
	    return KRB5_CC_END;
    }
    return 0;
}

krb5_error_code
krb5_ret_stringz(krb5_storage *sp,
		char **string)
{
    char c;
    char *s = NULL;
    size_t len = 0;
    ssize_t ret;

    while((ret = sp->fetch(sp, &c, 1)) == 1){
	char *tmp;

	len++;
	tmp = realloc (s, len);
	if (tmp == NULL) {
	    free (s);
	    return ENOMEM;
	}
	s = tmp;
	s[len - 1] = c;
	if(c == 0)
	    break;
    }
    if(ret != 1){
	free(s);
	if(ret == 0)
	    return KRB5_CC_END;
	return ret;
    }
    *string = s;
    return 0;
}


krb5_error_code
krb5_store_principal(krb5_storage *sp,
		     krb5_principal p)
{
    int i;
    int ret;

    if(!krb5_storage_is_flags(sp, KRB5_STORAGE_PRINCIPAL_NO_NAME_TYPE)) {
    ret = krb5_store_int32(sp, p->name.name_type);
    if(ret) return ret;
    }
    if(krb5_storage_is_flags(sp, KRB5_STORAGE_PRINCIPAL_WRONG_NUM_COMPONENTS))
	ret = krb5_store_int32(sp, p->name.name_string.len + 1);
    else
    ret = krb5_store_int32(sp, p->name.name_string.len);
    
    if(ret) return ret;
    ret = krb5_store_string(sp, p->realm);
    if(ret) return ret;
    for(i = 0; i < p->name.name_string.len; i++){
	ret = krb5_store_string(sp, p->name.name_string.val[i]);
	if(ret) return ret;
    }
    return 0;
}

krb5_error_code
krb5_ret_principal(krb5_storage *sp,
		   krb5_principal *princ)
{
    int i;
    int ret;
    krb5_principal p;
    int32_t type;
    int32_t ncomp;
    
    p = calloc(1, sizeof(*p));
    if(p == NULL)
	return ENOMEM;

    if(krb5_storage_is_flags(sp, KRB5_STORAGE_PRINCIPAL_NO_NAME_TYPE))
	type = KRB5_NT_UNKNOWN;
    else 	if((ret = krb5_ret_int32(sp, &type))){
	free(p);
	return ret;
    }
    if((ret = krb5_ret_int32(sp, &ncomp))){
	free(p);
	return ret;
    }
    if(krb5_storage_is_flags(sp, KRB5_STORAGE_PRINCIPAL_WRONG_NUM_COMPONENTS))
	ncomp--;
    p->name.name_type = type;
    p->name.name_string.len = ncomp;
    ret = krb5_ret_string(sp, &p->realm);
    if(ret) return ret;
    p->name.name_string.val = calloc(ncomp, sizeof(*p->name.name_string.val));
    if(p->name.name_string.val == NULL){
	free(p->realm);
	return ENOMEM;
    }
    for(i = 0; i < ncomp; i++){
	ret = krb5_ret_string(sp, &p->name.name_string.val[i]);
	if(ret) return ret; /* XXX */
    }
    *princ = p;
    return 0;
}

krb5_error_code
krb5_store_keyblock(krb5_storage *sp, krb5_keyblock p)
{
    int ret;
    ret = krb5_store_int16(sp, p.keytype);
    if(ret) return ret;

    if(krb5_storage_is_flags(sp, KRB5_STORAGE_KEYBLOCK_KEYTYPE_TWICE)){
	/* this should really be enctype, but it is the same as
           keytype nowadays */
    ret = krb5_store_int16(sp, p.keytype);
    if(ret) return ret;
    }

    ret = krb5_store_data(sp, p.keyvalue);
    return ret;
}

krb5_error_code
krb5_ret_keyblock(krb5_storage *sp, krb5_keyblock *p)
{
    int ret;
    int16_t tmp;

    ret = krb5_ret_int16(sp, &tmp);
    if(ret) return ret;
    p->keytype = tmp;

    if(krb5_storage_is_flags(sp, KRB5_STORAGE_KEYBLOCK_KEYTYPE_TWICE)){
    ret = krb5_ret_int16(sp, &tmp);
    if(ret) return ret;
    }

    ret = krb5_ret_data(sp, &p->keyvalue);
    return ret;
}

krb5_error_code
krb5_store_times(krb5_storage *sp, krb5_times times)
{
    int ret;
    ret = krb5_store_int32(sp, times.authtime);
    if(ret) return ret;
    ret = krb5_store_int32(sp, times.starttime);
    if(ret) return ret;
    ret = krb5_store_int32(sp, times.endtime);
    if(ret) return ret;
    ret = krb5_store_int32(sp, times.renew_till);
    return ret;
}

krb5_error_code
krb5_ret_times(krb5_storage *sp, krb5_times *times)
{
    int ret;
    int32_t tmp;
    ret = krb5_ret_int32(sp, &tmp);
    times->authtime = tmp;
    if(ret) return ret;
    ret = krb5_ret_int32(sp, &tmp);
    times->starttime = tmp;
    if(ret) return ret;
    ret = krb5_ret_int32(sp, &tmp);
    times->endtime = tmp;
    if(ret) return ret;
    ret = krb5_ret_int32(sp, &tmp);
    times->renew_till = tmp;
    return ret;
}

krb5_error_code
krb5_store_address(krb5_storage *sp, krb5_address p)
{
    int ret;
    ret = krb5_store_int16(sp, p.addr_type);
    if(ret) return ret;
    ret = krb5_store_data(sp, p.address);
    return ret;
}

krb5_error_code
krb5_ret_address(krb5_storage *sp, krb5_address *adr)
{
    int16_t t;
    int ret;
    ret = krb5_ret_int16(sp, &t);
    if(ret) return ret;
    adr->addr_type = t;
    ret = krb5_ret_data(sp, &adr->address);
    return ret;
}

krb5_error_code
krb5_store_addrs(krb5_storage *sp, krb5_addresses p)
{
    int i;
    int ret;
    ret = krb5_store_int32(sp, p.len);
    if(ret) return ret;
    for(i = 0; i<p.len; i++){
	ret = krb5_store_address(sp, p.val[i]);
	if(ret) break;
    }
    return ret;
}

krb5_error_code
krb5_ret_addrs(krb5_storage *sp, krb5_addresses *adr)
{
    int i;
    int ret;
    int32_t tmp;

    ret = krb5_ret_int32(sp, &tmp);
    if(ret) return ret;
    adr->len = tmp;
    ALLOC(adr->val, adr->len);
    for(i = 0; i < adr->len; i++){
	ret = krb5_ret_address(sp, &adr->val[i]);
	if(ret) break;
    }
    return ret;
}

krb5_error_code
krb5_store_authdata(krb5_storage *sp, krb5_authdata auth)
{
    krb5_error_code ret;
    int i;
    ret = krb5_store_int32(sp, auth.len);
    if(ret) return ret;
    for(i = 0; i < auth.len; i++){
	ret = krb5_store_int16(sp, auth.val[i].ad_type);
	if(ret) break;
	ret = krb5_store_data(sp, auth.val[i].ad_data);
	if(ret) break;
    }
    return 0;
}

krb5_error_code
krb5_ret_authdata(krb5_storage *sp, krb5_authdata *auth)
{
    krb5_error_code ret;
    int32_t tmp;
    int16_t tmp2;
    int i;
    ret = krb5_ret_int32(sp, &tmp);
    if(ret) return ret;
    ALLOC_SEQ(auth, tmp);
    for(i = 0; i < tmp; i++){
	ret = krb5_ret_int16(sp, &tmp2);
	if(ret) break;
	auth->val[i].ad_type = tmp2;
	ret = krb5_ret_data(sp, &auth->val[i].ad_data);
	if(ret) break;
    }
    return ret;
}

/*
 * store `creds' on `sp' returning error or zero
 */

krb5_error_code
krb5_store_creds(krb5_storage *sp, krb5_creds *creds)
{
    int ret;

    ret = krb5_store_principal(sp, creds->client);
    if (ret)
	return ret;
    ret = krb5_store_principal(sp, creds->server);
    if (ret)
	return ret;
    ret = krb5_store_keyblock(sp, creds->session);
    if (ret)
	return ret;
    ret = krb5_store_times(sp, creds->times);
    if (ret)
	return ret;
    ret = krb5_store_int8(sp, 0);  /* this is probably the
				enc-tkt-in-skey bit from KDCOptions */
    if (ret)
	return ret;
    ret = krb5_store_int32(sp, creds->flags.i);
    if (ret)
	return ret;
    ret = krb5_store_addrs(sp, creds->addresses);
    if (ret)
	return ret;
    ret = krb5_store_authdata(sp, creds->authdata);
    if (ret)
	return ret;
    ret = krb5_store_data(sp, creds->ticket);
    if (ret)
	return ret;
    ret = krb5_store_data(sp, creds->second_ticket);
    if (ret)
	return ret;
    return 0;
}

krb5_error_code
krb5_ret_creds(krb5_storage *sp, krb5_creds *creds)
{
    krb5_error_code ret;
    int8_t dummy8;
    int32_t dummy32;

    memset(creds, 0, sizeof(*creds));
    ret = krb5_ret_principal (sp,  &creds->client);
    if(ret) goto cleanup;
    ret = krb5_ret_principal (sp,  &creds->server);
    if(ret) goto cleanup;
    ret = krb5_ret_keyblock (sp,  &creds->session);
    if(ret) goto cleanup;
    ret = krb5_ret_times (sp,  &creds->times);
    if(ret) goto cleanup;
    ret = krb5_ret_int8 (sp,  &dummy8);
    if(ret) goto cleanup;
    ret = krb5_ret_int32 (sp,  &dummy32);
    if(ret) goto cleanup;
    creds->flags.i = dummy32;
    ret = krb5_ret_addrs (sp,  &creds->addresses);
    if(ret) goto cleanup;
    ret = krb5_ret_authdata (sp,  &creds->authdata);
    if(ret) goto cleanup;
    ret = krb5_ret_data (sp,  &creds->ticket);
    if(ret) goto cleanup;
    ret = krb5_ret_data (sp,  &creds->second_ticket);
cleanup:
    if(ret)
#if 0	
	krb5_free_creds_contents(context, creds) /* XXX */
#endif
	    ;
    return ret;
}
