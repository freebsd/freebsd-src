# $FreeBSD$

PROG=	gbde
SRCS=	gbde.c template.c
SRCS+=	rijndael-alg-fst.c
SRCS+=	rijndael-api-fst.c
SRCS+=	sha2.c
SRCS+=	g_bde_lock.c

# rijndael-fst.c does evil casting things which results in warnings on
# 64 bit machines, the test-vectors check out however, so it works right.
.if ${MACHINE_ARCH} == "i386"
WARNS?=	5
.else
WARNS?=	3
.endif

CFLAGS+= -I${.CURDIR}/../../sys
.PATH:	${.CURDIR}/../../sys/geom/bde \
	${.CURDIR}/../../sys/crypto/rijndael \
	${.CURDIR}/../../sys/crypto/sha2

CLEANFILES+= template.c

MAN=	gbde.8
DPADD=	${LIBMD} ${LIBUTIL} ${LIBGEOM}
LDADD=	-lmd -lutil -lgeom

template.c: template.txt
	file2c 'const char template[] = {' ',0};' \
		< ${.CURDIR}/template.txt > template.c

test: ${PROG}
	sh ${.CURDIR}/test.sh

.include <bsd.prog.mk>
