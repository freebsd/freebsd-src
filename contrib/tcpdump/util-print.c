/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * txtproto_print() derived from original code by Hannes Gredler
 * (hannes@gredler.at):
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

#include <config.h>

#include "netdissect-stdinc.h"

#include <sys/stat.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "netdissect-ctype.h"

#include "netdissect.h"
#include "extract.h"
#include "ascii_strcasecmp.h"
#include "timeval-operations.h"

#define TOKBUFSIZE 128

enum date_flag { WITHOUT_DATE = 0, WITH_DATE = 1 };
enum time_flag { UTC_TIME = 0, LOCAL_TIME = 1 };

/*
 * Print out a character, filtering out the non-printable ones
 */
void
fn_print_char(netdissect_options *ndo, u_char c)
{
	if (!ND_ISASCII(c)) {
		c = ND_TOASCII(c);
		ND_PRINT("M-");
	}
	if (!ND_ASCII_ISPRINT(c)) {
		c ^= 0x40;	/* DEL to ?, others to alpha */
		ND_PRINT("^");
	}
	ND_PRINT("%c", c);
}

/*
 * Print a null-terminated string, filtering out non-printable characters.
 * DON'T USE IT with a pointer on the packet buffer because there is no
 * truncation check. For this use, see the nd_printX() functions below.
 */
void
fn_print_str(netdissect_options *ndo, const u_char *s)
{
	while (*s != '\0') {
		fn_print_char(ndo, *s);
		s++;
       }
}

/*
 * Print out a null-terminated filename (or other ASCII string) from
 * a fixed-length field in the packet buffer, or from what remains of
 * the packet.
 *
 * n is the length of the fixed-length field, or the number of bytes
 * remaining in the packet based on its on-the-network length.
 *
 * If ep is non-null, it should point just past the last captured byte
 * of the packet, e.g. ndo->ndo_snapend.  If ep is NULL, we assume no
 * truncation check, other than the checks of the field length/remaining
 * packet data length, is needed.
 *
 * Return the number of bytes of string processed, including the
 * terminating null, if not truncated; as the terminating null is
 * included in the count, and as there must be a terminating null,
 * this will always be non-zero.  Return 0 if truncated.
 */
u_int
nd_printztn(netdissect_options *ndo,
         const u_char *s, u_int n, const u_char *ep)
{
	u_int bytes;
	u_char c;

	bytes = 0;
	for (;;) {
		if (n == 0 || (ep != NULL && s >= ep)) {
			/*
			 * Truncated.  This includes "no null before we
			 * got to the end of the fixed-length buffer or
			 * the end of the packet".
			 *
			 * XXX - BOOTP says "null-terminated", which
			 * means the maximum length of the string, in
			 * bytes, is 1 less than the size of the buffer,
			 * as there must always be a terminating null.
			 */
			bytes = 0;
			break;
		}

		c = GET_U_1(s);
		s++;
		bytes++;
		n--;
		if (c == '\0') {
			/* End of string */
			break;
		}
		fn_print_char(ndo, c);
	}
	return(bytes);
}

/*
 * Print out a counted filename (or other ASCII string), part of
 * the packet buffer.
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 * Stop at ep (if given) or after n bytes, whichever is first.
 */
int
nd_printn(netdissect_options *ndo,
          const u_char *s, u_int n, const u_char *ep)
{
	u_char c;

	while (n > 0 && (ep == NULL || s < ep)) {
		n--;
		c = GET_U_1(s);
		s++;
		fn_print_char(ndo, c);
	}
	return (n == 0) ? 0 : 1;
}

/*
 * Print a counted filename (or other ASCII string), part of
 * the packet buffer, filtering out non-printable characters.
 * Stop if truncated (via GET_U_1/longjmp) or after n bytes,
 * whichever is first.
 * The suffix comes from: j:longJmp, n:after N bytes.
 */
void
nd_printjn(netdissect_options *ndo, const u_char *s, u_int n)
{
	while (n > 0) {
		fn_print_char(ndo, GET_U_1(s));
		n--;
		s++;
	}
}

