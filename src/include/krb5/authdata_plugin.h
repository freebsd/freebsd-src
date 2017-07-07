/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2007 Apple Inc.  All Rights Reserved.
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

/*
 * Authorization data plugin definitions for Kerberos 5.
 * This is considered an INTERNAL interface at this time.
 *
 * Some work is needed before exporting it:
 *
 * + Documentation.
 * + Sample code.
 * + Test cases (preferably automated testing under "make check").
 *
 * Other changes that would be nice to have, but not necessarily
 * before making this interface public:
 *
 * + Library support for AD-IF-RELEVANT and similar wrappers.  (We can
 *   make the plugin construct them if it wants them.)
 * + KDC could combine/optimize wrapped AD elements provided by
 *   multiple plugins, e.g., two IF-RELEVANT sequences could be
 *   merged.  (The preauth plugin API also has this bug, we're going
 *   to need a general fix.)
 */

#ifndef KRB5_AUTHDATA_PLUGIN_H_INCLUDED
#define KRB5_AUTHDATA_PLUGIN_H_INCLUDED
#include <krb5/krb5.h>

typedef krb5_error_code
(*authdata_client_plugin_init_proc)(krb5_context context,
                                    void **plugin_context);

#define AD_USAGE_AS_REQ         0x01
#define AD_USAGE_TGS_REQ        0x02
#define AD_USAGE_AP_REQ         0x04
#define AD_USAGE_KDC_ISSUED     0x08
#define AD_INFORMATIONAL        0x10
#define AD_CAMMAC_PROTECTED     0x20
#define AD_USAGE_MASK           0x2F

struct _krb5_authdata_context;

typedef void
(*authdata_client_plugin_flags_proc)(krb5_context kcontext,
                                     void *plugin_context,
                                     krb5_authdatatype ad_type,
                                     krb5_flags *flags);

typedef void
(*authdata_client_plugin_fini_proc)(krb5_context kcontext,
                                    void *plugin_context);

typedef krb5_error_code
(*authdata_client_request_init_proc)(krb5_context kcontext,
                                     struct _krb5_authdata_context *context,
                                     void *plugin_context,
                                     void **request_context);

typedef void
(*authdata_client_request_fini_proc)(krb5_context kcontext,
                                     struct _krb5_authdata_context *context,
                                     void *plugin_context,
                                     void *request_context);

typedef krb5_error_code
(*authdata_client_import_authdata_proc)(krb5_context kcontext,
                                        struct _krb5_authdata_context *context,
                                        void *plugin_context,
                                        void *request_context,
                                        krb5_authdata **authdata,
                                        krb5_boolean kdc_issued_flag,
                                        krb5_const_principal issuer);

typedef krb5_error_code
(*authdata_client_export_authdata_proc)(krb5_context kcontext,
                                        struct _krb5_authdata_context *context,
                                        void *plugin_context,
                                        void *request_context,
                                        krb5_flags usage,
                                        krb5_authdata ***authdata);

typedef krb5_error_code
(*authdata_client_get_attribute_types_proc)(krb5_context kcontext,
                                            struct _krb5_authdata_context *context,
                                            void *plugin_context,
                                            void *request_context,
                                            krb5_data **attrs);

typedef krb5_error_code
(*authdata_client_get_attribute_proc)(krb5_context kcontext,
                                      struct _krb5_authdata_context *context,
                                      void *plugin_context,
                                      void *request_context,
                                      const krb5_data *attribute,
                                      krb5_boolean *authenticated,
                                      krb5_boolean *complete,
                                      krb5_data *value,
                                      krb5_data *display_value,
                                      int *more);

typedef krb5_error_code
(*authdata_client_set_attribute_proc)(krb5_context kcontext,
                                      struct _krb5_authdata_context *context,
                                      void *plugin_context,
                                      void *request_context,
                                      krb5_boolean complete,
                                      const krb5_data *attribute,
                                      const krb5_data *value);

typedef krb5_error_code
(*authdata_client_delete_attribute_proc)(krb5_context kcontext,
                                         struct _krb5_authdata_context *context,
                                         void *plugin_context,
                                         void *request_context,
                                         const krb5_data *attribute);

typedef krb5_error_code
(*authdata_client_export_internal_proc)(krb5_context kcontext,
                                        struct _krb5_authdata_context *context,
                                        void *plugin_context,
                                        void *request_context,
                                        krb5_boolean restrict_authenticated,
                                        void **ptr);

typedef void
(*authdata_client_free_internal_proc)(krb5_context kcontext,
                                      struct _krb5_authdata_context *context,
                                      void *plugin_context,
                                      void *request_context,
                                      void *ptr);

typedef krb5_error_code
(*authdata_client_verify_proc)(krb5_context kcontext,
                               struct _krb5_authdata_context *context,
                               void *plugin_context,
                               void *request_context,
                               const krb5_auth_context *auth_context,
                               const krb5_keyblock *key,
                               const krb5_ap_req *req);

typedef krb5_error_code
(*authdata_client_size_proc)(krb5_context kcontext,
                             struct _krb5_authdata_context *context,
                             void *plugin_context,
                             void *request_context,
                             size_t *sizep);

typedef krb5_error_code
(*authdata_client_externalize_proc)(krb5_context kcontext,
                                    struct _krb5_authdata_context *context,
                                    void *plugin_context,
                                    void *request_context,
                                    krb5_octet **buffer,
                                    size_t *lenremain);

typedef krb5_error_code
(*authdata_client_internalize_proc)(krb5_context kcontext,
                                    struct _krb5_authdata_context *context,
                                    void *plugin_context,
                                    void *request_context,
                                    krb5_octet **buffer,
                                    size_t *lenremain);

typedef krb5_error_code
(*authdata_client_copy_proc)(krb5_context kcontext,
                             struct _krb5_authdata_context *context,
                             void *plugin_context,
                             void *request_context,
                             void *dst_plugin_context,
                             void *dst_request_context);

typedef struct krb5plugin_authdata_client_ftable_v0 {
    char *name;
    krb5_authdatatype *ad_type_list;
    authdata_client_plugin_init_proc init;
    authdata_client_plugin_fini_proc fini;
    authdata_client_plugin_flags_proc flags;
    authdata_client_request_init_proc request_init;
    authdata_client_request_fini_proc request_fini;
    authdata_client_get_attribute_types_proc get_attribute_types;
    authdata_client_get_attribute_proc get_attribute;
    authdata_client_set_attribute_proc set_attribute;
    authdata_client_delete_attribute_proc delete_attribute;
    authdata_client_export_authdata_proc export_authdata;
    authdata_client_import_authdata_proc import_authdata;
    authdata_client_export_internal_proc export_internal;
    authdata_client_free_internal_proc free_internal;
    authdata_client_verify_proc verify;
    authdata_client_size_proc size;
    authdata_client_externalize_proc externalize;
    authdata_client_internalize_proc internalize;
    authdata_client_copy_proc copy; /* optional */
} krb5plugin_authdata_client_ftable_v0;

#endif /* KRB5_AUTHDATA_PLUGIN_H_INCLUDED */
