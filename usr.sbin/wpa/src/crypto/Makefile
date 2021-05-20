.include <src.opts.mk>

.include "../../Makefile.inc"

LIB=	wpacrypto
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/crypto

SRCS=	aes-cbc.c \
	aes-ctr.c \
	aes-eax.c \
	aes-encblock.c \
	aes-internal.c \
	aes-internal-dec.c \
	aes-internal-enc.c \
	aes-omac1.c \
	aes-unwrap.c \
	aes-wrap.c \
	crypto_internal.c \
	crypto_internal-cipher.c \
	crypto_internal-modexp.c \
	crypto_internal-rsa.c \
	des-internal.c \
	dh_group5.c \
	dh_groups.c \
	fips_prf_internal.c \
	md4-internal.c \
	md5.c \
	md5-internal.c \
	ms_funcs.c \
	random.c \
	rc4.c \
	sha1.c \
	sha1-internal.c \
	sha1-pbkdf2.c \
	sha1-prf.c \
	sha1-tlsprf.c \
	sha256.c \
	sha256-prf.c \
	sha256-tlsprf.c \
	sha256-internal.c \
	sha384.c \
	sha384-prf.c \
	sha384-internal.c \
	sha512-internal.c \
	tls_internal.c

CFLAGS+=-DCONFIG_CRYPTO_INTERNAL \
	-DCONFIG_TLS_INTERNAL_CLIENT \
	-DCONFIG_TLS_INTERNAL_SERVER \
	-DCONFIG_SHA256 \
	-DCONFIG_SHA384 \
	-DCONFIG_HMAC_SHA384_KDF \
	-DCONFIG_INTERNAL_SHA384
#CFLAGS+=-DALL_DH_GROUPS

.include "../../Makefile.crypto"

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
