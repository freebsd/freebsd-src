/*-
 * Copyright 1998 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	$FreeBSD$
 */

#include <security/pam_modules.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "pam_mod_misc.h"

/* Everyone has to have these options. It is not an error to
 * specify them and then not use them.
 */
struct opttab std_options[PAM_MAX_OPTIONS] = {
	{ "debug",		PAM_OPT_DEBUG },
	{ "no_warn",		PAM_OPT_NO_WARN },
	{ "echo_pass",		PAM_OPT_ECHO_PASS },
	{ "use_first_pass",	PAM_OPT_USE_FIRST_PASS },
	{ "try_first_pass",	PAM_OPT_TRY_FIRST_PASS },
	{ "use_mapped_pass",	PAM_OPT_USE_MAPPED_PASS },
	{ "expose_account",	PAM_OPT_EXPOSE_ACCOUNT },
	{ NULL,			0 }
};

/* Populate the options structure, syslogging all errors */
void
pam_std_option(struct options *options, struct opttab other_options[],
    int argc, const char **argv)
{
	struct opttab *oo;
	int i, j, arglen, found;

	j = 0;
	for (i = 0; i < PAM_MAX_OPTIONS; i++) {
		if (std_options[i].name == NULL) {
			j = i;
			break;
		}
		/* XXX Bad juju happens if loop exits with j == 0 */
	}
	if (other_options)
		for (oo = other_options; oo->name != NULL; oo++) {
			found = 0;
			for (i = 0; std_options[i].name; i++)
				if (strcmp((char *)oo->name,
				    std_options[i].name) == 0)
					found = 1;
			if (!found) {
				std_options[j].name = oo->name;
				std_options[j].value = oo->value;
				j++;
			}
		}
	for (i = 0; i < PAM_MAX_OPTIONS; i++) {
		options->opt[i].bool = 0;
		options->opt[i].arg = NULL;
	}
	if (j < PAM_MAX_OPTIONS) {
		std_options[j].name = NULL;
		std_options[j].value = 0;
	}
	for (j = 0; j < argc; j++) {
#ifdef DEBUG
		syslog(LOG_DEBUG, "Doing arg %s ", argv[j]);
#endif
		found = 0;
		for (i = 0; i < PAM_MAX_OPTIONS; i++) {
			if (std_options[i].name == NULL)
				break;
			if (strcmp(argv[j], std_options[i].name) == 0) {
				options->opt[std_options[i].value].bool = 1;
				found = 1;
				break;
			}
			else {
				arglen = strlen(std_options[i].name);
				if (strncmp(argv[j], std_options[i].name,
					    arglen) == 0
				    && argv[j][arglen] == '=')  {
					options->opt[std_options[i].value].bool
					    = 1;
					options->opt[std_options[i].value].arg =
					    strdup(&argv[j][arglen + 1]);
					found = 1;
					break;
				}
			}
		}
		if (!found)
			syslog(LOG_WARNING, "PAM option: %s invalid", argv[j]);
	}
}

/* Test if option is set in options */
int
pam_test_option(struct options *options, enum opt option, char **arg)
{
	if (arg != NULL)
		*arg = options->opt[option].arg;
	return options->opt[option].bool;
}

/* Set option in options, errors to syslog */
void
pam_set_option(struct options *options, enum opt option)
{
	if (option < PAM_OPT_STD_MAX)
		options->opt[option].bool = 1;
#ifdef DEBUG
	else
		syslog(LOG_DEBUG, "PAM options: attempt to set option %d",
		    option);
#endif
}

/* Clear option in options, errors to syslog */
void
pam_clear_option(struct options *options, enum opt option)
{
	if (option < PAM_OPT_STD_MAX)
		options->opt[option].bool = 0;
#ifdef DEBUG
	else
		syslog(LOG_DEBUG, "PAM options: attempt to clear option %d",
		    option);
#endif
}

#if DEBUG1
enum { PAM_OPT_FOO=PAM_OPT_STD_MAX, PAM_OPT_BAR, PAM_OPT_BAZ, PAM_OPT_QUX };

struct opttab other_options[] = {
	{ "foo", PAM_OPT_FOO },
	{ "bar", PAM_OPT_BAR },
	{ "baz", PAM_OPT_BAZ },
	{ "qux", PAM_OPT_QUX },
	{ NULL, 0 }
};

int
main(int argc, const char *argv[])
{
	struct options options;
	int i, opt;
	char *arg;

	pam_std_option(&options, other_options, argc, argv);
	for (i = 0; i < PAM_MAX_OPTIONS; i++) {
		opt = pam_test_option(&options, i, &arg);
		if (opt) {
			if (arg == NULL)
				printf("%d []\n", i);
			else
				printf("%d [%s]\n", i, arg);
		}
	}
	return 0;
}
#endif
