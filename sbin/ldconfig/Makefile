# $FreeBSD$

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
WARNS?=	0
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
