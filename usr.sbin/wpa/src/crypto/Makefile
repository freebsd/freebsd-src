.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpacrypto
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/crypto

.if ${MK_OPENSSL} != "no"
SRCS=	crypto_openssl.c \
	random.c \
	sha1-prf.c \
	sha256-prf.c \
	sha256-tlsprf.c \
	sha384-prf.c \
	sha384-tlsprf.c \
	sha512.c
.else
SRCS=	crypto_internal.c \
	random.c
.endif

.if defined(TLS_FUNCS)
.if defined(CONFIG_INTERNAL_TLS)
SRCS+=	crypto_internal-cipher.c \
	crypto_internal-modexp.c \
	crypto_internal-rsa.c \
	tls_internal.c
.else
SRCS+=	tls_openssl.c \
	tls_openssl_ocsp.c
.endif
.endif

.if defined(CONFIG_INTERNAL_AES)
SRCS+=  aes-unwrap.c aes-wrap.c \
        aes-internal.c \
        aes-internal-dec.c \
        aes-internal-enc.c
.else
.endif

.if defined(NEED_AES_CBC)
SRCS+=	aes-cbc.c
.endif

.if defined(NEED_AES_EAX)
SRCS+=	aes-eax.c
.endif

.if defined(NEED_AES_CTR)
SRCS+=	aes-ctr.c
.endif

.if defined(NEED_AES_ENCBLOCK)
SRCS+=	aes-encblock.c
.endif

.if defined(NEED_AES_OMAC1)
SRCS+=	aes-omac1.c
.endif

.if defined(NEED_DES)
.if defined(CONFIG_INTERNAL_DES)
SRCS+=	des-internal.c
.endif
.endif

.if defined(NEED_MD4)
.if defined(CONFIG_INTERNAL_MD4)
SRCS+=	md4-internal.c
.endif
.endif

.if defined(CONFIG_INTERNAL_MD5)
SRCS+=	md5.c \
	md5-internal.c
.endif

.if defined(NEED_FIPS186_2_PRF)
.if defined(CONFIG_INTERNAL_SHA1)
SRCS+=	fips_prf_internal.c
.else
SRCS+=	fips_prf_openssl.c
.endif
.endif

.if defined(CONFIG_INTERNAL_RC4)
SRCS+=	rc4.c
.endif

.if defined(CONFIG_INTERNAL_SHA1)
SRCS+=	sha1-internal.c \
	sha1-pbkdf2.c \
	sha1.c \
	sha1-prf.c
.endif

.if defined(NEED_SHA256)
SRCS+=	sha256.c
.if defined(CONFIG_INTERNAL_SHA256)
SRCS+=	sha256-internal.c \
	sha256-prf.c
.endif
.endif

.if defined(NEED_SHA384)
SRCS+=	sha384.c
.if defined(CONFIG_INTERNAL_SHA384)
SRCS+=	sha384-internal.c \
	sha384-prf.c \
	sha384-tlsprf.c
.endif
.endif

.if defined(NEED_SHA512)
SRCS+=	sha512.c
.if defined(CONFIG_INTERNAL_SHA512)
SRCS+=	sha512-internal.c \
	sha512-prf.c
.endif
.endif

.if defined(NEED_TLS_PRF)
SRCS+=	sha1-tlsprf.c
.endif

.if defined(CONFIG_INTERNAL_DH5)
.if defined(NEED_DH_GROUPS)
SRCS+=	dh_group5.c
.endif
.endif

.if defined(NEED_DH_GROUPS)
SRCS+=	dh_groups.c
.endif

SRCS+=	ms_funcs.c

CFLAGS+=-DCONFIG_CRYPTO_INTERNAL \
	-DCONFIG_TLS_INTERNAL_CLIENT \
	-DCONFIG_TLS_INTERNAL_SERVER \
	-DCONFIG_SHA256 \
	-DCONFIG_SHA384 \
	-DCONFIG_HMAC_SHA384_KDF \
	-DCONFIG_INTERNAL_SHA384
#CFLAGS+=-DALL_DH_GROUPS

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
