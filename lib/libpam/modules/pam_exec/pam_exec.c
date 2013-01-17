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

struct pe_opts {
	int	return_prog_exit_status;
};

#define	PAM_RV_COUNT 24

static int
parse_options(const char *func, int *argc, const char **argv[],
    struct pe_opts *options)
{
	int i;

	/*
	 * Parse options:
	 *   return_prog_exit_status:
	 *     use the program exit status as the return code of pam_exec
	 *   --:
	 *     stop options parsing; what follows is the command to execute
	 */
	options->return_prog_exit_status = 0;

	for (i = 0; i < *argc; ++i) {
		if (strcmp((*argv)[i], "return_prog_exit_status") == 0) {
			openpam_log(PAM_LOG_DEBUG,
			    "%s: Option \"return_prog_exit_status\" enabled",
			    func);
			options->return_prog_exit_status = 1;
		} else {
			if (strcmp((*argv)[i], "--") == 0) {
				(*argc)--;
				(*argv)++;
			}

			break;
		}
	}

	(*argc) -= i;
	(*argv) += i;

	return (0);
}

static int
_pam_exec(pam_handle_t *pamh __unused,
    const char *func, int flags __unused, int argc, const char *argv[],
    struct pe_opts *options)
{
	int envlen, i, nitems, pam_err, status;
	int nitems_rv;
	char **envlist, **tmp, *envstr;
	volatile int childerr;
	pid_t pid;

	/*
	 * XXX For additional credit, divert child's stdin/stdout/stderr
	 * to the conversation function.
	 */

	/* Check there's a program name left after parsing options. */
	if (argc < 1) {
		openpam_log(PAM_LOG_ERROR, "%s: No program specified: aborting",
		    func);
		return (PAM_SERVICE_ERR);
	}

	/*
	 * Set up the child's environment list. It consists of the PAM
	 * environment, plus a few hand-picked PAM items, the pam_sm_*
	 * function name calling it and, if return_prog_exit_status is
	 * set, the valid return codes numerical values.
	 */
	envlist = pam_getenvlist(pamh);
	for (envlen = 0; envlist[envlen] != NULL; ++envlen)
		/* nothing */ ;
	nitems = sizeof(env_items) / sizeof(*env_items);
	/* Count PAM return values put in the environment. */
	nitems_rv = options->return_prog_exit_status ? PAM_RV_COUNT : 0;
	tmp = realloc(envlist, (envlen + nitems + 1 + nitems_rv + 1) *
	    sizeof(*envlist));
	if (tmp == NULL) {
		openpam_free_envlist(envlist);
		return (PAM_BUF_ERR);
	}
	envlist = tmp;
	for (i = 0; i < nitems; ++i) {
		const void *item;

		pam_err = pam_get_item(pamh, env_items[i].item, &item);
		if (pam_err != PAM_SUCCESS || item == NULL)
			continue;
		asprintf(&envstr, "%s=%s", env_items[i].name,
		    (const char *)item);
		if (envstr == NULL) {
			openpam_free_envlist(envlist);
			return (PAM_BUF_ERR);
		}
		envlist[envlen++] = envstr;
		envlist[envlen] = NULL;
	}

	/* Add the pam_sm_* function name to the environment. */
	asprintf(&envstr, "PAM_SM_FUNC=%s", func);
	if (envstr == NULL) {
		openpam_free_envlist(envlist);
		return (PAM_BUF_ERR);
	}
	envlist[envlen++] = envstr;