/*
 * Print a null-padded filename (or other ASCII string), part of
 * the packet buffer, filtering out non-printable characters.
 * Stop if truncated (via GET_U_1/longjmp) or after n bytes or before
 * the null char, whichever occurs first.
 * The suffix comes from: j:longJmp, n:after N bytes, p:null-Padded.
 */
void
nd_printjnp(netdissect_options *ndo, const u_char *s, u_int n)
{
	u_char c;

	while (n > 0) {
		c = GET_U_1(s);
		if (c == '\0')
			break;
		fn_print_char(ndo, c);
		n--;
		s++;
	}
}

/*
 * Print the timestamp .FRAC part (Microseconds/nanoseconds)
 */
static void
ts_frac_print(netdissect_options *ndo, const struct timeval *tv)
{
#ifdef HAVE_PCAP_SET_TSTAMP_PRECISION
	switch (ndo->ndo_tstamp_precision) {

	case PCAP_TSTAMP_PRECISION_MICRO:
		ND_PRINT(".%06u", (unsigned)tv->tv_usec);
		break;

	case PCAP_TSTAMP_PRECISION_NANO:
		ND_PRINT(".%09u", (unsigned)tv->tv_usec);
		break;

	default:
		ND_PRINT(".{unknown}");
		break;
	}
#else
	ND_PRINT(".%06u", (unsigned)tv->tv_usec);
#endif
}

/*
 * Print the timestamp as [YY:MM:DD] HH:MM:SS.FRAC.
 *   if time_flag == LOCAL_TIME print local time else UTC/GMT time
 *   if date_flag == WITH_DATE print YY:MM:DD before HH:MM:SS.FRAC
 */
static void
ts_date_hmsfrac_print(netdissect_options *ndo, const struct timeval *tv,
		      enum date_flag date_flag, enum time_flag time_flag)
{
	struct tm *tm;
	char timebuf[32];
	const char *timestr;

	if (tv->tv_sec < 0) {
		ND_PRINT("[timestamp < 1970-01-01 00:00:00 UTC]");
		return;
	}

	if (time_flag == LOCAL_TIME)
		tm = localtime(&tv->tv_sec);
	else
		tm = gmtime(&tv->tv_sec);

	if (date_flag == WITH_DATE) {
		timestr = nd_format_time(timebuf, sizeof(timebuf),
		    "%Y-%m-%d %H:%M:%S", tm);
	} else {
		timestr = nd_format_time(timebuf, sizeof(timebuf),
		    "%H:%M:%S", tm);
	}
	ND_PRINT("%s", timestr);

	ts_frac_print(ndo, tv);
}

/*
 * Print the timestamp - Unix timeval style, as SECS.FRAC.
 */
static void
ts_unix_print(netdissect_options *ndo, const struct timeval *tv)
{
	if (tv->tv_sec < 0) {
		ND_PRINT("[timestamp < 1970-01-01 00:00:00 UTC]");
		return;
	}

	ND_PRINT("%u", (unsigned)tv->tv_sec);
	ts_frac_print(ndo, tv);
}

/*
 * Print the timestamp
 */
