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
 * $P4: //depot/projects/openpam/lib/openpam_configure.c#10 $
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

const char *_pam_facility_name[PAM_NUM_FACILITIES] = {
	[PAM_ACCOUNT]		= "account",
	[PAM_AUTH]		= "auth",
	[PAM_PASSWORD]		= "password",
	[PAM_SESSION]		= "session",
};

const char *_pam_control_flag_name[PAM_NUM_CONTROL_FLAGS] = {
	[PAM_BINDING]		= "binding",
	[PAM_OPTIONAL]		= "optional",
	[PAM_REQUIRED]		= "required",
	[PAM_REQUISITE]		= "requisite",
	[PAM_SUFFICIENT]	= "sufficient",
};

static int openpam_load_chain(pam_handle_t *, const char *, pam_facility_t);

/*
 * Matches a word against the first one in a string.
 * Returns non-zero if they match.
 */
static int
match_word(const char *str, const char *word)
{

	while (*str && tolower(*str) == tolower(*word))
		++str, ++word;
	return (*str == ' ' && *word == '\0');
}

/*
 * Return a pointer to the next word (or the final NUL) in a string.
 */
static const char *
next_word(const char *str)
{

	/* skip current word */
	while (*str && *str != ' ')
		++str;
	/* skip whitespace */
	while (*str == ' ')
		++str;
	return (str);
}

/*
 * Return a malloc()ed copy of the first word in a string.
 */
static char *
dup_word(const char *str)
{
	const char *end;
	char *word;

	for (end = str; *end && *end != ' '; ++end)
		/* nothing */ ;
	if (asprintf(&word, "%.*s", (int)(end - str), str) < 0)
		return (NULL);
	return (word);
}

/*
 * Return the length of the first word in a string.
 */
static int
wordlen(const char *str)
{
	int i;

	for (i = 0; str[i] && str[i] != ' '; ++i)
		/* nothing */ ;
	return (i);
}

typedef enum { pam_conf_style, pam_d_style } openpam_style_t;

/*
 * Extracts given chains from a policy file.
 */
