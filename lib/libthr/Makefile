
PACKAGE=	clibs
SHLIBDIR?= /lib

.include <src.opts.mk>
MK_SSP=	no

LIB=thr
SHLIB_MAJOR= 3
NO_WTHREAD_SAFETY=1
NO_WCAST_ALIGN.gcc=1    # for gcc 4.2
CFLAGS+=-DPTHREAD_KERNEL
CFLAGS+=-I${SRCTOP}/lib/libc/include
CFLAGS+=-I${SRCTOP}/lib/libc/${MACHINE_CPUARCH}
CFLAGS+=-I${.CURDIR}/thread
CFLAGS+=-I${.CURDIR}/arch/${MACHINE_CPUARCH}/include
CFLAGS+=-I${.CURDIR}/sys
CFLAGS+=-I${SRCTOP}/libexec/rtld-elf
CFLAGS+=-I${SRCTOP}/libexec/rtld-elf/${MACHINE_CPUARCH}
CFLAGS+=-I${SRCTOP}/lib/libthread_db

CFLAGS.thr_stack.c+=	-Wno-cast-align
CFLAGS.rtld_malloc.c+=	-Wno-cast-align
CFLAGS.thr_symbols.c+=	-Wno-missing-variable-declarations

.if ${MACHINE_CPUARCH} == "arm"
NO_THREAD_UNWIND_STACK= yes
.endif

.ifndef NO_THREAD_UNWIND_STACK
CFLAGS+=-fexceptions
CFLAGS+=-D_PTHREAD_FORCED_UNWIND
.endif

LDFLAGS+=-Wl,-znodelete

VERSION_DEF=${SRCTOP}/lib/libc/Versions.def
SYMBOL_MAPS=${.CURDIR}/pthread.map

MAN=	libthr.3

.if ${MK_PTHREADS_ASSERTIONS} != "no"
# enable extra internal consistency checks
CFLAGS+=-D_PTHREADS_INVARIANTS
.endif

PRECIOUSLIB=

.PATH: ${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}
.PATH: ${SRCTOP}/libexec/rtld-elf

.if exists(${.CURDIR}/arch/${MACHINE_CPUARCH}/Makefile.inc)
.include "${.CURDIR}/arch/${MACHINE_CPUARCH}/Makefile.inc"
.endif
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/thread/Makefile.inc"
SRCS+= rtld_malloc.c

.if ${MK_INSTALLLIB} != "no"
SYMLINKS+=lib${LIB}.a ${LIBDIR}/libpthread.a
.endif
.if !defined(NO_PIC)
SYMLINKS+=lib${LIB}.so ${LIBDIR}/libpthread.so
.endif
.if ${MK_PROFILE} != "no"
SYMLINKS+=lib${LIB}_p.a ${LIBDIR}/libpthread_p.a
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
