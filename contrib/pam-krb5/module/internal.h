/*
 * Internal prototypes and structures for pam-krb5.
 *
 * Copyright 2005-2009, 2014, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#ifndef INTERNAL_H
#define INTERNAL_H 1

#include <config.h>
#include <portable/krb5.h>
#include <portable/macros.h>
#include <portable/pam.h>

#include <stdarg.h>
#include <syslog.h>

/* Forward declarations to avoid unnecessary includes. */
struct pam_args;
struct passwd;
struct vector;

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))

/*
 * An authentication context, including all the data we want to preserve
 * across calls to the public entry points.  This context is stored in the PAM
 * state and a pointer to it is stored in the pam_args struct that is passed
 * as the first argument to most internal functions.
 */
struct context {
    char *name;             /* Username being authenticated. */
    krb5_context context;   /* Kerberos context. */
    krb5_ccache cache;      /* Active credential cache, if any. */
    krb5_principal princ;   /* Principal being authenticated. */
    int expired;            /* If set, account was expired. */
    int dont_destroy_cache; /* If set, don't destroy cache on shutdown. */
    int initialized;        /* If set, ticket cache initialized. */
    krb5_creds *creds;      /* Credentials for password changing. */
    krb5_ccache fast_cache; /* Temporary credential cache for FAST. */
};

/*
 * The global structure holding our arguments, both from krb5.conf and from
 * the PAM configuration.  Filled in by pamk5_init and stored in the pam_args
 * struct passed as a first argument to most internal functions.  Sort by
 * documentation order.
 */
struct pam_config {
    /* Authorization. */
    char *alt_auth_map;  /* An sprintf pattern to map principals. */
    bool force_alt_auth; /* Alt principal must be used if it exists. */
    bool ignore_k5login; /* Don't check .k5login files. */
    bool ignore_root;    /* Skip authentication for root. */
    long minimum_uid;    /* Ignore users below this UID. */
    bool only_alt_auth;  /* Alt principal must be used. */
    bool search_k5login; /* Try password with each line of .k5login. */
#ifdef __FreeBSD__
    bool allow_kdc_spoof;/* Allow auth even if KDC cannot be verified */
#endif /* __FreeBSD__ */

    /* Kerberos behavior. */
    char *fast_ccache;           /* Cache containing armor ticket. */
    bool anon_fast;              /* sets up an anonymous fast armor cache */
    bool forwardable;            /* Obtain forwardable tickets. */
    char *keytab;                /* Keytab for credential validation. */
    char *realm;                 /* Default realm for Kerberos. */
    krb5_deltat renew_lifetime;  /* Renewable lifetime of credentials. */
    krb5_deltat ticket_lifetime; /* Lifetime of credentials. */
    char *user_realm;            /* Default realm for user principals. */

    /* PAM behavior. */
    bool clear_on_fail;  /* Delete saved password on change failure. */
    bool debug;          /* Log debugging information. */
    bool defer_pwchange; /* Defer expired account fail to account. */
    bool fail_pwchange;  /* Treat expired password as auth failure. */
    bool force_pwchange; /* Change expired passwords in auth. */
    bool no_update_user; /* Don't update PAM_USER with local name. */
    bool silent;         /* Suppress text and errors (PAM_SILENT). */
    char *trace;         /* File name for trace logging. */

    /* PKINIT. */
    char *pkinit_anchors;       /* Trusted certificates, usually per realm. */
    bool pkinit_prompt;         /* Prompt user to insert smart card. */
    char *pkinit_user;          /* User ID to pass to PKINIT. */
    struct vector *preauth_opt; /* Preauth options. */
    bool try_pkinit;            /* Attempt PKINIT, fall back to password. */
    bool use_pkinit;            /* Require PKINIT. */

    /* Prompting. */
    char *banner;          /* Addition to password changing prompts. */
    bool expose_account;   /* Display principal in password prompts. */
    bool force_first_pass; /* Require a previous password be stored. */
    bool no_prompt;        /* Let Kerberos handle password prompting. */
    bool prompt_principal; /* Prompt for the Kerberos principal. */
    bool try_first_pass;   /* Try the previously entered password. */
    bool use_authtok;      /* Use the stored new password for changes. */
    bool use_first_pass;   /* Always use the previous password. */

    /* Ticket caches. */
    char *ccache;            /* Path to write ticket cache to. */
    char *ccache_dir;        /* Directory for ticket cache. */
    bool no_ccache;          /* Don't create a ticket cache. */
    bool retain_after_close; /* Don't destroy the cache on session end. */

    /* The authentication context, which bundles together Kerberos data. */
    struct context *ctx;
    bool no_warn;            /* XXX Dummy argument, remove when Heimdal is removed. */
};

/* Default to a hidden visibility for all internal functions. */
#pragma GCC visibility push(hidden)

/* Parse the PAM flags, arguments, and krb5.conf and fill out pam_args. */
struct pam_args *pamk5_init(pam_handle_t *, int flags, int, const char **);

/* Free the pam_args struct when we're done. */
void pamk5_free(struct pam_args *);

/*
 * The underlying functions between several of the major PAM interfaces.
 */
int pamk5_account(struct pam_args *);
int pamk5_authenticate(struct pam_args *);

