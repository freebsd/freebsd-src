# $FreeBSD: src/lib/libc/powerpc/sys/Makefile.inc,v 1.2.14.1 2008/10/02 02:57:24 kensmith Exp $

MDASM+=	brk.S cerror.S exect.S pipe.S ptrace.S sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mmap.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o yield.o

PSEUDO=	_getlogin.o _exit.o

