# $FreeBSD$

.include <src.opts.mk>

SDEVRANDOM=	${SRCTOP}/sys/dev/random
.PATH:		${SDEVRANDOM}

TESTSDIR=	${TESTSBASE}/sys/devrandom

CFLAGS+=	-I${SRCTOP}/sys

ATF_TESTS_C+=	uint128_test

# Test Chacha CTR behavior <-> uint128
LDADD.uint128_test+=	${SDEVRANDOM}/hash.c
LDFLAGS.uint128_test+=	-Wno-unused-parameter

# hash.c deps:
LIBADD.uint128_test+=	md	# SHA256
LDADD.uint128_test+=	${SRCTOP}/sys/crypto/rijndael/rijndael-alg-fst.c
LDADD.uint128_test+=	${SRCTOP}/sys/crypto/rijndael/rijndael-api-fst.c
LDFLAGS.uint128_test+=	-Wno-cast-align


.include <bsd.test.mk>
