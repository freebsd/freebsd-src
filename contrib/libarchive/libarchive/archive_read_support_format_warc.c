/*-
 * Copyright (c) 2014 Sebastian Freundt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

/*
 * An overview of WARC format:
 *
 * WARC files are laid out as a sequence of records.  Each record has
 * a text header followed by a content block whose size is given by
 * Content-Length.  This reader supports WARC/0.12 through WARC/1.0
 * and was written using the final draft that became ISO 28500:2009:
 * http://bibnum.bnf.fr/warc/WARC_ISO_28500_version1_latestdraft.pdf
 *
 * This reader exposes resource and response records as regular files
 * when they have a usable WARC-Target-URI.  WARC-Date is exposed as
 * ctime, and a Last-Modified record header is exposed as mtime when
 * present.
 *
 * TODO: Real-world WARCs can contain resources at endpoints ending in
 * a slash, for example http://bibnum.bnf.fr/warc/.  Some responses
 * include a Content-Location header that points to a Unix-compatible
 * filename such as http://bibnum.bnf.fr/warc/index.html, but WARC does
 * not require that convention and some sites do not follow it.  Until
 * archive options exist to control these entries, this reader skips
 * them instead of creating directory endpoints as files.
 */

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_integer.h"
#include "archive_private.h"
#include "archive_read_private.h"

typedef enum {
	WT_NONE,
	/* WARC info */
	WT_INFO,
	/* Metadata */
	WT_META,
	/* Resource */
	WT_RSRC,
	/* Request, unsupported */
	WT_REQ,
	/* Response */
	WT_RSP,
	/* Revisit, unsupported */
	WT_RVIS,
	/* Conversion, unsupported */
	WT_CONV,
	/* Continuation, currently unsupported */
	WT_CONT,
	/* Invalid type */
	LAST_WT
} warc_type_t;

typedef struct {
	size_t len;
	const char *str;
} warc_string_t;

typedef struct {
	size_t len;
	char *str;
} warc_strbuf_t;

struct warc {
	/* Content length of the current record */
	int64_t cntlen;
	/* Bytes processed from the current record */
	int64_t cntoff;
	/* Bytes to consume before the next read */
	int64_t unconsumed;

	/* String pool */
	warc_strbuf_t pool;
	/* Previous version */
	unsigned int pver;
	/* Stringified format name */
	struct archive_string sver;
};

static int	archive_read_format_warc_bid(struct archive_read *, int);
static int	archive_read_format_warc_cleanup(struct archive_read *);
static int	archive_read_format_warc_read_data(struct archive_read *,
		    const void **, size_t *, int64_t *);
static int	archive_read_format_warc_skip(struct archive_read *);
static int	archive_read_format_warc_read_header(struct archive_read *,
		    struct archive_entry *);

/* Private routines */
static unsigned int	warc_read_version(const char *, size_t);
static unsigned int	warc_read_type(const char *, size_t);
static warc_string_t	warc_read_uri(const char *, size_t);
static int64_t		warc_read_length(const char *, size_t);
static time_t		warc_read_date(const char *, size_t);
static time_t		warc_read_last_modified(const char *, size_t);
static const char	*warc_find_eoh(const char *, size_t);
static const char	*warc_find_eol(const char *, size_t);

int
archive_read_support_format_warc(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct warc *warc;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_warc");

	if ((warc = calloc(1, sizeof(*warc))) == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate warc data");
		return (ARCHIVE_FATAL);
	}

	r = __archive_read_register_format(a,
	    warc,
	    "warc",
	    archive_read_format_warc_bid,
	    NULL,
	    archive_read_format_warc_read_header,
	    archive_read_format_warc_read_data,
	    archive_read_format_warc_skip,
	    NULL,
	    archive_read_format_warc_cleanup,
	    NULL,
	    NULL);

	if (r != ARCHIVE_OK) {
		free(warc);
		return (r);
	}
	return (ARCHIVE_OK);
}

static int
archive_read_format_warc_cleanup(struct archive_read *a)
{
	struct warc *warc = a->format->data;

	if (warc->pool.len > 0U) {
		free(warc->pool.str);
	}
	archive_string_free(&warc->sver);
	free(warc);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}

