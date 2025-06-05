/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

#include <k5-int.h>
#include <socket-utils.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h> /* for gss_nt_krb5_name */
#include <krb5.h>
#include <kadm5/admin.h>
#include <kadm5/kadm_rpc.h>
#include <kadm5/server_internal.h>
#include <syslog.h>
#include <adm_proto.h>  /* krb5_klog_syslog */
#include "misc.h"
#include "auth.h"

extern gss_name_t                       gss_changepw_name;
extern gss_name_t                       gss_oldchangepw_name;
extern void *                           global_server_handle;

#define CHANGEPW_SERVICE(rqstp)                                         \
    (cmp_gss_names_rel_1(acceptor_name(rqstp->rq_svccred), gss_changepw_name) | \
     (gss_oldchangepw_name &&                                           \
      cmp_gss_names_rel_1(acceptor_name(rqstp->rq_svccred),             \
                          gss_oldchangepw_name)))


static int gss_to_krb5_name(kadm5_server_handle_t handle,
                            gss_name_t gss_name, krb5_principal *princ);

static int gss_name_to_string(gss_name_t gss_name, gss_buffer_desc *str);

static gss_name_t acceptor_name(gss_ctx_id_t context);

gss_name_t rqst2name(struct svc_req *rqstp);

static int cmp_gss_names(gss_name_t n1, gss_name_t n2)
{
    OM_uint32 emin;
    int equal;

    if (GSS_ERROR(gss_compare_name(&emin, n1, n2, &equal)))
        return(0);

    return(equal);
}

/* Does a comparison of the names and then releases the first entity */
/* For use above in CHANGEPW_SERVICE */
static int cmp_gss_names_rel_1(gss_name_t n1, gss_name_t n2)
{
    OM_uint32 min_stat;
    int ret;

    ret = cmp_gss_names(n1, n2);
    if (n1) (void) gss_release_name(&min_stat, &n1);
    return ret;
}

/*
 * Function check_handle
 *
 * Purpose: Check a server handle and return a com_err code if it is
 * invalid or 0 if it is valid.
 *
 * Arguments:
 *
 *      handle          The server handle.
 */

static int check_handle(void *handle)
{
    CHECK_HANDLE(handle);
    return 0;
}

/*
 * Function: new_server_handle
 *
 * Purpose: Constructs a server handle suitable for passing into the
 * server library API functions, by folding the client's API version
 * and calling principal into the server handle returned by
 * kadm5_init.
 *
 * Arguments:
 *      api_version     (input) The API version specified by the client
 *      rqstp           (input) The RPC request
 *      handle          (output) The returned handle
 *      <return value>  (output) An error code, or 0 if no error occurred
 *
 * Effects:
 *      Returns a pointer to allocated storage containing the server
 *      handle.  If an error occurs, then no allocated storage is
 *      returned, and the return value of the function will be a
 *      non-zero com_err code.
 *
 *      The allocated storage for the handle should be freed with
 *      free_server_handle (see below) when it is no longer needed.
 */

static kadm5_ret_t new_server_handle(krb5_ui_4 api_version,
                                     struct svc_req *rqstp,
                                     kadm5_server_handle_t
                                     *out_handle)
{
    kadm5_server_handle_t handle;

    *out_handle = NULL;

    if (! (handle = (kadm5_server_handle_t)
           malloc(sizeof(*handle))))
        return ENOMEM;

    *handle = *(kadm5_server_handle_t)global_server_handle;
    handle->api_version = api_version;

    if (! gss_to_krb5_name(handle, rqst2name(rqstp),
                           &handle->current_caller)) {
        free(handle);
        return KADM5_FAILURE;
    }

    *out_handle = handle;
    return 0;
}

/*
 * Function: free_server_handle
 *
 * Purpose: Free handle memory allocated by new_server_handle
 *
 * Arguments:
 *      handle          (input/output) The handle to free
 */
static void free_server_handle(kadm5_server_handle_t handle)
{
    if (!handle)
        return;
    krb5_free_principal(handle->context, handle->current_caller);
    free(handle);
}

/* Result is stored in a static buffer and is invalidated by the next call. */
const char *
client_addr(SVCXPRT *xprt)
{
    static char abuf[128];
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    const char *p = NULL;

    if (getpeername(xprt->xp_sock, ss2sa(&ss), &len) != 0)
        return "(unknown)";
    if (ss2sa(&ss)->sa_family == AF_INET)
        p = inet_ntop(AF_INET, &ss2sin(&ss)->sin_addr, abuf, sizeof(abuf));
    else if (ss2sa(&ss)->sa_family == AF_INET6)
        p = inet_ntop(AF_INET6, &ss2sin6(&ss)->sin6_addr, abuf, sizeof(abuf));
    return (p == NULL) ? "(unknown)" : p;
}

