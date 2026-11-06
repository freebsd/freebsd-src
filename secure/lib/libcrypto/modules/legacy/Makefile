SHLIB_NAME?=	legacy.so
LIBADD=		crypto
VERSION_MAP=	${.CURDIR}/Version.map

# Pull in ../Makefile.inc now: the ASM_* tests below need Makefile.common.
.include <bsd.init.mk>

SRCS+=	legacyprov.c prov_running.c

# ciphers
SRCS+=	ciphercommon.c ciphercommon_block.c
SRCS+=	cipher_desx.c cipher_desx_hw.c cipher_des.c cipher_des_hw.c
SRCS+=	cipher_tdes_common.c
SRCS+=	cipher_blowfish.c cipher_blowfish_hw.c
SRCS+=	cipher_cast5.c cipher_cast5_hw.c
SRCS+=	cipher_rc2.c cipher_rc2_hw.c
SRCS+=	cipher_rc4.c cipher_rc4_hw.c
SRCS+=	cipher_rc4_hmac_md5.c cipher_rc4_hmac_md5_hw.c
SRCS+=	cipher_seed.c cipher_seed_hw.c

# crypto: the provider has its own copy of the CPU capability code
SRCS+=	cpuid.c ctype.c
.if defined(ASM_aarch64)
SRCS+=	arm64cpuid.S armcap.c
.elif defined(ASM_amd64)
SRCS+=	x86_64cpuid.S
.elif defined(ASM_arm)
SRCS+=	armv4cpuid.S armcap.c
.elif defined(ASM_i386)
SRCS+=	x86cpuid.S
.elif defined(ASM_powerpc) || defined(ASM_powerpc64) || defined(ASM_powerpc64le)
SRCS+=	ppccpuid.S ppccap.c
.else
SRCS+=	mem_clr.c
.endif

# crypto/des
.if defined(ASM_i386)
SRCS+=	crypt586.S des-586.S
.else
SRCS+=	des_enc.c
.endif

# crypto/md5
SRCS+=	md5_dgst.c
.if defined(ASM_aarch64)
SRCS+=	md5-aarch64.S
.elif defined(ASM_amd64)
SRCS+=	md5-x86_64.S
.elif defined(ASM_i386)
SRCS+=	md5-586.S
.endif

# crypto/rc4
.if defined(ASM_amd64)
SRCS+=	rc4-md5-x86_64.S rc4-x86_64.S
.elif defined(ASM_i386)
SRCS+=	rc4-586.S
.else
SRCS+=	rc4_enc.c rc4_skey.c
.endif

# providers/implementations/digests
SRCS+=	digestcommon.c
SRCS+=	md4_prov.c wp_prov.c ripemd_prov.c

# providers/implementations/kdfs
SRCS+=	pbkdf1.c
SRCS+=	pvkkdf.c

# common
SRCS+=	provider_ctx.c provider_util.c

SRCS+=	tls_pad.c

.include <bsd.lib.mk>

.if defined(ASM_${MACHINE_CPUARCH})
.PATH:	${SRCTOP}/sys/crypto/openssl/${MACHINE_CPUARCH}
.elif defined(ASM_${MACHINE_ARCH})
.PATH:	${SRCTOP}/sys/crypto/openssl/${MACHINE_ARCH}
.endif

.PATH:	${LCRYPTO_SRC}/crypto \
	${LCRYPTO_SRC}/crypto/des \
	${LCRYPTO_SRC}/crypto/md5 \
	${LCRYPTO_SRC}/crypto/rc4 \
	${LCRYPTO_SRC}/providers \
	${LCRYPTO_SRC}/providers/common \
	${LCRYPTO_SRC}/providers/implementations/ciphers \
	${LCRYPTO_SRC}/providers/implementations/digests \
	${LCRYPTO_SRC}/providers/implementations/kdfs \
	${LCRYPTO_SRC}/ssl \
	${LCRYPTO_SRC}/ssl/record/methods
