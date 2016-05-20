# $FreeBSD$

MIASM:=	${MIASM:Nfreebsd[467]_*}

SRCS+=	__vdso_gettc.c

#MDASM= ptrace.S
MDASM=	cerror.S \
	pipe.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S

# Don't generate default code for these syscalls:
NOASM=	break.o \
	exit.o \
	getlogin.o \
	openbsd_poll.o \
	sbrk.o \
	sstk.o \
	vfork.o \
	yield.o

PSEUDO= _exit.o \
	_getlogin.o
