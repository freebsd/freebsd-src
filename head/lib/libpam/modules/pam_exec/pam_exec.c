/*-
 * Copyright (c) 2001,2003 Networks Associates Technology, Inc.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/openpam.h>

#define ENV_ITEM(n) { (n), #n }
static struct {
	int item;
	const char *name;
} env_items[] = {
	ENV_ITEM(PAM_SERVICE),
	ENV_ITEM(PAM_USER),
	ENV_ITEM(PAM_TTY),
	ENV_ITEM(PAM_RHOST),
	ENV_ITEM(PAM_RUSER),
};

static int
_pam_exec(pam_handle_t *pamh __unused, int flags __unused,
    int argc, const char *argv[])
{
	int envlen, i, nitems, pam_err, status;
	char *env, **envlist, **tmp;
	volatile int childerr;
	pid_t pid;

	if (argc < 1)
		return (PAM_SERVICE_ERR);

	/*
	 * XXX For additional credit, divert child's stdin/stdout/stderr
	 * to the conversation function.
	 */

	/*
	 * Set up the child's environment list.  It consists of the PAM
	 * environment, plus a few hand-picked PAM items.
	 */
	envlist = pam_getenvlist(pamh);
	for (envlen = 0; envlist[envlen] != NULL; ++envlen)
		/* nothing */ ;
	nitems = sizeof(env_items) / sizeof(*env_items);
	tmp = realloc(envlist, (envlen + nitems + 1) * sizeof(*envlist));
	if (tmp == NULL) {
		openpam_free_envlist(envlist);
		return (PAM_BUF_ERR);
	}
	envlist = tmp;
	for (i = 0; i < nitems; ++i) {
		const void *item;
		char *envstr;

		pam_err = pam_get_item(pamh, env_items[i].item, &item);
		if (pam_err != PAM_SUCCESS || item == NULL)
			continue;
		asprintf(&envstr, "%s=%s", env_items[i].name, item);
		if (envstr == NULL) {
			openpam_free_envlist(envlist);
			return (PAM_BUF_ERR);
		}
		envlist[envlen++] = envstr;
		envlist[envlen] = NULL;
	}

	/*
	 * Fork and run the command.  By using vfork() instead of fork(),
	 * we can distinguish between an execve() failure and a non-zero
	 * exit code from the command.
	 */
	childerr = 0;
	if ((pid = vfork()) == 0) {
		execve(argv[0], (char * const *)argv, (char * const *)envlist);
		childerr = errno;
		_exit(1);
	}
	openpam_free_envlist(envlist);
	if (pid == -1) {
		openpam_log(PAM_LOG_ERROR, "vfork(): %m");
		return (PAM_SYSTEM_ERR);
	}
	if (waitpid(pid, &status, 0) == -1) {
		openpam_log(PAM_LOG_ERROR, "waitpid(): %m");
		return (PAM_SYSTEM_ERR);
	}
	if (childerr != 0) {
		openpam_log(PAM_LOG_ERROR, "execve(): %m");
		return (PAM_SYSTEM_ERR);
	}
	if (WIFSIGNALED(status)) {
		openpam_log(PAM_LOG_ERROR, "%s caught signal %d%s",
		    argv[0], WTERMSIG(status),
		    WCOREDUMP(status) ? " (core dumped)" : "");
		return (PAM_SYSTEM_ERR);
	}
	if (!WIFEXITED(status)) {
		openpam_log(PAM_LOG_ERROR, "unknown status 0x%x", status);
		return (PAM_SYSTEM_ERR);
	}
	if (WEXITSTATUS(status) != 0) {
		openpam_log(PAM_LOG_ERROR, "%s returned code %d",
		    argv[0], WEXITSTATUS(status));
		return (PAM_SYSTEM_ERR);
	}
	return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_exec(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_exec(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_exec(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_exec(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_exec(pamh, flags, argc, argv));
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{

	return (_pam_exec(pamh, flags, argc, argv));
}

PAM_MODULE_ENTRY("pam_exec");
