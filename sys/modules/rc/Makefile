# $FreeBSD: src/sys/modules/rc/Makefile,v 1.2.12.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/rc

KMOD=	rc
SRCS=	rc.c device_if.h bus_if.h isa_if.h opt_tty.h

.if !defined(KERNBUILDDIR)
opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h
.endif

.include <bsd.kmod.mk>
