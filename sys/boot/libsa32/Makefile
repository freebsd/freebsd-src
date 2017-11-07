# $FreeBSD$

DO32=1

.include <bsd.init.mk>

LIB=sa32
LIBSA_CPUARCH=${MACHINE_CPUARCH:C/amd64/i386/}

CFLAGS+=	-I.

.PATH:	${SASRC}
.include "${SASRC}/Makefile"
