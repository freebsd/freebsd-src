/*
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Provides a _start routine that calls a _start_c routine that takes a pointer
 * to the stack as documented in crt1.c. We skip the pointer to _DYNAMIC since
 * we don't support dynamic libraries, at all. And while _start_c is our own
 * thing, we comport to the calling conventions that glibc and musl have and
 * make sure the second argument (%esi) is 0 for _DYNAMIC placeholder.  We
 * likely could call main directly with only a few more lines of code, but this
 * is simple enough and concentrates all the expressable in C stuff there.  We
 * also generate eh_frames should we need to debug things (it doesn't change the
 * genreated code, but leaves enough breadcrumbs to keep gdb happy).
 */

__asm__(
".text\n"			/* ENTRY(_start) */
".p2align 4,0x90\n"
".global _start\n"
".type _start, @function\n"
"_start:\n"
".cfi_startproc\n"
"	xor	%rbp, %rbp\n"		/* Clear out the stack frame pointer */
"	mov	%rsp, %rdi\n"		/* Pass pointer to current stack with argc, argv and envp on it */
"	xor	%rsi, %rsi\n"		/* No dynamic pointer for us, to keep it simple */
"	andq	$-16, %rsp\n"		/* Align stack to 16-byte boundary */
"	call	_start_c\n"		/* Our MI code takes it from here and won't return */
/* NORETURN */
".size _start, . - _start\n"	/* END(_start) */
".cfi_endproc"
);
