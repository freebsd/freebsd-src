/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013-2019 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <dev/evdev/input.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xlocale.h>

#include "util.h"
#include "util-evdev.h"
#include "util-list.h"

/* util-strings.c */

/**
 * Return the next word in a string pointed to by state before the first
 * separator character. Call repeatedly to tokenize a whole string.
 *
 * @param state Current state
 * @param len String length of the word returned
 * @param separators List of separator characters
 *
 * @return The first word in *state, NOT null-terminated
 */
static const char *
next_word(const char **state, size_t *len, const char *separators)
{
	const char *next = *state;
	size_t l;

	if (!*next)
		return NULL;

	next += strspn(next, separators);
	if (!*next) {
		*state = next;
		return NULL;
	}

	l = strcspn(next, separators);
	*state = next + l;
	*len = l;

	return next;
}

/**
 * Return a null-terminated string array with the tokens in the input
 * string, e.g. "one two\tthree" with a separator list of " \t" will return
 * an array [ "one", "two", "three", NULL ] and num elements 3.
 *
 * Use strv_free() to free the array.
 *
 * Another example:
 *   result = strv_from_string("+1-2++3--4++-+5-+-", "+-", &nelem)
 *   result == [ "1", "2", "3", "4", "5", NULL ] and nelem == 5
 *
 * @param in Input string
 * @param separators List of separator characters
 * @param num_elements Number of elements found in the input string
 *
 * @return A null-terminated string array or NULL on errors
 */
char **
strv_from_string(const char *in, const char *separators, size_t *num_elements)
{
	assert(in != NULL);
	assert(separators != NULL);
	assert(num_elements != NULL);

	const char *s = in;
	size_t l, nelems = 0;
	while (next_word(&s, &l, separators) != NULL)
		nelems++;

	if (nelems == 0) {
		*num_elements = 0;
		return NULL;
	}

	size_t strv_len = nelems + 1; /* NULL-terminated */
	char **strv = zalloc(strv_len * sizeof *strv);

	size_t idx = 0;
	const char *word;
	s = in;
	while ((word = next_word(&s, &l, separators)) != NULL) {
		char *copy = strndup(word, l);
		if (!copy) {
                        strv_free(strv);
			*num_elements = 0;
			return NULL;
		}

		strv[idx++] = copy;
	}

	*num_elements = nelems;

	return strv;
}

/**
 * Iterate through strv, calling func with each string and its respective index.
 * Iteration stops successfully after max elements or at the last element,
 * whichever occurs first.
 *
 * If func returns non-zero, iteration stops and strv_for_each returns
 * that value.
 *
 * @return zero on success, otherwise the error returned by the callback
 */
int strv_for_each_n(const char **strv, size_t max, strv_foreach_callback_t func, void *data)
{
	for (size_t i = 0; i < max && strv && strv[i]; i++) {
		int ret = func(strv[i], i, data);
		if (ret)
			return ret;
	}
	return 0;
}

/* !util-strings.c */

/* util-prop-parsers.c */

/**
 * Parses a simple dimension string in the form of "10x40". The two
 * numbers must be positive integers in decimal notation.
 * On success, the two numbers are stored in w and h. On failure, w and h
 * are unmodified.
 *
 * @param prop The value of the property
 * @param w Returns the first component of the dimension
 * @param h Returns the second component of the dimension
 * @return true on success, false otherwise
 */
bool
parse_dimension_property(const char *prop, size_t *w, size_t *h)
{
	int x, y;

	if (!prop)
		return false;

	if (sscanf(prop, "%dx%d", &x, &y) != 2)
		return false;

	if (x <= 0 || y <= 0)
		return false;

	*w = (size_t)x;
	*h = (size_t)y;
	return true;
}

/**
 * Parses a string of the format "a:b" where both a and b must be integer
 * numbers and a > b. Also allowed is the special string value "none" which
 * amounts to unsetting the property.
 *
 * @param prop The value of the property
 * @param hi Set to the first digit or 0 in case of 'none'
 * @param lo Set to the second digit or 0 in case of 'none'
 * @return true on success, false otherwise
 */
bool
parse_range_property(const char *prop, int *hi, int *lo)
{
	int first, second;

	if (!prop)
		return false;

	if (streq(prop, "none")) {
		*hi = 0;
		*lo = 0;
		return true;
	}

	if (sscanf(prop, "%d:%d", &first, &second) != 2)
		return false;

	if (second >= first)
		return false;

	*hi = first;
	*lo = second;

	return true;
}

bool
parse_boolean_property(const char *prop, bool *b)
{
	if (!prop)
		return false;

	if (streq(prop, "1"))
		*b = true;
	else if (streq(prop, "0"))
		*b = false;
	else
		return false;

	return true;
}

