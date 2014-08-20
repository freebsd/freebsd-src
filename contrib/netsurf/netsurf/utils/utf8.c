/*
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * UTF-8 manipulation functions (implementation).
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iconv.h>

#include <parserutils/charset/utf8.h>

#include "desktop/gui_factory.h"

#include "utils/config.h"
#include "utils/log.h"
#include "utils/utf8.h"

/* exported interface documented in utils/utf8.h */
uint32_t utf8_to_ucs4(const char *s_in, size_t l)
{
	uint32_t ucs4;
	size_t len;
	parserutils_error perror;

	perror = parserutils_charset_utf8_to_ucs4((const uint8_t *) s_in, l,
			&ucs4, &len);
	if (perror != PARSERUTILS_OK)
		ucs4 = 0xfffd;

	return ucs4;
}

/* exported interface documented in utils/utf8.h */
size_t utf8_from_ucs4(uint32_t c, char *s)
{
	uint8_t *in = (uint8_t *) s;
	size_t len = 6;
	parserutils_error perror;

	perror = parserutils_charset_utf8_from_ucs4(c, &in, &len);
	if (perror != PARSERUTILS_OK) {
		s[0] = 0xef;
		s[1] = 0xbf;
		s[2] = 0xbd;
		return 3;
	}

	return 6 - len;
}

/* exported interface documented in utils/utf8.h */
size_t utf8_length(const char *s)
{
	return utf8_bounded_length(s, strlen(s));
}

/* exported interface documented in utils/utf8.h */
size_t utf8_bounded_length(const char *s, size_t l)
{
	size_t len;
	parserutils_error perror;

	perror = parserutils_charset_utf8_length((const uint8_t *) s, l, &len);
	if (perror != PARSERUTILS_OK)
		return 0;

	return len;
}

/* exported interface documented in utils/utf8.h */
size_t utf8_bounded_byte_length(const char *s, size_t l, size_t c)
{
	size_t len = 0;

	while (len < l && c-- > 0)
		len = utf8_next(s, l, len);

	return len;
}

/* exported interface documented in utils/utf8.h */
size_t utf8_char_byte_length(const char *s)
{
	size_t len;
	parserutils_error perror;

	perror = parserutils_charset_utf8_char_byte_length((const uint8_t *) s,
			&len);
	assert(perror == PARSERUTILS_OK);

	return len;
}

/* exported interface documented in utils/utf8.h */
size_t utf8_prev(const char *s, size_t o)
{
	uint32_t prev;
	parserutils_error perror;

	perror = parserutils_charset_utf8_prev((const uint8_t *) s, o, &prev);
	assert(perror == PARSERUTILS_OK);

	return prev;
}

/* exported interface documented in utils/utf8.h */
size_t utf8_next(const char *s, size_t l, size_t o)
{
	uint32_t next;
	parserutils_error perror;

	perror = parserutils_charset_utf8_next((const uint8_t *) s, l, o,
			&next);
	assert(perror == PARSERUTILS_OK);

	return next;
}

/* Cache of previous iconv conversion descriptor used by utf8_convert */
static struct {
	char from[32];	/**< Encoding name to convert from */
	char to[32];	/**< Encoding name to convert to */
	iconv_t cd;	/**< Iconv conversion descriptor */
} last_cd;

static inline void utf8_clear_cd_cache(void)
{
	last_cd.from[0] = '\0';
	last_cd.to[0] = '\0';
	last_cd.cd = 0;
}

/* exported interface documented in utils/utf8.h */
nserror utf8_finalise(void)
{
	if (last_cd.cd != 0)
		iconv_close(last_cd.cd);

	/* paranoia follows */
	utf8_clear_cd_cache();

	return NSERROR_OK;
}


/**
 * Convert a string from one encoding to another
 *
 * \param string  The NULL-terminated string to convert
 * \param len     Length of input string to consider (in bytes), or 0
 * \param from    The encoding name to convert from
 * \param to      The encoding name to convert to
 * \param result  Pointer to location in which to store result.
 * \param result_len Pointer to location in which to store result length.
 * \return NSERROR_OK for no error, NSERROR_NOMEM on allocation error,
 *         NSERROR_BAD_ENCODING for a bad character encoding
 */
