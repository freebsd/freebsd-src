# $FreeBSD$

.PATH: ${SRCTOP}/contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${SRCTOP}/sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

WARNS?=	2

.include <bsd.prog.mk>
