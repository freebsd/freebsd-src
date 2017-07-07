/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/generic/oid_ops.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* GSS-API V2 interfaces to manipulate OIDs */

#include "gssapiP_generic.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gssapi/gssapi_generic.h>
#include <errno.h>
#include <ctype.h>

/*
 * The functions for allocating and releasing individual OIDs use malloc and
 * free instead of the gssalloc wrappers, because the mechglue currently mixes
 * generic_gss_copy_oid() with hand-freeing of OIDs.  We do not need to free
 * free OIDs allocated by mechanisms, so this should not be a problem.
 */

OM_uint32
generic_gss_release_oid(OM_uint32 *minor_status, gss_OID *oid)
{
    if (minor_status)
        *minor_status = 0;

    if (oid == NULL || *oid == GSS_C_NO_OID)
        return(GSS_S_COMPLETE);

    /*
     * The V2 API says the following!
     *
     * gss_release_oid[()] will recognize any of the GSSAPI's own OID values,
     * and will silently ignore attempts to free these OIDs; for other OIDs
     * it will call the C free() routine for both the OID data and the
     * descriptor.  This allows applications to freely mix their own heap-
     * allocated OID values with OIDs returned by GSS-API.
     */

    /*
     * We use the official OID definitions instead of the unofficial OID
     * defintions. But we continue to support the unofficial OID
     * gss_nt_service_name just in case if some gss applications use
     * the old OID.
     */

    if ((*oid != GSS_C_NT_USER_NAME) &&
        (*oid != GSS_C_NT_MACHINE_UID_NAME) &&
        (*oid != GSS_C_NT_STRING_UID_NAME) &&
        (*oid != GSS_C_NT_HOSTBASED_SERVICE) &&
        (*oid != GSS_C_NT_ANONYMOUS) &&
        (*oid != GSS_C_NT_EXPORT_NAME) &&
        (*oid != GSS_C_NT_COMPOSITE_EXPORT) &&
        (*oid != gss_nt_service_name)) {
        free((*oid)->elements);
        free(*oid);
    }
    *oid = GSS_C_NO_OID;
    return(GSS_S_COMPLETE);
}

OM_uint32
generic_gss_copy_oid(OM_uint32 *minor_status,
                     const gss_OID_desc * const oid,
                     gss_OID *new_oid)
{
    gss_OID         p;

    *minor_status = 0;

    p = (gss_OID) malloc(sizeof(gss_OID_desc));
    if (!p) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }
    p->length = oid->length;
    p->elements = malloc(p->length);
    if (!p->elements) {
        free(p);
        return GSS_S_FAILURE;
    }
    memcpy(p->elements, oid->elements, p->length);
    *new_oid = p;
    return(GSS_S_COMPLETE);
}


