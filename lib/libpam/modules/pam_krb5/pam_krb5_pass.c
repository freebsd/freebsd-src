/*
 * pam_krb5_pass.c
 *
 * PAM password management functions for pam_krb5
 *
 * $FreeBSD$
 */

static const char rcsid[] = "$Id: pam_krb5_pass.c,v 1.3 1999/01/19 23:43:11 fcusack Exp $";

#include <errno.h>
#include <stdio.h>	/* sprintf */
#include <stdlib.h>	/* malloc */
#include <syslog.h>	/* syslog */
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <krb5.h>
#include <com_err.h>
#include "pam_krb5.h"

/* A useful logging macro */
#define DLOG(error_func, error_msg) \
if (debug) \
    syslog(LOG_DEBUG, "pam_krb5: pam_sm_chauthtok(%s %s): %s: %s", \
	   service, name, error_func, error_msg)

/* Change a user's password */
int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    krb5_error_code	krbret;
    krb5_context	pam_context;
    krb5_creds		creds;
    krb5_principal	princ;
    krb5_get_init_creds_opt opts;

    int		result_code;
    krb5_data	result_code_string, result_string;

    int		pamret, i;
    char	*name, *service = NULL, *pass = NULL, *pass2;
    char	*princ_name = NULL;
    char	*prompt = NULL;

    int debug = 0;
    int try_first_pass = 0, use_first_pass = 0;

    if (!(flags & PAM_UPDATE_AUTHTOK))
	return PAM_AUTHTOK_ERR;

    for (i = 0; i < argc; i++) {
	if (strcmp(argv[i], "debug") == 0)
	    debug = 1;
	else if (strcmp(argv[i], "try_first_pass") == 0)
	    try_first_pass = 1;
	else if (strcmp(argv[i], "use_first_pass") == 0)
	    use_first_pass = 1;
    }

    /* Get username */
    if ((pam_get_item(pamh, PAM_USER, (const void **) &name)) != 0) {
	return PAM_SERVICE_ERR;
    }

    /* Get service name */
    (void) pam_get_item(pamh, PAM_SERVICE, (const void **) &service);
    if (!service)
	service = "unknown";

    DLOG("entry", "");

    if ((krbret = krb5_init_context(&pam_context)) != 0) {
	DLOG("krb5_init_context()", error_message(krbret));
	return PAM_SERVICE_ERR;
    }

    if ((krbret = krb5_init_context(&pam_context)) != 0) {
	DLOG("krb5_init_context()", error_message(krbret));
	return PAM_SERVICE_ERR;
    }
    krb5_get_init_creds_opt_init(&opts);
    memset(&creds, 0, sizeof(krb5_creds));

    /* Get principal name */
    if ((krbret = krb5_parse_name(pam_context, name, &princ)) != 0) {
	DLOG("krb5_parse_name()", error_message(krbret));
	pamret = PAM_USER_UNKNOWN;
	goto cleanup3;
    }

    /* Now convert the principal name into something human readable */
    if ((krbret = krb5_unparse_name(pam_context, princ, &princ_name)) != 0) {
	DLOG("krb5_unparse_name()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
	goto cleanup2;
    }

    /* Get password */
    prompt = malloc(16 + strlen(princ_name));
    if (!prompt) {
	DLOG("malloc()", "failure");
	pamret = PAM_BUF_ERR;
	goto cleanup2;
    }
    (void) sprintf(prompt, "Password for %s: ", princ_name);

    if (try_first_pass || use_first_pass)
	(void) pam_get_item(pamh, PAM_AUTHTOK, (const void **) &pass);

get_pass:
    if (!pass) {
	try_first_pass = 0;
	if ((pamret = get_user_info(pamh, prompt, PAM_PROMPT_ECHO_OFF, 
	  &pass)) != 0) {
	    DLOG("get_user_info()", pam_strerror(pamh, pamret));
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup2;
	}
	/* We have to free pass. */
	if ((pamret = pam_set_item(pamh, PAM_AUTHTOK, pass)) != 0) {
	    DLOG("pam_set_item()", pam_strerror(pamh, pamret));
	    free(pass);
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup2;
	}
	free(pass);
	/* Now we get it back from the library. */
	(void) pam_get_item(pamh, PAM_AUTHTOK, (const void **) &pass);
    }

    if ((krbret = krb5_get_init_creds_password(pam_context, &creds, princ, 
      pass, pam_prompter, pamh, 0, "kadmin/changepw", &opts)) != 0) {
	DLOG("krb5_get_init_creds_password()", error_message(krbret));
	if (try_first_pass && krbret == KRB5KRB_AP_ERR_BAD_INTEGRITY) {
	    pass = NULL;
	    goto get_pass;
	}
	pamret = PAM_AUTH_ERR;
	goto cleanup2;
    }

    /* Now get the new password */
    free(prompt);
    prompt = "Enter new password: ";
    if ((pamret = get_user_info(pamh, prompt, PAM_PROMPT_ECHO_OFF, &pass)) 
      != 0) {
	DLOG("get_user_info()", pam_strerror(pamh, pamret));
	prompt = NULL;
	pamret = PAM_SERVICE_ERR;
	goto cleanup;
    }
    prompt = "Enter it again: ";
    if ((pamret = get_user_info(pamh, prompt, PAM_PROMPT_ECHO_OFF, &pass2)) 
      != 0) {
	DLOG("get_user_info()", pam_strerror(pamh, pamret));
	prompt = NULL;
	pamret = PAM_SERVICE_ERR;
	goto cleanup;
    }
    prompt = NULL;

    if (strcmp(pass, pass2) != 0) {
	DLOG("strcmp()", "passwords not equal");
        pamret = PAM_AUTHTOK_ERR;
	goto cleanup;
    }

    /* Change it */
    if ((krbret = krb5_change_password(pam_context, &creds, pass,
      &result_code, &result_code_string, &result_string)) != 0) {
	DLOG("krb5_change_password()", error_message(krbret));
	pamret = PAM_AUTHTOK_ERR;
	goto cleanup;
    }
    if (result_code) {
	DLOG("krb5_change_password() (result_code)", "");
	pamret = PAM_AUTHTOK_ERR;
	goto cleanup;
    }

    if (result_string.data)
	free(result_string.data);
    if (result_code_string.data)
	free(result_code_string.data);

cleanup:
    krb5_free_cred_contents(pam_context, &creds);
cleanup2:
    krb5_free_principal(pam_context, princ);
cleanup3:
    if (prompt)
	free(prompt);
    if (princ_name)
	free(princ_name);

    krb5_free_context(pam_context);
    DLOG("exit", pamret ? "failure" : "success");
    return pamret;
}

