/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * COPYRIGHT (C) 2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>

#include "pkinit.h"

static void
free_list(char **list)
{
    int i;

    if (list == NULL)
        return;

    for (i = 0; list[i] != NULL; i++)
        free(list[i]);
    free(list);
}

static krb5_error_code
copy_list(char ***dst, char **src)
{
    int i;
    char **newlist;

    if (dst == NULL)
        return EINVAL;
    *dst = NULL;

    if (src == NULL)
        return 0;

    for (i = 0; src[i] != NULL; i++);

    newlist = calloc(1, (i + 1) * sizeof(*newlist));
    if (newlist == NULL)
        return ENOMEM;

    for (i = 0; src[i] != NULL; i++) {
        newlist[i] = strdup(src[i]);
        if (newlist[i] == NULL)
            goto cleanup;
    }
    newlist[i] = NULL;
    *dst = newlist;
    return 0;
cleanup:
    free_list(newlist);
    return ENOMEM;
}

char *
idtype2string(int idtype)
{
    switch(idtype) {
    case IDTYPE_FILE: return "FILE"; break;
    case IDTYPE_DIR: return "DIR"; break;
    case IDTYPE_PKCS11: return "PKCS11"; break;
    case IDTYPE_PKCS12: return "PKCS12"; break;
    case IDTYPE_ENVVAR: return "ENV"; break;
#ifdef PKINIT_CRYPTO_IMPL_NSS
    case IDTYPE_NSS: return "NSS"; break;
#endif
    default: return "INVALID"; break;
    }
}

char *
catype2string(int catype)
{
    switch(catype) {
    case CATYPE_ANCHORS: return "ANCHORS"; break;
    case CATYPE_INTERMEDIATES: return "INTERMEDIATES"; break;
    case CATYPE_CRLS: return "CRLS"; break;
    default: return "INVALID"; break;
    }
}

krb5_error_code
pkinit_init_identity_opts(pkinit_identity_opts **idopts)
{
    pkinit_identity_opts *opts = NULL;

    *idopts = NULL;
    opts = calloc(1, sizeof(pkinit_identity_opts));
    if (opts == NULL)
        return ENOMEM;

    opts->identity = NULL;
    opts->anchors = NULL;
    opts->intermediates = NULL;
    opts->crls = NULL;
    opts->ocsp = NULL;

    opts->cert_filename = NULL;
    opts->key_filename = NULL;
#ifndef WITHOUT_PKCS11
    opts->p11_module_name = NULL;
    opts->slotid = PK_NOSLOT;
    opts->token_label = NULL;
    opts->cert_id_string = NULL;
    opts->cert_label = NULL;
#endif

    *idopts = opts;

    return 0;
}

krb5_error_code
pkinit_dup_identity_opts(pkinit_identity_opts *src_opts,
                         pkinit_identity_opts **dest_opts)
{
    pkinit_identity_opts *newopts;
    krb5_error_code retval;

    *dest_opts = NULL;
    retval = pkinit_init_identity_opts(&newopts);
    if (retval)
        return retval;

    retval = ENOMEM;

    if (src_opts->identity != NULL) {
        newopts->identity = strdup(src_opts->identity);
        if (newopts->identity == NULL)
            goto cleanup;
    }

    retval = copy_list(&newopts->anchors, src_opts->anchors);
    if (retval)
        goto cleanup;

    retval = copy_list(&newopts->intermediates,src_opts->intermediates);
    if (retval)
        goto cleanup;

    retval = copy_list(&newopts->crls, src_opts->crls);
    if (retval)
        goto cleanup;

    if (src_opts->ocsp != NULL) {
        newopts->ocsp = strdup(src_opts->ocsp);
        if (newopts->ocsp == NULL)
            goto cleanup;
    }

    if (src_opts->cert_filename != NULL) {
        newopts->cert_filename = strdup(src_opts->cert_filename);
        if (newopts->cert_filename == NULL)
            goto cleanup;
    }

    if (src_opts->key_filename != NULL) {
        newopts->key_filename = strdup(src_opts->key_filename);
        if (newopts->key_filename == NULL)
            goto cleanup;
    }

#ifndef WITHOUT_PKCS11
    if (src_opts->p11_module_name != NULL) {
        newopts->p11_module_name = strdup(src_opts->p11_module_name);
        if (newopts->p11_module_name == NULL)
            goto cleanup;
    }

    newopts->slotid = src_opts->slotid;

    if (src_opts->token_label != NULL) {
        newopts->token_label = strdup(src_opts->token_label);
        if (newopts->token_label == NULL)
            goto cleanup;
    }

    if (src_opts->cert_id_string != NULL) {
        newopts->cert_id_string = strdup(src_opts->cert_id_string);
        if (newopts->cert_id_string == NULL)
            goto cleanup;
    }

    if (src_opts->cert_label != NULL) {
        newopts->cert_label = strdup(src_opts->cert_label);
        if (newopts->cert_label == NULL)
            goto cleanup;
    }
#endif


    *dest_opts = newopts;
    return 0;
cleanup:
    pkinit_fini_identity_opts(newopts);
    return retval;
}

