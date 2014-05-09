/*-
 * Copyright (c) 2010 Jilles Tjoelker
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>

struct testcase {
	const char *pattern;
	const char *string;
	int flags;
	int result;
} testcases[] = {
	"", "", 0, 0,
	"a", "a", 0, 0,
	"a", "b", 0, FNM_NOMATCH,
	"a", "A", 0, FNM_NOMATCH,
	"*", "a", 0, 0,
	"*", "aa", 0, 0,
	"*a", "a", 0, 0,
	"*a", "b", 0, FNM_NOMATCH,
	"*a*", "b", 0, FNM_NOMATCH,
	"*a*b*", "ab", 0, 0,
	"*a*b*", "qaqbq", 0, 0,
	"*a*bb*", "qaqbqbbq", 0, 0,
	"*a*bc*", "qaqbqbcq", 0, 0,
	"*a*bb*", "qaqbqbb", 0, 0,
	"*a*bc*", "qaqbqbc", 0, 0,
	"*a*bb", "qaqbqbb", 0, 0,
	"*a*bc", "qaqbqbc", 0, 0,
	"*a*bb", "qaqbqbbq", 0, FNM_NOMATCH,
	"*a*bc", "qaqbqbcq", 0, FNM_NOMATCH,
	"*a*a*a*a*a*a*a*a*a*a*", "aaaaaaaaa", 0, FNM_NOMATCH,
	"*a*a*a*a*a*a*a*a*a*a*", "aaaaaaaaaa", 0, 0,
	"*a*a*a*a*a*a*a*a*a*a*", "aaaaaaaaaaa", 0, 0,
	".*.*.*.*.*.*.*.*.*.*", ".........", 0, FNM_NOMATCH,
	".*.*.*.*.*.*.*.*.*.*", "..........", 0, 0,
	".*.*.*.*.*.*.*.*.*.*", "...........", 0, 0,
	"*?*?*?*?*?*?*?*?*?*?*", "123456789", 0, FNM_NOMATCH,
	"??????????*", "123456789", 0, FNM_NOMATCH,
	"*??????????", "123456789", 0, FNM_NOMATCH,
	"*?*?*?*?*?*?*?*?*?*?*", "1234567890", 0, 0,
	"??????????*", "1234567890", 0, 0,
	"*??????????", "1234567890", 0, 0,
	"*?*?*?*?*?*?*?*?*?*?*", "12345678901", 0, 0,
	"??????????*", "12345678901", 0, 0,
	"*??????????", "12345678901", 0, 0,
	"[x]", "x", 0, 0,
	"[*]", "*", 0, 0,
	"[?]", "?", 0, 0,
	"[", "[", 0, 0,
	"[[]", "[", 0, 0,
	"[[]", "x", 0, FNM_NOMATCH,
	"[*]", "", 0, FNM_NOMATCH,
	"[*]", "x", 0, FNM_NOMATCH,
	"[?]", "x", 0, FNM_NOMATCH,
	"*[*]*", "foo*foo", 0, 0,
	"*[*]*", "foo", 0, FNM_NOMATCH,
	"[0-9]", "0", 0, 0,
	"[0-9]", "5", 0, 0,
	"[0-9]", "9", 0, 0,
	"[0-9]", "/", 0, FNM_NOMATCH,
	"[0-9]", ":", 0, FNM_NOMATCH,
	"[0-9]", "*", 0, FNM_NOMATCH,
	"[!0-9]", "0", 0, FNM_NOMATCH,
	"[!0-9]", "5", 0, FNM_NOMATCH,
	"[!0-9]", "9", 0, FNM_NOMATCH,
	"[!0-9]", "/", 0, 0,
	"[!0-9]", ":", 0, 0,
	"[!0-9]", "*", 0, 0,
	"*[0-9]", "a0", 0, 0,
	"*[0-9]", "a5", 0, 0,
	"*[0-9]", "a9", 0, 0,
	"*[0-9]", "a/", 0, FNM_NOMATCH,
	"*[0-9]", "a:", 0, FNM_NOMATCH,
	"*[0-9]", "a*", 0, FNM_NOMATCH,
	"*[!0-9]", "a0", 0, FNM_NOMATCH,
	"*[!0-9]", "a5", 0, FNM_NOMATCH,
	"*[!0-9]", "a9", 0, FNM_NOMATCH,
	"*[!0-9]", "a/", 0, 0,
	"*[!0-9]", "a:", 0, 0,
	"*[!0-9]", "a*", 0, 0,
	"*[0-9]", "a00", 0, 0,
	"*[0-9]", "a55", 0, 0,
	"*[0-9]", "a99", 0, 0,
	"*[0-9]", "a0a0", 0, 0,
	"*[0-9]", "a5a5", 0, 0,
	"*[0-9]", "a9a9", 0, 0,
	"\\*", "*", 0, 0,
	"\\?", "?", 0, 0,
	"\\[x]", "[x]", 0, 0,
	"\\[", "[", 0, 0,
	"\\\\", "\\", 0, 0,
	"*\\**", "foo*foo", 0, 0,
	"*\\**", "foo", 0, FNM_NOMATCH,
	"*\\\\*", "foo\\foo", 0, 0,
	"*\\\\*", "foo", 0, FNM_NOMATCH,
	"\\(", "(", 0, 0,
	"\\a", "a", 0, 0,
	"\\*", "a", 0, FNM_NOMATCH,
	"\\?", "a", 0, FNM_NOMATCH,
	"\\*", "\\*", 0, FNM_NOMATCH,
	"\\?", "\\?", 0, FNM_NOMATCH,
	"\\[x]", "\\[x]", 0, FNM_NOMATCH,
	"\\[x]", "\\x", 0, FNM_NOMATCH,
	"\\[", "\\[", 0, FNM_NOMATCH,
	"\\(", "\\(", 0, FNM_NOMATCH,
	"\\a", "\\a", 0, FNM_NOMATCH,
	"\\", "\\", 0, FNM_NOMATCH,
	"\\", "", 0, 0,
	"\\*", "\\*", FNM_NOESCAPE, 0,
	"\\?", "\\?", FNM_NOESCAPE, 0,
	"\\", "\\", FNM_NOESCAPE, 0,
	"\\\\", "\\", FNM_NOESCAPE, FNM_NOMATCH,
	"\\\\", "\\\\", FNM_NOESCAPE, 0,
	"*\\*", "foo\\foo", FNM_NOESCAPE, 0,
	"*\\*", "foo", FNM_NOESCAPE, FNM_NOMATCH,
	"*", ".", FNM_PERIOD, FNM_NOMATCH,
	"?", ".", FNM_PERIOD, FNM_NOMATCH,
	".*", ".", 0, 0,
	".*", "..", 0, 0,
	".*", ".a", 0, 0,
	"[0-9]", ".", FNM_PERIOD, FNM_NOMATCH,
	"a*", "a.", 0, 0,
	"a/a", "a/a", FNM_PATHNAME, 0,
	"a/*", "a/a", FNM_PATHNAME, 0,
	"*/a", "a/a", FNM_PATHNAME, 0,
	"*/*", "a/a", FNM_PATHNAME, 0,
	"a*b/*", "abbb/x", FNM_PATHNAME, 0,
	"a*b/*", "abbb/.x", FNM_PATHNAME, 0,
	"*", "a/a", FNM_PATHNAME, FNM_NOMATCH,
	"*/*", "a/a/a", FNM_PATHNAME, FNM_NOMATCH,
	"b/*", "b/.x", FNM_PATHNAME | FNM_PERIOD, FNM_NOMATCH,
	"b*/*", "a/.x", FNM_PATHNAME | FNM_PERIOD, FNM_NOMATCH,
	"b/.*", "b/.x", FNM_PATHNAME | FNM_PERIOD, 0,
	"b*/.*", "b/.x", FNM_PATHNAME | FNM_PERIOD, 0,
	"a", "A", FNM_CASEFOLD, 0,
	"A", "a", FNM_CASEFOLD, 0,
	"[a]", "A", FNM_CASEFOLD, 0,
	"[A]", "a", FNM_CASEFOLD, 0,
	"a", "b", FNM_CASEFOLD, FNM_NOMATCH,
	"a", "a/b", FNM_PATHNAME, FNM_NOMATCH,
	"*", "a/b", FNM_PATHNAME, FNM_NOMATCH,
	"*b", "a/b", FNM_PATHNAME, FNM_NOMATCH,
	"a", "a/b", FNM_PATHNAME | FNM_LEADING_DIR, 0,
	"*", "a/b", FNM_PATHNAME | FNM_LEADING_DIR, 0,
	"*", ".a/b", FNM_PATHNAME | FNM_LEADING_DIR, 0,
	"*a", ".a/b", FNM_PATHNAME | FNM_LEADING_DIR, 0,
	"*", ".a/b", FNM_PATHNAME | FNM_PERIOD | FNM_LEADING_DIR, FNM_NOMATCH,
	"*a", ".a/b", FNM_PATHNAME | FNM_PERIOD | FNM_LEADING_DIR, FNM_NOMATCH,
	"a*b/*", "abbb/.x", FNM_PATHNAME | FNM_PERIOD, FNM_NOMATCH,
};

