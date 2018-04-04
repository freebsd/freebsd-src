#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
# $FreeBSD$

SRCS+=	\
	amd64_detect_rdfsgsbase.c \
	amd64_get_fsbase.c \
	amd64_get_gsbase.c \
	amd64_set_fsbase.c \
	amd64_set_gsbase.c

MDASM=	vfork.S brk.S cerror.S exect.S getcontext.S \
	sbrk.S setlogin.S

# Don't generate default code for these syscalls:
NOASM+=	vfork.o
