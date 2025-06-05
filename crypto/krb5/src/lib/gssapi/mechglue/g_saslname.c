/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/mechglue/g_saslname.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "mglueP.h"
#include <krb5/krb5.h>

static char basis_32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

#define OID_SASL_NAME_LENGTH  (sizeof("GS2-XXXXXXXXXXX") - 1)

static OM_uint32
oidToSaslName(OM_uint32 *minor, const gss_OID mech,
              char sasl_name[OID_SASL_NAME_LENGTH + 1])
{
    unsigned char derBuf[2];
    krb5_crypto_iov iov[3];
    unsigned char cksumBuf[20], *q = cksumBuf;
    char *p = sasl_name;

    if (mech->length > 127) {
        *minor = ERANGE;
        return GSS_S_BAD_MECH;
    }

    derBuf[0] = 0x06;
    derBuf[1] = (unsigned char)mech->length;

    iov[0].flags = KRB5_CRYPTO_TYPE_SIGN_ONLY;
    iov[0].data.length = 2;
    iov[0].data.data = (char *)derBuf;
    iov[1].flags = KRB5_CRYPTO_TYPE_SIGN_ONLY;
    iov[1].data.length = mech->length;
    iov[1].data.data = (char *)mech->elements;
    iov[2].flags = KRB5_CRYPTO_TYPE_CHECKSUM;
    iov[2].data.length = sizeof(cksumBuf);
    iov[2].data.data = (char *)cksumBuf;

    *minor = krb5_k_make_checksum_iov(NULL, CKSUMTYPE_SHA1, NULL, 0, iov, 3);
    if (*minor != 0)
        return GSS_S_FAILURE;

    memcpy(p, "GS2-", 4);
    p += 4;

    *p++ = basis_32[q[0] >> 3];
    *p++ = basis_32[((q[0] & 7) << 2) | (q[1] >> 6)];
    *p++ = basis_32[(q[1] & 0x3f) >> 1];
    *p++ = basis_32[((q[1] & 1) << 4) | (q[2] >> 4)];
    *p++ = basis_32[((q[2] & 0xf) << 1) | (q[3] >> 7)];
    *p++ = basis_32[(q[3] & 0x7f) >> 2];
    *p++ = basis_32[((q[3] & 3) << 3) | (q[4] >> 5)];
    *p++ = basis_32[(q[4] & 0x1f)];
    *p++ = basis_32[q[5] >> 3];
    *p++ = basis_32[((q[5] & 7) << 2) | (q[6] >> 6)];
    *p++ = basis_32[(q[6] & 0x3f) >> 1];

    *p++ = '\0';

    *minor = 0;
    return GSS_S_COMPLETE;
}

static OM_uint32
oidToSaslNameAlloc(OM_uint32 *minor, const gss_OID mech,
                   gss_buffer_t sasl_name)
{
    OM_uint32 status, tmpMinor;

    sasl_name->value = malloc(OID_SASL_NAME_LENGTH + 1);
    if (sasl_name->value == NULL) {
        *minor = ENOMEM;
        return GSS_S_FAILURE;
    }
    sasl_name->length = OID_SASL_NAME_LENGTH;

    status = oidToSaslName(minor, mech, (char *)sasl_name->value);
    if (GSS_ERROR(status)) {
        gss_release_buffer(&tmpMinor, sasl_name);
        return status;
    }

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV gss_inquire_saslname_for_mech(
    OM_uint32     *minor_status,
    const gss_OID  desired_mech,
    gss_buffer_t   sasl_mech_name,
    gss_buffer_t   mech_name,
    gss_buffer_t   mech_description)
{
    OM_uint32       status;
    gss_OID         selected_mech, public_mech;
    gss_mechanism   mech;

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    if (sasl_mech_name != GSS_C_NO_BUFFER) {
        sasl_mech_name->length = 0;
        sasl_mech_name->value = NULL;
    }

    if (mech_name != GSS_C_NO_BUFFER) {
        mech_name->length = 0;
        mech_name->value = NULL;
    }

    if (mech_description != GSS_C_NO_BUFFER) {
        mech_description->length = 0;
        mech_description->value = NULL;
    }

    status = gssint_select_mech_type(minor_status, desired_mech,
                                     &selected_mech);
    if (status != GSS_S_COMPLETE)
        return status;

    mech = gssint_get_mechanism(desired_mech);
    if (mech == NULL) {
        return GSS_S_BAD_MECH;
    } else if (mech->gss_inquire_saslname_for_mech == NULL) {
        status = GSS_S_UNAVAILABLE;
    } else {
        public_mech = gssint_get_public_oid(selected_mech);
        status = mech->gss_inquire_saslname_for_mech(minor_status, public_mech,
                                                     sasl_mech_name, mech_name,
                                                     mech_description);
        if (status != GSS_S_COMPLETE)
            map_error(minor_status, mech);
    }

    if (status == GSS_S_UNAVAILABLE) {
        if (sasl_mech_name != GSS_C_NO_BUFFER)
            status = oidToSaslNameAlloc(minor_status, desired_mech,
                                        sasl_mech_name);
        else
            status = GSS_S_COMPLETE;
    }

    return status;
}

/* We cannot interpose this function as mech_type is an output parameter. */
OM_uint32 KRB5_CALLCONV gss_inquire_mech_for_saslname(
    OM_uint32           *minor_status,
    const gss_buffer_t   sasl_mech_name,
    gss_OID             *mech_type)
{
    OM_uint32       status, tmpMinor;
    gss_OID_set     mechSet = GSS_C_NO_OID_SET;
    size_t          i;

    if (minor_status != NULL)
        *minor_status = 0;

    if (mech_type != NULL)
        *mech_type = GSS_C_NO_OID;

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    status = gss_indicate_mechs(minor_status, &mechSet);
    if (status != GSS_S_COMPLETE)
        return status;

    for (i = 0, status = GSS_S_BAD_MECH; i < mechSet->count; i++) {
        gss_mechanism mech;
        char mappedName[OID_SASL_NAME_LENGTH + 1];

        mech = gssint_get_mechanism(&mechSet->elements[i]);
        if (mech != NULL && mech->gss_inquire_mech_for_saslname != NULL) {
            status = mech->gss_inquire_mech_for_saslname(minor_status,
                                                         sasl_mech_name,
                                                         mech_type);
            if (status == GSS_S_COMPLETE)
                break;
        }
        if (status == GSS_S_BAD_MECH &&
            sasl_mech_name->length == OID_SASL_NAME_LENGTH &&
            oidToSaslName(&tmpMinor, &mechSet->elements[i],
                          mappedName) == GSS_S_COMPLETE &&
            memcmp(sasl_mech_name->value, mappedName,
                   OID_SASL_NAME_LENGTH) == 0) {
            if (mech_type != NULL)
                *mech_type = &mech->mech_type;
            status = GSS_S_COMPLETE;
            break;
        }
    }

    gss_release_oid_set(&tmpMinor, &mechSet);

    return status;
}
