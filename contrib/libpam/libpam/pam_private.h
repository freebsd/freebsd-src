/*
 * pam_private.h
 *
 * $Id: pam_private.h,v 1.4 2001/02/05 06:50:41 agmorgan Exp $
 *
 * This is the Linux-PAM Library Private Header. It contains things
 * internal to the Linux-PAM library. Things not needed by either an
 * application or module.
 *
 * Please see end of file for copyright.
 *
 * Creator: Marc Ewing.
 * Maintained: CVS
 */

#ifndef _PAM_PRIVATE_H
#define _PAM_PRIVATE_H

#include <security/_pam_aconf.h>

/* this is not used at the moment --- AGM */
#define LIBPAM_VERSION (LIBPAM_VERSION_MAJOR*0x100 + LIBPAM_VERSION_MINOR)

#include <security/pam_appl.h>
#include <security/pam_modules.h>

/* the Linux-PAM configuration file */

#define PAM_CONFIG    "/etc/pam.conf"
#define PAM_CONFIG_D  "/etc/pam.d"
#define PAM_CONFIG_DF "/etc/pam.d/%s"

#define PAM_DEFAULT_SERVICE        "other"     /* lower case */
#define PAM_DEFAULT_SERVICE_FILE   PAM_CONFIG_D "/" PAM_DEFAULT_SERVICE

#ifdef PAM_LOCKING
/*
 * the Linux-PAM lock file. If it exists Linux-PAM will abort. Use it
 * to block access to libpam
 */
#define PAM_LOCK_FILE "/var/lock/subsys/PAM"
#endif

/* components of the pam_handle structure */

struct handler {
    int must_fail;
    int (*func)(pam_handle_t *pamh, int flags, int argc, char **argv);
    int actions[_PAM_RETURN_VALUES];
    /* set by authenticate, open_session, chauthtok(1st)
       consumed by setcred, close_session, chauthtok(2nd) */
    int cached_retval; int *cached_retval_p;
    int argc;
    char **argv;
    struct handler *next;
};

struct loaded_module {
    char *name;
    int type; /* PAM_STATIC_MOD or PAM_DYNAMIC_MOD */
    void *dl_handle;
};

#define PAM_MT_DYNAMIC_MOD 0
#define PAM_MT_STATIC_MOD  1
#define PAM_MT_FAULTY_MOD 2

struct handlers {
    struct handler *authenticate;
    struct handler *setcred;
    struct handler *acct_mgmt;
    struct handler *open_session;
    struct handler *close_session;
    struct handler *chauthtok;
};

struct service {
    struct loaded_module *module; /* Only used for dynamic loading */
    int modules_allocated;
    int modules_used;
    int handlers_loaded;

    struct handlers conf;        /* the configured handlers */
    struct handlers other;       /* the default handlers */
};

/*
 * Environment helper functions
 */

#define PAM_ENV_CHUNK         10 /* chunks of memory calloc()'d      *
				  * at once                          */

struct pam_environ {
    int entries;                 /* the number of pointers available */
    int requested;               /* the number of pointers used:     *
				  *     1 <= requested <= entries    */
    char **list;                 /* the environment storage (a list  *
				  * of pointers to malloc() memory)  */
};

#include <sys/time.h>

typedef enum { PAM_FALSE, PAM_TRUE } _pam_boolean;

struct _pam_fail_delay {
    _pam_boolean set;
    unsigned int delay;
    time_t begin;
    const void *delay_fn_ptr;
};

struct _pam_former_state {
/* this is known and set by _pam_dispatch() */
    int choice;            /* which flavor of module function did we call? */

/* state info for the _pam_dispatch_aux() function */
    int depth;             /* how deep in the stack were we? */
    int impression;        /* the impression at that time */
    int status;            /* the status before returning incomplete */

/* state info used by pam_get_user() function */
    int want_user;
    char *prompt;          /* saved prompt information */

/* state info for the pam_chauthtok() function */
    _pam_boolean update;
};

struct pam_handle {
    char *authtok;
    unsigned caller_is;
    struct pam_conv *pam_conversation;
    char *oldauthtok;
    char *prompt;                /* for use by pam_get_user() */
    char *service_name;
    char *user;
    char *rhost;
    char *ruser;
    char *tty;
    struct pam_data *data;
    struct pam_environ *env;      /* structure to maintain environment list */
    struct _pam_fail_delay fail_delay;   /* helper function for easy delays */
    struct service handlers;
    struct _pam_former_state former;  /* library state - support for
					 event driven applications */
};

/* Values for select arg to _pam_dispatch() */
#define PAM_NOT_STACKED   0
#define PAM_AUTHENTICATE  1
#define PAM_SETCRED       2
#define PAM_ACCOUNT       3
#define PAM_OPEN_SESSION  4
#define PAM_CLOSE_SESSION 5
#define PAM_CHAUTHTOK     6

