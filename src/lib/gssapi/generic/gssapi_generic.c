/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

/*
 * $Id$
 */

#include "gssapiP_generic.h"

/*
 * See krb5/gssapi_krb5.c for a description of the algorithm for
 * encoding an object identifier.
 */

/* Reserved static storage for GSS_oids.  Comments are quotes from RFC 2744. */

#define oids ((gss_OID_desc *)const_oids)
static const gss_OID_desc const_oids[] = {
    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01"},
    /* corresponding to an object-identifier value of
     * {iso(1) member-body(2) United States(840) mit(113554)
     * infosys(1) gssapi(2) generic(1) user_name(1)}.  The constant
     * GSS_C_NT_USER_NAME should be initialized to point
     * to that gss_OID_desc.
     */

    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x02"},
    /* corresponding to an object-identifier value of
     * {iso(1) member-body(2) United States(840) mit(113554)
     * infosys(1) gssapi(2) generic(1) machine_uid_name(2)}.
     * The constant GSS_C_NT_MACHINE_UID_NAME should be
     * initialized to point to that gss_OID_desc.
     */

    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x03"},
    /* corresponding to an object-identifier value of
     * {iso(1) member-body(2) United States(840) mit(113554)
     * infosys(1) gssapi(2) generic(1) string_uid_name(3)}.
     * The constant GSS_C_NT_STRING_UID_NAME should be
     * initialized to point to that gss_OID_desc.
     */

    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {6, (void *)"\x2b\x06\x01\x05\x06\x02"},
    /* corresponding to an object-identifier value of
     * {iso(1) org(3) dod(6) internet(1) security(5)
     * nametypes(6) gss-host-based-services(2)).  The constant
     * GSS_C_NT_HOSTBASED_SERVICE_X should be initialized to point
     * to that gss_OID_desc.  This is a deprecated OID value, and
     * implementations wishing to support hostbased-service names
     * should instead use the GSS_C_NT_HOSTBASED_SERVICE OID,
     * defined below, to identify such names;
     * GSS_C_NT_HOSTBASED_SERVICE_X should be accepted a synonym
     * for GSS_C_NT_HOSTBASED_SERVICE when presented as an input
     * parameter, but should not be emitted by GSS-API
     * implementations
     */

    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04"},
    /* corresponding to an object-identifier value of
     * {iso(1) member-body(2) Unites States(840) mit(113554)
     * infosys(1) gssapi(2) generic(1) service_name(4)}.
     * The constant GSS_C_NT_HOSTBASED_SERVICE should be
     * initialized to point to that gss_OID_desc.
     */

    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {6, (void *)"\x2b\x06\01\x05\x06\x03"},
    /* corresponding to an object identifier value of
     * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
     * 6(nametypes), 3(gss-anonymous-name)}.  The constant
     * and GSS_C_NT_ANONYMOUS should be initialized to point
     * to that gss_OID_desc.
     */

    /*
     * The implementation must reserve static storage for a
     * gss_OID_desc object containing the value */
    {6, (void *)"\x2b\x06\x01\x05\x06\x04"},
    /* corresponding to an object-identifier value of
     * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
     * 6(nametypes), 4(gss-api-exported-name)}.  The constant
     * GSS_C_NT_EXPORT_NAME should be initialized to point
     * to that gss_OID_desc.
     */
    {6, (void *)"\x2b\x06\x01\x05\x06\x06"},
    /* corresponding to an object-identifier value of
     * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
     * 6(nametypes), 6(gss-composite-export)}.  The constant
     * GSS_C_NT_COMPOSITE_EXPORT should be initialized to point
     * to that gss_OID_desc.
     */
    /* GSS_C_INQ_SSPI_SESSION_KEY 1.2.840.113554.1.2.2.5.5 */
    {11, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x05\x05"},

    /* RFC 5587 attributes, see below */
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x01"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x02"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x03"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x04"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x05"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x06"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x07"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x08"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x09"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x0a"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x0b"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x0c"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x0d"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x0e"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x0f"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x10"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x11"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x12"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x13"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x14"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x15"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x16"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x17"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x18"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x19"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x1a"},
    {7, (void *)"\x2b\x06\x01\x05\x05\x0d\x1b"},
};

