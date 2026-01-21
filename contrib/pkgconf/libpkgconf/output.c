/*
 * output.c
 * I/O abstraction layer
 *
 * Copyright (c) 2025 pkgconf authors (see AUTHORS).
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
pkgconf_output_putbuf(pkgconf_output_t *output, pkgconf_output_stream_t stream, const pkgconf_buffer_t *buffer, bool newline)
{
	bool ret;
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	if (pkgconf_buffer_len(buffer) != 0)
		pkgconf_buffer_append(&buf, pkgconf_buffer_str(buffer));

	if (newline)
		pkgconf_buffer_push_byte(&buf, '\n');

	ret = output->write(output, stream, &buf);
	pkgconf_buffer_finalize(&buf);

	return ret;
}

bool
pkgconf_output_puts(pkgconf_output_t *output, pkgconf_output_stream_t stream, const char *str)
{
	bool ret;
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, str);
	pkgconf_buffer_push_byte(&buf, '\n');
	ret = output->write(output, stream, &buf);
	pkgconf_buffer_finalize(&buf);

	return ret;
}

bool
pkgconf_output_fmt(pkgconf_output_t *output, pkgconf_output_stream_t stream, const char *fmt, ...)
{
	bool ret;
	va_list va;

	va_start(va, fmt);
	ret = pkgconf_output_vfmt(output, stream, fmt, va);
	va_end(va);

	return ret;
}

bool
pkgconf_output_vfmt(pkgconf_output_t *output, pkgconf_output_stream_t stream, const char *fmt, va_list src_va)
{
	va_list va;
	bool ret;
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	va_copy(va, src_va);
	pkgconf_buffer_append_vfmt(&buf, fmt, va);
	va_end(va);

	ret = output->write(output, stream, &buf);
	pkgconf_buffer_finalize(&buf);

	return ret;
}

static bool
pkgconf_output_stdio_write(pkgconf_output_t *output, pkgconf_output_stream_t stream, const pkgconf_buffer_t *buffer)
{
	(void) output;

	FILE *target = stream == PKGCONF_OUTPUT_STDERR ? stderr : stdout;

	if (buffer != NULL)
	{
		const char *str = pkgconf_buffer_str(buffer);
		size_t size = pkgconf_buffer_len(buffer);

		if (size > 0 && !fwrite(str, size, 1, target))
			return false;
	}

	fflush(target);
	return true;
}

static pkgconf_output_t pkgconf_default_output = {
	.privdata = NULL,
	.write = pkgconf_output_stdio_write,
};

pkgconf_output_t *
pkgconf_output_default(void)
{
	return &pkgconf_default_output;
}
