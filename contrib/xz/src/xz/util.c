///////////////////////////////////////////////////////////////////////////////
//
/// \file       util.c
/// \brief      Miscellaneous utility functions
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include <stdarg.h>


extern void *
xrealloc(void *ptr, size_t size)
{
	assert(size > 0);

	ptr = realloc(ptr, size);
	if (ptr == NULL)
		message_fatal("%s", strerror(errno));

	return ptr;
}


extern char *
xstrdup(const char *src)
{
	assert(src != NULL);
	const size_t size = strlen(src) + 1;
	char *dest = xmalloc(size);
	return memcpy(dest, src, size);
}


extern uint64_t
str_to_uint64(const char *name, const char *value, uint64_t min, uint64_t max)
{
	uint64_t result = 0;

	// Skip blanks.
	while (*value == ' ' || *value == '\t')
		++value;

	// Accept special value "max". Supporting "min" doesn't seem useful.
	if (strcmp(value, "max") == 0)
		return max;

	if (*value < '0' || *value > '9')
		message_fatal(_("%s: Value is not a non-negative "
				"decimal integer"), value);

	do {
		// Don't overflow.
		if (result > (UINT64_MAX - 9) / 10)
			goto error;

		result *= 10;
		result += *value - '0';
		++value;
	} while (*value >= '0' && *value <= '9');

	if (*value != '\0') {
		// Look for suffix. Originally this supported both base-2
		// and base-10, but since there seems to be little need
		// for base-10 in this program, treat everything as base-2
		// and also be more relaxed about the case of the first
		// letter of the suffix.
		uint64_t multiplier = 0;
		if (*value == 'k' || *value == 'K')
			multiplier = UINT64_C(1) << 10;
		else if (*value == 'm' || *value == 'M')
			multiplier = UINT64_C(1) << 20;
		else if (*value == 'g' || *value == 'G')
			multiplier = UINT64_C(1) << 30;

		++value;

		// Allow also e.g. Ki, KiB, and KB.
		if (*value != '\0' && strcmp(value, "i") != 0
				&& strcmp(value, "iB") != 0
				&& strcmp(value, "B") != 0)
			multiplier = 0;

		if (multiplier == 0) {
			message(V_ERROR, _("%s: Invalid multiplier suffix"),
					value - 1);
			message_fatal(_("Valid suffixes are `KiB' (2^10), "
					"`MiB' (2^20), and `GiB' (2^30)."));
		}

		// Don't overflow here either.
		if (result > UINT64_MAX / multiplier)
			goto error;

		result *= multiplier;
	}

	if (result < min || result > max)
		goto error;

	return result;

error:
	message_fatal(_("Value of the option `%s' must be in the range "
				"[%" PRIu64 ", %" PRIu64 "]"),
				name, min, max);
}


extern uint64_t
round_up_to_mib(uint64_t n)
{
	return (n >> 20) + ((n & ((UINT32_C(1) << 20) - 1)) != 0);
}


extern const char *
uint64_to_str(uint64_t value, uint32_t slot)
{
	// 2^64 with thousand separators is 26 bytes plus trailing '\0'.
	static char bufs[4][32];

	assert(slot < ARRAY_SIZE(bufs));

	static enum { UNKNOWN, WORKS, BROKEN } thousand = UNKNOWN;
	if (thousand == UNKNOWN) {
		bufs[slot][0] = '\0';
		snprintf(bufs[slot], sizeof(bufs[slot]), "%'" PRIu64,
				UINT64_C(1));
		thousand = bufs[slot][0] == '1' ? WORKS : BROKEN;
	}

	if (thousand == WORKS)
		snprintf(bufs[slot], sizeof(bufs[slot]), "%'" PRIu64, value);
	else
		snprintf(bufs[slot], sizeof(bufs[slot]), "%" PRIu64, value);

	return bufs[slot];
}


