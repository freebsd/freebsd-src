/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */

#include "bsdtar_platform.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "bsdtar.h"
#include "lafe_err.h"

struct creation_set {
	char		 *create_format;
	struct filter_set {
		int	  program;	/* Set 1 if filter is a program name */
		char	 *filter_name;
	}		 *filters;
	int		  filter_count;
};

struct suffix_codes_t {
	const char *suffix;
	const char *format;
	const char *filter;
};

static int
get_filter_format_codes(const char *suffix, const char **format,
    const char **filter)
{
	/* Resolver table of suffix to format and filter. */
	static const struct suffix_codes_t table[] = {
		{ ".7z",		"7zip",		NULL },
		{ ".ar",		"arbsd",	NULL },
		{ ".bz2",		NULL,		"bzip2" },
		{ ".cpio",		"cpio",		NULL },
		{ ".grz",		NULL,		"grzip" },
		{ ".gz",		NULL,		"gzip" },
		{ ".iso",		"iso9660",	NULL },
		{ ".lrz",		NULL,		"lrzip" },
		{ ".lz4",		NULL,		"lz4" },
		{ ".lzma",		NULL,		"lzma" },
		{ ".lz",		NULL,		"lzip" },
		{ ".lzo",		NULL,		"lzop" },
		{ ".mtree",		"mtree",	NULL },
		{ ".shar",		"shar",		NULL },
		{ ".tar",		"paxr",		NULL },
		{ ".taZ",		"paxr",		"compress" },
		{ ".taz",		"paxr",		"gzip" },
		{ ".tbz2",		"paxr",		"bzip2" },
		{ ".tbz",		"paxr",		"bzip2" },
		{ ".tgz",		"paxr",		"gzip" },
		{ ".tlz",		"paxr",		"lzma" },
		{ ".txz",		"paxr",		"xz" },
		{ ".tz2",		"paxr",		"bzip2" },
		{ ".tzo",		"paxr",		"lzop" },
		{ ".tZ",		"paxr",		"compress" },
		{ ".tzst",		"paxr",		"zstd" },
		{ ".uu",		NULL,		"uuencode" },
		{ ".warc",		"warc",		NULL },
		{ ".xar",		"xar",		NULL },
		{ ".xz",		NULL,		"xz" },
		{ ".zip",		"zip",		NULL },
		{ ".Z",			NULL,		"compress" },
		{ ".zst",		NULL,		"zstd"},
		{ NULL,			NULL,		NULL }
	};
	size_t i;

	for (i = 0; table[i].suffix != NULL; i++) {
		if (strcmp(table[i].suffix, suffix) == 0) {
			*format = table[i].format;
			*filter = table[i].filter;
			return (ARCHIVE_OK);
		}
	}

	return (ARCHIVE_EOF);
}

static void
_cset_add_filter(struct creation_set *cset, int program, const char *filter)
{
	struct filter_set *new_ptr;
	char *new_filter;

	new_ptr = realloc(cset->filters,
	    sizeof(*cset->filters) * (cset->filter_count + 1));
	if (new_ptr == NULL)
		lafe_errc(1, 0, "No memory");
	new_filter = strdup(filter);
	if (new_filter == NULL)
		lafe_errc(1, 0, "No memory");
	cset->filters = new_ptr;
	cset->filters[cset->filter_count].program = program;
	cset->filters[cset->filter_count].filter_name = new_filter;
	cset->filter_count++;
}

void
cset_add_filter(struct creation_set *cset, const char *filter)
{
	_cset_add_filter(cset, 0, filter);
}

void
cset_add_filter_program(struct creation_set *cset, const char *filter)
{
	_cset_add_filter(cset, 1, filter);
}

int
cset_read_support_filter_program(struct creation_set *cset, struct archive *a)
{
	int cnt = 0, i;

	for (i = 0; i < cset->filter_count; i++) {
		if (cset->filters[i].program) {
			archive_read_support_filter_program(a,
			    cset->filters[i].filter_name);
			++cnt;
		}
	}
	return (cnt);
}

int
cset_write_add_filters(struct creation_set *cset, struct archive *a,
    const void **filter_name)
{
	int cnt = 0, i, r;

	for (i = 0; i < cset->filter_count; i++) {
		if (cset->filters[i].program)
			r = archive_write_add_filter_program(a,
				cset->filters[i].filter_name);
		else
			r = archive_write_add_filter_by_name(a,
				cset->filters[i].filter_name);
		if (r < ARCHIVE_WARN) {
			*filter_name = cset->filters[i].filter_name;
			return (r);
		}
		++cnt;
	}
	return (cnt);
}

void
cset_set_format(struct creation_set *cset, const char *format)
{
	char *f;

	f = strdup(format);
	if (f == NULL)
		lafe_errc(1, 0, "No memory");
	free(cset->create_format);
	cset->create_format = f;
}

const char *
cset_get_format(struct creation_set *cset)
{
	return (cset->create_format);
}

static void
_cleanup_filters(struct filter_set *filters, int count)
{
	int i;

	for (i = 0; i < count; i++)
		free(filters[i].filter_name);
	free(filters);
}

/*
 * Clean up a creation set.
 */
void
cset_free(struct creation_set *cset)
{
	_cleanup_filters(cset->filters, cset->filter_count);
	free(cset->create_format);
	free(cset);
}

struct creation_set *
cset_new(void)
{
	return calloc(1, sizeof(struct creation_set));
}

/*
 * Build a creation set by a file name suffix.
 */
int
cset_auto_compress(struct creation_set *cset, const char *filename)
{
	struct filter_set *old_filters;
	char *name;
	int old_filter_count;

	name = strdup(filename);
	if (name == NULL)
		lafe_errc(1, 0, "No memory");
	/* Save previous filters. */
	old_filters = cset->filters;
	old_filter_count = cset->filter_count;
	cset->filters = NULL;
	cset->filter_count = 0;

	for (;;) {
		const char *format, *filter;
		char *p;
		int r;

		/* Get the suffix. */
		p = strrchr(name, '.');
		if (p == NULL)
			break;
		/* Resolve suffix to format and filter information. */
		r = get_filter_format_codes(p, &format, &filter);
		if (r != ARCHIVE_OK)
			break;
		if (filter != NULL)
			cset_add_filter(cset, filter);
		if (format != NULL) {
			cset_set_format(cset, format);
			break;
		}
		/* Remove the resolved suffix. */
		*p = '\0';
	}
	free(name);
	if (cset->filters) {
		struct filter_set *v;
		int i, r;

		/* Release previous filters. */
		_cleanup_filters(old_filters, old_filter_count);

		v = malloc(sizeof(*v) * cset->filter_count);
		if (v == NULL)
			lafe_errc(1, 0, "No memory");
		/* Reverse filter sequence. */
		for (i = 0, r = cset->filter_count; r > 0; )
			v[i++] = cset->filters[--r];
		free(cset->filters);
		cset->filters = v;
		return (1);
	} else {
		/* Put previous filters back. */
		cset->filters = old_filters;
		cset->filter_count = old_filter_count;
		return (0);
	}
}
