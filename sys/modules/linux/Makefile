# $FreeBSD$

.PATH:	${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_ARCH}/linux

MAINTAINER=	marcel@FreeBSD.org

KMOD=	linux
SRCS=	linux_file.c linux_ioctl.c linux_machdep.c linux_misc.c \
	linux_signal.c linux_socket.c linux_stats.c linux_mib.c \
	linux_dummy.c linux_sysent.c linux_sysvec.c linux_util.c \
	opt_compat.h opt_linux.h opt_vmpage.h vnode_if.h
OBJS=	linux_locore.o
MAN8=	linux.8

.if ${MACHINE_ARCH} != "alpha"
SRCS+=  imgact_linux.c linux_ipc.c
.endif

EXPORT_SYMS=_linux_mod
CLEANFILES= linux_assym.h linux_genassym.o

linux_assym.h:	linux_genassym.o
.if exists(@)
linux_assym.h:	@/kern/genassym.sh
.endif
	sh @/kern/genassym.sh linux_genassym.o > ${.TARGET}

linux_locore.o:	linux_locore.s linux_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
		${.IMPSRC} -o ${.TARGET}

linux_genassym.o:	linux_genassym.c linux.h @ machine
	${CC} -c ${CFLAGS} ${.IMPSRC}

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/linux.sh ${DESTDIR}/usr/sbin/linux

.include <bsd.kmod.mk>
