# $FreeBSD$
#
# All library objects contain FreeBSD revision strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, add -DSTRIP_FBSDID (see <sys/cdefs.h>) to CFLAGS
# below.  Note, there are no IDs for syscall stubs whose sources are generated.
# To included legacy CSRG sccsid strings, add -DLIBC_SCCS and -DSYSLIBC_SCCS
# (for system call stubs) to CFLAGS below.  -DSYSLIBC_SCCS affects just the
# system call stubs.
LIB=thr
SHLIB_MAJOR= 2
CFLAGS+=-DPTHREAD_KERNEL
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}/thread \
	-I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/arch/${MACHINE_ARCH}/include
CFLAGS+=-I${.CURDIR}/sys
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf/${MACHINE_ARCH}
CFLAGS+=-I${.CURDIR}/../libthread_db
CFLAGS+=-Winline

# CFLAGS+=-DSYSTEM_SCOPE_ONLY

LDFLAGS= -Wl,--version-script=${.CURDIR}/pthread.map

MAN=	libthr.3

# enable extra internal consistancy checks
CFLAGS+=-D_PTHREADS_INVARIANTS -Wall
CFLAGS+=-g

PRECIOUSLIB=

.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/thread/Makefile.inc"

.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "sparc64"
SYMLINKS+=lib${LIB}.a ${LIBDIR}/libpthread.a
.if !defined(NO_PIC)
SYMLINKS+=lib${LIB}.so ${SHLIBDIR}/libpthread.so
.endif
.if !defined(NO_PROFILE)
SYMLINKS+=lib${LIB}_p.a ${LIBDIR}/libpthread_p.a
.endif
.endif

.include <bsd.lib.mk>
