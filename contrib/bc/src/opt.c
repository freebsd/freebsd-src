/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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

/**
 * Returns true if index @a i is the end of the longopts array.
 * @param longopts  The long options array.
 * @param i         The index to test.
 * @return          True if @a i is the last index, false otherwise.
 */
static inline bool
bc_opt_longoptsEnd(const BcOptLong* longopts, size_t i)
{
	return !longopts[i].name && !longopts[i].val;
}

/**
 * Returns the name of the long option that matches the character @a c.
 * @param longopts  The long options array.
 * @param c         The character to match against.
 * @return          The name of the long option that matches @a c, or "NULL".
 */
static const char*
bc_opt_longopt(const BcOptLong* longopts, int c)
{
	size_t i;

	for (i = 0; !bc_opt_longoptsEnd(longopts, i); ++i)
	{
		if (longopts[i].val == c) return longopts[i].name;
	}

	BC_UNREACHABLE

#if !BC_CLANG
	return "NULL";
#endif // !BC_CLANG
}

/**
 * Issues a fatal error for an option parsing failure.
 * @param err        The error.
 * @param c          The character for the failing option.
 * @param str        Either the string for the failing option, or the invalid
 *                   option.
 * @param use_short  True if the short option should be used for error printing,
 *                   false otherwise.
 */
static void
bc_opt_error(BcErr err, int c, const char* str, bool use_short)
{
	if (err == BC_ERR_FATAL_OPTION)
	{
		if (use_short)
		{
			char short_str[2];

			short_str[0] = (char) c;
			short_str[1] = '\0';

			bc_error(err, 0, short_str);
		}
		else bc_error(err, 0, str);
	}
	else bc_error(err, 0, (int) c, str);
}

/**
 * Returns the type of the long option that matches @a c.
 * @param longopts  The long options array.
 * @param c         The character to match against.
 * @return          The type of the long option as an integer, or -1 if none.
 */
static int
bc_opt_type(const BcOptLong* longopts, char c)
{
	size_t i;

	if (c == ':') return -1;

	for (i = 0; !bc_opt_longoptsEnd(longopts, i) && longopts[i].val != c; ++i)
	{
		continue;
	}

	if (bc_opt_longoptsEnd(longopts, i)) return -1;

	return (int) longopts[i].type;
}

/**
 * Parses a short option.
 * @param o         The option parser.
 * @param longopts  The long options array.
 * @return          The character for the short option, or -1 if none left.
 */
static int
bc_opt_parseShort(BcOpt* o, const BcOptLong* longopts)
{
	int type;
	char* next;
	char* option = o->argv[o->optind];
	int ret = -1;

	// Make sure to clear these.
	o->optopt = 0;
	o->optarg = NULL;

	// Get the next option.
	option += o->subopt + 1;
	o->optopt = option[0];

	// Get the type and the next data.
	type = bc_opt_type(longopts, option[0]);
	next = o->argv[o->optind + 1];

	switch (type)
	{
		case -1:
		case BC_OPT_BC_ONLY:
		case BC_OPT_DC_ONLY:
		{
			// Check for invalid option and barf if so.
			if (type == -1 || (type == BC_OPT_BC_ONLY && BC_IS_DC) ||
			    (type == BC_OPT_DC_ONLY && BC_IS_BC))
			{
				char str[2] = { 0, 0 };

				str[0] = option[0];
				o->optind += 1;

				bc_opt_error(BC_ERR_FATAL_OPTION, option[0], str, true);
			}

			// Fallthrough.
			BC_FALLTHROUGH
		}

		case BC_OPT_NONE:
		{
			// If there is something else, update the suboption.
			if (option[1]) o->subopt += 1;
			else
			{
				// Go to the next argument.
				o->subopt = 0;
				o->optind += 1;
			}

			ret = (int) option[0];

			break;
		}

		case BC_OPT_REQUIRED_BC_ONLY:
		{
#if DC_ENABLED
			if (BC_IS_DC)
			{
				bc_opt_error(BC_ERR_FATAL_OPTION, option[0],
				             bc_opt_longopt(longopts, option[0]), true);
			}
#endif // DC_ENABLED

			// Fallthrough
			BC_FALLTHROUGH
		}

		case BC_OPT_REQUIRED:
		{
			// Always go to the next argument.
			o->subopt = 0;
			o->optind += 1;

			// Use the next characters, if they exist.
			if (option[1]) o->optarg = option + 1;
			else if (next != NULL)
			{
				// USe the next.
				o->optarg = next;
				o->optind += 1;
			}
			// No argument, barf.
			else
			{
				bc_opt_error(BC_ERR_FATAL_OPTION_NO_ARG, option[0],
				             bc_opt_longopt(longopts, option[0]), true);
			}

			ret = (int) option[0];

			break;
		}
	}

	return ret;
}

