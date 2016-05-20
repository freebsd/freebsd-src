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
