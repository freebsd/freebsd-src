/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-trace.h */
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

/*
 * This header contains trace macro definitions, which map trace points within
 * the code to krb5int_trace() calls with descriptive text strings.
 *
 * A new trace macro must be defined in this file for each new location to
 * be traced; the TRACE() macro should never be used directly.  This keeps
 * the tracing logic centralized in one place, to facilitate integration with
 * alternate tracing backends such as DTrace.
 *
 * Trace logging is intended to aid power users in diagnosing configuration
 * problems by showing what's going on behind the scenes of complex operations.
 * Although trace logging is sometimes useful to developers, it is not intended
 * as a replacement for a debugger, and it is not desirable to drown the user
 * in output.  Observe the following guidelines when adding trace points:
 *
 *   - Avoid mentioning function or variable names in messages.
 *
 *   - Try to convey what decisions are being made and what external inputs
 *     they are based on, not the process of making decisions.
 *
 *   - It is generally not necessary to trace before returning an unrecoverable
 *     error.  If an error code is unclear by itself, make it clearer with
 *     krb5_set_error_message().
 *
 *   - Keep macros simple.  Add format specifiers to krb5int_trace's formatter
 *     as necessary (and document them here) instead of transforming macro
 *     arguments.
 *
 *   - Like printf, the trace formatter interface is not type-safe.  Check your
 *     formats carefully.  Cast integral arguments to the appropriate type if
 *     they do not already patch.
 *
 * The following specifiers are supported by the formatter (see the
 * implementation in lib/krb5/os/trace.c for details):
 *
 *   {int}         int, in decimal
 *   {long}        long, in decimal
 *   {str}         const char *, display as C string
 *   {lenstr}      size_t and const char *, as a counted string
 *   {hexlenstr}   size_t and const char *, as hex bytes
 *   {hashlenstr}  size_t and const char *, as four-character hex hash
 *   {raddr}       struct remote_address *, show socket type, address, port
 *   {data}        krb5_data *, display as counted string
 *   {hexdata}     krb5_data *, display as hex bytes
 *   {errno}       int, display as number/errorstring
 *   {kerr}        krb5_error_code, display as number/errorstring
 *   {keyblock}    const krb5_keyblock *, display enctype and hash of key
 *   {key}         krb5_key, display enctype and hash of key
 *   {cksum}       const krb5_checksum *, display cksumtype and hex checksum
 *   {princ}       krb5_principal, unparse and display
 *   {ptype}       krb5_int32, krb5_principal type, display name
 *   {patype}      krb5_preauthtype, a single padata type number
 *   {patypes}     krb5_pa_data **, display list of padata type numbers
 *   {etype}       krb5_enctype, display shortest name of enctype
 *   {etypes}      krb5_enctype *, display list of enctypes
 *   {ccache}      krb5_ccache, display type:name
 *   {keytab}      krb5_keytab, display name
 *   {creds}       krb5_creds *, display clientprinc -> serverprinc
 */

#ifndef K5_TRACE_H
#define K5_TRACE_H

#ifdef DISABLE_TRACING
#define TRACE(ctx, ...)
#else

void krb5int_trace(krb5_context context, const char *fmt, ...);

/* Try to optimize away argument evaluation and function call when we're not
 * tracing, if this source file knows the internals of the context. */
#ifdef _KRB5_INT_H
#define TRACE(ctx, ...)                                        \
    do { if (ctx->trace_callback != NULL)                      \
            krb5int_trace(ctx, __VA_ARGS__); } while (0)
#else
#define TRACE(ctx, ...) krb5int_trace(ctx, __VA_ARGS__)
#endif

#endif /* DISABLE_TRACING */

#define TRACE_CC_CACHE_MATCH(c, princ, ret)                             \
    TRACE(c, "Matching {princ} in collection with result: {kerr}",      \
          princ, ret)
#define TRACE_CC_DESTROY(c, cache)                      \
    TRACE(c, "Destroying ccache {ccache}", cache)
#define TRACE_CC_GEN_NEW(c, cache)                                      \
    TRACE(c, "Generating new unique ccache based on {ccache}", cache)