static const char *
flags_to_string(int flags)
{
	static const int flagvalues[] = { FNM_NOESCAPE, FNM_PATHNAME,
		FNM_PERIOD, FNM_LEADING_DIR, FNM_CASEFOLD, 0 };
	static const char flagnames[] = "FNM_NOESCAPE\0FNM_PATHNAME\0FNM_PERIOD\0FNM_LEADING_DIR\0FNM_CASEFOLD\0";
	static char result[sizeof(flagnames) + 3 * sizeof(int) + 2];
	char *p;
	size_t i, len;
	const char *fp;

	p = result;
	fp = flagnames;
	for (i = 0; flagvalues[i] != 0; i++) {
		len = strlen(fp);
		if (flags & flagvalues[i]) {
			if (p != result)
				*p++ = '|';
			memcpy(p, fp, len);
			p += len;
			flags &= ~flagvalues[i];
		}
		fp += len + 1;
	}
	if (p == result)
		memcpy(p, "0", 2);
	else if (flags != 0)
		sprintf(p, "%d", flags);
	else
		*p = '\0';
	return result;
}

static int
write_sh_tests(const char *progname, int num)
{
	size_t i, n;
	struct testcase *t;

	printf("# Generated by %s -s %d, do not edit.\n", progname, num);
	printf("# $" "FreeBSD$\n");
	printf("failures=\n");
	printf("failed() { printf '%%s\\n' \"Failed: $1 '$2' '$3'\"; failures=x$failures; }\n");
	if (num == 1) {
		printf("testmatch() { eval \"case \\$2 in ''$1) ;; *) failed testmatch \\\"\\$@\\\";; esac\"; }\n");
		printf("testnomatch() { eval \"case \\$2 in ''$1) failed testnomatch \\\"\\$@\\\";; esac\"; }\n");
	} else if (num == 2) {
		printf("# We do not treat a backslash specially in this case,\n");
		printf("# but this is not the case in all shells.\n");
		printf("netestmatch() { case $2 in $1) ;; *) failed netestmatch \"$@\";; esac; }\n");
		printf("netestnomatch() { case $2 in $1) failed netestnomatch \"$@\";; esac; }\n");
	}
	n = sizeof(testcases) / sizeof(testcases[0]);
	for (i = 0; i < n; i++) {
		t = &testcases[i];
		if (strchr(t->pattern, '\'') != NULL ||
		    strchr(t->string, '\'') != NULL)
			continue;
		if (t->flags == 0 && strcmp(t->pattern, "\\") == 0)
			continue;
		if (num == 1 && t->flags == 0)
			printf("test%smatch '%s' '%s'\n",
			    t->result == FNM_NOMATCH ? "no" : "",
			    t->pattern, t->string);
		if (num == 2 && (t->flags == FNM_NOESCAPE ||
		    (t->flags == 0 && strchr(t->pattern, '\\') == NULL)))
			printf("netest%smatch '%s' '%s'\n",
			    t->result == FNM_NOMATCH ? "no" : "",
			    t->pattern, t->string);
	}
	printf("[ -z \"$failures\" ]\n");
	return 0;
}

