/* pam_dispatch.c - handles module function dispatch */

/*
 * $Id: pam_dispatch.c,v 1.8 1997/01/04 20:04:09 morgan Exp morgan $
 * $FreeBSD$
 *
 * last modified by AGM
 */

#include <stdlib.h>
#include <stdio.h>

#include "pam_private.h"

/*
 * this is the return code we return when a function pointer is NULL
 * or, the handler structure indicates a broken module config line
 */
#define PAM_MUST_FAIL_CODE        PAM_PERM_DENIED

/* impression codes - this gives some sense to the logical choices */
#define _PAM_UNDEF     0
#define _PAM_POSITIVE +1
#define _PAM_NEGATIVE -1

/*
 * walk a stack of modules.  Interpret the administrator's instructions
 * when combining the return code of each module.
 */

static int _pam_dispatch_aux(pam_handle_t *pamh, int flags, struct handler *h,
			     _pam_boolean resumed)
{
    int depth, impression, status, skip_depth;

    IF_NO_PAMH("_pam_dispatch_aux", pamh, PAM_SYSTEM_ERR);

    if (h == NULL) {
	const char *service=NULL;

	(void) pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
	pam_system_log(pamh, NULL, LOG_ERR,
		       "no modules loaded for `%s' service",
		       service ? service:"<unknown>" );
	service = NULL;
	return PAM_MUST_FAIL_CODE;
    }

    /* if we are recalling this module stack because a former call did
       not complete, we restore the state of play from pamh. */
    if (resumed) {
	skip_depth = pamh->former.depth;
	status = pamh->former.status;
	impression = pamh->former.impression;
	/* forget all that */
	pamh->former.impression = _PAM_UNDEF;
	pamh->former.status = PAM_MUST_FAIL_CODE;
	pamh->former.depth = 0;
    } else {
	skip_depth = 0;
	impression = _PAM_UNDEF;
	status = PAM_MUST_FAIL_CODE;
    }

    /* Loop through module logic stack */
    for (depth=0 ; h != NULL ; h = h->next, ++depth) {
	int retval, action;

	/* skip leading modules if they have already returned */
	if (depth < skip_depth) {
	    continue;
	}

	/* attempt to call the module */
	if (h->func == NULL) {
	    D(("module function is not defined, indicating failure"));
	    retval = PAM_MODULE_UNKNOWN;
	} else {
	    D(("passing control to module..."));
	    retval = h->func(pamh, flags, h->argc, h->argv);
	    D(("module returned: %s", pam_strerror(pamh, retval)));
	    if (h->must_fail) {
		D(("module poorly listed in pam.conf; forcing failure"));
		retval = PAM_MUST_FAIL_CODE;
	    }
	}

	/*
	 * PAM_INCOMPLETE return is special.  It indicates that the
	 * module wants to wait for the application before continuing.
	 * In order to return this, the module will have saved its
	 * state so it can resume from an equivalent position when it
	 * is called next time.  (This was added as of 0.65)
	 */
	if (retval == PAM_INCOMPLETE) {
	    pamh->former.impression = impression;
	    pamh->former.status = status;
	    pamh->former.depth = depth;

	    D(("module %d returned PAM_INCOMPLETE", depth));
	    return retval;
	}

	/* verify that the return value is a valid one */
	if (retval < PAM_SUCCESS || retval >= _PAM_RETURN_VALUES) {
	    retval = PAM_MUST_FAIL_CODE;
	    action = _PAM_ACTION_BAD;
	} else {
	    action = h->actions[retval];
	}

	/* decide what to do */
	switch (action) {
	case _PAM_ACTION_RESET:
	    impression = _PAM_UNDEF;
	    status = PAM_MUST_FAIL_CODE;
	    break;

	case _PAM_ACTION_OK:
	case _PAM_ACTION_DONE:
	    if ( impression == _PAM_UNDEF
		 || (impression == _PAM_POSITIVE && status == PAM_SUCCESS) ) {
		impression = _PAM_POSITIVE;
		status = retval;
	    }
	    if ( impression == _PAM_POSITIVE && action == _PAM_ACTION_DONE ) {
		goto decision_made;
	    }
	    break;

	case _PAM_ACTION_BAD:
	case _PAM_ACTION_DIE:
#ifdef PAM_FAIL_NOW_ON
	    if ( retval == PAM_ABORT ) {
		impression = _PAM_NEGATIVE;
		status = PAM_PERM_DENIED;
		goto decision_made;
	    }
#endif /* PAM_FAIL_NOW_ON */
	    if ( impression != _PAM_NEGATIVE ) {
		impression = _PAM_NEGATIVE;
		status = retval;
	    }
	    if ( action == _PAM_ACTION_DIE ) {
		goto decision_made;
	    }
	    break;

	case _PAM_ACTION_IGNORE:
	    break;

        /* if we get here, we expect action is a positive number --
           this is what the ...JUMP macro checks. */

	default:
	    if ( _PAM_ACTION_IS_JUMP(action) ) {
		/* this means that we need to skip #action stacked modules */
		do {
 		    h = h->next;
		} while ( --action > 0 && h != NULL );

		/* note if we try to skip too many modules action is
                   still non-zero and we snag the next if. */
	    }

	    /* this case is a syntax error: we can't succeed */
	    if (action) {
		D(("action syntax error"));
		impression = _PAM_NEGATIVE;
		status = PAM_MUST_FAIL_CODE;
	    }
	}
    }

decision_made:     /* by getting  here we have made a decision */

    /* Sanity check */
    if ( status == PAM_SUCCESS && impression != _PAM_POSITIVE ) {
	D(("caught on sanity check -- this is probably a config error!"));
	status = PAM_MUST_FAIL_CODE;
    }

    /* We have made a decision about the modules executed */
    return status;
}