	/* Add the PAM return values to the environment. */
	if (options->return_prog_exit_status) {
#define	ADD_PAM_RV_TO_ENV(name)						\
		asprintf(&envstr, #name "=%d", name);			\
		if (envstr == NULL) {					\
			openpam_free_envlist(envlist);			\
			return (PAM_BUF_ERR);				\
		}							\
		envlist[envlen++] = envstr
		/*
		 * CAUTION: When adding/removing an item in the list
		 * below, be sure to update the value of PAM_RV_COUNT.
		 */
		ADD_PAM_RV_TO_ENV(PAM_ABORT);
		ADD_PAM_RV_TO_ENV(PAM_ACCT_EXPIRED);
		ADD_PAM_RV_TO_ENV(PAM_AUTHINFO_UNAVAIL);
		ADD_PAM_RV_TO_ENV(PAM_AUTHTOK_DISABLE_AGING);
		ADD_PAM_RV_TO_ENV(PAM_AUTHTOK_ERR);
		ADD_PAM_RV_TO_ENV(PAM_AUTHTOK_LOCK_BUSY);
		ADD_PAM_RV_TO_ENV(PAM_AUTHTOK_RECOVERY_ERR);
		ADD_PAM_RV_TO_ENV(PAM_AUTH_ERR);
		ADD_PAM_RV_TO_ENV(PAM_BUF_ERR);
		ADD_PAM_RV_TO_ENV(PAM_CONV_ERR);
		ADD_PAM_RV_TO_ENV(PAM_CRED_ERR);
		ADD_PAM_RV_TO_ENV(PAM_CRED_EXPIRED);
		ADD_PAM_RV_TO_ENV(PAM_CRED_INSUFFICIENT);
		ADD_PAM_RV_TO_ENV(PAM_CRED_UNAVAIL);
		ADD_PAM_RV_TO_ENV(PAM_IGNORE);
		ADD_PAM_RV_TO_ENV(PAM_MAXTRIES);
		ADD_PAM_RV_TO_ENV(PAM_NEW_AUTHTOK_REQD);
		ADD_PAM_RV_TO_ENV(PAM_PERM_DENIED);
		ADD_PAM_RV_TO_ENV(PAM_SERVICE_ERR);
		ADD_PAM_RV_TO_ENV(PAM_SESSION_ERR);
		ADD_PAM_RV_TO_ENV(PAM_SUCCESS);
		ADD_PAM_RV_TO_ENV(PAM_SYSTEM_ERR);
		ADD_PAM_RV_TO_ENV(PAM_TRY_AGAIN);
		ADD_PAM_RV_TO_ENV(PAM_USER_UNKNOWN);
	}

	envlist[envlen] = NULL;

	/*
	 * Fork and run the command.  By using vfork() instead of fork(),
	 * we can distinguish between an execve() failure and a non-zero
	 * exit status from the command.
	 */
	childerr = 0;
	if ((pid = vfork()) == 0) {
		execve(argv[0], (char * const *)argv, (char * const *)envlist);
		childerr = errno;
		_exit(1);
	}
	openpam_free_envlist(envlist);
	if (pid == -1) {
		openpam_log(PAM_LOG_ERROR, "%s: vfork(): %m", func);
		return (PAM_SYSTEM_ERR);
	}
	while (waitpid(pid, &status, 0) == -1) {
		if (errno == EINTR)
			continue;
		openpam_log(PAM_LOG_ERROR, "%s: waitpid(): %m", func);
		return (PAM_SYSTEM_ERR);
	}
	if (childerr != 0) {
		openpam_log(PAM_LOG_ERROR, "%s: execve(): %m", func);
		return (PAM_SYSTEM_ERR);
	}
	if (WIFSIGNALED(status)) {
		openpam_log(PAM_LOG_ERROR, "%s: %s caught signal %d%s",
		    func, argv[0], WTERMSIG(status),
		    WCOREDUMP(status) ? " (core dumped)" : "");
		return (PAM_SERVICE_ERR);
	}
	if (!WIFEXITED(status)) {
		openpam_log(PAM_LOG_ERROR, "%s: unknown status 0x%x",
		    func, status);
		return (PAM_SERVICE_ERR);
	}

	if (options->return_prog_exit_status) {
		openpam_log(PAM_LOG_DEBUG,
		    "%s: Use program exit status as return value: %d",
		    func, WEXITSTATUS(status));
		return (WEXITSTATUS(status));
	} else {
		return (WEXITSTATUS(status) == 0 ?
		    PAM_SUCCESS : PAM_PERM_DENIED);
	}
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	int ret;
	struct pe_opts options;

	ret = parse_options(__func__, &argc, &argv, &options);
	if (ret != 0)
		return (PAM_SERVICE_ERR);

	ret = _pam_exec(pamh, __func__, flags, argc, argv, &options);

	/*
	 * We must check that the program returned a valid code for this
	 * function.
	 */
	switch (ret) {
	case PAM_SUCCESS:
	case PAM_ABORT:
	case PAM_AUTHINFO_UNAVAIL:
	case PAM_AUTH_ERR:
	case PAM_BUF_ERR:
	case PAM_CONV_ERR:
	case PAM_CRED_INSUFFICIENT:
	case PAM_IGNORE:
	case PAM_MAXTRIES:
	case PAM_PERM_DENIED:
	case PAM_SERVICE_ERR:
	case PAM_SYSTEM_ERR:
	case PAM_USER_UNKNOWN:
		break;
	default:
		openpam_log(PAM_LOG_ERROR, "%s returned invalid code %d",
		    argv[0], ret);
		ret = PAM_SERVICE_ERR;
	}

	return (ret);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	int ret;
	struct pe_opts options;

	ret = parse_options(__func__, &argc, &argv, &options);
	if (ret != 0)
		return (PAM_SERVICE_ERR);

	ret = _pam_exec(pamh, __func__, flags, argc, argv, &options);

	/*
	 * We must check that the program returned a valid code for this
	 * function.
	 */
	switch (ret) {
	case PAM_SUCCESS:
	case PAM_ABORT:
	case PAM_BUF_ERR:
	case PAM_CONV_ERR:
	case PAM_CRED_ERR:
	case PAM_CRED_EXPIRED:
	case PAM_CRED_UNAVAIL:
	case PAM_IGNORE:
	case PAM_PERM_DENIED:
	case PAM_SERVICE_ERR:
	case PAM_SYSTEM_ERR:
	case PAM_USER_UNKNOWN:
		break;
	default:
		openpam_log(PAM_LOG_ERROR, "%s returned invalid code %d",
		    argv[0], ret);
		ret = PAM_SERVICE_ERR;
	}

	return (ret);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	int ret;
	struct pe_opts options;

	ret = parse_options(__func__, &argc, &argv, &options);
	if (ret != 0)
		return (PAM_SERVICE_ERR);

	ret = _pam_exec(pamh, __func__, flags, argc, argv, &options);

	/*
	 * We must check that the program returned a valid code for this
	 * function.
	 */
	switch (ret) {
	case PAM_SUCCESS:
	case PAM_ABORT:
	case PAM_ACCT_EXPIRED:
	case PAM_AUTH_ERR:
	case PAM_BUF_ERR:
	case PAM_CONV_ERR:
	case PAM_IGNORE:
	case PAM_NEW_AUTHTOK_REQD:
	case PAM_PERM_DENIED:
	case PAM_SERVICE_ERR:
	case PAM_SYSTEM_ERR:
	case PAM_USER_UNKNOWN:
		break;
	default:
		openpam_log(PAM_LOG_ERROR, "%s returned invalid code %d",
		    argv[0], ret);
		ret = PAM_SERVICE_ERR;
	}

	return (ret);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	int ret;
	struct pe_opts options;

	ret = parse_options(__func__, &argc, &argv, &options);
	if (ret != 0)
		return (PAM_SERVICE_ERR);

	ret = _pam_exec(pamh, __func__, flags, argc, argv, &options);

	/*
	 * We must check that the program returned a valid code for this
	 * function.
	 */
	switch (ret) {
	case PAM_SUCCESS:
	case PAM_ABORT:
	case PAM_BUF_ERR:
	case PAM_CONV_ERR:
	case PAM_IGNORE:
	case PAM_PERM_DENIED:
	case PAM_SERVICE_ERR:
	case PAM_SESSION_ERR:
	case PAM_SYSTEM_ERR:
		break;
	default:
		openpam_log(PAM_LOG_ERROR, "%s returned invalid code %d",
		    argv[0], ret);
		ret = PAM_SERVICE_ERR;
	}

	return (ret);
}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	int ret;
	struct pe_opts options;

