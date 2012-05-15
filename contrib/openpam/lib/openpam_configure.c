/*-
 * Copyright (c) 2001-2003 Networks Associates Technology, Inc.
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
 * $Id: openpam_configure.c 500 2011-11-22 12:07:03Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"
#include "openpam_strlcmp.h"

static int openpam_load_chain(pam_handle_t *, const char *, pam_facility_t);

/*
 * Evaluates to non-zero if the argument is a linear whitespace character.
 */
#define is_lws(ch)				\
	(ch == ' ' || ch == '\t')

/*
 * Evaluates to non-zero if the argument is a printable ASCII character.
 * Assumes that the execution character set is a superset of ASCII.
 */
#define is_p(ch) \
	(ch >= '!' && ch <= '~')

/*
 * Returns non-zero if the argument belongs to the POSIX Portable Filename
 * Character Set.  Assumes that the execution character set is a superset
 * of ASCII.
 */
#define is_pfcs(ch)				\
	((ch >= '0' && ch <= '9') ||		\
	 (ch >= 'A' && ch <= 'Z') ||		\
	 (ch >= 'a' && ch <= 'z') ||		\
	 ch == '.' || ch == '_' || ch == '-')

/*
 * Parse the service name.
 *
 * Returns the length of the service name, or 0 if the end of the string
 * was reached or a disallowed non-whitespace character was encountered.
 *
 * If parse_service_name() is successful, it updates *service to point to
 * the first character of the service name and *line to point one
 * character past the end.  If it reaches the end of the string, it
 * updates *line to point to the terminating NUL character and leaves
 * *service unmodified.  In all other cases, it leaves both *line and
 * *service unmodified.
 *
 * Allowed characters are all characters in the POSIX portable filename
 * character set.
 */
static int
parse_service_name(char **line, char **service)
{
	char *b, *e;

	for (b = *line; *b && is_lws(*b); ++b)
		/* nothing */ ;
	if (!*b) {
		*line = b;
		return (0);
	}
	for (e = b; *e && !is_lws(*e); ++e)
		if (!is_pfcs(*e))
			return (0);
	if (e == b)
		return (0);
	*line = e;
	*service = b;
	return (e - b);
}

/*
 * Parse the facility name.
 *
 * Returns the corresponding pam_facility_t value, or -1 if the end of the
 * string was reached, a disallowed non-whitespace character was
 * encountered, or the first word was not a recognized facility name.
 *
 * If parse_facility_name() is successful, it updates *line to point one
 * character past the end of the facility name.  If it reaches the end of
 * the string, it updates *line to point to the terminating NUL character.
 * In all other cases, it leaves *line unmodified.
 */
static pam_facility_t
parse_facility_name(char **line)
{
	char *b, *e;
	int i;

	for (b = *line; *b && is_lws(*b); ++b)
		/* nothing */ ;
	if (!*b) {
		*line = b;
		return ((pam_facility_t)-1);
	}
	for (e = b; *e && !is_lws(*e); ++e)
		/* nothing */ ;
	if (e == b)
		return ((pam_facility_t)-1);
	for (i = 0; i < PAM_NUM_FACILITIES; ++i)
		if (strlcmp(pam_facility_name[i], b, e - b) == 0)
			break;
	if (i == PAM_NUM_FACILITIES)
		return ((pam_facility_t)-1);
	*line = e;
	return (i);
}

/*
 * Parse the word "include".
 *
 * If the next word on the line is "include", parse_include() updates
 * *line to point one character past "include" and returns 1.  Otherwise,
 * it leaves *line unmodified and returns 0.
 */
static int
parse_include(char **line)
{
	char *b, *e;

	for (b = *line; *b && is_lws(*b); ++b)
		/* nothing */ ;
	if (!*b) {
		*line = b;
		return (-1);
	}
	for (e = b; *e && !is_lws(*e); ++e)
		/* nothing */ ;
	if (e == b)
		return (0);
	if (strlcmp("include", b, e - b) != 0)
		return (0);
	*line = e;
	return (1);
}

/*
 * Parse the control flag.
 *
 * Returns the corresponding pam_control_t value, or -1 if the end of the
 * string was reached, a disallowed non-whitespace character was
 * encountered, or the first word was not a recognized control flag.
 *
 * If parse_control_flag() is successful, it updates *line to point one
 * character past the end of the control flag.  If it reaches the end of
 * the string, it updates *line to point to the terminating NUL character.
 * In all other cases, it leaves *line unmodified.
 */
static pam_control_t
parse_control_flag(char **line)
{
	char *b, *e;
	int i;

	for (b = *line; *b && is_lws(*b); ++b)
		/* nothing */ ;
	if (!*b) {
		*line = b;
		return ((pam_control_t)-1);
	}
	for (e = b; *e && !is_lws(*e); ++e)
		/* nothing */ ;
	if (e == b)
		return ((pam_control_t)-1);
	for (i = 0; i < PAM_NUM_CONTROL_FLAGS; ++i)
		if (strlcmp(pam_control_flag_name[i], b, e - b) == 0)
			break;
	if (i == PAM_NUM_CONTROL_FLAGS)
		return ((pam_control_t)-1);
	*line = e;
	return (i);
}

