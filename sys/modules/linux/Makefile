# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_ARCH}/linux

KMOD=	linux
SRCS=	linux_dummy.c linux_file.c linux_getcwd.c linux_ioctl.c linux_ipc.c \
	linux_machdep.c linux_mib.c linux_misc.c linux_signal.c linux_socket.c \
	linux_stats.c linux_sysctl.c linux_sysent.c linux_sysvec.c \
	linux_util.c opt_compat.h opt_inet6.h opt_mac.h \
	opt_vmpage.h vnode_if.h device_if.h bus_if.h
OBJS=	linux_locore.o

.if ${MACHINE_ARCH} == "i386"
SRCS+=	linux_ptrace.c linux_uid16.c imgact_linux.c opt_cpu.h
.endif

EXPORT_SYMS=
EXPORT_SYMS+=	linux_emul_path
EXPORT_SYMS+=	linux_get_osname
EXPORT_SYMS+=	linux_get_osrelease
EXPORT_SYMS+=	linux_ifname
EXPORT_SYMS+=	linux_ioctl_register_handler
EXPORT_SYMS+=	linux_ioctl_unregister_handler

CLEANFILES=	linux_assym.h linux_genassym.o

linux_assym.h: linux_genassym.o
.if exists(@)
linux_assym.h: @/kern/genassym.sh
.endif
	sh @/kern/genassym.sh linux_genassym.o > ${.TARGET}

linux_locore.o: linux_locore.s linux_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

linux_genassym.o: linux_genassym.c linux.h @ machine
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h
.endif

.include <bsd.kmod.mk>