#define TRACE_CC_GET_CONFIG(c, cache, princ, key, data)             \
    TRACE(c, "Read config in {ccache} for {princ}: {str}: {data}",  \
          cache, princ, key, data)
#define TRACE_CC_INIT(c, cache, princ)                              \
    TRACE(c, "Initializing {ccache} with default princ {princ}",    \
          cache, princ)
#define TRACE_CC_MOVE(c, src, dst)                                      \
    TRACE(c, "Moving ccache {ccache} to {ccache}", src, dst)
#define TRACE_CC_NEW_UNIQUE(c, type)                            \
    TRACE(c, "Resolving unique ccache of type {str}", type)
#define TRACE_CC_REMOVE(c, cache, creds)                        \
    TRACE(c, "Removing {creds} from {ccache}", creds, cache)
#define TRACE_CC_RETRIEVE(c, cache, creds, ret)                      \
    TRACE(c, "Retrieving {creds} from {ccache} with result: {kerr}", \
              creds, cache, ret)
#define TRACE_CC_RETRIEVE_REF(c, cache, creds, ret)                     \
    TRACE(c, "Retrying {creds} with result: {kerr}", creds, ret)
#define TRACE_CC_SET_CONFIG(c, cache, princ, key, data)               \
    TRACE(c, "Storing config in {ccache} for {princ}: {str}: {data}", \
          cache, princ, key, data)
#define TRACE_CC_STORE(c, cache, creds)                         \
    TRACE(c, "Storing {creds} in {ccache}", creds, cache)
#define TRACE_CC_STORE_TKT(c, cache, creds)                     \
    TRACE(c, "Also storing {creds} based on ticket", creds)

#define TRACE_CCSELECT_VTINIT_FAIL(c, ret)                              \
    TRACE(c, "ccselect module failed to init vtable: {kerr}", ret)
#define TRACE_CCSELECT_INIT_FAIL(c, name, ret)                          \
    TRACE(c, "ccselect module {str} failed to init: {kerr}", name, ret)
#define TRACE_CCSELECT_MODCHOICE(c, name, server, cache, princ)         \
    TRACE(c, "ccselect module {str} chose cache {ccache} with client "  \
          "principal {princ} for server principal {princ}", name, cache, \
          princ, server)
#define TRACE_CCSELECT_MODNOTFOUND(c, name, server, princ)              \
    TRACE(c, "ccselect module {str} chose client principal {princ} "    \
          "for server principal {princ} but found no cache", name, princ, \
          server)
#define TRACE_CCSELECT_MODFAIL(c, name, ret, server)                  \
    TRACE(c, "ccselect module {str} yielded error {kerr} for server " \
          "principal {princ}", name, ret, server)
#define TRACE_CCSELECT_NOTFOUND(c, server)                          \
    TRACE(c, "ccselect can't find appropriate cache for server "    \
          "principal {princ}", server)
#define TRACE_CCSELECT_DEFAULT(c, cache, server)                    \
    TRACE(c, "ccselect choosing default cache {ccache} for server " \
          "principal {princ}", cache, server)

#define TRACE_DNS_SRV_ANS(c, host, port, prio, weight)                \
    TRACE(c, "SRV answer: {int} {int} {int} \"{str}\"", prio, weight, \
          port, host)
#define TRACE_DNS_SRV_NOTFOUND(c)               \
    TRACE(c, "No SRV records found")
#define TRACE_DNS_SRV_SEND(c, domain)                   \
    TRACE(c, "Sending DNS SRV query for {str}", domain)
#define TRACE_DNS_URI_ANS(c, uri, prio, weight)                         \
    TRACE(c, "URI answer: {int} {int} \"{str}\"", prio, weight, uri)
#define TRACE_DNS_URI_NOTFOUND(c)               \
    TRACE(c, "No URI records found")
#define TRACE_DNS_URI_SEND(c, domain)                   \
    TRACE(c, "Sending DNS URI query for {str}", domain)

#define TRACE_FAST_ARMOR_CCACHE(c, ccache_name)         \
    TRACE(c, "FAST armor ccache: {str}", ccache_name)
