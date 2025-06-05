/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/krb5/kdcpolicy_plugin.h - KDC policy plugin interface */
/*
 * Copyright (C) 2017 by Red Hat, Inc.
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

#include "k5-int.h"
#include "kdb.h"
#include <krb5/kdcpolicy_plugin.h>

static krb5_error_code
output_from_indicator(const char *const *auth_indicators, int divisor,
                      krb5_deltat *lifetime_out,
                      krb5_deltat *renew_lifetime_out,
                      const char **status)
{
    if (auth_indicators[0] == NULL) {
        *status = NULL;
        return 0;
    }

    if (strcmp(auth_indicators[0], "ONE_HOUR") == 0) {
        *lifetime_out = 3600 / divisor;
        *renew_lifetime_out = *lifetime_out * 2;
        return 0;
    } else if (strcmp(auth_indicators[0], "SEVEN_HOURS") == 0) {
        *lifetime_out = 7 * 3600 / divisor;
        *renew_lifetime_out = *lifetime_out * 2;
        return 0;
    }

    *status = "LOCAL_POLICY";
    return KRB5KDC_ERR_POLICY;
}

static krb5_error_code
test_check_as(krb5_context context, krb5_kdcpolicy_moddata moddata,
              const krb5_kdc_req *request, const krb5_db_entry *client,
              const krb5_db_entry *server, const char *const *auth_indicators,
              const char **status, krb5_deltat *lifetime_out,
              krb5_deltat *renew_lifetime_out)
{
    if (request->client != NULL && request->client->length >= 1 &&
        data_eq_string(request->client->data[0], "fail")) {
        *status = "LOCAL_POLICY";
        return KRB5KDC_ERR_POLICY;
    }
    return output_from_indicator(auth_indicators, 1, lifetime_out,
                                 renew_lifetime_out, status);
}

static krb5_error_code
test_check_tgs(krb5_context context, krb5_kdcpolicy_moddata moddata,
               const krb5_kdc_req *request, const krb5_db_entry *server,
               const krb5_ticket *ticket, const char *const *auth_indicators,
               const char **status, krb5_deltat *lifetime_out,
               krb5_deltat *renew_lifetime_out)
{
    if (request->server != NULL && request->server->length >= 1 &&
        data_eq_string(request->server->data[0], "fail")) {
        *status = "LOCAL_POLICY";
        return KRB5KDC_ERR_POLICY;
    }
    return output_from_indicator(auth_indicators, 2, lifetime_out,
                                 renew_lifetime_out, status);
}

krb5_error_code
kdcpolicy_test_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable);
krb5_error_code
kdcpolicy_test_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_kdcpolicy_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt = (krb5_kdcpolicy_vtable)vtable;
    vt->name = "test";
    vt->check_as = test_check_as;
    vt->check_tgs = test_check_tgs;
    return 0;
}
