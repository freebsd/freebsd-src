/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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
 * Dummy implementation of javascript engine functions.
 */

#include "content/content.h"
#include "utils/nsoption.h"

#include "javascript/js.h"
#include "utils/log.h"

void js_initialise(void)
{
	nsoption_set_bool(enable_javascript, false);
}

void js_finalise(void)
{
}

jscontext *js_newcontext(int timeout, jscallback *cb, void *cbctx)
{
	return NULL;
}

void js_destroycontext(jscontext *ctx)
{
}

jsobject *js_newcompartment(jscontext *ctx, void *win_priv, void *doc_priv)
{
	return NULL;
}

bool js_exec(jscontext *ctx, const char *txt, size_t txtlen)
{
	return true;
}

bool js_fire_event(jscontext *ctx, const char *type, struct dom_document *doc, struct dom_node *target)
{
	return true;
}
