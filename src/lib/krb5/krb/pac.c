/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/pac.c */
/*
 * Copyright 2008 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"
#include "authdata.h"
#include "k5-input.h"

#define MAX_BUFFERS 4096

/*
 * Add a buffer containing data to pac's metadata and encoding.  If zerofill is
 * true, data->data must be NULL and the buffer will be zero-filled with length
 * data->length.
 */
krb5_error_code
k5_pac_add_buffer(krb5_context context, krb5_pac pac, uint32_t type,
                  const krb5_data *data, krb5_boolean zerofill,
                  krb5_data *data_out)
{
    struct k5_pac_buffer *nbufs;
    size_t header_len, i, pad = 0;
    char *ndata, *bufdata;

    assert((data->data == NULL) == zerofill);

    /* Check for an existing buffer of this type. */
    if (k5_pac_locate_buffer(context, pac, type, NULL) == 0)
        return EEXIST;

    if (pac->nbuffers >= MAX_BUFFERS)
        return ERANGE;
    nbufs = realloc(pac->buffers, (pac->nbuffers + 1) * sizeof(*pac->buffers));
    if (nbufs == NULL)
        return ENOMEM;
    pac->buffers = nbufs;

    header_len = PACTYPE_LENGTH + pac->nbuffers * PAC_INFO_BUFFER_LENGTH;

    if (data->length % PAC_ALIGNMENT)
        pad = PAC_ALIGNMENT - (data->length % PAC_ALIGNMENT);
    ndata = realloc(pac->data.data,
                    pac->data.length + PAC_INFO_BUFFER_LENGTH +
                    data->length + pad);
    if (ndata == NULL)
        return ENOMEM;
    pac->data.data = ndata;

    /* Update the offsets of existing buffers. */
    for (i = 0; i < pac->nbuffers; i++)
        pac->buffers[i].offset += PAC_INFO_BUFFER_LENGTH;

    /* Make room for the new buffer's metadata. */
    memmove(pac->data.data + header_len + PAC_INFO_BUFFER_LENGTH,
            pac->data.data + header_len,
            pac->data.length - header_len);
    memset(pac->data.data + header_len, 0, PAC_INFO_BUFFER_LENGTH);

    /* Initialize the new buffer. */
    pac->buffers[i].type = type;
    pac->buffers[i].size = data->length;
    pac->buffers[i].offset = pac->data.length + PAC_INFO_BUFFER_LENGTH;
    assert((pac->buffers[i].offset % PAC_ALIGNMENT) == 0);

    /* Copy in new PAC data and zero padding bytes. */
    bufdata = pac->data.data + pac->buffers[i].offset;
    if (zerofill)
        memset(bufdata, 0, data->length);
    else
        memcpy(bufdata, data->data, data->length);
    memset(bufdata + data->length, 0, pad);

    pac->nbuffers++;
    pac->data.length += PAC_INFO_BUFFER_LENGTH + data->length + pad;

    if (data_out != NULL)
        *data_out = make_data(bufdata, data->length);

    pac->verified = FALSE;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_pac_add_buffer(krb5_context context, krb5_pac pac, uint32_t type,
                    const krb5_data *data)
{
    return k5_pac_add_buffer(context, pac, type, data, FALSE, NULL);
}

/*
 * Free a PAC
 */
void KRB5_CALLCONV
krb5_pac_free(krb5_context context, krb5_pac pac)
{
    if (pac != NULL) {
        zapfree(pac->data.data, pac->data.length);
        free(pac->buffers);
        zapfree(pac, sizeof(*pac));
    }
}

krb5_error_code
k5_pac_locate_buffer(krb5_context context, const krb5_pac pac, uint32_t type,
                     krb5_data *data_out)
{
    struct k5_pac_buffer *buffer = NULL;
    size_t i;

    if (pac == NULL)
        return EINVAL;

    for (i = 0; i < pac->nbuffers; i++) {
        if (pac->buffers[i].type == type) {
            if (buffer == NULL)
                buffer = &pac->buffers[i];
            else
                return EINVAL;
        }
    }

    if (buffer == NULL)
        return ENOENT;

    assert(buffer->offset < pac->data.length);
    assert(buffer->size <= pac->data.length - buffer->offset);

    if (data_out != NULL)
        *data_out = make_data(pac->data.data + buffer->offset, buffer->size);

    return 0;
}

/*
 * Find a buffer and copy data into output
 */
krb5_error_code KRB5_CALLCONV
krb5_pac_get_buffer(krb5_context context, krb5_pac pac, uint32_t type,
                    krb5_data *data_out)
{
    krb5_data d;
    krb5_error_code ret;

    ret = k5_pac_locate_buffer(context, pac, type, &d);
    if (ret)
        return ret;

    data_out->data = k5memdup(d.data, d.length, &ret);
    if (data_out->data == NULL)
        return ret;
    data_out->length = d.length;
    return 0;
}

/*
 * Return an array of the types of data in the PAC
 */
krb5_error_code KRB5_CALLCONV
krb5_pac_get_types(krb5_context context, krb5_pac pac, size_t *len_out,
                   uint32_t **types_out)
{
    size_t i;

    *types_out = calloc(pac->nbuffers, sizeof(*types_out));
    if (*types_out == NULL)
        return ENOMEM;

    *len_out = pac->nbuffers;

    for (i = 0; i < pac->nbuffers; i++)
        (*types_out)[i] = pac->buffers[i].type;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_pac_init(krb5_context context, krb5_pac *pac_out)
{
    krb5_error_code ret;
    krb5_pac pac;

    *pac_out = NULL;

    pac = malloc(sizeof(*pac));
    if (pac == NULL)
        return ENOMEM;

    pac->nbuffers = 0;
    pac->buffers = NULL;
    pac->version = 0;

    pac->data.length = PACTYPE_LENGTH;
    pac->data.data = k5alloc(pac->data.length, &ret);
    if (pac->data.data == NULL) {
        free(pac);
        return ret;
    }

    pac->verified = FALSE;

    *pac_out = pac;
    return 0;
}

static krb5_error_code
copy_pac(krb5_context context, krb5_pac src, krb5_pac *dst_out)
{
    krb5_error_code ret;
    krb5_pac pac;

    *dst_out = NULL;

    pac = k5alloc(sizeof(*pac), &ret);
    if (pac == NULL)
        goto fail;

    pac->buffers = k5memdup(src->buffers,
                            src->nbuffers * sizeof(*pac->buffers), &ret);
    if (pac->buffers == NULL)
        goto fail;

    ret = krb5int_copy_data_contents(context, &src->data, &pac->data);
    if (ret)
        goto fail;

    pac->nbuffers = src->nbuffers;
    pac->version = src->version;
    pac->verified = src->verified;

    *dst_out = pac;
    return 0;

fail:
    krb5_pac_free(context, pac);
    return ret;
}

/* Parse the supplied data into an allocated PAC. */
krb5_error_code KRB5_CALLCONV
krb5_pac_parse(krb5_context context, const void *ptr, size_t len,
               krb5_pac *ppac)
{
    krb5_error_code ret;
    size_t i;
    krb5_pac pac;
    size_t header_len;
    uint32_t nbuffers, version;
    struct k5input in;
    char *ndata;

    *ppac = NULL;

    k5_input_init(&in, ptr, len);

    nbuffers = k5_input_get_uint32_le(&in);
    version = k5_input_get_uint32_le(&in);
    if (in.status || version != 0)
        return EINVAL;

    if (nbuffers < 1 || nbuffers > MAX_BUFFERS)
        return ERANGE;

    header_len = PACTYPE_LENGTH + (nbuffers * PAC_INFO_BUFFER_LENGTH);
    if (len < header_len)
        return ERANGE;

    ret = krb5_pac_init(context, &pac);
    if (ret)
        return ret;

    pac->buffers = k5calloc(nbuffers, sizeof(*pac->buffers), &ret);
    if (ret)
        goto fail;

    pac->nbuffers = nbuffers;
    pac->version = version;

    for (i = 0; i < nbuffers; i++) {
        struct k5_pac_buffer *buffer = &pac->buffers[i];

        buffer->type = k5_input_get_uint32_le(&in);
        buffer->size = k5_input_get_uint32_le(&in);
        buffer->offset = k5_input_get_uint64_le(&in);

        if (in.status || buffer->offset % PAC_ALIGNMENT) {
            ret = EINVAL;
            goto fail;
        }
        if (buffer->offset < header_len || buffer->offset > len ||
            buffer->size > len - buffer->offset) {
            ret = ERANGE;
            goto fail;
        }
    }

    ndata = realloc(pac->data.data, len);
    if (ndata == NULL) {
        krb5_pac_free(context, pac);
        return ENOMEM;
    }
    pac->data.data = ndata;
    memcpy(ndata, ptr, len);

    pac->data.length = len;

    *ppac = pac;

    return 0;

fail:
    krb5_pac_free(context, pac);
    return ret;
}

static krb5_error_code
k5_time_to_seconds_since_1970(uint64_t ntTime, krb5_timestamp *elapsedSeconds)
{
    uint64_t abstime = ntTime / 10000000 - NT_TIME_EPOCH;

    if (abstime > UINT32_MAX)
        return ERANGE;
    *elapsedSeconds = abstime;
    return 0;
}

krb5_error_code
k5_seconds_since_1970_to_time(krb5_timestamp elapsedSeconds, uint64_t *ntTime)
{
    *ntTime = (uint32_t)elapsedSeconds;
    *ntTime += NT_TIME_EPOCH;
    *ntTime *= 10000000;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_pac_get_client_info(krb5_context context, const krb5_pac pac,
                         krb5_timestamp *authtime_out, char **princname_out)
{
    krb5_error_code ret;
    krb5_data client_info;
    char *pac_princname;
    unsigned char *p;
    krb5_timestamp pac_authtime;
    uint16_t pac_princname_length;
    uint64_t pac_nt_authtime;

    if (authtime_out != NULL)
        *authtime_out = 0;
    *princname_out = NULL;

    ret = k5_pac_locate_buffer(context, pac, KRB5_PAC_CLIENT_INFO,
                               &client_info);
    if (ret)
        return ret;

    if (client_info.length < PAC_CLIENT_INFO_LENGTH)
        return ERANGE;

    p = (unsigned char *)client_info.data;
    pac_nt_authtime = load_64_le(p);
    p += 8;
    pac_princname_length = load_16_le(p);
    p += 2;

    ret = k5_time_to_seconds_since_1970(pac_nt_authtime, &pac_authtime);
    if (ret)
        return ret;

    if (client_info.length < PAC_CLIENT_INFO_LENGTH + pac_princname_length ||
        pac_princname_length % 2)
        return ERANGE;

    ret = k5_utf16le_to_utf8(p, pac_princname_length, &pac_princname);
    if (ret)
        return ret;

    if (authtime_out != NULL)
        *authtime_out = pac_authtime;
    *princname_out = pac_princname;

    return 0;
}

krb5_error_code
k5_pac_validate_client(krb5_context context, const krb5_pac pac,
                       krb5_timestamp authtime, krb5_const_principal principal,
                       krb5_boolean with_realm)
{
    krb5_error_code ret;
    char *pac_princname, *princname;
    krb5_timestamp pac_authtime;
    int flags = 0;

    ret = krb5_pac_get_client_info(context, pac, &pac_authtime,
                                   &pac_princname);
    if (ret)
        return ret;

    flags = KRB5_PRINCIPAL_UNPARSE_DISPLAY;
    if (!with_realm)
        flags |= KRB5_PRINCIPAL_UNPARSE_NO_REALM;

    ret = krb5_unparse_name_flags(context, principal, flags, &princname);
    if (ret) {
        free(pac_princname);
        return ret;
    }

    if (pac_authtime != authtime || strcmp(pac_princname, princname) != 0)
        ret = KRB5KRB_AP_WRONG_PRINC;

    free(pac_princname);
    krb5_free_unparsed_name(context, princname);

    return ret;
}

/* Zero out the signature in a copy of the PAC data. */
static krb5_error_code
zero_signature(krb5_context context, const krb5_pac pac, uint32_t type,
               krb5_data *data)
{
    struct k5_pac_buffer *buffer = NULL;
    size_t i;

    assert(type == KRB5_PAC_SERVER_CHECKSUM ||
           type == KRB5_PAC_PRIVSVR_CHECKSUM ||
           type == KRB5_PAC_FULL_CHECKSUM);
    assert(data->length >= pac->data.length);

    for (i = 0; i < pac->nbuffers; i++) {
        if (pac->buffers[i].type == type) {
            buffer = &pac->buffers[i];
            break;
        }
    }

    if (buffer == NULL)
        return ENOENT;

    if (buffer->size < PAC_SIGNATURE_DATA_LENGTH)
        return KRB5_BAD_MSIZE;
    if (buffer->size > pac->data.length ||
        buffer->offset > pac->data.length - buffer->size)
        return ERANGE;

    /* Within the copy, zero out the data portion of the checksum only. */
    memset(data->data + buffer->offset + PAC_SIGNATURE_DATA_LENGTH, 0,
           buffer->size - PAC_SIGNATURE_DATA_LENGTH);

    return 0;
}

static krb5_error_code
verify_checksum(krb5_context context, const krb5_pac pac, uint32_t buffer_type,
                const krb5_keyblock *key, krb5_keyusage usage,
                const krb5_data *data)
{
    krb5_error_code ret;
    krb5_data buffer;
    krb5_cksumtype cksumtype;
    krb5_checksum checksum;
    krb5_boolean valid;
    size_t cksumlen;

    ret = k5_pac_locate_buffer(context, pac, buffer_type, &buffer);
    if (ret)
        return ret;
    if (buffer.length < PAC_SIGNATURE_DATA_LENGTH)
        return KRB5_BAD_MSIZE;

    cksumtype = load_32_le(buffer.data);
    if (buffer_type == KRB5_PAC_SERVER_CHECKSUM && cksumtype == CKSUMTYPE_SHA1)
        return KRB5KDC_ERR_SUMTYPE_NOSUPP;
    if (!krb5_c_is_keyed_cksum(cksumtype))
        return KRB5KRB_ERR_GENERIC;

    /* There may be an RODCIdentifier trailer (see [MS-PAC] 2.8), so look up
     * the length of the checksum by its type. */
    ret = krb5_c_checksum_length(context, cksumtype, &cksumlen);
    if (ret)
        return ret;
    if (cksumlen > buffer.length - PAC_SIGNATURE_DATA_LENGTH)
        return KRB5_BAD_MSIZE;
    checksum.checksum_type = cksumtype;
    checksum.length = cksumlen;
    checksum.contents = (uint8_t *)buffer.data + PAC_SIGNATURE_DATA_LENGTH;

    ret = krb5_c_verify_checksum(context, key, usage, data, &checksum, &valid);
    return ret ? ret : (valid ? 0 : KRB5KRB_AP_ERR_MODIFIED);
}

static krb5_error_code
verify_pac_checksums(krb5_context context, const krb5_pac pac,
                     krb5_boolean expect_full_checksum,
                     const krb5_keyblock *server, const krb5_keyblock *privsvr)
{
    krb5_error_code ret;
    krb5_data copy, server_checksum;

    /* Make a copy of the PAC with zeroed out server and privsvr checksums. */
    ret = krb5int_copy_data_contents(context, &pac->data, &copy);
    if (ret)
        return ret;

    ret = zero_signature(context, pac, KRB5_PAC_SERVER_CHECKSUM, &copy);
    if (ret)
        goto cleanup;
    ret = zero_signature(context, pac, KRB5_PAC_PRIVSVR_CHECKSUM, &copy);
    if (ret)
        goto cleanup;

    if (server != NULL) {
        /* Verify the server checksum over the PAC copy. */
        ret = verify_checksum(context, pac, KRB5_PAC_SERVER_CHECKSUM, server,
                              KRB5_KEYUSAGE_APP_DATA_CKSUM, &copy);
    }

    if (privsvr != NULL && expect_full_checksum) {
        /* Zero the full checksum buffer in the copy and verify the full
         * checksum over the copy with all three checksums zeroed. */
        ret = zero_signature(context, pac, KRB5_PAC_FULL_CHECKSUM, &copy);
        if (ret)
            goto cleanup;
        ret = verify_checksum(context, pac, KRB5_PAC_FULL_CHECKSUM, privsvr,
                              KRB5_KEYUSAGE_APP_DATA_CKSUM, &copy);
        if (ret)
            goto cleanup;
    }

    if (privsvr != NULL) {
        /* Verify the privsvr checksum over the server checksum. */
        ret = k5_pac_locate_buffer(context, pac, KRB5_PAC_SERVER_CHECKSUM,
                                   &server_checksum);
        if (ret)
            return ret;
        if (server_checksum.length < PAC_SIGNATURE_DATA_LENGTH)
            return KRB5_BAD_MSIZE;
        server_checksum.data += PAC_SIGNATURE_DATA_LENGTH;
        server_checksum.length -= PAC_SIGNATURE_DATA_LENGTH;

        ret = verify_checksum(context, pac, KRB5_PAC_PRIVSVR_CHECKSUM, privsvr,
                              KRB5_KEYUSAGE_APP_DATA_CKSUM, &server_checksum);
        if (ret)
            goto cleanup;
    }

    pac->verified = TRUE;

cleanup:
    free(copy.data);
    return ret;
}

/* Per MS-PAC 2.8.3, tickets encrypted to TGS and password change principals
 * should not have ticket signatures. */
krb5_boolean
k5_pac_should_have_ticket_signature(krb5_const_principal sprinc)
{
    if (IS_TGS_PRINC(sprinc))
        return FALSE;
    if (sprinc->length == 2 && data_eq_string(sprinc->data[0], "kadmin") &&
        data_eq_string(sprinc->data[1], "changepw"))
        return FALSE;
    return TRUE;
}

krb5_error_code KRB5_CALLCONV
krb5_kdc_verify_ticket(krb5_context context, const krb5_enc_tkt_part *enc_tkt,
                       krb5_const_principal server_princ,
                       const krb5_keyblock *server,
                       const krb5_keyblock *privsvr, krb5_pac *pac_out)
{
    krb5_error_code ret;
    krb5_pac pac = NULL;
    krb5_data *recoded_tkt = NULL;
    krb5_authdata **authdata = enc_tkt->authorization_data;
    krb5_authdata *orig, **ifrel = NULL, **recoded_ifrel = NULL;
    uint8_t z = 0;
    krb5_authdata zpac = { KV5M_AUTHDATA, KRB5_AUTHDATA_WIN2K_PAC, 1, &z };
    krb5_boolean is_service_tkt;
    size_t i, j;

    *pac_out = NULL;

    if (authdata == NULL)
        return 0;

    /*
     * Find the position of the PAC in the ticket authdata.  ifrel will be the
     * decoded AD-IF-RELEVANT container at position i containing a PAC, and j
     * will be the offset within the container.
     */
    for (i = 0; authdata[i] != NULL; i++) {
        if (authdata[i]->ad_type != KRB5_AUTHDATA_IF_RELEVANT)
            continue;

        ret = krb5_decode_authdata_container(context,
                                             KRB5_AUTHDATA_IF_RELEVANT,
                                             authdata[i], &ifrel);
        if (ret)
            goto cleanup;

        for (j = 0; ifrel[j] != NULL; j++) {
            if (ifrel[j]->ad_type == KRB5_AUTHDATA_WIN2K_PAC)
                break;
        }
        if (ifrel[j] != NULL)
            break;

        krb5_free_authdata(context, ifrel);
        ifrel = NULL;
    }

    /* Stop and return successfully if we didn't find a PAC. */
    if (authdata[i] == NULL) {
        ret = 0;
        goto cleanup;
    }

    ret = krb5_pac_parse(context, ifrel[j]->contents, ifrel[j]->length, &pac);
    if (ret)
        goto cleanup;

    is_service_tkt = k5_pac_should_have_ticket_signature(server_princ);
    if (privsvr != NULL && is_service_tkt) {
        /* To check the PAC ticket signatures, re-encode the ticket with the
         * PAC contents replaced by a single zero. */
        orig = ifrel[j];
        ifrel[j] = &zpac;
        ret = krb5_encode_authdata_container(context,
                                             KRB5_AUTHDATA_IF_RELEVANT,
                                             ifrel, &recoded_ifrel);
        ifrel[j] = orig;
        if (ret)
            goto cleanup;
        orig = authdata[i];
        authdata[i] = recoded_ifrel[0];
        ret = encode_krb5_enc_tkt_part(enc_tkt, &recoded_tkt);
        authdata[i] = orig;
        if (ret)
            goto cleanup;

        ret = verify_checksum(context, pac, KRB5_PAC_TICKET_CHECKSUM, privsvr,
                              KRB5_KEYUSAGE_APP_DATA_CKSUM, recoded_tkt);
        if (ret)
            goto cleanup;
    }

    ret = verify_pac_checksums(context, pac, is_service_tkt, server, privsvr);
    if (ret)
        goto cleanup;

    *pac_out = pac;
    pac = NULL;

cleanup:
    krb5_pac_free(context, pac);
    krb5_free_data(context, recoded_tkt);
    krb5_free_authdata(context, ifrel);
    krb5_free_authdata(context, recoded_ifrel);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_pac_verify(krb5_context context,
                const krb5_pac pac,
                krb5_timestamp authtime,
                krb5_const_principal principal,
                const krb5_keyblock *server,
                const krb5_keyblock *privsvr)
{
    return krb5_pac_verify_ext(context, pac, authtime, principal, server,
                               privsvr, FALSE);
}

krb5_error_code KRB5_CALLCONV
krb5_pac_verify_ext(krb5_context context,
                    const krb5_pac pac,
                    krb5_timestamp authtime,
                    krb5_const_principal principal,
                    const krb5_keyblock *server,
                    const krb5_keyblock *privsvr,
                    krb5_boolean with_realm)
{
    krb5_error_code ret;

    if (server != NULL || privsvr != NULL) {
        ret = verify_pac_checksums(context, pac, FALSE, server, privsvr);
        if (ret)
            return ret;
    }

    if (principal != NULL) {
        ret = k5_pac_validate_client(context, pac, authtime,
                                     principal, with_realm);
        if (ret)
            return ret;
    }

    return 0;
}

/*
 * PAC auth data attribute backend
 */
struct mspac_context {
    krb5_pac pac;
};

static krb5_error_code
mspac_init(krb5_context context, void **plugin_context)
{
    *plugin_context = NULL;
    return 0;
}

static void
mspac_flags(krb5_context context, void *plugin_context,
            krb5_authdatatype ad_type, krb5_flags *flags)
{
    *flags = AD_USAGE_TGS_REQ;
}

static void
mspac_fini(krb5_context context, void *plugin_context)
{
    return;
}

static krb5_error_code
mspac_request_init(krb5_context context, krb5_authdata_context actx,
                   void *plugin_context, void **request_context)
{
    struct mspac_context *pacctx;

    pacctx = malloc(sizeof(*pacctx));
    if (pacctx == NULL)
        return ENOMEM;

    pacctx->pac = NULL;

    *request_context = pacctx;

    return 0;
}

static krb5_error_code
mspac_import_authdata(krb5_context context, krb5_authdata_context actx,
                      void *plugin_context, void *request_context,
                      krb5_authdata **authdata, krb5_boolean kdc_issued,
                      krb5_const_principal kdc_issuer)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;

    if (kdc_issued)
        return EINVAL;

    if (pacctx->pac != NULL) {
        krb5_pac_free(context, pacctx->pac);
        pacctx->pac = NULL;
    }

    assert(authdata[0] != NULL);
    assert((authdata[0]->ad_type & AD_TYPE_FIELD_TYPE_MASK) ==
           KRB5_AUTHDATA_WIN2K_PAC);

    return krb5_pac_parse(context, authdata[0]->contents, authdata[0]->length,
                          &pacctx->pac);
}

static krb5_error_code
mspac_export_authdata(krb5_context context, krb5_authdata_context actx,
                      void *plugin_context, void *request_context,
                      krb5_flags usage, krb5_authdata ***authdata_out)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    krb5_error_code code;
    krb5_authdata **authdata;
    krb5_data data;

    if (pacctx->pac == NULL)
        return 0;

    authdata = calloc(2, sizeof(krb5_authdata *));
    if (authdata == NULL)
        return ENOMEM;

    authdata[0] = calloc(1, sizeof(krb5_authdata));
    if (authdata[0] == NULL) {
        free(authdata);
        return ENOMEM;
    }
    authdata[1] = NULL;

    code = krb5int_copy_data_contents(context, &pacctx->pac->data, &data);
    if (code != 0) {
        krb5_free_authdata(context, authdata);
        return code;
    }

    authdata[0]->magic = KV5M_AUTHDATA;
    authdata[0]->ad_type = KRB5_AUTHDATA_WIN2K_PAC;
    authdata[0]->length = data.length;
    authdata[0]->contents = (krb5_octet *)data.data;

    authdata[1] = NULL;

    *authdata_out = authdata;

    return 0;
}

static krb5_error_code
mspac_verify(krb5_context context, krb5_authdata_context actx,
             void *plugin_context, void *request_context,
             const krb5_auth_context *auth_context, const krb5_keyblock *key,
             const krb5_ap_req *req)
{
    krb5_error_code ret;
    struct mspac_context *pacctx = (struct mspac_context *)request_context;

    if (pacctx->pac == NULL)
        return EINVAL;

    ret = krb5_pac_verify(context, pacctx->pac,
                          req->ticket->enc_part2->times.authtime,
                          req->ticket->enc_part2->client, key, NULL);
    if (ret)
        TRACE_MSPAC_VERIFY_FAIL(context, ret);

    /*
     * If the above verification failed, don't fail the whole authentication,
     * just don't mark the PAC as verified.  A checksum mismatch can occur if
     * the PAC was copied from a cross-realm TGT by an ignorant KDC, and Apple
     * macOS Server Open Directory (as of 10.6) generates PACs with no server
     * checksum at all.
     */
    return 0;
}

static void
mspac_request_fini(krb5_context context, krb5_authdata_context actx,
                   void *plugin_context, void *request_context)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;

    if (pacctx != NULL) {
        krb5_pac_free(context, pacctx->pac);
        free(pacctx);
    }
}

#define STRLENOF(x) (sizeof((x)) - 1)

static struct {
    uint32_t type;
    krb5_data attribute;
} mspac_attribute_types[] = {
    { (uint32_t)-1,             { KV5M_DATA, STRLENOF("urn:mspac:"),
                                  "urn:mspac:" } },
    { KRB5_PAC_LOGON_INFO,       { KV5M_DATA,
                                   STRLENOF("urn:mspac:logon-info"),
                                   "urn:mspac:logon-info" } },
    { KRB5_PAC_CREDENTIALS_INFO, { KV5M_DATA,
                                   STRLENOF("urn:mspac:credentials-info"),
                                   "urn:mspac:credentials-info" } },
    { KRB5_PAC_SERVER_CHECKSUM,  { KV5M_DATA,
                                   STRLENOF("urn:mspac:server-checksum"),
                                   "urn:mspac:server-checksum" } },
    { KRB5_PAC_PRIVSVR_CHECKSUM, { KV5M_DATA,
                                   STRLENOF("urn:mspac:privsvr-checksum"),
                                   "urn:mspac:privsvr-checksum" } },
    { KRB5_PAC_CLIENT_INFO,      { KV5M_DATA,
                                   STRLENOF("urn:mspac:client-info"),
                                   "urn:mspac:client-info" } },
    { KRB5_PAC_DELEGATION_INFO,  { KV5M_DATA,
                                   STRLENOF("urn:mspac:delegation-info"),
                                   "urn:mspac:delegation-info" } },
    { KRB5_PAC_UPN_DNS_INFO,     { KV5M_DATA,
                                   STRLENOF("urn:mspac:upn-dns-info"),
                                   "urn:mspac:upn-dns-info" } },
};

#define MSPAC_ATTRIBUTE_COUNT   (sizeof(mspac_attribute_types)/sizeof(mspac_attribute_types[0]))

static krb5_error_code
mspac_type2attr(uint32_t type, krb5_data *attr)
{
    unsigned int i;

    for (i = 0; i < MSPAC_ATTRIBUTE_COUNT; i++) {
        if (mspac_attribute_types[i].type == type) {
            *attr = mspac_attribute_types[i].attribute;
            return 0;
        }
    }

    return ENOENT;
}

static krb5_error_code
mspac_attr2type(const krb5_data *attr, uint32_t *type)
{
    unsigned int i;

    for (i = 0; i < MSPAC_ATTRIBUTE_COUNT; i++) {
        if (attr->length == mspac_attribute_types[i].attribute.length &&
            strncasecmp(attr->data, mspac_attribute_types[i].attribute.data, attr->length) == 0) {
            *type = mspac_attribute_types[i].type;
            return 0;
        }
    }

    if (attr->length > STRLENOF("urn:mspac:") &&
        strncasecmp(attr->data, "urn:mspac:", STRLENOF("urn:mspac:")) == 0)
    {
        char *p = &attr->data[STRLENOF("urn:mspac:")];
        char *endptr;

        *type = strtoul(p, &endptr, 10);
        if (*type != 0 && *endptr == '\0')
            return 0;
    }

    return ENOENT;
}

static krb5_error_code
mspac_get_attribute_types(krb5_context context, krb5_authdata_context actx,
                          void *plugin_context, void *request_context,
                          krb5_data **attrs_out)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    unsigned int i, j;
    krb5_data *attrs;
    krb5_error_code ret;

    if (pacctx->pac == NULL)
        return ENOENT;

    attrs = calloc(1 + pacctx->pac->nbuffers + 1, sizeof(krb5_data));
    if (attrs == NULL)
        return ENOMEM;

    j = 0;

    /* The entire PAC */
    ret = krb5int_copy_data_contents(context,
                                     &mspac_attribute_types[0].attribute,
                                     &attrs[j++]);
    if (ret)
        goto fail;

    /* PAC buffers */
    for (i = 0; i < pacctx->pac->nbuffers; i++) {
        krb5_data attr;

        ret = mspac_type2attr(pacctx->pac->buffers[i].type, &attr);
        if (!ret) {
            ret = krb5int_copy_data_contents(context, &attr, &attrs[j++]);
            if (ret)
                goto fail;
        } else {
            int length;

            length = asprintf(&attrs[j].data, "urn:mspac:%d",
                              pacctx->pac->buffers[i].type);
            if (length < 0) {
                ret = ENOMEM;
                goto fail;
            }
            attrs[j++].length = length;
        }
    }
    attrs[j].data = NULL;
    attrs[j].length = 0;

    *attrs_out = attrs;

    return 0;

fail:
    krb5int_free_data_list(context, attrs);
    return ret;
}

