/*
 * pam_env.c
 *
 * Copyright (c) Andrew G. Morgan <morgan@parc.power.net> 1996,1997
 * All rights reserved.
 *
 * This file was written from a "hint" provided by the people at SUN.
 * and the X/Open XSSO draft of March 1997.
 *
 * $Id: pam_env.c,v 1.2 1997/02/15 15:56:48 morgan Exp morgan $
 * $FreeBSD$
 *
 * $Log: pam_env.c,v $
 * Revision 1.2  1997/02/15 15:56:48  morgan
 * liberate pamh->env structure too!
 *
 * Revision 1.1  1996/12/01 03:14:13  morgan
 * Initial revision
 */

#include <string.h>
#include <stdlib.h>
#ifdef sunos
#define memmove(x,y,z) bcopy(y,x,z)
#endif

#include "pam_private.h"

/* helper functions */

#ifdef DEBUG
static void _pam_dump_env(pam_handle_t *pamh)
{
    int i;

    D(("Listing environment of pamh=%p", pamh));
    D(("pamh->env = %p", pamh->env));
    D(("environment entries used = %d [of %d allocated]"
       , pamh->env->requested, pamh->env->entries));

    for (i=0; i<pamh->env->requested; ++i) {
	_pam_output_debug(">%-3d [%9p]:[%s]"
			  , i, pamh->env->list[i], pamh->env->list[i]);
    }
    _pam_output_debug("*NOTE* the last item should be (nil)");
}
#else
#define _pam_dump_env(x)
#endif

/*
 * Create the environment
 */

int _pam_make_env(pam_handle_t *pamh)
{
    D(("called."));
    IF_NO_PAMH("_pam_make_env", pamh, PAM_ABORT);

    /*
     * get structure memory
     */

    pamh->env = (struct pam_environ *) malloc(sizeof(struct pam_environ));
    if (pamh->env == NULL) {
	pam_system_log(pamh, NULL, LOG_CRIT, "_pam_make_env: out of memory");
	return PAM_BUF_ERR;
    }

    /*
     * get list memory
     */

    pamh->env->list = (char **)calloc( PAM_ENV_CHUNK, sizeof(char *) );
    if (pamh->env->list == NULL) {
	pam_system_log(pamh, NULL, LOG_CRIT,
		       "_pam_make_env: no memory for list");
	_pam_drop(pamh->env);
	return PAM_BUF_ERR;
    }

    /*
     * fill entries in pamh->env
     */
    
    pamh->env->entries = PAM_ENV_CHUNK;
    pamh->env->requested = 1;
    pamh->env->list[0] = NULL;

    _pam_dump_env(pamh);                    /* only active when debugging */

    return PAM_SUCCESS;
}

/*
 * purge the environment
 */

void _pam_drop_env(pam_handle_t *pamh)
{
    D(("called."));
    IF_NO_PAMH("_pam_make_env", pamh, /* nothing to return */);

    if (pamh->env != NULL) {
	int i;
	/* we will only purge the pamh->env->requested number of elements */

	for (i=pamh->env->requested-1; i-- > 0; ) {
	    D(("dropping #%3d>%s<", i, pamh->env->list[i]));
	    _pam_overwrite(pamh->env->list[i]);          /* clean */
	    _pam_drop(pamh->env->list[i]);              /* forget */
	}
	pamh->env->requested = 0;
	pamh->env->entries = 0;
	_pam_drop(pamh->env->list);                     /* forget */
	_pam_drop(pamh->env);                           /* forget */
    } else {
	D(("no environment present in pamh?"));
    }
}

/*
 * Return the item number of the given variable = first 'length' chars
 * of 'name_value'. Since this is a static function, it is safe to
 * assume its supplied arguments are well defined.
 */

static int _pam_search_env(const struct pam_environ *env
			   , const char *name_value, int length)
{
    int i;

    for (i=env->requested-1; i-- > 0; ) {
	if (strncmp(name_value,env->list[i],length) == 0
	    && env->list[i][length] == '=') {

	    return i;                                   /* Got it! */

	}
    }

    return -1;                                          /* no luck */
}

/*
 * externally visible functions
 */

/*
 *  pam_putenv(): Add/replace/delete a PAM-environment variable.
 *
 *  Add/replace:
 *      name_value = "NAME=VALUE" or "NAME=" (for empty value="\0")
 *
 *  delete:
 *      name_value = "NAME"
 */

