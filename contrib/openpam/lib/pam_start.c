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
 * $P4: //depot/projects/openpam/lib/pam_start.c#12 $
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <security/pam_appl.h>

#include "openpam_impl.h"

static int _pam_configure_service(pam_handle_t *pamh, const char *service);

/*
 * XSSO 4.2.1
 * XSSO 6 page 89
 *
 * Initiate a PAM transaction
 */

int
pam_start(const char *service,
	const char *user,
	const struct pam_conv *pam_conv,
	pam_handle_t **pamh)
{
	struct pam_handle *ph;
	int r;

	if ((ph = calloc(1, sizeof *ph)) == NULL)
		return (PAM_BUF_ERR);
	if ((r = pam_set_item(ph, PAM_SERVICE, service)) != PAM_SUCCESS)
		goto fail;
	if ((r = pam_set_item(ph, PAM_USER, user)) != PAM_SUCCESS)
		goto fail;
	if ((r = pam_set_item(ph, PAM_CONV, pam_conv)) != PAM_SUCCESS)
		goto fail;

	if ((r = _pam_configure_service(ph, service)) != PAM_SUCCESS &&
	    r != PAM_BUF_ERR)
		r = _pam_configure_service(ph, PAM_OTHER);
	if (r != PAM_SUCCESS)
		goto fail;

	*pamh = ph;
	openpam_log(PAM_LOG_DEBUG, "pam_start(\"%s\") succeeded", service);
	return (PAM_SUCCESS);

 fail:
	pam_end(ph, r);
	return (r);
}

#define PAM_CONF_STYLE	0
#define PAM_D_STYLE	1
#define MAX_LINE_LEN	1024
#define MAX_OPTIONS	256

static int
_pam_read_policy_file(pam_handle_t *pamh,
	const char *service,
	const char *filename,
	int style)
{
	char buf[MAX_LINE_LEN], *p, *q;
	const char *optv[MAX_OPTIONS + 1];
	int ch, chain, flag, line, optc, n, r;
	size_t len;
	FILE *f;

	n = 0;

	if ((f = fopen(filename, "r")) == NULL) {
		openpam_log(errno == ENOENT ? PAM_LOG_DEBUG : PAM_LOG_NOTICE,
		    "%s: %m", filename);
		return (0);
	}
	openpam_log(PAM_LOG_DEBUG, "looking for '%s' in %s",
	    service, filename);

	for (line = 1; fgets(buf, MAX_LINE_LEN, f) != NULL; ++line) {
		if ((len = strlen(buf)) == 0)
			continue;

		/* check for overflow */
		if (buf[--len] != '\n' && !feof(f)) {
			openpam_log(PAM_LOG_ERROR, "%s: line %d too long",
			    filename, line);
			openpam_log(PAM_LOG_ERROR, "%s: ignoring line %d",
			    filename, line);
			while ((ch = fgetc(f)) != EOF)
				if (ch == '\n')
					break;
			continue;
		}

		/* strip comments and trailing whitespace */
		if ((p = strchr(buf, '#')) != NULL)
			len = p - buf ? p - buf - 1 : p - buf;
		while (len > 0 && isspace(buf[len - 1]))
			--len;
		if (len == 0)
			continue;
		buf[len] = '\0';
		p = q = buf;

		/* check service name */
		if (style == PAM_CONF_STYLE) {
			for (q = p = buf; *q != '\0' && !isspace(*q); ++q)
				/* nothing */;
			if (*q == '\0')
				goto syntax_error;
			*q++ = '\0';
			if (strcmp(p, service) != 0)
				continue;
			openpam_log(PAM_LOG_DEBUG, "%s: line %d matches '%s'",
			    filename, line, service);
		}


		/* get module type */
		for (p = q; isspace(*p); ++p)
			/* nothing */;
		for (q = p; *q != '\0' && !isspace(*q); ++q)
			/* nothing */;
		if (q == p || *q == '\0')
			goto syntax_error;
		*q++ = '\0';
		if (strcmp(p, "auth") == 0) {
			chain = PAM_AUTH;
		} else if (strcmp(p, "account") == 0) {
			chain = PAM_ACCOUNT;
		} else if (strcmp(p, "session") == 0) {
			chain = PAM_SESSION;
		} else if (strcmp(p, "password") == 0) {
			chain = PAM_PASSWORD;
		} else {
			openpam_log(PAM_LOG_ERROR,
			    "%s: invalid module type on line %d: '%s'",
			    filename, line, p);
			continue;
		}

		/* get control flag */
		for (p = q; isspace(*p); ++p)
			/* nothing */;
		for (q = p; *q != '\0' && !isspace(*q); ++q)
			/* nothing */;
		if (q == p || *q == '\0')
			goto syntax_error;
		*q++ = '\0';
		if (strcmp(p, "required") == 0) {
			flag = PAM_REQUIRED;
		} else if (strcmp(p, "requisite") == 0) {
			flag = PAM_REQUISITE;
		} else if (strcmp(p, "sufficient") == 0) {
			flag = PAM_SUFFICIENT;
		} else if (strcmp(p, "optional") == 0) {
			flag = PAM_OPTIONAL;
		} else {
			openpam_log(PAM_LOG_ERROR,
			    "%s: invalid control flag on line %d: '%s'",
			    filename, line, p);
			continue;
		}

		/* get module name */
		for (p = q; isspace(*p); ++p)
			/* nothing */;
		for (q = p; *q != '\0' && !isspace(*q); ++q)
			/* nothing */;
		if (q == p)
			goto syntax_error;

		/* get options */
		for (optc = 0; *q != '\0' && optc < MAX_OPTIONS; ++optc) {
			*q++ = '\0';
			while (isspace(*q))
				++q;
			optv[optc] = q;
			while (*q != '\0' && !isspace(*q))
				++q;
		}
		optv[optc] = NULL;
		if (*q != '\0') {
			*q = '\0';
			openpam_log(PAM_LOG_ERROR,
			    "%s: too many options on line %d",
			    filename, line);
		}

		/*
		 * Finally, add the module at the end of the
		 * appropriate chain and bump the counter.
		 */
		r = openpam_add_module(pamh, chain, flag, p, optc, optv);
		if (r != PAM_SUCCESS)
			return (-r);
		++n;
		continue;
 syntax_error:
		openpam_log(PAM_LOG_ERROR, "%s: syntax error on line %d",
		    filename, line);
		openpam_log(PAM_LOG_DEBUG, "%s: line %d: [%s]",
		    filename, line, q);
		openpam_log(PAM_LOG_ERROR, "%s: ignoring line %d",
		    filename, line);
	}

	if (ferror(f))
		openpam_log(PAM_LOG_ERROR, "%s: %m", filename);

	fclose(f);
	return (n);
}

