/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $P4: //depot/projects/openpam/lib/openpam_load.c#12 $
 */

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

const char *_pam_sm_func_name[PAM_NUM_PRIMITIVES] = {
	"pam_sm_authenticate",
	"pam_sm_setcred",
	"pam_sm_acct_mgmt",
	"pam_sm_open_session",
	"pam_sm_close_session",
	"pam_sm_chauthtok"
};

static pam_module_t *modules;

/*
 * Locate a matching dynamic or static module.  Keep a list of previously
 * found modules to speed up the process.
 */

static pam_module_t *
openpam_load_module(const char *path)
{
	pam_module_t *module;

	/* check cache first */
	for (module = modules; module != NULL; module = module->next)
		if (strcmp(module->path, path) == 0)
			goto found;

	/* nope; try to load */
	module = openpam_dynamic(path);
	openpam_log(PAM_LOG_DEBUG, "%s dynamic %s",
	    (module == NULL) ? "no" : "using", path);

#ifdef OPENPAM_STATIC_MODULES
	/* look for a static module */
	if (module == NULL && strchr(path, '/') == NULL) {
		module = openpam_static(path);
		openpam_log(PAM_LOG_DEBUG, "%s static %s",
		    (module == NULL) ? "no" : "using", path);
	}
#endif
	if (module == NULL) {
		openpam_log(PAM_LOG_ERROR, "no %s found", path);
		return (NULL);
	}
	openpam_log(PAM_LOG_DEBUG, "adding %s to cache", module->path);
	module->next = modules;
	if (module->next != NULL)
		module->next->prev = module;
	module->prev = NULL;
	modules = module;
 found:
	++module->refcount;
	return (module);
}


/*
 * Release a module.
 * XXX highly thread-unsafe
 */

static void
openpam_release_module(pam_module_t *module)
{
	if (module == NULL)
		return;
	--module->refcount;
	if (module->refcount > 0)
		/* still in use */
		return;
	if (module->refcount < 0) {
		openpam_log(PAM_LOG_ERROR, "module %s has negative refcount",
		    module->path);
		module->refcount = 0;
	}
	if (module->dlh == NULL)
		/* static module */
		return;
	dlclose(module->dlh);
	if (module->prev != NULL)
		module->prev->next = module->next;
	if (module->next != NULL)
		module->next->prev = module->prev;
	if (module == modules)
		modules = module->next;
	openpam_log(PAM_LOG_DEBUG, "releasing %s", module->path);
	free(module->path);
	free(module);
}


/*
 * Destroy a chain, freeing all its links and releasing the modules
 * they point to.
 */

static void
openpam_destroy_chain(pam_chain_t *chain)
{
	if (chain == NULL)
		return;
	openpam_destroy_chain(chain->next);
	chain->next = NULL;
	while (chain->optc--)
		free(chain->optv[chain->optc]);
	free(chain->optv);
	openpam_release_module(chain->module);
	free(chain);
}

/*
 * Add a module to a chain.
 */

int
openpam_add_module(pam_handle_t *pamh,
	int chain,
	int flag,
	const char *modpath,
	int optc,
	const char *optv[])
{
	pam_chain_t *new, *iterator;

	if ((new = calloc(1, sizeof *new)) == NULL)
		goto buf_err;
	if ((new->optv = malloc(sizeof(char *) * (optc + 1))) == NULL)
		goto buf_err;
	while (optc--)
		if ((new->optv[new->optc++] = strdup(*optv++)) == NULL)
			goto buf_err;
	new->optv[new->optc] = NULL;
	new->flag = flag;
	if ((new->module = openpam_load_module(modpath)) == NULL) {
		openpam_destroy_chain(new);
		return (PAM_OPEN_ERR);
	}
	if ((iterator = pamh->chains[chain]) != NULL) {
		while (iterator->next != NULL)
			iterator = iterator->next;
		iterator->next = new;
	} else {
		pamh->chains[chain] = new;
	}
	return (PAM_SUCCESS);

 buf_err:
	openpam_log(PAM_LOG_ERROR, "%m");
	openpam_destroy_chain(new);
	return (PAM_BUF_ERR);
}


/*
 * Clear the chains and release the modules
 */

void
openpam_clear_chains(pam_handle_t *pamh)
{
	int i;

	for (i = 0; i < PAM_NUM_CHAINS; ++i)
		openpam_destroy_chain(pamh->chains[i]);
}

/*
 * NOPARSE
 */
