/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2013-2015 Red Hat, Inc.
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

#pragma once

#include <sys/types.h>
#include <sys/mouse.h>

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <xlocale.h>

#define	HAVE_LOCALE_H	1

#define MOUSED_ATTRIBUTE_PRINTF(_format, _args) \
	__attribute__ ((format (printf, _format, _args)))

#define	ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
/**
 * Iterate through the array _arr, assigning the variable elem to each
 * element. elem only exists within the loop.
 */
#define ARRAY_FOR_EACH(_arr, _elem) \
	for (__typeof__((_arr)[0]) *_elem = _arr; \
	     _elem < (_arr) + ARRAY_LENGTH(_arr); \
	     _elem++)

#define versionsort(...) alphasort(__VA_ARGS__)
#define bit(x_) (1UL << (x_))
#define min(a, b) (((a) < (b)) ? (a) : (b))

/* Supported device interfaces */
enum device_if {
	DEVICE_IF_UNKNOWN = -1,
	DEVICE_IF_EVDEV = 0,
	DEVICE_IF_SYSMOUSE,
};

/* Recognized device types */
enum device_type {
	DEVICE_TYPE_UNKNOWN = -1,
	DEVICE_TYPE_MOUSE = 0,
	DEVICE_TYPE_POINTINGSTICK,
	DEVICE_TYPE_TOUCHPAD,
	DEVICE_TYPE_TOUCHSCREEN,
	DEVICE_TYPE_TABLET,
	DEVICE_TYPE_TABLET_PAD,
	DEVICE_TYPE_KEYBOARD,
	DEVICE_TYPE_JOYSTICK,
};

struct device {
	char path[80];
	enum device_if iftype;
	enum device_type type;
	char name[80];
	char uniq[80];
	struct input_id id;
	mousemode_t mode;
};

/**
 * @ingroup base
 *
 * Log handler type for custom logging.
 *
 * @param priority The priority of the current message
 * @param format Message format in printf-style
 * @param args Message arguments
 */
typedef void moused_log_handler(int priority, int errnum,
				const char *format, va_list args);

/* util-mem.h */

/**
 * Use: _unref_(foo) struct foo *bar;
 *
 * This requires foo_unrefp() to be present, use DEFINE_UNREF_CLEANUP_FUNC.
 */
#define _unref_(_type) __attribute__((cleanup(_type##_unrefp))) struct _type

/**
 * Define a cleanup function for the struct type foo with a matching
 * foo_unref(). Use:
 * DEFINE_UNREF_CLEANUP_FUNC(foo)
 * _unref_(foo) struct foo *bar;
 */
#define DEFINE_UNREF_CLEANUP_FUNC(_type)		\
	static inline void _type##_unrefp(struct _type **_p) {  \
		if (*_p)					\
			_type##_unref(*_p);			\
	}							\
	struct __useless_struct_to_allow_trailing_semicolon__

static inline void*
_steal(void *ptr) {
	void **original = (void**)ptr;
	void *swapped = *original;
	*original = NULL;
	return swapped;
}

/**
 * Resets the pointer content and resets the data to NULL.
 * This circumvents _cleanup_ handling for that pointer.
 * Use:
 *   _cleanup_free_ char *data = malloc();
 *   return steal(&data);
 *
 */
#define steal(ptr_) \
  (typeof(*ptr_))_steal(ptr_)

/* ! util-mem.h */

/* util-strings.h */

static inline bool
streq(const char *str1, const char *str2)
{
	/* one NULL, one not NULL is always false */
	if (str1 && str2)
		return strcmp(str1, str2) == 0;
	return str1 == str2;
}

static inline bool
strneq(const char *str1, const char *str2, int n)
{
	/* one NULL, one not NULL is always false */
	if (str1 && str2)
		return strncmp(str1, str2, n) == 0;
	return str1 == str2;
}

static inline void *
zalloc(size_t size)
{
	void *p;

	/* We never need to alloc anything more than 1,5 MB so we can assume
	 * if we ever get above that something's going wrong */
	if (size > 1536 * 1024)
		assert(!"bug: internal malloc size limit exceeded");

	p = calloc(1, size);
	if (!p)
		abort();

	return p;
}

/**
 * strdup guaranteed to succeed. If the input string is NULL, the output
 * string is NULL. If the input string is a string pointer, we strdup or
 * abort on failure.
 */
static inline char*
safe_strdup(const char *str)
{
	char *s;

	if (!str)
		return NULL;

	s = strdup(str);
	if (!s)
		abort();
	return s;
}

/**
 * Simple wrapper for asprintf that ensures the passed in-pointer is set
 * to NULL upon error.
 * The standard asprintf() call does not guarantee the passed in pointer
 * will be NULL'ed upon failure, whereas this wrapper does.
 *
 * @param strp pointer to set to newly allocated string.
 * This pointer should be passed to free() to release when done.
 * @param fmt the format string to use for printing.
 * @return The number of bytes printed (excluding the null byte terminator)
 * upon success or -1 upon failure. In the case of failure the pointer is set
 * to NULL.
 */
