/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Index chains for the ucl target. Under an array a dotted component
 * is an index (name.i, name.i.j, name.-1); name.# reads the count and
 * name.#=value appends. Under an object the same component is a key.
 */


#include <ctype.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/*
 * The index `idx' against an array of `size' elements: a negative
 * index counts from the end, UCL_INDEX_APPEND is one past the end.
 * Returns -1 when the index falls before the first element.
 */
long
ucl_resolve_index(long idx, unsigned int size)
{

	if (idx == UCL_INDEX_APPEND)
		return ((long)size);
	if (idx >= 0)
		return (idx);
	if (idx == UCL_INDEX_COUNT || (long)size + idx < 0)
		return (-1);
	return ((long)size + idx);
}

/*
 * The resolved form of an indexed request cut after `depth' indices
 * (`BAR.0' for depth 1); an index not yet resolved shows as typed.
 * Returns newly allocated storage.
 */
char *
ucl_index_label(const struct request *req, size_t depth)
{
	char *label;
	char *tmp;
	size_t i;
	int rv;

	if ((label = strdup(req->ucl_base)) == NULL)
		err(EXIT_FAILURE, NULL);
	for (i = 0; i < depth; i++) {
		if (req->ucl_idx[i] == UCL_INDEX_COUNT)
			break;
		if (req->ucl_res[i] >= 0)
			rv = asprintf(&tmp, "%s.%ld", label, req->ucl_res[i]);
		else if (req->ucl_idx[i] == UCL_INDEX_APPEND)
			rv = asprintf(&tmp, "%s.#", label);
		else
			rv = asprintf(&tmp, "%s.%ld", label, req->ucl_idx[i]);
		if (rv < 0)
			err(EXIT_FAILURE, NULL);
		free(label);
		label = tmp;
	}
	return (label);
}

/* Show the request under its resolved indices from here on */
void
ucl_relabel(struct request *req)
{
	char *label;

	label = ucl_index_label(req, req->ucl_nidx);
	if (req->name == req->ucl_label)
		req->name = label;
	free(req->ucl_label);
	req->ucl_label = label;
}

/*
 * The scan found an array at `base'. A request whose path runs on past
 * it, `base.i', `base.i.j', or `base.i.#', is an index chain; split it
 * once into the array's path (which the scan then matches) and its
 * indices. ucl_index_requests() puts the typed form back for the
 * report. A component that is not a number or `#' is a key, and the
 * request is left to read as unknown. A negative index counts from
 * the end; `#' is the count on a read and one past the end on a set.
 */
void
ucl_split_index(const char *base)
{
	struct request *req;
	const char *p;
	char *end;
	long *idx;
	size_t blen;
	size_t d;
	size_t n;
	unsigned int r;

	blen = strlen(base);
	for (r = 0; r < nreqs; r++) {
		req = &reqs[r];
		if (req->ucl_base != NULL ||
		    strncmp(req->name, base, blen) != 0 ||
		    req->name[blen] != '.')
			continue;
		if ((idx = calloc(strlen(req->name + blen) / 2 + 1,
		    sizeof(*idx))) == NULL)
			err(EXIT_FAILURE, NULL);
		n = 0;
		p = req->name + blen;
		while (*p == '.') {
			if (p[1] == '#' && p[2] == '\0') {
				idx[n++] = UCL_INDEX_COUNT;
				p += 2;
				break;
			}
			if (!isdigit((unsigned char)p[1]) &&
			    !(p[1] == '-' && isdigit((unsigned char)p[2])))
				break;
			/*
			 * libucl counts elements in unsigned int; past
			 * that it is no index (and LONG_MIN and its
			 * successor are the count and append markers).
			 */
			errno = 0;
			idx[n] = strtol(p + 1, &end, 10);
			if (errno == ERANGE || idx[n] <= UCL_INDEX_APPEND ||
			    (idx[n] < 0 ? -(unsigned long)idx[n] :
			    (unsigned long)idx[n]) > UINT_MAX)
				break;
			n++;
			p = end;
		}
		if (*p != '\0') {
			free(idx);
			continue;
		}
		if (idx[n - 1] == UCL_INDEX_COUNT) {
			if (req->remove)
				errx(EXIT_FAILURE,
				    "%s: the count cannot be removed",
				    req->name);
			if (req->listop != '\0')
				errx(EXIT_FAILURE,
				    "%s: the count cannot be edited",
				    req->name);
			if (req->newvalue != NULL)
				idx[n - 1] = UCL_INDEX_APPEND;
		}
		if ((req->ucl_base = strdup(base)) == NULL)
			err(EXIT_FAILURE, NULL);
		req->ucl_idx = idx;
		req->ucl_nidx = n;
		if ((req->ucl_res = calloc(n, sizeof(*idx))) == NULL)
			err(EXIT_FAILURE, NULL);
		for (d = 0; d < n; d++)
			req->ucl_res[d] = -1;
		req->ucl_label = ucl_index_label(req, n);
		req->name = req->ucl_base;
	}
}

