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
	bool got_data = false;
	char in[PKGCONF_ITEM_SIZE];

	while (fgets(in, sizeof in, stream) != NULL)
	{
		char *p = in;

		got_data = true;

		while (*p != '\0')
		{
			unsigned char c = (unsigned char) *p++;

			if (c == '\\' && !quoted)
			{
				quoted = true;
				continue;
			}
			else if (c == '\n')
			{
				if (quoted)
				{
					quoted = false;
					continue;
				}
				else
					pkgconf_buffer_push_byte(buffer, (char) c);

				goto done;
			}
			else if (c == '\r')
			{
				pkgconf_buffer_push_byte(buffer, '\n');

				if (*p == '\n')
					p++;

				if (quoted)
				{
					quoted = false;
					continue;
				}

				goto done;
			}
			else
			{
				if (quoted)
				{
					pkgconf_buffer_push_byte(buffer, '\\');
					quoted = false;
				}

				pkgconf_buffer_push_byte(buffer, (char) c);
			}
		}
	}

done:
	/* Remove newline character. */
	if (pkgconf_buffer_lastc(buffer) == '\n')
		pkgconf_buffer_trim_byte(buffer);

	if (pkgconf_buffer_lastc(buffer) == '\r')
		pkgconf_buffer_trim_byte(buffer);

	if (!got_data)
		return false;

	return !ferror(stream);
}
