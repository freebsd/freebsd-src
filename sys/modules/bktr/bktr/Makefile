# $FreeBSD$

# Makefile for the bktr Brooktree Bt848/Bt878 PCI video capture device

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr 
SRCS=	bktr_core.c bktr_os.c bktr_audio.c bktr_tuner.c bktr_card.c \
	opt_devfs.h opt_bktr.h bus_if.h device_if.h \
	pci_if.h vnode_if.h

opt_bktr.h:
	echo "#define BKTR_FREEBSD_MODULE 1" > opt_bktr.h

.include <bsd.kmod.mk>