/*
 * Parse a file name.
 *
 * Returns the length of the file name, or 0 if the end of the string was
 * reached or a disallowed non-whitespace character was encountered.
 *
 * If parse_filename() is successful, it updates *filename to point to the
 * first character of the filename and *line to point one character past
 * the end.  If it reaches the end of the string, it updates *line to
 * point to the terminating NUL character and leaves *filename unmodified.
 * In all other cases, it leaves both *line and *filename unmodified.
 *
 * Allowed characters are all characters in the POSIX portable filename
 * character set, plus the path separator (forward slash).
 */
static int
parse_filename(char **line, char **filename)
{
	char *b, *e;

	for (b = *line; *b && is_lws(*b); ++b)
		/* nothing */ ;
	if (!*b) {
		*line = b;
		return (0);
	}
	for (e = b; *e && !is_lws(*e); ++e)
		if (!is_pfcs(*e) && *e != '/')
			return (0);
	if (e == b)
		return (0);
	*line = e;
	*filename = b;
	return (e - b);
}

/*
 * Parse an option.
 *
 * Returns a dynamically allocated string containing the next module
 * option, or NULL if the end of the string was reached or a disallowed
 * non-whitespace character was encountered.
 *
 * If parse_option() is successful, it updates *line to point one
 * character past the end of the option.  If it reaches the end of the
 * string, it updates *line to point to the terminating NUL character.  In
 * all other cases, it leaves *line unmodified.
 *
 * If parse_option() fails to allocate memory, it will return NULL and set
 * errno to a non-zero value.
 *
 * Allowed characters for option names are all characters in the POSIX
 * portable filename character set.  Allowed characters for option values
 * are any printable non-whitespace characters.  The option value may be
 * quoted in either single or double quotes, in which case space
 * characters and whichever quote character was not used are allowed.
 * Note that the entire value must be quoted, not just part of it.
 */
static char *
parse_option(char **line)
{
	char *nb, *ne, *vb, *ve;
	unsigned char q = 0;
	char *option;
	size_t size;

	errno = 0;
	for (nb = *line; *nb && is_lws(*nb); ++nb)
		/* nothing */ ;
	if (!*nb) {
		*line = nb;
		return (NULL);
	}
	for (ne = nb; *ne && !is_lws(*ne) && *ne != '='; ++ne)
		if (!is_pfcs(*ne))
			return (NULL);
	if (ne == nb)
		return (NULL);
	if (*ne == '=') {
		vb = ne + 1;
		if (*vb == '"' || *vb == '\'')
			q = *vb++;
		for (ve = vb;
		     *ve && *ve != q && (is_p(*ve) || (q && is_lws(*ve)));
		     ++ve)
			/* nothing */ ;
		if (q && *ve != q)
			/* non-printable character or missing endquote */
			return (NULL);
		if (q && *(ve + 1) && !is_lws(*(ve + 1)))
			/* garbage after value */
			return (NULL);
	} else {
		vb = ve = ne;
	}
	size = (ne - nb) + 1;
	if (ve > vb)
		size += (ve - vb) + 1;
	if ((option = malloc(size)) == NULL)
		return (NULL);
	strncpy(option, nb, ne - nb);
	if (ve > vb) {
		option[ne - nb] = '=';
		strncpy(option + (ne - nb) + 1, vb, ve - vb);
	}
	option[size - 1] = '\0';
	*line = q ? ve + 1 : ve;
	return (option);
}

/*
 * Consume trailing whitespace.
 *
 * If there are no non-whitespace characters left on the line, parse_eol()
 * updates *line to point at the terminating NUL character and returns 0.
 * Otherwise, it leaves *line unmodified and returns a non-zero value.
 */
static int
parse_eol(char **line)
{
	char *p;

	for (p = *line; *p && is_lws(*p); ++p)
		/* nothing */ ;
	if (*p)
		return ((unsigned char)*p);
	*line = p;
	return (0);
}

typedef enum { pam_conf_style, pam_d_style } openpam_style_t;

/*
 * Extracts given chains from a policy file.
 */
