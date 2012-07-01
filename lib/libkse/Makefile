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

SHLIB=kse
SHLIB_MAJOR= 4
CFLAGS+=-DPTHREAD_KERNEL
CFLAGS+=-I${.CURDIR}/../libc/include -I${.CURDIR}/thread \
	-I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/arch/${MACHINE_CPUARCH}/include
CFLAGS+=-I${.CURDIR}/sys
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf
CFLAGS+=-I${.CURDIR}/../../libexec/rtld-elf/${MACHINE_CPUARCH}
CFLAGS+=-fno-builtin

# Uncomment this if you want libkse to contain debug information for
# thread locking.
CFLAGS+=-D_LOCK_DEBUG
WARNS?=3

# Uncomment this if you want to build a 1:1 threading mode library
# however it is no longer strictly conformed to POSIX
# CFLAGS+=-DSYSTEM_SCOPE_ONLY

# Enable extra internal consistancy checks.
CFLAGS+=-D_PTHREADS_INVARIANTS -Wall

VERSION_DEF=${.CURDIR}/../libc/Versions.def
SYMBOL_MAPS=${.CURDIR}/kse.map

PRECIOUSLIB=

.PATH: ${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

.include "${.CURDIR}/arch/${MACHINE_CPUARCH}/Makefile.inc"
.include "${.CURDIR}/support/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/thread/Makefile.inc"

.include <bsd.lib.mk>
