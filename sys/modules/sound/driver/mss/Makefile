# $FreeBSD: src/sys/modules/sound/driver/mss/Makefile,v 1.1.2.2 2000/10/21 02:38:07 nyan Exp $

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_mss
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= mss.c
KMODDEPS 	= snd_pcm snd_gusc

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
