# $FreeBSD$

.if ${MACHINE_CPUARCH} == "amd64"
SFX= 32
CFLAGS+=-DCOMPAT_FREEBSD32 -DCOMPAT_LINUX32
.endif

.PATH: ${.CURDIR}/../../compat/linux ${.CURDIR}/../../${MACHINE_CPUARCH}/linux${SFX}

KMOD=	linux
SRCS=	linux${SFX}_dummy.c linux_emul.c linux_file.c \
	linux_futex.c linux_getcwd.c linux_ioctl.c linux_ipc.c \
	linux${SFX}_machdep.c linux_mib.c linux_misc.c linux_signal.c \
	linux_socket.c linux_stats.c linux_sysctl.c linux${SFX}_sysent.c \
	linux${SFX}_sysvec.c linux_uid16.c linux_util.c linux_time.c \
	opt_inet6.h opt_compat.h opt_posix.h opt_usb.h vnode_if.h \
	device_if.h bus_if.h assym.s

# XXX: for assym.s
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_compat.h opt_hwpmc_hooks.h
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	opt_apic.h
.endif

OBJS=	linux${SFX}_locore.o linux${SFX}_support.o

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	linux_ptrace.c imgact_linux.c opt_cpu.h
.endif

EXPORT_SYMS=
EXPORT_SYMS+=	linux_emul_path
EXPORT_SYMS+=	linux_get_osname
EXPORT_SYMS+=	linux_get_osrelease
EXPORT_SYMS+=	linux_ifname
EXPORT_SYMS+=	linux_ioctl_register_handler
EXPORT_SYMS+=	linux_ioctl_unregister_handler

CLEANFILES=	linux${SFX}_assym.h linux${SFX}_genassym.o

linux${SFX}_assym.h: linux${SFX}_genassym.o
.if exists(@)
linux${SFX}_assym.h: @/kern/genassym.sh
.endif
	sh @/kern/genassym.sh linux${SFX}_genassym.o > ${.TARGET}

linux${SFX}_locore.o: linux${SFX}_locore.s linux${SFX}_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

linux${SFX}_support.o: linux${SFX}_support.s assym.s linux${SFX}_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

linux${SFX}_genassym.o: linux${SFX}_genassym.c linux.h @ machine
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

.if !defined(KERNBUILDDIR)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.if defined(KTR)
CFLAGS+=	-DKTR
.endif
.endif

.include <bsd.kmod.mk>
