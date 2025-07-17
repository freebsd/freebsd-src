/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1994 by OpenVision Technologies, Inc.
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

#include "k5-int.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "port-sockets.h"
#include "socket-utils.h"

krb5_error_code KRB5_CALLCONV
krb5_524_convert_creds(krb5_context context, krb5_creds *v5creds,
                       struct credentials *v4creds)
{
    return KRB524_KRB4_DISABLED;
}

/* These may be needed for object-level backwards compatibility on Mac
   OS and UNIX, but Windows should be okay.  */
#ifndef _WIN32
#undef krb524_convert_creds_kdc
#undef krb524_init_ets

/* Declarations ahead of the definitions will suppress some gcc
   warnings.  */
void KRB5_CALLCONV krb524_init_ets (void);
krb5_error_code KRB5_CALLCONV
krb524_convert_creds_kdc(krb5_context context, krb5_creds *v5creds,
                         struct credentials *v4creds);

krb5_error_code KRB5_CALLCONV
krb524_convert_creds_kdc(krb5_context context, krb5_creds *v5creds,
                         struct credentials *v4creds)
{
    return KRB524_KRB4_DISABLED;
}

void KRB5_CALLCONV krb524_init_ets ()
{
}
#endif
