# $FreeBSD: src/sys/boot/alpha/libalpha/Makefile,v 1.14 2004/02/11 22:01:17 ru Exp $

LIB=		alpha
INTERNALLIB=	true

#CFLAGS+=	-DDISK_DEBUG

CFLAGS+=	-I${LIBSTANDDIR}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common

SRCS=	OSFpal.c elf_freebsd.c prom.c prom_disp.S prom_swpal.S \
	pal.S reboot.c delay.c time.c alpha_module.c devicename.c \
	srmdisk.c srmnet.c getsecs.c alpha_copy.c bootinfo.c

.include <bsd.lib.mk>
