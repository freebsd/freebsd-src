# $FreeBSD: src/sys/modules/en/Makefile,v 1.1 2003/04/25 16:14:02 harti Exp $

.PATH: ${.CURDIR}/../../pci ${.CURDIR}/../../dev/en

KMOD=	if_en
SRCS=	if_en_pci.c midway.c device_if.h bus_if.h pci_if.h
SRCS+=	opt_inet.h opt_natm.h opt_ddb.h
CFLAGS+= -DENABLE_BPF
# CFLAGS+= -DINVARIANTS -DEN_DEBUG=0

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

opt_natm.h:
	echo "#define NATM 1" > ${.TARGET}

# opt_ddb.h:
# 	echo "#define DDB 1" > ${.TARGET}

.include <bsd.kmod.mk>
