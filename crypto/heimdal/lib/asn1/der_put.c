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

RCSID("$Id: der_put.c,v 1.27 2001/09/25 23:37:25 assar Exp $");

/*
 * All encoding functions take a pointer `p' to first position in
 * which to write, from the right, `len' which means the maximum
 * number of characters we are able to write.  The function returns
 * the number of characters written in `size' (if non-NULL).
 * The return value is 0 or an error.
 */

static int
der_put_unsigned (unsigned char *p, size_t len, unsigned val, size_t *size)
{
    unsigned char *base = p;

    if (val) {
	while (len > 0 && val) {
	    *p-- = val % 256;
	    val /= 256;
	    --len;
	}
	if (val != 0)
	    return ASN1_OVERFLOW;
	else {
	    *size = base - p;
	    return 0;
	}
    } else if (len < 1)
	return ASN1_OVERFLOW;
    else {
	*p    = 0;
	*size = 1;
	return 0;
    }
}

int
der_put_int (unsigned char *p, size_t len, int val, size_t *size)
{
    unsigned char *base = p;

    if(val >= 0) {
	do {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = val % 256;
	    len--;
	    val /= 256;
	} while(val);
	if(p[1] >= 128) {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0;
	    len--;
	}
    } else {
	val = ~val;
	do {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = ~(val % 256);
	    len--;
	    val /= 256;
	} while(val);
	if(p[1] < 128) {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0xff;
	    len--;
	}
    }
    *size = base - p;
    return 0;
}


int
der_put_length (unsigned char *p, size_t len, size_t val, size_t *size)
{
    if (len < 1)
	return ASN1_OVERFLOW;
    if (val < 128) {
	*p = val;
	*size = 1;
	return 0;
    } else {
	size_t l;
	int e;

	e = der_put_unsigned (p, len - 1, val, &l);
	if (e)
	    return e;
	p -= l;
	*p = 0x80 | l;
	*size = l + 1;
	return 0;
    }
}

int
der_put_general_string (unsigned char *p, size_t len, 
			const general_string *str, size_t *size)
{
    size_t slen = strlen(*str);

    if (len < slen)
	return ASN1_OVERFLOW;
    p -= slen;
    len -= slen;
    memcpy (p+1, *str, slen);
    *size = slen;
    return 0;
}

int
der_put_octet_string (unsigned char *p, size_t len, 
		      const octet_string *data, size_t *size)
{
    if (len < data->length)
	return ASN1_OVERFLOW;
    p -= data->length;
    len -= data->length;
    memcpy (p+1, data->data, data->length);
    *size = data->length;
    return 0;
}

int
der_put_oid (unsigned char *p, size_t len,
	     const oid *data, size_t *size)
{
    unsigned char *base = p;
    int n;

    for (n = data->length - 1; n >= 2; --n) {
	unsigned u = data->components[n];

	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = u % 128;
	u /= 128;
	--len;
	while (u > 0) {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 128 + u % 128;
	    u /= 128;
	    --len;
	}
    }
    if (len < 1)
	return ASN1_OVERFLOW;
    *p-- = 40 * data->components[0] + data->components[1];
    *size = base - p;
    return 0;
}

int
der_put_tag (unsigned char *p, size_t len, Der_class class, Der_type type,
	     int tag, size_t *size)
{
    if (len < 1)
	return ASN1_OVERFLOW;
    *p = (class << 6) | (type << 5) | tag; /* XXX */
    *size = 1;
    return 0;
}

int
der_put_length_and_tag (unsigned char *p, size_t len, size_t len_val,
			Der_class class, Der_type type, int tag, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_length (p, len, len_val, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_tag (p, len, class, type, tag, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_integer (unsigned char *p, size_t len, const int *data, size_t *size)
{
    int num = *data;
    size_t ret = 0;
    size_t l;
    int e;
    
    e = der_put_int (p, len, num, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, l, UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_unsigned (unsigned char *p, size_t len, const unsigned *data,
		 size_t *size)
{
    unsigned num = *data;
    size_t ret = 0;
    size_t l;
    int e;
    
    e = der_put_unsigned (p, len, num, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, l, UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_enumerated (unsigned char *p, size_t len, const unsigned *data,
		   size_t *size)
{
    unsigned num = *data;
    size_t ret = 0;
    size_t l;
    int e;
    
    e = der_put_int (p, len, num, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, l, UNIV, PRIM, UT_Enumerated, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_general_string (unsigned char *p, size_t len, 
		       const general_string *data, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_general_string (p, len, data, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, l, UNIV, PRIM, UT_GeneralString, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_octet_string (unsigned char *p, size_t len, 
		     const octet_string *k, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_octet_string (p, len, k, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, l, UNIV, PRIM, UT_OctetString, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_oid(unsigned char *p, size_t len,
	   const oid *k, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_oid (p, len, k, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, l, UNIV, PRIM, UT_OID, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
time2generalizedtime (time_t t, octet_string *s)
{
     struct tm *tm;

     s->data = malloc(16);
     if (s->data == NULL)
	 return ENOMEM;
     s->length = 15;
     tm = gmtime (&t);
     sprintf (s->data, "%04d%02d%02d%02d%02d%02dZ", tm->tm_year + 1900,
	      tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
	      tm->tm_sec);
     return 0;
}

int
encode_generalized_time (unsigned char *p, size_t len,
			 const time_t *t, size_t *size)
{
    size_t ret = 0;
    size_t l;
    octet_string k;
    int e;

    e = time2generalizedtime (*t, &k);
    if (e)
	return e;
    e = der_put_octet_string (p, len, &k, &l);
    free (k.data);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag (p, len, k.length, UNIV, PRIM, 
				UT_GeneralizedTime, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}
