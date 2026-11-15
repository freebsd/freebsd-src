/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Writes for the ucl target. One file at a time: splice each request
 * into the text, then let libucl confirm the result.
 */


#include <ctype.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/*
 * The element `idx[0..nidx)' names under `arr', or NULL if none;
 * `res' receives the indices as resolved.
 */
static const ucl_object_t *
ucl_array_walk(const ucl_object_t *arr, const long *idx, long *res,
    size_t nidx)
{
	size_t d;

	for (d = 0; d < nidx && arr != NULL; d++) {
		if (ucl_object_type(arr) != UCL_ARRAY)
			return (NULL);
		res[d] = ucl_resolve_index(idx[d], ucl_array_size(arr));
		if (res[d] < 0)
			return (NULL);
		arr = ucl_array_find_index(arr, (unsigned int)res[d]);
	}
	return (arr);
}

/*
 * The array `arr' as the indices `idx[0..nidx)' leave it: without the
 * element they name for a removal (`newel' NULL), or with that element
 * set to `newel' (one past the end appends). `res' receives the
 * indices as resolved; `*oldelp' the rendering of the element replaced
 * or removed (NULL when there was none); `*parentp' the parent array,
 * as it was, of the deepest step reached. An index that leads through
 * something other than an array is fatal, named by `label'.
 */
static ucl_object_t *
ucl_array_set(const ucl_object_t *arr, const long *idx, long *res,
    size_t nidx, const ucl_object_t *newel, const char *label,
    char **oldelp, const ucl_object_t **parentp)
{
	ucl_object_t *out;
	ucl_object_t *sub;
	const ucl_object_t *old;
	ucl_object_iter_t it;
	long i = 0;

	if (ucl_object_type(arr) != UCL_ARRAY)
		errx(EXIT_FAILURE, "%s: not an array", label);
	*parentp = arr;
	res[0] = ucl_resolve_index(idx[0], ucl_array_size(arr));
	if ((out = ucl_object_typed_new(UCL_ARRAY)) == NULL ||
	    (it = ucl_object_iterate_new(arr)) == NULL)
		err(EXIT_FAILURE, NULL);
	while ((old = ucl_object_iterate_safe(it, false)) != NULL) {
		if (i++ != res[0]) {
			ucl_array_append(out, ucl_object_copy(old));
			continue;
		}
		if (nidx > 1) {
			sub = ucl_array_set(old, idx + 1, res + 1, nidx - 1,
			    newel, label, oldelp, parentp);
			ucl_array_append(out, sub);
			continue;
		}
		*oldelp = ucl_render(old);
		if (newel != NULL)
			ucl_array_append(out, ucl_object_copy(newel));
	}
	ucl_object_iterate_free(it);
	if (nidx == 1 && newel != NULL && *oldelp == NULL && i == res[0])
		ucl_array_append(out, ucl_object_copy(newel));
	return (out);
}

/*
 * do_writes() for the ucl target. For each file owning at least one
 * request: read the text, apply every request to it surgically, prove
 * with libucl that the result means exactly what was asked, and replace
 * the file atomically -- or leave it alone when nothing changed. Returns
 * EXIT_SUCCESS or EXIT_FAILURE.
 */
int
ucl_do_writes(void)
{
	struct flat expect;
	struct flat got;
	struct request *req;
	ucl_object_t *top;
	ucl_object_t *val;
	ucl_object_t *after;
	const ucl_object_t *cur;
	const char *perr;
	char *newtext;
	char *newv;
	char *oldv;
	char *style;
	char *text;
	size_t f;
	size_t newlen;
	size_t style_len;
	size_t textlen;
	unsigned int n;
	unsigned int nmine;
	bool changed;
	bool dirtied;
	bool dirty;
	int rv = EXIT_SUCCESS;

	for (f = 0; f < nconf_files; f++) {
		nmine = 0;
		for (n = 0; n < nreqs; n++) {
			if (reqs[n].remove) {
				if (reqs[n].infile[f])
					nmine++;
			} else if (reqs[n].newvalue != NULL &&
			    ucl_write_target(&reqs[n]) == f)
				nmine++;
		}
		if (nmine == 0)
			continue;

		text = slurp(conf_files[f], &textlen);
		top = ucl_parse_text(conf_files[f], text, textlen, NULL);
		memset(&expect, 0, sizeof(expect));
		(void)ucl_walk(top, NULL, flat_cb, &expect);

		dirty = false;
		for (n = 0; n < nreqs; n++) {
			req = &reqs[n];
			if (req->ucl_base != NULL) {
				/*
				 * `name.i...': the innermost array gains,
				 * loses, or changes one element; the file
				 * sees the whole array, the echo the element
				 * (a removal echoes its array, like -=).
				 */
				const ucl_object_t *parent = NULL;
				ucl_object_t *newel = NULL;
				char *newelv;
				char *plabel;

				if (req->remove ? !req->infile[f] :
				    (req->newvalue == NULL ||
				    ucl_write_target(req) != f))
					continue;
				cur = ucl_object_lookup_path(top,
				    req->ucl_base);
				if (cur == NULL ||
				    ucl_object_type(cur) != UCL_ARRAY) {
					if (req->remove)
						continue;
					errx(EXIT_FAILURE,
					    "%s: no array %s in %s",
					    req->ucl_label, req->ucl_base,
					    conf_files[f]);
				}
				if (req->remove)
					newelv = NULL;
				else if (req->listop != '\0') {
					/* Words apply to the element as is */
					const ucl_object_t *fresh;
					char *freshv;

					fresh = ucl_array_walk(cur,
					    req->ucl_idx, req->ucl_res,
					    req->ucl_nidx);
					freshv = (fresh != NULL) ?
					    ucl_render(fresh) : NULL;
					newel = ucl_list_apply(freshv,
					    req->rawvalue, req->listop);
					free(freshv);
					newelv = ucl_render(newel);
				} else {
					newel = ucl_value(req->rawvalue != NULL ?
					    req->rawvalue : req->newvalue);
					newelv = ucl_render(newel);
				}
				plabel = ucl_index_label(req,
				    req->ucl_nidx - 1);
				oldv = NULL;
				val = ucl_array_set(cur, req->ucl_idx,
				    req->ucl_res, req->ucl_nidx, newel,
				    plabel, &oldv, &parent);
				if (newel != NULL)
					ucl_object_unref(newel);
				if (req->remove && oldv == NULL) {
					ucl_object_unref(val);
					free(plabel);
					continue;
				}
				/* The indices as this file resolves them */
				ucl_relabel(req);
				free(plabel);
				plabel = ucl_index_label(req,
				    req->ucl_nidx - 1);
				if (oldv == NULL && (parent == NULL ||
				    ucl_object_type(parent) != UCL_ARRAY))
					errx(EXIT_FAILURE, "%s: not an array",
					    plabel);
				if (oldv == NULL &&
				    req->ucl_res[req->ucl_nidx - 1] !=
				    (long)ucl_array_size(parent))
					errx(EXIT_FAILURE,
					    "%s: %s has %u element%s",
					    req->ucl_label, plabel,
					    ucl_array_size(parent),
					    ucl_array_size(parent) == 1 ?
					    "" : "s");
				newv = ucl_render(val);
				changed = false;
				if (oldv == NULL || req->remove ||
				    strcmp(oldv, newelv) != 0) {
					newtext = ucl_text_edit(text, textlen,
					    req->ucl_base, val, newv, NULL, 0,
					    &changed);
					free(text);
					text = newtext;
					textlen = strlen(text);
					if (changed) {
						flat_set(&expect,
						    req->ucl_base, newv);
						req->found = 1;
						dirty = true;
					}
				}
				if (!quiet) {
					if (req->remove) {
						char *pold;
						char *pnew;
						const ucl_object_t *pafter;
						size_t d;

						/* The array that changed */
						pold = ucl_render(parent);
						pafter = val;
						for (d = 0;
						    d + 1 < req->ucl_nidx; d++)
							pafter =
							    ucl_array_find_index(
							    pafter, (unsigned int)
							    req->ucl_res[d]);
						pnew = ucl_render(pafter);
						print_write_echo(conf_files[f],
						    0, plabel, req->op, pold,
						    pnew, changed, 0, 0);
						free(pold);
						free(pnew);
					} else if (verbose_trail &&
					    oldv == NULL)
						print_write_echo(conf_files[f],
						    0, req->ucl_label, req->op,
						    "", newelv, changed, 1, 0);
					else
						print_write_echo(conf_files[f],
						    0, req->ucl_label, req->op,
						    oldv, newelv, changed, 0,
						    0);
				}
				/* `parent' and `cur' die with the old parse */
				if (changed)
					top = ucl_reparse(top, conf_files[f],
					    text, textlen);
				free(newelv);
				free(plabel);
				ucl_object_unref(val);
				free(oldv);
				free(newv);
				continue;
			}
			if (req->remove) {
				if (!req->infile[f])
					continue;
				/*
				 * A key repeated in the file is removed
				 * every time, lest a later one be unmasked.
				 * Each pass cuts text, so this ends.
				 */
				dirtied = false;
				while (ucl_object_lookup_path(top, req->name) !=
				    NULL) {
					newtext = ucl_text_edit(text, textlen,
					    req->name, NULL, NULL, NULL, 0,
					    &changed);
					free(text);
					text = newtext;
					textlen = strlen(text);
					if (!changed)
						break;
					dirtied = true;
					top = ucl_reparse(top, conf_files[f],
					    text, textlen);
				}
				if (!dirtied)
					continue;
				flat_remove(&expect, req->name);
				req->found = 1;
				dirty = true;
				if (!quiet && (verbose || verbose_trail))
					print_write_echo(conf_files[f], 0,
					    req->name, req->op, "", "", 0, 0,
					    1);
				continue;
			}
			if (req->newvalue == NULL ||
			    ucl_write_target(req) != f)
				continue;
			cur = ucl_object_lookup_path(top, req->name);
			oldv = (cur != NULL) ? ucl_render(cur) : NULL;
			if (req->listop != '\0' && oldv != NULL)
				/* Words apply to the value as it is now */
				val = ucl_list_apply(oldv, req->rawvalue,
				    req->listop);
			else if (req->listop != '\0')
				/* Not in this file: the scan's merge stands */
				val = ucl_value(req->newvalue);
			else
				val = ucl_value(req->rawvalue != NULL ?
				    req->rawvalue : req->newvalue);
			newv = ucl_render(val);
			changed = false;
			if (oldv == NULL || strcmp(oldv, newv) != 0) {
				/*
				 * A file with nothing to mimic takes its
				 * style from the file that defined the key
				 * or, for pkg's local repos file, from the
				 * base file it overrides.
				 */
				style = NULL;
				style_len = 0;
				if (req->srcidx >= 0 &&
				    (size_t)req->srcidx != f)
					style = slurp(conf_files[req->srcidx],
					    &style_len);
				else if (pkg_target && f == pkg_repos_idx &&
				    conf_ro != NULL) {
					size_t s;

					for (s = 0; s < nconf_files; s++)
						if (conf_ro[s])
							break;
					if (s < nconf_files)
						style = slurp(conf_files[s],
						    &style_len);
				}
				newtext = ucl_text_edit(text, textlen,
				    req->name, val, req->listop != '\0' ?
				    newv : req->rawvalue != NULL ?
				    req->rawvalue : req->newvalue, style,
				    style_len, &changed);
				free(style);
				free(text);
				text = newtext;
				textlen = strlen(text);
				if (changed) {
					top = ucl_reparse(top, conf_files[f],
					    text, textlen);
					flat_remove(&expect, req->name);
					if (ucl_object_type(val) == UCL_OBJECT)
						(void)ucl_walk(val, req->name,
						    flat_cb, &expect);
					else
						flat_set(&expect, req->name,
						    newv);
					dirty = true;
				}
			}
			ucl_object_unref(val);
			/*
			 * Echo against the value as it stood before this
			 * edit, or, when the key is not in this file, the
			 * effective value the scan found (possibly in an
			 * earlier, read-only file); `added' and `changed'
			 * describe this file.
			 */
			if (!quiet) {
				if (verbose_trail && oldv == NULL)
					print_write_echo(conf_files[f], 0,
					    req->name, req->op, "", newv,
					    changed, 1, 0);
				else
					print_write_echo(conf_files[f], 0,
					    req->name, req->op, oldv != NULL ?
					    oldv : req->value, newv, changed,
					    0, 0);
			}
			free(oldv);
			free(newv);
		}

		if (dirty) {
			/* The library has the last word on what we wrote */
			after = ucl_parse_text(conf_files[f], text, textlen,
			    &perr);
			if (after == NULL)
				errx(EXIT_FAILURE,
				    "%s: edit would not parse (%s); "
				    "file left unchanged", conf_files[f],
				    perr != NULL ? perr : "unknown error");
			memset(&got, 0, sizeof(got));
			(void)ucl_walk(after, NULL, flat_cb, &got);
			if (!flat_equal(&expect, &got))
				errx(EXIT_FAILURE,
				    "%s: edit would not mean what was asked; "
				    "file left unchanged", conf_files[f]);
			flat_free(&got);
			ucl_object_unref(after);
			newlen = textlen;
			ucl_replace_file(conf_files[f], text, newlen);
		}
		flat_free(&expect);
		ucl_object_unref(top);
		free(text);
	}

	/* A removal that matched no file at all is an unknown directive */
	for (n = 0; n < nreqs; n++) {
		if (!reqs[n].remove || reqs[n].found)
			continue;
		if (ignore_unknown)
			continue;
		if (!quiet)
			warnx("unknown directive '%s'", reqs[n].name);
		rv = EXIT_FAILURE;
	}

	return (rv);
}
