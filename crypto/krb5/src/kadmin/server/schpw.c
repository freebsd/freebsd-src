/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-int.h"
#include <kadm5/admin.h>
#include <syslog.h>
#include <adm_proto.h>  /* krb5_klog_syslog */
#include <stdio.h>
#include <errno.h>

#include "kadm5/server_internal.h" /* XXX for kadm5_server_handle_t */

#include "misc.h"

#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE int
#endif

#define RFC3244_VERSION 0xff80

static krb5_error_code
process_chpw_request(krb5_context context, void *server_handle, char *realm,
                     krb5_keytab keytab, const krb5_fulladdr *local_addr,
                     const krb5_fulladdr *remote_addr, krb5_data *req,
                     krb5_data *rep)
{
    krb5_error_code ret;
    char *ptr;
    unsigned int plen, vno;
    krb5_data ap_req, ap_rep = empty_data();
    krb5_data cipher = empty_data(), clear = empty_data();
    krb5_auth_context auth_context = NULL;
    krb5_principal changepw = NULL;
    krb5_principal client, target = NULL;
    krb5_ticket *ticket = NULL;
    krb5_replay_data replay;
    krb5_error krberror;
    int numresult;
    char strresult[1024];
    char *clientstr = NULL, *targetstr = NULL;
    const char *errmsg = NULL;
    size_t clen;
    char *cdots;
    struct sockaddr_storage ss;
    socklen_t salen;
    char addrbuf[100];
    krb5_address *addr = remote_addr->address;

    *rep = empty_data();

    if (req->length < 4) {
        /* either this, or the server is printing bad messages,
           or the caller passed in garbage */
        ret = KRB5KRB_AP_ERR_MODIFIED;
        numresult = KRB5_KPASSWD_MALFORMED;
        strlcpy(strresult, "Request was truncated", sizeof(strresult));
        goto bailout;
    }

    ptr = req->data;

    /* verify length */

    plen = (*ptr++ & 0xff);
    plen = (plen<<8) | (*ptr++ & 0xff);

    if (plen != req->length) {
        ret = KRB5KRB_AP_ERR_MODIFIED;
        numresult = KRB5_KPASSWD_MALFORMED;
        strlcpy(strresult, "Request length was inconsistent",
                sizeof(strresult));
        goto bailout;
    }

    /* verify version number */

    vno = (*ptr++ & 0xff) ;
    vno = (vno<<8) | (*ptr++ & 0xff);

    if (vno != 1 && vno != RFC3244_VERSION) {
        ret = KRB5KDC_ERR_BAD_PVNO;
        numresult = KRB5_KPASSWD_BAD_VERSION;
        snprintf(strresult, sizeof(strresult),
                 "Request contained unknown protocol version number %d", vno);
        goto bailout;
    }

    /* read, check ap-req length */

    ap_req.length = (*ptr++ & 0xff);
    ap_req.length = (ap_req.length<<8) | (*ptr++ & 0xff);

    if (ptr + ap_req.length >= req->data + req->length) {
        ret = KRB5KRB_AP_ERR_MODIFIED;
        numresult = KRB5_KPASSWD_MALFORMED;
        strlcpy(strresult, "Request was truncated in AP-REQ",
                sizeof(strresult));
        goto bailout;
    }

    /* verify ap_req */

    ap_req.data = ptr;
    ptr += ap_req.length;

    ret = krb5_auth_con_init(context, &auth_context);
    if (ret) {
        numresult = KRB5_KPASSWD_HARDERROR;
        strlcpy(strresult, "Failed initializing auth context",
                sizeof(strresult));
        goto chpwfail;
    }

    ret = krb5_auth_con_setflags(context, auth_context,
                                 KRB5_AUTH_CONTEXT_DO_SEQUENCE);
    if (ret) {
        numresult = KRB5_KPASSWD_HARDERROR;
        strlcpy(strresult, "Failed initializing auth context",
                sizeof(strresult));
        goto chpwfail;
    }

    ret = krb5_build_principal(context, &changepw, strlen(realm), realm,
                               "kadmin", "changepw", NULL);
    if (ret) {
        numresult = KRB5_KPASSWD_HARDERROR;
        strlcpy(strresult, "Failed building kadmin/changepw principal",
                sizeof(strresult));
        goto chpwfail;
    }

    ret = krb5_rd_req(context, &auth_context, &ap_req, changepw, keytab,
                      NULL, &ticket);

    if (ret) {
        numresult = KRB5_KPASSWD_AUTHERROR;
        strlcpy(strresult, "Failed reading application request",
                sizeof(strresult));
        goto chpwfail;
    }

    /* construct the ap-rep */

    ret = krb5_mk_rep(context, auth_context, &ap_rep);
    if (ret) {
        numresult = KRB5_KPASSWD_AUTHERROR;
        strlcpy(strresult, "Failed replying to application request",
                sizeof(strresult));
        goto chpwfail;
    }

    /* decrypt the ChangePasswdData */

    cipher.length = (req->data + req->length) - ptr;
    cipher.data = ptr;

    /*
     * Don't set a remote address in auth_context before calling krb5_rd_priv,
     * so that we can work against clients behind a NAT.  Reflection attacks
     * aren't a concern since we use sequence numbers and since our requests
     * don't look anything like our responses.  Also don't set a local address,
     * since we don't know what interface the request was received on.
     */

    ret = krb5_rd_priv(context, auth_context, &cipher, &clear, &replay);
    if (ret) {
        numresult = KRB5_KPASSWD_HARDERROR;
        strlcpy(strresult, "Failed decrypting request", sizeof(strresult));
        goto chpwfail;
    }

    client = ticket->enc_part2->client;

    /* decode ChangePasswdData for setpw requests */
    if (vno == RFC3244_VERSION) {
        krb5_data *clear_data;

        ret = decode_krb5_setpw_req(&clear, &clear_data, &target);
        if (ret != 0) {
            numresult = KRB5_KPASSWD_MALFORMED;
            strlcpy(strresult, "Failed decoding ChangePasswdData",
                    sizeof(strresult));
            goto chpwfail;
        }

        zapfree(clear.data, clear.length);

        clear = *clear_data;
        free(clear_data);

        if (target != NULL) {
            ret = krb5_unparse_name(context, target, &targetstr);
            if (ret != 0) {
                numresult = KRB5_KPASSWD_HARDERROR;
                strlcpy(strresult, "Failed unparsing target name for log",
                        sizeof(strresult));
                goto chpwfail;
            }
        }
    }

    ret = krb5_unparse_name(context, client, &clientstr);
    if (ret) {
        numresult = KRB5_KPASSWD_HARDERROR;
        strlcpy(strresult, "Failed unparsing client name for log",
                sizeof(strresult));
        goto chpwfail;
    }

    /* change the password */

    ptr = k5memdup0(clear.data, clear.length, &ret);
    ret = schpw_util_wrapper(server_handle, client, target,
                             (ticket->enc_part2->flags & TKT_FLG_INITIAL) != 0,
                             ptr, NULL, strresult, sizeof(strresult));
    if (ret)
        errmsg = krb5_get_error_message(context, ret);

    /* zap the password */
    zapfree(clear.data, clear.length);
    zapfree(ptr, clear.length);
    clear = empty_data();

    clen = strlen(clientstr);
    trunc_name(&clen, &cdots);

    switch (addr->addrtype) {
    case ADDRTYPE_INET: {
        struct sockaddr_in *sin = ss2sin(&ss);

        sin->sin_family = AF_INET;
        memcpy(&sin->sin_addr, addr->contents, addr->length);
        sin->sin_port = htons(remote_addr->port);
        salen = sizeof(*sin);
        break;
    }
    case ADDRTYPE_INET6: {
        struct sockaddr_in6 *sin6 = ss2sin6(&ss);

        sin6->sin6_family = AF_INET6;
        memcpy(&sin6->sin6_addr, addr->contents, addr->length);
        sin6->sin6_port = htons(remote_addr->port);
        salen = sizeof(*sin6);
        break;
    }
    default: {
        struct sockaddr *sa = ss2sa(&ss);

        sa->sa_family = AF_UNSPEC;
        salen = sizeof(*sa);
        break;
    }
    }

    if (getnameinfo(ss2sa(&ss), salen,
                    addrbuf, sizeof(addrbuf), NULL, 0,
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
        strlcpy(addrbuf, "<unprintable>", sizeof(addrbuf));

    if (vno == RFC3244_VERSION) {
        size_t tlen;
        char *tdots;
        const char *targetp;

        if (target == NULL) {
            tlen = clen;
            tdots = cdots;
            targetp = targetstr;
        } else {
            tlen = strlen(targetstr);
            trunc_name(&tlen, &tdots);
            targetp = clientstr;
        }

        krb5_klog_syslog(LOG_NOTICE, _("setpw request from %s by %.*s%s for "
                                       "%.*s%s: %s"), addrbuf, (int) clen,
                         clientstr, cdots, (int) tlen, targetp, tdots,
                         errmsg ? errmsg : "success");
    } else {
        krb5_klog_syslog(LOG_NOTICE, _("chpw request from %s for %.*s%s: %s"),
                         addrbuf, (int) clen, clientstr, cdots,
                         errmsg ? errmsg : "success");
    }
    switch (ret) {
    case KADM5_AUTH_CHANGEPW:
        numresult = KRB5_KPASSWD_ACCESSDENIED;
        break;
    case KADM5_AUTH_INITIAL:
        numresult = KRB5_KPASSWD_INITIAL_FLAG_NEEDED;
        break;
    case KADM5_PASS_Q_TOOSHORT:
    case KADM5_PASS_REUSE:
    case KADM5_PASS_Q_CLASS:
    case KADM5_PASS_Q_DICT:
    case KADM5_PASS_Q_GENERIC:
    case KADM5_PASS_TOOSOON:
        numresult = KRB5_KPASSWD_SOFTERROR;
        break;
    case 0:
        numresult = KRB5_KPASSWD_SUCCESS;
        strlcpy(strresult, "", sizeof(strresult));
        break;
    default:
        numresult = KRB5_KPASSWD_HARDERROR;
        break;
    }

chpwfail:

    ret = alloc_data(&clear, 2 + strlen(strresult));
    if (ret)
        goto bailout;

    ptr = clear.data;

    *ptr++ = (numresult>>8) & 0xff;
    *ptr++ = numresult & 0xff;

    memcpy(ptr, strresult, strlen(strresult));

    cipher = empty_data();

    if (ap_rep.length) {
        ret = krb5_auth_con_setaddrs(context, auth_context,
                                     local_addr->address, NULL);
        if (ret) {
            numresult = KRB5_KPASSWD_HARDERROR;
            strlcpy(strresult,
                    "Failed storing client and server internet addresses",
                    sizeof(strresult));
        } else {
            ret = krb5_mk_priv(context, auth_context, &clear, &cipher,
                               &replay);
            if (ret) {
                numresult = KRB5_KPASSWD_HARDERROR;
                strlcpy(strresult, "Failed encrypting reply",
                        sizeof(strresult));
            }
        }
    }

    /* if no KRB-PRIV was constructed, then we need a KRB-ERROR.
       if this fails, just bail.  there's nothing else we can do. */

    if (cipher.length == 0) {
        /* clear out ap_rep now, so that it won't be inserted in the
           reply */

        if (ap_rep.length) {
            free(ap_rep.data);
            ap_rep = empty_data();
        }

        krberror.ctime = 0;
        krberror.cusec = 0;
        krberror.susec = 0;
        ret = krb5_timeofday(context, &krberror.stime);
        if (ret)
            goto bailout;

        /* this is really icky.  but it's what all the other callers
           to mk_error do. */
        krberror.error = ret;
        krberror.error -= ERROR_TABLE_BASE_krb5;
        if (krberror.error > KRB_ERR_MAX)
            krberror.error = KRB_ERR_GENERIC;

        krberror.client = NULL;

        ret = krb5_build_principal(context, &krberror.server,
                                   strlen(realm), realm,
                                   "kadmin", "changepw", NULL);
        if (ret)
            goto bailout;
        krberror.text.length = 0;
        krberror.e_data = clear;

        ret = krb5_mk_error(context, &krberror, &cipher);

        krb5_free_principal(context, krberror.server);

        if (ret)
            goto bailout;
    }

    /* construct the reply */

    ret = alloc_data(rep, 6 + ap_rep.length + cipher.length);
    if (ret)
        goto bailout;
    ptr = rep->data;

    /* length */

    *ptr++ = (rep->length>>8) & 0xff;
    *ptr++ = rep->length & 0xff;

    /* version == 0x0001 big-endian */

    *ptr++ = 0;
    *ptr++ = 1;

    /* ap_rep length, big-endian */

    *ptr++ = (ap_rep.length>>8) & 0xff;
    *ptr++ = ap_rep.length & 0xff;

    /* ap-rep data */

    if (ap_rep.length) {
        memcpy(ptr, ap_rep.data, ap_rep.length);
        ptr += ap_rep.length;
    }

    /* krb-priv or krb-error */

    memcpy(ptr, cipher.data, cipher.length);

bailout:
    krb5_auth_con_free(context, auth_context);
    krb5_free_principal(context, changepw);
    krb5_free_ticket(context, ticket);
    free(ap_rep.data);
    free(clear.data);
    free(cipher.data);
    krb5_free_principal(context, target);
    krb5_free_unparsed_name(context, targetstr);
    krb5_free_unparsed_name(context, clientstr);
    krb5_free_error_message(context, errmsg);
    return ret;
}

/* Dispatch routine for set/change password */
void
dispatch(void *handle, const krb5_fulladdr *local_addr,
         const krb5_fulladdr *remote_addr, krb5_data *request, int is_tcp,
         verto_ctx *vctx, loop_respond_fn respond, void *arg)
{
    krb5_error_code ret;
    krb5_keytab kt = NULL;
    kadm5_server_handle_t server_handle = *(void **)handle;
    krb5_data *response = NULL;
    const char *emsg;

    ret = krb5_kt_resolve(server_handle->context, "KDB:", &kt);
    if (ret != 0) {
        emsg = krb5_get_error_message(server_handle->context, ret);
        krb5_klog_syslog(LOG_ERR, _("chpw: Couldn't open admin keytab %s"),
                         emsg);
        krb5_free_error_message(server_handle->context, emsg);
        goto egress;
    }

    response = k5alloc(sizeof(krb5_data), &ret);
    if (response == NULL)
        goto egress;

    ret = process_chpw_request(server_handle->context,
                               server_handle,
                               server_handle->params.realm,
                               kt,
                               local_addr,
                               remote_addr,
                               request,
                               response);
egress:
    if (ret)
        krb5_free_data(server_handle->context, response);
    krb5_kt_close(server_handle->context, kt);
    (*respond)(arg, ret, ret == 0 ? response : NULL);
}
