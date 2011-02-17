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

#include <config.h>

RCSID("$Id: ntlm.c 22370 2007-12-28 16:12:01Z lha $");

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include <krb5.h>
#include <roken.h>

#include "krb5-types.h"
#include "crypto-headers.h"

#include <heimntlm.h>

/*! \mainpage Heimdal NTLM library
 *
 * \section intro Introduction
 *
 * Heimdal libheimntlm library is a implementation of the NTLM
 * protocol, both version 1 and 2. The GSS-API mech that uses this
 * library adds support for transport encryption and integrity
 * checking.
 * 
 * NTLM is a protocol for mutual authentication, its still used in
 * many protocol where Kerberos is not support, one example is
 * EAP/X802.1x mechanism LEAP from Microsoft and Cisco.
 *
 * This is a support library for the core protocol, its used in
 * Heimdal to implement and GSS-API mechanism. There is also support
 * in the KDC to do remote digest authenticiation, this to allow
 * services to authenticate users w/o direct access to the users ntlm
 * hashes (same as Kerberos arcfour enctype hashes).
 *
 * More information about the NTLM protocol can found here
 * http://davenport.sourceforge.net/ntlm.html .
 * 
 * The Heimdal projects web page: http://www.h5l.org/
 */

/** @defgroup ntlm_core Heimdal NTLM library 
 * 
 * The NTLM core functions implement the string2key generation
 * function, message encode and decode function, and the hash function
 * functions.
 */

struct sec_buffer {
    uint16_t length;
    uint16_t allocated;
    uint32_t offset;
};

static const unsigned char ntlmsigature[8] = "NTLMSSP\x00";

/*
 *
 */

#define CHECK(f, e)							\
    do { ret = f ; if (ret != (e)) { ret = EINVAL; goto out; } } while(0)

/**
 * heim_ntlm_free_buf frees the ntlm buffer
 *
 * @param p buffer to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_buf(struct ntlm_buf *p)
{
    if (p->data)
	free(p->data);
    p->data = NULL;
    p->length = 0;
}
    

static int
ascii2ucs2le(const char *string, int up, struct ntlm_buf *buf)
{
    unsigned char *p;
    size_t len, i;

    len = strlen(string);
    if (len / 2 > UINT_MAX)
	return ERANGE;

    buf->length = len * 2;
    buf->data = malloc(buf->length);
    if (buf->data == NULL && len != 0) {
	heim_ntlm_free_buf(buf);
	return ENOMEM;
    }

    p = buf->data;
    for (i = 0; i < len; i++) {
	unsigned char t = (unsigned char)string[i];
	if (t & 0x80) {
	    heim_ntlm_free_buf(buf);
	    return EINVAL;
	}
	if (up)
	    t = toupper(t);
	p[(i * 2) + 0] = t;
	p[(i * 2) + 1] = 0;
    }
    return 0;
}

/*
 *
 */

static krb5_error_code
ret_sec_buffer(krb5_storage *sp, struct sec_buffer *buf)
{
    krb5_error_code ret;
    CHECK(krb5_ret_uint16(sp, &buf->length), 0);
    CHECK(krb5_ret_uint16(sp, &buf->allocated), 0);
    CHECK(krb5_ret_uint32(sp, &buf->offset), 0);
out:
    return ret;
}

static krb5_error_code
store_sec_buffer(krb5_storage *sp, const struct sec_buffer *buf)
{
    krb5_error_code ret;
    CHECK(krb5_store_uint16(sp, buf->length), 0);
    CHECK(krb5_store_uint16(sp, buf->allocated), 0);
    CHECK(krb5_store_uint32(sp, buf->offset), 0);
out:
    return ret;
}

/*
 * Strings are either OEM or UNICODE. The later is encoded as ucs2 on
 * wire, but using utf8 in memory.
 */

static krb5_error_code
len_string(int ucs2, const char *s)
{
    size_t len = strlen(s);
    if (ucs2)
	len *= 2;
    return len;
}

static krb5_error_code
ret_string(krb5_storage *sp, int ucs2, struct sec_buffer *desc, char **s)
{
    krb5_error_code ret;

    *s = malloc(desc->length + 1);
    CHECK(krb5_storage_seek(sp, desc->offset, SEEK_SET), desc->offset);
    CHECK(krb5_storage_read(sp, *s, desc->length), desc->length);
    (*s)[desc->length] = '\0';

    if (ucs2) {
	size_t i;
	for (i = 0; i < desc->length / 2; i++) {
	    (*s)[i] = (*s)[i * 2];
	    if ((*s)[i * 2 + 1]) {
		free(*s);
		*s = NULL;
		return EINVAL;
	    }
	}
	(*s)[i] = '\0';
    }
    ret = 0;
out:
    return ret;

    return 0;
}

