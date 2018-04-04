# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

#MDASM= ptrace.S
MDASM=	cerror.S \
	shmat.S \
	syscall.S \
	vfork.S

# Don't generate default code for these syscalls:
NOASM+=	vfork.o
