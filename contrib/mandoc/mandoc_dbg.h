/* $Id: mandoc_dbg.h,v 1.1 2022/04/14 16:43:44 schwarze Exp $ */
/*
 * Copyright (c) 2021 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

int	  mandoc_dbg_asprintf(const char *, int, char **, const char *, ...)
		__attribute__((__format__ (__printf__, 4, 5)));
void	 *mandoc_dbg_calloc(size_t, size_t, const char *, int);
void	 *mandoc_dbg_malloc(size_t, const char *, int);
void	 *mandoc_dbg_realloc(void *, size_t, const char *, int);
void	 *mandoc_dbg_reallocarray(void *, size_t, size_t,
		const char *, int);
void	 *mandoc_dbg_recallocarray(void *, size_t, size_t, size_t,
		const char *, int);
char	 *mandoc_dbg_strdup(const char *, const char *, int);
char	 *mandoc_dbg_strndup(const char *, size_t, const char *, int);
void	  mandoc_dbg_free(void *, const char *, int);

void	  mandoc_dbg_init(int argc, char *argv[]);
void	  mandoc_dbg_name(const char *);
void	  mandoc_dbg_finish(void);

#ifndef DEBUG_NODEF
#define mandoc_asprintf(dest, fmt, ...) \
	mandoc_dbg_asprintf(__FILE__, __LINE__, (dest), (fmt), __VA_ARGS__)
#define mandoc_calloc(num, size) \
	mandoc_dbg_calloc((num), (size), __FILE__, __LINE__)
#define mandoc_malloc(size) \
	mandoc_dbg_malloc((size), __FILE__, __LINE__)
#define mandoc_realloc(ptr, size) \
	mandoc_dbg_realloc((ptr), (size), __FILE__, __LINE__)
#define mandoc_reallocarray(ptr, num, size) \
	mandoc_dbg_reallocarray((ptr), (num), (size), __FILE__, __LINE__)
#define mandoc_recallocarray(ptr, old, num, size) \
	mandoc_dbg_recallocarray((ptr), (old), (num), (size), \
	__FILE__, __LINE__)
#define mandoc_strdup(ptr) \
	mandoc_dbg_strdup((ptr), __FILE__, __LINE__)
#define mandoc_strndup(ptr, size) \
	mandoc_dbg_strndup((ptr), (size), __FILE__, __LINE__)
#define free(ptr) \
	mandoc_dbg_free((ptr), __FILE__, __LINE__)
#endif