void
ts_print(netdissect_options *ndo,
         const struct timeval *tvp)
{
	static struct timeval tv_ref;
	struct timeval tv_result;
	int negative_offset;
	int nano_prec;

	switch (ndo->ndo_tflag) {

	case 0: /* Default */
		ts_date_hmsfrac_print(ndo, tvp, WITHOUT_DATE, LOCAL_TIME);
		ND_PRINT(" ");
		break;

	case 1: /* No time stamp */
		break;

	case 2: /* Unix timeval style */
		ts_unix_print(ndo, tvp);
		ND_PRINT(" ");
		break;

	case 3: /* Microseconds/nanoseconds since previous packet */
        case 5: /* Microseconds/nanoseconds since first packet */
#ifdef HAVE_PCAP_SET_TSTAMP_PRECISION
		switch (ndo->ndo_tstamp_precision) {
		case PCAP_TSTAMP_PRECISION_MICRO:
			nano_prec = 0;
			break;
		case PCAP_TSTAMP_PRECISION_NANO:
			nano_prec = 1;
			break;
		default:
			nano_prec = 0;
			break;
		}
#else
		nano_prec = 0;
#endif
		if (!(netdissect_timevalisset(&tv_ref)))
			tv_ref = *tvp; /* set timestamp for first packet */

		negative_offset = netdissect_timevalcmp(tvp, &tv_ref, <);
		if (negative_offset)
			netdissect_timevalsub(&tv_ref, tvp, &tv_result, nano_prec);
		else
			netdissect_timevalsub(tvp, &tv_ref, &tv_result, nano_prec);

		ND_PRINT((negative_offset ? "-" : " "));
		ts_date_hmsfrac_print(ndo, &tv_result, WITHOUT_DATE, UTC_TIME);
		ND_PRINT(" ");

                if (ndo->ndo_tflag == 3)
			tv_ref = *tvp; /* set timestamp for previous packet */
		break;

	case 4: /* Date + Default */
		ts_date_hmsfrac_print(ndo, tvp, WITH_DATE, LOCAL_TIME);
		ND_PRINT(" ");
		break;
	}
}

/*
 * Print an unsigned relative number of seconds (e.g. hold time, prune timer)
 * in the form 5m1s.  This does no truncation, so 32230861 seconds
 * is represented as 1y1w1d1h1m1s.
 */
void
unsigned_relts_print(netdissect_options *ndo,
                     uint32_t secs)
{
	static const char *lengths[] = {"y", "w", "d", "h", "m", "s"};
	static const u_int seconds[] = {31536000, 604800, 86400, 3600, 60, 1};
	const char **l = lengths;
	const u_int *s = seconds;

	if (secs == 0) {
		ND_PRINT("0s");
		return;
	}
	while (secs > 0) {
		if (secs >= *s) {
			ND_PRINT("%u%s", secs / *s, *l);
			secs -= (secs / *s) * *s;
		}
		s++;
		l++;
	}
}

/*
 * Print a signed relative number of seconds (e.g. hold time, prune timer)
 * in the form 5m1s.  This does no truncation, so 32230861 seconds
 * is represented as 1y1w1d1h1m1s.
 */
void
signed_relts_print(netdissect_options *ndo,
                   int32_t secs)
{
	if (secs < 0) {
		ND_PRINT("-");
		if (secs == INT32_MIN) {
			/*
			 * -2^31; you can't fit its absolute value into
			 * a 32-bit signed integer.
			 *
			 * Just directly pass said absolute value to
			 * unsigned_relts_print() directly.
			 *
			 * (XXX - does ISO C guarantee that -(-2^n),
			 * when calculated and cast to an n-bit unsigned
			 * integer type, will have the value 2^n?)
			 */
			unsigned_relts_print(ndo, 2147483648U);
		} else {
			/*
			 * We now know -secs will fit into an int32_t;
			 * negate it and pass that to unsigned_relts_print().
			 */
			unsigned_relts_print(ndo, -secs);
		}
		return;
	}
	unsigned_relts_print(ndo, secs);
}

/*
 * Format a struct tm with strftime().
 * If the pointer to the struct tm is null, that means that the
 * routine to convert a time_t to a struct tm failed; the localtime()
 * and gmtime() in the Microsoft Visual Studio C library will fail,
 * returning null, if the value is before the UNIX Epoch.
 */
const char *
nd_format_time(char *buf, size_t bufsize, const char *format,
         const struct tm *timeptr)
{
	if (timeptr != NULL) {
		if (strftime(buf, bufsize, format, timeptr) != 0)
			return (buf);
		else
			return ("[nd_format_time() buffer is too small]");
	} else
		return ("[localtime() or gmtime() couldn't convert the date and time]");
}

/* Print the truncated string */
void nd_print_trunc(netdissect_options *ndo)
{
	ND_PRINT(" [|%s]", ndo->ndo_protocol);
}

