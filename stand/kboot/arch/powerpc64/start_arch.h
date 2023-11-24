/*
 * Copyright (c) 2022 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Due to the PowerPC ABI, We can call main directly from here, so do so.
 *
 * Note: there may be some static initializers that aren't called, but we don't
 * worry about that elsewhere. This is a stripped down environment.
 *
 * I think we could also do something like
 *
 * mflr		r0
 * stw		r0,4(r1)
 * stwu		r1,-16(r1)
 * b		_start_c
 *
 * But my powerpc assembler fu is quite lacking...
 */

#define __unused __attribute__((__unused__))

void
_start(int argc, const char **argv, char **env, void *obj __unused,
    void (*cleanup)(void) __unused)
{
	main(argc, argv, env);
}
