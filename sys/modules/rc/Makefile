# $FreeBSD: src/sys/modules/rc/Makefile,v 1.2.12.1 2006/02/10 00:38:05 yar Exp $

.PATH: ${.CURDIR}/../../dev/rc

KMOD=	rc
SRCS=	rc.c device_if.h bus_if.h isa_if.h opt_tty.h

.if !defined(KERNBUILDDIR)
opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h
.endif

.include <bsd.kmod.mk>