OM_uint32
generic_gss_create_empty_oid_set(OM_uint32 *minor_status, gss_OID_set *oid_set)
{
    *minor_status = 0;

    if (oid_set == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    if ((*oid_set = (gss_OID_set) gssalloc_malloc(sizeof(gss_OID_set_desc)))) {
        memset(*oid_set, 0, sizeof(gss_OID_set_desc));
        return(GSS_S_COMPLETE);
    }
    else {
        *minor_status = ENOMEM;
        return(GSS_S_FAILURE);
    }
}

OM_uint32
generic_gss_add_oid_set_member(OM_uint32 *minor_status,
                               const gss_OID_desc * const member_oid,
                               gss_OID_set *oid_set)
{
    gss_OID     elist;
    gss_OID     lastel;

    *minor_status = 0;

    if (member_oid == NULL || member_oid->length == 0 ||
        member_oid->elements == NULL)
        return (GSS_S_CALL_INACCESSIBLE_READ);

    if (oid_set == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    elist = (*oid_set)->elements;
    /* Get an enlarged copy of the array */
    if (((*oid_set)->elements = (gss_OID) gssalloc_malloc(((*oid_set)->count+1) *
                                                          sizeof(gss_OID_desc)))) {
        /* Copy in the old junk */
        if (elist)
            memcpy((*oid_set)->elements,
                   elist,
                   ((*oid_set)->count * sizeof(gss_OID_desc)));

        /* Duplicate the input element */
        lastel = &(*oid_set)->elements[(*oid_set)->count];
        if ((lastel->elements =
             (void *) gssalloc_malloc((size_t) member_oid->length))) {
            /* Success - copy elements */
            memcpy(lastel->elements, member_oid->elements,
                   (size_t) member_oid->length);
            /* Set length */
            lastel->length = member_oid->length;

            /* Update count */
            (*oid_set)->count++;
            if (elist)
                gssalloc_free(elist);
            *minor_status = 0;
            return(GSS_S_COMPLETE);
        }
        else
            gssalloc_free((*oid_set)->elements);
    }
    /* Failure - restore old contents of list */
    (*oid_set)->elements = elist;
    *minor_status = ENOMEM;
    return(GSS_S_FAILURE);
}

OM_uint32
generic_gss_test_oid_set_member(OM_uint32 *minor_status,
                                const gss_OID_desc * const member,
                                gss_OID_set set,
                                int * present)
{
    OM_uint32   i;
    int         result;

    *minor_status = 0;

    if (member == NULL || set == NULL)
        return (GSS_S_CALL_INACCESSIBLE_READ);

    if (present == NULL)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    result = 0;
    for (i=0; i<set->count; i++) {
        if ((set->elements[i].length == member->length) &&
            !memcmp(set->elements[i].elements,
                    member->elements,
                    (size_t) member->length)) {
            result = 1;
            break;
        }
    }
    *present = result;
    return(GSS_S_COMPLETE);
}

OM_uint32
generic_gss_oid_to_str(OM_uint32 *minor_status,
                       const gss_OID_desc * const oid,
                       gss_buffer_t oid_str)
{
    unsigned long       number, n;
    OM_uint32           i;
    int                 first;
    unsigned char       *cp;
    struct k5buf        buf;

    if (minor_status != NULL)
        *minor_status = 0;

    if (oid_str != GSS_C_NO_BUFFER) {
        oid_str->length = 0;
        oid_str->value = NULL;
    }

    if (oid == NULL || oid->length == 0 || oid->elements == NULL)
        return (GSS_S_CALL_INACCESSIBLE_READ);

    if (oid_str == GSS_C_NO_BUFFER)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    /* Decoded according to krb5/gssapi_krb5.c */

    cp = (unsigned char *) oid->elements;
    number = (unsigned long) cp[0];
    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "{ ");
    number = 0;
    cp = (unsigned char *) oid->elements;
    first = 1;
    for (i = 0; i < oid->length; i++) {
        number = (number << 7) | (cp[i] & 0x7f);
        if ((cp[i] & 0x80) == 0) {
            if (first) {
                n = (number < 40) ? 0 : (number < 80) ? 1 : 2;
                k5_buf_add_fmt(&buf, "%lu %lu ", n, number - (n * 40));
                first = 0;
            } else {
                k5_buf_add_fmt(&buf, "%lu ", number);
            }
            number = 0;
        }
    }
    k5_buf_add_len(&buf, "}\0", 2);
    return k5buf_to_gss(minor_status, &buf, oid_str);
}

/* Return the length of a DER OID subidentifier encoding. */
static size_t
arc_encoded_length(unsigned long arc)
{
    size_t len = 1;

    for (arc >>= 7; arc; arc >>= 7)
        len++;
    return len;
}

/* Encode a subidentifier into *bufp and advance it to the encoding's end. */
static void
arc_encode(unsigned long arc, unsigned char **bufp)
{
    unsigned char *p;

    /* Advance to the end and encode backwards. */
    p = *bufp = *bufp + arc_encoded_length(arc);
    *--p = arc & 0x7f;
    for (arc >>= 7; arc; arc >>= 7)
        *--p = (arc & 0x7f) | 0x80;
}

/* Fetch an arc value from *bufp and advance past it and any following spaces
 * or periods.  Return 1 on success, 0 if *bufp is not at a valid arc value. */
static int
get_arc(const unsigned char **bufp, const unsigned char *end,
        unsigned long *arc_out)
{
    const unsigned char *p = *bufp;
    unsigned long arc = 0, newval;

    if (p == end || !isdigit(*p))
        return 0;
    for (; p < end && isdigit(*p); p++) {
        newval = arc * 10 + (*p - '0');
        if (newval < arc)
            return 0;
        arc = newval;
    }
    while (p < end && (isspace(*p) || *p == '.'))
        p++;
    *bufp = p;
    *arc_out = arc;
    return 1;
}

/*
 * Convert a sequence of two or more decimal arc values into a DER-encoded OID.
 * The values may be separated by any combination of whitespace and period
 * characters, and may be optionally surrounded with braces.  Leading
 * whitespace and trailing garbage is allowed.  The first arc value must be 0,
 * 1, or 2, and the second value must be less than 40 if the first value is not
 * 2.
 */
OM_uint32
generic_gss_str_to_oid(OM_uint32 *minor_status,
                       gss_buffer_t oid_str,
                       gss_OID *oid_out)
{
    const unsigned char *p, *end, *arc3_start;
    unsigned char *out;
    unsigned long arc, arc1, arc2;
    size_t nbytes;
    int brace = 0;
    gss_OID oid;

    if (minor_status != NULL)
        *minor_status = 0;

    if (oid_out != NULL)
        *oid_out = GSS_C_NO_OID;

    if (GSS_EMPTY_BUFFER(oid_str))
        return (GSS_S_CALL_INACCESSIBLE_READ);

    if (oid_out == NULL)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    /* Skip past initial spaces and, optionally, an open brace. */
    brace = 0;
    p = oid_str->value;
    end = p + oid_str->length;
    while (p < end && isspace(*p))
        p++;
    if (p < end && *p == '{') {
        brace = 1;
        p++;
    }
    while (p < end && isspace(*p))
        p++;

    /* Get the first two arc values, to be encoded as one subidentifier. */
    if (!get_arc(&p, end, &arc1) || !get_arc(&p, end, &arc2))
        return (GSS_S_FAILURE);
    if (arc1 > 2 || (arc1 < 2 && arc2 > 39) || arc2 > ULONG_MAX - 80)
        return (GSS_S_FAILURE);
    arc3_start = p;

    /* Compute the total length of the encoding while checking syntax. */
    nbytes = arc_encoded_length(arc1 * 40 + arc2);
    while (get_arc(&p, end, &arc))
        nbytes += arc_encoded_length(arc);
    if (brace && (p == end || *p != '}'))
        return (GSS_S_FAILURE);

    /* Allocate an oid structure. */
    oid = malloc(sizeof(*oid));
    if (oid == NULL)
        return (GSS_S_FAILURE);
    oid->elements = malloc(nbytes);
    if (oid->elements == NULL) {
        free(oid);
        return (GSS_S_FAILURE);
    }
    oid->length = nbytes;

    out = oid->elements;
    arc_encode(arc1 * 40 + arc2, &out);
    p = arc3_start;
    while (get_arc(&p, end, &arc))
        arc_encode(arc, &out);
    assert(out - nbytes == oid->elements);
    *oid_out = oid;
    return(GSS_S_COMPLETE);
}

/* Compose an OID of a prefix and an integer suffix */
OM_uint32
generic_gss_oid_compose(OM_uint32 *minor_status,
                        const char *prefix,
                        size_t prefix_len,
                        int suffix,
                        gss_OID_desc *oid)
{
    int osuffix, i;
    size_t nbytes;
    unsigned char *op;

    if (oid == GSS_C_NO_OID) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }
    if (oid->length < prefix_len) {
        *minor_status = ERANGE;
        return GSS_S_FAILURE;
    }

    memcpy(oid->elements, prefix, prefix_len);

    nbytes = 0;
    osuffix = suffix;
    while (suffix) {
        nbytes++;
        suffix >>= 7;
    }
    suffix = osuffix;

    if (oid->length < prefix_len + nbytes) {
        *minor_status = ERANGE;
        return GSS_S_FAILURE;
    }

    op = (unsigned char *) oid->elements + prefix_len + nbytes;
    i = -1;
    while (suffix) {
        op[i] = (unsigned char)suffix & 0x7f;
        if (i != -1)
            op[i] |= 0x80;
        i--;
        suffix >>= 7;
    }

    oid->length = prefix_len + nbytes;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
generic_gss_oid_decompose(OM_uint32 *minor_status,
                          const char *prefix,
                          size_t prefix_len,
                          gss_OID_desc *oid,
                          int *suffix)
{
    size_t i, slen;
    unsigned char *op;

    if (oid->length < prefix_len ||
        memcmp(oid->elements, prefix, prefix_len) != 0) {
        return GSS_S_BAD_MECH;
    }

    op = (unsigned char *) oid->elements + prefix_len;

    *suffix = 0;

    slen = oid->length - prefix_len;

    for (i = 0; i < slen; i++) {
        *suffix = (*suffix << 7) | (op[i] & 0x7f);
        if (i + 1 != slen && (op[i] & 0x80) == 0) {
            *minor_status = EINVAL;
            return GSS_S_FAILURE;
        }
    }

    return GSS_S_COMPLETE;
}

OM_uint32
generic_gss_copy_oid_set(OM_uint32 *minor_status,
                         const gss_OID_set_desc * const oidset,
                         gss_OID_set *new_oidset)
{
    gss_OID_set_desc *copy;
    OM_uint32 minor = 0;
    OM_uint32 major = GSS_S_COMPLETE;
    OM_uint32 i;

    if (minor_status != NULL)
        *minor_status = 0;

    if (new_oidset != NULL)
        *new_oidset = GSS_C_NO_OID_SET;

    if (oidset == GSS_C_NO_OID_SET)
        return (GSS_S_CALL_INACCESSIBLE_READ);

    if (new_oidset == NULL)
        return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if ((copy = (gss_OID_set_desc *) gssalloc_calloc(1, sizeof (*copy))) == NULL) {
        major = GSS_S_FAILURE;
        goto done;
    }

    if ((copy->elements = (gss_OID_desc *)
         gssalloc_calloc(oidset->count, sizeof (*copy->elements))) == NULL) {
        major = GSS_S_FAILURE;
        goto done;
    }
    copy->count = oidset->count;

    for (i = 0; i < copy->count; i++) {
        gss_OID_desc *out = &copy->elements[i];
        gss_OID_desc *in = &oidset->elements[i];

        if ((out->elements = (void *) gssalloc_malloc(in->length)) == NULL) {
            major = GSS_S_FAILURE;
            goto done;
        }
        (void) memcpy(out->elements, in->elements, in->length);
        out->length = in->length;
    }

    *new_oidset = copy;
done:
    if (major != GSS_S_COMPLETE) {
        (void) generic_gss_release_oid_set(&minor, &copy);
    }

    return (major);
}
