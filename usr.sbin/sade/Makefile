# $FreeBSD$

.if ${MACHINE_CPUARCH} != "ia64"
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
CFLAGS+= -I${.CURDIR}/../../gnu/lib/libodialog -I.

DPADD=	${LIBODIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK}
LDADD=	-lodialog -lncurses -lutil -ldisk

.include <bsd.prog.mk>
