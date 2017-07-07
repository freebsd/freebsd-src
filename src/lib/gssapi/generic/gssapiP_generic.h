/* -*- mode: c; indent-tabs-mode: nil -*- */
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

#ifndef _GSSAPIP_GENERIC_H_
#define _GSSAPIP_GENERIC_H_

/*
 * $Id$
 */

#if defined(_WIN32)
#include "k5-int.h"
#else
#include "autoconf.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#endif

#include "k5-thread.h"

#include "gssapi_generic.h"
#include "gssapi_ext.h"
#include <gssapi/gssapi_alloc.h>
#include "gssapi_err_generic.h"
#include <errno.h>

#include "k5-platform.h"
#include "k5-buf.h"

/** helper macros **/

#define g_OID_equal(o1, o2)                                             \
        (((o1)->length == (o2)->length) &&                              \
        (memcmp((o1)->elements, (o2)->elements, (o1)->length) == 0))

/* this code knows that an int on the wire is 32 bits.  The type of
   num should be at least this big, or the extra shifts may do weird
   things */

#define TWRITE_INT(ptr, num, bigend)                                    \
   if (bigend) store_32_be(num, ptr); else store_32_le(num, ptr);       \
   (ptr) += 4;

#define TWRITE_INT16(ptr, num, bigend)                                  \
   if (bigend) store_16_be((num)>>16, ptr); else store_16_le(num, ptr); \
   (ptr) += 2;

#define TREAD_INT(ptr, num, bigend)                        \
   (num) = ((bigend) ? load_32_be(ptr) : load_32_le(ptr)); \
   (ptr) += 4;

#define TREAD_INT16(ptr, num, bigend)                              \
   (num) = ((bigend) ? (load_16_be(ptr) << 16) : load_16_le(ptr)); \
   (ptr) += 2;

#define TWRITE_STR(ptr, str, len)               \
   memcpy((ptr), (str), (len));                 \
   (ptr) += (len);

#define TREAD_STR(ptr, str, len)                \
   (str) = (ptr);                               \
   (ptr) += (len);

#define TWRITE_BUF(ptr, buf, bigend)                    \
   TWRITE_INT((ptr), (buf).length, (bigend));           \
   TWRITE_STR((ptr), (buf).value, (buf).length);

/** malloc wrappers; these may actually do something later */

#define xmalloc(n) malloc(n)
#define xrealloc(p,n) realloc(p,n)
#ifdef xfree
#undef xfree
#endif
#define xfree(p) free(p)

/** helper functions **/

/* hide names from applications, especially glib applications */
#define g_set_init              gssint_g_set_init
#define g_set_destroy           gssint_g_set_destroy
#define g_set_entry_add         gssint_g_set_entry_add
#define g_set_entry_delete      gssint_g_set_entry_delete
#define g_set_entry_get         gssint_g_set_entry_get
#define g_make_string_buffer    gssint_g_make_string_buffer
#define g_token_size            gssint_g_token_size
#define g_make_token_header     gssint_g_make_token_header
#define g_verify_token_header   gssint_g_verify_token_header
#define g_display_major_status  gssint_g_display_major_status
#define g_display_com_err_status gssint_g_display_com_err_status
#define g_seqstate_init         gssint_g_seqstate_init
#define g_seqstate_check        gssint_g_seqstate_check
#define g_seqstate_free         gssint_g_seqstate_free
#define g_seqstate_size         gssint_g_seqstate_size
#define g_seqstate_externalize  gssint_g_seqstate_externalize
#define g_seqstate_internalize  gssint_g_seqstate_internalize
#define g_canonicalize_host     gssint_g_canonicalize_host
#define g_local_host_name       gssint_g_local_host_name
#define g_strdup                gssint_g_strdup

typedef struct _g_set_elt *g_set_elt;
typedef struct {
    k5_mutex_t mutex;
    void *data;
} g_set;
#define G_SET_INIT { K5_MUTEX_PARTIAL_INITIALIZER, 0 }

