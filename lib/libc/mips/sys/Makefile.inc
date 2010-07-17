# $FreeBSD: src/lib/libc/mips/sys/Makefile.inc,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

MDASM=  Ovfork.S brk.S cerror.S exect.S \
	fork.S pipe.S ptrace.S sbrk.S shmat.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o ftruncate.o getlogin.o lseek.o mmap.o \
	openbsd_poll.o pread.o pwrite.o sstk.o truncate.o vfork.o yield.o

PSEUDO= _exit.o _getlogin.o
.if !defined(WITHOUT_SYSCALL_COMPAT)
PSEUDO+= _pread.o _pwrite.o _lseek.o _mmap.o _ftruncate.o _truncate.o
.endif
