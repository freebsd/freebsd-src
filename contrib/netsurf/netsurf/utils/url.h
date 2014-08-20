/*
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

/** 
 * \file utils/url.h
 * \brief URL parsing and joining (interface).
 */

#ifndef _NETSURF_UTILS_URL_H_
#define _NETSURF_UTILS_URL_H_

/** File url prefix */
#define FILE_SCHEME_PREFIX "file:///"
/** File url prefix length */
#define FILE_SCHEME_PREFIX_LEN 8

/** URL utility function return codes */
typedef enum {
	URL_FUNC_OK,     /**< No error */
	URL_FUNC_NOMEM,  /**< Insufficient memory */
	URL_FUNC_FAILED  /**< Non fatal error (eg failed to match regex) */
} url_func_result;

struct url_components {
  	const char *buffer;
	const char *scheme;
	const char *authority;
	const char *path;
	const char *query;
	const char *fragment;
};

void url_init(void);
bool url_host_is_ip_address(const char *host);
url_func_result url_join(const char *rel, const char *base, char **result);
url_func_result url_host(const char *url, char **result);
url_func_result url_scheme(const char *url, char **result);
url_func_result url_nice(const char *url, char **result,
		bool remove_extensions);
url_func_result url_escape(const char *unescaped, size_t toskip,
		bool sptoplus, const char *escexceptions, char **result);
url_func_result url_unescape(const char *str, char **result);
url_func_result url_path(const char *url, char **result);

#endif