typedef struct g_seqnum_state_st *g_seqnum_state;

int g_set_init (g_set_elt *s);
int g_set_destroy (g_set_elt *s);
int g_set_entry_add (g_set_elt *s, void *key, void *value);
int g_set_entry_delete (g_set_elt *s, void *key);
int g_set_entry_get (g_set_elt *s, void *key, void **value);

int g_save_name (g_set *vdb, gss_name_t name);
int g_save_cred_id (g_set *vdb, gss_cred_id_t cred);
int g_save_ctx_id (g_set *vdb, gss_ctx_id_t ctx);
int g_save_lucidctx_id (g_set *vdb, void *lctx);

int g_validate_name (g_set *vdb, gss_name_t name);
int g_validate_cred_id (g_set *vdb, gss_cred_id_t cred);
int g_validate_ctx_id (g_set *vdb, gss_ctx_id_t ctx);
int g_validate_lucidctx_id (g_set *vdb, void *lctx);

int g_delete_name (g_set *vdb, gss_name_t name);
int g_delete_cred_id (g_set *vdb, gss_cred_id_t cred);
int g_delete_ctx_id (g_set *vdb, gss_ctx_id_t ctx);
int g_delete_lucidctx_id (g_set *vdb, void *lctx);

int g_make_string_buffer (const char *str, gss_buffer_t buffer);

unsigned int g_token_size (const gss_OID_desc * mech, unsigned int body_size);

void g_make_token_header (const gss_OID_desc * mech, unsigned int body_size,
                          unsigned char **buf, int tok_type);

/* flags for g_verify_token_header() */
#define G_VFY_TOKEN_HDR_WRAPPER_REQUIRED        0x01

gss_int32 g_verify_token_header (const gss_OID_desc * mech,
                                 unsigned int *body_size,
                                 unsigned char **buf, int tok_type,
                                 unsigned int toksize_in,
                                 int flags);

OM_uint32 g_display_major_status (OM_uint32 *minor_status,
                                  OM_uint32 status_value,
                                  OM_uint32 *message_context,
                                  gss_buffer_t status_string);

OM_uint32 g_display_com_err_status (OM_uint32 *minor_status,
                                    OM_uint32 status_value,
                                    gss_buffer_t status_string);

long g_seqstate_init(g_seqnum_state *state_out, uint64_t seqnum,
                     int do_replay, int do_sequence, int wide);
OM_uint32 g_seqstate_check(g_seqnum_state state, uint64_t seqnum);
void g_seqstate_free(g_seqnum_state state);
void g_seqstate_size(g_seqnum_state state, size_t *sizep);
long g_seqstate_externalize(g_seqnum_state state, unsigned char **buf,
                            size_t *lenremain);
long g_seqstate_internalize(g_seqnum_state *state_out, unsigned char **buf,
                            size_t *lenremain);

char *g_strdup (char *str);

/** declarations of internal name mechanism functions **/

OM_uint32
generic_gss_release_buffer(
    OM_uint32 *,        /* minor_status */
    gss_buffer_t);      /* buffer */

OM_uint32
generic_gss_release_oid_set(
    OM_uint32 *,        /* minor_status */
    gss_OID_set *);     /* set */

OM_uint32
generic_gss_release_oid(
    OM_uint32 *,        /* minor_status */
    gss_OID *);         /* set */

OM_uint32
generic_gss_copy_oid(
    OM_uint32 *,                /* minor_status */
    const gss_OID_desc * const, /* oid */
    gss_OID *);                 /* new_oid */

OM_uint32
generic_gss_create_empty_oid_set(
    OM_uint32 *,        /* minor_status */
    gss_OID_set *);     /* oid_set */

OM_uint32
generic_gss_add_oid_set_member(
    OM_uint32 *,                /* minor_status */
    const gss_OID_desc * const, /* member_oid */
    gss_OID_set *);             /* oid_set */