__attribute__ ((format (printf, 2, 3)))
static inline int
xasprintf(char **strp, const char *fmt, ...)
{
	int rc = 0;
	va_list args;

	va_start(args, fmt);
	rc = vasprintf(strp, fmt, args);
	va_end(args);
	if ((rc == -1) && strp)
		*strp = NULL;

	return rc;
}

static inline bool
safe_atoi_base(const char *str, int *val, int base)
{
	assert(str != NULL);

	char *endptr;
	long v;

	assert(base == 10 || base == 16 || base == 8);

	errno = 0;
	v = strtol(str, &endptr, base);
	if (errno > 0)
		return false;
	if (str == endptr)
		return false;
	if (*str != '\0' && *endptr != '\0')
		return false;

	if (v > INT_MAX || v < INT_MIN)
		return false;

	*val = v;
	return true;
}

static inline bool
safe_atoi(const char *str, int *val)
{
	assert(str != NULL);
	return safe_atoi_base(str, val, 10);
}

static inline bool
safe_atou_base(const char *str, unsigned int *val, int base)
{
	assert(str != NULL);

	char *endptr;
	unsigned long v;

	assert(base == 10 || base == 16 || base == 8);

	errno = 0;
	v = strtoul(str, &endptr, base);
	if (errno > 0)
		return false;
	if (str == endptr)
		return false;
	if (*str != '\0' && *endptr != '\0')
		return false;

	if ((long)v < 0)
		return false;

	*val = v;
	return true;
}

static inline bool
safe_atou(const char *str, unsigned int *val)
{
	assert(str != NULL);
	return safe_atou_base(str, val, 10);
}

static inline bool
safe_atod(const char *str, double *val)
{
	assert(str != NULL);

	char *endptr;
	double v;
	size_t slen = strlen(str);

	/* We don't have a use-case where we want to accept hex for a double
	 * or any of the other values strtod can parse */
	for (size_t i = 0; i < slen; i++) {
		char c = str[i];

		if (isdigit(c))
		       continue;
		switch(c) {
		case '+':
		case '-':
		case '.':
			break;
		default:
			return false;
		}
	}

#ifdef HAVE_LOCALE_H
	/* Create a "C" locale to force strtod to use '.' as separator */
	locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
	if (c_locale == (locale_t)0)
		return false;

	errno = 0;
	v = strtod_l(str, &endptr, c_locale);
	freelocale(c_locale);
#else
	/* No locale support in provided libc, assume it already uses '.' */
	errno = 0;
	v = strtod(str, &endptr);
#endif
	if (errno > 0)
		return false;
	if (str == endptr)
		return false;
	if (*str != '\0' && *endptr != '\0')
		return false;
	if (v != 0.0 && !isnormal(v))
		return false;

	*val = v;
	return true;
}

char **strv_from_string(const char *in, const char *separator, size_t *num_elements);

typedef int (*strv_foreach_callback_t)(const char *str, size_t index, void *data);
int strv_for_each_n(const char **strv, size_t max, strv_foreach_callback_t func, void *data);

static inline void
strv_free(char **strv) {
	char **s = strv;

	if (!strv)
		return;

	while (*s != NULL) {
		free(*s);
		*s = (char*)0x1; /* detect use-after-free */
		s++;
	}

	free (strv);
}

/**
 * Return true if str ends in suffix, false otherwise. If the suffix is the
 * empty string, strendswith() always returns false.
 */
static inline bool
strendswith(const char *str, const char *suffix)
{
	if (str == NULL)
		return false;

	size_t slen = strlen(str);
	size_t suffixlen = strlen(suffix);
	size_t offset;

	if (slen == 0 || suffixlen == 0 || suffixlen > slen)
		return false;

	offset = slen - suffixlen;
	return strneq(&str[offset], suffix, suffixlen);
}

static inline bool
strstartswith(const char *str, const char *prefix)
{
	if (str == NULL)
		return false;

	size_t prefixlen = strlen(prefix);

	return prefixlen > 0 ? strneq(str, prefix, strlen(prefix)) : false;
}

/* !util-strings.h */

/* util-prop-parsers.h */

struct input_prop {
	unsigned int prop;
	bool enabled;
};

bool parse_dimension_property(const char *prop, size_t *w, size_t *h);
bool parse_range_property(const char *prop, int *hi, int *lo);
bool parse_boolean_property(const char *prop, bool *b);
#define	EVENT_CODE_UNDEFINED 0xffff
bool parse_evcode_property(const char *prop, struct input_event *events, size_t *nevents);
bool parse_input_prop_property(const char *prop, struct input_prop *props_out, size_t *nprops);

/* !util-prop-parsers.h */
