/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* Coding Buffer Implementation */

/*
 *  Implementation
 *
 *    Encoding mode
 *
 *    The encoding buffer is filled from bottom (lowest address) to top
 *    (highest address).  This makes it easier to expand the buffer,
 *    since realloc preserves the existing portion of the buffer.
 *
 *    Note: Since ASN.1 encoding must be done in reverse, this means
 *    that you can't simply memcpy out the buffer data, since it will be
 *    backwards.  You need to reverse-iterate through it, instead.
 *
 *    ***This decision may have been a mistake.  In practice, the
 *    implementation will probably be tuned such that reallocation is
 *    rarely necessary.  Also, the realloc probably has recopy the
 *    buffer itself, so we don't really gain that much by avoiding an
 *    explicit copy of the buffer.  --Keep this in mind for future reference.
 *
 *
 *    Decoding mode
 *
 *    The decoding buffer is in normal order and is created by wrapping
 *    an asn1buf around a krb5_data structure.
 */

/*
 * Abstraction Function
 *
 *   Programs should use just pointers to asn1buf's (e.g. asn1buf *mybuf).
 *   These pointers must always point to a valid, allocated asn1buf
 *   structure or be NULL.
 *
 *   The contents of the asn1buf represent an octet string.  This string
 *   begins at base and continues to the octet immediately preceding next.
 *   If next == base or mybuf == NULL, then the asn1buf represents an empty
 *   octet string.
 */

/*
 * Representation Invariant
 *
 *   Pointers to asn1buf's must always point to a valid, allocated
 *   asn1buf structure or be NULL.
 *
 *   base points to a valid, allocated octet array or is NULL
 *   bound, if non-NULL, points to the last valid octet
 *   next >= base
 *   next <= bound+2  (i.e. next should be able to step just past the bound,
 *                     but no further.  (The bound should move out in response
 *                     to being crossed by next.))
 */

#define ASN1BUF_OMIT_INLINE_FUNCS
#include "asn1buf.h"
#include <stdio.h>

#ifdef USE_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_CHECK_READABLE(PTR,SIZE) ((void)0)
#endif

#if !defined(__GNUC__) || defined(CONFIG_SMALL)
/*
 * Declare private procedures as static if they're not used for inline
 * expansion of other stuff elsewhere.
 */
static unsigned int asn1buf_free(const asn1buf *);
static asn1_error_code asn1buf_ensure_space(asn1buf *, unsigned int);
static asn1_error_code asn1buf_expand(asn1buf *, unsigned int);
#endif

#define asn1_is_eoc(class, num, indef)                  \
    ((class) == UNIVERSAL && !(num) && !(indef))

asn1_error_code
asn1buf_create(asn1buf **buf)
{
    *buf = (asn1buf*)malloc(sizeof(asn1buf));
    if (*buf == NULL) return ENOMEM;
    (*buf)->base = NULL;
    (*buf)->bound = NULL;
    (*buf)->next = NULL;
    return 0;
}

void
asn1buf_destroy(asn1buf **buf)
{
    if (*buf != NULL) {
        free((*buf)->base);
        free(*buf);
        *buf = NULL;
    }
}

#ifdef asn1buf_insert_octet
#undef asn1buf_insert_octet
#endif
asn1_error_code
asn1buf_insert_octet(asn1buf *buf, const int o)
{
    asn1_error_code retval;

    retval = asn1buf_ensure_space(buf,1U);
    if (retval) return retval;
    *(buf->next) = (char)o;
    (buf->next)++;
    return 0;
}

asn1_error_code
asn1buf_insert_bytestring(asn1buf *buf, const unsigned int len, const void *sv)
{
    asn1_error_code retval;
    unsigned int length;
    const char *s = sv;

    retval = asn1buf_ensure_space(buf,len);
    if (retval) return retval;
    VALGRIND_CHECK_READABLE(sv, len);
    for (length=1; length<=len; length++,(buf->next)++)
        *(buf->next) = (s[len-length]);
    return 0;
}

asn1_error_code
asn12krb5_buf(const asn1buf *buf, krb5_data **code)
{
    unsigned int i;
    krb5_data *d;

    *code = NULL;

    d = calloc(1, sizeof(krb5_data));
    if (d == NULL)
        return ENOMEM;
    d->length = asn1buf_len(buf);
    d->data = malloc(d->length + 1);
    if (d->data == NULL) {
        free(d);
        return ENOMEM;
    }
    for (i=0; i < d->length; i++)
        d->data[i] = buf->base[d->length - i - 1];
    d->data[d->length] = '\0';
    d->magic = KV5M_DATA;
    *code = d;
    return 0;
}

/****************************************************************/
/* Private Procedures */

static int
asn1buf_size(const asn1buf *buf)
{
    if (buf == NULL || buf->base == NULL) return 0;
    return buf->bound - buf->base + 1;
}

#undef asn1buf_free
unsigned int
asn1buf_free(const asn1buf *buf)
{
    if (buf == NULL || buf->base == NULL) return 0;
    else return buf->bound - buf->next + 1;
}

#undef asn1buf_ensure_space
asn1_error_code
asn1buf_ensure_space(asn1buf *buf, const unsigned int amount)
{
    unsigned int avail = asn1buf_free(buf);
    if (avail >= amount)
        return 0;
    return asn1buf_expand(buf, amount-avail);
}

asn1_error_code
asn1buf_expand(asn1buf *buf, unsigned int inc)
{
#define STANDARD_INCREMENT 200
    int next_offset = buf->next - buf->base;
    int bound_offset;
    if (buf->base == NULL) bound_offset = -1;
    else bound_offset = buf->bound - buf->base;

    if (inc < STANDARD_INCREMENT)
        inc = STANDARD_INCREMENT;

    buf->base = realloc(buf->base,
                        (asn1buf_size(buf)+inc) * sizeof(asn1_octet));
    if (buf->base == NULL) return ENOMEM; /* XXX leak */
    buf->bound = (buf->base) + bound_offset + inc;
    buf->next = (buf->base) + next_offset;
    return 0;
}

#undef asn1buf_len
int
asn1buf_len(const asn1buf *buf)
{
    return buf->next - buf->base;
}