static krb5_error_code
mspac_get_attribute(krb5_context context, krb5_authdata_context actx,
                    void *plugin_context, void *request_context,
                    const krb5_data *attribute, krb5_boolean *authenticated,
                    krb5_boolean *complete, krb5_data *value,
                    krb5_data *display_value, int *more)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    krb5_error_code ret;
    uint32_t type;

    if (display_value != NULL) {
        display_value->data = NULL;
        display_value->length = 0;
    }

    if (*more != -1 || pacctx->pac == NULL)
        return ENOENT;

    /* If it didn't verify, pretend it didn't exist. */
    if (!pacctx->pac->verified) {
        TRACE_MSPAC_DISCARD_UNVERF(context);
        return ENOENT;
    }

    ret = mspac_attr2type(attribute, &type);
    if (ret)
        return ret;

    /* -1 is a magic type that refers to the entire PAC */
    if (type == (uint32_t)-1) {
        if (value != NULL)
            ret = krb5int_copy_data_contents(context, &pacctx->pac->data,
                                             value);
        else
            ret = 0;
    } else {
        if (value != NULL)
            ret = krb5_pac_get_buffer(context, pacctx->pac, type, value);
        else
            ret = k5_pac_locate_buffer(context, pacctx->pac, type, NULL);
    }
    if (!ret) {
        *authenticated = pacctx->pac->verified;
        *complete = TRUE;
    }

    *more = 0;

    return ret;
}

