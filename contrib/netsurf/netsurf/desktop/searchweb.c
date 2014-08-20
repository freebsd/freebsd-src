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
 
 /** \file
 * web search (core)
 */
#include "utils/config.h"

#include <ctype.h>
#include <string.h>
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser.h"
#include "desktop/gui_factory.h"
#include "utils/nsoption.h"
#include "desktop/searchweb.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

static struct search_provider {
	char *name; /**< readable name such as 'google', 'yahoo', etc */
	char *hostname; /**< host address such as www.google.com */
	char *searchstring; /** < such as "www.google.com?search=%s" */
	char *ico; /** < location of domain's favicon */
} current_search_provider;

static hlcache_handle *search_ico = NULL;
char *search_engines_file_location;
char *search_default_ico_location;

#ifdef WITH_BMP
static nserror search_web_ico_callback(hlcache_handle *ico,
		const hlcache_event *event, void *pw);
#endif

/** 
 * creates a new browser window according to the search term
 * \param searchterm such as "my search term"
 */

bool search_web_new_window(struct browser_window *bw, const char *searchterm)
{
	char *encsearchterm;
	char *urltxt;
	nsurl *url;
	nserror error;

	if (url_escape(searchterm,0, true, NULL, &encsearchterm) != URL_FUNC_OK)
		return false;

	urltxt = search_web_get_url(encsearchterm);
	free(encsearchterm);

	error = nsurl_create(urltxt, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY |
					      BW_CREATE_TAB,
					      url,
					      NULL,
					      bw,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	free(urltxt);
	return true;
}

/** simplistic way of checking whether an entry from the url bar is an
 * url / a search; could be improved to properly test terms
 */

bool search_is_url(const char *url)
{
	/** \todo Implement this properly */

	/* For now, everything is an URL */
	return true;
}

/**
 * caches the details of the current web search provider
 * \param reference the enum value of the provider
 * browser init code [as well as changing preferences code] should call
 * search_web_provider_details(option_search_provider)
 */

void search_web_provider_details(int reference)
{
	char buf[300];
	int ref = 0;
	FILE *f;
	if (search_engines_file_location == NULL)
		return;
	f = fopen(search_engines_file_location, "r");
	if (f == NULL)
		return;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] == '\0')
			continue;
		buf[strlen(buf)-1] = '\0';
		if (ref++ == (int)reference)
			break;
	}
	fclose(f);
	if (current_search_provider.name != NULL)
		free(current_search_provider.name);
	current_search_provider.name = strdup(strtok(buf, "|"));
	if (current_search_provider.hostname != NULL)
		free(current_search_provider.hostname);
	current_search_provider.hostname = strdup(strtok(NULL, "|"));
	if (current_search_provider.searchstring != NULL)
		free(current_search_provider.searchstring);
	current_search_provider.searchstring = strdup(strtok(NULL, "|"));
	if (current_search_provider.ico != NULL)
		free(current_search_provider.ico);
	current_search_provider.ico = strdup(strtok(NULL, "|"));
	return;
}

/**
 * escapes a search term then creates the appropriate url from it
 */

char *search_web_from_term(const char *searchterm)
{
	char *encsearchterm, *url;
	if (url_escape(searchterm, 0, true, NULL, &encsearchterm)
			!= URL_FUNC_OK)
		return strdup(searchterm);
	url = search_web_get_url(encsearchterm);
	free(encsearchterm);
	return url;
}

/** accessor for global search provider name */

char *search_web_provider_name(void)
{
	if (current_search_provider.name)
		return strdup(current_search_provider.name);
	return strdup("google");
}

/** accessor for global search provider hostname */

char *search_web_provider_host(void)
{
	if (current_search_provider.hostname)
		return strdup(current_search_provider.hostname);
	return strdup("www.google.com");
}

/** accessor for global search provider ico name */

char *search_web_ico_name(void)
{
	if (current_search_provider.ico)
		return strdup(current_search_provider.ico);
	return strdup("http://www.google.com/favicon.ico");
}

/**
 * creates a full url from an encoded search term
 */

char *search_web_get_url(const char *encsearchterm)
{
	char *pref, *ret;
	int len;
	if (current_search_provider.searchstring)
		pref = strdup(current_search_provider.searchstring);
	else
		pref = strdup("http://www.google.com/search?q=%s");
	if (pref == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	len = strlen(encsearchterm) + strlen(pref);
	ret = malloc(len -1); /* + '\0' - "%s" */
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(pref);
		return NULL;
	}
	snprintf(ret, len-1, pref, encsearchterm);
	free(pref);
	return ret;
}

/**
 * function to retrieve the search web ico, from cache / from local
 * filesystem / from the web
 * \param localdefault true when there is no appropriate favicon
 * update the search_ico cache else delay until fetcher callback
 */

void search_web_retrieve_ico(bool localdefault)
{
#if !defined(WITH_BMP)
	/* This function is of limited use when no BMP support
	 * is enabled, given the icons it is fetching are BMPs
	 * more often than not.  This also avoids an issue where
	 * all this code goes mad if BMP support is not enabled.
	 */
	return;
#else
	content_type accept = CONTENT_IMAGE;
	char *url;
	nserror error;
	nsurl *icon_nsurl;

	if (localdefault) {
		if (search_default_ico_location == NULL)
			return;
		url = guit->fetch->path_to_url(search_default_ico_location);
	} else {
		url = search_web_ico_name();
	}

	if (url == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}

	error = nsurl_create(url, &icon_nsurl);
	if (error != NSERROR_OK) {
		free(url);
		search_ico = NULL;
		return;
	}

	error = hlcache_handle_retrieve(icon_nsurl, 0, NULL, NULL,
			search_web_ico_callback, NULL, NULL, accept,
			&search_ico);

	nsurl_unref(icon_nsurl);

	if (error != NSERROR_OK)
		search_ico = NULL;

	free(url);
#endif /* WITH_BMP */
}

/**
 * returns a reference to the static global search_ico [ / NULL]
 * caller may adjust ico's settings; clearing / free()ing is the core's
 * responsibility
 */

hlcache_handle *search_web_ico(void)
{
	return search_ico;
}

/**
 * Cleans up any remaining resources during shutdown.
 */
void search_web_cleanup(void)
{
	if (search_ico != NULL) {
		hlcache_handle_release(search_ico);
		search_ico = NULL;
	}
}

/**
 * callback function to cache ico then notify front when successful
 * else retry default from local file system
 */

#ifdef WITH_BMP
nserror search_web_ico_callback(hlcache_handle *ico,
		const hlcache_event *event, void *pw)
{
	switch (event->type) {

	case CONTENT_MSG_DONE:
		LOG(("got favicon '%s'", nsurl_access(hlcache_handle_get_url(ico))));
		guit->browser->set_search_ico(search_ico);
		break;

	case CONTENT_MSG_ERROR:
		LOG(("favicon %s error: %s",
				nsurl_access(hlcache_handle_get_url(ico)),
				event->data.error));
		hlcache_handle_release(search_ico);
		search_ico = NULL;
		search_web_retrieve_ico(true);
		break;

	default:
		break;
	}

	return NSERROR_OK;
}
#endif /* WITH_BMP */
