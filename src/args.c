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
 * Code for processing command-line arguments.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif // _WIN32

#include <vector.h>
#include <read.h>
#include <args.h>
#include <opt.h>
#include <num.h>

/**
 * Adds @a str to the list of expressions to execute later.
 * @param str  The string to add to the list of expressions.
 */
static void
bc_args_exprs(const char* str)
{
	BC_SIG_ASSERT_LOCKED;
	if (vm.exprs.v == NULL) bc_vec_init(&vm.exprs, sizeof(uchar), BC_DTOR_NONE);
	bc_vec_concat(&vm.exprs, str);
	bc_vec_concat(&vm.exprs, "\n");
}

/**
 * Adds the contents of @a file to the list of expressions to execute later.
 * @param file  The name of the file whose contents should be added to the list
 *              of expressions to execute.
 */
static void
bc_args_file(const char* file)
{
	char* buf;

	BC_SIG_ASSERT_LOCKED;

	vm.file = file;

	buf = bc_read_file(file);

	assert(buf != NULL);

	bc_args_exprs(buf);
	free(buf);
}

static BcBigDig
bc_args_builtin(const char* arg)
{
	bool strvalid;
	BcNum n;
	BcBigDig res;

	strvalid = bc_num_strValid(arg);

	if (BC_ERR(!strvalid))
	{
		bc_verr(BC_ERR_FATAL_ARG, arg);
	}

	bc_num_init(&n, 0);

	bc_num_parse(&n, arg, 10);

	res = bc_num_bigdig(&n);

	bc_num_free(&n);

	return res;
}

#if BC_ENABLED

/**
 * Redefines a keyword, if it exists and is not a POSIX keyword. Otherwise, it
 * throws a fatal error.
 * @param keyword  The keyword to redefine.
 */
static void
bc_args_redefine(const char* keyword)
{
	size_t i;

	BC_SIG_ASSERT_LOCKED;

	for (i = 0; i < bc_lex_kws_len; ++i)
	{
		const BcLexKeyword* kw = bc_lex_kws + i;

		if (!strcmp(keyword, kw->name))
		{
			if (BC_LEX_KW_POSIX(kw)) break;

			vm.redefined_kws[i] = true;

			return;
		}
	}

	bc_error(BC_ERR_FATAL_ARG, 0, keyword);
}

#endif // BC_ENABLED