static krb5_error_code
mspac_set_attribute(krb5_context context, krb5_authdata_context actx,
                    void *plugin_context, void *request_context,
                    krb5_boolean complete, const krb5_data *attribute,
                    const krb5_data *value)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    krb5_error_code ret;
    uint32_t type;

    if (pacctx->pac == NULL)
        return ENOENT;

    ret = mspac_attr2type(attribute, &type);
    if (ret)
        return ret;

    /* -1 is a magic type that refers to the entire PAC */
    if (type == (uint32_t)-1) {
        krb5_pac newpac;

        ret = krb5_pac_parse(context, value->data, value->length, &newpac);
        if (ret)
            return ret;

        krb5_pac_free(context, pacctx->pac);
        pacctx->pac = newpac;
    } else {
        ret = krb5_pac_add_buffer(context, pacctx->pac, type, value);
    }

    return ret;
}

static krb5_error_code
mspac_export_internal(krb5_context context, krb5_authdata_context actx,
                      void *plugin_context, void *request_context,
                      krb5_boolean restrict_authenticated, void **ptr)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    krb5_error_code ret;
    krb5_pac pac;

    *ptr = NULL;

    if (pacctx->pac == NULL)
        return ENOENT;

    if (restrict_authenticated && (pacctx->pac->verified) == FALSE)
        return ENOENT;

    ret = krb5_pac_parse(context, pacctx->pac->data.data,
                         pacctx->pac->data.length, &pac);
    if (!ret) {
        pac->verified = pacctx->pac->verified;
        *ptr = pac;
    }

    return ret;
}

