#	$Id: Makefile,v 1.3 1996/01/06 23:10:25 ache Exp $

.PATH:	${.CURDIR}/../../sys/i386/linux
KMOD=	linux_mod
SRCS=	linux.c linux_file.c linux_ioctl.c linux_misc.c linux_signal.c \
	linux_generic.c linux_ipc.c linux_socket.c linux_stats.c \
	linux_dummy.c linux_sysent.c imgact_linux.c vnode_if.h
NOMAN=
CFLAGS+= -DLKM -I. -DCOMPAT_LINUX # -DDEBUG
CLEANFILES+= vnode_if.h vnode_if.c

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/linux /usr/bin

.include <bsd.kmod.mk>
