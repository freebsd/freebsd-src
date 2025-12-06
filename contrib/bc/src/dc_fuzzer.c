/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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
 * The entry point for libFuzzer when fuzzing dc.
 *
 */

#include <setjmp.h>
#include <string.h>

#include <status.h>
#include <ossfuzz.h>
#include <vm.h>
#include <bc.h>
#include <dc.h>

uint8_t* bc_fuzzer_data;

/// A boolean about whether we should use -c (false) or -C (true).
static bool dc_C;

int
LLVMFuzzerInitialize(int* argc, char*** argv)
{
	BC_UNUSED(argc);

	if (argv == NULL || *argv == NULL)
	{
		dc_C = false;
	}
	else
	{
		char* name;

		// Get the basename
		name = strrchr((*argv)[0], BC_FILE_SEP);
		name = name == NULL ? (*argv)[0] : name + 1;

		// Figure out which to use.
		dc_C = (strcmp(name, "dc_fuzzer_C") == 0);
	}

	return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	BcStatus s;

	// I've already tested empty input, so just ignore.
	if (Size == 0 || Data[0] == '\0') return 0;

	// Clear the global. This is to ensure a clean start.
	memset(vm, 0, sizeof(BcVm));

	// Make sure to set the name.
	vm->name = "dc";

	BC_SIG_LOCK;

	// We *must* do this here. Otherwise, other code could not jump out all of
	// the way.
	bc_vec_init(&vm->jmp_bufs, sizeof(sigjmp_buf), BC_DTOR_NONE);

	BC_SETJMP_LOCKED(vm, exit);

	// Create a string with the data.
	bc_fuzzer_data = bc_vm_malloc(Size + 1);
	memcpy(bc_fuzzer_data, Data, Size);
	bc_fuzzer_data[Size] = '\0';

	s = dc_main((int) (bc_fuzzer_args_len - 1),
	            dc_C ? dc_fuzzer_args_C : dc_fuzzer_args_c);

exit:

	BC_SIG_MAYLOCK;

	free(bc_fuzzer_data);

	return s == BC_STATUS_SUCCESS || s == BC_STATUS_QUIT ? 0 : -1;
}