static void
mspac_free_internal(krb5_context context, krb5_authdata_context actx,
                    void *plugin_context, void *request_context, void *ptr)
{
    if (ptr != NULL)
        krb5_pac_free(context, (krb5_pac)ptr);

    return;
}

static krb5_error_code
mspac_size(krb5_context context, krb5_authdata_context actx,
           void *plugin_context, void *request_context, size_t *sizep)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;

    *sizep += sizeof(int32_t);

    if (pacctx->pac != NULL)
        *sizep += pacctx->pac->data.length;

    *sizep += sizeof(int32_t);

    return 0;
}

static krb5_error_code
mspac_externalize(krb5_context context, krb5_authdata_context actx,
                  void *plugin_context, void *request_context,
                  krb5_octet **buffer, size_t *lenremain)
{
    krb5_error_code ret = 0;
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    size_t required = 0;
    krb5_octet *bp;
    size_t remain;

    bp = *buffer;
    remain = *lenremain;

    if (pacctx->pac != NULL) {
        mspac_size(context, actx, plugin_context, request_context, &required);

        if (required <= remain) {
            krb5_ser_pack_int32(pacctx->pac->data.length, &bp, &remain);
            krb5_ser_pack_bytes((krb5_octet *)pacctx->pac->data.data,
                                (size_t)pacctx->pac->data.length,
                                &bp, &remain);
            krb5_ser_pack_int32(pacctx->pac->verified, &bp, &remain);
        } else {
            ret = ENOMEM;
        }
    } else {
        krb5_ser_pack_int32(0, &bp, &remain); /* length */
        krb5_ser_pack_int32(0, &bp, &remain); /* verified */
    }

    *buffer = bp;
    *lenremain = remain;

    return ret;
}

