/*-
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
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
 * $P4: //depot/projects/openpam/lib/openpam_dynamic.c#1 $
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

/*
 * OpenPAM internal
 *
 * Locate a dynamically linked module
 */

pam_module_t *
openpam_dynamic(const char *path)
{
	pam_module_t *module;
	char *vpath;
	void *dlh;
	int i;

	if ((module = calloc(1, sizeof *module)) == NULL)
		goto buf_err;

	/* try versioned module first, then unversioned module */
	if (asprintf(&vpath, "%s.%d", path, LIB_MAJ) == -1)
		goto buf_err;
	if ((dlh = dlopen(vpath, RTLD_NOW)) == NULL) {
		openpam_log(PAM_LOG_ERROR, "dlopen(): %s", dlerror());
		*strrchr(vpath, '.') = '\0';
		if ((dlh = dlopen(vpath, RTLD_NOW)) == NULL) {
			openpam_log(PAM_LOG_ERROR, "dlopen(): %s", dlerror());
			free(module);
			return (NULL);
		}
	}
	module->path = vpath;
	module->dlh = dlh;
	for (i = 0; i < PAM_NUM_PRIMITIVES; ++i)
		module->func[i] = dlsym(dlh, _pam_sm_func_name[i]);
	return (module);
 buf_err:
	openpam_log(PAM_LOG_ERROR, "%m");
	dlclose(dlh);
	free(module);
	return (NULL);
}

/*
 * NOPARSE
 */
