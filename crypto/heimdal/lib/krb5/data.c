/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: data.c,v 1.15 1999/12/02 17:05:09 joda Exp $");

void
krb5_data_zero(krb5_data *p)
{
    p->length = 0;
    p->data   = NULL;
}

void
krb5_data_free(krb5_data *p)
{
    if(p->data != NULL)
	free(p->data);
    p->length = 0;
}

void
krb5_free_data(krb5_context context,
	       krb5_data *p)
{
    krb5_data_free(p);
    free(p);
}

krb5_error_code
krb5_data_alloc(krb5_data *p, int len)
{
    p->data = malloc(len);
    if(len && p->data == NULL)
	return ENOMEM;
    p->length = len;
    return 0;
}

krb5_error_code
krb5_data_realloc(krb5_data *p, int len)
{
    void *tmp;
    tmp = realloc(p->data, len);
    if(len && !tmp)
	return ENOMEM;
    p->data = tmp;
    p->length = len;
    return 0;
}

krb5_error_code
krb5_data_copy(krb5_data *p, const void *data, size_t len)
{
    if (len) {
	if(krb5_data_alloc(p, len))
	    return ENOMEM;
	memmove(p->data, data, len);
    } else
	p->data = NULL;
    p->length = len;
    return 0;
}

krb5_error_code
krb5_copy_data(krb5_context context, 
	       const krb5_data *indata, 
	       krb5_data **outdata)
{
    krb5_error_code ret;
    ALLOC(*outdata, 1);
    if(*outdata == NULL)
	return ENOMEM;
    ret = copy_octet_string(indata, *outdata);
    if(ret)
	free(*outdata);
    return ret;
}
