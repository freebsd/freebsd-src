# $FreeBSD: src/sys/modules/snc/Makefile,v 1.2.14.1 2006/02/10 00:38:06 yar Exp $

.PATH: ${.CURDIR}/../../dev/snc

KMOD=	if_snc
SRCS=	if_snc.c if_snc_cbus.c if_snc_pccard.c dp83932.c dp83932subr.c
SRCS+=	opt_inet.h opt_bdg.h device_if.h bus_if.h isa_if.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