#define TRACE_FAST_ARMOR_CCACHE_KEY(c, keyblock)                \
    TRACE(c, "Armor ccache session key: {keyblock}", keyblock)
#define TRACE_FAST_ARMOR_KEY(c, keyblock)               \
    TRACE(c, "FAST armor key: {keyblock}", keyblock)
#define TRACE_FAST_CCACHE_CONFIG(c)                                     \
    TRACE(c, "Using FAST due to armor ccache negotiation result")
#define TRACE_FAST_DECODE(c)                    \
    TRACE(c, "Decoding FAST response")
#define TRACE_FAST_ENCODE(c)                                            \
    TRACE(c, "Encoding request body and padata into FAST request")
#define TRACE_FAST_NEGO(c, avail)                                       \
    TRACE(c, "FAST negotiation: {str}available", (avail) ? "" : "un")
#define TRACE_FAST_PADATA_UPGRADE(c)                                    \
    TRACE(c, "Upgrading to FAST due to presence of PA_FX_FAST in reply")
#define TRACE_FAST_REPLY_KEY(c, keyblock)                       \
    TRACE(c, "FAST reply key: {keyblock}", keyblock)
#define TRACE_FAST_REQUIRED(c)                                  \
    TRACE(c, "Using FAST due to KRB5_FAST_REQUIRED flag")

#define TRACE_GET_CREDS_FALLBACK(c, hostname)                           \
    TRACE(c, "Falling back to canonicalized server hostname {str}", hostname)

#define TRACE_GIC_PWD_CHANGED(c)                                \
    TRACE(c, "Getting initial TGT with changed password")
#define TRACE_GIC_PWD_CHANGEPW(c, tries)                                \
    TRACE(c, "Attempting password change; {int} tries remaining", tries)
#define TRACE_GIC_PWD_EXPIRED(c)                                \
    TRACE(c, "Principal expired; getting changepw ticket")
#define TRACE_GIC_PWD_PRIMARY(c)                        \
    TRACE(c, "Retrying AS request with primary KDC")

#define TRACE_GSS_CLIENT_KEYTAB_FAIL(c, ret)                            \
    TRACE(c, "Unable to resolve default client keytab: {kerr}", ret)

#define TRACE_ENCTYPE_LIST_UNKNOWN(c, profvar, name)                    \
    TRACE(c, "Unrecognized enctype name in {str}: {str}", profvar, name)

#define TRACE_HOSTREALM_VTINIT_FAIL(c, ret)                             \
    TRACE(c, "hostrealm module failed to init vtable: {kerr}", ret)
#define TRACE_HOSTREALM_INIT_FAIL(c, name, ret)                         \
    TRACE(c, "hostrealm module {str} failed to init: {kerr}", name, ret)

#define TRACE_INIT_CREDS(c, princ)                              \
    TRACE(c, "Getting initial credentials for {princ}", princ)
#define TRACE_INIT_CREDS_AS_KEY_GAK(c, keyblock)                        \
    TRACE(c, "AS key obtained from gak_fct: {keyblock}", keyblock)
#define TRACE_INIT_CREDS_AS_KEY_PREAUTH(c, keyblock)                    \
    TRACE(c, "AS key determined by preauth: {keyblock}", keyblock)
#define TRACE_INIT_CREDS_DECRYPTED_REPLY(c, keyblock)                   \
    TRACE(c, "Decrypted AS reply; session key is: {keyblock}", keyblock)
#define TRACE_INIT_CREDS_ERROR_REPLY(c, code)           \
    TRACE(c, "Received error from KDC: {kerr}", code)
#define TRACE_INIT_CREDS_GAK(c, salt, s2kparams)                    \
    TRACE(c, "Getting AS key, salt \"{data}\", params \"{data}\"",  \
          salt, s2kparams)
#define TRACE_INIT_CREDS_IDENTIFIED_REALM(c, realm)                     \
    TRACE(c, "Identified realm of client principal as {data}", realm)
#define TRACE_INIT_CREDS_KEYTAB_LOOKUP(c, princ, etypes)                \
    TRACE(c, "Found entries for {princ} in keytab: {etypes}", princ, etypes)
