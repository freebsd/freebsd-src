/*
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2009 John Tytgat <joty@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <curl/curl.h>

#include "image/bitmap.h"
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/cookie_manager.h"
#include "utils/nsoption.h"
#ifdef riscos
/** \todo lose this */
#include "riscos/bitmap.h"
#endif
#include "utils/log.h"
#include "utils/corestrings.h"
#include "utils/filename.h"
#include "utils/url.h"
#include "utils/utils.h"

int option_expire_url = 0;
bool verbose_log = true;

static void netsurf_lwc_iterator(lwc_string *str, void *pw)
{
	LOG(("[%3u] %.*s", str->refcnt, (int) lwc_string_length(str), lwc_string_data(str)));
}

bool cookie_manager_add(const struct cookie_data *data)
{
	return true;
}

void cookie_manager_remove(const struct cookie_data *data)
{
}

void die(const char *error)
{
	printf("die: %s\n", error);
	exit(1);
}


void warn_user(const char *warning, const char *detail)
{
	printf("WARNING: %s %s\n", warning, detail);
}

void bitmap_destroy(void *bitmap)
{
}

char *path_to_url(const char *path)
{
	char *r = malloc(strlen(path) + 7 + 1);

	strcpy(r, "file://");
	strcat(r, path);

	return r;
}

nsurl *make_url(const char *url)
{
	nsurl *nsurl;
	if (nsurl_create(url, &nsurl) != NSERROR_OK) {
		LOG(("failed creating nsurl"));
		exit(1);
	}
	return nsurl;
}

char *make_path_query(nsurl *url)
{
	size_t len;
	char *path_query;
	if (nsurl_get(url, NSURL_PATH | NSURL_QUERY, &path_query, &len) !=
			NSERROR_OK) {
		LOG(("failed creating path_query"));
		exit(1);
	}
	return path_query;
}

lwc_string *make_lwc(const char *str)
{
	lwc_string *lwc;
	if (lwc_intern_string(str, strlen(str), &lwc) != lwc_error_ok) {
		LOG(("failed creating lwc_string"));
		exit(1);
	}
	return lwc;
}


bool test_urldb_set_cookie(const char *header, const char *url,
		const char *referer)
{
	nsurl *r = NULL;
	nsurl *nsurl = make_url(url);
	bool ret;

	if (referer != NULL)
		r = make_url(referer);

	ret = urldb_set_cookie(header, nsurl, r);

	if (referer != NULL)
		nsurl_unref(r);
	nsurl_unref(nsurl);

	return ret;
}

char *test_urldb_get_cookie(const char *url)
{
	nsurl *nsurl = make_url(url);
	char *ret;

	ret = urldb_get_cookie(nsurl, true);
	nsurl_unref(nsurl);

	return ret;
}

