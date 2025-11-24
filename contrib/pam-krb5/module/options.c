/*
 * Option handling for pam-krb5.
 *
 * Responsible for initializing the args struct that's passed to nearly all
 * internal functions.  Retrieves configuration information from krb5.conf and
 * parses the PAM configuration.
 *
 * Copyright 2005-2010, 2014, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/system.h>

#include <errno.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>
#include <pam-util/options.h>
#include <pam-util/vector.h>

/* Our option definition.  Must be sorted. */
#define K(name) (#name), offsetof(struct pam_config, name)
/* clang-format off */
static const struct option options[] = {
#ifdef __FreeBSD__
    { K(allow_kdc_spoof),    true,  BOOL   (false) },
#endif /* __FreeBSD__ */
    { K(alt_auth_map),       true,  STRING (NULL)  },
    { K(anon_fast),          true,  BOOL   (false) },
    { K(banner),             true,  STRING ("Kerberos") },
    { K(ccache),             true,  STRING (NULL)  },
    { K(ccache_dir),         true,  STRING ("FILE:/tmp") },
    { K(clear_on_fail),      true,  BOOL   (false) },
    { K(debug),              true,  BOOL   (false) },
    { K(defer_pwchange),     true,  BOOL   (false) },
    { K(expose_account),     true,  BOOL   (false) },
    { K(fail_pwchange),      true,  BOOL   (false) },
    { K(fast_ccache),        true,  STRING (NULL)  },
    { K(force_alt_auth),     true,  BOOL   (false) },
    { K(force_first_pass),   false, BOOL   (false) },
    { K(force_pwchange),     true,  BOOL   (false) },
    { K(forwardable),        true,  BOOL   (false) },
    { K(ignore_k5login),     true,  BOOL   (false) },
    { K(ignore_root),        true,  BOOL   (false) },
    { K(keytab),             true,  STRING (NULL)  },
    { K(minimum_uid),        true,  NUMBER (0)     },
    { K(no_ccache),          false, BOOL   (false) },
    { K(no_prompt),          true,  BOOL   (false) },
    { K(no_update_user),     true,  BOOL   (false) },
    { K(no_warn),	     true,  BOOL   (false) },
    { K(only_alt_auth),      true,  BOOL   (false) },
    { K(pkinit_anchors),     true,  STRING (NULL)  },
    { K(pkinit_prompt),      true,  BOOL   (false) },
    { K(pkinit_user),        true,  STRING (NULL)  },
    { K(preauth_opt),        true,  LIST   (NULL)  },
    { K(prompt_principal),   true,  BOOL   (false) },
    { K(realm),              false, STRING (NULL)  },
    { K(renew_lifetime),     true,  TIME   (0)     },
    { K(retain_after_close), true,  BOOL   (false) },
    { K(search_k5login),     true,  BOOL   (false) },
    { K(silent),             false, BOOL   (false) },
    { K(ticket_lifetime),    true,  TIME   (0)     },
    { K(trace),              false, STRING (NULL)  },
    { K(try_first_pass),     false, BOOL   (false) },
    { K(try_pkinit),         true,  BOOL   (false) },
    { K(use_authtok),        false, BOOL   (false) },
    { K(use_first_pass),     false, BOOL   (false) },
    { K(use_pkinit),         true,  BOOL   (false) },
    { K(user_realm),         true,  STRING (NULL)  },
};
/* clang-format on */
static const size_t optlen = sizeof(options) / sizeof(options[0]);


/*
 * Allocate a new struct pam_args and initialize its data members, including
 * parsing the arguments and getting settings from krb5.conf.  Check the
 * resulting options for consistency.
 */
