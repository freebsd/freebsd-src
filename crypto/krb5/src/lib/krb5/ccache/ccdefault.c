/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccdefault.c - Find default credential cache */
/*
 * Copyright 1990, 2007, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"

#ifdef USE_LEASH
static void (*pLeash_AcquireInitialTicketsIfNeeded)(krb5_context,krb5_principal,char*,int) = NULL;
static HANDLE hLeashDLL = INVALID_HANDLE_VALUE;
#ifdef _WIN64
#define LEASH_DLL "leashw64.dll"
#else
#define LEASH_DLL "leashw32.dll"
#endif
#endif


krb5_error_code KRB5_CALLCONV
krb5_cc_default(krb5_context context, krb5_ccache *ccache)
{
    const char *default_name;

    if (!context || context->magic != KV5M_CONTEXT)
        return KV5M_CONTEXT;

    default_name = krb5_cc_default_name(context);
    if (default_name == NULL) {
        /* Could be a bogus context, or an allocation failure, or
           other things.  Unfortunately the API doesn't allow us
           to find out any specifics.  */
        return KRB5_FCC_INTERNAL;
    }

    return krb5_cc_resolve(context, default_name, ccache);
}

/* This is the internal function which opens the default ccache.  On
   platforms supporting the login library's automatic popup dialog to
   get tickets, this function also updated the library's internal view
   of the current principal associated with this cache.

   All krb5 and GSS functions which need to open a cache to get a tgt
   to obtain service tickets should call this function, not
   krb5_cc_default().  */

krb5_error_code KRB5_CALLCONV
krb5int_cc_default(krb5_context context, krb5_ccache *ccache)
{
    if (!context || context->magic != KV5M_CONTEXT) {
        return KV5M_CONTEXT;
    }

#ifdef USE_LEASH
    if ( hLeashDLL == INVALID_HANDLE_VALUE ) {
        hLeashDLL = LoadLibrary(LEASH_DLL);
        if ( hLeashDLL != INVALID_HANDLE_VALUE ) {
            (FARPROC) pLeash_AcquireInitialTicketsIfNeeded =
                GetProcAddress(hLeashDLL, "not_an_API_Leash_AcquireInitialTicketsIfNeeded");
        }
    }

    if ( pLeash_AcquireInitialTicketsIfNeeded ) {
        char ccname[256]="";
        pLeash_AcquireInitialTicketsIfNeeded(context, NULL, ccname, sizeof(ccname));
        if (ccname[0]) {
            char * ccdefname = krb5_cc_default_name (context);
            if (!ccdefname || strcmp (ccdefname, ccname) != 0) {
                krb5_cc_set_default_name (context, ccname);
            }
        }
    }
#endif

    return krb5_cc_default (context, ccache);
}
