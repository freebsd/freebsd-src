/*-
 * Copyright (c) 2005 McAfee, Inc.
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/mac.h>

#include <security/mac_bsdextended/mac_bsdextended.h>

#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <ugidfw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Starting point for a regression test for mac_bsdextended(4) and the
 * supporting libugidfw(3).
 */
void
usage(void)
{

	fprintf(stderr, "test_ugidfw\n");
	exit(-1);
}

/*
 * This section of the regression test passes some test cases through the
 * rule<->string routines to confirm they work approximately as desired.
 */

/*
 * List of users and groups we must check exists before we can begin, since
 * they are used in the string test rules.  We use users and groups that will
 * always exist in a default install used for regression testing.
 */
static const char *test_users[] = {
	"root",
	"daemon",
	"operator",
	"bin",
};
static const int test_users_len = sizeof(test_users) / sizeof(char *);

static const char *test_groups[] = {
	"wheel",
	"daemon",
	"operator",
	"bin",
};
static const int test_groups_len = sizeof(test_groups) / sizeof(char *);

/*
 * List of test strings that must go in (and come out) of libugidfw intact.
 */
static const char *test_strings[] = {
	/* Variations on subject and object uids. */
	"subject uid root object uid root mode n",
	"subject uid root object uid daemon mode n",
	"subject uid daemon object uid root mode n",
	"subject uid daemon object uid daemon mode n",
	/* Variations on mode. */
	"subject uid root object uid root mode a",
	"subject uid root object uid root mode r",
	"subject uid root object uid root mode s",
	"subject uid root object uid root mode w",
	"subject uid root object uid root mode x",
	"subject uid root object uid root mode arswx",
	/* Variations on subject and object gids. */
	"subject gid wheel object gid wheel mode n",
	"subject gid wheel object gid daemon mode n",
	"subject gid daemon object gid wheel mode n",
	"subject gid daemon object gid daemon mode n",
	/* Subject uids and subject gids. */
	"subject uid bin gid daemon object uid operator gid wheel mode n",
	/* Not */
	"subject not uid operator object uid bin mode n",
	"subject uid bin object not uid operator mode n",
	"subject not uid daemon object not uid operator mode n",
};
static const int test_strings_len = sizeof(test_strings) / sizeof(char *);

static void
test_libugidfw_strings(void)
{
	struct mac_bsdextended_rule rule;
	char errorstr[128];
	char rulestr[128];
	int i, error;

	for (i = 0; i < test_users_len; i++) {
		if (getpwnam(test_users[i]) == NULL)
			err(-1, "test_libugidfw_strings: getpwnam: %s",
			    test_users[i]);
	}

	for (i = 0; i < test_groups_len; i++) {
		if (getgrnam(test_groups[i]) == NULL)
			err(-1, "test_libugidfw_strings: getgrnam: %s",
			    test_groups[i]);
	}

	for (i = 0; i < test_strings_len; i++) {
		error = bsde_parse_rule_string(test_strings[i], &rule,
		    128, errorstr);
		if (error == -1)
			errx(-1, "bsde_parse_rule_string: '%s' (%d): %s",
			    test_strings[i], i, errorstr);
		error = bsde_rule_to_string(&rule, rulestr, 128);
		if (error < 0)
			errx(-1, "bsde_rule_to_string: rule for '%s' "
			    "returned %d", test_strings[i], error);

		if (strcmp(test_strings[i], rulestr) != 0)
			errx(-1, "test_libugidfw: '%s' in, '%s' out",
			    test_strings[i], rulestr);
	}
}

int
main(int argc, char *argv[])
{
	char errorstr[128];
	int count, slots;

	if (argc != 1)
		usage();

	/* Print an error if a non-root user attemps to run the tests. */
	if (getuid() != 0) {
		fprintf(stderr, "Error!  Only root may run this utility\n");
		return (EXIT_FAILURE);
	}

	/*
	 * We can test some parts of the library without the MAC Framework
	 * and policy loaded, so run those tests before calling
	 * mac_is_present().
	 */
	test_libugidfw_strings();

	switch (mac_is_present("bsdextended")) {
	case -1:
		err(-1, "mac_is_present");
	case 1:
		break;
	case 0:
	default:
		errx(-1, "mac_bsdextended not loaded");
	}

	/*
	 * Some simple up-front checks to see if we're able to query the
	 * policy for basic state.  We want the rule count to be 0 before
	 * starting, but "slots" is a property of prior runs and so we ignore
	 * the return value.
	 */
	count = bsde_get_rule_count(128, errorstr);
	if (count == -1)
		errx(-1, "bsde_get_rule_count: %s", errorstr);
	if (count != 0)
		errx(-1, "bsde_get_rule_count: %d rules", count);

	slots = bsde_get_rule_slots(128, errorstr);
	if (slots == -1)
		errx(-1, "bsde_get_rule_slots: %s", errorstr);

	return (0);
}
