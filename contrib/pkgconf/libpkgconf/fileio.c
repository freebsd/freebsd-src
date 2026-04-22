/*
 * fileio.c
 * File reading utilities
 *
 * Copyright (c) 2012, 2025 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

bool
pkgconf_fgetline(pkgconf_buffer_t *buffer, FILE *stream)
{
	bool quoted = false;
	int c = '\0', c2;

	while ((c = getc(stream)) != EOF)
	{
		if (c == '\\' && !quoted)
		{
			quoted = true;
			continue;
		}
		else if (c == '#')
		{
			if (!quoted) {
				/* Skip the rest of the line */
				do {
					c = getc(stream);
				} while (c != '\n' && c != EOF);
				pkgconf_buffer_push_byte(buffer, c);
				break;
			}
			else
				pkgconf_buffer_push_byte(buffer, c);

			quoted = false;
			continue;
		}
		else if (c == '\n')
		{
			if (quoted)
			{
				/* Trim spaces */
				do {
					c2 = getc(stream);
				} while (c2 == '\t' || c2 == ' ');

				ungetc(c2, stream);

				quoted = false;
				continue;
			}
			else
			{
				pkgconf_buffer_push_byte(buffer, c);
			}

			break;
		}
		else if (c == '\r')
		{
			pkgconf_buffer_push_byte(buffer, '\n');

			if ((c2 = getc(stream)) == '\n')
			{
				if (quoted)
				{
					quoted = false;
					continue;
				}

				break;
			}

			ungetc(c2, stream);

			if (quoted)
			{
				quoted = false;
				continue;
			}

			break;
		}
		else
		{
			if (quoted) {
				pkgconf_buffer_push_byte(buffer, '\\');
				quoted = false;
			}
			pkgconf_buffer_push_byte(buffer, c);
		}

	}

	/* Remove newline character. */
	if (pkgconf_buffer_lastc(buffer) == '\n')
		pkgconf_buffer_trim_byte(buffer);

	if (pkgconf_buffer_lastc(buffer) == '\r')
		pkgconf_buffer_trim_byte(buffer);

	return !(c == EOF || ferror(stream));
}
