# $FreeBSD$

MIASM:=	${MIASM:Nfreebsd[467]_*}

SRCS+=	__vdso_gettc.c

MDASM=	cerror.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S
