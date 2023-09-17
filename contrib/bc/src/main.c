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
 * The entry point for bc.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if BC_ENABLE_NLS
#include <locale.h>
#endif // BC_ENABLE_NLS

#ifndef _WIN32
#include <libgen.h>
#endif // _WIN32

#include <setjmp.h>

#include <version.h>
#include <status.h>
#include <vm.h>
#include <bc.h>
#include <dc.h>

int
main(int argc, char* argv[])
{
	char* name;
	size_t len = strlen(BC_EXECPREFIX);

#if BC_ENABLE_NLS
	// Must set the locale properly in order to have the right error messages.
	vm->locale = setlocale(LC_ALL, "");
#endif // BC_ENABLE_NLS

	// Set the start pledge().
	bc_pledge(bc_pledge_start, NULL);

	// Sometimes, argv[0] can be NULL. Better make sure to be robust against it.
	if (argv[0] != NULL)
	{
		// Figure out the name of the calculator we are using. We can't use
		// basename because it's not portable, but yes, this is stripping off
		// the directory.
		name = strrchr(argv[0], BC_FILE_SEP);
		vm->name = (name == NULL) ? argv[0] : name + 1;
	}
	else
	{
#if !DC_ENABLED
		vm->name = "bc";
#elif !BC_ENABLED
		vm->name = "dc";
#else
		// Just default to bc in that case.
		vm->name = "bc";
#endif
	}

	// If the name is longer than the length of the prefix, skip the prefix.
	if (strlen(vm->name) > len) vm->name += len;

	BC_SIG_LOCK;

	// We *must* do this here. Otherwise, other code could not jump out all of
	// the way.
	bc_vec_init(&vm->jmp_bufs, sizeof(sigjmp_buf), BC_DTOR_NONE);

	BC_SETJMP_LOCKED(vm, exit);

#if !DC_ENABLED
	bc_main(argc, argv);
#elif !BC_ENABLED
	dc_main(argc, argv);
#else
	// BC_IS_BC uses vm->name, which was set above. So we're good.
	if (BC_IS_BC) bc_main(argc, argv);
	else dc_main(argc, argv);
#endif

exit:
	BC_SIG_MAYLOCK;

	// Ensure we exit appropriately.
	return bc_vm_atexit((int) vm->status);
}