static int
archive_read_format_warc_bid(struct archive_read *a, int best_bid)
{
	const char *hdr;
	ssize_t nrd;
	unsigned int ver;

	(void)best_bid; /* UNUSED */

	/* Check the first line, which should already be a record header. */
	if ((hdr = __archive_read_ahead(a, 12, &nrd)) == NULL) {
		/* Not enough data to identify this format. */
		return -1;
	}

	/* Parse the record version number. */
	ver = warc_read_version(hdr, nrd);
	if (ver < 1200U || ver > 10000U) {
		/* Only WARC 0.12 through WARC 1.0 are supported. */
		return -1;
	}

	/* WARC magic and version checks passed. */
	return (64);
}

static int
archive_read_format_warc_read_header(struct archive_read *a,
    struct archive_entry *entry)
{
#define HDR_PROBE_LEN		(12U)
	struct warc *warc = a->format->data;
	unsigned int ver;
	const char *buf;
	ssize_t nrd;
	const char *eoh;
	char *tmp;
	/* Reuse the header buffer while parsing the file name. */
	warc_string_t fnam;
	/* WARC record type */
	warc_type_t ftyp;
	/* Content length, or a negative error indicator */
	int64_t cntlen;
	/* WARC-Date is exposed as the entry ctime. */
	time_t rtime;
	/* A Last-Modified record header is exposed as the entry mtime. */
	time_t mtime;

start_over:
	/* Use read_ahead(); it already tracks unconsumed bytes, so this
	 * reader does not need a separate shift buffer. */
	buf = __archive_read_ahead(a, HDR_PROBE_LEN, &nrd);

	if (nrd < 0) {
		/* I/O or stream error. */
		archive_set_error(
			&a->archive, ARCHIVE_ERRNO_MISC,
			"Bad record header");
		return (ARCHIVE_FATAL);
	} else if (buf == NULL) {
		/* there should be room for at least WARC/bla\r\n
		 * must be EOF therefore */
		return (ARCHIVE_EOF);
	}
	/* Locate the end of the record header. */
	eoh = warc_find_eoh(buf, nrd);
	if (eoh == NULL) {
		/* The header terminator was not found in the probed data. */
		archive_set_error(
			&a->archive, ARCHIVE_ERRNO_MISC,
			"Bad record header");
		return (ARCHIVE_FATAL);
	}
	ver = warc_read_version(buf, eoh - buf);
	/* Only WARC 0.12 through WARC 1.0 are supported. */
	if (ver == 0U) {
		archive_set_error(
			&a->archive, ARCHIVE_ERRNO_MISC,
			"Invalid record version");
		return (ARCHIVE_FATAL);
	} else if (ver < 1200U || ver > 10000U) {
		archive_set_error(
			&a->archive, ARCHIVE_ERRNO_MISC,
			"Unsupported record version: %u.%u",
			ver / 10000, (ver % 10000) / 100);
		return (ARCHIVE_FATAL);
	}
	cntlen = warc_read_length(buf, eoh - buf);
	if (cntlen < 0) {
		/* This reader requires Content-Length before processing a record. */
		archive_set_error(
			&a->archive, EINVAL,
			"Bad content length");
		return (ARCHIVE_FATAL);
	}
	rtime = warc_read_date(buf, eoh - buf);
	if (rtime == (time_t)-1) {
		/* This reader requires WARC-Date before processing a record. */
		archive_set_error(
			&a->archive, EINVAL,
			"Bad record time");
		return (ARCHIVE_FATAL);
	}

	/* Report this archive as WARC. */
	a->archive.archive_format = ARCHIVE_FORMAT_WARC;
	if (ver != warc->pver) {
		/* Format this entry's WARC version. */
		archive_string_sprintf(&warc->sver,
			"WARC/%u.%u", ver / 10000, (ver % 10000) / 100);
		/* Remember the version for later entries. */
		warc->pver = ver;
	}
	/* Parse the record type. */
	ftyp = warc_read_type(buf, eoh - buf);
	/* Save content state for subsequent read calls. */
	warc->cntlen = cntlen;
	warc->cntoff = 0;
	mtime = 0;/* Avoid compiler warnings on some platforms. */

