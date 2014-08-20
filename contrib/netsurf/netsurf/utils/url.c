/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
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

/** \file
 * URL parsing and joining (implementation).
 */

#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "curl/curl.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

struct url_components_internal {
	char *buffer;	/* buffer used for all the following data */
	char *scheme;
	char *authority;
	char *path;
	char *query;
	char *fragment;
};


regex_t url_re, url_up_re;

/**
 * Initialise URL routines.
 *
 * Compiles regular expressions required by the url_ functions.
 */

void url_init(void)
{
	/* regex from RFC 2396 */
	regcomp_wrapper(&url_re, "^[[:space:]]*"
#define URL_RE_SCHEME 2
			"(([a-zA-Z][-a-zA-Z0-9+.]*):)?"
#define URL_RE_AUTHORITY 4
			"(//([^/?#[:space:]]*))?"
#define URL_RE_PATH 5
			"([^?#[:space:]]*)"
#define URL_RE_QUERY 7
			"(\\?([^#[:space:]]*))?"
#define URL_RE_FRAGMENT 9
			"(#([^[:space:]]*))?"
			"[[:space:]]*$", REG_EXTENDED);
	regcomp_wrapper(&url_up_re,
			"/([^/]?|[.][^./]|[^./][.]|[^./][^./]|[^/][^/][^/]+)"
			"/[.][.](/|$)",
			REG_EXTENDED);
}


/**
 * Check whether a host string is an IP address.  It should support and
 * detect IPv4 addresses (all of dotted-quad or subsets, decimal or
 * hexadecimal notations) and IPv6 addresses (including those containing
 * embedded IPv4 addresses.)
 *
 * \param  host a hostname terminated by '\0'
 * \return true if the hostname is an IP address, false otherwise
 */
bool url_host_is_ip_address(const char *host)
{
	struct in_addr ipv4;
	size_t host_len = strlen(host);
	const char *sane_host;
	const char *slash;
#ifndef NO_IPV6
	struct in6_addr ipv6;
	char ipv6_addr[64];
#endif
	/* FIXME TODO: Some parts of urldb.c (and perhaps other parts of
	 * NetSurf) make confusions between hosts and "prefixes", we can
	 * sometimes be erroneously passed more than just a host.  Sometimes
	 * we may be passed trailing slashes, or even whole path segments.
	 * A specific criminal in this class is urldb_iterate_partial, which
	 * takes a prefix to search for, but passes that prefix to functions
	 * that expect only hosts.
	 *
	 * For the time being, we will accept such calls; we check if there
	 * is a / in the host parameter, and if there is, we take a copy and
	 * replace the / with a \0.  This is not a permanent solution; we
	 * should search through NetSurf and find all the callers that are
	 * in error and fix them.  When doing this task, it might be wise
	 * to replace the hideousness below with code that doesn't have to do
	 * this, and add assert(strchr(host, '/') == NULL); somewhere.
	 * -- rjek - 2010-11-04
	 */

	slash = strchr(host, '/');
	if (slash == NULL) {
		sane_host = host;
	} else {
		char *c = strdup(host);
		c[slash - host] = '\0';
		sane_host = c;
		host_len = slash - host - 1;
		LOG(("WARNING: called with non-host '%s'", host));
	}

	if (strspn(sane_host, "0123456789abcdefABCDEF[].:") < host_len)
		goto out_false;

	if (inet_aton(sane_host, &ipv4) != 0) {
		/* This can only be a sane IPv4 address if it contains 3 dots.
		 * Helpfully, inet_aton is happy to treat "a", "a.b", "a.b.c",
		 * and "a.b.c.d" as valid IPv4 address strings where we only
		 * support the full, dotted-quad, form.
		 */
		int num_dots = 0;
		size_t index;

		for (index = 0; index < host_len; index++) {
			if (sane_host[index] == '.')
				num_dots++;
		}

		if (num_dots == 3)
			goto out_true;
		else
			goto out_false;
	}

#ifndef NO_IPV6
	if (sane_host[0] != '[' || sane_host[host_len] != ']')
		goto out_false;

	strncpy(ipv6_addr, sane_host + 1, sizeof(ipv6_addr));
	ipv6_addr[sizeof(ipv6_addr) - 1] = '\0';

	if (inet_pton(AF_INET6, ipv6_addr, &ipv6) == 1)
		goto out_true;
#endif

out_false:
	if (slash != NULL) free((void *)sane_host);
	return false;

out_true:
	if (slash != NULL) free((void *)sane_host);
	return true;
}