static nserror
utf8_convert(const char *string,
	     size_t len,
	     const char *from,
	     const char *to,
	     char **result,
	     size_t *result_len)
{
	iconv_t cd;
	char *temp, *out, *in;
	size_t slen, rlen;

	assert(string && from && to && result);

	if (string[0] == '\0') {
		/* On AmigaOS, iconv() returns an error if we pass an
		 * empty string.  This prevents iconv() being called as
		 * there is no conversion necessary anyway. */
		*result = strdup("");
		if (!(*result)) {
			*result = NULL;
			return NSERROR_NOMEM;
		}

		return NSERROR_OK;
	}

	if (strcasecmp(from, to) == 0) {
		/* conversion from an encoding to itself == strdup */
		slen = len ? len : strlen(string);
		*(result) = strndup(string, slen);
		if (!(*result)) {
			*(result) = NULL;
			return NSERROR_NOMEM;
		}

		return NSERROR_OK;
	}

	in = (char *)string;

	/* we cache the last used conversion descriptor,
	 * so check if we're trying to use it here */
	if (strncasecmp(last_cd.from, from, sizeof(last_cd.from)) == 0 &&
			strncasecmp(last_cd.to, to, sizeof(last_cd.to)) == 0) {
		cd = last_cd.cd;
	}
	else {
		/* no match, so create a new cd */
		cd = iconv_open(to, from);
		if (cd == (iconv_t)-1) {
			if (errno == EINVAL)
				return NSERROR_BAD_ENCODING;
			/* default to no memory */
			return NSERROR_NOMEM;
		}

		/* close the last cd - we don't care if this fails */
		if (last_cd.cd)
			iconv_close(last_cd.cd);

		/* and copy the to/from/cd data into last_cd */
		strncpy(last_cd.from, from, sizeof(last_cd.from));
		strncpy(last_cd.to, to, sizeof(last_cd.to));
		last_cd.cd = cd;
	}

	slen = len ? len : strlen(string);
	/* Worst case = ASCII -> UCS4, so allocate an output buffer
	 * 4 times larger than the input buffer, and add 4 bytes at
	 * the end for the NULL terminator
	 */
	rlen = slen * 4 + 4;

	temp = out = malloc(rlen);
	if (!out) {
		return NSERROR_NOMEM;
	}

	/* perform conversion */
	if (iconv(cd, (void *) &in, &slen, &out, &rlen) == (size_t)-1) {
		free(temp);
		/* clear the cached conversion descriptor as it's invalid */
		if (last_cd.cd)
			iconv_close(last_cd.cd);
		utf8_clear_cd_cache();
		/** \todo handle the various cases properly
		 * There are 3 possible error cases:
		 * a) Insufficiently large output buffer
		 * b) Invalid input byte sequence
		 * c) Incomplete input sequence */
		return NSERROR_NOMEM;
	}

	*(result) = realloc(temp, out - temp + 4);
	if (!(*result)) {
		free(temp);
		*(result) = NULL; /* for sanity's sake */
		return NSERROR_NOMEM;
	}

	/* NULL terminate - needs 4 characters as we may have
	 * converted to UTF-32 */
	memset((*result) + (out - temp), 0, 4);

	if (result_len != NULL) {
		*result_len = (out - temp);
	}

	return NSERROR_OK;
}

/* exported interface documented in utils/utf8.h */
nserror utf8_to_enc(const char *string, const char *encname,
		size_t len, char **result)
{
	return utf8_convert(string, len, "UTF-8", encname, result, NULL);
}

/* exported interface documented in utils/utf8.h */
nserror utf8_from_enc(const char *string, const char *encname,
		size_t len, char **result, size_t *result_len)
{
	return utf8_convert(string, len, encname, "UTF-8", result, result_len);
}

/**
 * convert a chunk of html data
 */
static nserror
utf8_convert_html_chunk(iconv_t cd,
			const char *chunk,
			size_t inlen,
			char **out,
			size_t *outlen)
{
	size_t ret, esclen;
	uint32_t ucs4;
	char *pescape, escape[11];

	while (inlen > 0) {
		ret = iconv(cd, (void *) &chunk, &inlen, (void *) out, outlen);
		if (ret != (size_t) -1)
			break;

		if (errno != EILSEQ)
			return NSERROR_NOMEM;

		ucs4 = utf8_to_ucs4(chunk, inlen);
		esclen = snprintf(escape, sizeof(escape), "&#x%06x;", ucs4);
		pescape = escape;
		ret = iconv(cd, (void *) &pescape, &esclen,
				(void *) out, outlen);
		if (ret == (size_t) -1)
			return NSERROR_NOMEM;

		esclen = utf8_next(chunk, inlen, 0);
		chunk += esclen;
		inlen -= esclen;
	}

	return NSERROR_OK;
}

