# $FreeBSD: src/sys/modules/asr/Makefile,v 1.1.2.1 2000/09/21 20:33:53 msmith Exp $

.PATH:		${.CURDIR}/../../dev/asr
KMOD		=  asr
SRCS		=  asr.c
SRCS		+= opt_scsi.h opt_cam.h opt_asr.h
SRCS		+= device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
