# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

MDASM=	cerror.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S

# Don't generate default code for these syscalls:
NOASM+=	vfork.o
