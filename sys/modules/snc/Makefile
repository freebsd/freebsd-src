# $FreeBSD: src/sys/modules/snc/Makefile,v 1.2.14.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/snc

KMOD=	if_snc
SRCS=	if_snc.c if_snc_cbus.c if_snc_pccard.c dp83932.c dp83932subr.c
SRCS+=	opt_inet.h opt_bdg.h device_if.h bus_if.h isa_if.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
