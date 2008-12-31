# $FreeBSD: src/lib/libc/powerpc/sys/Makefile.inc,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

MDASM+=	brk.S cerror.S exect.S pipe.S ptrace.S sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o getdomainname.o getlogin.o openbsd_poll.o \
	setdomainname.o sstk.o uname.o yield.o

PSEUDO=	_getlogin.o _exit.o
.if !defined(WITHOUT_SYSCALL_COMPAT)
PSEUDO+= _pread.o _pwrite.o _lseek.o _mmap.o _ftruncate.o _truncate.o
.endif
