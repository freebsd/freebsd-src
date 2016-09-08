# $FreeBSD$

SRCS+=	__vdso_gettc.c

MDASM= Ovfork.S brk.S cerror.S sbrk.S shmat.S sigreturn.S syscall.S

# Don't generate default code for these syscalls:
NOASM+=	vfork.o