/* Print the protocol name */
void nd_print_protocol(netdissect_options *ndo)
{
	ND_PRINT("%s", ndo->ndo_protocol);
}

/* Print the protocol name in caps (uppercases) */
void nd_print_protocol_caps(netdissect_options *ndo)
{
	const char *p;
        for (p = ndo->ndo_protocol; *p != '\0'; p++)
                ND_PRINT("%c", ND_ASCII_TOUPPER(*p));
}

/* Print the invalid string */
void nd_print_invalid(netdissect_options *ndo)
{
	ND_PRINT(" (invalid)");
}

/*
 *  this is a generic routine for printing unknown data;
 *  we pass on the linefeed plus indentation string to
 *  get a proper output - returns 0 on error
 */

int
print_unknown_data(netdissect_options *ndo, const u_char *cp,
                   const char *ident, u_int len)
{
	u_int len_to_print;

	len_to_print = len;
	if (!ND_TTEST_LEN(cp, 0)) {
		ND_PRINT("%sDissector error: print_unknown_data called with pointer past end of packet",
		    ident);
		return(0);
	}
	if (ND_BYTES_AVAILABLE_AFTER(cp) < len_to_print)
		len_to_print = ND_BYTES_AVAILABLE_AFTER(cp);
	hex_print(ndo, ident, cp, len_to_print);
	return(1); /* everything is ok */
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 */
static const char *
tok2strbuf(const struct tok *lp, const char *fmt,
	   u_int v, char *buf, size_t bufsize)
{
	if (lp != NULL) {
		while (lp->s != NULL) {
			if (lp->v == v)
				return (lp->s);
			++lp;
		}
	}
	if (fmt == NULL)
		fmt = "#%d";

	(void)snprintf(buf, bufsize, fmt, v);
	return (const char *)buf;
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 * Uses tok2strbuf() on one of four local static buffers of size TOKBUFSIZE
 * in round-robin fashion.
 */
const char *
tok2str(const struct tok *lp, const char *fmt,
	u_int v)
{
	static char buf[4][TOKBUFSIZE];
	static int idx = 0;
	char *ret;

	ret = buf[idx];
	idx = (idx+1) & 3;
	return tok2strbuf(lp, fmt, v, ret, sizeof(buf[0]));
}

/*
 * Convert a bit token value to a string; use "fmt" if not found.
 * this is useful for parsing bitfields, the output strings are separated
 * if the s field is positive.
 *
 * A token matches iff it has one or more bits set and every bit that is set
 * in the token is set in v. Consequently, a 0 token never matches.
 */
static char *
bittok2str_internal(const struct tok *lp, const char *fmt,
	   u_int v, const char *sep)
{
        static char buf[1024+1]; /* our string buffer */
        char *bufp = buf;
        size_t space_left = sizeof(buf), string_size;
        const char * sepstr = "";

        while (lp != NULL && lp->s != NULL) {
            if (lp->v && (v & lp->v) == lp->v) {
                /* ok we have found something */
                if (space_left <= 1)
                    return (buf); /* only enough room left for NUL, if that */
                string_size = strlcpy(bufp, sepstr, space_left);
                if (string_size >= space_left)
                    return (buf);    /* we ran out of room */
                bufp += string_size;
                space_left -= string_size;
                if (space_left <= 1)
                    return (buf); /* only enough room left for NUL, if that */
                string_size = strlcpy(bufp, lp->s, space_left);
                if (string_size >= space_left)
                    return (buf);    /* we ran out of room */
                bufp += string_size;
                space_left -= string_size;
                sepstr = sep;
            }
            lp++;
        }

        if (bufp == buf)
            /* bummer - lets print the "unknown" message as advised in the fmt string if we got one */
            (void)snprintf(buf, sizeof(buf), fmt == NULL ? "#%08x" : fmt, v);
        return (buf);
}

/*
 * Convert a bit token value to a string; use "fmt" if not found.
 * this is useful for parsing bitfields, the output strings are not separated.
 */
char *
bittok2str_nosep(const struct tok *lp, const char *fmt,
	   u_int v)
{
    return (bittok2str_internal(lp, fmt, v, ""));
}

/*
 * Convert a bit token value to a string; use "fmt" if not found.
 * this is useful for parsing bitfields, the output strings are comma separated.
 */
char *
bittok2str(const struct tok *lp, const char *fmt,
	   u_int v)
{
    return (bittok2str_internal(lp, fmt, v, ", "));
}

/*
 * Convert a value to a string using an array; the macro
 * tok2strary() in <netdissect.h> is the public interface to
 * this function and ensures that the second argument is
 * correct for bounds-checking.
 */
const char *
tok2strary_internal(const char **lp, int n, const char *fmt,
	int v)
{
	static char buf[TOKBUFSIZE];

	if (v >= 0 && v < n && lp[v] != NULL)
		return lp[v];
	if (fmt == NULL)
		fmt = "#%d";
	(void)snprintf(buf, sizeof(buf), fmt, v);
	return (buf);
}

const struct tok *
uint2tokary_internal(const struct uint_tokary dict[], const size_t size,
                     const u_int val)
{
	size_t i;
	/* Try a direct lookup before the full scan. */
	if (val < size && dict[val].uintval == val)
		return dict[val].tokary; /* OK if NULL */
	for (i = 0; i < size; i++)
		if (dict[i].uintval == val)
			return dict[i].tokary; /* OK if NULL */
	return NULL;
}

/*
 * Convert a 32-bit netmask to prefixlen if possible
 * the function returns the prefix-len; if plen == -1
 * then conversion was not possible;
 */

int
mask2plen(uint32_t mask)
{
	const uint32_t bitmasks[33] = {
		0x00000000,
		0x80000000, 0xc0000000, 0xe0000000, 0xf0000000,
		0xf8000000, 0xfc000000, 0xfe000000, 0xff000000,
		0xff800000, 0xffc00000, 0xffe00000, 0xfff00000,
		0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000,
		0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000,
		0xfffff800, 0xfffffc00, 0xfffffe00, 0xffffff00,
		0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0,
		0xfffffff8, 0xfffffffc, 0xfffffffe, 0xffffffff
	};
	int prefix_len = 32;

	/* let's see if we can transform the mask into a prefixlen */
	while (prefix_len >= 0) {
		if (bitmasks[prefix_len] == mask)
			break;
		prefix_len--;
	}
	return (prefix_len);
}

int
mask62plen(const u_char *mask)
{
	u_char bitmasks[9] = {
		0x00,
		0x80, 0xc0, 0xe0, 0xf0,
		0xf8, 0xfc, 0xfe, 0xff
	};
	int byte;
	int cidr_len = 0;

	for (byte = 0; byte < 16; byte++) {
		u_int bits;

		for (bits = 0; bits < (sizeof (bitmasks) / sizeof (bitmasks[0])); bits++) {
			if (mask[byte] == bitmasks[bits]) {
				cidr_len += bits;
				break;
			}
		}

		if (mask[byte] != 0xff)
			break;
	}
	return (cidr_len);
}

/*
 * Routine to print out information for text-based protocols such as FTP,
 * HTTP, SMTP, RTSP, SIP, ....
 */
#define MAX_TOKEN	128

/*
 * Fetch a token from a packet, starting at the specified index,
 * and return the length of the token.
 *
 * Returns 0 on error; yes, this is indistinguishable from an empty
 * token, but an "empty token" isn't a valid token - it just means
 * either a space character at the beginning of the line (this
 * includes a blank line) or no more tokens remaining on the line.
 */
static int
fetch_token(netdissect_options *ndo, const u_char *pptr, u_int idx, u_int len,
    u_char *tbuf, size_t tbuflen)
{
	size_t toklen = 0;
	u_char c;

	for (; idx < len; idx++) {
		if (!ND_TTEST_1(pptr + idx)) {
			/* ran past end of captured data */
			return (0);
		}
		c = GET_U_1(pptr + idx);
		if (!ND_ISASCII(c)) {
			/* not an ASCII character */
			return (0);
		}
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			/* end of token */
			break;
		}
		if (!ND_ASCII_ISPRINT(c)) {
			/* not part of a command token or response code */
			return (0);
		}
		if (toklen + 2 > tbuflen) {
			/* no room for this character and terminating '\0' */
			return (0);
		}
		tbuf[toklen] = c;
		toklen++;
	}
	if (toklen == 0) {
		/* no token */
		return (0);
	}
	tbuf[toklen] = '\0';

	/*
	 * Skip past any white space after the token, until we see
	 * an end-of-line (CR or LF).
	 */
	for (; idx < len; idx++) {
		if (!ND_TTEST_1(pptr + idx)) {
			/* ran past end of captured data */
			break;
		}
		c = GET_U_1(pptr + idx);
		if (c == '\r' || c == '\n') {
			/* end of line */
			break;
		}
		if (!ND_ASCII_ISPRINT(c)) {
			/* not a printable ASCII character */
			break;
		}
		if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
			/* beginning of next token */
			break;
		}
	}
	return (idx);
}