/*
 * Function: setup_gss_names
 *
 * Purpose: Create printable representations of the client and server
 * names.
 *
 * Arguments:
 *      rqstp           (r) the RPC request
 *      client_name     (w) the gss_buffer_t for the client name
 *      server_name     (w) the gss_buffer_t for the server name
 *
 * Effects:
 *
 * Unparses the client and server names into client_name and
 * server_name, both of which must be freed by the caller.  Returns 0
 * on success and -1 on failure.
 */
int setup_gss_names(struct svc_req *rqstp,
                    gss_buffer_desc *client_name,
                    gss_buffer_desc *server_name)
{
    OM_uint32 maj_stat, min_stat;
    gss_name_t server_gss_name;

    if (gss_name_to_string(rqst2name(rqstp), client_name) != 0)
        return -1;
    maj_stat = gss_inquire_context(&min_stat, rqstp->rq_svccred, NULL,
                                   &server_gss_name, NULL, NULL, NULL,
                                   NULL, NULL);
    if (maj_stat != GSS_S_COMPLETE) {
        gss_release_buffer(&min_stat, client_name);
        gss_release_name(&min_stat, &server_gss_name);
        return -1;
    }
    if (gss_name_to_string(server_gss_name, server_name) != 0) {
        gss_release_buffer(&min_stat, client_name);
        gss_release_name(&min_stat, &server_gss_name);
        return -1;
    }
    gss_release_name(&min_stat, &server_gss_name);
    return 0;
}

static gss_name_t acceptor_name(gss_ctx_id_t context)
{
    OM_uint32 maj_stat, min_stat;
    gss_name_t name;

    maj_stat = gss_inquire_context(&min_stat, context, NULL, &name,
                                   NULL, NULL, NULL, NULL, NULL);
    if (maj_stat != GSS_S_COMPLETE)
        return NULL;
    return name;
}

static int gss_to_krb5_name(kadm5_server_handle_t handle,
                            gss_name_t gss_name, krb5_principal *princ)
{
    krb5_error_code ret;
    OM_uint32 minor_stat;
    gss_buffer_desc gss_str;
    int success;
    char *s;

    if (gss_name_to_string(gss_name, &gss_str) != 0)
        return 0;
    s = k5memdup0(gss_str.value, gss_str.length, &ret);
    if (s == NULL) {
        gss_release_buffer(&minor_stat, &gss_str);
        return 0;
    }
    success = (krb5_parse_name(handle->context, s, princ) == 0);
    free(s);
    gss_release_buffer(&minor_stat, &gss_str);
    return success;
}

static int
gss_name_to_string(gss_name_t gss_name, gss_buffer_desc *str)
{
    OM_uint32 status, minor_stat;
    gss_OID gss_type;
    const char pref[] = KRB5_WELLKNOWN_NAMESTR "/" KRB5_ANONYMOUS_PRINCSTR "@";
    const size_t preflen = sizeof(pref) - 1;

    status = gss_display_name(&minor_stat, gss_name, str, &gss_type);
    if (status != GSS_S_COMPLETE)
        return 1;
    if (gss_oid_equal(gss_type, GSS_C_NT_ANONYMOUS)) {
        /* Guard against non-krb5 mechs with different anonymous displays. */
        if (str->length < preflen || memcmp(str->value, pref, preflen) != 0)
            return 1;
    } else if (!gss_oid_equal(gss_type, GSS_KRB5_NT_PRINCIPAL_NAME)) {
        return 1;
    }
    return 0;
}

/*
 * Perform common initialization for server stub functions.  A subset of the
 * output arguments may be set on failure; the caller is responsible for
 * initializing outputs and calling stub_cleanup() on success or failure.
 * princ and princ_str_out may be NULL to omit unparsing a principal name.
 */
static kadm5_ret_t
stub_setup(krb5_ui_4 api_version, struct svc_req *rqstp, krb5_principal princ,
           kadm5_server_handle_t *handle_out, krb5_ui_4 *api_version_out,
           gss_buffer_t client_name_out, gss_buffer_t service_name_out,
           char **princ_str_out)
{
    kadm5_ret_t ret;

    ret = new_server_handle(api_version, rqstp, handle_out);
    if (ret)
        return ret;

    ret = check_handle(*handle_out);
    if (ret)
        return ret;

    *api_version_out = (*handle_out)->api_version;

    if (setup_gss_names(rqstp, client_name_out, service_name_out) < 0)
        return KADM5_FAILURE;

    if (princ_str_out != NULL) {
        if (princ == NULL)
            return KADM5_BAD_PRINCIPAL;
        if (krb5_unparse_name((*handle_out)->context, princ, princ_str_out))
            return KADM5_BAD_PRINCIPAL;
    }

    return KADM5_OK;
}

