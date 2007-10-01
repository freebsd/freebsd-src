# $FreeBSD$
#
# All library objects contain FreeBSD revision strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, add -DSTRIP_FBSDID (see <sys/cdefs.h>) to CFLAGS
# below.  Note, there are no IDs for syscall stubs whose sources are generated.
# To included legacy CSRG sccsid strings, add -DLIBC_SCCS and -DSYSLIBC_SCCS
# (for system call stubs) to CFLAGS below.  -DSYSLIBC_SCCS affects just the
# system call stubs.

.include <bsd.own.mk>

.if (${DEFAULT_THREAD_LIB} == "libpthread" || ${MK_LIBTHR} == "no") && \
    ${SHLIBDIR} == "/usr/lib"
SHLIBDIR= /lib
.endif

LIB=kse
SHLIB_MAJOR= 3
CFLAGS+=-DPTHREAD_KERNEL
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}/thread \
	-I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/arch/${MACHINE_ARCH}/include
CFLAGS+=-I${.CURDIR}/sys
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf/${MACHINE_ARCH}
CFLAGS+=-fno-builtin

# Uncomment this if you want libpthread to contain debug information for
# thread locking.
CFLAGS+=-D_LOCK_DEBUG
WARNS?=2

# Uncomment this if you want to build a 1:1 threading mode library
# however it is no longer strictly conformed to POSIX
# CFLAGS+=-DSYSTEM_SCOPE_ONLY

# Enable extra internal consistancy checks.
CFLAGS+=-D_PTHREADS_INVARIANTS -Wall

VERSION_MAP=${.CURDIR}/pthread.map

PRECIOUSLIB=

.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"
.include "${.CURDIR}/support/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/thread/Makefile.inc"

.if ${DEFAULT_THREAD_LIB} == "libpthread" || ${MK_LIBTHR} == "no"
SYMLINKS+=lib${LIB}.a ${LIBDIR}/libpthread.a
.if !defined(NO_PIC)
SYMLINKS+=lib${LIB}.so ${LIBDIR}/libpthread.so
.endif
.if ${MK_PROFILE} != "no"
SYMLINKS+=lib${LIB}_p.a ${LIBDIR}/libpthread_p.a
.endif
.endif

.include <bsd.lib.mk>
