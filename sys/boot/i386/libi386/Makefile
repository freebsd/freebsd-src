# $Id$
#
SUBDIR=	crt
LIB=	i386
NOPIC=
NOPROFILE=

SRCS=	aout_freebsd.c biosdelay.S biosdisk.c biosdisk_support.S biosgetrtc.S \
	biosmem.S biosreboot.S bootinfo.c comconsole.c comconsole_support.S \
	devicename.c gatea20.c getsecs.c i386_module.c pread.c startprog.S \
	vidconsole.c vidconsole_support.S

CFLAGS+=	-I${.CURDIR}/../../common

# Make the disk code more talkative
#CFLAGS+= -DDISK_DEBUG

# Minimise the pread() buffer at the price of slower loads
#CPPFLAGS+= -DSAVE_MEMORY

.include <bsd.lib.mk>