/* Perform common cleanup for server stub functions. */
static void
stub_cleanup(kadm5_server_handle_t handle, char *princ_str,
             gss_buffer_t client_name, gss_buffer_t service_name)
{
    OM_uint32 minor_stat;

    auth_end(handle->context);
    free_server_handle(handle);
    free(princ_str);
    gss_release_buffer(&minor_stat, client_name);
    gss_release_buffer(&minor_stat, service_name);
}

static krb5_boolean
stub_auth(kadm5_server_handle_t handle, int opcode, krb5_const_principal p1,
          krb5_const_principal p2, const char *s1, const char *s2)
{
    return auth(handle->context, opcode, handle->current_caller, p1, p2,
                s1, s2, NULL, 0);
}

static krb5_boolean
stub_auth_pol(kadm5_server_handle_t handle, int opcode, const char *policy,
              const kadm5_policy_ent_rec *polent, long mask)
{
    return auth(handle->context, opcode, handle->current_caller, NULL, NULL,
                policy, NULL, polent, mask);
}

static krb5_boolean
stub_auth_restrict(kadm5_server_handle_t handle, int opcode,
                   kadm5_principal_ent_t ent, long *mask)
{
    return auth_restrict(handle->context, opcode, handle->current_caller,
                         ent, mask);
}

/* Return true if the client authenticated to kadmin/changepw and princ is not
 * the client principal. */
static krb5_boolean
changepw_not_self(kadm5_server_handle_t handle, struct svc_req *rqstp,
                  krb5_const_principal princ)
{
    return CHANGEPW_SERVICE(rqstp) &&
        !krb5_principal_compare(handle->context, handle->current_caller,
                                princ);
}

static krb5_boolean
ticket_is_initial(struct svc_req *rqstp)
{
    OM_uint32 status, minor_stat;
    krb5_flags flags;

    status = gss_krb5_get_tkt_flags(&minor_stat, rqstp->rq_svccred, &flags);
    if (status != GSS_S_COMPLETE)
        return 0;
    return (flags & TKT_FLG_INITIAL) != 0;
}

/* If a key change request is for the client's own principal, verify that the
 * client used an initial ticket and enforce the policy min_life. */
static kadm5_ret_t
check_self_keychange(kadm5_server_handle_t handle, struct svc_req *rqstp,
                     krb5_principal princ)
{
    if (!krb5_principal_compare(handle->context, handle->current_caller,
                                princ))
        return 0;

    if (!ticket_is_initial(rqstp))
        return KADM5_AUTH_INITIAL;

    return check_min_life(handle, princ, NULL, 0);
}

static int
log_unauth(
    char *op,
    char *target,
    gss_buffer_t client,
    gss_buffer_t server,
    struct svc_req *rqstp)
{
    size_t tlen, clen, slen;
    char *tdots, *cdots, *sdots;

    tlen = strlen(target);
    trunc_name(&tlen, &tdots);
    clen = client->length;
    trunc_name(&clen, &cdots);
    slen = server->length;
    trunc_name(&slen, &sdots);

    /* okay to cast lengths to int because trunc_name limits max value */
    return krb5_klog_syslog(LOG_NOTICE,
                            _("Unauthorized request: %s, %.*s%s, "
                              "client=%.*s%s, service=%.*s%s, addr=%s"),
                            op, (int)tlen, target, tdots,
                            (int)clen, (char *)client->value, cdots,
                            (int)slen, (char *)server->value, sdots,
                            client_addr(rqstp->rq_xprt));
}

static int
log_done(
    char *op,
    char *target,
    const char *errmsg,
    gss_buffer_t client,
    gss_buffer_t server,
    struct svc_req *rqstp)
{
    size_t tlen, clen, slen;
    char *tdots, *cdots, *sdots;

    if (errmsg == NULL)
        errmsg = _("success");
    tlen = strlen(target);
    trunc_name(&tlen, &tdots);
    clen = client->length;
    trunc_name(&clen, &cdots);
    slen = server->length;
    trunc_name(&slen, &sdots);

    /* okay to cast lengths to int because trunc_name limits max value */
    return krb5_klog_syslog(LOG_NOTICE,
                            _("Request: %s, %.*s%s, %s, "
                              "client=%.*s%s, service=%.*s%s, addr=%s"),
                            op, (int)tlen, target, tdots, errmsg,
                            (int)clen, (char *)client->value, cdots,
                            (int)slen, (char *)server->value, sdots,
                            client_addr(rqstp->rq_xprt));
}

