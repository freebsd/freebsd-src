# $FreeBSD$

LIB=		alpha
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

# XXX hack to pick up stand.h
LIBSTANDDIR=	${.CURDIR}/../../../../lib/libstand
CFLAGS=		-I${LIBSTANDDIR}
CFLAGS+=	-DDEBUG

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -mno-fp-regs \
		-I${.CURDIR}/../../.. -I.

#CFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DSAVE_MEMORY

SRCS=	OSFpal.c elf_freebsd.c prom.c prom_disp.S prom_swpal.S \
	pal.S reboot.c delay.c time.c alpha_module.c devicename.c \
	srmdisk.c srmnet.c getsecs.c alpha_copy.c bootinfo.c

all: libalpha.a

machine:
	ln -sf ${.CURDIR}/../../../alpha/include machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