static krb5_error_code
put_string(krb5_storage *sp, int ucs2, const char *s)
{
    krb5_error_code ret;
    struct ntlm_buf buf;

    if (ucs2) {
	ret = ascii2ucs2le(s, 0, &buf);
	if (ret)
	    return ret;
    } else {
	buf.data = rk_UNCONST(s);
	buf.length = strlen(s);
    }

    CHECK(krb5_storage_write(sp, buf.data, buf.length), buf.length);
    if (ucs2)
	heim_ntlm_free_buf(&buf);
    ret = 0;
out:
    return ret;
}

/*
 *
 */

static krb5_error_code
ret_buf(krb5_storage *sp, struct sec_buffer *desc, struct ntlm_buf *buf)
{
    krb5_error_code ret;

    buf->data = malloc(desc->length);
    buf->length = desc->length;
    CHECK(krb5_storage_seek(sp, desc->offset, SEEK_SET), desc->offset);
    CHECK(krb5_storage_read(sp, buf->data, buf->length), buf->length);
    ret = 0;
out:
    return ret;
}

static krb5_error_code
put_buf(krb5_storage *sp, const struct ntlm_buf *buf)
{
    krb5_error_code ret;
    CHECK(krb5_storage_write(sp, buf->data, buf->length), buf->length);
    ret = 0;
out:
    return ret;
}

/**
 * Frees the ntlm_targetinfo message
 *
 * @param ti targetinfo to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_targetinfo(struct ntlm_targetinfo *ti)
{
    free(ti->servername);
    free(ti->domainname);
    free(ti->dnsdomainname);
    free(ti->dnsservername);
    memset(ti, 0, sizeof(*ti));
}

static int
encode_ti_blob(krb5_storage *out, uint16_t type, int ucs2, char *s)
{
    krb5_error_code ret;
    CHECK(krb5_store_uint16(out, type), 0);
    CHECK(krb5_store_uint16(out, len_string(ucs2, s)), 0);
    CHECK(put_string(out, ucs2, s), 0);
out:
    return ret;
}

/**
 * Encodes a ntlm_targetinfo message.
 *
 * @param ti the ntlm_targetinfo message to encode.
 * @param ucs2 if the strings should be encoded with ucs2 (selected by flag in message).
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_targetinfo(const struct ntlm_targetinfo *ti,
			    int ucs2, 
			    struct ntlm_buf *data)
{
    krb5_error_code ret;
    krb5_storage *out;

    data->data = NULL;
    data->length = 0;

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    if (ti->servername)
	CHECK(encode_ti_blob(out, 1, ucs2, ti->servername), 0);
    if (ti->domainname)
	CHECK(encode_ti_blob(out, 2, ucs2, ti->domainname), 0);
    if (ti->dnsservername)
	CHECK(encode_ti_blob(out, 3, ucs2, ti->dnsservername), 0);
    if (ti->dnsdomainname)
	CHECK(encode_ti_blob(out, 4, ucs2, ti->dnsdomainname), 0);

    /* end tag */
    CHECK(krb5_store_int16(out, 0), 0);
    CHECK(krb5_store_int16(out, 0), 0);

    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }
out:
    krb5_storage_free(out);
    return ret;
}