/**
 * Split a URL into separate components
 *
 * URLs passed to this function are assumed to be valid and no error checking
 * or recovery is attempted.
 *
 * See RFC 3986 for reference.
 *
 * \param  url	     a valid absolute or relative URL
 * \param  result    pointer to buffer to hold components
 * \return  URL_FUNC_OK on success
 */

static url_func_result url_get_components(const char *url,
		struct url_components *result)
{
  	int storage_length;
	char *storage_end;
	const char *scheme;
	const char *authority;
	const char *path;
	const char *query;
	const char *fragment;
	struct url_components_internal *internal;

	assert(url);

	/* clear our return value */
	internal = (struct url_components_internal *)result;
	memset(result, 0x00, sizeof(struct url_components));

	/* get enough storage space for a URL with termination at each node */
	storage_length = strlen(url) + 8;
	internal->buffer = malloc(storage_length);
	if (!internal->buffer)
		return URL_FUNC_NOMEM;
	storage_end = internal->buffer;

	/* look for a valid scheme */
	scheme = url;
	if (isalpha(*scheme)) {
		for (scheme = url + 1;
				((*scheme != ':') && (*scheme != '\0'));
				scheme++) {
			if (!isalnum(*scheme) && (*scheme != '+') &&
					(*scheme != '-') && (*scheme != '.'))
				break;
		}

		if (*scheme == ':') {
			memcpy(storage_end, url, scheme - url);
			storage_end[scheme - url] = '\0';
			result->scheme = storage_end;
			storage_end += scheme - url + 1;
			scheme++;
		} else {
			scheme = url;
		}
	}


	/* look for an authority */
	authority = scheme;
	if ((authority[0] == '/') && (authority[1] == '/')) {
		authority = strpbrk(scheme + 2, "/?#");
		if (!authority)
			authority = scheme + strlen(scheme);
		memcpy(storage_end, scheme + 2, authority - scheme - 2);
		storage_end[authority - scheme - 2] = '\0';
		result->authority = storage_end;
		storage_end += authority - scheme - 1;
	}


	/* look for a path */
	path = authority;
	if ((*path != '?') && (*path != '#') && (*path != '\0')) {
		path = strpbrk(path, "?#");
		if (!path)
			path = authority + strlen(authority);
		memcpy(storage_end, authority, path - authority);
		storage_end[path - authority] = '\0';
		result->path = storage_end;
		storage_end += path - authority + 1;
	}


	/* look for a query */
	query = path;
	if (*query == '?') {
		query = strchr(query, '#');
		if (!query)
			query = path + strlen(path);
		memcpy(storage_end, path + 1, query - path - 1);
		storage_end[query - path - 1] = '\0';
		result->query = storage_end;
		storage_end += query - path;
	}


	/* look for a fragment */
	fragment = query;
	if (*fragment == '#') {
		fragment = query + strlen(query);

		/* make a copy of the result for the caller */
		memcpy(storage_end, query + 1, fragment - query - 1);
		storage_end[fragment - query - 1] = '\0';
		result->fragment = storage_end;
		storage_end += fragment - query;
	}

	assert((result->buffer + storage_length) >= storage_end);
	return URL_FUNC_OK;
}


/**
 * Reform a URL from separate components
 *
 * See RFC 3986 for reference.
 *
 * \param  components  the components to reform into a URL
 * \return  a new URL allocated on the heap, or NULL on failure
 */

