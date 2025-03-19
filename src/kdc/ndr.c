/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/ndr.c - NDR encoding and decoding functions */
/*
 * Copyright (C) 2021 by Red Hat, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "k5-input.h"
#include "k5-buf.h"
#include "k5-utf8.h"
#include "kdc_util.h"

struct encoded_wchars {
    uint16_t bytes_len;
    uint16_t num_wchars;
    uint8_t *encoded;
};

/*
 * MS-DTYP 2.3.10:
 *
 * typedef struct _RPC_UNICODE_STRING {
 *     unsigned short Length;
 *     unsigned short MaximumLength;
 *     [size_is(MaximumLength/2), length_is(Length/2)] WCHAR* Buffer;
 * } RPC_UNICODE_STRING, *PRPC_UNICODE_STRING;
 *
 * Note that Buffer is not a String - there's no termination.
 *
 * We don't actually decode Length and MaximumLength here - this is a
 * conformant-varying array, which means that (per DCE-1.1-RPC 14.3.7.2) where
 * those actually appear in the serialized data is variable depending on
 * whether the string is at top level of the struct or not.  (This also
 * affects where the pointer identifier appears.)
 *
 * See MS-RPCE 4.7 for what an RPC_UNICODE_STRING looks like when not at
 * top-level.
 */
static krb5_error_code
dec_wchar_pointer(struct k5input *in, char **out)
{
    const uint8_t *bytes;
    uint32_t actual_count;

    /* Maximum count. */
    (void)k5_input_get_uint32_le(in);
    /* Offset - all zeroes, "should" not be checked. */
    (void)k5_input_get_uint32_le(in);

    actual_count = k5_input_get_uint32_le(in);
    if (actual_count > UINT32_MAX / 2)
        return ERANGE;

    bytes = k5_input_get_bytes(in, actual_count * 2);
    if (bytes == NULL || k5_utf16le_to_utf8(bytes, actual_count * 2, out) != 0)
        return EINVAL;

    /* Always align on 4. */
    if (actual_count % 2 == 1)
        (void)k5_input_get_uint16_le(in);

    return 0;
}

static krb5_error_code
enc_wchar_pointer(const char *utf8, struct encoded_wchars *encoded_out)
{
    krb5_error_code ret;
    struct k5buf b;
    size_t utf16len, num_wchars;
    uint8_t *utf16;

    ret = k5_utf8_to_utf16le(utf8, &utf16, &utf16len);
    if (ret)
        return ret;

    num_wchars = utf16len / 2;

    k5_buf_init_dynamic(&b);
    k5_buf_add_uint32_le(&b, num_wchars + 1);
    k5_buf_add_uint32_le(&b, 0);
    k5_buf_add_uint32_le(&b, num_wchars);
    k5_buf_add_len(&b, utf16, utf16len);

    free(utf16);

    if (num_wchars % 2 == 1)
        k5_buf_add_uint16_le(&b, 0);

    ret = k5_buf_status(&b);
    if (ret)
        return ret;

    encoded_out->bytes_len = b.len;
    encoded_out->num_wchars = num_wchars;
    encoded_out->encoded = b.data;
    return 0;
}

/*
 * Decode a delegation info structure, leaving room to add an additional
 * service.
 *
 * MS-PAC 2.9:
 *
 * typedef struct _S4U_DELEGATION_INFO {
 *     RPC_UNICODE_STRING S4U2proxyTarget;
 *     ULONG TransitedListSize;
 *     [size_is(TransitedListSize)] PRPC_UNICODE_STRING S4UTransitedServices;
 * } S4U_DELEGATION_INFO, *PS4U_DELEGATION_INFO;
 */
krb5_error_code
ndr_dec_delegation_info(krb5_data *data, struct pac_s4u_delegation_info **out)
{
    krb5_error_code ret;
    struct pac_s4u_delegation_info *di = NULL;
    struct k5input in;
    uint32_t i, object_buffer_length, nservices;
    uint8_t version, endianness, common_header_length;

    *out = NULL;

    di = k5alloc(sizeof(*di), &ret);
    if (di == NULL)
        return ret;

    k5_input_init(&in, data->data, data->length);

    /* Common Type Header - MS-RPCE 2.2.6.1 */
    version = k5_input_get_byte(&in);
    endianness = k5_input_get_byte(&in);
    common_header_length = k5_input_get_uint16_le(&in);
    (void)k5_input_get_uint32_le(&in); /* Filler - 0xcccccccc. */
    if (version != 1 || endianness != 0x10 || common_header_length != 8) {
        ret = EINVAL;
        goto error;
    }

    /* Private Header for Constructed Type - MS-RPCE 2.2.6.2 */
    object_buffer_length = k5_input_get_uint32_le(&in);
    if (data->length < 16 || object_buffer_length != data->length - 16) {
        ret = EINVAL;
        goto error;
    }

    (void)k5_input_get_uint32_le(&in); /* Filler - 0. */

    /* This code doesn't handle re-used pointers, which could come into play in
     * the unlikely case of a delegation loop. */

    /* Pointer.  Microsoft always starts at 00 00 02 00 */
    (void)k5_input_get_uint32_le(&in);
    /* Length of proxy target - 2 */
    (void)k5_input_get_uint16_le(&in);
    /* Length of proxy target */
    (void)k5_input_get_uint16_le(&in);
    /* Another pointer - 04 00 02 00.  Microsoft increments by 4 (le). */
    (void)k5_input_get_uint32_le(&in);

    /* Transited services length - header version. */
    (void)k5_input_get_uint32_le(&in);

    /* More pointer: 08 00 02 00 */
    (void)k5_input_get_uint32_le(&in);

    ret = dec_wchar_pointer(&in, &di->proxy_target);
    if (ret)
        goto error;
    nservices = k5_input_get_uint32_le(&in);

    /* Here, we have encoded 2 bytes of length, 2 bytes of (length + 2), and 4
     * bytes of pointer, for each element (deferred pointers). */
    if (nservices > data->length / 8) {
        ret = ERANGE;
        goto error;
    }
    (void)k5_input_get_bytes(&in, 8 * nservices);

    /* Since we're likely to add another entry, leave a blank at the end. */
    di->transited_services = k5calloc(nservices + 1, sizeof(char *), &ret);
    if (di->transited_services == NULL)
        goto error;

    for (i = 0; i < nservices; i++) {
        ret = dec_wchar_pointer(&in, &di->transited_services[i]);
        if (ret)
            goto error;
        di->transited_services_length++;
    }

    ret = in.status;
    if (ret)
        goto error;

    *out = di;
    return 0;

error:
    ndr_free_delegation_info(di);
    return ret;
}

