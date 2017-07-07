/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "gssapiP_generic.h"
#include <string.h>
#include <stdio.h>

/*
 * $Id$
 */

/* This code has knowledge of the min and max errors of each type
   within the gssapi major status */

#define GSS_ERROR_STR(value, array, select, min, max, num)              \
    (((select(value) < (min)) || (select(value) > (max))) ? NULL :      \
     _((array)[num(value)]))

/**/

static const char * const calling_error_string[] = {
    NULL,
    N_("A required input parameter could not be read"),
    N_("A required input parameter could not be written"),
    N_("A parameter was malformed"),
};

static const char * const calling_error = N_("calling error");

#define GSS_CALLING_ERROR_STR(x)                                        \
    GSS_ERROR_STR((x), calling_error_string, GSS_CALLING_ERROR,         \
                  GSS_S_CALL_INACCESSIBLE_READ, GSS_S_CALL_BAD_STRUCTURE, \
                  GSS_CALLING_ERROR_FIELD)

/**/

static const char * const routine_error_string[] = {
    NULL,
    N_("An unsupported mechanism was requested"),
    N_("An invalid name was supplied"),
    N_("A supplied name was of an unsupported type"),
    N_("Incorrect channel bindings were supplied"),
    N_("An invalid status code was supplied"),
    N_("A token had an invalid signature"),
    N_("No credentials were supplied"),
    N_("No context has been established"),
    N_("A token was invalid"),
    N_("A credential was invalid"),
    N_("The referenced credentials have expired"),
    N_("The context has expired"),
    N_("Miscellaneous failure"),
    N_("The quality-of-protection requested could not be provided"),
    N_("The operation is forbidden by the local security policy"),
    N_("The operation or option is not available"),
};

static const char * const routine_error = N_("routine error");

#define GSS_ROUTINE_ERROR_STR(x)                                \
    GSS_ERROR_STR((x), routine_error_string, GSS_ROUTINE_ERROR, \
                  GSS_S_BAD_MECH, GSS_S_FAILURE,                \
                  GSS_ROUTINE_ERROR_FIELD)

/**/

/* this becomes overly gross after about 4 strings */

static const char * const sinfo_string[] = {
    N_("The routine must be called again to complete its function"),
    N_("The token was a duplicate of an earlier token"),
    N_("The token's validity period has expired"),
    N_("A later token has already been processed"),
};

static const char * const sinfo_code = N_("supplementary info code");

#define LSBGET(x) ((((x)^((x)-1))+1)>>1)
#define LSBMASK(n) ((1<<(n))^((1<<(n))-1))

#define GSS_SINFO_STR(x)                                                \
    ((((1<<(x)) < GSS_S_CONTINUE_NEEDED) || ((1<<(x)) > GSS_S_UNSEQ_TOKEN)) ? \
     /**/NULL:sinfo_string[(x)])

/**/

static const char * const no_error = N_("No error");
static const char * const unknown_error = N_("Unknown %s (field = %d)");

/**/

static int
display_unknown(const char *kind, OM_uint32 value, gss_buffer_t buffer)
{
    char *str;

    if (asprintf(&str, _(unknown_error), kind, value) < 0)
        return(0);

    buffer->length = strlen(str);
    buffer->value = str;

    return(1);
}

/* code should be set to the calling error field */