/*
 * Scan a buffer looking for a line ending - LF or CR-LF.
 * Return the index of the character after the line ending or 0 if
 * we encounter a non-ASCII or non-printable character or don't find
 * the line ending.
 */
static u_int
print_txt_line(netdissect_options *ndo, const char *prefix,
	       const u_char *pptr, u_int idx, u_int len)
{
	u_int startidx;
	u_int linelen;
	u_char c;

	startidx = idx;
	while (idx < len) {
		c = GET_U_1(pptr + idx);
		if (c == '\n') {
			/*
			 * LF without CR; end of line.
			 * Skip the LF and print the line, with the
			 * exception of the LF.
			 */
			linelen = idx - startidx;
			idx++;
			goto print;
		} else if (c == '\r') {
			/* CR - any LF? */
			if ((idx+1) >= len) {
				/* not in this packet */
				return (0);
			}
			if (GET_U_1(pptr + idx + 1) == '\n') {
				/*
				 * CR-LF; end of line.
				 * Skip the CR-LF and print the line, with
				 * the exception of the CR-LF.
				 */
				linelen = idx - startidx;
				idx += 2;
				goto print;
			}

			/*
			 * CR followed by something else; treat this
			 * as if it were binary data, and don't print
			 * it.
			 */
			return (0);
		} else if (!ND_ASCII_ISPRINT(c) && c != '\t') {
			/*
			 * Not a printable ASCII character and not a tab;
			 * treat this as if it were binary data, and
			 * don't print it.
			 */
			return (0);
		}
		idx++;
	}

	/*
	 * All printable ASCII, but no line ending after that point
	 * in the buffer.
	 */
	linelen = idx - startidx;
	ND_PRINT("%s%.*s", prefix, (int)linelen, pptr + startidx);
	return (0);

print:
	ND_PRINT("%s%.*s", prefix, (int)linelen, pptr + startidx);
	return (idx);
}