static char *url_reform_components(const struct url_components *components)
{
	int scheme_len = 0, authority_len = 0, path_len = 0, query_len = 0,
			fragment_len = 0;
	char *result, *url;

	/* 5.3 */
	if (components->scheme)
		scheme_len = strlen(components->scheme) + 1;
	if (components->authority)
		authority_len = strlen(components->authority) + 2;
	if (components->path)
		path_len = strlen(components->path);
	if (components->query)
		query_len = strlen(components->query) + 1;
	if (components->fragment)
		fragment_len = strlen(components->fragment) + 1;

	/* claim memory */
	url = result = malloc(scheme_len + authority_len + path_len +
			query_len + fragment_len + 1);
	if (!url) {
		LOG(("malloc failed"));
		return NULL;
	}

	/* rebuild URL */
	if (components->scheme) {
	  	sprintf(url, "%s:", components->scheme);
		url += scheme_len;
	}
	if (components->authority) {
	  	sprintf(url, "//%s", components->authority);
		url += authority_len;
	}
	if (components->path) {
	  	sprintf(url, "%s", components->path);
		url += path_len;
	}
	if (components->query) {
	  	sprintf(url, "?%s", components->query);
		url += query_len;
	}
	if (components->fragment)
	  	sprintf(url, "#%s", components->fragment);
	return result;
}


/**
 * Release some url components from memory
 *
 * \param  result  pointer to buffer containing components
 */
static void url_destroy_components(const struct url_components *components)
{
	const struct url_components_internal *internal;

	assert(components);

	internal = (const struct url_components_internal *)components;
	if (internal->buffer)
		free(internal->buffer);
}


/**
 * Resolve a relative URL to absolute form.
 *
 * \param  rel	   relative URL
 * \param  base	   base URL, must be absolute and cleaned as by nsurl_create()
 * \param  result  pointer to pointer to buffer to hold absolute url
 * \return  URL_FUNC_OK on success
 */

url_func_result url_join(const char *rel, const char *base, char **result)
{
	url_func_result status = URL_FUNC_NOMEM;
	struct url_components_internal base_components = {0,0,0,0,0,0};
	struct url_components_internal *base_ptr = &base_components;
	struct url_components_internal rel_components = {0,0,0,0,0,0};
	struct url_components_internal *rel_ptr = &rel_components;
	struct url_components_internal merged_components = {0,0,0,0,0,0};
	struct url_components_internal *merged_ptr = &merged_components;
	char *merge_path = NULL, *split_point;
	char *input, *output, *start = NULL;
	int len, buf_len;

	(*result) = 0;

	assert(base);
	assert(rel);


	/* break down the relative URL (not cached, corruptable) */
	status = url_get_components(rel, (struct url_components *) rel_ptr);
	if (status != URL_FUNC_OK) {
		LOG(("relative url '%s' failed to get components", rel));
		return URL_FUNC_FAILED;
	}

	/* [1] relative URL is absolute, use it entirely */
	merged_components = rel_components;
	if (rel_components.scheme)
		goto url_join_reform_url;

	/* break down the base URL (possibly cached, not corruptable) */
	status = url_get_components(base, (struct url_components *) base_ptr);
	if (status != URL_FUNC_OK) {
		url_destroy_components((struct url_components *) rel_ptr);
		LOG(("base url '%s' failed to get components", base));
		return URL_FUNC_FAILED;
	}

	/* [2] relative authority takes presidence */
	merged_components.scheme = base_components.scheme;
	if (rel_components.authority)
		goto url_join_reform_url;

	/* [3] handle empty paths */
	merged_components.authority = base_components.authority;
	if (!rel_components.path) {
	  	merged_components.path = base_components.path;
		if (!rel_components.query)
			merged_components.query = base_components.query;
		goto url_join_reform_url;
	}

	/* [4] handle valid paths */
	if (rel_components.path[0] == '/')
		merged_components.path = rel_components.path;
	else {
		/* 5.2.3 */
		if ((base_components.authority) && (!base_components.path)) {
			merge_path = malloc(strlen(rel_components.path) + 2);
			if (!merge_path) {
				LOG(("malloc failed"));
				goto url_join_no_mem;
			}
			sprintf(merge_path, "/%s", rel_components.path);
			merged_components.path = merge_path;
		} else {
			split_point = base_components.path ?
					strrchr(base_components.path, '/') :
					NULL;
			if (!split_point) {
				merged_components.path = rel_components.path;
			} else {
				len = ++split_point - base_components.path;
				buf_len = len + 1 + strlen(rel_components.path);
				merge_path = malloc(buf_len);
				if (!merge_path) {
					LOG(("malloc failed"));
					goto url_join_no_mem;
				}
				memcpy(merge_path, base_components.path, len);
				memcpy(merge_path + len, rel_components.path,
						strlen(rel_components.path));
				merge_path[buf_len - 1] = '\0';
				merged_components.path = merge_path;
			}
		}
	}

url_join_reform_url:
	/* 5.2.4 */
	input = merged_components.path;
	if ((input) && (strchr(input, '.'))) {
	  	/* [1] remove all dot references */
	  	output = start = malloc(strlen(input) + 1);
	  	if (!output) {
			LOG(("malloc failed"));
			goto url_join_no_mem;
		}
		merged_components.path = output;
		*output = '\0';

		while (*input != '\0') {
		  	/* [2A] */
		  	if (input[0] == '.') {
		  		if (input[1] == '/') {
		  			input = input + 2;
		  			continue;
		  		} else if ((input[1] == '.') &&
		  				(input[2] == '/')) {
		  			input = input + 3;
		  			continue;
		  		}
		  	}

		  	/* [2B] */
		  	if ((input[0] == '/') && (input[1] == '.')) {
		  		if (input[2] == '/') {
		  		  	input = input + 2;
		  		  	continue;
		  		} else if (input[2] == '\0') {
		  		  	input = input + 1;
		  		  	*input = '/';
		  		  	continue;
		  		}

		  		/* [2C] */
		  		if ((input[2] == '.') && ((input[3] == '/') ||
		  				(input[3] == '\0'))) {
			  		if (input[3] == '/') {
			  		  	input = input + 3;
			  		} else {
		  				input = input + 2;
		  			  	*input = '/';
		  			}

		  			if ((output > start) &&
		  					(output[-1] == '/'))
		  				*--output = '\0';
		  			split_point = strrchr(start, '/');
		  			if (!split_point)
		  				output = start;
		  			else
		  				output = split_point;
		  			*output = '\0';
		  			continue;
		  		}
		  	}


		  	/* [2D] */
		  	if (input[0] == '.') {
		  		if (input[1] == '\0') {
		  			input = input + 1;
		  			continue;
		  		} else if ((input[1] == '.') &&
		  				(input[2] == '\0')) {
		  			input = input + 2;
		  			continue;
		  		}
		  	}

		  	/* [2E] */
		  	if (*input == '/')
		  		*output++ = *input++;
		  	while ((*input != '/') && (*input != '\0'))
		  		*output++ = *input++;
		  	*output = '\0';
                }
                /* [3] */
      		merged_components.path = start;
	}

	/* 5.3 */
	*result = url_reform_components((struct url_components *) merged_ptr);
  	if (!(*result))
		goto url_join_no_mem;

	/* return success */
	status = URL_FUNC_OK;

url_join_no_mem:
	free(start);
	free(merge_path);
	url_destroy_components((struct url_components *) base_ptr);
	url_destroy_components((struct url_components *) rel_ptr);
	return status;
}


