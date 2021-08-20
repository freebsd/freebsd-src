# $FreeBSD$

.PATH: ${.CURDIR:H}/common

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-DCRT_IRELOC_RELA

CRT1OBJS+=	crt1_s.o

.include <bsd.lib.mk>
