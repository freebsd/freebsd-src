# $FreeBSD$

.PATH:	${.CURDIR}/../../opencrypto
.PATH:	${.CURDIR}/../../crypto
.PATH:	${.CURDIR}/../../crypto/blowfish
.PATH:	${.CURDIR}/../../crypto/des
.PATH:	${.CURDIR}/../../crypto/rijndael
.PATH:	${.CURDIR}/../../crypto/sha2
.PATH:	${.CURDIR}/../../crypto/camellia

KMOD	= crypto
SRCS	= crypto.c cryptodev_if.c
SRCS	+= criov.c cryptosoft.c xform.c
SRCS	+= cast.c deflate.c rmd160.c rijndael-alg-fst.c rijndael-api.c
SRCS	+= skipjack.c bf_enc.c bf_ecb.c bf_skey.c
SRCS	+= des_ecb.c des_enc.c des_setkey.c
SRCS	+= sha1.c sha2.c
SRCS	+= opt_param.h cryptodev_if.h bus_if.h device_if.h
SRCS	+= opt_ddb.h opt_kdtrace.h
SRCS	+= camellia.c camellia-api.c
.include <bsd.kmod.mk>