/**
 * Decodes an NTLM targetinfo message
 *
 * @param data input data buffer with the encode NTLM targetinfo message
 * @param ucs2 if the strings should be encoded with ucs2 (selected by flag in message).
 * @param ti the decoded target info, should be freed with heim_ntlm_free_targetinfo().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_decode_targetinfo(const struct ntlm_buf *data,
			    int ucs2,
			    struct ntlm_targetinfo *ti)
{
    memset(ti, 0, sizeof(*ti));
    return 0;
}

/**
 * Frees the ntlm_type1 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type1(struct ntlm_type1 *data)
{
    if (data->domain)
	free(data->domain);
    if (data->hostname)
	free(data->hostname);
    memset(data, 0, sizeof(*data));
}

int
heim_ntlm_decode_type1(const struct ntlm_buf *buf, struct ntlm_type1 *data)
{
    krb5_error_code ret;
    unsigned char sig[8];
    uint32_t type;
    struct sec_buffer domain, hostname;
    krb5_storage *in;
    
    memset(data, 0, sizeof(*data));

    in = krb5_storage_from_readonly_mem(buf->data, buf->length);
    if (in == NULL) {
	ret = EINVAL;
	goto out;
    }
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(krb5_storage_read(in, sig, sizeof(sig)), sizeof(sig));
    CHECK(memcmp(ntlmsigature, sig, sizeof(ntlmsigature)), 0);
    CHECK(krb5_ret_uint32(in, &type), 0);
    CHECK(type, 1);
    CHECK(krb5_ret_uint32(in, &data->flags), 0);
    if (data->flags & NTLM_SUPPLIED_DOMAIN)
	CHECK(ret_sec_buffer(in, &domain), 0);
    if (data->flags & NTLM_SUPPLIED_WORKSTAION)
	CHECK(ret_sec_buffer(in, &hostname), 0);
#if 0
    if (domain.offset > 32) {
	CHECK(krb5_ret_uint32(in, &data->os[0]), 0);
	CHECK(krb5_ret_uint32(in, &data->os[1]), 0);
    }
#endif
    if (data->flags & NTLM_SUPPLIED_DOMAIN)
	CHECK(ret_string(in, 0, &domain, &data->domain), 0);
    if (data->flags & NTLM_SUPPLIED_WORKSTAION)
	CHECK(ret_string(in, 0, &hostname, &data->hostname), 0);

out:
    krb5_storage_free(in);
    if (ret)
	heim_ntlm_free_type1(data);

    return ret;
}

/**
 * Encodes an ntlm_type1 message.
 *
 * @param type1 the ntlm_type1 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type1(const struct ntlm_type1 *type1, struct ntlm_buf *data)
{
    krb5_error_code ret;
    struct sec_buffer domain, hostname;
    krb5_storage *out;
    uint32_t base, flags;
    
    flags = type1->flags;
    base = 16;

    if (type1->domain) {
	base += 8;
	flags |= NTLM_SUPPLIED_DOMAIN;
    }
    if (type1->hostname) {
	base += 8;
	flags |= NTLM_SUPPLIED_WORKSTAION;
    }
    if (type1->os[0])
	base += 8;

    if (type1->domain) {
	domain.offset = base;
	domain.length = len_string(0, type1->domain);
	domain.allocated = domain.length;
    }
    if (type1->hostname) {
	hostname.offset = domain.allocated + domain.offset;
	hostname.length = len_string(0, type1->hostname);
	hostname.allocated = hostname.length;
    }

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);
    CHECK(krb5_storage_write(out, ntlmsigature, sizeof(ntlmsigature)), 
	  sizeof(ntlmsigature));
    CHECK(krb5_store_uint32(out, 1), 0);
    CHECK(krb5_store_uint32(out, flags), 0);
    
    if (type1->domain)
	CHECK(store_sec_buffer(out, &domain), 0);
    if (type1->hostname)
	CHECK(store_sec_buffer(out, &hostname), 0);
    if (type1->os[0]) {
	CHECK(krb5_store_uint32(out, type1->os[0]), 0);
	CHECK(krb5_store_uint32(out, type1->os[1]), 0);
    }
    if (type1->domain)
	CHECK(put_string(out, 0, type1->domain), 0);
    if (type1->hostname)
	CHECK(put_string(out, 0, type1->hostname), 0);

    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }
out:
    krb5_storage_free(out);

    return ret;
}

/**
 * Frees the ntlm_type2 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type2(struct ntlm_type2 *data)
{
    if (data->targetname)
	free(data->targetname);
    heim_ntlm_free_buf(&data->targetinfo);
    memset(data, 0, sizeof(*data));
}

int
heim_ntlm_decode_type2(const struct ntlm_buf *buf, struct ntlm_type2 *type2)
{
    krb5_error_code ret;
    unsigned char sig[8];
    uint32_t type, ctx[2];
    struct sec_buffer targetname, targetinfo;
    krb5_storage *in;
    int ucs2 = 0;
    
    memset(type2, 0, sizeof(*type2));

    in = krb5_storage_from_readonly_mem(buf->data, buf->length);
    if (in == NULL) {
	ret = EINVAL;
	goto out;
    }
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(krb5_storage_read(in, sig, sizeof(sig)), sizeof(sig));
    CHECK(memcmp(ntlmsigature, sig, sizeof(ntlmsigature)), 0);
    CHECK(krb5_ret_uint32(in, &type), 0);
    CHECK(type, 2);

    CHECK(ret_sec_buffer(in, &targetname), 0);
    CHECK(krb5_ret_uint32(in, &type2->flags), 0);
    if (type2->flags & NTLM_NEG_UNICODE)
	ucs2 = 1;
    CHECK(krb5_storage_read(in, type2->challange, sizeof(type2->challange)),
	  sizeof(type2->challange));
    CHECK(krb5_ret_uint32(in, &ctx[0]), 0); /* context */
    CHECK(krb5_ret_uint32(in, &ctx[1]), 0);
    CHECK(ret_sec_buffer(in, &targetinfo), 0);
    /* os version */