static int
openpam_parse_chain(pam_handle_t *pamh,
	const char *service,
	pam_facility_t facility,
	const char *filename,
	openpam_style_t style)
{
	pam_chain_t *this, **next;
	pam_facility_t fclt;
	pam_control_t ctlf;
	char *line0, *line, *str, *name;
	char *option, **optv;
	int len, lineno, ret;
	FILE *f;

	if ((f = fopen(filename, "r")) == NULL) {
		openpam_log(errno == ENOENT ? PAM_LOG_DEBUG : PAM_LOG_NOTICE,
		    "%s: %m", filename);
		return (PAM_SUCCESS);
	}
	if (openpam_check_desc_owner_perms(filename, fileno(f)) != 0) {
		fclose(f);
		return (PAM_SYSTEM_ERR);
	}
	this = NULL;
	name = NULL;
	lineno = 0;
	while ((line0 = line = openpam_readline(f, &lineno, NULL)) != NULL) {
		/* get service name if necessary */
		if (style == pam_conf_style) {
			if ((len = parse_service_name(&line, &str)) == 0) {
				openpam_log(PAM_LOG_NOTICE,
				    "%s(%d): invalid service name (ignored)",
				    filename, lineno);
				FREE(line0);
				continue;
			}
			if (strlcmp(service, str, len) != 0) {
				FREE(line0);
				continue;
			}
		}

		/* get facility name */
		if ((fclt = parse_facility_name(&line)) == (pam_facility_t)-1) {
			openpam_log(PAM_LOG_ERROR,
			    "%s(%d): missing or invalid facility",
			    filename, lineno);
			goto fail;
		}
		if (facility != fclt && facility != PAM_FACILITY_ANY) {
			FREE(line0);
			continue;
		}

		/* check for "include" */
		if (parse_include(&line)) {
			if ((len = parse_service_name(&line, &str)) == 0) {
				openpam_log(PAM_LOG_ERROR,
				    "%s(%d): missing or invalid filename",
				    filename, lineno);
				goto fail;
			}
			if ((name = strndup(str, len)) == NULL)
				goto syserr;
			if (parse_eol(&line) != 0) {
				openpam_log(PAM_LOG_ERROR,
				    "%s(%d): garbage at end of line",
				    filename, lineno);
				goto fail;
			}
			ret = openpam_load_chain(pamh, name, fclt);
			FREE(name);
			if (ret != PAM_SUCCESS)
				goto fail;
			FREE(line0);
			continue;
		}

		/* get control flag */
		if ((ctlf = parse_control_flag(&line)) == (pam_control_t)-1) {
			openpam_log(PAM_LOG_ERROR,
			    "%s(%d): missing or invalid control flag",
			    filename, lineno);
			goto fail;
		}

		/* get module name */
		if ((len = parse_filename(&line, &str)) == 0) {
			openpam_log(PAM_LOG_ERROR,
			    "%s(%d): missing or invalid module name",
			    filename, lineno);
			goto fail;
		}
		if ((name = strndup(str, len)) == NULL)
			goto syserr;

		/* allocate new entry */
		if ((this = calloc(1, sizeof *this)) == NULL)
			goto syserr;
		this->flag = ctlf;

		/* get module options */
		if ((this->optv = malloc(sizeof *optv)) == NULL)
			goto syserr;
		this->optc = 0;
		while ((option = parse_option(&line)) != NULL) {
			optv = realloc(this->optv,
			    (this->optc + 2) * sizeof *optv);
			if (optv == NULL)
				goto syserr;
			this->optv = optv;
			this->optv[this->optc++] = option;
		}
		this->optv[this->optc] = NULL;
		if (*line != '\0') {
			openpam_log(PAM_LOG_ERROR,
			    "%s(%d): syntax error in module options",
			    filename, lineno);
			goto fail;
		}

		/* load module */
		this->module = openpam_load_module(name);
		FREE(name);
		if (this->module == NULL)
			goto fail;

		/* hook it up */
		for (next = &pamh->chains[fclt]; *next != NULL;
		     next = &(*next)->next)
			/* nothing */ ;
		*next = this;
		this = NULL;

		/* next please... */
		FREE(line0);
	}
	if (!feof(f))
		goto syserr;
	fclose(f);
	return (PAM_SUCCESS);
syserr:
	openpam_log(PAM_LOG_ERROR, "%s: %m", filename);
fail:
	if (this && this->optc) {
		while (this->optc--)
			FREE(this->optv[this->optc]);
		FREE(this->optv);
	}
	FREE(this);
	FREE(line0);
	FREE(name);
	fclose(f);
	return (PAM_SYSTEM_ERR);
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
	int ret;

	/* don't allow to escape from policy_path */
	if (strchr(service, '/')) {
		openpam_log(PAM_LOG_ERROR, "invalid service name: %s",
		    service);
		return (-PAM_SYSTEM_ERR);
	}

	for (path = openpam_policy_path; *path != NULL; ++path) {
		len = strlen(*path);
		if ((*path)[len - 1] == '/') {
			if (asprintf(&filename, "%s%s", *path, service) < 0) {
				openpam_log(PAM_LOG_ERROR, "asprintf(): %m");
				return (PAM_BUF_ERR);
			}
			ret = openpam_parse_chain(pamh, service, facility,
			    filename, pam_d_style);
			FREE(filename);
		} else {
			ret = openpam_parse_chain(pamh, service, facility,
			    *path, pam_conf_style);
		}
		if (ret != PAM_SUCCESS)
			return (ret);
	}
	return (PAM_SUCCESS);
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
	const char *p;

	for (p = service; *p; ++p)
		if (!is_pfcs(*p))
			return (PAM_SYSTEM_ERR);

	if (openpam_load_chain(pamh, service, PAM_FACILITY_ANY) != PAM_SUCCESS)
		goto load_err;

	for (fclt = 0; fclt < PAM_NUM_FACILITIES; ++fclt) {
		if (pamh->chains[fclt] != NULL)
			continue;
		if (openpam_load_chain(pamh, PAM_OTHER, fclt) != PAM_SUCCESS)
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
