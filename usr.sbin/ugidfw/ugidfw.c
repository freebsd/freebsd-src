/*-
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
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
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <security/mac_bsdextended/mac_bsdextended.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ugidfw.h>

void
usage(void)
{

	fprintf(stderr, "ugidfw list\n");
	fprintf(stderr, "ugidfw set rulenum [subject [not] [uid uid] [gid gid]]"
	    " [object [not] \\\n");
	fprintf(stderr, "    [uid uid] [gid gid]] mode arswxn\n");
	fprintf(stderr, "ugidfw remove rulenum\n");

	exit(-1);
}

void
list_rules(void)
{
	char errstr[BUFSIZ], charstr[BUFSIZ];
	struct mac_bsdextended_rule rule;
	int error, i, rule_count, rule_slots;

	rule_slots = bsde_get_rule_slots(BUFSIZ, errstr);
	if (rule_slots == -1) {
		fprintf(stderr, "Unable to get rule slots; mac_bsdextended.ko "
		    "may not be loaded.\n");
		fprintf(stderr, "bsde_get_rule_slots: %s\n", errstr);
		exit (-1);
	}

	rule_count = bsde_get_rule_count(BUFSIZ, errstr);
	if (rule_count == -1) {
		fprintf(stderr, "bsde_get_rule_count: %s\n", errstr);
		exit (-1);
	}

	printf("%d slots, %d rules\n", rule_slots, rule_count);

	for (i = 0; i <= rule_slots; i++) {
		error = bsde_get_rule(i, &rule, BUFSIZ, errstr);
		switch (error) {
		case -2:
			continue;
		case -1:
			fprintf(stderr, "rule %d: %s\n", i, errstr);
			continue;
		case 0:
			break;
		}

		if (bsde_rule_to_string(&rule, charstr, BUFSIZ) == -1)
			fprintf(stderr,
			    "Unable to translate rule %d to string\n", i);
		else
			printf("%d %s\n", i, charstr);
	}
}

void
set_rule(int argc, char *argv[])
{
	char errstr[BUFSIZ];
	struct mac_bsdextended_rule rule;
	long value;
	int error, rulenum;
	char *endp;

	if (argc < 1)
		usage();

	value = strtol(argv[0], &endp, 10);
	if (*endp != '\0')
		usage();

	if ((long) value != (int) value || value < 0)
		usage();

	rulenum = value;

	error = bsde_parse_rule(argc - 1, argv + 1, &rule, BUFSIZ, errstr);
	if (error) {
		fprintf(stderr, "%s\n", errstr);
		return;
	}

	error = bsde_set_rule(rulenum, &rule, BUFSIZ, errstr);
	if (error) {
		fprintf(stderr, "%s\n", errstr);
		return;
	}
}

void
remove_rule(int argc, char *argv[])
{
	char errstr[BUFSIZ];
	long value;
	int error, rulenum;
	char *endp;

	if (argc != 1)
		usage();

	value = strtol(argv[0], &endp, 10);
	if (*endp != '\0')  
		usage();

	if ((long) value != (int) value || value < 0)
		usage();

	rulenum = value;

	error = bsde_delete_rule(rulenum, BUFSIZ, errstr);
	if (error)
		fprintf(stderr, "%s\n", errstr);
}

int
main(int argc, char *argv[])
{

	if (argc < 2)
		usage();

	if (strcmp("list", argv[1]) == 0) {
		if (argc != 2)
			usage();
		list_rules();
	} else if (strcmp("set", argv[1]) == 0) {
		set_rule(argc-2, argv+2);
	} else if (strcmp("remove", argv[1]) == 0) {
		remove_rule(argc-2, argv+2);
	} else
		usage();

	return (0);
}