#if 0
    CHECK(krb5_ret_uint32(in, &type2->os[0]), 0);
    CHECK(krb5_ret_uint32(in, &type2->os[1]), 0);
#endif

    CHECK(ret_string(in, ucs2, &targetname, &type2->targetname), 0);
    CHECK(ret_buf(in, &targetinfo, &type2->targetinfo), 0);
    ret = 0;

out:
    krb5_storage_free(in);
    if (ret)
	heim_ntlm_free_type2(type2);

    return ret;
}

/**
 * Encodes an ntlm_type2 message.
 *
 * @param type2 the ntlm_type2 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type2(const struct ntlm_type2 *type2, struct ntlm_buf *data)
{
    struct sec_buffer targetname, targetinfo;
    krb5_error_code ret;
    krb5_storage *out = NULL;
    uint32_t base;
    int ucs2 = 0;

    if (type2->os[0])
	base = 56;
    else
	base = 48;

    if (type2->flags & NTLM_NEG_UNICODE)
	ucs2 = 1;

    targetname.offset = base;
    targetname.length = len_string(ucs2, type2->targetname);
    targetname.allocated = targetname.length;

    targetinfo.offset = targetname.allocated + targetname.offset;
    targetinfo.length = type2->targetinfo.length;
    targetinfo.allocated = type2->targetinfo.length;

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);
    CHECK(krb5_storage_write(out, ntlmsigature, sizeof(ntlmsigature)), 
	  sizeof(ntlmsigature));
    CHECK(krb5_store_uint32(out, 2), 0);
    CHECK(store_sec_buffer(out, &targetname), 0);
    CHECK(krb5_store_uint32(out, type2->flags), 0);
    CHECK(krb5_storage_write(out, type2->challange, sizeof(type2->challange)),
	  sizeof(type2->challange));
    CHECK(krb5_store_uint32(out, 0), 0); /* context */
    CHECK(krb5_store_uint32(out, 0), 0);
    CHECK(store_sec_buffer(out, &targetinfo), 0);
    /* os version */
    if (type2->os[0]) {
	CHECK(krb5_store_uint32(out, type2->os[0]), 0);
	CHECK(krb5_store_uint32(out, type2->os[1]), 0);
    }
    CHECK(put_string(out, ucs2, type2->targetname), 0);
    CHECK(krb5_storage_write(out, type2->targetinfo.data, 
			     type2->targetinfo.length),
	  type2->targetinfo.length);
    
    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }

out:
    krb5_storage_free(out);

    return ret;
}

/**
 * Frees the ntlm_type3 message
 *
 * @param data message to be freed
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_free_type3(struct ntlm_type3 *data)
{
    heim_ntlm_free_buf(&data->lm);
    heim_ntlm_free_buf(&data->ntlm);
    if (data->targetname)
	free(data->targetname);
    if (data->username)
	free(data->username);
    if (data->ws)
	free(data->ws);
    heim_ntlm_free_buf(&data->sessionkey);
    memset(data, 0, sizeof(*data));
}

/*
 *
 */

int
heim_ntlm_decode_type3(const struct ntlm_buf *buf,
		       int ucs2,
		       struct ntlm_type3 *type3)
{
    krb5_error_code ret;
    unsigned char sig[8];
    uint32_t type;
    krb5_storage *in;
    struct sec_buffer lm, ntlm, target, username, sessionkey, ws;

    memset(type3, 0, sizeof(*type3));
    memset(&sessionkey, 0, sizeof(sessionkey));

