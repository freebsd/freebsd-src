#	$Id: Makefile,v 1.10 1998/06/01 02:12:15 jb Exp $
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
LIB=c_r
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
CFLAGS+=-DLIBC_RCS -DSYSLIBC_RCS -I${.CURDIR}/../libc/include
CFLAGS+=-DPTHREAD_KERNEL -D_THREAD_SAFE -I${.CURDIR}/uthread

# Uncomment this if you want libc_r to contain debug information for
# thread locking.
# CFLAGS+=-D_LOCK_DEBUG

AINC=	-I${.CURDIR}/../libc/${MACHINE_ARCH} -I${.CURDIR}/uthread
PRECIOUSLIB=	yes

#
# This is a list of syscalls that are renamed as _thread_sys_{syscall}
# so that libc_r can provide replacement functions.
#
HIDDEN_SYSCALLS= accept.o bind.o close.o connect.o dup.o dup2.o \
	execve.o fchflags.o fchmod.o fchown.o fcntl.o \
	flock.o fpathconf.o fstat.o fstatfs.o fsync.o getdirentries.o \
	getpeername.o getsockname.o getsockopt.o ioctl.o listen.o \
	mknod.o nanosleep.o nfssvc.o open.o read.o readv.o recvfrom.o \
	recvmsg.o sched_yield.o select.o sendmsg.o sendto.o \
	setsockopt.o shutdown.o sigaction.o sigaltstack.o \
	signanosleep.o socket.o socketpair.o wait4.o write.o writev.o

.include "${.CURDIR}/../libc/Makefile.inc"
.include "${.CURDIR}/man/Makefile.inc"
.include "${.CURDIR}/uthread/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"

.include <bsd.lib.mk>
