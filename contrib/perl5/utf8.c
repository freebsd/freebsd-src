/*    utf8.c
 *
 *    Copyright (c) 1998-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'What a fix!' said Sam. 'That's the one place in all the lands we've ever
 * heard of that we don't want to see any closer; and that's the one place
 * we're trying to get to!  And that's just where we can't get, nohow.'
 *
 * 'Well do I understand your speech,' he answered in the same language;
 * 'yet few strangers do so.  Why then do you not speak in the Common Tongue,
 * as is the custom in the West, if you wish to be answered?'
 *
 * ...the travellers perceived that the floor was paved with stones of many
 * hues; branching runes and strange devices intertwined beneath their feet.
 */

#include "EXTERN.h"
#define PERL_IN_UTF8_C
#include "perl.h"

/* Unicode support */

/*
=for apidoc A|U8*|uv_to_utf8|U8 *d|UV uv

Adds the UTF8 representation of the Unicode codepoint C<uv> to the end
of the string C<d>; C<d> should be have at least C<UTF8_MAXLEN+1> free
bytes available. The return value is the pointer to the byte after the
end of the new character. In other words, 

    d = uv_to_utf8(d, uv);

is the recommended Unicode-aware way of saying

    *(d++) = uv;

=cut
*/

U8 *
Perl_uv_to_utf8(pTHX_ U8 *d, UV uv)
{
    if (uv < 0x80) {
	*d++ = uv;
	return d;
    }
    if (uv < 0x800) {
	*d++ = (( uv >>  6)         | 0xc0);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x10000) {
	*d++ = (( uv >> 12)         | 0xe0);
	*d++ = (((uv >>  6) & 0x3f) | 0x80);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x200000) {
	*d++ = (( uv >> 18)         | 0xf0);
	*d++ = (((uv >> 12) & 0x3f) | 0x80);
	*d++ = (((uv >>  6) & 0x3f) | 0x80);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x4000000) {
	*d++ = (( uv >> 24)         | 0xf8);
	*d++ = (((uv >> 18) & 0x3f) | 0x80);
	*d++ = (((uv >> 12) & 0x3f) | 0x80);
	*d++ = (((uv >>  6) & 0x3f) | 0x80);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
    if (uv < 0x80000000) {
	*d++ = (( uv >> 30)         | 0xfc);
	*d++ = (((uv >> 24) & 0x3f) | 0x80);
	*d++ = (((uv >> 18) & 0x3f) | 0x80);
	*d++ = (((uv >> 12) & 0x3f) | 0x80);
	*d++ = (((uv >>  6) & 0x3f) | 0x80);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
#ifdef HAS_QUAD
    if (uv < UTF8_QUAD_MAX)
#endif
    {
	*d++ =                        0xfe;	/* Can't match U+FEFF! */
	*d++ = (((uv >> 30) & 0x3f) | 0x80);
	*d++ = (((uv >> 24) & 0x3f) | 0x80);
	*d++ = (((uv >> 18) & 0x3f) | 0x80);
	*d++ = (((uv >> 12) & 0x3f) | 0x80);
	*d++ = (((uv >>  6) & 0x3f) | 0x80);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
#ifdef HAS_QUAD
    {
	*d++ =                        0xff;	/* Can't match U+FFFE! */
	*d++ =                        0x80;	/* 6 Reserved bits */
	*d++ = (((uv >> 60) & 0x0f) | 0x80);	/* 2 Reserved bits */
	*d++ = (((uv >> 54) & 0x3f) | 0x80);
	*d++ = (((uv >> 48) & 0x3f) | 0x80);
	*d++ = (((uv >> 42) & 0x3f) | 0x80);
	*d++ = (((uv >> 36) & 0x3f) | 0x80);
	*d++ = (((uv >> 30) & 0x3f) | 0x80);
	*d++ = (((uv >> 24) & 0x3f) | 0x80);
	*d++ = (((uv >> 18) & 0x3f) | 0x80);
	*d++ = (((uv >> 12) & 0x3f) | 0x80);
	*d++ = (((uv >>  6) & 0x3f) | 0x80);
	*d++ = (( uv        & 0x3f) | 0x80);
	return d;
    }
#endif
}

/*
=for apidoc A|STRLEN|is_utf8_char|U8 *s

Tests if some arbitrary number of bytes begins in a valid UTF-8 character.
The actual number of bytes in the UTF-8 character will be returned if it
is valid, otherwise 0. 
 
=cut
*/
STRLEN
Perl_is_utf8_char(pTHX_ U8 *s)
{
    U8 u = *s;
    STRLEN slen, len;
    UV uv, ouv;

    if (UTF8_IS_ASCII(u))
	return 1;

    if (!UTF8_IS_START(u))
	return 0;

    len = UTF8SKIP(s);

    if (len < 2 || !UTF8_IS_CONTINUATION(s[1]))
	return 0;

    slen = len - 1;
    s++;
    uv = u;
    ouv = uv;
    while (slen--) {
	if (!UTF8_IS_CONTINUATION(*s))
	    return 0;
	uv = UTF8_ACCUMULATE(uv, *s);
	if (uv < ouv)
	    return 0;
	ouv = uv;
	s++;
    }

    if (UNISKIP(uv) < len)
	return 0;

    return len;
}

/*
=for apidoc A|bool|is_utf8_string|U8 *s|STRLEN len

Returns true if first C<len> bytes of the given string form valid a UTF8
string, false otherwise.

=cut
*/

bool
Perl_is_utf8_string(pTHX_ U8 *s, STRLEN len)
{
    U8* x = s;
    U8* send;
    STRLEN c;

    if (!len)
	len = strlen((char *)s);
    send = s + len;

    while (x < send) {
        c = is_utf8_char(x);
	if (!c)
	    return FALSE;
        x += c;
    }
    if (x != send)
	return FALSE;

    return TRUE;
}

/*
=for apidoc A|U8* s|utf8_to_uv|STRLEN curlen|STRLEN *retlen|U32 flags

Returns the character value of the first character in the string C<s>
which is assumed to be in UTF8 encoding and no longer than C<curlen>;
C<retlen> will be set to the length, in bytes, of that character.

If C<s> does not point to a well-formed UTF8 character, the behaviour
is dependent on the value of C<flags>: if it contains UTF8_CHECK_ONLY,
it is assumed that the caller will raise a warning, and this function
will silently just set C<retlen> to C<-1> and return zero.  If the
C<flags> does not contain UTF8_CHECK_ONLY, warnings about
malformations will be given, C<retlen> will be set to the expected
length of the UTF-8 character in bytes, and zero will be returned.

The C<flags> can also contain various flags to allow deviations from
the strict UTF-8 encoding (see F<utf8.h>).

=cut */

UV
Perl_utf8_to_uv(pTHX_ U8* s, STRLEN curlen, STRLEN* retlen, U32 flags)
{
    UV uv = *s, ouv;
    STRLEN len = 1;
#ifdef EBCDIC
    bool dowarn = 0;
#else
    bool dowarn = ckWARN_d(WARN_UTF8);
#endif
    STRLEN expectlen = 0;
    U32 warning = 0;

/* This list is a superset of the UTF8_ALLOW_XXX. */

#define UTF8_WARN_EMPTY				 1
#define UTF8_WARN_CONTINUATION			 2
#define UTF8_WARN_NON_CONTINUATION	 	 3
#define UTF8_WARN_FE_FF				 4
#define UTF8_WARN_SHORT				 5
#define UTF8_WARN_OVERFLOW			 6
#define UTF8_WARN_SURROGATE			 7
#define UTF8_WARN_BOM				 8
#define UTF8_WARN_LONG				 9
#define UTF8_WARN_FFFF				10

    if (curlen == 0 &&
	!(flags & UTF8_ALLOW_EMPTY)) {
	warning = UTF8_WARN_EMPTY;
	goto malformed;
    }

    if (UTF8_IS_ASCII(uv)) {
	if (retlen)
	    *retlen = 1;
	return *s;
    }

    if (UTF8_IS_CONTINUATION(uv) &&
	!(flags & UTF8_ALLOW_CONTINUATION)) {
	warning = UTF8_WARN_CONTINUATION;
	goto malformed;
    }

    if (UTF8_IS_START(uv) && curlen > 1 && !UTF8_IS_CONTINUATION(s[1]) &&
	!(flags & UTF8_ALLOW_NON_CONTINUATION)) {
	warning = UTF8_WARN_NON_CONTINUATION;
	goto malformed;
    }
    
    if ((uv == 0xfe || uv == 0xff) &&
	!(flags & UTF8_ALLOW_FE_FF)) {
	warning = UTF8_WARN_FE_FF;
	goto malformed;
    }
	
    if      (!(uv & 0x20))	{ len =  2; uv &= 0x1f; }
    else if (!(uv & 0x10))	{ len =  3; uv &= 0x0f; }
    else if (!(uv & 0x08))	{ len =  4; uv &= 0x07; }
    else if (!(uv & 0x04))	{ len =  5; uv &= 0x03; }
    else if (!(uv & 0x02))	{ len =  6; uv &= 0x01; }
    else if (!(uv & 0x01))	{ len =  7; uv = 0; }
    else 			{ len = 13; uv = 0; } /* whoa! */
	
    if (retlen)
	*retlen = len;
    
    expectlen = len;

    if ((curlen < expectlen) &&
	!(flags & UTF8_ALLOW_SHORT)) {
	warning = UTF8_WARN_SHORT;
	goto malformed;
    }

    len--;
    s++;
    ouv = uv;

    while (len--) {
	if (!UTF8_IS_CONTINUATION(*s) &&
	    !(flags & UTF8_ALLOW_NON_CONTINUATION)) {
	    s--;
	    warning = UTF8_WARN_NON_CONTINUATION;
	    goto malformed;
	}
	else
	    uv = UTF8_ACCUMULATE(uv, *s);
	if (!(uv > ouv)) {
	    /* These cannot be allowed. */
	    if (uv == ouv) {
		if (!(flags & UTF8_ALLOW_LONG)) {
		    warning = UTF8_WARN_LONG;
		    goto malformed;
		}
	    }
	    else { /* uv < ouv */
		/* This cannot be allowed. */
		warning = UTF8_WARN_OVERFLOW;
		goto malformed;
	    }
	}
	s++;
	ouv = uv;
    }

    if (UNICODE_IS_SURROGATE(uv) &&
	!(flags & UTF8_ALLOW_SURROGATE)) {
	warning = UTF8_WARN_SURROGATE;
	goto malformed;
    } else if (UNICODE_IS_BYTE_ORDER_MARK(uv) &&
	       !(flags & UTF8_ALLOW_BOM)) {
	warning = UTF8_WARN_BOM;
	goto malformed;
    } else if ((expectlen > UNISKIP(uv)) &&
	       !(flags & UTF8_ALLOW_LONG)) {
	warning = UTF8_WARN_LONG;
	goto malformed;
    } else if (UNICODE_IS_ILLEGAL(uv) &&
	       !(flags & UTF8_ALLOW_FFFF)) {
	warning = UTF8_WARN_FFFF;
	goto malformed;
    }

    return uv;

malformed:

    if (flags & UTF8_CHECK_ONLY) {
	if (retlen)
	    *retlen = -1;
	return 0;
    }

    if (dowarn) {
	SV* sv = sv_2mortal(newSVpv("Malformed UTF-8 character ", 0));

	switch (warning) {
	case 0: /* Intentionally empty. */ break;
	case UTF8_WARN_EMPTY:
	    Perl_sv_catpvf(aTHX_ sv, "(empty string)");
	    break;
	case UTF8_WARN_CONTINUATION:
	    Perl_sv_catpvf(aTHX_ sv, "(unexpected continuation byte 0x%02"UVxf")", uv);
	    break;
	case UTF8_WARN_NON_CONTINUATION:
	    Perl_sv_catpvf(aTHX_ sv, "(unexpected non-continuation byte 0x%02"UVxf" after start byte 0x%02"UVxf")",
                           (UV)s[1], uv);
	    break;
	case UTF8_WARN_FE_FF:
	    Perl_sv_catpvf(aTHX_ sv, "(byte 0x%02"UVxf")", uv);
	    break;
	case UTF8_WARN_SHORT:
	    Perl_sv_catpvf(aTHX_ sv, "(%d byte%s, need %d)",
                           curlen, curlen == 1 ? "" : "s", expectlen);
	    break;
	case UTF8_WARN_OVERFLOW:
	    Perl_sv_catpvf(aTHX_ sv, "(overflow at 0x%"UVxf", byte 0x%02x)",
                           ouv, *s);
	    break;
	case UTF8_WARN_SURROGATE:
	    Perl_sv_catpvf(aTHX_ sv, "(UTF-16 surrogate 0x%04"UVxf")", uv);
	    break;
	case UTF8_WARN_BOM:
	    Perl_sv_catpvf(aTHX_ sv, "(byte order mark 0x%04"UVxf")", uv);
	    break;
	case UTF8_WARN_LONG:
	    Perl_sv_catpvf(aTHX_ sv, "(%d byte%s, need %d)",
			   expectlen, expectlen == 1 ? "": "s", UNISKIP(uv));
	    break;
	case UTF8_WARN_FFFF:
	    Perl_sv_catpvf(aTHX_ sv, "(character 0x%04"UVxf")", uv);
	    break;
	default:
	    Perl_sv_catpvf(aTHX_ sv, "(unknown reason)");
	    break;
	}
	
	if (warning) {
	    char *s = SvPVX(sv);

	    if (PL_op)
		Perl_warner(aTHX_ WARN_UTF8,
			    "%s in %s", s,  PL_op_desc[PL_op->op_type]);
	    else
		Perl_warner(aTHX_ WARN_UTF8, "%s", s);
	}
    }

    if (retlen)
	*retlen = expectlen ? expectlen : len;

    return 0;
}

/*
=for apidoc A|U8* s|utf8_to_uv_simple|STRLEN *retlen

Returns the character value of the first character in the string C<s>
which is assumed to be in UTF8 encoding; C<retlen> will be set to the
length, in bytes, of that character.

If C<s> does not point to a well-formed UTF8 character, zero is
returned and retlen is set, if possible, to -1.

=cut
*/

UV
Perl_utf8_to_uv_simple(pTHX_ U8* s, STRLEN* retlen)
{
    return Perl_utf8_to_uv(aTHX_ s, UTF8_MAXLEN, retlen, 0);
}

/*
=for apidoc A|STRLEN|utf8_length|U8* s|U8 *e

Return the length of the UTF-8 char encoded string C<s> in characters.
Stops at C<e> (inclusive).  If C<e E<lt> s> or if the scan would end
up past C<e>, croaks.

=cut
*/

STRLEN
Perl_utf8_length(pTHX_ U8* s, U8* e)
{
    STRLEN len = 0;

    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g.
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (e < s)
	Perl_croak(aTHX_ "panic: utf8_length: unexpected end");
    while (s < e) {
	U8 t = UTF8SKIP(s);

	if (e - s < t)
	    Perl_croak(aTHX_ "panic: utf8_length: unaligned end");
	s += t;
	len++;
    }

    return len;
}

/*
=for apidoc A|IV|utf8_distance|U8 *a|U8 *b

Returns the number of UTF8 characters between the UTF-8 pointers C<a>
and C<b>.

WARNING: use only if you *know* that the pointers point inside the
same UTF-8 buffer.

=cut */

IV
Perl_utf8_distance(pTHX_ U8 *a, U8 *b)
{
    IV off = 0;

    /* Note: cannot use UTF8_IS_...() too eagerly here since  e.g.
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (a < b) {
	while (a < b) {
	    U8 c = UTF8SKIP(a);

	    if (b - a < c)
		Perl_croak(aTHX_ "panic: utf8_distance: unaligned end");
	    a += c;
	    off--;
	}
    }
    else {
	while (b < a) {
	    U8 c = UTF8SKIP(b);

	    if (a - b < c)
		Perl_croak(aTHX_ "panic: utf8_distance: unaligned end");
	    b += c;
	    off++;
	}
    }

    return off;
}

/*
=for apidoc A|U8*|utf8_hop|U8 *s|I32 off

Return the UTF-8 pointer C<s> displaced by C<off> characters, either
forward or backward.

WARNING: do not use the following unless you *know* C<off> is within
the UTF-8 data pointed to by C<s> *and* that on entry C<s> is aligned
on the first byte of character or just after the last byte of a character.

=cut */

U8 *
Perl_utf8_hop(pTHX_ U8 *s, I32 off)
{
    /* Note: cannot use UTF8_IS_...() too eagerly here since e.g
     * the bitops (especially ~) can create illegal UTF-8.
     * In other words: in Perl UTF-8 is not just for Unicode. */

    if (off >= 0) {
	while (off--)
	    s += UTF8SKIP(s);
    }
    else {
	while (off++) {
	    s--;
	    while (UTF8_IS_CONTINUATION(*s))
		s--;
	}
    }
    return s;
}

/*
=for apidoc A|U8 *|utf8_to_bytes|U8 *s|STRLEN *len

Converts a string C<s> of length C<len> from UTF8 into byte encoding.
Unlike C<bytes_to_utf8>, this over-writes the original string, and
updates len to contain the new length.
Returns zero on failure, setting C<len> to -1.

=cut
*/

U8 *
Perl_utf8_to_bytes(pTHX_ U8* s, STRLEN *len)
{
    U8 *send;
    U8 *d;
    U8 *save = s;

    /* ensure valid UTF8 and chars < 256 before updating string */
    for (send = s + *len; s < send; ) {
        U8 c = *s++;

        if (c >= 0x80 &&
            ((s >= send) ||
	     ((*s++ & 0xc0) != 0x80) || ((c & 0xfe) != 0xc2))) {
            *len = -1;
            return 0;
        }
    }

    d = s = save;
    while (s < send) {
        if (UTF8_IS_ASCII(*s)) {
	    *d++ = *s++;
	}
        else {
            STRLEN ulen;
            *d++ = (U8)utf8_to_uv_simple(s, &ulen);
            s += ulen;
        }
    }
    *d = '\0';
    *len = d - save;
    return save;
}

/*
=for apidoc A|U8 *|bytes_from_utf8|U8 *s|STRLEN *len|bool *is_utf8

Converts a string C<s> of length C<len> from UTF8 into byte encoding.
Unlike <utf8_to_bytes> but like C<bytes_to_utf8>, returns a pointer to
the newly-created string, and updates C<len> to contain the new
length.  Returns the original string if no conversion occurs, C<len>
is unchanged. Do nothing if C<is_utf8> points to 0. Sets C<is_utf8> to
0 if C<s> is converted or contains all 7bit characters.

=cut */

U8 *
Perl_bytes_from_utf8(pTHX_ U8* s, STRLEN *len, bool *is_utf8)
{
    U8 *send;
    U8 *d;
    U8 *start = s;
    I32 count = 0;

    if (!*is_utf8)
	return start;

    /* ensure valid UTF8 and chars < 256 before converting string */
    for (send = s + *len; s < send;) {
	U8 c = *s++;
        if (!UTF8_IS_ASCII(c)) {
	    if (UTF8_IS_CONTINUATION(c) || s >= send ||
		!UTF8_IS_CONTINUATION(*s) || UTF8_IS_DOWNGRADEABLE_START(c))
		return start;
	    s++, count++;
        }
    }

    *is_utf8 = 0;		

    if (!count)
	return start;

    Newz(801, d, (*len) - count + 1, U8);
    s = start; start = d;
    while (s < send) {
	U8 c = *s++;

	if (UTF8_IS_ASCII(c))
	    *d++ = c;
	else
	    *d++ = UTF8_ACCUMULATE(c, *s++);
    }
    *d = '\0';
    *len = d - start;
    return start;
}

/*
=for apidoc A|U8 *|bytes_to_utf8|U8 *s|STRLEN *len

Converts a string C<s> of length C<len> from ASCII into UTF8 encoding.
Returns a pointer to the newly-created string, and sets C<len> to
reflect the new length.

=cut
*/

U8*
Perl_bytes_to_utf8(pTHX_ U8* s, STRLEN *len)
{
    U8 *send;
    U8 *d;
    U8 *dst;
    send = s + (*len);

    Newz(801, d, (*len) * 2 + 1, U8);
    dst = d;

    while (s < send) {
        if (UTF8_IS_ASCII(*s))
            *d++ = *s++;
        else {
            UV uv = *s++;

            *d++ = UTF8_EIGHT_BIT_HI(uv);
            *d++ = UTF8_EIGHT_BIT_LO(uv);
        }
    }
    *d = '\0';
    *len = d-dst;
    return dst;
}

/*
 * Convert native (big-endian) or reversed (little-endian) UTF-16 to UTF-8.
 *
 * Destination must be pre-extended to 3/2 source.  Do not use in-place.
 * We optimize for native, for obvious reasons. */

U8*
Perl_utf16_to_utf8(pTHX_ U8* p, U8* d, I32 bytelen, I32 *newlen)
{
    U8* pend;
    U8* dstart = d;

    if (bytelen & 1)
	Perl_croak(aTHX_ "panic: utf16_to_utf8: odd bytelen");

    pend = p + bytelen;

    while (p < pend) {
	UV uv = (p[0] << 8) + p[1]; /* UTF-16BE */
	p += 2;
	if (uv < 0x80) {
	    *d++ = uv;
	    continue;
	}
	if (uv < 0x800) {
	    *d++ = (( uv >>  6)         | 0xc0);
	    *d++ = (( uv        & 0x3f) | 0x80);
	    continue;
	}
	if (uv >= 0xd800 && uv < 0xdbff) {	/* surrogates */
	    UV low = *p++;
	    if (low < 0xdc00 || low >= 0xdfff)
		Perl_croak(aTHX_ "Malformed UTF-16 surrogate");
	    uv = ((uv - 0xd800) << 10) + (low - 0xdc00) + 0x10000;
	}
	if (uv < 0x10000) {
	    *d++ = (( uv >> 12)         | 0xe0);
	    *d++ = (((uv >>  6) & 0x3f) | 0x80);
	    *d++ = (( uv        & 0x3f) | 0x80);
	    continue;
	}
	else {
	    *d++ = (( uv >> 18)         | 0xf0);
	    *d++ = (((uv >> 12) & 0x3f) | 0x80);
	    *d++ = (((uv >>  6) & 0x3f) | 0x80);
	    *d++ = (( uv        & 0x3f) | 0x80);
	    continue;
	}
    }
    *newlen = d - dstart;
    return d;
}

/* Note: this one is slightly destructive of the source. */

U8*
Perl_utf16_to_utf8_reversed(pTHX_ U8* p, U8* d, I32 bytelen, I32 *newlen)
{
    U8* s = (U8*)p;
    U8* send = s + bytelen;
    while (s < send) {
	U8 tmp = s[0];
	s[0] = s[1];
	s[1] = tmp;
	s += 2;
    }
    return utf16_to_utf8(p, d, bytelen, newlen);
}

/* for now these are all defined (inefficiently) in terms of the utf8 versions */

bool
Perl_is_uni_alnum(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_alnum(tmpbuf);
}

bool
Perl_is_uni_alnumc(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_alnumc(tmpbuf);
}

bool
Perl_is_uni_idfirst(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_idfirst(tmpbuf);
}

bool
Perl_is_uni_alpha(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_alpha(tmpbuf);
}

bool
Perl_is_uni_ascii(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_ascii(tmpbuf);
}

bool
Perl_is_uni_space(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_space(tmpbuf);
}

bool
Perl_is_uni_digit(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_digit(tmpbuf);
}

bool
Perl_is_uni_upper(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_upper(tmpbuf);
}

bool
Perl_is_uni_lower(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_lower(tmpbuf);
}

bool
Perl_is_uni_cntrl(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_cntrl(tmpbuf);
}

bool
Perl_is_uni_graph(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_graph(tmpbuf);
}

bool
Perl_is_uni_print(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_print(tmpbuf);
}

bool
Perl_is_uni_punct(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_punct(tmpbuf);
}

bool
Perl_is_uni_xdigit(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return is_utf8_xdigit(tmpbuf);
}

U32
Perl_to_uni_upper(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return to_utf8_upper(tmpbuf);
}

U32
Perl_to_uni_title(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return to_utf8_title(tmpbuf);
}

U32
Perl_to_uni_lower(pTHX_ U32 c)
{
    U8 tmpbuf[UTF8_MAXLEN+1];
    uv_to_utf8(tmpbuf, (UV)c);
    return to_utf8_lower(tmpbuf);
}

/* for now these all assume no locale info available for Unicode > 255 */

bool
Perl_is_uni_alnum_lc(pTHX_ U32 c)
{
    return is_uni_alnum(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_alnumc_lc(pTHX_ U32 c)
{
    return is_uni_alnumc(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_idfirst_lc(pTHX_ U32 c)
{
    return is_uni_idfirst(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_alpha_lc(pTHX_ U32 c)
{
    return is_uni_alpha(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_ascii_lc(pTHX_ U32 c)
{
    return is_uni_ascii(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_space_lc(pTHX_ U32 c)
{
    return is_uni_space(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_digit_lc(pTHX_ U32 c)
{
    return is_uni_digit(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_upper_lc(pTHX_ U32 c)
{
    return is_uni_upper(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_lower_lc(pTHX_ U32 c)
{
    return is_uni_lower(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_cntrl_lc(pTHX_ U32 c)
{
    return is_uni_cntrl(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_graph_lc(pTHX_ U32 c)
{
    return is_uni_graph(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_print_lc(pTHX_ U32 c)
{
    return is_uni_print(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_punct_lc(pTHX_ U32 c)
{
    return is_uni_punct(c);	/* XXX no locale support yet */
}

bool
Perl_is_uni_xdigit_lc(pTHX_ U32 c)
{
    return is_uni_xdigit(c);	/* XXX no locale support yet */
}

U32
Perl_to_uni_upper_lc(pTHX_ U32 c)
{
    return to_uni_upper(c);	/* XXX no locale support yet */
}

U32
Perl_to_uni_title_lc(pTHX_ U32 c)
{
    return to_uni_title(c);	/* XXX no locale support yet */
}

U32
Perl_to_uni_lower_lc(pTHX_ U32 c)
{
    return to_uni_lower(c);	/* XXX no locale support yet */
}

bool
Perl_is_utf8_alnum(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_alnum)
	/* NOTE: "IsWord", not "IsAlnum", since Alnum is a true
	 * descendant of isalnum(3), in other words, it doesn't
	 * contain the '_'. --jhi */
	PL_utf8_alnum = swash_init("utf8", "IsWord", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_alnum, p);
/*    return *p == '_' || is_utf8_alpha(p) || is_utf8_digit(p); */
#ifdef SURPRISINGLY_SLOWER  /* probably because alpha is usually true */
    if (!PL_utf8_alnum)
	PL_utf8_alnum = swash_init("utf8", "",
	    sv_2mortal(newSVpv("+utf8::IsAlpha\n+utf8::IsDigit\n005F\n",0)), 0, 0);
    return swash_fetch(PL_utf8_alnum, p);
#endif
}

bool
Perl_is_utf8_alnumc(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_alnum)
	PL_utf8_alnum = swash_init("utf8", "IsAlnumC", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_alnum, p);
/*    return is_utf8_alpha(p) || is_utf8_digit(p); */
#ifdef SURPRISINGLY_SLOWER  /* probably because alpha is usually true */
    if (!PL_utf8_alnum)
	PL_utf8_alnum = swash_init("utf8", "",
	    sv_2mortal(newSVpv("+utf8::IsAlpha\n+utf8::IsDigit\n005F\n",0)), 0, 0);
    return swash_fetch(PL_utf8_alnum, p);
#endif
}

bool
Perl_is_utf8_idfirst(pTHX_ U8 *p)
{
    return *p == '_' || is_utf8_alpha(p);
}

bool
Perl_is_utf8_alpha(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_alpha)
	PL_utf8_alpha = swash_init("utf8", "IsAlpha", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_alpha, p);
}

bool
Perl_is_utf8_ascii(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_ascii)
	PL_utf8_ascii = swash_init("utf8", "IsAscii", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_ascii, p);
}

bool
Perl_is_utf8_space(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_space)
	PL_utf8_space = swash_init("utf8", "IsSpacePerl", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_space, p);
}

bool
Perl_is_utf8_digit(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_digit)
	PL_utf8_digit = swash_init("utf8", "IsDigit", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_digit, p);
}

bool
Perl_is_utf8_upper(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_upper)
	PL_utf8_upper = swash_init("utf8", "IsUpper", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_upper, p);
}

bool
Perl_is_utf8_lower(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_lower)
	PL_utf8_lower = swash_init("utf8", "IsLower", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_lower, p);
}

bool
Perl_is_utf8_cntrl(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_cntrl)
	PL_utf8_cntrl = swash_init("utf8", "IsCntrl", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_cntrl, p);
}

bool
Perl_is_utf8_graph(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_graph)
	PL_utf8_graph = swash_init("utf8", "IsGraph", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_graph, p);
}

bool
Perl_is_utf8_print(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_print)
	PL_utf8_print = swash_init("utf8", "IsPrint", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_print, p);
}

bool
Perl_is_utf8_punct(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_punct)
	PL_utf8_punct = swash_init("utf8", "IsPunct", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_punct, p);
}

bool
Perl_is_utf8_xdigit(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_xdigit)
	PL_utf8_xdigit = swash_init("utf8", "IsXDigit", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_xdigit, p);
}

bool
Perl_is_utf8_mark(pTHX_ U8 *p)
{
    if (!is_utf8_char(p))
	return FALSE;
    if (!PL_utf8_mark)
	PL_utf8_mark = swash_init("utf8", "IsM", &PL_sv_undef, 0, 0);
    return swash_fetch(PL_utf8_mark, p);
}

UV
Perl_to_utf8_upper(pTHX_ U8 *p)
{
    UV uv;

    if (!PL_utf8_toupper)
	PL_utf8_toupper = swash_init("utf8", "ToUpper", &PL_sv_undef, 4, 0);
    uv = swash_fetch(PL_utf8_toupper, p);
    return uv ? uv : utf8_to_uv(p,UTF8_MAXLEN,0,0);
}

UV
Perl_to_utf8_title(pTHX_ U8 *p)
{
    UV uv;

    if (!PL_utf8_totitle)
	PL_utf8_totitle = swash_init("utf8", "ToTitle", &PL_sv_undef, 4, 0);
    uv = swash_fetch(PL_utf8_totitle, p);
    return uv ? uv : utf8_to_uv(p,UTF8_MAXLEN,0,0);
}

UV
Perl_to_utf8_lower(pTHX_ U8 *p)
{
    UV uv;

    if (!PL_utf8_tolower)
	PL_utf8_tolower = swash_init("utf8", "ToLower", &PL_sv_undef, 4, 0);
    uv = swash_fetch(PL_utf8_tolower, p);
    return uv ? uv : utf8_to_uv(p,UTF8_MAXLEN,0,0);
}

/* a "swash" is a swatch hash */

SV*
Perl_swash_init(pTHX_ char* pkg, char* name, SV *listsv, I32 minbits, I32 none)
{
    SV* retval;
    SV* tokenbufsv = sv_2mortal(NEWSV(0,0));
    dSP;

    if (!gv_stashpv(pkg, 0)) {	/* demand load utf8 */
	ENTER;
	Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT, newSVpv(pkg,0), Nullsv);
	LEAVE;
    }
    SPAGAIN;
    PUSHSTACKi(PERLSI_MAGIC);
    PUSHMARK(SP);
    EXTEND(SP,5);
    PUSHs(sv_2mortal(newSVpvn(pkg, strlen(pkg))));
    PUSHs(sv_2mortal(newSVpvn(name, strlen(name))));
    PUSHs(listsv);
    PUSHs(sv_2mortal(newSViv(minbits)));
    PUSHs(sv_2mortal(newSViv(none)));
    PUTBACK;
    ENTER;
    SAVEI32(PL_hints);
    PL_hints = 0;
    save_re_context();
    if (PL_curcop == &PL_compiling)
	/* XXX ought to be handled by lex_start */
	sv_setpv(tokenbufsv, PL_tokenbuf);
    if (call_method("SWASHNEW", G_SCALAR))
	retval = newSVsv(*PL_stack_sp--);
    else
	retval = &PL_sv_undef;
    LEAVE;
    POPSTACK;
    if (PL_curcop == &PL_compiling) {
	STRLEN len;
	char* pv = SvPV(tokenbufsv, len);

	Copy(pv, PL_tokenbuf, len+1, char);
	PL_curcop->op_private = PL_hints;
    }
    if (!SvROK(retval) || SvTYPE(SvRV(retval)) != SVt_PVHV)
	Perl_croak(aTHX_ "SWASHNEW didn't return an HV ref");
    return retval;
}

UV
Perl_swash_fetch(pTHX_ SV *sv, U8 *ptr)
{
    HV* hv = (HV*)SvRV(sv);
    U32 klen = UTF8SKIP(ptr) - 1;
    U32 off = ptr[klen] & 127;  /* NB: 64 bit always 0 when len > 1 */
    STRLEN slen;
    STRLEN needents = (klen ? 64 : 128);
    U8 *tmps;
    U32 bit;
    SV *retval;

    /*
     * This single-entry cache saves about 1/3 of the utf8 overhead in test
     * suite.  (That is, only 7-8% overall over just a hash cache.  Still,
     * it's nothing to sniff at.)  Pity we usually come through at least
     * two function calls to get here...
     *
     * NB: this code assumes that swatches are never modified, once generated!
     */

    if (hv == PL_last_swash_hv &&
	klen == PL_last_swash_klen &&
	(!klen || memEQ(ptr,PL_last_swash_key,klen)) )
    {
	tmps = PL_last_swash_tmps;
	slen = PL_last_swash_slen;
    }
    else {
	/* Try our second-level swatch cache, kept in a hash. */
	SV** svp = hv_fetch(hv, (char*)ptr, klen, FALSE);

	/* If not cached, generate it via utf8::SWASHGET */
	if (!svp || !SvPOK(*svp) || !(tmps = (U8*)SvPV(*svp, slen))) {
	    dSP;
	    ENTER;
	    SAVETMPS;
	    save_re_context();
	    PUSHSTACKi(PERLSI_MAGIC);
	    PUSHMARK(SP);
	    EXTEND(SP,3);
	    PUSHs((SV*)sv);
	    PUSHs(sv_2mortal(newSViv(utf8_to_uv(ptr, UTF8_MAXLEN, 0, 0) & ~(needents - 1))));
	    PUSHs(sv_2mortal(newSViv(needents)));
	    PUTBACK;
	    if (call_method("SWASHGET", G_SCALAR))
		retval = newSVsv(*PL_stack_sp--);
	    else
		retval = &PL_sv_undef;
	    POPSTACK;
	    FREETMPS;
	    LEAVE;
	    if (PL_curcop == &PL_compiling)
		PL_curcop->op_private = PL_hints;

	    svp = hv_store(hv, (char*)ptr, klen, retval, 0);

	    if (!svp || !(tmps = (U8*)SvPV(*svp, slen)) || slen < 8)
		Perl_croak(aTHX_ "SWASHGET didn't return result of proper length");
	}

	PL_last_swash_hv = hv;
	PL_last_swash_klen = klen;
	PL_last_swash_tmps = tmps;
	PL_last_swash_slen = slen;
	if (klen)
	    Copy(ptr, PL_last_swash_key, klen, U8);
    }

    switch ((int)((slen << 3) / needents)) {
    case 1:
	bit = 1 << (off & 7);
	off >>= 3;
	return (tmps[off] & bit) != 0;
    case 8:
	return tmps[off];
    case 16:
	off <<= 1;
	return (tmps[off] << 8) + tmps[off + 1] ;
    case 32:
	off <<= 2;
	return (tmps[off] << 24) + (tmps[off+1] << 16) + (tmps[off+2] << 8) + tmps[off + 3] ;
    }
    Perl_croak(aTHX_ "panic: swash_fetch");
    return 0;
}
