# $FreeBSD: src/sys/modules/crypto/Makefile,v 1.3.2.2.2.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../../opencrypto
.PATH:	${.CURDIR}/../../crypto
.PATH:	${.CURDIR}/../../crypto/blowfish
.PATH:	${.CURDIR}/../../crypto/des
.PATH:	${.CURDIR}/../../crypto/rijndael
.PATH:	${.CURDIR}/../../crypto/sha2
.PATH:	${.CURDIR}/../../crypto/camellia

KMOD	= crypto
SRCS	= crypto.c
SRCS	+= criov.c cryptosoft.c xform.c
SRCS	+= cast.c deflate.c rmd160.c rijndael-alg-fst.c rijndael-api.c
SRCS	+= skipjack.c bf_enc.c bf_skey.c
SRCS	+= des_ecb.c des_enc.c des_setkey.c
SRCS	+= sha1.c sha2.c
SRCS	+= opt_param.h
SRCS	+= camellia.c camellia-api.c

.include <bsd.kmod.mk>
