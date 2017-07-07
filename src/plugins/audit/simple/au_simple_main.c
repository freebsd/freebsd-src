/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/audit/au_simple_main.c - Sample Audit plugin implementation */
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
 * This is a demo implementation of Audit JSON-based module.
 * It utilizes MIT Kerberos <kdc_j_encode.h> routines for JSON processing and
 * the Fedora/Debian libaudit library for audit logs.
 */

#include <k5-int.h>
#include <krb5/audit_plugin.h>
#include <libaudit.h>
#include <kdc_j_encode.h>

krb5_error_code
audit_simple_initvt(krb5_context context, int maj_ver, int min_ver,
                    krb5_plugin_vtable vtable);

struct krb5_audit_moddata_st {
    int fd;
};

/* Open connection to the audit system. Returns 0 on success. */
static krb5_error_code
open_au(krb5_audit_moddata *auctx_out)
{
    krb5_error_code ret;
    int fd = 0;
    krb5_audit_moddata auctx;

    auctx = k5calloc(1, sizeof(*auctx), &ret);
    if (ret)
        return ENOMEM;
    fd = audit_open();
    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    auctx->fd = fd;
    *auctx_out = auctx;

    return 0;
}

/* Close connection to the audit system. Returns 0 on success. */
static krb5_error_code
close_au(krb5_audit_moddata auctx)
{
    int fd = auctx->fd;

    audit_close(fd);
    return 0;
}

/* Log KDC-start event. Returns 0 on success. */
static krb5_error_code
j_kdc_start(krb5_audit_moddata auctx, krb5_boolean ev_success)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_START;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_kdc_start(ev_success, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

/* Log KDC-stop event. Returns 0 on success. */
static krb5_error_code
j_kdc_stop(krb5_audit_moddata auctx, krb5_boolean ev_success)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_END;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_kdc_stop(ev_success, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

/* Log AS_REQ event. Returns 0 on success */
static krb5_error_code
j_as_req(krb5_audit_moddata auctx, krb5_boolean ev_success,
         krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_AUTH;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_as_req(ev_success, state, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

/* Log TGS_REQ event. Returns 0 on success */
static krb5_error_code
j_tgs_req(krb5_audit_moddata auctx, krb5_boolean ev_success,
          krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_AUTH;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_tgs_req(ev_success, state, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

/* Log S4U2SELF event. Returns 0 on success */
static krb5_error_code
j_tgs_s4u2self(krb5_audit_moddata auctx, krb5_boolean ev_success,
               krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_AUTH;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_tgs_s4u2self(ev_success, state, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

/* Log S4U2PROXY event. Returns 0 on success */
static krb5_error_code
j_tgs_s4u2proxy(krb5_audit_moddata auctx, krb5_boolean ev_success,
                krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_AUTH;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_tgs_s4u2proxy(ev_success, state, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

/* Log user-to-user event. Returns 0 on success */
static krb5_error_code
j_tgs_u2u(krb5_audit_moddata auctx, krb5_boolean ev_success,
          krb5_audit_state *state)
{
    krb5_error_code ret = 0;
    int local_type = AUDIT_USER_AUTH;
    int fd = auctx->fd;
    char *jout = NULL;

    if (fd < 0)
        return KRB5_PLUGIN_NO_HANDLE; /* audit module is unavailable */

    ret = kau_j_tgs_u2u(ev_success, state, &jout);
    if (ret)
        return ret;
    if (audit_log_user_message(fd, local_type, jout,
                               NULL, NULL, NULL, ev_success) <= 0)
        ret = EIO;
    free(jout);
    return ret;
}

krb5_error_code
audit_simple_initvt(krb5_context context, int maj_ver,
                    int min_ver, krb5_plugin_vtable vtable)
{
    krb5_audit_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt = (krb5_audit_vtable)vtable;
    vt->name = "simple";
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