#define TRACE_INIT_CREDS_KEYTAB_LOOKUP_FAILED(c, code)          \
    TRACE(c, "Couldn't lookup etypes in keytab: {kerr}", code)
#define TRACE_INIT_CREDS_PREAUTH(c)                     \
    TRACE(c, "Preauthenticating using KDC method data")
#define TRACE_INIT_CREDS_PREAUTH_DECRYPT_FAIL(c, code)                  \
    TRACE(c, "Decrypt with preauth AS key failed: {kerr}", code)
#define TRACE_INIT_CREDS_PREAUTH_MORE(c, patype)                \
    TRACE(c, "Continuing preauth mech {patype}", patype)
#define TRACE_INIT_CREDS_PREAUTH_NONE(c)        \
    TRACE(c, "Sending unauthenticated request")
#define TRACE_INIT_CREDS_PREAUTH_OPTIMISTIC(c)  \
    TRACE(c, "Attempting optimistic preauth")
#define TRACE_INIT_CREDS_PREAUTH_TRYAGAIN(c, patype, code)              \
    TRACE(c, "Recovering from KDC error {int} using preauth mech {patype}", \
          patype, (int)code)
#define TRACE_INIT_CREDS_RESTART_FAST(c)        \
    TRACE(c, "Restarting to upgrade to FAST")
#define TRACE_INIT_CREDS_RESTART_PREAUTH_FAILED(c)                      \
    TRACE(c, "Restarting due to PREAUTH_FAILED from FAST negotiation")
#define TRACE_INIT_CREDS_REFERRAL(c, realm)                     \
    TRACE(c, "Following referral to realm {data}", realm)
#define TRACE_INIT_CREDS_RETRY_TCP(c)                                   \
    TRACE(c, "Request or response is too big for UDP; retrying with TCP")
#define TRACE_INIT_CREDS_SALT_PRINC(c, salt)                    \
    TRACE(c, "Salt derived from principal: {data}", salt)
#define TRACE_INIT_CREDS_SERVICE(c, service)                    \
    TRACE(c, "Setting initial creds service to {str}", service)

#define TRACE_KADM5_AUTH_VTINIT_FAIL(c, ret)                            \
    TRACE(c, "kadm5_auth module failed to init vtable: {kerr}", ret)
#define TRACE_KADM5_AUTH_INIT_FAIL(c, name, ret)                        \
    TRACE(c, "kadm5_auth module {str} failed to init: {kerr}", ret)
#define TRACE_KADM5_AUTH_INIT_SKIP(c, name)                             \
    TRACE(c, "kadm5_auth module {str} declined to initialize", name)

#define TRACE_KT_GET_ENTRY(c, keytab, princ, vno, enctype, err)         \
    TRACE(c, "Retrieving {princ} from {keytab} (vno {int}, enctype {etype}) " \
          "with result: {kerr}", princ, keytab, (int) vno, enctype, err)

#define TRACE_LOCALAUTH_INIT_CONFLICT(c, type, oldname, newname)        \
    TRACE(c, "Ignoring localauth module {str} because it conflicts "    \
          "with an2ln type {str} from module {str}", newname, type, oldname)
#define TRACE_LOCALAUTH_VTINIT_FAIL(c, ret)                             \
    TRACE(c, "localauth module failed to init vtable: {kerr}", ret)
#define TRACE_LOCALAUTH_INIT_FAIL(c, name, ret)                         \
    TRACE(c, "localauth module {str} failed to init: {kerr}", name, ret)

#define TRACE_MK_REP(c, ctime, cusec, subkey, seqnum)                   \
    TRACE(c, "Creating AP-REP, time {long}.{int}, subkey {keyblock}, "  \
          "seqnum {int}", (long) ctime, (int) cusec, subkey, (int) seqnum)

#define TRACE_MK_REQ(c, creds, seqnum, subkey, sesskeyblock)            \
    TRACE(c, "Creating authenticator for {creds}, seqnum {int}, "       \
          "subkey {key}, session key {keyblock}", creds, (int) seqnum,  \
          subkey, sesskeyblock)
