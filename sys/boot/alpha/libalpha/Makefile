# $FreeBSD: src/sys/boot/alpha/libalpha/Makefile,v 1.7.2.2 2002/07/19 18:46:28 ru Exp $

LIB=		alpha
INTERNALLIB=	true

#CFLAGS+=	-DDISK_DEBUG
#CPPFLAGS+=	-DNO_DISKLABEL
#CPPFLAGS+=	-DSAVE_MEMORY

# XXX hack to pick up stand.h
LIBSTANDDIR=	${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-DDEBUG
CFLAGS+=	-I${LIBSTANDDIR}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -mno-fp-regs \
		-I${.CURDIR}/../../.. -I.

SRCS=	OSFpal.c elf_freebsd.c prom.c prom_disp.S prom_swpal.S \
	pal.S reboot.c delay.c time.c alpha_module.c devicename.c \
	srmdisk.c srmnet.c getsecs.c alpha_copy.c bootinfo.c

machine:
	ln -sf ${.CURDIR}/../../../alpha/include machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
