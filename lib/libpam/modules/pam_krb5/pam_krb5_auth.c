/*
 * pam_krb5_auth.c
 *
 * PAM authentication management functions for pam_krb5
 *
 * $FreeBSD$
 */

static const char rcsid[] = "$Id: pam_krb5_auth.c,v 1.18 2000/01/04 08:44:08 fcusack Exp $";

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>	/* PATH_MAX */
#include <pwd.h>	/* getpwnam */
#include <stdio.h>	/* tmpnam */
#include <stdlib.h>	/* malloc  */
#include <strings.h>	/* strchr */
#include <syslog.h>	/* syslog */
#include <unistd.h>	/* chown */

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <krb5.h>
#include <com_err.h>
#include "pam_krb5.h"

extern krb5_cc_ops krb5_mcc_ops;

/* A useful logging macro */
#define DLOG(error_func, error_msg) \
if (debug) \
    syslog(LOG_DEBUG, "pam_krb5: pam_sm_authenticate(%s %s): %s: %s", \
	   service, name, error_func, error_msg)

/* Authenticate a user via krb5 */
int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
		    const char **argv)
{
    krb5_error_code	krbret;
    krb5_context	pam_context;
    krb5_creds		creds;
    krb5_principal	princ;
    krb5_ccache		ccache, ccache_check;
    krb5_get_init_creds_opt opts;

    int			pamret, i;
    const char		*name;
    char		*source_princ = NULL;
    char		*princ_name = NULL;
    char		*pass = NULL, *service = NULL;
    char		*prompt = NULL;
    char		cache_name[L_tmpnam + 8];
    char		lname[64]; /* local acct name */
    struct passwd	*pw;
    uid_t		ruid;

    int debug = 0, try_first_pass = 0, use_first_pass = 0;
    int forwardable = 0, reuse_ccache = 0, no_ccache = 0;

    for (i = 0; i < argc; i++) {
	if (strcmp(argv[i], "debug") == 0)
	    debug = 1;
	else if (strcmp(argv[i], "try_first_pass") == 0)
	    try_first_pass = 1;
	else if (strcmp(argv[i], "use_first_pass") == 0)
	    use_first_pass = 1;
	else if (strcmp(argv[i], "forwardable") == 0)
	    forwardable = 1;
	else if (strcmp(argv[i], "reuse_ccache") == 0)
	    reuse_ccache = 1;
	else if (strcmp(argv[i], "no_ccache") == 0)
	    no_ccache = 1;
    }

    /* Get username */
    if ((pamret = pam_get_user(pamh, &name, "login: ")) != PAM_SUCCESS) {
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
    krb5_get_init_creds_opt_init(&opts);
    memset(&creds, 0, sizeof(krb5_creds));
    memset(cache_name, 0, sizeof(cache_name));
    memset(lname, 0, sizeof(lname));

    if (forwardable)
	krb5_get_init_creds_opt_set_forwardable(&opts, 1);

    /* For CNS */
    if ((krbret = krb5_cc_register(pam_context, &krb5_mcc_ops, FALSE)) != 0) {
	/* Solaris dtlogin doesn't call pam_end() on failure */
	if (krbret != KRB5_CC_TYPE_EXISTS) {
	    DLOG("krb5_cc_register()", error_message(krbret));
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup3;
	}
    }

    /* Get principal name */
    /* This case is for use mainly by su.
       If non-root is authenticating as "root", use "source_user/root".  */
    if (!strcmp(name, "root") && (ruid = getuid()) != 0) {
	pw = getpwuid(ruid);
	if (pw != NULL)
	    source_princ = (char *)malloc(strlen(pw->pw_name) + 6);
	if (source_princ)
	    sprintf(source_princ, "%s/root", pw->pw_name);
    } else {
	source_princ = strdup(name);
    }
    if (!source_princ) {
	DLOG("malloc()", "failure");
	pamret = PAM_BUF_ERR;
	goto cleanup2;
    }

    if ((krbret = krb5_parse_name(pam_context, source_princ, &princ)) != 0) {
	DLOG("krb5_parse_name()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
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

    /* get a local account name for this principal */
    if ((krbret = krb5_aname_to_localname(pam_context, princ, 
					  sizeof(lname), lname)) == 0) {
	DLOG("changing PAM_USER to", lname);
	if ((pamret = pam_set_item(pamh, PAM_USER, lname)) != 0) {
	    DLOG("pam_set_item()", pam_strerror(pamh, pamret));
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup2;
	}
	if ((pamret = pam_get_item(pamh, PAM_USER, (const void **) &name)
	  != 0)) {
	    DLOG("pam_get_item()", pam_strerror(pamh, pamret));
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup2;
	}
    } else {
	DLOG("krb5_aname_to_localname()", error_message(krbret));
	/* Not an error.  */
    }

    /* Verify the local user exists (AFTER getting the password) */
    pw = getpwnam(name);
    if (!pw) {
	DLOG("getpwnam()", lname);
	pamret = PAM_USER_UNKNOWN;
	goto cleanup2;
    }

    /* Get a TGT */
    if ((krbret = krb5_get_init_creds_password(pam_context, &creds, princ,
      pass, pam_prompter, pamh, 0, NULL, &opts)) != 0) {
	DLOG("krb5_get_init_creds_password()", error_message(krbret));
	if (try_first_pass && krbret == KRB5KRB_AP_ERR_BAD_INTEGRITY) {
	    pass = NULL;
	    goto get_pass;
	}
	pamret = PAM_AUTH_ERR;
	goto cleanup2;
    }

    /* Generate a unique cache_name */
    strcpy(cache_name, "MEMORY:");
    (void) tmpnam(&cache_name[7]);

    if ((krbret = krb5_cc_resolve(pam_context, cache_name, &ccache)) != 0) {
	DLOG("krb5_cc_resolve()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
	goto cleanup;
    }
    if ((krbret = krb5_cc_initialize(pam_context, ccache, princ)) != 0) {
	DLOG("krb5_cc_initialize()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
 	goto cleanup;
    }
    if ((krbret = krb5_cc_store_cred(pam_context, ccache, &creds)) != 0) {
	DLOG("krb5_cc_store_cred()", error_message(krbret));
	(void) krb5_cc_destroy(pam_context, ccache);
	pamret = PAM_SERVICE_ERR;
	goto cleanup;
    }

    /* Verify it */
    if (verify_krb_v5_tgt(pam_context, ccache, service, debug) == -1) {
	(void) krb5_cc_destroy(pam_context, ccache);
	pamret = PAM_AUTH_ERR;
	goto cleanup;
    }

    /* A successful authentication, store ccache for sm_setcred() */
    if (!pam_get_data(pamh, "ccache", (const void **) &ccache_check)) {
	DLOG("pam_get_data()", "ccache data already present");
	(void) krb5_cc_destroy(pam_context, ccache);
	pamret = PAM_AUTH_ERR;
	goto cleanup;
    }
    if ((pamret = pam_set_data(pamh, "ccache", ccache, cleanup_cache)) != 0) {
	DLOG("pam_set_data()", pam_strerror(pamh, pamret));
	(void) krb5_cc_destroy(pam_context, ccache);
	pamret = PAM_SERVICE_ERR;
	goto cleanup;
    }

cleanup:
    krb5_free_cred_contents(pam_context, &creds);
cleanup2:
    krb5_free_principal(pam_context, princ);
cleanup3:
    if (prompt)
	free(prompt);
    if (princ_name)
	free(princ_name);
    if (source_princ)
	free(source_princ);

    krb5_free_context(pam_context);
    DLOG("exit", pamret ? "failure" : "success");
    return pamret;
}



/* redefine this for pam_sm_setcred() */
#undef DLOG
#define DLOG(error_func, error_msg) \
if (debug) \
    syslog(LOG_DEBUG, "pam_krb5: pam_sm_setcred(%s %s): %s: %s", \
	   service, name, error_func, error_msg)

/* Called after a successful authentication. Set user credentials. */
int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
	       const char **argv)
{

    krb5_error_code	krbret;
    krb5_context	pam_context;
    krb5_principal	princ;
    krb5_creds		creds;
    krb5_ccache		ccache_temp, ccache_perm;
    krb5_cc_cursor	cursor;

    int			i, pamret;
    char		*name, *service = NULL;
    char		*cache_name = NULL, *cache_env_name;
    struct passwd	*pw = NULL;

    int		debug = 0;
    uid_t	euid;
    gid_t	egid;

    if (flags == PAM_REINITIALIZE_CRED)
	return PAM_SUCCESS; /* XXX Incorrect behavior */

    if (flags != PAM_ESTABLISH_CRED && flags != PAM_DELETE_CRED)
	return PAM_SERVICE_ERR;

    for (i = 0; i < argc; i++) {
	if (strcmp(argv[i], "debug") == 0)
	    debug = 1;
	else if (strcmp(argv[i], "no_ccache") == 0)
	    return PAM_SUCCESS;
	else if (strstr(argv[i], "ccache=") == argv[i])
	    cache_name = (char *) &argv[i][7]; /* save for later */
    }

    /* Get username */
    if (pam_get_item(pamh, PAM_USER, (const void **) &name)) {
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

    euid = geteuid(); /* Usually 0 */
    egid = getegid();

    /* Retrieve the cache name */
    if ((pamret = pam_get_data(pamh, "ccache", (const void **) &ccache_temp)) 
      != 0) {
	/* User did not use krb5 to login */
	DLOG("ccache", "not found");
	pamret = PAM_SUCCESS;
	goto cleanup3;
    }

    /* Get the uid. This should exist. */
    pw = getpwnam(name);
    if (!pw) {
	DLOG("getpwnam()", name);
	pamret = PAM_USER_UNKNOWN;
	goto cleanup3;
    }

    /* Avoid following a symlink as root */
    if (setegid(pw->pw_gid)) {
	DLOG("setegid()", name); /* XXX should really log group name or id */
	pamret = PAM_SERVICE_ERR;
	goto cleanup3;
    }
    if (seteuid(pw->pw_uid)) {
	DLOG("seteuid()", name);
	pamret = PAM_SERVICE_ERR;
	goto cleanup3;
    }

    /* Get the cache name */
    if (!cache_name) {
	cache_name = malloc(64); /* plenty big */
	if (!cache_name) {
	    DLOG("malloc()", "failure");
	    pamret = PAM_BUF_ERR;
	    goto cleanup3;
	}
	sprintf(cache_name, "FILE:/tmp/krb5cc_%d", pw->pw_uid);
    } else {
	/* cache_name was supplied */
	char *p = calloc(PATH_MAX + 10, 1); /* should be plenty */
	char *q = cache_name;
	if (!p) {
	    DLOG("malloc()", "failure");
	    pamret = PAM_BUF_ERR;
	    goto cleanup3;
	}
	cache_name = p;
	
	/* convert %u and %p */
	while (*q) {
	    if (*q == '%') {
		q++;
		if (*q == 'u') {
		    sprintf(p, "%d", pw->pw_uid);
		    p += strlen(p);
		} else if (*q == 'p') {
		    sprintf(p, "%d", getpid());
		    p += strlen(p);
		} else {
		    /* Not a special token */
		    *p++ = '%';
		    q--;
		}
		q++;
	    } else {
		*p++ = *q++;
	    }
	}
    }

    if ((krbret = krb5_cc_resolve(pam_context, cache_name, &ccache_perm)) 
      != 0) {
	DLOG("krb5_cc_resolve()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
	goto cleanup3;
    }
    if (flags == PAM_ESTABLISH_CRED) {
    /* Initialize the new ccache */
    if ((krbret = krb5_cc_get_principal(pam_context, ccache_temp, &princ)) 
      != 0) {
	DLOG("krb5_cc_get_principal()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
	goto cleanup3;
    }
    if ((krbret = krb5_cc_initialize(pam_context, ccache_perm, princ)) != 0) {
	DLOG("krb5_cc_initialize()", error_message(krbret));
	pamret = PAM_SERVICE_ERR;
 	goto cleanup2;
    }

    /* Prepare for iteration over creds */
    if ((krbret = krb5_cc_start_seq_get(pam_context, ccache_temp, &cursor)) 
      != 0) {
	DLOG("krb5_cc_start_seq_get()", error_message(krbret));
	(void) krb5_cc_destroy(pam_context, ccache_perm);
	pamret = PAM_SERVICE_ERR;
	goto cleanup2;
    }

    /* Copy the creds (should be two of them) */
    while ((krbret = compat_cc_next_cred(pam_context, ccache_temp,
	&cursor, &creds) == 0)) {
	    if ((krbret = krb5_cc_store_cred(pam_context, ccache_perm, 
		&creds)) != 0) {
	    DLOG("krb5_cc_store_cred()", error_message(krbret));
	    (void) krb5_cc_destroy(pam_context, ccache_perm);
	    krb5_free_cred_contents(pam_context, &creds);
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup2;
	}
	krb5_free_cred_contents(pam_context, &creds);
    }
    (void) krb5_cc_end_seq_get(pam_context, ccache_temp, &cursor);

    if (strstr(cache_name, "FILE:") == cache_name) {
	if (chown(&cache_name[5], pw->pw_uid, pw->pw_gid) == -1) {
	    DLOG("chown()", strerror(errno));
	    (void) krb5_cc_destroy(pam_context, ccache_perm);
	    pamret = PAM_SERVICE_ERR;	
	    goto cleanup2;
	}
	if (chmod(&cache_name[5], (S_IRUSR|S_IWUSR)) == -1) {
	    DLOG("chmod()", strerror(errno));
	    (void) krb5_cc_destroy(pam_context, ccache_perm);
	    pamret = PAM_SERVICE_ERR;
	    goto cleanup2;
	}
    }
    (void) krb5_cc_close(pam_context, ccache_perm);

    cache_env_name = malloc(strlen(cache_name) + 12);
    if (!cache_env_name) {
	DLOG("malloc()", "failure");
	(void) krb5_cc_destroy(pam_context, ccache_perm);
	pamret = PAM_BUF_ERR;
	goto cleanup2;
    }

    sprintf(cache_env_name, "KRB5CCNAME=%s", cache_name);
    if ((pamret = pam_putenv(pamh, cache_env_name)) != 0) {
	DLOG("pam_putenv()", pam_strerror(pamh, pamret));
	(void) krb5_cc_destroy(pam_context, ccache_perm);
	pamret = PAM_SERVICE_ERR;
	goto cleanup2;
    }
    } else {
	/* flag == PAM_DELETE_CRED */
	if ((krbret = krb5_cc_destroy(pam_context, ccache_perm)) != 0) {
		/* log error, but otherwise ignore it */
		DLOG("krb5_cc_destroy()", error_message(krbret));
	}
	goto cleanup3;
    }

cleanup2:
    krb5_free_principal(pam_context, princ);
cleanup3:
    krb5_free_context(pam_context);
    DLOG("exit", pamret ? "failure" : "success");
    (void) seteuid(euid);
    (void) setegid(egid);
    return pamret;
}

