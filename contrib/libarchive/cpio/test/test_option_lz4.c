/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_lz4)
{
	char *p;
	int r;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with lz4 compression. */
	r = systemf("echo f | %s -o --lz4 >archive.out 2>archive.err",
	    testprog);
	p = slurpfile(&s, "archive.err");
	p[s] = '\0';
	if (r != 0) {
		if (strstr(p, "compression not available") != NULL) {
			skipping("This version of bsdcpio was compiled "
			    "without lz4 support");
			free(p);
			return;
		}
		/* POSIX permits different handling of the spawnp
		 * system call used to launch the subsidiary
		 * program: */
		/* Some systems fail immediately to spawn the new process. */
		if (strstr(p, "Can't launch") != NULL && !canLz4()) {
			skipping("This version of bsdcpio uses an external lz4 program "
			    "but no such program is available on this system.");
			free(p);
			return;
		}
		/* Some systems successfully spawn the new process,
		 * but fail to exec a program within that process.
		 * This results in failure at the first attempt to
		 * write. */
		if (strstr(p, "Can't write") != NULL && !canLz4()) {
			skipping("This version of bsdcpio uses an external lz4 program "
			    "but no such program is available on this system.");
			free(p);
			return;
		}
		/* On some systems the error won't be detected until closing
		   time, by a 127 exit error returned by waitpid. */
		if (strstr(p, "Error closing") != NULL && !canLz4()) {
			skipping("This version of bsdcpio uses an external lz4 program "
			    "but no such program is available on this system.");
			free(p);
			return;
		}
		failure("--lz4 option is broken: %s", p);
		free(p);
		assertEqualInt(r, 0);
		return;
	}
	free(p);
	/* Check that the archive file has an lz4 signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "\x04\x22\x4d\x18", 4);
	free(p);
}
