# $FreeBSD$

PROG=	gbde
SRCS=	gbde.c template.c
SRCS+=	geom_enc.c
SRCS+=	rijndael-alg-fst.c
SRCS+=	rijndael-api-fst.c
SRCS+=	g_bde_lock.c

CFLAGS+= -I/sys
VPATH += ${.CURDIR}/../../sys/geom:${.CURDIR}/../../sys/geom/bde:${.CURDIR}/../../sys/crypto/rijndael

CLEANFILES+= template.c

MAN=	gbde.8
LDADD=	-lmd  -lutil

.include <bsd.prog.mk>

template.c:	template.txt
	file2c 'const char template[] = {' ',0};' \
		< ${.CURDIR}/template.txt > template.c

test:	${PROG}
	sh ${.CURDIR}/test.sh
