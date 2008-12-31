# $FreeBSD: src/sys/modules/en/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/en

KMOD=	if_en
SRCS=	if_en_pci.c midway.c device_if.h bus_if.h pci_if.h
SRCS+=	opt_inet.h opt_natm.h opt_ddb.h
CFLAGS+= -DENABLE_BPF
# CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS -DEN_DEBUG=0

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_natm.h:
	echo "#define NATM 1" > ${.TARGET}

# opt_ddb.h:
# 	echo "#define DDB 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
