/*
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MI part of the C startup code. We take a long * pointer (we assume long is
 * the same size as a pointer, as the Linux world is wont to do). We get a
 * pointer to the stack with the main args on it. We don't bother decoding the
 * aux vector, but may need to do so in the future.
 *
 * The long *p points to:
 *
 * +--------------------+
 * | argc               | Small address
 * +--------------------+
 * | argv[0]            | argv
 * +--------------------+
 * | argv[1]            |
 * +--------------------+
 *  ...
 * +--------------------+
 * | NULL               | &argv[argc]
 * +--------------------+
 * | envp[0]            | envp
 * +--------------------+
 * | envp[1]            |
 * +--------------------+
 *  ...
 * +--------------------+
 * | NULL               |
 * +--------------------+
 * | aux type           | AT_xxxx
 * +--------------------+
 * | aux value          |
 * +--------------------+
 * | aux type           | AT_xxxx
 * +--------------------+
 * | aux value          |
 * +--------------------+
 * | aux type           | AT_xxxx
 * +--------------------+
 * | aux value          |
 * +--------------------+
 *...
 * +--------------------+
 * | NULL               |
 * +--------------------+
 *
 * The AUX vector contains additional information for the process to know from
 * the kernel (not parsed currently). AT_xxxx constants are small (< 50).
 */

extern void _start_c(long *);
extern int main(int, const char **, char **);

#include "start_arch.h"

void
_start_c(long *p)
{
	int argc;
	const char **argv;
	char **envp;

	argc = p[0];
	argv = (const char **)(p + 1);
	envp = (char **)argv + argc + 1;

	/* Note: we don't ensure that fd 0, 1, and 2 are sane at this level */
	/* Also note: we expect main to exit, not return off the end */
	main(argc, argv, envp);
}