/**
 * Return the host name from an URL.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold host name
 * \return  URL_FUNC_OK on success
 */

url_func_result url_host(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;
	const char *host_start, *host_end;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.authority) {
			url_destroy_components(&components);
			return URL_FUNC_FAILED;
		}
		host_start = strchr(components.authority, '@');
		host_start = host_start ? host_start + 1 : components.authority;

		/* skip over an IPv6 address if there is one */
		if (host_start[0] == '[') {
			host_end = strchr(host_start, ']') + 1;
		} else {
			host_end = strchr(host_start, ':');
		}

		if (!host_end)
			host_end = components.authority +
					strlen(components.authority);

		*result = malloc(host_end - host_start + 1);
		if (!(*result)) {
			url_destroy_components(&components);
			return URL_FUNC_FAILED;
		}
		memcpy((*result), host_start, host_end - host_start);
		(*result)[host_end - host_start] = '\0';
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Return the scheme name from an URL.
 *
 * See RFC 3986, 3.1 for reference.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold scheme name
 * \return  URL_FUNC_OK on success
 */

url_func_result url_scheme(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.scheme) {
			status = URL_FUNC_FAILED;
		} else {
			*result = strdup(components.scheme);
			if (!(*result))
				status = URL_FUNC_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}


/**
 * Extract path segment from an URL
 *
 * \param url	  an absolute URL
 * \param result  pointer to pointer to buffer to hold result
 * \return URL_FUNC_OK on success
 */