/* Here are the constants which point to the static structure above.
 *
 * Constants of the form GSS_C_NT_* are specified by rfc 2744.
 *
 * Constants of the form gss_nt_* are the original MIT krb5 names
 * found in gssapi_generic.h.  They are provided for compatibility. */

GSS_DLLIMP gss_OID GSS_C_NT_USER_NAME           = oids+0;
GSS_DLLIMP gss_OID gss_nt_user_name             = oids+0;

GSS_DLLIMP gss_OID GSS_C_NT_MACHINE_UID_NAME    = oids+1;
GSS_DLLIMP gss_OID gss_nt_machine_uid_name      = oids+1;

GSS_DLLIMP gss_OID GSS_C_NT_STRING_UID_NAME     = oids+2;
GSS_DLLIMP gss_OID gss_nt_string_uid_name       = oids+2;

GSS_DLLIMP gss_OID GSS_C_NT_HOSTBASED_SERVICE_X = oids+3;
gss_OID gss_nt_service_name_v2                  = oids+3;

GSS_DLLIMP gss_OID GSS_C_NT_HOSTBASED_SERVICE   = oids+4;
GSS_DLLIMP gss_OID gss_nt_service_name          = oids+4;

GSS_DLLIMP gss_OID GSS_C_NT_ANONYMOUS           = oids+5;

GSS_DLLIMP gss_OID GSS_C_NT_EXPORT_NAME         = oids+6;
gss_OID gss_nt_exported_name                    = oids+6;

GSS_DLLIMP gss_OID GSS_C_NT_COMPOSITE_EXPORT    = oids+7;

GSS_DLLIMP gss_OID GSS_C_INQ_SSPI_SESSION_KEY   = oids+8;

GSS_DLLIMP gss_const_OID GSS_C_MA_MECH_CONCRETE     = oids+9;
GSS_DLLIMP gss_const_OID GSS_C_MA_MECH_PSEUDO       = oids+10;
GSS_DLLIMP gss_const_OID GSS_C_MA_MECH_COMPOSITE    = oids+11;
GSS_DLLIMP gss_const_OID GSS_C_MA_MECH_NEGO         = oids+12;
GSS_DLLIMP gss_const_OID GSS_C_MA_MECH_GLUE         = oids+13;
GSS_DLLIMP gss_const_OID GSS_C_MA_NOT_MECH          = oids+14;
GSS_DLLIMP gss_const_OID GSS_C_MA_DEPRECATED        = oids+15;
GSS_DLLIMP gss_const_OID GSS_C_MA_NOT_DFLT_MECH     = oids+16;
GSS_DLLIMP gss_const_OID GSS_C_MA_ITOK_FRAMED       = oids+17;
GSS_DLLIMP gss_const_OID GSS_C_MA_AUTH_INIT         = oids+18;
GSS_DLLIMP gss_const_OID GSS_C_MA_AUTH_TARG         = oids+19;
GSS_DLLIMP gss_const_OID GSS_C_MA_AUTH_INIT_INIT    = oids+20;
GSS_DLLIMP gss_const_OID GSS_C_MA_AUTH_TARG_INIT    = oids+21;
GSS_DLLIMP gss_const_OID GSS_C_MA_AUTH_INIT_ANON    = oids+22;
GSS_DLLIMP gss_const_OID GSS_C_MA_AUTH_TARG_ANON    = oids+23;
GSS_DLLIMP gss_const_OID GSS_C_MA_DELEG_CRED        = oids+24;
GSS_DLLIMP gss_const_OID GSS_C_MA_INTEG_PROT        = oids+25;
GSS_DLLIMP gss_const_OID GSS_C_MA_CONF_PROT         = oids+26;
GSS_DLLIMP gss_const_OID GSS_C_MA_MIC               = oids+27;
GSS_DLLIMP gss_const_OID GSS_C_MA_WRAP              = oids+28;
GSS_DLLIMP gss_const_OID GSS_C_MA_PROT_READY        = oids+29;
GSS_DLLIMP gss_const_OID GSS_C_MA_REPLAY_DET        = oids+30;
GSS_DLLIMP gss_const_OID GSS_C_MA_OOS_DET           = oids+31;
GSS_DLLIMP gss_const_OID GSS_C_MA_CBINDINGS         = oids+32;
GSS_DLLIMP gss_const_OID GSS_C_MA_PFS               = oids+33;
GSS_DLLIMP gss_const_OID GSS_C_MA_COMPRESS          = oids+34;
GSS_DLLIMP gss_const_OID GSS_C_MA_CTX_TRANS         = oids+35;

