# $FreeBSD: src/sys/modules/syscons/apm/Makefile,v 1.1.4.1 2001/02/08 13:43:41 nyan Exp $

KMOD=	apm_saver
SRCS= 	apm_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
