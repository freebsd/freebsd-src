# $Id: Makefile,v 1.10 1998/10/22 20:23:15 msmith Exp $
#
LIB=			i386
NOPIC=
NOPROFILE=
INTERNALLIB=		true
INTERNALSTATICLIB=	true

SRCS=	aout_freebsd.c biosdisk.c biosmem.c biospnp.c biospci.c \
	bootinfo.c comconsole.c devicename.c elf_freebsd.c gatea20.c \
	i386_copy.c i386_module.c time.c vidconsole.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../btx/lib \
		-I${.CURDIR}/../../.. -I.

# Make the disk code more talkative
#CFLAGS+= -DDISK_DEBUG

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine

.include <bsd.lib.mk>
