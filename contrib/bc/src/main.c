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
 * The entry point for bc.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>
#include <libgen.h>

#include <setjmp.h>

#include <status.h>
#include <vm.h>
#include <bc.h>
#include <dc.h>

char output_bufs[BC_VM_BUF_SIZE];
BcVm vm;

int main(int argc, char *argv[]) {

	int s;
	char *name;
	size_t len = strlen(BC_EXECPREFIX);

	vm.locale = setlocale(LC_ALL, "");

	name = strrchr(argv[0], '/');
	vm.name = (name == NULL) ? argv[0] : name + 1;

	if (strlen(vm.name) > len) vm.name += len;

	BC_SIG_LOCK;

	bc_vec_init(&vm.jmp_bufs, sizeof(sigjmp_buf), NULL);

	BC_SETJMP_LOCKED(exit);

#if !DC_ENABLED
	bc_main(argc, argv);
#elif !BC_ENABLED
	dc_main(argc, argv);
#else
	if (BC_IS_BC) bc_main(argc, argv);
	else dc_main(argc, argv);
#endif

exit:
	BC_SIG_MAYLOCK;

	s = !BC_STATUS_IS_ERROR(vm.status) ? BC_STATUS_SUCCESS : (int) vm.status;

	bc_vm_shutdown();

#ifndef NDEBUG
	bc_vec_free(&vm.jmp_bufs);
#endif // NDEBUG

	return s;
}
