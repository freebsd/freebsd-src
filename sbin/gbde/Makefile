# $FreeBSD$

PROG=	gbde
SRCS=	gbde.c template.c
SRCS+=	geom_enc.c
SRCS+=	rijndael-alg-fst.c
SRCS+=	rijndael-api-fst.c
SRCS+=	sha2.c
SRCS+=	g_bde_lock.c

WARNS?=	5

CFLAGS+= -I${.CURDIR}/../../sys
.PATH:	${.CURDIR}/../../sys/geom \
	${.CURDIR}/../../sys/geom/bde \
	${.CURDIR}/../../sys/crypto/rijndael \
	${.CURDIR}/../../sys/crypto/sha2

CLEANFILES+= template.c

MAN=	gbde.8
LDADD=	-lmd  -lutil

.include <bsd.prog.mk>

template.c:	template.txt
	file2c 'const char template[] = {' ',0};' \
		< ${.CURDIR}/template.txt > template.c

test:	${PROG}
	sh ${.CURDIR}/test.sh