/*
 * This function translates the module dispatch request into a pointer
 * to the stack of modules that will actually be run.  the
 * _pam_dispatch_aux() function (above) is responsible for walking the
 * module stack.
 */

int _pam_dispatch(pam_handle_t *pamh, int flags, int choice)
{
    struct handler *h = NULL;
    int retval;
    _pam_boolean resumed;

    IF_NO_PAMH("_pam_dispatch",pamh,PAM_SYSTEM_ERR);

    /* Load all modules, resolve all symbols */

    if ((retval = _pam_init_handlers(pamh)) != PAM_SUCCESS) {
	pam_system_log(pamh, NULL, LOG_ERR, "unable to dispatch function");
	return retval;
    }

    switch (choice) {
    case PAM_AUTHENTICATE:
	h = pamh->handlers.conf.authenticate;
	break;
    case PAM_SETCRED:
	h = pamh->handlers.conf.setcred;
	break;
    case PAM_ACCOUNT:
	h = pamh->handlers.conf.acct_mgmt;
	break;
    case PAM_OPEN_SESSION:
	h = pamh->handlers.conf.open_session;
	break;
    case PAM_CLOSE_SESSION:
	h = pamh->handlers.conf.close_session;
	break;
    case PAM_CHAUTHTOK:
	h = pamh->handlers.conf.chauthtok;
	break;
    default:
	pam_system_log(pamh, NULL, LOG_ERR, "undefined fn choice; %d", choice);
	return PAM_ABORT;
    }

    if (h == NULL) {     /* there was no handlers.conf... entry; will use
			  * handlers.other... */
	switch (choice) {
	case PAM_AUTHENTICATE:
	    h = pamh->handlers.other.authenticate;
	    break;
	case PAM_SETCRED:
	    h = pamh->handlers.other.setcred;
	    break;
	case PAM_ACCOUNT:
	    h = pamh->handlers.other.acct_mgmt;
	    break;
	case PAM_OPEN_SESSION:
	    h = pamh->handlers.other.open_session;
	    break;
	case PAM_CLOSE_SESSION:
	    h = pamh->handlers.other.close_session;
	    break;
	case PAM_CHAUTHTOK:
	    h = pamh->handlers.other.chauthtok;
	    break;
	}
    }

    /* Did a module return an "incomplete state" last time? */
    if (pamh->former.choice != PAM_NOT_STACKED) {
	if (pamh->former.choice != choice) {
	    pam_system_log(pamh, NULL, LOG_ERR,
			   "application failed to re-exec stack [%d:%d]",
			   pamh->former.choice, choice);
	    return PAM_ABORT;
	}
	resumed = PAM_TRUE;
    } else {
	resumed = PAM_FALSE;
    }

    /* call the list of module functions */
    retval = _pam_dispatch_aux(pamh, flags, h, resumed);
    resumed = PAM_FALSE;

    /* Should we recall where to resume next time? */
    if (retval == PAM_INCOMPLETE) {
	D(("module [%d] returned PAM_INCOMPLETE"));
	pamh->former.choice = choice;
    } else {
	pamh->former.choice = PAM_NOT_STACKED;
    }

    return retval;
}

/*
 * $Log: pam_dispatch.c,v $
 */
