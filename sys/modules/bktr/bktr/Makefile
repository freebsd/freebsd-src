# $FreeBSD$

# Makefile for the bktr Brooktree Bt848/Bt878 PCI video capture device

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr 
SRCS=	bktr_core.c bktr_os.c bktr_audio.c bktr_tuner.c bktr_card.c \
	bktr.h opt_devfs.h opt_bktr.h smbus.h bus_if.h device_if.h \
	pci_if.h vnode_if.h
CLEANFILES= bktr.h

bktr.h:
	echo "#define NBKTR 1" > bktr.h
	echo "#define BKTR_FREEBSD_MODULE 1" >> bktr.h

.include <bsd.kmod.mk>
