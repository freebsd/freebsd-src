# $FreeBSD$
#
# All library objects contain FreeBSD revision strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, add -DSTRIP_FBSDID (see <sys/cdefs.h>) to CFLAGS
# below.  Note, there are no IDs for syscall stubs whose sources are generated.
# To included legacy CSRG sccsid strings, add -DLIBC_SCCS and -DSYSLIBC_SCCS
# (for system call stubs) to CFLAGS below.  -DSYSLIBC_SCCS affects just the
# system call stubs.
LIB=c_r
SHLIB_MAJOR= 5
CFLAGS+=-DPTHREAD_KERNEL -D_THREAD_SAFE 
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}/uthread \
	-I${.CURDIR}/../../include -I${.CURDIR}/../libc/${MACHINE_ARCH}

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
CFLAGS+=-D_LOCK_DEBUG

# enable extra internal consistancy checks
CFLAGS+=-D_PTHREADS_INVARIANTS

PRECIOUSLIB=	yes

.include "${.CURDIR}/man/Makefile.inc"
.include "${.CURDIR}/uthread/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"

.include <bsd.lib.mk>
