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
 * $Id$
 */

#ifndef _SECURITY_OPENPAM_H_INCLUDED
#define _SECURITY_OPENPAM_H_INCLUDED

/*
 * Annoying but necessary header pollution
 */
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * API extensions
 */
const char *
openpam_get_option(pam_handle_t *_pamh,
	const char *_option);

int
openpam_set_option(pam_handle_t *_pamh,
	const char *_option,
	const char *_value);

int
pam_error(pam_handle_t *_pamh,
	const char *_fmt,
	...);

int
pam_get_authtok(pam_handle_t *_pamh,
	const char **_authtok,
	const char *_prompt);

int
pam_info(pam_handle_t *_pamh,
	const char *_fmt,
	...);

int
pam_prompt(pam_handle_t *_pamh,
	int _style,
	char **_resp,
	const char *_fmt,
	...);

int
pam_setenv(pam_handle_t *_pamh,
	const char *_name,
	const char *_value,
	int _overwrite);

int
pam_vinfo(pam_handle_t *_pamh,
	const char *_fmt,
	va_list _ap);

int
pam_verror(pam_handle_t *_pamh,
	const char *_fmt,
	va_list _ap);

int
pam_vprompt(pam_handle_t *_pamh,
	int _style,
	char **_resp,
	const char *_fmt,
	va_list _ap);

/*
 * Log levels
 */
enum {
	PAM_LOG_DEBUG,
	PAM_LOG_VERBOSE,
	PAM_LOG_NOTICE,
	PAM_LOG_ERROR
};

/*
 * Log to syslog
 */
void _openpam_log(int _level,
	const char *_func,
	const char *_fmt,
	...);

#if defined(__STDC__) && (__STDC_VERSION__ > 199901L)
#define openpam_log(lvl, fmt, ...) \
	_openpam_log((lvl), __func__, fmt, __VA_ARGS__)
#elif defined(__GNUC__)
#define openpam_log(lvl, fmt...) \
	_openpam_log((lvl), __func__, ##fmt)
#else
extern openpam_log(int _level, const char *_format, ...);
#endif

/*
 * Generic conversation function
 */
struct pam_message;
struct pam_response;
int openpam_ttyconv(int _n,
	const struct pam_message **_msg,
	struct pam_response **_resp,
	void *_data);

/*
 * PAM primitives
 */
enum {
	PAM_SM_AUTHENTICATE,
	PAM_SM_SETCRED,
	PAM_SM_ACCT_MGMT,
	PAM_SM_OPEN_SESSION,
	PAM_SM_CLOSE_SESSION,
	PAM_SM_CHAUTHTOK,
	/* keep this last */
	PAM_NUM_PRIMITIVES
};

/*
 * Dummy service module function
 */
#define PAM_SM_DUMMY(type)						\
PAM_EXTERN int								\
pam_sm_##type(pam_handle_t *pamh, int flags,				\
    int argc, const char *argv[])					\
{									\
	return (PAM_IGNORE);						\
}

/*
 * PAM service module functions match this typedef
 */
struct pam_handle;
typedef int (*pam_func_t)(struct pam_handle *, int, int, const char **);

/*
 * A struct that describes a module.
 */
typedef struct pam_module pam_module_t;
struct pam_module {
	const char	*path;
	pam_func_t	 func[PAM_NUM_PRIMITIVES];
	void		*dlh;
	int		 refcount;
	pam_module_t	*prev;
	pam_module_t	*next;
};

/*
 * Infrastructure for static modules using GCC linker sets.
 * You are not expected to understand this.
 */
#if defined(__GNUC__) && !defined(__PIC__)
#if defined(__FreeBSD__)
#define PAM_SOEXT ".so"
#else
#error Static linking is not supported on your platform
#endif
/* gcc, static linking */
#include <sys/cdefs.h>
#include <linker_set.h>
#define OPENPAM_STATIC_MODULES
#define PAM_EXTERN static
#define PAM_MODULE_ENTRY(name)						\
static struct pam_module _pam_module = { name PAM_SOEXT, {		\
    pam_sm_authenticate, pam_sm_setcred, pam_sm_acct_mgmt,		\
    pam_sm_open_session, pam_sm_close_session, pam_sm_chauthtok },	\
    NULL, 0, NULL, NULL };						\
DATA_SET(_openpam_static_modules, _pam_module)
#else
/* normal case */
#define PAM_EXTERN
#define PAM_MODULE_ENTRY(name)
#endif

#ifdef __cplusplus
}
#endif

#endif
