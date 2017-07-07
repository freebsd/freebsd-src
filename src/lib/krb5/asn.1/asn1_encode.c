/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/asn.1/asn1_encode.c */
/*
 * Copyright 1994, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "asn1_encode.h"

/**** Functions for encoding primitive types ****/

asn1_error_code
k5_asn1_encode_bool(asn1buf *buf, intmax_t val, size_t *len_out)
{
    asn1_octet bval = val ? 0xFF : 0x00;

    *len_out = 1;
    return asn1buf_insert_octet(buf, bval);
}

asn1_error_code
k5_asn1_encode_int(asn1buf *buf, intmax_t val, size_t *len_out)
{
    asn1_error_code ret;
    size_t len = 0;
    long valcopy;
    int digit;

    valcopy = val;
    do {
        digit = valcopy & 0xFF;
        ret = asn1buf_insert_octet(buf, digit);
        if (ret)
            return ret;
        len++;
        valcopy = valcopy >> 8;
    } while (valcopy != 0 && valcopy != ~0);

    if (val > 0 && (digit & 0x80) == 0x80) { /* make sure the high bit is */
        ret = asn1buf_insert_octet(buf, 0);  /* of the proper signed-ness */
        if (ret)
            return ret;
        len++;
    } else if (val < 0 && (digit & 0x80) != 0x80) {
        ret = asn1buf_insert_octet(buf, 0xFF);
        if (ret)
            return ret;
        len++;
    }


    *len_out = len;
    return 0;
}

asn1_error_code
k5_asn1_encode_uint(asn1buf *buf, uintmax_t val, size_t *len_out)
{
    asn1_error_code ret;
    size_t len = 0;
    uintmax_t valcopy;
    int digit;

    valcopy = val;
    do {
        digit = valcopy & 0xFF;
        ret = asn1buf_insert_octet(buf, digit);
        if (ret)
            return ret;
        len++;
        valcopy = valcopy >> 8;
    } while (valcopy != 0);

    if (digit & 0x80) {                     /* make sure the high bit is */
        ret = asn1buf_insert_octet(buf, 0); /* of the proper signed-ness */
        if (ret)
            return ret;
        len++;
    }

    *len_out = len;
    return 0;
}

asn1_error_code
k5_asn1_encode_bytestring(asn1buf *buf, unsigned char *const *val, size_t len,
                          size_t *len_out)
{
    if (len > 0 && val == NULL)
        return ASN1_MISSING_FIELD;
    *len_out = len;
    return asn1buf_insert_octetstring(buf, len, *val);
}

asn1_error_code
k5_asn1_encode_generaltime(asn1buf *buf, time_t val, size_t *len_out)
{
    struct tm *gtime, gtimebuf;
    char s[16];
    unsigned char *sp;
    time_t gmt_time = val;
    int len;

    /*
     * Time encoding: YYYYMMDDhhmmssZ
     */
    if (gmt_time == 0) {
        sp = (unsigned char *)"19700101000000Z";
    } else {
        /*
         * Sanity check this just to be paranoid, as gmtime can return NULL,
         * and some bogus implementations might overrun on the sprintf.
         */
#ifdef HAVE_GMTIME_R
#ifdef GMTIME_R_RETURNS_INT
        if (gmtime_r(&gmt_time, &gtimebuf) != 0)
            return ASN1_BAD_GMTIME;
#else
        if (gmtime_r(&gmt_time, &gtimebuf) == NULL)
            return ASN1_BAD_GMTIME;
#endif
#else /* HAVE_GMTIME_R */
        gtime = gmtime(&gmt_time);
        if (gtime == NULL)
            return ASN1_BAD_GMTIME;
        memcpy(&gtimebuf, gtime, sizeof(gtimebuf));
#endif /* HAVE_GMTIME_R */
        gtime = &gtimebuf;

        if (gtime->tm_year > 8099 || gtime->tm_mon > 11 ||
            gtime->tm_mday > 31 || gtime->tm_hour > 23 ||
            gtime->tm_min > 59 || gtime->tm_sec > 59)
            return ASN1_BAD_GMTIME;
        len = snprintf(s, sizeof(s), "%04d%02d%02d%02d%02d%02dZ",
                       1900 + gtime->tm_year, gtime->tm_mon + 1,
                       gtime->tm_mday, gtime->tm_hour,
                       gtime->tm_min, gtime->tm_sec);
        if (SNPRINTF_OVERFLOW(len, sizeof(s)))
            /* Shouldn't be possible given above tests.  */
            return ASN1_BAD_GMTIME;
        sp = (unsigned char *)s;
    }

    return k5_asn1_encode_bytestring(buf, &sp, 15, len_out);
}

asn1_error_code
k5_asn1_encode_bitstring(asn1buf *buf, unsigned char *const *val, size_t len,
                         size_t *len_out)
{
    asn1_error_code ret;

    ret = asn1buf_insert_octetstring(buf, len, *val);
    if (ret)
        return ret;
    *len_out = len + 1;
    return asn1buf_insert_octet(buf, '\0');
}

/**** Functions for decoding primitive types ****/

asn1_error_code
k5_asn1_decode_bool(const unsigned char *asn1, size_t len, intmax_t *val)
{
    if (len != 1)
        return ASN1_BAD_LENGTH;
    *val = (*asn1 != 0);
    return 0;
}

/* Decode asn1/len as the contents of a DER integer, placing the signed result
 * in val. */
asn1_error_code
k5_asn1_decode_int(const unsigned char *asn1, size_t len, intmax_t *val)
{
    intmax_t n;
    size_t i;

    if (len == 0)
        return ASN1_BAD_LENGTH;
    n = (asn1[0] & 0x80) ? -1 : 0;
    /* Check length; allow extra octet if first octet is 0. */
    if (len > sizeof(intmax_t) + (asn1[0] == 0))
        return ASN1_OVERFLOW;
    for (i = 0; i < len; i++)
        n = (n << 8) | asn1[i];
    *val = n;
    return 0;
}

/* Decode asn1/len as the contents of a DER integer, placing the unsigned
 * result in val. */
