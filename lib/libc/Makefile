#	@(#)Makefile	8.2 (Berkeley) 2/3/94
# $FreeBSD$
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
LIB=c
SHLIB_MAJOR= 5
SHLIB_MINOR= 0
CFLAGS+=-DLIBC_RCS -DSYSLIBC_RCS -I${.CURDIR}/include
AINC=	-I${.CURDIR}/${MACHINE_ARCH}
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

#
# This is a list of syscalls that are renamed as __sys_{syscall}
# so that libpthread and libc_r can override and/or replace them.
# In the case of libc_r replacement functions are provided, whereas
# libpthread can both override and provide replacement functions.
#
HIDDEN_SYSCALLS= _exit.o accept.o aio_suspend.o bind.o close.o connect.o \
	dup.o dup2.o execve.o fchflags.o fchmod.o fchown.o fcntl.o \
	flock.o fpathconf.o fstat.o fstatfs.o fsync.o getdirentries.o \
	getpeername.o getsockname.o getsockopt.o ioctl.o \
	kevent.o listen.o \
	msync.o nanosleep.o nfssvc.o open.o poll.o read.o readv.o recvfrom.o \
	recvmsg.o sched_yield.o select.o sendfile.o sendmsg.o sendto.o \
	setsockopt.o shutdown.o sigaction.o sigaltstack.o \
	sigpending.o sigprocmask.o sigreturn.o \
	sigsuspend.o socket.o \
	socketpair.o wait4.o write.o writev.o

#
# Include make rules that are shared with libc_r.
#
.include "${.CURDIR}/Makefile.inc"

KQSRCS=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
KSRCS=	bcmp.c ffs.c index.c mcount.c rindex.c strcat.c strcmp.c strcpy.c \
	strlen.c strncpy.c

libkern: libkern.gen libkern.${MACHINE_ARCH}

libkern.gen: ${KQSRCS} ${KSRCS}
	cp -p ${.CURDIR}/quad/quad.h ${.ALLSRC} ${DESTDIR}/sys/libkern

libkern.${MACHINE_ARCH}:: ${KMSRCS}
.if defined(KMSRCS) && !empty(KMSRCS)
	cp -p ${.ALLSRC} ${DESTDIR}/sys/libkern/${MACHINE_ARCH}
.endif

.include <bsd.lib.mk>
