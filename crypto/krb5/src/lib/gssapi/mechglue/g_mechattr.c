/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/mechglue/g_mechattr.c */
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

static int
testMechAttr(gss_const_OID attr,
             gss_const_OID_set against)
{
    int present = 0;
    OM_uint32 minor;

    if (GSS_ERROR(generic_gss_test_oid_set_member(&minor, attr,
                                                  (gss_OID_set)against,
                                                  &present)))
        return 0;

    return present;
}

/*
 * Return TRUE iff all the elements of desired and none of the elements
 * of except exist in available.
 */
static int
testMechAttrsOffered(gss_const_OID_set desired,
                     gss_const_OID_set except,
                     gss_const_OID_set available)
{
    size_t i;

    if (desired != GSS_C_NO_OID_SET) {
        for (i = 0; i < desired->count; i++) {
            if (!testMechAttr(&desired->elements[i], available))
                return 0;
        }
    }

    if (except != GSS_C_NO_OID_SET) {
        for (i = 0; i < except->count; i++) {
            if (testMechAttr(&except->elements[i], available))
                return 0;
        }
    }

    return 1;
}

/*
 * Return TRUE iff all the elements of critical exist in known.
 */
static int
testMechAttrsKnown(gss_const_OID_set critical,
                   gss_const_OID_set known)
{
    size_t i;

    if (critical != GSS_C_NO_OID_SET) {
        for (i = 0; i < critical->count; i++) {
            if (!testMechAttr(&critical->elements[i], known))
                return 0;
        }
    }

    return 1;
}

OM_uint32 KRB5_CALLCONV
gss_indicate_mechs_by_attrs(
    OM_uint32         *minor,
    gss_const_OID_set  desired_mech_attrs,
    gss_const_OID_set  except_mech_attrs,
    gss_const_OID_set  critical_mech_attrs,
    gss_OID_set       *mechs)
{
    OM_uint32       status, tmpMinor;
    gss_OID_set     allMechs = GSS_C_NO_OID_SET;
    size_t          i;

    if (minor != NULL)
        *minor = 0;

    if (mechs != NULL)
        *mechs = GSS_C_NO_OID_SET;

    if (minor == NULL || mechs == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    status = gss_indicate_mechs(minor, &allMechs);
    if (GSS_ERROR(status))
        goto cleanup;

    status = generic_gss_create_empty_oid_set(minor, mechs);
    if (GSS_ERROR(status))
        goto cleanup;

    for (i = 0; i < allMechs->count; i++) {
        gss_OID_set supportedAttrs = GSS_C_NO_OID_SET;
        gss_OID_set knownAttrs = GSS_C_NO_OID_SET;

        status = gss_inquire_attrs_for_mech(minor, &allMechs->elements[i],
                                            &supportedAttrs, &knownAttrs);
        if (GSS_ERROR(status))
            continue;

        if (testMechAttrsOffered(desired_mech_attrs,
                                 except_mech_attrs, supportedAttrs) &&
            testMechAttrsKnown(critical_mech_attrs, knownAttrs)) {
            status = gss_add_oid_set_member(minor, &allMechs->elements[i],
                                            mechs);
            if (GSS_ERROR(status)) {
                gss_release_oid_set(&tmpMinor, &supportedAttrs);
                gss_release_oid_set(&tmpMinor, &knownAttrs);
                goto cleanup;
            }
        }

        gss_release_oid_set(&tmpMinor, &supportedAttrs);
        gss_release_oid_set(&tmpMinor, &knownAttrs);
    }

    *minor = 0;
    status = GSS_S_COMPLETE;

cleanup:
    gss_release_oid_set(&tmpMinor, &allMechs);

    return status;
}

OM_uint32 KRB5_CALLCONV
gss_inquire_attrs_for_mech(
    OM_uint32         *minor,
    gss_const_OID      mech_oid,
    gss_OID_set       *mech_attrs,
    gss_OID_set       *known_mech_attrs)
{
    OM_uint32       status, tmpMinor;
    gss_OID         selected_mech, public_mech;
    gss_mechanism   mech;

    if (minor != NULL)
        *minor = 0;

    if (mech_attrs != NULL)
        *mech_attrs = GSS_C_NO_OID_SET;

    if (known_mech_attrs != NULL)
        *known_mech_attrs = GSS_C_NO_OID_SET;

    if (minor == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    status = gssint_select_mech_type(minor, mech_oid, &selected_mech);
    if (status != GSS_S_COMPLETE)
        return status;

    mech = gssint_get_mechanism(selected_mech);
    if (mech == NULL)
        return GSS_S_BAD_MECH;

    /* If the mech does not implement RFC 5587, return success with an empty
     * mech_attrs and known_mech_attrs. */
    if (mech->gss_inquire_attrs_for_mech == NULL)
        return GSS_S_COMPLETE;

    public_mech = gssint_get_public_oid(selected_mech);
    status = mech->gss_inquire_attrs_for_mech(minor, public_mech, mech_attrs,
                                              known_mech_attrs);
    if (GSS_ERROR(status)) {
        map_error(minor, mech);
        return status;
    }

    if (known_mech_attrs != NULL && *known_mech_attrs == GSS_C_NO_OID_SET) {
        status = generic_gss_copy_oid_set(minor,
                                          gss_ma_known_attrs,
                                          known_mech_attrs);
        if (GSS_ERROR(status)) {
            gss_release_oid_set(&tmpMinor, mech_attrs);
            if (mech_attrs != NULL)
                *mech_attrs = GSS_C_NO_OID_SET;
        }
    }

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_display_mech_attr(
    OM_uint32         *minor,
    gss_const_OID      mech_attr,
    gss_buffer_t       name,
    gss_buffer_t       short_desc,
    gss_buffer_t       long_desc)
{
    return generic_gss_display_mech_attr(minor, mech_attr,
                                         name, short_desc, long_desc);
}