/* exported interface documented in utils/utf8.h */
nserror
utf8_to_html(const char *string, const char *encname, size_t len, char **result)
{
	iconv_t cd;
	const char *in;
	char *out, *origout;
	size_t off, prev_off, inlen, outlen, origoutlen, esclen;
	nserror ret;
	char *pescape, escape[11];

	if (len == 0)
		len = strlen(string);

	/* we cache the last used conversion descriptor,
	 * so check if we're trying to use it here */
	if (strncasecmp(last_cd.from, "UTF-8", sizeof(last_cd.from)) == 0 &&
			strncasecmp(last_cd.to, encname,
					sizeof(last_cd.to)) == 0 &&
			last_cd.cd != 0) {
		cd = last_cd.cd;
	} else {
		/* no match, so create a new cd */
		cd = iconv_open(encname, "UTF-8");
		if (cd == (iconv_t) -1) {
			if (errno == EINVAL)
				return NSERROR_BAD_ENCODING;
			/* default to no memory */
			return NSERROR_NOMEM;
		}

		/* close the last cd - we don't care if this fails */
		if (last_cd.cd)
			iconv_close(last_cd.cd);

		/* and copy the to/from/cd data into last_cd */
		strncpy(last_cd.from, "UTF-8", sizeof(last_cd.from));
		strncpy(last_cd.to, encname, sizeof(last_cd.to));
		last_cd.cd = cd;
	}

	/* Worst case is ASCII -> UCS4, with all characters escaped:
	 * "&#xYYYYYY;", thus each input character may become a string
	 * of 10 UCS4 characters, each 4 bytes in length, plus four for
	 * terminating the string */
	origoutlen = outlen = len * 10 * 4 + 4;
	origout = out = malloc(outlen);
	if (out == NULL) {
		iconv_close(cd);
		utf8_clear_cd_cache();
		return NSERROR_NOMEM;
	}

	/* Process input in chunks between characters we must escape */
	prev_off = off = 0;
	while (off < len) {
		/* Must escape '&', '<', and '>' */
		if (string[off] == '&' || string[off] == '<' ||
				string[off] == '>') {
			if (off - prev_off > 0) {
				/* Emit chunk */
				in = string + prev_off;
				inlen = off - prev_off;
				ret = utf8_convert_html_chunk(cd, in, inlen,
						&out, &outlen);
				if (ret != NSERROR_OK) {
					free(origout);
					iconv_close(cd);
					utf8_clear_cd_cache();
					return ret;
				}
			}

			/* Emit mandatory escape */
			esclen = snprintf(escape, sizeof(escape),
					"&#x%06x;", string[off]);
			pescape = escape;
			ret = utf8_convert_html_chunk(cd, pescape, esclen,
					&out, &outlen);
			if (ret != NSERROR_OK) {
				free(origout);
				iconv_close(cd);
				utf8_clear_cd_cache();
				return ret;
			}

			prev_off = off = utf8_next(string, len, off);
		} else {
			off = utf8_next(string, len, off);
		}
	}

	/* Process final chunk */
	if (prev_off < len) {
		in = string + prev_off;
		inlen = len - prev_off;
		ret = utf8_convert_html_chunk(cd, in, inlen, &out, &outlen);
		if (ret != NSERROR_OK) {
			free(origout);
			iconv_close(cd);
			utf8_clear_cd_cache();
			return ret;
		}
	}

	/* Terminate string */
	memset(out, 0, 4);
	outlen -= 4;

	/* Shrink-wrap */
	*result = realloc(origout, origoutlen - outlen);
	if (*result == NULL) {
		free(origout);
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/* exported interface documented in utils/utf8.h */
bool utf8_save_text(const char *utf8_text, const char *path)
{
	nserror ret;
	char *conv;
	FILE *out;

	ret = guit->utf8->utf8_to_local(utf8_text, strlen(utf8_text), &conv);
	if (ret != NSERROR_OK) {
		LOG(("failed to convert to local encoding, return %d", ret));
		return false;
	}

	out = fopen(path, "w");
	if (out) {
		int res = fputs(conv, out);
		if (res < 0) {
			LOG(("Warning: writing data failed"));
		}

		res = fputs("\n", out);
		fclose(out);
		free(conv);
		return (res != EOF);
	}
	free(conv);

	return false;
}
