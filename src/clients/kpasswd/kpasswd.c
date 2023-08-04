/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "k5-platform.h"
#include <locale.h>
#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <krb5.h>

#define P1 _("Enter new password")
#define P2 _("Enter it again")

#ifdef HAVE_PWD_H
#include <pwd.h>

static void
get_name_from_passwd_file(char *program_name, krb5_context context,
                          krb5_principal *me)
{
    struct passwd *pw;
    krb5_error_code ret;

    pw = getpwuid(getuid());
    if (pw != NULL) {
        ret = krb5_parse_name(context, pw->pw_name, me);
        if (ret) {
            com_err(program_name, ret, _("when parsing name %s"), pw->pw_name);
            exit(1);
        }
    } else {
        fprintf(stderr, _("Unable to identify user from password file\n"));
        exit(1);
    }
}
#else /* HAVE_PWD_H */
static void
get_name_from_passwd_file(char *program_name, krb5_context context,
                          krb5_principal *me)
{
    fprintf(stderr, _("Unable to identify user\n"));
    exit(1);
}
#endif /* HAVE_PWD_H */

int main(int argc, char *argv[])
{
    krb5_error_code ret;
    krb5_context context;
    krb5_principal princ = NULL;
    char *pname, *message;
    char pw[1024];
    krb5_ccache ccache;
    krb5_get_init_creds_opt *opts = NULL;
    krb5_creds creds;
    unsigned int pwlen;
    int result_code;
    krb5_data result_code_string, result_string;

    setlocale(LC_ALL, "");
    if (argc > 2) {
        fprintf(stderr, _("usage: %s [principal]\n"), argv[0]);
        exit(1);
    }

    pname = argv[1];

    ret = krb5_init_context(&context);
    if (ret) {
        com_err(argv[0], ret, _("initializing kerberos library"));
        exit(1);
    }
    ret = krb5_get_init_creds_opt_alloc(context, &opts);
    if (ret) {
        com_err(argv[0], ret, _("allocating krb5_get_init_creds_opt"));
        exit(1);
    }

    /*
     * In order, use the first of:
     * - A name specified on the command line
     * - The principal name from an existing ccache
     * - The name corresponding to the ruid of the process
     *
     * Otherwise, it's an error.
     * We always attempt to open the default ccache in order to use FAST if
     * possible.
     */
    ret = krb5_cc_default(context, &ccache);
    if (ret) {
        com_err(argv[0], ret, _("opening default ccache"));
        exit(1);
    }
    ret = krb5_cc_get_principal(context, ccache, &princ);
    if (ret && ret != KRB5_CC_NOTFOUND && ret != KRB5_FCC_NOFILE) {
        com_err(argv[0], ret, _("getting principal from ccache"));
        exit(1);
    } else if (princ != NULL) {
        ret = krb5_get_init_creds_opt_set_fast_ccache(context, opts, ccache);
        if (ret) {
            com_err(argv[0], ret, _("while setting FAST ccache"));
            exit(1);
        }
    }
    ret = krb5_cc_close(context, ccache);
    if (ret) {
        com_err(argv[0], ret, _("closing ccache"));
        exit(1);
    }
    if (pname != NULL) {
        krb5_free_principal(context, princ);
        princ = NULL;
        ret = krb5_parse_name(context, pname, &princ);
        if (ret) {
            com_err(argv[0], ret, _("parsing client name"));
            exit(1);
        }
    }
    if (princ == NULL)
        get_name_from_passwd_file(argv[0], context, &princ);

    krb5_get_init_creds_opt_set_tkt_life(opts, 5 * 60);
    krb5_get_init_creds_opt_set_renew_life(opts, 0);
    krb5_get_init_creds_opt_set_forwardable(opts, 0);
    krb5_get_init_creds_opt_set_proxiable(opts, 0);

    ret = krb5_get_init_creds_password(context, &creds, princ, NULL,
                                       krb5_prompter_posix, NULL, 0,
                                       "kadmin/changepw", opts);
    if (ret) {
        if (ret == KRB5KRB_AP_ERR_BAD_INTEGRITY) {
            com_err(argv[0], 0,
                    _("Password incorrect while getting initial ticket"));
        } else {
            com_err(argv[0], ret, _("getting initial ticket"));
        }

        krb5_get_init_creds_opt_free(context, opts);
        exit(1);
    }

    pwlen = sizeof(pw);
    ret = krb5_read_password(context, P1, P2, pw, &pwlen);
    if (ret) {
        com_err(argv[0], ret, _("while reading password"));
        krb5_get_init_creds_opt_free(context, opts);
        exit(1);
    }

    ret = krb5_change_password(context, &creds, pw, &result_code,
                               &result_code_string, &result_string);
    if (ret) {
        com_err(argv[0], ret, _("changing password"));
        krb5_get_init_creds_opt_free(context, opts);
        exit(1);
    }

    if (result_code) {
        if (krb5_chpw_message(context, &result_string, &message) != 0)
            message = NULL;
        printf("%.*s%s%s\n",
               (int)result_code_string.length, result_code_string.data,
               message ? ": " : "", message ? message : NULL);
        krb5_free_string(context, message);
        krb5_get_init_creds_opt_free(context, opts);
        exit(2);
    }

    free(result_string.data);
    free(result_code_string.data);
    krb5_get_init_creds_opt_free(context, opts);

    printf(_("Password changed.\n"));
    exit(0);
}
