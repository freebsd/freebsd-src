/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/server/auth.h - kadmin authorization declarations */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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

#ifndef AUTH_H
#define AUTH_H

#define OP_ADDPRINC     1
#define OP_MODPRINC     2
#define OP_SETSTR       3
#define OP_CPW          4
#define OP_CHRAND       5
#define OP_SETKEY       6
#define OP_PURGEKEYS    7
#define OP_DELPRINC     8
#define OP_RENPRINC     9
#define OP_GETPRINC    10
#define OP_GETSTRS     11
#define OP_EXTRACT     12
#define OP_LISTPRINCS  13
#define OP_ADDPOL      14
#define OP_MODPOL      15
#define OP_DELPOL      16
#define OP_GETPOL      17
#define OP_LISTPOLS    18
#define OP_IPROP       19

/* Initialize all authorization modules. */
krb5_error_code auth_init(krb5_context context, const char *acl_file);

/* Release authorization module state. */
void auth_fini(krb5_context context);

/* Authorize the operation given by opcode, using the appropriate subset of p1,
 * p2, s1, s2, polent, and mask. */
krb5_boolean auth(krb5_context context, int opcode,
                  krb5_const_principal client, krb5_const_principal p1,
                  krb5_const_principal p2, const char *s1, const char *s2,
                  const kadm5_policy_ent_rec *polent, long mask);

/* Authorize an add-principal or modify-principal operation, and apply
 * restrictions to ent and mask if any modules supply them. */
krb5_boolean auth_restrict(krb5_context context, int opcode,
                           krb5_const_principal client,
                           kadm5_principal_ent_t ent, long *mask);

/* Notify modules that the most recent authorized operation has ended. */
void auth_end(krb5_context context);

/* initvt declarations for built-in modules */

krb5_error_code kadm5_auth_acl_initvt(krb5_context context, int maj_ver,
                                      int min_ver, krb5_plugin_vtable vtable);
krb5_error_code kadm5_auth_self_initvt(krb5_context context, int maj_ver,
                                       int min_ver, krb5_plugin_vtable vtable);

#endif /* AUTH_H */
