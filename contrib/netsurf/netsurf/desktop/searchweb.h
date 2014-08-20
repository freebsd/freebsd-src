/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#ifndef _NETSURF_DESKTOP_SEARCH_WEB_H_
#define _NETSURF_DESKTOP_SEARCH_WEB_H_

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

struct browser_window;
struct hlcache_handle;

extern char *search_engines_file_location;
extern char *search_default_ico_location;

/**
 * open new tab/window for web search term
 */
bool search_web_new_window(struct browser_window *bw, const char *searchterm);

/**
 * retrieve full search url from unencoded search term
 */
char *search_web_from_term(const char *searchterm);

/**
 * retrieve full search url from encoded web search term
 */
char *search_web_get_url(const char *encsearchterm);

/**
 *  cache details of web search provider from file
 */
void search_web_provider_details(int reference);

/**
 * retrieve name of web search provider
 */
char *search_web_provider_name(void);

/**
 * retrieve hostname of web search provider
 */
char *search_web_provider_host(void);

/**
 * retrieve name of .ico for search bar
 */
char *search_web_ico_name(void);

/**
 * check whether an URL is in fact a search term
 * \param url the url being checked
 * \return true for url, false for search
 */
bool search_is_url(const char *url);

void search_web_retrieve_ico(bool localdefault);

struct hlcache_handle *search_web_ico(void);

void search_web_cleanup(void);

#endif
