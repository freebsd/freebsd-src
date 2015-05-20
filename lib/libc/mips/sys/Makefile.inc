# $FreeBSD$

SRCS+=	trivial-vdso_tc.c

MDASM=	Ovfork.S cerror.S exect.S \
	pipe.S ptrace.S syscall.S

.if !defined(LIBCHERI)
MDASM+=	brk.S sbrk.S
.endif

# Don't generate default code for these syscalls:
NOASM=	vfork.o
