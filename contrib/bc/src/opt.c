/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Adapted from https://github.com/skeeto/optparse
 *
 * *****************************************************************************
 *
 * Code for getopt_long() replacement. It turns out that getopt_long() has
 * different behavior on different platforms.
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <status.h>
#include <opt.h>
#include <vm.h>

static inline bool bc_opt_longoptsEnd(const BcOptLong *longopts, size_t i) {
	return !longopts[i].name && !longopts[i].val;
}

static const char* bc_opt_longopt(const BcOptLong *longopts, int c) {

	size_t i;

	for (i = 0; !bc_opt_longoptsEnd(longopts, i); ++i) {
		if (longopts[i].val == c) return longopts[i].name;
	}

	return "NULL";
}

static void bc_opt_error(BcErr err, int c, const char *str) {
	if (err == BC_ERR_FATAL_OPTION) bc_vm_error(err, 0, str);
	else bc_vm_error(err, 0, (int) c, str);
}

static int bc_opt_type(const BcOptLong *longopts, char c) {

	size_t i;

	if (c == ':') return -1;

	for (i = 0; !bc_opt_longoptsEnd(longopts, i) && longopts[i].val != c; ++i);

	if (bc_opt_longoptsEnd(longopts, i)) return -1;

	return (int) longopts[i].type;
}

static int bc_opt_parseShort(BcOpt *o, const BcOptLong *longopts) {

	int type;
	char *next;
	char *option = o->argv[o->optind];
	int ret = -1;

	o->optopt = 0;
	o->optarg = NULL;

	option += o->subopt + 1;
	o->optopt = option[0];

	type = bc_opt_type(longopts, option[0]);
	next = o->argv[o->optind + 1];

	switch (type) {

		case -1:
		case BC_OPT_BC_ONLY:
		case BC_OPT_DC_ONLY:
		{
			if (type == -1 || (type == BC_OPT_BC_ONLY && BC_IS_DC) ||
			    (type == BC_OPT_DC_ONLY && BC_IS_BC))
			{
				char str[2] = {0, 0};

				str[0] = option[0];
				o->optind += 1;

				bc_opt_error(BC_ERR_FATAL_OPTION, option[0], str);
			}
		}
		// Fallthrough.
		BC_FALLTHROUGH

		case BC_OPT_NONE:
		{
			if (option[1]) o->subopt += 1;
			else {
				o->subopt = 0;
				o->optind += 1;
			}

			ret = (int) option[0];
			break;
		}

		case BC_OPT_REQUIRED:
		{
			o->subopt = 0;
			o->optind += 1;

			if (option[1]) o->optarg = option + 1;
			else if (next != NULL) {
				o->optarg = next;
				o->optind += 1;
			}
			else bc_opt_error(BC_ERR_FATAL_OPTION_NO_ARG, option[0],
			                  bc_opt_longopt(longopts, option[0]));


			ret = (int) option[0];
			break;
		}
	}

	return ret;
}

static bool bc_opt_longoptsMatch(const char *name, const char *option) {

	const char *a = option, *n = name;

	if (name == NULL) return false;

	for (; *a && *n && *a != '='; ++a, ++n) {
		if (*a != *n) return false;
	}

	return (*n == '\0' && (*a == '\0' || *a == '='));
}

static char* bc_opt_longoptsArg(char *option) {

	for (; *option && *option != '='; ++option);

	if (*option == '=') return option + 1;
	else return NULL;
}

int bc_opt_parse(BcOpt *o, const BcOptLong *longopts) {

	size_t i;
	char *option;
	bool empty;

	do {

		option = o->argv[o->optind];
		if (option == NULL) return -1;

		empty = !strcmp(option, "");
		o->optind += empty;

	} while (empty);

	if (BC_OPT_ISDASHDASH(option)) {

		// Consume "--".
		o->optind += 1;
		return -1;
	}
	else if (BC_OPT_ISSHORTOPT(option)) return bc_opt_parseShort(o, longopts);
	else if (!BC_OPT_ISLONGOPT(option)) return -1;

	o->optopt = 0;
	o->optarg = NULL;

	// Skip "--" at beginning of the option.
	option += 2;
	o->optind += 1;

	for (i = 0; !bc_opt_longoptsEnd(longopts, i); i++) {

		const char *name = longopts[i].name;

		if (bc_opt_longoptsMatch(name, option)) {

			char *arg;

			o->optopt = longopts[i].val;
			arg = bc_opt_longoptsArg(option);

			if ((longopts[i].type == BC_OPT_BC_ONLY && BC_IS_DC) ||
			    (longopts[i].type == BC_OPT_DC_ONLY && BC_IS_BC))
			{
				bc_opt_error(BC_ERR_FATAL_OPTION, o->optopt, name);
			}

			if (longopts[i].type == BC_OPT_NONE && arg != NULL)
			{
				bc_opt_error(BC_ERR_FATAL_OPTION_ARG, o->optopt, name);
			}

			if (arg != NULL) o->optarg = arg;
			else if (longopts[i].type == BC_OPT_REQUIRED) {

				o->optarg = o->argv[o->optind];

				if (o->optarg != NULL) o->optind += 1;
				else bc_opt_error(BC_ERR_FATAL_OPTION_NO_ARG,
				                  o->optopt, name);
			}

			return o->optopt;
		}
	}

	bc_opt_error(BC_ERR_FATAL_OPTION, 0, option);

	return -1;
}

void bc_opt_init(BcOpt *o, char *argv[]) {
	o->argv = argv;
	o->optind = 1;
	o->subopt = 0;
	o->optarg = NULL;
}