int pam_putenv(pam_handle_t *pamh, const char *name_value)
{
    int l2eq, item, retval;

    D(("called."));
    IF_NO_PAMH("pam_putenv", pamh, PAM_ABORT);

    if (name_value == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_putenv: no variable indicated");
	return PAM_PERM_DENIED;
    }

    /*
     * establish if we are setting or deleting; scan for '='
     */

    for (l2eq=0; name_value[l2eq] && name_value[l2eq] != '='; ++l2eq);
    if (l2eq <= 0) {
	pam_system_log(pamh, NULL, LOG_ERR, "pam_putenv: bad variable");
	return PAM_BAD_ITEM;
    }

    /*
     *  Look first for environment.
     */

    if (pamh->env == NULL || pamh->env->list == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR, "pam_putenv: no env%s found"
		       , pamh->env == NULL ? "":"-list");
	return PAM_ABORT;
    }

    /* find the item to replace */

    item = _pam_search_env(pamh->env, name_value, l2eq);

    if (name_value[l2eq]) {                     /* (re)setting */

	if (item == -1) {                      /* new variable */
	    D(("adding item: %s", name_value));
	    /* enough space? */
	    if (pamh->env->entries <= pamh->env->requested) {
		register int i;
		register char **tmp;

		/* get some new space */
		tmp = calloc( pamh->env->entries + PAM_ENV_CHUNK
				     , sizeof(char *) );
		if (tmp == NULL) {
		    /* nothing has changed - old env intact */
		    pam_system_log(pamh, NULL, LOG_CRIT,
				   "pam_putenv: cannot grow environment");
		    return PAM_BUF_ERR;
		}

		/* copy old env-item pointers/forget old */
		for (i=0; i<pamh->env->requested; ++i) {
		    tmp[i] = pamh->env->list[i];
		    pamh->env->list[i] = NULL;
		}

		/* drop old list and replace with new */
		_pam_drop(pamh->env->list);
		pamh->env->list = tmp;
		pamh->env->entries += PAM_ENV_CHUNK;

		D(("resized env list"));
		_pam_dump_env(pamh);              /* only when debugging */
	    }

	    item = pamh->env->requested-1;        /* old last item (NULL) */

	    /* add a new NULL entry at end; increase counter */
	    pamh->env->list[pamh->env->requested++] = NULL;
	    
	} else {                                /* replace old */
	    D(("replacing item: %s\n          with: %s"
	       , pamh->env->list[item], name_value));
	    _pam_overwrite(pamh->env->list[item]);
	    _pam_drop(pamh->env->list[item]);
	}

	/*
	 * now we have a place to put the new env-item, insert at 'item'
	 */

	pamh->env->list[item] = _pam_strdup(name_value);
	if (pamh->env->list[item] != NULL) {
	    _pam_dump_env(pamh);                   /* only when debugging */
	    return PAM_SUCCESS;
	}

	/* something went wrong; we should delete the item - fall through */

	retval = PAM_BUF_ERR;                        /* an error occurred */
    } else {
	retval = PAM_SUCCESS;                      /* we requested delete */
    }

    /* getting to here implies we are deleting an item */

    if (item < 0) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_putenv: delete non-existent entry; %s",
		       name_value);
	return PAM_BAD_ITEM;
    }

    /*
     * remove item: purge memory; reset counter; resize [; display-env]
     */

    D(("deleting: env#%3d:[%s]", item, pamh->env->list[item]));
    _pam_overwrite(pamh->env->list[item]);
    _pam_drop(pamh->env->list[item]);
    --(pamh->env->requested);
    D(("mmove: item[%d]+%d -> item[%d]"
       , item+1, ( pamh->env->requested - item ), item));
    (void) memmove(&pamh->env->list[item], &pamh->env->list[item+1]
		   , ( pamh->env->requested - item )*sizeof(char *) );

    _pam_dump_env(pamh);                   /* only when debugging */

    /*
     * deleted.
     */

    return retval;
}

/*
 *  Return the value of the requested environment variable
 */

const char *pam_getenv(pam_handle_t *pamh, const char *name)
{
    int item;

    D(("called."));
    IF_NO_PAMH("pam_getenv", pamh, NULL);

    if (name == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_getenv: no variable indicated");
	return NULL;
    }

    if (pamh->env == NULL || pamh->env->list == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR, "pam_getenv: no env%s found",
		       pamh->env == NULL ? "":"-list" );
	return NULL;
    }

    /* find the requested item */

    item = _pam_search_env(pamh->env, name, strlen(name));
    if (item != -1) {

	D(("env-item: %s, found!", name));
	return (pamh->env->list[item] + 1 + strlen(name));

    } else {

	D(("env-item: %s, not found", name));
	return NULL;

    }
}

static char **_copy_env(pam_handle_t *pamh)
{
    char **dump;
    int i = pamh->env->requested;          /* reckon size of environment */
    char *const *env = pamh->env->list;

    D(("now get some memory for dump"));

    /* allocate some memory for this (plus the null tail-pointer) */
    dump = (char **) calloc(i, sizeof(char *));
    D(("dump = %p", dump));
    if (dump == NULL) {
	return NULL;
    }

    /* now run through entries and copy the variables over */
    dump[--i] = NULL;
    while (i-- > 0) {
	D(("env[%d]=`%s'", i,env[i]));
	dump[i] = _pam_strdup(env[i]);
	D(("->dump[%d]=`%s'", i,dump[i]));
	if (dump[i] == NULL) {
	    /* out of memory */

	    while (dump[++i]) {
		_pam_overwrite(dump[i]);
		_pam_drop(dump[i]);
	    }
	    return NULL;
	}
    }

    env = NULL;                             /* forget now */

    /* return transcribed environment */
    return dump;
}

char **pam_getenvlist(pam_handle_t *pamh)
{
    int i;

    D(("called."));
    IF_NO_PAMH("pam_getenvlist", pamh, NULL);

    if (pamh->env == NULL || pamh->env->list == NULL) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_getenvlist: no env%s found",
		       pamh->env == NULL ? "":"-list" );
	return NULL;
    }

    /* some quick checks */

    if (pamh->env->requested > pamh->env->entries) {
	pam_system_log(pamh, NULL, LOG_ERR,
		       "pam_getenvlist: environment corruption");
	_pam_dump_env(pamh);                 /* only active when debugging */
	return NULL;
    }

    for (i=pamh->env->requested-1; i-- > 0; ) {
	if (pamh->env->list[i] == NULL) {
	    pam_system_log(pamh, NULL, LOG_ERR,
			   "pam_getenvlist: environment broken");
	    _pam_dump_env(pamh);              /* only active when debugging */
	    return NULL;          /* somehow we've broken the environment!? */
	}
    }

    /* Seems fine; copy environment */

    _pam_dump_env(pamh);                    /* only active when debugging */

    return _copy_env(pamh);
}
