# $Id: Makefile,v 1.1.1.1 1998/08/21 03:17:41 msmith Exp $
#
SUBDIR=	crt
LIB=	i386
NOPIC=
NOPROFILE=

SRCS=	aout_freebsd.c biosdelay.S biosdisk.c biosdisk_support.S biosgetrtc.S \
	biosmem.S biosreboot.S bootinfo.c comconsole.c comconsole_support.S \
	devicename.c gatea20.c getsecs.c i386_copy.c i386_module.c \
	startprog.S vidconsole.c vidconsole_support.S

CFLAGS+=	-I${.CURDIR}/../../common

# Make the disk code more talkative
#CFLAGS+= -DDISK_DEBUG

.include <bsd.lib.mk>