    in = krb5_storage_from_readonly_mem(buf->data, buf->length);
    if (in == NULL) {
	ret = EINVAL;
	goto out;
    }
    krb5_storage_set_byteorder(in, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(krb5_storage_read(in, sig, sizeof(sig)), sizeof(sig));
    CHECK(memcmp(ntlmsigature, sig, sizeof(ntlmsigature)), 0);
    CHECK(krb5_ret_uint32(in, &type), 0);
    CHECK(type, 3);
    CHECK(ret_sec_buffer(in, &lm), 0);
    CHECK(ret_sec_buffer(in, &ntlm), 0);
    CHECK(ret_sec_buffer(in, &target), 0);
    CHECK(ret_sec_buffer(in, &username), 0);
    CHECK(ret_sec_buffer(in, &ws), 0);
    if (lm.offset >= 60) {
	CHECK(ret_sec_buffer(in, &sessionkey), 0);
    }
    if (lm.offset >= 64) {
	CHECK(krb5_ret_uint32(in, &type3->flags), 0);
    }
    if (lm.offset >= 72) {
	CHECK(krb5_ret_uint32(in, &type3->os[0]), 0);
	CHECK(krb5_ret_uint32(in, &type3->os[1]), 0);
    }
    CHECK(ret_buf(in, &lm, &type3->lm), 0);
    CHECK(ret_buf(in, &ntlm, &type3->ntlm), 0);
    CHECK(ret_string(in, ucs2, &target, &type3->targetname), 0);
    CHECK(ret_string(in, ucs2, &username, &type3->username), 0);
    CHECK(ret_string(in, ucs2, &ws, &type3->ws), 0);
    if (sessionkey.offset)
	CHECK(ret_buf(in, &sessionkey, &type3->sessionkey), 0);

out:
    krb5_storage_free(in);
    if (ret)
	heim_ntlm_free_type3(type3);

    return ret;
}

/**
 * Encodes an ntlm_type3 message.
 *
 * @param type3 the ntlm_type3 message to encode.
 * @param data is the return buffer with the encoded message, should be
 * freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_encode_type3(const struct ntlm_type3 *type3, struct ntlm_buf *data)
{
    struct sec_buffer lm, ntlm, target, username, sessionkey, ws;
    krb5_error_code ret;
    krb5_storage *out = NULL;
    uint32_t base;
    int ucs2 = 0;

    memset(&lm, 0, sizeof(lm));
    memset(&ntlm, 0, sizeof(ntlm));
    memset(&target, 0, sizeof(target));
    memset(&username, 0, sizeof(username));
    memset(&ws, 0, sizeof(ws));
    memset(&sessionkey, 0, sizeof(sessionkey));

    base = 52;
    if (type3->sessionkey.length) {
	base += 8; /* sessionkey sec buf */
	base += 4; /* flags */
    }
    if (type3->os[0]) {
	base += 8;
    }

    if (type3->flags & NTLM_NEG_UNICODE)
	ucs2 = 1;

    lm.offset = base;
    lm.length = type3->lm.length;
    lm.allocated = type3->lm.length;

    ntlm.offset = lm.offset + lm.allocated;
    ntlm.length = type3->ntlm.length;
    ntlm.allocated = ntlm.length;

    target.offset = ntlm.offset + ntlm.allocated;
    target.length = len_string(ucs2, type3->targetname);
    target.allocated = target.length;

    username.offset = target.offset + target.allocated;
    username.length = len_string(ucs2, type3->username);
    username.allocated = username.length;

    ws.offset = username.offset + username.allocated;
    ws.length = len_string(ucs2, type3->ws);
    ws.allocated = ws.length;

    sessionkey.offset = ws.offset + ws.allocated;
    sessionkey.length = type3->sessionkey.length;
    sessionkey.allocated = type3->sessionkey.length;

    out = krb5_storage_emem();
    if (out == NULL)
	return ENOMEM;

    krb5_storage_set_byteorder(out, KRB5_STORAGE_BYTEORDER_LE);
    CHECK(krb5_storage_write(out, ntlmsigature, sizeof(ntlmsigature)), 
	  sizeof(ntlmsigature));
    CHECK(krb5_store_uint32(out, 3), 0);

    CHECK(store_sec_buffer(out, &lm), 0);
    CHECK(store_sec_buffer(out, &ntlm), 0);
    CHECK(store_sec_buffer(out, &target), 0);
    CHECK(store_sec_buffer(out, &username), 0);
    CHECK(store_sec_buffer(out, &ws), 0);
    /* optional */
    if (type3->sessionkey.length) {
	CHECK(store_sec_buffer(out, &sessionkey), 0);
	CHECK(krb5_store_uint32(out, type3->flags), 0);
    }
#if 0
    CHECK(krb5_store_uint32(out, 0), 0); /* os0 */
    CHECK(krb5_store_uint32(out, 0), 0); /* os1 */
#endif

    CHECK(put_buf(out, &type3->lm), 0);
    CHECK(put_buf(out, &type3->ntlm), 0);
    CHECK(put_string(out, ucs2, type3->targetname), 0);
    CHECK(put_string(out, ucs2, type3->username), 0);
    CHECK(put_string(out, ucs2, type3->ws), 0);
    CHECK(put_buf(out, &type3->sessionkey), 0);
    
    {
	krb5_data d;
	ret = krb5_storage_to_data(out, &d);
	data->data = d.data;
	data->length = d.length;
    }

out:
    krb5_storage_free(out);

    return ret;
}


/*
 *
 */

