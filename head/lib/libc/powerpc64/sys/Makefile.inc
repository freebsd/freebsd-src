# $FreeBSD$

MDASM+=	brk.S cerror.S exect.S ptrace.S sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o getlogin.o sstk.o yield.o

PSEUDO=	_getlogin.o _exit.o
