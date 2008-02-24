# $FreeBSD: src/sys/modules/rc/Makefile,v 1.3 2005/10/14 23:30:14 yar Exp $

.PATH: ${.CURDIR}/../../dev/rc

KMOD=	rc
SRCS=	rc.c device_if.h bus_if.h isa_if.h opt_tty.h

.if !defined(KERNBUILDDIR)
opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h
.endif

.include <bsd.kmod.mk>
