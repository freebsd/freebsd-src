# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_ARCH}/linux

VDSO=	linux_vdso

KMOD=	linux64
SRCS=	linux_fork.c linux_dummy.c linux_file.c linux_event.c \
	linux_futex.c linux_getcwd.c linux_ioctl.c linux_ipc.c \
	linux_machdep.c linux_misc.c linux_signal.c \
	linux_socket.c linux_stats.c linux_sysctl.c linux_sysent.c \
	linux_sysvec.c linux_time.c linux_vdso.c linux_timer.c \
	opt_inet6.h opt_compat.h opt_kdtrace.h opt_posix.h opt_usb.h \
	vnode_if.h device_if.h bus_if.h assym.s \
	linux_support.s
DPSRCS=	linux_genassym.c

# XXX: for assym.s
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_apic.h opt_hwpmc_hooks.h

CLEANFILES=	linux_assym.h linux_genassym.o linux_locore.o

OBJS=	${VDSO}.so

linux_assym.h: linux_genassym.o
	sh ${SYSDIR}/kern/genassym.sh linux_genassym.o > ${.TARGET}

linux_locore.o: linux_locore.s linux_assym.h
	${CC} -x assembler-with-cpp -DLOCORE -shared -mcmodel=small	\
	-pipe -I. -I${SYSDIR} -Werror -Wall -fno-common -nostdinc 	\
	-Wl,-T${.CURDIR}/../../${MACHINE_CPUARCH}/linux/${VDSO}.lds.s	\
	-Wl,-soname=${VDSO}.so.1,-fPIC,-warn-common -nostdlib		\
	    ${.IMPSRC} -o ${.TARGET}

${VDSO}.so: linux_locore.o
	${OBJCOPY} --input-target binary --output-target elf64-x86-64-freebsd		\
	    -S -g --binary-architecture i386:x86-64 linux_locore.o ${.TARGET}

linux_support.o: assym.s linux_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

linux_genassym.o:
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

.if !defined(KERNBUILDDIR)
.if defined(DEBUG)
CFLAGS+=-DDEBUG
.endif
.if defined(KTR)
CFLAGS+=-DKTR
.endif
.endif

.include <bsd.kmod.mk>