	switch (ftyp) {
	case WT_RSRC:
	case WT_RSP:
		/* Read the filename only for record types that are expected to
		 * have a target URI. */
		fnam = warc_read_uri(buf, eoh - buf);
		/* Avoid creating directory endpoints as files. */
		if (fnam.len == 0 || fnam.str[fnam.len - 1] == '/') {
			/* Skip this record. */
			fnam.len = 0U;
			fnam.str = NULL;
			break;
		}
		/* Copy the name into the reusable string pool to avoid a malloc/free
		 * roundtrip for each entry. */
		if (fnam.len + 1U > warc->pool.len) {
			warc->pool.len = ((fnam.len + 64U) / 64U) * 64U;
			tmp = realloc(warc->pool.str, warc->pool.len);
			if (tmp == NULL) {
				archive_set_error(
					&a->archive, ENOMEM,
					"Out of memory");
				return (ARCHIVE_FATAL);
			}
			warc->pool.str = tmp;
		}
		memcpy(warc->pool.str, fnam.str, fnam.len);
		warc->pool.str[fnam.len] = '\0';
		/* Hide the pool implementation behind the parsed string. */
		fnam.str = warc->pool.str;

		/* Use a Last-Modified record header when present; otherwise fall back
		 * to WARC-Date. */
		if ((mtime = warc_read_last_modified(buf, eoh - buf)) == (time_t)-1) {
			mtime = rtime;
		}
		break;
	case WT_NONE:
	case WT_INFO:
	case WT_META:
	case WT_REQ:
	case WT_RVIS:
	case WT_CONV:
	case WT_CONT:
	case LAST_WT:
	default:
		fnam.len = 0U;
		fnam.str = NULL;
		break;
	}

	/* Consume the record header. */
	__archive_read_consume(a, eoh - buf);

	switch (ftyp) {
	case WT_RSRC:
	case WT_RSP:
		if (fnam.len > 0U) {
			/* Populate the entry object. */
			archive_entry_set_filetype(entry, AE_IFREG);
			archive_entry_copy_pathname(entry, fnam.str);
			archive_entry_set_size(entry, cntlen);
			archive_entry_set_perm(entry, 0644);
			/* WARC-Date becomes ctime; mtime comes from Last-Modified or WARC-Date. */
			archive_entry_set_ctime(entry, rtime, 0L);
			archive_entry_set_mtime(entry, mtime, 0L);
			break;
		}
		/* FALLTHROUGH */
	case WT_NONE:
	case WT_INFO:
	case WT_META:
	case WT_REQ:
	case WT_RVIS:
	case WT_CONV:
	case WT_CONT:
	case LAST_WT:
	default:
		/* Skip this record body and look for the next one. */
		if (archive_read_format_warc_skip(a) < 0)
			return (ARCHIVE_FATAL);
		goto start_over;
	}
	return (ARCHIVE_OK);
}

static int
archive_read_format_warc_read_data(struct archive_read *a, const void **buf,
    size_t *bsz, int64_t *off)
{
	struct warc *warc = a->format->data;
	const char *rab;
	ssize_t nrd;

	if (warc->unconsumed) {
		__archive_read_consume(a, warc->unconsumed);
		warc->unconsumed = 0;
	}

	if (warc->cntoff >= warc->cntlen) {
		/* No data is available to return for this entry. */
		*buf = NULL;
		*bsz = 0U;
		*off = warc->cntoff;
		return (ARCHIVE_EOF);
	}

	rab = __archive_read_ahead(a, 1U, &nrd);
	if (nrd < 0) {
		*bsz = 0U;
		/* Propagate the read error. */
		return (int)nrd;
	} else if (nrd == 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated WARC file data");
		return (ARCHIVE_FATAL);
	} else if ((int64_t)nrd > warc->cntlen - warc->cntoff) {
		/* Clamp reads to Content-Length. */
		nrd = warc->cntlen - warc->cntoff;
	}
	*off = warc->cntoff;
	*bsz = nrd;
	*buf = rab;

	warc->cntoff += nrd;
	warc->unconsumed = nrd;
	return (ARCHIVE_OK);
}

static int
archive_read_format_warc_skip(struct archive_read *a)
{
	struct warc *warc = a->format->data;

	if (warc->cntoff > warc->cntlen)
		return (ARCHIVE_FATAL);
	if (warc->unconsumed) {
		__archive_read_consume(a, warc->unconsumed);
		warc->unconsumed = 0;
	}
	if (__archive_read_consume(a, warc->cntlen - warc->cntoff) < 0 ||
	    __archive_read_consume(a, 4U/*\r\n\r\n separator*/) < 0)
		return (ARCHIVE_FATAL);
	warc->cntlen = 0;
	warc->cntoff = 0;
	return (ARCHIVE_OK);
}


