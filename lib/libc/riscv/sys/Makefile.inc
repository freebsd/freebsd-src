# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

#MDASM= ptrace.S
MDASM=	brk.S \
	cerror.S \
	pipe.S \
	sbrk.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S
