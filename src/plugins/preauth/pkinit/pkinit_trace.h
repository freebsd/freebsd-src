/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/pkinit/pkinit_trace.h - PKINIT tracing macros */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

#ifndef PKINIT_TRACE_H
#define PKINIT_TRACE_H

#include "k5-trace.h"

#define TRACE_PKINIT_CLIENT_EKU_ACCEPT(c)                       \
    TRACE(c, "PKINIT client found acceptable EKU in KDC cert")
#define TRACE_PKINIT_CLIENT_EKU_REJECT(c)                               \
    TRACE(c, "PKINIT client found no acceptable EKU in KDC cert")
#define TRACE_PKINIT_CLIENT_EKU_SKIP(c)                                 \
    TRACE(c, "PKINIT client skipping EKU check due to configuration")
#define TRACE_PKINIT_CLIENT_KDF_ALG(c, kdf, keyblock)                   \
    TRACE(c, "PKINIT client used KDF {hexdata} to compute reply key "   \
          "{keyblock}", kdf, keyblock)
#define TRACE_PKINIT_CLIENT_KDF_OS2K(c, keyblock)                       \
    TRACE(c, "PKINIT client used octetstring2key to compute reply key " \
          "{keyblock}", keyblock)
#define TRACE_PKINIT_CLIENT_NO_IDENTITY(c)                              \
    TRACE(c, "PKINIT client has no configured identity; giving up")
#define TRACE_PKINIT_CLIENT_REP_CHECKSUM_FAIL(c, expected, received)    \
    TRACE(c, "PKINIT client checksum mismatch: expected {cksum}, "      \
          "received {cksum}", expected, received)
#define TRACE_PKINIT_CLIENT_REP_DH(c)                   \
    TRACE(c, "PKINIT client verified DH reply")
#define TRACE_PKINIT_CLIENT_REP_DH_FAIL(c)              \
    TRACE(c, "PKINIT client could not verify DH reply")
#define TRACE_PKINIT_CLIENT_REP_RSA(c)                  \
    TRACE(c, "PKINIT client verified RSA reply")
#define TRACE_PKINIT_CLIENT_REP_RSA_KEY(c, keyblock, cksum)             \
    TRACE(c, "PKINIT client retrieved reply key {keyblock} from RSA "   \
          "reply (checksum {cksum})", keyblock, cksum)
#define TRACE_PKINIT_CLIENT_REP_RSA_FAIL(c)                     \
    TRACE(c, "PKINIT client could not verify RSA reply")
#define TRACE_PKINIT_CLIENT_REQ_CHECKSUM(c, cksum)                      \
    TRACE(c, "PKINIT client computed kdc-req-body checksum {cksum}", cksum)
#define TRACE_PKINIT_CLIENT_REQ_DH(c)                           \
    TRACE(c, "PKINIT client making DH request")
#define TRACE_PKINIT_CLIENT_REQ_RSA(c)                  \
    TRACE(c, "PKINIT client making RSA request")
#define TRACE_PKINIT_CLIENT_SAN_CONFIG_DNSNAME(c, host)                 \
    TRACE(c, "PKINIT client config accepts KDC dNSName SAN {string}", host)
#define TRACE_PKINIT_CLIENT_SAN_MATCH_DNSNAME(c, host)                  \
    TRACE(c, "PKINIT client matched KDC hostname {string} against "     \
          "dNSName SAN; EKU check still required", host)
#define TRACE_PKINIT_CLIENT_SAN_MATCH_NONE(c)                           \
    TRACE(c, "PKINIT client found no acceptable SAN in KDC cert")
#define TRACE_PKINIT_CLIENT_SAN_MATCH_PRINC(c, princ)                   \
    TRACE(c, "PKINIT client matched KDC principal {princ} against "     \
          "id-pkinit-san; no EKU check required", princ)
#define TRACE_PKINIT_CLIENT_SAN_ERR(c)                          \
    TRACE(c, "PKINIT client failed to decode SANs in KDC cert")
#define TRACE_PKINIT_CLIENT_SAN_KDCCERT_DNSNAME(c, host)                \
    TRACE(c, "PKINIT client found dNSName SAN in KDC cert: {string}", host)
#define TRACE_PKINIT_CLIENT_SAN_KDCCERT_PRINC(c, princ)                 \
    TRACE(c, "PKINIT client found id-pkinit-san in KDC cert: {princ}", princ)
#define TRACE_PKINIT_CLIENT_TRYAGAIN(c)                                 \
    TRACE(c, "PKINIT client trying again with KDC-provided parameters")

#define TRACE_PKINIT_OPENSSL_ERROR(c, msg)              \
    TRACE(c, "PKINIT OpenSSL error: {str}", msg)

#endif /* PKINIT_TRACE_H */
