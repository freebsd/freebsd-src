# $FreeBSD: src/usr.sbin/mfiutil/Makefile,v 1.2.2.2.6.1 2010/12/21 17:09:25 kensmith Exp $
PROG=	mfiutil

SRCS=	mfiutil.c mfi_cmd.c mfi_config.c mfi_drive.c mfi_evt.c mfi_flash.c \
	mfi_patrol.c mfi_show.c mfi_volume.c
MAN8=	mfiutil.8

CFLAGS+= -fno-builtin-strftime
WARNS?=3

LDADD=	-lutil

# Here be dragons
.ifdef DEBUG
CFLAGS+= -DDEBUG
.endif

.include <bsd.prog.mk>