asn1_error_code
k5_asn1_decode_uint(const unsigned char *asn1, size_t len, uintmax_t *val)
{
    uintmax_t n;
    size_t i;

    if (len == 0)
        return ASN1_BAD_LENGTH;
    /* Check for negative values and check length. */
    if ((asn1[0] & 0x80) || len > sizeof(uintmax_t) + (asn1[0] == 0))
        return ASN1_OVERFLOW;
    for (i = 0, n = 0; i < len; i++)
        n = (n << 8) | asn1[i];
    *val = n;
    return 0;
}

asn1_error_code
k5_asn1_decode_bytestring(const unsigned char *asn1, size_t len,
                          unsigned char **str_out, size_t *len_out)
{
    unsigned char *str;

    *str_out = NULL;
    *len_out = 0;
    if (len == 0)
        return 0;
    str = malloc(len);
    if (str == NULL)
        return ENOMEM;
    memcpy(str, asn1, len);
    *str_out = str;
    *len_out = len;
    return 0;
}

asn1_error_code
k5_asn1_decode_generaltime(const unsigned char *asn1, size_t len,
                           time_t *time_out)
{
    const char *s = (char *)asn1;
    struct tm ts;
    time_t t;

    *time_out = 0;
    if (len != 15)
        return ASN1_BAD_LENGTH;
    /* Time encoding: YYYYMMDDhhmmssZ */
    if (s[14] != 'Z')
        return ASN1_BAD_FORMAT;
    if (memcmp(s, "19700101000000Z", 15) == 0) {
        *time_out = 0;
        return 0;
    }
#define c2i(c) ((c) - '0')
    ts.tm_year = 1000 * c2i(s[0]) + 100 * c2i(s[1]) + 10 * c2i(s[2]) +
        c2i(s[3]) - 1900;
    ts.tm_mon = 10 * c2i(s[4]) + c2i(s[5]) - 1;
    ts.tm_mday = 10 * c2i(s[6]) + c2i(s[7]);
    ts.tm_hour = 10 * c2i(s[8]) + c2i(s[9]);
    ts.tm_min = 10 * c2i(s[10]) + c2i(s[11]);
    ts.tm_sec = 10 * c2i(s[12]) + c2i(s[13]);
    ts.tm_isdst = -1;
    t = krb5int_gmt_mktime(&ts);
    if (t == -1)
        return ASN1_BAD_TIMEFORMAT;
    *time_out = t;
    return 0;
}

/*
 * Note: we return the number of bytes, not bits, in the bit string.  If the
 * number of bits is not a multiple of 8 we effectively round up to the next
 * multiple of 8.
 */
asn1_error_code
k5_asn1_decode_bitstring(const unsigned char *asn1, size_t len,
                         unsigned char **bits_out, size_t *len_out)
{
    unsigned char unused, *bits;

    *bits_out = NULL;
    *len_out = 0;
    if (len == 0)
        return ASN1_BAD_LENGTH;
    unused = *asn1++;
    len--;
    if (unused > 7)
        return ASN1_BAD_FORMAT;

    bits = malloc(len);
    if (bits == NULL)
        return ENOMEM;
    memcpy(bits, asn1, len);
    if (len > 1)
        bits[len - 1] &= (0xff << unused);

    *bits_out = bits;
    *len_out = len;
    return 0;
}

/**** Functions for encoding and decoding tags ****/

/* Encode a DER tag into buf with the tag parameters in t and the content
 * length len.  Place the length of the encoded tag in *retlen. */
static asn1_error_code
make_tag(asn1buf *buf, const taginfo *t, size_t len, size_t *retlen)
{
    asn1_error_code ret;
    asn1_tagnum tag_copy;
    size_t sum = 0, length, len_copy;

    if (t->tagnum > ASN1_TAGNUM_MAX)
        return ASN1_OVERFLOW;

    /* Encode the length of the content within the tag. */
    if (len < 128) {
        ret = asn1buf_insert_octet(buf, len & 0x7F);
        if (ret)
            return ret;
        length = 1;
    } else {
        length = 0;
        for (len_copy = len; len_copy != 0; len_copy >>= 8) {
            ret = asn1buf_insert_octet(buf, len_copy & 0xFF);
            if (ret)
                return ret;
            length++;
        }
        ret = asn1buf_insert_octet(buf, 0x80 | (length & 0x7F));
        if (ret)
            return ret;
        length++;
    }
    sum += length;

    /* Encode the tag and construction bit. */
    if (t->tagnum < 31) {
        ret = asn1buf_insert_octet(buf,
                                   t->asn1class | t->construction | t->tagnum);
        if (ret)
            return ret;
        length = 1;
    } else {
        tag_copy = t->tagnum;
        length = 0;
        ret = asn1buf_insert_octet(buf, tag_copy & 0x7F);
        if (ret)
            return ret;
        tag_copy >>= 7;
        length++;

        for (; tag_copy != 0; tag_copy >>= 7) {
            ret = asn1buf_insert_octet(buf, 0x80 | (tag_copy & 0x7F));
            if (ret)
                return ret;
            length++;
        }

        ret = asn1buf_insert_octet(buf, t->asn1class | t->construction | 0x1F);
        if (ret)
            return ret;
        length++;
    }
    sum += length;

    *retlen = sum;
    return 0;
}

/*
 * Read a BER tag and length from asn1/len.  Place the tag parameters in
 * tag_out.  Set contents_out/clen_out to the octet range of the tag's
 * contents, and remainder_out/rlen_out to the octet range after the end of the
 * BER encoding.
 *
 * (krb5 ASN.1 encodings should be in DER, but for compatibility with some
 * really ancient implementations we handle the indefinite length form in tags.
 * However, we still insist on the primitive form of string types.)
 */