void
pkinit_fini_identity_opts(pkinit_identity_opts *idopts)
{
    if (idopts == NULL)
        return;

    if (idopts->identity != NULL)
        free(idopts->identity);
    free_list(idopts->anchors);
    free_list(idopts->intermediates);
    free_list(idopts->crls);
    free_list(idopts->identity_alt);

    free(idopts->cert_filename);
    free(idopts->key_filename);
#ifndef WITHOUT_PKCS11
    free(idopts->p11_module_name);
    free(idopts->token_label);
    free(idopts->cert_id_string);
    free(idopts->cert_label);
#endif
    free(idopts);
}

#ifndef WITHOUT_PKCS11
static krb5_error_code
parse_pkcs11_options(krb5_context context,
                     pkinit_identity_opts *idopts,
                     const char *residual)
{
    char *s, *cp, *vp, *save;
    krb5_error_code retval = ENOMEM;

    if (residual == NULL || residual[0] == '\0')
        return 0;

    /* Split string into attr=value substrings */
    s = strdup(residual);
    if (s == NULL)
        return retval;

    for (cp = strtok_r(s, ":", &save); cp; cp = strtok_r(NULL, ":", &save)) {
        vp = strchr(cp, '=');

        /* If there is no "=", this is a pkcs11 module name */
        if (vp == NULL) {
            free(idopts->p11_module_name);
            idopts->p11_module_name = strdup(cp);
            if (idopts->p11_module_name == NULL)
                goto cleanup;
            continue;
        }
        *vp++ = '\0';
        if (!strcmp(cp, "module_name")) {
            free(idopts->p11_module_name);
            idopts->p11_module_name = strdup(vp);
            if (idopts->p11_module_name == NULL)
                goto cleanup;
        } else if (!strcmp(cp, "slotid")) {
            long slotid = strtol(vp, NULL, 10);
            if ((slotid == LONG_MIN || slotid == LONG_MAX) && errno != 0) {
                retval = EINVAL;
                goto cleanup;
            }
            if ((long) (int) slotid != slotid) {
                retval = EINVAL;
                goto cleanup;
            }
            idopts->slotid = slotid;
        } else if (!strcmp(cp, "token")) {
            free(idopts->token_label);
            idopts->token_label = strdup(vp);
            if (idopts->token_label == NULL)
                goto cleanup;
        } else if (!strcmp(cp, "certid")) {
            free(idopts->cert_id_string);
            idopts->cert_id_string = strdup(vp);
            if (idopts->cert_id_string == NULL)
                goto cleanup;
        } else if (!strcmp(cp, "certlabel")) {
            free(idopts->cert_label);
            idopts->cert_label = strdup(vp);
            if (idopts->cert_label == NULL)
                goto cleanup;
        }
    }
    retval = 0;
cleanup:
    free(s);
    return retval;
}
#endif

