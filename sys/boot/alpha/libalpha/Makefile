# $FreeBSD: src/sys/boot/alpha/libalpha/Makefile,v 1.15.12.1 2008/10/02 02:57:24 kensmith Exp $

LIB=		alpha
INTERNALLIB=

#CFLAGS+=	-DDISK_DEBUG

CFLAGS+=	-I${LIBSTANDDIR}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common

SRCS=	OSFpal.c elf_freebsd.c prom.c prom_disp.S prom_swpal.S \
	pal.S reboot.c delay.c time.c alpha_module.c devicename.c \
	srmdisk.c srmnet.c getsecs.c alpha_copy.c bootinfo.c

.include <bsd.lib.mk>