#define TRACE_MK_REQ_ETYPES(c, etypes)                                  \
    TRACE(c, "Negotiating for enctypes in authenticator: {etypes}", etypes)

#define TRACE_MSPAC_VERIFY_FAIL(c, err)                         \
    TRACE(c, "PAC checksum verification failed: {kerr}", err)
#define TRACE_MSPAC_DISCARD_UNVERF(c)           \
    TRACE(c, "Filtering out unverified MS PAC")

#define TRACE_NEGOEX_INCOMING(c, seqnum, typestr, info)                 \
    TRACE(c, "NegoEx received [{int}]{str}: {str}", (int)seqnum, typestr, info)
#define TRACE_NEGOEX_OUTGOING(c, seqnum, typestr, info)                 \
    TRACE(c, "NegoEx sending [{int}]{str}: {str}", (int)seqnum, typestr, info)

#define TRACE_PLUGIN_LOAD_FAIL(c, modname, err)                         \
    TRACE(c, "Error loading plugin module {str}: {kerr}", modname, err)
#define TRACE_PLUGIN_LOOKUP_FAIL(c, modname, err)                       \
    TRACE(c, "Error initializing module {str}: {kerr}", modname, err)

#define TRACE_PREAUTH_CONFLICT(c, name1, name2, patype)                 \
    TRACE(c, "Preauth module {str} conflicts with module {str} for pa " \
          "type {patype}", name1, name2, patype)
#define TRACE_PREAUTH_COOKIE(c, len, data)                      \
    TRACE(c, "Received cookie: {lenstr}", (size_t) len, data)
#define TRACE_PREAUTH_ENC_TS_KEY_GAK(c, keyblock)                       \
    TRACE(c, "AS key obtained for encrypted timestamp: {keyblock}", keyblock)
#define TRACE_PREAUTH_ENC_TS(c, sec, usec, plain, enc)                  \
    TRACE(c, "Encrypted timestamp (for {long}.{int}): plain {hexdata}, " \
          "encrypted {hexdata}", (long) sec, (int) usec, plain, enc)
#define TRACE_PREAUTH_ENC_TS_DISABLED(c)                                \
    TRACE(c, "Ignoring encrypted timestamp because it is disabled")
#define TRACE_PREAUTH_ETYPE_INFO(c, etype, salt, s2kparams)          \
    TRACE(c, "Selected etype info: etype {etype}, salt \"{data}\", " \
          "params \"{data}\"", etype, salt, s2kparams)
#define TRACE_PREAUTH_INFO_FAIL(c, patype, code)                        \
    TRACE(c, "Preauth builtin info function failure, type={patype}: {kerr}", \
          patype, code)
#define TRACE_PREAUTH_INPUT(c, padata)                          \
    TRACE(c, "Processing preauth types: {patypes}", padata)
#define TRACE_PREAUTH_OUTPUT(c, padata)                                 \
    TRACE(c, "Produced preauth for next request: {patypes}", padata)
#define TRACE_PREAUTH_PROCESS(c, name, patype, real, code)              \
    TRACE(c, "Preauth module {str} ({int}) ({str}) returned: "          \
          "{kerr}", name, (int) patype, real ? "real" : "info", code)
#define TRACE_PREAUTH_SAM_KEY_GAK(c, keyblock)                  \
    TRACE(c, "AS key obtained for SAM: {keyblock}", keyblock)
#define TRACE_PREAUTH_SALT(c, salt, patype)                          \
    TRACE(c, "Received salt \"{data}\" via padata type {patype}", salt, \
          patype)
#define TRACE_PREAUTH_SKIP(c, name, patype)                           \
    TRACE(c, "Skipping previously used preauth module {str} ({int})", \
          name, (int) patype)
#define TRACE_PREAUTH_TRYAGAIN_INPUT(c, patype, padata)                 \
    TRACE(c, "Preauth tryagain input types ({int}): {patypes}", patype, padata)
#define TRACE_PREAUTH_TRYAGAIN(c, name, patype, code)                   \
    TRACE(c, "Preauth module {str} ({int}) tryagain returned: {kerr}",  \
          name, (int)patype, code)