static void
splitandenc(unsigned char *hash, 
	    unsigned char *challange,
	    unsigned char *answer)
{
    DES_cblock key;
    DES_key_schedule sched;

    ((unsigned char*)key)[0] =  hash[0];
    ((unsigned char*)key)[1] = (hash[0] << 7) | (hash[1] >> 1);
    ((unsigned char*)key)[2] = (hash[1] << 6) | (hash[2] >> 2);
    ((unsigned char*)key)[3] = (hash[2] << 5) | (hash[3] >> 3);
    ((unsigned char*)key)[4] = (hash[3] << 4) | (hash[4] >> 4);
    ((unsigned char*)key)[5] = (hash[4] << 3) | (hash[5] >> 5);
    ((unsigned char*)key)[6] = (hash[5] << 2) | (hash[6] >> 6);
    ((unsigned char*)key)[7] = (hash[6] << 1);

    DES_set_odd_parity(&key);
    DES_set_key(&key, &sched);
    DES_ecb_encrypt((DES_cblock *)challange, (DES_cblock *)answer, &sched, 1);
    memset(&sched, 0, sizeof(sched));
    memset(key, 0, sizeof(key));
}

/**
 * Calculate the NTLM key, the password is assumed to be in UTF8.
 *
 * @param password password to calcute the key for.
 * @param key calcuted key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_nt_key(const char *password, struct ntlm_buf *key)
{
    struct ntlm_buf buf;
    MD4_CTX ctx;
    int ret;

    key->data = malloc(MD5_DIGEST_LENGTH);
    if (key->data == NULL)
	return ENOMEM;
    key->length = MD5_DIGEST_LENGTH;

    ret = ascii2ucs2le(password, 0, &buf);
    if (ret) {
	heim_ntlm_free_buf(key);
	return ret;
    }
    MD4_Init(&ctx);
    MD4_Update(&ctx, buf.data, buf.length);
    MD4_Final(key->data, &ctx);
    heim_ntlm_free_buf(&buf);
    return 0;
}

/**
 * Calculate NTLMv1 response hash
 *
 * @param key the ntlm v1 key
 * @param len length of key
 * @param challange sent by the server
 * @param answer calculated answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm1(void *key, size_t len,
			  unsigned char challange[8],
			  struct ntlm_buf *answer)
{
    unsigned char res[21];

    if (len != MD4_DIGEST_LENGTH)
	return EINVAL;

    memcpy(res, key, len);
    memset(&res[MD4_DIGEST_LENGTH], 0, sizeof(res) - MD4_DIGEST_LENGTH);

    answer->data = malloc(24);
    if (answer->data == NULL)
	return ENOMEM;
    answer->length = 24;

    splitandenc(&res[0],  challange, ((unsigned char *)answer->data) + 0);
    splitandenc(&res[7],  challange, ((unsigned char *)answer->data) + 8);
    splitandenc(&res[14], challange, ((unsigned char *)answer->data) + 16);

    return 0;
}

/**
 * Generates an NTLMv1 session random with assosited session master key.
 *
 * @param key the ntlm v1 key
 * @param len length of key
 * @param session generated session nonce, should be freed with heim_ntlm_free_buf().
 * @param master calculated session master key, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_build_ntlm1_master(void *key, size_t len,
			     struct ntlm_buf *session,
			     struct ntlm_buf *master)
{
    RC4_KEY rc4;

    memset(master, 0, sizeof(*master));
    memset(session, 0, sizeof(*session));

    if (len != MD4_DIGEST_LENGTH)
	return EINVAL;
    
    session->length = MD4_DIGEST_LENGTH;
    session->data = malloc(session->length);
    if (session->data == NULL) {
	session->length = 0;
	return EINVAL;
    }    
    master->length = MD4_DIGEST_LENGTH;
    master->data = malloc(master->length);
    if (master->data == NULL) {
	heim_ntlm_free_buf(master);
	heim_ntlm_free_buf(session);
	return EINVAL;
    }
    
    {
	unsigned char sessionkey[MD4_DIGEST_LENGTH];
	MD4_CTX ctx;
    
	MD4_Init(&ctx);
	MD4_Update(&ctx, key, len);
	MD4_Final(sessionkey, &ctx);
	
	RC4_set_key(&rc4, sizeof(sessionkey), sessionkey);
    }
    
    if (RAND_bytes(session->data, session->length) != 1) {
	heim_ntlm_free_buf(master);
	heim_ntlm_free_buf(session);
	return EINVAL;
    }
    
    RC4(&rc4, master->length, session->data, master->data);
    memset(&rc4, 0, sizeof(rc4));
    
    return 0;
}

/**
 * Generates an NTLMv2 session key.
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param ntlmv2 the ntlmv2 session key
 *
 * @ingroup ntlm_core
 */