url_func_result url_path(const char *url, char **result)
{
	url_func_result status;
	struct url_components components;

	assert(url);

	status = url_get_components(url, &components);
	if (status == URL_FUNC_OK) {
		if (!components.path) {
			status = URL_FUNC_FAILED;
		} else {
			*result = strdup(components.path);
			if (!(*result))
				status = URL_FUNC_NOMEM;
		}
	}
	url_destroy_components(&components);
	return status;
}

/**
 * Attempt to find a nice filename for a URL.
 *
 * \param  url	   an absolute URL
 * \param  result  pointer to pointer to buffer to hold filename
 * \param  remove_extensions  remove any extensions from the filename
 * \return  URL_FUNC_OK on success
 */

url_func_result url_nice(const char *url, char **result,
		bool remove_extensions)
{
	int m;
	regmatch_t match[10];
	regoff_t start, end;
	size_t i;
	char *dot;

	*result = 0;

	m = regexec(&url_re, url, 10, match, 0);
	if (m) {
		LOG(("url '%s' failed to match regex", url));
		return URL_FUNC_FAILED;
	}

	/* extract the last component of the path, if possible */
	if (match[URL_RE_PATH].rm_so == -1 || match[URL_RE_PATH].rm_so ==
			match[URL_RE_PATH].rm_eo)
		goto no_path;  /* no path, or empty */
	for (end = match[URL_RE_PATH].rm_eo - 1;
			end != match[URL_RE_PATH].rm_so && url[end] == '/';
			end--)
		;
	if (end == match[URL_RE_PATH].rm_so)
		goto no_path;  /* path is a string of '/' */
	end++;
	for (start = end - 1;
			start != match[URL_RE_PATH].rm_so && url[start] != '/';
			start--)
		;
	if (url[start] == '/')
		start++;

	if (!strncasecmp(url + start, "index.", 6) ||
			!strncasecmp(url + start, "default.", 8)) {
		/* try again */
		if (start == match[URL_RE_PATH].rm_so)
			goto no_path;
		for (end = start - 1;
				end != match[URL_RE_PATH].rm_so &&
				url[end] == '/';
				end--)
			;
		if (end == match[URL_RE_PATH].rm_so)
			goto no_path;
		end++;
		for (start = end - 1;
				start != match[URL_RE_PATH].rm_so &&
				url[start] != '/';
				start--)
		;
		if (url[start] == '/')
			start++;
	}

	*result = malloc(end - start + 1);
	if (!*result) {
		LOG(("malloc failed"));
		return URL_FUNC_NOMEM;
	}
	strncpy(*result, url + start, end - start);
	(*result)[end - start] = 0;

	if (remove_extensions) {
		dot = strchr(*result, '.');
		if (dot && dot != *result)
			*dot = 0;
	}

	return URL_FUNC_OK;

no_path:

	/* otherwise, use the host name, with '.' replaced by '_' */
	if (match[URL_RE_AUTHORITY].rm_so != -1 &&
			match[URL_RE_AUTHORITY].rm_so !=
			match[URL_RE_AUTHORITY].rm_eo) {
		*result = malloc(match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so + 1);
		if (!*result) {
			LOG(("malloc failed"));
			return URL_FUNC_NOMEM;
		}
		strncpy(*result, url + match[URL_RE_AUTHORITY].rm_so,
				match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so);
		(*result)[match[URL_RE_AUTHORITY].rm_eo -
				match[URL_RE_AUTHORITY].rm_so] = 0;

		for (i = 0; (*result)[i]; i++)
			if ((*result)[i] == '.')
				(*result)[i] = '_';

		return URL_FUNC_OK;
	}

	return URL_FUNC_FAILED;
}

/**
 * Convert an escaped string to plain.
 * \param result unescaped string owned by caller must be freed with free()
 * \return  URL_FUNC_OK on success
 */
url_func_result url_unescape(const char *str, char **result)
{
	char *curlstr;
	char *retstr;

	curlstr = curl_unescape(str, 0);
	if (curlstr == NULL) {
		return URL_FUNC_NOMEM;
	}

	retstr = strdup(curlstr);
	curl_free(curlstr);

	if (retstr == NULL) {
		return URL_FUNC_NOMEM;
	}

	*result = retstr;
	return URL_FUNC_OK;
}

