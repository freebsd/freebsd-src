/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "gssapi_locl.h"

RCSID("$Id: external.c,v 1.5 2000/07/22 03:45:28 assar Exp $");

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x01"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) user_name(1)}.  The constant
 * GSS_C_NT_USER_NAME should be initialized to point
 * to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_user_name_oid_desc =
{10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 "\x01\x02\x01\x01"};

gss_OID GSS_C_NT_USER_NAME = &gss_c_nt_user_name_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x02"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) machine_uid_name(2)}.
 * The constant GSS_C_NT_MACHINE_UID_NAME should be
 * initialized to point to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_machine_uid_name_oid_desc =
{10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 "\x01\x02\x01\x02"};

gss_OID GSS_C_NT_MACHINE_UID_NAME = &gss_c_nt_machine_uid_name_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x03"},
 * corresponding to an object-identifier value of
 * {iso(1) member-body(2) United States(840) mit(113554)
 *  infosys(1) gssapi(2) generic(1) string_uid_name(3)}.
 * The constant GSS_C_NT_STRING_UID_NAME should be
 * initialized to point to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_string_uid_name_oid_desc =
{10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 "\x01\x02\x01\x03"};

gss_OID GSS_C_NT_STRING_UID_NAME = &gss_c_nt_string_uid_name_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\x01\x05\x06\x02"},
 * corresponding to an object-identifier value of
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

static gss_OID_desc gss_c_nt_hostbased_service_x_oid_desc =
{6, (void *)"\x2b\x06\x01\x05\x06\x02"};

gss_OID GSS_C_NT_HOSTBASED_SERVICE_X = &gss_c_nt_hostbased_service_x_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {10, (void *)"\x2a\x86\x48\x86\xf7\x12"
 *              "\x01\x02\x01\x04"}, corresponding to an
 * object-identifier value of {iso(1) member-body(2)
 * Unites States(840) mit(113554) infosys(1) gssapi(2)
 * generic(1) service_name(4)}.  The constant
 * GSS_C_NT_HOSTBASED_SERVICE should be initialized
 * to point to that gss_OID_desc.
 */
static gss_OID_desc gss_c_nt_hostbased_service_oid_desc =
{10, (void *)"\x2a\x86\x48\x86\xf7\x12" "\x01\x02\x01\x04"};

gss_OID GSS_C_NT_HOSTBASED_SERVICE = &gss_c_nt_hostbased_service_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\01\x05\x06\x03"},
 * corresponding to an object identifier value of
 * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
 * 6(nametypes), 3(gss-anonymous-name)}.  The constant
 * and GSS_C_NT_ANONYMOUS should be initialized to point
 * to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_anonymous_oid_desc =
{6, (void *)"\x2b\x06\01\x05\x06\x03"};

gss_OID GSS_C_NT_ANONYMOUS = &gss_c_nt_anonymous_oid_desc;

/*
 * The implementation must reserve static storage for a
 * gss_OID_desc object containing the value
 * {6, (void *)"\x2b\x06\x01\x05\x06\x04"},
 * corresponding to an object-identifier value of
 * {1(iso), 3(org), 6(dod), 1(internet), 5(security),
 * 6(nametypes), 4(gss-api-exported-name)}.  The constant
 * GSS_C_NT_EXPORT_NAME should be initialized to point
 * to that gss_OID_desc.
 */

static gss_OID_desc gss_c_nt_export_name_oid_desc =
{6, (void *)"\x2b\x06\x01\x05\x06\x04"};

gss_OID GSS_C_NT_EXPORT_NAME = &gss_c_nt_export_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   krb5(2) krb5_name(1)}.  The recommended symbolic name for this type
 *   is "GSS_KRB5_NT_PRINCIPAL_NAME".
 */

static gss_OID_desc gss_krb5_nt_principal_name_oid_desc =
{10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01"};

gss_OID GSS_KRB5_NT_PRINCIPAL_NAME = &gss_krb5_nt_principal_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) user_name(1)}.  The recommended symbolic name for this
 *   type is "GSS_KRB5_NT_USER_NAME".
 */

gss_OID GSS_KRB5_NT_USER_NAME = &gss_c_nt_user_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) machine_uid_name(2)}.  The recommended symbolic name for
 *   this type is "GSS_KRB5_NT_MACHINE_UID_NAME".
 */

gss_OID GSS_KRB5_NT_MACHINE_UID_NAME = &gss_c_nt_machine_uid_name_oid_desc;

/*
 *   This name form shall be represented by the Object Identifier {iso(1)
 *   member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
 *   generic(1) string_uid_name(3)}.  The recommended symbolic name for
 *   this type is "GSS_KRB5_NT_STRING_UID_NAME".
 */

gss_OID GSS_KRB5_NT_STRING_UID_NAME = &gss_c_nt_string_uid_name_oid_desc;

/*
 *   To support ongoing experimentation, testing, and evolution of the
 *   specification, the Kerberos V5 GSS-API mechanism as defined in this
 *   and any successor memos will be identified with the following Object
 *   Identifier, as defined in RFC-1510, until the specification is
 *   advanced to the level of Proposed Standard RFC:
 *
 *   {iso(1), org(3), dod(5), internet(1), security(5), kerberosv5(2)}
 *
 *   Upon advancement to the level of Proposed Standard RFC, the Kerberos
 *   V5 GSS-API mechanism will be identified by an Object Identifier
 *   having the value:
 *
 *   {iso(1) member-body(2) United States(840) mit(113554) infosys(1)
 *   gssapi(2) krb5(2)}
 */

#if 0 /* This is the old OID */

static gss_OID_desc gss_krb5_mechanism_oid_desc =
{5, (void *)"\x2b\x05\x01\x05\x02"};

#endif

static gss_OID_desc gss_krb5_mechanism_oid_desc =
{9, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"};

gss_OID GSS_KRB5_MECHANISM = &gss_krb5_mechanism_oid_desc;

/*
 * Context for krb5 calls.
 */

krb5_context gssapi_krb5_context;