static int
openpam_read_chain(pam_handle_t *pamh,
	const char *service,
	pam_facility_t facility,
	const char *filename,
	openpam_style_t style)
{
	pam_chain_t *this, **next;
	const char *p, *q;
	int count, i, lineno, ret;
	pam_facility_t fclt;
	pam_control_t ctlf;
	char *line, *name;
	FILE *f;

	if ((f = fopen(filename, "r")) == NULL) {
		openpam_log(errno == ENOENT ? PAM_LOG_DEBUG : PAM_LOG_NOTICE,
		    "%s: %m", filename);
		return (0);
	}
	this = NULL;
	count = lineno = 0;
	while ((line = openpam_readline(f, &lineno, NULL)) != NULL) {
		p = line;

		/* match service name */
		if (style == pam_conf_style) {
			if (!match_word(p, service)) {
				FREE(line);
				continue;
			}
			p = next_word(p);
		}

		/* match facility name */
		for (fclt = 0; fclt < PAM_NUM_FACILITIES; ++fclt)
			if (match_word(p, _pam_facility_name[fclt]))
				break;
		if (fclt == PAM_NUM_FACILITIES) {
			openpam_log(PAM_LOG_NOTICE,
			    "%s(%d): invalid facility '%.*s' (ignored)",
			    filename, lineno, wordlen(p), p);
			goto fail;
		}
		if (facility != fclt && facility != PAM_FACILITY_ANY) {
			FREE(line);
			continue;
		}
		p = next_word(p);

		/* include other chain */
		if (match_word(p, "include")) {
			p = next_word(p);
			if (*next_word(p) != '\0')
				openpam_log(PAM_LOG_NOTICE,
				    "%s(%d): garbage at end of 'include' line",
				    filename, lineno);
			if ((name = dup_word(p)) == NULL)
				goto syserr;
			ret = openpam_load_chain(pamh, name, fclt);
			fprintf(stderr, "include %s returned %d\n", name, ret);
			FREE(name);
			if (ret < 0)
				goto fail;
			count += ret;
			FREE(line);
			continue;
		}

		/* allocate new entry */
		if ((this = calloc(1, sizeof *this)) == NULL)
			goto syserr;

		/* control flag */
		for (ctlf = 0; ctlf < PAM_NUM_CONTROL_FLAGS; ++ctlf)
			if (match_word(p, _pam_control_flag_name[ctlf]))
				break;
		if (ctlf == PAM_NUM_CONTROL_FLAGS) {
			openpam_log(PAM_LOG_ERROR,
			    "%s(%d): invalid control flag '%.*s'",
			    filename, lineno, wordlen(p), p);
			goto fail;
		}
		this->flag = ctlf;

		/* module name */
		p = next_word(p);
		if (*p == '\0') {
			openpam_log(PAM_LOG_ERROR,
			    "%s(%d): missing module name",
			    filename, lineno);
			goto fail;
		}
		if ((name = dup_word(p)) == NULL)
			goto syserr;
		this->module = openpam_load_module(name);
		FREE(name);
		if (this->module == NULL)
			goto fail;

		/* module options */
		p = q = next_word(p);
		while (*q != '\0') {
			++this->optc;
			q = next_word(q);
		}
		this->optv = calloc(this->optc + 1, sizeof(char *));
		if (this->optv == NULL)
			goto syserr;
		for (i = 0; i < this->optc; ++i) {
			if ((this->optv[i] = dup_word(p)) == NULL)
				goto syserr;
			p = next_word(p);
		}

		/* hook it up */
		for (next = &pamh->chains[fclt]; *next != NULL;
		     next = &(*next)->next)
			/* nothing */ ;
		*next = this;
		this = NULL;
		++count;

		/* next please... */
		FREE(line);
	}
	if (!feof(f))
		goto syserr;
	fclose(f);
	return (count);
 syserr:
	openpam_log(PAM_LOG_ERROR, "%s: %m", filename);
 fail:
	FREE(this);
	FREE(line);
	fclose(f);
	return (-1);
}

static const char *openpam_policy_path[] = {
	"/etc/pam.d/",
	"/etc/pam.conf",
	"/usr/local/etc/pam.d/",
	"/usr/local/etc/pam.conf",
	NULL
};

/*
 * Locates the policy file for a given service and reads the given chains
 * from it.
 */
static int
openpam_load_chain(pam_handle_t *pamh,
	const char *service,
	pam_facility_t facility)
{
	const char **path;
	char *filename;
	size_t len;
	int r;

	for (path = openpam_policy_path; *path != NULL; ++path) {
		len = strlen(*path);
		if ((*path)[len - 1] == '/') {
			if (asprintf(&filename, "%s%s", *path, service) < 0) {
				openpam_log(PAM_LOG_ERROR, "asprintf(): %m");
				return (-PAM_BUF_ERR);
			}
			r = openpam_read_chain(pamh, service, facility,
			    filename, pam_d_style);
			FREE(filename);
		} else {
			r = openpam_read_chain(pamh, service, facility,
			    *path, pam_conf_style);
		}
		if (r != 0)
			return (r);
	}
	return (0);
}

/*
 * OpenPAM internal
 *
 * Configure a service
 */

int
openpam_configure(pam_handle_t *pamh,
	const char *service)
{
	pam_facility_t fclt;

	if (openpam_load_chain(pamh, service, PAM_FACILITY_ANY) < 0)
		goto load_err;

	for (fclt = 0; fclt < PAM_NUM_FACILITIES; ++fclt) {
		if (pamh->chains[fclt] != NULL)
			continue;
		if (openpam_load_chain(pamh, PAM_OTHER, fclt) < 0)
			goto load_err;
	}
	return (PAM_SUCCESS);
 load_err:
	openpam_clear_chains(pamh->chains);
	return (PAM_SYSTEM_ERR);
}

/*
 * NODOC
 *
 * Error codes:
 *	PAM_SYSTEM_ERR
 */
