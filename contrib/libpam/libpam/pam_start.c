/* pam_start.c */

/* Creator Marc Ewing
 * Maintained by AGM
 *
 * $Id: pam_start.c,v 1.10 1997/04/05 06:58:11 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_start.c,v $
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

#include "pam_private.h"

int pam_start (
    const char *service_name,
    const char *user,
    const struct pam_conv *pam_conversation,
    pam_handle_t **pamh)
{
    D(("called pam_start: [%s] [%s] [%p] [%p]"
       ,service_name, user, pam_conversation, pamh));

    if ((*pamh = calloc(1, sizeof(**pamh))) == NULL) {
	pam_system_log(NULL, NULL, LOG_CRIT,
		       "pam_start: calloc failed for *pamh");
	return (PAM_BUF_ERR);
    }

    if (service_name) {
	char *tmp;

	if (((*pamh)->service_name = _pam_strdup(service_name)) == NULL) {
	    pam_system_log(NULL, NULL, LOG_CRIT,
			   "pam_start: _pam_strdup failed for service name");
	    _pam_drop(*pamh);
	    return (PAM_BUF_ERR);
	}
	for (tmp=(*pamh)->service_name; *tmp; ++tmp)
	    *tmp = tolower(*tmp);                   /* require lower case */
    } else
       	(*pamh)->service_name = NULL;

    if (user) {
	if (((*pamh)->user = _pam_strdup(user)) == NULL) {
	    pam_system_log(NULL, NULL, LOG_CRIT,
			   "pam_start: _pam_strdup failed for user");
	    _pam_drop((*pamh)->service_name);
	    _pam_drop(*pamh);
	    return (PAM_BUF_ERR);
	}
    } else
	(*pamh)->user = NULL;

    (*pamh)->tty = NULL;
    (*pamh)->prompt = NULL;              /* prompt for pam_get_user() */
    (*pamh)->ruser = NULL;
    (*pamh)->rhost = NULL;
    (*pamh)->authtok = NULL;
    (*pamh)->oldauthtok = NULL;
    (*pamh)->fail_delay.delay_fn_ptr = NULL;
    (*pamh)->former.choice = PAM_NOT_STACKED;

    if (pam_conversation == NULL
	|| ((*pamh)->pam_conversation = (struct pam_conv *)
	    malloc(sizeof(struct pam_conv))) == NULL) {
	pam_system_log(NULL, NULL, LOG_CRIT,
		       "pam_start: malloc failed for pam_conv");
	_pam_drop((*pamh)->service_name);
	_pam_drop((*pamh)->user);
	_pam_drop(*pamh);
	return (PAM_BUF_ERR);
    } else {
	memcpy((*pamh)->pam_conversation, pam_conversation,
	       sizeof(struct pam_conv));
    }

    (*pamh)->data = NULL;
    if ( _pam_make_env(*pamh) != PAM_SUCCESS ) {
	pam_system_log(NULL, NULL, LOG_ERR,
		       "pam_start: failed to initialize environment");
	_pam_drop((*pamh)->service_name);
	_pam_drop((*pamh)->user);
	_pam_drop(*pamh);
	return PAM_ABORT;
    }

    _pam_reset_timer(*pamh);         /* initialize timer support */

    _pam_start_handlers(*pamh);                   /* cannot fail */

    /* According to the SunOS man pages, loading modules and resolving
     * symbols happens on the first call from the application. */

    /*
     * XXX - should we call _pam_init_handlers() here ? The following
     * is new as of Linux-PAM 0.55
     */

    if ( _pam_init_handlers(*pamh) != PAM_SUCCESS ) {
	pam_system_log(NULL, NULL, LOG_ERR,
		       "pam_start: failed to initialize handlers");
	_pam_drop_env(*pamh);                 /* purge the environment */
	_pam_drop((*pamh)->service_name);
	_pam_drop((*pamh)->user);
	_pam_drop(*pamh);
	return PAM_ABORT;
    }

    D(("exiting pam_start successfully"));

    return PAM_SUCCESS;
}
