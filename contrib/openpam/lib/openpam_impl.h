/*-
 * Copyright (c) 2001-2003 Networks Associates Technology, Inc.
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
 * $P4: //depot/projects/openpam/lib/openpam_impl.h#29 $
 */

#ifndef _OPENPAM_IMPL_H_INCLUDED
#define _OPENPAM_IMPL_H_INCLUDED

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <security/openpam.h>

extern const char *_pam_func_name[PAM_NUM_PRIMITIVES];
extern const char *_pam_sm_func_name[PAM_NUM_PRIMITIVES];
extern const char *_pam_err_name[PAM_NUM_ERRORS];
extern const char *_pam_item_name[PAM_NUM_ITEMS];

extern int _openpam_debug;

/*
 * Control flags
 */
typedef enum {
	PAM_BINDING,
	PAM_REQUIRED,
	PAM_REQUISITE,
	PAM_SUFFICIENT,
	PAM_OPTIONAL,
	PAM_NUM_CONTROL_FLAGS
} pam_control_t;

/*
 * Facilities
 */
typedef enum {
	PAM_FACILITY_ANY = -1,
	PAM_AUTH = 0,
	PAM_ACCOUNT,
	PAM_SESSION,
	PAM_PASSWORD,
	PAM_NUM_FACILITIES
} pam_facility_t;

typedef struct pam_chain pam_chain_t;
struct pam_chain {
	pam_module_t	*module;
	int		 flag;
	int		 optc;
	char	       **optv;
	pam_chain_t	*next;
};

typedef struct pam_data pam_data_t;
struct pam_data {
	char		*name;
	void		*data;
	void		(*cleanup)(pam_handle_t *, void *, int);
	pam_data_t	*next;
};

struct pam_handle {
	char		*service;

	/* chains */
	pam_chain_t	*chains[PAM_NUM_FACILITIES];
	pam_chain_t	*current;
	int		 primitive;

	/* items and data */
	void		*item[PAM_NUM_ITEMS];
	pam_data_t	*module_data;

	/* environment list */
	char	       **env;
	int		 env_count;
	int		 env_size;
};

#ifdef NGROUPS_MAX
#define PAM_SAVED_CRED "pam_saved_cred"
struct pam_saved_cred {
	uid_t	 euid;
	gid_t	 egid;
	gid_t	 groups[NGROUPS_MAX];
	int	 ngroups;
};
#endif

#define PAM_OTHER	"other"

int		 openpam_configure(pam_handle_t *, const char *);
int		 openpam_dispatch(pam_handle_t *, int, int);
int		 openpam_findenv(pam_handle_t *, const char *, size_t);
pam_module_t	*openpam_load_module(const char *);
void		 openpam_clear_chains(pam_chain_t **);

#ifdef OPENPAM_STATIC_MODULES
pam_module_t	*openpam_static(const char *);
#endif
pam_module_t	*openpam_dynamic(const char *);

#define	FREE(p) do { free((p)); (p) = NULL; } while (0)

#ifdef DEBUG
#define ENTER() openpam_log(PAM_LOG_DEBUG, "entering")
#define ENTERI(i) do { \
	int _i = (i); \
	if (_i > 0 && _i < PAM_NUM_ITEMS) \
		openpam_log(PAM_LOG_DEBUG, "entering: %s", _pam_item_name[_i]); \
	else \
		openpam_log(PAM_LOG_DEBUG, "entering: %d", _i); \
} while (0)
#define ENTERN(n) do { \
	int _n = (n); \
	openpam_log(PAM_LOG_DEBUG, "entering: %d", _n); \
} while (0)
#define ENTERS(s) do { \
	const char *_s = (s); \
	if (_s == NULL) \
		openpam_log(PAM_LOG_DEBUG, "entering: NULL"); \
	else \
		openpam_log(PAM_LOG_DEBUG, "entering: '%s'", _s); \
} while (0)
#define	RETURNV() openpam_log(PAM_LOG_DEBUG, "returning")
#define RETURNC(c) do { \
	int _c = (c); \
	if (_c >= 0 && _c < PAM_NUM_ERRORS) \
		openpam_log(PAM_LOG_DEBUG, "returning %s", _pam_err_name[_c]); \
	else \
		openpam_log(PAM_LOG_DEBUG, "returning %d!", _c); \
	return (_c); \
} while (0)
#define	RETURNN(n) do { \
	int _n = (n); \
	openpam_log(PAM_LOG_DEBUG, "returning %d", _n); \
	return (_n); \
} while (0)
#define	RETURNP(p) do { \
	const void *_p = (p); \
	if (_p == NULL) \
		openpam_log(PAM_LOG_DEBUG, "returning NULL"); \
	else \
		openpam_log(PAM_LOG_DEBUG, "returning %p", _p); \
	return (p); \
} while (0)
#define	RETURNS(s) do { \
	const char *_s = (s); \
	if (_s == NULL) \
		openpam_log(PAM_LOG_DEBUG, "returning NULL"); \
	else \
		openpam_log(PAM_LOG_DEBUG, "returning '%s'", _s); \
	return (_s); \
} while (0)
#else
#define ENTER()
#define ENTERI(i)
#define ENTERN(n)
#define ENTERS(s)
#define RETURNV() return
#define RETURNC(c) return (c)
#define RETURNN(n) return (n)
#define RETURNP(p) return (p)
#define RETURNS(s) return (s)
#endif

#endif