/**
 * Ensures that a long option argument matches a long option name, regardless of
 * "=<data>" at the end.
 * @param name    The name to match.
 * @param option  The command-line argument.
 * @return        True if @a option matches @a name, false otherwise.
 */
static bool
bc_opt_longoptsMatch(const char* name, const char* option)
{
	const char* a = option;
	const char* n = name;

	// Can never match a NULL name.
	if (name == NULL) return false;

	// Loop through.
	for (; *a && *n && *a != '='; ++a, ++n)
	{
		if (*a != *n) return false;
	}

	// Ensure they both end at the same place.
	return (*n == '\0' && (*a == '\0' || *a == '='));
}

/**
 * Returns a pointer to the argument of a long option, or NULL if it not in the
 * same argument.
 * @param option  The option to find the argument of.
 * @return        A pointer to the argument of the option, or NULL if none.
 */
static char*
bc_opt_longoptsArg(char* option)
{
	// Find the end or equals sign.
	for (; *option && *option != '='; ++option)
	{
		continue;
	}

	if (*option == '=') return option + 1;
	else return NULL;
}

int
bc_opt_parse(BcOpt* o, const BcOptLong* longopts)
{
	size_t i;
	char* option;
	bool empty;

	// This just eats empty options.
	do
	{
		option = o->argv[o->optind];
		if (option == NULL) return -1;

		empty = !strcmp(option, "");
		o->optind += empty;
	}
	while (empty);

	// If the option is just a "--".
	if (BC_OPT_ISDASHDASH(option))
	{
		// Consume "--".
		o->optind += 1;
		return -1;
	}
	// Parse a short option.
	else if (BC_OPT_ISSHORTOPT(option)) return bc_opt_parseShort(o, longopts);
	// If the option is not long at this point, we are done.
	else if (!BC_OPT_ISLONGOPT(option)) return -1;

	// Clear these.
	o->optopt = 0;
	o->optarg = NULL;

	// Skip "--" at beginning of the option.
	option += 2;
	o->optind += 1;

	// Loop through the valid long options.
	for (i = 0; !bc_opt_longoptsEnd(longopts, i); i++)
	{
		const char* name = longopts[i].name;

		// If we have a match...
		if (bc_opt_longoptsMatch(name, option))
		{
			char* arg;

			// Get the option char and the argument.
			o->optopt = longopts[i].val;
			arg = bc_opt_longoptsArg(option);

			// Error if the option is invalid..
			if ((longopts[i].type == BC_OPT_BC_ONLY && BC_IS_DC) ||
			    (longopts[i].type == BC_OPT_REQUIRED_BC_ONLY && BC_IS_DC) ||
			    (longopts[i].type == BC_OPT_DC_ONLY && BC_IS_BC))
			{
				bc_opt_error(BC_ERR_FATAL_OPTION, o->optopt, name, false);
			}

			// Error if we have an argument and should not.
			if (longopts[i].type == BC_OPT_NONE && arg != NULL)
			{
				bc_opt_error(BC_ERR_FATAL_OPTION_ARG, o->optopt, name, false);
			}

			// Set the argument, or check the next argument if we don't have
			// one.
			if (arg != NULL) o->optarg = arg;
			else if (longopts[i].type == BC_OPT_REQUIRED ||
			         longopts[i].type == BC_OPT_REQUIRED_BC_ONLY)
			{
				// Get the next argument.
				o->optarg = o->argv[o->optind];

				// All's good if it exists; otherwise, barf.
				if (o->optarg != NULL) o->optind += 1;
				else
				{
					bc_opt_error(BC_ERR_FATAL_OPTION_NO_ARG, o->optopt, name,
					             false);
				}
			}

			return o->optopt;
		}
	}

	// If we reach this point, the option is invalid.
	bc_opt_error(BC_ERR_FATAL_OPTION, 0, option, false);

	BC_UNREACHABLE

#if !BC_CLANG
	return -1;
#endif // !BC_CLANG
}

void
bc_opt_init(BcOpt* o, char* argv[])
{
	o->argv = argv;
	o->optind = 1;
	o->subopt = 0;
	o->optarg = NULL;
}
