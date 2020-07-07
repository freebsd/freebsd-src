/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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

#include <unistd.h>

#include <status.h>
#include <vector.h>
#include <read.h>
#include <vm.h>
#include <args.h>
#include <opt.h>

static const BcOptLong bc_args_lopt[] = {

	{ "expression", BC_OPT_REQUIRED, 'e' },
	{ "file", BC_OPT_REQUIRED, 'f' },
	{ "help", BC_OPT_NONE, 'h' },
	{ "interactive", BC_OPT_NONE, 'i' },
	{ "no-prompt", BC_OPT_NONE, 'P' },
#if BC_ENABLED
	{ "global-stacks", BC_OPT_BC_ONLY, 'g' },
	{ "mathlib", BC_OPT_BC_ONLY, 'l' },
	{ "quiet", BC_OPT_BC_ONLY, 'q' },
	{ "standard", BC_OPT_BC_ONLY, 's' },
	{ "warn", BC_OPT_BC_ONLY, 'w' },
#endif // BC_ENABLED
	{ "version", BC_OPT_NONE, 'v' },
	{ "version", BC_OPT_NONE, 'V' },
#if DC_ENABLED
	{ "extended-register", BC_OPT_DC_ONLY, 'x' },
#endif // DC_ENABLED
	{ NULL, 0, 0 },

};

static void bc_args_exprs(const char *str) {
	BC_SIG_ASSERT_LOCKED;
	if (vm.exprs.v == NULL) bc_vec_init(&vm.exprs, sizeof(uchar), NULL);
	bc_vec_concat(&vm.exprs, str);
	bc_vec_concat(&vm.exprs, "\n");
}

static void bc_args_file(const char *file) {

	char *buf;

	BC_SIG_ASSERT_LOCKED;

	vm.file = file;

	bc_read_file(file, &buf);
	bc_args_exprs(buf);
	free(buf);
}

void bc_args(int argc, char *argv[]) {

	int c;
	size_t i;
	bool do_exit = false, version = false;
	BcOpt opts;

	BC_SIG_ASSERT_LOCKED;

	bc_opt_init(&opts, argv);

	while ((c = bc_opt_parse(&opts, bc_args_lopt)) != -1) {

		switch (c) {

			case 'e':
			{
				bc_args_exprs(opts.optarg);
				break;
			}

			case 'f':
			{
				bc_args_file(opts.optarg);
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

			case 'P':
			{
				vm.flags |= BC_FLAG_P;
				break;
			}

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
				vm.flags |= BC_FLAG_Q;
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
			// We shouldn't get here because bc_opt_error()/bc_vm_error() should
			// longjmp() out.
			case '?':
			case ':':
			default:
			{
				abort();
			}
#endif // NDEBUG
		}
	}

	if (version) bc_vm_info(NULL);
	if (do_exit) exit((int) vm.status);
	if (vm.exprs.len > 1 || BC_IS_DC) vm.flags |= BC_FLAG_Q;

	if (opts.optind < (size_t) argc)
		bc_vec_init(&vm.files, sizeof(char*), NULL);

	for (i = opts.optind; i < (size_t) argc; ++i)
		bc_vec_push(&vm.files, argv + i);
}