/**
 * Escape a string suitable for inclusion in an URL.
 *
 * \param  unescaped      the unescaped string
 * \param  toskip         number of bytes to skip in unescaped string
 * \param  sptoplus       true iff spaces should be converted to +
 * \param  escexceptions  NULL or a string of characters excluded to be escaped
 * \param  result         pointer to pointer to buffer to hold escaped string
 * \return  URL_FUNC_OK on success
 */

url_func_result url_escape(const char *unescaped, size_t toskip,
		bool sptoplus, const char *escexceptions, char **result)
{
	size_t len;
	char *escaped, *d, *tmpres;
	const char *c;

	if (!unescaped || !result)
		return URL_FUNC_FAILED;

	*result = NULL;

	len = strlen(unescaped);
	if (len < toskip)
		return URL_FUNC_FAILED;
	len -= toskip;

	escaped = malloc(len * 3 + 1);
	if (!escaped)
		return URL_FUNC_NOMEM;

	for (c = unescaped + toskip, d = escaped; *c; c++) {
		/* Check if we should escape this byte.
		 * '~' is unreserved and should not be percent encoded, if
		 * you believe the spec; however, leaving it unescaped
		 * breaks a bunch of websites, so we escape it anyway. */
		if (!isascii(*c)
			|| (strchr(":/?#[]@" /* gen-delims */
				  "!$&'()*+,;=" /* sub-delims */
				  "<>%\"{}|\\^`~" /* others */,	*c)
				&& (!escexceptions || !strchr(escexceptions, *c)))
			|| *c <= 0x20 || *c == 0x7f) {
			if (*c == 0x20 && sptoplus) {
				*d++ = '+';
			} else {
				*d++ = '%';
				*d++ = "0123456789ABCDEF"[((*c >> 4) & 0xf)];
				*d++ = "0123456789ABCDEF"[(*c & 0xf)];
			}
		} else {
			/* unreserved characters: [a-zA-Z0-9-._] */
			*d++ = *c;
		}
	}
	*d++ = '\0';

	tmpres = malloc(d - escaped + toskip);
	if (!tmpres) {
		free(escaped);
		return URL_FUNC_NOMEM;
	}

	memcpy(tmpres, unescaped, toskip); 
	memcpy(tmpres + toskip, escaped, d - escaped);
	*result = tmpres;

	free(escaped);

	return URL_FUNC_OK;
}


#ifdef TEST

int main(int argc, char *argv[])
{
	int i;
	url_func_result res;
	char *s;
	url_init();
	for (i = 1; i != argc; i++) {
/*		printf("==> '%s'\n", argv[i]);
		res = url_normalize(argv[i], &s);
		if (res == URL_FUNC_OK) {
			printf("<== '%s'\n", s);
			free(s);
		}*/
/*		printf("==> '%s'\n", argv[i]);
		res = url_host(argv[i], &s);
		if (res == URL_FUNC_OK) {
			printf("<== '%s'\n", s);
			free(s);
		}*/
		if (1 != i) {
			res = url_join(argv[i], argv[1], &s);
			if (res == URL_FUNC_OK) {
				printf("'%s' + '%s' \t= '%s'\n", argv[1],
						argv[i], s);
				free(s);
			}
		}
/*		printf("'%s' => ", argv[i]);
		res = url_nice(argv[i], &s, true);
		if (res == URL_FUNC_OK) {
			printf("'%s', ", s);
			free(s);
		} else {
			printf("failed %u, ", res);
		}
		res = url_nice(argv[i], &s, false);
		if (res == URL_FUNC_OK) {
			printf("'%s', ", s);
			free(s);
		} else {
			printf("failed %u, ", res);
		}
		printf("\n");*/
	}
	return 0;
}

void regcomp_wrapper(regex_t *preg, const char *regex, int cflags)
{
	char errbuf[200];
	int r;
	r = regcomp(preg, regex, cflags);
	if (r) {
		regerror(r, preg, errbuf, sizeof errbuf);
		fprintf(stderr, "Failed to compile regexp '%s'\n", regex);
		fprintf(stderr, "error: %s\n", errbuf);
		exit(1);
	}
}

#endif