int main(void)
{
	struct host_part *h;
	struct path_data *p;
	const struct url_data *u;
	int i;
	lwc_string *scheme;
	lwc_string *fragment;
	nsurl *url;
	nsurl *urlr;
	char *path_query;

	corestrings_init();
	url_init();

	h = urldb_add_host("127.0.0.1");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	h = urldb_add_host("intranet");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	url = make_url("http://intranet/");
	scheme = nsurl_get_component(url, NSURL_SCHEME);
	p = urldb_add_path(scheme, 0, h, strdup("/"), NULL, url);
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}
	lwc_string_unref(scheme);

	urldb_set_url_title(url, "foo");

	u = urldb_get_url_data(url);
	assert(u && strcmp(u->title, "foo") == 0);
	nsurl_unref(url);

	/* Get host entry */
	h = urldb_add_host("netsurf.strcprstskrzkrk.co.uk");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	/* Get path entry */
	url = make_url("http://netsurf.strcprstskrzkrk.co.uk/path/to/resource.htm?a=b");
	scheme = nsurl_get_component(url, NSURL_SCHEME);
	path_query = make_path_query(url);
	fragment = make_lwc("zz");
	p = urldb_add_path(scheme, 0, h, strdup(path_query), fragment, url);
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}
	lwc_string_unref(fragment);

	fragment = make_lwc("aa");
	p = urldb_add_path(scheme, 0, h, strdup(path_query), fragment, url);
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}
	lwc_string_unref(fragment);

	fragment = make_lwc("yy");
	p = urldb_add_path(scheme, 0, h, strdup(path_query), fragment, url);
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}
	free(path_query);
	lwc_string_unref(fragment);
	lwc_string_unref(scheme);
	nsurl_unref(url);

	url = make_url("file:///home/");
	urldb_add_url(url);
	nsurl_unref(url);

	url = make_url("http://www.minimarcos.org.uk/cgi-bin/forum/Blah.pl?,v=login,p=2");
	urldb_set_cookie("mmblah=foo; path=/; expires=Thur, 31-Dec-2099 00:00:00 GMT\r\n", url, NULL);
	nsurl_unref(url);

	url = make_url("http://www.minimarcos.org.uk/cgi-bin/forum/Blah.pl?,v=login,p=2");
	urldb_set_cookie("BlahPW=bar; path=/; expires=Thur, 31-Dec-2099 00:00:00 GMT\r\n", url, NULL);
	nsurl_unref(url);

	url = make_url("http://ccdb.cropcircleresearch.com/");
	urldb_set_cookie("details=foo|bar|Sun, 03-Jun-2007;expires=Mon, 24-Jul-2006 09:53:45 GMT\r\n", url, NULL);
	nsurl_unref(url);

	url = make_url("http://www.google.com/");
	urldb_set_cookie("PREF=ID=a:TM=b:LM=c:S=d; path=/; domain=.google.com\r\n", url, NULL);
	nsurl_unref(url);

	url = make_url("http://www.bbc.co.uk/");
	urldb_set_cookie("test=foo, bar, baz; path=/, quux=blah; path=/", url, NULL);
	nsurl_unref(url);

