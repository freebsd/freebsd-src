/*
 * Copyright 2012 Vincent Sanders <vince@kyllikki.org>
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
 * Content for javascript (implementation)
 */

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "utils/config.h"
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "javascript/content.h"

typedef struct javascript_content {
	struct content base;
} javascript_content;

static nserror javascript_create(const content_handler *handler,
		lwc_string *imime_type, const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	javascript_content *script;
	nserror error;

	script = calloc(1, sizeof(javascript_content));
	if (script == NULL)
		return NSERROR_NOMEM;

	error = content__init(&script->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(script);
		return error;
	}

	*c = (struct content *) script;

	return NSERROR_OK;
}

static bool javascript_convert(struct content *c)
{
	content_set_ready(c);
	content_set_done(c);

	return true;
}

static nserror 
javascript_clone(const struct content *old, struct content **newc)
{
	javascript_content *script;
	nserror error;

	script = calloc(1, sizeof(javascript_content));
	if (script == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, &script->base);
	if (error != NSERROR_OK) {
		content_destroy(&script->base);
		return error;
	}

	*newc = (struct content *) script;

	return NSERROR_OK;
}

static void javascript_destroy(struct content *c)
{
}

static content_type javascript_content_type(void)
{
	return CONTENT_JS;
}


static const content_handler javascript_content_handler = {
	.create = javascript_create,
	.data_complete = javascript_convert,
	.destroy = javascript_destroy,
	.clone = javascript_clone,
	.type = javascript_content_type,
	.no_share = false,
};

static const char *javascript_types[] = {
	"application/javascript", /* RFC 4329 */
	"application/ecmascript", /* RFC 4329 */
	"application/x-javascript", /* common usage */
	"text/javascript", /* common usage */
	"text/ecmascript", /* common usage */
};

CONTENT_FACTORY_REGISTER_TYPES(javascript, javascript_types, javascript_content_handler);