/* Assign needed before calling txtproto_print(): ndo->ndo_protocol = "proto" */
void
txtproto_print(netdissect_options *ndo, const u_char *pptr, u_int len,
	       const char **cmds, u_int flags)
{
	u_int idx, eol;
	u_char token[MAX_TOKEN+1];
	const char *cmd;
	int print_this = 0;

	if (cmds != NULL) {
		/*
		 * This protocol has more than just request and
		 * response lines; see whether this looks like a
		 * request or response and, if so, print it and,
		 * in verbose mode, print everything after it.
		 *
		 * This is for HTTP-like protocols, where we
		 * want to print requests and responses, but
		 * don't want to print continuations of request
		 * or response bodies in packets that don't
		 * contain the request or response line.
		 */
		idx = fetch_token(ndo, pptr, 0, len, token, sizeof(token));
		if (idx != 0) {
			/* Is this a valid request name? */
			while ((cmd = *cmds++) != NULL) {
				if (ascii_strcasecmp((const char *)token, cmd) == 0) {
					/* Yes. */
					print_this = 1;
					break;
				}
			}

			/*
			 * No - is this a valid response code (3 digits)?
			 *
			 * Is this token the response code, or is the next
			 * token the response code?
			 */
			if (flags & RESP_CODE_SECOND_TOKEN) {
				/*
				 * Next token - get it.
				 */
				idx = fetch_token(ndo, pptr, idx, len, token,
				    sizeof(token));
			}
			if (idx != 0) {
				if (ND_ASCII_ISDIGIT(token[0]) && ND_ASCII_ISDIGIT(token[1]) &&
				    ND_ASCII_ISDIGIT(token[2]) && token[3] == '\0') {
					/* Yes. */
					print_this = 1;
				}
			}
		}
	} else {
		/*
		 * Either:
		 *
		 * 1) This protocol has only request and response lines
		 *    (e.g., FTP, where all the data goes over a different
		 *    connection); assume the payload is a request or
		 *    response.
		 *
		 * or
		 *
		 * 2) This protocol is just text, so that we should
		 *    always, at minimum, print the first line and,
		 *    in verbose mode, print all lines.
		 */
		print_this = 1;
	}

	nd_print_protocol_caps(ndo);

	if (print_this) {
		/*
		 * In non-verbose mode, just print the protocol, followed
		 * by the first line.
		 *
		 * In verbose mode, print lines as text until we run out
		 * of characters or see something that's not a
		 * printable-ASCII line.
		 */
		if (ndo->ndo_vflag) {
			/*
			 * We're going to print all the text lines in the
			 * request or response; just print the length
			 * on the first line of the output.
			 */
			ND_PRINT(", length: %u", len);
			for (idx = 0;
			    idx < len && (eol = print_txt_line(ndo, "\n\t", pptr, idx, len)) != 0;
			    idx = eol)
				;
		} else {
			/*
			 * Just print the first text line.
			 */
			print_txt_line(ndo, ": ", pptr, 0, len);
		}
	}
}