static krb5_error_code
mspac_internalize(krb5_context context, krb5_authdata_context actx,
                  void *plugin_context, void *request_context,
                  krb5_octet **buffer, size_t *lenremain)
{
    struct mspac_context *pacctx = (struct mspac_context *)request_context;
    krb5_error_code ret;
    int32_t ibuf;
    uint8_t *bp;
    size_t remain;
    krb5_pac pac = NULL;

    bp = *buffer;
    remain = *lenremain;

    /* length */
    ret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (ret)
        return ret;

    if (ibuf != 0) {
        ret = krb5_pac_parse(context, bp, ibuf, &pac);
        if (ret)
            return ret;

        bp += ibuf;
        remain -= ibuf;
    }

    /* verified */
    ret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
    if (ret) {
        krb5_pac_free(context, pac);
        return ret;
    }

    if (pac != NULL)
        pac->verified = (ibuf != 0);

    if (pacctx->pac != NULL)
        krb5_pac_free(context, pacctx->pac);

    pacctx->pac = pac;

    *buffer = bp;
    *lenremain = remain;

    return 0;
}

static krb5_error_code
mspac_copy(krb5_context context, krb5_authdata_context actx,
           void *plugin_context, void *request_context,
           void *dst_plugin_context, void *dst_request_context)
{
    struct mspac_context *srcctx = (struct mspac_context *)request_context;
    struct mspac_context *dstctx = (struct mspac_context *)dst_request_context;
    krb5_error_code ret = 0;

    assert(dstctx != NULL);
    assert(dstctx->pac == NULL);

    if (srcctx->pac != NULL)
        ret = copy_pac(context, srcctx->pac, &dstctx->pac);

    return ret;
}

static krb5_authdatatype mspac_ad_types[] = { KRB5_AUTHDATA_WIN2K_PAC, 0 };

krb5plugin_authdata_client_ftable_v0 k5_mspac_ad_client_ftable = {
    "mspac",
    mspac_ad_types,
    mspac_init,
    mspac_fini,
    mspac_flags,
    mspac_request_init,
    mspac_request_fini,
    mspac_get_attribute_types,
    mspac_get_attribute,
    mspac_set_attribute,
    NULL, /* delete_attribute_proc */
    mspac_export_authdata,
    mspac_import_authdata,
    mspac_export_internal,
    mspac_free_internal,
    mspac_verify,
    mspac_size,
    mspac_externalize,
    mspac_internalize,
    mspac_copy
};
