# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/mana

KMOD	= if_mana
SRCS	= gdma_main.c mana_sysctl.c shm_channel.c
SRCS	+= mana_en.c gdma_util.c hw_channel.c
SRCS	+= device_if.h bus_if.h pci_if.h

CFLAGS  += -I${SRCTOP}/sys/dev/mana

.include <bsd.kmod.mk>
