#	$Id: Makefile,v 1.3 1994/10/14 22:56:40 sos Exp $

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_mod
SRCS= 	ibcs2.c ibcs2_dummy.c ibcs2_file.c ibcs2_ioctl.c ibcs2_ipc.c \
	ibcs2_isc.c ibcs2_misc.c ibcs2_signal.c ibcs2_stats.c \
	ibcs2_sysent.c ibcs2_sysi86.c ibcs2_xenix.c vnode_if.h
NOMAN=
CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2
CLEANFILES+= vnode_if.h vnode_if.c

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/ibcs2 /usr/bin

.include <bsd.kmod.mk>