static asn1_error_code
get_tag(const unsigned char *asn1, size_t len, taginfo *tag_out,
        const unsigned char **contents_out, size_t *clen_out,
        const unsigned char **remainder_out, size_t *rlen_out)
{
    asn1_error_code ret;
    unsigned char o;
    const unsigned char *c, *p, *tag_start = asn1;
    size_t clen, llen, i;
    taginfo t;

    *contents_out = *remainder_out = NULL;
    *clen_out = *rlen_out = 0;
    if (len == 0)
        return ASN1_OVERRUN;
    o = *asn1++;
    len--;
    tag_out->asn1class = o & 0xC0;
    tag_out->construction = o & 0x20;
    if ((o & 0x1F) != 0x1F) {
        tag_out->tagnum = o & 0x1F;
    } else {
        tag_out->tagnum = 0;
        do {
            if (len == 0)
                return ASN1_OVERRUN;
            o = *asn1++;
            len--;
            tag_out->tagnum = (tag_out->tagnum << 7) | (o & 0x7F);
        } while (o & 0x80);
    }

    if (len == 0)
        return ASN1_OVERRUN;
    o = *asn1++;
    len--;

    if (o == 0x80) {
        /* Indefinite form (should not be present in DER, but we accept it). */
        if (tag_out->construction != CONSTRUCTED)
            return ASN1_MISMATCH_INDEF;
        p = asn1;
        while (!(len >= 2 && p[0] == 0 && p[1] == 0)) {
            ret = get_tag(p, len, &t, &c, &clen, &p, &len);
            if (ret)
                return ret;
        }
        tag_out->tag_end_len = 2;
        *contents_out = asn1;
        *clen_out = p - asn1;
        *remainder_out = p + 2;
        *rlen_out = len - 2;
    } else if ((o & 0x80) == 0) {
        /* Short form (first octet gives content length). */
        if (o > len)
            return ASN1_OVERRUN;
        tag_out->tag_end_len = 0;
        *contents_out = asn1;
        *clen_out = o;
        *remainder_out = asn1 + *clen_out;
        *rlen_out = len - (*remainder_out - asn1);
    } else {
        /* Long form (first octet gives number of base-256 length octets). */
        llen = o & 0x7F;
        if (llen > len)
            return ASN1_OVERRUN;
        if (llen > sizeof(*clen_out))
            return ASN1_OVERFLOW;
        for (i = 0, clen = 0; i < llen; i++)
            clen = (clen << 8) | asn1[i];
        if (clen > len - llen)
            return ASN1_OVERRUN;
        tag_out->tag_end_len = 0;
        *contents_out = asn1 + llen;
        *clen_out = clen;
        *remainder_out = *contents_out + clen;
        *rlen_out = len - (*remainder_out - asn1);
    }
    tag_out->tag_len = *contents_out - tag_start;
    return 0;
}

#ifdef POINTERS_ARE_ALL_THE_SAME
#define LOADPTR(PTR, TYPE) (*(const void *const *)(PTR))
#define STOREPTR(PTR, TYPE, VAL) (*(void **)(VAL) = (PTR))
#else
#define LOADPTR(PTR, PTRINFO)                                           \
    (assert((PTRINFO)->loadptr != NULL), (PTRINFO)->loadptr(PTR))
#define STOREPTR(PTR, PTRINFO, VAL)                                     \
    (assert((PTRINFO)->storeptr != NULL), (PTRINFO)->storeptr(PTR, VAL))
#endif

static size_t
get_nullterm_sequence_len(const void *valp, const struct atype_info *seq)
{
    size_t i;
    const struct atype_info *a;
    const struct ptr_info *ptr;
    const void *elt, *eltptr;

    a = seq;
    i = 0;
    assert(a->type == atype_ptr);
    assert(seq->size != 0);
    ptr = a->tinfo;

    while (1) {
        eltptr = (const char *)valp + i * seq->size;
        elt = LOADPTR(eltptr, ptr);
        if (elt == NULL)
            break;
        i++;
    }
    return i;
}
static asn1_error_code
encode_sequence_of(asn1buf *buf, size_t seqlen, const void *val,
                   const struct atype_info *eltinfo, size_t *len_out);

static asn1_error_code
encode_nullterm_sequence_of(asn1buf *buf, const void *val,
                            const struct atype_info *type,
                            int can_be_empty, size_t *len_out)
{
    size_t len = get_nullterm_sequence_len(val, type);

    if (!can_be_empty && len == 0)
        return ASN1_MISSING_FIELD;
    return encode_sequence_of(buf, len, val, type, len_out);
}

static intmax_t
load_int(const void *val, size_t size)
{
    switch (size) {
    case 1: return *(signed char *)val;
    case 2: return *(krb5_int16 *)val;
    case 4: return *(krb5_int32 *)val;
    case 8: return *(int64_t *)val;
    default: abort();
    }
}

static uintmax_t
load_uint(const void *val, size_t size)
{
    switch (size) {
    case 1: return *(unsigned char *)val;
    case 2: return *(krb5_ui_2 *)val;
    case 4: return *(krb5_ui_4 *)val;
    case 8: return *(uint64_t *)val;
    default: abort();
    }
}

static asn1_error_code
load_count(const void *val, const struct counted_info *counted,
           size_t *count_out)
{
    const void *countptr = (const char *)val + counted->lenoff;

    assert(sizeof(size_t) <= sizeof(uintmax_t));
    if (counted->lensigned) {
        intmax_t xlen = load_int(countptr, counted->lensize);
        if (xlen < 0 || (uintmax_t)xlen > SIZE_MAX)
            return EINVAL;
        *count_out = xlen;
    } else {
        uintmax_t xlen = load_uint(countptr, counted->lensize);
        if ((size_t)xlen != xlen || xlen > SIZE_MAX)
            return EINVAL;
        *count_out = xlen;
    }
    return 0;
}

static asn1_error_code
store_int(intmax_t intval, size_t size, void *val)
{
    switch (size) {
    case 1:
        if ((signed char)intval != intval)
            return ASN1_OVERFLOW;
        *(signed char *)val = intval;
        return 0;
    case 2:
        if ((krb5_int16)intval != intval)
            return ASN1_OVERFLOW;
        *(krb5_int16 *)val = intval;
        return 0;
    case 4:
        if ((krb5_int32)intval != intval)
            return ASN1_OVERFLOW;
        *(krb5_int32 *)val = intval;
        return 0;
    case 8:
        if ((int64_t)intval != intval)
            return ASN1_OVERFLOW;
        *(int64_t *)val = intval;
        return 0;
    default:
        abort();
    }
}

static asn1_error_code
store_uint(uintmax_t intval, size_t size, void *val)
{
    switch (size) {
    case 1:
        if ((unsigned char)intval != intval)
            return ASN1_OVERFLOW;
        *(unsigned char *)val = intval;
        return 0;
    case 2:
        if ((krb5_ui_2)intval != intval)
            return ASN1_OVERFLOW;
        *(krb5_ui_2 *)val = intval;
        return 0;
    case 4:
        if ((krb5_ui_4)intval != intval)
            return ASN1_OVERFLOW;
        *(krb5_ui_4 *)val = intval;
        return 0;
    case 8:
        if ((uint64_t)intval != intval)
            return ASN1_OVERFLOW;
        *(uint64_t *)val = intval;
        return 0;
    default:
        abort();
    }
}

