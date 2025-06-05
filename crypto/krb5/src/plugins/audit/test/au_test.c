/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/audit/au_test.c - Test Audit plugin implementation */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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
 * This test is to verify the JSON-based KDC audit functionality.
 * It utilized MIT Kerberos <kdc_j_encode.h> routines for JSON processing.
 */

#include <k5-int.h>
#include <krb5/audit_plugin.h>
#include <kdc_j_encode.h>
#include "k5-thread.h"

struct krb5_audit_moddata_st {
    int au_fd;
};

krb5_error_code
audit_test_initvt(krb5_context context, int maj_ver, int min_ver,
                  krb5_plugin_vtable vtable);

static FILE *au_fd;
static k5_mutex_t lock = K5_MUTEX_PARTIAL_INITIALIZER;

/* Open connection to the audit system. Returns 0 on success. */
static krb5_error_code
open_au(krb5_audit_moddata *auctx)
{
    au_fd = fopen("au.log", "a+");
    if (au_fd == NULL)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */
    k5_mutex_init(&lock);
    return 0;
}

/* Close connection to the audit system. Returns 0. */
static krb5_error_code
close_au(krb5_audit_moddata auctx)
{
    fclose(au_fd);
    k5_mutex_destroy(&lock);
    return 0;
}

/* Log KDC-start event. Returns 0 on success. */
static krb5_error_code
j_kdc_start(krb5_audit_moddata auctx, krb5_boolean ev_success)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_kdc_start(ev_success, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

/* Log KDC-stop event. Returns 0 on success. */
static krb5_error_code
j_kdc_stop(krb5_audit_moddata auctx, krb5_boolean ev_success)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_kdc_stop(ev_success, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

/* Log AS_REQ event. Returns 0 on success. */
static krb5_error_code
j_as_req(krb5_audit_moddata auctx, krb5_boolean ev_success,
         krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_as_req(ev_success, state, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

/* Log TGS_REQ event. Returns 0 on success. */
static krb5_error_code
j_tgs_req(krb5_audit_moddata auctx, krb5_boolean ev_success,
          krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_tgs_req(ev_success, state, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

/* Log S4U2SELF event. Returns 0 on success. */
static krb5_error_code
j_tgs_s4u2self(krb5_audit_moddata auctx, krb5_boolean ev_success,
               krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_tgs_s4u2self(ev_success, state, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

/* Log S4U2PROXY event. Returns 0 on success. */
static krb5_error_code
j_tgs_s4u2proxy(krb5_audit_moddata auctx, krb5_boolean ev_success,
                krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_tgs_s4u2proxy(ev_success, state, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

/* Log user-to-user event. Returns 0 on success. */
static krb5_error_code
j_tgs_u2u(krb5_audit_moddata auctx, krb5_boolean ev_success,
          krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    char *jout = NULL;

    ret = kau_j_tgs_u2u(ev_success, state, &jout);
    if (ret)
        return ret;
    k5_mutex_lock(&lock);
    fprintf(au_fd,"%s\n", jout);
    fflush(au_fd);
    k5_mutex_unlock(&lock);
    free(jout);
    return ret;
}

krb5_error_code
audit_test_initvt(krb5_context context, int maj_ver, int min_ver,
                  krb5_plugin_vtable vtable)
{
    krb5_audit_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt = (krb5_audit_vtable)vtable;
    vt->name = "test";

    vt->open = open_au;
    vt->close = close_au;
    vt->kdc_start = j_kdc_start;
    vt->kdc_stop = j_kdc_stop;
    vt->as_req = j_as_req;
    vt->tgs_req = j_tgs_req;
    vt->tgs_s4u2self = j_tgs_s4u2self;
    vt->tgs_s4u2proxy = j_tgs_s4u2proxy;
    vt->tgs_u2u = j_tgs_u2u;

    return 0;
}
