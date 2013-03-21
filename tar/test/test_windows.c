/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
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

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <direct.h>
#include <windows.h>

static void
mkfile(const char *name)
{
	FILE *f;

	f = fopen(name, "wb");
	assert(f != NULL);
	assertEqualInt(5, fwrite("01234", 1, 5, f));
	fclose(f);
}

static void
mkfullpath(char **path1, char **path2, const char *tpath, int type)
{
	char *fp1 = NULL, *fp2 = NULL, *p1 = NULL, *p2 = NULL;
	size_t l;

	/*
	 * Get full path name of "tpath"
	 */
	l = GetFullPathNameA(tpath, 0, NULL, NULL);
	assert(0 != l);
	fp1 = malloc(l);
	assert(NULL != fp1);
	fp2 = malloc(l*2);
	assert(NULL != fp2);
	l = GetFullPathNameA(tpath, (DWORD)l, fp1, NULL);
	if ((type & 0x01) == 0) {
		for (p1 = fp1; *p1 != '\0'; p1++)
			if (*p1 == '\\')
				*p1 = '/';
	}

	switch(type) {
	case 0: /* start with "/" */
	case 1: /* start with "\" */
		/* strip "c:" */
		memmove(fp1, fp1 + 2, l - 2);
		fp1[l -2] = '\0';
		p1 = fp1 + 1;
		break;
	case 2: /* start with "c:/" */
	case 3: /* start with "c:\" */
		p1 = fp1 + 3;
		break;
	case 4: /* start with "//./c:/" */
	case 5: /* start with "\\.\c:\" */
	case 6: /* start with "//?/c:/" */
	case 7: /* start with "\\?\c:\" */
		p1 = malloc(l + 4 + 1);
		assert(NULL != p1);
		if (type & 0x1)
			memcpy(p1, "\\\\.\\", 4);
		else
			memcpy(p1, "//./", 4);
		if (type == 6 || type == 7)
			p1[2] = '?';
		memcpy(p1 + 4, fp1, l);
		p1[l + 4] = '\0';
		free(fp1);
		fp1 = p1;
		p1 = fp1 + 7;
		break;
	}

	/*
	 * Strip leading drive names and converting "\" to "\\"
	 */
	p2 = fp2;
	while (*p1 != '\0') {
		if (*p1 == '\\')
			*p2 = '/';
		else
			*p2 = *p1;
		++p1;
		++p2;
	}
	*p2++ = '\r';
	*p2++ = '\n';
	*p2 = '\0';

	*path1 = fp1;
	*path2 = fp2;
}

static const char *list1[] = {"aaa/", "aaa/file1", "aaa/xxa/", "aaa/xxb/",
	"aaa/zzc/", "aaa/zzc/file1", "aaa/xxb/file1", "aaa/xxa/file1",
	"aab/", "aac/", "abb/", "abc/", "abd/", NULL};
static const char *list2[] = {"bbb/", "bbb/file1", "bbb/xxa/", "bbb/xxb/",
	"bbb/zzc/", "bbb/zzc/file1", "bbb/xxb/file1", "bbb/xxa/file1", "bbc/",
	"bbd/", "bcc/", "bcd/", "bce/", NULL};
static const char *list3[] = {"aac/", "abc/", "bbc/", "bcc/", "ccc/", NULL};
static const char *list4[] = {"fff/abca", "fff/acca", NULL};
static const char *list5[] = {"aaa/file1", "aaa/xxa/", "aaa/xxa/file1",
	"aaa/xxb/", "aaa/xxb/file1", "aaa/zzc/", "aaa/zzc/file1", NULL};
static const char *list6[] = {"fff/abca", "fff/acca", "aaa/xxa/",
	"aaa/xxa/file1", "aaa/xxb/", "aaa/xxb/file1", NULL};
#endif /* _WIN32 && !__CYGWIN__ */