/* Store a count value in an integer field of a structure.  If count is
 * SIZE_MAX and the target is a signed field, store -1. */
static asn1_error_code
store_count(size_t count, const struct counted_info *counted, void *val)
{
    void *countptr = (char *)val + counted->lenoff;

    if (counted->lensigned) {
        if (count == SIZE_MAX)
            return store_int(-1, counted->lensize, countptr);
        else if ((intmax_t)count < 0)
            return ASN1_OVERFLOW;
        else
            return store_int(count, counted->lensize, countptr);
    } else
        return store_uint(count, counted->lensize, countptr);
}

/* Split a DER encoding into tag and contents.  Insert the contents into buf,
 * then return the length of the contents and the tag. */
static asn1_error_code
split_der(asn1buf *buf, unsigned char *const *der, size_t len,
          taginfo *tag_out, size_t *len_out)
{
    asn1_error_code ret;
    const unsigned char *contents, *remainder;
    size_t clen, rlen;

    ret = get_tag(*der, len, tag_out, &contents, &clen, &remainder, &rlen);
    if (ret)
        return ret;
    if (rlen != 0)
        return ASN1_BAD_LENGTH;
    *len_out = clen;
    return asn1buf_insert_bytestring(buf, clen, contents);
}

/*
 * Store the DER encoding given by t and asn1/len into the char * or
 * unsigned char * pointed to by val.  Set *count_out to the length of the
 * DER encoding.
 */
static asn1_error_code
store_der(const taginfo *t, const unsigned char *asn1, size_t len, void *val,
          size_t *count_out)
{
    unsigned char *der;
    size_t der_len;

    *count_out = 0;
    der_len = t->tag_len + len + t->tag_end_len;
    der = malloc(der_len);
    if (der == NULL)
        return ENOMEM;
    memcpy(der, asn1 - t->tag_len, der_len);
    *(unsigned char **)val = der;
    *count_out = der_len;
    return 0;
}

static asn1_error_code
encode_sequence(asn1buf *buf, const void *val, const struct seq_info *seq,
                size_t *len_out);
static asn1_error_code
encode_cntype(asn1buf *buf, const void *val, size_t len,
              const struct cntype_info *c, taginfo *tag_out, size_t *len_out);

/* Encode a value (contents only, no outer tag) according to a type, and return
 * its encoded tag information. */
