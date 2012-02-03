/*
 * Copyright (c) 2001, 2003, 2005 - 2006 Kungliga Tekniska Högskolan
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

RCSID("$Id: error_string.c 22142 2007-12-04 16:56:02Z lha $");

#undef __attribute__
#define __attribute__(X)

void KRB5_LIB_FUNCTION
krb5_free_error_string(krb5_context context, char *str)
{
    HEIMDAL_MUTEX_lock(context->mutex);
    if (str != context->error_buf)
	free(str);
    HEIMDAL_MUTEX_unlock(context->mutex);
}

void KRB5_LIB_FUNCTION
krb5_clear_error_string(krb5_context context)
{
    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_string != NULL
	&& context->error_string != context->error_buf)
	free(context->error_string);
    context->error_string = NULL;
    HEIMDAL_MUTEX_unlock(context->mutex);
}

krb5_error_code KRB5_LIB_FUNCTION
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

krb5_error_code KRB5_LIB_FUNCTION
krb5_vset_error_string(krb5_context context, const char *fmt, va_list args)
    __attribute__ ((format (printf, 2, 0)))
{
    krb5_clear_error_string(context);
    HEIMDAL_MUTEX_lock(context->mutex);
    vasprintf(&context->error_string, fmt, args);
    if(context->error_string == NULL) {
	vsnprintf (context->error_buf, sizeof(context->error_buf), fmt, args);
	context->error_string = context->error_buf;
    }
    HEIMDAL_MUTEX_unlock(context->mutex);
    return 0;
}

/**
 * Return the error message in context. On error or no error string,
 * the function returns NULL.
 *
 * @param context Kerberos 5 context
 *
 * @return an error string, needs to be freed with
 * krb5_free_error_string(). The functions return NULL on error.
 *
 * @ingroup krb5_error
 */

char * KRB5_LIB_FUNCTION
krb5_get_error_string(krb5_context context)
{
    char *ret = NULL;

    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_string)
	ret = strdup(context->error_string);
    HEIMDAL_MUTEX_unlock(context->mutex);
    return ret;
}

krb5_boolean KRB5_LIB_FUNCTION
krb5_have_error_string(krb5_context context)
{
    char *str;
    HEIMDAL_MUTEX_lock(context->mutex);
    str = context->error_string;
    HEIMDAL_MUTEX_unlock(context->mutex);
    return str != NULL;
}

/**
 * Return the error message for `code' in context. On error the
 * function returns NULL.
 *
 * @param context Kerberos 5 context
 * @param code Error code related to the error
 *
 * @return an error string, needs to be freed with
 * krb5_free_error_string(). The functions return NULL on error.
 *
 * @ingroup krb5_error
 */

char * KRB5_LIB_FUNCTION
krb5_get_error_message(krb5_context context, krb5_error_code code)
{
    const char *cstr;
    char *str;

    str = krb5_get_error_string(context);
    if (str)
	return str;

    cstr = krb5_get_err_text(context, code);
    if (cstr)
	return strdup(cstr);

    if (asprintf(&str, "<unknown error: %d>", code) == -1)
	return NULL;

    return str;
}

