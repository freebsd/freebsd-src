/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This interface is not yet public and may change without honoring the
 * pluggable interface version system.  The header file is deliberately not
 * installed by "make install".
 *
 * This interface references kdb.h, which is not stable.  A module which
 * references fields of krb5_db_entry, invokes libkdb5 functions, or uses the
 * flags argument may not be future-proof even if this interface becomes
 * public.
 *
 * Expecting modules to hand-modify enc_tkt_reply->authorization_data is
 * cumbersome for the module, and doesn't work if the module uses a different
 * allocator from the krb5 tree.  A libkrb5 API to add an authdata value to a
 * list would improve this situation.
 */

/*
 * Declarations for kdcauthdata plugin module implementors.
 *
 * The kdcauthdata interface has a single supported major version, which is 1.
 * Major version 1 has a current minor version of 1.  kdcauthdata modules
 * should define a function named kdcauthdata_<modulename>_initvt, matching the
 * signature:
 *
 *   krb5_error_code
 *   kdcauthdata_modname_initvt(krb5_context context, int maj_ver, int min_ver,
 *                              krb5_plugin_vtable vtable);
 *
 * The initvt function should:
 *
 * - Check that the supplied maj_ver number is supported by the module, or
 *   return KRB5_PLUGIN_VER_NOTSUPP if it is not.
 *
 * - Cast the vtable pointer as appropriate for the interface and maj_ver:
 *     maj_ver == 1: Cast to krb5_kdcauthdata_vtable
 *
 * - Initialize the methods of the vtable, stopping as appropriate for the
 *   supplied min_ver.  Optional methods may be left uninitialized.
 *
 * Memory for the vtable is allocated by the caller, not by the module.
 */

#ifndef KRB5_KDCAUTHDATA_PLUGIN_H
#define KRB5_KDCAUTHDATA_PLUGIN_H

#include <krb5/krb5.h>
#include <krb5/plugin.h>
#include <kdb.h>

/* Abstract type for module data. */
typedef struct krb5_kdcauthdata_moddata_st *krb5_kdcauthdata_moddata;

/* Optional: module initialization function.  If this function returns an
 * error, the KDC will log the failure and ignore the module. */
typedef krb5_error_code
(*krb5_kdcauthdata_init_fn)(krb5_context context,
                            krb5_kdcauthdata_moddata *moddata_out);

/* Optional: module cleanup function. */
typedef void
(*krb5_kdcauthdata_fini_fn)(krb5_context context,
                            krb5_kdcauthdata_moddata moddata);

/*
 * Mandatory: authorization data handling function.
 *
 * All values should be considered input-only, except that the method can
 * modify enc_tkt_reply->authorization_data to add authdata values.  This field
 * is a null-terminated list of allocated pointers; the plugin method must
 * reallocate it to make room for any added authdata values.
 *
 * If this function returns an error, the AS or TGS request will be rejected.
 */
typedef krb5_error_code
(*krb5_kdcauthdata_handle_fn)(krb5_context context,
                              krb5_kdcauthdata_moddata moddata,
                              unsigned int flags,
                              krb5_db_entry *client, krb5_db_entry *server,
                              krb5_db_entry *header_server,
                              krb5_keyblock *client_key,
                              krb5_keyblock *server_key,
                              krb5_keyblock *header_key,
                              krb5_data *req_pkt, krb5_kdc_req *req,
                              krb5_const_principal for_user_princ,
                              krb5_enc_tkt_part *enc_tkt_req,
                              krb5_enc_tkt_part *enc_tkt_reply);

typedef struct krb5_kdcauthdata_vtable_st {
    /* Mandatory: name of module. */
    const char *name;

    krb5_kdcauthdata_init_fn init;
    krb5_kdcauthdata_fini_fn fini;
    krb5_kdcauthdata_handle_fn handle;
    /* Minor 1 ends here. */
} *krb5_kdcauthdata_vtable;

#endif /* KRB5_KDCAUTHDATA_PLUGIN_H */
