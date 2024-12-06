/* $Id: mandoc_dbg.c,v 1.1 2022/04/14 16:43:44 schwarze Exp $ */
/*
 * Copyright (c) 2021, 2022 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/types.h>

#if HAVE_ERR
#include <err.h>
#endif
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_OHASH
#include <ohash.h>
#else
#include "compat_ohash.h"
#endif

#define DEBUG_NODEF 1
#include "mandoc_aux.h"
#include "mandoc_dbg.h"
#include "mandoc.h"

/* Store information about one allocation. */
struct dhash_entry {
	const char	*file;
	int		 line;
	const char	*func;
	size_t		 num;
	size_t		 size;
	void		*ptr;
};

/* Store information about all allocations. */
static struct ohash	  dhash_table;
static FILE		 *dhash_fp;
static int		  dhash_aflag;
static int		  dhash_fflag;
static int		  dhash_lflag;
static int		  dhash_nflag;
static int		  dhash_sflag;

static	void		 *dhash_alloc(size_t, void *);
static	void		 *dhash_calloc(size_t, size_t, void *);
static	void		  dhash_free(void *, void *);
static	unsigned int	  dhash_slot(void *);
static	void		  dhash_register(const char *, int, const char *,
				size_t, size_t, void *, const char *);
static	void		  dhash_print(struct dhash_entry *);
static	void		  dhash_purge(const char *, int, const char *, void *);


/* *** Debugging wrappers of public API functions. ************************ */

int
mandoc_dbg_asprintf(const char *file, int line,
    char **dest, const char *fmt, ...)
{
	va_list	 ap;
	int	 ret;

	va_start(ap, fmt);
	ret = vasprintf(dest, fmt, ap);
	va_end(ap);

	if (ret == -1)
		err((int)MANDOCLEVEL_SYSERR, NULL);

	dhash_register(file, line, "asprintf", 1, strlen(*dest) + 1,
	    *dest, *dest);

	return ret;
}

void *
mandoc_dbg_calloc(size_t num, size_t size, const char *file, int line)
{
	void *ptr = mandoc_calloc(num, size);
	dhash_register(file, line, "calloc", num, size, ptr, NULL);
	return ptr;
}

void *
mandoc_dbg_malloc(size_t size, const char *file, int line)
{
	void *ptr = mandoc_malloc(size);
	dhash_register(file, line, "malloc", 1, size, ptr, NULL);
	return ptr;
}

void *
mandoc_dbg_realloc(void *ptr, size_t size, const char *file, int line)
{
	dhash_purge(file, line, "realloc", ptr);
	ptr = mandoc_realloc(ptr, size);
	dhash_register(file, line, "realloc", 1, size, ptr, NULL);
	return ptr;
}

void *
mandoc_dbg_reallocarray(void *ptr, size_t num, size_t size,
    const char *file, int line)
{
	dhash_purge(file, line, "reallocarray", ptr);
	ptr = mandoc_reallocarray(ptr, num, size);
	dhash_register(file, line, "reallocarray", num, size, ptr, NULL);
	return ptr;
}

void *
mandoc_dbg_recallocarray(void *ptr, size_t oldnum, size_t num, size_t size,
    const char *file, int line)
{
	dhash_purge(file, line, "recallocarray", ptr);
	ptr = mandoc_recallocarray(ptr, oldnum, num, size);
	dhash_register(file, line, "recallocarray", num, size, ptr, NULL);
	return ptr;
}

char *
mandoc_dbg_strdup(const char *ptr, const char *file, int line)
{
	char *p = mandoc_strdup(ptr);
	dhash_register(file, line, "strdup", 1, strlen(p) + 1, p, ptr);
	return p;
}

char *
mandoc_dbg_strndup(const char *ptr, size_t sz, const char *file, int line)
{
	char *p = mandoc_strndup(ptr, sz);
	dhash_register(file, line, "strndup", 1, strlen(p) + 1, p, NULL);
	return p;
}

void
mandoc_dbg_free(void *ptr, const char *file, int line)
{
	dhash_purge(file, line, "free", ptr);
	free(ptr);
}


/* *** Memory allocation callbacks for the debugging table. *************** */

static void *
dhash_alloc(size_t sz, void *arg)
{
        return malloc(sz);
}

static void *
dhash_calloc(size_t n, size_t sz, void *arg)
{
        return calloc(n, sz);
}

static void
dhash_free(void *p, void *arg)
{
        free(p);
}


/* *** Debugging utility functions. *************************************** */

