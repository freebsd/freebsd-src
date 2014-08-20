#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libwapcaplet/libwapcaplet.h>

#include "desktop/netsurf.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/nsurl.h"

struct test_pairs {
	const char* test;
	const char* res;
};

struct test_triplets {
	const char* test1;
	const char* test2;
	const char* res;
};

static void netsurf_lwc_iterator(lwc_string *str, void *pw)
{
	LOG(("[%3u] %.*s", str->refcnt, (int) lwc_string_length(str),
			lwc_string_data(str)));
}

static const struct test_pairs create_tests[] = {
	{ "",			NULL },
	{ "http:",		NULL },
	{ "http:/",		NULL },
	{ "http://",		NULL },
	{ "http:a",		"http://a/" },
	{ "http:a/",		"http://a/" },
	{ "http:a/b",		"http://a/b" },
	{ "http:/a",		"http://a/" },
	{ "http:/a/b",		"http://a/b" },
	{ "http://a",		"http://a/" },
	{ "http://a/b",		"http://a/b" },
	{ "www.example.org",	"http://www.example.org/" },
	{ "www.example.org/x",	"http://www.example.org/x" },
	{ "about:",		"about:" },
	{ "about:blank",	"about:blank" },

	{ "http://www.ns-b.org:8080/",
		"http://www.ns-b.org:8080/" },
	{ "http://user@www.ns-b.org:8080/hello", 
		"http://user@www.ns-b.org:8080/hello" },
	{ "http://user:pass@www.ns-b.org:8080/hello", 
		"http://user:pass@www.ns-b.org:8080/hello" },

	{ "http://www.ns-b.org:80/",
		"http://www.ns-b.org/" },
	{ "http://user@www.ns-b.org:80/hello", 
		"http://user@www.ns-b.org/hello" },
	{ "http://user:pass@www.ns-b.org:80/hello", 
		"http://user:pass@www.ns-b.org/hello" },

	{ "http://www.ns-b.org:/",
		"http://www.ns-b.org/" },
	{ "http://u@www.ns-b.org:/hello", 
		"http://u@www.ns-b.org/hello" },
	{ "http://u:p@www.ns-b.org:/hello", 
		"http://u:p@www.ns-b.org/hello" },

	{ "http:a/",		"http://a/" },
	{ "http:/a/",		"http://a/" },
	{ "http://u@a",		"http://u@a/" },
	{ "http://@a",		"http://a/" },

	{ "mailto:u@a",		"mailto:u@a" },
	{ "mailto:@a",		"mailto:a" },

	{ NULL,			NULL }
};

static const struct test_pairs join_tests[] = {
	/* Normal Examples rfc3986 5.4.1 */
	{ "g:h",		"g:h" },
	{ "g",			"http://a/b/c/g" },
	{ "./g",		"http://a/b/c/g" },
	{ "g/",			"http://a/b/c/g/" },
	{ "/g",			"http://a/g" },
	{ "//g",		"http://g" /* [1] */ "/" },
	{ "?y",			"http://a/b/c/d;p?y" },
	{ "g?y",		"http://a/b/c/g?y" },
	{ "#s",			"http://a/b/c/d;p?q#s" },
	{ "g#s",		"http://a/b/c/g#s" },
	{ "g?y#s",		"http://a/b/c/g?y#s" },
	{ ";x",			"http://a/b/c/;x" },
	{ "g;x",		"http://a/b/c/g;x" },
	{ "g;x?y#s",		"http://a/b/c/g;x?y#s" },
	{ "",			"http://a/b/c/d;p?q" },
	{ ".",			"http://a/b/c/" },
	{ "./",			"http://a/b/c/" },
	{ "..",			"http://a/b/" },
	{ "../",		"http://a/b/" },
	{ "../g",		"http://a/b/g" },
	{ "../..",		"http://a/" },
	{ "../../",		"http://a/" },
	{ "../../g",		"http://a/g" },

	/* Abnormal Examples rfc3986 5.4.2 */
	{ "../../../g",		"http://a/g" },
	{ "../../../../g",	"http://a/g" },

	{ "/./g",		"http://a/g" },
	{ "/../g",		"http://a/g" },
	{ "g.",			"http://a/b/c/g." },
	{ ".g",			"http://a/b/c/.g" },
	{ "g..",		"http://a/b/c/g.." },
	{ "..g",		"http://a/b/c/..g" },

	{ "./../g",		"http://a/b/g" },
	{ "./g/.",		"http://a/b/c/g/" },
	{ "g/./h",		"http://a/b/c/g/h" },
	{ "g/../h",		"http://a/b/c/h" },
	{ "g;x=1/./y",		"http://a/b/c/g;x=1/y" },
	{ "g;x=1/../y",		"http://a/b/c/y" },

	{ "g?y/./x",		"http://a/b/c/g?y/./x" },
	{ "g?y/../x",		"http://a/b/c/g?y/../x" },
	{ "g#s/./x",		"http://a/b/c/g#s/./x" },
	{ "g#s/../x",		"http://a/b/c/g#s/../x" },

	{ "http:g",		"http:g" /* [2] */ },

	/* Extra tests */
	{ " g",			"http://a/b/c/g" },
	{ "g ",			"http://a/b/c/g" },
	{ " g ",		"http://a/b/c/g" },
	{ "http:/b/c",		"http://b/c" },
	{ "http://",		"http:" },
	{ "http:/",		"http:" },
	{ "http:",		"http:" },
	{ " ",			"http://a/b/c/d;p?q" },
	{ "  ",			"http://a/b/c/d;p?q" },
	{ "/",			"http://a/" },
	{ "  /  ",		"http://a/" },
	{ "  ?  ",		"http://a/b/c/d;p?" },
	{ "  h  ",		"http://a/b/c/h" },
	/* [1] Extra slash beyond rfc3986 5.4.1 example, since we're
	 *     testing normalisation in addition to joining */
	/* [2] Using the strict parsers option */
	{ NULL,			NULL }
};