#if (defined(__i386__) || defined(_M_IX86) || defined(__X86__) || defined(__x86_64__) || defined(_M_X64)) || \
    (defined(__arm__) || defined(_M_ARM) || defined(__aarch64__)) || \
    (defined(__m68k__) && (!defined(__mc68000__) && !defined(__mc68010__))) || \
    (defined(__ppc__) || defined(__ppc64__) || defined(_M_PPC) || defined(_ARCH_PPC) || defined(_ARCH_PPC64)) || \
    (defined(__s390__) || defined(__s390x__) || defined(__zarch__)) || \
    defined(__vax__)
/*
 * The processor natively handles unaligned loads, so just use memcpy()
 * and memcmp(), to enable those optimizations.
 *
 * XXX - are those all the x86 tests we need?
 * XXX - do we need to worry about ARMv1 through ARMv5, which didn't
 * support unaligned loads, and, if so, do we need to worry about all
 * of them, or just some of them, e.g. ARMv5?
 * XXX - are those the only 68k tests we need not to generated
 * unaligned accesses if the target is the 68000 or 68010?
 * XXX - are there any tests we don't need, because some definitions are for
 * compilers that also predefine the GCC symbols?
 * XXX - do we need to test for both 32-bit and 64-bit versions of those
 * architectures in all cases?
 */
#else
/*
 * The processor doesn't natively handle unaligned loads,
 * and the compiler might "helpfully" optimize memcpy()
 * and memcmp(), when handed pointers that would normally
 * be properly aligned, into sequences that assume proper
 * alignment.
 *
 * Do copies and compares of possibly-unaligned data by
 * calling routines that wrap memcpy() and memcmp(), to
 * prevent that optimization.
 */
void
unaligned_memcpy(void *p, const void *q, size_t l)
{
	memcpy(p, q, l);
}

/* As with memcpy(), so with memcmp(). */
int
unaligned_memcmp(const void *p, const void *q, size_t l)
{
	return (memcmp(p, q, l));
}
#endif