/* Initialize the debugging table, to be called from the top of main(). */
void
mandoc_dbg_init(int argc, char *argv[])
{
	struct ohash_info	  info;
	char			 *dhash_fn;
	int			  argi;

	info.alloc = dhash_alloc;
	info.calloc = dhash_calloc;
	info.free = dhash_free;
	info.data = NULL;
	info.key_offset = offsetof(struct dhash_entry, ptr);
	ohash_init(&dhash_table, 18, &info);

	dhash_fp = stderr;
	if ((dhash_fn = getenv("DEBUG_MEMORY")) == NULL)
		return;

	dhash_sflag = 1;
	for(;; dhash_fn++) {
		switch (*dhash_fn) {
		case '\0':
			break;
		case 'A':
			dhash_aflag = 1;
			continue;
		case 'F':
			dhash_fflag = 1;
			continue;
		case 'L':
			dhash_lflag = 1;
			continue;
		case 'N':
			dhash_nflag = 1;
			continue;
		case '/':
			if ((dhash_fp = fopen(dhash_fn, "a+e")) == NULL)
				err((int)MANDOCLEVEL_SYSERR, "%s", dhash_fn);
			break;
		default:
			errx((int)MANDOCLEVEL_BADARG,
			    "invalid char '%c' in $DEBUG_MEMORY",
			    *dhash_fn);
		}
		break;
	}
	if (setvbuf(dhash_fp, NULL, _IOLBF, 0) != 0)
		err((int)MANDOCLEVEL_SYSERR, "setvbuf");

	fprintf(dhash_fp, "P %d", getpid());
	for (argi = 0; argi < argc; argi++)
		fprintf(dhash_fp, " [%s]", argv[argi]);
	fprintf(dhash_fp, "\n");
}

void
mandoc_dbg_name(const char *name)
{
	if (dhash_nflag)
		fprintf(dhash_fp, "N %s\n", name);
}

/* Hash a pointer and return the table slot currently used for it. */
static unsigned int
dhash_slot(void *ptr)
{
	const char	*ks, *ke;
	uint32_t	 hv;

	ks = (const char *)&ptr;
	ke = ks + sizeof(ptr);
	hv = ohash_interval(ks, &ke);
	return ohash_lookup_memory(&dhash_table, ks, sizeof(ptr), hv);
}

/* Record one allocation in the debugging table. */
static void
dhash_register(const char *file, int line, const char *func,
    size_t num, size_t size, void *ptr, const char *str)
{
	struct dhash_entry	*e;
	unsigned int		 slot;

	slot = dhash_slot(ptr);
	e = ohash_find(&dhash_table, slot);
	if (dhash_aflag || e != NULL) {
		fprintf(dhash_fp, "A %s:%d %s(%zu, %zu) = %p",
		    file, line, func, num, size, ptr);
		if (str != NULL)
			fprintf(dhash_fp, " \"%s\"", str);
		fprintf(dhash_fp, "\n");
	}
	if (e != NULL) {
		dhash_print(e);
		fprintf(dhash_fp, "E duplicate address %p\n", e->ptr);
		errx((int)MANDOCLEVEL_BADARG, "duplicate address %p", e->ptr);
	}

	if ((e = malloc(sizeof(*e))) == NULL)
		err(1, NULL);
	e->file = file;
	e->line = line;
	e->func = func;
	e->num  = num;
	e->size = size;
	e->ptr  = ptr;

	ohash_insert(&dhash_table, slot, e);
}

/* Remove one allocation from the debugging table. */
static void
dhash_purge(const char *file, int line, const char *func, void *ptr)
{
	struct dhash_entry	*e;
	unsigned int		 slot;

	if (ptr == NULL)
		return;

	if (dhash_fflag)
		fprintf(dhash_fp, "F %s:%d %s(%p)\n", file, line, func, ptr);

	slot = dhash_slot(ptr);
	e = ohash_remove(&dhash_table, slot);
	free(e);
}

/* Pretty-print information about one allocation. */
static void
dhash_print(struct dhash_entry *e)
{
	fprintf(dhash_fp, "L %s:%d %s(%zu, %zu) = %p\n",
	    e->file, e->line, e->func, e->num, e->size, e->ptr);
}

/* Pretty-print information about all active allocations. */
void
mandoc_dbg_finish(void)
{
	struct dhash_entry	*e;
	unsigned int		 errcount, slot;

	errcount = ohash_entries(&dhash_table);
	e = ohash_first(&dhash_table, &slot);
	while (e != NULL) {
		if (dhash_lflag)
			dhash_print(e);
		free(e);
		e = ohash_next(&dhash_table, &slot);
	}
	ohash_delete(&dhash_table);
	if (dhash_sflag)
		fprintf(dhash_fp, "S %u memory leaks found\n", errcount);
	if (dhash_fp != stderr)
		fclose(dhash_fp);
}