static const struct test_triplets replace_query_tests[] = {
	{ "http://netsurf-browser.org/?magical=true",
	  "?magical=true&result=win",
	  "http://netsurf-browser.org/?magical=true&result=win"},

	{ "http://netsurf-browser.org/?magical=true#fragment",
	  "?magical=true&result=win",
	  "http://netsurf-browser.org/?magical=true&result=win#fragment"},

	{ "http://netsurf-browser.org/#fragment",
	  "?magical=true&result=win",
	  "http://netsurf-browser.org/?magical=true&result=win#fragment"},

	{ "http://netsurf-browser.org/path",
	  "?magical=true",
	  "http://netsurf-browser.org/path?magical=true"},

	{ NULL,	NULL, NULL }
};

/**
 * Test nsurl
 */
int main(void)
{
	nsurl *base;
	nsurl *joined;
	char *string;
	size_t len;
	const char *url;
	const struct test_pairs *test;
	const struct test_triplets *ttest;
	int passed = 0;
	int count = 0;
	nserror err;

	verbose_log = true;

	if (corestrings_init() != NSERROR_OK) {
		assert(0 && "Failed to init corestrings.");
	}

	/* Create base URL */
	if (nsurl_create("http://a/b/c/d;p?q", &base) != NSERROR_OK) {
		assert(0 && "Failed to create base URL.");
	}

	if (nsurl_get(base, NSURL_WITH_FRAGMENT, &string, &len) != NSERROR_OK) {
		LOG(("Failed to get string"));
	} else {
		LOG(("Testing nsurl_join with base %s", string));
		free(string);
	}

	for (test = join_tests; test->test != NULL; test++) {
		if (nsurl_join(base, test->test, &joined) != NSERROR_OK) {
			LOG(("Failed to join test URL."));
		} else {
			if (nsurl_get(joined, NSURL_WITH_FRAGMENT,
					&string, &len) !=
					NSERROR_OK) {
				LOG(("Failed to get string"));
			} else {
				if (strcmp(test->res, string) == 0) {
					LOG(("\tPASS: \"%s\"\t--> %s",
						test->test,
						string));
					passed++;
				} else {
					LOG(("\tFAIL: \"%s\"\t--> %s",
						test->test,
						string));
					LOG(("\t\tExpecting: %s",
						test->res));
				}
				free(string);
			}
			nsurl_unref(joined);
		}
		count++;
	}

	nsurl_unref(base);

	/* Create tests */
	LOG(("Testing nsurl_create"));
	for (test = create_tests; test->test != NULL; test++) {
		err = nsurl_create(test->test, &base);
		if (err != NSERROR_OK || test->res == NULL) {
			if (test->res == NULL && err != NSERROR_OK) {
				LOG(("\tPASS: \"%s\"\t--> BAD INPUT",
						test->test));
				passed++;
			} else if (test->res != NULL && err != NSERROR_OK) {
				LOG(("Failed to create URL:\n\t\t%s.",
						test->test));
			} else {
				LOG(("\tFAIL: \"%s\"\t--> %s",
					test->test, nsurl_access(base)));
				LOG(("\t\tExpecting BAD INPUT"));
			}
			if (err == NSERROR_OK)
				nsurl_unref(base);
		} else {
			if (strcmp(nsurl_access(base), test->res) == 0) {
				LOG(("\tPASS: \"%s\"\t--> %s",
					test->test, nsurl_access(base)));
				passed++;
			} else {
				LOG(("\tFAIL: \"%s\"\t--> %s",
					test->test, nsurl_access(base)));
				LOG(("\t\tExpecting %s", test->res));
			}

			nsurl_unref(base);
		}
		count++;
	}

	/* Replace query tests */
	LOG(("Testing nsurl_replace_query"));
	for (ttest = replace_query_tests; ttest->test1 != NULL; ttest++) {
		if (nsurl_create(ttest->test1, &base) != NSERROR_OK) {
			LOG(("Failed to create URL:\n\t\t%s.", ttest->test1));
		} else {
			if (nsurl_replace_query(base, ttest->test2, &joined) !=
					NSERROR_OK) {
				LOG(("Failed to make test URL"));
			} else {
				if (strcmp(nsurl_access(joined),
						ttest->res) == 0) {
					LOG(("\tPASS: \"%s\" + %s",
						ttest->test1,
						ttest->test2));
					passed++;
				} else {
					LOG(("\tFAIL: \"%s\" + %s",
						ttest->test1,
						ttest->test2));
					LOG(("\t\tExpecting %s", ttest->res));
					LOG(("\t\tGot %s",
							nsurl_access(joined)));
				}

				nsurl_unref(joined);
			}

			nsurl_unref(base);
		}
		count++;
	}

	if (passed == count) {
		LOG(("Testing complete: SUCCESS"));
	} else {
		LOG(("Testing complete: FAILURE"));
		LOG(("Failed %d out of %d", count - passed, count));
	}

	corestrings_fini();

	LOG(("Remaining lwc strings:"));
	lwc_iterate_strings(netsurf_lwc_iterator, NULL);

	return 0;
}

