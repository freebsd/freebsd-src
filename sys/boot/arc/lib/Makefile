# $FreeBSD$

LIB=		arc
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

.PATH:		${.CURDIR}/arch/${MACHINE_ARCH}
# XXX hack to pick up stand.h
LIBSTANDDIR=	${.CURDIR}/../../../../lib/libstand
CFLAGS=		-I${LIBSTANDDIR}
CFLAGS+=	-DDEBUG

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -mno-fp-regs \
		-I${.CURDIR}/../../.. -I${.CURDIR}/../include

#CFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DSAVE_MEMORY

SRCS=	delay.c time.c abort.c setjmperr.c copy.c devicename.c module.c \
	arcconsole.c arcdisk.c elf_freebsd.c bootinfo.c

.if ${MACHINE_ARCH} == "alpha"
SRCS+=	rpb.c
.endif

all: libarc.a

CLEANFILES+= machine

machine:
	ln -sf ${.CURDIR}/../../../alpha/include machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