void
heim_ntlm_ntlmv2_key(const void *key, size_t len,
		     const char *username,
		     const char *target,
		     unsigned char ntlmv2[16])
{
    unsigned int hmaclen;
    HMAC_CTX c;

    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, key, len, EVP_md5(), NULL);
    {
	struct ntlm_buf buf;
	/* uppercase username and turn it inte ucs2-le */
	ascii2ucs2le(username, 1, &buf);
	HMAC_Update(&c, buf.data, buf.length);
	free(buf.data);
	/* uppercase target and turn into ucs2-le */
	ascii2ucs2le(target, 1, &buf);
	HMAC_Update(&c, buf.data, buf.length);
	free(buf.data);
    }
    HMAC_Final(&c, ntlmv2, &hmaclen);
    HMAC_CTX_cleanup(&c);

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

static time_t
nt2unixtime(uint64_t t)
{
    t = ((t - (uint64_t)NTTIME_EPOCH) / (uint64_t)10000000);
    if (t > (((time_t)(~(uint64_t)0)) >> 1))
	return 0;
    return (time_t)t;
}


/**
 * Calculate NTLMv2 response
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param serverchallange challange as sent by the server in the type2 message.
 * @param infotarget infotarget as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm2(const void *key, size_t len,
			  const char *username,
			  const char *target,
			  const unsigned char serverchallange[8],
			  const struct ntlm_buf *infotarget,
			  unsigned char ntlmv2[16],
			  struct ntlm_buf *answer)
{
    krb5_error_code ret;
    krb5_data data;
    unsigned int hmaclen;
    unsigned char ntlmv2answer[16];
    krb5_storage *sp;
    unsigned char clientchallange[8];
    HMAC_CTX c;
    uint64_t t;
    
    t = unix2nttime(time(NULL));

    if (RAND_bytes(clientchallange, sizeof(clientchallange)) != 1)
	return EINVAL;
    
    /* calculate ntlmv2 key */

    heim_ntlm_ntlmv2_key(key, len, username, target, ntlmv2);

    /* calculate and build ntlmv2 answer */

    sp = krb5_storage_emem();
    if (sp == NULL)
	return ENOMEM;
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(krb5_store_uint32(sp, 0x00000101), 0);
    CHECK(krb5_store_uint32(sp, 0), 0);
    /* timestamp le 64 bit ts */
    CHECK(krb5_store_uint32(sp, t & 0xffffffff), 0);
    CHECK(krb5_store_uint32(sp, t >> 32), 0);

    CHECK(krb5_storage_write(sp, clientchallange, 8), 8);

    CHECK(krb5_store_uint32(sp, 0), 0);  /* unknown but zero will work */
    CHECK(krb5_storage_write(sp, infotarget->data, infotarget->length), 
	  infotarget->length);
    CHECK(krb5_store_uint32(sp, 0), 0); /* unknown but zero will work */
    
    CHECK(krb5_storage_to_data(sp, &data), 0);
    krb5_storage_free(sp);
    sp = NULL;

    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, ntlmv2, 16, EVP_md5(), NULL);
    HMAC_Update(&c, serverchallange, 8);
    HMAC_Update(&c, data.data, data.length);
    HMAC_Final(&c, ntlmv2answer, &hmaclen);
    HMAC_CTX_cleanup(&c);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_data_free(&data);
	return ENOMEM;
    }

    CHECK(krb5_storage_write(sp, ntlmv2answer, 16), 16);
    CHECK(krb5_storage_write(sp, data.data, data.length), data.length);
    krb5_data_free(&data);
    
    CHECK(krb5_storage_to_data(sp, &data), 0);
    krb5_storage_free(sp);
    sp = NULL;

    answer->data = data.data;
    answer->length = data.length;

    return 0;
out:
    if (sp)
	krb5_storage_free(sp);
    return ret;
}

static const int authtimediff = 3600 * 2; /* 2 hours */

