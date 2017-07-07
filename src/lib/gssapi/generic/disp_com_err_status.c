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

/*
 * $Id$
 */

#include "gssapiP_generic.h"
#include "com_err.h"

/* XXXX internationalization!! */

/**/

static const char * const no_error = "No error";

/**/

/* if status_type == GSS_C_GSS_CODE, return up to three error messages,
   for routine errors, call error, and status, in that order.
   message_context == 0 : print the routine error
   message_context == 1 : print the calling error
   message_context > 2  : print supplementary info bit (message_context-2)
   if status_type == GSS_C_MECH_CODE, return the output from error_message()
*/

OM_uint32
g_display_com_err_status(OM_uint32 *minor_status, OM_uint32 status_value,
                         gss_buffer_t status_string)
{
    status_string->length = 0;
    status_string->value = NULL;

    if (! g_make_string_buffer(((status_value == 0)?no_error:
                                error_message(status_value)),
                               status_string)) {
        *minor_status = ENOMEM;
        return(GSS_S_FAILURE);
    }
    *minor_status = 0;
    return(GSS_S_COMPLETE);
}
