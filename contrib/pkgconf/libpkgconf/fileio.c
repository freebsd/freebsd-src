/*
 * fileio.c
 * File reading utilities
 *
 * SPDX-License-Identifier: pkgconf
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

#include <libpkgconf/config.h>
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

#if HAVE_DECL_GETC_UNLOCKED
# define pkgconf_getc(stream) getc_unlocked(stream)
#else
# define pkgconf_getc(stream) getc(stream)
#endif

bool
pkgconf_fgetline(pkgconf_buffer_t *buffer, FILE *stream)
{
	bool quoted = false;
	bool got_data = false;
	char run[256];
	size_t runlen = 0;
	int c;

	while ((c = pkgconf_getc(stream)) != EOF)
	{
		got_data = true;

		if (c == '\\' && !quoted)
		{
			quoted = true;
			continue;
		}

		if (c == '\n')
		{
			if (quoted)
			{
				quoted = false;
				continue;
			}

			break;
		}

		if (c == '\r')
		{
			int next = pkgconf_getc(stream);

			if (next != '\n' && next != EOF && ungetc(next, stream) == EOF)
				return false;

			if (quoted)
			{
				quoted = false;
				continue;
			}

			break;
		}

		if (runlen > sizeof run - 2)
		{
			if (!pkgconf_buffer_append_slice(buffer, run, runlen))
				return false;

			runlen = 0;
		}

		if (quoted)
		{
			run[runlen++] = '\\';
			quoted = false;
		}

		run[runlen++] = (char) c;
	}

	if (runlen != 0 && !pkgconf_buffer_append_slice(buffer, run, runlen))
		return false;

	return got_data && !ferror(stream);
}