/*
 * Keep the value as typed (for the file) and rewrite the request's
 * value in the library's rendering so `-c' and the write echo compare
 * like with like (`enabled=yes' against a file that reads back `true').
 * Runs at the start of every scan; indexed requests go back to matching
 * on the array's path.
 */
void
ucl_canon_requests(void)
{
	ucl_object_t *val;
	unsigned int n;

	for (n = 0; n < nreqs; n++) {
		if (reqs[n].ucl_base != NULL)
			reqs[n].name = reqs[n].ucl_base;
		if (reqs[n].newvalue == NULL || reqs[n].remove ||
		    reqs[n].listop != '\0' || reqs[n].rawvalue != NULL)
			continue;
		reqs[n].rawvalue = reqs[n].newvalue;
		val = ucl_value(reqs[n].newvalue);
		reqs[n].newvalue = ucl_render(val);
		ucl_object_unref(val);
	}
}

/*
 * After a scan, turn the array an indexed request matched into the one
 * element (or the count) asked for, and show the request as typed. An
 * index past the end is an unknown directive; a value that is not an
 * array cannot be indexed.
 */
void
ucl_index_requests(void)
{
	struct request *req;
	ucl_object_t *top;
	const ucl_object_t *arr;
	const ucl_object_t *elem;
	char *label;
	char *v;
	size_t d;
	long i;
	unsigned int n;

	for (n = 0; n < nreqs; n++) {
		req = &reqs[n];
		if (req->ucl_base == NULL)
			continue;
		/* -V shows the files' assignments, which are of the array */
		if (!verbose_trail)
			req->name = req->ucl_label;
		if (!req->found)
			continue;
		top = ucl_value(req->value);
		arr = top;
		v = NULL;
		for (d = 0; d < req->ucl_nidx; d++) {
			if (ucl_object_type(arr) != UCL_ARRAY) {
				label = ucl_index_label(req, d);
				errx(EXIT_FAILURE, "%s: not an array", label);
			}
			if (req->ucl_idx[d] == UCL_INDEX_COUNT) {
				if (asprintf(&v, "%u", ucl_array_size(arr)) < 0)
					err(EXIT_FAILURE, NULL);
				break;
			}
			i = ucl_resolve_index(req->ucl_idx[d],
			    ucl_array_size(arr));
			if (i < 0)
				break;
			req->ucl_res[d] = i;
			elem = ucl_array_find_index(arr, (unsigned int)i);
			if (elem == NULL) {
				if (d + 1 == req->ucl_nidx &&
				    req->newvalue != NULL && !req->remove &&
				    (unsigned int)i == ucl_array_size(arr)) {
					/* One past the end: the set appends */
					if ((v = strdup("")) == NULL)
						err(EXIT_FAILURE, NULL);
				}
				break;
			}
			if (d + 1 == req->ucl_nidx)
				v = ucl_render(elem);
			arr = elem;
		}
		if (v != NULL)
			ucl_relabel(req);
		if (v == NULL) {
			/* Out of range: nothing to read, set, or remove */
			req->name = req->ucl_label;
			req->found = 0;
			req->srcidx = -1;
			memset(req->infile, 0, nconf_files);
		}
		ucl_object_unref(top);
		free(req->value);
		req->value = v;
	}
}