static OM_uint32
display_calling(OM_uint32 *minor_status, OM_uint32 code,
                gss_buffer_t status_string)
{
    const char *str;

    if ((str = GSS_CALLING_ERROR_STR(code))) {
        if (! g_make_string_buffer(str, status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
    } else {
        if (! display_unknown(_(calling_error), GSS_CALLING_ERROR_FIELD(code),
                              status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
    }
    *minor_status = 0;
    return(GSS_S_COMPLETE);
}

/* code should be set to the routine error field */

static OM_uint32
display_routine(OM_uint32 *minor_status, OM_uint32 code,
                gss_buffer_t status_string)
{
    const char *str;

    if ((str = GSS_ROUTINE_ERROR_STR(code))) {
        if (! g_make_string_buffer(str, status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
    } else {
        if (! display_unknown(_(routine_error), GSS_ROUTINE_ERROR_FIELD(code),
                              status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
    }
    *minor_status = 0;
    return(GSS_S_COMPLETE);
}

/* code should be set to the bit offset (log_2) of a supplementary info bit */

static OM_uint32
display_bit(OM_uint32 *minor_status, OM_uint32 code,
            gss_buffer_t status_string)
{
    const char *str;

    if ((str = GSS_SINFO_STR(code))) {
        if (! g_make_string_buffer(str, status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
    } else {
        if (! display_unknown(_(sinfo_code), 1<<code, status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
    }
    *minor_status = 0;
    return(GSS_S_COMPLETE);
}

/**/

/* return error messages, for routine errors, call error, and status,
   in that order.
   message_context == 0 : print the routine error
   message_context == 1 : print the calling error
   message_context > 2  : print supplementary info bit (message_context-2)
*/

OM_uint32
g_display_major_status(OM_uint32 *minor_status, OM_uint32 status_value,
                       OM_uint32 *message_context, gss_buffer_t status_string)
{
    OM_uint32 ret, tmp;
    int bit;

    /*** deal with no error at all specially */

    if (status_value == 0) {
        if (! g_make_string_buffer(no_error, status_string)) {
            *minor_status = ENOMEM;
            return(GSS_S_FAILURE);
        }
        *message_context = 0;
        *minor_status = 0;
        return(GSS_S_COMPLETE);
    }

    /*** do routine error */

    if (*message_context == 0) {
        if ((tmp = GSS_ROUTINE_ERROR(status_value))) {
            status_value -= tmp;
            if ((ret = display_routine(minor_status, tmp, status_string)))
                return(ret);
            *minor_status = 0;
            if (status_value) {
                (*message_context)++;
                return(GSS_S_COMPLETE);
            } else {
                *message_context = 0;
                return(GSS_S_COMPLETE);
            }
        } else {
            (*message_context)++;
        }
    } else {
        status_value -= GSS_ROUTINE_ERROR(status_value);
    }

    /*** do calling error */

    if (*message_context == 1) {
        if ((tmp = GSS_CALLING_ERROR(status_value))) {
            status_value -= tmp;
            if ((ret = display_calling(minor_status, tmp, status_string)))
                return(ret);
            *minor_status = 0;
            if (status_value) {
                (*message_context)++;
                return(GSS_S_COMPLETE);
            } else {
                *message_context = 0;
                return(GSS_S_COMPLETE);
            }
        } else {
            (*message_context)++;
        }
    } else {
        status_value -= GSS_CALLING_ERROR(status_value);
    }

    /*** do sinfo bits (*message_context == 2 + number of bits done) */

    tmp = GSS_SUPPLEMENTARY_INFO_FIELD(status_value);
    /* mask off the bits which have been done */
    if (*message_context > 2) {
        tmp &= ~LSBMASK(*message_context-3);
        status_value &= ~LSBMASK(*message_context-3);
    }

    if (!tmp) {
        /* bogon input - there should be something left */
        *minor_status = (OM_uint32) G_BAD_MSG_CTX;
        return(GSS_S_FAILURE);
    }

    /* compute the bit offset */
    /*SUPPRESS 570*/
    for (bit=0; (((OM_uint32) 1)<<bit) != LSBGET(tmp); bit++) ;

    /* print it */
    if ((ret = display_bit(minor_status, bit, status_string)))
        return(ret);

    /* compute the new status_value/message_context */
    status_value -= ((OM_uint32) 1)<<bit;

    if (status_value) {
        *message_context = bit+3;
        return(GSS_S_COMPLETE);
    } else {
        *message_context = 0;
        return(GSS_S_COMPLETE);
    }
}
