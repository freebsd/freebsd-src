/*
 * parser.c
 * rfc822 message parser
 *
 * Copyright (c) 2018, 2025 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/config.h>
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

static void
pkgconf_parser_canonicalize_line(pkgconf_buffer_t *buffer)
{
	bool escaped = false;
	char *buf = buffer->base;
	char *src = buf, *dst = buf;

	if (buf == NULL)
		return;

	for (; *src != '\0'; src++)
	{
		if (*src == '\\' && !escaped)
		{
			escaped = true;
			continue;
		}

		if (*src == '#' && !escaped)
			break;

		if (escaped)
		{
			if (*src == '#')
			{
				*dst++ = '#';
				escaped = false;
				continue;
			}

			*dst++ = '\\';
			escaped = false;
		}

		*dst++ = *src;
	}

	if (escaped)
		*dst++ = '\\';

	*dst = '\0';
	buffer->end = dst;
}

void
pkgconf_parser_parse_buffer(void *data, const pkgconf_parser_operand_func_t *ops, const pkgconf_parser_warn_func_t warnfunc, pkgconf_buffer_t *buffer, const char *warnprefix)
{
	char op, *p, *key, *value;
	size_t vallen;

	pkgconf_parser_canonicalize_line(buffer);

	p = buffer->base;
	if (p == NULL)
		return;
	while (*p && isspace((unsigned char)*p))
		p++;
	if (*p && p != buffer->base)
	{
		warnfunc(data, "%s: warning: whitespace encountered while parsing key section\n",
			warnprefix);
	}
	key = p;
	while (*p && (isalpha((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_' || *p == '.'))
		p++;

	if (!isalpha((unsigned char)*key) &&
	    !isdigit((unsigned char)*p))
		return;

	while (*p && isspace((unsigned char)*p))
	{
		warnfunc(data, "%s: warning: whitespace encountered while parsing key section\n",
			warnprefix);

		/* set to null to avoid trailing spaces in key */
		*p = '\0';
		p++;
	}

	op = *p;
	if (*p != '\0')
	{
		*p = '\0';
		p++;
	}

	while (*p && isspace((unsigned char)*p))
		p++;

	value = p;
	vallen = strlen(value);
	if (vallen)
		p = value + (vallen - 1);

	while (*p && isspace((unsigned char) *p) && p > value)
	{
		if (op == '=')
		{
			warnfunc(data, "%s: warning: trailing whitespace encountered while parsing value section\n",
				warnprefix);
		}

		*p = '\0';
		p--;
	}

	if (ops[(unsigned char) op])
		ops[(unsigned char) op](data, warnprefix, key, value);
}

void
pkgconf_parser_parse(FILE *f, void *data, const pkgconf_parser_operand_func_t *ops, const pkgconf_parser_warn_func_t warnfunc, const char *filename)
{
	pkgconf_buffer_t readbuf = PKGCONF_BUFFER_INITIALIZER;
	size_t lineno = 0;
	bool continue_reading = true;

	while (continue_reading)
	{
		char warnprefix[PKGCONF_ITEM_SIZE];

		continue_reading = pkgconf_fgetline(&readbuf, f);
		lineno++;

		snprintf(warnprefix, sizeof warnprefix, "%s:" SIZE_FMT_SPECIFIER, filename, lineno);
		pkgconf_parser_parse_buffer(data, ops, warnfunc, &readbuf, warnprefix);
		pkgconf_buffer_reset(&readbuf);
	}

	pkgconf_buffer_finalize(&readbuf);
}
