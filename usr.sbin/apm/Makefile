# $FreeBSD$

PROG=	apm
MAN=	apm.8
MLINKS=	apm.8 apmconf.8
MANSUBDIR= /i386
WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