static gss_OID_set_desc gss_ma_known_attrs_desc = { 27, oids+9 };
gss_OID_set gss_ma_known_attrs = &gss_ma_known_attrs_desc;

static struct mech_attr_info_desc {
    gss_OID mech_attr;
    const char *name;
    const char *short_desc;
    const char *long_desc;
} mech_attr_info[] = {
    {
        oids+9,
        "GSS_C_MA_MECH_CONCRETE",
        "concrete-mech",
        "Mechanism is neither a pseudo-mechanism nor a composite mechanism.",
    },
    {
        oids+10,
        "GSS_C_MA_MECH_PSEUDO",
        "pseudo-mech",
        "Mechanism is a pseudo-mechanism.",
    },
    {
        oids+11,
        "GSS_C_MA_MECH_COMPOSITE",
        "composite-mech",
        "Mechanism is a composite of other mechanisms.",
    },
    {
        oids+12,
        "GSS_C_MA_MECH_NEGO",
        "mech-negotiation-mech",
        "Mechanism negotiates other mechanisms.",
    },
    {
        oids+13,
        "GSS_C_MA_MECH_GLUE",
        "mech-glue",
        "OID is not a mechanism but the GSS-API itself.",
    },
    {
        oids+14,
        "GSS_C_MA_NOT_MECH",
        "not-mech",
        "Known OID but not a mechanism OID.",
    },
    {
        oids+15,
        "GSS_C_MA_DEPRECATED",
        "mech-deprecated",
        "Mechanism is deprecated.",
    },
    {
        oids+16,
        "GSS_C_MA_NOT_DFLT_MECH",
        "mech-not-default",
        "Mechanism must not be used as a default mechanism.",
    },
    {
        oids+17,
        "GSS_C_MA_ITOK_FRAMED",
        "initial-is-framed",
        "Mechanism's initial contexts are properly framed.",
    },
    {
        oids+18,
        "GSS_C_MA_AUTH_INIT",
        "auth-init-princ",
        "Mechanism supports authentication of initiator to acceptor.",
    },
    {
        oids+19,
        "GSS_C_MA_AUTH_TARG",
        "auth-targ-princ",
        "Mechanism supports authentication of acceptor to initiator.",
    },
    {
        oids+20,
        "GSS_C_MA_AUTH_INIT_INIT",
        "auth-init-princ-initial",
        "Mechanism supports authentication of initiator using "
        "initial credentials.",
    },
    {
        oids+21,
        "GSS_C_MA_AUTH_TARG_INIT",
        "auth-target-princ-initial",
        "Mechanism supports authentication of acceptor using "
        "initial credentials.",
    },
    {
        oids+22,
        "GSS_C_MA_AUTH_INIT_ANON",
        "auth-init-princ-anon",
        "Mechanism supports GSS_C_NT_ANONYMOUS as an initiator name.",
    },
    {
        oids+23,
        "GSS_C_MA_AUTH_TARG_ANON",
        "auth-targ-princ-anon",
        "Mechanism supports GSS_C_NT_ANONYMOUS as an acceptor name.",
    },
    {
        oids+24,
        "GSS_C_MA_DELEG_CRED",
        "deleg-cred",
        "Mechanism supports credential delegation.",
    },
    {
        oids+25,
        "GSS_C_MA_INTEG_PROT",
        "integ-prot",
        "Mechanism supports per-message integrity protection.",
    },
    {
        oids+26,
        "GSS_C_MA_CONF_PROT",
        "conf-prot",
        "Mechanism supports per-message confidentiality protection.",
    },
    {
        oids+27,
        "GSS_C_MA_MIC",
        "mic",
        "Mechanism supports Message Integrity Code (MIC) tokens.",
    },
    {
        oids+28,
        "GSS_C_MA_WRAP",
        "wrap",
        "Mechanism supports wrap tokens.",
    },
    {
        oids+29,
        "GSS_C_MA_PROT_READY",
        "prot-ready",
        "Mechanism supports per-message proteciton prior to "
        "full context establishment.",
    },
    {
        oids+30,
        "GSS_C_MA_REPLAY_DET",
        "replay-detection",
        "Mechanism supports replay detection.",
    },
    {
        oids+31,
        "GSS_C_MA_OOS_DET",
        "oos-detection",
        "Mechanism supports out-of-sequence detection.",
    },
    {
        oids+32,
        "GSS_C_MA_CBINDINGS",
        "channel-bindings",
        "Mechanism supports channel bindings.",
    },
    {
        oids+33,
        "GSS_C_MA_PFS",
        "pfs",
        "Mechanism supports Perfect Forward Security.",
    },
    {
        oids+34,
        "GSS_C_MA_COMPRESS",
        "compress",
        "Mechanism supports compression of data inputs to gss_wrap().",
    },
    {
        oids+35,
        "GSS_C_MA_CTX_TRANS",
        "context-transfer",
        "Mechanism supports security context export/import.",
    },
};

