/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * += and -= for the ucl target: elements on an array, words on a string.
 */


#include <ctype.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/* True when some element of `arr' renders as `want' */
static bool
ucl_array_has(const ucl_object_t *arr, const char *want)
{
	const ucl_object_t *cur;
	ucl_object_iter_t it;
	char *r;
	bool found = false;

	if ((it = ucl_object_iterate_new(arr)) == NULL)
		err(EXIT_FAILURE, NULL);
	while (!found && (cur = ucl_object_iterate_safe(it, false)) != NULL) {
		r = ucl_render(cur);
		found = (strcmp(r, want) == 0);
		free(r);
	}
	ucl_object_iterate_free(it);
	return (found);
}

/*
 * sysrc(8)-style `name+=word ...' and `name-=word ...' applied to the
 * value rendered in `curtext' (NULL when unset). An array takes the
 * words as elements, appended when absent or struck when present,
 * with the survivors in their old order; anything else takes them as
 * words of a string. Returns the new value.
 */
ucl_object_t *
ucl_list_apply(const char *curtext, const char *words, int op)
{
	ucl_object_t *cur = NULL;
	ucl_object_t *elem;
	ucl_object_t *out;
	const ucl_object_t *old;
	ucl_object_iter_t it;
	const char *token;
	char *merged;
	char *r;
	char *word;
	size_t len;

	if (curtext != NULL)
		cur = ucl_value(curtext);
	if (cur == NULL || ucl_object_type(cur) != UCL_ARRAY) {
		if (cur != NULL)
			ucl_object_unref(cur);
		merged = list_merge(curtext, words, op);
		out = ucl_value(merged);
		free(merged);
		return (out);
	}
	if ((out = ucl_object_typed_new(UCL_ARRAY)) == NULL)
		err(EXIT_FAILURE, NULL);

	/* Copy the current elements, striking matches under `-' */
	if ((it = ucl_object_iterate_new(cur)) == NULL)
		err(EXIT_FAILURE, NULL);
	while ((old = ucl_object_iterate_safe(it, false)) != NULL) {
		if (op == '-') {
			r = ucl_render(old);
			token = words;
			while (*token != '\0') {
				token += strspn(token, " \t");
				if ((len = strcspn(token, " \t")) == 0)
					break;
				if (strlen(r) == len &&
				    strncmp(r, token, len) == 0)
					break;
				token += len;
			}
			free(r);
			if (*token != '\0')
				continue;
		}
		ucl_array_append(out, ucl_object_copy(old));
	}
	ucl_object_iterate_free(it);

	/* Append the words not already present under `+' */
	if (op == '+') {
		token = words;
		while (*token != '\0') {
			token += strspn(token, " \t");
			if ((len = strcspn(token, " \t")) == 0)
				break;
			if ((word = strndup(token, len)) == NULL)
				err(EXIT_FAILURE, NULL);
			elem = ucl_value(word);
			r = ucl_render(elem);
			if (ucl_array_has(out, r))
				ucl_object_unref(elem);
			else
				ucl_array_append(out, elem);
			free(r);
			free(word);
			token += len;
		}
	}
	ucl_object_unref(cur);
	return (out);
}

/*
 * merge_list_requests() for the ucl target: keep the words in
 * `rawvalue' (the write applies them again to whatever the file holds
 * by then, so requests on one array compose) and put the merged value,
 * as the scan rendered it, in `newvalue' for -c and the echo. A value
 * that is not an array is left to the word merge, words kept likewise.
 * Returns true when the request was taken.
 */
bool
ucl_list_merge(struct request *req)
{
	ucl_object_t *cur;
	ucl_object_t *out;

	req->rawvalue = req->newvalue;
	if (req->value == NULL)
		return (false);
	cur = ucl_value(req->value);
	if (ucl_object_type(cur) != UCL_ARRAY) {
		ucl_object_unref(cur);
		return (false);
	}
	ucl_object_unref(cur);
	out = ucl_list_apply(req->value, req->rawvalue, req->listop);
	req->newvalue = ucl_render(out);
	ucl_object_unref(out);
	return (true);
}

