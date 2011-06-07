/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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

RCSID("$Id: pac.c 21934 2007-08-27 14:21:04Z lha $");

struct PAC_INFO_BUFFER {
    uint32_t type;
    uint32_t buffersize;
    uint32_t offset_hi;
    uint32_t offset_lo;
};

struct PACTYPE {
    uint32_t numbuffers;
    uint32_t version;                         
    struct PAC_INFO_BUFFER buffers[1];
};

struct krb5_pac_data {
    struct PACTYPE *pac;
    krb5_data data;
    struct PAC_INFO_BUFFER *server_checksum;
    struct PAC_INFO_BUFFER *privsvr_checksum;
    struct PAC_INFO_BUFFER *logon_name;
};

#define PAC_ALIGNMENT			8

#define PACTYPE_SIZE			8
#define PAC_INFO_BUFFER_SIZE		16

#define PAC_SERVER_CHECKSUM		6
#define PAC_PRIVSVR_CHECKSUM		7
#define PAC_LOGON_NAME			10
#define PAC_CONSTRAINED_DELEGATION	11

#define CHECK(r,f,l)						\
	do {							\
		if (((r) = f ) != 0) {				\
			krb5_clear_error_string(context);	\
			goto l;					\
		}						\
	} while(0)

static const char zeros[PAC_ALIGNMENT] = { 0 };

/*
 *
 */

krb5_error_code
krb5_pac_parse(krb5_context context, const void *ptr, size_t len,
	       krb5_pac *pac)
{
    krb5_error_code ret;
    krb5_pac p;
    krb5_storage *sp = NULL;
    uint32_t i, tmp, tmp2, header_end;

    p = calloc(1, sizeof(*p));
    if (p == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "out of memory");
	goto out;
    }

    sp = krb5_storage_from_readonly_mem(ptr, len);
    if (sp == NULL) {
	ret = ENOMEM;
	krb5_set_error_string(context, "out of memory");
	goto out;
    }
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(ret, krb5_ret_uint32(sp, &tmp), out);
    CHECK(ret, krb5_ret_uint32(sp, &tmp2), out);
    if (tmp < 1) {
	krb5_set_error_string(context, "PAC have too few buffer");
	ret = EINVAL; /* Too few buffers */
	goto out;
    }
    if (tmp2 != 0) {
	krb5_set_error_string(context, "PAC have wrong version");
	ret = EINVAL; /* Wrong version */
	goto out;
    }

    p->pac = calloc(1, 
		    sizeof(*p->pac) + (sizeof(p->pac->buffers[0]) * (tmp - 1)));
    if (p->pac == NULL) {
	krb5_set_error_string(context, "out of memory");
	ret = ENOMEM;
	goto out;
    }

    p->pac->numbuffers = tmp;
    p->pac->version = tmp2;

    header_end = PACTYPE_SIZE + (PAC_INFO_BUFFER_SIZE * p->pac->numbuffers);
    if (header_end > len) {
	ret = EINVAL;
	goto out;
    }

    for (i = 0; i < p->pac->numbuffers; i++) {
	CHECK(ret, krb5_ret_uint32(sp, &p->pac->buffers[i].type), out);
	CHECK(ret, krb5_ret_uint32(sp, &p->pac->buffers[i].buffersize), out);
	CHECK(ret, krb5_ret_uint32(sp, &p->pac->buffers[i].offset_lo), out);
	CHECK(ret, krb5_ret_uint32(sp, &p->pac->buffers[i].offset_hi), out);

	/* consistency checks */
	if (p->pac->buffers[i].offset_lo & (PAC_ALIGNMENT - 1)) {
	    krb5_set_error_string(context, "PAC out of allignment");
	    ret = EINVAL;
	    goto out;
	}
	if (p->pac->buffers[i].offset_hi) {
	    krb5_set_error_string(context, "PAC high offset set");
	    ret = EINVAL;
	    goto out;
	}
	if (p->pac->buffers[i].offset_lo > len) {
	    krb5_set_error_string(context, "PAC offset off end");
	    ret = EINVAL;
	    goto out;
	}
	if (p->pac->buffers[i].offset_lo < header_end) {
	    krb5_set_error_string(context, "PAC offset inside header: %d %d",
				  p->pac->buffers[i].offset_lo, header_end);
	    ret = EINVAL;
	    goto out;
	}
	if (p->pac->buffers[i].buffersize > len - p->pac->buffers[i].offset_lo){
	    krb5_set_error_string(context, "PAC length off end");
	    ret = EINVAL;
	    goto out;
	}

	/* let save pointer to data we need later */
	if (p->pac->buffers[i].type == PAC_SERVER_CHECKSUM) {
	    if (p->server_checksum) {
		krb5_set_error_string(context, "PAC have two server checksums");
		ret = EINVAL;
		goto out;
	    }
	    p->server_checksum = &p->pac->buffers[i];
	} else if (p->pac->buffers[i].type == PAC_PRIVSVR_CHECKSUM) {
	    if (p->privsvr_checksum) {
		krb5_set_error_string(context, "PAC have two KDC checksums");
		ret = EINVAL;
		goto out;
	    }
	    p->privsvr_checksum = &p->pac->buffers[i];
	} else if (p->pac->buffers[i].type == PAC_LOGON_NAME) {
	    if (p->logon_name) {
		krb5_set_error_string(context, "PAC have two logon names");
		ret = EINVAL;
		goto out;
	    }
	    p->logon_name = &p->pac->buffers[i];
	}
    }

    ret = krb5_data_copy(&p->data, ptr, len);
    if (ret)
	goto out;

    krb5_storage_free(sp);

    *pac = p;
    return 0;

