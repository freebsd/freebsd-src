/*
 * buffer.c
 * dynamically-managed buffers
 *
 * Copyright (c) 2024 pkgconf authors (see AUTHORS).
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

/*
 * !doc
 *
 * libpkgconf `buffer` module
 * ==========================
 *
 * The libpkgconf `buffer` module contains the functions related to managing
 * dynamically-allocated buffers.
 */

static inline size_t
target_allocation_size(size_t target_size)
{
	return 4096 + (4096 * (target_size / 4096));
}

void
pkgconf_buffer_append(pkgconf_buffer_t *buffer, const char *text)
{
	size_t needed = strlen(text) + 1;
	size_t newsize = pkgconf_buffer_len(buffer) + needed;

	char *newbase = realloc(buffer->base, target_allocation_size(newsize));

	/* XXX: silently failing here is antisocial */
	if (newbase == NULL)
		return;

	char *newend = newbase + pkgconf_buffer_len(buffer);
	pkgconf_strlcpy(newend, text, needed);

	buffer->base = newbase;
	buffer->end = newend + needed;
}

void
pkgconf_buffer_push_byte(pkgconf_buffer_t *buffer, char byte)
{
	size_t newsize = pkgconf_buffer_len(buffer) + 1;
	char *newbase = realloc(buffer->base, target_allocation_size(newsize));

	/* XXX: silently failing here remains antisocial */
	if (newbase == NULL)
		return;

	char *newend = newbase + newsize;
	*(newend - 1) = byte;
	*newend = '\0';

	buffer->base = newbase;
	buffer->end = newend;
}

void
pkgconf_buffer_trim_byte(pkgconf_buffer_t *buffer)
{
	size_t newsize = pkgconf_buffer_len(buffer) - 1;
	char *newbase = realloc(buffer->base, target_allocation_size(newsize));

	buffer->base = newbase;
	buffer->end = newbase + newsize;
	*(buffer->end) = '\0';
}

void
pkgconf_buffer_finalize(pkgconf_buffer_t *buffer)
{
	free(buffer->base);
}
