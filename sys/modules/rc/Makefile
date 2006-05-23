# $FreeBSD: src/sys/modules/rc/Makefile,v 1.2 2003/03/05 08:16:29 das Exp $

.PATH: ${.CURDIR}/../../dev/rc

KMOD=	rc
SRCS=	rc.c device_if.h bus_if.h isa_if.h opt_tty.h

opt_tty.h:
	echo "#define TTYHOG 8192" >opt_tty.h

.include <bsd.kmod.mk>