#define _PAM_ACTION_IS_JUMP(x)  ((x) > 0)
#define _PAM_ACTION_IGNORE      0
#define _PAM_ACTION_OK         -1
#define _PAM_ACTION_DONE       -2
#define _PAM_ACTION_BAD        -3
#define _PAM_ACTION_DIE        -4
#define _PAM_ACTION_RESET      -5
/* Add any new entries here.  Will need to change ..._UNDEF and then
 * need to change pam_tokens.h */
#define _PAM_ACTION_UNDEF      -6   /* this is treated as an error
				       ( = _PAM_ACTION_BAD) */

/* character tables for parsing config files */
extern const char * const _pam_token_actions[-_PAM_ACTION_UNDEF];
extern const char * const _pam_token_returns[_PAM_RETURN_VALUES+1];

/*
 * internally defined functions --- these should not be directly
 * called by applications or modules
 */
int _pam_dispatch(pam_handle_t *pamh, int flags, int choice);

/* Free various allocated structures and dlclose() the libs */
int _pam_free_handlers(pam_handle_t *pamh);

/* Parse config file, allocate handler structures, dlopen() */
int _pam_init_handlers(pam_handle_t *pamh);

/* Set all hander stuff to 0/NULL - called once from pam_start() */
void _pam_start_handlers(pam_handle_t *pamh);

/* environment helper functions */

/* create the environment structure */
int _pam_make_env(pam_handle_t *pamh);

/* delete the environment structure */
void _pam_drop_env(pam_handle_t *pamh);

/* these functions deal with failure delays as required by the
   authentication modules and application. Their *interface* is likely
   to remain the same although their function is hopefully going to
   improve */

/* reset the timer to no-delay */
void _pam_reset_timer(pam_handle_t *pamh);

/* this sets the clock ticking */
void _pam_start_timer(pam_handle_t *pamh);

/* this waits for the clock to stop ticking if status != PAM_SUCCESS */
void _pam_await_timer(pam_handle_t *pamh, int status);

typedef void (*voidfunc(void))(void);
#ifdef PAM_STATIC

/* The next two in ../modules/_pam_static/pam_static.c */

/* Return pointer to data structure used to define a static module */
struct pam_module * _pam_open_static_handler(const char *path);

/* Return pointer to function requested from static module */

voidfunc *_pam_get_static_sym(struct pam_module *mod, const char *symname);

#endif

/* For now we just use a stack and linear search for module data. */
/* If it becomes apparent that there is a lot of data, it should  */
/* changed to either a sorted list or a hash table.               */

struct pam_data {
     char *name;
     void *data;
     void (*cleanup)(pam_handle_t *pamh, void *data, int error_status);
     struct pam_data *next;
};

void _pam_free_data(pam_handle_t *pamh, int status);

int _pam_strCMP(const char *s, const char *t);
char *_pam_StrTok(char *from, const char *format, char **next);

char *_pam_strdup(const char *s);

int _pam_mkargv(char *s, char ***argv, int *argc);

void _pam_sanitize(pam_handle_t *pamh);

void _pam_set_default_control(int *control_array, int default_action);

void _pam_parse_control(int *control_array, char *tok);

void _pam_system_log(int priority, const char *format,  ... );
#define _PAM_SYSTEM_LOG_PREFIX "PAM "

/*
 * XXX - Take care with this. It could confuse the logic of a trailing
 *       else
 */

#define IF_NO_PAMH(X,pamh,ERR)                    \
if ((pamh) == NULL) {                             \
    _pam_system_log(LOG_ERR, X ": NULL pam handle passed"); \
    return ERR;                                   \
}

/* Definition for the default username prompt used by pam_get_user() */

#define PAM_DEFAULT_PROMPT "Please enter username: "

/*
 * include some helpful macros
 */

#include <security/_pam_macros.h>

/* used to work out where control currently resides (in an application
   or in a module) */

#define _PAM_CALLED_FROM_MODULE         1
#define _PAM_CALLED_FROM_APP            2

#define __PAM_FROM_MODULE(pamh)  ((pamh)->caller_is == _PAM_CALLED_FROM_MODULE)
#define __PAM_FROM_APP(pamh)     ((pamh)->caller_is == _PAM_CALLED_FROM_APP)
#define __PAM_TO_MODULE(pamh) \
        do { (pamh)->caller_is = _PAM_CALLED_FROM_MODULE; } while (0)
#define __PAM_TO_APP(pamh)    \
        do { (pamh)->caller_is = _PAM_CALLED_FROM_APP; } while (0)

/*
 * Copyright (C) 1995 by Red Hat Software, Marc Ewing
 * Copyright (c) 1996-8,2001 by Andrew G. Morgan <morgan@kernel.org>
 *
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#endif /* _PAM_PRIVATE_H_ */
