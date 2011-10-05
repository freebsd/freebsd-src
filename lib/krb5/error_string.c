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

#undef __attribute__
#define __attribute__(x)

/**
 * Clears the error message from the Kerberos 5 context.
 *
 * @param context The Kerberos 5 context to clear
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_clear_error_message(krb5_context context)
{
    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_string)
	free(context->error_string);
    context->error_code = 0;
    context->error_string = NULL;
    HEIMDAL_MUTEX_unlock(context->mutex);
}

/**
 * Set the context full error string for a specific error code.
 * The error that is stored should be internationalized.
 *
 * The if context is NULL, no error string is stored.
 *
 * @param context Kerberos 5 context
 * @param ret The error code
 * @param fmt Error string for the error code
 * @param ... printf(3) style parameters.
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_set_error_message(krb5_context context, krb5_error_code ret,
		       const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)))
{
    va_list ap;

    va_start(ap, fmt);
    krb5_vset_error_message (context, ret, fmt, ap);
    va_end(ap);
}

/**
 * Set the context full error string for a specific error code.
 *
 * The if context is NULL, no error string is stored.
 *
 * @param context Kerberos 5 context
 * @param ret The error code
 * @param fmt Error string for the error code
 * @param args printf(3) style parameters.
 *
 * @ingroup krb5_error
 */


KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_vset_error_message (krb5_context context, krb5_error_code ret,
			 const char *fmt, va_list args)
    __attribute__ ((format (printf, 3, 0)))
{
    int r;

    if (context == NULL)
	return;

    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_string) {
	free(context->error_string);
	context->error_string = NULL;
    }
    context->error_code = ret;
    r = vasprintf(&context->error_string, fmt, args);
    if (r < 0)
	context->error_string = NULL;
    HEIMDAL_MUTEX_unlock(context->mutex);
}

/**
 * Prepend the context full error string for a specific error code.
 * The error that is stored should be internationalized.
 *
 * The if context is NULL, no error string is stored.
 *
 * @param context Kerberos 5 context
 * @param ret The error code
 * @param fmt Error string for the error code
 * @param ... printf(3) style parameters.
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_prepend_error_message(krb5_context context, krb5_error_code ret,
			   const char *fmt, ...)
    __attribute__ ((format (printf, 3, 4)))
{
    va_list ap;

    va_start(ap, fmt);
    krb5_vprepend_error_message(context, ret, fmt, ap);
    va_end(ap);
}

/**
 * Prepend the contexts's full error string for a specific error code.
 *
 * The if context is NULL, no error string is stored.
 *
 * @param context Kerberos 5 context
 * @param ret The error code
 * @param fmt Error string for the error code
 * @param args printf(3) style parameters.
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_vprepend_error_message(krb5_context context, krb5_error_code ret,
			    const char *fmt, va_list args)
    __attribute__ ((format (printf, 3, 0)))
{
    char *str = NULL, *str2 = NULL;

    if (context == NULL)
	return;

    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_code != ret) {
	HEIMDAL_MUTEX_unlock(context->mutex);
	return;
    }
    if (vasprintf(&str, fmt, args) < 0 || str == NULL) {
	HEIMDAL_MUTEX_unlock(context->mutex);
	return;
    }
    if (context->error_string) {
	int e;

	e = asprintf(&str2, "%s: %s", str, context->error_string);
	free(context->error_string);
	if (e < 0 || str2 == NULL)
	    context->error_string = NULL;
	else
	    context->error_string = str2;
	free(str);
    } else
	context->error_string = str;
    HEIMDAL_MUTEX_unlock(context->mutex);
}


/**
 * Return the error message in context. On error or no error string,
 * the function returns NULL.
 *
 * @param context Kerberos 5 context
 *
 * @return an error string, needs to be freed with
 * krb5_free_error_message(). The functions return NULL on error.
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION char * KRB5_LIB_CALL
krb5_get_error_string(krb5_context context)
{
    char *ret = NULL;

    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_string)
	ret = strdup(context->error_string);
    HEIMDAL_MUTEX_unlock(context->mutex);
    return ret;
}

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_have_error_string(krb5_context context)
{
    char *str;
    HEIMDAL_MUTEX_lock(context->mutex);
    str = context->error_string;
    HEIMDAL_MUTEX_unlock(context->mutex);
    return str != NULL;
}

/**
 * Return the error message for `code' in context. On memory
 * allocation error the function returns NULL.
 *
 * @param context Kerberos 5 context
 * @param code Error code related to the error
 *
 * @return an error string, needs to be freed with
 * krb5_free_error_message(). The functions return NULL on error.
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION const char * KRB5_LIB_CALL
krb5_get_error_message(krb5_context context, krb5_error_code code)
{
    char *str;

    HEIMDAL_MUTEX_lock(context->mutex);
    if (context->error_string &&
	(code == context->error_code || context->error_code == 0))
    {
	str = strdup(context->error_string);
	if (str) {
	    HEIMDAL_MUTEX_unlock(context->mutex);
	    return str;
	}
    }
    HEIMDAL_MUTEX_unlock(context->mutex);

    if (code == 0)
	return strdup("Success");
    {
	const char *msg;
	char buf[128];
	msg = com_right_r(context->et_list, code, buf, sizeof(buf));
	if (msg)
	    return strdup(msg);
    }

    if (asprintf(&str, "<unknown error: %d>", (int)code) == -1 || str == NULL)
	return NULL;

    return str;
}


/**
 * Free the error message returned by krb5_get_error_message().
 *
 * @param context Kerberos context
 * @param msg error message to free, returned byg
 *        krb5_get_error_message().
 *
 * @ingroup krb5_error
 */

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_free_error_message(krb5_context context, const char *msg)
{
    free(rk_UNCONST(msg));
}


/**
 * Return the error string for the error code. The caller must not
 * free the string.
 *
 * This function is deprecated since its not threadsafe.
 *
 * @param context Kerberos 5 context.
 * @param code Kerberos error code.
 *
 * @return the error message matching code
 *
 * @ingroup krb5
 */

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_get_err_text(krb5_context context, krb5_error_code code)
    KRB5_DEPRECATED_FUNCTION("Use X instead")
{
    const char *p = NULL;
    if(context != NULL)
	p = com_right(context->et_list, code);
    if(p == NULL)
	p = strerror(code);
    if (p == NULL)
	p = "Unknown error";
    return p;
}
