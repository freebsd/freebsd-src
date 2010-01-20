/*
 * Copyright (c) 2003 - 2005 Kungliga Tekniska Högskolan
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
#include "heim_asn1.h"

RCSID("$Id: extra.c 16672 2006-01-31 09:44:54Z lha $");

int
encode_heim_any(unsigned char *p, size_t len, 
		const heim_any *data, size_t *size)
{
    if (data->length > len)
	return ASN1_OVERFLOW;
    p -= data->length;
    len -= data->length;
    memcpy (p+1, data->data, data->length);
    *size = data->length;
    return 0;
}

int
decode_heim_any(const unsigned char *p, size_t len, 
		heim_any *data, size_t *size)
{
    size_t len_len, length, l;
    Der_class thisclass;
    Der_type thistype;
    unsigned int thistag;
    int e;

    memset(data, 0, sizeof(*data));

    e = der_get_tag (p, len, &thisclass, &thistype, &thistag, &l);
    if (e) return e;
    if (l > len)
	return ASN1_OVERFLOW;
    e = der_get_length(p + l, len - l, &length, &len_len);
    if (e) return e;
    if (length + len_len + l > len)
	return ASN1_OVERFLOW;

    data->data = malloc(length + len_len + l);
    if (data->data == NULL)
	return ENOMEM;
    data->length = length + len_len + l;
    memcpy(data->data, p, length + len_len + l);

    if (size)
	*size = length + len_len + l;

    return 0;
}

void
free_heim_any(heim_any *data)
{
    free(data->data);
    data->data = NULL;
}

size_t
length_heim_any(const heim_any *data)
{
    return data->length;
}

int
copy_heim_any(const heim_any *from, heim_any *to)
{
    to->data = malloc(from->length);
    if (to->data == NULL && from->length != 0)
	return ENOMEM;
    memcpy(to->data, from->data, from->length);
    to->length = from->length;
    return 0;
}

int
encode_heim_any_set(unsigned char *p, size_t len, 
		    const heim_any_set *data, size_t *size)
{
    return encode_heim_any(p, len, data, size);
}


int
decode_heim_any_set(const unsigned char *p, size_t len, 
		heim_any_set *data, size_t *size)
{
    memset(data, 0, sizeof(*data));
    data->data = malloc(len);
    if (data->data == NULL && len != 0)
	return ENOMEM;
    data->length = len;
    memcpy(data->data, p, len);
    if (size) *size = len;
    return 0;
}

void
free_heim_any_set(heim_any_set *data)
{
    free_heim_any(data);
}

size_t
length_heim_any_set(const heim_any *data)
{
    return length_heim_any(data);
}

int
copy_heim_any_set(const heim_any_set *from, heim_any_set *to)
{
    return copy_heim_any(from, to);
}

int
heim_any_cmp(const heim_any_set *p, const heim_any_set *q)
{
    if (p->length != q->length)
	return p->length - q->length;
    return memcmp(p->data, q->data, p->length);
}
