# $FreeBSD: src/lib/libc/ia64/sys/Makefile.inc,v 1.9.6.1 2008/11/25 02:59:29 kensmith Exp $

MDASM+=	Ovfork.S brk.S cerror.S exect.S fork.S getcontext.S pipe.S ptrace.S \
	sbrk.S setlogin.S sigreturn.S swapcontext.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o getdomainname.o getlogin.o openbsd_poll.o \
	setdomainname.o sstk.o uname.o vfork.o yield.o

PSEUDO=	_getlogin.o _exit.o
.if !defined(WITHOUT_SYSCALL_COMPAT)
PSEUDO+= _pread.o _pwrite.o _lseek.o _mmap.o _ftruncate.o _truncate.o
.endif
