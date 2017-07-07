/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* Coding Buffer Specifications */
#ifndef __ASN1BUF_H__
#define __ASN1BUF_H__

#include "k5-int.h"
#include "krbasn1.h"

typedef struct code_buffer_rep {
    char *base, *bound, *next;
} asn1buf;


/**************** Private Procedures ****************/

#if (__GNUC__ >= 2) && !defined(CONFIG_SMALL)
unsigned int asn1buf_free(const asn1buf *buf);
/*
 * requires  *buf is allocated
 * effects   Returns the number of unused, allocated octets in *buf.
 */
#define asn1buf_free(buf)                               \
    (((buf) == NULL || (buf)->base == NULL)             \
     ? 0U                                               \
     : (unsigned int)((buf)->bound - (buf)->next + 1))


asn1_error_code asn1buf_ensure_space(asn1buf *buf, const unsigned int amount);
/*
 * requires  *buf is allocated
 * modifies  *buf
 * effects  If buf has less than amount octets of free space, then it is
 *          expanded to have at least amount octets of free space.
 *          Returns ENOMEM memory is exhausted.
 */
#define asn1buf_ensure_space(buf,amount)                        \
    ((asn1buf_free(buf) < (amount))                             \
     ? (asn1buf_expand((buf), (amount)-asn1buf_free(buf)))      \
     : 0)

asn1_error_code asn1buf_expand(asn1buf *buf, unsigned int inc);
/*
 * requires  *buf is allocated
 * modifies  *buf
 * effects   Expands *buf by allocating space for inc more octets.
 *            Returns ENOMEM if memory is exhausted.
 */
#endif

int asn1buf_len(const asn1buf *buf);
/*
 * requires  *buf is allocated
 * effects   Returns the length of the encoding in *buf.
 */
#define asn1buf_len(buf)        ((buf)->next - (buf)->base)

/****** End of private procedures *****/

/*
 * Overview
 *
 *  The coding buffer is an array of char (to match a krb5_data structure)
 *   with 3 reference pointers:
 *   1) base - The bottom of the octet array.  Used for memory management
 *             operations on the array (e.g. alloc, realloc, free).
 *   2) next - Points to the next available octet position in the array.
 *             During encoding, this is the next free position, and it
 *               advances as octets are added to the array.
 *             During decoding, this is the next unread position, and it
 *               advances as octets are read from the array.
 *   3) bound - Points to the top of the array. Used for bounds-checking.
 *
 *  All pointers to encoding buffers should be initalized to NULL.
 *
 * Operations
 *
 *  asn1buf_create
 *  asn1buf_wrap_data
 *  asn1buf_destroy
 *  asn1buf_insert_octet
 *  asn1buf_insert_charstring
 *  asn1buf_remove_octet
 *  asn1buf_remove_charstring
 *  asn1buf_unparse
 *  asn1buf_hex_unparse
 *  asn12krb5_buf
 *  asn1buf_remains
 *
 *  (asn1buf_size)
 *  (asn1buf_free)
 *  (asn1buf_ensure_space)
 *  (asn1buf_expand)
 *  (asn1buf_len)
 */

asn1_error_code asn1buf_create(asn1buf **buf);
/*
 * effects   Creates a new encoding buffer pointed to by *buf.
 *           Returns ENOMEM if the buffer can't be created.
 */

void asn1buf_destroy(asn1buf **buf);
/* effects   Deallocates **buf, sets *buf to NULL. */

/*
 * requires  *buf is allocated
 * effects   Inserts o into the buffer *buf, expanding the buffer if
 *           necessary.  Returns ENOMEM memory is exhausted.
 */
#if ((__GNUC__ >= 2) && !defined(ASN1BUF_OMIT_INLINE_FUNCS)) && !defined(CONFIG_SMALL)
static inline asn1_error_code
asn1buf_insert_octet(asn1buf *buf, const int o)
{
    asn1_error_code retval;

    retval = asn1buf_ensure_space(buf,1U);
    if (retval) return retval;
    *(buf->next) = (char)o;
    (buf->next)++;
    return 0;
}
#else
asn1_error_code asn1buf_insert_octet(asn1buf *buf, const int o);
#endif

asn1_error_code
asn1buf_insert_bytestring(
    asn1buf *buf,
    const unsigned int len,
    const void *s);
/*
 * requires  *buf is allocated
 * modifies  *buf
 * effects   Inserts the contents of s (an array of length len)
 *            into the buffer *buf, expanding the buffer if necessary.
 *           Returns ENOMEM if memory is exhausted.
 */

#define asn1buf_insert_octetstring asn1buf_insert_bytestring

asn1_error_code asn12krb5_buf(const asn1buf *buf, krb5_data **code);
/*
 * modifies  *code
 * effects   Instantiates **code with the krb5_data representation of **buf.
 */

#endif
