/* pam_static.c -- static module loading helper functions */

/* created by Michael K. Johnson, johnsonm@redhat.com
 *
 * $Id: pam_static.c,v 1.1.1.1 2000/06/20 22:11:21 agmorgan Exp $
 * $FreeBSD$
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pam_private.h"

/* This whole file is only used for PAM_STATIC */

#ifdef PAM_STATIC

extern struct linker_set _pam_static_modules;

/* Return pointer to data structure used to define a static module */
struct pam_module * _pam_open_static_handler(const char *path)
{
    int i;
    const char *clpath = path;
    char *lpath, *end;
    struct pam_module **static_modules =
	(struct pam_module **)_pam_static_modules.ls_items;

    if (strchr(clpath, '/')) {
        /* ignore path and leading "/" */
	clpath = strrchr(lpath, '/') + 1;
    }
    /* create copy to muck with (must free before return) */
    lpath = _pam_strdup(clpath);
    /* chop .so off copy if it exists (or other extension on other
       platform...) */
    end = strstr(lpath, ".so");
    if (end) {
        *end = '\0';
    }

    /* now go find the module */
    for (i = 0; static_modules[i] != NULL; i++) {
	D(("%s=?%s\n", lpath, static_modules[i]->name));
        if (static_modules[i]->name &&
	    ! strcmp(static_modules[i]->name, lpath)) {
	    break;
	}
    }

    free(lpath);
    return (static_modules[i]);
}

/* Return pointer to function requested from static module
 * Can't just return void *, because ANSI C disallows casting a
 * pointer to a function to a void *...
 * This definition means:
 * _pam_get_static_sym is a function taking two arguments and
 * returning a pointer to a function which takes no arguments
 * and returns void... */
voidfunc *_pam_get_static_sym(struct pam_module *mod, const char *symname) {

    if (! strcmp(symname, "pam_sm_authenticate")) {
        return ((voidfunc *)mod->pam_sm_authenticate);
    } else if (! strcmp(symname, "pam_sm_setcred")) {
        return ((voidfunc *)mod->pam_sm_setcred);
    } else if (! strcmp(symname, "pam_sm_acct_mgmt")) {
        return ((voidfunc *)mod->pam_sm_acct_mgmt);
    } else if (! strcmp(symname, "pam_sm_open_session")) {
        return ((voidfunc *)mod->pam_sm_open_session);
    } else if (! strcmp(symname, "pam_sm_close_session")) {
        return ((voidfunc *)mod->pam_sm_close_session);
    } else if (! strcmp(symname, "pam_sm_chauthtok")) {
        return ((voidfunc *)mod->pam_sm_chauthtok);
    }
    /* getting to this point is an error */
    return ((voidfunc *)NULL);
}

#endif /* PAM_STATIC */

/*
 * Copyright (C) 1995 by Red Hat Software, Michael K. Johnson
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