/* Private routines */
static void*
deconst(const void *c)
{
	return (void *)(uintptr_t)c;
}

static char*
xmemmem(const char *hay, const size_t haysize,
	const char *needle, const size_t needlesize)
{
	const char *const eoh = hay + haysize;
	const char *const eon = needle + needlesize;
	const char *hp;
	const char *np;
	const char *cand;
	unsigned int hsum;
	unsigned int nsum;
	unsigned int eqp;

	/* Handle trivial cases first.  A zero-sized needle is defined to be
	 * found anywhere in the haystack; otherwise find the first candidate
	 * that begins with *NEEDLE. */
	if (needlesize == 0UL) {
		return deconst(hay);
	} else if ((hay = memchr(hay, *needle, haysize)) == NULL) {
		/* No candidate match remains. */
		return NULL;
	}

	/* The first characters of haystack and needle already match, and both
	 * strings are at least one character long.  Compute the rolling XOR
	 * values for the needle and the first NEEDLESIZE characters of haystack. */
	for (hp = hay + 1U, np = needle + 1U, hsum = *hay, nsum = *hay, eqp = 1U;
	     hp < eoh && np < eon;
	     hsum ^= *hp, nsum ^= *np, eqp &= *hp == *np, hp++, np++);

	/* HP now references the (NEEDLESIZE + 1)-th character. */
	if (np < eon) {
		/* The haystack is smaller than the needle. */
		return NULL;
	} else if (eqp) {
		/* Found a match. */
		return deconst(hay);
	}

	/* Loop through the rest of the haystack and update the rolling XOR
	 * iteratively. */
	for (cand = hay; hp < eoh; hp++) {
		hsum ^= *cand++;
		hsum ^= *hp;

		/* When the rolling XOR values match, it is enough to check
		 * NEEDLESIZE - 1 characters for equality.  CAND is always before
		 * HP by design, so no range check is needed. */
		if (hsum == nsum && memcmp(cand, needle, needlesize - 1U) == 0) {
			return deconst(cand);
		}
	}
	return NULL;
}

static int
strtoi_lim(const char *str, const char **ep, int llim, int ulim)
{
	int res = 0;
	const char *sp;
	/* Track the number of digits with rulim. */
	int rulim;

	for (sp = str, rulim = ulim > 10 ? ulim : 10;
	     res * 10 <= ulim && rulim && *sp >= '0' && *sp <= '9';
	     sp++, rulim /= 10) {
		res *= 10;
		res += *sp - '0';
	}
	if (sp == str) {
		res = -1;
	} else if (res < llim || res > ulim) {
		res = -2;
	}
	*ep = (const char*)sp;
	return res;
}

static time_t
time_from_tm(struct tm *t)
{
#if HAVE__MKGMTIME
        return _mkgmtime(t);
#elif HAVE_TIMEGM
        /* Use platform timegm() if available. */
        return (timegm(t));
#else
        /* Otherwise, calculate directly using POSIX assumptions. */
        /* First, fix up tm_yday based on the year, month, and day. */
        if (mktime(t) == (time_t)-1)
                return ((time_t)-1);
        /* Then compute timegm() from first principles. */
        return (t->tm_sec
            + t->tm_min * 60
            + t->tm_hour * 3600
            + t->tm_yday * 86400
            + (t->tm_year - 70) * 31536000
            + ((t->tm_year - 69) / 4) * 86400
            - ((t->tm_year - 1) / 100) * 86400
            + ((t->tm_year + 299) / 400) * 86400);
#endif
}

