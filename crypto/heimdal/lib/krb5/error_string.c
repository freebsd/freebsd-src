/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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

RCSID("$Id: error_string.c,v 1.1 2001/05/06 23:07:22 assar Exp $");

#undef __attribute__
#define __attribute__(X)

void
krb5_free_error_string(krb5_context context, char *str)
{
    if (str != context->error_buf)
	free(str);
}

void
krb5_clear_error_string(krb5_context context)
{
    if (context->error_string != NULL
	&& context->error_string != context->error_buf)
	free(context->error_string);
    context->error_string = NULL;
}

krb5_error_code
krb5_set_error_string(krb5_context context, const char *fmt, ...)
    __attribute__((format (printf, 2, 3)))
{
    krb5_error_code ret;
    va_list ap;

    va_start(ap, fmt);
    ret = krb5_vset_error_string (context, fmt, ap);
    va_end(ap);
    return ret;
}

krb5_error_code
krb5_vset_error_string(krb5_context context, const char *fmt, va_list args)
    __attribute__ ((format (printf, 2, 0)))
{
    krb5_clear_error_string(context);
    vasprintf(&context->error_string, fmt, args);
    if(context->error_string == NULL) {
	vsnprintf (context->error_buf, sizeof(context->error_buf), fmt, args);
	context->error_string = context->error_buf;
    }
    return 0;
}

char*
krb5_get_error_string(krb5_context context)
{
    char *ret = context->error_string;
    context->error_string = NULL;
    return ret;
}

krb5_boolean
krb5_have_error_string(krb5_context context)
{
    return context->error_string != NULL;
}