OM_uint32
generic_gss_display_mech_attr(
    OM_uint32         *minor_status,
    gss_const_OID      mech_attr,
    gss_buffer_t       name,
    gss_buffer_t       short_desc,
    gss_buffer_t       long_desc)
{
    size_t i;

    if (name != GSS_C_NO_BUFFER) {
        name->length = 0;
        name->value = NULL;
    }
    if (short_desc != GSS_C_NO_BUFFER) {
        short_desc->length = 0;
        short_desc->value = NULL;
    }
    if (long_desc != GSS_C_NO_BUFFER) {
        long_desc->length = 0;
        long_desc->value = NULL;
    }
    for (i = 0; i < sizeof(mech_attr_info)/sizeof(mech_attr_info[0]); i++) {
        struct mech_attr_info_desc *mai = &mech_attr_info[i];

        if (g_OID_equal(mech_attr, mai->mech_attr)) {
            if (name != GSS_C_NO_BUFFER &&
                !g_make_string_buffer(mai->name, name)) {
                *minor_status = ENOMEM;
                return GSS_S_FAILURE;
            }
            if (short_desc != GSS_C_NO_BUFFER &&
                !g_make_string_buffer(mai->short_desc, short_desc)) {
                *minor_status = ENOMEM;
                return GSS_S_FAILURE;
            }
            if (long_desc != GSS_C_NO_BUFFER &&
                !g_make_string_buffer(mai->long_desc, long_desc)) {
                *minor_status = ENOMEM;
                return GSS_S_FAILURE;
            }
            return GSS_S_COMPLETE;
        }
    }

    return GSS_S_BAD_MECH_ATTR;
}

static gss_buffer_desc const_attrs[] = {
    { sizeof("local-login-user") - 1,
      "local-login-user" },
};

GSS_DLLIMP gss_buffer_t GSS_C_ATTR_LOCAL_LOGIN_USER = &const_attrs[0];
