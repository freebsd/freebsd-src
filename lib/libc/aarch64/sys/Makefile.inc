# $FreeBSD$

MIASM:=	${MIASM:Nfreebsd[467]_*}

SRCS+=	__vdso_gettc.c

#MDASM= ptrace.S
MDASM=	brk.S \
	cerror.S \
	pipe.S \
	sbrk.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S

# Don't generate default code for these syscalls:
NOASM=	break.o \
	exit.o \
	getlogin.o \
	openbsd_poll.o \
	sstk.o \
	vfork.o \
	yield.o

PSEUDO= _exit.o \
	_getlogin.o
