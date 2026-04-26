/*
 * parser.c
 * rfc822 message parser
 *
 * Copyright (c) 2018 pkgconf authors (see AUTHORS).
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

/*
 * !doc
 *
 * .. c:function:: pkgconf_pkg_t *pkgconf_pkg_new_from_file(const pkgconf_client_t *client, const char *filename, FILE *f)
 *
 *    Parse a .pc file into a pkgconf_pkg_t object structure.
 *
 *    :param pkgconf_client_t* client: The pkgconf client object to use for dependency resolution.
 *    :param char* filename: The filename of the package file (including full path).
 *    :param FILE* f: The file object to read from.
 *    :returns: A ``pkgconf_pkg_t`` object which contains the package data.
 *    :rtype: pkgconf_pkg_t *
 */
void
pkgconf_parser_parse(FILE *f, void *data, const pkgconf_parser_operand_func_t *ops, const pkgconf_parser_warn_func_t warnfunc, const char *filename)
{
	pkgconf_buffer_t readbuf = PKGCONF_BUFFER_INITIALIZER;
	size_t lineno = 0;
	bool continue_reading = true;

	while (continue_reading)
	{
		char op, *p, *key, *value;
		bool warned_key_whitespace = false, warned_value_whitespace = false;

		continue_reading = pkgconf_fgetline(&readbuf, f);
		lineno++;

		p = readbuf.base;
		if (p == NULL)
			continue;
		while (*p && isspace((unsigned char)*p))
			p++;
		if (*p && p != readbuf.base)
		{
			warnfunc(data, "%s:" SIZE_FMT_SPECIFIER ": warning: whitespace encountered while parsing key section\n",
				filename, lineno);
			warned_key_whitespace = true;
		}
		key = p;
		while (*p && (isalpha((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_' || *p == '.'))
			p++;

		if (!isalpha((unsigned char)*key) &&
		    !isdigit((unsigned char)*p))
		{
			pkgconf_buffer_reset(&readbuf);
			continue;
		}

		while (*p && isspace((unsigned char)*p))
		{
			if (!warned_key_whitespace)
			{
				warnfunc(data, "%s:" SIZE_FMT_SPECIFIER ": warning: whitespace encountered while parsing key section\n",
					filename, lineno);
				warned_key_whitespace = true;
			}

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
		p = value + (strlen(value) - 1);
		while (*p && isspace((unsigned char) *p) && p > value)
		{
			if (!warned_value_whitespace && op == '=')
			{
				warnfunc(data, "%s:" SIZE_FMT_SPECIFIER ": warning: trailing whitespace encountered while parsing value section\n",
					filename, lineno);
				warned_value_whitespace = true;
			}

			*p = '\0';
			p--;
		}
		if (ops[(unsigned char) op])
			ops[(unsigned char) op](data, lineno, key, value);

		pkgconf_buffer_reset(&readbuf);
	}

	pkgconf_buffer_finalize(&readbuf);
}
