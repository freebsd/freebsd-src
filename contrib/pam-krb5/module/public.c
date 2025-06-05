/*
 * The public APIs of the pam-afs-session PAM module.
 *
 * Provides the public pam_sm_authenticate, pam_sm_setcred,
 * pam_sm_open_session, pam_sm_close_session, and pam_sm_chauthtok functions.
 * These must all be specified in the same file to work with the symbol export
 * and linking mechanism used in OpenPAM, since OpenPAM will mark them all as
 * static functions and export a function table instead.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2005-2009, 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

/* Get prototypes for all of the functions. */
#define PAM_SM_ACCOUNT
#define PAM_SM_AUTH
#define PAM_SM_PASSWORD
#define PAM_SM_SESSION

#include <config.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * The main PAM interface for authorization checking.
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    struct pam_args *args;
    int pamret;

    args = pamk5_init(pamh, flags, argc, argv);
    if (args == NULL) {
        pamret = PAM_AUTH_ERR;
        goto done;
    }
    pamret = pamk5_context_fetch(args);
    ENTRY(args, flags);

    /*
     * Succeed if the user did not use krb5 to login.  Ideally, we should
     * probably fail and require that the user set up policy properly in their
     * PAM configuration, but it's not common for the user to do so and that's
     * not how other krb5 PAM modules work.  If we don't do this, root logins
     * with the system root password fail, which is a bad failure mode.
     */
    if (pamret != PAM_SUCCESS || args->config->ctx == NULL) {
        pamret = PAM_IGNORE;
        putil_debug(args, "skipping non-Kerberos login");
        goto done;
    }

    pamret = pamk5_account(args);

done:
    EXIT(args, pamret);
    pamk5_free(args);
    return pamret;
}


/*
 * The main PAM interface for authentication.  We also do authorization checks
 * here, since many applications don't call pam_acct_mgmt.
 */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    struct pam_args *args;
    int pamret;

    args = pamk5_init(pamh, flags, argc, argv);
    if (args == NULL) {
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    ENTRY(args, flags);

    pamret = pamk5_authenticate(args);

done:
    EXIT(args, pamret);
    pamk5_free(args);
    return pamret;
}


/*
 * The main PAM interface, in the auth stack, for establishing credentials
 * obtained during authentication.
 */
PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    struct pam_args *args;
    bool refresh = false;
    int pamret, allow;

    args = pamk5_init(pamh, flags, argc, argv);
    if (args == NULL) {
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    ENTRY(args, flags);

    /*
     * Special case.  Just free the context data, which will destroy the
     * ticket cache as well.
     */
    if (flags & PAM_DELETE_CRED) {
        pamret = pam_set_data(pamh, "pam_krb5", NULL, NULL);
        if (pamret != PAM_SUCCESS)
            putil_err_pam(args, pamret, "cannot clear context data");
        goto done;
    }

    /*
     * Reinitialization requested, which means that rather than creating a new
     * ticket cache and setting KRB5CCNAME, we should figure out the existing
     * ticket cache and just refresh its tickets.
     */
    if (flags & (PAM_REINITIALIZE_CRED | PAM_REFRESH_CRED))
        refresh = true;
    if (refresh && (flags & PAM_ESTABLISH_CRED)) {
        putil_err(args, "requested establish and refresh at the same time");
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    allow = PAM_REINITIALIZE_CRED | PAM_REFRESH_CRED | PAM_ESTABLISH_CRED;
    if (!(flags & allow)) {
        putil_err(args, "invalid pam_setcred flags %d", flags);
        pamret = PAM_SERVICE_ERR;
        goto done;
    }

    /* Do the work. */
    pamret = pamk5_setcred(args, refresh);

    /*
     * Never return PAM_IGNORE from pam_setcred since this can confuse the
     * Linux PAM library, at least for applications that call pam_setcred
     * without pam_authenticate (possibly because authentication was done
     * some other way), when used with jumps with the [] syntax.  Since we
     * do nothing in this case, and since the stack is already frozen from
     * the auth group, success makes sense.
     *
     * Don't return an error here or the PAM stack will fail if pam-krb5 is
     * used with [success=ok default=1], since jumps are treated as required
     * during the second pass with pam_setcred.
     */
    if (pamret == PAM_IGNORE)
        pamret = PAM_SUCCESS;

done:
    EXIT(args, pamret);
    pamk5_free(args);
    return pamret;
}


/*
 * The main PAM interface for password changing.
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    struct pam_args *args;
    int pamret;

    args = pamk5_init(pamh, flags, argc, argv);
    if (args == NULL) {
        pamret = PAM_AUTHTOK_ERR;
        goto done;
    }
    pamk5_context_fetch(args);
    ENTRY(args, flags);

    /* We only support password changes. */
    if (!(flags & PAM_UPDATE_AUTHTOK) && !(flags & PAM_PRELIM_CHECK)) {
        putil_err(args, "invalid pam_chauthtok flags %d", flags);
        pamret = PAM_AUTHTOK_ERR;
        goto done;
    }

    pamret = pamk5_password(args, (flags & PAM_PRELIM_CHECK) != 0);

done:
    EXIT(args, pamret);
    pamk5_free(args);
    return pamret;
}


/*
 * The main PAM interface for opening a session.
 */
PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    struct pam_args *args;
    int pamret;

    args = pamk5_init(pamh, flags, argc, argv);
    if (args == NULL) {
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    ENTRY(args, flags);
    pamret = pamk5_setcred(args, 0);

done:
    EXIT(args, pamret);
    pamk5_free(args);
    return pamret;
}


/*
 * The main PAM interface for closing a session.
 */
PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc,
                     const char **argv)
{
    struct pam_args *args;
    int pamret;

    args = pamk5_init(pamh, flags, argc, argv);
    if (args == NULL) {
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    ENTRY(args, flags);
    pamret = pam_set_data(pamh, "pam_krb5", NULL, NULL);
    if (pamret != PAM_SUCCESS)
        putil_err_pam(args, pamret, "cannot clear context data");

done:
    EXIT(args, pamret);
    pamk5_free(args);
    return pamret;
}


/* OpenPAM uses this macro to set up a table of entry points. */
#ifdef PAM_MODULE_ENTRY
PAM_MODULE_ENTRY("pam_krb5");
#endif
