# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.1.2.3 2001/02/27 04:31:29 cg Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_mss
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= mss.c gusc.c
KMODDEPS 	= snd_pcm

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
