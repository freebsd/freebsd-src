/* pam_item.c */

/*
 * $Id: pam_item.c,v 1.8 1997/02/15 15:58:49 morgan Exp morgan $
 * $FreeBSD$
 *
 * $Log: pam_item.c,v $
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "pam_private.h"

#define RESET(X, Y)                    \
{                                      \
    char *_TMP_ = (X);                 \
    if (_TMP_ != (Y)) {                \
	 (X) = (Y) ? _pam_strdup(Y) : NULL; \
	 if (_TMP_)                    \
	      free(_TMP_);             \
    }                                  \
}

/* functions */

int pam_set_item (
    pam_handle_t *pamh,
    int item_type,
    const void *item)
{
    int retval;

    D(("called"));

    IF_NO_PAMH("pam_set_item", pamh, PAM_SYSTEM_ERR);
    
    retval = PAM_SUCCESS;

    switch (item_type) {
    case PAM_SERVICE:
	/* Setting handlers_loaded to 0 will cause the handlers
	 * to be reloaded on the next call to a service module.
	 */
	pamh->handlers.handlers_loaded = 0;
	RESET(pamh->service_name, item);
	{
	    char *tmp;
	    for (tmp=pamh->service_name; *tmp; ++tmp)
		*tmp = tolower(*tmp);                 /* require lower case */
	}
	break;
    case PAM_USER:
	RESET(pamh->user, item);
	break;
    case PAM_USER_PROMPT:
	RESET(pamh->prompt, item);
	break;
    case PAM_TTY:
	D(("setting tty to %s", item));
	RESET(pamh->tty, item);
	break;
    case PAM_RUSER:
	RESET(pamh->ruser, item);
	break;
    case PAM_RHOST:
	RESET(pamh->rhost, item);
	break;
    case PAM_AUTHTOK:
	 /*
	  * The man page says this is only supposed to be available to
	  * the module providers.  In order to use this item the app
	  * has to #include <security/pam_modules.h>. This is something
	  * it is *not* supposed to do with "Linux-"PAM!  - AGM.
	  */
    {
	char *_TMP_ = pamh->authtok;
	if (_TMP_ == item)            /* not changed so leave alone */
	     break;
	pamh->authtok = (item) ? _pam_strdup(item) : NULL;
	if (_TMP_) {
	    _pam_overwrite(_TMP_);
	    free(_TMP_);
	}
	break;
    }
    case PAM_OLDAUTHTOK:
	 /* See note above. */
    {
	char *_TMP_ = pamh->oldauthtok;
	if (_TMP_ == item)            /* not changed so leave alone */
	     break;
	pamh->oldauthtok = (item) ? _pam_strdup(item) : NULL;
	if (_TMP_) {
	    _pam_overwrite(_TMP_);
	    free(_TMP_);
	}
	break;
    }
    case PAM_CONV:              /* want to change the conversation function */
	if (item == NULL) {
	    pam_system_log(pamh, NULL, LOG_ERR,
			   "pam_set_item: attempt to set conv() to NULL");
	    retval = PAM_PERM_DENIED;
	} else {
	    struct pam_conv *tconv;
	    
	    if ((tconv=
		 (struct pam_conv *) malloc(sizeof(struct pam_conv))
		) == NULL) {
		pam_system_log(pamh, NULL, LOG_CRIT,
			       "pam_set_item: malloc failed for pam_conv");
		retval = PAM_BUF_ERR;
	    } else {
		memcpy(tconv, item, sizeof(struct pam_conv));
		_pam_drop(pamh->pam_conversation);
		pamh->pam_conversation = tconv;
	    }
	}
        break;
    case PAM_FAIL_DELAY:
	pamh->fail_delay.delay_fn_ptr = item;
	break;
    case PAM_LOG_STATE:
    {
	char *old_ident = pamh->pam_default_log.ident;

	if (item == NULL) {
	    /* reset the default state */
	    pamh->pam_default_log.ident = x_strdup(PAM_LOG_STATE_IDENT);
	    pamh->pam_default_log.option = PAM_LOG_STATE_OPTION;
	    pamh->pam_default_log.facility = PAM_LOG_STATE_FACILITY;
	} else {
	    const struct pam_log_state *state = item;

	    pamh->pam_default_log.ident = x_strdup(state->ident);
	    pamh->pam_default_log.option = state->option;
	    pamh->pam_default_log.facility = state->facility;
	}
	_pam_overwrite(old_ident);
	_pam_drop(old_ident);

	break;
    }
    default:
	retval = PAM_BAD_ITEM;
    }

    return (retval);
}

