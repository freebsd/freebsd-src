/* pam_end.c */

/*
 * $Id: pam_end.c,v 1.5 1996/12/01 03:14:13 morgan Exp $
 * $FreeBSD$
 *
 * $Log: pam_end.c,v $
 */

#include <stdlib.h>

#include "pam_private.h"

int pam_end(pam_handle_t *pamh, int pam_status)
{
    int ret;

    IF_NO_PAMH("pam_end", pamh, PAM_SYSTEM_ERR);

    D(("entering pam_end()"));

    /* first liberate the modules (it is not inconcevible that the
       modules may need to use the service_name etc. to clean up) */

    _pam_free_data(pamh, pam_status);

    /* now drop all modules */

    if ((ret = _pam_free_handlers(pamh)) != PAM_SUCCESS) {
	return ret;                 /* error occurred */
    }

    /* from this point we cannot call the modules any more. Free the remaining
       memory used by the Linux-PAM interface */

    _pam_drop_env(pamh);                      /* purge the environment */

    _pam_overwrite(pamh->authtok);            /* blank out old token */
    _pam_drop(pamh->authtok);

    _pam_overwrite(pamh->oldauthtok);         /* blank out old token */
    _pam_drop(pamh->oldauthtok);

    _pam_overwrite(pamh->former.prompt);
    _pam_drop(pamh->former.prompt);           /* drop saved prompt */

    _pam_overwrite(pamh->service_name);
    _pam_drop(pamh->service_name);

    _pam_overwrite(pamh->user);
    _pam_drop(pamh->user);

    _pam_overwrite(pamh->prompt);
    _pam_drop(pamh->prompt);                  /* prompt for pam_get_user() */

    _pam_overwrite(pamh->tty);
    _pam_drop(pamh->tty);

    _pam_overwrite(pamh->rhost);
    _pam_drop(pamh->rhost);

    _pam_overwrite(pamh->ruser);
    _pam_drop(pamh->ruser);

    _pam_drop(pamh->pam_conversation);
    pamh->fail_delay.delay_fn_ptr = NULL;

    _pam_overwrite(pamh->pam_default_log.ident);
    _pam_drop(pamh->pam_default_log.ident);

    /* and finally liberate the memory for the pam_handle structure */

    _pam_drop(pamh);

    D(("exiting pam_end() successfully"));

    return PAM_SUCCESS;
}
