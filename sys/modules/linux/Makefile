#	$Id: Makefile,v 1.6 1996/03/02 20:00:35 peter Exp $

.PATH:	${.CURDIR}/../../sys/i386/linux
KMOD=	linux_mod
SRCS=	linux.c linux_file.c linux_ioctl.c linux_misc.c linux_signal.c \
	linux_ipc.c linux_socket.c linux_stats.c \
	linux_dummy.c linux_sysent.c linux_sysvec.c linux_util.c \
	imgact_linux.c vnode_if.h
OBJS=	linux_locore.o
NOMAN=

CFLAGS+= -DLKM -I. -DCOMPAT_43 -DCOMPAT_LINUX #-DDEBUG
CPPFLAGS= -I. -I${.CURDIR}/../../sys
EXPORT_SYMS=_linux_mod
CLEANFILES+= vnode_if.h vnode_if.c linux_genassym.o linux_genassym machine \
	linux_assym.h

linux_assym.h:	linux_genassym
	./linux_genassym > linux_assym.h

linux_locore.o:	linux_locore.s linux_assym.h
	@if [ ! -h machine ]; then ln -s ${.CURDIR}/../../i386/include machine \
	; fi
	${CPP} -DLOCORE -DKERNEL ${CPPFLAGS} ${.IMPSRC} | ${AS} ${ASFLAGS} -o ${.TARGET}

linux_genassym.o:	linux_genassym.c linux.h
	${CC} -c ${CFLAGS} -UKERNEL ${.IMPSRC}

linux_genassym:	linux_genassym.o
	${CC} ${CFLAGS} ${.ALLSRC} -o ${.TARGET}

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	${.CURDIR}/linux ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
