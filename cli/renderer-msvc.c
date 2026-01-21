/*
 * renderer-msvc.c
 * MSVC library syntax renderer
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
fragment_should_quote(const pkgconf_fragment_t *frag)
{
	const char *src;

	if (frag->data == NULL)
		return false;

	for (src = frag->data; *src; src++)
	{
		if (((*src < ' ') ||
		    (*src >= (' ' + (frag->children.head != NULL ? 1 : 0)) && *src < '$') ||
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

static inline size_t
fragment_len(const pkgconf_fragment_t *frag)
{
	size_t len = 1;

	if (frag->type)
		len += 2;

	if (frag->data != NULL)
	{
		len += strlen(frag->data);

		if (fragment_should_quote(frag))
			len += 2;
	}

	return len;
}

static inline bool
allowed_fragment(const pkgconf_fragment_t *frag)
{
	return !(!frag->type || frag->data == NULL || strchr("DILl", frag->type) == NULL);
}

static size_t
msvc_renderer_render_len(const pkgconf_list_t *list, bool escape)
{
	(void) escape;

	size_t out = 1;		/* trailing nul */
	pkgconf_node_t *node;

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		const pkgconf_fragment_t *frag = node->data;

		if (!allowed_fragment(frag))
			continue;

		switch (frag->type)
		{
			case 'L':
				out += 9; /* "/libpath:" */
				break;
			case 'l':
				out += 4; /* ".lib" */
				break;
			default:
				break;
		}

		out += fragment_len(frag);
	}

	return out;
}

static void
msvc_renderer_render_buf(const pkgconf_list_t *list, char *buf, size_t buflen, bool escape)
{
	pkgconf_node_t *node;
	char *bptr = buf;

	memset(buf, 0, buflen);

	PKGCONF_FOREACH_LIST_ENTRY(list->head, node)
	{
		const pkgconf_fragment_t *frag = node->data;
		size_t buf_remaining = buflen - (bptr - buf);
		size_t cnt;

		if (!allowed_fragment(frag))
			continue;

		if (fragment_len(frag) > buf_remaining)
			break;

		switch(frag->type) {
		case 'D':
		case 'I':
			*bptr++ = '/';
			*bptr++ = frag->type;
			break;
		case 'L':
			cnt = pkgconf_strlcpy(bptr, "/libpath:", buf_remaining);
			bptr += cnt;
			buf_remaining -= cnt;
			break;
		}

		escape = fragment_should_quote(frag);

		if (escape)
			*bptr++ = '"';

		cnt = pkgconf_strlcpy(bptr, frag->data, buf_remaining);
		bptr += cnt;
		buf_remaining -= cnt;

		if (frag->type == 'l')
		{
			cnt = pkgconf_strlcpy(bptr, ".lib", buf_remaining);
			bptr += cnt;
		}

		if (escape)
			*bptr++ = '"';

		*bptr++ = ' ';
	}

	*bptr = '\0';
}

static const pkgconf_fragment_render_ops_t msvc_renderer_ops = {
	.render_len = msvc_renderer_render_len,
	.render_buf = msvc_renderer_render_buf
};

const pkgconf_fragment_render_ops_t *
msvc_renderer_get(void)
{
	return &msvc_renderer_ops;
}
