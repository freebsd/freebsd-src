SHLIB_NAME?=	legacy.so
LIBADD=		crypto

SRCS+=	legacyprov.c prov_running.c

# ciphers
SRCS+=	ciphercommon.c ciphercommon_hw.c ciphercommon_block.c \
	ciphercommon_gcm.c ciphercommon_gcm_hw.c \
	ciphercommon_ccm.c ciphercommon_ccm_hw.c
SRCS+=	cipher_desx.c cipher_desx_hw.c cipher_des.c cipher_des_hw.c
SRCS+=	cipher_tdes_common.c
SRCS+=	cipher_blowfish.c cipher_blowfish_hw.c
SRCS+=	cipher_cast5.c cipher_cast5_hw.c
SRCS+=	cipher_rc2.c cipher_rc2_hw.c
SRCS+=	cipher_rc4.c cipher_rc4_hw.c
SRCS+=	cipher_rc4_hmac_md5.c cipher_rc4_hmac_md5_hw.c
SRCS+=	cipher_seed.c cipher_seed_hw.c

# crypto
SRCS+=	cpuid.c
SRCS+=	ctype.c

# crypto/des
SRCS+=	des_enc.c
SRCS+=	fcrypt_b.c

# crypto/md5
SRCS+=	md5_dgst.c
SRCS+=	md5_one.c
SRCS+=	md5_sha1.c

# providers/implementations/digests
SRCS+=	digestcommon.c
SRCS+=	md4_prov.c wp_prov.c ripemd_prov.c

# providers/implementations/kdfs
SRCS+=	pbkdf1.c
SRCS+=	pvkkdf.c

# common
SRCS+= provider_err.c provider_ctx.c
SRCS+= provider_util.c

SRCS+=	tls_pad.c

# This is needed so the provider can be loaded for us.
#
# There's a discrepancy between how this provider gets built in OpenSSL proper
# and FreeBSD.
SRCS+=	params_idx.c

.include <bsd.lib.mk>

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/des \
	${LCRYPTO_SRC}/crypto/md5 \
	${LCRYPTO_SRC}/providers \
	${LCRYPTO_SRC}/providers/common \
	${LCRYPTO_SRC}/providers/implementations/ciphers \
	${LCRYPTO_SRC}/providers/implementations/digests \
	${LCRYPTO_SRC}/providers/implementations/kdfs \
	${LCRYPTO_SRC}/ssl \
	${LCRYPTO_SRC}/ssl/record/methods \
