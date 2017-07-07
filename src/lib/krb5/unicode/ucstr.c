/*
 * Copyright 1998-2008 The OpenLDAP Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP Public
 * License.
 *
 * A copy of this license is available in file LICENSE in the top-level
 * directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

/*
 * This work is part of OpenLDAP Software <http://www.openldap.org/>.
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ucstr.c,v 1.40 2008/03/04 06:24:05 hyc Exp $
 */

#include "k5-int.h"
#include "k5-utf8.h"
#include "k5-unicode.h"
#include "ucdata/ucdata.h"

#include <ctype.h>

int
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

int
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

krb5_unicode *
krb5int_ucstrnchr(
		  const krb5_unicode * u,
		  size_t n,
		  krb5_unicode c)
{
    for (; 0 < n; ++u, --n) {
	if (*u == c) {
	    return (krb5_unicode *) u;
	}
    }

    return NULL;
}

krb5_unicode *
krb5int_ucstrncasechr(
		      const krb5_unicode * u,
		      size_t n,
		      krb5_unicode c)
{
    c = uctolower(c);
    for (; 0 < n; ++u, --n) {
	if ((krb5_unicode) uctolower(*u) == c) {
	    return (krb5_unicode *) u;
	}
    }

    return NULL;
}

void
krb5int_ucstr2upper(
		    krb5_unicode * u,
		    size_t n)
{
    for (; 0 < n; ++u, --n) {
	*u = uctoupper(*u);
    }
}

#define TOUPPER(c)  (islower(c) ? toupper(c) : (c))
#define TOLOWER(c)  (isupper(c) ? tolower(c) : (c))

krb5_error_code
krb5int_utf8_normalize(
		       const krb5_data * data,
		       krb5_data ** newdataptr,
		       unsigned flags)
{
    int i, j, len, clen, outpos = 0, ucsoutlen, outsize;
    char *out = NULL, *outtmp, *s;
    krb5_ucs4 *ucs = NULL, *p, *ucsout = NULL;
    krb5_data *newdata;
    krb5_error_code retval = 0;

    static unsigned char mask[] = {
    0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01};

    unsigned casefold = flags & KRB5_UTF8_CASEFOLD;
    unsigned approx = flags & KRB5_UTF8_APPROX;

    *newdataptr = NULL;

    s = data->data;
    len = data->length;

    newdata = malloc(sizeof(*newdata));
    if (newdata == NULL)
	return ENOMEM;

    /*
     * Should first check to see if string is already in proper normalized
     * form. This is almost as time consuming as the normalization though.
     */

    /* finish off everything up to character before first non-ascii */
    if (KRB5_UTF8_ISASCII(s)) {
	if (casefold) {
	    outsize = len + 7;
	    out = malloc(outsize);
	    if (out == NULL) {
		retval = ENOMEM;
		goto cleanup;
	    }

	    for (i = 1; (i < len) && KRB5_UTF8_ISASCII(s + i); i++) {
		out[outpos++] = TOLOWER(s[i - 1]);
	    }
	    if (i == len) {
		out[outpos++] = TOLOWER(s[len - 1]);
		goto cleanup;
	    }
	} else {
	    for (i = 1; (i < len) && KRB5_UTF8_ISASCII(s + i); i++) {
		/* empty */
	    }

	    if (i == len) {
		newdata->length = len;
		newdata->data = k5memdup0(s, len, &retval);
		if (newdata->data == NULL)
		    goto cleanup;
		*newdataptr = newdata;
		return 0;
	    }
	    outsize = len + 7;
	    out = malloc(outsize);
	    if (out == NULL) {
		retval = ENOMEM;
		goto cleanup;
	    }
	    outpos = i - 1;
	    memcpy(out, s, outpos);
	}
    } else {
	outsize = len + 7;
	out = malloc(outsize);
	if (out == NULL) {
	    retval = ENOMEM;
	    goto cleanup;
	}
	i = 0;
    }

    p = ucs = malloc(len * sizeof(*ucs));
    if (ucs == NULL) {
	retval = ENOMEM;
	goto cleanup;
    }
    /* convert character before first non-ascii to ucs-4 */
    if (i > 0) {
	*p = casefold ? TOLOWER(s[i - 1]) : s[i - 1];
	p++;
    }
    /* s[i] is now first non-ascii character */
    for (;;) {
	/* s[i] is non-ascii */
	/* convert everything up to next ascii to ucs-4 */
	while (i < len) {
	    clen = KRB5_UTF8_CHARLEN2(s + i, clen);
	    if (clen == 0) {
		retval = KRB5_ERR_INVALID_UTF8;
		goto cleanup;
	    }
	    if (clen == 1) {
		/* ascii */
		break;
	    }
	    *p = s[i] & mask[clen];
	    i++;
	    for (j = 1; j < clen; j++) {
		if ((s[i] & 0xc0) != 0x80) {
		    retval = KRB5_ERR_INVALID_UTF8;
		    goto cleanup;
		}
		*p <<= 6;
		*p |= s[i] & 0x3f;
		i++;
	    }
	    if (casefold) {
		*p = uctolower(*p);
	    }
	    p++;
	}
	/* normalize ucs of length p - ucs */
	uccompatdecomp(ucs, p - ucs, &ucsout, &ucsoutlen);
	if (approx) {
	    for (j = 0; j < ucsoutlen; j++) {
		if (ucsout[j] < 0x80) {
		    out[outpos++] = ucsout[j];
		}
	    }
	} else {
	    ucsoutlen = uccanoncomp(ucsout, ucsoutlen);
	    /* convert ucs to utf-8 and store in out */
	    for (j = 0; j < ucsoutlen; j++) {
		/*
		 * allocate more space if not enough room for 6 bytes and
		 * terminator
		 */
		if (outsize - outpos < 7) {
		    outsize = ucsoutlen - j + outpos + 6;
		    outtmp = realloc(out, outsize);
		    if (outtmp == NULL) {
			retval = ENOMEM;
			goto cleanup;
		    }
		    out = outtmp;
		}
		outpos += krb5int_ucs4_to_utf8(ucsout[j], &out[outpos]);
	    }
	}

	free(ucsout);
	ucsout = NULL;

	if (i == len) {
	    break;
	}

	/* Allocate more space in out if necessary */
	if (len - i >= outsize - outpos) {
	    outsize += 1 + ((len - i) - (outsize - outpos));
	    outtmp = realloc(out, outsize);
	    if (outtmp == NULL) {
		retval = ENOMEM;
		goto cleanup;
	    }
	    out = outtmp;
	}
	/* s[i] is ascii */
	/* finish off everything up to char before next non-ascii */
	for (i++; (i < len) && KRB5_UTF8_ISASCII(s + i); i++) {
	    out[outpos++] = casefold ? TOLOWER(s[i - 1]) : s[i - 1];
	}
	if (i == len) {
	    out[outpos++] = casefold ? TOLOWER(s[len - 1]) : s[len - 1];
	    break;
	}
	/* convert character before next non-ascii to ucs-4 */
	*ucs = casefold ? TOLOWER(s[i - 1]) : s[i - 1];
	p = ucs + 1;
    }

cleanup:
    free(ucs);
    free(ucsout);
    if (retval) {
	free(out);
	free(newdata);
	return retval;
    }
    out[outpos] = '\0';
    newdata->data = out;
    newdata->length = outpos;
    *newdataptr = newdata;
    return 0;
}

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
