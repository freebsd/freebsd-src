# $FreeBSD: src/lib/libc/powerpc/sys/Makefile.inc,v 1.2 2003/08/11 07:14:07 bms Exp $

MDASM+=	brk.S cerror.S exect.S pipe.S ptrace.S sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mmap.o openbsd_poll.o pread.o \
	pwrite.o setdomainname.o sstk.o truncate.o uname.o yield.o

PSEUDO=	_getlogin.o _exit.o