static krb5_error_code
parse_fs_options(krb5_context context,
                 pkinit_identity_opts *idopts,
                 const char *residual)
{
    char *certname, *keyname, *save;
    krb5_error_code retval = ENOMEM;

    if (residual == NULL || residual[0] == '\0')
        return 0;

    certname = strdup(residual);
    if (certname == NULL)
        goto cleanup;

    certname = strtok_r(certname, ",", &save);
    keyname = strtok_r(NULL, ",", &save);

    idopts->cert_filename = strdup(certname);
    if (idopts->cert_filename == NULL)
        goto cleanup;

    idopts->key_filename = strdup(keyname ? keyname : certname);
    if (idopts->key_filename == NULL)
        goto cleanup;

    retval = 0;
cleanup:
    free(certname);
    return retval;
}

static krb5_error_code
parse_pkcs12_options(krb5_context context,
                     pkinit_identity_opts *idopts,
                     const char *residual)
{
    krb5_error_code retval = ENOMEM;

    if (residual == NULL || residual[0] == '\0')
        return 0;

    idopts->cert_filename = strdup(residual);
    if (idopts->cert_filename == NULL)
        goto cleanup;

    idopts->key_filename = strdup(residual);
    if (idopts->key_filename == NULL)
        goto cleanup;

    pkiDebug("%s: cert_filename '%s' key_filename '%s'\n",
             __FUNCTION__, idopts->cert_filename,
             idopts->key_filename);
    retval = 0;
cleanup:
    return retval;
}

static krb5_error_code
process_option_identity(krb5_context context,
                        pkinit_plg_crypto_context plg_cryptoctx,
                        pkinit_req_crypto_context req_cryptoctx,
                        pkinit_identity_opts *idopts,
                        pkinit_identity_crypto_context id_cryptoctx,
                        const char *value)
{
    const char *residual;
    int idtype;
    krb5_error_code retval = 0;

    pkiDebug("%s: processing value '%s'\n",
             __FUNCTION__, value ? value : "NULL");
    if (value == NULL)
        return EINVAL;

    residual = strchr(value, ':');
    if (residual != NULL) {
        unsigned int typelen;
        residual++; /* skip past colon */
        typelen = residual - value;
        if (strncmp(value, "FILE:", typelen) == 0) {
            idtype = IDTYPE_FILE;
#ifndef WITHOUT_PKCS11
        } else if (strncmp(value, "PKCS11:", typelen) == 0) {
            idtype = IDTYPE_PKCS11;
#endif
        } else if (strncmp(value, "PKCS12:", typelen) == 0) {
            idtype = IDTYPE_PKCS12;
        } else if (strncmp(value, "DIR:", typelen) == 0) {
            idtype = IDTYPE_DIR;
        } else if (strncmp(value, "ENV:", typelen) == 0) {
            idtype = IDTYPE_ENVVAR;
#ifdef PKINIT_CRYPTO_IMPL_NSS
        } else if (strncmp(value, "NSS:", typelen) == 0) {
            idtype = IDTYPE_NSS;
#endif
        } else {
            pkiDebug("%s: Unsupported type while processing '%s'\n",
                     __FUNCTION__, value);
            krb5_set_error_message(context, KRB5_PREAUTH_FAILED,
                                   _("Unsupported type while processing "
                                     "'%s'\n"), value);
            return KRB5_PREAUTH_FAILED;
        }
    } else {
        idtype = IDTYPE_FILE;
        residual = value;
    }

    idopts->idtype = idtype;
    pkiDebug("%s: idtype is %s\n", __FUNCTION__, idtype2string(idopts->idtype));
    switch (idtype) {
    case IDTYPE_ENVVAR:
        return process_option_identity(context, plg_cryptoctx, req_cryptoctx,
                                       idopts, id_cryptoctx, getenv(residual));
        break;
    case IDTYPE_FILE:
        retval = parse_fs_options(context, idopts, residual);
        break;
    case IDTYPE_PKCS12:
        retval = parse_pkcs12_options(context, idopts, residual);
        break;
#ifndef WITHOUT_PKCS11
    case IDTYPE_PKCS11:
        retval = parse_pkcs11_options(context, idopts, residual);
        break;
#endif
    case IDTYPE_DIR:
        idopts->cert_filename = strdup(residual);
        if (idopts->cert_filename == NULL)
            retval = ENOMEM;
        break;
#ifdef PKINIT_CRYPTO_IMPL_NSS
    case IDTYPE_NSS:
        idopts->cert_filename = strdup(residual);
        if (idopts->cert_filename == NULL)
            retval = ENOMEM;
        break;
#endif
    default:
        krb5_set_error_message(context, KRB5_PREAUTH_FAILED,
                               _("Internal error parsing "
                                 "X509_user_identity\n"));
        retval = EINVAL;
        break;
    }
    return retval;
}

