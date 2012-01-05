/*-
 * Copyright (c) 2003-2008 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

static int
touch(const char *fn, int fail)
{
	FILE *f = fopen(fn, "w");
	if (fail) {
		failure("Couldn't create file '%s', errno=%d (%s)\n",
		    fn, errno, strerror(errno));
		if (!assert(f != NULL))
			return (0); /* Failure. */
	} else {
		if (f == NULL)
			return (0); /* Soft failure. */
	}
	fclose(f);
	return (1); /* Success */
}

DEFINE_TEST(test_option_T_upper)
{
	FILE *f;
	int r;
	struct stat st;
	int gnarlyFilesSupported;

	/* Create a simple dir heirarchy; bail if anything fails. */
	if (!assertMakeDir("d1", 0755)) return;
	if (!assertMakeDir("d1/d2", 0755))	return;
	if (!touch("f", 1)) return;
	if (!touch("d1/f1", 1)) return;
	if (!touch("d1/f2", 1)) return;
	if (!touch("d1/d2/f3", 1)) return;
	if (!touch("d1/d2/f4", 1)) return;
	if (!touch("d1/d2/f5", 1)) return;
	if (!touch("d1/d2/f6", 1)) return;
	/* Some platforms don't permit such things; just skip it. */
	gnarlyFilesSupported = touch("d1/d2/f\x0a", 0);

	/* Populate a file list */
	f = fopen("filelist", "w+");
	if (!assert(f != NULL))
		return;
	/* Use a variety of text line endings. */
	fprintf(f, "f\x0d"); /* CR */
	fprintf(f, "d1/f1\x0d\x0a"); /* CRLF */
	fprintf(f, "d1/d2/f4\x0a"); /* NL */
	fprintf(f, "d1/d2/f6"); /* EOF */
	fclose(f);

	/* Populate a second file list */
	f = fopen("filelist2", "w+");
	if (!assert(f != NULL))
		return;
	/* Use null-terminated names. */
	fprintf(f, "d1/d2/f3");
	fwrite("\0", 1, 1, f);
	fprintf(f, "d1/d2/f5");
	fwrite("\0", 1, 1, f);
	if (gnarlyFilesSupported) {
		fprintf(f, "d1/d2/f\x0a");
		fwrite("\0", 1, 1, f);
	}
	fclose(f);

	/* Use -c -T to archive up the files. */
	r = systemf("%s -c -f test1.tar -T filelist > test1.out 2> test1.err",
	    testprog);
	assert(r == 0);
	assertEmptyFile("test1.out");
	assertEmptyFile("test1.err");

	/* Use -x -T to dearchive the files */
	if (!assertMakeDir("test1", 0755)) return;
	systemf("%s -x -f test1.tar -T filelist -C test1"
	    " > test1b.out 2> test1b.err", testprog);
	assertEmptyFile("test1b.out");
	assertEmptyFile("test1b.err");

	/* Verify the files were extracted. */
	assertFileExists("test1/f");
	assertFileExists("test1/d1/f1");
	assertFileNotExists("test1/d1/f2");
	assertFileNotExists("test1/d1/d2/f3");
	assertFileExists("test1/d1/d2/f4");
	assertFileNotExists("test1/d1/d2/f5");
	assertFileExists("test1/d1/d2/f6");
	if (gnarlyFilesSupported) {
		assertFileNotExists("test1/d1/d2/f\x0a");
	}

	/* Use -r -T to add more files to the archive. */
	systemf("%s -r -f test1.tar --null -T filelist2 > test2.out 2> test2.err",
	    testprog);
	assertEmptyFile("test2.out");
	assertEmptyFile("test2.err");

	/* Use -x without -T to dearchive the files (ensure -r worked) */
	if (!assertMakeDir("test3", 0755)) return;
	systemf("%s -x -f test1.tar -C test3"
	    " > test3.out 2> test3.err", testprog);
	assertEmptyFile("test3.out");
	assertEmptyFile("test3.err");
	/* Verify the files were extracted.*/
	assertFileExists("test3/f");
	assertFileExists("test3/d1/f1");
	assertFileNotExists("test3/d1/f2");
	assertFileExists("test3/d1/d2/f3");
	assertFileExists("test3/d1/d2/f4");
	assertFileExists("test3/d1/d2/f5");
	assertFileExists("test3/d1/d2/f6");
	if (gnarlyFilesSupported) {
		assertFileExists("test3/d1/d2/f\x0a");
	}

	/* Use -x -T to dearchive the files (verify -x -T together) */
	if (!assertMakeDir("test2", 0755)) return;
	systemf("%s -x -f test1.tar -T filelist -C test2"
	    " > test2b.out 2> test2b.err", testprog);
	assertEmptyFile("test2b.out");
	assertEmptyFile("test2b.err");
	/* Verify the files were extracted.*/
	assertFileExists("test2/f");
	assertFileExists("test2/d1/f1");
	assertFileNotExists("test2/d1/f2");
	assertFileNotExists("test2/d1/d2/f3");
	assertFileExists("test2/d1/d2/f4");
	assertFileNotExists("test2/d1/d2/f5");
	assertFileExists("test2/d1/d2/f6");
	if (gnarlyFilesSupported) {
		assertFileNotExists("test2/d1/d2/f\x0a");
	}

	assertMakeDir("test4", 0755);
	assertMakeDir("test4_out", 0755);
	assertMakeDir("test4_out2", 0755);
	assertMakeDir("test4/d1", 0755);
	assertEqualInt(1, touch("test4/d1/foo", 0));

	/* Does bsdtar support -s option ? */
	systemf("%s -cf - -s /foo/bar/ test4/d1/foo > check.out 2> check.err",
	    testprog);
	assertEqualInt(0, stat("check.err", &st));
	if (st.st_size == 0) {
		systemf("%s -cf - -s /foo/bar/ test4/d1/foo | %s -xf - -C test4_out",
		    testprog, testprog);
		assertEmptyFile("test4_out/test4/d1/bar");
		systemf("%s -cf - -s /d1/d2/ test4/d1/foo | %s -xf - -C test4_out",
		    testprog, testprog);
		assertEmptyFile("test4_out/test4/d2/foo");
		systemf("%s -cf - -s ,test4/d1/foo,, test4/d1/foo | %s -tvf - > test4.lst",
		    testprog, testprog);
		assertEmptyFile("test4.lst");
		systemf("%s -cf - test4/d1/foo | %s -xf - -s /foo/bar/ -C test4_out2",
		    testprog, testprog);
		assertEmptyFile("test4_out2/test4/d1/bar");
	} else {
		skipping("bsdtar does not support -s option on this platform");
	}

	/* TODO: Include some use of -C directory-changing within the filelist. */
	/* I'm pretty sure -C within the filelist is broken on extract. */
}