DEFINE_TEST(test_windows)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	char *fp1, *fp2;

	/*
	 * Preparre tests.
	 * Create directories and files.
	 */
	assertMakeDir("tmp", 0775);
	assertChdir("tmp");

	assertMakeDir("aaa", 0775);
	assertMakeDir("aaa/xxa", 0775);
	assertMakeDir("aaa/xxb", 0775);
	assertMakeDir("aaa/zzc", 0775);
	mkfile("aaa/file1");
	mkfile("aaa/xxa/file1");
	mkfile("aaa/xxb/file1");
	mkfile("aaa/zzc/file1");
	assertMakeDir("aab", 0775);
	assertMakeDir("aac", 0775);
	assertMakeDir("abb", 0775);
	assertMakeDir("abc", 0775);
	assertMakeDir("abd", 0775);
	assertMakeDir("bbb", 0775);
	assertMakeDir("bbb/xxa", 0775);
	assertMakeDir("bbb/xxb", 0775);
	assertMakeDir("bbb/zzc", 0775);
	mkfile("bbb/file1");
	mkfile("bbb/xxa/file1");
	mkfile("bbb/xxb/file1");
	mkfile("bbb/zzc/file1");
	assertMakeDir("bbc", 0775);
	assertMakeDir("bbd", 0775);
	assertMakeDir("bcc", 0775);
	assertMakeDir("bcd", 0775);
	assertEqualInt(0, _mkdir("bce"));
	assertEqualInt(0, _mkdir("ccc"));
	assertEqualInt(0, _mkdir("fff"));
	mkfile("fff/aaaa");
	mkfile("fff/abba");
	mkfile("fff/abca");
	mkfile("fff/acba");
	mkfile("fff/acca");

	/*
	 * Test1: Command line pattern matching.
	 */
	assertEqualInt(0,
	    systemf("%s -cf ../archive1.tar a*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive1.tar > ../list1", testprog));
	assertFileContainsLinesAnyOrder("../list1", list1);

	assertEqualInt(0,
	    systemf("%s -cf ../archive2.tar b*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive2.tar > ../list2", testprog));
	assertFileContainsLinesAnyOrder("../list2", list2);

	assertEqualInt(0,
	    systemf("%s -cf ../archive3.tar ??c", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive3.tar > ../list3", testprog));
	assertFileContainsLinesAnyOrder("../list3", list3);

	assertEqualInt(0,
	    systemf("%s -cf ../archive3b.tar *c", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive3b.tar > ../list3b", testprog));
	assertFileContainsLinesAnyOrder("../list3b", list3);

	assertEqualInt(0,
	    systemf("%s -cf ../archive4.tar fff/a?ca", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive4.tar > ../list4", testprog));
	assertFileContainsLinesAnyOrder("../list4", list4);

	assertEqualInt(0,
	    systemf("%s -cf ../archive5.tar aaa\\*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive5.tar > ../list5", testprog));
	assertFileContainsLinesAnyOrder("../list5", list5);

	assertEqualInt(0,
	    systemf("%s -cf ../archive6.tar fff\\a?ca aaa\\xx*", testprog));
	assertEqualInt(0,
	    systemf("%s -tf ../archive6.tar > ../list6", testprog));
	assertFileContainsLinesAnyOrder("../list6", list6);

	/*
	 * Test2: Archive the file start with drive letters.
	 */
	/* Test2a: start with "/" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 0);
	assertEqualInt(0,
	    systemf("%s -cf ../archive10.tar %s > ../out10 2> ../err10",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive10.tar > ../list10", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list10");
	free(fp1);
	free(fp2);

	/* Test2b: start with "\" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 1);
	assertEqualInt(0,
	    systemf("%s -cf ../archive11.tar %s > ../out11 2> ../err11",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive11.tar > ../list11", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list11");
	free(fp1);
	free(fp2);

	/* Test2c: start with "c:/" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 2);
	assertEqualInt(0,
	    systemf("%s -cf ../archive12.tar %s > ../out12 2> ../err12",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive12.tar > ../list12", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list12");
	free(fp1);
	free(fp2);

	/* Test2d: start with "c:\" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 3);
	assertEqualInt(0,
	    systemf("%s -cf ../archive13.tar %s > ../out13 2> ../err13",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive13.tar > ../list13", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list13");
	free(fp1);
	free(fp2);

	/* Test2e: start with "//./c:/" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 4);
	assertEqualInt(0,
	    systemf("%s -cf ../archive14.tar %s > ../out14 2> ../err14",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive14.tar > ../list14", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list14");
	free(fp1);
	free(fp2);

	/* Test2f: start with "\\.\c:\" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 5);
	assertEqualInt(0,
	    systemf("%s -cf ../archive15.tar %s > ../out15 2> ../err15",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive15.tar > ../list15", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list15");
	free(fp1);
	free(fp2);

	/* Test2g: start with "//?/c:/" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 6);
	failure("fp1=%s, fp2=%s", fp1, fp2);
	assertEqualInt(0,
	    systemf("%s -cf ../archive16.tar %s > ../out16 2> ../err16",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive16.tar > ../list16", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list16");
	free(fp1);
	free(fp2);

	/* Test2h: start with "\\?\c:\" */
	mkfullpath(&fp1, &fp2, "aaa/file1", 7);
	failure("fp1=%s, fp2=%s", fp1, fp2);
	assertEqualInt(0,
	    systemf("%s -cf ../archive17.tar %s > ../out17 2> ../err17",
	        testprog, fp1));
	assertEqualInt(0,
	    systemf("%s -tf ../archive17.tar > ../list17", testprog));
	/* Check drive letters have been stripped. */
	assertFileContents(fp2, (int)strlen(fp2), "../list17");
	free(fp1);
	free(fp2);
#else
	skipping("Windows specific test");
#endif /* _WIN32 && !__CYGWIN__ */
}
