# $FreeBSD: src/sys/modules/syscons/daemon/Makefile,v 1.6.2.1 2000/05/09 13:42:14 nyan Exp $

KMOD=	daemon_saver
SRCS= 	daemon_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