OM_uint32
generic_gss_test_oid_set_member(
    OM_uint32 *,                /* minor_status */
    const gss_OID_desc * const, /* member */
    gss_OID_set,                /* set */
    int *);                     /* present */

OM_uint32
generic_gss_oid_to_str(
    OM_uint32 *,                /* minor_status */
    const gss_OID_desc * const, /* oid */
    gss_buffer_t);              /* oid_str */

OM_uint32
generic_gss_str_to_oid(
    OM_uint32 *,        /* minor_status */
    gss_buffer_t,       /* oid_str */
    gss_OID *);         /* oid */

OM_uint32
generic_gss_oid_compose(
    OM_uint32 *,        /* minor_status */
    const char *,       /* prefix */
    size_t,             /* prefix_len */
    int,                /* suffix */
    gss_OID_desc *);    /* oid */

OM_uint32
generic_gss_oid_decompose(
    OM_uint32 *,        /* minor_status */
    const char *,       /*prefix */
    size_t,             /* prefix_len */
    gss_OID_desc *,     /* oid */
    int *);             /* suffix */

int gssint_mecherrmap_init(void);
void gssint_mecherrmap_destroy(void);
OM_uint32 gssint_mecherrmap_map(OM_uint32 minor, const gss_OID_desc *oid);
int gssint_mecherrmap_get(OM_uint32 minor, gss_OID mech_oid,
                          OM_uint32 *mech_minor);
OM_uint32 gssint_mecherrmap_map_errcode(OM_uint32 errcode);

/*
 * Transfer contents of a k5buf to a gss_buffer and invalidate the source
 * On unix, this is a simple pointer copy
 * On windows, memory is reallocated and copied.
 */
static inline OM_uint32
k5buf_to_gss(OM_uint32 *minor,
             struct k5buf *input_k5buf,
             gss_buffer_t output_buffer)
{
    OM_uint32 status = GSS_S_COMPLETE;

    if (k5_buf_status(input_k5buf) != 0) {
        *minor = ENOMEM;
        return GSS_S_FAILURE;
    }
    output_buffer->length = input_k5buf->len;
#if defined(_WIN32) || defined(DEBUG_GSSALLOC)
    if (output_buffer->length > 0) {
        output_buffer->value = gssalloc_malloc(output_buffer->length);
        if (output_buffer->value) {
            memcpy(output_buffer->value, input_k5buf->data,
                   output_buffer->length);
        } else {
            status = GSS_S_FAILURE;
            *minor = ENOMEM;
        }
    } else {
        output_buffer->value = NULL;
    }
    k5_buf_free(input_k5buf);
#else
    output_buffer->value = input_k5buf->data;
    memset(input_k5buf, 0, sizeof(*input_k5buf));
#endif
    return status;
}

OM_uint32 generic_gss_create_empty_buffer_set
(OM_uint32 * /*minor_status*/,
            gss_buffer_set_t * /*buffer_set*/);

OM_uint32 generic_gss_add_buffer_set_member
(OM_uint32 * /*minor_status*/,
            const gss_buffer_t /*member_buffer*/,
            gss_buffer_set_t * /*buffer_set*/);

OM_uint32 generic_gss_release_buffer_set
(OM_uint32 * /*minor_status*/,
            gss_buffer_set_t * /*buffer_set*/);

OM_uint32 generic_gss_copy_oid_set
(OM_uint32 *, /* minor_status */
            const gss_OID_set_desc * const /*oidset*/,
            gss_OID_set * /*new_oidset*/);

extern gss_OID_set gss_ma_known_attrs;

OM_uint32 generic_gss_display_mech_attr(
      OM_uint32         *minor_status,
      gss_const_OID      mech_attr,
      gss_buffer_t       name,
      gss_buffer_t       short_desc,
      gss_buffer_t       long_desc);

#endif /* _GSSAPIP_GENERIC_H_ */
