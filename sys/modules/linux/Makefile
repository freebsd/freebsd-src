# $FreeBSD$

.PATH:	${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_ARCH}/linux

MAINTAINER=	marcel@FreeBSD.org

KMOD=	linux
SRCS=	linux_file.c linux_getcwd.c linux_ioctl.c linux_ipc.c \
	linux_machdep.c linux_misc.c linux_signal.c linux_socket.c \
	linux_stats.c linux_mib.c linux_dummy.c linux_sysvec.c  \
	linux_util.c opt_compat.h opt_linux.h opt_vmpage.h vnode_if.h
OBJS=	linux_locore.o
MAN=	linux.8

.if ${MACHINE_ARCH} != "alpha"
SRCS+=  imgact_linux.c
.endif

# CFLAGS+=	-DDEBUG
CFLAGS += -I${.CURDIR}/../../${MACHINE_ARCH}/linux
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

GENSYSCALL=	linux_sysent.c linux_syscall.h linux_proto.h
SRCS+= ${GENSYSCALL}
CLEANFILES+= ${GENSYSCALL}

.ORDER: ${GENSYSCALL}
${GENSYSCALL}: @/kern/makesyscalls.sh \
	    @/${MACHINE_ARCH}/linux/syscalls.master \
		@/${MACHINE_ARCH}/linux/syscalls.conf
	sh @/kern/makesyscalls.sh @/${MACHINE_ARCH}/linux/syscalls.master \
		@/${MACHINE_ARCH}/linux/syscalls.conf
	sed -e 's|${MACHINE_ARCH}/linux/linux_proto\.h|linux_proto.h|g' \
	    linux_sysent.c > linux_sysent.c.fixup
	    mv -f linux_sysent.c.fixup linux_sysent.c

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/linux.sh ${DESTDIR}/usr/sbin/linux

.include <bsd.kmod.mk>