static krb5_error_code
process_option_ca_crl(krb5_context context,
                      pkinit_plg_crypto_context plg_cryptoctx,
                      pkinit_req_crypto_context req_cryptoctx,
                      pkinit_identity_opts *idopts,
                      pkinit_identity_crypto_context id_cryptoctx,
                      const char *value,
                      int catype)
{
    char *residual;
    unsigned int typelen;
    int idtype;

    pkiDebug("%s: processing catype %s, value '%s'\n",
             __FUNCTION__, catype2string(catype), value);
    residual = strchr(value, ':');
    if (residual == NULL) {
        pkiDebug("No type given for '%s'\n", value);
        return EINVAL;
    }
    residual++; /* skip past colon */
    typelen = residual - value;
    if (strncmp(value, "FILE:", typelen) == 0) {
        idtype = IDTYPE_FILE;
    } else if (strncmp(value, "DIR:", typelen) == 0) {
        idtype = IDTYPE_DIR;
#ifdef PKINIT_CRYPTO_IMPL_NSS
    } else if (strncmp(value, "NSS:", typelen) == 0) {
        idtype = IDTYPE_NSS;
#endif
    } else {
        return ENOTSUP;
    }
    return crypto_load_cas_and_crls(context,
                                    plg_cryptoctx,
                                    req_cryptoctx,
                                    idopts, id_cryptoctx,
                                    idtype, catype, residual);
}

/*
 * Load any identity information which doesn't require us to ask a controlling
 * user any questions, and record the names of anything else which would
 * require us to ask questions.
 */
krb5_error_code
pkinit_identity_initialize(krb5_context context,
                           pkinit_plg_crypto_context plg_cryptoctx,
                           pkinit_req_crypto_context req_cryptoctx,
                           pkinit_identity_opts *idopts,
                           pkinit_identity_crypto_context id_cryptoctx,
                           krb5_clpreauth_callbacks cb,
                           krb5_clpreauth_rock rock,
                           krb5_principal princ)
{
    krb5_error_code retval = EINVAL;
    int i;

    pkiDebug("%s: %p %p %p\n", __FUNCTION__, context, idopts, id_cryptoctx);
    if (!(princ &&
          krb5_principal_compare_any_realm(context, princ,
                                           krb5_anonymous_principal()))) {
        if (idopts == NULL || id_cryptoctx == NULL)
            goto errout;

        /*
         * If identity was specified, use that.  (For the kdc, this
         * is specified as pkinit_identity in the kdc.conf.  For users,
         * this is specified on the command line via X509_user_identity.)
         * If a user did not specify identity on the command line,
         * then we will try alternatives which may have been specified
         * in the config file.
         */
        if (idopts->identity != NULL) {
            retval = process_option_identity(context, plg_cryptoctx,
                                             req_cryptoctx, idopts,
                                             id_cryptoctx, idopts->identity);
        } else if (idopts->identity_alt != NULL) {
            for (i = 0; retval != 0 && idopts->identity_alt[i] != NULL; i++) {
                retval = process_option_identity(context, plg_cryptoctx,
                                                 req_cryptoctx, idopts,
                                                 id_cryptoctx,
                                                 idopts->identity_alt[i]);
            }
        } else {
            retval = KRB5_PREAUTH_FAILED;
            krb5_set_error_message(context, retval,
                                   _("No user identity options specified"));
            pkiDebug("%s: no user identity options specified\n", __FUNCTION__);
            goto errout;
        }
        if (retval)
            goto errout;

        retval = crypto_load_certs(context, plg_cryptoctx, req_cryptoctx,
                                   idopts, id_cryptoctx, princ, TRUE);
        if (retval)
            goto errout;
    } else {
        /* We're the anonymous principal. */
        retval = 0;
    }

errout:
    return retval;
}