struct pam_args *
pamk5_init(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int i;
    struct pam_args *args;
    struct pam_config *config = NULL;

    args = putil_args_new(pamh, flags);
    if (args == NULL) {
        return NULL;
    }
    config = calloc(1, sizeof(struct pam_config));
    if (config == NULL) {
        goto nomem;
    }
    args->config = config;

    /*
     * Do an initial scan to see if the realm is already set in our options.
     * If so, make sure that's set before we start loading option values,
     * since it affects what comes out of krb5.conf.
     *
     * We will then ignore args->config->realm, set later by option parsing,
     * in favor of using args->realm extracted here.  However, the latter must
     * exist to avoid throwing unknown option errors.
     */
    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "realm=", 6) != 0)
            continue;
        free(args->realm);
        args->realm = strdup(&argv[i][strlen("realm=")]);
        if (args->realm == NULL)
            goto nomem;
    }

    if (!putil_args_defaults(args, options, optlen)) {
        free(config);
        putil_args_free(args);
        return NULL;
    }
    if (!putil_args_krb5(args, "pam", options, optlen)) {
        goto fail;
    }
    if (!putil_args_parse(args, argc, argv, options, optlen)) {
        goto fail;
    }
    if (config->debug) {
        args->debug = true;
    }
    if (config->silent) {
        args->silent = true;
    }

    /* An empty banner should be treated the same as not having one. */
    if (config->banner != NULL && config->banner[0] == '\0') {
        free(config->banner);
        config->banner = NULL;
    }

    /* Sanity-check try_first_pass, use_first_pass, and force_first_pass. */
    if (config->force_first_pass && config->try_first_pass) {
        putil_err(args, "force_first_pass set, ignoring try_first_pass");
        config->try_first_pass = 0;
    }
    if (config->force_first_pass && config->use_first_pass) {
        putil_err(args, "force_first_pass set, ignoring use_first_pass");
        config->use_first_pass = 0;
    }
    if (config->use_first_pass && config->try_first_pass) {
        putil_err(args, "use_first_pass set, ignoring try_first_pass");
        config->try_first_pass = 0;
    }

    /*
     * Don't set expose_account if we're using search_k5login.  The user will
     * get a principal formed from the account into which they're logging in,
     * which isn't the password they'll use (that's the whole point of
     * search_k5login).
     */
    if (config->search_k5login) {
        config->expose_account = 0;
    }

    /* UIDs are unsigned on some systems. */
    if (config->minimum_uid < 0) {
        config->minimum_uid = 0;
    }

    /*
     * Warn if PKINIT options were set and PKINIT isn't supported.  The MIT
     * method (krb5_get_init_creds_opt_set_pa) can't support use_pkinit.
     */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT
#    ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PA
    if (config->try_pkinit) {
        putil_err(args, "try_pkinit requested but PKINIT not available");
    } else if (config->use_pkinit) {
        putil_err(args, "use_pkinit requested but PKINIT not available");
    }
#    endif
#    ifndef HAVE_KRB5_GET_PROMPT_TYPES
    if (config->use_pkinit) {
        putil_err(args, "use_pkinit requested but PKINIT cannot be enforced");
    }
#    endif
#endif

    /* Warn if the FAST option was set and FAST isn't supported. */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_FAST_CCACHE_NAME
    if (config->fast_ccache || config->anon_fast) {
        putil_err(args, "fast_ccache or anon_fast requested but FAST not"
                        " supported by Kerberos libraries");
    }
#endif

    /* If tracing was requested enable it if possible. */
#ifdef HAVE_KRB5_SET_TRACE_FILENAME
    if (config->trace != NULL) {
        krb5_error_code retval;

        retval = krb5_set_trace_filename(args->ctx, config->trace);
        if (retval == 0)
            putil_debug(args, "enabled trace logging to %s", config->trace);
        else
            putil_err_krb5(args, retval, "cannot enable trace logging to %s",
                           config->trace);
    }
#else
    if (config->trace != NULL) {
        putil_err(args, "trace logging requested but not supported");
    }
#endif

    return args;

nomem:
    putil_crit(args, "cannot allocate memory: %s", strerror(errno));
    free(config);
    putil_args_free(args);
    return NULL;

fail:
    pamk5_free(args);
    return NULL;
}


/*
 * Free the allocated args struct and any memory it points to.
 */
void
pamk5_free(struct pam_args *args)
{
    struct pam_config *config;

    if (args == NULL)
        return;
    config = args->config;
    if (config != NULL) {
        free(config->alt_auth_map);
        free(config->banner);
        free(config->ccache);
        free(config->ccache_dir);
        free(config->fast_ccache);
        free(config->keytab);
        free(config->pkinit_anchors);
        free(config->pkinit_user);
        vector_free(config->preauth_opt);
        free(config->realm);
        free(config->trace);
        free(config->user_realm);
        free(args->config);
        args->config = NULL;
    }
    putil_args_free(args);
}