/* Empirically, Microsoft starts pointers at 00 00 02 00, and if treated little
 * endian, they increase by 4. */
static inline void
write_ptr(struct k5buf *buf, uint32_t *pointer)
{
    if (*pointer == 0)
        *pointer = 0x00020000;
    k5_buf_add_uint32_le(buf, *pointer);
    *pointer += 4;
}

krb5_error_code
ndr_enc_delegation_info(struct pac_s4u_delegation_info *in, krb5_data *out)
{
    krb5_error_code ret;
    size_t i;
    struct k5buf b;
    struct encoded_wchars pt_encoded = { 0 }, *tss_encoded = NULL;
    uint32_t pointer = 0;

    /* Encode ahead of time since we need the lengths. */
    ret = enc_wchar_pointer(in->proxy_target, &pt_encoded);
    if (ret)
        goto cleanup;

    tss_encoded = k5calloc(in->transited_services_length, sizeof(*tss_encoded),
                           &ret);
    if (tss_encoded == NULL)
        goto cleanup;

    k5_buf_init_dynamic(&b);

    /* Common Type Header - MS-RPCE 2.2.6.1 */
    k5_buf_add_len(&b, "\x01\x10\x08\x00", 4);
    k5_buf_add_uint32_le(&b, 0xcccccccc);

    /* Private Header for Constructed Type - MS-RPCE 2.2.6.2 */
    k5_buf_add_uint32_le(&b, 0); /* Skip over where payload length goes. */
    k5_buf_add_uint32_le(&b, 0); /* Filler - all zeroes. */

    write_ptr(&b, &pointer);
    k5_buf_add_uint16_le(&b, 2 * pt_encoded.num_wchars);
    k5_buf_add_uint16_le(&b, 2 * (pt_encoded.num_wchars + 1));
    write_ptr(&b, &pointer);

    k5_buf_add_uint32_le(&b, in->transited_services_length);
    write_ptr(&b, &pointer);

    k5_buf_add_len(&b, pt_encoded.encoded, pt_encoded.bytes_len);

    k5_buf_add_uint32_le(&b, in->transited_services_length);

    /* Deferred pointers. */
    for (i = 0; i < in->transited_services_length; i++) {
        ret = enc_wchar_pointer(in->transited_services[i], &tss_encoded[i]);
        if (ret)
            goto cleanup;

        k5_buf_add_uint16_le(&b, 2 * tss_encoded[i].num_wchars);
        k5_buf_add_uint16_le(&b, 2 * (tss_encoded[i].num_wchars + 1));
        write_ptr(&b, &pointer);
    }

    for (i = 0; i < in->transited_services_length; i++)
        k5_buf_add_len(&b, tss_encoded[i].encoded, tss_encoded[i].bytes_len);

    /* Now, pad to 8 bytes.  RPC_UNICODE_STRING is aligned on 4 bytes. */
    if (b.len % 8 != 0)
        k5_buf_add_uint32_le(&b, 0);

    /* Record the payload length where we skipped over it previously. */
    if (b.data != NULL)
        store_32_le(b.len - 0x10, ((uint8_t *)b.data) + 8);

    ret = k5_buf_status(&b);
    if (ret)
        goto cleanup;

    *out = make_data(b.data, b.len);
    b.data = NULL;

cleanup:
    free(b.data);
    free(pt_encoded.encoded);
    for (i = 0; tss_encoded != NULL && i < in->transited_services_length; i++)
        free(tss_encoded[i].encoded);
    free(tss_encoded);
    return ret;
}

void
ndr_free_delegation_info(struct pac_s4u_delegation_info *di)
{
    uint32_t i;

    if (di == NULL)
        return;
    free(di->proxy_target);
    for (i = 0; i < di->transited_services_length; i++)
        free(di->transited_services[i]);
    free(di->transited_services);
    free(di);
}