int pam_get_item (
    const pam_handle_t *pamh,
    int item_type,
    const void **item)
{
    D(("called."));
    IF_NO_PAMH("pam_get_item",pamh,PAM_SYSTEM_ERR);

    if (item == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_get_item: nowhere to place requested item");
	return PAM_PERM_DENIED;
    }

    switch (item_type) {
    case PAM_SERVICE:
	*item = pamh->service_name;
	break;
    case PAM_USER:
	*item = pamh->user;
	break;
    case PAM_USER_PROMPT:
	*item = pamh->prompt;
	break;
    case PAM_TTY:
	D(("returning tty=%s", pamh->tty));
	*item = pamh->tty;
	break;
    case PAM_RUSER:
	*item = pamh->ruser;
	break;
    case PAM_RHOST:
	*item = pamh->rhost;
	break;
    case PAM_AUTHTOK:
	*item = pamh->authtok;
	break;
    case PAM_OLDAUTHTOK:
	*item = pamh->oldauthtok;
	break;
    case PAM_CONV:
	*item = pamh->pam_conversation;
	break;
    case PAM_FAIL_DELAY:
	*item = pamh->fail_delay.delay_fn_ptr;
	break;
    case PAM_LOG_STATE:
	*item = &(pamh->pam_default_log);
	break;
    default:
	/* XXX - I made this up */
	return PAM_BAD_ITEM;
    }
  
    return PAM_SUCCESS;
}

/* added by AGM 1996/3/2 */

int pam_get_user(pam_handle_t *pamh, const char **user, const char *prompt)
{
    const char *use_prompt;
    int retval;
    struct pam_message msg,*pmsg;
    struct pam_response *resp;

    D(("called."));
    IF_NO_PAMH("pam_get_user", pamh, PAM_SYSTEM_ERR);

    if (pamh->pam_conversation == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_get_user: no conv element in pamh");
	return PAM_SERVICE_ERR;
    }

    if (user == NULL) {  /* ensure the the module has suplied a destination */
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_get_user: nowhere to record username");
	return PAM_PERM_DENIED;
    } else
	*user = NULL;
    
    if (pamh->user) {    /* have one so return it */
	*user = pamh->user;
	return PAM_SUCCESS;
    }

    /* will need a prompt */
    use_prompt = prompt;
    if (use_prompt == NULL) {
	use_prompt = pamh->prompt;
	if (use_prompt == NULL) {
	    use_prompt = PAM_DEFAULT_PROMPT;
	}
    }

    /* If we are resuming an old conversation, we verify that the prompt
       is the same.  Anything else is an error. */
    if (pamh->former.want_user) {
	/* must have a prompt to resume with */
	if (! pamh->former.prompt) {
	    	    pam_system_log(pamh, NULL, LOG_ERR,
				   "pam_get_user: failed to resume with prompt"
			);
	    return PAM_ABORT;
	}

	/* must be the same prompt as last time */
	if (strcmp(pamh->former.prompt, use_prompt)) {
	    pam_system_log(pamh, NULL, LOG_ERR,
			   "pam_get_user: resumed with different prompt");
	    return PAM_ABORT;
	}

	/* ok, we can resume where we left off last time */
	pamh->former.want_user = PAM_FALSE;
	_pam_overwrite(pamh->former.prompt);
	_pam_drop(pamh->former.prompt);
    }

    /* converse with application -- prompt user for a username */
    pmsg = &msg;
    msg.msg_style = PAM_PROMPT_ECHO_ON;
    msg.msg = use_prompt;
    resp = NULL;

    retval = pamh->pam_conversation->
	conv(1, (const struct pam_message **) &pmsg, &resp,
	     pamh->pam_conversation->appdata_ptr);

    if (retval == PAM_CONV_AGAIN) {
	/* conversation function is waiting for an event - save state */
	D(("conversation function is not ready yet"));
	pamh->former.want_user = PAM_TRUE;
	pamh->former.prompt = _pam_strdup(use_prompt);
    } else if (resp == NULL) {
	/*
	 * conversation should have given a response
	 */
	D(("pam_get_user: no response provided"));
	retval = PAM_CONV_ERR;
    } else if (retval == PAM_SUCCESS) {            /* copy the username */
	/*
	 * now we set the PAM_USER item -- this was missing from pre.53
	 * releases. However, reading the Sun manual, it is part of
	 * the standard API.
	 */
	RESET(pamh->user, resp->resp);
	*user = pamh->user;
    }

    if (resp) {
	/*
	 * note 'resp' is allocated by the application and is
         * correctly free()'d here
	 */
	_pam_drop_reply(resp, 1);
    }

    return retval;        /* pass on any error from conversation */
}
