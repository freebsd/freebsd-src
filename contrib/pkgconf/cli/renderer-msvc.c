/*
 * renderer-msvc.c
 * MSVC library syntax renderer
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2017 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <string.h>
#include <stdlib.h>

#include <libpkgconf/libpkgconf.h>
#include "renderer-msvc.h"

static inline bool
should_quote(const pkgconf_buffer_t *buf)
{
	const char *src;

	for (src = buf->base; *src && src < buf->end; src++)
	{
		if (((*src < ' ') ||
		    (*src >= (' ') && *src < '$') ||
		    (*src > '$' && *src < '(') ||
		    (*src > ')' && *src < '+') ||
		    (*src > ':' && *src < '=') ||
		    (*src > '=' && *src < '@') ||
		    (*src > 'Z' && *src < '^') ||
		    (*src == '`') ||
		    (*src > 'z' && *src < '~') ||
		    (*src > '~')))
			return true;
	}

	return false;
}

static inline bool
allowed_fragment(const pkgconf_fragment_t *frag)
{
	return !(!frag->type || frag->data == NULL || strchr("DILl", frag->type) == NULL);
}

static bool
msvc_renderer_render(const pkgconf_fragment_render_ctx_t *ctx, const pkgconf_fragment_t *frag, pkgconf_buffer_t *buf)
{
	pkgconf_buffer_t tmpbuf = PKGCONF_BUFFER_INITIALIZER;
	bool escape = ctx->escape;

	if (!allowed_fragment(frag))
		return true;

	switch(frag->type) {
	case 'D':
	case 'I':
		if (!pkgconf_buffer_push_byte(buf, '/') || !pkgconf_buffer_push_byte(buf, frag->type))
			return false;
		break;
	case 'L':
		if (!pkgconf_buffer_append(buf, "/libpath:"))
			return false;
		break;
	}

	if (!pkgconf_buffer_append(&tmpbuf, frag->data))
		return false;

	if (frag->type == 'l' && !pkgconf_buffer_append(&tmpbuf, ".lib"))
		goto fail;

	escape = should_quote(&tmpbuf);

	if (escape && !pkgconf_buffer_push_byte(buf, '"'))
		goto fail;

	if (!pkgconf_buffer_append(buf, pkgconf_buffer_str_or_empty(&tmpbuf)))
		goto fail;

	if (escape && !pkgconf_buffer_push_byte(buf, '"'))
		goto fail;

	pkgconf_buffer_finalize(&tmpbuf);
	return true;

fail:
	pkgconf_buffer_finalize(&tmpbuf);
	return false;
}

static pkgconf_fragment_render_ops_t msvc_renderer_ops = {
	.render = msvc_renderer_render
};

pkgconf_fragment_render_ops_t *
msvc_renderer_get(void)
{
	return &msvc_renderer_ops;
}
