#	$Id$
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
LIB=c_r
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
CFLAGS+=-DLIBC_RCS -DSYSLIBC_RCS
CFLAGS+=-DPTHREAD_KERNEL -D_THREAD_SAFE -I${.CURDIR}/uthread
AINC=	-I${.CURDIR}/../libc/${MACHINE} -I${.CURDIR}/uthread
PRECIOUSLIB=	yes

.include "${.CURDIR}/../libc/Makefile.inc"
.include "${.CURDIR}/man/Makefile.inc"
.include "${.CURDIR}/uthread/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"

.include <bsd.lib.mk>