static time_t
xstrpisotime(const char *s, char **endptr)
{
/* Like strptime(), but only for ISO 8601 Zulu strings. */
	struct tm tm;
	time_t res = (time_t)-1;

	/* Clear the tm structure. */
	memset(&tm, 0, sizeof(tm));

	/* This is a non-standard routine, so skip leading whitespace for
	 * caller convenience. */
	while (*s == ' ' || *s == '\t')
		++s;

	/* Read the year. */
	if ((tm.tm_year = strtoi_lim(s, &s, 1583, 4095)) < 0 || *s++ != '-') {
		goto out;
	}
	/* Read the month. */
	if ((tm.tm_mon = strtoi_lim(s, &s, 1, 12)) < 0 || *s++ != '-') {
		goto out;
	}
	/* Read the day of the month. */
	if ((tm.tm_mday = strtoi_lim(s, &s, 1, 31)) < 0 || *s++ != 'T') {
		goto out;
	}
	/* Read the hour. */
	if ((tm.tm_hour = strtoi_lim(s, &s, 0, 23)) < 0 || *s++ != ':') {
		goto out;
	}
	/* Read the minute. */
	if ((tm.tm_min = strtoi_lim(s, &s, 0, 59)) < 0 || *s++ != ':') {
		goto out;
	}
	/* Read the second. */
	if ((tm.tm_sec = strtoi_lim(s, &s, 0, 60)) < 0 || *s++ != 'Z') {
		goto out;
	}

	/* Adjust tm fields to satisfy POSIX constraints. */
	tm.tm_year -= 1900;
	tm.tm_mon--;

	/* Convert the tm structure to a Unix timestamp in UTC. */
	res = time_from_tm(&tm);

out:
	if (endptr != NULL) {
		*endptr = deconst(s);
	}
	return res;
}

static int
warc_isdigit(const char c)
{
	return c >= '0' && c <= '9';
}

static unsigned int
warc_read_version(const char *buf, size_t bsz)
{
	static const char magic[] = "WARC/";
	const char *c;
	unsigned int ver = 0U;
	unsigned int end = 0U;

	if (bsz < 12 || memcmp(buf, magic, sizeof(magic) - 1U) != 0) {
		/* Buffer too small or invalid magic. */
		return ver;
	}
	/* Parse the version number. */
	buf += sizeof(magic) - 1U;

	if (warc_isdigit(buf[0]) && buf[1] == '.' && warc_isdigit(buf[2])) {
		/* Support at most two digits in the minor version. */
		if (warc_isdigit(buf[3]))
			end = 1U;
		/* Set up the major version. */
		ver = (buf[0U] - '0') * 10000U;
		/* Set up the minor version. */
		if (end == 1U) {
			ver += (buf[2U] - '0') * 1000U;
			ver += (buf[3U] - '0') * 100U;
		} else
			ver += (buf[2U] - '0') * 100U;
		/*
		 * WARC versions before 0.12 use a space-separated header.
		 * WARC 0.12 and later terminate the version with CRLF.
		 */
		c = buf + 3U + end;
		if (ver >= 1200U) {
			if (memcmp(c, "\r\n", 2U) != 0)
				ver = 0U;
		} else {
			/* Version is below WARC 0.12. */
			if (*c != ' ' && *c != '\t')
				ver = 0U;
		}
	}
	return ver;
}

static unsigned int
warc_read_type(const char *buf, size_t bsz)
{
	static const char _key[] = "\r\nWARC-Type:";
	const char *val, *eol;

	if ((val = xmemmem(buf, bsz, _key, sizeof(_key) - 1U)) == NULL) {
		/* Header field is absent. */
		return WT_NONE;
	}
	val += sizeof(_key) - 1U;
	if ((eol = warc_find_eol(val, buf + bsz - val)) == NULL) {
		/* Header field has no end of line. */
		return WT_NONE;
	}

	/* Skip leading whitespace. */
	while (val < eol && (*val == ' ' || *val == '\t'))
		++val;

	if (val + 8U == eol) {
		if (memcmp(val, "resource", 8U) == 0)
			return WT_RSRC;
		else if (memcmp(val, "response", 8U) == 0)
			return WT_RSP;
	}
	return WT_NONE;
}