void
bc_args(int argc, char* argv[], bool exit_exprs, BcBigDig scale)
{
	int c;
	size_t i;
	bool do_exit = false, version = false;
	BcOpt opts;
	BcBigDig newscale = scale, ibase = BC_BASE, obase = BC_BASE;
#if BC_ENABLE_EXTRA_MATH
	char* seed = NULL;
#endif // BC_ENABLE_EXTRA_MATH

	BC_SIG_ASSERT_LOCKED;

	bc_opt_init(&opts, argv);

	// This loop should look familiar to anyone who has used getopt() or
	// getopt_long() in C.
	while ((c = bc_opt_parse(&opts, bc_args_lopt)) != -1)
	{
		switch (c)
		{
			case 'e':
			{
				// Barf if not allowed.
				if (vm.no_exprs)
				{
					bc_verr(BC_ERR_FATAL_OPTION, "-e (--expression)");
				}

				// Add the expressions and set exit.
				bc_args_exprs(opts.optarg);
				vm.exit_exprs = (exit_exprs || vm.exit_exprs);

				break;
			}

			case 'f':
			{
				// Figure out if exiting on expressions is disabled.
				if (!strcmp(opts.optarg, "-")) vm.no_exprs = true;
				else
				{
					// Barf if not allowed.
					if (vm.no_exprs)
					{
						bc_verr(BC_ERR_FATAL_OPTION, "-f (--file)");
					}

					// Add the expressions and set exit.
					bc_args_file(opts.optarg);
					vm.exit_exprs = (exit_exprs || vm.exit_exprs);
				}

				break;
			}

			case 'h':
			{
				bc_vm_info(vm.help);
				do_exit = true;
				break;
			}

			case 'i':
			{
				vm.flags |= BC_FLAG_I;
				break;
			}

			case 'I':
			{
				ibase = bc_args_builtin(opts.optarg);
				break;
			}

			case 'z':
			{
				vm.flags |= BC_FLAG_Z;
				break;
			}

			case 'L':
			{
				vm.line_len = 0;
				break;
			}

			case 'O':
			{
				obase = bc_args_builtin(opts.optarg);
				break;
			}

			case 'P':
			{
				vm.flags &= ~(BC_FLAG_P);
				break;
			}

			case 'R':
			{
				vm.flags &= ~(BC_FLAG_R);
				break;
			}

			case 'S':
			{
				newscale = bc_args_builtin(opts.optarg);
				break;
			}

#if BC_ENABLE_EXTRA_MATH
			case 'E':
			{
				if (BC_ERR(!bc_num_strValid(opts.optarg)))
				{
					bc_verr(BC_ERR_FATAL_ARG, opts.optarg);
				}

				seed = opts.optarg;

				break;
			}
#endif // BC_ENABLE_EXTRA_MATH

#if BC_ENABLED
			case 'g':
			{
				assert(BC_IS_BC);
				vm.flags |= BC_FLAG_G;
				break;
			}

			case 'l':
			{
				assert(BC_IS_BC);
				vm.flags |= BC_FLAG_L;
				break;
			}

			case 'q':
			{
				assert(BC_IS_BC);
				vm.flags &= ~(BC_FLAG_Q);
				break;
			}

			case 'r':
			{
				bc_args_redefine(opts.optarg);
				break;
			}

			case 's':
			{
				assert(BC_IS_BC);
				vm.flags |= BC_FLAG_S;
				break;
			}

			case 'w':
			{
				assert(BC_IS_BC);
				vm.flags |= BC_FLAG_W;
				break;
			}
#endif // BC_ENABLED

			case 'V':
			case 'v':
			{
				do_exit = version = true;
				break;
			}

#if DC_ENABLED
			case 'x':
			{
				assert(BC_IS_DC);
				vm.flags |= DC_FLAG_X;
				break;
			}
#endif // DC_ENABLED

#ifndef NDEBUG
			// We shouldn't get here because bc_opt_error()/bc_error() should
			// longjmp() out.
			case '?':
			case ':':
			default:
			{
				BC_UNREACHABLE
				abort();
			}
#endif // NDEBUG
		}
	}

	if (version) bc_vm_info(NULL);
	if (do_exit)
	{
		vm.status = (sig_atomic_t) BC_STATUS_QUIT;
		BC_JMP;
	}

	// We do not print the banner if expressions are used or dc is used.
	if (!BC_IS_BC || vm.exprs.len > 1) vm.flags &= ~(BC_FLAG_Q);

	// We need to make sure the files list is initialized. We don't want to
	// initialize it if there are no files because it's just a waste of memory.
	if (opts.optind < (size_t) argc && vm.files.v == NULL)
	{
		bc_vec_init(&vm.files, sizeof(char*), BC_DTOR_NONE);
	}

	// Add all the files to the vector.
	for (i = opts.optind; i < (size_t) argc; ++i)
	{
		bc_vec_push(&vm.files, argv + i);
	}

#if BC_ENABLE_EXTRA_MATH
	if (seed != NULL)
	{
		BcNum n;

		bc_num_init(&n, strlen(seed));

		BC_SIG_UNLOCK;

		bc_num_parse(&n, seed, BC_BASE);

		bc_program_assignSeed(&vm.prog, &n);

		BC_SIG_LOCK;

		bc_num_free(&n);
	}
#endif // BC_ENABLE_EXTRA_MATH

	BC_SIG_UNLOCK;

	if (newscale != scale)
	{
		bc_program_assignBuiltin(&vm.prog, true, false, newscale);
	}

	if (obase != BC_BASE)
	{
		bc_program_assignBuiltin(&vm.prog, false, true, obase);
	}

	// This is last to avoid it affecting the value of the others.
	if (ibase != BC_BASE)
	{
		bc_program_assignBuiltin(&vm.prog, false, false, ibase);
	}

	BC_SIG_LOCK;
}
