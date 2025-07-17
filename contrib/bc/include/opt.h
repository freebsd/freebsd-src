/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2024 Gavin D. Howard and contributors.
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
 * Definitions for getopt_long() replacement.
 *
 */

#ifndef BC_OPT_H
#define BC_OPT_H

#include <stdbool.h>
#include <stdlib.h>

/// The data required to parse command-line arguments.
typedef struct BcOpt
{
	/// The array of arguments.
	const char** argv;

	/// The index of the current argument.
	size_t optind;

	/// The actual parse option character.
	int optopt;

	/// Where in the option we are for multi-character single-character options.
	int subopt;

	/// The option argument.
	const char* optarg;

} BcOpt;

/// The types of arguments. This is specially adapted for bc.
typedef enum BcOptType
{
	/// No argument required.
	BC_OPT_NONE,

	/// An argument required.
	BC_OPT_REQUIRED,

	/// An option that is bc-only.
	BC_OPT_BC_ONLY,

	/// An option that is bc-only that requires an argument.
	BC_OPT_REQUIRED_BC_ONLY,

	/// An option that is dc-only.
	BC_OPT_DC_ONLY,

} BcOptType;

/// A struct to hold const data for long options.
typedef struct BcOptLong
{
	/// The name of the option.
	const char* name;

	/// The type of the option.
	BcOptType type;

	/// The character to return if the long option was parsed.
	int val;

} BcOptLong;

/**
 * Initialize data for parsing options.
 * @param o     The option data to initialize.
 * @param argv  The array of arguments.
 */
void
bc_opt_init(BcOpt* o, const char** argv);

/**
 * Parse an option. This returns a value the same way getopt() and getopt_long()
 * do, so it returns a character for the parsed option or -1 if done.
 * @param o         The option data.
 * @param longopts  The long options.
 * @return          A character for the parsed option, or -1 if done.
 */
int
bc_opt_parse(BcOpt* o, const BcOptLong* longopts);

/**
 * Returns true if the option is `--` and not a long option.
 * @param a  The argument to parse.
 * @return   True if @a a is the `--` option, false otherwise.
 */
#define BC_OPT_ISDASHDASH(a) \
	((a) != NULL && (a)[0] == '-' && (a)[1] == '-' && (a)[2] == '\0')

/**
 * Returns true if the option is a short option.
 * @param a  The argument to parse.
 * @return   True if @a a is a short option, false otherwise.
 */
#define BC_OPT_ISSHORTOPT(a) \
	((a) != NULL && (a)[0] == '-' && (a)[1] != '-' && (a)[1] != '\0')

/**
 * Returns true if the option has `--` at the beginning, i.e., is a long option.
 * @param a  The argument to parse.
 * @return   True if @a a is a long option, false otherwise.
 */
#define BC_OPT_ISLONGOPT(a) \
	((a) != NULL && (a)[0] == '-' && (a)[1] == '-' && (a)[2] != '\0')

#endif // BC_OPT_H
