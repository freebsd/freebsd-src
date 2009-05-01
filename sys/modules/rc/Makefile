# $FreeBSD: src/sys/modules/rc/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/rc

KMOD=	rc
SRCS=	rc.c device_if.h bus_if.h isa_if.h opt_tty.h

.if !defined(KERNBUILDDIR)
opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h
.endif

.include <bsd.kmod.mk>
