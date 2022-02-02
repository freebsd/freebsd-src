# $FreeBSD$

LIBSODIUM=${SRCTOP}/sys/contrib/libsodium/src/libsodium

.PATH:	${SRCTOP}/sys/opencrypto
.PATH:	${SRCTOP}/sys/crypto
.PATH:	${SRCTOP}/sys/crypto/camellia
.PATH:	${SRCTOP}/sys/crypto/des
.PATH:	${SRCTOP}/sys/crypto/rijndael
.PATH:	${SRCTOP}/sys/crypto/sha2
.PATH:	${SRCTOP}/sys/crypto/siphash
.PATH:	${SRCTOP}/sys/crypto/skein
.PATH:	${SRCTOP}/sys/crypto/blake2
.PATH:	${SRCTOP}/sys/crypto/chacha20
.PATH:	${SRCTOP}/sys/contrib/libb2
.PATH:	${LIBSODIUM}/crypto_core/ed25519/ref10
.PATH:	${LIBSODIUM}/crypto_core/hchacha20
.PATH:	${LIBSODIUM}/crypto_onetimeauth/poly1305
.PATH:	${LIBSODIUM}/crypto_onetimeauth/poly1305/donna
.PATH:	${LIBSODIUM}/crypto_scalarmult/curve25519
.PATH:	${LIBSODIUM}/crypto_scalarmult/curve25519/ref10
.PATH:	${LIBSODIUM}/crypto_stream/chacha20
.PATH:	${LIBSODIUM}/crypto_stream/chacha20/ref
.PATH:	${LIBSODIUM}/crypto_verify/sodium
.PATH:	${SRCTOP}/sys/crypto/libsodium

KMOD	= crypto
SRCS	= crypto.c cryptodev_if.c
SRCS	+= criov.c cryptosoft.c
SRCS	+= xform_aes_cbc.c xform_aes_icm.c xform_aes_xts.c xform_cml.c
SRCS	+= xform_deflate.c xform_gmac.c xform_null.c xform_rmd160.c
SRCS	+= xform_sha1.c xform_sha2.c
SRCS	+= cryptodeflate.c rmd160.c rijndael-alg-fst.c rijndael-api.c rijndael-api-fst.c
SRCS	+= camellia.c camellia-api.c
SRCS	+= des_ecb.c des_enc.c des_setkey.c
SRCS	+= sha1.c sha256c.c sha512c.c
SRCS	+= skein.c skein_block.c
# unroll the 256 and 512 loops, half unroll the 1024
CFLAGS.skein_block.c	+= -DSKEIN_LOOP=995
.if exists(${MACHINE_ARCH}/skein_block_asm.S)
.PATH:	${SRCTOP}/sys/crypto/skein/${MACHINE_ARCH}
SRCS	+= skein_block_asm.S
CFLAGS	+= -DSKEIN_ASM -DSKEIN_USE_ASM=1792 # list of block functions to replace with assembly: 256+512+1024 = 1792
ACFLAGS	+= -DELF -Wa,--noexecstack
# Fully unroll all loops in the assembly optimized version
ACFLAGS	+= -DSKEIN_LOOP=0
# 20201002 Add explict Makefile dependency for reenabled assembly optimized
# version. SKEIN_USE_ASM determines which routines should come from the assembly
# vs C versions, and skein_block needs to be rebuilt if it changes.
skein_block.o: Makefile
.endif
SRCS	+= siphash.c
SRCS	+= gmac.c gfmult.c
SRCS	+= blake2b-ref.c
SRCS	+= blake2s-ref.c
SRCS	+= blake2-sw.c
CFLAGS.blake2b-ref.c	+= -I${SRCTOP}/sys/crypto/blake2 -DSUFFIX=_ref
CFLAGS.blake2s-ref.c	+= -I${SRCTOP}/sys/crypto/blake2 -DSUFFIX=_ref
CFLAGS.blake2-sw.c	+= -I${SRCTOP}/sys/crypto/blake2
CWARNFLAGS.blake2b-ref.c	+= -Wno-cast-qual -Wno-unused-function
CWARNFLAGS.blake2s-ref.c	+= -Wno-cast-qual -Wno-unused-function
SRCS	+= chacha.c
SRCS	+= chacha-sw.c
SRCS	+= chacha20_poly1305.c

LIBSODIUM_INC=${LIBSODIUM}/include
LIBSODIUM_COMPAT=${SRCTOP}/sys/crypto/libsodium
SRCS	+= xform_chacha20_poly1305.c
CFLAGS.xform_chacha20_poly1305.c+= -I${LIBSODIUM_INC} -I${LIBSODIUM_COMPAT}
SRCS	+= xform_poly1305.c
CFLAGS.xform_poly1305.c		+= -I${LIBSODIUM_INC} -I${LIBSODIUM_COMPAT}
SRCS	+= ed25519_ref10.c
CFLAGS.ed25519_ref10.c		+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
CWARNFLAGS.ed25519_ref10.c	+= -Wno-unused-function
SRCS	+= core_hchacha20.c
CFLAGS.core_hchacha20.c		+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= onetimeauth_poly1305.c
CFLAGS.onetimeauth_poly1305.c	+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= poly1305_donna.c
CFLAGS.poly1305_donna.c		+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= stream_chacha20.c
CFLAGS.stream_chacha20.c	+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= chacha20_ref.c
CFLAGS.chacha20_ref.c		+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= scalarmult_curve25519.c
CFLAGS.scalarmult_curve25519.c	+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= x25519_ref10.c
CFLAGS.x25519_ref10.c		+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
CWARNFLAGS.x25519_ref10.c	+= -Wno-unused-function
SRCS	+= verify.c
CFLAGS.verify.c			+= -I${LIBSODIUM_INC}/sodium -I${LIBSODIUM_COMPAT}
SRCS	+= randombytes.c
CFLAGS.randombytes.c		+= -I${LIBSODIUM_INC} -I${LIBSODIUM_COMPAT}
SRCS	+= utils.c
CFLAGS.utils.c			+= -I${LIBSODIUM_INC} -I${LIBSODIUM_COMPAT}

SRCS	+= curve25519.c
CFLAGS.curve25519.c		+= -I${LIBSODIUM_INC} -I${LIBSODIUM_COMPAT}

SRCS	+= opt_param.h cryptodev_if.h bus_if.h device_if.h
SRCS	+= opt_compat.h
SRCS	+= opt_ddb.h
SRCS	+= cbc_mac.c
SRCS	+= xform_cbc_mac.c

.include <bsd.kmod.mk>