#define TRACE_PREAUTH_TRYAGAIN_OUTPUT(c, padata)                        \
    TRACE(c, "Followup preauth for next request: {patypes}", padata)
#define TRACE_PREAUTH_WRONG_CONTEXT(c)                                  \
    TRACE(c, "Wrong context passed to krb5_init_creds_free(); leaking " \
          "modreq objects")

#define TRACE_PROFILE_ERR(c,subsection, section, retval)             \
    TRACE(c, "Bad value of {str} from [{str}] in conf file: {kerr}", \
          subsection, section, retval)

#define TRACE_RD_REP(c, ctime, cusec, subkey, seqnum)               \
    TRACE(c, "Read AP-REP, time {long}.{int}, subkey {keyblock}, "      \
          "seqnum {int}", (long) ctime, (int) cusec, subkey, (int) seqnum)
#define TRACE_RD_REP_DCE(c, ctime, cusec, seqnum)                      \
    TRACE(c, "Read DCE-style AP-REP, time {long}.{int}, seqnum {int}", \
          (long) ctime, (int) cusec, (int) seqnum)

#define TRACE_RD_REQ_DECRYPT_ANY(c, princ, keyblock)                \
    TRACE(c, "Decrypted AP-REQ with server principal {princ}: "     \
          "{keyblock}", princ, keyblock)
#define TRACE_RD_REQ_DECRYPT_SPECIFIC(c, princ, keyblock)               \
    TRACE(c, "Decrypted AP-REQ with specified server principal {princ}: " \
          "{keyblock}", princ, keyblock)
#define TRACE_RD_REQ_DECRYPT_FAIL(c, err)                       \
    TRACE(c, "Failed to decrypt AP-REQ ticket: {kerr}", err)
#define TRACE_RD_REQ_NEGOTIATED_ETYPE(c, etype)                     \
    TRACE(c, "Negotiated enctype based on authenticator: {etype}",  \
          etype)
#define TRACE_RD_REQ_SUBKEY(c, keyblock)                                \
    TRACE(c, "Authenticator contains subkey: {keyblock}", keyblock)
#define TRACE_RD_REQ_TICKET(c, client, server, keyblock)                \
    TRACE(c, "AP-REQ ticket: {princ} -> {princ}, session key {keyblock}", \
          client, server, keyblock)

#define TRACE_SENDTO_KDC_ERROR_SET_MESSAGE(c, raddr, err)               \
    TRACE(c, "Error preparing message to send to {raddr}: {errno}",     \
          raddr, err)
#define TRACE_SENDTO_KDC(c, len, rlm, primary, tcp)                     \
    TRACE(c, "Sending request ({int} bytes) to {data}{str}{str}", len,  \
          rlm, (primary) ? " (primary)" : "", (tcp) ? " (tcp only)" : "")
#define TRACE_SENDTO_KDC_K5TLS_LOAD_ERROR(c, ret)       \
    TRACE(c, "Error loading k5tls module: {kerr}", ret)
#define TRACE_SENDTO_KDC_PRIMARY(c, primary)                            \
    TRACE(c, "Response was{str} from primary KDC", (primary) ? "" : " not")
#define TRACE_SENDTO_KDC_RESOLVING(c, hostname)         \
    TRACE(c, "Resolving hostname {str}", hostname)
#define TRACE_SENDTO_KDC_RESPONSE(c, len, raddr)                        \
    TRACE(c, "Received answer ({int} bytes) from {raddr}", len, raddr)
#define TRACE_SENDTO_KDC_HTTPS_ERROR_CONNECT(c, raddr)          \
    TRACE(c, "HTTPS error connecting to {raddr}", raddr)
#define TRACE_SENDTO_KDC_HTTPS_ERROR_RECV(c, raddr)             \
    TRACE(c, "HTTPS error receiving from {raddr}", raddr)
#define TRACE_SENDTO_KDC_HTTPS_ERROR_SEND(c, raddr)     \
    TRACE(c, "HTTPS error sending to {raddr}", raddr)
#define TRACE_SENDTO_KDC_HTTPS_SEND(c, raddr)                           \
    TRACE(c, "Sending HTTPS request to {raddr}", raddr)