/*
 * The underlying function below pam_sm_chauthtok.  If the second argument is
 * true, we're doing the preliminary check and shouldn't actually change the
 * password.
 */
int pamk5_password(struct pam_args *, bool only_auth);

/*
 * Create or refresh the user's ticket cache.  This is the underlying function
 * beneath pam_sm_setcred and pam_sm_open_session.
 */
int pamk5_setcred(struct pam_args *, bool refresh);

/*
 * Authenticate the user.  Prompts for the password as needed and obtains
 * tickets for in_tkt_service, krbtgt/<realm> by default.  Stores the initial
 * credentials in the final argument, allocating a new krb5_creds structure.
 * If possible, the initial credentials are verified by checking them against
 * the local system key.
 */
int pamk5_password_auth(struct pam_args *, const char *service, krb5_creds **);

/*
 * Prompt the user for a new password, twice so that they can confirm.  Sets
 * PAM_AUTHTOK and puts the new password in newly allocated memory in pass if
 * it's not NULL.
 */
int pamk5_password_prompt(struct pam_args *, char **pass);

/*
 * Change the user's password.  Prompts for the current password as needed and
 * the new password.  If the second argument is true, only obtains the
 * necessary credentials without changing anything.
 */
int pamk5_password_change(struct pam_args *, bool only_auth);

/*
 * Generic conversation function to display messages or get information from
 * the user.  Takes the message, the message type, and a place to put the
 * result of a prompt.
 */
int pamk5_conv(struct pam_args *, const char *, int, char **);

/*
 * Function specifically for getting a password.  Takes a prefix (if non-NULL,
 * args->banner will also be prepended) and a pointer into which to store the
 * password.  The password must be freed by the caller.
 */
int pamk5_get_password(struct pam_args *, const char *, char **);

/* Prompting function for the Kerberos libraries. */
krb5_error_code pamk5_prompter_krb5(krb5_context, void *data, const char *name,
                                    const char *banner, int, krb5_prompt *);

/* Prompting function that doesn't allow passwords. */
krb5_error_code pamk5_prompter_krb5_no_password(krb5_context, void *data,
                                                const char *name,
                                                const char *banner, int,
                                                krb5_prompt *);

/* Check the user with krb5_kuserok or the configured equivalent. */
int pamk5_authorized(struct pam_args *);

/* Returns true if we should ignore this user (root or low UID). */
int pamk5_should_ignore(struct pam_args *, PAM_CONST char *);

/*
 * alt_auth_map support.
 *
 * pamk5_map_principal attempts to map the user to a Kerberos principal
 * according to alt_auth_map.  Returns 0 on success, storing the mapped
 * principal name in newly allocated memory in principal.  The caller is
 * responsiple for freeing. Returns an errno value on any error.
 *
 * pamk5_alt_auth attempts an authentication to the given service with the
 * given options and password and returns a Kerberos error code.  On success,
 * the new credentials are stored in krb5_creds.
 *
 * pamk5_alt_auth_verify verifies that Kerberos credentials are authorized to
 * access the account given the configured alt_auth_map and is meant to be
 * called from pamk5_authorized.  It returns a PAM status code.
 */
int pamk5_map_principal(struct pam_args *, const char *username,
                        char **principal);
krb5_error_code pamk5_alt_auth(struct pam_args *, const char *service,
                               krb5_get_init_creds_opt *, const char *pass,
                               krb5_creds *);
int pamk5_alt_auth_verify(struct pam_args *);

/* FAST support.  Set up FAST protection of authentication. */
void pamk5_fast_setup(struct pam_args *, krb5_get_init_creds_opt *);

/* Context management. */
int pamk5_context_new(struct pam_args *);
int pamk5_context_fetch(struct pam_args *);
void pamk5_context_free(struct pam_args *);
void pamk5_context_destroy(pam_handle_t *, void *data, int pam_end_status);

/* Get and set environment variables for the ticket cache. */
const char *pamk5_get_krb5ccname(struct pam_args *, const char *key);
int pamk5_set_krb5ccname(struct pam_args *, const char *, const char *key);

/*
 * Create a ticket cache file securely given a mkstemp template.  Modifies
 * template in place to store the name of the created file.
 */
int pamk5_cache_mkstemp(struct pam_args *, char *template);

/*
 * Create a ticket cache and initialize it with the provided credentials,
 * returning the new cache in the last argument
 */
int pamk5_cache_init(struct pam_args *, const char *ccname, krb5_creds *,
                     krb5_ccache *);

/*
 * Create a ticket cache with a random path, initialize it with the provided
 * credentials, store it in the context, and put the path into PAM_KRB5CCNAME.
 */
int pamk5_cache_init_random(struct pam_args *, krb5_creds *);

/*
 * Compatibility functions.  Depending on whether pam_krb5 is built with MIT
 * Kerberos or Heimdal, appropriate implementations for the Kerberos
 * implementation will be provided.
 */
krb5_error_code pamk5_compat_set_realm(struct pam_config *, const char *);
void pamk5_compat_free_realm(struct pam_config *);

/* Undo default visibility change. */
#pragma GCC visibility pop

#endif /* !INTERNAL_H */
