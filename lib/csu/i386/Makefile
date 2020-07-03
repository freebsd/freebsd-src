# $FreeBSD$

.PATH: ${.CURDIR:H}/common

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-DCRT_IRELOC_REL

CRT1OBJS+=	crt1_s.o

.include <bsd.lib.mk>
