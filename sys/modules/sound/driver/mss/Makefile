# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/sound/isa
KMOD		= snd_mss
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= mss.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