static asn1_error_code
encode_atype(asn1buf *buf, const void *val, const struct atype_info *a,
             taginfo *tag_out, size_t *len_out)
{
    asn1_error_code ret;

    if (val == NULL)
        return ASN1_MISSING_FIELD;

    switch (a->type) {
    case atype_fn: {
        const struct fn_info *fn = a->tinfo;
        assert(fn->enc != NULL);
        return fn->enc(buf, val, tag_out, len_out);
    }
    case atype_sequence:
        assert(a->tinfo != NULL);
        ret = encode_sequence(buf, val, a->tinfo, len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = CONSTRUCTED;
        tag_out->tagnum = ASN1_SEQUENCE;
        break;
    case atype_ptr: {
        const struct ptr_info *ptr = a->tinfo;
        assert(ptr->basetype != NULL);
        return encode_atype(buf, LOADPTR(val, ptr), ptr->basetype, tag_out,
                            len_out);
    }
    case atype_offset: {
        const struct offset_info *off = a->tinfo;
        assert(off->basetype != NULL);
        return encode_atype(buf, (const char *)val + off->dataoff,
                            off->basetype, tag_out, len_out);
    }
    case atype_optional: {
        const struct optional_info *opt = a->tinfo;
        assert(opt->is_present != NULL);
        if (opt->is_present(val))
            return encode_atype(buf, val, opt->basetype, tag_out, len_out);
        else
            return ASN1_OMITTED;
    }
    case atype_counted: {
        const struct counted_info *counted = a->tinfo;
        const void *dataptr = (const char *)val + counted->dataoff;
        size_t count;
        assert(counted->basetype != NULL);
        ret = load_count(val, counted, &count);
        if (ret)
            return ret;
        return encode_cntype(buf, dataptr, count, counted->basetype, tag_out,
                             len_out);
    }
    case atype_nullterm_sequence_of:
    case atype_nonempty_nullterm_sequence_of:
        assert(a->tinfo != NULL);
        ret = encode_nullterm_sequence_of(buf, val, a->tinfo,
                                          a->type ==
                                          atype_nullterm_sequence_of,
                                          len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = CONSTRUCTED;
        tag_out->tagnum = ASN1_SEQUENCE;
        break;
    case atype_tagged_thing: {
        const struct tagged_info *tag = a->tinfo;
        ret = encode_atype(buf, val, tag->basetype, tag_out, len_out);
        if (ret)
            return ret;
        if (!tag->implicit) {
            size_t tlen;
            ret = make_tag(buf, tag_out, *len_out, &tlen);
            if (ret)
                return ret;
            *len_out += tlen;
            tag_out->construction = tag->construction;
        }
        tag_out->asn1class = tag->tagtype;
        tag_out->tagnum = tag->tagval;
        break;
    }
    case atype_bool:
        ret = k5_asn1_encode_bool(buf, load_int(val, a->size), len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = PRIMITIVE;
        tag_out->tagnum = ASN1_BOOLEAN;
        break;
    case atype_int:
        ret = k5_asn1_encode_int(buf, load_int(val, a->size), len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = PRIMITIVE;
        tag_out->tagnum = ASN1_INTEGER;
        break;
    case atype_uint:
        ret = k5_asn1_encode_uint(buf, load_uint(val, a->size), len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = PRIMITIVE;
        tag_out->tagnum = ASN1_INTEGER;
        break;
    case atype_int_immediate: {
        const struct immediate_info *imm = a->tinfo;
        ret = k5_asn1_encode_int(buf, imm->val, len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = PRIMITIVE;
        tag_out->tagnum = ASN1_INTEGER;
        break;
    }
    default:
        assert(a->type > atype_min);
        assert(a->type < atype_max);
        abort();
    }

    return 0;
}

static asn1_error_code
encode_atype_and_tag(asn1buf *buf, const void *val, const struct atype_info *a,
                     size_t *len_out)
{
    taginfo t;
    asn1_error_code ret;
    size_t clen, tlen;

    ret = encode_atype(buf, val, a, &t, &clen);
    if (ret)
        return ret;
    ret = make_tag(buf, &t, clen, &tlen);
    if (ret)
        return ret;
    *len_out = clen + tlen;
    return 0;
}

/*
 * Encode an object and count according to a cntype_info structure.  val is a
 * pointer to the object being encoded, which in most cases is itself a
 * pointer (but is a union in the cntype_choice case).
 */
static asn1_error_code
encode_cntype(asn1buf *buf, const void *val, size_t count,
              const struct cntype_info *c, taginfo *tag_out, size_t *len_out)
{
    asn1_error_code ret;

    switch (c->type) {
    case cntype_string: {
        const struct string_info *string = c->tinfo;
        assert(string->enc != NULL);
        ret = string->enc(buf, val, count, len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = PRIMITIVE;
        tag_out->tagnum = string->tagval;
        break;
    }
    case cntype_der:
        return split_der(buf, val, count, tag_out, len_out);
    case cntype_seqof: {
        const struct atype_info *a = c->tinfo;
        const struct ptr_info *ptr = a->tinfo;
        assert(a->type == atype_ptr);
        val = LOADPTR(val, ptr);
        ret = encode_sequence_of(buf, count, val, ptr->basetype, len_out);
        if (ret)
            return ret;
        tag_out->asn1class = UNIVERSAL;
        tag_out->construction = CONSTRUCTED;
        tag_out->tagnum = ASN1_SEQUENCE;
        break;
    }
    case cntype_choice: {
        const struct choice_info *choice = c->tinfo;
        if (count >= choice->n_options)
            return ASN1_MISSING_FIELD;
        return encode_atype(buf, val, choice->options[count], tag_out,
                            len_out);
    }

    default:
        assert(c->type > cntype_min);
        assert(c->type < cntype_max);
        abort();
    }

    return 0;
}

static asn1_error_code
encode_sequence(asn1buf *buf, const void *val, const struct seq_info *seq,
                size_t *len_out)
{
    asn1_error_code ret;
    size_t i, len, sum = 0;

    for (i = seq->n_fields; i > 0; i--) {
        ret = encode_atype_and_tag(buf, val, seq->fields[i - 1], &len);
        if (ret == ASN1_OMITTED)
            continue;
        else if (ret != 0)
            return ret;
        sum += len;
    }
    *len_out = sum;
    return 0;
}

static asn1_error_code
encode_sequence_of(asn1buf *buf, size_t seqlen, const void *val,
                   const struct atype_info *eltinfo, size_t *len_out)
{
    asn1_error_code ret;
    size_t sum = 0, i, len;
    const void *eltptr;

    assert(eltinfo->size != 0);
    for (i = seqlen; i > 0; i--) {
        eltptr = (const char *)val + (i - 1) * eltinfo->size;
        ret = encode_atype_and_tag(buf, eltptr, eltinfo, &len);
        if (ret)
            return ret;
        sum += len;
    }
    *len_out = sum;
    return 0;
}

/**** Functions for freeing C objects based on type info ****/

static void free_atype_ptr(const struct atype_info *a, void *val);
static void free_sequence(const struct seq_info *seq, void *val);
static void free_sequence_of(const struct atype_info *eltinfo, void *val,
                             size_t count);
static void free_cntype(const struct cntype_info *a, void *val, size_t count);

/*
 * Free a C object according to a type description.  Do not free pointers at
 * the first level; they may be referenced by other fields of a sequence, and
 * will be freed by free_atype_ptr in a second pass.
 */
static void
free_atype(const struct atype_info *a, void *val)
{
    switch (a->type) {
    case atype_fn: {
        const struct fn_info *fn = a->tinfo;
        if (fn->free_func != NULL)
            fn->free_func(val);
        break;
    }
    case atype_sequence:
        free_sequence(a->tinfo, val);
        break;
    case atype_ptr: {
        const struct ptr_info *ptrinfo = a->tinfo;
        void *ptr = LOADPTR(val, ptrinfo);
        if (ptr != NULL) {
            free_atype(ptrinfo->basetype, ptr);
            free_atype_ptr(ptrinfo->basetype, ptr);
        }
        break;
    }
    case atype_offset: {
        const struct offset_info *off = a->tinfo;
        assert(off->basetype != NULL);
        free_atype(off->basetype, (char *)val + off->dataoff);
        break;
    }
    case atype_optional: {
        const struct optional_info *opt = a->tinfo;
        free_atype(opt->basetype, val);
        break;
    }
    case atype_counted: {
        const struct counted_info *counted = a->tinfo;
        void *dataptr = (char *)val + counted->dataoff;
        size_t count;
        if (load_count(val, counted, &count) == 0)
            free_cntype(counted->basetype, dataptr, count);
        break;
    }
    case atype_nullterm_sequence_of:
    case atype_nonempty_nullterm_sequence_of: {
        size_t count = get_nullterm_sequence_len(val, a->tinfo);
        free_sequence_of(a->tinfo, val, count);
        break;
    }
    case atype_tagged_thing: {
        const struct tagged_info *tag = a->tinfo;
        free_atype(tag->basetype, val);
        break;
    }
    case atype_bool:
    case atype_int:
    case atype_uint:
    case atype_int_immediate:
        break;
    default:
        abort();
    }
}

static void
free_atype_ptr(const struct atype_info *a, void *val)
{
    switch (a->type) {
    case atype_fn:
    case atype_sequence:
    case atype_counted:
    case atype_nullterm_sequence_of:
    case atype_nonempty_nullterm_sequence_of:
    case atype_bool:
    case atype_int:
    case atype_uint:
    case atype_int_immediate:
         break;
    case atype_ptr: {
        const struct ptr_info *ptrinfo = a->tinfo;
        void *ptr = LOADPTR(val, ptrinfo);
        free(ptr);
        STOREPTR(NULL, ptrinfo, val);
        break;
    }
    case atype_offset: {
        const struct offset_info *off = a->tinfo;
        assert(off->basetype != NULL);
        free_atype_ptr(off->basetype, (char *)val + off->dataoff);
        break;
    }
    case atype_optional: {
        const struct optional_info *opt = a->tinfo;
        free_atype_ptr(opt->basetype, val);
        break;
    }
    case atype_tagged_thing: {
        const struct tagged_info *tag = a->tinfo;
        free_atype_ptr(tag->basetype, val);
        break;
    }
    default:
        abort();
    }
}

static void
free_cntype(const struct cntype_info *c, void *val, size_t count)
{
    switch (c->type) {
    case cntype_string:
    case cntype_der:
        free(*(char **)val);
        *(char **)val = NULL;
        break;
    case cntype_seqof: {
        const struct atype_info *a = c->tinfo;
        const struct ptr_info *ptrinfo = a->tinfo;
        void *seqptr = LOADPTR(val, ptrinfo);
        free_sequence_of(ptrinfo->basetype, seqptr, count);
        free(seqptr);
        STOREPTR(NULL, ptrinfo, val);
        break;
    }
    case cntype_choice: {
        const struct choice_info *choice = c->tinfo;
        if (count < choice->n_options) {
            free_atype(choice->options[count], val);
            free_atype_ptr(choice->options[count], val);
        }
        break;
    }
    default:
        abort();
    }
}

static void
free_sequence(const struct seq_info *seq, void *val)
{
    size_t i;

    for (i = 0; i < seq->n_fields; i++)
        free_atype(seq->fields[i], val);
    for (i = 0; i < seq->n_fields; i++)
        free_atype_ptr(seq->fields[i], val);
}

static void
free_sequence_of(const struct atype_info *eltinfo, void *val, size_t count)
{
    void *eltptr;

    assert(eltinfo->size != 0);
    while (count-- > 0) {
        eltptr = (char *)val + count * eltinfo->size;
        free_atype(eltinfo, eltptr);
        free_atype_ptr(eltinfo, eltptr);
    }
}

/**** Functions for decoding objects based on type info ****/

/* Return nonzero if t is an expected tag for an ASN.1 object of type a. */
static int
check_atype_tag(const struct atype_info *a, const taginfo *t)
{
    switch (a->type) {
    case atype_fn: {
        const struct fn_info *fn = a->tinfo;
        assert(fn->check_tag != NULL);
        return fn->check_tag(t);
    }
    case atype_sequence:
    case atype_nullterm_sequence_of:
    case atype_nonempty_nullterm_sequence_of:
        return (t->asn1class == UNIVERSAL && t->construction == CONSTRUCTED &&
                t->tagnum == ASN1_SEQUENCE);
    case atype_ptr: {
        const struct ptr_info *ptrinfo = a->tinfo;
        return check_atype_tag(ptrinfo->basetype, t);
    }
    case atype_offset: {
        const struct offset_info *off = a->tinfo;
        return check_atype_tag(off->basetype, t);
    }
    case atype_optional: {
        const struct optional_info *opt = a->tinfo;
        return check_atype_tag(opt->basetype, t);
    }
    case atype_counted: {
        const struct counted_info *counted = a->tinfo;
        switch (counted->basetype->type) {
        case cntype_string: {
            const struct string_info *string = counted->basetype->tinfo;
            return (t->asn1class == UNIVERSAL &&
                    t->construction == PRIMITIVE &&
                    t->tagnum == string->tagval);
        }
        case cntype_seqof:
            return (t->asn1class == UNIVERSAL &&
                    t->construction == CONSTRUCTED &&
                    t->tagnum == ASN1_SEQUENCE);
        case cntype_der:
            /*
             * We treat any tag as matching a stored DER encoding.  In some
             * cases we know what the tag should be; in others, we truly want
             * to accept any tag.  If it ever becomes an issue, we could add
             * optional tag info to the type and check it here.
             */
            return 1;
        case cntype_choice:
            /*
             * ASN.1 choices may or may not be extensible.  For now, we treat
             * all choices as extensible and match any tag.  We should consider
             * modeling whether choices are extensible before making the
             * encoder visible to plugins.
             */
            return 1;
        default:
            abort();
        }
    }
    case atype_tagged_thing: {
        const struct tagged_info *tag = a->tinfo;
        /* NOTE: Doesn't check construction bit for implicit tags. */
        if (!tag->implicit && t->construction != tag->construction)
            return 0;
        return (t->asn1class == tag->tagtype && t->tagnum == tag->tagval);
    }
    case atype_bool:
        return (t->asn1class == UNIVERSAL && t->construction == PRIMITIVE &&
                t->tagnum == ASN1_BOOLEAN);
    case atype_int:
    case atype_uint:
    case atype_int_immediate:
        return (t->asn1class == UNIVERSAL && t->construction == PRIMITIVE &&
                t->tagnum == ASN1_INTEGER);
    default:
        abort();
    }
}

static asn1_error_code
decode_cntype(const taginfo *t, const unsigned char *asn1, size_t len,
              const struct cntype_info *c, void *val, size_t *count_out);
static asn1_error_code
decode_atype_to_ptr(const taginfo *t, const unsigned char *asn1, size_t len,
                    const struct atype_info *basetype, void **ptr_out);
static asn1_error_code
decode_sequence(const unsigned char *asn1, size_t len,
                const struct seq_info *seq, void *val);
static asn1_error_code
decode_sequence_of(const unsigned char *asn1, size_t len,
                   const struct atype_info *elemtype, void **seq_out,
                   size_t *count_out);

/* Given the enclosing tag t, decode from asn1/len the contents of the ASN.1
 * type specified by a, placing the result into val (caller-allocated). */
static asn1_error_code
decode_atype(const taginfo *t, const unsigned char *asn1,
             size_t len, const struct atype_info *a, void *val)
{
    asn1_error_code ret;

    switch (a->type) {
    case atype_fn: {
        const struct fn_info *fn = a->tinfo;
        assert(fn->dec != NULL);
        return fn->dec(t, asn1, len, val);
    }
    case atype_sequence:
        return decode_sequence(asn1, len, a->tinfo, val);
    case atype_ptr: {
        const struct ptr_info *ptrinfo = a->tinfo;
        void *ptr = LOADPTR(val, ptrinfo);
        assert(ptrinfo->basetype != NULL);
        if (ptr != NULL) {
            /* Container was already allocated by a previous sequence field. */
            return decode_atype(t, asn1, len, ptrinfo->basetype, ptr);
        } else {
            ret = decode_atype_to_ptr(t, asn1, len, ptrinfo->basetype, &ptr);
            if (ret)
                return ret;
            STOREPTR(ptr, ptrinfo, val);
            break;
        }
    }
    case atype_offset: {
        const struct offset_info *off = a->tinfo;
        assert(off->basetype != NULL);
        return decode_atype(t, asn1, len, off->basetype,
                            (char *)val + off->dataoff);
    }
    case atype_optional: {
        const struct optional_info *opt = a->tinfo;
        return decode_atype(t, asn1, len, opt->basetype, val);
    }
    case atype_counted: {
        const struct counted_info *counted = a->tinfo;
        void *dataptr = (char *)val + counted->dataoff;
        size_t count;
        assert(counted->basetype != NULL);
        ret = decode_cntype(t, asn1, len, counted->basetype, dataptr, &count);
        if (ret)
            return ret;
        return store_count(count, counted, val);
    }
    case atype_tagged_thing: {
        const struct tagged_info *tag = a->tinfo;
        taginfo inner_tag;
        const taginfo *tp = t;
        const unsigned char *rem;
        size_t rlen;
        if (!tag->implicit) {
            ret = get_tag(asn1, len, &inner_tag, &asn1, &len, &rem, &rlen);
            if (ret)
                return ret;
            /* Note: we don't check rlen (it should be 0). */
            tp = &inner_tag;
            if (!check_atype_tag(tag->basetype, tp))
                return ASN1_BAD_ID;
        }
        return decode_atype(tp, asn1, len, tag->basetype, val);
    }
    case atype_bool: {
        intmax_t intval;
        ret = k5_asn1_decode_bool(asn1, len, &intval);
        if (ret)
            return ret;
        return store_int(intval, a->size, val);
    }
    case atype_int: {
        intmax_t intval;
        ret = k5_asn1_decode_int(asn1, len, &intval);
        if (ret)
            return ret;
        return store_int(intval, a->size, val);
    }
    case atype_uint: {
        uintmax_t intval;
        ret = k5_asn1_decode_uint(asn1, len, &intval);
        if (ret)
            return ret;
        return store_uint(intval, a->size, val);
    }
    case atype_int_immediate: {
        const struct immediate_info *imm = a->tinfo;
        intmax_t intval;
        ret = k5_asn1_decode_int(asn1, len, &intval);
        if (ret)
            return ret;
        if (intval != imm->val && imm->err != 0)
            return imm->err;
        break;
    }
    default:
        /* Null-terminated sequence types are handled in decode_atype_to_ptr,
         * since they create variable-sized objects. */
        assert(a->type != atype_nullterm_sequence_of);
        assert(a->type != atype_nonempty_nullterm_sequence_of);
        assert(a->type > atype_min);
        assert(a->type < atype_max);
        abort();
    }
    return 0;
}

/*
 * Given the enclosing tag t, decode from asn1/len the contents of the
 * ASN.1 type described by c, placing the counted result into val/count_out.
 * If the resulting count should be -1 (for an unknown union distinguisher),
 * set *count_out to SIZE_MAX.
 */
static asn1_error_code
decode_cntype(const taginfo *t, const unsigned char *asn1, size_t len,
              const struct cntype_info *c, void *val, size_t *count_out)
{
    asn1_error_code ret;

    switch (c->type) {
    case cntype_string: {
        const struct string_info *string = c->tinfo;
        assert(string->dec != NULL);
        return string->dec(asn1, len, val, count_out);
    }
    case cntype_der:
        return store_der(t, asn1, len, val, count_out);
    case cntype_seqof: {
        const struct atype_info *a = c->tinfo;
        const struct ptr_info *ptrinfo = a->tinfo;
        void *seq;
        assert(a->type == atype_ptr);
        ret = decode_sequence_of(asn1, len, ptrinfo->basetype, &seq,
                                 count_out);
        if (ret)
            return ret;
        STOREPTR(seq, ptrinfo, val);
        break;
    }
    case cntype_choice: {
        const struct choice_info *choice = c->tinfo;
        size_t i;
        for (i = 0; i < choice->n_options; i++) {
            if (check_atype_tag(choice->options[i], t)) {
                ret = decode_atype(t, asn1, len, choice->options[i], val);
                if (ret)
                    return ret;
                *count_out = i;
                return 0;
            }
        }
        /* SIZE_MAX will be stored as -1 in the distinguisher.  If we start
         * modeling non-extensible choices we should check that here. */
        *count_out = SIZE_MAX;
        break;
    }
    default:
        assert(c->type > cntype_min);
        assert(c->type < cntype_max);
        abort();
    }
    return 0;
}

/* Add a null pointer to the end of a sequence.  ptr is consumed on success
 * (to be replaced by *ptr_out), left alone on failure. */
static asn1_error_code
null_terminate(const struct atype_info *eltinfo, void *ptr, size_t count,
               void **ptr_out)
{
    const struct ptr_info *ptrinfo = eltinfo->tinfo;
    void *endptr;

    assert(eltinfo->type == atype_ptr);
    ptr = realloc(ptr, (count + 1) * eltinfo->size);
    if (ptr == NULL)
        return ENOMEM;
    endptr = (char *)ptr + count * eltinfo->size;
    STOREPTR(NULL, ptrinfo, endptr);
    *ptr_out = ptr;
    return 0;
}

static asn1_error_code
decode_atype_to_ptr(const taginfo *t, const unsigned char *asn1,
                    size_t len, const struct atype_info *a,
                    void **ptr_out)
{
    asn1_error_code ret;
    void *ptr;
    size_t count;

    *ptr_out = NULL;
    switch (a->type) {
    case atype_nullterm_sequence_of:
    case atype_nonempty_nullterm_sequence_of:
        ret = decode_sequence_of(asn1, len, a->tinfo, &ptr, &count);
        if (ret)
            return ret;
        ret = null_terminate(a->tinfo, ptr, count, &ptr);
        if (ret) {
            free_sequence_of(a->tinfo, ptr, count);
            return ret;
        }
        /* Historically we do not enforce non-emptiness of sequences when
         * decoding, even when it is required by the ASN.1 type. */
        break;
    default:
        ptr = calloc(a->size, 1);
        if (ptr == NULL)
            return ENOMEM;
        ret = decode_atype(t, asn1, len, a, ptr);
        if (ret) {
            free(ptr);
            return ret;
        }
        break;
    }
    *ptr_out = ptr;
    return 0;
}

/* Initialize a C object when the corresponding ASN.1 type was omitted within a
 * sequence.  If the ASN.1 type is not optional, return ASN1_MISSING_FIELD. */
static asn1_error_code
omit_atype(const struct atype_info *a, void *val)
{
    switch (a->type)
    {
    case atype_fn:
    case atype_sequence:
    case atype_nullterm_sequence_of:
    case atype_nonempty_nullterm_sequence_of:
    case atype_counted:
    case atype_bool:
    case atype_int:
    case atype_uint:
    case atype_int_immediate:
        return ASN1_MISSING_FIELD;
    case atype_ptr: {
        const struct ptr_info *ptrinfo = a->tinfo;
        return omit_atype(ptrinfo->basetype, val);
    }
    case atype_offset: {
        const struct offset_info *off = a->tinfo;
        return omit_atype(off->basetype, (char *)val + off->dataoff);
    }
    case atype_tagged_thing: {
        const struct tagged_info *tag = a->tinfo;
        return omit_atype(tag->basetype, val);
    }
    case atype_optional: {
        const struct optional_info *opt = a->tinfo;
        if (opt->init != NULL)
            opt->init(val);
        return 0;
    }
    default:
        abort();
    }
}

/* Decode an ASN.1 sequence into a C object. */
static asn1_error_code
decode_sequence(const unsigned char *asn1, size_t len,
                const struct seq_info *seq, void *val)
{
    asn1_error_code ret;
    const unsigned char *contents;
    size_t i, j, clen;
    taginfo t;

    assert(seq->n_fields > 0);
    for (i = 0; i < seq->n_fields; i++) {
        if (len == 0)
            break;
        ret = get_tag(asn1, len, &t, &contents, &clen, &asn1, &len);
        if (ret)
            goto error;
        /*
         * Find the applicable sequence field.  This logic is a little
         * oversimplified; we could match an element to an optional extensible
         * choice or optional stored-DER type when we ought to match a
         * subsequent non-optional field.  But it's unwise and (hopefully) very
         * rare for ASN.1 modules to require such precision.
         */
        for (; i < seq->n_fields; i++) {
            if (check_atype_tag(seq->fields[i], &t))
                break;
            ret = omit_atype(seq->fields[i], val);
            if (ret)
                goto error;
        }
        /* We currently model all sequences as extensible.  We should consider
         * changing this before making the encoder visible to plugins. */
        if (i == seq->n_fields)
            break;
        ret = decode_atype(&t, contents, clen, seq->fields[i], val);
        if (ret)
            goto error;
    }
    /* Initialize any fields in the C object which were not accounted for in
     * the sequence.  Error out if any of them aren't optional. */
    for (; i < seq->n_fields; i++) {
        ret = omit_atype(seq->fields[i], val);
        if (ret)
            goto error;
    }
    return 0;

error:
    /* Free what we've decoded so far.  Free pointers in a second pass in
     * case multiple fields refer to the same pointer. */
    for (j = 0; j < i; j++)
        free_atype(seq->fields[j], val);
    for (j = 0; j < i; j++)
        free_atype_ptr(seq->fields[j], val);
    return ret;
}

static asn1_error_code
decode_sequence_of(const unsigned char *asn1, size_t len,
                   const struct atype_info *elemtype, void **seq_out,
                   size_t *count_out)
{
    asn1_error_code ret;
    void *seq = NULL, *elem, *newseq;
    const unsigned char *contents;
    size_t clen, count = 0;
    taginfo t;

    *seq_out = NULL;
    *count_out = 0;
    while (len > 0) {
        ret = get_tag(asn1, len, &t, &contents, &clen, &asn1, &len);
        if (ret)
            goto error;
        if (!check_atype_tag(elemtype, &t)) {
            ret = ASN1_BAD_ID;
            goto error;
        }
        newseq = realloc(seq, (count + 1) * elemtype->size);
        if (newseq == NULL) {
            ret = ENOMEM;
            goto error;
        }
        seq = newseq;
        elem = (char *)seq + count * elemtype->size;
        memset(elem, 0, elemtype->size);
        ret = decode_atype(&t, contents, clen, elemtype, elem);
        if (ret)
            goto error;
        count++;
    }
    *seq_out = seq;
    *count_out = count;
    return 0;

error:
    free_sequence_of(elemtype, seq, count);
    free(seq);
    return ret;
}

/* These three entry points are only needed for the kdc_req_body hack and may
 * go away at some point.  Define them here so we can use short names above. */

asn1_error_code
k5_asn1_encode_atype(asn1buf *buf, const void *val, const struct atype_info *a,
                     taginfo *tag_out, size_t *len_out)
{
    return encode_atype(buf, val, a, tag_out, len_out);
}

asn1_error_code
k5_asn1_decode_atype(const taginfo *t, const unsigned char *asn1,
                     size_t len, const struct atype_info *a, void *val)
{
    return decode_atype(t, asn1, len, a, val);
}

krb5_error_code
k5_asn1_full_encode(const void *rep, const struct atype_info *a,
                    krb5_data **code_out)
{
    size_t len;
    asn1_error_code ret;
    asn1buf *buf = NULL;
    krb5_data *d;

    *code_out = NULL;

    if (rep == NULL)
        return ASN1_MISSING_FIELD;
    ret = asn1buf_create(&buf);
    if (ret)
        return ret;
    ret = encode_atype_and_tag(buf, rep, a, &len);
    if (ret)
        goto cleanup;
    ret = asn12krb5_buf(buf, &d);
    if (ret)
        goto cleanup;
    *code_out = d;
cleanup:
    asn1buf_destroy(&buf);
    return ret;
}

asn1_error_code
k5_asn1_full_decode(const krb5_data *code, const struct atype_info *a,
                    void **retrep)
{
    asn1_error_code ret;
    const unsigned char *contents, *remainder;
    size_t clen, rlen;
    taginfo t;

    *retrep = NULL;
    ret = get_tag((unsigned char *)code->data, code->length, &t, &contents,
                  &clen, &remainder, &rlen);
    if (ret)
        return ret;
    /* rlen should be 0, but we don't check it (and due to padding in
     * non-length-preserving enctypes, it will sometimes be nonzero). */
    if (!check_atype_tag(a, &t))
        return ASN1_BAD_ID;
    return decode_atype_to_ptr(&t, contents, clen, a, retrep);
}
