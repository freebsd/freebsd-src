# $FreeBSD: src/lib/libc/powerpc/sys/Makefile.inc,v 1.1 2002/12/04 07:25:14 grehan Exp $

MDASM+=	brk.S cerror.S exect.S pipe.S ptrace.S sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mlockall.o mmap.o munlockall.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o yield.o

PSEUDO=	_getlogin.o _exit.o