	ret = parse_options(__func__, &argc, &argv, &options);
	if (ret != 0)
		return (PAM_SERVICE_ERR);

	ret = _pam_exec(pamh, __func__, flags, argc, argv, &options);

	/*
	 * We must check that the program returned a valid code for this
	 * function.
	 */
	switch (ret) {
	case PAM_SUCCESS:
	case PAM_ABORT:
	case PAM_BUF_ERR:
	case PAM_CONV_ERR:
	case PAM_IGNORE:
	case PAM_PERM_DENIED:
	case PAM_SERVICE_ERR:
	case PAM_SESSION_ERR:
	case PAM_SYSTEM_ERR:
		break;
	default:
		openpam_log(PAM_LOG_ERROR, "%s returned invalid code %d",
		    argv[0], ret);
		ret = PAM_SERVICE_ERR;
	}

	return (ret);
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
	int ret;
	struct pe_opts options;

	ret = parse_options(__func__, &argc, &argv, &options);
	if (ret != 0)
		return (PAM_SERVICE_ERR);

	ret = _pam_exec(pamh, __func__, flags, argc, argv, &options);

	/*
	 * We must check that the program returned a valid code for this
	 * function.
	 */
	switch (ret) {
	case PAM_SUCCESS:
	case PAM_ABORT:
	case PAM_AUTHTOK_DISABLE_AGING:
	case PAM_AUTHTOK_ERR:
	case PAM_AUTHTOK_LOCK_BUSY:
	case PAM_AUTHTOK_RECOVERY_ERR:
	case PAM_BUF_ERR:
	case PAM_CONV_ERR:
	case PAM_IGNORE:
	case PAM_PERM_DENIED:
	case PAM_SERVICE_ERR:
	case PAM_SYSTEM_ERR:
	case PAM_TRY_AGAIN:
		break;
	default:
		openpam_log(PAM_LOG_ERROR, "%s returned invalid code %d",
		    argv[0], ret);
		ret = PAM_SERVICE_ERR;
	}

	return (ret);
}

PAM_MODULE_ENTRY("pam_exec");