#define TRACE_SENDTO_KDC_HTTPS_ERROR(c, errs)                           \
    TRACE(c, "HTTPS error: {str}", errs)
#define TRACE_SENDTO_KDC_TCP_CONNECT(c, raddr)                  \
    TRACE(c, "Initiating TCP connection to {raddr}", raddr)
#define TRACE_SENDTO_KDC_TCP_DISCONNECT(c, raddr)               \
    TRACE(c, "Terminating TCP connection to {raddr}", raddr)
#define TRACE_SENDTO_KDC_TCP_ERROR_CONNECT(c, raddr, err)               \
    TRACE(c, "TCP error connecting to {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_TCP_ERROR_RECV(c, raddr, err)                  \
    TRACE(c, "TCP error receiving from {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_TCP_ERROR_RECV_LEN(c, raddr, err)              \
    TRACE(c, "TCP error receiving from {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_TCP_ERROR_SEND(c, raddr, err)                  \
    TRACE(c, "TCP error sending to {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_TCP_SEND(c, raddr)             \
    TRACE(c, "Sending TCP request to {raddr}", raddr)
#define TRACE_SENDTO_KDC_UDP_ERROR_RECV(c, raddr, err)                  \
    TRACE(c, "UDP error receiving from {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_UDP_ERROR_SEND_INITIAL(c, raddr, err)          \
    TRACE(c, "UDP error sending to {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_UDP_ERROR_SEND_RETRY(c, raddr, err)            \
    TRACE(c, "UDP error sending to {raddr}: {errno}", raddr, err)
#define TRACE_SENDTO_KDC_UDP_SEND_INITIAL(c, raddr)             \
    TRACE(c, "Sending initial UDP request to {raddr}", raddr)
#define TRACE_SENDTO_KDC_UDP_SEND_RETRY(c, raddr)               \
    TRACE(c, "Sending retry UDP request to {raddr}", raddr)

#define TRACE_SEND_TGS_ETYPES(c, etypes)                                \
    TRACE(c, "etypes requested in TGS request: {etypes}", etypes)
#define TRACE_SEND_TGS_SUBKEY(c, keyblock)                              \
    TRACE(c, "Generated subkey for TGS request: {keyblock}", keyblock)

#define TRACE_TGS_REPLY(c, client, server, keyblock)                 \
    TRACE(c, "TGS reply is for {princ} -> {princ} with session key " \
          "{keyblock}", client, server, keyblock)
#define TRACE_TGS_REPLY_DECODE_SESSION(c, keyblock)                     \
    TRACE(c, "TGS reply didn't decode with subkey; trying session key " \
          "({keyblock)}", keyblock)

#define TRACE_TLS_ERROR(c, errs)                \
    TRACE(c, "TLS error: {str}", errs)
#define TRACE_TLS_NO_REMOTE_CERTIFICATE(c)              \
    TRACE(c, "TLS server certificate not received")
#define TRACE_TLS_CERT_ERROR(c, depth, namelen, name, err, errs)        \
    TRACE(c, "TLS certificate error at {int} ({lenstr}): {int} ({str})", \
          depth, namelen, name, err, errs)
#define TRACE_TLS_SERVER_NAME_MISMATCH(c, hostname)                     \
    TRACE(c, "TLS certificate name mismatch: server certificate is "    \
          "not for \"{str}\"", hostname)
#define TRACE_TLS_SERVER_NAME_MATCH(c, hostname)                        \
    TRACE(c, "TLS certificate name matched \"{str}\"", hostname)

#define TRACE_TKT_CREDS(c, creds, cache)                            \
    TRACE(c, "Getting credentials {creds} using ccache {ccache}",   \
          creds, cache)
#define TRACE_TKT_CREDS_ADVANCE(c, realm)                               \
    TRACE(c, "Received TGT for {data}; advancing current realm", realm)
#define TRACE_TKT_CREDS_CACHED_INTERMEDIATE_TGT(c, tgt)                 \
    TRACE(c, "Found cached TGT for intermediate realm: {creds}", tgt)
#define TRACE_TKT_CREDS_CACHED_SERVICE_TGT(c, tgt)                      \
    TRACE(c, "Found cached TGT for service realm: {creds}", tgt)
#define TRACE_TKT_CREDS_CLOSER_REALM(c, realm)                  \
    TRACE(c, "Trying next closer realm in path: {data}", realm)
#define TRACE_TKT_CREDS_COMPLETE(c, princ)                              \
    TRACE(c, "Received creds for desired service {princ}", princ)
#define TRACE_TKT_CREDS_FALLBACK(c, realm)                              \
    TRACE(c, "Local realm referral failed; trying fallback realm {data}", \
          realm)
#define TRACE_TKT_CREDS_LOCAL_TGT(c, tgt)                               \
    TRACE(c, "Starting with TGT for client realm: {creds}", tgt)
#define TRACE_TKT_CREDS_NON_TGT(c, princ)                            \
    TRACE(c, "Received non-TGT referral response ({princ}); trying " \
          "again without referrals", princ)
#define TRACE_TKT_CREDS_OFFPATH(c, realm)                       \
    TRACE(c, "Received TGT for offpath realm {data}", realm)
#define TRACE_TKT_CREDS_REFERRAL(c, princ)              \
    TRACE(c, "Following referral TGT {princ}", princ)
#define TRACE_TKT_CREDS_REFERRAL_REALM(c, princ)                        \
    TRACE(c, "Server has referral realm; starting with {princ}", princ)
#define TRACE_TKT_CREDS_RESPONSE_CODE(c, code)          \
    TRACE(c, "TGS request result: {kerr}", code)
#define TRACE_TKT_CREDS_RETRY_TCP(c)                                    \
    TRACE(c, "Request or response is too big for UDP; retrying with TCP")
#define TRACE_TKT_CREDS_SAME_REALM_TGT(c, realm)                        \
    TRACE(c, "Received TGT referral back to same realm ({data}); trying " \
          "again without referrals", realm)
#define TRACE_TKT_CREDS_SERVICE_REQ(c, princ, referral)                \
    TRACE(c, "Requesting tickets for {princ}, referrals {str}", princ, \
          (referral) ? "on" : "off")
#define TRACE_TKT_CREDS_TARGET_TGT(c, princ)                    \
    TRACE(c, "Received TGT for service realm: {princ}", princ)
#define TRACE_TKT_CREDS_TARGET_TGT_OFFPATH(c, princ)            \
    TRACE(c, "Received TGT for service realm: {princ}", princ)
#define TRACE_TKT_CREDS_TGT_REQ(c, next, cur)                           \
    TRACE(c, "Requesting TGT {princ} using TGT {princ}", next, cur)
#define TRACE_TKT_CREDS_WRONG_ENCTYPE(c)                                \
    TRACE(c, "Retrying TGS request with desired service ticket enctypes")

#define TRACE_TXT_LOOKUP_NOTFOUND(c, host)              \
    TRACE(c, "TXT record {str} not found", host)
#define TRACE_TXT_LOOKUP_SUCCESS(c, host, realm)                \
    TRACE(c, "TXT record {str} found: {str}", host, realm)

#define TRACE_CHECK_REPLY_SERVER_DIFFERS(c, request, reply) \
    TRACE(c, "Reply server {princ} differs from requested {princ}", \
          reply, request)

#define TRACE_GET_CRED_VIA_TKT_EXT(c, request, reply, kdcoptions) \
    TRACE(c, "Get cred via TGT {princ} after requesting {princ} " \
          "(canonicalize {str})", \
          reply, request, (kdcoptions & KDC_OPT_CANONICALIZE) ? "on" : "off")
#define TRACE_GET_CRED_VIA_TKT_EXT_RETURN(c, ret) \
    TRACE(c, "Got cred; {kerr}", ret)

#define TRACE_KDCPOLICY_VTINIT_FAIL(c, ret)                             \
    TRACE(c, "KDC policy module failed to init vtable: {kerr}", ret)
#define TRACE_KDCPOLICY_INIT_SKIP(c, name)                              \
    TRACE(c, "kadm5_auth module {str} declined to initialize", name)

#endif /* K5_TRACE_H */
