/*
 * Copyright 1998-2008 The OpenLDAP Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP Public
 * License.
 *
 * A copy of this license is available in file LICENSE in the top-level
 * directory of the distribution or, alternatively, at
 * <https://www.OpenLDAP.org/license.html>.
 */

/*
 * This work is part of OpenLDAP Software <https://www.openldap.org/>.
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ucstr.c,v 1.40 2008/03/04 06:24:05 hyc Exp $
 */

#include "k5-int.h"
#include "k5-utf8.h"
#include "k5-unicode.h"
#include "k5-input.h"
#include "ucdata/ucdata.h"

#include <ctype.h>

static int
krb5int_ucstrncmp(
		  const krb5_unicode * u1,
		  const krb5_unicode * u2,
		  size_t n)
{
    for (; 0 < n; ++u1, ++u2, --n) {
	if (*u1 != *u2) {
	    return *u1 < *u2 ? -1 : +1;
	}
	if (*u1 == 0) {
	    return 0;
	}
    }
    return 0;
}

static int
krb5int_ucstrncasecmp(
		      const krb5_unicode * u1,
		      const krb5_unicode * u2,
		      size_t n)
{
    for (; 0 < n; ++u1, ++u2, --n) {
	krb5_unicode uu1 = uctolower(*u1);
	krb5_unicode uu2 = uctolower(*u2);

	if (uu1 != uu2) {
	    return uu1 < uu2 ? -1 : +1;
	}
	if (uu1 == 0) {
	    return 0;
	}
    }
    return 0;
}

/* Return true if data contains valid UTF-8 sequences. */
krb5_boolean
k5_utf8_validate(const krb5_data *data)
{
    struct k5input in;
    int len, tmplen, i;
    const uint8_t *bytes;

    k5_input_init(&in, data->data, data->length);
    while (!in.status && in.len > 0) {
	len = KRB5_UTF8_CHARLEN(in.ptr);
	if (len < 1 || len > 4)
	    return FALSE;
	bytes = k5_input_get_bytes(&in, len);
	if (bytes == NULL)
	    return FALSE;
	if (KRB5_UTF8_CHARLEN2(bytes, tmplen) != len)
	    return FALSE;
	for (i = 1; i < len; i++) {
	    if ((bytes[i] & 0xc0) != 0x80)
		return FALSE;
	}
    }
    return !in.status;
}

#define TOLOWER(c)  (isupper(c) ? tolower(c) : (c))

/* compare UTF8-strings, optionally ignore casing */
/* slow, should be optimized */
int
krb5int_utf8_normcmp(
		     const krb5_data * data1,
		     const krb5_data * data2,
		     unsigned flags)
{
    int i, l1, l2, len, ulen, res = 0;
    char *s1, *s2, *done;
    krb5_ucs4 *ucs, *ucsout1, *ucsout2;

    unsigned casefold = flags & KRB5_UTF8_CASEFOLD;
    unsigned norm1 = flags & KRB5_UTF8_ARG1NFC;
    unsigned norm2 = flags & KRB5_UTF8_ARG2NFC;

    if (data1 == NULL) {
	return data2 == NULL ? 0 : -1;

    } else if (data2 == NULL) {
	return 1;
    }
    l1 = data1->length;
    l2 = data2->length;

    len = (l1 < l2) ? l1 : l2;
    if (len == 0) {
	return l1 == 0 ? (l2 == 0 ? 0 : -1) : 1;
    }
    s1 = data1->data;
    s2 = data2->data;
    done = s1 + len;

    while ((s1 < done) && KRB5_UTF8_ISASCII(s1) && KRB5_UTF8_ISASCII(s2)) {
	if (casefold) {
	    char c1 = TOLOWER(*s1);
	    char c2 = TOLOWER(*s2);
	    res = c1 - c2;
	} else {
	    res = *s1 - *s2;
	}
	s1++;
	s2++;
	if (res) {
	    /* done unless next character in s1 or s2 is non-ascii */
	    if (s1 < done) {
		if (!KRB5_UTF8_ISASCII(s1) || !KRB5_UTF8_ISASCII(s2)) {
		    break;
		}
	    } else if (((len < l1) && !KRB5_UTF8_ISASCII(s1)) ||
		       ((len < l2) && !KRB5_UTF8_ISASCII(s2))) {
		break;
	    }
	    return res;
	}
    }

    /* We have encountered non-ascii or strings equal up to len */

    /* set i to number of iterations */
    i = s1 - done + len;
    /* passed through loop at least once? */
    if (i > 0) {
	if (!res && (s1 == done) &&
	    ((len == l1) || KRB5_UTF8_ISASCII(s1)) &&
	    ((len == l2) || KRB5_UTF8_ISASCII(s2))) {
	    /* all ascii and equal up to len */
	    return l1 - l2;
	}
	/* rewind one char, and do normalized compare from there */
	s1--;
	s2--;
	l1 -= i - 1;
	l2 -= i - 1;
    }
    /*
     * Should first check to see if strings are already in proper normalized
     * form.
     */
    ucs = malloc(((norm1 || l1 > l2) ? l1 : l2) * sizeof(*ucs));
    if (ucs == NULL) {
	return l1 > l2 ? 1 : -1;/* what to do??? */
    }
    /*
     * XXYYZ: we convert to ucs4 even though -llunicode
     * expects ucs2 in an ac_uint4
     */

    /* convert and normalize 1st string */
    for (i = 0, ulen = 0; i < l1; i += len, ulen++) {
	if (krb5int_utf8_to_ucs4(s1 + i, &ucs[ulen]) == -1) {
	    free(ucs);
	    return -1;		/* what to do??? */
	}
	len = KRB5_UTF8_CHARLEN(s1 + i);
    }

    if (norm1) {
	ucsout1 = ucs;
	l1 = ulen;
	ucs = malloc(l2 * sizeof(*ucs));
	if (ucs == NULL) {
	    free(ucsout1);
	    return l1 > l2 ? 1 : -1;	/* what to do??? */
	}
    } else {
	uccompatdecomp(ucs, ulen, &ucsout1, &l1);
	l1 = uccanoncomp(ucsout1, l1);
    }

    /* convert and normalize 2nd string */
    for (i = 0, ulen = 0; i < l2; i += len, ulen++) {
	if (krb5int_utf8_to_ucs4(s2 + i, &ucs[ulen]) == -1) {
	    free(ucsout1);
	    free(ucs);
	    return 1;		/* what to do??? */
	}
	len = KRB5_UTF8_CHARLEN(s2 + i);
    }

    if (norm2) {
	ucsout2 = ucs;
	l2 = ulen;
    } else {
	uccompatdecomp(ucs, ulen, &ucsout2, &l2);
	l2 = uccanoncomp(ucsout2, l2);
	free(ucs);
    }

    res = casefold
	? krb5int_ucstrncasecmp(ucsout1, ucsout2, l1 < l2 ? l1 : l2)
	: krb5int_ucstrncmp(ucsout1, ucsout2, l1 < l2 ? l1 : l2);
    free(ucsout1);
    free(ucsout2);

    if (res != 0) {
	return res;
    }
    if (l1 == l2) {
	return 0;
    }
    return l1 > l2 ? 1 : -1;
}