/**
 * Verify NTLMv2 response.
 *
 * @param key the ntlm key
 * @param len length of key
 * @param username name of the user, as sent in the message, assumed to be in UTF8.
 * @param target the name of the target, assumed to be in UTF8.
 * @param now the time now (0 if the library should pick it up itself)
 * @param serverchallange challange as sent by the server in the type2 message.
 * @param answer ntlm response answer, should be freed with heim_ntlm_free_buf().
 * @param infotarget infotarget as sent by the server in the type2 message.
 * @param ntlmv2 calculated session key
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_verify_ntlm2(const void *key, size_t len,
		       const char *username,
		       const char *target,
		       time_t now,
		       const unsigned char serverchallange[8],
		       const struct ntlm_buf *answer,
		       struct ntlm_buf *infotarget,
		       unsigned char ntlmv2[16])
{
    krb5_error_code ret;
    unsigned int hmaclen;
    unsigned char clientanswer[16];
    unsigned char clientnonce[8];
    unsigned char serveranswer[16];
    krb5_storage *sp;
    HMAC_CTX c;
    uint64_t t;
    time_t authtime;
    uint32_t temp;

    infotarget->length = 0;    
    infotarget->data = NULL;    

    if (answer->length < 16)
	return EINVAL;

    if (now == 0)
	now = time(NULL);

    /* calculate ntlmv2 key */

    heim_ntlm_ntlmv2_key(key, len, username, target, ntlmv2);

    /* calculate and build ntlmv2 answer */

    sp = krb5_storage_from_readonly_mem(answer->data, answer->length);
    if (sp == NULL)
	return ENOMEM;
    krb5_storage_set_flags(sp, KRB5_STORAGE_BYTEORDER_LE);

    CHECK(krb5_storage_read(sp, clientanswer, 16), 16);

    CHECK(krb5_ret_uint32(sp, &temp), 0);
    CHECK(temp, 0x00000101);
    CHECK(krb5_ret_uint32(sp, &temp), 0);
    CHECK(temp, 0);
    /* timestamp le 64 bit ts */
    CHECK(krb5_ret_uint32(sp, &temp), 0);
    t = temp;
    CHECK(krb5_ret_uint32(sp, &temp), 0);
    t |= ((uint64_t)temp)<< 32;

    authtime = nt2unixtime(t);

    if (abs((int)(authtime - now)) > authtimediff) {
	ret = EINVAL;
	goto out;
    }

    /* client challange */
    CHECK(krb5_storage_read(sp, clientnonce, 8), 8);

    CHECK(krb5_ret_uint32(sp, &temp), 0); /* unknown */

    /* should really unparse the infotarget, but lets pick up everything */
    infotarget->length = answer->length - krb5_storage_seek(sp, 0, SEEK_CUR);
    infotarget->data = malloc(infotarget->length);
    if (infotarget->data == NULL) {
	ret = ENOMEM;
	goto out;
    }
    CHECK(krb5_storage_read(sp, infotarget->data, infotarget->length), 
	  infotarget->length);
    /* XXX remove the unknown ?? */
    krb5_storage_free(sp);
    sp = NULL;

    HMAC_CTX_init(&c);
    HMAC_Init_ex(&c, ntlmv2, 16, EVP_md5(), NULL);
    HMAC_Update(&c, serverchallange, 8);
    HMAC_Update(&c, ((unsigned char *)answer->data) + 16, answer->length - 16);
    HMAC_Final(&c, serveranswer, &hmaclen);
    HMAC_CTX_cleanup(&c);

    if (memcmp(serveranswer, clientanswer, 16) != 0) {
	heim_ntlm_free_buf(infotarget);
	return EINVAL;
    }

    return 0;
out:
    heim_ntlm_free_buf(infotarget);
    if (sp)
	krb5_storage_free(sp);
    return ret;
}


/*
 * Calculate the NTLM2 Session Response
 *
 * @param clnt_nonce client nonce
 * @param svr_chal server challage
 * @param ntlm2_hash ntlm hash
 * @param lm The LM response, should be freed with heim_ntlm_free_buf().
 * @param ntlm The NTLM response, should be freed with heim_ntlm_free_buf().
 *
 * @return In case of success 0 is return, an errors, a errno in what
 * went wrong.
 *
 * @ingroup ntlm_core
 */

int
heim_ntlm_calculate_ntlm2_sess(const unsigned char clnt_nonce[8],
			       const unsigned char svr_chal[8],
			       const unsigned char ntlm_hash[16],
			       struct ntlm_buf *lm,
			       struct ntlm_buf *ntlm)
{
    unsigned char ntlm2_sess_hash[MD5_DIGEST_LENGTH];
    unsigned char res[21], *resp;
    MD5_CTX md5;

    lm->data = malloc(24);
    if (lm->data == NULL)
	return ENOMEM;
    lm->length = 24;

    ntlm->data = malloc(24);
    if (ntlm->data == NULL) {
	free(lm->data);
	lm->data = NULL;
	return ENOMEM;
    }
    ntlm->length = 24;

    /* first setup the lm resp */
    memset(lm->data, 0, 24);
    memcpy(lm->data, clnt_nonce, 8);

    MD5_Init(&md5);
    MD5_Update(&md5, svr_chal, 8); /* session nonce part 1 */
    MD5_Update(&md5, clnt_nonce, 8); /* session nonce part 2 */
    MD5_Final(ntlm2_sess_hash, &md5); /* will only use first 8 bytes */

    memset(res, 0, sizeof(res));
    memcpy(res, ntlm_hash, 16);

    resp = ntlm->data;
    splitandenc(&res[0], ntlm2_sess_hash, resp + 0);
    splitandenc(&res[7], ntlm2_sess_hash, resp + 8);
    splitandenc(&res[14], ntlm2_sess_hash, resp + 16);

    return 0;
}
