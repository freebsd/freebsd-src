/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Gordon Tetlow <gordon@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Compatibility symbols for consumers linked against libcrypto.so.35 from
 * before c01c0a27cb82, which stopped exporting 52 names that upstream
 * OpenSSL does not export.  The old libssl.so.35 imports some of them.
 *
 * Each name is provided here as a non-default version, so that existing
 * binaries and libraries still resolve it but newly linked ones cannot.
 * Remove this file when SHLIB_MAJOR is next bumped.
 */

#include <sys/cdefs.h>

#include <openssl/asn1.h>
#include <openssl/crmf.h>

#include "internal/cryptlib.h"
#include "internal/packet_quic.h"
#include "internal/ssl3_cbc.h"
#include "internal/thread_arch.h"
#include "crypto/cmp/cmp_local.h"
#include "crypto/x509/x509_acert.h"

/* Defined in crypto/crmf/crmf_asn.c, but not declared in any header. */
DECLARE_ASN1_ITEM(OSSL_CRMF_ATTRIBUTETYPEANDVALUE)
OSSL_CRMF_ATTRIBUTETYPEANDVALUE *OSSL_CRMF_ATTRIBUTETYPEANDVALUE_new(void);

#define	COMPAT_IT(name)							\
const ASN1_ITEM *__compat_##name##_it(void);				\
const ASN1_ITEM *							\
__compat_##name##_it(void)						\
{									\
									\
	return (name##_it());						\
}									\
__sym_compat(name##_it, __compat_##name##_it, OPENSSL_3_5_0)

#define	COMPAT_NEW(name)						\
name *__compat_##name##_new(void);					\
name *									\
__compat_##name##_new(void)						\
{									\
									\
	return (name##_new());						\
}									\
__sym_compat(name##_new, __compat_##name##_new, OPENSSL_3_5_0)

#define	COMPAT_FREE(name)						\
void __compat_##name##_free(name *);					\
void									\
__compat_##name##_free(name *a)						\
{									\
									\
	name##_free(a);							\
}									\
__sym_compat(name##_free, __compat_##name##_free, OPENSSL_3_5_0)

#define	COMPAT_ENCODE(name)						\
name *__compat_d2i_##name(name **, const unsigned char **, long);	\
name *									\
__compat_d2i_##name(name **a, const unsigned char **in, long len)	\
{									\
									\
	return (d2i_##name(a, in, len));				\
}									\
__sym_compat(d2i_##name, __compat_d2i_##name, OPENSSL_3_5_0);		\
int __compat_i2d_##name(const name *, unsigned char **);		\
int									\
__compat_i2d_##name(const name *a, unsigned char **out)			\
{									\
									\
	return (i2d_##name(a, out));					\
}									\
__sym_compat(i2d_##name, __compat_i2d_##name, OPENSSL_3_5_0)

/* crypto/cmp */
COMPAT_IT(OSSL_CMP_CERTREQTEMPLATE);
COMPAT_NEW(OSSL_CMP_CERTREQTEMPLATE);
COMPAT_FREE(OSSL_CMP_CERTREQTEMPLATE);
COMPAT_ENCODE(OSSL_CMP_CERTREQTEMPLATE);
COMPAT_IT(OSSL_CMP_CRLSOURCE);
COMPAT_NEW(OSSL_CMP_CRLSOURCE);
COMPAT_FREE(OSSL_CMP_CRLSOURCE);
COMPAT_ENCODE(OSSL_CMP_CRLSOURCE);
COMPAT_IT(OSSL_CMP_CRLSTATUS);
COMPAT_NEW(OSSL_CMP_CRLSTATUS);
COMPAT_ENCODE(OSSL_CMP_CRLSTATUS);
COMPAT_IT(OSSL_CMP_ROOTCAKEYUPDATE);
COMPAT_NEW(OSSL_CMP_ROOTCAKEYUPDATE);
COMPAT_FREE(OSSL_CMP_ROOTCAKEYUPDATE);
COMPAT_ENCODE(OSSL_CMP_ROOTCAKEYUPDATE);

/* crypto/crmf */
COMPAT_IT(OSSL_CRMF_ATTRIBUTETYPEANDVALUE);
COMPAT_NEW(OSSL_CRMF_ATTRIBUTETYPEANDVALUE);

/* crypto/x509 */
COMPAT_IT(OSSL_ISSUER_SERIAL);
COMPAT_IT(OSSL_OBJECT_DIGEST_INFO);
COMPAT_IT(X509_ACERT_ISSUER);
COMPAT_IT(X509_ACERT_ISSUER_V2FORM);
COMPAT_IT(X509_HOLDER);

/* crypto/cryptlib.c */
void __compat_OPENSSL_cpuid_setup(void);

void
__compat_OPENSSL_cpuid_setup(void)
{

	OPENSSL_cpuid_setup();
}
__sym_compat(OPENSSL_cpuid_setup, __compat_OPENSSL_cpuid_setup, OPENSSL_3_0_9);

/* crypto/packet.c */
#ifndef OPENSSL_NO_QUIC
int __compat_WPACKET_quic_sub_allocate_bytes(WPACKET *, size_t,
    unsigned char **);
int __compat_WPACKET_quic_write_vlint(WPACKET *, uint64_t);
int __compat_WPACKET_start_quic_sub_packet(WPACKET *);
int __compat_WPACKET_start_quic_sub_packet_bound(WPACKET *, size_t);

int
__compat_WPACKET_quic_sub_allocate_bytes(WPACKET *pkt, size_t len,
    unsigned char **bytes)
{

	return (WPACKET_quic_sub_allocate_bytes(pkt, len, bytes));
}
__sym_compat(WPACKET_quic_sub_allocate_bytes,
    __compat_WPACKET_quic_sub_allocate_bytes, OPENSSL_3_5_0);

int
__compat_WPACKET_quic_write_vlint(WPACKET *pkt, uint64_t v)
{

	return (WPACKET_quic_write_vlint(pkt, v));
}
__sym_compat(WPACKET_quic_write_vlint, __compat_WPACKET_quic_write_vlint,
    OPENSSL_3_5_0);

int
__compat_WPACKET_start_quic_sub_packet(WPACKET *pkt)
{

	return (WPACKET_start_quic_sub_packet(pkt));
}
__sym_compat(WPACKET_start_quic_sub_packet,
    __compat_WPACKET_start_quic_sub_packet, OPENSSL_3_5_0);

int
__compat_WPACKET_start_quic_sub_packet_bound(WPACKET *pkt, size_t max_len)
{

	return (WPACKET_start_quic_sub_packet_bound(pkt, max_len));
}
__sym_compat(WPACKET_start_quic_sub_packet_bound,
    __compat_WPACKET_start_quic_sub_packet_bound, OPENSSL_3_5_0);
#endif /* !OPENSSL_NO_QUIC */

/* crypto/thread/arch */
CRYPTO_MUTEX *__compat_ossl_crypto_mutex_new(void);
void __compat_ossl_crypto_mutex_lock(CRYPTO_MUTEX *);
int __compat_ossl_crypto_mutex_try_lock(CRYPTO_MUTEX *);
void __compat_ossl_crypto_mutex_unlock(CRYPTO_MUTEX *);
void __compat_ossl_crypto_mutex_free(CRYPTO_MUTEX **);
CRYPTO_CONDVAR *__compat_ossl_crypto_condvar_new(void);
void __compat_ossl_crypto_condvar_wait(CRYPTO_CONDVAR *, CRYPTO_MUTEX *);
void __compat_ossl_crypto_condvar_wait_timeout(CRYPTO_CONDVAR *,
    CRYPTO_MUTEX *, OSSL_TIME);
void __compat_ossl_crypto_condvar_broadcast(CRYPTO_CONDVAR *);
void __compat_ossl_crypto_condvar_signal(CRYPTO_CONDVAR *);
void __compat_ossl_crypto_condvar_free(CRYPTO_CONDVAR **);
CRYPTO_THREAD *__compat_ossl_crypto_thread_native_start(CRYPTO_THREAD_ROUTINE,
    void *, int);
int __compat_ossl_crypto_thread_native_spawn(CRYPTO_THREAD *);
int __compat_ossl_crypto_thread_native_join(CRYPTO_THREAD *,
    CRYPTO_THREAD_RETVAL *);
int __compat_ossl_crypto_thread_native_perform_join(CRYPTO_THREAD *,
    CRYPTO_THREAD_RETVAL *);
int __compat_ossl_crypto_thread_native_exit(void);
int __compat_ossl_crypto_thread_native_is_self(CRYPTO_THREAD *);
int __compat_ossl_crypto_thread_native_clean(CRYPTO_THREAD *);

CRYPTO_MUTEX *
__compat_ossl_crypto_mutex_new(void)
{

	return (ossl_crypto_mutex_new());
}
__sym_compat(ossl_crypto_mutex_new, __compat_ossl_crypto_mutex_new,
    OPENSSL_3_5_0);

void
__compat_ossl_crypto_mutex_lock(CRYPTO_MUTEX *mutex)
{

	ossl_crypto_mutex_lock(mutex);
}
__sym_compat(ossl_crypto_mutex_lock, __compat_ossl_crypto_mutex_lock,
    OPENSSL_3_5_0);

int
__compat_ossl_crypto_mutex_try_lock(CRYPTO_MUTEX *mutex)
{

	return (ossl_crypto_mutex_try_lock(mutex));
}
__sym_compat(ossl_crypto_mutex_try_lock, __compat_ossl_crypto_mutex_try_lock,
    OPENSSL_3_5_0);

void
__compat_ossl_crypto_mutex_unlock(CRYPTO_MUTEX *mutex)
{

	ossl_crypto_mutex_unlock(mutex);
}
__sym_compat(ossl_crypto_mutex_unlock, __compat_ossl_crypto_mutex_unlock,
    OPENSSL_3_5_0);

void
__compat_ossl_crypto_mutex_free(CRYPTO_MUTEX **mutex)
{

	ossl_crypto_mutex_free(mutex);
}
__sym_compat(ossl_crypto_mutex_free, __compat_ossl_crypto_mutex_free,
    OPENSSL_3_5_0);

CRYPTO_CONDVAR *
__compat_ossl_crypto_condvar_new(void)
{

	return (ossl_crypto_condvar_new());
}
__sym_compat(ossl_crypto_condvar_new, __compat_ossl_crypto_condvar_new,
    OPENSSL_3_5_0);

void
__compat_ossl_crypto_condvar_wait(CRYPTO_CONDVAR *cv, CRYPTO_MUTEX *mutex)
{

	ossl_crypto_condvar_wait(cv, mutex);
}
__sym_compat(ossl_crypto_condvar_wait, __compat_ossl_crypto_condvar_wait,
    OPENSSL_3_5_0);

void
__compat_ossl_crypto_condvar_wait_timeout(CRYPTO_CONDVAR *cv,
    CRYPTO_MUTEX *mutex, OSSL_TIME deadline)
{

	ossl_crypto_condvar_wait_timeout(cv, mutex, deadline);
}
__sym_compat(ossl_crypto_condvar_wait_timeout,
    __compat_ossl_crypto_condvar_wait_timeout, OPENSSL_3_5_0);

void
__compat_ossl_crypto_condvar_broadcast(CRYPTO_CONDVAR *cv)
{

	ossl_crypto_condvar_broadcast(cv);
}
__sym_compat(ossl_crypto_condvar_broadcast,
    __compat_ossl_crypto_condvar_broadcast, OPENSSL_3_5_0);

void
__compat_ossl_crypto_condvar_signal(CRYPTO_CONDVAR *cv)
{

	ossl_crypto_condvar_signal(cv);
}
__sym_compat(ossl_crypto_condvar_signal, __compat_ossl_crypto_condvar_signal,
    OPENSSL_3_5_0);

void
__compat_ossl_crypto_condvar_free(CRYPTO_CONDVAR **cv)
{

	ossl_crypto_condvar_free(cv);
}
__sym_compat(ossl_crypto_condvar_free, __compat_ossl_crypto_condvar_free,
    OPENSSL_3_5_0);

CRYPTO_THREAD *
__compat_ossl_crypto_thread_native_start(CRYPTO_THREAD_ROUTINE routine,
    void *data, int joinable)
{

	return (ossl_crypto_thread_native_start(routine, data, joinable));
}
__sym_compat(ossl_crypto_thread_native_start,
    __compat_ossl_crypto_thread_native_start, OPENSSL_3_5_0);

int
__compat_ossl_crypto_thread_native_spawn(CRYPTO_THREAD *thread)
{

	return (ossl_crypto_thread_native_spawn(thread));
}
__sym_compat(ossl_crypto_thread_native_spawn,
    __compat_ossl_crypto_thread_native_spawn, OPENSSL_3_5_0);

int
__compat_ossl_crypto_thread_native_join(CRYPTO_THREAD *thread,
    CRYPTO_THREAD_RETVAL *retval)
{

	return (ossl_crypto_thread_native_join(thread, retval));
}
__sym_compat(ossl_crypto_thread_native_join,
    __compat_ossl_crypto_thread_native_join, OPENSSL_3_5_0);

int
__compat_ossl_crypto_thread_native_perform_join(CRYPTO_THREAD *thread,
    CRYPTO_THREAD_RETVAL *retval)
{

	return (ossl_crypto_thread_native_perform_join(thread, retval));
}
__sym_compat(ossl_crypto_thread_native_perform_join,
    __compat_ossl_crypto_thread_native_perform_join, OPENSSL_3_5_0);

int
__compat_ossl_crypto_thread_native_exit(void)
{

	return (ossl_crypto_thread_native_exit());
}
__sym_compat(ossl_crypto_thread_native_exit,
    __compat_ossl_crypto_thread_native_exit, OPENSSL_3_5_0);

int
__compat_ossl_crypto_thread_native_is_self(CRYPTO_THREAD *thread)
{

	return (ossl_crypto_thread_native_is_self(thread));
}
__sym_compat(ossl_crypto_thread_native_is_self,
    __compat_ossl_crypto_thread_native_is_self, OPENSSL_3_5_0);

int
__compat_ossl_crypto_thread_native_clean(CRYPTO_THREAD *thread)
{

	return (ossl_crypto_thread_native_clean(thread));
}
__sym_compat(ossl_crypto_thread_native_clean,
    __compat_ossl_crypto_thread_native_clean, OPENSSL_3_5_0);

/* ssl/record/methods/ssl3_cbc.c, tls_pad.c */
int __compat_ssl3_cbc_digest_record(const EVP_MD *, unsigned char *,
    size_t *, const unsigned char *, const unsigned char *, size_t, size_t,
    const unsigned char *, size_t, char);
int __compat_ssl3_cbc_remove_padding_and_mac(size_t *, size_t,
    unsigned char *, unsigned char **, int *, size_t, size_t,
    OSSL_LIB_CTX *);
int __compat_tls1_cbc_remove_padding_and_mac(size_t *, size_t,
    unsigned char *, unsigned char **, int *, size_t, size_t, int,
    OSSL_LIB_CTX *);

int
__compat_ssl3_cbc_digest_record(const EVP_MD *md, unsigned char *md_out,
    size_t *md_out_size, const unsigned char *header,
    const unsigned char *data, size_t data_size,
    size_t data_plus_mac_plus_padding_size, const unsigned char *mac_secret,
    size_t mac_secret_length, char is_sslv3)
{

	return (ssl3_cbc_digest_record(md, md_out, md_out_size, header, data,
	    data_size, data_plus_mac_plus_padding_size, mac_secret,
	    mac_secret_length, is_sslv3));
}
__sym_compat(ssl3_cbc_digest_record, __compat_ssl3_cbc_digest_record,
    OPENSSL_3_5_0);

int
__compat_ssl3_cbc_remove_padding_and_mac(size_t *reclen, size_t origreclen,
    unsigned char *recdata, unsigned char **mac, int *alloced,
    size_t block_size, size_t mac_size, OSSL_LIB_CTX *libctx)
{

	return (ssl3_cbc_remove_padding_and_mac(reclen, origreclen, recdata,
	    mac, alloced, block_size, mac_size, libctx));
}
__sym_compat(ssl3_cbc_remove_padding_and_mac,
    __compat_ssl3_cbc_remove_padding_and_mac, OPENSSL_3_5_0);

int
__compat_tls1_cbc_remove_padding_and_mac(size_t *reclen, size_t origreclen,
    unsigned char *recdata, unsigned char **mac, int *alloced,
    size_t block_size, size_t mac_size, int aead, OSSL_LIB_CTX *libctx)
{

	return (tls1_cbc_remove_padding_and_mac(reclen, origreclen, recdata,
	    mac, alloced, block_size, mac_size, aead, libctx));
}
__sym_compat(tls1_cbc_remove_padding_and_mac,
    __compat_tls1_cbc_remove_padding_and_mac, OPENSSL_3_5_0);