int
main(int argc, char *argv[])
{
	size_t i, n;
	int opt, flags, result, extra, errors;
	struct testcase *t;

	while ((opt = getopt(argc, argv, "s:")) != -1) {
		switch (opt) {
			case 's':
				return (write_sh_tests(argv[0], atoi(optarg)));
			default:
				fprintf(stderr, "usage: %s [-s num]\n", argv[0]);
				fprintf(stderr, "-s option writes tests for sh(1), num is 1 or 2\n");
				exit(1);
		}
	}
	n = sizeof(testcases) / sizeof(testcases[0]);
	errors = 0;
	printf("1..%zu\n", n);
	for (i = 0; i < n; i++) {
		t = &testcases[i];
		flags = t->flags;
		extra = 0;
		do {
			result = fnmatch(t->pattern, t->string, flags);
			if (result != t->result)
				break;
			if (strchr(t->pattern, '\\') == NULL &&
			    !(flags & FNM_NOESCAPE)) {
				flags |= FNM_NOESCAPE;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if (strchr(t->pattern, '\\') != NULL &&
			    strchr(t->string, '\\') == NULL &&
			    t->result == FNM_NOMATCH &&
			    !(flags & (FNM_NOESCAPE | FNM_LEADING_DIR))) {
				flags |= FNM_NOESCAPE;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if ((t->string[0] != '.' || t->pattern[0] == '.' ||
			    t->result == FNM_NOMATCH) &&
			    !(flags & (FNM_PATHNAME | FNM_PERIOD))) {
				flags |= FNM_PERIOD;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if ((strchr(t->string, '/') == NULL ||
			    t->result == FNM_NOMATCH) &&
			    !(flags & FNM_PATHNAME)) {
				flags |= FNM_PATHNAME;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if ((((t->string[0] != '.' || t->pattern[0] == '.') &&
			    strstr(t->string, "/.") == NULL) ||
			    t->result == FNM_NOMATCH) &&
			    flags & FNM_PATHNAME && !(flags & FNM_PERIOD)) {
				flags |= FNM_PERIOD;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if ((((t->string[0] != '.' || t->pattern[0] == '.') &&
			    strchr(t->string, '/') == NULL) ||
			    t->result == FNM_NOMATCH) &&
			    !(flags & (FNM_PATHNAME | FNM_PERIOD))) {
				flags |= FNM_PATHNAME | FNM_PERIOD;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if ((strchr(t->string, '/') == NULL || t->result == 0)
			    && !(flags & FNM_LEADING_DIR)) {
				flags |= FNM_LEADING_DIR;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if (t->result == 0 && !(flags & FNM_CASEFOLD)) {
				flags |= FNM_CASEFOLD;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
			if (strchr(t->pattern, '\\') == NULL &&
			    t->result == 0 &&
			    !(flags & (FNM_NOESCAPE | FNM_CASEFOLD))) {
				flags |= FNM_NOESCAPE | FNM_CASEFOLD;
				result = fnmatch(t->pattern, t->string, flags);
				if (result != t->result)
					break;
				flags = t->flags;
				extra++;
			}
		} while (0);
		if (result == t->result)
			printf("ok %zu - fnmatch(\"%s\", \"%s\", %s) = %d (+%d)\n",
			    i + 1, t->pattern, t->string,
			    flags_to_string(flags),
			    result, extra);
		else {
			printf("not ok %zu - fnmatch(\"%s\", \"%s\", %s) = %d != %d\n",
			    i + 1, t->pattern, t->string,
			    flags_to_string(flags),
			    result, t->result);
			errors = 1;
		}
	}

	return (errors);
}
