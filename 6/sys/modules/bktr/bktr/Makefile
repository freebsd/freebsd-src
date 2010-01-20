# $FreeBSD$

# Makefile for the bktr Brooktree Bt848/Bt878 PCI video capture device

.PATH: ${.CURDIR}/../../../dev/bktr

KMOD=	bktr 
SRCS=	bktr_core.c bktr_os.c bktr_audio.c bktr_tuner.c bktr_card.c msp34xx.c \
	opt_devfs.h opt_bktr.h bus_if.h device_if.h \
	pci_if.h

CFLAGS+= -DBKTR_FREEBSD_MODULE

.include <bsd.kmod.mk>
