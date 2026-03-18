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

#if 0
static void
buffer_debug(pkgconf_buffer_t *buffer)
{
	for (char *c = buffer->base; c <= buffer->end; c++) {
		fprintf(stderr, "%02x ", (unsigned char) *c);
	}

	fprintf(stderr, "\n");
}
#endif

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
	buffer->end = newend + (needed - 1);

	*buffer->end = '\0';
}

void
pkgconf_buffer_append_vfmt(pkgconf_buffer_t *buffer, const char *fmt, va_list src_va)
{
	va_list va;
	char *buf;
	size_t needed;

	va_copy(va, src_va);
	needed = vsnprintf(NULL, 0, fmt, va) + 1;
	va_end(va);

	buf = malloc(needed);

	va_copy(va, src_va);
	vsnprintf(buf, needed, fmt, va);
	va_end(va);

	pkgconf_buffer_append(buffer, buf);

	free(buf);
}

void
pkgconf_buffer_append_fmt(pkgconf_buffer_t *buffer, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	pkgconf_buffer_append_vfmt(buffer, fmt, va);
	va_end(va);
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
	buffer->base = buffer->end = NULL;
}

void
pkgconf_buffer_fputs(pkgconf_buffer_t *buffer, FILE *out)
{
	if (pkgconf_buffer_len(buffer) != 0)
		fputs(pkgconf_buffer_str(buffer), out);

	fputc('\n', out);
}

void
pkgconf_buffer_vjoin(pkgconf_buffer_t *buffer, char delim, va_list src_va)
{
	va_list va;
	const char *arg;

	va_copy(va, src_va);

	while ((arg = va_arg(va, const char *)) != NULL)
	{
		if (pkgconf_buffer_str(buffer) != NULL)
			pkgconf_buffer_push_byte(buffer, delim);

		pkgconf_buffer_append(buffer, arg);
	}

	va_end(va);
}

// NOTE: due to C's rules regarding promotion in variable args and permissible variables, delim must
// be an int here.
void
pkgconf_buffer_join(pkgconf_buffer_t *buffer, int delim, ...)
{
	va_list va;

	va_start(va, delim);
	pkgconf_buffer_vjoin(buffer, (char)delim, va);
	va_end(va);
}

bool
pkgconf_buffer_contains(const pkgconf_buffer_t *haystack, const pkgconf_buffer_t *needle)
{
	const char *haystack_str = pkgconf_buffer_str_or_empty(haystack);
	const char *needle_str = pkgconf_buffer_str_or_empty(needle);

	return strstr(haystack_str, needle_str) != NULL;
}

bool
pkgconf_buffer_contains_byte(const pkgconf_buffer_t *haystack, char needle)
{
	const char *haystack_str = pkgconf_buffer_str_or_empty(haystack);
	return strchr(haystack_str, needle) != NULL;
}

bool
pkgconf_buffer_match(const pkgconf_buffer_t *haystack, const pkgconf_buffer_t *needle)
{
	const char *haystack_str = pkgconf_buffer_str_or_empty(haystack);
	const char *needle_str = pkgconf_buffer_str_or_empty(needle);

	if (pkgconf_buffer_len(haystack) != pkgconf_buffer_len(needle))
		return false;

	return memcmp(haystack_str, needle_str, pkgconf_buffer_len(haystack)) == 0;
}

void
pkgconf_buffer_subst(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src, const char *pattern, const char *value)
{
	const char *iter = src->base;
	size_t pattern_len = strlen(pattern);

	if (!pkgconf_buffer_len(src))
		return;

	if (!pattern_len)
	{
		pkgconf_buffer_append(dest, pkgconf_buffer_str(src));
		return;
	}

	while (iter < src->end)
	{
		if ((size_t)(src->end - iter) >= pattern_len && !memcmp(iter, pattern, pattern_len))
		{
			pkgconf_buffer_append(dest, value);
			iter += pattern_len;
		}
		else
			pkgconf_buffer_push_byte(dest, *iter++);
	}
}

void
pkgconf_buffer_escape(pkgconf_buffer_t *dest, const pkgconf_buffer_t *src, const pkgconf_span_t *spans, size_t nspans)
{
	const char *p = pkgconf_buffer_str(src);

	if (!pkgconf_buffer_len(src))
		return;

	for (; *p; p++)
	{
		if (pkgconf_span_contains((unsigned char) *p, spans, nspans))
			pkgconf_buffer_push_byte(dest, '\\');

		pkgconf_buffer_push_byte(dest, *p);
	}
}
