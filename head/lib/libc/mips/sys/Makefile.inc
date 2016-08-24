# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

MDASM=  Ovfork.S brk.S cerror.S exect.S \
	ptrace.S sbrk.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o getlogin.o sstk.o vfork.o yield.o

PSEUDO= _exit.o _getlogin.o
