/*
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Provides a _start routine that calls a _start_c routine that takes a pointer
 * to the stack as documented in crt1.c. We skip the pointer to _DYNAMIC since
 * we don't support dynamic libraries, at all. And while _start_c is our own
 * thing and doesn't have a second arg, we comport to the calling conventions
 * that glibc and musl have by passing x1 as 0 for the dynamic pointer. We
 * likely could call main directly with only a few more lines of code, but this
 * is simple enough and concentrates all the expressable in C stuff there.  We
 * also generate eh_frames should we need to debug things (it doesn't change the
 * genreated code, but leaves enough breadcrumbs to keep gdb happy)
 */

__asm__(
".text\n"		/* ENTRY(_start) -- can't expand and stringify, so by hand */
".align 2\n"
".global _start\n"
".type _start, #function\n"
"_start:\n"
".cfi_startproc\n"
/*
 * Linux zeros all registers so x29 (frame pointer) and x30 (link register) are 0.
 */
"	mov	x0, sp\n"	/* Pointer to argc, etc kernel left on the stack */
"	and	sp, x0, #-16\n"	/* Align stack to 16-byte boundary */
"	b	_start_c\n"	/* Our MI code takes it from here */
/* NORETURN */
".ltorg\n"		/* END(_start) */
".cfi_endproc\n"
".size _start, .-_start\n"
);
