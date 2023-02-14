# $FreeBSD$

.if ${MACHINE_CPUARCH} == "amd64"
SFX= 32
CFLAGS+=-DCOMPAT_FREEBSD32 -DCOMPAT_LINUX32
.endif

.PATH: ${SRCTOP}/sys/compat/linux ${SRCTOP}/sys/${MACHINE_CPUARCH}/linux${SFX}
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
.PATH: ${SRCTOP}/sys/x86/linux
.endif

KMOD=	linux
SRCS=	linux${SFX}_dummy_machdep.c \
	linux_elf32.c \
	linux_event.c \
	linux_file.c \
	linux_fork.c \
	linux_futex.c \
	linux_getcwd.c \
	linux_ioctl.c \
	linux_ipc.c \
	linux${SFX}_machdep.c \
	linux_misc.c \
	linux_rseq.c \
	linux_signal.c \
	linux_socket.c \
	linux_stats.c \
	linux${SFX}_syscalls.c \
	linux_sysctl.c \
	linux${SFX}_sysent.c \
	linux${SFX}_sysvec.c \
	linux_time.c \
	linux_timer.c \
	linux_uid16.c \
	linux_vdso.c \
	opt_inet6.h \
	opt_ktrace.h \
	opt_posix.h \
	bus_if.h \
	device_if.h \
	vnode_if.h
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SRCS+=	linux_dummy_x86.c
VDSODEPS=linux_vdso_gettc_x86.inc
.endif
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	linux${SFX}_support.s
.else
SRCS+=	linux_copyout.c
SRCS+=	linux_netlink.c
.endif
DPSRCS=	assym.inc linux${SFX}_genassym.c

# XXX: for assym.inc
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_hwpmc_hooks.h
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	opt_apic.h opt_cpu.h
.endif

OBJS=	linux${SFX}_vdso.so

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	imgact_linux.c \
	linux.c \
	linux_dummy.c \
	linux_emul.c \
	linux_errno.c \
	linux_mib.c \
	linux_mmap.c \
	linux_ptrace_machdep.c \
	linux_util.c \
	linux_vdso_selector_x86.c \
	linux_x86.c
.endif

.if ${MACHINE_CPUARCH} == "i386"
EXPORT_SYMS=
EXPORT_SYMS+=	linux_emul_path
EXPORT_SYMS+=	linux_get_osname
EXPORT_SYMS+=	linux_get_osrelease
EXPORT_SYMS+=	linux_ioctl_register_handler
EXPORT_SYMS+=	linux_ioctl_unregister_handler
.endif

CLEANFILES=	linux${SFX}_assym.h linux${SFX}_genassym.o linux${SFX}_locore.o \
		genassym.o linux${SFX}_vdso_gtod.o linux${SFX}_vdso.so.o

linux${SFX}_assym.h: linux${SFX}_genassym.o
	sh ${SYSDIR}/kern/genassym.sh linux${SFX}_genassym.o > ${.TARGET}

.if ${MACHINE_CPUARCH} == "amd64"
VDSOFLAGS=-DCOMPAT_FREEBSD32 -DCOMPAT_LINUX32 -m32
.else
VDSOFLAGS=-mregparm=0
.endif

linux${SFX}_locore.o: linux${SFX}_assym.h assym.inc
	${CC} -c -x assembler-with-cpp -DLOCORE -fPIC -pipe -O2 -Werror \
	-msoft-float \
	-fno-common -nostdinc -fasynchronous-unwind-tables \
	-fno-omit-frame-pointer -foptimize-sibling-calls ${VDSOFLAGS} \
	-fno-stack-protector -I. -I${SYSDIR} -I${SRCTOP}/include \
	    ${.IMPSRC} -o ${.TARGET}

linux${SFX}_vdso_gtod.o: linux_vdso_gtod.inc ${VDSODEPS}
	${CC} -c -fPIC -pipe -O2 -Werror -msoft-float \
	-fno-common -nostdinc -fasynchronous-unwind-tables \
	-fno-omit-frame-pointer -foptimize-sibling-calls ${VDSOFLAGS} \
	-fno-stack-protector -I. -I${SYSDIR} -I${SRCTOP}/include \
	    ${.IMPSRC} -o ${.TARGET}

linux${SFX}_vdso.so.o: linux${SFX}_locore.o linux${SFX}_vdso_gtod.o
	${LD} -m elf_i386 --shared --eh-frame-hdr -soname=linux-gate.so.1 \
	--no-undefined --hash-style=both -warn-common -nostdlib \
	--strip-debug -s --build-id=sha1 --Bsymbolic \
	-T${SRCTOP}/sys/${MACHINE}/linux${SFX}/linux${SFX}_vdso.lds.s \
	-o ${.TARGET} ${.ALLSRC:M*.o}

.if ${MACHINE_CPUARCH} == "amd64"
OBJCOPY_TARGET=--output-target elf64-x86-64-freebsd --binary-architecture i386
.elif ${MACHINE_CPUARCH} == "i386"
OBJCOPY_TARGET=--output-target elf32-i386-freebsd --binary-architecture i386
.else
.error ${MACHINE_CPUARCH} not yet supported by linux
.endif

linux${SFX}_vdso.so: linux${SFX}_vdso.so.o
	${OBJCOPY} --input-target binary ${OBJCOPY_TARGET} \
	    linux${SFX}_vdso.so.o ${.TARGET}
	${STRIPBIN} -N _binary_linux${SFX}_vdso_so_o_size ${.TARGET}

.if ${MACHINE_CPUARCH} == "amd64"
linux${SFX}_support.o: linux${SFX}_assym.h assym.inc
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}
.endif

linux${SFX}_genassym.o: offset.inc
	${CC} -c ${CFLAGS:N-flto:N-fno-common} -fcommon ${.IMPSRC}

.if !defined(KERNBUILDDIR)
.warning Building Linuxulator outside of a kernel does not make sense
.endif

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