//	urldb_set_cookie("a=b; path=/; domain=.a.com", "http://a.com/", NULL);

	url = make_url("https://www.foo.com/blah/moose");
	urlr = make_url("https://www.foo.com/blah/moose");
	urldb_set_cookie("foo=bar;Path=/blah;Secure\r\n", url, urlr);
	nsurl_unref(url);
	nsurl_unref(urlr);

	url = make_url("https://www.foo.com/blah/wxyzabc");
	urldb_get_cookie(url, true);
	nsurl_unref(url);

	/* 1563546 */
	url = make_url("http:moodle.org");
	assert(urldb_add_url(url) == true);
	assert(urldb_get_url(url) != NULL);
	nsurl_unref(url);

	/* also 1563546 */
	url = make_url("http://a_a/");
	assert(urldb_add_url(url));
	assert(urldb_get_url(url));
	nsurl_unref(url);

	/* 1597646 */
	url = make_url("http://foo@moose.com/");
	if (urldb_add_url(url)) {
		LOG(("added %s", nsurl_access(url)));
		assert(urldb_get_url(url) != NULL);
	}
	nsurl_unref(url);

	/* 1535120 */
	url = make_url("http://www2.2checkout.com/");
	assert(urldb_add_url(url));
	assert(urldb_get_url(url));
	nsurl_unref(url);

	/* Numeric subdomains */
	url = make_url("http://2.bp.blogspot.com/_448y6kVhntg/TSekubcLJ7I/AAAAAAAAHJE/yZTsV5xT5t4/s1600/covers.jpg");
	assert(urldb_add_url(url));
	assert(urldb_get_url(url));
	nsurl_unref(url);

	/* Valid path */
	assert(test_urldb_set_cookie("name=value;Path=/\r\n", "http://www.google.com/", NULL));

	/* Valid path (non-root directory) */
	assert(test_urldb_set_cookie("name=value;Path=/foo/bar/\r\n", "http://www.example.org/foo/bar/", NULL));

	/* Defaulted path */
	assert(test_urldb_set_cookie("name=value\r\n", "http://www.example.org/foo/bar/baz/bat.html", NULL));
	assert(test_urldb_get_cookie("http://www.example.org/foo/bar/baz/quux.htm"));

	/* Defaulted path with no non-leaf path segments */
	assert(test_urldb_set_cookie("name=value\r\n", "http://no-non-leaf.example.org/index.html", NULL));
	assert(test_urldb_get_cookie("http://no-non-leaf.example.org/page2.html"));
	assert(test_urldb_get_cookie("http://no-non-leaf.example.org/"));

	/* Valid path (includes leafname) */
	assert(test_urldb_set_cookie("name=value;Version=1;Path=/index.cgi\r\n", "http://example.org/index.cgi", NULL));
	assert(test_urldb_get_cookie("http://example.org/index.cgi"));

	/* Valid path (includes leafname in non-root directory) */
	assert(test_urldb_set_cookie("name=value;Path=/foo/index.html\r\n", "http://www.example.org/foo/index.html", NULL));
	/* Should _not_ match the above, as the leafnames differ */
	assert(test_urldb_get_cookie("http://www.example.org/foo/bar.html") == NULL);

	/* Invalid path (contains different leafname) */
	assert(test_urldb_set_cookie("name=value;Path=/index.html\r\n", "http://example.org/index.htm", NULL) == false);
	
	/* Invalid path (contains leafname in different directory) */
	assert(test_urldb_set_cookie("name=value;Path=/foo/index.html\r\n", "http://www.example.org/bar/index.html", NULL) == false);

	/* Test partial domain match with IP address failing */
	assert(test_urldb_set_cookie("name=value;Domain=.foo.org\r\n", "http://192.168.0.1/", NULL) == false);

	/* Test handling of non-domain cookie sent by server (domain part should
	 * be ignored) */
	assert(test_urldb_set_cookie("foo=value;Domain=blah.com\r\n", "http://www.example.com/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://www.example.com/"), "foo=value") == 0);

	/* Test handling of domain cookie from wrong host (strictly invalid but
	 * required to support the real world) */
	assert(test_urldb_set_cookie("name=value;Domain=.example.com\r\n", "http://foo.bar.example.com/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://www.example.com/"), "foo=value; name=value") == 0);

	/* Test presence of separators in cookie value */
	assert(test_urldb_set_cookie("name=\"value=foo\\\\bar\\\\\\\";\\\\baz=quux\";Version=1\r\n", "http://www.example.org/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://www.example.org/"), "$Version=1; name=\"value=foo\\\\bar\\\\\\\";\\\\baz=quux\"") == 0);

	/* Test cookie with blank value */
	assert(test_urldb_set_cookie("a=\r\n", "http://www.example.net/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://www.example.net/"), "a=") == 0);

	/* Test specification of multiple cookies in one header */
	assert(test_urldb_set_cookie("a=b, foo=bar; Path=/\r\n", "http://www.example.net/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://www.example.net/"), "a=b; foo=bar") == 0);

	/* Test use of separators in unquoted cookie value */
	assert(test_urldb_set_cookie("foo=moo@foo:blah?moar\\ text\r\n", "http://example.com/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://example.com/"), "foo=moo@foo:blah?moar\\ text; name=value") == 0);

	/* Test use of unnecessary quotes */
	assert(test_urldb_set_cookie("foo=\"hello\";Version=1,bar=bat\r\n", "http://example.com/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://example.com/"), "foo=\"hello\"; bar=bat; name=value") == 0);

	/* Test domain matching in unverifiable transactions */
	assert(test_urldb_set_cookie("foo=bar; domain=.example.tld\r\n", "http://www.foo.example.tld/", "http://bar.example.tld/"));
	assert(strcmp(test_urldb_get_cookie("http://www.foo.example.tld/"), "foo=bar") == 0);

	/* Test expiry */
	assert(test_urldb_set_cookie("foo=bar", "http://expires.com/", NULL));
	assert(strcmp(test_urldb_get_cookie("http://expires.com/"), "foo=bar") == 0);
	assert(test_urldb_set_cookie("foo=bar; expires=Thu, 01-Jan-1970 00:00:01 GMT\r\n", "http://expires.com/", NULL));
	assert(test_urldb_get_cookie("http://expires.com/") == NULL);

	urldb_dump();
	urldb_destroy();

	printf("PASS\n");

	corestrings_fini();
	LOG(("Remaining lwc strings:"));
	lwc_iterate_strings(netsurf_lwc_iterator, NULL);

	return 0;
}

