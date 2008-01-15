# $FreeBSD: src/usr.sbin/sade/Makefile,v 1.147.2.1 2007/09/10 14:25:01 obrien Exp $

.if ${MACHINE_ARCH} != "ia64"
_wizard=	wizard.c
.endif

PROG=	sade
MAN=	sade.8
SRCS=	command.c config.c devices.c \
	disks.c dispatch.c dmenu.c \
	globals.c install.c \
	label.c main.c menus.c misc.c \
	msg.c system.c termcap.c \
	variable.c ${_wizard}
WARNS?=	3

# command.c

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif
CFLAGS+= -I${.CURDIR}/../../gnu/lib/libdialog -I.

DPADD=	${LIBDIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK} ${LIBFTPIO}
LDADD=	-ldialog -lncurses -lutil -ldisk -lftpio

.include <bsd.prog.mk>
