/*-
 * Copyright (c) 2001 Networks Associates Technologies, Inc.
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

#ifndef _OPENPAM_IMPL_H_INCLUDED
#define _OPENPAM_IMPL_H_INCLUDED

#include <security/openpam.h>

extern const char *_pam_sm_func_name[PAM_NUM_PRIMITIVES];

/*
 * Control flags
 */
#define PAM_REQUIRED		1
#define PAM_REQUISITE		2
#define PAM_SUFFICIENT		3
#define PAM_OPTIONAL		4
#define PAM_NUM_CONTROLFLAGS	5

/*
 * Chains
 */
#define PAM_AUTH		0
#define PAM_ACCOUNT		1
#define PAM_SESSION		2
#define PAM_PASSWORD		3
#define PAM_NUM_CHAINS		4

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
	pam_chain_t	*chains[PAM_NUM_CHAINS];
	pam_chain_t	*current;

	/* items and data */
	void		*item[PAM_NUM_ITEMS];
	pam_data_t	*module_data;

	/* environment list */
	char	       **env;
	int		 env_count;
	int		 env_size;
};

#define PAM_OTHER	"other"

int		openpam_dispatch(pam_handle_t *, int, int);
int		openpam_findenv(pam_handle_t *, const char *, size_t);
int		openpam_add_module(pam_handle_t *, int, int,
				   const char *, int, const char **);
void		openpam_clear_chains(pam_handle_t *);

#ifdef OPENPAM_STATIC_MODULES
pam_module_t   *openpam_static(const char *);
#endif

#endif
