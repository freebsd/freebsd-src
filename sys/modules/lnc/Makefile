# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/lnc

KMOD=	if_lnc
SRCS=	if_lnc.c if_lnc_pci.c
.if ${MACHINE} == "pc98"
SRCS+=	if_lnc_cbus.c
.else
SRCS+=	if_lnc_isa.c
.endif
SRCS+=	opt_inet.h device_if.h bus_if.h isa_if.h pci_if.h
#SRCS+=	miibus_if.h

CFLAGS+= -I${.CURDIR}/../../dev/lnc

.include <bsd.kmod.mk>
