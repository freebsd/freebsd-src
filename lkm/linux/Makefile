#	$Id: Makefile,v 1.9 1997/01/10 05:48:49 mpp Exp $

.PATH:	${.CURDIR}/../../sys/i386/linux
KMOD=	linux_mod
SRCS=	linux.c linux_file.c linux_ioctl.c linux_misc.c linux_signal.c \
	linux_ipc.c linux_socket.c linux_stats.c \
	linux_dummy.c linux_sysent.c linux_sysvec.c linux_util.c \
	imgact_linux.c vnode_if.h
OBJS=	linux_locore.o
MAN8=	linux.8

CFLAGS+= -DLKM -I. -DCOMPAT_43 -DCOMPAT_LINUX #-DDEBUG
EXPORT_SYMS=_linux_mod
CLEANFILES+= vnode_if.h vnode_if.c linux_genassym.o linux_genassym machine \
	linux_assym.h

linux_assym.h:	linux_genassym
	./linux_genassym > linux_assym.h

linux_locore.o:	linux_locore.s linux_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE -DKERNEL ${CFLAGS} \
		${.IMPSRC} -o ${.TARGET}

linux_genassym.o:	linux_genassym.c linux.h
	${CC} -c ${CFLAGS} -UKERNEL ${.IMPSRC}

linux_genassym:	linux_genassym.o
	${CC} ${CFLAGS} ${.ALLSRC} -o ${.TARGET}

machine:
	ln -fs ${.CURDIR}/../../sys/i386/include machine

vnode_if.h:	machine

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/linux ${DESTDIR}/usr/bin

.include <bsd.kmod.mk>
