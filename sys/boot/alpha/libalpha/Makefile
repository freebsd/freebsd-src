# $Id: Makefile,v 1.2 1998/08/22 10:31:01 dfr Exp $

LIB=		alpha
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

# XXX hack to pick up stand.h
CFLAGS= -I/home/dfr/FreeBSD/alpha/src/lib/libstand
CFLAGS+= -DDEBUG

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -mno-fp-regs

CFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DSAVE_MEMORY

SRCS=	OSFpal.c elf_freebsd.c prom.c prom_disp.S prom_swpal.S start.S \
	pal.S reboot.c delay.c time.c alpha_module.c devicename.c \
	srmdisk.c srmnet.c getsecs.c alpha_copy.c

all: libalpha.a

.include <bsd.lib.mk>