static bool
parse_evcode_string(const char *s, int *type_out, int *code_out)
{
	int type, code;

	if (strstartswith(s, "EV_")) {
		type = libevdev_event_type_from_name(s);
		if (type == -1)
			return false;

		code = EVENT_CODE_UNDEFINED;
	} else {
		struct map {
			const char *str;
			int type;
		} map[] = {
			{ "KEY_", EV_KEY },
			{ "BTN_", EV_KEY },
			{ "ABS_", EV_ABS },
			{ "REL_", EV_REL },
			{ "SW_", EV_SW },
		};
		bool found = false;

		ARRAY_FOR_EACH(map, m) {
			if (!strstartswith(s, m->str))
				continue;

			type = m->type;
			code = libevdev_event_code_from_name(type, s);
			if (code == -1)
				return false;

			found = true;
			break;
		}
		if (!found)
			return false;
	}

	*type_out = type;
	*code_out = code;

	return true;
}

/**
 * Parses a string of the format "+EV_ABS;+KEY_A;-BTN_TOOL_DOUBLETAP;-ABS_X;"
 * where each element must be + or - (enable/disable) followed by a named event
 * type OR a named event code OR a tuple in the form of EV_KEY:0x123, i.e. a
 * named event type followed by a hex event code.
 *
 * events must point to an existing array of size nevents.
 * nevents specifies the size of the array in events and returns the number
 * of items, elements exceeding nevents are simply ignored, just make sure
 * events is large enough for your use-case.
 *
 * The results are returned as input events with type and code set, all
 * other fields undefined. Where only the event type is specified, the code
 * is set to EVENT_CODE_UNDEFINED.
 *
 * On success, events contains nevents events with each event's value set to 1
 * or 0 depending on the + or - prefix.
 */
bool
parse_evcode_property(const char *prop, struct input_event *events, size_t *nevents)
{
	bool rc = false;
	/* A randomly chosen max so we avoid crazy quirks */
	struct input_event evs[32];

	memset(evs, 0, sizeof evs);

	size_t ncodes;
	char **strv = strv_from_string(prop, ";", &ncodes);
	if (!strv || ncodes == 0 || ncodes > ARRAY_LENGTH(evs))
		goto out;

	ncodes = min(*nevents, ncodes);
	for (size_t idx = 0; strv[idx]; idx++) {
		char *s = strv[idx];
		bool enable;

		switch (*s) {
		case '+': enable = true; break;
		case '-': enable = false; break;
		default:
			goto out;
		}

		s++;

		int type, code;

		if (strstr(s, ":") == NULL) {
			if (!parse_evcode_string(s, &type, &code))
				goto out;
		} else {
			int consumed;
			char stype[13] = {0}; /* EV_FF_STATUS + '\0' */

			if (sscanf(s, "%12[A-Z_]:%x%n", stype, &code, &consumed) != 2 ||
			    strlen(s) != (size_t)consumed ||
			    (type = libevdev_event_type_from_name(stype)) == -1 ||
			    code < 0 || code > libevdev_event_type_get_max(type))
			    goto out;
		}

		evs[idx].type = type;
		evs[idx].code = code;
		evs[idx].value = enable;
	}

	memcpy(events, evs, ncodes * sizeof *events);
	*nevents = ncodes;
	rc = true;

out:
	strv_free(strv);
	return rc;
}

/**
 * Parses a string of the format "+INPUT_PROP_BUTTONPAD;-INPUT_PROP_POINTER;+0x123;"
 * where each element must be a named input prop OR a hexcode in the form
 * 0x1234. The prefix for each element must be either '+' (enable) or '-' (disable).
 *
 * props must point to an existing array of size nprops.
 * nprops specifies the size of the array in props and returns the number
 * of elements, elements exceeding nprops are simply ignored, just make sure
 * props is large enough for your use-case.
 *
 * On success, props contains nprops elements.
 */
bool
parse_input_prop_property(const char *prop, struct input_prop *props_out, size_t *nprops)
{
	bool rc = false;
	struct input_prop props[INPUT_PROP_CNT]; /* doubling up on quirks is a bug */

	size_t count;
	char **strv = strv_from_string(prop, ";", &count);
	if (!strv || count == 0 || count > ARRAY_LENGTH(props))
		goto out;

	count = min(*nprops, count);
	for (size_t idx = 0; strv[idx]; idx++) {
		char *s = strv[idx];
		unsigned int prop;
		bool enable;

		switch (*s) {
		case '+': enable = true; break;
		case '-': enable = false; break;
		default:
			goto out;
		}

		s++;

		if (safe_atou_base(s, &prop, 16)) {
			if (prop > INPUT_PROP_MAX)
				goto out;
		} else {
			int val = libevdev_property_from_name(s);
			if (val == -1)
				goto out;
			prop = (unsigned int)val;
		}
		props[idx].prop = prop;
		props[idx].enabled = enable;
	}

	memcpy(props_out, props, count * sizeof *props);
	*nprops = count;
	rc = true;

out:
	strv_free(strv);
	return rc;
}

/* !util-prop-parsers.c */