/*
 * Load identity information, including that which requires us to ask a
 * controlling user any questions.  If we have PIN/password values which
 * correspond to a given identity, use that, otherwise, if one is available,
 * we'll use the prompter callback.
 */
krb5_error_code
pkinit_identity_prompt(krb5_context context,
                       pkinit_plg_crypto_context plg_cryptoctx,
                       pkinit_req_crypto_context req_cryptoctx,
                       pkinit_identity_opts *idopts,
                       pkinit_identity_crypto_context id_cryptoctx,
                       krb5_clpreauth_callbacks cb,
                       krb5_clpreauth_rock rock,
                       int do_matching,
                       krb5_principal princ)
{
    krb5_error_code retval = EINVAL;
    const char *signer_identity;
    int i;

    pkiDebug("%s: %p %p %p\n", __FUNCTION__, context, idopts, id_cryptoctx);
    if (!(princ &&
          krb5_principal_compare_any_realm(context, princ,
                                           krb5_anonymous_principal()))) {
        retval = crypto_load_certs(context, plg_cryptoctx, req_cryptoctx,
                                   idopts, id_cryptoctx, princ, FALSE);
        if (retval)
            goto errout;

        if (do_matching) {
            /*
             * Try to select exactly one certificate based on matching
             * criteria.  Typical used for clients.
             */
            retval = pkinit_cert_matching(context, plg_cryptoctx,
                                          req_cryptoctx, id_cryptoctx, princ);
            if (retval) {
                pkiDebug("%s: No matching certificate found\n", __FUNCTION__);
                crypto_free_cert_info(context, plg_cryptoctx, req_cryptoctx,
                                      id_cryptoctx);
                goto errout;
            }
        } else {
            /*
             * Tell crypto code to use the "default" identity.  Typically used
             * for KDCs.
             */
            retval = crypto_cert_select_default(context, plg_cryptoctx,
                                                req_cryptoctx, id_cryptoctx);
            if (retval) {
                pkiDebug("%s: Failed while selecting default certificate\n",
                         __FUNCTION__);
                crypto_free_cert_info(context, plg_cryptoctx, req_cryptoctx,
                                      id_cryptoctx);
                goto errout;
            }
        }

        if (rock != NULL && cb != NULL && retval == 0) {
            /* Save the signer identity if we're the client. */
            if (crypto_retrieve_signer_identity(context, id_cryptoctx,
                                                &signer_identity) == 0) {
                cb->set_cc_config(context, rock, "X509_user_identity",
                                  signer_identity);
            }
        }

        retval = crypto_free_cert_info(context, plg_cryptoctx, req_cryptoctx,
                                       id_cryptoctx);
        if (retval)
            goto errout;
    } /* Not anonymous principal */

    for (i = 0; idopts->anchors != NULL && idopts->anchors[i] != NULL; i++) {
        retval = process_option_ca_crl(context, plg_cryptoctx, req_cryptoctx,
                                       idopts, id_cryptoctx,
                                       idopts->anchors[i], CATYPE_ANCHORS);
        if (retval)
            goto errout;
    }
    for (i = 0; idopts->intermediates != NULL
             && idopts->intermediates[i] != NULL; i++) {
        retval = process_option_ca_crl(context, plg_cryptoctx, req_cryptoctx,
                                       idopts, id_cryptoctx,
                                       idopts->intermediates[i],
                                       CATYPE_INTERMEDIATES);
        if (retval)
            goto errout;
    }
    for (i = 0; idopts->crls != NULL && idopts->crls[i] != NULL; i++) {
        retval = process_option_ca_crl(context, plg_cryptoctx, req_cryptoctx,
                                       idopts, id_cryptoctx, idopts->crls[i],
                                       CATYPE_CRLS);
        if (retval)
            goto errout;
    }
    if (idopts->ocsp != NULL) {
        retval = ENOTSUP;
        goto errout;
    }

errout:
    return retval;
}

