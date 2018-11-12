# $FreeBSD$

.PATH:	${.CURDIR}/../../opencrypto
.PATH:	${.CURDIR}/../../crypto
.PATH:	${.CURDIR}/../../crypto/blowfish
.PATH:	${.CURDIR}/../../crypto/camellia
.PATH:	${.CURDIR}/../../crypto/des
.PATH:	${.CURDIR}/../../crypto/rijndael
.PATH:	${.CURDIR}/../../crypto/sha2
.PATH:	${.CURDIR}/../../crypto/siphash
.PATH:	${.CURDIR}/../../crypto/skein

KMOD	= crypto
SRCS	= crypto.c cryptodev_if.c
SRCS	+= criov.c cryptosoft.c xform.c
SRCS	+= cast.c cryptodeflate.c rmd160.c rijndael-alg-fst.c rijndael-api.c rijndael-api-fst.c
SRCS	+= skipjack.c bf_enc.c bf_ecb.c bf_skey.c
SRCS	+= camellia.c camellia-api.c
SRCS	+= des_ecb.c des_enc.c des_setkey.c
SRCS	+= sha1.c sha256c.c sha512c.c
SRCS	+= skein.c skein_block.c
.if exists(${MACHINE_ARCH}/skein_block_asm.s)
.PATH:	${.CURDIR}/../../crypto/skein/${MACHINE_ARCH}
SRCS	+= skein_block_asm.s
CFLAGS	+= -DSKEIN_ASM -DSKEIN_USE_ASM=1792 # list of block functions to replace with assembly: 256+512+1024 = 1792
ACFLAGS	+= -DELF -Wa,--noexecstack
.endif
SRCS	+= siphash.c
SRCS	+= gmac.c gfmult.c
SRCS	+= opt_param.h cryptodev_if.h bus_if.h device_if.h
SRCS	+= opt_ddb.h

.include <bsd.kmod.mk>