extern const char *
uint64_to_nicestr(uint64_t value, enum nicestr_unit unit_min,
		enum nicestr_unit unit_max, bool always_also_bytes,
		uint32_t slot)
{
	assert(unit_min <= unit_max);
	assert(unit_max <= NICESTR_TIB);

	enum nicestr_unit unit = NICESTR_B;
	const char *str;

	if ((unit_min == NICESTR_B && value < 10000)
			|| unit_max == NICESTR_B) {
		// The value is shown as bytes.
		str = uint64_to_str(value, slot);
	} else {
		// Scale the value to a nicer unit. Unless unit_min and
		// unit_max limit us, we will show at most five significant
		// digits with one decimal place.
		double d = (double)(value);
		do {
			d /= 1024.0;
			++unit;
		} while (unit < unit_min || (d > 9999.9 && unit < unit_max));

		str = double_to_str(d);
	}

	static const char suffix[5][4] = { "B", "KiB", "MiB", "GiB", "TiB" };

	// Minimum buffer size:
	// 26   2^64 with thousand separators
	//  4   " KiB"
	//  2   " ("
	// 26   2^64 with thousand separators
	//  3   " B)"
	//  1   '\0'
	// 62   Total
	static char buf[4][64];
	char *pos = buf[slot];
	size_t left = sizeof(buf[slot]);
	my_snprintf(&pos, &left, "%s %s", str, suffix[unit]);

	if (always_also_bytes && value >= 10000)
		snprintf(pos, left, " (%s B)", uint64_to_str(value, slot));

	return buf[slot];
}


extern const char *
double_to_str(double value)
{
	static char buf[64];

	static enum { UNKNOWN, WORKS, BROKEN } thousand = UNKNOWN;
	if (thousand == UNKNOWN) {
		buf[0] = '\0';
		snprintf(buf, sizeof(buf), "%'.1f", 2.0);
		thousand = buf[0] == '2' ? WORKS : BROKEN;
	}

	if (thousand == WORKS)
		snprintf(buf, sizeof(buf), "%'.1f", value);
	else
		snprintf(buf, sizeof(buf), "%.1f", value);

	return buf;
}


extern void
my_snprintf(char **pos, size_t *left, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	const int len = vsnprintf(*pos, *left, fmt, ap);
	va_end(ap);

	// If an error occurred, we want the caller to think that the whole
	// buffer was used. This way no more data will be written to the
	// buffer. We don't need better error handling here.
	if (len < 0 || (size_t)(len) >= *left) {
		*left = 0;
	} else {
		*pos += len;
		*left -= len;
	}

	return;
}


/*
/// \brief      Simple quoting to get rid of ASCII control characters
///
/// This is not so cool and locale-dependent, but should be good enough
/// At least we don't print any control characters on the terminal.
///
extern char *
str_quote(const char *str)
{
	size_t dest_len = 0;
	bool has_ctrl = false;

	while (str[dest_len] != '\0')
		if (*(unsigned char *)(str + dest_len++) < 0x20)
			has_ctrl = true;

	char *dest = malloc(dest_len + 1);
	if (dest != NULL) {
		if (has_ctrl) {
			for (size_t i = 0; i < dest_len; ++i)
				if (*(unsigned char *)(str + i) < 0x20)
					dest[i] = '?';
				else
					dest[i] = str[i];

			dest[dest_len] = '\0';

		} else {
			// Usually there are no control characters,
			// so we can optimize.
			memcpy(dest, str, dest_len + 1);
		}
	}

	return dest;
}
*/


extern bool
is_empty_filename(const char *filename)
{
	if (filename[0] == '\0') {
		message_error(_("Empty filename, skipping"));
		return true;
	}

	return false;
}


extern bool
is_tty_stdin(void)
{
	const bool ret = isatty(STDIN_FILENO);

	if (ret)
		message_error(_("Compressed data cannot be read from "
				"a terminal"));

	return ret;
}


extern bool
is_tty_stdout(void)
{
	const bool ret = isatty(STDOUT_FILENO);

	if (ret)
		message_error(_("Compressed data cannot be written to "
				"a terminal"));

	return ret;
}
