/*
 * alloc-inject.c
 * allocator fault injection for fuzzing harnesses
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <stddef.h>
#include "alloc-inject.h"

/* the library's allocator calls are redirected here via -Wl,--wrap, so __real_*
 * reaches the real (sanitizer-instrumented) allocator.  injection is only active
 * while armed, which a harness does exclusively around the code under test.
 */
static bool alloc_armed = false;
static unsigned long alloc_seen = 0;
static unsigned long alloc_fail_at = 0;
static bool alloc_fired = false;

void *__real_malloc(size_t size);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);
void *__real_reallocarray(void *ptr, size_t nmemb, size_t size);
char *__real_strdup(const char *s);
char *__real_strndup(const char *s, size_t n);

void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t nmemb, size_t size);
void *__wrap_realloc(void *ptr, size_t size);
void *__wrap_reallocarray(void *ptr, size_t nmemb, size_t size);
char *__wrap_strdup(const char *s);
char *__wrap_strndup(const char *s, size_t n);

void
alloc_inject_arm(unsigned long fail_at)
{
	alloc_seen = 0;
	alloc_fail_at = fail_at;
	alloc_fired = false;
	alloc_armed = true;
}

void
alloc_inject_disarm(void)
{
	alloc_armed = false;
}

bool
alloc_inject_fired(void)
{
	return alloc_fired;
}

static bool
alloc_should_fail(void)
{
	if (!alloc_armed)
		return false;

	if (++alloc_seen != alloc_fail_at)
		return false;

	alloc_fired = true;
	return true;
}

void *
__wrap_malloc(size_t size)
{
	if (alloc_should_fail())
		return NULL;

	return __real_malloc(size);
}

void *
__wrap_calloc(size_t nmemb, size_t size)
{
	if (alloc_should_fail())
		return NULL;

	return __real_calloc(nmemb, size);
}

void *
__wrap_realloc(void *ptr, size_t size)
{
	if (alloc_should_fail())
		return NULL;

	return __real_realloc(ptr, size);
}

void *
__wrap_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if (alloc_should_fail())
		return NULL;

	return __real_reallocarray(ptr, nmemb, size);
}

char *
__wrap_strdup(const char *s)
{
	if (alloc_should_fail())
		return NULL;

	return __real_strdup(s);
}

char *
__wrap_strndup(const char *s, size_t n)
{
	if (alloc_should_fail())
		return NULL;

	return __real_strndup(s, n);
}
