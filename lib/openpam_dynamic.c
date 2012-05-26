/*-
 * Copyright (c) 2002-2003 Networks Associates Technology, Inc.
 * Copyright (c) 2004-2011 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * Network Associates Laboratories, the Security Research Division of
 * Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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
 * $Id: openpam_dynamic.c 607 2012-04-20 11:09:37Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

#ifndef RTLD_NOW
#define RTLD_NOW RTLD_LAZY
#endif

/*
 * OpenPAM internal
 *
 * Perform sanity checks and attempt to load a module
 */

#ifdef HAVE_FDLOPEN
static void *
try_dlopen(const char *modfn)
{
	void *dlh;
	int fd;

	if ((fd = open(modfn, O_RDONLY)) < 0)
		return (NULL);
	if (OPENPAM_FEATURE(VERIFY_MODULE_FILE) &&
	    openpam_check_desc_owner_perms(modfn, fd) != 0) {
		close(fd);
		return (NULL);
	}
	if ((dlh = fdlopen(fd, RTLD_NOW)) == NULL) {
		openpam_log(PAM_LOG_ERROR, "%s: %s", modfn, dlerror());
		close(fd);
		errno = 0;
		return (NULL);
	}
	close(fd);
	return (dlh);
}
#else
static void *
try_dlopen(const char *modfn)
{
	int check_module_file;
	void *dlh;

	openpam_get_feature(OPENPAM_VERIFY_MODULE_FILE,
	    &check_module_file);
	if (check_module_file &&
	    openpam_check_path_owner_perms(modfn) != 0)
		return (NULL);
	if ((dlh = dlopen(modfn, RTLD_NOW)) == NULL) {
		openpam_log(PAM_LOG_ERROR, "%s: %s", modfn, dlerror());
		errno = 0;
		return (NULL);
	}
	return (dlh);
}
#endif

/*
 * OpenPAM internal
 *
 * Locate a dynamically linked module
 */

pam_module_t *
openpam_dynamic(const char *path)
{
	const pam_module_t *dlmodule;
	pam_module_t *module;
	const char *prefix;
	char *vpath;
	void *dlh;
	int i, serrno;

	dlh = NULL;

	/* Prepend the standard prefix if not an absolute pathname. */
	if (path[0] != '/')
		prefix = OPENPAM_MODULES_DIR;
	else
		prefix = "";

	/* try versioned module first, then unversioned module */
	if (asprintf(&vpath, "%s%s.%d", prefix, path, LIB_MAJ) < 0)
		goto err;
	if ((dlh = try_dlopen(vpath)) == NULL && errno == ENOENT) {
		*strrchr(vpath, '.') = '\0';
		dlh = try_dlopen(vpath);
	}
	if (dlh == NULL)
		goto err;
	if ((module = calloc(1, sizeof *module)) == NULL)
		goto buf_err;
	if ((module->path = strdup(path)) == NULL)
		goto buf_err;
	module->dlh = dlh;
	dlmodule = dlsym(dlh, "_pam_module");
	for (i = 0; i < PAM_NUM_PRIMITIVES; ++i) {
		if (dlmodule) {
			module->func[i] = dlmodule->func[i];
		} else {
			module->func[i] =
			    (pam_func_t)dlsym(dlh, pam_sm_func_name[i]);
			/*
			 * This openpam_log() call is a major source of
			 * log spam, and the cases that matter are caught
			 * and logged in openpam_dispatch().  This would
			 * be less problematic if dlerror() returned an
			 * error code so we could log an error only when
			 * dlsym() failed for a reason other than "no such
			 * symbol".
			 */
#if 0
			if (module->func[i] == NULL)
				openpam_log(PAM_LOG_DEBUG, "%s: %s(): %s",
				    path, pam_sm_func_name[i], dlerror());
#endif
		}
	}
	FREE(vpath);
	return (module);
buf_err:
	serrno = errno;
	if (dlh != NULL)
		dlclose(dlh);
	FREE(module);
	errno = serrno;
err:
	serrno = errno;
	if (errno != 0)
		openpam_log(PAM_LOG_ERROR, "%s: %m", vpath);
	FREE(vpath);
	errno = serrno;
	return (NULL);
}

/*
 * NOPARSE
 */
