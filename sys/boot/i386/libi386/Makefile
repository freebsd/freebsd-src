# $Id: Makefile,v 1.7 1998/09/30 22:36:45 peter Exp $
#
LIB=	i386
NOPIC=
NOPROFILE=
INTERNALLIB=	true
INTERNALSTATICLIB=	true

SRCS=	aout_freebsd.c biosdisk.c biosmem.c bootinfo.c comconsole.c \
	devicename.c elf_freebsd.c gatea20.c i386_copy.c i386_module.c time.c vidconsole.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../btx/lib \
		-I${.CURDIR}/../../.. -I.

# Make the disk code more talkative
#CFLAGS+= -DDISK_DEBUG


machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
