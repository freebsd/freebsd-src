# $FreeBSD$

.include <bsd.own.mk>

SYSDIR = ${SRCTOP}/sys
.PATH: ${SYSDIR}/geom/eli
.PATH: ${SYSDIR}/crypto/sha2

TESTSDIR=	${TESTBASE}/sys/geom/eli/pbkdf2

ATF_TESTS_C=	pbkdf2

CFLAGS.pbkdf2=	-I${SYSDIR}

SRCS.pbkdf2=	\
		hmactest.c \
		g_eli_crypto.c \
		g_eli_hmac.c \
		pkcs5v2.c \
		sha512c.c \
		sha256c.c

LIBADD.pbkdf2= crypto

testvect.h:
	python gentestvect.py > ${.TARGET}

.include <bsd.test.mk>
