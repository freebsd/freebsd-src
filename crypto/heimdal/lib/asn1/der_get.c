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

#include "der_locl.h"

RCSID("$Id: der_get.c,v 1.31 2001/09/28 22:53:24 assar Exp $");

#include <version.h>

/* 
 * All decoding functions take a pointer `p' to first position in
 * which to read, from the left, `len' which means the maximum number
 * of characters we are able to read, `ret' were the value will be
 * returned and `size' where the number of used bytes is stored.
 * Either 0 or an error code is returned.
 */

static int
der_get_unsigned (const unsigned char *p, size_t len,
		  unsigned *ret, size_t *size)
{
    unsigned val = 0;
    size_t oldlen = len;

    while (len--)
	val = val * 256 + *p++;
    *ret = val;
    if(size) *size = oldlen;
    return 0;
}

int
der_get_int (const unsigned char *p, size_t len,
	     int *ret, size_t *size)
{
    int val = 0;
    size_t oldlen = len;

    if (len--)
	val = (signed char)*p++;
    while (len--)
	val = val * 256 + *p++;
    *ret = val;
    if(size) *size = oldlen;
    return 0;
}

int
der_get_length (const unsigned char *p, size_t len,
		size_t *val, size_t *size)
{
    size_t v;

    if (len <= 0)
	return ASN1_OVERRUN;
    --len;
    v = *p++;
    if (v < 128) {
	*val = v;
	if(size) *size = 1;
    } else {
	int e;
	size_t l;
	unsigned tmp;

	if(v == 0x80){
	    *val = ASN1_INDEFINITE;
	    if(size) *size = 1;
	    return 0;
	}
	v &= 0x7F;
	if (len < v)
	    return ASN1_OVERRUN;
	e = der_get_unsigned (p, v, &tmp, &l);
	if(e) return e;
	*val = tmp;
	if(size) *size = l + 1;
    }
    return 0;
}

int
der_get_general_string (const unsigned char *p, size_t len, 
			general_string *str, size_t *size)
{
    char *s;

    s = malloc (len + 1);
    if (s == NULL)
	return ENOMEM;
    memcpy (s, p, len);
    s[len] = '\0';
    *str = s;
    if(size) *size = len;
    return 0;
}

int
der_get_octet_string (const unsigned char *p, size_t len, 
		      octet_string *data, size_t *size)
{
    data->length = len;
    data->data = malloc(len);
    if (data->data == NULL && data->length != 0)
	return ENOMEM;
    memcpy (data->data, p, len);
    if(size) *size = len;
    return 0;
}

int
der_get_oid (const unsigned char *p, size_t len,
	     oid *data, size_t *size)
{
    int n;
    size_t oldlen = len;

    if (len < 1)
	return ASN1_OVERRUN;

    data->components = malloc(len * sizeof(*data->components));
    if (data->components == NULL && len != 0)
	return ENOMEM;
    data->components[0] = (*p) / 40;
    data->components[1] = (*p) % 40;
    --len;
    ++p;
    for (n = 2; len > 0; ++n) {
	unsigned u = 0;

	do {
	    --len;
	    u = u * 128 + (*p++ % 128);
	} while (len > 0 && p[-1] & 0x80);
	data->components[n] = u;
    }
    if (p[-1] & 0x80) {
	free_oid (data);
	return ASN1_OVERRUN;
    }
    data->length = n;
    if (size)
	*size = oldlen;
    return 0;
}

int
der_get_tag (const unsigned char *p, size_t len,
	     Der_class *class, Der_type *type,
	     int *tag, size_t *size)
{
    if (len < 1)
	return ASN1_OVERRUN;
    *class = (Der_class)(((*p) >> 6) & 0x03);
    *type = (Der_type)(((*p) >> 5) & 0x01);
    *tag = (*p) & 0x1F;
    if(size) *size = 1;
    return 0;
}

int
der_match_tag (const unsigned char *p, size_t len,
	       Der_class class, Der_type type,
	       int tag, size_t *size)
{
    size_t l;
    Der_class thisclass;
    Der_type thistype;
    int thistag;
    int e;

    e = der_get_tag (p, len, &thisclass, &thistype, &thistag, &l);
    if (e) return e;
    if (class != thisclass || type != thistype)
	return ASN1_BAD_ID;
    if(tag > thistag)
	return ASN1_MISPLACED_FIELD;
    if(tag < thistag)
	return ASN1_MISSING_FIELD;
    if(size) *size = l;
    return 0;
}

int
der_match_tag_and_length (const unsigned char *p, size_t len,
			  Der_class class, Der_type type, int tag,
			  size_t *length_ret, size_t *size)
{
    size_t l, ret = 0;
    int e;

    e = der_match_tag (p, len, class, type, tag, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length (p, len, length_ret, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

int
decode_integer (const unsigned char *p, size_t len,
		int *num, size_t *size)
{
    size_t ret = 0;
    size_t l, reallen;
    int e;

    e = der_match_tag (p, len, UNIV, PRIM, UT_Integer, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length (p, len, &reallen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_int (p, reallen, num, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

int
decode_unsigned (const unsigned char *p, size_t len,
		 unsigned *num, size_t *size)
{
    size_t ret = 0;
    size_t l, reallen;
    int e;

    e = der_match_tag (p, len, UNIV, PRIM, UT_Integer, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length (p, len, &reallen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_unsigned (p, reallen, num, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

int
decode_enumerated (const unsigned char *p, size_t len,
		   unsigned *num, size_t *size)
{
    size_t ret = 0;
    size_t l, reallen;
    int e;

    e = der_match_tag (p, len, UNIV, PRIM, UT_Enumerated, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length (p, len, &reallen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_int (p, reallen, num, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

int
decode_general_string (const unsigned char *p, size_t len, 
		       general_string *str, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag (p, len, UNIV, PRIM, UT_GeneralString, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length (p, len, &slen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;

    e = der_get_general_string (p, slen, str, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

int
decode_octet_string (const unsigned char *p, size_t len, 
		     octet_string *k, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag (p, len, UNIV, PRIM, UT_OctetString, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length (p, len, &slen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;

    e = der_get_octet_string (p, slen, k, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

int
decode_oid (const unsigned char *p, size_t len, 
	    oid *k, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag (p, len, UNIV, PRIM, UT_OID, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length (p, len, &slen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;

    e = der_get_oid (p, slen, k, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if(size) *size = ret;
    return 0;
}

static void
generalizedtime2time (const char *s, time_t *t)
{
    struct tm tm;

    memset(&tm, 0, sizeof(tm));
    sscanf (s, "%04d%02d%02d%02d%02d%02dZ",
	    &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour,
	    &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    *t = timegm (&tm);
}

int
decode_generalized_time (const unsigned char *p, size_t len,
			 time_t *t, size_t *size)
{
    octet_string k;
    char *times;
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag (p, len, UNIV, PRIM, UT_GeneralizedTime, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length (p, len, &slen, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;
    e = der_get_octet_string (p, slen, &k, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    times = realloc(k.data, k.length + 1);
    if (times == NULL){
	free(k.data);
	return ENOMEM;
    }
    times[k.length] = 0;
    generalizedtime2time (times, t);
    free (times);
    if(size) *size = ret;
    return 0;
}


int
fix_dce(size_t reallen, size_t *len)
{
    if(reallen == ASN1_INDEFINITE)
	return 1;
    if(*len < reallen)
	return -1;
    *len = reallen;
    return 0;
}
