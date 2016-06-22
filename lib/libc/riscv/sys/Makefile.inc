# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

#MDASM= ptrace.S
MDASM=	cerror.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S
