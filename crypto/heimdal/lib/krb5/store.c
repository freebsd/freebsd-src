/*
 * Copyright (c) 1997-2002 Kungliga Tekniska Högskolan
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
#include "store-int.h"

RCSID("$Id: store.c,v 1.38.4.1 2004/03/09 19:32:14 lha Exp $");

#define BYTEORDER_IS(SP, V) (((SP)->flags & KRB5_STORAGE_BYTEORDER_MASK) == (V))
#define BYTEORDER_IS_LE(SP) BYTEORDER_IS((SP), KRB5_STORAGE_BYTEORDER_LE)
#define BYTEORDER_IS_BE(SP) BYTEORDER_IS((SP), KRB5_STORAGE_BYTEORDER_BE)
#define BYTEORDER_IS_HOST(SP) (BYTEORDER_IS((SP), KRB5_STORAGE_BYTEORDER_HOST) || \
			       krb5_storage_is_flags((SP), KRB5_STORAGE_HOST_BYTEORDER))

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

void
krb5_storage_set_byteorder(krb5_storage *sp, krb5_flags byteorder)
{
    sp->flags &= ~KRB5_STORAGE_BYTEORDER_MASK;
    sp->flags |= byteorder;
}

krb5_flags
krb5_storage_get_byteorder(krb5_storage *sp, krb5_flags byteorder)
{
    return sp->flags & KRB5_STORAGE_BYTEORDER_MASK;
}

off_t
krb5_storage_seek(krb5_storage *sp, off_t offset, int whence)
{
    return (*sp->seek)(sp, offset, whence);
}

krb5_ssize_t
krb5_storage_read(krb5_storage *sp, void *buf, size_t len)
{
    return sp->fetch(sp, buf, len);
}

krb5_ssize_t
krb5_storage_write(krb5_storage *sp, const void *buf, size_t len)
{
    return sp->store(sp, buf, len);
}

void
krb5_storage_set_eof_code(krb5_storage *sp, int code)
{
    sp->eof_code = code;
}

krb5_ssize_t
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

krb5_ssize_t
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
    unsigned char v[16];

    if(len > sizeof(v))
	return EINVAL;
    _krb5_put_int(v, value, len);
    ret = sp->store(sp, v, len);
    if (ret != len)
	return (ret<0)?errno:sp->eof_code;
    return 0;
}

krb5_error_code
krb5_store_int32(krb5_storage *sp,
		 int32_t value)
{
    if(BYTEORDER_IS_HOST(sp))
	value = htonl(value);
    else if(BYTEORDER_IS_LE(sp))
	value = bswap32(value);
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
	return (ret<0)?errno:sp->eof_code;
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
    if(BYTEORDER_IS_HOST(sp))
	*value = htonl(*value);
    else if(BYTEORDER_IS_LE(sp))
	*value = bswap32(*value);
    return 0;
}

krb5_error_code
krb5_store_int16(krb5_storage *sp,
		 int16_t value)
{
    if(BYTEORDER_IS_HOST(sp))
	value = htons(value);
    else if(BYTEORDER_IS_LE(sp))
	value = bswap16(value);
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
    if(BYTEORDER_IS_HOST(sp))
	*value = htons(*value);
    else if(BYTEORDER_IS_LE(sp))
	*value = bswap16(*value);
    return 0;
}

krb5_error_code
krb5_store_int8(krb5_storage *sp,
		int8_t value)
{
    int ret;

    ret = sp->store(sp, &value, sizeof(value));
    if (ret != sizeof(value))
	return (ret<0)?errno:sp->eof_code;
    return 0;
}

krb5_error_code
krb5_ret_int8(krb5_storage *sp,
	      int8_t *value)
{
    int ret;

    ret = sp->fetch(sp, value, sizeof(*value));
    if (ret != sizeof(*value))
	return (ret<0)?errno:sp->eof_code;
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
	return sp->eof_code;
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
	    return (ret < 0)? errno : sp->eof_code;
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
	    return sp->eof_code;
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
	    return sp->eof_code;
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

static int32_t
bitswap32(int32_t b)
{
    int32_t r = 0;
    int i;
    for (i = 0; i < 32; i++) {
	r = r << 1 | (b & 1);
	b = b >> 1;
    }
    return r;
}


/*
 *
 */

krb5_error_code
_krb5_store_creds_internal(krb5_storage *sp, krb5_creds *creds, int v0_6)
{
    int ret;

    ret = krb5_store_principal(sp, creds->client);
    if(ret)
	return ret;
    ret = krb5_store_principal(sp, creds->server);
    if(ret)
	return ret;
    ret = krb5_store_keyblock(sp, creds->session);
    if(ret)
	return ret;
    ret = krb5_store_times(sp, creds->times);
    if(ret)
	return ret;
    ret = krb5_store_int8(sp, 0);  /* this is probably the
				enc-tkt-in-skey bit from KDCOptions */
    if(ret)
	return ret;
    if (v0_6) {
	ret = krb5_store_int32(sp, creds->flags.i);
	if(ret)
	    return ret;
    } else {
	ret = krb5_store_int32(sp, bitswap32(TicketFlags2int(creds->flags.b)));
	if(ret)
	    return ret;
    }
    ret = krb5_store_addrs(sp, creds->addresses);
    if(ret)
	return ret;
    ret = krb5_store_authdata(sp, creds->authdata);
    if(ret)
	return ret;
    ret = krb5_store_data(sp, creds->ticket);
    if(ret)
	return ret;
    ret = krb5_store_data(sp, creds->second_ticket);
    return ret;
}

/*
 * store `creds' on `sp' returning error or zero
 */

krb5_error_code
krb5_store_creds(krb5_storage *sp, krb5_creds *creds)
{
    return _krb5_store_creds_internal(sp, creds, 1);
}

krb5_error_code
_krb5_store_creds_heimdal_0_7(krb5_storage *sp, krb5_creds *creds)
{
    return _krb5_store_creds_internal(sp, creds, 0);
}

krb5_error_code
_krb5_store_creds_heimdal_pre_0_7(krb5_storage *sp, krb5_creds *creds)
{
    return _krb5_store_creds_internal(sp, creds, 1);
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
    /*
     * Runtime detect the what is the higher bits of the bitfield. If
     * any of the higher bits are set in the input data, its either a
     * new ticket flag (and this code need to be removed), or its a
     * MIT cache (or new Heimdal cache), lets change it to our current
     * format.
     */
    {
	u_int32_t mask = 0xffff0000;
	creds->flags.i = 0;
	creds->flags.b.anonymous = 1;
	if (creds->flags.i & mask)
	    mask = ~mask;
	if (dummy32 & mask)
	    dummy32 = bitswap32(dummy32);
    }
    creds->flags.i = dummy32;
    ret = krb5_ret_addrs (sp,  &creds->addresses);
    if(ret) goto cleanup;
    ret = krb5_ret_authdata (sp,  &creds->authdata);
    if(ret) goto cleanup;
    ret = krb5_ret_data (sp,  &creds->ticket);
    if(ret) goto cleanup;
    ret = krb5_ret_data (sp,  &creds->second_ticket);
cleanup:
    if(ret) {
#if 0	
	krb5_free_creds_contents(context, creds); /* XXX */
#endif
    }
    return ret;
}
