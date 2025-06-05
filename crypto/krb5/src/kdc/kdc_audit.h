/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/kdc_audit.h - KDC-facing API for audit */
/*
 * Copyright 2013 by the Massachusetts Institute of Technology.
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

#ifndef KRB5_KDC_AUDIT__
#define KRB5_KDC_AUDIT__

#include <krb5/krb5.h>
#include <net-server.h>
#include <krb5/audit_plugin.h>

krb5_error_code load_audit_modules(krb5_context context);
void unload_audit_modules(krb5_context context);

/* Utilities */

krb5_error_code
kau_make_tkt_id(krb5_context context,
                const krb5_ticket *ticket, char **out);

krb5_error_code
kau_init_kdc_req(krb5_context context, krb5_kdc_req *request,
                 const krb5_fulladdr *from, krb5_audit_state **au_state);

void kau_free_kdc_req(krb5_audit_state *state);

/* KDC-facing audit API */

void
kau_kdc_start(krb5_context context, const krb5_boolean ev_success);

void
kau_kdc_stop(krb5_context context, const krb5_boolean ev_success);

void
kau_as_req(krb5_context context, const krb5_boolean ev_success,
          krb5_audit_state *state);

void
kau_tgs_req(krb5_context context, const krb5_boolean ev_success,
           krb5_audit_state *state);

void
kau_s4u2self(krb5_context context, const krb5_boolean ev_success,
             krb5_audit_state *state);

void
kau_s4u2proxy(krb5_context context, const krb5_boolean ev_success,
              krb5_audit_state *state);

void
kau_u2u(krb5_context context, const krb5_boolean ev_success,
        krb5_audit_state *state);

#endif /* KRB5_KDC_AUDIT__ */