out:
    if (sp)
	krb5_storage_free(sp);
    if (p) {
	if (p->pac)
	    free(p->pac);
	free(p);
    }
    *pac = NULL;

    return ret;
}

krb5_error_code
krb5_pac_init(krb5_context context, krb5_pac *pac)
{
    krb5_error_code ret;
    krb5_pac p;

    p = calloc(1, sizeof(*p));
    if (p == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }

    p->pac = calloc(1, sizeof(*p->pac));
    if (p->pac == NULL) {
	free(p);
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }

    ret = krb5_data_alloc(&p->data, PACTYPE_SIZE);
    if (ret) {
	free (p->pac);
	free(p);
	krb5_set_error_string(context, "out of memory");
	return ret;
    }


    *pac = p;
    return 0;
}

krb5_error_code
krb5_pac_add_buffer(krb5_context context, krb5_pac p,
		    uint32_t type, const krb5_data *data)
{
    krb5_error_code ret;
    void *ptr;
    size_t len, offset, header_end, old_end;
    uint32_t i;

    len = p->pac->numbuffers;

    ptr = realloc(p->pac,
		  sizeof(*p->pac) + (sizeof(p->pac->buffers[0]) * len));
    if (ptr == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    p->pac = ptr;

    for (i = 0; i < len; i++)
	p->pac->buffers[i].offset_lo += PAC_INFO_BUFFER_SIZE;

    offset = p->data.length + PAC_INFO_BUFFER_SIZE;

    p->pac->buffers[len].type = type;
    p->pac->buffers[len].buffersize = data->length;
    p->pac->buffers[len].offset_lo = offset;
    p->pac->buffers[len].offset_hi = 0;

    old_end = p->data.length;
    len = p->data.length + data->length + PAC_INFO_BUFFER_SIZE;
    if (len < p->data.length) {
	krb5_set_error_string(context, "integer overrun");
	return EINVAL;
    }
    
    /* align to PAC_ALIGNMENT */
    len = ((len + PAC_ALIGNMENT - 1) / PAC_ALIGNMENT) * PAC_ALIGNMENT;

    ret = krb5_data_realloc(&p->data, len);
    if (ret) {
	krb5_set_error_string(context, "out of memory");
	return ret;
    }

    /* 
     * make place for new PAC INFO BUFFER header
     */
    header_end = PACTYPE_SIZE + (PAC_INFO_BUFFER_SIZE * p->pac->numbuffers);
    memmove((unsigned char *)p->data.data + header_end + PAC_INFO_BUFFER_SIZE,
	    (unsigned char *)p->data.data + header_end ,
	    old_end - header_end);
    memset((unsigned char *)p->data.data + header_end, 0, PAC_INFO_BUFFER_SIZE);

    /*
     * copy in new data part
     */

    memcpy((unsigned char *)p->data.data + offset,
	   data->data, data->length);
    memset((unsigned char *)p->data.data + offset + data->length,
	   0, p->data.length - offset - data->length);

    p->pac->numbuffers += 1;

    return 0;
}

krb5_error_code
krb5_pac_get_buffer(krb5_context context, krb5_pac p,
		    uint32_t type, krb5_data *data)
{
    krb5_error_code ret;
    uint32_t i;

    /*
     * Hide the checksums from external consumers
     */

    if (type == PAC_PRIVSVR_CHECKSUM || type == PAC_SERVER_CHECKSUM) {
	ret = krb5_data_alloc(data, 16);
	if (ret) {
	    krb5_set_error_string(context, "out of memory");
	    return ret;
	}
	memset(data->data, 0, data->length);
	return 0;
    }

    for (i = 0; i < p->pac->numbuffers; i++) {
	size_t len = p->pac->buffers[i].buffersize;
	size_t offset = p->pac->buffers[i].offset_lo;

	if (p->pac->buffers[i].type != type)
	    continue;

	ret = krb5_data_copy(data, (unsigned char *)p->data.data + offset, len);
	if (ret) {
	    krb5_set_error_string(context, "Out of memory");
	    return ret;
	}
	return 0;
    }
    krb5_set_error_string(context, "No PAC buffer of type %lu was found",
			  (unsigned long)type);
    return ENOENT;
}

/*
 *
 */

krb5_error_code
krb5_pac_get_types(krb5_context context,
		   krb5_pac p,
		   size_t *len,
		   uint32_t **types)
{
    size_t i;

    *types = calloc(p->pac->numbuffers, sizeof(*types));
    if (*types == NULL) {
	*len = 0;
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    for (i = 0; i < p->pac->numbuffers; i++)
	(*types)[i] = p->pac->buffers[i].type;
    *len = p->pac->numbuffers;

    return 0;
}

/*
 *
 */

void
krb5_pac_free(krb5_context context, krb5_pac pac)
{
    krb5_data_free(&pac->data);
    free(pac->pac);
    free(pac);
}

/*
 *
 */

static krb5_error_code
verify_checksum(krb5_context context,
		const struct PAC_INFO_BUFFER *sig,
		const krb5_data *data,
		void *ptr, size_t len,
		const krb5_keyblock *key)
{
    krb5_crypto crypto = NULL;
    krb5_storage *sp = NULL;
    uint32_t type;
    krb5_error_code ret;
    Checksum cksum;

    memset(&cksum, 0, sizeof(cksum));

    sp = krb5_storage_from_mem((char *)data->data + sig->offset_lo,
			       sig->buffersize);
    if (sp == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(ret, krb5_ret_uint32(sp, &type), out);
    cksum.cksumtype = type;
    cksum.checksum.length = 
	sig->buffersize - krb5_storage_seek(sp, 0, SEEK_CUR);
    cksum.checksum.data = malloc(cksum.checksum.length);
    if (cksum.checksum.data == NULL) {
	krb5_set_error_string(context, "out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = krb5_storage_read(sp, cksum.checksum.data, cksum.checksum.length);
    if (ret != cksum.checksum.length) {
	krb5_set_error_string(context, "PAC checksum missing checksum");
	ret = EINVAL;
	goto out;
    }

    if (!krb5_checksum_is_keyed(context, cksum.cksumtype)) {
	krb5_set_error_string (context, "Checksum type %d not keyed",
			       cksum.cksumtype);
	ret = EINVAL;
	goto out;
    }

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	goto out;

    ret = krb5_verify_checksum(context, crypto, KRB5_KU_OTHER_CKSUM,
			       ptr, len, &cksum);
    free(cksum.checksum.data);
    krb5_crypto_destroy(context, crypto);
    krb5_storage_free(sp);

    return ret;

out:
    if (cksum.checksum.data)
	free(cksum.checksum.data);
    if (sp)
	krb5_storage_free(sp);
    if (crypto)
	krb5_crypto_destroy(context, crypto);
    return ret;
}

static krb5_error_code
create_checksum(krb5_context context,
		const krb5_keyblock *key,
		void *data, size_t datalen,
		void *sig, size_t siglen)
{
    krb5_crypto crypto = NULL;
    krb5_error_code ret;
    Checksum cksum;

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	return ret;

    ret = krb5_create_checksum(context, crypto, KRB5_KU_OTHER_CKSUM, 0,
			       data, datalen, &cksum);
    krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;

    if (cksum.checksum.length != siglen) {
	krb5_set_error_string(context, "pac checksum wrong length");
	free_Checksum(&cksum);
	return EINVAL;
    }

    memcpy(sig, cksum.checksum.data, siglen);
    free_Checksum(&cksum);

    return 0;
}


/*
 *
 */

#define NTTIME_EPOCH 0x019DB1DED53E8000LL

static uint64_t
unix2nttime(time_t unix_time)
{
    long long wt;
    wt = unix_time * (uint64_t)10000000 + (uint64_t)NTTIME_EPOCH;
    return wt;
}

static krb5_error_code
verify_logonname(krb5_context context,
		 const struct PAC_INFO_BUFFER *logon_name,
		 const krb5_data *data,
		 time_t authtime,
		 krb5_const_principal principal)
{
    krb5_error_code ret;
    krb5_principal p2;
    uint32_t time1, time2;
    krb5_storage *sp;
    uint16_t len;
    char *s;

    sp = krb5_storage_from_readonly_mem((const char *)data->data + logon_name->offset_lo,
					logon_name->buffersize);
    if (sp == NULL) {
	krb5_set_error_string(context, "Out of memory");
	return ENOMEM;
    }

    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(ret, krb5_ret_uint32(sp, &time1), out);
    CHECK(ret, krb5_ret_uint32(sp, &time2), out);

    {
	uint64_t t1, t2;
	t1 = unix2nttime(authtime);
	t2 = ((uint64_t)time2 << 32) | time1;
	if (t1 != t2) {
	    krb5_storage_free(sp);
	    krb5_set_error_string(context, "PAC timestamp mismatch");
	    return EINVAL;
	}
    }
    CHECK(ret, krb5_ret_uint16(sp, &len), out);
    if (len == 0) {
	krb5_storage_free(sp);
	krb5_set_error_string(context, "PAC logon name length missing");
	return EINVAL;
    }

    s = malloc(len);
    if (s == NULL) {
	krb5_storage_free(sp);
	krb5_set_error_string(context, "Out of memory");
	return ENOMEM;
    }
    ret = krb5_storage_read(sp, s, len);
    if (ret != len) {
	krb5_storage_free(sp);
	krb5_set_error_string(context, "Failed to read pac logon name");
	return EINVAL;
    }
    krb5_storage_free(sp);
#if 1 /* cheat for now */
    {
	size_t i;

	if (len & 1) {
	    krb5_set_error_string(context, "PAC logon name malformed");
	    return EINVAL;
	}

	for (i = 0; i < len / 2; i++) {
	    if (s[(i * 2) + 1]) {
		krb5_set_error_string(context, "PAC logon name not ASCII");
		return EINVAL;
	    }
	    s[i] = s[i * 2];
	}
	s[i] = '\0';
    }
#else
    {
	uint16_t *ucs2;
	ssize_t ucs2len;
	size_t u8len;

	ucs2 = malloc(sizeof(ucs2[0]) * len / 2);
	if (ucs2)
	    abort();
	ucs2len = wind_ucs2read(s, len / 2, ucs2);
	free(s);
	if (len < 0)
	    return -1;
	ret = wind_ucs2toutf8(ucs2, ucs2len, NULL, &u8len);
	if (ret < 0)
	    abort();
	s = malloc(u8len + 1);
	if (s == NULL)
	    abort();
	wind_ucs2toutf8(ucs2, ucs2len, s, &u8len);
	free(ucs2);
    }
#endif
    ret = krb5_parse_name_flags(context, s, KRB5_PRINCIPAL_PARSE_NO_REALM, &p2);
    free(s);
    if (ret)
	return ret;
    
    if (krb5_principal_compare_any_realm(context, principal, p2) != TRUE) {
	krb5_set_error_string(context, "PAC logon name mismatch");
	ret = EINVAL;
    }
    krb5_free_principal(context, p2);
    return ret;
out:
    return ret;
}

/*
 *
 */

static krb5_error_code
build_logon_name(krb5_context context, 
		 time_t authtime,
		 krb5_const_principal principal, 
		 krb5_data *logon)
{
    krb5_error_code ret;
    krb5_storage *sp;
    uint64_t t;
    char *s, *s2;
    size_t i, len;

    t = unix2nttime(authtime);

    krb5_data_zero(logon);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(ret, krb5_store_uint32(sp, t & 0xffffffff), out);
    CHECK(ret, krb5_store_uint32(sp, t >> 32), out);

    ret = krb5_unparse_name_flags(context, principal,
				  KRB5_PRINCIPAL_UNPARSE_NO_REALM, &s);
    if (ret)
	goto out;

    len = strlen(s);
    
    CHECK(ret, krb5_store_uint16(sp, len * 2), out);

#if 1 /* cheat for now */
    s2 = malloc(len * 2);
    if (s2 == NULL) {
	ret = ENOMEM;
	free(s);
	goto out;
    }
    for (i = 0; i < len; i++) {
	s2[i * 2] = s[i];
	s2[i * 2 + 1] = 0;
    }
    free(s);
#else
    /* write libwind code here */
#endif

    ret = krb5_storage_write(sp, s2, len * 2);
    free(s2);
    if (ret != len * 2) {
	ret = ENOMEM;
	goto out;
    }
    ret = krb5_storage_to_data(sp, logon);
    if (ret)
	goto out;
    krb5_storage_free(sp);

    return 0;
out:
    krb5_storage_free(sp);
    return ret;
}


/*
 *
 */

krb5_error_code
krb5_pac_verify(krb5_context context, 
		const krb5_pac pac,
		time_t authtime,
		krb5_const_principal principal,
		const krb5_keyblock *server,
		const krb5_keyblock *privsvr)
{
    krb5_error_code ret;

    if (pac->server_checksum == NULL) {
	krb5_set_error_string(context, "PAC missing server checksum");
	return EINVAL;
    }
    if (pac->privsvr_checksum == NULL) {
	krb5_set_error_string(context, "PAC missing kdc checksum");
	return EINVAL;
    }
    if (pac->logon_name == NULL) {
	krb5_set_error_string(context, "PAC missing logon name");
	return EINVAL;
    }

    ret = verify_logonname(context, 
			   pac->logon_name,
			   &pac->data,
			   authtime,
			   principal);
    if (ret)
	return ret;

    /* 
     * in the service case, clean out data option of the privsvr and
     * server checksum before checking the checksum.
     */
    {
	krb5_data *copy;

	ret = krb5_copy_data(context, &pac->data, &copy);
	if (ret)
	    return ret;

	if (pac->server_checksum->buffersize < 4)
	    return EINVAL;
	if (pac->privsvr_checksum->buffersize < 4)
	    return EINVAL;

	memset((char *)copy->data + pac->server_checksum->offset_lo + 4,
	       0,
	       pac->server_checksum->buffersize - 4);

	memset((char *)copy->data + pac->privsvr_checksum->offset_lo + 4,
	       0,
	       pac->privsvr_checksum->buffersize - 4);

	ret = verify_checksum(context,
			      pac->server_checksum,
			      &pac->data,
			      copy->data,
			      copy->length,
			      server);
	krb5_free_data(context, copy);
	if (ret)
	    return ret;
    }
    if (privsvr) {
	ret = verify_checksum(context,
			      pac->privsvr_checksum,
			      &pac->data,
			      (char *)pac->data.data
			      + pac->server_checksum->offset_lo + 4,
			      pac->server_checksum->buffersize - 4,
			      privsvr);
	if (ret)
	    return ret;
    }

    return 0;
}

/*
 *
 */

static krb5_error_code
fill_zeros(krb5_context context, krb5_storage *sp, size_t len)
{
    ssize_t sret;
    size_t l;

    while (len) {
	l = len;
	if (l > sizeof(zeros))
	    l = sizeof(zeros);
	sret = krb5_storage_write(sp, zeros, l);
	if (sret <= 0) {
	    krb5_set_error_string(context, "out of memory");
	    return ENOMEM;
	}
	len -= sret;
    }
    return 0;
}

static krb5_error_code
pac_checksum(krb5_context context, 
	     const krb5_keyblock *key,
	     uint32_t *cksumtype,
	     size_t *cksumsize)
{
    krb5_cksumtype cktype;
    krb5_error_code ret;
    krb5_crypto crypto = NULL;

    ret = krb5_crypto_init(context, key, 0, &crypto);
    if (ret)
	return ret;

    ret = krb5_crypto_get_checksum_type(context, crypto, &cktype);
    ret = krb5_crypto_destroy(context, crypto);
    if (ret)
	return ret;

    if (krb5_checksum_is_keyed(context, cktype) == FALSE) {
	krb5_set_error_string(context, "PAC checksum type is not keyed");
	return EINVAL;
    }

    ret = krb5_checksumsize(context, cktype, cksumsize);
    if (ret)
	return ret;
    
    *cksumtype = (uint32_t)cktype;

    return 0;
}

krb5_error_code
_krb5_pac_sign(krb5_context context,
	       krb5_pac p,
	       time_t authtime,
	       krb5_principal principal,
	       const krb5_keyblock *server_key,
	       const krb5_keyblock *priv_key,
	       krb5_data *data)
{
    krb5_error_code ret;
    krb5_storage *sp = NULL, *spdata = NULL;
    uint32_t end;
    size_t server_size, priv_size;
    uint32_t server_offset = 0, priv_offset = 0;
    uint32_t server_cksumtype = 0, priv_cksumtype = 0;
    int i, num = 0;
    krb5_data logon, d;

    krb5_data_zero(&logon);

    if (p->logon_name == NULL)
	num++;
    if (p->server_checksum == NULL)
	num++;
    if (p->privsvr_checksum == NULL)
	num++;

    if (num) {
	void *ptr;

	ptr = realloc(p->pac, sizeof(*p->pac) + (sizeof(p->pac->buffers[0]) * (p->pac->numbuffers + num - 1)));
	if (ptr == NULL) {
	    krb5_set_error_string(context, "out of memory");
	    return ENOMEM;
	}
	p->pac = ptr;

	if (p->logon_name == NULL) {
	    p->logon_name = &p->pac->buffers[p->pac->numbuffers++];
	    memset(p->logon_name, 0, sizeof(*p->logon_name));
	    p->logon_name->type = PAC_LOGON_NAME;
	}
	if (p->server_checksum == NULL) {
	    p->server_checksum = &p->pac->buffers[p->pac->numbuffers++];
	    memset(p->server_checksum, 0, sizeof(*p->server_checksum));
	    p->server_checksum->type = PAC_SERVER_CHECKSUM;
	}
	if (p->privsvr_checksum == NULL) {
	    p->privsvr_checksum = &p->pac->buffers[p->pac->numbuffers++];
	    memset(p->privsvr_checksum, 0, sizeof(*p->privsvr_checksum));
	    p->privsvr_checksum->type = PAC_PRIVSVR_CHECKSUM;
	}
    }

    /* Calculate LOGON NAME */
    ret = build_logon_name(context, authtime, principal, &logon);
    if (ret)
	goto out;

    /* Set lengths for checksum */
    ret = pac_checksum(context, server_key, &server_cksumtype, &server_size);
    if (ret)
	goto out;
    ret = pac_checksum(context, priv_key, &priv_cksumtype, &priv_size);
    if (ret)
	goto out;

    /* Encode PAC */
    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    spdata = krb5_storage_emem();
    if (spdata == NULL) {
	krb5_storage_free(sp);
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    krb5_storage_set_flags(spdata, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(ret, krb5_store_uint32(sp, p->pac->numbuffers), out);
    CHECK(ret, krb5_store_uint32(sp, p->pac->version), out);

    end = PACTYPE_SIZE + (PAC_INFO_BUFFER_SIZE * p->pac->numbuffers);

    for (i = 0; i < p->pac->numbuffers; i++) {
	uint32_t len;
	size_t sret;
	void *ptr = NULL;

	/* store data */

	if (p->pac->buffers[i].type == PAC_SERVER_CHECKSUM) {
	    len = server_size + 4;
	    server_offset = end + 4;
	    CHECK(ret, krb5_store_uint32(spdata, server_cksumtype), out);
	    CHECK(ret, fill_zeros(context, spdata, server_size), out);
	} else if (p->pac->buffers[i].type == PAC_PRIVSVR_CHECKSUM) {
	    len = priv_size + 4;
	    priv_offset = end + 4;
	    CHECK(ret, krb5_store_uint32(spdata, priv_cksumtype), out);
	    CHECK(ret, fill_zeros(context, spdata, priv_size), out);
	} else if (p->pac->buffers[i].type == PAC_LOGON_NAME) {
	    len = krb5_storage_write(spdata, logon.data, logon.length);
	    if (logon.length != len) {
		ret = EINVAL;
		goto out;
	    }
	} else {
	    len = p->pac->buffers[i].buffersize;
	    ptr = (char *)p->data.data + p->pac->buffers[i].offset_lo;

	    sret = krb5_storage_write(spdata, ptr, len);
	    if (sret != len) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    /* XXX if not aligned, fill_zeros */
	}

	/* write header */
	CHECK(ret, krb5_store_uint32(sp, p->pac->buffers[i].type), out);
	CHECK(ret, krb5_store_uint32(sp, len), out);
	CHECK(ret, krb5_store_uint32(sp, end), out);
	CHECK(ret, krb5_store_uint32(sp, 0), out);

	/* advance data endpointer and align */
	{
	    int32_t e;

	    end += len;
	    e = ((end + PAC_ALIGNMENT - 1) / PAC_ALIGNMENT) * PAC_ALIGNMENT;
	    if (end != e) {
		CHECK(ret, fill_zeros(context, spdata, e - end), out);
	    }
	    end = e;
	}

    }

    /* assert (server_offset != 0 && priv_offset != 0); */

    /* export PAC */
    ret = krb5_storage_to_data(spdata, &d);
    if (ret) {
	krb5_set_error_string(context, "out of memory");
	goto out;
    }
    ret = krb5_storage_write(sp, d.data, d.length);
    if (ret != d.length) {
	krb5_data_free(&d);
	krb5_set_error_string(context, "out of memory");
	ret = ENOMEM;
	goto out;
    }
    krb5_data_free(&d);

    ret = krb5_storage_to_data(sp, &d);
    if (ret) {
	krb5_set_error_string(context, "out of memory");
	goto out;
    }

    /* sign */

    ret = create_checksum(context, server_key,
			  d.data, d.length,
			  (char *)d.data + server_offset, server_size);
    if (ret) {
	krb5_data_free(&d);
	goto out;
    }

    ret = create_checksum(context, priv_key,
			  (char *)d.data + server_offset, server_size,
			  (char *)d.data + priv_offset, priv_size);
    if (ret) {
	krb5_data_free(&d);
	goto out;
    }

    /* done */
    *data = d;

    krb5_data_free(&logon);
    krb5_storage_free(sp);
    krb5_storage_free(spdata);

    return 0;
out:
    krb5_data_free(&logon);
    if (sp)
	krb5_storage_free(sp);
    if (spdata)
	krb5_storage_free(spdata);
    return ret;
}
