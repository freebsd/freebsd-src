# $Id: Makefile,v 1.5 1998/09/26 01:30:20 msmith Exp $
#
LIB=	i386
NOPIC=
NOPROFILE=

SRCS=	aout_freebsd.c biosdisk.c biosmem.c bootinfo.c comconsole.c \
	devicename.c elf_freebsd.c gatea20.c i386_copy.c i386_module.c time.c vidconsole.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../btx/lib

# Make the disk code more talkative
#CFLAGS+= -DDISK_DEBUG

.include <bsd.lib.mk>
