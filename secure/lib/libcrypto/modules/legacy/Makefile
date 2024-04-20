
SHLIB_NAME?=	legacy.so
LIBADD=		crypto

VERSION_MAP=	${SRCTOP}/crypto/openssl/providers/legacy.ld

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

# digests
SRCS+=	digestcommon.c
SRCS+=	md4_prov.c wp_prov.c ripemd_prov.c

# kdfs
SRCS+=	pbkdf1.c

# ssl
SRCS+=	record/tls_pad.c

.include <bsd.lib.mk>

.PATH:	${LCRYPTO_SRC}/providers/implementations/ciphers \
	${LCRYPTO_SRC}/providers/implementations/digests \
	${LCRYPTO_SRC}/providers/implementations/kdfs \
	${LCRYPTO_SRC}/ssl