bool_t
create_principal_2_svc(cprinc_arg *arg, generic_ret *ret,
                       struct svc_req *rqstp)
{
    char                        *prime_arg = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t       handle;
    const char                  *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->rec.principal,
                           &handle, &ret->api_version, &client_name,
                           &service_name, &prime_arg);
    if (ret->code)
        goto exit_func;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth_restrict(handle, OP_ADDPRINC, &arg->rec, &arg->mask)) {
        ret->code = KADM5_AUTH_ADD;
        log_unauth("kadm5_create_principal", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_create_principal(handle, &arg->rec, arg->mask,
                                           arg->passwd);

        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_create_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
create_principal3_2_svc(cprinc3_arg *arg, generic_ret *ret,
                        struct svc_req *rqstp)
{
    char                        *prime_arg = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t       handle;
    const char                  *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->rec.principal,
                           &handle, &ret->api_version, &client_name,
                           &service_name, &prime_arg);
    if (ret->code)
        goto exit_func;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth_restrict(handle, OP_ADDPRINC, &arg->rec, &arg->mask)) {
        ret->code = KADM5_AUTH_ADD;
        log_unauth("kadm5_create_principal", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_create_principal_3(handle, &arg->rec, arg->mask,
                                             arg->n_ks_tuple, arg->ks_tuple,
                                             arg->passwd);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_create_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

/* Return KADM5_PROTECT_KEYS if KRB5_KDB_LOCKDOWN_KEYS is set for princ. */
static kadm5_ret_t
check_lockdown_keys(kadm5_server_handle_t handle, krb5_principal princ)
{
    kadm5_principal_ent_rec rec;
    kadm5_ret_t ret;

    ret = kadm5_get_principal(handle, princ, &rec, KADM5_ATTRIBUTES);
    if (ret)
        return ret;
    ret = (rec.attributes & KRB5_KDB_LOCKDOWN_KEYS) ? KADM5_PROTECT_KEYS : 0;
    kadm5_free_principal_ent(handle, &rec);
    return ret;
}

bool_t
delete_principal_2_svc(dprinc_arg *arg, generic_ret *ret,
                       struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_DELPRINC, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_DELETE;
        log_unauth("kadm5_delete_principal", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = check_lockdown_keys(handle, arg->princ);
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_delete_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_DELETE;
        }
    }

    if (ret->code == KADM5_OK)
        ret->code = kadm5_delete_principal(handle, arg->princ);
    if (ret->code != KADM5_AUTH_DELETE) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_delete_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);

    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
modify_principal_2_svc(mprinc_arg *arg, generic_ret *ret,
                       struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->rec.principal,
                           &handle, &ret->api_version, &client_name,
                           &service_name, &prime_arg);
    if (ret->code)
        goto exit_func;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth_restrict(handle, OP_MODPRINC, &arg->rec, &arg->mask)) {
        ret->code = KADM5_AUTH_MODIFY;
        log_unauth("kadm5_modify_principal", prime_arg,
                   &client_name, &service_name, rqstp);
    } else if ((arg->mask & KADM5_ATTRIBUTES) &&
               (!(arg->rec.attributes & KRB5_KDB_LOCKDOWN_KEYS))) {
        ret->code = check_lockdown_keys(handle, arg->rec.principal);
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_modify_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_MODIFY;
        }
    }

    if (ret->code == KADM5_OK) {
        ret->code = kadm5_modify_principal(handle, &arg->rec, arg->mask);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_modify_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
rename_principal_2_svc(rprinc_arg *arg, generic_ret *ret,
                       struct svc_req *rqstp)
{
    char                        *prime_arg1 = NULL, *prime_arg2 = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t       handle;
    const char                  *errmsg = NULL;
    size_t                      tlen1, tlen2, clen, slen;
    char                        *tdots1, *tdots2, *cdots, *sdots;

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    if (krb5_unparse_name(handle->context, arg->src, &prime_arg1) ||
        krb5_unparse_name(handle->context, arg->dest, &prime_arg2)) {
        ret->code = KADM5_BAD_PRINCIPAL;
        goto exit_func;
    }
    tlen1 = strlen(prime_arg1);
    trunc_name(&tlen1, &tdots1);
    tlen2 = strlen(prime_arg2);
    trunc_name(&tlen2, &tdots2);
    clen = client_name.length;
    trunc_name(&clen, &cdots);
    slen = service_name.length;
    trunc_name(&slen, &sdots);

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_RENPRINC, arg->src, arg->dest, NULL, NULL)) {
        ret->code = KADM5_AUTH_INSUFFICIENT;
        log_unauth("kadm5_rename_principal", prime_arg1, &client_name,
                   &service_name, rqstp);
    } else {
        ret->code = check_lockdown_keys(handle, arg->src);
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_rename_principal", prime_arg1, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_DELETE;
        }
    }
    if (ret->code != KADM5_OK) {
        /* okay to cast lengths to int because trunc_name limits max value */
        krb5_klog_syslog(LOG_NOTICE,
                         _("Unauthorized request: kadm5_rename_principal, "
                           "%.*s%s to %.*s%s, "
                           "client=%.*s%s, service=%.*s%s, addr=%s"),
                         (int)tlen1, prime_arg1, tdots1,
                         (int)tlen2, prime_arg2, tdots2,
                         (int)clen, (char *)client_name.value, cdots,
                         (int)slen, (char *)service_name.value, sdots,
                         client_addr(rqstp->rq_xprt));
    } else {
        ret->code = kadm5_rename_principal(handle, arg->src, arg->dest);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        /* okay to cast lengths to int because trunc_name limits max value */
        krb5_klog_syslog(LOG_NOTICE,
                         _("Request: kadm5_rename_principal, "
                           "%.*s%s to %.*s%s, %s, "
                           "client=%.*s%s, service=%.*s%s, addr=%s"),
                         (int)tlen1, prime_arg1, tdots1,
                         (int)tlen2, prime_arg2, tdots2,
                         errmsg ? errmsg : _("success"),
                         (int)clen, (char *)client_name.value, cdots,
                         (int)slen, (char *)service_name.value, sdots,
                         client_addr(rqstp->rq_xprt));

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);

    }