static warc_string_t
warc_read_uri(const char *buf, size_t bsz)
{
	static const char _key[] = "\r\nWARC-Target-URI:";
	const char *val, *uri, *eol, *p;
	warc_string_t res = {0U, NULL};

	if ((val = xmemmem(buf, bsz, _key, sizeof(_key) - 1U)) == NULL) {
		/* Header field is absent. */
		return res;
	}
	/* Skip leading whitespace. */
	val += sizeof(_key) - 1U;
	if ((eol = warc_find_eol(val, buf + bsz - val)) == NULL) {
		/* Header field has no end of line. */
		return res;
	}

	while (val < eol && (*val == ' ' || *val == '\t'))
		++val;

	/* Locate the :// separator. */
	if ((uri = xmemmem(val, eol - val, "://", 3U)) == NULL) {
		/* Ignore values without a :// separator. */
		return res;
	}

	/* Spaces inside a URI are not allowed; CRLF should follow. */
	for (p = val; p < eol; p++) {
		if (isspace((unsigned char)*p))
			return res;
	}

	/* Require enough room for the shortest supported scheme. */
	if (uri < (val + 3U))
		return res;

	/* Move uri past the :// separator. */
	uri += 3U;

	/* Inspect the scheme prefix. */
	if (memcmp(val, "file", 4U) == 0) {
		/* Keep file:// paths as-is. */

	} else if (memcmp(val, "http", 4U) == 0 ||
		   memcmp(val, "ftp", 3U) == 0) {
		/* Skip the domain and the first slash. */
		while (uri < eol && *uri++ != '/');
	} else {
		/* Unsupported URI scheme. */
		return res;
	}
	res.str = uri;
	res.len = eol - uri;
	return res;
}

static int64_t
warc_read_length(const char *buf, size_t bsz)
{
	static const char _key[] = "\r\nContent-Length:";
	const char *val, *eol, *p;
	int64_t len;

	if ((val = xmemmem(buf, bsz, _key, sizeof(_key) - 1U)) == NULL) {
		/* Header field is absent. */
		return -1;
	}
	val += sizeof(_key) - 1U;

	if ((eol = warc_find_eol(val, buf + bsz - val)) == NULL) {
		/* Malformed field with no end of line. */
		return -1;
	}

	/* Skip leading whitespace. */
	while (val < eol && (*val == ' ' || *val == '\t'))
		val++;

	/* Require at least one digit. */
	if (val >= eol || *val < '0' || *val > '9')
		return -1;

	len = 0;
	for (p = val; p < eol; p++) {
		int64_t digit;

		if (*p < '0' || *p > '9')
			return -1;
		digit = *p - '0';
		if (archive_ckd_mul_i64(&len, len, 10) ||
		    archive_ckd_add_i64(&len, len, digit))
			return -1;
	}

	return len;
}

static time_t
warc_read_date(const char *buf, size_t bsz)
{
	static const char _key[] = "\r\nWARC-Date:";
	const char *val, *eol;
	char *on = NULL;
	time_t res;

	if ((val = xmemmem(buf, bsz, _key, sizeof(_key) - 1U)) == NULL) {
		/* Header field is absent. */
		return (time_t)-1;
	}
	val += sizeof(_key) - 1U;
	if ((eol = warc_find_eol(val, buf + bsz - val)) == NULL ) {
		/* Header field has no end of line. */
		return -1;
	}

	/* xstrpisotime() skips leading whitespace. */
	res = xstrpisotime(val, &on);
	if (on != eol) {
		/* The field must end here. */
		return -1;
	}
	return res;
}

static time_t
warc_read_last_modified(const char *buf, size_t bsz)
{
	static const char _key[] = "\r\nLast-Modified:";
	const char *val, *eol;
	char *on = NULL;
	time_t res;

	if ((val = xmemmem(buf, bsz, _key, sizeof(_key) - 1U)) == NULL) {
		/* Header field is absent. */
		return (time_t)-1;
	}
	val += sizeof(_key) - 1U;
	if ((eol = warc_find_eol(val, buf + bsz - val)) == NULL ) {
		/* Header field has no end of line. */
		return -1;
	}

	/* xstrpisotime() skips leading whitespace. */
	res = xstrpisotime(val, &on);
	if (on != eol) {
		/* The field must end here. */
		return -1;
	}
	return res;
}

static const char *
warc_find_eoh(const char *buf, size_t bsz)
{
	static const char _marker[] = "\r\n\r\n";
	const char *hit = xmemmem(buf, bsz, _marker, sizeof(_marker) - 1U);

	if (hit != NULL) {
		hit += sizeof(_marker) - 1U;
	}
	return hit;
}

static const char *
warc_find_eol(const char *buf, size_t bsz)
{
	static const char _marker[] = "\r\n";
	const char *hit = xmemmem(buf, bsz, _marker, sizeof(_marker) - 1U);

	return hit;
}