/*
 * Create an entry in the passed-in list for the named identity, optionally
 * with the specified token flag value and/or supplied password, replacing any
 * existing entry with the same identity name.
 */
krb5_error_code
pkinit_set_deferred_id(pkinit_deferred_id **identities,
                       const char *identity, unsigned long ck_flags,
                       const char *password)
{
    int i;
    pkinit_deferred_id *out = NULL, *ids;
    char *tmp;

    /* Search for an entry that's already in the list. */
    ids = *identities;
    for (i = 0; ids != NULL && ids[i] != NULL; i++) {
        if (strcmp(ids[i]->identity, identity) == 0) {
            /* Replace its password value, then we're done. */
            tmp = password ? strdup(password) : NULL;
            if (password != NULL && tmp == NULL)
                return ENOMEM;
            ids[i]->ck_flags = ck_flags;
            free(ids[i]->password);
            ids[i]->password = tmp;
            return 0;
        }
    }

    /* Resize the list. */
    out = realloc(ids, sizeof(*ids) * (i + 2));
    if (out == NULL)
        goto oom;
    *identities = out;

    /* Allocate the new final entry. */
    out[i] = malloc(sizeof(*(out[i])));
    if (out[i] == NULL)
        goto oom;

    /* Populate the new entry. */
    out[i]->magic = PKINIT_DEFERRED_ID_MAGIC;
    out[i]->identity = strdup(identity);
    if (out[i]->identity == NULL)
        goto oom;

    out[i]->ck_flags = ck_flags;
    out[i]->password = password ? strdup(password) : NULL;
    if (password != NULL && out[i]->password == NULL)
        goto oom;

    /* Terminate the list. */
    out[i + 1] = NULL;
    return 0;

oom:
    if (out != NULL && out[i] != NULL) {
        free(out[i]->identity);
        free(out[i]);
        out[i] = NULL;
    }
    return ENOMEM;
}

/*
 * Return a password which we've associated with the named identity, if we've
 * stored one.  Otherwise return NULL.
 */
const char *
pkinit_find_deferred_id(pkinit_deferred_id *identities,
                        const char *identity)
{
    int i;

    for (i = 0; identities != NULL && identities[i] != NULL; i++) {
        if (strcmp(identities[i]->identity, identity) == 0)
            return identities[i]->password;
    }
    return NULL;
}

/*
 * Return the flags associated with the specified identity, or 0 if we don't
 * have such an identity.
 */
unsigned long
pkinit_get_deferred_id_flags(pkinit_deferred_id *identities,
                             const char *identity)
{
    int i;

    for (i = 0; identities != NULL && identities[i] != NULL; i++) {
        if (strcmp(identities[i]->identity, identity) == 0)
            return identities[i]->ck_flags;
    }
    return 0;
}

/*
 * Free a deferred_id list.
 */
void
pkinit_free_deferred_ids(pkinit_deferred_id *identities)
{
    int i;

    for (i = 0; identities != NULL && identities[i] != NULL; i++) {
        free(identities[i]->identity);
        free(identities[i]->password);
        free(identities[i]);
    }
    free(identities);
}