static const char *_pam_policy_path[] = {
	"/etc/pam.d/",
	"/etc/pam.conf",
	"/usr/local/etc/pam.d/",
	NULL
};

static int
_pam_configure_service(pam_handle_t *pamh,
	const char *service)
{
	const char **path;
	char *filename;
	size_t len;
	int r;

	for (path = _pam_policy_path; *path != NULL; ++path) {
		len = strlen(*path);
		if ((*path)[len - 1] == '/') {
			filename = malloc(len + strlen(service) + 1);
			if (filename == NULL) {
				openpam_log(PAM_LOG_ERROR, "malloc(): %m");
				return (PAM_BUF_ERR);
			}
			strcpy(filename, *path);
			strcat(filename, service);
			r = _pam_read_policy_file(pamh,
			    service, filename, PAM_D_STYLE);
			free(filename);
		} else {
			r = _pam_read_policy_file(pamh,
			    service, *path, PAM_CONF_STYLE);
		}
		if (r < 0)
			return (-r);
		if (r > 0)
			return (PAM_SUCCESS);
	}

	return (PAM_SYSTEM_ERR);
}

/*
 * Error codes:
 *
 *	=pam_set_item
 *	!PAM_SYMBOL_ERR
 *	PAM_SYSTEM_ERR
 *	PAM_BUF_ERR
 */

/**
 * The =pam_start function creates and initializes a PAM context.
 *
 * The =service argument specifies the name of the policy to apply, and is
 * stored in the =PAM_SERVICE item in the created context.
 *
 * The =user argument specifies the name of the target user - the user the
 * created context will serve to authenticate.
 * It is stored in the =PAM_USER item in the created context.
 *
 * The =pam_conv argument points to a =struct pam_conv describing the
 * conversation function to use.
 * This structure is defined as follows:
 *
 *     struct pam_conv {
 *          int   (*conv)(int, const struct pam_message **,
 *              struct pam_response **, void *);
 *          void   *appdata_ptr;
 *     };
 *
 * >pam_get_item
 * >pam_set_item
 * >pam_end
 */