exit_func:
    free(prime_arg1);
    free(prime_arg2);
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
get_principal_2_svc(gprinc_arg *arg, gprinc_ret *ret, struct svc_req *rqstp)
{
    char                            *funcname, *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    funcname = "kadm5_get_principal";

    if (changepw_not_self(handle, rqstp, arg->princ) ||
        !stub_auth(handle, OP_GETPRINC, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_GET;
        log_unauth(funcname, prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_get_principal(handle, arg->princ, &ret->rec,
                                        arg->mask);

        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done(funcname, prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
get_princs_2_svc(gprincs_arg *arg, gprincs_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    prime_arg = arg->exp;
    if (prime_arg == NULL)
        prime_arg = "*";

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_LISTPRINCS, NULL, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_LIST;
        log_unauth("kadm5_get_principals", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_get_principals(handle, arg->exp, &ret->princs,
                                         &ret->count);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_get_principals", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);

    }

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
chpass_principal_2_svc(chpass_arg *arg, generic_ret *ret,
                       struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    ret->code = check_lockdown_keys(handle, arg->princ);
    if (ret->code != KADM5_OK) {
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_chpass_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_CHANGEPW;
        }
    } else if (changepw_not_self(handle, rqstp, arg->princ) ||
               !stub_auth(handle, OP_CPW, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_CHANGEPW;
        log_unauth("kadm5_chpass_principal", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = check_self_keychange(handle, rqstp, arg->princ);
        if (!ret->code)
            ret->code = kadm5_chpass_principal(handle, arg->princ, arg->pass);
    }

    if (ret->code != KADM5_AUTH_CHANGEPW) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_chpass_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
chpass_principal3_2_svc(chpass3_arg *arg, generic_ret *ret,
                        struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    ret->code = check_lockdown_keys(handle, arg->princ);
    if (ret->code != KADM5_OK) {
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_chpass_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_CHANGEPW;
        }
    } else if (changepw_not_self(handle, rqstp, arg->princ) ||
               !stub_auth(handle, OP_CPW, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_CHANGEPW;
        log_unauth("kadm5_chpass_principal", prime_arg,
                   &client_name, &service_name, rqstp);
    } else  {
        ret->code = check_self_keychange(handle, rqstp, arg->princ);
        if (!ret->code) {
            ret->code = kadm5_chpass_principal_3(handle, arg->princ,
                                                 arg->keepold, arg->n_ks_tuple,
                                                 arg->ks_tuple, arg->pass);
        }
    }

    if (ret->code != KADM5_AUTH_CHANGEPW) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_chpass_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
setkey_principal_2_svc(setkey_arg *arg, generic_ret *ret,
                       struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    ret->code = check_lockdown_keys(handle, arg->princ);
    if (ret->code != KADM5_OK) {
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_setkey_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_SETKEY;
        }
    } else if (!(CHANGEPW_SERVICE(rqstp)) &&
               stub_auth(handle, OP_SETKEY, arg->princ, NULL, NULL, NULL)) {
        ret->code = kadm5_setkey_principal(handle, arg->princ, arg->keyblocks,
                                           arg->n_keys);
    } else {
        log_unauth("kadm5_setkey_principal", prime_arg,
                   &client_name, &service_name, rqstp);
        ret->code = KADM5_AUTH_SETKEY;
    }

    if (ret->code != KADM5_AUTH_SETKEY) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_setkey_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
setkey_principal3_2_svc(setkey3_arg *arg, generic_ret *ret,
                        struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    ret->code = check_lockdown_keys(handle, arg->princ);
    if (ret->code != KADM5_OK) {
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_setkey_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_SETKEY;
        }
    } else if (!(CHANGEPW_SERVICE(rqstp)) &&
               stub_auth(handle, OP_SETKEY, arg->princ, NULL, NULL, NULL)) {
        ret->code = kadm5_setkey_principal_3(handle, arg->princ, arg->keepold,
                                             arg->n_ks_tuple, arg->ks_tuple,
                                             arg->keyblocks, arg->n_keys);
    } else {
        log_unauth("kadm5_setkey_principal", prime_arg,
                   &client_name, &service_name, rqstp);
        ret->code = KADM5_AUTH_SETKEY;
    }

    if (ret->code != KADM5_AUTH_SETKEY) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_setkey_principal", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
setkey_principal4_2_svc(setkey4_arg *arg, generic_ret *ret,
                        struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    ret->code = check_lockdown_keys(handle, arg->princ);
    if (ret->code != KADM5_OK) {
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_setkey_principal", prime_arg, &client_name,
                       &service_name, rqstp);
            ret->code = KADM5_AUTH_SETKEY;
        }
    } else if (!(CHANGEPW_SERVICE(rqstp)) &&
               stub_auth(handle, OP_SETKEY, arg->princ, NULL, NULL, NULL)) {
        ret->code = kadm5_setkey_principal_4(handle, arg->princ, arg->keepold,
                                             arg->key_data, arg->n_key_data);
    } else {
        log_unauth("kadm5_setkey_principal", prime_arg, &client_name,
                   &service_name, rqstp);
        ret->code = KADM5_AUTH_SETKEY;
    }

    if (ret->code != KADM5_AUTH_SETKEY) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_setkey_principal", prime_arg, errmsg, &client_name,
                 &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

/* Empty out *keys / *nkeys if princ is protected with the lockdown
 * attribute, or if we fail to check. */
static kadm5_ret_t
chrand_check_lockdown(kadm5_server_handle_t handle, krb5_principal princ,
                      krb5_keyblock **keys, int *nkeys)
{
    kadm5_ret_t ret;
    int i;

    ret = check_lockdown_keys(handle, princ);
    if (!ret)
        return 0;

    for (i = 0; i < *nkeys; i++)
        krb5_free_keyblock_contents(handle->context, &((*keys)[i]));
    free(*keys);
    *keys = NULL;
    *nkeys = 0;
    return (ret == KADM5_PROTECT_KEYS) ? KADM5_OK : ret;
}

bool_t
chrand_principal_2_svc(chrand_arg *arg, chrand_ret *ret, struct svc_req *rqstp)
{
    char                        *funcname, *prime_arg = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    krb5_keyblock               *k;
    int                         nkeys;
    kadm5_server_handle_t       handle;
    const char                  *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    funcname = "kadm5_randkey_principal";

    if (changepw_not_self(handle, rqstp, arg->princ) ||
        !stub_auth(handle, OP_CHRAND, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_CHANGEPW;
        log_unauth(funcname, prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = check_self_keychange(handle, rqstp, arg->princ);
        if (!ret->code) {
            ret->code = kadm5_randkey_principal(handle, arg->princ,
                                                &k, &nkeys);
        }
    }

    if (ret->code == KADM5_OK) {
        ret->code = chrand_check_lockdown(handle, arg->princ, &k, &nkeys);
        if (ret->code == KADM5_PROTECT_KEYS)
            ret->code = KADM5_OK;
        ret->keys = k;
        ret->n_keys = nkeys;
    }

    if (ret->code != KADM5_AUTH_CHANGEPW) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done(funcname, prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
chrand_principal3_2_svc(chrand3_arg *arg, chrand_ret *ret,
                        struct svc_req *rqstp)
{
    char                        *funcname, *prime_arg = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    krb5_keyblock               *k;
    int                         nkeys;
    kadm5_server_handle_t       handle;
    const char                  *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    funcname = "kadm5_randkey_principal";

    if (changepw_not_self(handle, rqstp, arg->princ) ||
        !stub_auth(handle, OP_CHRAND, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_CHANGEPW;
        log_unauth(funcname, prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = check_self_keychange(handle, rqstp, arg->princ);
        if (!ret->code) {
            ret->code = kadm5_randkey_principal_3(handle, arg->princ,
                                                  arg->keepold,
                                                  arg->n_ks_tuple,
                                                  arg->ks_tuple, &k, &nkeys);
        }
    }

    if (ret->code == KADM5_OK) {
        ret->code = chrand_check_lockdown(handle, arg->princ, &k, &nkeys);
        if (ret->code == KADM5_PROTECT_KEYS)
            ret->code = KADM5_OK;
        ret->keys = k;
        ret->n_keys = nkeys;
    }

    if (ret->code != KADM5_AUTH_CHANGEPW) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done(funcname, prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
create_policy_2_svc(cpol_arg *arg, generic_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    prime_arg = arg->rec.policy;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth_pol(handle, OP_ADDPOL, arg->rec.policy,
                       &arg->rec, arg->mask)) {
        ret->code = KADM5_AUTH_ADD;
        log_unauth("kadm5_create_policy", prime_arg,
                   &client_name, &service_name, rqstp);

    } else {
        ret->code = kadm5_create_policy(handle, &arg->rec, arg->mask);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_create_policy",
                 ((prime_arg == NULL) ? "(null)" : prime_arg), errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
delete_policy_2_svc(dpol_arg *arg, generic_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    prime_arg = arg->name;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_DELPOL, NULL, NULL, arg->name, NULL)) {
        log_unauth("kadm5_delete_policy", prime_arg,
                   &client_name, &service_name, rqstp);
        ret->code = KADM5_AUTH_DELETE;
    } else {
        ret->code = kadm5_delete_policy(handle, arg->name);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_delete_policy",
                 ((prime_arg == NULL) ? "(null)" : prime_arg), errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
modify_policy_2_svc(mpol_arg *arg, generic_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    prime_arg = arg->rec.policy;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth_pol(handle, OP_MODPOL, arg->rec.policy,
                       &arg->rec, arg->mask)) {
        log_unauth("kadm5_modify_policy", prime_arg,
                   &client_name, &service_name, rqstp);
        ret->code = KADM5_AUTH_MODIFY;
    } else {
        ret->code = kadm5_modify_policy(handle, &arg->rec, arg->mask);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_modify_policy",
                 ((prime_arg == NULL) ? "(null)" : prime_arg), errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
get_policy_2_svc(gpol_arg *arg, gpol_ret *ret, struct svc_req *rqstp)
{
    char                        *funcname, *prime_arg = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    kadm5_ret_t         ret2;
    kadm5_principal_ent_rec     caller_ent;
    kadm5_server_handle_t       handle;
    const char                  *errmsg = NULL, *cpolicy = NULL;

    memset(&caller_ent, 0, sizeof(caller_ent));

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    funcname = "kadm5_get_policy";

    prime_arg = arg->name;

    /* Look up the client principal's policy value. */
    ret2 = kadm5_get_principal(handle->lhandle, handle->current_caller,
                               &caller_ent, KADM5_PRINCIPAL_NORMAL_MASK);
    if (ret2 == KADM5_OK && (caller_ent.aux_attributes & KADM5_POLICY))
        cpolicy = caller_ent.policy;

    ret->code = KADM5_AUTH_GET;
    if ((CHANGEPW_SERVICE(rqstp) &&
         (cpolicy == NULL || strcmp(cpolicy, arg->name) != 0)) ||
        !stub_auth(handle, OP_GETPOL, NULL, NULL, arg->name, cpolicy)) {
        ret->code = KADM5_AUTH_GET;
        log_unauth(funcname, prime_arg, &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_get_policy(handle, arg->name, &ret->rec);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done(funcname,
                 ((prime_arg == NULL) ? "(null)" : prime_arg), errmsg,
                 &client_name, &service_name, rqstp);
        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    (void)kadm5_free_principal_ent(handle->lhandle, &caller_ent);
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
get_pols_2_svc(gpols_arg *arg, gpols_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, NULL, &handle,
                           &ret->api_version, &client_name, &service_name,
                           NULL);
    if (ret->code)
        goto exit_func;

    prime_arg = arg->exp;
    if (prime_arg == NULL)
        prime_arg = "*";

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_LISTPOLS, NULL, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_LIST;
        log_unauth("kadm5_get_policies", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_get_policies(handle, arg->exp, &ret->pols,
                                       &ret->count);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_get_policies", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
get_privs_2_svc(krb5_ui_4 *arg, getprivs_ret *ret, struct svc_req *rqstp)
{
    gss_buffer_desc                client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t          handle;
    const char                     *errmsg = NULL;

    ret->code = stub_setup(*arg, rqstp, NULL, &handle, &ret->api_version,
                           &client_name, &service_name, NULL);
    if (ret->code)
        goto exit_func;

    ret->code = kadm5_get_privs(handle, &ret->privs);
    if (ret->code != 0)
        errmsg = krb5_get_error_message(handle->context, ret->code);

    log_done("kadm5_get_privs", client_name.value, errmsg,
             &client_name, &service_name, rqstp);

    if (errmsg != NULL)
        krb5_free_error_message(handle->context, errmsg);

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

bool_t
purgekeys_2_svc(purgekeys_arg *arg, generic_ret *ret, struct svc_req *rqstp)
{
    char                        *funcname, *prime_arg = NULL;
    gss_buffer_desc             client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc             service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t       handle;

    const char                  *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    funcname = "kadm5_purgekeys";

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_PURGEKEYS, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_MODIFY;
        log_unauth(funcname, prime_arg, &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_purgekeys(handle, arg->princ, arg->keepkvno);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done(funcname, prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
get_strings_2_svc(gstrings_arg *arg, gstrings_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_GETSTRS, arg->princ, NULL, NULL, NULL)) {
        ret->code = KADM5_AUTH_GET;
        log_unauth("kadm5_get_strings", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_get_strings(handle, arg->princ, &ret->strings,
                                      &ret->count);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_get_strings", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
set_string_2_svc(sstring_arg *arg, generic_ret *ret, struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    if (CHANGEPW_SERVICE(rqstp) ||
        !stub_auth(handle, OP_SETSTR, arg->princ, NULL,
                   arg->key, arg->value)) {
        ret->code = KADM5_AUTH_MODIFY;
        log_unauth("kadm5_mod_strings", prime_arg,
                   &client_name, &service_name, rqstp);
    } else {
        ret->code = kadm5_set_string(handle, arg->princ, arg->key, arg->value);
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_mod_strings", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}

bool_t
init_2_svc(krb5_ui_4 *arg, generic_ret *ret, struct svc_req *rqstp)
{
    gss_buffer_desc            client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc            service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t      handle;
    const char                 *errmsg = NULL;
    size_t clen, slen;
    char *cdots, *sdots;

    ret->code = stub_setup(*arg, rqstp, NULL, &handle, &ret->api_version,
                           &client_name, &service_name, NULL);
    if (ret->code)
        goto exit_func;

    if (ret->code != 0)
        errmsg = krb5_get_error_message(handle->context, ret->code);

    clen = client_name.length;
    trunc_name(&clen, &cdots);
    slen = service_name.length;
    trunc_name(&slen, &sdots);
    /* okay to cast lengths to int because trunc_name limits max value */
    krb5_klog_syslog(LOG_NOTICE, _("Request: kadm5_init, %.*s%s, %s, "
                                   "client=%.*s%s, service=%.*s%s, addr=%s, "
                                   "vers=%d, flavor=%d"),
                     (int)clen, (char *)client_name.value, cdots,
                     errmsg ? errmsg : _("success"),
                     (int)clen, (char *)client_name.value, cdots,
                     (int)slen, (char *)service_name.value, sdots,
                     client_addr(rqstp->rq_xprt),
                     ret->api_version & ~(KADM5_API_VERSION_MASK),
                     rqstp->rq_cred.oa_flavor);
    if (errmsg != NULL)
        krb5_free_error_message(handle->context, errmsg);

exit_func:
    stub_cleanup(handle, NULL, &client_name, &service_name);
    return TRUE;
}

gss_name_t
rqst2name(struct svc_req *rqstp)
{

    if (rqstp->rq_cred.oa_flavor == RPCSEC_GSS)
        return rqstp->rq_clntname;
    else
        return rqstp->rq_clntcred;
}

bool_t
get_principal_keys_2_svc(getpkeys_arg *arg, getpkeys_ret *ret,
                         struct svc_req *rqstp)
{
    char                            *prime_arg = NULL;
    gss_buffer_desc                 client_name = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc                 service_name = GSS_C_EMPTY_BUFFER;
    kadm5_server_handle_t           handle;
    const char                      *errmsg = NULL;

    ret->code = stub_setup(arg->api_version, rqstp, arg->princ, &handle,
                           &ret->api_version, &client_name, &service_name,
                           &prime_arg);
    if (ret->code)
        goto exit_func;

    if (!(CHANGEPW_SERVICE(rqstp)) &&
        stub_auth(handle, OP_EXTRACT, arg->princ, NULL, NULL, NULL)) {
        ret->code = kadm5_get_principal_keys(handle, arg->princ, arg->kvno,
                                             &ret->key_data, &ret->n_key_data);
    } else {
        log_unauth("kadm5_get_principal_keys", prime_arg,
                   &client_name, &service_name, rqstp);
        ret->code = KADM5_AUTH_EXTRACT;
    }

    if (ret->code == KADM5_OK) {
        ret->code = check_lockdown_keys(handle, arg->princ);
        if (ret->code != KADM5_OK) {
            kadm5_free_kadm5_key_data(handle->context, ret->n_key_data,
                                      ret->key_data);
            ret->key_data = NULL;
            ret->n_key_data = 0;
        }
        if (ret->code == KADM5_PROTECT_KEYS) {
            log_unauth("kadm5_get_principal_keys", prime_arg,
                       &client_name, &service_name, rqstp);
            ret->code = KADM5_AUTH_EXTRACT;
        }
    }

    if (ret->code != KADM5_AUTH_EXTRACT) {
        if (ret->code != 0)
            errmsg = krb5_get_error_message(handle->context, ret->code);

        log_done("kadm5_get_principal_keys", prime_arg, errmsg,
                 &client_name, &service_name, rqstp);

        if (errmsg != NULL)
            krb5_free_error_message(handle->context, errmsg);
    }

exit_func:
    stub_cleanup(handle, prime_arg, &client_name, &service_name);
    return TRUE;
}
