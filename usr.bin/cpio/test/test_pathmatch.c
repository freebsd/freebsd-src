/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#include "../pathmatch.h"

/*
 * Verify that the pattern matcher implements the wildcard logic specified
 * in SUSv2 for the cpio command.  This is essentially the
 * shell glob syntax:
 *   * - matches any sequence of chars, including '/'
 *   ? - matches any single char, including '/'
 *   [...] - matches any of a set of chars, '-' specifies a range,
 *        initial '!' is undefined
 *
 * The specification in SUSv2 is a bit incomplete, I assume the following:
 *   Trailing '-' in [...] is not special.
 */

DEFINE_TEST(test_pathmatch)
{
	assertEqualInt(1, pathmatch("*","", 0));
	assertEqualInt(1, pathmatch("*","a", 0));
	assertEqualInt(1, pathmatch("*","abcd", 0));
	/* SUSv2: * matches / */
	assertEqualInt(1, pathmatch("*","abcd/efgh/ijkl", 0));
	assertEqualInt(1, pathmatch("abcd*efgh/ijkl","abcd/efgh/ijkl", 0));
	assertEqualInt(1, pathmatch("abcd***efgh/ijkl","abcd/efgh/ijkl", 0));
	assertEqualInt(1, pathmatch("abcd***/efgh/ijkl","abcd/efgh/ijkl", 0));
	assertEqualInt(0, pathmatch("?", "", 0));
	assertEqualInt(0, pathmatch("?", "\0", 0));
	assertEqualInt(1, pathmatch("?", "a", 0));
	assertEqualInt(0, pathmatch("?", "ab", 0));
	assertEqualInt(1, pathmatch("?", ".", 0));
	assertEqualInt(1, pathmatch("?", "?", 0));
	assertEqualInt(1, pathmatch("a", "a", 0));
	assertEqualInt(0, pathmatch("a", "ab", 0));
	assertEqualInt(0, pathmatch("a", "ab", 0));
	assertEqualInt(1, pathmatch("a?c", "abc", 0));
	/* SUSv2: ? matches / */
	assertEqualInt(1, pathmatch("a?c", "a/c", 0));
	assertEqualInt(1, pathmatch("a?*c*", "a/c", 0));
	assertEqualInt(1, pathmatch("*a*", "a/c", 0));
	assertEqualInt(1, pathmatch("*a*", "/a/c", 0));
	assertEqualInt(1, pathmatch("*a*", "defaaaaaaa", 0));
	assertEqualInt(0, pathmatch("a*", "defghi", 0));
	assertEqualInt(0, pathmatch("*a*", "defghi", 0));
	assertEqualInt(1, pathmatch("abc[def", "abc[def", 0));
	assertEqualInt(0, pathmatch("abc[def]", "abc[def", 0));
	assertEqualInt(0, pathmatch("abc[def", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[def]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[def]", "abce", 0));
	assertEqualInt(1, pathmatch("abc[def]", "abcf", 0));
	assertEqualInt(0, pathmatch("abc[def]", "abcg", 0));
	assertEqualInt(1, pathmatch("abc[d*f]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[d*f]", "abc*", 0));
	assertEqualInt(0, pathmatch("abc[d*f]", "abcdefghi", 0));
	assertEqualInt(0, pathmatch("abc[d*", "abcdefghi", 0));
	assertEqualInt(1, pathmatch("abc[d*", "abc[defghi", 0));
	assertEqualInt(1, pathmatch("abc[d-f]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[d-f]", "abce", 0));
	assertEqualInt(1, pathmatch("abc[d-f]", "abcf", 0));
	assertEqualInt(0, pathmatch("abc[d-f]", "abcg", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abce", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abcf", 0));
	assertEqualInt(0, pathmatch("abc[d-fh-k]", "abcg", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abch", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abci", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abcj", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-k]", "abck", 0));
	assertEqualInt(0, pathmatch("abc[d-fh-k]", "abcl", 0));
	assertEqualInt(0, pathmatch("abc[d-fh-k]", "abc-", 0));

	/* I assume: Trailing '-' is non-special. */
	assertEqualInt(0, pathmatch("abc[d-fh-]", "abcl", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-]", "abch", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-]", "abc-", 0));
	assertEqualInt(1, pathmatch("abc[d-fh-]", "abc-", 0));

	/* ']' can be backslash-quoted within a character class. */
	assertEqualInt(1, pathmatch("abc[\\]]", "abc]", 0));
	assertEqualInt(1, pathmatch("abc[\\]d]", "abc]", 0));
	assertEqualInt(1, pathmatch("abc[\\]d]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[d\\]]", "abc]", 0));
	assertEqualInt(1, pathmatch("abc[d\\]]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[d]e]", "abcde]", 0));
	assertEqualInt(1, pathmatch("abc[d\\]e]", "abc]", 0));
	assertEqualInt(0, pathmatch("abc[d\\]e]", "abcd]e", 0));
	assertEqualInt(0, pathmatch("abc[d]e]", "abc]", 0));

	/* backslash-quoted chars can appear as either end of a range. */
	assertEqualInt(1, pathmatch("abc[\\d-f]gh", "abcegh", 0));
	assertEqualInt(0, pathmatch("abc[\\d-f]gh", "abcggh", 0));
	assertEqualInt(0, pathmatch("abc[\\d-f]gh", "abc\\gh", 0));
	assertEqualInt(1, pathmatch("abc[d-\\f]gh", "abcegh", 0));
	assertEqualInt(1, pathmatch("abc[\\d-\\f]gh", "abcegh", 0));
	assertEqualInt(1, pathmatch("abc[\\d-\\f]gh", "abcegh", 0));
	/* backslash-quoted '-' isn't special. */
	assertEqualInt(0, pathmatch("abc[d\\-f]gh", "abcegh", 0));
	assertEqualInt(1, pathmatch("abc[d\\-f]gh", "abc-gh", 0));

	/* Leading '!' negates a character class. */
	assertEqualInt(0, pathmatch("abc[!d]", "abcd", 0));
	assertEqualInt(1, pathmatch("abc[!d]", "abce", 0));
	assertEqualInt(1, pathmatch("abc[!d]", "abcc", 0));
	assertEqualInt(0, pathmatch("abc[!d-z]", "abcq", 0));
	assertEqualInt(1, pathmatch("abc[!d-gi-z]", "abch", 0));
	assertEqualInt(1, pathmatch("abc[!fgijkl]", "abch", 0));
	assertEqualInt(0, pathmatch("abc[!fghijkl]", "abch", 0));

	/* Backslash quotes next character. */
	assertEqualInt(0, pathmatch("abc\\[def]", "abc\\d", 0));
	assertEqualInt(1, pathmatch("abc\\[def]", "abc[def]", 0));
	assertEqualInt(0, pathmatch("abc\\\\[def]", "abc[def]", 0));
	assertEqualInt(0, pathmatch("abc\\\\[def]", "abc\\[def]", 0));
	assertEqualInt(1, pathmatch("abc\\\\[def]", "abc\\d", 0));

	/*
	 * Because '.' and '/' have special meanings, we can
	 * identify many equivalent paths even if they're expressed
	 * differently.
	 */
	assertEqualInt(1, pathmatch("./abc/./def/", "abc/def/", 0));
	assertEqualInt(1, pathmatch("abc/def", "./././abc/./def", 0));
	assertEqualInt(1, pathmatch("abc/def/././//", "./././abc/./def/", 0));
	assertEqualInt(1, pathmatch(".////abc/.//def", "./././abc/./def", 0));
	assertEqualInt(1, pathmatch("./abc?def/", "abc/def/", 0));
	failure("\"?./\" is not the same as \"/./\"");
	assertEqualInt(0, pathmatch("./abc?./def/", "abc/def/", 0));
	failure("Trailing '/' should match no trailing '/'");
	assertEqualInt(1, pathmatch("./abc/./def/", "abc/def", 0));
	failure("Trailing '/./' is still the same directory.");
	assertEqualInt(1, pathmatch("./abc/./def/./", "abc/def", 0));
	failure("Trailing '/.' is still the same directory.");
	assertEqualInt(1, pathmatch("./abc/./def/.", "abc/def", 0));
	assertEqualInt(1, pathmatch("./abc/./def", "abc/def/", 0));
	failure("Trailing '/./' is still the same directory.");
	assertEqualInt(1, pathmatch("./abc/./def", "abc/def/./", 0));
	failure("Trailing '/.' is still the same directory.");
	assertEqualInt(1, pathmatch("./abc*/./def", "abc/def/.", 0));
}
